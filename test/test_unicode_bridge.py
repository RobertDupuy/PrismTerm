import sys,pathlib,unittest,random,re,subprocess
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'bridge'))
from unicode_bridge import UnicodeScreen,Renderer,SAMPLE,cell_packet
import pyte
class Bridge(unittest.TestCase):
 def test_streaming_grapheme_boundaries(self):
  for text in ['👩\u200d💻','👨\u200d👩\u200d👧\u200d👦','🇺🇸','❤️','A\u030a']:
   for cut in range(len(text)+1):
    s=UnicodeScreen();stream=pyte.Stream(s);stream.feed(text[:cut]);stream.feed(text[cut:])
    self.assertEqual(s.buffer[0][0].data,text);self.assertEqual(s.cursor.x,1 if text.startswith('A') else 2)
 def test_ascii_cursor_and_wrap(self):
  s=UnicodeScreen();pyte.Stream(s).feed('A'*79+'中')
  self.assertEqual((s.cursor.y,s.cursor.x),(1,2))
 def test_alternate_screen(self):
  s=UnicodeScreen();stream=pyte.Stream(s);stream.feed('主🙂\x1b[?1049hOTHER\x1b[?1049l')
  self.assertEqual(s.buffer[0][0].data,'主');self.assertEqual(s.buffer[0][2].data,'🙂')
 def test_shaped_scripts_and_changed_cells(self):
  s=UnicodeScreen();stream=pyte.Stream(s);stream.feed(SAMPLE);r=Renderer(s)
  first=r.updates(force=True,cursor=False)
  self.assertLess(len(first),20000);self.assertEqual(r.updates(cursor=False),b'')
  stream.feed('X');delta=r.updates(cursor=False);self.assertGreater(len(delta),0);self.assertLess(len(delta),2000)
 def test_scalar_not_literal_markup(self):
  s=UnicodeScreen();pyte.Stream(s).feed('<b>שלום</b> & नमस्ते');r=Renderer(s);wire=r.updates(cursor=False)
  self.assertTrue(wire);self.assertIn('<',s.buffer[0][0].data)
 def test_wire_colors_match_atari_palette(self):
  for color in range(16):
   s=UnicodeScreen();pyte.Stream(s).feed(f'\x1b[{40+color if color<8 else 100+color-8}m ')
   wire=Renderer(s).updates(force=True,cursor=False)
   cells=[bytes.fromhex(x.decode()) for x in re.findall(rb'\x1bP=U([0-9a-f]{28})\x1b\\',wire)]
   cell=next((b for b in cells if b[:2]==b'\x00\x00'),bytes(14))
   self.assertEqual(((cell[2]>>4)&7)|((cell[3]&16)>>1),color)
 def test_interactive_shell_exits_and_restores_cursor(self):
  script=pathlib.Path(__file__).resolve().parents[1]/'bridge/unicode_bridge.py'
  result=subprocess.run([sys.executable,str(script),'--shell','/bin/sh'],input='printf "Привет 世界\\n"\nexit\n'.encode(),capture_output=True,timeout=20)
  self.assertEqual(result.returncode,0,result.stderr.decode(errors='replace'))
  self.assertIn(b'\x1bP=U',result.stdout)
  self.assertTrue(result.stdout.endswith(b'\x1b[?25h\x1b[0m\x1b[2J\x1b[H'))
if __name__=='__main__':unittest.main()
