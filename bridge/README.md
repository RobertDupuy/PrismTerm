# Unicode layout companion

Run this on the **remote Linux/POSIX host** reached from the Atari, inside the existing SSH/raw terminal session. The Atari renders bounded pixel updates; the host handles complex text layout. This companion is required for contextual Arabic/Indic shaping, bidirectional text and composed emoji. Native UTF-8 mode by itself does not do that shaping.

## Setup

Copy the complete `bridge/` directory to the host, including `fonts/`. It uses Python 3, Cairo, Pango, HarfBuzz, FriBidi and Fontconfig. On Debian/Ubuntu, the system libraries can be installed with:

```sh
sudo apt install python3-venv libcairo2 libpango-1.0-0 libpangocairo-1.0-0 libfontconfig1
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install -r requirements.txt
python3 unicode_bridge.py --shell /bin/bash
```

Pango's dependencies provide HarfBuzz/FriBidi. Requirements pin the tested Python versions. Font files are registered only in this process; no system font installation is required. Keep the supplied `ANSI.PAL` on the Atari for matching colors.

Use the Atari's **80-column ANSI display** and a byte-transparent connection. `TRANS.BIN` must have loaded at Atari startup. D2: is unnecessary for companion pixels. The child shell receives an 80×12 PTY, UTF-8 locale and `TERM=xterm-256color`; colors are reduced to the Atari palette. Run ordinary shell commands within it.

Type `exit` to return to the outer shell. **Ctrl-]** also closes the companion's child session; save work in that child first. If the keyboard cannot send Ctrl-], the Atari macro `^]` sends that control byte.

## Transfers

**Exit the companion before starting sz/rz, Kermit, XMODEM or YMODEM.** Start the remote transfer from the outer shell, then select the matching Atari menu action. Restart the companion afterward. Binary protocol bytes cannot be interpreted as terminal text, so file transfers do not pass through the layout renderer.

## Rendering limits

The logical screen has 80 columns and 12 rows. Each 16-pixel text row is painted as two native 8-pixel rows. Pango shapes each complete row, preserving bidi and joining context across adjacent style runs; combining/ZWJ/variation-selector/regional-indicator sequences are accumulated as extended grapheme clusters. Bundled Noto fonts and Unifont provide fallback.

The display is 640×192 with 16 palette colors and two colors per 8×8 tile. Small emoji and stacked marks lose detail. Tall fallback fonts are scaled into the row. Full-row layout can differ from strict cell positioning in complex-script text; exhaustive text-editor/curses compatibility is not claimed. Glyph availability depends on fonts, and not every Unicode character has a bundled glyph. Blink animation, terminal graphics protocols, mouse reporting and full xterm conformance are outside this companion's implementation.

Updates are checked at up to 10 Hz and sent only for changed cells. Transmission and Atari drawing impose the actual rate. The included multilingual sample requires about 15 KB for its first frame; a slow serial connection will take time to draw it. There is no network listener, remote execution command or file-write command in the Atari pixel extension.

## Wire format

One packet is `ESC P = U`, followed by **28 ASCII hex digits**, then `ESC \\`. Decoded bytes:

| Bytes | Meaning |
|---|---|
| 0–1 | Atari row (0–23), column (0–79) |
| 2 | Native attribute: foreground low nibble, low 3 background bits in bits 4–6, bit 7 set |
| 3 | Style: bit 0 underline, bit 1 blink, bit 4 high background bit |
| 4–11 | Eight glyph rows, most significant bit at the left |
| 12–13 | CRC16-CCITT, polynomial 0x1021, initial zero, big endian, over bytes 0–11 |

The parser requires the exact length, terminator, valid coordinates and CRC before modifying a cell. It ignores malformed/unknown/oversized DCS bodies. The custom-glyph flag is managed by the receiver. Initial frames clear the screen/hide the native cursor; unchanged black tiles are omitted. Exit restores the cursor and clears the pixel display contents.

Generate a host preview and sample wire stream with:

```sh
python3 unicode_bridge.py --demo sample.png
```

For a compiled-Atari rendering of that stream, run `PYTHONPATH=test python3 test/render_unicode_preview.py` from the project root after building. `release/preview-unicode-companion.png` is the modeled Atari output.
