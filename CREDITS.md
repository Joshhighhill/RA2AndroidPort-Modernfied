# Credits and source provenance

This is a modified Android touch-control fork of **RA2 Engine**, not an
original implementation of the engine by this fork's maintainer.

## Original engine — Omar El Arab

- Author: **Omar El Arab** (`omarelarab`).
- Source: https://github.com/omarelarab/ra2-engine
- Starting revision: `a8cfdccfef8592c13a6517c3fb76d834b1534027`.
- License: **Zero-Clause BSD (0BSD)**; the original [LICENSE](LICENSE) is retained.
- The C++17 engine, SDL Android shell/importer, asset readers, rendering,
  simulation, existing touch controls, tests and development documentation
  originate in that upstream project. Upstream authorship and history are
  preserved in this fork.

## Changes in this fork

Android fork modifications maintained by **Bryle13 (shietgrupe-cpu13)**, with OpenAI Codex assistance for source
inspection, implementation, testing and APK packaging.

The native changes add a separate 250 ms hold-before-drag selection threshold,
classify holds from event timestamps, allow a second finger to upgrade a
stationary hold, and prevent an existing pan from becoming a cancel when
another finger joins. Regression tests and comparison documentation accompany
those changes. Most controls and all game-engine systems remain upstream work.

## Control-design reference — MYSOREZ / GeneralsX lineage

- Reference project: https://github.com/MYSOREZ/GeneralsZH-Android-Port
- Inspected revision: `e707c1dafe597cfb92e75f942ae320741d59e047`.
- Credit: **MYSOREZ and the contributors to that port**, and the GeneralsX
  lineage acknowledged by its own project.
- Used as an interaction-design reference for drag-to-pan, hold-then-box
  selection and multi-finger cancellation. **No Generals engine source,
  rendering code or game assets were incorporated into this native fork.**
  The reference project's licenses remain its own; it is not the code license
  for this fork.

## Comparison research — Mustafa-MS

- Compared project: https://github.com/Mustafa-MS/RedAlert2-Android
- Inspected revision: `b332c3f78ef4cff46ce4f5b6a93703b970beb3a2`.
- Credit: **Mustafa-MS and that repository's upstream contributors** for their
  separately developed Android/WebView port.
- Studied for Android architecture, resource import and touch handling.
  **No code from that repository is included in this native APK or source
  fork.** A separate local web experiment is discussed in the comparison
  report; it is not part of this native release.

## Behavioral research — Gagarin

- Reference: https://github.com/r-melvin/gagarin
- Research snapshot: `03f19f62e61c731ff804cfbd0471d2d11c931aa5`.
- Its reconstruction is GPLv3 with its stated additional terms. It remains a
  behavioral research reference, not a source-code dependency. No function
  bodies, headers or reconstructed code from Gagarin were incorporated into
  this fork in this release. This does not certify all upstream code provenance.
- Its matching statistics do not prove full gameplay or rendering parity.

## Bundled and development components

- **SDL 2.30.9** — Sam Lantinga and SDL contributors; zlib license. Supplies
  the native platform library and SDL Android Java glue.
- **stb_image_write** — Sean Barrett and contributors; MIT alternative used
  by upstream for screenshot output.
- **LLVM libc++** — LLVM contributors; Apache-2.0 with LLVM Exceptions.
  `libc++_shared.so` comes from Android NDK r27c.
- **doctest** — Viktor Kirilov and contributors; MIT; test executable only.
- Other upstream build/runtime notices remain in
  [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Those notices are shipped
  with the APK and displayed through the engine's existing About page.

## Game content and trademarks

Command & Conquer: Red Alert 2 and Yuri's Revenge are Electronic Arts / Westwood
titles. Their names and game content belong to their respective owners.
This is an unofficial fan project, not affiliated with or endorsed by EA.
The APK contains no retail game archives; players supply files from their
own legally obtained copy. Credits do not grant rights to redistribute those
game files.
