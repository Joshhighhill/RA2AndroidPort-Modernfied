# Red Alert 2 → Android Native Port — Sub-Project 1: Asset Pipeline (MIX→PAL→SHP)

**Date:** 2026-07-08
**Status:** Approved design, ready for implementation planning
**Program:** Native ARM64 Android port of Red Alert 2 / Yuri's Revenge

---

## Context & Motivation

The user wants to port Red Alert 2 (Yuri's Revenge) to Android as a **true native ARM64
port**, in the spirit of the [Generals-Mac-iOS-iPad](https://github.com/ammaarreshi/Generals-Mac-iOS-iPad)
project.

**Critical difference from that project:** the Generals port succeeded because EA released
the actual Generals: Zero Hour C++ source under GPL v3 (Feb 2025). That work was
essentially "recompile the real engine for ARM64, swap DirectX 8 → DXVK → Metal, add
touch controls."

**Red Alert 2 has no released source code** — it was excluded from EA's 2025 release and is
widely reported to be lost. The working directory `d:\Red Alert 2\` is a retail install:
`gamemd.exe` (Yuri's Revenge), `ra2.exe`/`game.exe`, and Blowfish-encrypted `.mix` asset
archives — compiled 32-bit x86 binaries, no source, no symbols.

RA2 is also **not** a 3D engine like Generals. It is a **2D isometric sprite engine**:
DirectDraw blitting, SHP sprites, VXL voxel vehicles, TMP terrain tiles, deterministic
lockstep netcode. The Generals graphics-translation approach does not transfer.

### Chosen foundation: Ghidra decompilation

The port will be built by **reverse-engineering `gamemd.exe` with Ghidra** and
reimplementing the engine as portable, ARM64-compilable C++. The single biggest asset is
that we are **not** starting from zero: the community has spent ~20 years mapping this exact
binary — **YRpp** (headers for thousands of the engine's classes/structs/vtables),
**Ares** and **Phobos** (runtime engine extensions), and **Syringe** (their DLL injector).
These do not provide recompilable source, but they provide a detailed *map* of where
everything lives, which makes Ghidra work dramatically faster and more reliable.

### Methodology split (program-wide principle)

Because the target ISA is ARM64, byte-*matching* decompilation is impossible. Therefore:

- **Matching-grade fidelity** for the **simulation/logic** (preserves gameplay feel and
  multiplayer lockstep determinism) — enforced in later phases.
- **Functional reimplementation**, verified by output/behavior, for the **platform layer**
  (graphics/audio/input/net) and for **data-format parsers** (this sub-project).

---

## Program Roadmap (context only — not all in this spec)

| Phase | Sub-project | Status |
|-------|-------------|--------|
| **P1** | **Asset pipeline — thin vertical: MIX → PAL → SHP** | **THIS SPEC** |
| P2 | Remaining assets: TMP terrain, VXL/HVA voxels, CSF strings | future spec |
| P3 | Deterministic simulation core (decompiled, replay-verified) | future spec |
| P4 | Platform layer: DirectDraw→Vulkan/SDL, DirectSound→OpenAL, DirectInput→touch, DirectPlay→netcode | future spec |
| P5 | Android shell: APK packaging, asset routing, touch controls, app lifecycle | future spec |

Each sub-project gets its own spec → plan → implementation cycle. This document specs **P1 only.**

---

## Sub-Project 1 Goal

Deliver two things:

1. **`ra2assets`** — a portable, platform-agnostic C++ library that turns an encrypted MIX
   archive on disk into a decoded, palettized RGBA bitmap of a real in-game unit sprite.
2. **A proven, documented `decompile → reimplement → verify` workflow** — the reusable
   "machine" every later phase depends on.

The thin vertical (MIX→PAL→SHP) is chosen deliberately: these are the **only** formats in
the entire game that are already **fully publicly documented and reference-implemented**
(XCC source, OpenRA, ModEnc; Blowfish MIX decryption fully known). That lets us **calibrate
the Ghidra methodology on ground where we already know the correct answer**, measuring our
reverse-engineering accuracy before we reach undocumented formats and logic.

### Success criteria

1. `ra2assets` opens an encrypted MIX from the user's install, extracts a named SHP and a
   palette, decodes the SHP frames, and produces RGBA bitmaps.
2. Output matches an oracle (XCC Mixer and/or OpenRA) **byte-for-byte** on both the 8-bit
   indexed pixels and the final RGBA, across a fixed set of well-known assets.
3. The library **cross-compiles cleanly for Android `arm64-v8a`** via the NDK (in addition
   to host x86-64), proving portability — even though no Android app exists yet.
4. The Ghidra workflow is documented end-to-end with at least the MIX, PAL, and SHP loader
   functions located, decompiled, reconstructed, and confidence-rated.

---

## Workflow (applied per target function)

1. **Locate** loader functions in `gamemd.exe` using published YRpp/Ares address maps
   (`MixFileClass`, palette loading, `SHPStruct`/`SHPReference`, etc.).
2. **Decompile** in Ghidra (headless analyzer + scripting where practical) — study control
   flow, structs, and constants.
3. **Reconstruct** as clean portable C++ (functional, not byte-matching).
4. **Verify** output equality against oracles (XCC Mixer / OpenRA extracting the *same*
   files) — byte-for-byte on indexed pixels and final RGBA. Cross-check the reverse-
   engineered algorithm against public format documentation.
5. **Document** confidence level and any divergence from docs or oracle.

---

## Library Architecture

`ra2assets` is a clean, layered, platform-agnostic C++ library (C++17 or C++20). No
DirectX, no OS dependencies. Three focused, independently testable units:

### Unit: `mix` — archive container (read-only)
- Blowfish decryption reimplemented from RE, cross-checked against reference Blowfish.
- Handles encrypted and checksummed MIX flags.
- Filename → entry-ID hashing (RA2/TS scheme).
- Local-mix-database support (filename resolution) and nested-MIX (MIX inside MIX).
- **Interface:** open (file or in-memory blob) → enumerate entries by ID → read entry bytes.

### Unit: `pal` — palette
- 256-color, 6-bit RA2 palette (channel values 0–63, scaled to 0–255).
- **Interface:** load → 256 RGBA entries.

### Unit: `shp` — sprite (TS/RA2 SHP variant, not the TD/RA1 variant)
- File header + per-frame headers.
- Frame compression modes: uncompressed / RLE-zero / LCW.
- Decode frame N to an 8-bit indexed bitmap; palettize with a `pal` to RGBA.
- **Interface:** load → frame count & dimensions → decode frame N (indexed) → palettize.

Each unit has one clear purpose, a well-defined interface, no cross-unit hidden state, and
is testable in isolation.

---

## Verification Harness & Build

### CLI tool: `ra2extract`
Open a MIX → list entries → extract a SHP → decode frames → apply a palette → write PNG.
This is the human-visible proof that the pipeline works end-to-end.

### Automated oracle tests
For a fixed set of well-known assets, assert `ra2assets` output equals XCC/OpenRA output
byte-for-byte (indexed pixels and final RGBA). Test framework: doctest or Catch2
(header-only). Oracle outputs and game assets are generated/kept **locally**, never
committed (see Legal).

### Build system
- **CMake**, single source tree, two targets from the same code:
  - **Host x86-64** (Windows/Linux) — fast dev/test/oracle comparison.
  - **Android `arm64-v8a`** via the NDK toolchain file — proves portability now.
- External dependencies limited to: `stb_image_write` (single-header PNG writer) and a
  header-only test framework. Blowfish is reimplemented in-house.

---

## Scope Boundaries

### Out of scope for Sub-Project 1 (YAGNI)
- VXL/HVA voxels, TMP terrain, CSF strings (Phase 2).
- Any rendering/GUI, Vulkan/SDL, audio, input, networking (Phase 4).
- The Android APK / app shell (Phase 5). On-device execution is at most a smoke test that
  the `.so` loads.
- MIX **writing**/repacking — read-only only.
- Simulation logic and the game loop (Phase 3).

### Legal / ethical stance
This follows the same interoperability-and-reimplementation basis the OpenRA and Generals-
port communities operate under. **No game assets and no game binary are committed to the
repository.** The user supplies their own legally-owned copy (present at `d:\Red Alert 2\`).
Reconstructed code is functional reimplementation, not redistribution of EA's binary.

---

## Prerequisites (confirm/install at plan kickoff)
- Ghidra + a compatible JDK.
- A C++ toolchain, CMake, and the Android NDK.
- An oracle: XCC Mixer (Windows) and/or the OpenRA source for cross-checking.
- Target binary available: `d:\Red Alert 2\gamemd.exe` (Yuri's Revenge). Confirm exact
  build/version so RE addresses from YRpp/Ares match.

---

## Risks & Mitigations
- **Ghidra C++ decompilation quality (vtables, `thiscall`, name mangling).** Mitigated:
  these are low-level parsers, and YRpp supplies struct layouts and function addresses.
- **Formats are already known, so the RE is calibration rather than discovery.** This is
  intentional — the value of this slice is a validated methodology, not new format
  knowledge.
- **Fidelity pressure is low here** (parsers, verified by output equality); the harder
  determinism requirements arrive with the simulation core in Phase 3.
- **MIX encryption/hashing edge cases** (checksummed archives, local mix database, nested
  MIX). Mitigated by testing against multiple real archives from the install and comparing
  to oracles.
