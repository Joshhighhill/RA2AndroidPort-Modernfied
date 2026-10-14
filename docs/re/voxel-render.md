# Voxel rendering + Drive locomotion semantics — reverse-engineering notes

(Method: `docs/re/workflow.md`; image base `0x00400000`, YR 1.001 `gamemd.exe`.
Hunt scripts, all off-repo at `D:\ra2-re\scripts\`: `voxel_render_hunt.py` …
`voxel_render_hunt24.py` — normal-table xrefs → light-table builders → the
function-pointer-dispatched rasterizer variants (createFunction on xref-less
gaps) → the draw/cache/blit chain → matrix-stack family → camera-constant
setters → FacingClass/ROT → ini parsers → warhead/weapon fields. Full
decompiles and forced-disassembly listings under `D:\ra2-re\out\vr*_*.c/.txt`;
nothing under `D:\ra2-re` is committed. Content-anchored only: every function
below was reached from string/data/xref evidence in THIS binary, never from
published addresses. Prior anchors from `docs/re/vxl.md` (VXL loader
`FUN_00754bd0`, HVA loader `FUN_005bcff0`, `voxels.vpl` bootstrap
`FUN_0052b790`) were the entry points.)

This document is the calibration authority for the voxel-vehicle phase: §1
projection, §2 facing quantization, §3 normal tables + lighting, §4
voxels.vpl/palette/remap, §5 Drive locomotion (Speed/ROT), §6 FLH, §7
projectile + splash. Confidence per item; "Decisions for ra2engine" at the end
resolves every `[RE: voxel-render.md §N]` marker in the plan's Tasks 4–6.

## Located functions

| Function | Role | How found |
|----------|------|-----------|
| `FUN_00753AD0` @ `0x753AD0` | VoxLib geometry init: base view matrix, ramp-tilt matrices (`DAT_00B440F0`, stride 0x30), light axis table | callers of matrix-rotate fns; writes `DAT_00B43280` |
| `0x753730`–`0x753840` (7 tiny gap fns) | camera-global setters: `256√2`, leptons/px, 45°/60°/90° angles, z-scale | raw-scan for writers of `DAT_00B43278/B42E68/B43400/B444E0`; forced disassembly |
| `FUN_00753A00` | returns the global base matrix `DAT_00B43280` (12 floats) | callee of the unit draw |
| `FUN_00753A20` | scene light-vector setup: `DAT_008863F0 = RotY(θ)·(−√½,−√½,0)` | xref of `DAT_008863F0`; called from bootstrap `FUN_0052B790` |
| `FUN_00752C20` | per-model reset: clear prev dirty rect of the 256×256 color buffer (+z-buffer), reset section list, min/max = ±10000 | callee of `FUN_00706360` |
| `FUN_00752DB0` / `FUN_00752F10` | queue one section: transform its 8 runtime-tailer corners by the current matrix (y negated), track per-section+global min/max, store min-depth corner INDEX (the view-octant selector) | callers of tailer accessor `FUN_007552D0` |
| `FUN_00753110` / `FUN_00753330` | flush: draw shadow blobs, depth-sort sections (max-z first), dispatch each to a rasterizer, compute the buffer dirty rect + blit offsets | callers of the dispatcher |
| `FUN_007553B0` @ `0x7553B0` | **rasterizer dispatcher**: builds the 0x34-byte fixed-point context (span ptrs, column index/steps, 8.8 start + per-axis steps) and calls `kRasterizer[flags]` via the pointer table @ `0x8457C0` | callee of flush; holds the 12 ftol calls |
| `VXR_755F40/756180/7567A0/756A10/757010/757250` + `0x7DE5C0/7DE6C0/7DE7C0/7DE8E0/7DEA00/7DEB00` | **the rasterizer variants** (xref-less gap code, reached only via the table; `createFunction` + decompile) — the span `0xFFFFFFFF` test, skip/count column walk, vpl lookup, z-buffered 2-px plot | function-pointer table @ `0x8457C0` read raw from `.data` |
| `FUN_00757490` / `FUN_00757510` | **per-normal light-table builders** (fill `DAT_00B448F8[normal_idx] = level`), single-light and light+spec variants | xrefs to the normal-table pointer array `0x845960` |
| `0x845960` / `0x845974` | `float* kNormalTable[5]` = {0, t1, t2, t3, t4} and `int kNormalCount[5]` = {0, 16, 36, 64, 245}, indexed by `normals_mode` | raw unit-vector scan of the exe (one 361-triple run = 16+36+64+245) |
| `FUN_00752990` / `FUN_00757770` / `FUN_00757850` / `FUN_007577E0` | voxels.vpl loader: 16 B header, 768 B palette → `DAT_00B2EAE0`, `count·256` table bytes → `DAT_00B400E0` | called right after `FUN_00473920("voxels.vpl")` in `FUN_0052B790` |
| `FUN_00755680` | shadow renderer: for every non-empty column (`span_start != -1`) plot color 1 at the projected (x,y), no z-test | dispatcher-adjacent cluster sweep |
| `FUN_005AE010…FUN_005AF650` | Matrix3D family: `0x5AE290` reset, `0x5AE040` push, `0x5AE2C0` translate, `0x5AE990/5AEAB0/5AEBD0` rotate X/Y/Z, `0x5AE440` scale, `0x5AF3B0` 3×4 multiply, `0x5AF5B0` transform-point, `0x5AF650` inverse, `0x5AE120` tilt-about-axis, `0x5AEF00` rotate-vector | vxl.md's untraced `FUN_005AE010` = plain 12-dword copy; family enumerated 0x5AE000–0x5AF700 |
| `FUN_00705AD0` @ `0x705AD0` | **cached voxel-part draw** (vtable slot `+0x444` of Techno/Unit/Building/Infantry/Aircraft/Foot): cache hit → blit stored sprite; miss → rasterize + capture + append `{key, sprite}` | vtable stores found by dword scan; RTTI walk named all six classes |
| `FUN_0070FB80` | cache find: one-time qsort of `{int key, ptr}` pairs then bsearch | callee of `FUN_00705AD0` |
| `VXD_4DAE20` @ `0x4DAE20` | UnitClass vtable slot `+0x510`: adds the locomotor's pixel offset (ILocomotion `+0x2C`) to the screen anchor, forwards to `FUN_00705AD0` | only direct `E8` call to `0x705AD0` (gap fn, forced) |
| `~0x73A360` (gap; body `0x73A360–0x73B5xx`) | **UnitClass voxel draw**: HVA frame from walk counter, locomotor `Draw_Matrix` (matrix + cache key), hull/turret/barrel dispatch via slot `+0x510`, turret/barrel key assembly | forced linear disassembly; contains the `>>10 +1 >>1 &0x1F` quantizers |
| `FUN_004AFE70` | **DriveLocomotionClass::Draw_Matrix** (ILocomotion vtbl `0x7E6EB0` slot 9, `+0x24`): ramp matrix ∘ facing rotation; slope tilt (sets key −1 = uncacheable); ramp-transition lerp | RTTI walk to the Drive ILocomotion vtable |
| `FUN_0055A3C0` | shared loco helper: `Rz((idx32−8)·(−2π/32))` + `key = key<<5 \| idx32` | callee of Draw_Matrix |
| `FUN_006F3030` / `FUN_006F32C0` | **TechnoClass::GetFLH** (weapon-index + burst-flip variants): turret matrix ∘ translate(F, ±L, H) → world leptons | callers of rotate-Z with the `((f>>10)+1>>1&0x1F)−8` quantizer |
| `FUN_004C92F0` / `FUN_004C9140` / `FUN_004C9220` / `FUN_004C95A0` / `FUN_004C93A0` / `FUN_004C9450` | **FacingClass**: Current (timed interpolation), Set_Desired (arms the turn timer), Set (snap), Set_ROT (`rate = min(ROT,127)<<8`), Is_Rotating, Difference | region sweep 0x4C9140–0x4C9800 after the ROT data-flow |
| `FUN_00734490` | UnitClass ctor: `Set_ROT(type+0x71C)` on BOTH body and turret facings | consumer scan of TechnoType `+0x71C` |
| `FUN_00735A60` | UnitClass turret AI: aim at target dir, idle return-to-body / to move-direction after ROF+5 frames, TurretSpins path | Is_Rotating callers in the Unit range |
| `FUN_00711510` | TechnoTypeClass::LoadFromINI: `ROT`→`+0x71C`, `Speed`→`+0x678` (converted), `Turret`→`+0xCA1`, `TurretOffset`→`+0x720`, `[Elite][Primary\|Secondary]FireFLH`→ weapon entry `+4/+8/+0xC` | string xrefs (`"ROT"` @0x81A174 etc.) |
| `~0x770E00` (gap) | WeaponTypeClass::LoadFromINI: `Burst`→`+0x9C`, `Damage`→`+0xA4`, `Speed`→`+0xA8` (via `FUN_00474740`), `ROF`→`+0xB0`, `Range`→`+0xB4` (cells→leptons via `FUN_00474550`) | "Speed" push-site scan; forced disassembly |
| `0x75C1C0` (gap) | WarheadTypeClass::LoadFromINI: `CellSpread`→`+0x124` (float cells), `CellInset`→`+0x128`, `PercentAtMax`→`+0x12C` | "CellSpread"/"PercentAtMax" string byte-scan |
| `FUN_00489190` / `FUN_00489090` | Explosion/spread damage: victim collection within `CellSpread·256` leptons + per-cell ring tables; PercentAtMax scaling helper | `FLD [reg+0x124]` opcode scan |
| `FUN_005BD160` | HvaClass::scale — multiplies every HVA matrix's translation column by the section `det_scale`, once at load (`FUN_005F7900` calls it per part) | callee scan of the per-object voxel init |

## §1 Projection — the voxel → screen transform

**Architecture.** Voxel models are NOT rasterized straight to the screen. Each
(model, orientation) is rendered once into a **256×256 8-bit offscreen buffer**
(`DAT_00B2EEE0`, one byte per pixel = palette index) with a **256×256 byte
z-buffer** (`DAT_00B1C548`), then captured as a sprite, cached per facing key
(§2), and blitted like any SHP frame. The buffer's center (128,128) is the
model origin.

**The matrix chain** (3×4 row-major float, translation in column 3):

```
M = kBase ∘ M_loco ∘ M_hva_section
kBase  = Rx(−60°) · Rz(−45°)                    (DAT_00B43280, built by FUN_00753AD0:
                                                 angles = deg2rad·60.0 @0x7E0728 and
                                                 deg2rad·45.0 @0x7E0720, both negated)
M_loco = M_ramp(ramp_index) · Rz(yaw(facing))    (DriveLoco Draw_Matrix FUN_004AFE70;
                                                 M_ramp = identity on flat ground)
M_hva  = hva.matrix[frame][section]              (frame-major, stride 0x30; its
                                                 translation column was multiplied by the
                                                 section tailer det_scale at LOAD time,
                                                 FUN_005BD160 — NOT per frame)
```

There is **no scale anywhere in the chain** (pure rotations): **1 voxel model
unit projects to exactly 1 screen pixel** at the game's fixed zoom. `yaw()` is
the quantized facing angle of §2.

**Voxel positions.** The VXL loader (`FUN_00754BD0`) stores per section a
0xA4-byte runtime tailer: `+0` span_start ptr, `+4` span_end ptr, `+8`
span_data ptr (absolute), `+0xC` det_scale, `+0x10` transform[3][4] (verbatim
copy, unused by the draw), `+0x40..0x9F` the **8 bounding-box corners** as raw
`min_bounds`/`max_bounds` combinations (NO scale, NO half-voxel offset),
`+0xA0/A1/A2` size_x/y/z, `+0xA3` normals_mode. Voxel (i,j,k)'s position is
**corner-anchored**:

```
pos = min_bounds + (i, j, k) · (max_bounds − min_bounds) / (size_x, size_y, size_z)
```

(the dispatcher steps by `(corner_axis_end − corner_origin)/size` per voxel and
STARTS exactly at the transformed origin corner — there is no `+0.5` centering
in the original).

**Screen formulas.** Section corners are transformed by `M` with **y negated**
(`FUN_00752F10`); the whole model is centered: `center = (min+max)/2` over all
transformed corners (`0.5f @ 0x7E4168`). The dispatcher (`FUN_007553B0`)
converts to 8.8 fixed point with `K = 128.0 @ 0x7F595C`, `S = 256.0 @
0x7E1224`: `start8.8 = ftol((corner_origin + 128 − center) · 256)` per
component, steps likewise. Expanding `M` (c45 = √½ ≈ 0.70711, and the model
point `v = (x, y, z)` after loco/hva/yaw):

```
buf_x  = 128 − center.x + 0.70711·(v.x + v.y)
buf_y  = 128 − center.y + 0.35355·(v.x − v.y) − 0.86603·v.z     (screen y grows down)
depth  = 128 − center.z + 0.61237·(v.x − v.y) + 0.5·v.z         (bigger = nearer)
```

**Depth rule.** Two levels: (a) whole sections are painter-sorted by their
max view-space z, far first (`FUN_00753330` bubble sort on state `+0x24`); (b)
per pixel, the byte z-buffer keeps the voxel with the **strictly greater**
`depth>>8` byte (`if (zbuf[addr] < depth_hi) { zbuf=…; color=…; }` in every
variant). The rasterizer chooses column traversal order per view octant (the
min-depth bbox corner index picks one of 8 config rows @ `0x845840`, stride
0x24: origin/end corner ids + column/row step signs), so within a column the
walk is already back-to-front.

**Footprint.** Each voxel writes **2 horizontal pixels** (`addr` and
`addr+1`, both color and z) — that is the original's pinhole filler. Buffer
addressing is `addr = (y8.8 & 0xFF00) | (x8.8 >> 8)` (256-wide rows).

**Span walk (the piece vxl.md could not find).** Confirmed in every variant,
e.g. `VXR_756A10`: `span_start[column] == -1` → skip column (the 0xFFFFFFFF
empty-column test); otherwise walk `[skip][count][count × (color,normal)]
[count]` — skips advance the accumulators via a precomputed `k·step` table
(`DAT_00B444F8`), runs plot per-voxel. Reversed-octant variants walk the same
data backwards from `span_end`. Un-lit variants (dispatch flag bit 3, set when
`normals_mode == 0`) consume 1 byte/voxel; the RA2 path (mode 2/4) consumes
(color, normal) pairs.

**Blit / anchor.** The flush returns `(ftol(center.x) − (w+8)/2,
ftol(center.y) − (h+8)/2, rect)` and the unit draw adds that to the unit's
screen point (plus the locomotor pixel offset, `VXD_4DAE20`) — net effect:
**view-space (0,0,0) — the model origin — lands exactly on the unit's screen
anchor point**, the same tactical world→pixel point SHP units use.

**World scale relation** (from the camera setters at `0x753730…0x753840`):
`DAT_00B42E60 = sqrt(2·256²) = 256√2` (cell diagonal in leptons),
`DAT_00B440E8 = 256√2/60` = **≈6.034 leptons per screen pixel** along the
screen diamond (60.0 @ `0x7F5940` = cell width in px), inverse
`DAT_00B44008 ≈ 0.16573 px/lepton`. So 1 voxel unit ≈ 6.03 leptons
horizontally; a 46-unit Rhino hull renders 46 px ≈ 0.77 cells wide. Confidence
**High** (every constant read from the binary; formulas from disassembly).

## §2 Facing quantization

**32 facings for hull, turret and barrel.** The quantizer appears identically
in the unit draw, the loco Draw_Matrix helper and GetFLH:

```
idx32 = ((facing_u16 >> 10) + 1 >> 1) & 0x1F      // round-to-NEAREST, mod 32
yaw   = (double)(idx32 − 8) * −0.19634954084936207 // −2π/32 per step (dbl @0x7E3408)
```

The **matrix itself uses the quantized angle** (`FUN_0055A3C0`,
`FUN_006F3030`) — hulls/turrets visibly step through 32 orientations; smooth
sub-step rotation exists only in the 16-bit sim state, not on screen.

**Calibration (which index is which screen direction).** Facing 0 = North.
With `yaw(idx)` above and the §1 basis: facing `0x4000` (East, idx 8) → yaw 0
→ the model's authored +X nose projects to screen (+0.707, +0.354) = the
down-right 2:1 cell diagonal = **screen East**; facing 0 (idx 0) → yaw +90° →
nose up-right = **screen North**. I.e. `screen_angle` rotates clockwise as
facing increases, exactly the infantry convention (infantry-anim.md §5), and a
voxel authored nose-+X needs yaw 0 at facing East.

**The render cache** (`FUN_00705AD0` + `FUN_0070FB80`): per part (hull cache
at UnitType `+0x244`, barrel `+0x280`), sorted `{int key, sprite}` array,
key == −1 → render uncached (used while tilted on slopes). Key layout from the
unit draw + `FUN_0055A3C0`/`FUN_004AFE70`:

```
hull key   = (ramp_index << 5 | body_idx32) << 5 | hva_frame%32
turret key = (hull-key & ~0x1F) | turret_idx32, bits 5..9 cleared when type
             TurretOffset == 0 (turret sprite independent of body facing),
             | recoil/frame byte << 16
```

Caching is gated by the global `DAT_00886398` (set at bootstrap). Confidence
**High** (quantizer + angle constant read at three independent sites; key
assembly from forced disassembly of the unit draw).

## §3 Normal tables + lighting

The binary holds **four** normal tables, selected by `normals_mode` via
`float* kNormalTable[5] @ 0x845960` = {NULL, `0x845988`, `0x845A48`,
`0x845BF8`, `0x845EF8`} with `int kNormalCount[5] @ 0x845974` =
{0, **16, 36, 64, 245**}. Mode 2 (TS) = 36 entries; mode 4 (RA2) = **245**
entries — one more than the commonly cited 244; the last five entries
(240–244) are the identical vector (−0.328188, 0.140251, 0.934143) (padding —
but index 244 IS addressable). All 361 vectors are unit length (magnitude
0.999999–1.000001, straight IEEE-754 floats, no fixed point). Both tables are
emitted verbatim below.

**Lighting rule.** Lighting is per-NORMAL-INDEX, not per voxel: before each
section is rasterized, `FUN_00757490`/`FUN_00757510` fill a 256-byte table
`DAT_00B448F8[normal_index] = level`:

- The scene light vector `L_world = RotY(θ)·(−√½, −√½, 0)` (`FUN_00753A20`,
  θ = `DAT_008863EC`, set at init/scenario lighting) is rotated **into model
  space** with the inverse of the current matrix (`FUN_005AF650`), so lighting
  stays world-anchored while the model rotates.
- Simple path (`FUN_00757490`): `d = dot(n, L)`;
  `level = d < 0 ? 0 : ftol(d · 16.0)` (16.0f @ `0x7F5960`).
- Unit path (`FUN_00757510(L, Z, k=3.0, mode)` with `Z` = model-space image of
  world +z): `H = normalize(L + Z)`; `d = max(0, dot(n, L))`;
  `s = max(0, h/(k − (k−1)·h))` where `h = dot(n, H)` and k = 3.0 (a
  sharpened specular-ish term, `h/(3−2h)`);
  `level = (d + s) < 0 ? 0 : ftol((d + s) · 16.0)`.
- Normal indices **253, 254, 255 are forced to level 16** (both builders end
  with `tab[253..5] = 0x10`).

**Which row is "base color"? Row 8, per the real file — the formula maps
(d+s) = 0.5 to it.** The scale constant is definitively 16.0f (both builders
end with `FMUL dword ptr [0x7F5960]` — `0x007574D9` and `0x0075765F` — and the
dword at `0x7F5960` is 16.0), so levels span 0..31 (d+s < 2.0 → ftol(31.9) =
31, never out of range). What each of the 32 rows *means* is defined by the
voxels.vpl content, not the code: Task 2's scan of the real 8,976-byte file
(`.superpowers/sdd/task-2-report.md`) shows one sharp near-identity peak at
**row 8** (199 of 256 entries satisfy `table[8][i] == i`; neighbors 7/9 ≈ 100;
row 16 has only 13). So the correct reading is: **(d+s) = 0.5 → row 8 = the
unmodified palette color; rows 9–31 progressively brighten (a fully lit
surface, d ≈ 1 with the specular term, lands in the 20s–31); rows 7–0
progressively darken to row 0 = darkest** (not "unlit-black" — row 0's mapping
is whatever the file's darkest ramp says). An earlier revision of this doc
called row 16 "the neutral/full row" — that was an inference from d = 1 in the
single-light path, and the real file falsifies it; the recovered formula
itself is unchanged. The hard-wired normal indices 253–255 → level 16 land on
a *brightened* row (self-lit marker voxels), which also only makes sense with
the 16.0 scale — with a scale of 8, rows 17–31 could never be produced at all.
Per-voxel: `final_color = vpl_table[level[normal]][color]`
(`(&DAT_00B400E0)[color + level*0x100]` in `VXR_756A10`). `ftol`
(`FUN_007C4D00`) truncates toward zero. Confidence **High** (disassembly of
both builders + the rasterizer's table lookup + real-file row statistics;
table data byte-verified unit-length).

The tables (copy-paste ready; indices in comments):

```cpp
constexpr float kNormals2[36][3] = {
    {0.671213984f, 0.198492005f, -0.714194f},  // 0
    {0.269643009f, 0.584393978f, -0.765359998f},  // 1
    {-0.040546f, 0.096988f, -0.994458973f},  // 2
    {-0.572427988f, -0.0919139981f, -0.814786971f},  // 3
    {-0.171400994f, -0.572709978f, -0.80163902f},  // 4
    {0.362556994f, -0.30299899f, -0.881331027f},  // 5
    {0.810347021f, -0.348971993f, -0.470697999f},  // 6
    {0.103961997f, 0.938672006f, -0.328767002f},  // 7
    {-0.324046999f, 0.587669015f, -0.741375983f},  // 8
    {-0.800864995f, 0.340460986f, -0.492646992f},  // 9
    {-0.665498018f, -0.590147018f, -0.45698899f},  // 10
    {0.314767003f, -0.803002f, -0.506072998f},  // 11
    {0.972629011f, 0.151076004f, -0.176550001f},  // 12
    {0.680290997f, 0.68423599f, -0.262726992f},  // 13
    {-0.520079017f, 0.827777028f, -0.210483f},  // 14
    {-0.961643994f, -0.179001004f, -0.207846999f},  // 15
    {-0.262713999f, -0.937451005f, -0.228401005f},  // 16
    {0.219706997f, -0.971301019f, 0.0911249965f},  // 17
    {0.923807979f, -0.229975f, 0.306086987f},  // 18
    {-0.0824889988f, 0.970659971f, 0.225866005f},  // 19
    {-0.591798007f, 0.69678998f, 0.405288994f},  // 20
    {-0.925296009f, 0.36660099f, 0.0971110016f},  // 21
    {-0.705051005f, -0.687775016f, 0.172828004f},  // 22
    {0.7324f, -0.680366993f, -0.0263049994f},  // 23
    {0.855162024f, 0.374581993f, 0.358310997f},  // 24
    {0.47300601f, 0.836480021f, 0.276704997f},  // 25
    {-0.0976170003f, 0.654111981f, 0.750072002f},  // 26
    {-0.904124022f, -0.153724998f, 0.398658007f},  // 27
    {-0.211916f, -0.858089983f, 0.467732012f},  // 28
    {0.500226974f, -0.674408019f, 0.543090999f},  // 29
    {0.584538996f, -0.110248998f, 0.803840995f},  // 30
    {0.437373012f, 0.454643995f, 0.775888979f},  // 31
    {-0.0424409993f, 0.0833180025f, 0.995618999f},  // 32
    {-0.596251011f, 0.220131993f, 0.772028029f},  // 33
    {-0.506455004f, -0.396977007f, 0.765448987f},  // 34
    {0.0705690011f, -0.478473991f, 0.875262022f},  // 35
};

constexpr float kNormals4[245][3] = {
    {0.526578009f, -0.359620988f, -0.770317018f},  // 0
    {0.150481999f, 0.435983986f, 0.887283981f},  // 1
    {0.414195001f, 0.738255024f, -0.532374024f},  // 2
    {0.0751520023f, 0.916248977f, -0.393498003f},  // 3
    {-0.316148996f, 0.930736005f, -0.183792993f},  // 4
    {-0.773819029f, 0.623333991f, -0.112510003f},  // 5
    {-0.900842011f, 0.428537011f, -0.0695680007f},  // 6
    {-0.998942018f, -0.0109710004f, 0.0446650013f},  // 7
    {-0.979761004f, -0.157670006f, -0.123323999f},  // 8
    {-0.911274016f, -0.362370998f, -0.19562f},  // 9
    {-0.624068975f, -0.720941007f, -0.301301003f},  // 10
    {-0.310173005f, -0.809345007f, -0.498751998f},  // 11
    {0.146613002f, -0.815819025f, -0.559414029f},  // 12
    {-0.716516018f, -0.694356024f, -0.0668879971f},  // 13
    {0.503971994f, -0.114202f, -0.856136978f},  // 14
    {0.455491006f, 0.87262702f, -0.176210999f},  // 15
    {-0.00500999996f, -0.114372998f, -0.993425012f},  // 16
    {-0.104675002f, -0.327701002f, -0.938965023f},  // 17
    {0.56041199f, 0.752588987f, -0.345755994f},  // 18
    {-0.0605759993f, 0.821627975f, -0.566796005f},  // 19
    {-0.302341014f, 0.797007024f, -0.522846997f},  // 20
    {-0.671543002f, 0.670740008f, -0.314862996f},  // 21
    {-0.778401017f, -0.128356993f, 0.614504993f},  // 22
    {-0.924049973f, 0.278382003f, -0.261985004f},  // 23
    {-0.699773014f, -0.550490975f, -0.455278009f},  // 24
    {-0.568247974f, -0.517189026f, -0.640007973f},  // 25
    {0.0540979989f, -0.93286401f, -0.356142998f},  // 26
    {0.758382022f, 0.572893023f, -0.310887992f},  // 27
    {0.00362000009f, 0.305025995f, -0.952337027f},  // 28
    {-0.0608499981f, -0.986886024f, -0.149510995f},  // 29
    {0.635230005f, 0.0454780012f, -0.770982981f},  // 30
    {0.521704972f, 0.241309002f, -0.818287015f},  // 31
    {0.269403994f, 0.635424972f, -0.723640978f},  // 32
    {0.0456760004f, 0.67275399f, -0.738454998f},  // 33
    {-0.180510998f, 0.674656987f, -0.715718985f},  // 34
    {-0.397130996f, 0.636640012f, -0.661041975f},  // 35
    {-0.55200398f, 0.472514987f, -0.687038004f},  // 36
    {-0.772170007f, 0.0830899999f, -0.629960001f},  // 37
    {-0.669818997f, -0.119533002f, -0.732840002f},  // 38
    {-0.540454984f, -0.318444014f, -0.77878201f},  // 39
    {-0.386135012f, -0.522789001f, -0.75999397f},  // 40
    {-0.261465997f, -0.688566983f, -0.676394999f},  // 41
    {-0.0194119997f, -0.696102977f, -0.717679977f},  // 42
    {0.303568989f, -0.481844008f, -0.821992993f},  // 43
    {0.681939006f, -0.195129007f, -0.704900026f},  // 44
    {-0.244889006f, -0.116562001f, -0.96251899f},  // 45
    {0.800759017f, -0.0229790006f, -0.598546028f},  // 46
    {-0.370274991f, 0.0955839977f, -0.923991024f},  // 47
    {-0.330671012f, -0.326577991f, -0.885439992f},  // 48
    {-0.163220003f, -0.52757901f, -0.83367902f},  // 49
    {0.126389995f, -0.313145995f, -0.941257f},  // 50
    {0.349548012f, -0.272226006f, -0.896498024f},  // 51
    {0.239917994f, -0.0858250037f, -0.966992021f},  // 52
    {0.390845001f, 0.0815370008f, -0.91683799f},  // 53
    {0.255266994f, 0.268696994f, -0.928785026f},  // 54
    {0.146245003f, 0.480437994f, -0.864749014f},  // 55
    {-0.326016009f, 0.478455991f, -0.815348983f},  // 56
    {-0.469682008f, -0.112519003f, -0.875635982f},  // 57
    {0.81844002f, -0.258520007f, -0.51315099f},  // 58
    {-0.474317998f, 0.292237997f, -0.830433011f},  // 59
    {0.778943002f, 0.395841986f, -0.486371011f},  // 60
    {0.624094009f, 0.39377299f, -0.674870014f},  // 61
    {0.740885973f, 0.203833997f, -0.639953017f},  // 62
    {0.48021701f, 0.565768003f, -0.670297027f},  // 63
    {0.380930007f, 0.424535006f, -0.821377993f},  // 64
    {-0.0934220031f, 0.501124024f, -0.860318005f},  // 65
    {-0.236485004f, 0.29619801f, -0.925387025f},  // 66
    {-0.131531f, 0.0939590037f, -0.98684901f},  // 67
    {-0.823562026f, 0.295776993f, -0.484005988f},  // 68
    {0.611065984f, -0.624303997f, -0.486663997f},  // 69
    {0.0694959983f, -0.520330012f, -0.851132989f},  // 70
    {0.226521999f, -0.664879024f, -0.711775005f},  // 71
    {0.471307993f, -0.568903983f, -0.67395699f},  // 72
    {0.388424993f, -0.742623985f, -0.545560002f},  // 73
    {0.783675015f, -0.480729014f, -0.393384993f},  // 74
    {0.962393999f, 0.135675997f, -0.235349f},  // 75
    {0.876607001f, 0.172033995f, -0.449405998f},  // 76
    {0.63340503f, 0.589793026f, -0.500940979f},  // 77
    {0.182275996f, 0.800657988f, -0.570720971f},  // 78
    {0.177002996f, 0.76413399f, 0.620297015f},  // 79
    {-0.544016004f, 0.675514996f, -0.497720987f},  // 80
    {-0.67929697f, 0.286466986f, -0.675642014f},  // 81
    {-0.59039098f, 0.0913690031f, -0.801928997f},  // 82
    {-0.824360013f, -0.133123994f, -0.550189018f},  // 83
    {-0.715794027f, -0.334542006f, -0.612960994f},  // 84
    {0.174285993f, -0.892484009f, 0.416049004f},  // 85
    {-0.0825280026f, -0.837122977f, -0.540753007f},  // 86
    {0.283331007f, -0.880873978f, -0.379189014f},  // 87
    {0.675134003f, -0.42662701f, -0.601817012f},  // 88
    {0.843720019f, -0.512335002f, -0.160155997f},  // 89
    {0.977303982f, -0.098555997f, -0.187519997f},  // 90
    {0.846294999f, 0.522671998f, -0.102946997f},  // 91
    {0.677141011f, 0.72132498f, -0.145501003f},  // 92
    {0.320964992f, 0.870891988f, -0.372193992f},  // 93
    {-0.178977996f, 0.911532998f, -0.370236009f},  // 94
    {-0.447169006f, 0.826700985f, -0.341473997f},  // 95
    {-0.703203022f, 0.496327996f, -0.509081006f},  // 96
    {-0.977181017f, 0.0635629967f, -0.202674001f},  // 97
    {-0.878170013f, -0.412937999f, 0.241455004f},  // 98
    {-0.835830986f, -0.358550012f, -0.415728003f},  // 99
    {-0.499173999f, -0.693432987f, -0.519591987f},  // 100
    {-0.188788995f, -0.923753023f, -0.333225012f},  // 101
    {0.192254007f, -0.969361007f, -0.152896002f},  // 102
    {0.515940011f, -0.783906996f, -0.345391989f},  // 103
    {0.905924976f, -0.300951988f, -0.297870994f},  // 104
    {0.991111994f, -0.127746001f, 0.0371069983f},  // 105
    {0.995135009f, 0.0984240025f, -0.00438300008f},  // 106
    {0.760123014f, 0.64627701f, 0.0673670024f},  // 107
    {0.205220997f, 0.959580004f, -0.192590997f},  // 108
    {-0.042750001f, 0.97951299f, -0.196790993f},  // 109
    {-0.438017011f, 0.898926973f, 0.0084920004f},  // 110
    {-0.821994007f, 0.480785012f, -0.305238992f},  // 111
    {-0.899917006f, 0.0817100033f, -0.428337008f},  // 112
    {-0.92661202f, -0.144618005f, -0.347095996f},  // 113
    {-0.793659985f, -0.557792008f, -0.242838994f},  // 114
    {-0.431349993f, -0.847778976f, -0.308557987f},  // 115
    {-0.00549199991f, -0.964999974f, 0.262192994f},  // 116
    {0.58790499f, -0.804026008f, -0.088940002f},  // 117
    {0.699492991f, -0.667685986f, -0.254765004f},  // 118
    {0.889303029f, 0.359795004f, -0.282290995f},  // 119
    {0.780972004f, 0.197036996f, 0.59267199f},  // 120
    {0.520120978f, 0.506695986f, 0.687556982f},  // 121
    {0.403894991f, 0.693961024f, 0.596059978f},  // 122
    {-0.154982999f, 0.899236023f, 0.409090012f},  // 123
    {-0.657338023f, 0.537168026f, 0.528542995f},  // 124
    {-0.746195018f, 0.334091008f, 0.575827003f},  // 125
    {-0.624952018f, -0.0491439998f, 0.779115021f},  // 126
    {0.318141013f, -0.254714996f, 0.913185f},  // 127
    {-0.555896997f, 0.405294001f, 0.725751996f},  // 128
    {-0.794434011f, 0.0994059965f, 0.599160016f},  // 129
    {-0.640361011f, -0.689463019f, 0.338494986f},  // 130
    {-0.126712993f, -0.734094977f, 0.66711998f},  // 131
    {0.105457f, -0.780816972f, 0.615795016f},  // 132
    {0.407992989f, -0.480915993f, 0.776054978f},  // 133
    {0.695136011f, -0.545120001f, 0.468647003f},  // 134
    {0.973191023f, -0.00648899982f, 0.229908004f},  // 135
    {0.94689399f, 0.317508996f, -0.050799001f},  // 136
    {0.563583016f, 0.825612009f, 0.027183f},  // 137
    {0.325773001f, 0.945423007f, 0.00694900006f},  // 138
    {-0.171820998f, 0.985096991f, -0.00781499967f},  // 139
    {-0.670440972f, 0.739938974f, 0.0547689982f},  // 140
    {-0.822981f, 0.554961979f, 0.121321999f},  // 141
    {-0.96619302f, 0.117857002f, 0.229306996f},  // 142
    {-0.953769028f, -0.29470399f, 0.0589450002f},  // 143
    {-0.864386976f, -0.502727985f, -0.0100149997f},  // 144
    {-0.530609012f, -0.842006028f, -0.0973659977f},  // 145
    {-0.162617996f, -0.98407501f, 0.0717720017f},  // 146
    {0.0814469978f, -0.996011019f, 0.0364390016f},  // 147
    {0.745984018f, -0.665962994f, 0.000761999981f},  // 148
    {0.942057014f, -0.329268992f, -0.0641060024f},  // 149
    {0.939701974f, -0.281089991f, 0.194802999f},  // 150
    {0.771214008f, 0.550670028f, 0.319362998f},  // 151
    {0.641348004f, 0.730690002f, 0.234020993f},  // 152
    {0.080682002f, 0.996690989f, 0.00987899955f},  // 153
    {-0.0467250012f, 0.976643026f, 0.209725007f},  // 154
    {-0.531076014f, 0.821000993f, 0.209562004f},  // 155
    {-0.695815027f, 0.655990005f, 0.29243499f},  // 156
    {-0.976122022f, 0.216709003f, -0.0149130002f},  // 157
    {-0.961660981f, -0.144128993f, 0.233313993f},  // 158
    {-0.772083998f, -0.613646984f, 0.165298998f},  // 159
    {-0.449600011f, -0.836059988f, 0.314426005f},  // 160
    {-0.392699987f, -0.914615989f, 0.0962470025f},  // 161
    {0.390588999f, -0.919470012f, 0.0448900014f},  // 162
    {0.582529008f, -0.799197972f, 0.148127005f},  // 163
    {0.866430998f, -0.489811987f, 0.096864f},  // 164
    {0.904586971f, 0.111497998f, 0.411449999f},  // 165
    {0.953536987f, 0.232329994f, 0.191806003f},  // 166
    {0.497310996f, 0.770802975f, 0.398176998f},  // 167
    {0.194066003f, 0.956319988f, 0.218611002f},  // 168
    {0.422876f, 0.882275999f, 0.206797004f},  // 169
    {-0.373796999f, 0.849565983f, 0.372173995f},  // 170
    {-0.534497023f, 0.714022994f, 0.452199996f},  // 171
    {-0.881826997f, 0.237159997f, 0.407597989f},  // 172
    {-0.904947996f, -0.0140690003f, 0.425289005f},  // 173
    {-0.751827002f, -0.512817025f, 0.414458007f},  // 174
    {-0.501015007f, -0.697916985f, 0.511758029f},  // 175
    {-0.235190004f, -0.92592299f, 0.295554996f},  // 176
    {0.228983f, -0.953939974f, 0.193819001f},  // 177
    {0.734025002f, -0.634898007f, 0.241062f},  // 178
    {0.913752973f, -0.0632530004f, -0.401315987f},  // 179
    {0.905735016f, -0.161486998f, 0.391874999f},  // 180
    {0.858929992f, 0.342445999f, 0.380748987f},  // 181
    {0.624486029f, 0.607581019f, 0.490776986f},  // 182
    {0.289263994f, 0.857478976f, 0.425507993f},  // 183
    {0.0699680001f, 0.902168989f, 0.425671011f},  // 184
    {-0.28617999f, 0.940699995f, 0.182164997f},  // 185
    {-0.574012995f, 0.805118978f, -0.149308994f},  // 186
    {0.111258f, 0.099717997f, -0.988776028f},  // 187
    {-0.30539301f, -0.944227993f, -0.123159997f},  // 188
    {-0.60116601f, -0.789575994f, 0.123163f},  // 189
    {-0.290645003f, -0.812139988f, 0.50591898f},  // 190
    {-0.0649200007f, -0.877162993f, 0.475784987f},  // 191
    {0.408300996f, -0.862215996f, 0.299789011f},  // 192
    {0.566097021f, -0.72556603f, 0.391263992f},  // 193
    {0.839363992f, -0.427386999f, 0.335869014f},  // 194
    {0.818899989f, -0.0413050018f, 0.572448015f},  // 195
    {0.719784021f, 0.414997011f, 0.556496978f},  // 196
    {0.881744027f, 0.450269997f, 0.140659004f},  // 197
    {0.401823014f, -0.898220003f, -0.178151995f},  // 198
    {-0.0540199988f, 0.791343987f, 0.60898f},  // 199
    {-0.293774009f, 0.763993979f, 0.574464977f},  // 200
    {-0.450798005f, 0.610346973f, 0.651350975f},  // 201
    {-0.638221025f, 0.186693996f, 0.746873021f},  // 202
    {-0.872870028f, -0.257126987f, 0.414707989f},  // 203
    {-0.587257028f, -0.521709979f, 0.618827999f},  // 204
    {-0.353657991f, -0.641973972f, 0.680290997f},  // 205
    {0.0416489989f, -0.611272991f, 0.790323019f},  // 206
    {0.348342001f, -0.779182971f, 0.521086991f},  // 207
    {0.499166995f, -0.622440994f, 0.602825999f},  // 208
    {0.790018976f, -0.303831011f, 0.532500029f},  // 209
    {0.660117984f, 0.0607330017f, 0.74870199f},  // 210
    {0.604920983f, 0.294160992f, 0.739960015f},  // 211
    {0.385697007f, 0.379346013f, 0.841032028f},  // 212
    {0.239693001f, 0.207875997f, 0.948332012f},  // 213
    {0.012623f, 0.258531988f, 0.965919971f},  // 214
    {-0.100556999f, 0.457147002f, 0.883687973f},  // 215
    {0.0469669998f, 0.628588021f, 0.776319027f},  // 216
    {-0.430391014f, -0.445405006f, 0.785097003f},  // 217
    {-0.434291005f, -0.196227998f, 0.879139006f},  // 218
    {-0.256637007f, -0.336867005f, 0.905902028f},  // 219
    {-0.131372005f, -0.158910006f, 0.978514016f},  // 220
    {0.102379002f, -0.208766997f, 0.972591996f},  // 221
    {0.195686996f, -0.450129002f, 0.87125802f},  // 222
    {0.627318978f, -0.423148006f, 0.653770983f},  // 223
    {0.687439024f, -0.171582997f, 0.70568198f},  // 224
    {0.275920004f, -0.0212549996f, 0.960946023f},  // 225
    {0.459367007f, 0.157465994f, 0.874177992f},  // 226
    {0.285394996f, 0.583184004f, 0.760555983f},  // 227
    {-0.812174022f, 0.460303009f, 0.358460993f},  // 228
    {-0.189068004f, 0.641223013f, 0.743698001f},  // 229
    {-0.338874996f, 0.476480007f, 0.811251998f},  // 230
    {-0.920993984f, 0.347185999f, 0.176726997f},  // 231
    {0.0406389982f, 0.0244650003f, 0.998874009f},  // 232
    {-0.739131987f, -0.35374701f, 0.573189974f},  // 233
    {-0.603511989f, -0.286615014f, 0.74405998f},  // 234
    {-0.188676f, -0.547059f, 0.815554023f},  // 235
    {-0.0260450002f, -0.397819996f, 0.917093992f},  // 236
    {0.26789701f, -0.649040997f, 0.71202302f},  // 237
    {0.518245995f, -0.284891009f, 0.806385994f},  // 238
    {0.493450999f, -0.0665329993f, 0.867224991f},  // 239
    {-0.328188002f, 0.140250996f, 0.934143007f},  // 240
    {-0.328188002f, 0.140250996f, 0.934143007f},  // 241
    {-0.328188002f, 0.140250996f, 0.934143007f},  // 242
    {-0.328188002f, 0.140250996f, 0.934143007f},  // 243
    {-0.328188002f, 0.140250996f, 0.934143007f},  // 244
};
```

## §4 voxels.vpl + palette + house remap

**File layout confirmed** (`FUN_00757850`): 16-byte header read as 4 dwords —
`remap_start`, `remap_end`, `section_count`, `unused` (stored to
`DAT_00B2EADC`, `DAT_00B420E4`, `DAT_00B2EAC0`, `DAT_00B420E0`); then exactly
`0x300` palette bytes → the shared palette buffer `DAT_00B2EAE0`; then
`section_count << 8` table bytes → `DAT_00B400E0`. For the real file:
8976 = 16 + 768 + 32·256 ✓ (`section_count` = 32).

**What the draw actually indexes at blit time: the theater unit palette, not
the vpl's embedded palette.** The 256×256 buffer holds raw indices produced by
`vpl_table[level][color]`; the blit (`FUN_00706360` → `FUN_004AF1B0`) fetches
the unit's **ConvertClass** through the same virtual pair (`+0x2EC/+0x2F0`)
SHP drawing uses — i.e. voxels are palettized through `unittem.pal` (theater
unit palette, loaded from `ra2.mix → cache.mix` — same object the engine
already loads as `ua.pal`) with the standard per-house remap of **indices
16–31**, identical to SHP vehicles. Two corroborating negatives: (a) the
vpl-palette→16bpp converter `FUN_00752A10` has **zero callers** (dead code);
(b) the embedded VXL palette written into the same shared buffer is likewise
never consumed by the draw path. The vpl `remap_start/remap_end` header fields
are stored but no consumer was found — house remap is NOT driven by them.
Confidence: **High** on the vpl layout and the vpl-index chain; **Medium** on
"unittem.pal specifically" (the Convert is fetched indirectly per object; not
chased into the palette file byte-for-byte).

## §5 Drive locomotion — Speed and ROT

**Speed conversion** (TechnoTypeClass::LoadFromINI, disasm @ `0x7139EC`):

```
raw   = ini.ReadInt(section, "Speed", −1)         // key @0x81C9DC
if raw != −1:
    v = clamp(raw, 0, 100)
    type+0x678 = min(255, v · 256 / 100)          // ×0x51EB851F magic = /100
```

**Speed=6 → 15 leptons per frame** (6·256/100 = 15.36 → 15, truncated);
GI Speed=4 → 10. Cells/sec = `speed_leptons·tick_rate/256`: at the engine's
15 Hz sim, Speed=6 = **0.879 cells/s** and the tank:GI ratio is 15:10 = 3:2.
(Per-second numbers scale with tick rate; the per-frame lepton value is the
binary's ground truth.) This stored value is the unit's flat-ground maximum;
terrain/veterancy multipliers apply downstream. Confidence **High** for the
conversion; **Medium** for "leptons/frame" units (the constant's consumer
chain into the coord update was not fully unwound, but 256 leptons = 1 cell
and the identical formula is used for projectile Speed, §7, where
leptons/frame is unambiguous).

**ROT.** Parsed raw to `type+0x71C` ("ROT" @ `0x81A174`); UnitClass ctor
(`FUN_00734490`) calls `FacingClass::Set_ROT(ROT)` on **both** the body facing
and the turret facing — **turret slew rate = hull ROT, same constant**.
`Set_ROT` (`FUN_004C95A0`): `rate_u16_per_frame = min(ROT, 127) << 8`.

**FacingClass** (0x18-byte struct: `+0` desired u16, `+4` start u16, `+8`
timer start-frame (−1 idle), `+0x10` timer duration (frames), `+0x14` rate):

- `Set_Desired` (`FUN_004C9140`): `start = Current(); desired = new;
  timer.start = now; timer.duration = |signed16(new − Current())| / rate` —
  the signed 16-bit delta wraps, so the turn is **automatically shortest-arc**.
- `Current()` (`FUN_004C92F0`): idle → desired; else
  `desired − (delta/steps)·remaining` — i.e. facing advances **rate = ROT·256
  u16-units per frame** until it lands exactly on desired.
- `Set` (`FUN_004C9220`) is a separate instant-snap setter (ctors, teleports).

**ROT=5 → 1280 u16/frame → 180° in 25.6 frames, full circle 51.2 frames
(3.41 s at 15 Hz)**. The plan's `kRotU16PerRotPerSec = 256·15` is exactly the
recovered per-frame step × 15. Confidence **High** (all four functions read
directly).

**Turn-then-move / aim gating.** Direct evidence found: (a) mission-level
code (`FUN_00728490`) computes the target direction with atan2 and refuses to
proceed while `Current() != desired_dir` — the gate is **rotation-complete
(exact 16-bit equality after the timed turn)**, not a ±tolerance window; (b)
the turret AI (`FUN_00735A60`) stores `Is_Rotating(turret)` on the unit each
frame for the fire path to consult (FireError::ROTATING-shaped). The Drive
locomotor's own per-frame move gating (turn-in-place before rolling) was not
fully unwound within budget — its Draw_Matrix maintains a dedicated
turn-in-progress timer (loco `+0x20/+0x28/+0x2C`) used to blend ramp
transitions, consistent with hull-turn-before-move, but the Process-side
check was not isolated. Confidence: **High** on "fire waits for exact
aim-complete", **Medium** on the move gating internals `[RE-FALLBACK:
TS-source behavior prose — Drive turns the hull to the track heading before
accelerating]`.

**Turret idle return** (`FUN_00735A60`, direct read): a turreted unit with no
target slews the turret toward **the body facing** when stationary, or toward
**the movement destination direction** while driving — but only once
`now − last_fire ≥ ROF + 5` frames (the +5 is a literal). `TurretSpins` types
instead advance the desired facing by +8·256 u16 ahead of current
continuously. Confidence **High**.

## §6 FLH fire point

`TechnoClass::GetFLH` = `FUN_006F3030` (and the burst-flip variant
`FUN_006F32C0`). `[Elite]Primary/SecondaryFireFLH=F,L,H` is parsed
(`0x711510`) into the per-weapon entry at `+4/+8/+0xC` — **raw leptons, no
scaling**. At fire time:

```
turret_yaw = (idx32(turret_facing) − 8) · (−2π/32)      // same §2 quantizer
world = obj_coord                                        // leptons
      + M · (0, 0, 0)  where  M = [loco tilt-matrix ∘] T(type.TurretOffset,0,0)
                                   ∘ Rz(turret_yaw) ∘ T(F, ±L, H)
```

so: **F forward along the quantized turret facing, L lateral (negated every
other burst — object flag bit at dword index `0xEE` of the object, the burst-side flip), H up;
all in leptons in TURRET space**, plus the type's `TurretOffset` (leptons,
`type+0x720`) applied before the rotation, plus the slope tilt matrix when on
a ramp. The result is a world coordinate; the muzzle-flash screen offset is
just `world→screen(world) − world→screen(obj_coord)` through the ordinary
tactical projection (60×30 px per cell, 256 leptons per cell). Confidence
**High** (decompile is explicit; weapon-entry offsets corroborated by the
parser).

## §7 Projectile + splash deltas

> **[SUPERSEDED by `docs/re/projectile.md`, 2026-07-21.]** The two claims below
> about *how a cannon shell travels* are WRONG and were corrected by the
> projectile hunt:
>
> 1. **`Speed=` is not the launch speed for a `ROT==0` projectile.**
>    `WeaponTypeClass::GetSpeed` (`FUN_00771E90`) branches on `Projectile->ROT`;
>    for `ROT==0` it returns `ftol(sqrt(distance × Gravity × 1.2))`, recomputed
>    **per shot**. So `102 leptons/frame` is correct only at the Rhino's *maximum*
>    range, not in general. (Decisive counter-example: `[155mm] Speed=10` = 25
>    lept/frame provably cannot reach its own 8-cell range under `Gravity=6`; the
>    real launch speed is 121.)
> 2. **`[Cannon]` is `Arcing=true`, not straight-line.** The "no Arcing/ROT in
>    the rules data" corroboration below was a misread; cannon shells follow a
>    ballistic arc integrated as `V.z -= Gravity; pos += V` once per frame.
>
> Also note `Gravity=6` lives in **`[AudioVisual]`**, not `[General]`.
> The parse formula itself (`min(255, clamp(ini,0,100)·256/100)`) and the
> `Range` reading below remain correct; only the *interpretation* of `Speed=`
> for non-homing projectiles was wrong. See `projectile.md` for the full model.

**Weapon Speed** (projectile flight speed) is parsed by `FUN_00474740` with
the **identical** formula to unit speed: `min(255, clamp(ini,0,100)·256/100)`
→ **Speed=40 → 102 leptons/frame** (2.5 frames/cell). Cannon-class
projectiles fly a straight line at that rate (ROT=0/non-arcing bullet class;
the BulletType parser sits in a gap at `~0x46Bxxx` — flight integration not
decompiled within budget, **Medium**, straight-line is corroborated by the
rules data: `[Cannon]` has no Arcing/ROT). Weapon `Range` is read as a
float in cells and converted to leptons at parse (`FUN_00474550`, ftol; 5.75
cells → 1472 leptons), matching combat.md's lepton-space range checks.

**CellSpread / PercentAtMax** (WarheadTypeClass `+0x124`/`+0x12C`, floats,
parsed at `0x75C1C0`). The spread-damage applicator `FUN_00489190`:

- collects victim objects whose 3-D lepton distance from the impact point is
  `≤ ftol(CellSpread · 256)` (cell scan driven by cumulative ring-count table
  `DAT_007EC3D0[trunc(spread)]` with per-ring cell offsets @ `DAT_00ABC408`);
- calls each victim's `ReceiveDamage(&damage, distance, warhead, …)` with the
  exact lepton distance;
- `FUN_00489090` applies `PercentAtMax` (`damage · PercentAtMax`, clamped to
  `[CombatDamage]MaxDamage`) as the far-end factor. The exact interpolation
  between full damage at 0 and `damage·PercentAtMax` at CellSpread inside
  ReceiveDamage was not isolated (the 23 KB function reads CellSpread again at
  `0x70137D` only for the DelayKill path) — **Medium**, `[RE-FALLBACK:
  ModEnc "damage falls off linearly with distance, reaching
  Damage×PercentAtMax at the CellSpread edge"]`.

**For `[AP] CellSpread=.3`:** radius = ftol(0.3·256) = **76 leptons ≈ 0.3
cells** — reaches only objects inside a third of a cell around the impact
point, i.e. in practice **only the direct target** (adjacent-cell occupants
are ≥ ~181 leptons away for diagonals, ≥ 256 center-to-center).
**Recommendation: keep single-target damage for this phase** — a faithful
divergence-free simplification for the Rhino's AP warhead; PercentAtMax never
engages on a direct hit (distance ≈ 0 → full 90 damage, matching the plan's
`trunc(90×100%)` heavy / `trunc(90×25%)=22` none vectors). Multi-cell splash
only matters for CellSpread ≥ ~0.7 warheads; document as out of phase scope.

## Confidence table

| Item | Evidence | Confidence |
|------|----------|:----------:|
| §1 draw architecture (256×256 indexed buffer + byte z-buffer + capture/cache/blit) | rasterizer variants + `FUN_00752C20`/`FUN_00753330`/`FUN_00705AD0` decompiles | **High** |
| §1 base matrix Rx(−60°)·Rz(−45°), y-negation, 1 unit = 1 px | camera setters disassembled, constants 45.0/60.0/deg2rad read raw; corner transform in `FUN_00752F10` | **High** |
| §1 voxel positions corner-anchored, min/max-bounds steps, no det_scale on geometry | runtime-tailer construction in `FUN_00754BD0` + dispatcher step math | **High** |
| §1 det_scale × HVA translation (load-time) | `FUN_005BD160` decompile ×(+0xC,+0x1C,+0x2C) | **High** |
| §1 span walk + 0xFFFFFFFF test + 2-px footprint + strict-greater z-test | `VXR_756A10`/`VXR_757010` decompiles | **High** |
| §1 depth rule (section sort by max-z + per-pixel byte depth, +0.61/−0.61/+0.5 axis) | flush sort + context component 2 | **High** |
| §2 32 facings, round-to-nearest `((f>>10)+1)>>1 & 0x1F`, yaw = (idx−8)·(−2π/32) | three independent code sites + dbl constant @0x7E3408 | **High** |
| §2 cache key layout (ramp/facing/frame fields) | forced disasm of the unit draw + `FUN_0055A3C0` | **High** |
| §2 screen calibration (facing 0x4000 = East = down-right 2:1) | derived from the recovered basis; consistent with infantry-anim.md | **High** (arith), **Medium** (not visually verified in-game) |
| §3 table sizes 16/36/64/245 + data | pointer/count arrays + unit-length scan of all 361 vectors | **High** |
| §3 lighting formula (·16.0, clamp-at-0, spec h/(3−2h), indices 253–255→16) | disassembly of both builders; `FMUL [0x7F5960]`=16.0 at `0x7574D9`/`0x75765F` | **High** |
| §3 row semantics: base/identity row = 8 ((d+s)=0.5), rows 9–31 brighten, 7–0 darken | real voxels.vpl fixed-point scan (199 @ row 8 vs 13 @ row 16, task-2-report) — corrects this doc's earlier "row 16 neutral" inference | **High** |
| §4 vpl header/consumption (`level*256 + color` into `DAT_00B400E0`) | loader + rasterizer decompiles | **High** |
| §4 final palette = unit ConvertClass (SHP-style house remap 16–31), vpl palette unused at draw | blit path virtuals + dead converter | **Medium-High** |
| §5 Speed → `min(255, clamp(0..100)·256/100)` stored `+0x678` | disasm @0x7139EC | **High** (formula), **Medium** (leptons/frame unit attribution) |
| §5 ROT: rate = min(ROT,127)·256 u16/frame; timed shortest-arc turn; turret uses same ROT | `FUN_004C95A0`/`FUN_004C9140`/`FUN_004C92F0`/`FUN_00734490` | **High** |
| §5 fire gate = rotation-complete (exact facing), Is_Rotating consulted | `FUN_00728490`, `FUN_00735A60` | **High** for the mechanism, **Medium** that no other ±tolerance path exists |
| §5 Drive turn-before-move internals | loco turn timer seen; Process not unwound | **Medium** `[RE-FALLBACK]` |
| §5 turret idle return (to body / to move dir, after ROF+5) | `FUN_00735A60` direct read | **High** |
| §6 FLH semantics (leptons, turret space, quantized facing, burst L-flip, TurretOffset) | `FUN_006F3030` decompile | **High** |
| §7 weapon Speed=40 → 102 leptons/frame | `FUN_00474740` decompile | **High** |
| §7 Cannon straight-line flight integration | not decompiled | **Medium** `[RE-FALLBACK: rules data + ModEnc]` |
| §7 CellSpread victim radius = CellSpread·256 leptons; 0.3 → target-only | `FUN_00489190` decompile | **High** |
| §7 exact linear falloff form inside ReceiveDamage | PercentAtMax helper found; interpolation not isolated | **Medium** `[RE-FALLBACK: ModEnc]` |

## Decisions for ra2engine

Resolving every `[RE: voxel-render.md §N]` marker in Tasks 4–6:

- **§2 `kVehicleFacings = 32` — CONFIRMED.** Keep 32 for hull, turret and
  barrel (`facing_cache.hpp`).
- **§2 `facing_to_index`** — the original rounds to nearest:
  `idx = ((u16 >> 10) + 1 >> 1) & 31`. The plan's
  `floor(f/2048 + 0.5) mod 32` is arithmetically identical for all inputs —
  keep it.
- **§2 `index_to_yaw`** — AMEND to `yaw(i) = (i − 8) · (−2π/32)`: index 8
  (facing 0x4000, screen East = down-right 2:1 diagonal) is the unrotated
  model (+X nose); index 0 (facing 0, screen North = up-right) is +90°. Under
  the plan's own projection basis, what matters is: model +X must point
  screen-East at facing 0x4000 and rotate CLOCKWISE on screen with increasing
  facing. Spawn facing 32768 (South, screen down-left) is index 16 — keep the
  32768 spawn constant.
- **§1 voxel centers — AMEND**: the original is corner-anchored:
  `local = min_bounds + i · (max − min) / size` (no `+ 0.5`). The half-voxel
  difference only shifts the whole model by ≤ half a pixel; adopting the
  original's form costs nothing and removes a divergence.
- **§1 scale applies to HVA translation — CONFIRMED** (applied once at load,
  `FUN_005BD160`). The plan's `apply_xform` multiplying `t[r][3] * scale` per
  call is equivalent — keep.
- **§1 projection constants — AMEND** `kIsoX/kIsoY/kIsoZ` to the recovered
  basis: `sx = 0.70711·(x + y)`, `sy = 0.35355·(x − y) − 0.86603·z` (screen y
  down) — i.e. in the plan's `(wx − wy)·kIsoX / (wx + wy)·kIsoY − wz·kIsoZ`
  shape (whose x/y axis roles are swapped relative to the binary's): kIsoX =
  0.70711, kIsoY = 0.35355, kIsoZ = 0.86603, applied to the appropriately
  matched axis expressions. 1 voxel unit = 1 px; do NOT scale by cell metrics
  inside the rasterizer.
- **§1 depth rule — AMEND** to `depth = 0.61237·(x − y) + 0.5·z` (the third
  matrix row), larger = nearer, strictly-greater test. (The plan's
  `x + y + z` placeholder has the wrong axis weights: it would z-fight along
  the viewing diagonal.) Per-section painter sort by max depth before
  rasterizing matches the original and is optional when a full z-buffer is
  kept.
- **§1 footprint rule — RESOLVED**: the original plots exactly 2 horizontal
  pixels per voxel (no vertical doubling). With 1 unit = 1 px and 2-px
  writes, pinholes match vanilla; the plan's `ceil(kIsoX)`-square becomes a
  fixed 2×1.
- **§3 normal tables** — mode 2 = 36 entries as expected; **mode 4 has 245
  entries, not 244 — AMEND `kNormals4[244]` → `kNormals4[245]`** (or clamp
  indices ≥ 244 to 244; entries 240–244 are identical anyway). Out-of-range
  clamp to the last entry stays safe. htnkbarl (mode 4) needs the 245-table;
  htnk/htnktur use mode 2.
- **§3 lighting rule — AMEND** the plan's
  `(dot·0.5 + 0.5)·(sections−1)` to the recovered
  `level = ftol(max(0, dot(n, L)) · 16)` (+ the optional specular term
  `h/(3−2h)` with H = normalize(L + z_model); skipping it is a visible-but-
  small divergence — flat diffuse voxels; recommend implementing both terms,
  they are three dot products). Force normal indices 253–255 to level 16.
  kLightDir default: `RotY(θ)·(−√½, −√½, 0)` rotated into model space by the
  inverse model rotation; with θ=0 and yaw-only models this reduces to
  `L_model = Rz(−yaw)·(−0.7071, −0.7071, 0)`. **The base/identity row is 8,
  not 16** (real-file evidence: 199 fixed points in row 8 vs 13 in row 16 —
  see §3): (d+s) = 0.5 → unmodified color; use row 8 for Task 4's
  identity/"full brightness" spot-check and expect fully lit surfaces to land
  in the bright rows well above 8, not at a fixed "full" row.
- **§4 palette — CONFIRMED with correction**: final colors index the unit
  palette (`unittem.pal` from `ra2.mix → cache.mix`, already loaded as
  `ua.pal`) through the same per-house 16–31 remap as SHP (`pal_for_house`
  reuse is correct). The vpl's embedded 768-byte palette and the VXL's
  embedded palette are NOT consulted at draw time; vpl `remap_start/remap_end`
  drive nothing.
- **§5 `kCellsPerSecPerSpeed`** — vanilla is
  `leptons_per_frame = min(255, clamp(Speed,0,100)·256/100)`, i.e. at 15 Hz
  **cells/sec = 0.15·Speed** (Speed=6 → 0.879 c/s), NOT the plan's 0.75·Speed
  (6 → 4.5 c/s). The 3:2 tank:GI ratio is preserved either way. Decision for
  the controller: keeping the engine's established (≈5× faster) absolute
  scale is a *known divergence* inherited from the infantry phase — if so,
  keep `speed·kCellsPerSecPerSpeed` for consistency and record the
  divergence; if vanilla absolute feel is wanted, set
  `kCellsPerSecPerSpeed = 0.15` for ALL units (infantry included) in a later
  calibration pass, not piecemeal.
- **§5 `kRotU16PerRotPerSec = 256·15` — CONFIRMED** (`Set_ROT` = ROT·256
  u16/frame). Shortest-arc turn with exact-landing (the recovered
  `Set_Desired` computes duration = |signed delta|/rate) — the plan's
  `turn_toward` clamp-to-target behaves identically.
- **§5 `kAimToleranceU16 = 1024` — AMEND (or document divergence)**: vanilla
  holds fire until the turret's timed rotation COMPLETES (exact facing
  equality, tolerance 0), tracked via Is_Rotating. Since the plan's
  `turn_toward` reaches the target exactly within one final frame, setting
  the tolerance to 0-after-arrival (i.e. `vehicle_aimed == (turret_facing ==
  target_facing)` with the same quantized compare) is both faithful and
  simple; 1024 fires up to ~5.6° early — acceptable, but record it as a
  divergence if kept. Same rule for the hull turn-then-move gate.
- **§5 turret idle return — CONFIRMED**: slew back toward `body_facing` when
  idle (or toward the move direction while driving), same ROT, after an
  ROF+5-frame hold. The plan's step 3 (`step_turrets` slews toward body when
  no target) matches; the ROF+5 delay is a nuance, safe to omit or add.
- **§6 FLH — RESOLVED**: `PrimaryFireFLH=150,0,100` is leptons in turret
  space; muzzle world pos = unit pos + Rz(quantized turret yaw)·(150, ±0,
  100). Screen offset = project through the SAME cell→pixel transform used
  for unit anchors (256 leptons = 1 cell = 60 px wide / 30 px high; height
  leptons map through the z term, ≈ −0.866·z·(60/(256√2)) px… in engine
  practice: convert leptons→cells and reuse `cell_to_screen`'s existing
  basis). §6 landed High-confidence — implement the flash at the computed FLH
  point, no fallback needed. L is negated on alternate bursts (Burst=1 for
  the 120mm → never flips).
- **§7 — RESOLVED: keep single-target damage.** CellSpread 0.3 collects
  victims only within 76 leptons of impact — the direct target. Damage 90 vs
  heavy (100%) = 90, vs none (25%) = 22 (trunc) — the plan's `--demo-tank`
  assertions are exactly vanilla. Projectile flight (102 leptons/frame,
  ~0.63 s to cross max range 1472) may be modeled as instant-hit for this
  phase; record as divergence (vanilla shells take ~2.5 frames/cell of
  travel and can miss moving targets — out of scope).

## Worked example (HTNK / Rhino)

- Speed=6 → 15 leptons/frame; ROT=5 → 1280 u16/frame (full turn 51.2 frames);
  turret ROT the same.
- Hull htnk.vxl 46×34×12 mode 2 → kNormals2; barrel mode 4 → kNormals4[245].
- At facing 0x4000 the hull renders unrotated: 46 model units ≈ 46 px along
  screen-East; cached under key idx32=8 (ramp 0, frame 0).
- 120mm FLH (150, 0, 100): muzzle = unit + Rz(yaw)·(150,0,100) leptons →
  ~0.59 cells ahead of center, 100 leptons up.
- AP CellSpread .3 → 76-lepton splash: single victim; PercentAtMax .5 never
  applies on the direct hit.
