import unittest,random
from machine import Machine
class Display(unittest.TestCase):
    def setUp(self):self.m=Machine()
    def styles(self):
        m=self.m;p=m.addr('cell_styles');return bytes(m.mem[p:p+1920])
    def test_attributes_follow_character_edits(self):
        m=self.m;m.feed('\x1b[4mA\x1b[0mB\x1b[5mC\x1b[0mD\r\x1b[C\x1b[2@')
        self.assertEqual(self.styles()[:7],bytes([1,0,0,0,2,0,0]))
        m.feed('\x1b[2P');self.assertEqual(self.styles()[:4],bytes([1,0,2,0]))
        m.feed('\x1b[2X');self.assertEqual(self.styles()[:4],bytes([1,0,0,0]))
        m.feed('\x1b[2K');self.assertEqual(self.styles()[:80],bytes(80))
    def test_random_regions_move_characters_and_styles(self):
        m=self.m;m.set('bitmap_on',1);rng=random.Random(8128)
        for _ in range(45):
            cells=[bytearray([65+r]*80) for r in range(24)]
            attrs=[bytearray([r%4]*80) for r in range(24)]
            for r in range(24):
                p=0x7f100+r*160;m.mem.vram[p:p+160:2]=cells[r]
                p=m.addr('cell_styles')+r*80;m.mem[p:p+80]=attrs[r]
            top=rng.randrange(23);bot=rng.randrange(top+1,24);n=rng.choice([1,2,5,25,255]);k=min(n,bot-top+1)
            m.set('scroll_top',top);m.set('scroll_bot',bot);up=rng.choice([True,False]);m.set('cursor_flg',0)
            m.call('scroll_rgn_up' if up else 'scroll_rgn_down',a=top,x=bot,y=n)
            if up:
                cells[top:bot+1]=cells[top+k:bot+1]+[bytearray(80) for _ in range(k)]
                attrs[top:bot+1]=attrs[top+k:bot+1]+[bytearray(80) for _ in range(k)]
            else:
                cells[top:bot+1]=[bytearray(80) for _ in range(k)]+cells[top:bot+1-k]
                attrs[top:bot+1]=[bytearray(80) for _ in range(k)]+attrs[top:bot+1-k]
            self.assertEqual(m.screen(),list(map(bytes,cells)))
            self.assertEqual(self.styles(),b''.join(attrs))
    def test_alternate_attributes(self):
        m=self.m;m.feed('\x1b[4;31mMAIN\x1b[0m\x1b[?1049h\x1b[5mALT\x1b[?1049l')
        self.assertEqual(m.screen()[0][:4],b'MAIN');self.assertEqual(self.styles()[:4],b'\x01'*4)
        self.assertEqual(m.get('plus_style'),0)
    def test_clear_does_not_overwrite_vram_sessions(self):
        m=self.m;m.mem.vram[0x7a000:0x7f000]=b'\xa5'*0x5000;m.feed('\x1b[2J')
        self.assertEqual(bytes(m.mem.vram[0x7a000:0x7f000]),b'\xa5'*0x5000)
    def test_cursor_rendered_and_hidden(self):
        m=self.m;m.set('term_last_col',39);m.call('plus_enable_bitmap');m.call('cursor_on');m.call('plus_render_dirty')
        self.assertEqual(bytes(m.mem.vram[0x20000:0x20008]),b'\x07'*8)
        m.feed('\x1b[?25l');m.call('plus_render_dirty');self.assertEqual(bytes(m.mem.vram[0x20000:0x20008]),bytes(8))
    def test_dirty_glyphs_are_bounded(self):
        m=self.m;m.feed('\x1b[4m');m.call('plus_enable_bitmap');m.call('plus_render_dirty');n=m.mem.blits
        m.feed('A');m.call('plus_render_dirty');self.assertLessEqual(m.mem.blits-n,2)
    def test_bright_background_erase(self):
        m=self.m;m.feed('\x1b[104m\x1b[2J');m.call('cursor_off');m.call('plus_render_dirty')
        self.assertEqual(self.styles(),b'\x10'*1920)
        self.assertEqual(bytes(m.mem.vram[0x20000:0x2f000]),b'\xcc'*0xf000)
    def test_dialog_restores_host_state(self):
        m=self.m;m.feed('\x1b[4;31;7mHOST\x1b[5;15r\x1b[?6h\x1b7')
        m.call('cursor_off');cells=bytes(m.mem.vram[0x7f100:]);attrs=self.styles()
        fields=['plus_style','plus_fg','plus_inverse','origin_mode','scroll_top','scroll_bot','row','column']
        before=[m.get(k) for k in fields]
        m.call('plus_dialog_begin');m.feed('DIALOG');m.call('plus_dialog_end');m.call('cursor_off')
        self.assertEqual(bytes(m.mem.vram[0x7f100:]),cells);self.assertEqual(self.styles(),attrs)
        self.assertEqual([m.get(k) for k in fields],before)
    def test_reset_during_receive_batch(self):
        m=self.m;m.set('batch_mode',1);m.feed('\x1b[4mTEXT\x1bcOK')
        self.assertEqual(m.get('batch_mode'),1);self.assertEqual(m.get('bitmap_on'),0)
        self.assertEqual(m.screen()[0][:2],b'OK');self.assertEqual(m.mem.vram[0x805:0x807],b'\x61\x81')
    def test_scrollback_carries_styles(self):
        m=self.m;m.feed('\x1b[4mFIRST\x1b[0m'+('\r\n'*24));m.set('history_view',0)
        m.call('history_draw');self.assertEqual(m.screen()[0][:5],b'FIRST');self.assertEqual(self.styles()[:5],b'\x01'*5)
    def test_bss_initialization(self):
        m=self.m;a=m.addr('__BSS_RUN__');n=m.addr('__BSS_SIZE__');m.mem[a:a+n]=b'\xa5'*n
        m.call('plus_bss_init');self.assertEqual(bytes(m.mem[a:a+n]),bytes(n))
    def test_input_line_length(self):
        m=self.m;m.keys=list(b'123456789012345\x9b');p=m.addr('port_buf');m.mem[p:p+12]=b'Z'*12
        m.mem[m.addr('src_ptr')]=p&255;m.mem[m.addr('src_ptr')+1]=p>>8;m.set('input_limit',7)
        m.handles[0x20]=['K:',0,4];m.call('read_line_vbxe')
        self.assertEqual(bytes(m.mem[p:p+12]),b'1234567\x9bZZZZ')

