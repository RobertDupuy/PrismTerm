import unittest
from machine import Machine
from test_features import packet
class Telnet(unittest.TestCase):
    def setUp(self):
        self.m=Machine();self.m.set('device_type',1);self.m.set('n_unit',1);self.m.set('telnet_on',1)
    def receive(self,b):
        self.m.incoming+=b
        while self.m.incoming:self.m.call('recv_from_device')
    def test_iac_and_negotiation_split_across_reads(self):
        m=self.m
        for b in (b'A\xff',b'\xfb',b'\x01B\xff\xffC'):self.receive(b)
        self.assertEqual(m.screen()[0][:4],b'AB\xffC');self.assertEqual(m.out,b'\xff\xfd\x01')
        self.receive(b'\xff\xfb\x01');self.assertEqual(m.out,b'\xff\xfd\x01')
    def test_type_and_window(self):
        m=self.m;self.receive(b'\xff\xfd\x18\xff\xfa\x18\x01\xff\xf0\xff\xfd\x1f')
        self.assertEqual(m.out,b'\xff\xfb\x18\xff\xfa\x18\0VT100\xff\xf0\xff\xfb\x1f\xff\xfa\x1f\0P\0\x18\xff\xf0')
        m.set('term_last_col',39);m.call('telnet_naws');self.assertEqual(m.out[-9:],b'\xff\xfa\x1f\0(\0\x18\xff\xf0')
    def test_unsupported_option_refused(self):
        self.receive(b'\xff\xfb\x63\xff\xfd\x63')
        self.assertEqual(self.m.out,b'\xff\xfe\x63\xff\xfc\x63')
    def test_binary_offers_do_not_loop(self):
        m=self.m;m.set('proto_byte',ord('T'));m.call('telnet_start')
        self.assertEqual(m.out,b'\xff\xfb\0\xff\xfd\0')
        self.receive(b'\xff\xfd\0\xff\xfb\0');self.assertEqual(m.out,b'\xff\xfb\0\xff\xfd\0')
        self.assertEqual(bytes(m.mem[m.addr('tn_enabled'):m.addr('tn_enabled')+2]),b'\1\1')
        self.receive(b'\xff\xfe\0\xff\xfc\0');self.assertEqual(m.out[-6:],b'\xff\xfc\0\xff\xfe\0')
    def test_escape_large_binary_write(self):
        m=self.m;m.mem[m.addr('tn_enabled')+1]=1;p=m.addr('xbuf');data=bytes(range(256))*4;m.mem[p:p+1024]=data
        m.set('io_ptr',p&255);m.mem[m.addr('io_ptr')+1]=p>>8;m.set('io_size',0);m.mem[m.addr('io_size')+1]=4
        m.call('wire_send');self.assertEqual(m.out,data.replace(b'\xff',b'\xff\xff'))
    def test_escape_full_256_byte_output_chunk(self):
        m=self.m;p=m.addr('xbuf');m.mem[p:p+128]=b'\xff'*128
        m.set('io_ptr',p&255);m.mem[m.addr('io_ptr')+1]=p>>8;m.set('io_size',128);m.mem[m.addr('io_size')+1]=0
        m.call('wire_send');self.assertEqual(m.out,b'\xff'*256)
    def test_xmodem_over_telnet_binary(self):
        m=self.m;m.mem[m.addr('tn_enabled'):m.addr('tn_enabled')+2]=b'\1\1'
        p=m.addr('io_name');m.mem[p:p+11]=b'D:TEST.BIN\x9b';data=bytes(range(256))*4
        m.incoming+=packet(1,data).replace(b'\xff',b'\xff\xff')+b'\x04';m.call('xfer_receive')
        self.assertEqual(m.get('xresult'),0);self.assertEqual(m.files['D:TEST.BIN'],data)
    def test_nvt_cr(self):
        m=self.m;p=m.addr('xbuf');m.mem[p:p+3]=b'A\rB';m.set('io_ptr',p&255);m.mem[m.addr('io_ptr')+1]=p>>8
        m.set('io_size',3);m.mem[m.addr('io_size')+1]=0;m.call('wire_send');self.assertEqual(m.out,b'A\r\0B')
        self.receive(b'ONE\r\0TWO');self.assertEqual(m.screen()[0][:3],b'TWO')
if __name__=='__main__':unittest.main()
