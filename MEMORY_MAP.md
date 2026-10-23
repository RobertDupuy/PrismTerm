# Memory layout — PrismTerm 0.3

CPU startup remains $2800. The linker bounds normal code, data and BSS below $A000. It separately bounds resident gateways to $B000–$BFFF and each overlay to its reserved region. Shared UI buffers are in BSS above $8000; a linker assertion guards this requirement. BASIC must be disabled. BSS is explicitly cleared on startup.

The optional cartridge build adds a low-RAM gateway at $2500–$27FF. The bundled
MyDOS/R: initialization leaves MEMLO at $249A; the boot loader rejects overlap.
Cartridge ROM is normally disabled. Short reads temporarily map $8000–$BFFF,
with MEMAC A disabled, and stage data through a four-byte low-RAM buffer before
restoring RAM/VBXE mappings. No additional VBXE memory is reserved by this target.
See [the cartridge layout](cart/README.md).

| CPU address | Use |
|---|---|
| $2800–$9FFF | Main program, parser, transport, styles and buffers; linker rejects overflow |
| $4000–$7FFF while MEMAC B enabled | 16 KiB engine overlay, loaded from `D:TRANS.BIN` into VBXE $04000–$07FFF |
| $5000–$7FFF while MEMAC B bank 0 enabled | 12 KiB UI bank, loaded from `D:UI.BIN` into VBXE $01000–$03FFF |
| $A000–$AFFF | 4 KiB MEMAC A window; normally maps the native cell page to $A100 |
| $B000–$BFFF | Resident bank dispatch, OS/transport/blitter gateways, IRQ/reset wrappers and Unicode glue |

The resident entry wrapper replaces the keyboard/proceed vectors while banked code runs. A nesting counter and stacked prior bank allow UI, Unicode and protocol calls to nest; main/OS gates preserve arguments and return flags. Those IRQ wrappers restore normal RAM for the original handlers, then restore the bank. OS, disk, transport and main-code calls use resident gates with MEMAC B disabled; the Atari RAM under the overlay remains intact. Reset disables the bank. These mechanisms have functional tests; arbitrary third-party IRQ hooks and physical timing remain unvalidated.

| VBXE address range | Purpose |
|---|---|
| $00000–$007FF | Current 256-glyph native font |
| $00800 onward | XDL and native scroll/clear BCB lists |
| $00C00–$00C14 | Shared dynamic blitter control block |
| $00E00 onward | Scratch expanded custom glyph mask |
| $01000–$03FFF | Local menu code, text, palette and private UI state |
| $04000–$07FFF | Protocol/Unicode engine bank, including private bank state/CRC tables |
| $08000–$0BBFF | Live custom pixels, 80×24×8 bytes |
| $0C000–$0C7FF | Startup CP437 font for local menus opened from ATASCII |
| $0C800–$0CFFF | Saved session font while local menus are open |
| $0E000–$0FFFF | 256 direct-mapped glyph-cache entries, 32 bytes each |
| $10000–$13BFF | Unicode alternate-screen pixel snapshot when Unicode active |
| $14000–$17BFF | Unicode dialog pixel snapshot when Unicode active |
| $10000–$1BFFF when Unicode inactive | Four native expanded-font mask sets for 40/80 columns and underline |
| $1D000–$1D1FF | Temporary profile draft while choosing a save slot |
| $1E000–$1EFFF | Eight 512-byte connection records |
| $20000–$2EFFF | Framebuffer: 640×192 4bpp or 320×192 8bpp |
| $30000–$6FFFF | History: 1,024×256-byte native slots, or 256×1,024-byte Unicode slots |
| $70000–$77FFF | 32 KiB capture |
| $7A000–$7AEFF | Dialog cell snapshot |
| $7B000–$7B77F | Dialog style snapshot |
| $7C000–$7CEFF | Main cells saved during alternate-screen use |
| $7D000–$7D77F | Main styles saved during alternate-screen use |
| $7E000–$7E77F | Menu style snapshot |
| $7F100–$7FFFF | Live native cells, 80×24×2 bytes |

The 1,920 live style bytes reside in CPU BSS. Bits 0/1 mean underline/blink, bit 4 is the bright background bit, and bit 7 selects cell-owned custom pixels. The native character/color cell remains available for non-custom text. Cell pixels are copied to screen snapshots/history independently of the font cache.

A native history slot contains 160 cell bytes and 80 style bytes. A Unicode history slot also contains 640 pixel bytes. Activating Unicode clears the old history and switches slot geometry; it does not overwrite the protocol bank or captures. Native mask storage is reused only after the Unicode renderer has switched to scratch-glyph expansion.

A font lookup hashes all three scalar bytes into the 256-entry cache, validates the full scalar tag, then reads the indexed raw font disk on a miss. One directory sector and the last glyph-data sector are cached in CPU RAM. The index covers all 4,352 pages of the Unicode scalar range; only pages with font glyphs occupy disk space. The parser/glyph cache does not restrict scalars to the BMP.

Graphics updates use dirty spans and an aggregate dirty flag. The blitter expands one custom mask at a time; seven BCB bytes vary across glyphs. The native renderer retains its cached mask fast path until custom pixels are needed.

Local UI drawing uses palette 2; the terminal continues using palette 1. Palette 0 is left untouched. The XDL palette selection is restored on exit. UI transfers refresh wire-byte progress at most twice per 60 Hz second.
