# Building rendering semantics — reverse-engineering notes

(Method: `docs/re/workflow.md`; image base `0x00400000`, YR 1.001 `gamemd.exe`.
Hunt scripts, all off-repo at `D:\ra2-re\scripts\`: `building_hunt.py` …
`building_hunt11.py` — string-anchor sweep (`"BuildSpeed"`, `"NewTheater"`,
`"Foundation"`, `"Adjacent"`, `"Buildup"`, sidebar/theater filename strings) →
`getReferencesTo` + raw dword scans → forward gap-fill `createFunction` over the
undefined BuildingClass/BuildingTypeClass regions → targeted decompiles +
forced-disassembly listings. Full dumps under `D:\ra2-re\out\bh*_*.c/.txt`;
nothing under `D:\ra2-re` is committed. Content-anchored only: every function
below was reached from string/data/xref evidence in THIS binary, never from
published addresses. Empirical asset facts were measured with the repo's own
`ra2extract` against the real install and are restated inline.)

This document is one of the two calibration authorities for the buildings
phase (the other is `construction.md`): §1 NewTheater filename resolution,
§2 foundation semantics + canvas registration, §3 draw order, §4 damage
states/frame layout, §5 buildup anim, §6 palette + remap. Confidence per item;
"Decisions for ra2engine" at the end resolves every
`[RE: building-render.md §N]` marker in the plan's Tasks 2–7.

## Located functions

| Function | Role | How found |
|----------|------|-----------|
| `FUN_005F8860` | ObjectTypeClass main-image fetch: builds `<image>.shp` name, applies Theater= extension or NewTheater letter munge, resolves via mix lookup, 'G' fallback | xref of `"NewTheater"` sibling flag reads; reads theater letter `0x7E0BCE` |
| `FUN_005F8EA0` | the NewTheater letter-substitution helper (shared by buildup load) | xref of theater letter byte `0x7E0BCE` |
| `FUN_005F8F00` | 5-byte generic fallback: `name[1] = 'G'` | callee of the buildup-load miss path |
| `FUN_005B3AE0` | global MIX filename→file resolve (searches every registered mix) | known from `mix.md` (phase 1) |
| `FUN_00534700` | Init_Theater: loads `%s.MIX`, `ISO%s.MIX`, `<ext>.MIX`, `%sMD.MIX` variants + `%s.PAL`, `UNIT%s.PAL` | xrefs of `"%s.MIX"` @`0x826D74`, `"UNIT%s.PAL"` @`0x826D40` |
| `FUN_005301A0` | bootstrap mix load incl. `GENERIC.MIX`, `GENERMD.MIX`, `ISOGEN.MIX` | xref of `"GENERIC.MIX"` @`0x825820` |
| `0x7E0B78` | theater descriptor table, 6 entries, stride 0x70 (raw-dumped) | probe scan for `"TEMPERATE"`/`"SNOW"`/… |
| `FUN_0045FE40` | BuildingTypeClass::LoadFromINI (19,475 B, gap-filled): `Adjacent`→+0xEB4, `Power`→+0xEE0/+0xEE4, `Foundation`→+0xEF0, `Buildup`→+0xE5C, `Bib`→+0x1570, `Gate`→+0x16B7, `GateStages`→+0x16F8, `CanBeOccupied`→+0x157B, `ZShapePointMove`→+0x1530, `ExtraLight`→+0x1548, `BaseNormal`→+0x154F | string refs `"Foundation"` @`0x819744`, `"Adjacent"` @`0x819E50`, `"Buildup"` @`0x81961C` |
| `FUN_00474CD0` | `Foundation=` string→enum parse over the pair table @`0x81A9E8` | xref of the `"1x1"` string ptr |
| `FUN_0045B1B0` | fills the per-foundation occupy/overlap cell-list tables @`0x89B870`/`0x89C2D8` | raw dword scan for `0x89B870` in exec |
| `FUN_0045EC80` / `FUN_0045EC90` | foundation width / height getters (int tables @`0x8182C8`/`0x818320`; height +1 when `Bib=yes`) | callees of the proximity check |
| `FUN_00447A10` | BuildingClass::Center_Coord (vtbl +0x48): origin + ((w−1)·128,(h−1)·128) leptons | BuildingClass vtable walk (RTTI `.?AVBuildingClass@@` → COL → vtable @`0x7E2EBC`) |
| `FUN_005F63C0` | TechnoClass sort key: returns `coord.x + coord.y` | callee of the BuildingClass override |
| `FUN_00449360` | BuildingClass sort-key override: base ± type tweaks (`TurretAnimIsVoxel` +32, `Gate` −16) | BuildingClass vtable slot +0xB8 |
| `0x43D200`–`0x43E100` | BuildingClass::Draw_It body (forced listing): image fetch, frame clamp to `frames/2`, per-cell lighting, `Techno_Draw_Object` call | gap listing around the `ConditionYellow` xrefs @`0x43D58F`… |
| `FUN_0043EF10` | BuildingClass::Shape_Number — the frame-index rule (healthy/damaged/buildup/gate/garrison) | callee at `0x43D735` in Draw_It |
| `FUN_0043FAA0` | per-tick visual damage-state update (`Health_Ratio` vs ConditionYellow / ConditionRed-for-CanBeOccupied) | BuildingClass vtable slot +0x5C; `ConditionRed` xref `0x43FBD7` |
| `FUN_00705290` | Techno_Draw_Object (shared SHP blit, shadow flag 0x100) | call target in Draw_It |
| `FUN_00465950` | BuildingTypeClass::Get_Buildup_Image: theater munge + 'G' fallback, `frames/2`, rate = ftol(BuildupTime·900/frames) | `"Buildup"` field consumer; forced listing `0x465950`–`0x465AF8` |
| `FUN_00668C20` | `[AudioVisual]` loader: `ConditionYellow`→rules+0x1700, `ConditionRed`→+0x1708 (doubles), `BuildingSlam`→+0x6EC, `BuildingDieSound`→+0x6E8 | string refs `"ConditionYellow"` @`0x839368`, `"BuildingSlam"` @`0x8394B8` |
| `FUN_0044E7B0` / `FUN_0044E880` | BuildingClass::Power_Output / Power_Drain (see `construction.md` §8) | consumers of type +0xEE0/+0xEE4 |
| `FUN_004B6B50` / `FUN_0052B790` | cameo drawing path / bootstrap — the only referrers of `"CAMEO.PAL"` @`0x81F4F0` | string xref |

Empirical ground truth (measured with `ra2extract` on the real install; SHP
header = u16 zero, u16 canvas_w, u16 canvas_h, u16 frames):

| file (location) | canvas | frames |
|---|---|---|
| `generic.mix/ggcnst.shp` | 284×226 | 6 |
| `generic.mix/ggpowr.shp` | 142×110 | 6 |
| `generic.mix/ggpile.shp` | 250×148 | 6 |
| `generic.mix/ggweap.shp` | 266×224 | 6 |
| `isotemp.mix/gtcnstmk.shp` | 284×226 | 58 |
| `isotemp.mix/gtpowrmk.shp` | 142×110 | 50 |
| `snow.mix/gacnst.shp` etc. | (snow-letter variants exist) | — |

No `gtcnst.shp` exists anywhere on disk; `gtcnstmk.shp` does. §1 explains both.

## §1 NewTheater filename resolution

`FUN_005F8860` (ObjectTypeClass image fetch) builds the art filename from the
`Image=` string (fallback: section name) + `.shp` and then:

1. **`Theater=yes`** (flag at type+0x22C): the *extension* is replaced with the
   theater's 3-letter extension from the descriptor table (`TEM`, `SNO`, `URB`,
   `DES`, `UBN`, `LUN`). No letter substitution. (Terrain-style art; not used
   by our 8 buildings.)
2. **`NewTheater=yes`** (flag at type+0x237, set by the artmd key) and a
   theater is active: the first two characters are tested case-insensitively —
   **if `name[0] ∈ {g, n, c, y}` AND `name[1] ∈ {a, t}`, then `name[1]` is
   replaced by the current theater's letter.** Otherwise the name is used
   literally.
3. The (possibly munged) name is resolved with the **global** MIX lookup
   `FUN_005B3AE0`, which searches every registered mix — there is **no
   per-art-type mix scoping**; on-disk placement is purely where Westwood put
   the files.
4. **On miss, `name[1] = 'G'`** (`0x47`, hardcoded in both the main-image path
   and the 5-byte helper `FUN_005F8F00` used by the buildup path) **and the
   lookup runs once more.** On a second miss the image pointer stays null (the
   engine draws nothing); there is no third candidate.

Theater letter table (raw data @`0x7E0B78`, stride 0x70, letter at entry+0x56;
3-letter ext at +0x4E; mix/pal base names at +0x30/+0x3A/+0x44):

| ID | letter | ext | data mix base | iso mix base |
|----|:---:|:---:|---|---|
| TEMPERATE | `T` | TEM | TEMPERAT | ISOTEMP |
| SNOW | `A` | SNO | SNOW | ISOSNOW |
| URBAN | `U` | URB | URBAN | ISOURB |
| DESERT | `D` | DES | DESERT | ISODES |
| NEWURBAN | `N` | UBN | URBANN | ISOUBN |
| LUNAR | `L` | LUN | LUNAR | ISOLUN |

Mixes actually loaded: at bootstrap (`FUN_005301A0`): `GENERIC.MIX`,
`GENERMD.MIX`, `ISOGEN.MIX` (plus the phase-1 set). Per theater
(`FUN_00534700` Init_Theater, temperate example): `TEMPERAT.MIX`,
`TEMPERATMD.MIX`, `ISOTEMP.MIX`, `ISOTEMMD.MIX`, `TEM.MIX`; palettes
`TEMPERAT.PAL` (terrain) and `UNITTEM.PAL` (unit/building palette; name built
as `UNIT%s.PAL` with the 3-letter ext).

**Candidate-list algorithm** (what the Task-2 resolver implements verbatim), for
image name `img` (e.g. `GACNST`), active theater letter `L`:

```
name = lower(img) + ".shp"
if NewTheater=yes and name[0] in {g,n,c,y} and name[1] in {a,t}:
    candidates = [ name with name[1]=L,  name with name[1]='g' ]
else:
    candidates = [ name ]
resolve each candidate against ALL loaded mixes (first hit wins)
```

This exactly reproduces the on-disk layout: temperate main art `GACNST` →
`gtcnst.shp` (miss) → `ggcnst.shp` (hit, `generic.mix`); snow → `gacnst.shp`
(hit, `snow.mix`); buildup `GACNSTMK` → temperate `gtcnstmk.shp` (hit,
`isotemp.mix`) so the 'G' fallback never fires for buildups — which is why
`ggcnstmk.shp`-style generics are unnecessary and `gtcnst.shp` never needed to
exist.

## §2 Foundation semantics + canvas registration

**Parse.** `Foundation=` is read from BOTH the art INI and rules INI
(`FUN_0045FE40`, art value read first, rules value overrides if present) via
`FUN_00474CD0`, which matches the string against a `{name, enum}` pair table
@`0x81A9E8`. Recovered table (22 entries):

```
1x1=0  2x1=1  1x2=2  2x2=3  2x3=4  3x2=5  3x3=6  3x5=7  4x2=8
3x3Refinery=9  1x3=10  3x1=11  4x3=12  1x4=13  1x5=14  2x6=15
2x5=16  5x3=17  4x4=18  3x4=19  6x4=20  0x0=21
```

Width/height per enum: int tables @`0x8182C8` (w) / `0x818320` (h):
`w = [1,2,1,2,2,3,3,3,4,3,1,3,4,1,1,2,2,5,4,3,6,0]`,
`h = [1,1,2,2,3,2,3,5,2,3,3,1,3,4,5,6,5,3,4,4,4,0]`.
`Get_Foundation_Height(include_bib)` (`FUN_0045EC90`) returns `h + 1` when
`include_bib` and `Bib=yes` (type+0x1570) — the bib adds one foundation row for
placement purposes only.

**Occupied cells.** `FUN_0045B1B0` fills a static occupy-list table
@`0x89B870` (stride 0x78 per foundation enum; a second "overlap" table
@`0x89C2D8`). Each list entry is a dword `(dy<<16)|dx` cell offset from the
placement origin cell, terminated by `0x7FFF7FFF`. All recovered lists are
exactly the full W×H rectangle with offsets `dx ∈ [0,w)`, `dy ∈ [0,h)`:
**the origin cell is the foundation's minimum-(x,y) corner (top-left in cell
space) and the building occupies `[cx..cx+w-1] × [cy..cy+h-1]`.**

**Center.** `BuildingClass::Center_Coord` (`FUN_00447A10`):

```
center.x = coord.x + fw*128 − 128        (leptons; 256 leptons = 1 cell)
center.y = coord.y + fh*128 − 128
```

i.e. the stored object coordinate is the origin cell's position and the center
is the middle of the W×H foundation rectangle.

**Canvas registration.** The main image is blitted by the shared
`Techno_Draw_Object` path (`FUN_00705290`, same routine units/infantry use,
shadow pass via flag `0x100`). As established in the unit/infantry phases, that
path anchors the **SHP canvas center** at the object's screen draw point, with
per-frame offsets (`SHPFrame.Left/Top`) already relative to the canvas. The
building's draw point is the screen projection of `Center_Coord` (the Draw_It
listing converts between center- and origin-relative points by projecting
`((fw−1)·256, (fh−1)·256)` leptons — visible at `0x43D692`–`0x43D6FE`).

Exact pixel formula at our 60×30 cell metric (30-half-width), for canvas
`(cw, ch)` and foundation `(fw, fh)` at origin cell `(cx, cy)`:

```
fnd_center_screen = screen projection of cell-space point
                    (cx + (fw−1)/2 + 0.5,  cy + (fh−1)/2 + 0.5)
                  = center of the union bbox of the fw×fh cell diamonds
px = fnd_center_screen.x − cw/2
py = fnd_center_screen.y − ch/2
```

(center–center registration, NOT bottom-center; the art canvases are authored
around the foundation center — e.g. `ggcnst.shp` 284×226 vs the 4×4
foundation's 240×120 diamond bbox, leaving symmetric margin plus headroom that
the frame offsets consume.)

## §3 Draw order

The painter's sort key for Technos is **`coord.x + coord.y`** (leptons) —
`FUN_005F63C0` fetches the object's coordinate twice and returns
`y + x` verbatim. The BuildingClass override (`FUN_00449360`) returns the base
key with two small per-type tweaks: `+32` leptons when `TurretAnimIsVoxel=yes`,
`−16` leptons when `Gate=yes`; for every normal building it is exactly the base
key. Because the coordinate used is the foundation **center** (§2), a multi-cell
building sorts at:

```
depth = (cx + (fw−1)/2) + (cy + (fh−1)/2)      [cell units]
```

which slots directly into the engine's `(fx+fy)` unit depth space. `ZAdjust` /
`YSort` art keys: never read in the recovered main-image draw path — the only
Y-sort keys parsed are the per-anim `PowerUp##YSort` (idle-anim machinery,
out of scope) and `ZShapePointMove` (the Z-overlay shape, not the main image).
They can be ignored for this phase.

