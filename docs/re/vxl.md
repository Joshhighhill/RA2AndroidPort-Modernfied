# VXL + HVA (voxel models) — reverse-engineering notes

(Loader-location pass for the Foundation Batch VXL/HVA decoder. The oracle
byte-gate lands with the decoder — see the plan and `scripts/make_vxl_oracle.md`.)

## Layout (byte-verified against the real htnk.vxl, 34,550 bytes)

File header, 802 bytes: `char magic[16]="Voxel Animation"` (NUL padded) @0;
u32 palette_count(=1) @16; u32 section_header_count @20; u32
section_tailer_count @24 (== header count); u32 body_size @28; u8
palette_remap_start @32; u8 palette_remap_end @33; u8 palette[768] @34
(6-bit RGB, expanded `<<2` like `.pal` — see `docs/re/pal.md`).

Section headers, 28 B each at 802 + i*28: `char name[16]`; u32 limb_number;
u32 unknown1; u32 unknown2. Body at 802 + N*28, length body_size. Section
tailers, 92 B each after the body: u32 span_start_ofs/span_end_ofs/
span_data_ofs (body-relative) @+0/+4/+8; f32 det_scale @+12; f32
transform[3][4] row-major @+16; f32 min_bounds[3] @+64; f32 max_bounds[3]
@+76; u8 size_x/size_y/size_z @+88/89/90; u8 normals_mode @+91 (2=TS, 4=RA2).

Arithmetic: 802 + 1*28 + 33,628 + 1*92 = 34,550 = htnk.vxl exactly.

Span tables per section: u32[size_x*size_y] starts at body+span_start_ofs,
ends at body+span_end_ofs, 0xFFFFFFFF = empty column, column i = (i % size_x,
i / size_x), data at body + span_data_ofs + start[i]; column decode is the
skip/count/(color,normal)-pairs/trailing-count walk (cross-checked against
XCC `Cvxl_file`, off-repo).

HVA, cross-checked against the 88-byte htnk.hva (16+4+4+16+48): char
path[16]; u32 frame_count @16; u32 section_count @20; names[S][16]; then
F*S 3x4 f32 matrices. Size must equal 24 + 16*S + 48*F*S.

## Ghidra location in `gamemd.exe` (YR 1.001). Confidence: **High** (loader/layout); **Low** (span/column walk — not located, likely inlined into the renderer)

Method (`docs/re/workflow.md`): the literal 16-byte magic `"Voxel Animation"`
is **not present anywhere in this binary** (confirmed by a raw byte search of
the exe itself, independent of Ghidra — 0 hits), so the game apparently never
validates that string at load time. `.vxl`/`.hva` (lowercase) are likewise
absent. Raw-grepping the exe for case variants turned up the real anchors —
exact-case literals `DPOD.HVA` / `DPOD.VXL` (a hardcoded "Drop Pod" test
asset), `Failed to create VoxLib!` / `Failed to create MotLib!` (ctor-failure
log lines), and uppercase extension constants `.VXL` / `.HVA` used to build
per-object filenames. `D:\ra2-re\scripts\vxl_hunt.py` (magic/`.vxl`/`.hva`,
0 hits) and `vxl_hunt2.py` (the corrected-case anchors above) block-read-searched
every initialized block plus `getReferencesTo`; `vxl_dump3.py` then did a
targeted decompile of the two per-object constructors those anchors led to,
plus their real callees.

| Function | Role | Notes |
|----------|------|-------|
| `FUN_00754bd0` | **VXL file loader** (header validate + section headers/body/tailers + palette) | reads 32 B fixed prefix (magic\[16\]+3×u32 counts), then either skips or reads exactly 770 B (0x302) of remap_start/remap_end/palette[768] into a **shared global palette buffer** (see below); reads `section_header_count` × 28 B (0x1c) headers, `body_size` bytes of body, `section_tailer_count` × 92 B (0x5c) tailers; converts each tailer's `span_start_ofs/span_end_ofs/span_data_ofs` to absolute pointers by **adding the body buffer's base address** |
| `FUN_00754af0` | thin ctor shim for the above | sets up an MSVC exception-cleanup frame, then calls `FUN_00754bd0(this, file)` — no format logic of its own |
| VXL span/column walk | **not located** — see "Span walk" below | the loader only resolves span pointers; it never inspects span-table contents (no `0xFFFFFFFF` check, no skip/count walk visible in this function) |
| `FUN_005bcff0` | **HVA loader** (header validate + matrix block) | reads 24 B (0x18) header (`path[16]` discarded, `frame_count`@16, `section_count`@20), allocates `frame_count*section_count*0x30` bytes, seeks past `section_count*16` B of names (never stored), then double loop **outer=frame (0..frame_count), inner=section (0..section_count)**, reading one 48 B (0x30) matrix per iteration and storing at index `section_count*frame + section` |
| `FUN_005bcfa0` | thin ctor shim for the above | same exception-frame pattern as `FUN_00754af0`, no format logic |

