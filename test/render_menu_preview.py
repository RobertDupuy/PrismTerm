#!/usr/bin/env python3
"""Actual compiled Procomm-style menu screens; no illustrative UI mockups."""
from machine import Machine,ROOT
from render_preview import render
from ui_driver import MenuDriver

def main():
    out=ROOT/'build';m=Machine();m.mem.vram[:2048]=(ROOT/'disk/ATARIPC.FNT').read_bytes();m.set('active_font',4);m.call('plus_font_changed')
    m.set('dev_ready',0);ui=MenuDriver(m)
    ui.press('=\r\rVertrauen\r==\rvert.synchro.net\rS1\rX\x1b')
    m.feed('\x1b[2J\x1b[5;7H\x1b[1;36mPRISMTERM\x1b[0m\r\n\r\n      Your Atari. The whole world.\r\n\r\n      Full 24-line BBS display returns when menus close.\r\n\r\n      This is the compiled terminal, rendered from modeled VBXE RAM.')
    m.call('cursor_off');m.call('plus_render_dirty');m.set('dev_ready',1);m.set('serial_baud_code',10)
    ui=MenuDriver(m);render(m,out/'preview-menu.png')
    ui.press('\r');render(m,out/'preview-connections.png');ui.press('\x1bD');render(m,out/'preview-display.png')
    ui.press('T\r=\rD:ART.ANS\r');render(m,out/'preview-transfer.png')
    ui.press('\x1bD=\r');render(m,out/'preview-menu-40.png');ui.press('\x1b')
    print('Rendered five compiled menu screens.')

if __name__=='__main__':main()
