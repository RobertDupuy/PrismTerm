#!/usr/bin/env python3
"""Rebuild both boot disks from the preserved 0.1 MyDOS boot template.

Boot sectors and all unchanged DOS/font files retain their sectors. Replaced
files get freshly allocated chains; VTOC and directory are regenerated. No
external disk-format tool is required. Supports the bundled 720-sector DD disk.
"""
import pathlib,struct,shutil
ROOT=pathlib.Path(__file__).resolve().parents[1]
def make(image,files):
 data=bytearray((ROOT/'tools/boot-template.atr').read_bytes())
 def offset(n):return 16+(n-1)*128 if n<4 else 16+384+(n-4)*256
 def sector(n):return bytes(data[offset(n):offset(n)+(128 if n<4 else 256)])
 def put(n,b):data[offset(n):offset(n)+len(b)]=b
 entries=[];used={1,2,3,360,*range(361,369)}
 for n in range(361,369):
  block=sector(n)
  for i in range(0,128,16):
   e=bytearray(block[i:i+16]);idx=(n-361)*8+i//16
   if e[0]&64 and not e[0]&128:
    name=e[5:13].decode().strip();ext=e[13:16].decode().strip();name+='.'+ext if ext else ''
    # Keep the autorun slot while replacing the template's old program name.
    if name=='VBXEPLUS.AR1':name='PRISM.AR1'
    chain=[];out=bytearray();s=int.from_bytes(e[3:5],'little')
    while s:
     if s in chain:raise ValueError('loop in template')
     chain.append(s);b=sector(s);out+=b[:b[-1]];s=((b[-3]&3)<<8)|b[-2]
    entries.append([idx,name,e,chain,bytes(out)])
    used.update(chain)
 incoming={p.name:p.read_bytes() for p in files.iterdir() if p.is_file()}
 for entry in entries:
  idx,name,e,chain,old=entry
  if name not in incoming:raise ValueError(f'Template file missing from stage: {name}')
  if incoming[name]!=old:
   used.difference_update(chain);entry[3]=[]
 for name in incoming:
  if not any(e[1]==name for e in entries):
   idx=next(n for n in range(64) if n not in {e[0] for e in entries})
   entries.append([idx,name,bytearray(16),[],None])
 for idx,name,e,chain,old in entries:
  b=incoming[name]
  if not chain:
   count=(len(b)+252)//253;free=[s for s in range(4,721) if s not in used]
   if len(free)<count:raise ValueError('disk full')
   chain=free[:count];used.update(chain)
   for k,s in enumerate(chain):
    block=bytearray(256);part=b[k*253:(k+1)*253];block[:len(part)]=part;nxt=chain[k+1] if k+1<len(chain) else 0
    block[-3]=(idx<<2)|(nxt>>8);block[-2]=nxt&255;block[-1]=len(part);put(s,block)
  name8,_,ext=name.partition('.');e[:]=bytes(16);e[0]=0x42;e[1:3]=len(chain).to_bytes(2,'little');e[3:5]=(chain[0] if chain else 0).to_bytes(2,'little');e[5:13]=name8.encode().ljust(8);e[13:16]=ext.encode().ljust(3)
  block=bytearray(sector(361+idx//8));block[(idx%8)*16:(idx%8+1)*16]=e;put(361+idx//8,block)
 vtoc=bytearray(sector(360));vtoc[3:5]=(720-len(used)).to_bytes(2,'little');vtoc[10:100]=bytes(90)
 for n in range(1,721):
  if n not in used:vtoc[10+n//8]|=128>>(n%8)
 put(360,vtoc);image.write_bytes(data)
if __name__=='__main__':
 from verify_disk import verify
 for build,suffix in [('build','D600'),('build-d700','D700')]:
  out=ROOT/build;stage=out/'disk';shutil.rmtree(stage,ignore_errors=True);shutil.copytree(ROOT/'disk',stage)
  shutil.copyfile(out/'PRISM.XEX',stage/'PRISM.AR1');shutil.copyfile(out/'TRANS.BIN',stage/'TRANS.BIN')
  dest=out/f'PRISMTERM-{suffix}.ATR';make(dest,stage);verify(dest,stage)