Supporting/generic functions (not format-specific, included for completeness):
`FUN_00473920` (constructs a file-handle object, WW `RawFileClass`-shaped) and
`FUN_00473b80` (`Is_Available()`-shaped: local-disk check, then falls back to
the mix-archive lookup `FUN_005b3e60` seen in `mix.md`) are the generic
open/exists pair every loader above calls before allocating — same role as
the file layer documented for MIX/CSF, no VXL/HVA-specific behavior.

Contextual (not the per-file loader, but explains the anchors): `FUN_005313c0`
is an engine-bootstrap routine that constructs the global `VoxLib`/`MotLib`
singletons (`_DAT_00a8dc40`/`_DAT_00a8dc44`) by self-testing against the
hardcoded `DPOD.VXL`/`DPOD.HVA` pair, logging the two `"Failed to create
...Lib!"` strings on failure; its caller `FUN_0052b790` also loads
`voxels.vpl` (a separate shared voxel remap palette file — see "Palette"
below). `FUN_0045fa80`/`FUN_005f7280`/`FUN_005f75a0`/`FUN_005f84d0` are
`BuildingTypeClass`-level per-object loaders that build `<Image>.VXL`/`.HVA`,
`<Image>TUR[n].VXL`/`.HVA`, and `<Image>BARL[n].VXL`/`.HVA` filenames and call
the two shims above; `FUN_00749e80` is the analogous `WeaponType`-ish ini
parser that loads a body voxel for non-`ShareBodyData` projectiles. All of
these bottom out in the same two loader functions in the table above.

### Layout verification against the game loader

- **File header stride 802 / palette copy from offset 34: confirmed.** The
  loader's read is split as 32 B fixed prefix + 770 B (0x302) remap/palette
  region, and 32 + 770 = 802 either way (whether the palette bytes are
  actually read or the region is seeked-over — see "Palette" below). The
  770 B region decomposes as 1 (remap_start) + 1 (remap_end) + 768 (palette),
  i.e. palette starts at file offset 34, exactly as documented.
- **Section header stride 28 / tailer stride 92: confirmed directly** — the
  loader's read-length literals are `0x1c` (28) and `0x5c` (92).
- **`span_start_ofs`/`span_end_ofs`/`span_data_ofs` @ tailer +0/+4/+8,
  body-relative: confirmed, and confirmed *how*.** The 92-byte read target's
  stack layout maps byte-for-byte onto the tailer struct: the buffer starts at
  stack-frame offset `-0xD4`, and the locals the loader then uses sit at
  frame offsets `-0xD4`/`-0xD0`/`-0xCC` (tailer +0/+4/+8, the three span
  offsets), `-0xC8` (+12 = `det_scale`), a 48-byte block at `-0xC4`
  (+16 = `transform[3][4]`), `-0x94/-0x90/-0x8C` (+64 = `min_bounds[3]`),
  `-0x88/-0x84/-0x80` (+76 = `max_bounds[3]`), and single bytes at
  `-0x7C/-0x7B/-0x7A/-0x79` (+88/89/90/91 = `size_x/size_y/size_z/normals_mode`).
  The loader then does exactly `runtime.start = span_start_ofs + body_ptr`
  (and likewise for end/data) — i.e. "relative to body start" is realized as
  literal pointer arithmetic against the body buffer's base address. **High
  confidence, byte-exact.**
- **`0xFFFFFFFF` empty-column test on the span start entry: not visible
  here.** The loader never reads span-table *contents* — it only computes
  the three absolute pointers and moves on. The empty-column test must live
  in whatever consumes those pointers at render time.
- **Skip/count/(color,normal)-pairs/trailing-count column walk: not
  found.** No function reachable from the VXL loader inspects span-table or
  span-data bytes. Given the identical finding for SHP in `shp.md` (RLE
  decode fused into the draw-time blitters, no standalone decompressor), the
  most likely explanation is the same pattern: voxel column decode is
  inlined into a draw-time rasterizer this hunt did not chase down within
  budget. **Low confidence / not located** — this is an honest gap, not a
  contradiction; the format itself remains cross-checked against XCC's
  `Cvxl_file` (off-repo, unchanged from the ground-truth table).
- **`normals_mode` @+91 selecting a normal table (2 vs 4): partially
  confirmed.** The byte is read at exactly file offset +91 and cached into
  the runtime tailer struct — confirms the *offset*. The consumer that
  actually branches on 2-vs-4 was not traced (it would be in the same
  unlocated rasterizer as the column walk). **Medium confidence** on the
  offset, unconfirmed on the 2/4 semantic split.
- **`transform[3][4]` @+16, 3 rows × 4 cols: confirmed** by the exact byte
  range (+16..+63, 48 B = 12 floats). One wrinkle: the raw 12 floats are not
  cached as-is — they're passed through a small helper (`FUN_005ae010`, not
  traced further) before being stored, and the *same* helper is reused
  verbatim by the HVA matrix loader below. This is almost certainly a
  render-side repack/inverse step, not a file-format divergence — the 12
  floats are read from the file in the documented row-major 3×4 order either
  way, which is all the decoder's `load_vxl` needs.
