#!/usr/bin/env python3
"""Build a read-only, indexed Unicode glyph ATR for the Atari's D2: drive.

UFN1 format: sector 1 signature, sectors 4..37 = 4352 little-endian page
pointers. Each present 256-codepoint page is 32 sectors of 8 records each.
A 32-byte record holds flags, 8 rows of two bitmap bytes, reserved bytes.
Width (0, 1 or 2) is in bits 0..1; bit 7 says the glyph exists. Source glyphs
are 8x16 or 16x16; pairs of vertical rows are ORed for native 8-pixel cells.
The companion layout bridge preserves all 16 rows by using two Atari rows.
"""
import argparse,gzip,pathlib,struct,json,hashlib
from wcwidth import wcwidth
ROOT=pathlib.Path(__file__).resolve().parents[1]
def build(source,dest):
 data=gzip.decompress(source.read_bytes()) if source.suffix=='.gz' else source.read_bytes()
 pages={};glyphs=0
 for line in data.decode().splitlines():
  if ':' not in line:continue
  key,hexdata=line.split(':',1);cp=int(key,16)
  if cp>0x10ffff or 0xd800<=cp<=0xdfff:continue
  raw=bytes.fromhex(hexdata)
  if len(raw) not in (16,32):continue
  width=wcwidth(chr(cp),unicode_version='17.0.0')
  if width<0:continue
  record=bytearray(32);record[0]=0x80|width
  wide=len(raw)==32
  for y in range(8):
   for x in range(2):
    record[1+y*2+x]=(raw[y*4+x]|raw[y*4+2+x]) if wide else (raw[y*2]|raw[y*2+1] if x==0 else 0)
  # Variation selectors and format controls are invisible in direct mode.
  if 0xfe00<=cp<=0xfe0f or 0xe0000<=cp<=0xe0fff or cp in (0x200b,0x200c,0x200d,0x2060,0xfeff):record[1:17]=bytes(16)
  page=pages.setdefault(cp>>8,bytearray(8192));page[(cp&255)*32:(cp&255)*32+32]=record;glyphs+=1
 directory=bytearray(8704);payload=bytearray(384);payload[:4]=b'UFN1';payload[4:12]=b'17.0.0\0\0';sector=38
 for key,page in sorted(pages.items()):
  struct.pack_into('<H',directory,key*2,sector);sector+=32
 if sector>65535:raise ValueError('Font exceeds a 16-bit sector index')
 payload+=directory
 for page in [pages[k] for k in sorted(pages)]:payload+=page
 header=bytearray(16);struct.pack_into('<HHH',header,0,0x296,len(payload)//16&65535,256);header[6]=len(payload)//16>>16
 dest.write_bytes(header+payload)
 result={'glyphs':glyphs,'pages':len(pages),'sectors':sector-1,'size_bytes':dest.stat().st_size,'source':source.name,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'output_sha256':hashlib.sha256(dest.read_bytes()).hexdigest(),'unicode_width_version':'17.0.0','native_cell_height':8,'source_cell_height':16}
 dest.with_suffix('.json').write_text(json.dumps(result,indent=2)+'\n');return result
if __name__=='__main__':
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--source',type=pathlib.Path,default=ROOT/'fonts/unifont_all-17.0.04.hex.gz');ap.add_argument('--output',type=pathlib.Path,default=ROOT/'fonts/UNICODE.ATR');a=ap.parse_args();print(json.dumps(build(a.source,a.output),indent=2))
