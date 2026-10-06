# Roster breadth II + remaining producers — reverse-engineering notes (P6)

Method: `docs/re/workflow.md` (YR 1.001 `gamemd.exe`, SHA1 `7BE4E550…`, image base
`0x00400000`). Hunt scripts `roster2_hunt1.py … roster2_hunt18.py`, decompile dumps and
the extracted `artmd.ini` live OFF-REPO in `D:\ra2-re\scripts` / `D:\ra2-re\out\roster2`.
Every function below was reached from INI-key string xrefs → loader store offset →
displacement scan of that offset in THIS binary (never from published addresses — the
YRpp drift held again: YRpp's `AdjacentCell` is `0x89F688`, here it is `0x89E5F8`).

Tag legend (roster.md convention): **[CONFIRMED]** read from the binary / rulesmd /
an `ra2extract` probe of this install; **[ANCHORED]** corroborated by documented
TS/RA2 structure but the exact instruction was not pinned; **[RE-FALLBACK]** a design
decision for this port.

Scope of P6: (A) the remaining Allied/Soviet roster (no Yuri), (B) Naval Yard as a
producer, (C) Airforce Command docking + rearm for Harrier/Black Eagle, (D) walls
(GAWALL/NAWALL wall-line + connection frames). Unit special ABILITIES (Terror Drone,
Desolator, Chrono, SEAL, IFV, Ivan) are another branch's scope: bodies/stats only here.

---

## TL;DR — load-bearing findings

1. **Walls are OVERLAYS, not buildings, once placed** [CONFIRMED]. `BuildingClass`
   place (`FUN_0043F100`): `if (Type->IsWall /*+0x1571*/) { new OverlayClass(Type->ToOverlay
   /*+0xE54*/, cell, house); delete self; }` (ctor `FUN_005FBB70`).
2. **Wall connection frame = 4-bit N/E/S/W mask in the low nibble of `OverlayData`**
   [CONFIRMED]. `CellClass::Wall_Update` (`FUN_00480540`) walks the facing list
   `{0,2,4,6}` @`0x81BC80` (N,E,S,W), sets bit *i* when the adjacent cell
   (`AdjacentCell[facing]`) carries the SAME overlay index (`FUN_00480420`; also wall
   gates, absent here), and writes `OverlayData = (OverlayData & 0xF0) | mask`. The
   high nibble is the damage stage (`CellClass::Reduce_Wall` `FUN_00480BC0` adds 0x10
   per stage, bounded by `OverlayType::DamageLevels` +0x2A0 = 3). So
   **frame = stage*16 + mask**; `gtwall.shp` has 96 frames = 3 stages × 16 masks ×
   (image+shadow halves).
3. **`AdjacentCell[8]` @`0x89E5F8` is built at static init** (`0x0049F200`) [CONFIRMED]:
   `N(0,-1) NE(1,-1) E(1,0) SE(1,1) S(0,1) SW(-1,1) W(-1,0) NW(-1,-1)`. Wall mask bits:
   **bit0=N (0,-1), bit1=E (+1,0), bit2=S (0,+1), bit3=W (-1,0)**.
4. **Wall-LINE fill** [CONFIRMED] (`FUN_00588180`, called from `KickOutUnit` when a
   ToOverlay-wall type is placed). For each facing N,E,S,W: walk k=1..`GuardRange>>8`
   cells (`TechnoType+0x5B8`; GAWALL/NAWALL `GuardRange=5` → 5 cells). If cell k holds
   the SAME wall overlay owned by the SAME house → fill cells 1..k-1 with new wall
   objects and stop. Else if the cell is placeable for the wall
   (`FUN_0047C530(SpeedType, type, house)`) continue; else stop that direction. The
   filled segments are created via `CreateObject`+`Unlimbo` with **no money call**
   (free). The decompiler showed only x advancing in the fill loop — the disassembly
   (`0x588342..0x58835A`) advances BOTH x and y (decompiler artifact, not a game bug).
5. **Walls build 3× slower** [CONFIRMED]: `TechnoClass::Time_To_Build` (`FUN_006F3D00`):
   `if (WhatAmI==Building && Type->IsWall) frames = ftol((double)frames *
   Rules.WallBuildSpeedCoefficient /*+0x758*/)`; rulesmd `WallBuildSpeedCoefficient=3.0`
   ("multiplier to time, so it means SLOWER"). Applied AFTER the integer build time.
6. **WaterBound buildings use SpeedType Float for placement** [CONFIRMED]: the
   BuildingType loader (`FUN_0045FE40` @`0x45FF85`) does `SpeedType(+0x67C) = WaterBound
   ? 5 /*Float*/ : -1` — so the cell-placeability test (`FUN_0047C530`) evaluates the
   foundation against the Float column of the land-type table: every foundation cell
   must be **Water** (naval.md §2: Float is passable ONLY on `[Water]`).
7. **Naval factories are a separate production category** [CONFIRMED]. The house
   factory counters (`FUN_004FF7A0`/`FUN_004FF870`) split `Factory=UnitType` buildings
   by `Naval` (TechnoType +0xCCE): naval → `House+0x5388`, land → `House+0x5380`; the
   primary-factory toggle (`FUN_00447FC0`) only competes buildings with equal
   (Factory, Naval). A ship is built by a `Naval=yes` UnitType factory (GAYARD/NAYARD),
   never by GAWEAP/NAWEAP, and vice versa.
8. **Naval exit** [CONFIRMED shape] (`KickOutUnit` `FUN_00443BC0`, WeaponsFactory branch,
   `Naval` unit): with a rally target it steps from the yard centre toward the rally
   until off the yard's foundation; otherwise (and whenever that cell is not Water /
   blocked) it takes `MapClass::NearByLocation(centre, unit SpeedType=Float, …)`
   (`FUN_0056D750`) — the nearest free cell valid for a Float unit — and Unlimbos there.
