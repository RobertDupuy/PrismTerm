"""Drive an independent native protocol peer against the compiled Atari XEX."""
import os,select,subprocess,time,pathlib,tempfile,pty,tty,fcntl,termios,shutil
from machine import Machine
DEPS=pathlib.Path(os.environ.get('PEER_BIN_DIR',str(pathlib.Path(shutil.which('sz') or '/usr/bin/sz').parent)))
class ProcessPeer:
 def __init__(self,m,entry,cmd,cwd,mutate=None,mutate_send=None):
  self.m=m;self.pos=0;self.mutate=mutate;self.mutate_send=mutate_send;self.sent=bytearray();self.received=bytearray()
  env=dict(os.environ);env['LD_LIBRARY_PATH']=str(DEPS.parent/'lib/x86_64-linux-gnu')
  if pathlib.Path(cmd[0]).name=='kermit':
   master,slave=pty.openpty();tty.setraw(slave)
   self.proc=subprocess.Popen(cmd,env=env,cwd=cwd,stdin=slave,stdout=slave,stderr=subprocess.PIPE,bufsize=0,start_new_session=True,preexec_fn=lambda:fcntl.ioctl(slave,termios.TIOCSCTTY,0))
   os.close(slave);self.proc.stdin=os.fdopen(master,'r+b',buffering=0);self.proc.stdout=self.proc.stdin
  else:self.proc=subprocess.Popen(cmd,env=env,cwd=cwd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,bufsize=0)
  os.set_blocking(self.proc.stdout.fileno(),False);os.set_blocking(self.proc.stderr.fileno(),False)
  self.err=bytearray();m.cpu.stPushWord(0x05ff);m.cpu.pc=m.addr(entry)
 def run(self,seconds=30):
  m=self.m;deadline=time.monotonic()+seconds;steps=0
  while time.monotonic()<deadline:
   ready=select.select([self.proc.stdout,self.proc.stderr],[],[],0)[0]
   for f in ready:
    try:b=os.read(f.fileno(),65536)
    except OSError:b=b''
    if f==self.proc.stderr:self.err+=b
    else:
     self.received+=b
     if self.mutate:b=self.mutate(b)
     m.incoming+=b
   if len(m.out)>self.pos:
    b=m.out[self.pos:];self.pos=len(m.out);self.sent+=b
    if self.mutate_send:b=self.mutate_send(b)
    try:self.proc.stdin.write(b);self.proc.stdin.flush()
    except BrokenPipeError:pass
   if m.cpu.pc==0x600:
    # Send any bytes emitted by the last instruction before returning.
    if self.proc.poll() is None:
     try:self.proc.wait(timeout=2)
     except subprocess.TimeoutExpired:pass
    return m.get('xresult')
   for _ in range(1000):
    if m.cpu.pc==0x600:break
    m.step();steps+=1
   if steps%100000==0:time.sleep(.001)
  raise AssertionError(f'peer timeout pc={m.cpu.pc:04x} out={self.sent[-200:]!r} in={self.received[-200:]!r} err={self.err[-400:]!r}')
 def close(self):
  if self.proc.poll() is None:self.proc.kill()
  self.proc.wait()
  for f in (self.proc.stdin,self.proc.stdout,self.proc.stderr):f.close()
def setup():
 m=Machine();m.set('device_type',1);m.set('n_unit',1)
 p=m.addr('io_name');m.mem[p:p+11]=b'D:TEST.BIN\x9b';return m
if __name__=='__main__':
 import sys
 protocol=sys.argv[1] if len(sys.argv)>1 else 'z'
 direction=sys.argv[2] if len(sys.argv)>2 else 'receive'
 data=bytes(range(256))*5+b'end'
 with tempfile.TemporaryDirectory() as td:
  m=setup()
  if direction=='receive':pathlib.Path(td,'TEST.BIN').write_bytes(data)
  else:m.files['D:TEST.BIN']=bytearray(data)
  if protocol=='z':cmd=[str(DEPS/('sz' if direction=='receive' else 'rz')),'-q','-b','-y']+(['TEST.BIN'] if direction=='receive' else [])
  else:cmd=[str(DEPS/'kermit'),'-Y','-Q','-i']+(['-s','TEST.BIN'] if direction=='receive' else ['-r'])
  p=ProcessPeer(m,('zmodem_' if protocol=='z' else 'kermit_')+direction,cmd,td)
  try:
   print('result',p.run(), 'err',p.err.decode(errors='replace'))
   actual=m.files.get('D:TEST.BIN',b'') if direction=='receive' else next(pathlib.Path(td).glob('*')).read_bytes()
   print('match',bytes(actual)==data,'length',len(actual),'expected',len(data))
   print('sent',p.sent[:300],p.sent[-100:]);print('received',p.received[:300],p.received[-100:])
  finally:p.close()