## §4 Damage states + frame layout

Empirical: every main building SHP in our set has **6 frames = 3 image + 3
shadow**. Rendering `ggcnst.shp` shows f0 = pristine, f1 and f2 = two damaged
variants (differing in small debris/light details), f3–f5 = shadows of f0–f2
(f3's bounds exactly match f0's silhouette, etc.).

Frame index — `BuildingClass::Shape_Number` (`FUN_0043EF10`), for a normal
(non-Gate, non-garrisonable) building; `ratio = Health_Ratio()`,
`CY = rules ConditionYellow` (double @rules+0x1700, 50%):

- **healthy** (`ratio > CY`): frame = anim stage = **0** for a static building;
- **damaged** (`ratio ≤ CY`): frame = **1** when the building is in its normal
  idle state (BState 1); in other BStates the damaged base is
  `max(start+count)` over the type's anim-stage slots — irrelevant when no
  ActiveAnims are configured (our phase), where it degenerates to the same
  frame-1 layout;
- **nothing extra happens at ConditionRed** for the main image of normal
  buildings — ConditionRed (rules+0x1708, 25%) is used for the health-bar
  color, AI repair triggers, and the garrisonable special case below. Frame 2
  is only reachable through the anim-stage machinery; a static building never
  shows it.
- Special cases (recovered but out of scope): `CanBeOccupied=yes` uses a
  4-state layout (0 healthy / 1 damaged-at-ConditionRed / 2 occupied /
  3 occupied+damaged); `Gate=yes` uses `GateStages`.
