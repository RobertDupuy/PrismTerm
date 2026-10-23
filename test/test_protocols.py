import io,unittest
from machine import Machine
from test_features import packet
from third_party.xmodem import XMODEM
class Peer:
    """Independent XMODEM library drives the actual 6502 transfer routine."""
    def __init__(self,m,entry,corrupt=False):
        self.m=m;self.pos=0;self.corrupt=corrupt;self.corrupted=False
        m.cpu.stPushWord(0x05ff);m.cpu.pc=m.addr(entry)
    def getc(self,n,timeout=1):
        m=self.m
        for _ in range(2500000):
            if len(m.out)-self.pos>=n:
                data=bytes(m.out[self.pos:self.pos+n]);self.pos+=n;return data
            if m.cpu.pc==0x600:return None
            m.step()
        raise AssertionError('Independent peer stalled')
    def putc(self,data,timeout=1):
        if self.corrupt and not self.corrupted and len(data)>100:
            data=bytearray(data);data[-1]^=1;self.corrupted=True
        self.m.incoming+=data;return len(data)
    def finish(self):
        for _ in range(2500000):
            if self.m.cpu.pc==0x600:return
            self.m.step()
        raise AssertionError('6502 transfer failed to finish')
class Protocols(unittest.TestCase):
    def setup(self,serial=False):
        m=Machine();m.set('device_type',0 if serial else 1);m.set('n_unit',1)
        p=m.addr('io_name');m.mem[p:p+11]=b'D:TEST.BIN\x9b';return m
    def test_independent_sender_with_crc_recovery(self):
        m=self.setup();data=bytes(range(256))*9;peer=Peer(m,'xfer_receive',True)
        modem=XMODEM(peer.getc,peer.putc,mode='xmodem1k');modem.log.disabled=True
        self.assertTrue(modem.send(io.BytesIO(data),retry=10));peer.finish()
        self.assertEqual(m.get('xresult'),0);self.assertEqual(m.files['D:TEST.BIN'],data.ljust(3072,b'\x1a'))
        self.assertTrue(peer.corrupted)
    def test_independent_receiver_serial_transport(self):
        m=self.setup(serial=True);data=bytes(range(256))*9;m.files['D:TEST.BIN']=bytearray(data)
        peer=Peer(m,'xfer_send');modem=XMODEM(peer.getc,peer.putc);dest=io.BytesIO()
        self.assertEqual(modem.recv(dest,crc_mode=1,retry=10,delay=0),3072);peer.finish()
        self.assertEqual(m.get('xresult'),0);self.assertEqual(dest.getvalue(),data.ljust(3072,b'\x1a'))
    def test_bad_complement_consumes_packet(self):
        m=self.setup();data=bytes(range(128));bad=bytearray(packet(1,data));bad[2]^=1
        m.incoming+=bad+packet(1,data)+b'\x04';m.call('xfer_receive')
        self.assertEqual(m.get('xresult'),0);self.assertEqual(m.files['D:TEST.BIN'],data)
        self.assertEqual(m.out[:3],b'C\x15\x06')
    def test_duplicate_ymodem_header_requests_data_again(self):
        m=self.setup();m.set('xmode',1);head=packet(0,b'TEST\0' + b'1\0'+bytes(121))
        m.incoming+=head+head+packet(1,b'A'.ljust(128,b'\x1a'))+b'\x04\x04'+packet(0,bytes(128))
        m.call('xfer_receive');self.assertEqual(m.get('xresult'),0);self.assertEqual(m.files['D:TEST.BIN'],b'A')
        self.assertEqual(m.out[:5],b'C\x06C\x06C')
    def test_invalid_ymodem_size_is_rejected(self):
        for size in (b'',b'9999999999999',b'x'):
            m=self.setup();m.set('xmode',1);m.incoming+=packet(0,(b'FILE\0'+size+b'\0').ljust(128,b'\0'))
            m.call('xfer_receive');self.assertNotEqual(m.get('xresult'),0);self.assertEqual(m.files['D:TEST.BIN'],b'')
    def test_ymodem_truncated_file_not_success(self):
        m=self.setup();m.set('xmode',1)
        m.incoming+=packet(0,b'FILE\0' + b'200\0'+bytes(119))+packet(1,bytes(128))+b'\x04\x18'
        m.call('xfer_receive');self.assertNotEqual(m.get('xresult'),0)
    def test_disk_failure_is_not_acknowledged(self):
        m=self.setup();data=b'A'*128;m.incoming+=packet(1,data)
        original=m.cio
        def cio():
            if m.cpu.x==0x40 and m.mem[0x382]==11:m.ret(162)
            else:original()
        m.cio=cio;m.call('xfer_receive')
        self.assertEqual(m.get('xresult'),4);self.assertEqual(m.out,b'C\x18\x18')
    def test_bounded_retries(self):
        m=self.setup();m.incoming+=b'?'*10;m.call('xfer_receive')
        self.assertEqual(m.get('xresult'),1);self.assertEqual(m.out[-2:],b'\x18\x18')
if __name__=='__main__':unittest.main()
