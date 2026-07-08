# SHP (TS/RA2) format — reverse-engineering notes

(Full RE write-up lands in Task 11. This file currently records the header
layout confirmed against XCC and the Task 9 byte-for-byte oracle results.)

## Header layout (confirmed against XCC `misc/cc_structures.h`, commit 6f91bf8)

File header (`t_shp_ts_header`, 8 bytes, packed):

| offset | type  | field     | ra2assets read            |
|-------:|-------|-----------|---------------------------|
| 0      | i16   | zero      | (ignored)                 |
| 2      | u16   | cx        | `cx_` (global width)      |
| 4      | u16   | cy        | `cy_` (global height)     |
| 6      | u16   | c_images  | frame count               |

Per-frame image header (`t_shp_ts_image_header`, 24 bytes, packed):

| offset | type  | field       | ra2assets read     |
|-------:|-------|-------------|--------------------|
| 0      | i16   | x           | `h.x`              |
| 2      | i16   | y           | `h.y`              |
| 4      | i16   | cx          | `h.w`              |
| 6      | i16   | cy          | `h.h`              |
| 8      | i32   | compression | `h.flags`          |
| 12     | i32   | unknown     | (ignored)          |
| 16     | i32   | zero        | (ignored)          |
| 20     | i32   | offset      | `h.offset`         |

* **Compression flag:** XCC's `is_compressed(i)` is `compression & 2`. Our
  decoder branches on `flags & 2u`: set → RLE-zero (XCC "format 3", `decode3`);
  clear → uncompressed straight `cx*cy` copy.
* **Frame-local decode:** `decode_indexed(i)` returns a frame-local `cx*cy`
  buffer (compositing into the global `(cx,cy)` canvas at `(x,y)` is a separate
  concern, not part of the decode gate).
* **Empty frames:** `cx==cy==0` with `offset==0` decode to a 0-length buffer.

## Task 9 oracle: byte-for-byte parity with XCC's decoder

`tests/test_oracle.cpp` compares `ShpFile::decode_indexed(i)` against ground
truth produced by **XCC's own `decode3`** (built verbatim into a standalone
tool outside this repo — see `scripts/make_oracle.md` and the Task 9 report).
The oracle set (git-ignored `oracle/`) was generated from the real `ra2.mix`.

**Result: 4 SHPs, 2044 frames, 0 byte mismatches.**

| SHP (conquer.mix)     | global | frames | decode3 | uncompressed | empty |
|-----------------------|--------|-------:|--------:|-------------:|------:|
| `gi.shp`              | 78×66  |    744 |     293 |           79 |   372 |
| `engineer.shp`        | 76×80  |    506 |     226 |           27 |   253 |
| `dog.shp`             | 74×72  |    344 |     146 |           26 |   172 |
| `mouse.shp`           | 55×43  |    450 |       0 |          450 |     0 |
| **total**             |        | **2044** | **665** |      **582** | **797** |

Coverage rationale: `gi/engineer/dog` exercise the RLE-zero `decode3` path
heavily and include many empty (shadow-slot) frames; `mouse.shp` is entirely
uncompressed with no empty frames, exercising the plain-copy path in isolation.
Every non-empty frame's indices matched XCC byte-for-byte, and every frame's
size matched. No `decode80`/LCW (format-80) frames appear in the chosen set —
XCC selects `decode3` on `compression & 2` and a plain copy otherwise; none of
these assets use a format-80 branch, so our decoder needs no LCW path for them.

The test **skips gracefully** (doctest `MESSAGE`, no failure) when `oracle/` or
`manifest.txt` is absent, so asset-free CI still builds and links. A final
`CHECK(frames_checked > 0)` closes the silent-skip failure mode.

### Palette scaling note (RECORD ONLY — not changed by Task 9)

The oracle gate is on **indexed** buffers, so it is palette-independent. For the
separate RGBA path: XCC's export (`convert_palet_18_to_24`) scales 6-bit
channels with `v * 255 / 63`; our `expand6` uses bit-replication
`(v << 2) | (v >> 4)`. They agree at the endpoints and differ by ≤1 at mid
values. This is deferred to Task 11's RE (see `scripts/make_oracle.md`).

---

## Task 11 — Ghidra location in `gamemd.exe` (YR 1.001). Confidence: **Medium**

See `docs/re/workflow.md` for method. Image base `0x00400000`; addresses are
Ghidra addresses in this build (YRpp SHP addresses drifted / landed in gaps).

### Header + frame layout: confirmed

The `SHPStruct`/`SHPFrame` layout our loader reads is confirmed against YRpp
`FileFormats/SHP.h` and the located class methods:

- `SHPStruct { WORD Type; short Width; short Height; short Frames; }` — our
  8-byte header (`zero`/Type, `cx`, `cy`, frame count) matches exactly. A live
  `SHPReference` sets `Type = 0xFFFF` (`IsReference()`), so the first `i16` we
  ignore is the file/reference discriminator.
- `SHPFrame { short Left,Top,Width,Height; DWORD Flags; ColorStruct; DWORD; int Offset; }`
  — 24 bytes, `Flags` at +8 and `Offset` at +20, matching our `FrameHdr`
  (`x,y,w,h` then `flags` at +8, `offset` at +20; the two dwords at +12/+16 we
  skip).

### Compression flag: confirmed as `Flags & 2`

YRpp `SHPStruct::HasCompression(i)` is `Flags & 2`, and XCC's `is_compressed` is
`compression & 2`. Our decoder branches on `h.flags & 2u` → RLE-zero (XCC
`decode3`), else uncompressed copy. **No divergence.**

### Located subsystem functions

| Function | Role | Notes |
|----------|------|-------|
| `FUN_0069dac0` | `SHPReference` ctor | sets `Type=0xFFFF`, tags `"NULL_SHP"`, links the global loaded-SHP list (head `DAT_00b06720`; YRpp lists it at `0xB077B0`) |
| `FUN_0069dc10` | `SHPStruct::GetData` | returns self if not a reference; else loads the raw `.shp` bytes into a shared scratch buffer (≥`0x300000` bytes) and caches by index |
| `FUN_0069e710` | `GetPixels`-side dispatch | virtual call `(*(this-0x14)->vtbl[4])()`, then reads frame flags at `+0x4d/+0x50` |
| `FUN_005b3ae0` | retrieve-by-name | for a `.shp` name, hashes via `mix_id` (see `mix.md`) and constructs the `SHPReference` above |

### Why this is Medium, not High: the decode is fused into the RLE blitters

There is **no standalone "decompress frame → indexed buffer" function** in this
binary to diff against `ShpFile::decode_indexed`. RA2 draws format-3 SHPs
directly from their compressed bytes using **RLE blitters**: `ConvertClass`
holds `Blitter* Blitters[50]` and `RLEBlitter* RLEBlitters[39]`, and
`SHPStruct::HasCompression` selects `SelectRLEBlitter` vs `SelectPlainBlitter`
per frame. The zero-run expansion (`0x00` → run byte → that many transparent
pixels) that our `decode_indexed` performs into a buffer is, in the engine,
performed inline during the blit. So a 1:1 function comparison is not
apples-to-apples — the *format* is what must match, not a shared routine.

That the format matches is proven far more strongly than a decompilation diff
could: the **Task 9 oracle** (above) decodes 2044 real frames through XCC's own
`decode3` and our decoder produces **byte-identical** indexed output, including
the per-scanline `uint16` line-length prefix (Review Correction #3) and the
`Flags & 2` selection confirmed here. The RE pins the header/flag semantics and
the subsystem entry points (Medium confidence on the *code*); the oracle pins the
*bytes* (the real correctness gate).
