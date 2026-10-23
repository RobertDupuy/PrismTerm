# Development record — 2026-10-23

Downloaded and verified all 44 upstream blobs at brad-colbert/ansivbxe commit c7d8bbc2c95718bb9fc0847fb8fe43ba42b9f60f (v0.25). Compiled the unchanged ca65 source and passed both original sim65 suites before modifying it.

Implemented file transfers, scrollback/capture, saved connections and explicit login macros, expanded ANSI/DEC behavior, UTF-8 mapping, real underline/blink, full-width 40-column graphics, and Telnet framing. The original authors and license remain credited.

Important fixes and optimizations:

- Moved recv/send/control buffers from a main-object-end equate that overlapped the separately linked library into linker-managed BSS.
- Added explicit BSS initialization and a linker limit below the MEMAC window.
- Bounded user input and CSI parameters; prevented invalid counts and parser overflows.
- Replaced row-dependent cursor-address arithmetic with lookup tables.
- Added a receive-burst ASCII fast path and hid the cursor once per burst.
- Restricted native scroll/clear blits to the visible screen, avoiding snapshot corruption and reducing full-screen scroll traffic by 80%.
- Used hardware blits for region movement and bounded, single-pass character editing.
- Cached font masks; updated only dirty graphics spans and six variable glyph BCB bytes.
- Preserved cell attributes across editing, history, dialogs and alternate screens, including scroll margins.
- Protected the foreground FIFO producer against keyboard IRQ races.
- Made menus fit 40 columns and corrected ATASCII dialog glyph translation.
- Normalized font filenames to the names actually used by DOS paths.

Validation executes the compiled instructions in a functional VBXE/CIO/SIO model. It includes an independent XMODEM peer and full-frame pixel comparisons. Recorded results are in VALIDATION.txt and PERFORMANCE.md. Disk images have their file contents verified separately. Physical Atari/VBXE/FujiNet testing, real SIO/DMA timing and end-to-end SSH testing have not been performed.

No upstream commit, pull request, forum post or message was published. This package is an independent experimental derivative.


## Plus 0.2

Recovered the prior 0.1 package and retained its baseline sources and original authorship. Conventional RAM was insufficient for the added engines, so the protocol/Unicode implementation now runs from a bounded MEMAC B overlay with resident gateways. The native screen, styles and I/O buffers remain outside that window.

ZMODEM and Kermit were exercised against unmodified lrzsz and C-Kermit processes over pipes/PTYs. Tests cover arbitrary binary bytes, empty files, CRC16/CRC32 reception, corrupted traffic in both directions, Kermit sequence wrap/duplicate data, cancellation, bounded bad headers and ZMODEM prefix verification. A fault-injection test found nonconsecutive ZDLE bytes being counted as cancellation; the counter now resets on intervening input. Resume tests confirm a mismatched local prefix is preserved.

Unicode tests cover supplementary scalars, wide wrapping, combining marks, cache eviction, font-disk reads/failure, edits, both scroll directions, alternate/dialog/history restore, reset, bounded CRC-checked DCS and reference pixels in both display widths. The host companion uses installed shaping libraries and bundled fonts rather than claiming a complete native shaping engine. Visual inspection caught a palette-order error; all 16 output colors now have a regression check. Host tests also exercise incremental grapheme boundaries and an actual child-shell lifecycle.

The font disk and D600/D700 boot disks are reproducible from bundled data. Documentation distinguishes functional-model validation from physical Atari/VBXE/FujiNet validation. Full host shaping requires the companion; native bitmap composition, font coverage, color depth, serial throughput and full terminal conformance have explicit limits.
