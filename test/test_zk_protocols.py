"""Interoperability with unmodified lrzsz and C-Kermit executables."""
import pathlib,tempfile,unittest,binascii,os
from peer_process import ProcessPeer,setup,DEPS
class Corrupt:
 def __init__(self,needle=b'PAYLOADabcdefghijklmnop'):self.needle=needle;self.done=False
 def __call__(self,b):
  if not self.done and self.needle in b:
   b=bytearray(b);b[b.index(self.needle)+9]^=1;self.done=True
  return b
@unittest.skipUnless((DEPS/'sz').exists() and (DEPS/'kermit').exists(),'Install lrzsz and C-Kermit (or set PEER_BIN_DIR)')
class IndependentProtocols(unittest.TestCase):
 def transfer(self,protocol,direction,data,corrupt=False,serial=False,crc16=False):
  with tempfile.TemporaryDirectory() as td:
   m=setup();m.set('device_type',0 if serial else 1)
   if direction=='receive':pathlib.Path(td,'TEST.BIN').write_bytes(data)
   else:m.files['D:TEST.BIN']=bytearray(data)
   if protocol=='z':cmd=[str(DEPS/('sz' if direction=='receive' else 'rz')),'-q','-b','-y']+(['-o'] if crc16 and direction=='receive' else [])+(['TEST.BIN'] if direction=='receive' else [])
   else:cmd=[str(DEPS/'kermit'),'-Y','-Q','-i']+(['-s','TEST.BIN'] if direction=='receive' else ['-r'])
   damage=Corrupt() if corrupt else None
   peer=ProcessPeer(m,('zmodem_' if protocol=='z' else 'kermit_')+direction,cmd,td,mutate=damage if direction=='receive' else None,mutate_send=damage if direction=='send' else None)
   try:
    result=peer.run(seconds=45)
    self.assertEqual(result,0,(protocol,direction,peer.err[-300:],peer.sent[-100:],peer.received[-100:]))
    actual=m.files.get('D:TEST.BIN',b'') if direction=='receive' else next(pathlib.Path(td).glob('*')).read_bytes()
    self.assertEqual(bytes(actual),data)
    self.assertEqual(m.mem.bank_b,0)
    if damage:self.assertTrue(damage.done,'test did not inject the intended fault')
   finally:peer.close()
 def test_z_receive_crc32(self):self.transfer('z','receive',bytes(range(256))*5+b'END')
 def test_z_receive_crc16(self):self.transfer('z','receive',bytes(range(256))*5+b'END',crc16=True)
 def test_z_upload_serial(self):self.transfer('z','send',bytes(range(256))*7+b'END',serial=True)
 def test_z_corrupted_download_recovers(self):self.transfer('z','receive',b'PAYLOADabcdefghijklmnop'*80,corrupt=True)
 def test_z_corrupted_upload_recovers(self):self.transfer('z','send',b'PAYLOADabcdefghijklmnop'*80,corrupt=True)
 def test_z_empty_both_directions(self):
  for direction in ('send','receive'):self.transfer('z',direction,b'')
 def resume(self,prefix,expected=0):
  with tempfile.TemporaryDirectory() as td:
   data=bytes(range(256))*20+b'END';pathlib.Path(td,'TEST.BIN').write_bytes(data)
   m=setup();m.set('xmode',4)
   if prefix is not None:m.files['D:TEST.BIN']=bytearray(prefix)
   peer=ProcessPeer(m,'zmodem_receive',[str(DEPS/'sz'),'-q','-b','-y','TEST.BIN'],td)
   try:
    self.assertEqual(peer.run(seconds=45),expected)
    self.assertEqual(bytes(m.files['D:TEST.BIN']),data if expected==0 else prefix)
    self.assertEqual(m.mem.bank_b,0)
    if prefix and expected==0:self.assertLess(len(peer.received),len(data))
   finally:peer.close()
 def test_z_resume_verified_prefix(self):self.resume((bytes(range(256))*20)[:2345])
 def test_z_resume_mismatch_preserves_file(self):self.resume(b'BAD!'+(bytes(range(256))*20)[4:2345],7)
 def test_z_resume_missing_and_empty(self):
  for prefix in (None,b''):self.resume(prefix)
 def test_k_receive_binary(self):self.transfer('k','receive',bytes(range(256))*5+b'END')
 def test_k_upload_wraps_sequence(self):self.transfer('k','send',bytes(range(256))*16+b'END')
 def test_k_corrupt_receive_recovers(self):self.transfer('k','receive',b'PAYLOADabcdefghijklmnop'*80,corrupt=True)
 def test_k_corrupt_upload_recovers(self):self.transfer('k','send',b'PAYLOADabcdefghijklmnop'*80,corrupt=True)
 def test_k_empty_both_directions(self):
  for direction in ('send','receive'):self.transfer('k',direction,b'')
class ProtocolBounds(unittest.TestCase):
 def test_z_cancel(self):
  m=setup();m.incoming+=b'\x18'*5;m.call('zmodem_receive');self.assertEqual(m.get('xresult'),2);self.assertEqual(m.mem.bank_b,0)
 def test_k_remote_error(self):
  m=setup();b=b'# E';s=sum(b);check=32+((s+((s&192)>>6))&63);m.incoming+=b'\x01'+b+bytes([check])+b'\r';m.call('kermit_receive');self.assertNotEqual(m.get('xresult'),0);self.assertEqual(m.mem.bank_b,0)
 def test_missing_bank_is_reported(self):
  m=setup();m.set('proto_ready',0);m.call('zmodem_send');self.assertEqual(m.get('xresult'),6);self.assertEqual(m.out,b'')
 def test_z_bad_headers_are_bounded(self):
  m=setup();m.incoming+=b'!'*(4096*10);m.call('zmodem_receive',limit=9000000);self.assertEqual(m.get('xresult'),1)
 def test_k_duplicate_data_written_once(self):
  def packet(seq,typ,data=b''):
   b=bytes([len(data)+35,seq+32,ord(typ)])+data;s=sum(b);return b'\x01'+b+bytes([32+((s+((s&192)>>6))&63)])+b'\r'
  m=setup();d=packet(2,'D',b'ABC');m.incoming+=packet(0,'S',b'~% @-#&1  ')+packet(1,'F',b'IGNORED')+d+d+packet(3,'Z')+packet(4,'B');m.call('kermit_receive');self.assertEqual(m.files['D:TEST.BIN'],b'ABC');self.assertEqual(m.get('xresult'),0)
if __name__=='__main__':unittest.main()