class PixelReference(unittest.TestCase):
    def test_bitmap_matches_independent_reference(self):
        rng=random.Random(48879)
        for width in (80,40):
            m=Machine();m.set('term_last_col',width-1);m.call('plus_enable_bitmap');m.set('blink_phase',1)
            expected=bytearray(320*192);font=m.mem.vram[:2048];stylebase=m.addr('cell_styles')
            for r in range(24):
                for c in range(width):
                    ch=rng.randrange(256);fg=rng.randrange(16);bg=rng.randrange(16);style=rng.randrange(4)
                    p=0x7f100+r*160+c*2;m.mem.vram[p]=ch;m.mem.vram[p+1]=0x80|((bg&7)<<4)|fg
                    m.mem[stylebase+r*80+c]=style|((bg&8)<<1)
                    for y in range(8):
                        bits=255 if style&1 and y==7 else font[ch*8+y]
                        if style&2:bits=0
                        pixels=[fg if bits&(128>>x) else bg for x in range(8)]
                        if width==80:pixels=[pixels[x]*16+pixels[x+1] for x in range(0,8,2)]
                        offset=(r*8+y)*320+c*(4 if width==80 else 8);expected[offset:offset+len(pixels)]=pixels
            m.call('plus_dirty_all');m.call('plus_render_dirty')
            self.assertEqual(bytes(m.mem.vram[0x20000:0x2f000]),bytes(expected),f'{width}-column pixels')

if __name__=='__main__':unittest.main()
