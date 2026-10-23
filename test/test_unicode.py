import unittest,binascii,random,pathlib
from machine import Machine,ROOT

def packet(row,col,bits,attr=0x8f,style=0):
 b=bytes([row,col,attr,style])+bytes(bits);return b'\x1bP=U'+(b+binascii.crc_hqx(b,0).to_bytes(2,'big')).hex().encode()+b'\x1b\\'
def raw(m,r,c):return bytes(m.mem.vram[0x8000+r*640+c*8:0x8000+r*640+c*8+8])
class Unicode(unittest.TestCase):
 def setup(self):
  m=Machine();m.set('utf8_mode',1);return m
 def test_scalar_planes_and_widths(self):
  m=self.setup();m.feed('Ж中🙂𠀀')
  self.assertEqual(m.get('column'),7);self.assertEqual(m.get('u_active'),1)
  self.assertNotEqual(raw(m,0,0),raw(m,0,1));self.assertNotEqual(raw(m,0,3),raw(m,0,5))
 def test_d700_bank_and_font_rendering(self):
  m=Machine(build_dir=ROOT/'build-d700');m.set('utf8_mode',1);m.feed('Ж中🙂')
  self.assertEqual((m.get('column'),m.get('u_active'),m.mem.bank_b),(5,1,0))
  self.assertNotEqual(raw(m,0,1),bytes(8));m.call('plus_render_dirty')
  self.assertEqual(m.mem.base,0xd700)
 def test_cache_owns_pixels_not_glyph_indices(self):
  m=self.setup();m.feed('Ж');before=raw(m,0,0)
  # U+0436 and U+0537 have the same direct cache hash.
  m.feed('\u0537');self.assertEqual(raw(m,0,0),before)
 def test_combining_does_not_advance_and_preserves_base(self):
  m=self.setup();m.feed('A');base=m.mem.vram[ord('A')*8:ord('A')*8+8]
  m.feed('\u0301');self.assertEqual(m.get('column'),1)
  after=raw(m,0,0);self.assertTrue(all((x&y)==x for x,y in zip(base,after)));self.assertNotEqual(after,bytes(base))
 def test_wide_wrap(self):
  m=self.setup();m.feed('\x1b[1;80H中');self.assertEqual((m.get('row'),m.get('column')),(1,2))
 def test_glyph_disk_read_cache(self):
  m=self.setup();reads=[];original=m.sio
  def sio():
   if m.mem[0x300]==0x31:reads.append(m.mem[0x30a]|m.mem[0x30b]<<8)
   original()
  m.sio=sio;m.feed('Ж');n=len(reads);m.feed('ЖЖЖ');self.assertEqual(len(reads),n)
  m.feed('З');self.assertLessEqual(len(reads),n+1)
 def test_missing_font_is_explicit_replacement(self):
  m=self.setup();m.font_atr=bytes(1024);m.feed('ЖЖ')
  self.assertEqual(m.get('u_disk_ok'),2);self.assertEqual(raw(m,0,0),raw(m,0,1));self.assertNotEqual(raw(m,0,0),bytes(8))
 def test_unicode_insert_delete_and_erase(self):
  m=self.setup();m.feed('Ж中🙂');a=[raw(m,0,c) for c in range(5)]
  m.feed('\r\x1b[2@');self.assertEqual([raw(m,0,c) for c in range(2,7)],a)
  m.feed('\x1b[2P');self.assertEqual([raw(m,0,c) for c in range(5)],a)
  m.feed('\x1b[2X');p=m.addr('cell_styles');self.assertFalse(m.mem[p]&128);self.assertFalse(m.mem[p+1]&128)
 def test_scroll_both_directions(self):
  m=self.setup()
  for row,ch in enumerate(('Ж','中','🙂','א','क'),3):m.feed(f'\x1b[{row+1};1H'+ch)
  before=[raw(m,r,0) for r in range(3,8)]
  m.feed('\x1b[4;8r\x1b[1S');self.assertEqual([raw(m,r,0) for r in range(3,7)],before[1:])
  m.feed('\x1b[1T');self.assertEqual([raw(m,r,0) for r in range(4,8)],before[1:])
 def test_alternate_and_dialog_restore_pixels(self):
  m=self.setup();m.feed('Ж中🙂');before=bytes(m.mem.vram[0x8000:0xbc00])
  m.feed('\x1b[?1049h別\x1b[?1049l');self.assertEqual(bytes(m.mem.vram[0x8000:0xbc00]),before)
  m.call('plus_dialog_begin');m.set('utf8_mode',1);m.feed('別\n'*26);m.call('plus_dialog_end')
  self.assertEqual(bytes(m.mem.vram[0x8000:0xbc00]),before)
 def test_history_restores_unicode(self):
  m=self.setup();m.feed('Ж\r\n');glyph=raw(m,0,0);m.feed('\r\n'*23)
  self.assertEqual(m.get('history_count'),1);m.call('plus_dialog_begin');m.call('history_draw')
  self.assertEqual(raw(m,0,0),glyph);self.assertTrue(m.mem[m.addr('cell_styles')]&128)
 def test_ris_reenables_unicode_cleanly(self):
  m=self.setup();m.feed('Ж\x1bc');self.assertEqual(m.get('u_active'),0)
  m.feed('\x1b%G中');self.assertEqual(m.get('u_active'),1);self.assertEqual(m.get('bitmap_on'),1)
 def test_bounded_dcs_and_crc(self):
  m=Machine();bits=bytes([0xaa,0x55]*4);good=packet(2,4,bits);bad=good[:-6]+b'0000'+good[-2:]
  m.feed(bad);self.assertEqual(m.get('u_active'),0)
  m.feed(b'\x1bP=U'+b'F'*5000+b'\x1b\\');self.assertEqual(m.get('u_active'),0)
  m.feed(good);self.assertEqual(raw(m,2,4),bits);self.assertTrue(m.mem[m.addr('cell_styles')+2*80+4]&128)
 def test_valid_crc_cannot_write_outside_screen(self):
  m=Machine()
  for r,c in [(24,0),(0,80),(255,255)]:m.feed(packet(r,c,bytes([255])*8))
  self.assertEqual(m.get('u_active'),0)
  self.assertEqual(bytes(m.mem.vram[0x8000:0xbc00]),bytes(15360))
 def test_dcs_pixel_reference_40_and_80(self):
  for width in (39,79):
   m=Machine();m.set('term_last_col',width);rng=random.Random(412);entries=[]
   for r,c in [(0,0),(0,31),(7,9),(17,39),(23,2)]:
    bits=bytes(rng.randrange(256) for _ in range(8));m.feed(packet(r,c,bits,0x9e));entries.append((r,c,bits))
   m.set('cursor_hidden',1);m.call('plus_render_dirty')
   for r,c,bits in entries:
    for y,v in enumerate(bits):
     for x in range(8):
      off=0x20000+(r*8+y)*320+c*(8 if width==39 else 4)+(x if width==39 else x//2)
      byte=m.mem.vram[off];actual=byte if width==39 else (byte>>4 if x%2==0 else byte&15)
      self.assertEqual(actual,14 if v&(128>>x) else 1,(r,c,x,y,width))
 def test_bank_irq_round_trip(self):
  m=Machine();m.mem[m.addr('proto_saved_key'):m.addr('proto_saved_key')+2]=m.addr('kbd_irq').to_bytes(2,'little')
  m.set('proto_bank',129);m.mem[m.mem.base+0x5d]=129;m.set('menu_active',1);m.mem[m.addr('KBCODE')]=28
  c=m.cpu;c.a=123;c.x=45;c.y=67;c.p=0x24;sp=c.sp;c.stPushWord(0x600);c.stPush(c.p);c.stPush(c.a);c.pc=m.addr('proto_key_irq')
  for _ in range(10000):
   if c.pc==0x600:break
   m.step()
  self.assertEqual((c.pc,c.sp,c.a,c.x,c.y,m.mem.bank_b),(0x600,sp,123,45,67,129))
  self.assertEqual(m.get('menu_key'),28)
if __name__=='__main__':unittest.main()
