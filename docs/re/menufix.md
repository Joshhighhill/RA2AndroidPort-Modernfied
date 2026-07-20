# Main-Menu Fidelity Fix — Westwood .fnt glyph format, backdrop red-tint, menu composition

Companion to `docs/re/mainmenu.md` (asset-presence + composition). This doc delivers the
three concrete render fixes the faithful-menu phase needs.

Tags: **[CONFIRMED]** = proven against the real bytes in *this* install (`D:\Red Alert 2`);
**[ANCHORED]** = from XCC source / documented RA2 / SHP headers; **[RE-FALLBACK]** = inference.

Tooling / repro paths used:
- Decoder under fix: `D:\ra2-android\src\fnt.cpp` (+ `include/ra2assets/fnt.hpp`).
- XCC reference source: `D:\ra2-re\reference\xcc\misc\fnt_file.h`,
  `D:\ra2-re\reference\xcc\misc\cc_structures.h` (struct `t_fnt_header`, line 149),
  `D:\ra2-re\reference\xcc\Mixer\XCCFileView.cpp` (`fnt_decode` line 222, `fnt_adjust`
  line 236, the font render loop lines 391-414).
- OpenRA (`D:\ra2-re\reference\OpenRA`) ships **no** Westwood `.fnt` loader (its fonts are
  TrueType via `OpenRA.Game/Graphics/SpriteFont.cs`); no cross-check available there.
- Extract: `D:\ra2-android\build\ra2extract.exe "D:\Red Alert 2\ra2.mix" --raw cache.mix/12metfnt.fnt out.bin`
  (font is `ra2.mix/cache.mix/12metfnt.fnt`, 10637 bytes). Backdrop:
  `--raw neutral.mix/mnscrnl.shp` (359008 B), palette `--raw cache.mix/palette.pal` (768 B).

---

## §1 Westwood `.fnt` glyph-bitmap format — THE fix for `fnt.cpp`  [CONFIRMED]

### 1.1 What `fnt.cpp` gets wrong (the shear)

`src/fnt.cpp` parses the header + tables correctly (glyph **count = 219**, per-glyph width,
rows, y-offset, and the u16 offset table are all right). It has **two** bugs, both in the
glyph-pixel step (lines 91-99), and **one** missing header field:

1. **Wrong bit-depth.** It reads each glyph as `w` **bytes** per row = `w` pixels
   (8bpp, `src[ry*w + rx]`). The real glyph is **4bpp packed: 2 pixels per byte**, so `w`
   bytes actually encode **`2*w` pixels** per row. Reading `w` bytes as `w` pixels lays down
   half a row per row, so every row starts half a glyph early → the classic **diagonal
   shear** the prior agent saw. (The offset-table gaps equal `w*rows` — which is why 8bpp
   *looked* plausible — but that byte count is `w` bytes/row × `rows`, i.e. 4bpp of width
   `2*w`, **not** 8bpp of width `w`.)

2. **Wrong glyph-data base.** It uses the offset-table value as an **absolute** file offset
   (`src = d + gdo`). The values are **relative to the `image_ofs` header field** (u16 at
   **0x0A**), which `fnt.cpp` never reads. Correct base = `d + image_ofs + gdo`.

3. **Advance width.** `widths_[i]` / `g.w` are the raw table value `w` (a **byte** count).
   The rendered glyph is `2*w` pixels wide, so both the bitmap width and the text-advance
   must be `2*w`.

### 1.2 The authoritative format (verified byte-for-byte on `12metfnt.fnt`)

Header (XCC `t_fnt_header`, `cc_structures.h:149`, 20 = 0x14 bytes, little-endian). Probed
values for `12metfnt.fnt` in brackets:

