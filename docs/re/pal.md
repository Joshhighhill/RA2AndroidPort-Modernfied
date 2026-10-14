# Palette (6-bit) — reverse-engineering notes

TS/RA2 palettes are 768-byte files: 256 entries × 3 bytes (R,G,B), each channel a
**6-bit** value (0–63). Displaying them requires expanding 6-bit → 8-bit. This
file records what our loader does, what the oracle can and cannot tell us, and
the **Task 11 authoritative finding** for how the game itself scales — plus the
resulting decision about `expand6`.

## Our loader (`src/pal.cpp`)

```cpp
static uint8_t expand6(uint8_t v){ v &= 63; return (uint8_t)(v << 2); }
```

Game-accurate scaling: a plain left-shift, so `0 → 0` and `63 → 252` (never
reaches 255). `Palette::load` applies it to all 256 entries and sets `a = 255`.

## Three scalings exist — and they differ

| Source | Formula | `v=63` → | `v=31` → | Notes |
|--------|---------|:--------:|:--------:|-------|
| **Game** (`gamemd.exe`) | `v << 2` | **252** | 124 | tops out at 252, never 255 |
| **XCC export** (`palet.cpp`) | `v * 255 / 63` | 255 | 125/126 | integer-rounded full range |
| **Bit-replication** (previous `expand6`) | `(v<<2)\|(v>>4)` | 255 | 124 | full range, endpoints exact |

They agree at `v=0`, agree closely mid-range (±1), and diverge at the top:
`<<2` caps every channel at 252 while the other two reach 255.

## Task 11 authoritative finding — the game uses `v << 2`. Confidence: **Medium-High**

Two lines of evidence from the RE, converging on plain `<< 2`:

1. **YRpp states it outright.** `FileSystem.h::AllocatePalette` (and the sibling
   `LoadPALFile`) expand a freshly-read palette with, verbatim:

   ```cpp
   // allocates a new palette with the 6 bit colors converted to 8 bit
   // (not the proper way. how the game does it.)
   for (auto& color : ret->Entries) { color.R <<= 2; color.G <<= 2; color.B <<= 2; }
   ```
   The comment *"not the proper way — how the game does it"* is a direct
   assertion by the YR-modding community (who hook this binary daily) that the
   engine expands with `<< 2`, deliberately *not* the "proper" full-range scale.

2. **The binary corroborates it downstream.** The palette-to-surface shade-table
   builder `FUN_004bba20` (called from the `ConvertClass` ctor `FUN_0048e650`)
   consumes palette channels **as already-8-bit values**:

   For each palette entry it multiplies the channel byte (`pal[i].R`, etc.)
   by a 16.16 fixed-point `shade_factor` (≈1.0 at full brightness) and
   shifts right by 16, clamps any result above `0xFE` to `0xFF` (255), and
   then packs (r,g,b) into the 16-bit surface pixel.
   At full brightness `shade_factor == 0x10000` (1.0), so the surface value is
   just `pal[i].R` unchanged — i.e. the 6→8 expansion already happened before
   `ConvertClass`, at `.pal` load. The **clamp-at-255 with a maximum possible
   input of 252** (`63<<2`) is consistent with `<<2` feeding it; a `*255/63`
   loader would already have produced 255 and needed no headroom there. The
   brightest *shades* (`shade_factor` up to ≈2.0) are what actually hit the
   clamp — additional confirmation the stored channels are 8-bit, not 6-bit.

**Honesty note on confidence.** This is Medium-High, not High, because the
literal `shl reg, 2` at the point a `.pal` file is loaded was **not isolated** in
this pass: the `.pal` load path is reached through DirectDraw/`ConvertClass`
indirection and the filename strings are not referenced by absolute address, so
the constant-anchor techniques that nailed the CRC did not pin the exact
instruction. The finding rests on YRpp's explicit assertion **plus** the
downstream corroboration above. To upgrade to High, disassemble the `.pal`
`Read_File`→expand loop directly.

## Decision: `expand6` is now `v << 2` — APPLIED

Per the final whole-branch review, the user chose to make the library
game-accurate rather than defer this. `expand6` in `src/pal.cpp` has been
changed from bit-replication (`(v<<2)|(v>>4)`) to plain `v << 2`, matching
`gamemd.exe`'s own expansion per the Task 11 finding above. `tests/test_pal.cpp`
was updated to assert the new values (e.g. `63 → 252`, `32 → 128`).

- **The primary correctness gate is unaffected.** Task 9's oracle compares
  **indexed** buffers (byte-for-byte, 2044 frames); it is blind to the RGBA
  scaling, so this change does not touch the verified pipeline. The full suite
  re-runs green after the change.
- **RGBA output now matches the game's own on-screen rendering**, rather than
  XCC's exporter convention (`*255/63`, full-range). Anyone diffing PNGs
  produced by this library against XCC's exports should expect the brightest
  channel values to differ by up to ~3/255 (252 vs 255) — this is expected and
  intentional, not a bug.
- This was a semantic change to shared code (`src/pal.cpp`), made explicitly
  and reviewed as its own item in the final-fixes batch, per the user's
  decision to prioritize game-accuracy over full-range cosmetics.