- During construction (BState 0) the frame is the buildup stage counter (§5),
  and when selling (mission 0x13) the same range is played **reversed**
  (`frame = start + count − stage − 1`, listing @`0x43D527`).

**Shadow half confirmed in code:** Draw_It clamps the image frame to
`< frames/2` (`0x43D71C`–`0x43D76E`: if `Shape_Number ≥ frames/2` the frame is
forced to `frames/2`), and the shadow pass draws `frame + frames/2` via the
same `Techno_Draw_Object` shadow flag used by units. The HP threshold values
are the rulesmd `[AudioVisual]` ones: `ConditionYellow=50%`,
`ConditionRed=25%` (loader `FUN_00668C20`).

## §5 Buildup anim

`BuildingTypeClass::Get_Buildup_Image` (`FUN_00465950`, forced listing):

- The `Buildup=` name (type+0xE5C) goes through the **same theater
  substitution** as §1 (`FUN_005F8EA0`), with the same `'G'` fallback
  (`FUN_005F8F00`) on a miss.
- **`image_frames = shp.frames / 2`** — the MK SHP's second half is shadow
  frames. (Empirical cross-check: `gtpowrmk.shp` has 50 frames; its frame 49 —
  the last of the shadow half — has bounds `92×79@(44,27)`, byte-identical
  header geometry to `ggpowr.shp` frame 3, the completed building's shadow.
  `gtcnstmk.shp` 58 frames → 29 image; its f57 matches `ggcnst.shp` f3.)