9. **Airfield docking pads** [CONFIRMED]: dock coordinate (`0x447C8D..0x447CF2`) =
   `Building->GetCoords() + DockingOffset[pad]` where `pad` = the aircraft's radio-contact
   slot (`FUN_0065A7D0`) and `pad < NumberOfDocks` (+0x1780; GAAIRC = 4). artmd
   `[GAAIRC] DockingOffset0..3 = (0,-128) (0,128) (256,-128) (256,128)` leptons.
10. **Rearm cadence** [CONFIRMED]: the airfield's repair/reload mission (`FUN_0044B780`,
    `UnitReload=yes` branch) loops its radio contacts, sends RELOAD, and when at least
    one docked aircraft took it returns the mission delay
    `ftol(Rules.ReloadRate /*+0x1508*/ * 900.0 /*@0x7E17F8*/)`; rulesmd `ReloadRate=.3`
    → **270 frames per reload step**. +1 ammo per step [ANCHORED: TS RADIO_RELOAD
    semantics; the exact `Ammo++` site for docked aircraft was not pinned — the
    `TechnoClass::Reload` slot (`FUN_006FA530`, vtable+0x424) increments by one].
    ORCA/BEAG `Ammo=1` → one step refills.
11. **Ammo drain on the attack run** [CONFIRMED] (`AircraftClass::Mission_Attack`
    `FUN_00417FE0`, status @+0xBC): a fired pass sets a flag (+0x6C8) consumed as
    `Ammo(+0x2FC)--`; at `Ammo==0` the status switches to return-to-base (10).
    `AirportBound=yes` (+0xE0D) aircraft only land on their Dock= airfield.

## §A Remaining roster (rules + art, this install)

Rostered BEFORE P6 (do not re-add): kRoster[0..23] = GACNST GAPOWR GAPILE GAWEAP NACNST
NAPOWR NAHAND NAWEAP GAREFN NAREFN GAPILL GATOW NALASR NABNKR TESLA NAFLAK NAWALL NARADR
GATECH CAOILD NANRCT ATESLA NASAM GAAIRC; units kUnits/kSoviet/kRosterUnits (E1 GGI MTNK FV
SREF / E2 HTNK APOC HTK / DRON V3 ZEP SHK DESO APOC TERROR GHOST CLEG JUMPJET MGTK ORCA) +
ENGINEER SPY DOG ADOG HARV CMIN AMCV SMCV + naval HYD/DRED/DEST (naval demo only).

### Buildings to APPEND (kRoster index 24+), rulesmd [CONFIRMED]