| off | field | type | meaning | 12metfnt |
|---|---|---|---|---|
| 0x00 | `size` | u16 | == file length (format guard) | 10637 |
| 0x02 | `id1` | u16 | 0x0002 (TS/RA2) / 0x0500 (TD) | 0x0002 |
| 0x04 | `id2` | u16 | 0x000e | 0x000e |
| 0x06 | `id3` | u16 | 0x0014 (== offset-table start) | 0x0014 |
| 0x08 | `cx_ofs` | u16 | file offset of **width** table (u8×count) | 458 |
| **0x0A** | **`image_ofs`** | **u16** | **base offset of glyph-bitmap data (ADD to table values)** | **1115** |
| 0x0C | `cy_ofs` | u16 | file offset of **height** table (u16×count) | 677 |
| 0x0E | `id4` | u16 | 0 (TS/RA2) | 0 |
| 0x10 | `zero` | u8 | 0 | 0 |
| 0x11 | `c_chars` | u8 | (unreliable on TS metal fonts — is 0 here; use table geometry) | 0 |
| 0x12 | `cy` | u8 | cell height | 16 |
| 0x13 | `cmax_x` | u8 | max advance (advisory) | 16 |

- **Glyph offset table:** `u16[count]` starting at **0x14** (= `sizeof(header)`; also equals
  `id3`). `count = (cx_ofs - 0x14)/2 = 219` (cross-checks: width table = `cy_ofs - cx_ofs = 219`
  bytes; height table `u16` = `(image_ofs - cy_ofs)/2 = 219`).
- **Width table:** `u8[count]` at `cx_ofs`. Value `cx` = **bytes per row** (NOT pixels).
- **Height table:** `u16[count]` at `cy_ofs`. **low byte = `yoff`** (top padding rows),
  **high byte = `rows`** (lit rows). Matches XCC `get_y` (`&0xff`) / `get_cy` (`>>8`).
- **Glyph bitmap:** at `image_ofs + table[i]`. `cx` bytes/row × `rows` rows. Each byte holds
  **two 4bpp pixels: low nibble = left pixel, high nibble = right pixel**. Pixel width = `2*cx`.
  Pixel value 0 = transparent; 1..15 = font colour-ramp index (12metfnt uses 0..4 — a bevel
  ramp, hence its striped "metal" look). Total bytes/glyph = `cx * rows`.