- **Playback rate** (ticks per buildup frame, stored type+0xF0C):

  ```
  rate = ftol( Rules.BuildupTime × 900.0 / image_frames )     [≥ trunc]
  ```

  `BuildupTime` is a rules `[General]` double in minutes (rules+0x1518,
  rulesmd value `.06`); `900.0` is a literal double @`0x7E17F8` (frames per
  minute at 15 Hz). For GACNSTMK: `0.06 × 900 = 54` ticks total →
  `rate = ftol(54/29) = 1` tick/frame → the anim actually takes
  `29 ticks ≈ 1.93 s`. GAPOWRMK (25 image frames): `rate = ftol(54/25) = 2` →
  `50 ticks ≈ 3.33 s`. (`Gate=yes` overrides the frame count with
  `GateStages+1`; not applicable here.)
- The buildup **plays exactly once** (BState 0 stage counter 0…frames−1, §4),
  then the building switches to its normal art/state; production of the NEXT
  item is unaffected (the factory slot freed at placement). Power/production
  service starts when construction completes (see `construction.md` §8).
- **Placement sound:** rules `[AudioVisual] BuildingSlam` (loader stores the
  VocClass index at rules+0x6EC; rulesmd value `BuildingSlam=PlaceBuilding`;
  soundmd `[PlaceBuilding] Sounds=uplace`). It is a global key, not
  per-building. (`BuildingDieSound=` sits next to it at +0x6E8.)

