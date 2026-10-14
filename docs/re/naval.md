# Naval movement + water land-type semantics — reverse-engineering notes

(Method: `docs/re/workflow.md`; image base `0x00400000`, YR 1.001 `gamemd.exe`,
SHA1 `7BE4E5508655F8AF482AB3FF5965D196E1B6B6C7`. Hunt scripts
(`naval_hunt1.py … naval_hunt12.py`), extracted `rulesmd.ini`/`artmd.ini`/
`temperat.ini`, decompile dumps, and the throwaway `--naval-water` map probe are
all off-repo in the session scratchpad; nothing off-repo is committed.
Content-anchored only: every function below was reached from string/data/xref/
vtable/GUID evidence in THIS binary, never from published addresses — the
Ares/YRpp drift documented in `combat.md` §A and `workflow.md` §4 held here too.
Prior anchors used as the entry map: `voxel-render.md` §located-functions —
the INI loader `FUN_00711510`, the DriveLoco Draw_Matrix `FUN_004AFE70`
(ILocomotion vtable `0x7E6EB0`), the FacingClass family `FUN_004C9140…`; the
locomotor-factory technique from `aircraft.md` (`FUN_006BB020`); and
`combat.md` for the RNG (`FUN_0065C220`) and damage math.)

This document is the calibration authority for the **CB-3 Naval & Water-Movement**
phase. The exemplar is the **Sea Scorpion `[HYD]`** (naval `ShipLocomotionClass`),
with `[DEST]`/`[SUB]`/`[DRED]` as siblings. The rules VALUES below were read on
this install and are ground truth already; this hunt pins the FORMULAS / offsets
/ semantics that turn them into engine constants, plus the map-water availability
check. Confidence per finding; "Deviations we take" at the end resolves the
`[RE-GATE: naval.md §N]` markers in the CB-3 plan Tasks 2–7.

## TL;DR — the deltas from the plan's pre-hunt assumptions (what the RE-GATE needs)

1. **LandType SOURCE is the theater tile-set classification, NOT the `.tmp`
   TerrainType byte.** [RE-GATE — §2, option (b)]. The `.tmp` image-header byte at
   offset 41 does NOT hold the LandType enum on these files (a direct decode of
   the reference map's tiles via that byte produced values 9–15, not the 0–11
   LandType enum, and zero Water cells). The real source is the theater control
   INI (`temperat.ini`) `[General]` tile-set indices — `WaterSet=21`,
   `ShorePieces=12`, `CliffSet=10`, `WaterCliffs=15`, `RoughTile=13`, … — which
   the `IsoTileTypeClass` load stamps into a per-tile land-type field
   (`IsoTileType+0x298`), later copied to `CellClass::LandType` (`CellClass+0xEC`).
   Classifying the reference map's cells by tile-set membership yields a coherent
   water body (468 Water cells; §2 / map-water result). **Task 2's
   `land_type_of_cell` / `resolve_land_types` must walk the theater `[General]`
   tile-set classification, not read a `.tmp` byte.** (The `land_type_from_terrain_byte`
   / `TmpFile::terrain_type=41` fallback is FALSIFIED for LandType and should be
   dropped.)

2. **Passability is a BINARY `== 0.0` test, not a speed-weighted cost.** [RE-GATE
   — §2]. The passability gate (`FUN_004833B0`) does exactly
   `Ground[LandType][SpeedType] == 0.0 ? blocked : passable`. The percentage is
   NOT folded into A\* step cost at this gate; it is the flat-terrain movement-speed
   multiplier applied downstream in the locomotor. CB-3's binary
   `build_water_passability` (Water⇒1, everything-else⇒0 for a `Float` ship) is
   faithful; speed-weighting is a documented carry-forward, not the original's
   passability rule.

3. **The naval locomotor is a genuine standalone `ShipLocomotionClass`, not the
   Drive loco gated by a flag.** [RE-GATE — §1]. CLSID
   `{2BEA74E1-7CCA-11d3-BE14-00104B62A16C}` (data `0x7E8AB0`) has its own COM
   factory, its own object (`operator new(0x70)`, ctor `FUN_0069E2E0`), its own
   ILocomotion vtable `0x7F1D8C`, its own `Process` (`FUN_0069F2A0`) and
   `Draw_Matrix` (`FUN_0069ED00`) in a distinct `0x69Exxx–0x6A3xxx` class — all
   different from Drive (`0x4AFExxx`/`0x4B04xx`). BUT it is *Drive-shaped* and
   shares the `FootClass` A\* path + cell machinery: the water-only restriction
   comes entirely from `SpeedType=Float` × the land-type speed table, not from the
   locomotor. So Task 4's grid-selected A\* (pathed cell-to-cell, one `occ_` cell)
   is correct; the naval grid is just "Land grid filtered by Float passability".

4. **SpeedType/LandType enum orders are pinned from the binary** (§2). LandType:
   `0 Clear, 1 Road, 2 Water, 3 Rock, 4 Wall, 5 Tiberium, 6 Beach, 7 Rough,
   8 Ice, 9 Railroad, 10 Tunnel, 11 Weeds`. SpeedType:
   `0 Foot, 1 Track, 2 Wheel, 3 Hover, 4 (aircraft/always-passable), 5 Float,
   6 Amphibious, 7 FloatBeach`. A `Float` ship indexes column 5; from the rules
   land-type table that is passable (1.0) ONLY on `[Water]` — `[Beach]` Float=0%
   is blocked, confirming the plan's "Beach is 0% for plain Float".

