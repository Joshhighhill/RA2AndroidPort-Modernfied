# TMP (TS/RA2) isometric theater tiles — reverse-engineering notes

Decoder: `ra2assets::TmpFile` (`include/ra2assets/theater.hpp`,
`src/theater.cpp`). Decodes `.tmp`-format isometric theater templates — in RA2
the files carry a theater extension (`.tem` temperate, `.sno` snow, `.urb`
urban) and live in `isotemp.mix` / `temperat.mix` (themselves nested in
`ra2.mix`). Each template is a `cblocks_x * cblocks_y` grid of diamond tiles.

## File layout (confirmed against XCC `misc/cc_structures.h` + real assets)

`t_tmp_ts_header` — 16 bytes, 4 × little-endian int32:

| offset | field       | ra2assets     |
|-------:|-------------|---------------|
| 0      | cblocks_x   | `cblocks_x_`  |
| 4      | cblocks_y   | `cblocks_y_`  |
| 8      | cx          | `cx_` (tile width, 48 or 60) |
| 12     | cy          | `cy_` (tile height, cx/2)    |

`count = cblocks_x * cblocks_y`.

Immediately after the header: an `int32[count]` **index table**. Entry `i` is a
byte offset into the file to tile `i`'s image header; **0 means the tile is
absent** (decodes to an all-transparent buffer).

`t_tmp_image_header` — **52 bytes**, at `index[i]`:

| offset | type       | field(s)                                             |
|-------:|------------|------------------------------------------------------|
| 0..35  | 9 × int32  | x, y, extra_ofs, z_ofs, extra_z_ofs, x_extra, y_extra, cx_extra, cy_extra |
| 36..39 | uint32     | bitfield word: bit0 has_extra_data, bit1 has_z_data, bit2 has_damaged_data |
| 40     | int8       | height                                               |
| 41     | int8       | terrain_type                                         |
| 42     | int8       | ramp_type                                            |
| 43..48 | 6 × uint8  | radar colors (RGB left, RGB right)                   |
| 49..51 | int8[3]    | pad                                                  |

The 3 flag bits are declared in XCC as `unsigned int … : 1`, so their storage
unit is a full 4-byte word (offset 36..39), **not** a single byte. This is what
makes the header 52 bytes rather than 49.

**Header size = 52 confirmed three independent ways:**
1. `sizeof(t_tmp_image_header)` compiled from XCC's verbatim struct prints
   `52` (and `sizeof(t_tmp_ts_header)` prints `16`).
2. XCC `Ctmp_ts_file::get_image(i)` = `image_header + 1` (pixels start exactly
   one struct past the header); `get_cb_diamond()` = `cx*cy/2`.
3. OpenRA `TmpTSLoader.IsTmpTS` asserts the `z_ofs` field (image-header byte
   +12) equals `cx*cy/2 + 52`. Verified against the real `clear01.tem`
   (cx=60, cy=30): `z_ofs == 952 == 900 + 52`.

Tile pixels (the **diamond**) start at `index[i] + 52` and are `cx*cy/2` bytes,
one palette index per pixel. (Present tiles are typically followed by a second
`cx*cy/2` z-buffer diamond and optional extra graphics — z/extra data are out
of scope for this indexed decoder and are not read.)

## Diamond unpack geometry

The `cx*cy/2` bytes are an isometric diamond that must be unpacked into a
`cx*cy` rectangle with the four corner triangles left transparent (0). Row `y`
(0-based) holds an opaque run of `width(y)` bytes, horizontally centered
(left margin `= (cx - width)/2`):

* top half `y = 0 .. cy/2-1`: `width` widens by 4 per row → `4, 8, …, cx`
  (the widest row, `width == cx`, is the last top row `y = cy/2-1`);
* bottom half `y = cy/2 .. cy-1`: `width` narrows by 4 per row → `cx-4, …, 0`.

The run widths sum to `(cy/2)*cx = cx*cy/2`, so the pixel bytes are consumed
exactly. This geometry is only self-consistent when `cx` is a multiple of 4 and
`cy == cx/2` (max width `4*(cy/2) == cx`); `TmpFile::load` rejects anything
else, matching XCC's `is_valid` (`cx ∈ {48,60}`, `2*cy == cx`).

