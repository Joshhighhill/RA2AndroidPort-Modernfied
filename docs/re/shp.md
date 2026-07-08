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
