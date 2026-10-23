import unittest
from machine import Machine
class Terminal(unittest.TestCase):
    def setUp(self):self.m=Machine()
    def feed(self,s):self.m.feed(s)
    def pos(self,r,c):self.assertEqual((self.m.get('row'),self.m.get('column')),(r,c))
    def test_buffers(self):
        self.assertGreater(self.m.addr('recv_buffer'),self.m.addr('_vbxe_shutdown'))
        self.assertLess(self.m.addr('ctrl_seq_buf')+256,0xa000)
    def test_ascii_wrap(self):
        self.feed(b'A'*80);self.pos(0,79)
        self.feed(b'B');self.pos(1,1)
        self.assertEqual(self.m.screen()[0],b'A'*80)
        self.assertEqual(self.m.screen()[1][0],ord('B'))
    def test_last_column_cr(self):
        self.feed(b'A'*80+b'\rZ');self.pos(0,1)
        self.assertEqual(self.m.screen()[0][0],ord('Z'))
    def test_cursor_counts(self):
        self.feed('\x1b[10;20H');self.pos(9,19)
        self.feed('\x1b[3A\x1b[4D');self.pos(6,15)
        self.feed('\x1b[200B\x1b[999C');self.pos(23,79)
        self.feed('\x1b[999A\x1b[999D');self.pos(0,0)
    def test_region_scroll(self):
        for r in range(24):self.feed(f'\x1b[{r+1};1H'+chr(65+r))
        self.feed('\x1b[5;10r\x1b[10;1H\n')
        s=self.m.screen();self.assertEqual(s[3][0],ord('D'));self.assertEqual(s[4][0],ord('F'));self.assertEqual(s[9][0],0);self.assertEqual(s[10][0],ord('K'))
    def test_reverse_index(self):
        self.feed('\x1b[5;10r\x1b[5;1HABC\x1b[5;1H\x1bM')
        self.assertEqual(self.m.screen()[4][:3],b'\0'*3);self.assertEqual(self.m.screen()[5][:3],b'ABC')
    def test_origin(self):
        self.feed('\x1b[5;10r\x1b[?6h');self.pos(4,0)
        self.feed('\x1b[2;3H');self.pos(5,2)
        self.feed('\x1b[99;1H');self.pos(9,0)
        self.feed('\x1b[?6l');self.pos(0,0)
    def test_wrap_disabled(self):
        self.feed('\x1b[?7l'+('X'*82));self.pos(0,79)
    def test_edit_cells(self):
        self.feed('ABCDE\r\x1b[2C\x1b[2@')
        self.assertEqual(self.m.screen()[0][:7],b'AB\0\0CDE')
        self.feed('\x1b[2P');self.assertEqual(self.m.screen()[0][:5],b'ABCDE')
        self.feed('\x1b[2X');self.assertEqual(self.m.screen()[0][:5],b'AB\0\0E')
    def test_sgr(self):
        self.feed('\x1b[31;44mA\x1b[7mB\x1b[7mC\x1b[27mD')
        self.m.call('cursor_off')
        self.assertEqual(list(self.m.mem.vram[0x7f101:0x7f108:2]),[0xc1,0x94,0x94,0xc1])
        self.feed('\x1b[91m');self.assertEqual(self.m.get('draw_fg'),9)
    def test_reports(self):
        self.feed('\x1b[10;20H\x1b[6n\x1b[5n\x1b[c')
        self.m.call('check_sendbuf');self.assertEqual(bytes(self.m.out),b'\x1b[10;20R\x1b[0n\x1b[?1;0c')
    def test_save_restore(self):
        self.feed('\x1b[5;8H\x1b[31m\x1b7\x1b[1;1H\x1b[32m\x1b8');self.pos(4,7);self.assertEqual(self.m.get('draw_fg'),1)
    def test_alternate_restores_scroll_margins(self):
        self.feed('\x1b[5;15r\x1b[?6h\x1b[3;4H\x1b[?1049h\x1b[2;20r\x1b[?1049l')
        self.assertEqual((self.m.get('scroll_top'),self.m.get('scroll_bot')),(4,14));self.pos(6,3)
    def test_restore_default_attributes(self):
        self.feed('\x1b[31m\x1b8X');self.assertEqual(self.m.get('draw_fg'),7)
    def test_tabs(self):
        self.feed('A\tB');self.pos(0,9)
        self.feed('\x1b[3g\r\t');self.pos(0,79)
    def test_utf8(self):
        self.m.set('utf8_mode',1);self.feed('café ┌─┐ π 😀')
        self.assertEqual(self.m.screen()[0][:12],b'caf\x82 \xda\xc4\xbf \xe3 ?')
    def test_bad_utf8(self):
        self.m.set('utf8_mode',1);self.feed(b'\xc2A\xed\xa0\x80B\xe2\x94\x80')
        self.assertIn(b'A',self.m.screen()[0]);self.assertIn(b'B',self.m.screen()[0]);self.assertIn(b'\xc4',self.m.screen()[0])
    def test_bounded_parser(self):
        before=bytes(self.m.mem.ram[self.m.addr('ctrl_seq_buf')+256:self.m.addr('ctrl_seq_buf')+272])
        self.feed(b'\x1b['+b'1;'*1000+b'mOK')
        self.assertIn(b'OK',self.m.screen()[0])
        self.assertEqual(bytes(self.m.mem.ram[self.m.addr('ctrl_seq_buf')+256:self.m.addr('ctrl_seq_buf')+272]),before)
    def test_string_termination(self):
        self.feed(b'\x1b]0;SECRET\x1bXHIDDEN\x1b\\OK')
        self.assertEqual(self.m.screen()[0][:2],b'OK')
    def test_nonblocking_bell(self):self.assertLess(self.m.feed(b'\a'),500)
if __name__=='__main__':unittest.main()
