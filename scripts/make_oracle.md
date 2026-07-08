# Generating the byte-for-byte SHP oracle

The oracle test (`tests/test_oracle.cpp`) compares `ra2assets`' indexed SHP
output against ground truth decoded by **XCC's own decoder**. The oracle set
lives in the git-ignored `oracle/` directory and is **never committed** (it is
derived from copyrighted game assets).

There are two ways to produce the ground-truth `frameNN.raw` files. The
**scriptable path (A)** is what was actually used for Task 9 and is
reproducible with no GUI. The **GUI path (B)** is the original XCC Mixer route
and is kept as a documented alternative.

Either way, the comparison contract is identical and unchanged:

* `oracle/<framedir>/frameNN.raw` — headerless, row-major, **top-down**,
  frame-local `cx*cy` palette indices (one byte per pixel). Empty frames
  (`cx==cy==0`) are 0-byte files.
* `oracle/manifest.txt` — one asset per line, whitespace-separated:
  `<mix-name-relative-to-install> <shp-path-within-mix> <frame-dir-under-oracle>`
  e.g. `ra2.mix conquer.mix/gi.shp ra2/gi`

> Extraction vs decode: the SHP bytes are extracted by **ra2assets** (already
> corroborated by MIX id resolution + offset tiling + Task 8). The decode of
> those bytes into indices is done by **XCC's code**. The byte-for-byte gate
> therefore specifically validates *our decoder* against XCC's, holding
> extraction constant.

---

## Path A — scriptable, from XCC's own source (used for Task 9)