5. **Naval targeting is opt-in per TechnoType, and the exemplar `[HYD]` uses the
   defaults** (§4). `NavalTargeting`/`LandTargeting`/`CanPassiveAquire` are
   TechnoType INI keys parsed by `FUN_00711510`. `[HYD]` sets none of them (it
   auto-acquires by the normal `Verses`>0 rule, `combat.md` §1–2), so it can fire
   its `FlakTrackGun` at both land and naval targets; `[SUB]` restricts
   (`NavalTargeting=5 LandTargeting=1`), `[DRED]` disables passive acquire
   (`CanPassiveAquire=no`). A naval-vs-naval duel keeps every approach cell on
   water regardless of the key.

6. **The naval MOVEMENT path has no RNG, but the original `FlakTrackGun`
   projectile DOES scatter** (§6). No `Randomizer` call (`FUN_0065C220`) appears
   in the ship `Process`/move helpers → movement is deterministic. However
   `[FlakTProj] FlakScatter=yes Inaccurate=yes Arcing=true` means the vanilla shell
   scatters (RNG via `TechnoClass::Fire`, `combat.md` §5). CB-3's instant-single-
   target-hit simplification (the `voxel-render.md` §7 / `aircraft.md` §4
   precedent) REMOVES that RNG, so the reimplemented naval critical path stays
   deterministic — recorded as a deviation, not a bug.

## Recovered exemplar ground truth (read from this install)

```
[HYD]  Sea Scorpion  Naval=yes  MovementRestrictedTo=Water  SpeedType=Float
       MovementZone=Water  Speed=8  ROT=6  Strength=400  Armor=heavy  Size=20
       Primary=FlakTrackGun  Secondary=FlakWeapon  ElitePrimary=FlakTrackGunE
       Locomotor={2BEA74E1-7CCA-11d3-BE14-00104B62A16C};{4A582741-9839-11d1-B709-00A024DDAFD1}
artmd [HYD]  Voxel=yes  Remapable=yes  PrimaryFireFLH=65,0,180  SecondaryFireFLH=65,0,180

[DEST] Speed=6 ROT=5 SpeedType=Float MovementZone=Water NavalTargeting=1 Turret=no
       Locomotor={2BEA74E1-7CCA-11d3-BE14-00104B62A16C}   (single CLSID — no submerge loco)
[SUB]  Speed=4 ROT=2 SpeedType=Float MovementZone=Water Underwater=yes Cloakable=yes
       NavalTargeting=5 LandTargeting=1 Locomotor={2BEA74E1-…};{4A582741-…}
[DRED] Speed=4 ROT=1 SpeedType=Float MovementZone=Water CanPassiveAquire=no GuardRange=10
       Locomotor={2BEA74E1-…};{4A582741-…}

[FlakTrackGun] Damage=25 ROF=25 Range=5 Projectile=FlakTProj Speed=50 Warhead=FlakTWH Anim=GUNFIRE
[FlakTProj]    Arcing=true AA=no AG=yes Inaccurate=yes FlakScatter=yes SubjectToCliffs=no
               SubjectToElevation=yes SubjectToWalls=yes
[FlakTWH]      CellSpread=1.0 PercentAtMax=1.0 Conventional=yes
               Verses=150%,125%,100%,60%,10%,10%,30%,20%,10%,100%,100%

; land-type × speed-type table ([GroundClass], as loaded by FUN_00673A40):
[Clear] Foot=100 Track=100 Wheel=100 Hover=50  Float=0   Amphibious=80 FloatBeach=0
[Water] Foot=0   Track=0   Wheel=0   Hover=100 Float=100 Amphibious=100 FloatBeach=100
[Beach] Foot=0   Track=0   Wheel=0   Hover=75  Float=0   Amphibious=60 FloatBeach=100
[Road]  Foot=100 Track=100 Wheel=100 Hover=75  Float=0   Amphibious=100 FloatBeach=0
[Rough] Foot=100 Track=100 Wheel=100 Hover=50  Float=0   Amphibious=80 FloatBeach=0
[Rock]  all=0                                             (Wall/Rock impassable to all)
[Ice]   Foot=50  Track=80  Wheel=50  Hover=100 Float=0   Amphibious=50 FloatBeach=0
[Tiberium] Foot=90 Track=70 Wheel=50 Hover=50  Float=0   Amphibious=50 FloatBeach=0
```

For a **`Float` ship (SpeedType column 5)** the ONLY land type with a non-zero
entry is `[Water]` (100%) — every other land type is 0% ⇒ blocked, including
`[Beach]`. Float ships are therefore pure open-water units.

## Located functions