- **HVA header 24 bytes, `names[S][16]`, matrix indexing: confirmed, and the
  frame-vs-section-major question is resolved — see next section.**

### HVA matrix order (frame-major vs section-major)

OpenRA reads the matrix block frame-major; XCC's `Chva_file` accessor indexes
it section-major (`c_frames * section + frame`). The 1-frame/1-section
htnk.hva cannot distinguish them. **Game loader verdict: frame-major.**
`FUN_005bcff0`'s matrix-read loop is nested **outer = frame (0..frame_count),
inner = section (0..section_count)** — i.e. for each frame it reads all
`section_count` matrices back-to-back before advancing to the next frame —
and it stores each into its runtime cache at index `section_count * frame +
section`, i.e. **`frame * section_count + section`**. That is exactly
frame-major, matching our `load_hva` and OpenRA, and is definitive (not
merely "not contradicted") because it comes directly from the loop
nesting/index arithmetic, not just cache-layout inference. XCC's accessor
formula indexes *its own* in-memory cache, which apparently transposes on
load or is simply a different convention for the same on-disk order — it is
not evidence the file itself is section-major. Our `load_hva` implements
frame-major per **the game loader**, confirming rather than merely retaining
the existing choice.

### Palette: a secondary finding worth recording

The embedded VXL palette (remap_start/remap_end/palette[768]) is read into a
**shared global buffer** (`DAT_00b2eae0`-ish region) rather than a per-object
field, and — per the decompiled branch on an apparently-optimizer-mangled
flag (a stack slot at frame offset `-0x4`, likely a "palette already established" static/global the
decompiler rendered as an uninitialized local; not fully resolved) — later
VXL loads within the same session appear to **skip** re-reading it (seeking
past the same 770-byte region instead). Separately, the engine's bootstrap
(`FUN_0052b790`) also loads a standalone `voxels.vpl` file, strongly
suggesting actual in-game voxel colors are sourced from that shared palette
file rather than each VXL's own embedded copy. This does not change the
*file format* (the embedded palette bytes are exactly where the ground-truth
table says, confirmed above) — it only means the game's *consumption* of
that palette is more indirect than "read once, use once." Not chased further
within budget; flagged for whoever eventually does color-accurate voxel
rendering, irrelevant to the indexed-data decoder.

## Confidence summary

| Item | Evidence | Confidence |
|------|----------|:----------:|
| File/section/tailer layout | htnk.vxl arithmetic + XCC structs + game loader (`FUN_00754bd0`, byte-exact stack-offset mapping) | **High** |
| Span walk shape | game loader not located (only resolves pointers; no table/data-byte consumer found within budget — cf. SHP's fused-blitter precedent in `shp.md`), BUT the decode is proven correct another way: the real-asset oracle below decodes every voxel of htnk.vxl byte-identically to XCC's `Cvxl_file` | **Low** (RE location) / **High** (correctness, via oracle) |
| Palette `<<2` expansion | reuse of `Palette::load` (docs/re/pal.md, Medium-High); embedded-palette *layout* confirmed by the game loader, but consumption is indirect (shared buffer + separate `voxels.vpl`, see above) | **Medium-High** (layout), **Medium** (consumption path) |
| `normals_mode` semantics (2/4) | offset confirmed (`FUN_00754bd0` @+91); 2-vs-4 table-selection consumer not traced | **Medium** |
| HVA layout + matrix order | `FUN_005bcff0`: header size, names-skip size, and matrix-count all match by read-length literals; **frame-major order settled directly from loop nesting + index arithmetic** (`section_count*frame + section`) | **High** |

## Oracle: line-for-line parity with XCC's decoder (htnk)

`tests/test_vxl.cpp` ("vxl oracle: htnk.vxl + htnk.hva match the XCC dump
line-for-line") decodes the real `htnk.vxl` (34,550 bytes) + `htnk.hva`
(88 bytes) from `ra2.mix -> local.mix` and compares a canonical dump (voxels
sorted by z,y,x; floats as raw IEEE-754 bit patterns) against XCC's own
`Cvxl_file`/`Chva_file` logic built into an off-repo tool
(`scripts/make_vxl_oracle.md`).

**Result: 4817 VXL lines (section header + every present voxel's
x/y/z/color/normal) + 4 HVA lines, 0 mismatches.** htnk section 0:
`section 0 DUMMY01 46 34 12 2 0x3daaaaab` (46×34×12, `normals_mode` = 2 — the
real Rhino uses the TS-style normal table, not 4). The test skips cleanly
(doctest MESSAGE) without the oracle or install, and `lines_checked > 0`
guards against a silent skip.

This is the authoritative correctness gate: it proves `load_vxl` (including
the span/column walk the Ghidra hunt could not locate) and `load_hva`
(including the frame-major matrix order) produce byte-exact output on a real
asset, independent of the RE.