**Relation to XCC (why XCC alone wasn't enough):** XCC's `fnt_decode`
(`XCCFileView.cpp:222`) has the right *core idea* — 4bpp, low-nibble-then-high-nibble:
```c
void fnt_decode(const byte* r, byte* w, int cx, int cy) {
    while (cy--) {
        for (int x = 1; x < cx; x += 2) { *w++ = *r & 0xf; *w++ = *r++ >> 4; }
        if (cx & 1) *w++ = *r++ & 0xf;
    }
}
```
But XCC treats `cx` as the **pixel** width (reads `ceil(cx/2)` bytes/row) and uses an
**absolute** table offset (`get_image` = `data()+table[i]`). Fed the real RA2 `12metfnt`
those two assumptions produce a **sheared** glyph (verified — see 1.3). The RA2 TS-era metal
font stores `cx` as the **byte** width (pixel width `2*cx`) and makes the table **relative to
`image_ofs`**. XCC's `fnt_decode` is calibrated for the older TD/RA1 faces; the nibble packing
is identical, the stride and offset base are not. `fnt_adjust` (line 236) is only XCC's
preview invert (`v ? 0 : 0xff`) — **ignore it**; keep the raw 0..15 ramp values.

### 1.3 Hand verification (real bytes)

`'A'` (index 65): `cx=7`, `rows=10`, `yoff=2`, `image_ofs+table[65] = 1115+2909 = 4024`.
Decoded `cx`-bytes/row, 2px/byte (0=`.`, ramp value shown):
```
..2.2.2.2.3...      2.2.........2.2.   <- 'H' (cx=8 -> 16px): two bevelled
2.3.1.1.1.1.3.      2.1.........2.1.      vertical bars + crossbar
2.1.......3.1.      2.1.........2.1.
2.1.......2.1.      2.1.........2.1.
2.1.2.2.2.2.1.      2.1.2.2.2.2.2.1.  <- crossbar
2.1.1.1.1.1.1.      2.1.1.1.1.1.1.1.
2.1.......2.1.      2.1.........2.1.
2.1.......2.1.      2.1.........2.1.
2.1.......2.1.      2.1.........2.1.
2.1.......2.1.      2.1.........2.1.
```
Both are unmistakable (`A` = peak + crossbar + two legs; `H` = two bars + crossbar). `I`,`L`,
`E`,`O`,`.`,`1` all decode cleanly the same way. Rendered word proof:
`scratchpad/fnt_verify.png` ("MAIN MENU HELLO 1942", legible).

**Offset-base proof (relative vs absolute):** summing `image_ofs + table[i] + cx*rows`, the
**maximum end = 10637 = the exact file size**; the minimum non-null table value is 196, which
as an *absolute* offset would fall at file byte 196 — **inside the header/tables** (tables run
to 1115) — impossible. Relative-to-`image_ofs` tiles the data region `[1115, 10637)` exactly.
[CONFIRMED]

### 1.4 Exact change for `fnt.cpp`

In `Font::load` (around lines 47-99):

1. Read the base: `const uint16_t image_ofs = rd16(d + 0x0A);` and bounds-check it.
2. Per glyph set the **pixel** width: `g.w = 2 * w;` (and `g.pixels.assign(2*w * g.h, 0);`).
3. Glyph byte count stays `glyph_bytes = w * rows;` (w bytes/row). Point at
   `const uint8_t* src = d + image_ofs + gdo;` (add `image_ofs`; keep the
   `image_ofs + gdo + glyph_bytes <= size` guard).
4. Replace the copy loop with a 4bpp unpack — `w` source bytes per row, 2 pixels each:
   ```cpp
   for (int ry = 0; ry < rows; ++ry) {
       const uint8_t* srow = src + static_cast<size_t>(ry) * w;   // w bytes this row
       uint8_t* drow = &g.pixels[static_cast<size_t>(yoff + ry) * g.w];
       for (int bx = 0; bx < w; ++bx) {
           drow[2*bx]     = srow[bx] & 0x0F;   // left  pixel = low nibble
           drow[2*bx + 1] = srow[bx] >> 4;     // right pixel = high nibble
       }
   }
   ```
5. Expose advance width = `2*w`: store `f.widths_[i] = 2 * w;` so `Font::width()` returns the
   pixel advance (else glyphs overlap by half). The `draw_fnt_text` helper treats pixel value
   0 as transparent and 1..15 as the colour-ramp/intensity index.

No header-parse changes are required beyond adding `image_ofs`; count/width/rows/yoff parsing
is already correct.

---

## §2 Backdrop red-tint fix  [palette pin CONFIRMED; band-fix RE-FALLBACK]

**Palette is correct; the pure-red highlight band is the problem, not the palette choice.**
`cache.mix/palette.pal` is confirmed the right geometry palette (prior 17-pal sweep in
`mainmenu.md §7`; it is the only one that renders the coherent East-Coast relief map).
`.pal` values are 6-bit VGA (0..63) and MUST be ×4-scaled to 8-bit before use.

Root cause (measured on `mnscrnl.shp` + `palette.pal`):
- Indices **240..254 = a smooth olive/grey terrain ramp** `(28,24,8) → (132,136,116)` — this
  is the correct muted land, and the dominant pixels (idx 254 = 107768 px, 252, 249, 245…).
- Indices **230..239 = a pure-red ramp** `(252,0,0) → (100,0,0)` (G=B=0).
- The backdrop uses the red-band slots **237 (52937 px) and 232 (17746 px)** for ~20% of the
  land → the heavy saturated-red speckle. `palette.pal` is the cache/theater default whose
  230-239 band is the engine's "highlight/remap" slots; the shell art was authored expecting
  those slots to be terrain tones, so the raw palette over-reddens the land. [CONFIRMED]

**Recommended fix (evidence-based):** at menu-background render time only, remap the red band
**230..239** to a muted olive-brown keyed off its brightness (kills chroma, preserves relief):
```
for i in 230..239:  R = pal8[i].r;  color = (R*0.42, R*0.40, R*0.28)
```
This is candidate **C** (`scratchpad/cand_C_olive.png`) — land reads as muted olive/green with
only faint coastline relief, matching the real RA2 tactical-map look. Compared candidates:
- **A** original (`cand_A_original.png`): heavy saturated red (the reported bug).
- **B** luminance-grey of 230-239 (`cand_B_grey.png`): muted but dark/noisy speckle.
- **C** olive remap of 230-239 (`cand_C_olive.png`): **best — recommended.**
- **D** partial-desaturate of the wider 200-239 band (`cand_D_partial.png`): still too red.

Keep it scoped to the menu backdrop LUT (don't mutate the shared palette — the 230-239 red band
is a live remap range used elsewhere in-game). This is an **[RE-FALLBACK]** render correction;
the pixel-exact shell palette may be embedded in `gamemd.exe` (a Ghidra pass on the shell
palette load could later replace the band values wholesale — carry-forward).

---

## §3 Composition / coordinates (1280×720)  [ANCHORED]

Real RA2 front-end: full-screen East-Coast backdrop, **RED ALERT 2 logo upper area**, and a
**right-aligned vertical stack of bevelled tech buttons** (right third of the screen). Assets
(all [CONFIRMED] present, `mainmenu.md §2`): backdrop `neutral.mix/mnscrnl.shp` (632×568, 1
frame, uncompressed), buttons `local.mix/mnbttn.shp` (126×25, 3 state-frames = up/hover/down),
logo `local.mix/logo.pcx`, title `local.mix/title.pcx`.

Layout for a 1280×720 window (right-aligned buttons + logo upper):
- **Backdrop:** the task calls for the backdrop to **fill** the screen — scale `mnscrnl.shp`
  (632×568) to **cover** 1280×720 (scale = max(1280/632, 720/568) ≈ **2.03**), centered, minor
  crop. (Alt faithful option = uniform **fit** ≈1.267 → ~801×720 centered with side bars filled
  in the palette background/land tone, idx 254 — the RA2 shell letterboxes this way.)
- **Button strip:** vertical stack of `mnbttn.shp` in the **right third** (x ≈ 853..1280).
  Scale the 126×25 button ~1.6× → ~**200×40**; left edge **x ≈ 980**, first button **y ≈ 250**,
  **~8 px** vertical gap, one per top-level item. Frame 0 = normal, 1 = hover, 2 = pressed;
  hit-test via `SbRect::contains`. Live = Skirmish + Exit; greyed = Campaign/Load/Network/
  Internet/Options/Sneak Peeks (Phase-3 flow, `mainmenu.md §7 Item 3`).
- **Logo:** `logo.pcx` centered in the upper area, **y ≈ 60**. `title.pcx` top-left.
- **Menu text:** authentic labels via the fixed `.fnt` (§1) using `12metfnt`/`metal12`; labels
  from `ra2.csf` `GUI:` keys.

Exact per-button pixel rects are hard-coded in `gamemd.exe` (not data-driven; OpenRA has no RA2
mod to borrow) — a Ghidra pass on the shell dialog init is the only way to pin them to the
pixel. The above is a faithful-*looking* **[RE-FALLBACK]** approximation. [ANCHORED]

---
## §CORRECTION (2026-07-20, verified during fnt.cpp fix — supersedes §1.1/§1.3/§1.4 bit-depth claims)
Implementation + an independent byte-level oracle proved the ONLY real bug was the
image_ofs-relative offset (doc bug #2). The other two doc claims were WRONG:
- The font is **8bpp (1 byte/pixel, ramp index in the low nibble, high nibble 0)** — NOT
  4bpp/2-px-per-byte. The 4bpp attempt produced comb/striped garbage (blank column between
  every pixel). The offset-gap argument is consistent with both; raw nibble content decides = 8bpp.
- **Advance/width = `w`** (original semantics), NOT `2*w`.
Correct fnt.cpp decode: glyph base `data + image_ofs + offset_table[i]`; read `w*rows` bytes
row-major, 1 byte/pixel (mask low nibble for the shade ramp); width=`w`. Verified: 'I'/'H'
exact-bitmap + menu labels legible (menu_fnt2_zoom.png). fnt_ok=TRUE.
