import pathlib,tempfile,unittest
from machine import Machine,ROOT
from ui_driver import MenuDriver
from test_features import packet
from peer_process import ProcessPeer,DEPS

class LocalUI(unittest.TestCase):
    def test_menu_restore_and_encoding_change(self):
        m=Machine();m.feed('\x1b[4m'+('HOST '*60)+'\x1b[7;8H');m.call('cursor_off')
        before=bytes(m.mem.vram[0x7f100:]);p=m.addr('cell_styles');styles=bytes(m.mem[p:p+1920]);font=bytes(m.mem.vram[:2048])
        vectors=bytes(m.mem[0x200:0x220]);out=len(m.out)
        ui=MenuDriver(m);self.assertIn('LOCAL MENU',ui.text());self.assertIn('Connection directory',ui.text())
        self.assertEqual(m.mem.vram[0x803],0x21)
        ui.press('D==\r\x1b');m.call('cursor_off')
        self.assertEqual(m.get('utf8_mode'),1);self.assertEqual(bytes(m.mem.vram[0x7f100:]),before)
        self.assertEqual(bytes(m.mem[p:p+1920]),styles);self.assertEqual(bytes(m.mem.vram[:2048]),font)
        self.assertEqual(bytes(m.mem[0x200:0x220]),vectors);self.assertEqual(m.mem.vram[0x803],0x11)
        self.assertEqual(m.get('proto_depth'),0);self.assertEqual(m.mem.bank_b,0)
        self.assertTrue(set(m.out[out:])<=set(b'\x11\x13'))

    def test_unicode_pixels_survive_nested_ui_banks(self):
        m=Machine();m.set('utf8_mode',1);m.feed('Hello 世界 e\u0301');m.call('cursor_off');m.call('plus_render_dirty')
        self.assertEqual(m.get('u_active'),1)
        pixels=bytes(m.mem.vram[0x20000:0x2f000]);cells=bytes(m.mem.vram[0x7f100:])
        ui=MenuDriver(m,3);ui.press('H\rX\x1b');m.call('cursor_off');m.call('plus_render_dirty')
        self.assertEqual(bytes(m.mem.vram[0x20000:0x2f000]),pixels);self.assertEqual(bytes(m.mem.vram[0x7f100:]),cells)
        self.assertEqual(m.get('proto_depth'),0)

    def test_40_column_atascii_restores_font_and_width(self):
        m=Machine();m.set('atascii_mode',1);m.set('term_last_col',39)
        m.mem.vram[:2048]=bytes((i*13+5)&255 for i in range(2048));m.call('plus_font_changed');m.feed('HELLO');m.call('cursor_off')
        font=bytes(m.mem.vram[:2048]);cells=bytes(m.mem.vram[0x7f100:])
        ui=MenuDriver(m,3);self.assertIn('40 columns',ui.text());ui.press('\x1b');m.call('cursor_off')
        self.assertEqual(m.get('atascii_mode'),1);self.assertEqual(m.get('term_last_col'),39)
        self.assertEqual(bytes(m.mem.vram[:2048]),font);self.assertEqual(bytes(m.mem.vram[0x7f100:]),cells)

    def test_editor_cancel_and_maximum_length(self):
        m=Machine();m.mem[m.addr('login_macro'):m.addr('login_macro')+4]=b'OLD\x9b'
        ui=MenuDriver(m,2);ui.press('===\rNEW\x1b');self.assertEqual(m.string(m.addr('login_macro')),'OLD')
        ui.press('\r'+ '\x08'*3+'X'*135+'\r');self.assertEqual(m.string(m.addr('login_macro')),'X'*127)
        ui.press('\x1b')

    def test_serial_1200_survives_disk_io(self):
        m=Machine();m.set('device_type',0);m.set('dev_ready',1);commands=[];cio=m.cio
        def record():
            b=0x340+m.cpu.x
            if m.mem[b+2]==36:commands.append(m.mem[b+10])
            cio()
        m.cio=record
        ui=MenuDriver(m);ui.press('===\r=\r\x1b\x1b')
        self.assertEqual(m.get('serial_baud_code'),10);self.assertIn(10,commands)
        m.call('font_swap_prep_r');m.call('font_swap_restore_r');self.assertEqual(commands[-1],10)
        m.incoming+=b'R1 SERIAL';m.call('recv_from_device');self.assertEqual(m.screen()[0][:9],b'R1 SERIAL')

    def test_new_profile_save_load_and_cancel(self):
        m=Machine();m.set('dev_ready',0)
        ui=MenuDriver(m);ui.press('=\r\rMY BBS\r==\rbbs.example\r=\r2323\rS')
        self.assertIn('DIRECTORY',ui.text());ui.press('3\rX\x1b')
        self.assertEqual(len(m.files['D:VBXE.DIR']),4096)
        record=m.files['D:VBXE.DIR'][1024:1536]
        self.assertEqual(record[:7],b'MY BBS\x9b');self.assertEqual(record[32:58],b'N1:TCP://bbs.example:2323\x9b')
        n=Machine();n.files['D:VBXE.DIR']=bytearray(m.files['D:VBXE.DIR']);n.call('plus_session_init');n.set('dev_ready',0)
        ui=MenuDriver(n);ui.press('\r');self.assertIn('N1:TCP://bbs.example:2323',ui.text());ui.press('3E\rCHANGED\x1b\x1b\r3\r')
        self.assertEqual(n.get('profile_ready'),1);self.assertEqual(n.string(n.addr('n_url_buf')),'N1:TCP://bbs.example:2323')
        self.assertEqual(n.files['D:VBXE.DIR'],m.files['D:VBXE.DIR'])

    def test_receive_confirm_cancel_preserves_file(self):
        m=Machine();m.set('dev_ready',1);m.files['D:OLD.BIN']=bytearray(b'keep me');ui=MenuDriver(m,1)
        ui.press('\r=\rD:OLD.BIN\r==\r');self.assertIn('Replace file?',ui.text())
        ui.press('\x1b\x1b');self.assertEqual(m.files['D:OLD.BIN'],b'keep me')
        self.assertEqual(m.get('ui_xfer_active'),0)

    def test_profile_port_rejects_out_of_range(self):
        for port in ('0','65536','99999'):
            m=Machine();m.set('dev_ready',0);ui=MenuDriver(m)
            ui.press('=\r==\rbbs.example\r=\r'+port+'\rC')
            self.assertIn('port 1-65535',ui.text());self.assertEqual(m.get('profile_ready'),0)
            ui.press('X\x1b\x1b')

    def test_serial_profile_can_be_saved_and_connected(self):
        m=Machine();ui=MenuDriver(m);ui.press('==\r2\rS')
        self.assertIn('Saved',ui.text());ui.press('X\x1b\x1b')
        rec=m.files['D:VBXE.DIR'][512:1024];self.assertEqual(rec[32:35],b'R:\x9b')
        self.assertEqual(rec[485],14);self.assertTrue(rec[0]!=0x9b)
        m.set('dev_ready',0);ui=MenuDriver(m);ui.press('\r2\r')
        self.assertEqual(m.get('profile_ready'),1);self.assertEqual(m.string(m.addr('n_url_buf')),'R:')

    def test_receive_xmodem_through_ui(self):
        m=Machine();m.set('dev_ready',1);m.set('device_type',1);m.set('n_unit',1);data=bytes(range(128))
        ui=MenuDriver(m,1);ui.press('\r\r\r=\rD:TEST.BIN\r==')
        m.incoming+=packet(1,data)+b'\x04';ui.press('\r')
        self.assertIn('Transfer complete',ui.text());self.assertEqual(m.files['D:TEST.BIN'],data)
        self.assertGreater(int.from_bytes(bytes(m.mem[m.addr('ui_rx_bytes'):m.addr('ui_rx_bytes')+4]),'little'),128)
        ui.press('X\x1b');self.assertEqual(m.get('proto_depth'),0)

    def test_missing_ui_bank_leaves_legacy_available(self):
        m=Machine();cio=m.cio
        def fail_ui():
            b=0x340+m.cpu.x;p=m.mem[b+4]|m.mem[b+5]<<8
            if m.mem[b+2]==3 and m.string(p)=='D:UI.BIN':m.ret(170)
            else:cio()
        m.cio=fail_ui;m.call('ui_load');self.assertEqual(m.get('ui_ready'),0);self.assertEqual(m.get('proto_ready'),1)

    @unittest.skipUnless((DEPS/'sz').exists() and (DEPS/'kermit').exists(),'Install lrzsz and C-Kermit')
    def test_independent_peers_through_ui_with_progress(self):
        for protocol,direction in [('z','receive'),('k','send')]:
            with tempfile.TemporaryDirectory() as td:
                m=Machine();m.set('dev_ready',1);m.set('device_type',1);m.set('n_unit',1)
                data=bytes(range(256))*8+b'END'
                if direction=='receive':pathlib.Path(td,'TEST.BIN').write_bytes(data)
                else:m.files['D:TEST.BIN']=bytearray(data)
                ui=MenuDriver(m,1);ui.press(('=' if direction=='send' else '')+'\r'+('\r' if protocol=='k' else '')+'=\rD:TEST.BIN\r==')
                # Start the native peer before pressing Begin, then dismiss its result.
                cmd=[str(DEPS/'sz'),'-q','-b','-y','TEST.BIN'] if protocol=='z' else [str(DEPS/'kermit'),'-Y','-Q','-i','-r']
                peer=ProcessPeer(m,None,cmd,td)
                m.out.clear();m.dialog_keys=[13,ord('X'),27]
                try:
                    self.assertEqual(peer.run(seconds=60),0,peer.err[-300:])
                    received=list(pathlib.Path(td).iterdir())
                    if direction=='send':self.assertEqual(len(received),1)
                    actual=m.files.get('D:TEST.BIN') if direction=='receive' else received[0].read_bytes()
                    self.assertEqual(actual,data);self.assertEqual(m.get('proto_depth'),0);self.assertEqual(m.cpu.sp,ui.sp)
                    self.assertEqual(m.mem.bank_b,0);self.assertEqual(m.get('ui_xfer_active'),0)
                finally:peer.close()

if __name__=='__main__':unittest.main()