| Function | Role | How found |
|----------|------|-----------|
| `FUN_006BB020` @ `0x6BB020` | locomotor **factory registration** — registers each CLSID into class map `0xB0ABF8`; naval `{2BEA74E1}`=`0x7E8AB0` pushed at `0x6BCA3D`, sub-2 `{4A582741}`=`0x7E8A30` at `0x6BC7DB` | GUID data-xref; disasm |
| `FUN_006C44C0` | naval-loco **factory ctor** (installs factory vtable `0x7F2D68`; `[EAX]=0x7f2d68`) | EAX producer for the `0x7E8AB0` registration |
| `FUN_006C4590` | factory **CreateInstance** (slot 3 of `0x7F2D68`) — `operator new(0x70)` → `FUN_0069E2E0` | factory vtable slot read + decompile |
| `FUN_0069E2E0` | **ShipLocomotionClass ctor** — installs vtables `0x7F1E58` (this+0), **ILocomotion `0x7F1D8C`** (this+4), `0x7F1D68` (this+0x18) | CreateInstance callee; vtable stores |
| `0x7F1D8C` | **ShipLocomotionClass ILocomotion vtable** (the one with a valid fat `Process`) | slot16(+0x40) valid code; `0x7F1E58` slot16 was data |
| `FUN_0069F2A0` | **ShipLoco `Process`** (ILoco slot 16, +0x40; 7 KB fat per-frame tick) — state machine: FacingClass turn gate (`FUN_004C93A0`/`FUN_004C92F0`), cell step via `FUN_006A1310`, water-wake at `cell+0xEC==2` | vtable slot 16; content |
| `FUN_0069ED00` | **ShipLoco `Draw_Matrix`** (slot 9, +0x24) — Matrix3D facing rotation + ramp/tilt (`FUN_005AE990/AB0` rotate, `FUN_005AE040` push) | vtable slot 9; content |
| `FUN_006A1310` / `FUN_0069FC80` | ShipLoco cell-step / move helpers — heavy `MapClass::CellAt` (`FUN_005653D0`) + FacingClass; no A\*/RNG of their own (path is FootClass's) | Process callees |
| `FUN_00740780` | UnitClass code that special-cases the sub via `GetClassID == {4A582741}` (`0x7E8A30`) — the deferred submerge/cloak loco | GUID xref |
| `FUN_00673A40` | **[GroundClass] land-type speed-table loader** — iterates the LandType name table `PTR_s_Clear_00838D60`, reads Foot/Track/Wheel/Hover/Float/Amphibious/FloatBeach/Buildable into `Ground[]` @ base `0x89D9B0`, stride 9 dwords (`0x24`) | `"Float"`/`"FloatBeach"` string xrefs |
| `FUN_004833B0` | **passability gate** (CellClass, `Can_Enter`-shaped) — `if (Ground[cell.LandType(+0xEC)][SpeedType] == 0.0 && !bridge) return blocked` (binary); SpeedType 4 bypasses all terrain checks | Ground-array (`0x89D9B0`) xref |
| `FUN_00483B90` | CellClass movement-zone / land classification (writes cell `+0x4c`), also indexes `Ground[IsoTileType+0x298][2]` | Ground-array xref |
| `FUN_0047D1C0` | **CellClass::Recalc_Attributes** — `CellClass.LandType(+0xEC) = IsoTileType(DAT_00a82cec[cell.Tile+0x44])+0x298`; overrides: wall/overlay→Rock(3), tiberium→5, bridge→special | `+0xEC` writer + `DAT_00a82cec` xref |
| `FUN_00711510` | **TechnoTypeClass::LoadFromINI** — parses `Naval`(`0x713E0A`), `Underwater`(`0x714114`), `NavalTargeting`(`0x71155E`), `LandTargeting`(`0x711544`), `CanPassiveAquire`(`0x713813`), `Locomotor`(`0x71179A`) | INI-key string xrefs |
| `FUN_00746450` | **UnitTypeClass::LoadFromINI** — `SpeedType`→`+0x67c` (enum, `FUN_00476EF0`, default `2-flag`), `MovementRestrictedTo`→`+0xdfc` (`FUN_004753E0`, default -1) | `"SpeedType"`/`"MovementRestrictedTo"` xrefs |
| `0x00838D60` | **LandType name table** — {Clear,Road,Water,Rock,Wall,Tiberium,Beach,Rough,Ice,Railroad,Tunnel,Weeds} = enum 0..11 | pointer-table read |
| `0x89D9B0` | **`Ground[]`** land-type speed table, `float[NLAND][9]`; col order Foot,Track,Wheel,Hover,(const 1.0),Float,Amphibious,FloatBeach,+Buildable byte | FUN_00673A40 store layout |

## §1 Naval locomotor & movement shape

**It is a standalone `ShipLocomotionClass` (the distinct CLSID), not the Drive
loco gated by a flag — but it reuses the ground A\* path + cell machinery.**
CLSID `{2BEA74E1-7CCA-11d3-BE14-00104B62A16C}` lives at `DAT_007E8AB0`
(`E1 74 EA 2B CA 7C D3 11 BE 14 00 10 4B 62 A1 6C`). The game-init factory
`FUN_006BB020` registers it (disasm `0x6BCA3D`: `PUSH 0x7e8ab0; CALL ESI`) with
its own factory object built by `FUN_006C44C0` (installs factory vtable
`0x7F2D68`). The factory `CreateInstance` (`FUN_006C4590`, slot 3) does
`operator new(0x70)` then the loco ctor `FUN_0069E2E0`, which installs ILocomotion
vtable **`0x7F1D8C`** (plus `0x7F1E58` at this+0 and `0x7F1D68` at this+0x18).
This is an entirely distinct class from Drive (Drive Draw_Matrix `FUN_004AFE70`
@ `0x4AF…`, ILoco vtable `0x7E6EB0`; ship's are `FUN_0069ED00` @ `0x69E…`), so
"standalone class" is **CONFIRMED**.

**`Process = FUN_0069F2A0`** (ILoco vtable `0x7F1D8C` slot 16, `+0x40`; 7 KB, the
fat tick — same slot/shape as Drive's `FUN_004B0410` and JJ's `FUN_00514130`).
Its shape:

- **Pathed, cell-to-cell (not straight-line).** Process fetches the object's
  head-to/destination and steps toward the next path cell, resolving coords→cells
  through `MapClass::CellAt` (`FUN_005653D0`, called dozens of times) in the move
  helper `FUN_006A1310`. There is no `atan2`+`cos/sin` free-flight translate (the
  Jumpjet signature, `aircraft.md` §1) — movement is grid-anchored. The A\*
  itself is `FootClass::Find_Path` (shared with ground units); the locomotor only
  *follows* the computed path. CONFIRMED cell-to-cell; the path A\* being the same
  FootClass routine is PROBABLE (shared machinery, not re-isolated here).
- **Turn model = ordinary timed shortest-arc `FacingClass`** (`FUN_004C93A0`
  Is_Rotating, `FUN_004C92F0` Current — `voxel-render.md` §5), armed from `ROT`
  (`type+0x71C`). Process gates on rotation state exactly like Drive (turn the
  hull toward the track heading before rolling); `Draw_Matrix` (`FUN_0069ED00`)
  applies the quantized facing rotation + a ramp/tilt matrix.
- **One reserved cell, shared `occ_` semantics.** Naval units are `UnitClass`/
  `FootClass`; nothing in Process marks occupancy differently from a ground unit
  — the water-only behavior is purely the `SpeedType=Float` passability filter,
  not a naval-specific cell rule. One-cell reservation like ground is PROBABLE
  (shared FootClass `Mark_Occupation`; no naval-specific occ instruction found,
  and none expected).
- **Water restriction is external to the loco.** The loco never special-cases
  water; it just moves the unit through cells the FootClass path produced, and
  the path/passability were filtered by `Ground[LandType][Float]` (§2). The only
  water-aware line in Process is cosmetic: `cell.LandType(+0xEC) == 2` (Water)
  spawns the wake/foam effect (`operator new(0x1c8)` + `FUN_00421EC0`).

**Verdict:** standalone `ShipLocomotionClass`, pathed cell-to-cell A\* via the
shared FootClass grid, timed ROT turn, one occupancy cell — **CONFIRMED** for the
class/vtable/Process and cell-based movement; **PROBABLE** for "same FootClass A\*"
and "one occ cell" (shared machinery, not byte-isolated). Which loco `[HYD]` uses:
the CLSID above — **CONFIRMED** from rules + the factory/vtable chain.

## §2 LandType → passability model (THE foundation)

**Cell LandType source — the tile-set classification, stamped into a per-tile
field. [RE-FALLBACK on the plan's `.tmp`-byte assumption → resolved to tile-set].**
The runtime chain is CONFIRMED from three functions:

- `FUN_0047D1C0` (CellClass::Recalc_Attributes): reads the cell's global
  tile index (int at `cell+0x44`), uses it to index the pointer array at
  global `0xA82CEC` to get that tile's IsoTileTypeClass, and copies the
  IsoTileType's LandType (int at `iso+0x298`) into the cell's LandType field
  (`cell+0xEC`). It then applies overrides: wall/overlay → 3 (Rock),
  tiberium → 5, bridge → special.
- `FUN_004833B0` (passability gate): reads the cell's LandType (`cell+0xEC`,
  a 0..11 enum), then looks up the float at index `SpeedType + land*9` of the
  ground speed-multiplier table at `0x89D9B0`. If that value is 0.0 and the
  cell is not a bridge, the move is BLOCKED.

`IsoTileType+0x298` (the per-tile land type) is populated at theater load. It is
**NOT** the `.tmp` image-header TerrainType byte: decoding the reference map's
tiles via that byte (offset 41 in the 52-byte header) produced values 9–15 (not
the 0–11 enum) and **zero** Water cells. The real classifier is the theater
control INI `[General]` tile-set map in `temperat.ini`:

```
[General] WaterSet=21  ShorePieces=12  CliffSet=10  WaterCliffs=15
          RoughTile=13 ClearTile=0  SandTile=33  GreenTile=41  IceShoreSet=32 …
```

i.e. every template in tile-set 21 is classified Water(2), set 12 Beach(6), etc.
Classifying the reference map's cells by tile-set membership yields a coherent
water body (§ map-water result), whereas the byte read does not — so the LandType
source is the **[TileSetNNNN]/[General] classification (option b)**. Confidence:
the runtime chain (`CellClass+0xEC ← IsoTileType+0x298 ← tile-set`) is
**CONFIRMED**; that the classifier is the `[General]` set indices (not the `.tmp`
byte) is **CONFIRMED by behavior** (map decode is coherent only this way + the
`.tmp` byte is falsified); the single instruction assigning `+0x298` from a set
index was not isolated (the IsoTileType heap ctors init flags but the land-type
write wasn't pinned to one line) — documented gap, does not change the source.

**LandType enum** (name table `0x00838D60`): `0 Clear, 1 Road, 2 Water, 3 Rock,
4 Wall, 5 Tiberium, 6 Beach, 7 Rough, 8 Ice, 9 Railroad, 10 Tunnel, 11 Weeds`.

**SpeedType enum** (`Ground[]` column order in `FUN_00673A40`): `0 Foot, 1 Track,
2 Wheel, 3 Hover, 4 (const 1.0 — the aircraft/"always-passable" column;
`FUN_004833B0` short-circuits `if (SpeedType != 4)`), 5 Float, 6 Amphibious,
7 FloatBeach`. `[HYD]`/`[DEST]`/`[SUB]`/`[DRED]` all use `SpeedType=Float` = **column 5**.

**Passability = BINARY, `== 0.0`.** `FUN_004833B0` tests `Ground[land][speed] == 0.0`
exactly (a strict float equality against behavioural `0.0`), returning blocked;
any `> 0` is passable. The percentage is **not** an A\* step-cost weight at the
gate — it is the flat-ground movement-speed multiplier applied later in the
locomotor. So A\* passability is binary. **CONFIRMED.** For a `Float` ship this
means: passable ONLY on `[Water]` (Float=100%); `[Clear]`/`[Beach]`/`[Rough]`/
`[Road]`/`[Rock]`/`[Ice]`/`[Tiberium]` are all Float=0% ⇒ blocked — the water/land
boundary, and "Beach is 0% for plain Float", both **CONFIRMED** from the rules
table.

**`MovementZone` vs SpeedType.** `SpeedType` (`type+0x67c`) indexes the speed
table above (the per-cell gate). `MovementZone` (`type`, parsed in `FUN_00711510`)
is the coarse zone-connectivity class used by the high-level path/zone reachability
(all naval = `MovementZone=Water`); it partitions the map into connected regions
so the AI/pathfinder can early-out. For CB-3 the SpeedType binary gate is the
load-bearing one; MovementZone maps 1:1 to "the Float-passable subgraph".

## §3 Water occupancy & pathing rules

- **One ship per cell — shared `occ_` semantics (PROBABLE).** Naval units are
  ordinary `UnitClass`/`FootClass`; the ship locomotor reserves/releases the
  destination cell through the same FootClass path/occupancy machinery ground
  units use (no naval-specific spacing or multi-cell footprint was found in the
  loco). `Size=` (HYD 20/DEST 30) is a threat/UI weight, not a cell footprint.
- **Land and water cell sets are disjoint (CONFIRMED via the gate).** A `Float`
  ship passes only Water(2) cells; a ground `Track`/`Wheel` unit has Water=0% ⇒
  blocked and Clear=100% ⇒ passable — so the Float-passable set (Water) and the
  Track/Wheel-passable set (Clear/Road/…) never overlap. The two grids are
  naturally disjoint from the single `Ground[LandType][SpeedType]` table; no
  separate data structures.
- **Repath.** A blocked ship re-plans through the same FootClass repath the
  ground grid uses (the `passable_minus_units` analogue) — the locomotor requests
  a new path from FootClass when the next cell is occupied. Not separately
  isolated for water (shared code); PROBABLE. For CB-3, build the water
  passability grid once (binary, Water⇒1) and run the existing A\* + repath on it.

## §4 Naval combat & targeting

- **Weapon:** `[HYD] Primary=FlakTrackGun` — `Damage=25 ROF=25 Range=5
  Warhead=FlakTWH Projectile=FlakTProj Speed=50`; `artmd PrimaryFireFLH=65,0,180`.
  Parsed by the weapon/warhead loaders (`voxel-render.md` §5/§7): `Range=5` cells
  → 1280 leptons; `ROF=25` → 25-frame cooldown (`combat.md` §5, `RearmTimer :=
  GetROF()`, no jitter at the reset); `Damage=25 × FlakTWH.Verses[armor]`
  truncated toward zero (`combat.md` §3); `PrimaryFireFLH=65,0,180` is the muzzle
  offset in turret space (F=65,L=0,H=180 leptons) via `GetFLH` (`FUN_006F3030`,
  `voxel-render.md` §6). `FlakTWH`: `CellSpread=1.0` (~256-lepton splash, one-ring),
  `PercentAtMax=1.0` (no distance falloff), `Verses=150,125,100,60,10,10,30,20,10,100,100`.
- **Target acquisition from water (CONFIRMED keys, DEFAULT behavior for HYD).**
  `NavalTargeting`/`LandTargeting`/`CanPassiveAquire` are TechnoType keys parsed by
  `FUN_00711510` (string xrefs `0x71155E`/`0x711544`/`0x713813`). `[HYD]` sets
  **none** of them → it uses the ordinary auto-acquire (fire at any target whose
  `Verses > 0`, `combat.md` §1–2; `FlakTWH` has non-zero verses vs every armor),
  so a Sea Scorpion can auto-acquire BOTH land/shore and naval targets. Siblings
  restrict: `[SUB] NavalTargeting=5 LandTargeting=1`, `[DEST] NavalTargeting=1`,
  `[DRED] CanPassiveAquire=no`. Range/ROF/target-choice all reuse `combat.md`'s
  machinery; there is no naval-specific range metric (3-D Euclidean, `combat.md`
  §6). For the CB-3 demo a naval-vs-naval duel keeps all approach cells on water
  regardless of these keys.
- **Delivery:** `[FlakTProj]` is `Arcing=true AA=no AG=yes` — an anti-surface
  arcing shell. CB-3 models it as an instant single-target-cell hit (the
  established projectile simplification), which also drops the `FlakScatter`/
  `Inaccurate` RNG (§6). Sub depth-charge/torpedo delivery is OUT of this phase.

## §5 Exemplar-stat formulas (values → engine constants)

| Rules value | Formula (recovered) | Engine constant |
|-------------|---------------------|-----------------|
| `[HYD] Speed=8` | `min(255, clamp(v,0,100)·256/100)` (`voxel-render.md` §5) | **20 leptons/frame** (`8·256/100=20.48→20`; ≈1.17 cells/s @15 Hz) |
| `[HYD] ROT=6` | FacingClass rate `= min(ROT,127)<<8` u16/frame (`voxel-render.md` §5) | `1536` u16/frame → full turn ≈ 42.7 frames |
| `[DEST] Speed=6 / [SUB] Speed=4 / [DRED] Speed=4` | same | 15 / 10 / 10 leptons/frame |
| `Range=5` | cells→leptons (×256) at weapon parse | 1280 leptons |
| `ROF=25` | frames (cooldown reset, no jitter, `combat.md` §5) | 25-frame cooldown |
| `Damage=25`, `FlakTWH Verses[armor]` | `trunc(Damage·Verses)` (`combat.md` §3) | e.g. vs heavy(=`[HYD]` armor) 60% → 15 |
| `Strength=400` | hp (direct) | 400 |
| `PrimaryFireFLH=65,0,180` | F/L/H leptons, turret space (`voxel-render.md` §6) | (65,0,180) |

Naval units reuse the **identical** `speed`/`rot`/`damage`/`rof`/`range`/`verses`
conversions ground vehicles use — Task 6's `resolve_combat_unit` needs no naval
special-casing.

## §6 Determinism + byte-identity

- **No RNG on the naval MOVEMENT path — CONFIRMED.** The ship `Process`
  (`FUN_0069F2A0`) and its move helpers (`FUN_006A1310`, `FUN_0069FC80`) contain
  no call to the R250 `Randomizer` (`FUN_0065C220`/`FUN_0065C1C0`, `combat.md` §5).
  Movement is deterministic frame arithmetic (FacingClass timers + cell steps).
- **The vanilla `FlakTrackGun` fire path DOES use RNG** — `[FlakTProj]
  FlakScatter=yes Inaccurate=yes` → ballistic scatter draw in `TechnoClass::Fire`
  (`combat.md` §5). CB-3's instant-single-target-hit simplification removes this,
  so the reimplemented naval combat path is deterministic. **[DEVIATION]** — the
  original scatters; we don't.
- **Step integration / ordering.** Naval units integrate through the same per-unit
  AI/`Locomotor->Process()` pass as ground units (no separate naval global pass in
  the original). A `step_units`/`step_combat` pass that iterates the unit vector in
  a fixed order, selecting the water grid for `SpeedType=Float` units, reproduces
  this deterministically WITHOUT changing ground-unit pathing (ground units keep
  the Land grid). PROBABLE (the exact call-site order vs `step_combat` wasn't
  isolated; with no RNG on the path, iteration order is the only determinism lever
  and it is under our control). AI does not field ships (out of scope).

## Map-water availability (Step 6 — CRITICAL)

**RESULT: USABLE water region on the reference map — NOT a [RE-FALLBACK].**

The temperate reference map (`mapsmd03.mix` entry `0x133a0cb2`, loaded at
`engine/main.cpp:138`, `[Map] Size=103x110`, 22,550 materialized cells) was
decoded with a throwaway scratch probe (`ra2extract --naval-water`, reverted —
not committed) that resolves each cell's LandType the way the game does: cell tile
index → theater `[TileSetNNNN]` slot → tile-set index → `[General]`
classification. Counts:

| LandType | classifier | cells |
|----------|-----------|------:|
| Water (2) | tile-set `WaterSet=21` | **468** |
| Beach (6) | tile-set `ShorePieces=12` | 893 |
| Clear/other (0) | all other sets | 21,189 |

- **Largest 4-connected Water region = 252 cells**, bounding box
  **`[71,11]`–`[95,40]`** in the raw IsoMapPack coordinate space (`MapCell::x/y`;
  ~25×30 iso). This is a large, contiguous, navigable open-water body — ample for
  the demo (move-across + land-block + attack). A demo ship placed inside this
  rectangle can path across it; the surrounding Beach(6)/Clear(0) cells are
  Float-blocked and form the natural shore that a "land block" test needs.

**Adjudication:** the demo (Task 7) can select start/goal cells inside the
`[71,11]`–`[95,40]` water rectangle on `mapsmd03`; no alternate water map is
required. (Task 7 works in whatever cell space `simulate_naval` uses — the region
is reported here in raw IsoMapPack coords; `pick_water_cell` should pick from the
468-cell Water set, ideally the 252-cell main body.) **NOT a [RE-FALLBACK].**

## Confidence table

| Item | Finding | Evidence | Confidence |
|------|---------|----------|:----------:|
| §1 | naval loco = standalone `ShipLocomotionClass`, CLSID `0x7E8AB0`, ctor `FUN_0069E2E0`, ILoco vtbl `0x7F1D8C`, Process `FUN_0069F2A0` | factory→CreateInstance→ctor→vtable chain | **CONFIRMED** |
| §1 | pathed cell-to-cell (not straight-line); water-only via SpeedType, not the loco | Process uses CellAt cell-stepping, no atan2 free-flight; wake at cell+0xEC==2 | **CONFIRMED** |
| §1 | same FootClass A\* + one `occ_` cell as ground | shared UnitClass/FootClass machinery; no naval-specific occ | **PROBABLE** |
| §1 | timed shortest-arc ROT turn (FacingClass) | Is_Rotating/Current in Process; Draw_Matrix facing rot | **CONFIRMED** |
| §2 | CellClass.LandType(+0xEC) ← IsoTileType(+0x298) ← tile | `FUN_0047D1C0`; read in `FUN_004833B0`/`FUN_00483B90` | **CONFIRMED** |
| §2 | LandType source = tile-set classification, NOT `.tmp` byte | map decode coherent by set; `.tmp` byte gave 9–15/0 water | **CONFIRMED (by behavior)** |
| §2 | exact `+0x298`-from-set write instruction | not isolated (heap-ctor init only) | **[gap — documented]** |
| §2 | passability BINARY `Ground[land][speed]==0.0` | `FUN_004833B0` strict float-eq gate | **CONFIRMED** |
| §2 | LandType/SpeedType enum orders; Float=col 5; Water passable only | name table `0x838D60`; `FUN_00673A40` layout; rules | **CONFIRMED** |
| §3 | one ship/cell, disjoint land/water sets, shared repath | single Ground table + shared FootClass | **PROBABLE** (disjointness CONFIRMED) |
| §4 | HYD auto-acquires land+naval (no targeting keys); SUB/DEST/DRED restrict | keys parsed in `FUN_00711510`; HYD has none | **CONFIRMED** |
| §4 | Range/ROF/Verses/FLH reuse combat.md/voxel-render.md | same parsers | **CONFIRMED** |
| §5 | Speed=8→20 lep/frame; ROT=6→1536 u16/frame; stat formulas | `voxel-render.md` §5 formulas | **CONFIRMED** |
| §6 | no RNG on naval movement path | no `FUN_0065C220` in Process/helpers | **CONFIRMED** |
| §6 | vanilla FlakTProj scatters (RNG); CB-3 instant-hit removes it | `[FlakTProj] FlakScatter/Inaccurate` + combat.md §5 | **CONFIRMED** (deviation) |
| §6 | deterministic step ordering | shared unit pass; no RNG | **PROBABLE** |
| Step 6 | reference map has 468 Water cells, 252-cell main body, bbox [71,11]-[95,40] | scratch map probe (tile-set classify) | **CONFIRMED** |


> **CORRECTION (fv7c, wave-7 audit 13a #1):** the TerrainType byte (+0x29) is
> NOT falsified -- it is an index into the exe table `DAT_008278F4`
> `{0,8,8,8,8,10,9,3,3,2,6,1,1,0,7,3}` read by `FUN_00544910` from the cell
> recalc `FUN_0047D1C0` (byte 9 -> Water). The original's LandType is per
> sub-tile through that table, followed by `CliffBackImpassability`
> (Rules+0x664 = 2: a Clear/Water/Beach/Ice cell with a back neighbour 4+
> levels higher becomes Rock). `resolve_land_types` now does this; the
> tile-set classification below is only its fall-back.

## Deviations we take

- **[PLAN DIFF — §2 LandType source, RE-GATE STOP].** The plan's `.tmp`
  TerrainType-byte source (`TmpFile::terrain_type=41`, `land_type_from_terrain_byte`)
  is **FALSIFIED** for LandType — that byte does not carry the 0–11 enum on these
  files. Task 2's `land_type_of_cell`/`resolve_land_types` must instead walk the
  theater control INI `[General]` tile-set classification (`WaterSet`,
  `ShorePieces`, `CliffSet`, `WaterCliffs`, `RoughTile`, …) exactly as the game's
  `IsoTileTypeClass` does, mapping each tile's set to the LandType enum. For CB-3's
  water-only need, the minimal faithful rule is: cell tile ∈ `WaterSet` ⇒ Water,
  else non-water (Float-blocked); shore/cliff sets are refinements.
- **[PLAN CONFIRM — §2 binary passability].** `build_water_passability` stays
  BINARY (Water⇒1, else⇒0 for a `Float` ship) — this matches the original's
  `== 0.0` gate. Speed-weighted terrain cost is a documented carry-forward, not
  the original's passability behavior.
- **[PLAN CONFIRM — §1 movement shape].** Grid-selected A\*, pathed cell-to-cell,
  one `occ_` cell, timed-ROT turn. The naval grid is the Land grid filtered by
  Float passability; do NOT retrofit ground-from-water blocking onto the Land grid.
- **[DEVIATION — §4 projectile / §6 RNG].** Model `FlakTProj` as an instant
  single-target-cell hit (the `voxel-render.md` §7 / `aircraft.md` §4 precedent),
  which removes the vanilla `FlakScatter`/`Inaccurate` ballistic-scatter RNG. Keep
  the `FlakTWH CellSpread=1.0` as at most a one-ring (~256-lepton) splash, or
  collapse to single-target for CB-3 (PercentAtMax=1.0 means no falloff either way).
- **[DEVIATION — pure-water ships].** No amphibious/beach crossing (Float=0% on
  Beach), no submarine cloak/submerge (the second CLSID `{4A582741}` loco is
  mapped, not implemented), no spawner delivery (DEST ASW / DRED missiles OUT).
- **[DEVIATION — determinism].** No RNG on the naval critical path; the only
  determinism lever is a fixed `step_units`/`step_combat` iteration order, which is
  under our control. AI does not field ships.
- **[DEFERRED — sub submerge/cloak loco `{4A582741-9839-11d1-B709-00A024DDAFD1}`].**
  Registered by the same factory `FUN_006BB020` (`0x7E8A30`), compared via
  `GetClassID` in `FUN_00740780`. Mapped for the deferred submarine; the surface
  ships `[HYD]`/`[DEST]`/`[DRED]` need only the primary ShipLocomotionClass.

## W7 (wave-7 air/naval fix) — recovered with addresses

- **Naval target test** [CONFIRMED-RE GetFireError @0x6FBC86, SelectWeapon
  FUN_006F2890]: a target is "in water" when its cell's LandType is Water (2)
  or Beach (6), it is not in the air and not on a bridge. Structures included
  (a Naval Yard is a naval target). The NavalTargeting switch FUN_006F2D80
  returns a fixed slot: 0 → `Underwater type && cloaked ? -1 : 0`; 1 →
  `Underwater type ? 1 : 0`; 2 → `Underwater type ? 0 : -1`; 3 → `Organic ||
  Unnatural ? 1 : 0`; 4 → `SpeedType != Hover && !Organic ? 1 : 0`; 5 → 0;
  6 → -1. Off the water, LandTargeting=1 is ILLEGAL.
- **SpawnManagerClass** [CONFIRMED-RE ctor FUN_006B6310, AI FUN_006B68B0 (runs
  every 10 frames), SetTarget FUN_006B7210, ResetTarget FUN_006B7230, Detach
  FUN_006B72E0, owner lost FUN_006B6780, launching count FUN_006B7400]. Node
  states 0 docked, 1 rocket after hand-off, 2 launched, 3 attacking, 4
  returning, 6 reloading (SpawnReloadRate), 7 dead (SpawnRegenRate, per node).
  A docked node launches while the manager holds a target, its launch timer
  ran out (20 frames, 9 for MissileSpawn) and the status is not 2; a rocket
  node also waits while the launcher moves. Aircraft gather beside the owner
  (cell N, then S) until every node is 2 or 7, then all attack together.
  Rockets are handed to the KamikazeTracker at that point and their node
  waits PauseFrames+TiltFrames (V3 block for the V3 type, else DMisl) before
  SpawnRegenRate starts (or at once if the rocket detonates first). Ship and
  drive locomotors hold position while FUN_006B7400 > 0 (ShipLoco
  FUN_006A1310 @0x6A13E6, DriveLoco @0x4B264C).
- **Naval Yard / Service Depot repair** [CONFIRMED-RE FUN_0044B780 → radio
  0x1C in FUN_006F4010]: RepairStep per step for GetRepairStepCost
  (FUN_00711470); refused (0x20) when the house cannot pay. UnitTypes
  (Dolphin, Squid included) use RepairStep, not IRepairStep.
- **Airfield pad repair** [CONFIRMED-RE FUN_0044B780 @0x44C84x]: every
  ReloadRate×900 frames each docked plane gets 0x1F (one ammo) and, once full,
  0x1C (one paid repair step).
- **Sonic wave** [CONFIRMED-RE WaveClass FUN_0075D770 / AI FUN_0075FD70 /
  FUN_00761910 / FUN_0075FF10 / FUN_0075E150]: front advances 0.05 of the line
  per frame while the firer keeps the target within 6×362 leptons; the back
  follows once 0.1 behind; every frame each cell under the moving segment
  deals AmbientDamage (weapon 0, its warhead) to everything but the firer.
- **DistributedFire / RadialFireSegments** [CONFIRMED-RE Fire FUN_006FD210
  @0x6FED3C / @0x6FDF7F, picker FUN_007089D0]: after each auto shot the target
  joins a shot list and is dropped; the next target is the best candidate not
  yet shot (list cleared when all were). Radial: launch facing = facing −
  k×32768/segments, k cycling.