| id | Name | Prereq | TL | Cost | Str | Armor | Power | flags | art (engine loader) |
|---|---|---|---|---|---|---|---|---|---|
| GAYARD | Allied Shipyard | PROC,POWER,GACNST | 4 | 1000 | 1500 | concrete | -25 | Factory=UnitType Naval WaterBound | ggyard.shp Y |
| NAYARD | Soviet Shipyard | PROC,POWER,NACNST | 2 | 1000 | 1500 | concrete | -20 | Factory=UnitType Naval WaterBound | ngyard.shp Y |
| GAWALL | Allied Wall | GAPILE | 1 | 100 | 300 | concrete | 0 | Wall ToOverlay=GAWALL GuardRange=5 | temperat gtwall.shp Y |
| GADEPT | Allied Service Depot | GAWEAP,GACNST | 6 | 800 | 1200 | wood | -25 | | ggdept.shp Y |
| NADEPT | Soviet Service Depot | NAWEAP,NACNST | 6 | 800 | 1200 | wood | -20 | | ngdept.shp Y |
| NATECH | Soviet Battle Lab | NAWEAP,RADAR,NACNST | 7 | 2000 | 500 | wood | -100 | | ngtech.shp Y |
| NAIRON | Iron Curtain | NATECH,NACNST | 10 | 2500 | 750 | concrete | -200 | SW IronCurtainSpecial | ngiron.shp Y |
| NAMISL | Nuclear Missile Silo | NATECH,NACNST | 10 | 5000 | 1000 | concrete | -200 | SW NukeSpecial | ngmisl.shp Y |
| GACSPH | Chronosphere | GATECH,GACNST | 10 | 2500 | 750 | concrete | -200 | SW ChronoSphereSpecial | ggcsph.shp Y |
| GAGAP | Gap Generator | GATECH,GACNST | 7 | 1000 | 600 | wood | -100 | | gggap.shp Y |
| GAOREP | Ore Processor | GATECH,PROC,GACNST | 10 | 2500 | 900 | wood | -200 | | ggorep.shp Y |
| GTGCAN | Grand Cannon | RADAR,GACNST | 7 | 2000 | 900 | steel | -100 | French; GrandCannonWeapon | gggcan.shp Y |
| AMRADR | American Airforce Cmd | GAREFN,GACNST | 3 | 1000 | 600 | steel | -50 | Factory=AircraftType, Americans, **Image=GAAIRC** | via Image= Y |
| GAWEAT | Weather Controller | GATECH,GACNST | 10 | 5000 | 1000 | concrete | -200 | Image=GAWETH | ABSENT (placeholder) |
| NAINDP | Industrial Plant | NATECH,PROC,NACNST | 10 | 2500 | 1000 | wood | -200 | | ABSENT (placeholder) |

Deferred: **GASPYSAT** (art absent under the loader AND it carries the shroud-phase
SpySat reveal-ordering carry, roster.md §5), **GAROBO** (art absent everywhere),
Yuri-prereq NAPSIS/NATBNK/YA*.

Art-loader note [RE-FALLBACK]: the building loader resolves the SHP by the rules
**id** (so ATESLA→GAPRIS stays art-absent and the 18th gate's `roster_hash` stays
pinned). For the NEW indices ≥24 only, the loader honours rules `Image=` (AMRADR draws
as GAAIRC, GAWEAT as GAWETH — absent here) — indices 0..23 resolve byte-identically.

### Units — new content table `kRoster2Units` [CONFIRMED stats]

Vehicles (voxel; art probe `local.mix`, md fallback `localmd.mix`): SAPC(TRS)
CARRIER SUB AEGIS LCRF DRED HYD DEST TNKD TTNK DTRUCK(TRUCKA) SHAD + md-only BFRT SCHP
ROBO; aircraft BEAG (FALC-class fixed-wing, `Ammo=1`, `Dock=GAAIRC,AMRADR`). Infantry:
IVAN SNIPE TANY FLAKT SENGINEER(ENGINEER) BORIS(conqmd) CCOMAND PTROOP CIVAN. Cut: SQD,
DLPH (no voxel art on this install). Stats in the table below are the gate's per-type
witness (cost/tech/strength), resolved generically by `resolve_combat_unit`.

