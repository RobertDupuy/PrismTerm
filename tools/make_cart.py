#!/usr/bin/env python3
"""Build a diskless, full-font 4 MiB Flash MegaCart (CAR type 63)."""
import argparse
import hashlib
import json
import pathlib
import re
import struct
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
SIZE = 4 * 1024 * 1024
BOOT_BANK = 254
HEADER = 0x20000
GLYPHS = 0x22400
FONTS = ['IBMPC','ATARIPC','ASCPRINP','BALOONPC','BOZOPC','BZZZ2PC',
         'CASUALGT','COMPUTPC','CURSIVEP','HEROPC','NEWSLTRP','PREPPIEP','SHADOWPC']

def segments(data):
    pos = 0
    while pos < len(data):
        if pos + 2 > len(data): raise ValueError('Truncated XEX header')
        start = int.from_bytes(data[pos:pos+2], 'little'); pos += 2
        if start == 65535: continue
        if pos + 2 > len(data): raise ValueError('Truncated XEX segment')
        end = int.from_bytes(data[pos:pos+2], 'little'); pos += 2
        size = end - start + 1
        if size <= 0 or pos + size > len(data): raise ValueError('Bad XEX extent')
        yield start, data[pos:pos+size]
        pos += size

def build(out, base):
    rom = bytearray([255]) * SIZE
    manifest = []
    def put(name, offset, data):
        end = offset + len(data)
        if end > SIZE or any(offset < e['offset']+e['bytes'] and end > e['offset'] for e in manifest):
            raise ValueError(f'Overlapping/out-of-range cartridge extent: {name}')
        rom[offset:end] = data
        manifest.append(dict(name=name,offset=offset,bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
    tables = {}
    pos = 0
    def payload(name, dest, data, table):
        nonlocal pos
        pos = (pos+255) & ~255
        put(name, pos, data)
        tables.setdefault(table, []).append((pos,dest,len(data)))
        pos += len(data)
    boot = (ROOT/'tools/boot-template.atr').read_bytes()[16:400]
    assert boot[0:6] == bytes.fromhex('4d030007e007'), 'Unexpected MyDOS boot layout'
    payload('MyDOS boot and init',0x700,boot,'dos')
    payload('MyDOS DOS.SYS',0xb0a,(ROOT/'disk/DOS.SYS').read_bytes(),'dos')
    serial = list(segments((ROOT/'disk/RS232.AR0').read_bytes()))
    assert serial[0][0] == 0x3000 and serial[-1] == (0x2e0,b'\x02\x35')
    payload('Original relocatable R: handler',0x3000,serial[0][1],'serial')
    runtime = None
    for addr, data in segments((out/'PRISM.XEX').read_bytes()):
        if addr == 0x2500:
            runtime=data;continue
        if addr == 0x2e0:
            assert data == b'\x00\x28';continue
        payload(f'Program RAM ${addr:04X}',addr,data,'program')
    assert runtime and len(runtime) <= 768
    assert pos <= 0x10000, 'Program and DOS overlap protocol data'
    put('Protocol and Unicode engine',0x10000,(out/'TRANS.BIN').read_bytes())
    put('Local menus',0x14000,(out/'UI.BIN').read_bytes())
    for i,name in enumerate(FONTS):
        b=(ROOT/'disk'/f'{name}.FNT').read_bytes();assert len(b)==2048
        put(name,0x18000+i*2048,b)
    put('ANSI palette',0x1e800,(ROOT/'disk/ANSI.PAL').read_bytes())
    atr=(ROOT/'fonts/UNICODE.ATR').read_bytes()
    assert atr[:2]==b'\x96\x02' and int.from_bytes(atr[4:6],'little')==256
    raw=atr[16:];prefix=384+34*256
    assert raw[:4]==b'UFN1'
    put('UFN1 header and complete page directory',HEADER,raw[:prefix])
    records=raw[prefix:];assert len(records)%32==0
    compact=bytearray()
    for i in range(0,len(records),32):
        if any(records[i+17:i+32]):raise ValueError('Nonzero reserved glyph bytes cannot be omitted')
        compact+=records[i:i+17]
    put('Complete Unicode glyphs (17 bytes per record)',GLYPHS,compact)
    symbols={}
    for line in (out/'symbols.lbl').read_text().splitlines():
        match=re.fullmatch(r'al ([0-9A-Fa-f]+) \.(.*)',line)
        if match:symbols[match[2]]=int(match[1],16)
    needed=['cart_return_bank','cart_nmi','cart_src','cart_dest','cart_len','cart_reboot','cart_launch','cart_read']
    (out/'cart-symbols.inc').write_text(f'VBXE_BASE = ${base:04x}\n'+''.join(f'{n} = ${symbols[n]:04x}\n' for n in needed))
    lines=[]
    for name, entries in tables.items():
        lines.append(name+'_table:')
        for source,dest,size in entries:
            lines += [f'.byte ${source&255:02x}, ${(source>>8)&255:02x}, ${source>>16:02x}',f'.word ${dest:04x}, ${size:04x}']
        lines.append('.res 7,0')
    (out/'cart-assets.inc').write_text('\n'.join(lines)+'\n')
    (out/'cart-runtime.bin').write_bytes(runtime)
    (out/'boot.cfg').write_text('MEMORY { ROM: start=$8000, size=$4000, file=%O, fill=yes, fillval=$FF; }\nSEGMENTS { BOOT: load=ROM, type=ro; }\n')
    subprocess.run(['ca65','-g','-I',str(out),'--bin-include-dir',str(out),str(ROOT/'cart/boot.s'),'-o',str(out/'boot.o'),'-l',str(out/'boot.lst')],check=True)
    subprocess.run(['ld65','-C',str(out/'boot.cfg'),'-Ln',str(out/'boot.lbl'),str(out/'boot.o'),'-o',str(out/'boot.bin')],check=True)
    bootrom=(out/'boot.bin').read_bytes();assert len(bootrom)==16384
    put('Boot ROM and low-RAM gateway',BOOT_BANK*16384,bootrom)
    assert max(e['offset']+e['bytes'] for e in manifest[:-1]) <= BOOT_BANK*16384
    header=b'CART'+struct.pack('>III',63,sum(rom)&0xffffffff,0)
    name=f'PRISMTERM-{base:04X}'
    (out/(name+'.ROM')).write_bytes(rom)
    (out/(name+'.CAR')).write_bytes(header+rom)
    report=dict(format='Flash MegaCart 4 MiB',car_type=63,rom_bytes=SIZE,
                occupied_bytes=sum(e['bytes'] for e in manifest),glyph_records=len(records)//32,
                original_font_bytes=len(atr),packed_glyph_bytes=len(compact),
                font_sectors=json.loads((ROOT/'fonts/UNICODE.json').read_text())['sectors'],
                vbxe_base=base,resources=manifest,program_tables=tables,
                sha256=hashlib.sha256(header+rom).hexdigest())
    (out/'cartridge.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f'{name}.CAR: {SIZE:,} bytes ROM, {report["occupied_bytes"]:,} occupied; complete font set retained')
    return report

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--build',default='build-cart')
    ap.add_argument('--base',type=lambda s:int(s,0),default=0xd600,choices=[0xd600,0xd700])
    args=ap.parse_args();build(ROOT/args.build,args.base)
