#!/usr/bin/env python3
"""Render the compiled program's modeled VRAM, not an illustrative mockup.
Requires Pillow only for exporting PNG files; integration tests do not.
"""
from machine import Machine,ROOT
from PIL import Image

def render(m,path):
    width=320 if m.get('bitmap_on')==2 else 640
    im=Image.new('P',(width,192));bank=(m.mem.vram[0x803]>>4)&3
    pal=list(m.mem.palette[bank][:48])
    if not any(pal):pal=list((ROOT/'disk/ANSI.PAL').read_bytes())
    im.putpalette(pal+[0]*(768-len(pal)))
    pixels=im.load()
    if m.get('bitmap_on'):
        frame=m.mem.vram[0x20000:0x2f000]
        for y in range(192):
            for x in range(width):
                v=frame[y*320+(x if width==320 else x//2)]
                pixels[x,y]=v if width==320 else (v>>4 if not x&1 else v&15)
    else:
        for r in range(24):
            for c in range(80):
                p=0x7f100+r*160+c*2;ch=m.mem.vram[p];attr=m.mem.vram[p+1]
                for y in range(8):
                    bits=m.mem.vram[ch*8+y]
                    for x in range(8):pixels[c*8+x,r*8+y]=attr&15 if bits&(128>>x) else (attr>>4)&7
    im=im.convert('RGB').resize((640,384),Image.Resampling.NEAREST)
    full=Image.new('RGB',(640,480),'black');full.paste(im,(0,48));full.save(path)

def main():
    out=ROOT/'build';m=Machine();m.set('utf8_mode',1)
    m.feed('\x1b[2J\x1b[2;4H\x1b[1;36mPrismTerm 0.3\x1b[0m\r\n\r\n')
    m.feed('   80-column color terminal for Atari VBXE\r\n\r\n')
    m.feed('   '+''.join(f'\x1b[{30+i}m COLOR ' for i in range(8))+'\x1b[0m\r\n\r\n')
    m.feed('   \x1b[4mReal underline\x1b[0m       \x1b[5mBlinking text\x1b[0m\r\n\r\n')
    m.feed('   UTF-8: café  £  π   ┌──────────────┐\r\n')
    m.feed('                       │ CP437 glyphs │\r\n')
    m.feed('                       └──────────────┘\r\n\r\n')
    m.feed('   START: transfers, scrollback, capture, connections\r\n')
    m.feed('   OPTION: terminal mode, width, fonts\r\n\r\n')
    m.feed('   Model render of the compiled 6502 program.\r\n')
    m.call('cursor_off');m.call('plus_render_dirty');render(m,out/'preview-80.png')
    m=Machine();m.set('term_last_col',39)
    m.feed('\x1b[2J\x1b[2;3H\x1b[1;36mPrismTerm\x1b[0m\r\n\r\n')
    m.feed('  Full-width 40-column display\r\n\r\n  \x1b[4mUnderline\x1b[0m and \x1b[5mblink\x1b[0m\r\n\r\n')
    m.feed('  XMODEM-CRC / YMODEM\r\n  1,024 lines of scrollback\r\n  32 KiB capture buffer\r\n  Eight saved connections\r\n\r\n  START opens the tools menu.\r\n')
    m.call('cursor_off');m.call('plus_render_dirty');render(m,out/'preview-40.png')
    from render_menu_preview import main as menus
    menus()
if __name__=='__main__':main()