XCC Mixer (the GUI) is not installed in this environment and cannot be driven
autonomously, so the oracle is built from XCC's **unmodified decode source**
(the same code the GUI's PCX/PNG export uses). No GPL code enters this repo:
the tool is built and kept OUTSIDE it.

### A.1 Build the standalone oracle tool

Location (OUTSIDE this repo, because XCC is GPLv3): `D:\ra2-re\oracle-tool\`

Files:
* `xcc_shp_dump.cpp` — the tool. Embeds, **verbatim** and with provenance
  comments, from the XCC clone at `D:\ra2-re\reference\xcc` (commit `6f91bf8`):
  * `struct t_shp_ts_header`       — `misc/cc_structures.h`
  * `struct t_shp_ts_image_header` — `misc/cc_structures.h`
  * `int decode3(const byte*, byte*, int cx, int cy)` — `misc/shp_decode.cpp`
* `xcc_shim.h` — **types-only** portability shim (zero decode logic): defines
  `byte`. MinGW-w64 g++ already accepts the MSVC `__int8/16/32/64` sized types
  and `#pragma pack`, so nothing else needs aliasing.

The tool's per-frame dispatch mirrors XCC's `Cshp_ts_file` /
`Cshp_ts_decoder::decode()` / `Cshp_ts_file::extract_as_pcx()`
(`misc/shp_ts_file.{h,cpp}`):
* `is_compressed(i)` is `image_header.compression & 2`;
* compressed frames go through `decode3(get_image(i), buf, cx, cy)`;
* uncompressed frames are a plain `cx*cy` copy from `get_image(i)`.

The only intentional divergence from `extract_as_pcx` is the output shape:
`extract_as_pcx` composites each frame into a **global `cx*cy` canvas** at
`(x,y)`; the tool instead writes the **frame-local `cx*cy` buffer**, because
that is exactly what `ra2assets::ShpFile::decode_indexed(i)` returns. The pixel
decode (`decode3` / raw copy) is byte-identical to XCC either way.

Build (static-linked so it runs without MinGW DLLs on PATH):

```sh
cd /d/ra2-re/oracle-tool
g++ -std=c++17 -O2 -Wall -static -static-libgcc -static-libstdc++ \
    xcc_shp_dump.cpp -o xcc_shp_dump.exe
```

### A.2 Extract the raw SHP bytes with ra2assets

`ra2extract` gained a `--raw` mode that dumps the raw extracted bytes of any
nested asset (see `tools/ra2extract.cpp`):

```
ra2extract <mix> --raw <asset-path> <out.bin>
```

```sh
RAW=/path/to/scratch/raw           # anywhere outside the repo
cd /d/ra2-android
./build/ra2extract.exe "D:/Red Alert 2/ra2.mix" --raw conquer.mix/gi.shp       "$RAW/gi.shp"
./build/ra2extract.exe "D:/Red Alert 2/ra2.mix" --raw conquer.mix/engineer.shp "$RAW/engineer.shp"
./build/ra2extract.exe "D:/Red Alert 2/ra2.mix" --raw conquer.mix/dog.shp      "$RAW/dog.shp"
./build/ra2extract.exe "D:/Red Alert 2/ra2.mix" --raw conquer.mix/mouse.shp    "$RAW/mouse.shp"
```

### A.3 Dump the oracle frames with XCC's decoder

```sh
OT=/d/ra2-re/oracle-tool/xcc_shp_dump.exe
ORD=/d/ra2-android/oracle
"$OT" "$RAW/gi.shp"       "$ORD/ra2/gi"
"$OT" "$RAW/engineer.shp" "$ORD/ra2/engineer"
"$OT" "$RAW/dog.shp"      "$ORD/ra2/dog"
"$OT" "$RAW/mouse.shp"    "$ORD/ra2/mouse"
```

Each run prints per-frame stats (compressed / uncompressed / empty counts).
Then write `oracle/manifest.txt` (already checked in below as the reference
set):

```
ra2.mix conquer.mix/gi.shp ra2/gi
ra2.mix conquer.mix/engineer.shp ra2/engineer
ra2.mix conquer.mix/dog.shp ra2/dog
ra2.mix conquer.mix/mouse.shp ra2/mouse
```

---

## Path B — XCC Mixer GUI (documented alternative)

If you have XCC Mixer installed:

1. Open the target MIX in XCC Mixer.
2. Select a fixed asset list (record the names, e.g. a small infantry SHP + its
   palette).
3. Export each SHP frame as an **8-bit indexed** image (BMP/PCX/PNG — whatever
   XCC offers) into `D:\ra2-android\oracle\<mixname>\<shpname>\`.
4. Convert every exported image to a headerless raw index dump
   (`frameNN.raw`: row-major `w*h` bytes, top-down) with
   `scripts/oracle_to_raw.py` (requires `pip install pillow`):

   ```sh
   python scripts/oracle_to_raw.py oracle/ra2/gi/*.png
   ```

   Pillow normalizes BMP/PCX headers, row padding, and bottom-up row order on
   load, so the test only ever compares clean top-down `w*h` index bytes.
5. Also export the reference **RGBA** PNG for one frame to calibrate palette
   scaling (see below).

> Caveat for Path B: XCC's frame export composites into the **global** canvas.
> To match `decode_indexed` (frame-local), export/crop to the frame's own
> `cx*cy` (or generate the oracle via Path A, which is frame-local by
> construction). The scriptable Path A avoids this ambiguity entirely.

---

## Step-5 note: XCC's 6-bit → 8-bit palette scaling (RECORD ONLY)

The oracle gate above is on the **indexed** buffers and is therefore
palette-independent. The palette-scaling constant only matters for the
*separate* RGBA path (Task 6) and is recorded here for Task 11's RE decision —
**our `expand6` is intentionally NOT changed by Task 9.**

Observed in XCC `misc/palet.cpp`, `convert_palet_18_to_24()` (the function
`Cshp_ts_file::extract_as_pcx` calls before writing a PCX/PNG):

```cpp
d[i].r = (s[i].r & 63) * 255 / 63;   // 6-bit value -> 8-bit
d[i].g = (s[i].g & 63) * 255 / 63;
d[i].b = (s[i].b & 63) * 255 / 63;
```

So XCC's display/export path uses **`v * 255 / 63`** (exact linear rescale of a
6-bit channel to 0..255), **not** a plain `v << 2`. (`v << 2` alone maxes at
252 for `v==63`, never reaching 255.)

`ra2assets`' current `expand6` (`src/pal.cpp`) uses bit-replication
`(v << 2) | (v >> 4)`.
The two agree at the endpoints (`v==0 -> 0`, `v==63 -> 255`) and differ by at
most 1 for mid values (e.g. `v==32`: XCC `129`, bit-replication `130`). This is
a sub-LSB difference in the RGBA path and does not affect the indexed oracle
gate. Task 11's RE of the game's own renderer decides the final constant.
