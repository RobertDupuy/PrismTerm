#!/usr/bin/env python3
"""Unicode layout companion for PrismTerm, using Pango/HarfBuzz/FriBidi.

Run on the SSH host: python3 unicode_bridge.py --shell /bin/bash
The Atari receives only CRC-checked, bounded monochrome cell DCS
updates. The host never asks the Atari to execute commands or access files.
Ctrl-] exits the companion. Run file transfers from the outer host shell.
"""
from __future__ import annotations
import argparse,binascii,codecs,ctypes,ctypes.util,io,os,pathlib,select,struct,sys,time,unicodedata
import pyte,pyte.modes as mo,regex
from wcwidth import wcswidth,wcwidth
import cairocffi as cairo
import pangocffi as pango
import pangocairocffi as pc
from PIL import Image
ROOT=pathlib.Path(__file__).resolve().parent
PALETTE=[(0,0,0),(170,0,0),(0,170,0),(170,170,0),(0,0,170),(170,0,170),(0,170,170),(170,170,170),(85,85,85),(255,85,85),(85,255,85),(255,255,85),(85,85,255),(255,85,255),(85,255,255),(255,255,255)]
COLOR_NAMES=['black','red','green','brown','blue','magenta','cyan','white','brightblack','brightred','brightgreen','brightbrown','brightblue','brightmagenta','brightcyan','brightwhite']
def register_fonts():
 fc=ctypes.CDLL(ctypes.util.find_library('fontconfig'))
 fc.FcConfigGetCurrent.restype=ctypes.c_void_p
 fc.FcConfigAppFontAddDir.argtypes=[ctypes.c_void_p,ctypes.c_char_p]
 fc.FcConfigBuildFonts.argtypes=[ctypes.c_void_p]
 cfg=fc.FcConfigGetCurrent();fc.FcConfigAppFontAddDir(cfg,os.fsencode(ROOT/'fonts'));fc.FcConfigBuildFonts(cfg)
def color_index(value,default,bold=False):
 if value=='bfightmagenta':value='brightmagenta' # pyte 0.8.2 BG_AIXTERM typo
 if value=='default':n=default
 elif value in COLOR_NAMES:n=COLOR_NAMES.index(value)
 elif len(value)==6:
  try:
   rgb=tuple(int(value[i:i+2],16) for i in (0,2,4));n=min(range(16),key=lambda n:sum((a-b)**2 for a,b in zip(rgb,PALETTE[n])))
  except ValueError:n=default
 else:n=default
 return n+8 if bold and n<8 else n
def cell_packet(row,col,attr,style,bits):
 body=bytes([row,col,attr,style])+bytes(bits)
 if len(body)!=12 or not 0<=row<24 or not 0<=col<80:raise ValueError('invalid cell')
 body+=binascii.crc_hqx(body,0).to_bytes(2,'big')
 return b'\x1bP=U'+body.hex().encode()+b'\x1b\\'
class UnicodeScreen(pyte.Screen):
 """Pyte ANSI core with streaming extended-grapheme cells and alternate screen."""
 def __init__(self,columns=80,lines=12):
  self.reply=lambda s:None;self.cluster=None;self.alt=None
  super().__init__(columns,lines)
 def write_process_input(self,data):self.reply(data.encode())
 def set_mode(self,*modes,**kw):
  if kw.get('private') and any(m in (47,1047,1049) for m in modes) and self.alt is None:
   import copy
   self.alt=(copy.deepcopy(self.buffer),copy.deepcopy(self.cursor),self.margins)
   self.erase_in_display(2);self.cursor_position();self.cluster=None
  super().set_mode(*modes,**kw)
 def reset_mode(self,*modes,**kw):
  if kw.get('private') and any(m in (47,1047,1049) for m in modes) and self.alt is not None:
   self.buffer,self.cursor,self.margins=self.alt;self.alt=None;self.cluster=None;self.dirty.update(range(self.lines))
  super().reset_mode(*modes,**kw)
 def draw(self,data):
  data=data.translate(self.g1_charset if self.charset else self.g0_charset)
  for ch in data:
   if self.cluster:
    r,c,oldwidth,expected=self.cluster
    old=self.buffer[r][c]
    if r==self.cursor.y and expected==self.cursor.x and regex.fullmatch(r'\X',old.data+ch):
     combined=old.data+ch;newwidth=max(1,wcswidth(combined));newwidth=min(newwidth,self.columns-c)
     self.buffer[r][c]=old._replace(data=combined)
     for x in range(c+1,c+newwidth):self.buffer[r][x]=old._replace(data='')
     for x in range(c+newwidth,c+oldwidth):self.buffer[r][x]=old._replace(data=' ')
     self.cursor.x=min(self.columns,self.cursor.x+newwidth-oldwidth)
     self.cluster=(r,c,newwidth,self.cursor.x);self.dirty.add(r);continue
   width=wcwidth(ch)
   if width<0:continue
   if width==0:
    # Preserve isolated format controls without printing a box.
    if self.cursor.x:
     c=min(self.cursor.x-1,self.columns-1);old=self.buffer[self.cursor.y][c]
     self.buffer[self.cursor.y][c]=old._replace(data=old.data+ch);self.dirty.add(self.cursor.y)
    continue
   if self.cursor.x+width>self.columns:
    if mo.DECAWM in self.mode:self.carriage_return();self.linefeed()
    else:self.cursor.x=max(0,self.columns-width)
   if mo.IRM in self.mode:self.insert_characters(width)
   row,col=self.cursor.y,self.cursor.x
   self.buffer[row][col]=self.cursor.attrs._replace(data=ch)
   for x in range(1,width):self.buffer[row][col+x]=self.cursor.attrs._replace(data='')
   self.cursor.x=min(self.columns,col+width);self.cluster=(row,col,width,self.cursor.x);self.dirty.add(row)
