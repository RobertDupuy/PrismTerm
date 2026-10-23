# Performance measurements

Pinned upstream: `c7d8bbc2c95718bb9fc0847fb8fe43ba42b9f60f`. PrismTerm is compiled from the current source; the comparison source is recovered and hash-verified from the original Git history.

CIO/SIO and VBXE blits are functional models. Their real waiting time and Atari DMA/interrupt contention are not included. These numbers are not measured baud rates or hardware throughput.

| Workload | Unit | Upstream | PrismTerm | Reduction |
|---|---|---:|---:|---:|
| Native text, 1200 bytes | 6502 cycles | 254,010 | 119,947 | 52.78% |
| 100 lines, 7800 printable bytes | 6502 cycles | 1,671,535 | 1,006,195 | 39.8% |
| ANSI color text, 100 lines | 6502 cycles | 1,643,046 | 1,143,376 | 30.41% |
| Cursor address at row 23 | 6502 cycles | 528 | 32 | 93.94% |
| Scroll rows 3-21 upward by 3 | 6502 cycles | 48,759 | 1,959 | 95.98% |
| Full-screen one-line scroll | blitter bytes written | 19,200 | 3,840 | 80.0% |

An initial complete 80-column graphics frame costs 796,556 modeled CPU cycles, plus hardware blitter time. Subsequent writes redraw only the dirty cell spans. Native text remains the normal 80-column path.

Run `make benchmark` to reproduce. Test inputs are in `test/benchmark.py`.