## §6 Palette + remap

- `Init_Theater` (`FUN_00534700`) loads **`UNIT%s.PAL`** with the theater's
  3-letter extension — `unittem.pal` on temperate — expanding 6→8 bits with the
  same `<<2` the phase-1 palette work established, into the global unit-palette
  slot. Buildings draw through `Techno_Draw_Object` exactly like SHP
  units/infantry, taking a per-cell lighting/color convert (cell light level +
  `ExtraLight` type offset, Draw_It @`0x43D797`). **Buildings use the theater
  unit palette (`unittem.pal` from `ra2.mix/cache.mix`), same as vehicles and
  infantry.**
- **House remap:** `Remapable=yes` buildings use the house-color convert, which
  recolors palette indices 16–31 — identical to the existing `pal_for_house`
  scheme used for infantry/vehicles (established in the earlier phases; the
  draw path selects the owner's color convert the same way).
- **Cameos:** `CAMEO.PAL` (@`0x81F4F0`) is referenced by the cameo draw path
  (`FUN_004B6B50`) and loaded at bootstrap (`FUN_0052B790`) — cameo icons
  (60×48, `language.mix/cameo.mix`) draw with `cameo.pal` from
  `ra2.mix/cache.mix`, not the theater palette.

## Confidence

| Item | Evidence | Confidence |
|------|----------|:----------:|
| §1 letter substitution rule (`{g,n,c,y}×{a,t}`, position 1, letter table) | decompiled `FUN_005F8860`/`FUN_005F8EA0` + theater table raw bytes | **High** |
| §1 'G' fallback, then null; global mix search | `FUN_005F8F00` (`name[1]=0x47`), retry visible in both paths; `FUN_005B3AE0` | **High** |
| §1 mix load set per theater | decompiled Init_Theater + bootstrap | **High** |
| §2 origin = min corner; occupy = full W×H rect | occupy-table filler `FUN_0045B1B0` dwords + name/enum/dims tables | **High** |
| §2 center = origin + (f−1)·128 leptons | `FUN_00447A10` | **High** |
| §2 canvas center ↦ foundation-center screen point | shared draw path convention (established phases) + Draw_It (fw−1,fh−1)·256 projection; not a single literal formula in one place | **Medium-High** |
| §3 sort key = center x+y (+Gate/TurretAnimIsVoxel tweaks); no YSort/ZAdjust on main image | `FUN_005F63C0`, `FUN_00449360`, key sweep of LoadFromINI | **High** |
| §4 frame rule (0 healthy, 1 damaged at ≤ ConditionYellow; clamp < frames/2; shadows = 2nd half) | `FUN_0043EF10` + Draw_It listing + empirical 6-frame layout | **High** |
| §4 role of frame 2 (anim-stage path only) | Shape_Number damaged path reading anim-slot ends | **Medium** |
| §5 buildup frames/2, rate = ftol(BuildupTime·900/frames), play once, reversed sell | Get_Buildup_Image listing + 900.0 @`0x7E17F8` + Shape_Number | **High** |
| §5 BuildingSlam=PlaceBuilding placement sound | `[AudioVisual]` loader + rulesmd/soundmd values | **High** |
| §6 unittem.pal for buildings; cameo.pal for cameos | Init_Theater `UNIT%s.PAL` + cameo path xrefs | **High** |
| §6 remap rows 16–31 via house convert | draw-path convert selection + established `pal_for_house` scheme (not re-derived to the literal row indices in this hunt) | **Medium-High** |

## Decisions for ra2engine

Resolving every `[RE: building-render.md §N]` marker in the plan (Tasks 2–7):

- **§1 theater resolver (Tasks 2/7 `theater_candidates`)**: implement exactly
  the candidate list of §1: `[letter-substituted name, 'g' name]` when
  `NewTheater=yes` and `name[0]∈{g,n,c,y} && name[1]∈{a,t}`, else
  `[literal name]`; theater letters `T`(temperate)/`A`(snow)/`U`(urban). The
  resolver searches ALL loaded mixes, not per-candidate mix sets — the plan's
  `theater_candidates(id,'T', "isotemp.mix", "temperat.mix", ".shp")` mix-set
  parameters should be treated as the walk ORDER over already-open nested
  mixes, and must include `generic.mix` and `isotemp.mix` for temperate
  (pinned real locations: `ggcnst→generic.mix`, `gacnst→snow.mix`,
  `gtcnstmk→isotemp.mix`).
- **§2 foundation origin (Task 3 `Building.cx,cy`)**: origin = top-left
  (minimum x,y) cell; occupancy = all `fw×fh` cells `[cx..cx+fw-1]×[cy..cy+fh-1]`.
  Confirmed; the plan's pre-hunt convention stands. Ignore the bib row for
  occupancy (Bib only pads placement height; GAWEAP renders fine without it —
  documented divergence if skipped).
- **§2 canvas registration (Task 7d)**: REPLACE the pre-hunt bottom-center
  formula. Correct rule: `px = fnd_bbox_center_x − canvas_w/2`,
  `py = fnd_bbox_center_y − canvas_h/2`, where `fnd_bbox` is the union of the
  `fw×fh` cell diamonds from `cell_to_screen` (center–center registration).
- **§3 depth key (Task 7d)**: use `(cx + (fw−1)/2) + (cy + (fh−1)/2)` — note
  the `(f−1)/2`, not `f/2`, per Center_Coord — merged into `unit_draw_order`'s
  `(fx+fy)` space; buildings before units on ties is a safe tiebreak (the
  original's ±32/−16 lepton tweaks don't apply to our 8 types).
- **§4 frame selection (Tasks 3/7d)**: `image_frames = shp_frames/2` (shadows =
  second half) confirmed. Healthy = frame 0; damaged = frame 1 when
  `hp/max_hp ≤ ConditionYellow` (0.50, ≤ not <); nothing changes at
  ConditionRed for the main image. Frame 2 unused this phase.
- **§5 buildup (Tasks 3/7c)**: `image_frames = buildup_shp_frames/2` confirmed.
  Frame period = `trunc(BuildupTime×900/image_frames)` ticks at 15 Hz — so
  `buildup_total` is `image_frames × max(1, trunc(0.06×900/image_frames)) / 15`
  seconds (GACNST ≈1.93 s, GAPOWR/GAPILE/GAWEAP ≈3.3 s typical), NOT the plan's
  pre-hunt `image_frames/15.0`; amend Task 7c. Plays once, then Idle. Placement
  sound = `[AudioVisual] BuildingSlam` → `PlaceBuilding` (pre-hunt name
  confirmed).
- **§6 palettes (Task 7)**: buildings = `unittem.pal` + house remap rows 16–31
  via the existing `pal_for_house`; cameos = `cameo.pal`. Both confirmed.