`ra2assets`' `decode(i)` implements this with a single row loop computing
`left = (cx - width)/2` explicitly. It is a **clean-room** reimplementation from
the format description above — it does **not** copy XCC's `decode_tile` (which
uses a two-loop `memcpy` with running `x`/`cx` pointers). The two express the
same centered-diamond geometry; correctness is pinned by the byte-for-byte
oracle below, not by shared code. (OpenRA's `UnpackTileData` uses the same
`(size.Width - width)/2` centered formulation, an independent corroboration.)

## Byte-for-byte XCC oracle (the correctness gate)

`tests/test_theater.cpp` ("tmp tile output matches XCC oracle byte-for-byte")
compares `TmpFile::decode(i)` against ground truth from **XCC's own
`decode_tile`**, built verbatim into a standalone tool **outside** this repo:
`D:\ra2-re\oracle-tool\xcc_tmp_dump.cpp` (embeds `t_tmp_ts_header`,
`t_tmp_image_header`, and `decode_tile` verbatim from XCC commit clone at
`D:\ra2-re\reference\xcc`; GPLv3, never committed here). The tool walks the
index table exactly as `Ctmp_ts_file` does and writes each tile's `cx*cy`
indexed buffer as `tileNN.raw` (absent tiles → `cx*cy` zeros).

Assets extracted by `ra2extract --raw` from the real `ra2.mix`
(`isotemp.mix/…`), holding extraction constant; only the decode is compared.

**Result: 4 templates, 15 tiles (13 present + 2 empty), 0 byte mismatches.**

| template (`isotemp.mix`) | mix id (in isotemp.mix) | grid | px    | tiles | present | empty |
|--------------------------|-------------------------|------|-------|------:|--------:|------:|
| `clear01.tem`            | `B157504F`              | 1×1  | 60×30 |     1 |       1 |     0 |
| `cliff01.tem`            | `37FBE2D9`              | 2×3  | 60×30 |     6 |       4 |     2 |
| `shore01.tem`            | `F987187E`              | 2×2  | 60×30 |     4 |       4 |     0 |
| `water01.tem`            | `E05D7011`              | 2×2  | 60×30 |     4 |       4 |     0 |
| **total**                |                         |      |       | **15** | **13** | **2** |

`clear01.tem` is the temperate "Clear"/grass master template (tile id 0 in the
theater tileset). `cliff01.tem` exercises the multi-tile grid **and** absent
index entries (its 2 empty slots). All present tiles matched XCC's `decode_tile`
byte-for-byte; every tile's size matched. The test skips gracefully (doctest
`MESSAGE`, no failure) when `oracle/tmp_manifest.txt` is absent, and a final
`CHECK(tiles_checked > 0)` closes the silent-skip failure mode.

Reproduce (PowerShell, from `D:\ra2-android`):

```
# 1. extract raw .tem bytes with ra2assets (outside the repo)
$RAW="<scratch>/raw"
./build/ra2extract.exe "D:/Red Alert 2/ra2.mix" --raw isotemp.mix/clear01.tem "$RAW/clear01.tem"
#   … cliff01.tem, shore01.tem, water01.tem likewise
# 2. build + run the GPL oracle tool (outside the repo)
g++ -std=c++17 -O2 -static -static-libgcc -static-libstdc++ `
    D:/ra2-re/oracle-tool/xcc_tmp_dump.cpp -o D:/ra2-re/oracle-tool/xcc_tmp_dump.exe
D:/ra2-re/oracle-tool/xcc_tmp_dump.exe "$RAW/clear01.tem" D:/ra2-android/oracle/tmp/clear01
#   … one run per template; then write oracle/tmp_manifest.txt (checked-in path list)
# 3. gate: run the suite with the install dir set
$env:RA2_INSTALL_DIR="D:\Red Alert 2"; ./build/ra2assets_tests.exe -tc="tmp*"
```

## Clean-room attestation

`include/ra2assets/theater.hpp` and `src/theater.cpp` were written from the
format description in this file (header field list from `cc_structures.h`, which
is a data-layout description, plus the diamond geometry above). No GPL XCC
decode code (`decode_tile` / `Ctmp_ts_file::decode` / `draw`) was copied into
the repo. XCC's GPL source is used only OUTSIDE the repo, as a byte-oracle, in
`D:\ra2-re\oracle-tool\xcc_tmp_dump.cpp`.