class Renderer:
 def __init__(self,screen):
  register_fonts();self.screen=screen;self.previous={};self.last_cursor=None
  self.font=pango.FontDescription();self.font.family='Noto Sans Mono';self.font.set_absolute_size(14*1024)
  self.qpalette=Image.new('P',(1,1));self.qpalette.putpalette([v for rgb in PALETTE for v in rgb]+[0]*(768-48))
  self.image=Image.new('RGB',(640,192))
 def row(self,r,cursor=True):
  screen=self.screen;cells=[screen.buffer[r][c] for c in range(80)]
  surface=cairo.ImageSurface(cairo.FORMAT_ARGB32,640,16);ctx=cairo.Context(surface)
  options=cairo.FontOptions();options.set_antialias(cairo.ANTIALIAS_NONE);options.set_hint_style(cairo.HINT_STYLE_FULL);ctx.set_font_options(options)
  # Shape the entire row with Pango: bidi ordering and joining need context
  # across individual cells. Pango's font fallback and OpenType shaping are
  # authoritative; the Atari only paints the resulting pixels.
  import html
  markup=[];byte_offsets=[];pos=0;run_attrs=None;run_text=[]
  for cell in cells:
   byte_offsets.append(pos)
   if not cell.data:continue
   fg=color_index(cell.fg,7,cell.bold);bg=color_index(cell.bg,0)
   if cell.reverse:fg,bg=bg,fg
   attrs=f'foreground="#{PALETTE[fg][0]:02x}{PALETTE[fg][1]:02x}{PALETTE[fg][2]:02x}" background="#{PALETTE[bg][0]:02x}{PALETTE[bg][1]:02x}{PALETTE[bg][2]:02x}"'
   if regex.search(r'\p{Extended_Pictographic}|\p{Regional_Indicator}',cell.data):attrs+=' font_family="Noto Color Emoji"'
   if cell.bold:attrs+=' weight="bold"'
   if cell.italics:attrs+=' style="italic"'
   if cell.underscore:attrs+=' underline="single"'
   if attrs!=run_attrs and run_text:
    markup.append(f'<span {run_attrs}>{html.escape("".join(run_text))}</span>');run_text=[]
   run_attrs=attrs;run_text.append(cell.data);pos+=len(cell.data.encode())
  if run_text:markup.append(f'<span {run_attrs}>{html.escape("".join(run_text))}</span>')
  layout=pc.create_layout(ctx);layout.font_description=self.font;layout.apply_markup(''.join(markup));layout.width=-1
  ink,logical=layout.get_extents();width=max(1,logical.width/1024);height=max(1,ink.height/1024)
  # A fixed grid remains 640 pixels wide. Fit horizontal advances to the
  # terminal line, and fit tall fallback fonts without clipping their marks.
  sx=min(1,640/width);sy=min(1,16/height)
  baseline=layout.get_baseline()/1024
  ctx.scale(sx,sy);ctx.move_to(0,13/sy-baseline);pc.show_layout(ctx,layout)
  if cursor and r==screen.cursor.y and not screen.cursor.hidden:
   rect=pango.ffi.new('PangoRectangle *');index=byte_offsets[min(screen.cursor.x,79)]
   pango.pango.pango_layout_index_to_pos(layout.pointer,index,rect)
   x=max(0,min(632,rect.x/1024*sx));ctx.identity_matrix();ctx.set_source_rgb(1,1,1);ctx.rectangle(x,14,8,2);ctx.fill()
  surface.flush();raw=bytes(surface.get_data());im=Image.frombytes('RGBA',(640,16),raw,'raw','BGRA',surface.get_stride()).convert('RGB')
  return im
 def updates(self,force=False,cursor=True):
  rows=set(range(12)) if force else set(self.screen.dirty)
  current=(self.screen.cursor.x,self.screen.cursor.y,self.screen.cursor.hidden)
  if cursor and current!=self.last_cursor:
   rows.add(current[1])
   if self.last_cursor:rows.add(self.last_cursor[1])
  self.last_cursor=current;out=bytearray(b"\x1b[0m\x1b[2J\x1b[H\x1b[?25l" if force else b"")
  for r in sorted(rows):
   if not 0<=r<12:continue
   im=self.row(r,cursor=cursor);self.image.paste(im,(0,r*16))
   # Quantize each 8x8 tile to its two most used ANSI colors. This includes
   # multicolor emoji while respecting the Atari cell color format.
   quantized=im.quantize(palette=self.qpalette,dither=Image.Dither.NONE)
   indexed=list(quantized.get_flattened_data() if hasattr(quantized,'get_flattened_data') else quantized.getdata())
   for half in range(2):
    for col in range(80):
     colors=[]
     for y in range(half*8,half*8+8):
      for x in range(col*8,col*8+8):
       colors.append(indexed[y*640+x] if indexed[y*640+x]<16 else 0)
     from collections import Counter
     order=Counter(colors).most_common();bg=order[0][0];fg=order[1][0] if len(order)>1 else bg
     bits=[]
     for y in range(8):
      value=0
      for x in range(8):
       n=colors[y*8+x];rgb=PALETTE[n]
       on=sum((a-b)**2 for a,b in zip(rgb,PALETTE[fg]))<sum((a-b)**2 for a,b in zip(rgb,PALETTE[bg]))
       value=(value<<1)|on
      bits.append(value)
     # ANSI.PAL uses ANSI color order directly, including red=1 and blue=4.
     attr=fg|((bg&7)<<4)|128;style=(bg&8)<<1
     cell=(attr,style,bytes(bits));key=(r*2+half,col)
     if force or self.previous.get(key)!=cell:
      if not (force and cell==(128,0,bytes(8))):out+=cell_packet(*key,*cell)
      self.previous[key]=cell
  self.screen.dirty.clear();return bytes(out)