| id | Prereq | TL | Cost | Str | Speed | Primary | note |
|---|---|---|---|---|---|---|---|
| SAPC | NAYARD | 2 | 900 | 300 | 6 | — | Naval=yes amphibious (hover CLSID), Passengers=12 |
| CARRIER | GAYARD,TECH | 7 | 2000 | 800 | 4 | HornetLauncher | spawner (ability deferred) |
| SUB | NAYARD | 2 | 1000 | 600 | 4 | SubTorpedo | |
| AEGIS | GAYARD,RADAR | 7 | 1200 | 800 | 4 | Medusa | |
| LCRF | GAYARD | 4 | 900 | 300 | 6 | — | amphibious transport |
| DRED | NAYARD,NATECH | 6 | 2000 | 800 | 4 | DredLauncher | spawner deferred |
| HYD | NAYARD,NARADR | 6 | 600 | 400 | 8 | FlakTrackGun | |
| DEST | GAYARD | 4 | 1000 | 600 | 6 | 155mm | |
| TNKD | GAWEAP,RADAR | 2 | 900 | 400 | 5 | SABOT | Germans |
| TTNK | NAWEAP,NARADR | 10 | 1200 | 300 | 6 | TankBolt | Russians |
| DTRUCK | NAWEAP,RADAR | 10 | 1500 | 150 | 5 | Demobomb | Africans (ability deferred) |
| SHAD | GAWEAP | 7 | 1000 | 175 | 14 | BlackHawkCannon | |
| BFRT | GAWEAP,GATECH | 10 | 2000 | 600 | 4 | 20mmRapid | md art |
| SCHP | NAWEAP,TECH | 7 | 1400 | 300 | 12 | BlackHawkCannon | md art |
| ROBO | GAWEAP,GAROBO | 2 | 600 | 180 | 10 | Robogun | md art; GAROBO absent |
| BEAG | RADAR | 3 | 1200 | 200 | 14 | Maverick2 | Ammo=1, Alliance |
| IVAN | NAHAND,NARADR | 5 | 600 | 125 | 4 | IvanBomber | ability other branch |
| SNIPE | GAPILE,RADAR | 1 | 600 | 125 | 4 | AWP | British |
| TANY | GAPILE,GATECH | 9 | 1500 | 200 | 6 | DoublePistols | |
| FLAKT | NAHAND,NARADR | 1 | 300 | 100 | 4 | FlakGuyGun | |
| SENGINEER | Barracks | 1 | 500 | 75 | 4 | DefuseKit | |
| BORIS | NAHAND,NATECH | 9 | 1500 | 200 | 5 | AKM | md art |
| CCOMAND | BARRACKS | 9 | 2000 | 100 | 5 | ChronoMP5 | stolen-tech |
| PTROOP | BARRACKS | 9 | 1000 | 100 | 5 | MindControl | stolen-tech |
| CIVAN | BARRACKS | 9 | 1750 | 100 | 6 | IvanBomber | stolen-tech |

## §B Naval Yard as a producer

Rules [CONFIRMED]: `[GAYARD]/[NAYARD] Factory=UnitType WeaponsFactory=yes Naval=yes
WaterBound=yes Foundation=4x4 NumberOfDocks=1 NumberImpassableRows=3 Adjacent=12`;
art `DockingOffset0=384,-128,0`. Loader offsets (`FUN_0045FE40`): WeaponsFactory +0x16BD,
Factory +0xEB8, NumberOfDocks +0x1780, NumberImpassableRows +0x1620, Naval(TechnoType)
+0xCCE, building SpeedType +0x67C (from WaterBound).

- **Placement** = all foundation cells Water (§TL;DR 6). Adjacency `Adjacent=12` is the
  usual expanded-rect own-building test.
- **Category** (§TL;DR 7): naval units ⇔ naval UnitType factory. Port: a separate naval
  `ProductionState` driven by the existing free `start/tick` (54-step economy, shared
  FactoryClass math — production.md §1.5).
