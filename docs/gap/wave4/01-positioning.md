# Wave 4 / 01: object placement and draw positioning

Read-only audit of where the clone draws every kind of world object compared with the original.
Clone: master `4647b17`. Original: YR 1.001 `gamemd.exe`, with RA2 `game.exe` cross-checked for the building
function.

**User report:** "the Statue of Liberty is not positioned well". The animation agent also saw the White House
ruin about 40 px down-right.

## TL;DR

1. **Buildings and landmarks are anchored at the wrong point, every one of them.** The original centres the SHP
   canvas on the **top (north) vertex of the foundation's origin cell**. The clone centres it on the **foundation
   centre** (`docs/re/building-render.md` §2 "centre-centre registration", which is wrong).
   * The clone draws every building `((fw-fh)*15, (fw+fh)*7.5)` px away from the original, measured as
     clone minus original. Examples: 1x1 is +15 px down, 2x2 +30, 3x3 (the Statue) +45, 4x4 (the White House,
     Eiffel Tower, St Basil's, ConYard) +60, and 2x6 (the Gateway Arch) is 60 px left and 60 px down.
   * Bibs, ActiveAnims, doors, rubble, voxel turrets and civilian dressing all inherit the same error.
2. **FX anims ignore terrain height.** `PresGame::level_at` (`engine/pres_game.inc:553`) is a stub that returns 0.
   Every ruin, impact, building explosion and superweapon anim on raised ground is drawn `15 * level` px too
   low.
   * The White House ruin (level 4) gets both errors: it is drawn 120 px low, which puts it 60 px below the
     clone's own (already wrong) building.
3. **Non-ore overlays, ore, gems and crates are anchored by their canvas top-left.** The original centres them on
   the cell centre plus a per-type Y offset.
   * Low bridges are off by (+60, +45) px and high bridges by (+60, +91 to +106). Walls and sandbags are off by
     about (+4..10, +30), ore, gems and crates by (0, +27), and rocks by (+30, +45).
4. **Smudges are also anchored by their canvas top-left.** The original centres them on the cell's top vertex,
   so 120x120 scorch marks are off by (+30, +60).
5. **Correct today:** terrain objects (trees), infantry and vehicle anchors, and building depth-sort keys.
   * Minor: anim `YDrawOffset` is not applied (5 anims, 8 to 50 px), and the shadow of a `SpawnsTiberium`
     ore mine sits 16 px low.

I proved each formula in the engine itself. A scratch build that switches to the recovered anchors puts the
Statue on its star plinth, the White House inside its sandbag ring and the Arch on its pad. Bridges join their
roads and the Kremlin walls meet their towers (§3).

## Method and artifacts (all local, nothing committed)

* **Ghidra:** a private copy of `p4proj` at `D:\ra2-re\w4-pos`, with outputs in `D:\ra2-re\w4-pos-out\`.
  * Scripts are in `D:\ra2-re\w4-pos\scripts\`: `rtti.py` (vtable from RTTI), `dec.py`, `dis.py`, `refs.py`,
    `callscan.py` and `dispscan.py`.
  * RA2 `game.exe` was checked in `D:\ra2-re\w4-pos-ra2`, a copy of `w3fix-a`.
  * Vtables come from RTTI (`.?AVBuildingClass@@` and so on), not from YRpp addresses. YRpp's `AbsVTable`
    values and its `GetRenderCoords` slot name do not match this binary's layout.
* **Worktree:** `D:\ra2-wt\w4-pos` (branch `w4-pos`, build in `build-rel`). It holds scratch-only, uncommitted
  hooks:
  * `RA2_POS_DBG=1` outlines every building or dressing foundation in magenta. It adds a red `+` at the clone
    anchor and a green `x` at the original anchor.
  * `RA2_POS_FIX=1` draws with the recovered original anchors and gives FX anims the real cell level.
  * A `cheat view_key <prefix> [n]` verb pans to the n-th `terr:`/`ovl:`/`smudge:` object.
  * Play scripts are `D:\ra2-re\w4-pos-out\play\pos-*.play`, run by `runpos.ps1`. Each script runs in both modes,
    one run at a time.
* **Evidence** (local copy in `docs/gap/wave4/01-evidence/`):
  * `cmp/*.png`: in-game crops at 1.5x. The left half is the clone (master formula). The right half uses the
    recovered original formula. Both carry the debug outlines.
  * `ref/*.png`: reference composites rendered straight from the original SHPs and palettes with my own decoder.
    Each shows the art at the original anchor and at the clone anchor, over the foundation diamonds.

## 1. Recovered original formulas

Projection (`FUN_006d16b0`, Tactical CoordsToClient, and `FUN_006d1480`/`FUN_006d1420`):

```
px = (X - Y) * 30 / 256                         X, Y, Z in leptons (256 per cell)
py = (X + Y) * 15 / 256 - Zpx(Z)                Zpx = FUN_006d1650 -> FUN_007c4d00 (15 px per cell level)
```

Lepton `(cx*256, cy*256)` is the **top (north) vertex** of cell `(cx, cy)`, and the cell centre is
`(cx*256+128, cy*256+128)`. In the clone's frame (`cell_to_screen` = top-left of the tile's 60x30 rect), the
top vertex is `P + (30, 0)` and the centre is `P + (30, 15)`.

### Generic object render

* `FUN_005f4320` (ObjectClass) and `FUN_0043ce20` (BuildingClass override) do the same three steps:
  * call `coord = vtbl+0xAC(this)`;
  * call `FUN_006d16b0(coord) -> point`;
  * call `DrawIt` (`vtbl+0x114`) with `point`.
* Every SHP is then blitted by `CC_Draw_Shape FUN_004aec80` with **Centered (0x200)** in its flags. The canvas
  centre lands on `point`, and frame offsets are relative to the canvas.

| Class | vtbl+0xAC (draw coord) | DrawIt (+0x114) | Flags at the blit |
|---|---|---|---|
| BuildingClass `0x7E2EBC` | **`FUN_00459ee0`: `Location - (128, 128, 0)`** | `FUN_0043d210` -> `FUN_00705290` (TechnoClass::DrawObject) | `|0x600` |
| Infantry `0x7EA058` / Unit `0x7F4C70` / Aircraft `0x7E12A4` | `0x41BE30` -> GetCoords (`+0x48` = `FUN_005f5db0`, `Location`) | `FUN_00518db0` / `FUN_0073bdf0` / `FUN_004144b0`, `point.y -= (char)obj+0x6B4` (bob) | centred |
| TerrainClass `0x7F422C` | `0x41BE30` -> `Location` (cell centre, z) | `FUN_0071b450`: `y -= 16` when TerrainType `+0x2B1` (SpawnsTiberium). The image and the shadow both use the same point. | `0x4E00`/`0x2E00` |
| AnimClass `0x7E2354` | `0x41BE30` -> `FUN_00422c00` (`Location`, plus the owner's coord when attached) | `FUN_00422cc0`: `y += AnimType+0x344` (**YDrawOffset**, loader `FUN_00427c80` @`0x428299`) | `anim+0x190` (`0x600` default) `|0x2000` |

### BuildingClass details

* The `Location` of a building is the origin cell's **centre**. `Center_Coord` is `vtbl+0x48`
  (`FUN_00447a10`) = `Location + (fw*128 - 128, fh*128 - 128)`.
* So the draw coord `Location - 128` is the **origin cell's top vertex**, at the building's z:

```
building canvas centre = cell_to_screen(cx, cy, level) + (30, 0)        [clone frame]
```

* `vtbl+0xA4` (`FUN_00450090`) = centre + `TargetCoordOffset` (`type+0xEBC`). It is not used for drawing.
* The sort key `vtbl+0xB8` (`FUN_00449360`) stays centre-based, as in `building-render.md` §3, which is correct.
* **RA2 `game.exe` is identical:** `FUN_00453c70` (`vtbl+0xA8` there) = `Location - 128`, and `FUN_00444080` is
  the centre. The vtable is `0x79CBBC`.

`DrawIt FUN_0043d210` draws everything at that same `point` and adds no foundation-size term:

* the main SHP;
* `type+0x1518` (bib-style shape);
* the `+0x14E4`/`+0x14FC`/`+0x14EC`/`+0x1504` shapes.

The `(fw-1)*256, (fh-1)*256` projection at `0x43D6D5` (`FUN_006d1550`) feeds only the **Z-shape point**
(`ZShapePointMove + (0xC6, 0x1BE)`) for the Z-buffer. That projection is the source of the earlier
misreading in `building-render.md` §2.

Things that use the same draw point as the building:

* **Slot anims** (Active, Idle, Production, Super and so on): `PlayAnim FUN_00451880` creates the AnimClass at
  `vtbl+0xAC + FUN_006d18d0(<Slot>X, <Slot>Y)` (pixel -> lepton) with flags `0x1600`.
  * `<Slot>ZAdjust` goes to `anim+0x100` and `<Slot>YSort` to `anim+0x104`. Both are **depth only**.
  * The same re-anchoring runs when a building moves (`0x43F675`/`0x43F6B8`).
* **DestroyAnim** (`0x441C2C`-`0x441C9A`): `new AnimClass(DestroyAnim[rand], vtbl+0xAC, 0, 1, 0x600, 0, 0)`.
  The ruin is drawn at **the same point and z as the building**. Ruin SHPs share the building's canvas
  (`cgws01dm` and `cgwash01` are both 316x268).
* **Voxel turret** `TurretAnimX/Y`: the rules values are consistent with offsets from the draw point. For
  example, GTGCAN (2x2) has `TurretAnimY=28`, about the foundation centre, which is 30 px below the vertex.
  The blit itself was not traced.

### Overlays

The overlay pass is `FUN_006d6280`. For each cell it projects lepton `(x*256, y*256, 0)` and subtracts 30, which
gives the cell rect's top-left. `CellClass::DrawOverlay` (`FUN_0047f5b0`, shadow `FUN_0047f420`) adds
`FUN_00480020`:

```
canvas centre = rect top-left + (30, 15 - 15*level + yoff)            flags 0x4E00 (centred)
yoff (FUN_005fd4b0) = -12 if Wall | Tiberium | Crate | overlay #126 (0x7E)
                      -1  if LandType == 9 (Railroad);  -1 more for overlay #126
                      -16 if the cell's bridge flag (+0x140 & 0x80) is set,
                          and -15 more if OverlayData is 9..17 or the overlay is #239 (0xEF)
```

Tiberium/ore and gems go through the same path, and so do crates (`Crate=yes`).

### Smudges

`FUN_00480260` is called from `FUN_006d7490`. The tile is drawn at the rect top-left, and the smudge point is
`(rect.x + 30, rect.y - 15*level)`, which is the **cell's top vertex**. `SmudgeType::DrawIt` (`FUN_006b4c70`)
blits with flags `0xE00` (centred). It shifts the pieces of a multi-cell smudge by
`((row-col)*30, -(row+col)*15)`.

### What the original art confirms (`ref/*.png`, bounding boxes from my SHP decoder)

* **Buildings:** the art is authored for the top-vertex anchor.
  * `ggcnst` (4x4) art spans y -39..+108 from the canvas centre. The foundation runs 0..120 below the top vertex.
    Centred on the foundation centre (as the clone does), the art would hang 48 px below the foundation.
  * `ggpowr` (2x2) spans -41..+50 against a foundation of 0..60.
* **Ore** `tib01`: the art sits in canvas rows 31..55 of 60. With `-12` it fills the cell. With the clone's
  top-left anchor it lands in the cell below.
* **Smudge** `burnt03`: the art is +5..+21 below the canvas centre, which is inside the cell when centred on the
  top vertex.
* **Trees** `tree01`: the trunk base is at the canvas centre (cell centre).

## 2. Per object kind

The error is in px at 1:1 zoom, as **clone minus original** (+x right, +y down). `P` is `cell_to_screen`, the
tile rect's top-left in the clone.

| Kind | Clone formula (file:line) | Error (px) | Cause | Fix |
|---|---|---|---|---|
| Buildings: player, AI, neutral, landmarks, walls built as buildings | canvas centre = foundation bbox centre, `main.cpp:2755-2767` (`fcx`, `fcy`) | `((fw-fh)*15, (fw+fh)*7.5)`: 1x1 (0,+15), 2x2 (0,+30), 3x3 (0,+45), 4x4 (0,+60), 3x4 (-15,+52), 2x5 (-45,+52), 2x6 (-60,+60) | Wrong RE in `building-render.md` §2: the Z-shape projection was read as the draw offset | `fcx, fcy = P(cx,cy,level) + (30, 0)` |
| Building shadow, rubble (LeaveRubble), death explosion (no-fx path) | same `fcx`/`fcy` (`main.cpp:2776-2797`, `2855`) | same as the building | same | follows the building fix |
| Bib, ActiveAnim..Four, Idle/Production/Special/Super, doors, under-doors | `pres_building_overlay(..., fcx, fcy)` `main.cpp:2838/2876` -> `pres_bldanim.inc:289-353` (`fcx+x`) | same as the building | anchor | follows the building fix (`<Slot>X/Y` are already correct as pixel offsets) |
| DamageFireOffset fires | `pres_bldanim.inc:355-367` (`fcx + off`) | same as the building (probably) | anchor; the spawner was never traced (RE-P6) | follow the render point |
| Voxel turret (`TurretAnimIsVoxel`) | foundation centre, `pres_turret.inc:81,101-106` | same as the building | anchor | draw at the render point + `TurretAnimX/Y` |
| **DestroyAnim ruin** (`CAWS01DM`) | `spawn_hold(da, centre, level_at()=0)` `pres_game.inc:800,839` | building error + `15*level`. White House (L4): **(0,+120)**, which is (0,+60) away from the clone's own building | anchor, plus the `level_at` stub | `spawn_hold(da, b.cx-0.5, b.cy-0.5, real level)` |
| Civilian buildings drawn as dressing (skirmish) | `pres_map.inc:313-318` | `((fw-fh)*15, (fw+fh)*7.5)` | anchor | `P + (30, 0)` for `civ:` objects (dressing anims `pres_map.inc:365` follow) |
| Terrain objects (trees, lamps, rocks as terrain) | `pres_map.inc:313-318` with fw=fh=1 = cell centre | **0** | correct | none |
| Ore mines (`SpawnsTiberium`) shadow | `pres_map.inc:348` (shadow not lifted) | shadow (0,+16) | the original lifts both passes by 16 | lift the shadow too |
| Non-ore overlays: walls, fences, sandbags, rocks, bridges | canvas top-left = `P`, `pres_map.inc:320-321` | `(w/2-30, h/2-15-yoff)`: GAWALL 78x70 (+9,+32), NAWALL (+10,+29), GASAND/CAFNC* 68x66 (+4,+30), SROCK 120x120 (+30,+45), **LOBRDG 180x120 (+60,+45)**, **BRIDGE 180x180 (+60,+91), data 9..17 (+60,+106)** | top-left anchoring; yoff ignored | centre = `P + (30, 15 + yoff)` (yoff: §1) |
| Ore and gems | top-left = `P`, `main.cpp:3116-3119` | 60x60: **(0,+27)**, so the ore draws in the cell below | same | centre `P + (30, 3)` |
| Crates | top-left = `P`, `main.cpp:3251` | crate.tem 60x60: (0,+27) | same | centre `P + (30, 3)` |
| Smudges (map and new craters/scorches) | top-left = `P`, `pres_map.inc:320-321` (`centered=false`) | `(w/2-30, h/2)`: BURNT*/CR* 120x120 **(+30,+60)** | wrong anchor | centre = `P + (30, 0)` (top vertex); multi-cell pieces as in §1 |
| Infantry | canvas centre at the cell or path point, `iso_renderer.cpp:449-476` | anchor 0 | correct. Sub-cell spots are not modelled (one unit per cell, `[Infantry]` SubCell parsed but unused). The original's spot table was not found (searched for TS-style tables; no hit). | separate gap |
| Voxel vehicles | model origin at the `(fx, fy)` point, `iso_renderer.cpp:520-535` | 0 | correct | none |
| Aircraft altitude | `altitude/256*36.74` px (`flight.cpp:35`) | <=1 px at FlightLevel 1500 against 15 px per 104-lepton level (scale constant not re-traced) | RE-FALLBACK | optional |
| FX anims: impacts, explosions, SW, bolt, building explosions | `level_at` stub `pres_game.inc:553`; explicit `0` at `pres_game.inc:509-541, 676, 895` | (0, `+15*level`) on raised ground | stub | real cell level (unit-attached anims already use `u.level`) |
| Anims with `YDrawOffset` | not read (`pres_fx.inc` has no key) | (0, `-YDrawOffset`): FSGRND +20, H2O_EXP1 +18, H2O_EXP2 +10, H2O_EXP3 +8, BIGBLUE +50 | key missing | `cy += YDrawOffset` in `PresFx::draw`. YR has no `XDrawOffset`; that key comes from Ares/Phobos. |
| Tile anims (waterfalls) | cell centre + X/YOffset, `pres_map.inc:282` | not traced (W3 marked it Medium) | none | none |
| Height lift for buildings, dressing, overlays and units | `- level*15` from the origin or current cell | 0 for buildings/terrain/overlays. Units step by whole levels where the original uses a continuous z on ramps. | none | none |

Depth: `ZAdjust`, `NormalZAdjust`, `<Slot>ZAdjust`, `<Slot>YSort` and `YSortAdjust` affect depth only and
never move pixels. The clone's building sort key (foundation centre) is correct.

## 3. Landmarks and probes on real missions

The error is clone minus original (the red `+` against the green `x` in the shots). The verdict checks the
right-hand image against independent ground features.

| Landmark | Mission | Type / foundation / origin / level | Error (px) | Original-formula verdict | In-game clone vs original | Reference composite |
|---|---|---|---|---|---|---|
| **Statue of Liberty** | ALL01T | CANEWY04 (cgnewy04, lib.pal) 3x3 @61,69 L6 | **(0,+45)** | pedestal centred on the star plinth. In the clone the pedestal spills over the plinth's front edge. | `cmp/all01_statue.png`, `cmp/all01_statue_zoom.png` | `ref/statue_CANEWY04_3x3.png` |
| White House | ALL03U | CAWASH01 4x4 @77,13 L4 | (0,+60) | sits inside its sandbag ring on the paved lot | `cmp/all03_whitehouse.png`, `cmp/misc03_sandbag.png` | `ref/whitehouse_CAWASH01_4x4.png` |
| White House ruin | ALL03U (second WH spawned @70,22 and killed) | CAWS01DM 4x4 L4 | **(0,+120)**, i.e. (0,+60) away from the clone's building, as reported | ruin lies exactly on the rubble/foundation | `cmp/all03r_wh2.png`, `cmp/all03r_wh2_ruin.png`, `cmp/all03r_wh2_ruin30.png` | `ref/whitehouse_ruin_CAWS01DM_4x4.png` |
| Washington Monument | ALL03U | CAWSH12 3x3 @85,68 L4 | (0,+45) | on its lot | `cmp/all03_monument.png` | none |
| Jefferson Memorial | ALL03U | CAWASH14 3x3 @76,124 L5 | (0,+45) | on its lot | `cmp/all03_jefferson.png` | none |
| Lincoln Memorial | ALL03U | CAWASH15 3x4 @25,68 L4 | (-15,+52) | on its lot | `cmp/all03_lincoln.png` | none |
| Eiffel Tower | SOV05U | CAPARS01 4x4 @46,32 L6 | (0,+60) | legs on the plaza | `cmp/sov05_eiffel.png` | `ref/eiffel_CAPARS01_4x4.png` |
| Arc de Triomphe | SOV05U | CAPARS11 3x4 @61,32 L6 | (-15,+52) | on the road hub. In the clone it covers the road. | `cmp/sov05_arc.png` | `ref/arc_CAPARS11_3x4.png` |
| Gateway Arch | ALL08U | CASTL04 2x6 @104,78 L2 | **(-60,+60)** | the arch's pad matches the 2x6 foundation exactly | `cmp/all08_arch.png` | `ref/arch_CASTL04_2x6.png` |
| St Basil's | ALL12S | CARUS01 4x4 @95,94 L2 | (0,+60) | inside the Kremlin wall. The walls (overlays) meet the 1x1 towers (+15 fixed). | `cmp/all12_basils.png` | `ref/basils_CARUS01_4x4.png` |
| Kremlin | ALL12S | CARUS03 2x5 @125,122 L2 | (-45,+52) | on its lot | `cmp/all12_kremlin.png` | none |
| 4x4 player building | ALL03U | GACNST @82,131 L4 | (0,+60) | clear of the ore field it overlapped in the clone | `cmp/misc03_gacnst.png` | `ref/conyard_GACNST_4x4.png` |
| 2x2 player building | (composite) | GAPOWR | (0,+30) | art base inside the 2x2 diamond | none | `ref/powerplant_GAPOWR_2x2.png` |
| Multi-cell civilian buildings | ALL03U/SOV05U/ALL12S (neutral World buildings in campaign) | e.g. the 3x2/4x2 tenements beside the Eiffel Tower | `((fw-fh)*15, (fw+fh)*7.5)` | on their paved lots | `cmp/sov05_eiffel.png` (lower half), `cmp/sov05_arc.png` | none |
| High bridge | ALL08U | BRIDGE2 @83,8 L2, data 9 | (+60,+106) | one continuous deck onto its abutment. The clone shows broken floating chunks (also visible in ALL01, top right of `play/all01-*/statue.png`). | `cmp/misc08_bridge0.png` | none |
| Low bridge | ALL08U | LOBRDG21 @114,42 L2 | (+60,+45) | deck joins the road end. In the clone it floats in the river. | `cmp/misc08_lobrdg0.png` | none |
| Walls / sandbags | ALL03U, ALL12S | GASAND, CAFNC*, Kremlin walls | (+4,+30) | wall runs connect to the towers and trace the lot edges | `cmp/misc03_sandbag.png`, `cmp/all12_basils.png` | `ref/wall_GAWALL.png` |
| Ore | ALL03U | TIB 60x60 | (0,+27) | ore sits in its own cells | `cmp/misc03_gacnst.png` | `ref/ore_TIB01.png` |
| Smudges | ALL08U | BURNT03/BURNT12 120x120 | (+30,+60) | scorches inside their cells | `cmp/misc08_smudge0.png`, `cmp/misc08_smudge20.png` | `ref/smudge_BURNT03.png` |
| Trees | ALL08U | TREE02 | 0 | unchanged (control) | `cmp/misc08_tree0.png` | `ref/tree_TREE01_1x1.png` |

All landmark and high-ground probes above sit on raised cells (L2 to L6). The building and overlay height lift
is already correct; only the FX-anim lift is wrong (§2).

## 4. Prioritised fix list

1. **P0 (S): one building render point.**
   * Add a pure `building_draw_point(cx, cy, level) = cell_to_screen(cx, cy, level) + (30, 0)` and unit-test it
     for 1x1, 2x2, 3x3, 4x4, 3x4 and 2x6.
   * Use it in:
     * `draw_building`: body, shadow, rubble and death explosion;
     * `pres_building_overlay`: bib, slot anims, doors and damage fires;
     * `PresTurrets::draw` (+ `TurretAnimX/Y`);
     * PresMap `civ:` dressing and `pres_dressing_overlay`;
     * the DestroyAnim spawn (`b.cx-0.5, b.cy-0.5` in PresFx cell units).
   * Keep the depth key at the centre.
   * Rewrite `docs/re/building-render.md` §2 with the formula in §1. This fixes the Statue, every landmark and
     every base.
2. **P0 (S): real height for FX anims.**
   * Implement `level_at` (`pres_game.inc:553`) from `w.map()`.
   * Replace the literal `0` levels at `pres_game.inc:509-541, 676, 895`.
   * This fixes the ruin offset together with item 1, plus every impact and explosion on raised ground.
3. **P1 (S): overlay anchor.**
   * Centre on `P + (30, 15 + yoff)` with the `FUN_005fd4b0`/`FUN_00480020` yoff table, in:
     `pres_map.inc:320` (image and shadow), `draw_ore` (`main.cpp:3119`) and `draw_p5_crates`
     (`main.cpp:3251`).
   * For a high bridge, the bridge-owner cell flag needs the World bridge cell set (or `BRIDGE*` overlay
     membership as a stand-in).
   * This fixes bridges, walls, fences, sandbags, rocks, ore, gems and crates.
4. **P1 (S): smudge anchor.** Centre on the top vertex `P + (30, 0)`, with multi-cell piece offsets
   (`FUN_006b4c70`). Fixes map smudges and the runtime craters and scorches from `add_smudge`.
5. **P2 (XS):** apply anim `YDrawOffset` (AnimType `+0x344`) in `PresFx::draw`/`draw_anim_frame`.
6. **P2 (XS):** lift the `SpawnsTiberium` terrain shadow 16 px as well (`pres_map.inc:348`).
7. **P3:** separate RE items:
   * infantry sub-cell spots (the spot table was not located);
   * DamageFireOffset spawn origin (RE-P6);
   * tile-anim offsets (RE-Q6);
   * the aircraft Z-to-pixel constant.

Regression plan: `scripts/play/art-*.play` plus this audit's `pos-*.play` (the `RA2_POS_DBG` overlay makes the
anchor visible). No `world_hash` reads pixels, so gates stay byte-identical. Demo screenshots of buildings will
move by the deltas in §2.

