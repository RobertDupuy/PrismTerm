#!/usr/bin/env python3
"""Build PrismTerm with cc65. No generated source or network access needed."""
import argparse,os,pathlib,shutil,subprocess
ROOT=pathlib.Path(__file__).resolve().parent
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--base',type=lambda s:int(s,0),choices=[0xd600,0xd700],default=0xd600)
    ap.add_argument('--output',default='build',help='build directory, relative to source root')
    args=ap.parse_args();out=ROOT/args.output;out.mkdir(parents=True,exist_ok=True)
    if not shutil.which('ca65'):raise SystemExit('Install cc65 and put ca65/ld65 on PATH.')
    def run(*cmd):subprocess.run(cmd,cwd=ROOT,check=True)
    define=f'VBXE_BASE={args.base}'
    run('ca65','-g','-D',define,'ANSIVBXE_ca65.asm','-o',str(out/'main.o'),'-l',str(out/'main.lst'))
    run('ca65','-g','-D',define,'vbxe_lib.asm','-o',str(out/'vbxe_lib.o'))
    run('ld65','-C','vbxe-plus.cfg','-S','0x2800','-D','start=0x2800','-Ln',str(out/'symbols.lbl'),'--dbgfile',str(out/'program.dbg'),'-m',str(out/'program.map'),str(out/'main.o'),str(out/'vbxe_lib.o'),'-o',str(out/'PRISM.XEX'))
    for suffix,filename,base,size in [('TRANS','TRANS.BIN',0x4000,16384),('UI','UI.BIN',0x5000,12288)]:
        raw=(out/('PRISM.XEX.'+suffix)).read_bytes()
        if raw[:2]==b'\xff\xff':
            pos=0;bank=bytearray(size)
            while pos<len(raw):
                a=int.from_bytes(raw[pos:pos+2],'little');pos+=2
                if a==65535:continue
                z=int.from_bytes(raw[pos:pos+2],'little');pos+=2
                if base<=a<=z<base+size:bank[a-base:z-base+1]=raw[pos:pos+z-a+1]
                pos+=z-a+1
            raw=bank
        (out/filename).write_bytes(raw)
    print(f'Built {out.relative_to(ROOT)}/PRISM.XEX (VBXE ${args.base:04X})')
if __name__=='__main__':main()
