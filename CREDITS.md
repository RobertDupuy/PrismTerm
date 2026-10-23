# Credits and provenance

PrismTerm is an independent Atari XL/XE and VBXE terminal project maintained
by **Robert Dupuy**.

- **Joseph Zatarski** wrote the original MADS terminal.
- **Brad Colbert** converted and developed VBXETERM with ca65, including its
  transport, display, menu and ATASCII foundations.
- PrismTerm continues the VBXETERM Plus extensions developed with **OpenAI
  Codex**: terminal compatibility, session tools, transfers, Unicode rendering,
  the host layout companion, and compiled-code tests.

The starting point is [brad-colbert/ansivbxe](https://github.com/brad-colbert/ansivbxe),
VBXETERM v0.25 at commit `c7d8bbc2c95718bb9fc0847fb8fe43ba42b9f60f`.
Its Git history and original source notices are preserved. Reproducible
comparisons recover the pinned source from that history and verify its hashes;
the published tree does not include a duplicate upstream source fixture.
The original [license](license.txt) remains in effect.

GNU Unifont and Noto provide the bundled fonts. Their provenance and licenses
are retained in [fonts/SOURCES.md](fonts/SOURCES.md),
[bridge/README.md](bridge/README.md), `LICENSES/`, and `bridge/fonts/LICENSES/`.
The independent XMODEM test peer retains its own license in `test/third_party/`.

PrismTerm is developed independently and is not an official release or
endorsement by the upstream authors.
