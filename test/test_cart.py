import hashlib
import json
import pathlib
import random
import struct
import unittest
from unittest.mock import patch
from machine import Machine, ROOT
from cart_machine import CartMachine
from ui_driver import MenuDriver
from test_unicode import raw

BUILT = all((ROOT/p).exists() for p in ('build-cart/PRISMTERM-D600.CAR','build-cart-d700/PRISMTERM-D700.CAR'))

@unittest.skipUnless(BUILT,'Run make cart to build both cartridge variants')
class Cartridge(unittest.TestCase):
    def test_images_and_every_glyph_record_round_trip(self):
        source=(ROOT/'fonts/UNICODE.ATR').read_bytes()[16:]
        for base,folder in [(0xd600,'build-cart'),(0xd700,'build-cart-d700')]:
            path=ROOT/folder;car=(path/f'PRISMTERM-{base:04X}.CAR').read_bytes();rom=car[16:]
            self.assertEqual(len(rom),4194304)
            self.assertEqual(car[:4],b'CART');self.assertEqual(struct.unpack('>III',car[4:16]),(63,sum(rom)&0xffffffff,0))
            report=json.loads((path/'cartridge.json').read_text())
            self.assertEqual(report['sha256'],hashlib.sha256(car).hexdigest())
            for r in report['resources']:
                self.assertEqual(hashlib.sha256(rom[r['offset']:r['offset']+r['bytes']]).hexdigest(),r['sha256'],r['name'])
            restored=bytearray(rom[0x20000:0x22380])
            end=0x22400+report['packed_glyph_bytes']
            for p in range(0x22400,end,17):restored+=rom[p:p+17]+bytes(15)
            self.assertEqual(restored,source)  # all 193,024 slots / 126,947 present glyphs
            boot=rom[254*16384:255*16384]
            self.assertEqual(boot[-4:-2],b'\x00\x04')
            self.assertEqual(int.from_bytes(boot[-6:-4],'little'),0x8000)
            self.assertEqual(rom[255*16384:],bytes([255])*16384)

    def test_cold_boot_both_addresses_without_program_disks(self):
        for base in (0xd600,0xd700):
            m=CartMachine(base)
            self.assertEqual((m.get('proto_ready'),m.get('ui_ready'),m.mem.cart_bank,m.mem.bank_b),(1,1,255,0))
            self.assertEqual(m.mem.base,base)
            self.assertEqual(bytes(m.mem.vram[:2048]),(ROOT/'disk/ATARIPC.FNT').read_bytes())
            self.assertEqual(bytes(m.mem.vram[0x1000:0x4000]),(m.build_dir/'UI.BIN').read_bytes())
            self.assertEqual(bytes(m.mem.vram[0x4000:0x8000]),(m.build_dir/'TRANS.BIN').read_bytes())
            self.assertEqual(bytes(m.mem.palette[1][:48]),(ROOT/'disk/ANSI.PAL').read_bytes())
            self.assertLessEqual(int.from_bytes(m.mem.ram[0x2e7:0x2e9],'little'),0x2500)
            self.assertIn(ord('D'),m.mem.ram[0x31a:0x33e:3]);self.assertIn(ord('R'),m.mem.ram[0x31a:0x33e:3])
            self.assertFalse(any(cmd==3 and name!='D:VBXE.DIR' for cmd,name in m.io_log))
            self.assertTrue(all(command==ord('S') for _,_,command,_ in m.sio_log))
            self.assertEqual(m.mem.rom_writes,0)
            self.assertLess(m.mem.max_masked_cycles,512)
            for _ in range(500000):
                if not m.mem.bank_b and m.cpu.pc==m.addr('plus_key'):break
                m.step()
            else:self.fail('Cold start did not open the command menu')
            self.assertIn('LOCAL MENU','\n'.join(r.decode('cp437') for r in m.screen()))

    def test_reader_crosses_banks_and_restores_ram_and_memac(self):
        m=CartMachine();rng=random.Random(63)
        for source,size in [(0x3ff1,70),(0xffe0,96),(0x1ffff,258),(0x33bfff,514),(0x10000,0)]+[(rng.randrange(0x340000),rng.randrange(1,600)) for _ in range(15)]:
            m.mem[m.addr('cart_src'):m.addr('cart_src')+3]=source.to_bytes(3,'little')
            m.mem[m.addr('cart_dest'):m.addr('cart_dest')+2]=(0x9000).to_bytes(2,'little')
            m.mem[m.addr('cart_len'):m.addr('cart_len')+2]=size.to_bytes(2,'little')
            bank=m.mem.bank;sp=m.cpu.sp;m.call('cart_read')
            self.assertEqual(bytes(m.mem.ram[0x9000:0x9000+size]),m.mem.rom[source:source+size])
            self.assertEqual((m.mem.cart_bank,m.mem.bank,m.cpu.sp,m.mem.ram[0xd40e]),(255,bank,sp,0x40))

    def test_all_fonts_load_from_rom(self):
        from tools.make_cart import FONTS
        m=CartMachine();reads=len(m.sio_log)
        for index,name in enumerate(FONTS,3):
            m.set('active_font',index);m.call('font_load_index',a=index)
            self.assertEqual(bytes(m.mem.vram[:2048]),(ROOT/'disk'/f'{name}.FNT').read_bytes(),name)
            self.assertEqual(m.mem.cart_bank,255)
        self.assertEqual(len(m.sio_log),reads)

    def test_compiled_sector_reconstruction_all_pages_and_boundaries(self):
        m=CartMachine();source=(ROOT/'fonts/UNICODE.ATR').read_bytes()[16:]
        sectors=set(range(1,39));sectors.update(range(38,24166,32));sectors.add(24165)
        # Exercise packed records split by every 16K cartridge bank boundary.
        for boundary in range(0x24000,0x344000,0x4000):
            n=(boundary-0x22400)//136+38
            sectors.update((n-1,n,n+1))
        io=len(m.sio_log)
        for sector in sorted(s for s in sectors if 1<=s<=24165):
            m.mem[m.addr('u_sector_no'):m.addr('u_sector_no')+2]=sector.to_bytes(2,'little')
            m.call('u_disk_read')
            off=(sector-1)*128 if sector<4 else 384+(sector-4)*256
            size=128 if sector<4 else 256
            actual=bytes(m.mem[m.addr('u_sector'):m.addr('u_sector')+size])
            self.assertEqual(actual,source[off:off+size],sector)
            self.assertEqual(m.get('u_disk_error'),1)
        self.assertEqual(len(m.sio_log),io)
        for bad in (0,24166,65535):
            m.mem[m.addr('u_sector_no'):m.addr('u_sector_no')+2]=bad.to_bytes(2,'little')
            m.call('u_disk_read');self.assertEqual(m.get('u_disk_error'),144)

    def test_unicode_pixels_match_disk_build_and_menu_restore(self):
        for base in (0xd600,0xd700):
            m=CartMachine(base);disk=Machine(build_dir=ROOT/('build' if base==0xd600 else 'build-d700'))
            disk.mem.vram[:2048]=(ROOT/'disk/ATARIPC.FNT').read_bytes();disk.set('active_font',4);disk.call('plus_font_changed')
            for machine in (m,disk):
                machine.call('scroll_page');machine.set('row',0);machine.set('column',0);machine.call('recalc_cursor')
                machine.set('utf8_mode',1);machine.feed('A\u0301 Ж中🙂𠀀 א क \u0537 Ж')
                machine.call('cursor_off');machine.call('plus_render_dirty')
            self.assertEqual(m.get('u_disk_ok'),1)
            self.assertEqual(bytes(m.mem.vram[0x8000:0xbc00]),bytes(disk.mem.vram[0x8000:0xbc00]))
            before=bytes(m.mem.vram[0x20000:0x2f000]);u=MenuDriver(m);u.press('D\x1b')
            m.call('cursor_off');m.call('plus_render_dirty')
            self.assertEqual(bytes(m.mem.vram[0x20000:0x2f000]),before)
            self.assertEqual((m.mem.cart_bank,m.mem.bank_b),(255,0))

    def test_restart_reloads_everything_and_does_not_chain_handlers(self):
        m=CartMachine();memlo=bytes(m.mem.ram[0x2e7:0x2e9])
        for reset in ('exit_to_dos','proto_reset'):
            m.set('utf8_mode',1);m.feed('中🙂');m.cpu.pc=m.addr(reset)
            for i in range(4000000):
                m.step()
                if m.cpu.pc==m.addr('device_select'):break
            else:self.fail(f'{reset} stalled at {m.cpu.pc:04x}')
            self.assertEqual(bytes(m.mem.ram[0x2e7:0x2e9]),memlo)
            self.assertEqual(list(m.mem.ram[0x31a:0x33e:3]).count(ord('R')),1)
            self.assertEqual((m.get('proto_ready'),m.get('ui_ready'),m.get('utf8_mode')),(1,1,0))

    def test_ui_serial_profiles_and_xmodem_with_cartridge(self):
        import test_ui
        # Reuse established tests against the ROM-built program and loader.
        with patch.object(test_ui,'Machine',CartMachine):
            tests=test_ui.LocalUI()
            tests.test_serial_1200_survives_disk_io()
            tests.test_new_profile_save_load_and_cancel()
            tests.test_receive_xmodem_through_ui()
            tests.test_receive_confirm_cancel_preserves_file()

    def test_zmodem_and_kermit_with_independent_peers(self):
        import test_ui
        with patch.object(test_ui,'Machine',CartMachine):
            test_ui.LocalUI().test_independent_peers_through_ui_with_progress()

if __name__=='__main__':unittest.main()
