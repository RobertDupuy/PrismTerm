#!/usr/bin/env python3
"""Reproduce Unicode previews from the compiled Atari program's modeled VRAM."""
import sys
from machine import Machine,ROOT
from render_preview import render
sys.path.insert(0,str(ROOT/'bridge'))
from unicode_bridge import demo

def main():
 out=ROOT/'build';out.mkdir(exist_ok=True)
 m=Machine();m.set('utf8_mode',1)
 m.feed('\x1b[1;36mPrismTerm 0.2 - native Unicode\x1b[0m\r\n\r\n')
 m.feed('80 columns x 24 rows; cached bitmap glyphs\r\n\r\n')
 m.feed('Latin: café Ångström   Greek: Ελληνικά\r\n\r\n')
 m.feed('Cyrillic: Привет мир\r\n\r\n')
 m.feed('CJK: 中文 日本語 한국어\r\n\r\n')
 m.feed('Supplementary: 𠀀 𠀁 🙂\r\n\r\n')
 m.feed('Combining: A\u0301 e\u0308 n\u0303\r\n\r\n')
 m.feed('ZMODEM / Kermit / XMODEM-CRC / YMODEM\r\n\r\n')
 m.feed('Complex scripts: run the host layout companion.\r\n')
 m.set('cursor_hidden',1);m.call('plus_render_dirty');render(m,out/'preview-unicode-native.png')
 _,_,wire=demo(out/'unicode-layout')
 m=Machine();cycles=m.feed(wire);m.set('cursor_hidden',1);cycles+=m.call('plus_render_dirty')
 render(m,out/'preview-unicode-companion.png')
 print(f'Companion sample: {len(wire):,} wire bytes, {cycles:,} modeled 6502 cycles')
if __name__=='__main__':main()