- **Exit** (§TL;DR 8): port = the existing `exit_cell` ring walk from the yard anchor,
  with the free-predicate = water-passable ∧ unoccupied ∧ not a building [RE-FALLBACK:
  ring order vs `NearByLocation`'s spiral order not replicated; same reachable set].
- `NumberImpassableRows=3` (the yard's water bay) [NOT IMPLEMENTED]: the whole 4×4
  footprint is blocked for ships (documented deviation).

## §C Airforce Command docking + rearm

Rules [CONFIRMED]: `[GAAIRC] Helipad=yes(+0x16CB) UnitReload=yes(+0x16AA)
NumberOfDocks=4 Factory=AircraftType`; `[ORCA]/[BEAG] Ammo=1(+0x684) Landable=yes(+0xE0A)
AirportBound=yes(+0xE0D) Dock=GAAIRC,AMRADR Speed=14 Locomotor={4A582746…}=Fly`;
`[General] ReloadRate=.3 FlightLevel=1500`.

- Pad k coord = building centre + DockingOffset[k] (leptons). Centre [ANCHORED:
  `GetCoords` of a building = foundation centre] = (cx*256 + fw*128, cy*256 + fh*128).
  GAAIRC 3×2 → pads at cells (cx+1,cy) (cx+1,cy+1) (cx+2,cy) (cx+2,cy+1).
- Pad assignment = first free radio slot [ANCHORED: RadioClass contact index].
- Sortie: Docked → TakeOff (climb to FlightLevel) → fly straight to target → fire once
  per pass, `Ammo--` → at Ammo 0 return to the SAME airfield/pad → Landing → Docked →
  each 270-frame reload cycle +1 ammo until full.
- [RE-FALLBACK] FlyLocomotion climb/descent rate not hunted: 100 leptons/frame (1500
  FlightLevel → 15 frames). Cruise speed = Speed × the engine's existing
  kCellsPerSecPerSpeed (0.75 cps per Speed unit), straight-line like aircraft.md §1.

## §D Walls

`[GAWALL]/[NAWALL] Wall=yes(+0x1571) ToOverlay=self(+0xE54) BuildCat=Combat Cost=100
Strength=300 Adjacent=8 GuardRange=5`; art `Foundation=1x1 DamageLevels=3`;
OverlayType `Wall` +0x2A8, `DamageLevels` +0x2A0.

- Frame = stage*16 + mask (§TL;DR 2/3). [RE-FALLBACK] the engine keeps walls as 1×1
  `Building`s (the AI already places NAWALL that way and its hashes depend on it), so
  the damage stage is derived from HP (stage = 0 above ConditionYellow, else 1; the
  third stage is the destroyed rubble frame, never shown on a live wall).
- Line fill (§TL;DR 4) is a pure function; the fill segments are free.
- Build time = ftol(build_frames(cost) × 3.0) (§TL;DR 5); exposed as a pure helper and
  used by the new gate (the live Defense queue keeps today's timing so the AI-base
  gate — which builds NAWALL — stays byte-identical; carry-forward).

## Located functions

| Function | Role |
|---|---|
| `FUN_0045FE40` | BuildingTypeClass::LoadFromINI — Wall +0x1571, ToOverlay +0xE54, Factory +0xEB8, WeaponsFactory +0x16BD, Helipad +0x16CB, UnitReload +0x16AA, NumberOfDocks +0x1780, DockingOffsets +0x1788, NumberImpassableRows +0x1620, WaterBound→SpeedType +0x67C |
| `FUN_00711510` | TechnoTypeClass::LoadFromINI — Naval +0xCCE, Ammo +0x684, Reload +0x698, GuardRange +0x5B8 |
| `0x41CC7x` | AircraftTypeClass loader — Landable +0xE0A, AirportBound +0xE0D, Fighter +0xE0E |
| `FUN_0066CF70` | RulesClass general — ReloadRate +0x1508, WallBuildSpeedCoefficient +0x758 |
| `FUN_0043F100` | BuildingClass place: IsWall → OverlayClass(ToOverlay) |
| `FUN_005FBB70` | OverlayClass ctor |
| `FUN_00480540` | CellClass::Wall_Update (N/E/S/W mask into OverlayData low nibble) |
| `FUN_00480420` | same-wall neighbour predicate |
| `FUN_00480BC0` | CellClass::Reduce_Wall (stage += 0x10, DamageLevels) |
| `0x0049F200` | AdjacentCell[8] static initializer |
| `FUN_00588180` | wall-line fill (GuardRange cells, same-house same-overlay, free) |
| `FUN_006F3D00` | Time_To_Build × WallBuildSpeedCoefficient for walls |
| `FUN_00443BC0` | BuildingClass::KickOutUnit (wall fill call; naval exit via NearByLocation) |
| `FUN_004FF7A0/FUN_004FF870` | house factory counters split by Naval |
| `FUN_00447FC0` | Toggle_Primary (same Factory+Naval) |
| `FUN_0044B780` | BuildingClass repair/reload mission (ReloadRate×900 cadence) |
| `0x447C8D` | dock coord = GetCoords + DockingOffset[pad] |
| `FUN_00417FE0` | AircraftClass::Mission_Attack (Ammo-- per pass, RTB at 0) |
| `FUN_004190B0` | AircraftClass::Receive_Message (ammo-full queries 0x1D/0x21) |

## Confidence summary

CONFIRMED: every rules/art value; wall mask layout + facing table; frame = stage*16+mask;
wall-line fill algorithm and its freeness; WallBuildSpeedCoefficient multiply; WaterBound
→ Float placement; naval/land factory split; naval exit via NearByLocation; dock pad
formula; 270-frame reload cadence; Ammo-- per attack pass. ANCHORED: +1 ammo per reload
step; building GetCoords = foundation centre; pad = first free radio slot.
RE-FALLBACK: walls stay Buildings (HP→stage), exit ring order, Fly climb rate, live
queue wall-time wiring deferred, NumberImpassableRows not modelled.
