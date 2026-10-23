import unittest
from machine import Machine
class Sessions(unittest.TestCase):
    def setUp(self):self.m=Machine()
    def test_capture_saved_including_partial_last_page(self):
        m=self.m;data=bytes(range(256))*2+b'TAIL';m.mem.vram[0x70000:0x70000+len(data)]=data
        m.set('capture_size',len(data)&255);m.mem[m.addr('capture_size')+1]=len(data)>>8
        m.dialog_keys=list(b'D:LOG.TXT\rx');m.call('tool_save_capture')
        self.assertEqual(m.files['D:LOG.TXT'],data);self.assertEqual(m.get('capture_size'),0)
    def test_profile_round_trip_preserves_settings(self):
        m=self.m;url=b'N1:TCP://bbs.example:23\x9b';p=m.addr('n_url_buf');m.mem[p:p+len(url)]=url
        m.set('proto_byte',ord('T'));m.set('utf8_mode',1);m.set('active_font',3)
        p=m.addr('login_macro');m.mem[p:p+5]=b'HI^M\0';m.dialog_keys=list(b'3\rMY BBS\rx')
        m.call('tool_save_profile');self.assertEqual(len(m.files['D:VBXE.DIR']),4096)
        p=m.addr('n_url_buf');m.mem[p:p+256]=bytes(256);m.set('utf8_mode',0);m.set('login_macro',0)
        m.dialog_keys=list(b'3\r');m.call('tool_load_profile')
        self.assertEqual(m.get('profile_ready'),1);self.assertEqual(bytes(m.mem[p:p+len(url)]),url)
        self.assertEqual(m.mem[m.addr('profile_rec')+481],1);self.assertEqual(bytes(m.mem[m.addr('login_macro'):m.addr('login_macro')+5]),b'HI^M\0')
    def test_batch_matches_parser_for_mixed_input(self):
        data=b'A'*100+b'\r\n\x1b[31mCOLOR\x1b[4mUNDER\x1b[0m\x1b[5;20Hhi\x1b[2P\x1b[?1049hALT\x1b[?1049l'+b'B'*260
        m=self.m;ref=Machine();ref.feed(data);m.set('device_type',1);m.set('n_unit',1);m.incoming+=data
        while m.incoming:m.call('recv_from_device')
        m.call('cursor_off');ref.call('cursor_off')
        self.assertEqual(bytes(m.mem.vram[0x7f100:]),bytes(ref.mem.vram[0x7f100:]))
        p=m.addr('cell_styles');q=ref.addr('cell_styles');self.assertEqual(bytes(m.mem[p:p+1920]),bytes(ref.mem[q:q+1920]))
    def test_csi_parameter_limit(self):
        m=self.m;m.feed(b'\x1b['+b'1;'*30+b'4mX');self.assertLessEqual(m.get('plus_param_n'),16)
    def test_cold_start_reaches_device_selection(self):
        m=self.m;a=m.addr('__BSS_RUN__');n=m.addr('__BSS_SIZE__');m.mem[a:a+n]=b'\xa5'*n
        m.cpu.pc=m.addr('start');target=m.addr('device_select')
        for i in range(4000000):
            if m.cpu.pc==target:break
            m.step()
        else:self.fail(f'startup did not reach selection: ${m.cpu.pc:04x}')
        self.assertEqual(m.get('fonts_ready'),1);self.assertEqual(m.mem.bank,255);self.assertEqual(m.get('term_last_col'),79)

class Packaging(unittest.TestCase):
    def test_d700_build(self):
        from machine import ROOT
        m=Machine(build_dir=ROOT/'build-d700');m.feed('\x1b[4mD700');m.call('plus_render_dirty')
        self.assertEqual(m.screen()[0][:4],b'D700');self.assertEqual(m.mem.base,0xd700)
    def test_all_font_paths_exist(self):
        from machine import ROOT
        import re
        source=(ROOT/'ANSIVBXE_ca65.asm').read_text()
        for filename in re.findall(r'"D:([^"\n]+\.FNT)"',source):self.assertTrue((ROOT/'disk'/filename).exists(),filename)
    def test_keyboard_application_cursor_mode(self):
        m=Machine();m.set('app_cursor',1);m.set('CH1',0);m.set('KEYDEL',0)
        m.mem[m.addr('KBCODE')]=142  # CTRL-minus, cursor up
        c=m.cpu;old=c.sp;c.stPushWord(0x600);c.stPush(c.p);c.stPush(c.a);c.pc=m.addr('kbd_irq')
        for _ in range(10000):
            if c.pc==0x600:break
            m.step()
        else:self.fail('keyboard IRQ did not return')
        self.assertEqual(c.sp,old);m.call('check_sendbuf');self.assertEqual(m.out,b'\x1bOA')


class Menus(unittest.TestCase):
    def test_tools_menu_dismiss_restores_cells_and_styles(self):
        m=Machine();m.set('ui_ready',0);m.feed('\x1b[4m'+('HOST '*60)+'\x1b[7;8H');m.call('cursor_off')
        before=bytes(m.mem.vram[0x7f100:]);p=m.addr('cell_styles');styles=bytes(m.mem[p:p+1920]);m.call('cursor_on')
        keys=[15]*16+[12];original=m.step;ready=m.addr('menu_key_ready')
        def step():
            pc=m.cpu.pc
            if keys and m.mem[pc]==0xa5 and m.mem[pc+1]==ready and not m.get('menu_key_ready'):
                m.set('menu_key',keys.pop(0));m.set('menu_key_ready',1)
            original()
        m.step=step;m.call('plus_tools',limit=12000000);m.call('cursor_off')
        self.assertEqual(keys,[]);self.assertEqual(m.get('utf8_mode'),1)
        self.assertEqual(bytes(m.mem.vram[0x7f100:]),before);self.assertEqual(bytes(m.mem[p:p+1920]),styles)
    def test_dialog_text_uses_atascii_font_mapping(self):
        m=Machine();m.set('atascii_mode',1);m.set('term_last_col',39)
        m.call('plus_dialog_begin');m.feed('FILE\r\nNEXT')
        self.assertEqual(m.screen()[0][:4],bytes(c-32 for c in b'FILE'))
        self.assertEqual(m.screen()[1][:4],bytes(c-32 for c in b'NEXT'))
        m.call('plus_dialog_end');self.assertEqual(m.get('atascii_mode'),1)
        self.assertEqual(m.get('dialog_font_atascii'),0)

if __name__=='__main__':unittest.main()

