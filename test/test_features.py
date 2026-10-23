import unittest,binascii,struct
from machine import Machine
def packet(seq,data):
    assert len(data) in (128,1024)
    return bytes([1 if len(data)==128 else 2,seq,seq^255])+data+binascii.crc_hqx(data,0).to_bytes(2,'big')
class Features(unittest.TestCase):
    def setUp(self):self.m=Machine()
    def test_alternate_screen(self):
        m=self.m;m.feed('MAIN\x1b[10;20H\x1b[?1049hALT\x1b[?1049l')
        self.assertEqual(m.screen()[0][:4],b'MAIN');self.assertEqual((m.get('row'),m.get('column')),(9,19))
    def test_scrollback(self):
        m=self.m
        for i in range(40):m.feed(f'{i:03d}\r\n')
        self.assertEqual(m.get('history_count'),17)
        self.assertEqual(bytes(m.mem.vram[0x30000:0x30006:2]),b'000')
        self.assertEqual(bytes(m.mem.vram[0x31000:0x31006:2]),b'016')
    def test_capture_boundaries(self):
        m=self.m;m.set('capture_on',1)
        for i in range(32770):m.set('temp_char',i&255);m.call('plus_capture_byte')
        self.assertEqual(bytes(m.mem.vram[0x70000:0x78000]),bytes(range(256))*128)
        self.assertEqual(m.get('capture_full'),1);self.assertEqual(m.get('capture_on'),0)
    def test_macro(self):
        m=self.m;p=m.addr('login_macro');m.mem[p:p+12]=b'USER^MPASS^M\0'
        m.set('macro_running',1)
        for i in range(20):m.call('plus_macro_pump')
        m.call('check_sendbuf');self.assertEqual(bytes(m.out),b'USER\rPASS\r')
    def test_fonts_and_underline(self):
        m=self.m;m.call('plus_expand_fonts')
        font=bytes(m.mem.vram[:2048])
        for g in (0,32,65,196,255):
            for fmt,base in [(0,0x10000),(1,0x12000),(2,0x14000),(3,0x18000)]:
                exp=bytearray()
                for y in range(8):
                    bits=255 if fmt&1 and y==7 else font[g*8+y]
                    if fmt<2:exp.extend((((15 if bits&(128>>x) else 0)<<4)|(15 if bits&(64>>x) else 0)) for x in range(0,8,2))
                    else:exp.extend(255 if bits&(128>>x) else 0 for x in range(8))
                n=32 if fmt<2 else 64
                self.assertEqual(bytes(m.mem.vram[base+g*n:base+(g+1)*n]),bytes(exp))
    def test_bitmap_render_and_blink(self):
        m=self.m;m.feed('\x1b[4;31mA\x1b[0mB\x1b[5mC')
        m.call('cursor_off');m.call('plus_render_dirty')
        self.assertEqual(bytes(m.mem.vram[0x20000+7*320:0x20000+7*320+4]),b'\x11'*4)
        old=bytes(m.mem.vram[0x20008:0x2000c]);self.assertNotEqual(old,b'\0'*4)
        m.set('blink_phase',1);m.call('plus_dirty_all');m.call('plus_render_dirty')
        self.assertEqual(bytes(m.mem.vram[0x20008:0x2000c]),b'\0'*4)
    def test_full_width_40(self):
        m=self.m;m.set('term_last_col',39);m.feed('A'*40);m.call('cursor_off');m.call('plus_render_dirty')
        self.assertEqual(m.get('bitmap_on'),2)
        self.assertEqual(bytes(m.mem.vram[0x20000:0x20008]),bytes(m.mem.vram[0x20138:0x20140]))
        self.assertEqual(m.mem.vram[0x806],0x80)
class Transfers(unittest.TestCase):
    def setUp(self):
        self.m=Machine();self.m.set('device_type',1);self.m.set('n_unit',1)
        p=self.m.addr('io_name');self.m.mem[p:p+11]=b'D:TEST.BIN\x9b'
    def test_crc(self):
        m=self.m;p=m.addr('xbuf');m.mem[p:p+9]=b'123456789';m.set('xlength',9);m.call('xcrc_block')
        self.assertEqual(m.get('xcrc')|m.mem[m.addr('xcrc')+1]<<8,0x31c3)
    def test_xmodem_receive_duplicates_and_crc(self):
        m=self.m;a=bytes(range(128));b=b'B'*1024
        bad=bytearray(packet(2,b));bad[-1]^=1
        m.incoming+=packet(1,a)+packet(1,a)+bad+packet(2,b)+b'\x04'
        m.call('xfer_receive',limit=8000000)
        self.assertEqual(m.get('xresult'),0);self.assertEqual(bytes(m.files['D:TEST.BIN']),a+b)
        self.assertIn(21,m.out);self.assertEqual(m.out[-1],6)
    def test_ymodem_receive_exact_length(self):
        m=self.m;m.set('xmode',1)
        header=b'TEST.BIN\x00137\x00'.ljust(128,b'\0')
        data=bytes(range(137)).ljust(1024,b'\x1a')
        m.incoming+=packet(0,header)+packet(1,data)+b'\x04\x04'+packet(0,bytes(128))
        m.call('xfer_receive',limit=8000000)
        self.assertEqual(m.get('xresult'),0);self.assertEqual(bytes(m.files['D:TEST.BIN']),bytes(range(137)))
    def test_xmodem_send(self):
        m=self.m;m.files['D:TEST.BIN']=bytearray(b'HELLO');m.incoming+=b'C\x06\x06'
        m.call('xfer_send',limit=8000000)
        self.assertEqual(m.get('xresult'),0)
        self.assertEqual(bytes(m.out),packet(1,b'HELLO'.ljust(1024,b'\x1a'))+b'\x04')
    def test_ymodem_send(self):
        m=self.m;m.set('xmode',1);m.files['D:TEST.BIN']=bytearray(bytes(range(137)))
        m.incoming+=b'C\x06C\x06\x15\x06C\x06'
        m.call('xfer_send',limit=8000000)
        self.assertEqual(m.get('xresult'),0)
        self.assertEqual(bytes(m.out[:133]),packet(0,b'TEST.BIN\x00137'.ljust(128,b'\0')))
        self.assertEqual(bytes(m.out[133:1162]),packet(1,bytes(range(137)).ljust(1024,b'\x1a')))
    def test_peer_cancel(self):
        m=self.m;m.incoming+=b'\x18';m.call('xfer_receive',limit=500000)
        self.assertEqual(m.get('xresult'),2);self.assertTrue(m.out.endswith(b'\x18\x18'))
if __name__=='__main__':unittest.main()
