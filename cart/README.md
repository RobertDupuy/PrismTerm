# PrismTerm cartridge target

The complete native terminal fits in a **4 MiB (32 Mbit) Flash MegaCart**.
The `.CAR` file declares **Atari cartridge type 63**; use a cartridge or emulator
that implements this mapper. Flash capacity alone does not establish compatibility.

Choose `PRISMTERM-D600.CAR` for VBXE registers at $D600, or
`PRISMTERM-D700.CAR` for registers at $D700. The corresponding `.ROM` is the
same 4,194,304-byte image without the 16-byte CAR header, for programmers that
require raw ROM data. Insert one image as the cartridge and cold-boot an XL/XE
with 64 KiB RAM and a 512 KiB VBXE FX expansion.

Neither program ATR nor `UNICODE.ATR` is needed. The cartridge supplies the
program, menu and transfer engines, palette, all 13 native fonts, and the same
126,947 Unicode glyphs as the disk release. It also initializes the bundled
MyDOS and original relocatable R: handler. BASIC is disabled by the loader.
The existing disk targets remain available.

## Saving and connecting

R: serial and FujiNet networking use the same terminal controls as the disk
version. The supplied R: handler has the same limitations as the disk release;
19,200 baud requires an extended handler. FujiNet remains responsible for SSH.

ROM is read-only. To save profiles or captured text, receive downloads, or
send local files, mount a writable DOS-compatible data disk as D1: (or use an
explicit `D2:`, `D3:`, etc. filename where supported). No boot/program files
need to be on that data disk. The connection directory is `D:VBXE.DIR` on D1:.
Missing data storage reports a file error; it never writes the cartridge flash.
The boot initializer may probe D1: status, but it does not load program files
from a drive. A missing disk does not prevent startup or Unicode rendering.

RESET and **Connect → Restart terminal** restart from ROM. They discard
unsaved capture/settings. The disk build retains its Exit to DOS command.

Native glyph rendering is unchanged. Complex script shaping, bidirectional
layout and emoji sequences still use the external host companion described
in `bridge/README.md`; it is not a cartridge-resident Unicode shaping engine.

## Rebuild

With cc65 and Python 3 on PATH:

```sh
make cart
make cart-test
```

Outputs go to `build-cart/` and `build-cart-d700/`. Each contains `.CAR`, `.ROM`,
`cartridge.json` (resource extents and hashes), the ROM boot assembly listing,
and the normal program symbols. `make disk` continues to build both ATRs.

## Layout and safety properties

The font ATR is 6,185,872 bytes. Every 32-byte glyph record contains 17 useful
bytes and 15 zero reserved bytes. The cartridge stores those 17 useful bytes
for every existing slot and reconstructs the original sector on a cache miss.
It does not reduce coverage, remove supplementary planes, or alter glyph pixels.
The full font image is reconstructed and compared byte-for-byte in the tests.

The type-63 mapper exposes 16 KiB at $8000–$BFFF, selects banks 0–254 by writing
$D500, and disables ROM with $FF. Power-on selects bank 254; bank 255 is unused.
The boot ROM copies a gateway to $2500 before changing banks. The application
runs from ordinary RAM with the cartridge disabled. Each resource read copies
at most four bytes into low RAM, disables the cartridge, restores MEMAC A, then
writes the destination. This avoids simultaneous cartridge/VBXE window drivers
and avoids writes to RAM hidden by ROM. IRQs and the OS VBI are masked only
around each ROM-read burst. The terminal does not install a DLI.

| ROM range | Contents |
|---|---|
| $000000–$00FFFF | MyDOS, R: loader, program RAM segments |
| $010000–$013FFF | Transfer and Unicode engine |
| $014000–$016FFF | Local menu engine |
| $018000–$01E7FF | Thirteen 2 KiB fonts |
| $01E800–$01E82F | ANSI palette |
| $020000–$02237F | UFN1 header and full Unicode page directory |
| $022400 onward | Packed 17-byte glyph records |
| $3F8000–$3FBFFF | Power-on boot bank 254 |
| $3FC000–$3FFFFF | Unaddressable bank 255; filled with $FF |

The banking specification is documented by the
[Flash MegaCart designer](https://abbuc.de/~bernd/4mb-flash-megacart/english/index.php)
and [Atari800's cartridge-format reference](https://github.com/atari800/atari800/blob/master/DOC/cart.txt).

Tests execute the compiled boot ROM, MyDOS initializer, R: relocator, mapper,
resource reader and terminal in the project's functional 6502/VBXE model.
They cover both register addresses, ROM/RAM restoration, bank boundaries,
Unicode sector reconstruction and pixels, all fonts, menus, reset/restart,
profile persistence and transfers. CIO/SIO device services remain modeled.
Physical cartridge/Atari/VBXE operation and interrupt/DMA timing are not yet
hardware-validated. See `VALIDATION.txt` for the completed checks.