SAMPLE='''\x1b[1;36mPRISMTERM — Unicode layout\x1b[0m\r
Greek: Ελληνικά    Cyrillic: Привет мир\r
CJK: 中文 日本語 한국어\r
Arabic: العربية مرحبا بالعالم\r
Hebrew: שלום עולם\r
Devanagari: नमस्ते दुनिया\r
Bengali: বাংলা ভাষা\r
Combining: é Å ñ    Math: ∑ ∞ √ ≠\r
Emoji sequences: 👩‍💻 👨‍👩‍👧‍👦 🇺🇸 ❤️\r
16-pixel glyphs, 80 columns, 12 rows\r
'''
def demo(path):
 s=UnicodeScreen();pyte.Stream(s).feed(SAMPLE);r=Renderer(s);wire=r.updates(force=True,cursor=False)
 path=pathlib.Path(path);path.with_suffix('.dcs').write_bytes(wire);r.image.resize((1280,384),Image.Resampling.NEAREST).save(path.with_suffix('.png'));return s,r,wire
def run(shell):
 import pty,fcntl,termios,tty
 pid,fd=pty.fork()
 if pid==0:
  os.environ['TERM']='xterm-256color';os.environ.pop('LC_ALL',None);os.environ['LC_CTYPE']='C.UTF-8';os.environ.setdefault('LANG','C.UTF-8');os.execvp(shell,[shell,'-i'])
 fcntl.ioctl(fd,termios.TIOCSWINSZ,struct.pack('HHHH',12,80,0,0))
 screen=UnicodeScreen();screen.reply=lambda data:os.write(fd,data);stream=pyte.Stream(screen);renderer=Renderer(screen)
 decoder=codecs.getincrementaldecoder('utf-8')('replace');old=None;last=0;input_open=True
 if os.isatty(0):old=termios.tcgetattr(0);tty.setraw(0)
 try:
  os.write(1,b'\x1b[2J\x1b[?25l');os.write(1,renderer.updates(force=True))
  while True:
   readable=select.select(([0] if input_open else [])+[fd],[],[],.05)[0]
   if 0 in readable:
    data=os.read(0,4096)
    if not data:input_open=False
    elif b'\x1d' in data:break
    else:os.write(fd,data)
   if fd in readable:
    try:data=os.read(fd,65536)
    except OSError:break
    if not data:break
    stream.feed(decoder.decode(data))
   if time.monotonic()-last>.1:
    wire=renderer.updates()
    if wire:os.write(1,wire)
    last=time.monotonic()
 finally:
  if old:termios.tcsetattr(0,termios.TCSADRAIN,old)
  try:
   os.write(1,renderer.updates())
   os.write(1,b'\x1b[?25h\x1b[0m\x1b[2J\x1b[H')
  finally:
   os.close(fd)
   try:os.waitpid(pid,os.WNOHANG)
   except ChildProcessError:pass
if __name__=='__main__':
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--shell',default='/bin/bash');ap.add_argument('--demo',type=pathlib.Path);a=ap.parse_args()
 if a.demo:demo(a.demo)
 else:run(a.shell)