## 5. Confidence

* **High:**
  * the building anchor (decompiled `FUN_00459ee0` in YR and `FUN_00453c70` in RA2, Render, DrawIt and the
    Centered blit);
  * DestroyAnim and slot anims at the same point;
  * overlay and smudge anchors (decompiled pass, plus art that fits only these anchors);
  * terrain at the cell centre;
  * `YDrawOffset` at `+0x344`.
* **Medium-high:** the bridge `-16`/`-31` (flag semantics inferred from `CellFlags` 0x80 and the observed deck
  continuity); the voxel turret anchor (inferred from the rules values).
* **Not traced:** infantry sub-cells, the damage-fire spawner, the tile-anim origin and the altitude scale.

## W4-FIX-POS status (branch `w4fix-pos`)

| Item | Status | Where |
|---|---|---|
| P0 building draw point | **Fixed.** `ra2engine::building_draw_point(cx, cy, level)` = origin-cell top vertex (`engine/iso_project.*`, unit-tested for 1x1..4x4, 3x4, 2x6). Used by `draw_building` (body, shadow, rubble, no-fx explosion), `pres_building_overlay` (bib, slot anims, doors, damage fires), `PresTurrets::draw` (voxel turret at the draw point + `TurretAnimX/Y`), PresMap `civ:` dressing (+ its slot anims), and the DestroyAnim spawn (`b.cx-0.5, b.cy-0.5`, the building's level). The depth key is unchanged (centre). The demo-only HP bar stays over the foundation. Placement cursor / selection brackets already used the cell diamonds (no change); the radar uses cell mapping (no change). | `main.cpp` `draw_building`; `pres_bldanim.inc`; `pres_turret.inc`; `pres_map.inc`; `pres_game.inc` |
| P0 FX height | **Fixed.** `PresGame::level_at` reads the cell level; the literal `0` levels (superweapon anims, building impacts/explosions, defence muzzles and beam origins, `target_point`) use the real level. Projectile `e.z` is ground-relative (`unit_lz` = 0 for ground units), so impacts add the level without double counting. | `pres_game.inc` |
| P1 overlays / ore / gems / crates | **Fixed.** Cell centre + `overlay_y_offset` (`FUN_005fd4b0`/`FUN_00480020`); high-bridge cells stood in by the `BRIDGE*` overlay family (BRIDGE1/2, BRIDGEB1/B2). | `pres_map.inc`, `draw_ore`, `draw_p5_crates` |
| P1 smudges | **Fixed.** Centred on the cell's top vertex (map smudges and runtime craters/scorches); a multi-cell smudge draws once at its origin, which equals the original's per-piece shift. | `pres_map.inc` |
| P2 `YDrawOffset` | **Fixed.** `FxAnimType::ydraw` (art key) added in `PresFx::draw`, building slot anims, damage fires and dressing anims. | `pres_fx.inc`, `pres_bldanim.inc` |
| P2 ore-mine shadow | **Fixed.** The shadow shares the image's 16 px lift. | `pres_map.inc` |
| P3 damage fires (RE-P6) | **Traced.** `FUN_0043c050` (from the damage-state update `FUN_0043faa0`): one anim per `DamageFireOffset` (type `+0x15D8`, up to 8) at the draw point (`vtbl+0xAC`) + the pixel offset, flags `0x600`; the type list starts at a random index (clone: from 0, RE-FALLBACK). The clone already drew at draw point + offset after P0. | `pres_bldanim.inc` |
| P3 infantry sub-cell spots | **Open.** Spot table not located; it is a sim/movement change, not a draw anchor. | — |
| P3 tile-anim origin (RE-Q6) | **Open.** The keys are read by the theater loader `FUN_00544e80` (`Tile%02dXOffset`/`YOffset`/`AttachesTo`); the consumer was not traced. W3's waterfall fit is unchanged. | — |
| P3 aircraft Z scale | **Open** (<= 1 px). | — |

Acceptance (real play, `scripts/play/pos-*.play`, all PASS; screenshots read against `01-evidence/ref`, local copies in
`01-evidence/fix/`): the Statue on its star plinth (`all01_statue`), the ALL01 and ALL08 high bridges one deck
(`all01_bridge`, `all08_bridge`), the ALL08 low bridge joining its road (`all08_lobrdg`), the Arch on its pad
(`all08_arch`), the White House inside its sandbag ring (`all03_whitehouse`) and its ruin over the foundation
(`all03_wh2_dbg` / `all03_wh2_ruin`, matches `ref/whitehouse_ruin_CAWS01DM_4x4.png`), the Lincoln Memorial on its lot,
St Basil's inside the Kremlin walls with the walls meeting the towers (`all12_basils`), the Eiffel Tower and the
Arc de Triomphe (`eiffel`, `arc`), and player 2x2/3x3/4x4 buildings with bib, ActiveAnim and a voxel turret inside
their foundations (`all03_bases_dbg`, `RA2_POS_DBG=1`).
