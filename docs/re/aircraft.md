# Aircraft + flight-locomotor semantics — reverse-engineering notes

(Method: `docs/re/workflow.md`; image base `0x00400000`, YR 1.001 `gamemd.exe`.
Hunt scripts, extracted `rulesmd.ini`/`artmd.ini`, and decompile dumps are all
off-repo in the session scratchpad — `hunt1.py … hunt11.py`, dumps under
`scratchpad/out/h*_*.c`; nothing off-repo is committed. Content-anchored only:
every function below was reached from string/data/xref/vtable evidence in THIS
binary, never from published addresses — the Ares/YRpp drift documented in
`combat.md` §A and `workflow.md` §4 held here too. Prior anchors used as the
entry map: `voxel-render.md` §located-functions — the shadow renderer
`FUN_00755680`, the flush `FUN_00753110`, the INI loader `FUN_00711510`, the
DriveLoco Draw_Matrix `FUN_004AFE70` (ILocomotion vtable `0x7E6EB0`), the
GetFLH `FUN_006F3030`, and the FacingClass family `FUN_004C9140…`; plus
`combat.md` for the RNG (`FUN_0065C220`) and damage math.)

This document is the calibration authority for the **CB-2 Aircraft & Flight
Locomotor** phase. The exemplar is the **Kirov airship `[ZEP]`** with the
**Jumpjet locomotor**. The `[ZEP]`/`[BlimpBomb]`/`[BlimpHE]`/`[JumpjetControls]`
rules VALUES were read on this install and are ground truth already (quoted
below); this hunt pins the FORMULAS / offsets / semantics that turn them into
engine constants. Confidence per finding; "Deviations we take" at the end
resolves the `[RE-GATE: aircraft.md §N]` markers in the CB-2 plan Tasks 2–7.

## TL;DR — the deltas from the plan's pre-hunt assumptions

> **W7 (2026-10): items 1 and 2 below are SUPERSEDED.** They describe
> HoverLocomotionClass (vtable `0x7E9CFC`), not the jumpjet. The jumpjet turns
> at `JumpJetTurnRate<<8` and moves `JumpjetSpeed` RAW leptons per frame with
> a `JumpJetAccel` ramp (§1a); fixed-wing planes are in §1b.

Load-bearing findings that differ from (or sharpen) the plan and need RE-GATE
adjudication:

1. **The body/heading turn rate is driven by the unit's `ROT`, not
   `JumpJetTurnRate`.** `JumpjetLocomotionClass::Link_To_Object`
   (`FUN_00513AD0`) sets the FacingClass rate from `TechnoType+0x71C` (**ROT**)
   `<< 1`, i.e. **ROT × 2**. No consumer of `JumpJetTurnRate` (`type+0xD70`) was
   found inside the whole Jumpjet locomotor class (`0x513A40…0x5173xx`) — it
   appears **vestigial for the turn** in this build. ZEP `ROT=10` (→ rate
   `20<<8` u16/frame class), `JumpJetTurnRate=2` is not the turn rate.
2. **Flight cruise uses a Jumpjet-specific speed (`JumpjetSpeed`, `type+0xD74`),
   a field distinct from base `Speed` (`type+0x678`).** Both are parsed
   separately by the INI loader. The loco reads the current speed through the
   object's `GetMaxSpeed` virtual (obj vtbl `+0x538`); the exact branch picking
   `JumpjetSpeed` vs `Speed` was not isolated, but the dedicated field's
   existence makes `JumpjetSpeed` the flight speed. **For ZEP both = 5**, so the
   exemplar is insensitive to the choice — but the plan must consult
   `JumpjetSpeed`, not `Speed`.
3. **The wobble is a deterministic SINE of the frame counter, not an RNG draw.**
   `FUN_00513B40` (called from Process) computes the BalloonHover bob from
   `sin(frame · k)` (`DAT_00A8DCEC` = current frame, `FUN_004CABD0` = sin) plus
   the `[JumpjetControls]` `WobblesPerSecond/WobbleDeviation` rules globals — the
   game's R250 RNG (`FUN_0065C220`, `combat.md` §5) is **never consulted** on the
   flight path. `JumpjetNoWobbles=yes` disables the bob entirely.
4. **`[BlimpHE] CellSpread=2` is a genuine multi-cell splash (~512 leptons).**
   Unlike the Rhino's `AP CellSpread=.3` (target-only, `voxel-render.md` §7),
   the Kirov's warhead really does splash ~2 cells — the single-target
   simplification does NOT apply here. Air-to-ground damage must run the
   `combat.md` §7 spread applicator (`FUN_00489190`).
5. **`Ammo` default = −1 (unlimited), verified at the binary level.** The
   TechnoType ctor (`FUN_0070FE90`) sets `type+0x684 = -1`; the INI loader reads
   `"Ammo"` into the same field with the current value as default; `[ZEP]` has no
   `Ammo=` key → stays −1 → the rearm/airfield-dock branch is provably not taken.

Items that match the plan / documented defaults: straight-line (unpathed)
flight; fire-from-cruise (no descend-to-attack); airborne unit off the pathing
grid; the shadow-in-color-1 convention.

## Recovered exemplar ground truth (read from this install)

```
[ZEP]  Speed=5  JumpjetSpeed=5  JumpjetClimb=6  JumpjetCrash=12  JumpJetAccel=10
       JumpJetTurnRate=2  JumpjetHeight=750  JumpjetNoWobbles=yes  Crashable=yes
       PitchSpeed=.9  PitchAngle=0  ROT=10  SpeedType=Hover  MovementZone=Fly
       ConsideredAircraft=yes  BalloonHover=yes  Size=50  Strength=2000  Armor=medium
       Locomotor={92612C46-F71F-11d1-AC9F-006008055BB5}   ; Jumpjet
       Primary=BlimpBomb  ElitePrimary=BlimpBombE   (no Ammo= key)
artmd [ZEP]  Voxel=yes  PrimaryFireFLH=-50,0,-140
[BlimpBomb]  Damage=250 Burst=1 ROF=50 Range=1.5 Projectile=BlimpBombP Speed=20
             Warhead=BlimpHE OmniFire=yes
[BlimpBombP] Image=ZBOMB Arm=10 Shadow=no Vertical=yes DetonationAltitude=20000
[BlimpHE]    CellSpread=2 PercentAtMax=.5 Wall=yes Wood=yes
             Verses=100,100,100,70,35,35,85,75,50,100,100
[JumpjetControls]  TurnRate=4  Speed=14  Climb=5  CruiseHeight=500
                   Acceleration=2  WobblesPerSecond=.15  WobbleDeviation=40
[ORCA] (deferred fixed-wing sibling)  Locomotor={4A582746-9839-11d1-B709-00A024DDAFD1}
                   Ammo=1  PipScale=Ammo  Dock=GAAIRC,AMRADR   MovementZone=Fly
```

## Recovered layout (offsets are content-verified)

**TechnoTypeClass jumpjet block** — a contiguous region parsed by
`TechnoTypeClass::LoadFromINI` (`FUN_00711510`) and defaulted by the TechnoType
ctor (`FUN_0070FE90`); the ctor defaults equal the `[JumpjetControls]` values,
confirming both the offsets and that `[JumpjetControls]` is the fallback source:

| Key | TechnoType offset | Read as | ctor default (`FUN_0070FE90`) |
|-----|-------------------|---------|-------------------------------|
| `JumpJetTurnRate` | `+0xD70` | int (`ReadInteger 0x527400`) | 4  (= `[JumpjetControls]TurnRate`) |
| `JumpjetSpeed` | `+0xD74` | int | 14 (= `Speed`) |
| `JumpjetClimb` | `+0xD78` | **float** (`ReadFloat 0x528100`) | 5.0 (= `Climb`) |
| `JumpjetCrash` | `+0xD7C` | **float** | 5.0 |
| `JumpjetHeight` | `+0xD80` | int (leptons) | 500 (= `CruiseHeight`) |
| `JumpJetAccel` | `+0xD84` | **float** | 2.0 (= `Acceleration`) |
| `JumpjetWobbles` | `+0xD88` | **float** | 0.15 (= `WobblesPerSecond`) |
| `JumpjetNoWobbles` | `+0xD8C` | bool (`ReadBool 0x529320`) | 0 |
| `JumpjetDeviation` | `+0xD90` | int | 40 (= `WobbleDeviation`) |
| `JumpJet` | `+0xD94` | bool | 0 |
| `Crashable` | `+0xD95` | bool | 0 |
| `ROT` | `+0x71C` | int (`voxel-render.md` §5) | — (the ACTUAL turn source, §1) |
| `Speed` | `+0x678` | int, `min(255,clamp·256/100)` (`voxel-render.md` §5) | — (base; distinct from JumpjetSpeed) |
| `Ammo` | `+0x684` | int | **-1 (unlimited)** |
| `BalloonHover` | (bool, read @`0x714135`) | bool | 0 |
| `MovementZone` / `SpeedType` / `ConsideredAircraft` | (read in `FUN_00711510`) | enum/bool | — |

INI-key matching is case-insensitive, so rules' `JumpJetTurnRate`/`JumpJetAccel`
match the binary's `"JumpjetTurnRate"`/`"JumpjetAccel"` literals (`0x842670` /
`0x842620`).

**WeaponTypeClass**: `OmniFire` → `WeaponType+0x12B` (bool, read in
`FUN_0077151B`).

**RulesClass `[JumpjetControls]` globals** (loaded by `FUN_00673E10`): TurnRate
`+0x40C`, Speed `+0x410`, Climb `+0x418` (dbl), CruiseHeight `+0x420`,
Acceleration `+0x428` (dbl), WobblesPerSecond `+0x430` (dbl), WobbleDeviation
`+0x438`.

**JumpjetLocomotionClass object** (COM, `new(0x78)` — `FUN_006C3990`):
`this+0` = ILocomotion vtable `0x7E9CFC`, `this+4` = IPersistStream vtable
`0x7E9DC8`; `this+8`/`+0xC` = LinkedTo object; FacingClass embedded at `+0x30`
(ctor `FUN_004C9100`); `+0x20…+0x28` head-to destination coord; `+0x60` (double)
= wobble/bob accumulator; `+0x2C…` = the render/pixel-offset matrix
(ILocomotion `+0x2C` per `voxel-render.md`'s `VXD_4DAE20`).

---

## §1 Flight movement model — straight-line, turned by the locomotor's own rate, off the pathing grid

> **W7 correction (wave-7 gap W7-05-15 / W7-05-02, re-derived 2026-10 from
> RTTI + decompiles; dumps `D:\ra2-re\w7-air-b-out`, `w7fix-move-air-out`).**
> The earlier version of this section described the vtable `0x7E9CFC`
> (ctor `FUN_00513A40`, Link `FUN_00513AD0`, Process `FUN_00514130`, wobble
> `FUN_00513B40`) as the Jumpjet locomotor. RTTI shows that class is
> **HoverLocomotionClass** — its `ROT<<1` turn and `Speed·256/100` step are the
> HOVER model, not the jumpjet one. The real Jumpjet and Fly models follow.

### §1a JumpjetLocomotionClass (Kirov, Night Hawk, Siege Chopper, Rocketeer)

ILocomotion vtable **`0x7EBD68`** (IPiggyback `0x7EBD44`). Link
`FUN_0054AA60`, Process `FUN_0054ABF0`, Move_To `FUN_0054AEF0`, integrator
`FUN_0054CDA0`; state handlers (loco `+0x4C`): 0 Grounded `FUN_0054B6B0`,
1 Ascending `FUN_0054B760`, 2 Hovering `FUN_0054BA60`, 3 Cruising
`FUN_0054BD20`, 4 Descending `FUN_0054C280`, 5 Crashing `FUN_0054C7C0`.
Process runs the integrator FIRST, then the state handler. [CONFIRMED-RE]

- **Link copies the type's jumpjet block** (loco offsets): `+0x18` =
  JumpJetTurnRate (`type+0xD70`), `+0x1C` = JumpjetSpeed (`+0xD74`, int),
  `+0x20` = JumpjetClimb (`+0xD78`, float), `+0x24` = JumpjetCrash (`+0xD7C`,
  float), `+0x28` = JumpjetHeight (`+0xD80`, at least 2·LevelHeight),
  `+0x2C` = JumpJetAccel (`+0xD84`, float), `+0x30` = JumpjetWobbles (`+0xD88`),
  `+0x34` = JumpjetDeviation (`+0xD90`), `+0x38` = JumpjetNoWobbles (`+0xD8C`).
  Absent keys take the `[JumpjetControls]` defaults (TurnRate=4, Speed=14,
  Climb=5, CruiseHeight=500, Acceleration=2).
- **Turn rate = JumpJetTurnRate, not ROT.** Link calls `FUN_004C9100(TurnRate)`
  on the loco's FacingClass (`+0x50`): `rate = min(TurnRate,127) << 8` u16 per
  frame (Kirov 2 → 512/frame, a full turn in 128 frames). The FacingClass
  (`FUN_004C9140` set-desired, `FUN_004C92F0` current) turns shortest-arc at that
  rate. Cruising re-sets the desired facing toward the goal every frame
  (`FUN_004CAD50` atan2 → `FUN_004C9140`).
- **Speed = JumpjetSpeed RAW leptons per frame.** No `·256/100`, no
  GameSpeedBias. The integrator moves `ftol(CurSpeed)` leptons along the
  **current** facing (`@0x54D1FB`: `y −= cos·v`, `x += sin·v`, angle =
  `(facing − 0x3FFF)·(−2π/65536)`, `DAT_007E1810`). Kirov 5 → 5 leptons/frame
  ≈ **0.29 cells/s** at 15 fps; Night Hawk / Siege Chopper / Rocketeer 30 → 1.76.
- **Acceleration** (integrator head): CurSpeed (`+0x70`, double) rises by
  JumpJetAccel per frame while below the target speed (`+0x78`), capped at
  JumpjetSpeed (not at the target); above the target it falls by
  `1.5·Accel` (`DAT_007E38F0` = 1.5), floored at 0 (`DAT_007E1800`).
- **Target speed** — Ascending (`FUN_0054B760`): 0 until the height passes
  `Height/4` (`Height/2` for BalloonHover), then JumpjetSpeed. Cruising
  (`FUN_0054BD20`), with `dist` the 2-D distance to the goal (int leptons):
  `dist < 0x14` → arrived (CurSpeed = target = 0, placed exactly on the goal,
  then Hovering/Descending); `dist < Speed` → `Speed/8`; `dist < 2·Speed` →
  `Speed/4` (`Speed/10`, at least 1.0, while the remaining turn
  `((d>>7)+1)>>1` exceeds TurnRate); `dist < 50·Speed/TurnRate` → `Speed/2`
  (`Speed/5`, at least 1.0, while the remaining turn exceeds `5·TurnRate`);
  else `Speed`. Integer divisions; `DAT_007E0718` = 1.0.
- **Crashing** (`FUN_0054C7C0`): the desired facing is set to `current +
  0x2D00` every frame (a spin at the turn rate) while the integrator keeps
  moving it along its facing at CurSpeed and Z drops by JumpjetCrash per frame;
  on reaching the ground it sends message `0x117C` (crash damage: W7-05-09).
- **Wobble**: phase `+0x88 += 2π / (15 / JumpjetWobbles)` per frame
  (`DAT_007E2CC0` = 2π, `DAT_007EBE60` = 15.0) in Hovering/Cruising unless
  JumpjetNoWobbles; height target swings by `sin(phase)·Deviation`. No RNG.

### §1b FlyLocomotionClass (Harrier, Black Eagle, Hornet, Osprey, paradrop plane)

ILocomotion vtable **`0x7E79F4`**; Process `FUN_004CCA60`, movement
`FUN_004CD520`, Apparent_Speed `FUN_004CFD40`. Loco fields: `+0x1C` moving
destination, `+0x38` FlightLevel, `+0x40` TargetSpeed, `+0x48` CurrentSpeed
(fractions), `+0x50` IsTakingOff, `+0x51` IsLanding, `+0x5C` IsElevating.
[CONFIRMED-RE unless marked]

- **Turn = ROT<<8 u16/frame** (no ×2): AircraftClass `FUN_00413F80` calls
  FacingClass::Set_ROT `FUN_004C95A0(type+0x71C)` = `min(ROT,127) << 8` on
  both facings (`+0x388`, `+0x3A0`). Harrier ROT 3 → 768/frame (85 frames per
  revolution).
- **Moves along its current facing**: `FUN_004CD520 @0x4CD9A4` steps
  Apparent_Speed leptons along the body facing, so a plane overflies its target
  and loops back (turn radius ≈ 35/0.0736 ≈ 475 leptons for the Harrier).
- **Speed**: Apparent_Speed = `ftol(type+0x678 · CurrentSpeed)`, `+0x678` =
  `min(255, Speed·256/100)` — no GameSpeedBias (Speed 14 → 35 leptons/frame).
  CurrentSpeed moves toward TargetSpeed by 0.1 per frame (`DAT_007E2860`).
  TargetSpeed = 1.0, except: an IsTakingOff plane gets no throttle until it is
  at half its FlightLevel; and when `FUN_004D00A0` says the plane should slow
  (not on an attack run with ammo: landing, returning, moving), TargetSpeed =
  `min(1, dist/SlowdownDistance)` (SlowdownDistance `type+0x2F8`, default 500
  @0x70FF52), and below 0.1 it is 0.1 — or 0 within 0x56 leptons, where the
  current speed is also halved (`DAT_007E0738` = 0.5).
- **Height** (`@0x4CDD84-0x4CDEA1`, IsDropship `type+0xC95` = no for every
  stock plane): climbing `+min(d, 20)` per frame (10 for one COM-queried case,
  not identified); above the target — cruise descent AND landing —
  `−clamp(d/20, 20, 50)` per frame. (The `≤16`/`≤6` landing and `d/20+10`
  capped at 48 rates belong to IsDropship planes only.)
- **Attack passes** — AircraftClass Mission_Attack `FUN_00417FE0` (status
  `+0xBC`; Ammo `+0x2FC`; fired flag `+0x6C8` consumed as one Ammo):
  status 1 Move_To the target → 3. Status 3: a Strafe or Fighter plane goes to
  status 4; any other plane turns to face the target within 0x200 leptons and
  goes to status 4 within 0xF. Status 4 (GetFireError OK): fires Burst shots
  (one Ammo); then a **Strafe** weapon (Is_Strafe `FUN_0041B820`: primary
  projectile ROT ≤ 1 and not Inviso — HornetBomb, ASWBomb) flies statuses
  6..9, each firing ONE more shot when rearmed and in range (no Ammo), then
  back to 3; a **Fighter** (Is_Fighter `FUN_0041B870`, AircraftType
  `+0xE0E`) re-attacks via status 1 while Ammo > 0; any other plane goes to
  status 5 and fires again until Ammo is 0. Ammo 0 → status 10 (return to
  base / Enter_Idle_Mode `FUN_004176F0`).

## §2 Altitude — Z coordinate in leptons, climb/crash rates, BalloonHover hold

- **Where altitude lives.** The unit's height is the **Z component of its world
  coordinate**, `object+0xA4` (leptons), read directly in Process
  (the int at `obj+0xA4` is loaded into a local). `JumpjetHeight` (`type+0xD80`, int
  leptons, ZEP `750`) is the **target cruise altitude** the unit climbs to and
  holds; `[JumpjetControls]CruiseHeight` (Rules `+0x420`, default 500) is the
  global fallback. So `JumpjetHeight=750` is stored/consumed as **leptons of Z**
  (not a screen z), matching the coord system where 256 leptons = 1 cell.
- **Climb / descend rates.** `JumpjetClimb` (`+0xD78`, float, ZEP `6`) is the
  ascend rate and `JumpjetCrash` (`+0xD7C`, float, ZEP `12`) the descend rate,
  both parsed as floats. `JumpJetAccel` (`+0xD84`, float, ZEP `10`) is the
  horizontal acceleration to full `JumpjetSpeed`; `PitchSpeed`/`PitchAngle` set
  the visual nose pitch (separate type fields). (Offsets/types CONFIRMED; the
  exact per-frame altitude-integration instruction toward `JumpjetHeight` was
  not isolated to one line — the target-Z + rate fields are pinned, the
  integrator is PROBABLE.)
- **Takeoff/travel overlap.** Process performs the horizontal translate and the
  altitude/hover update in the **same** tick, so takeoff-climb and horizontal
  travel overlap (the Kirov leaves the factory climbing and moving at once —
  matching the rules comment on `JumpjetClimb`). PROBABLE.
- **BalloonHover "never land".** `BalloonHover=yes` (type bool, read `0x714135`)
  makes the exemplar take off once and **hold cruise** — there is no
  land/descend cycle; the only vertical motion at cruise is the hover bob (§6).
  CONFIRMED that BalloonHover is read; the never-land branch is structural
  (PROBABLE).

- **Landing (wave 8, W8-05-01).** Descending `FUN_0054C280`: on a cell whose
  LandType is Water (2) or Beach (6) -- a high-bridge deck over the river
  keeps the tile's Water -- and not on an Enter mission, the jumpjet claims
  the cell and hovers at full height (never lands, never searches). Elsewhere
  Can_Enter_Cell (SpeedType Hover: no Rock/Wall) and a free spot decide; when
  they refuse, `ILocomotion::Stop_Moving` (`FUN_0054B200`, vtbl 0x7EBD68+0x48)
  re-targets `MapClass::NearByLocation` (`FUN_0056D750`) for the unit's own
  SpeedType/MovementZone and Move_To's there (no cell: the unit takes full
  damage). Move_To (`FUN_0054AEF0`) itself runs NearByLocation(Hover, MZ Fly)
  on the ordered cell. Touch-down on a bridge cell above its floor sets
  OnBridge (+0x8C) and clears IsMoving (+0x4C, read by Is_Moving
  `FUN_0054AB80`). UnitClass Mission_Unload `FUN_0073C560` (UnitClass vtbl
  0x7F4C70+0x23C) waits while the locomotor IsMoving, so a Night Hawk hovering
  over water never unloads; its own Water branch (NearByLocation SpeedType 2,
  SetDestination) serves stationary transports on water.

## §3 Altitude→screen offset + ground-shadow convention

- **Ground shadow — CONFIRMED.** `FUN_00755680` (the shadow renderer named in
  `voxel-render.md`) walks every non-empty voxel column
  (`span_start[col] != -1`) and writes **palette index `1`** to **two
  horizontal pixels** (`DAT_00B2EEE0[addr]` and `[addr+1]`) with **no z-test**,
  at the model's projected (x,y). The shadow branch (when no depth object is
  supplied) is a flat color-1 silhouette at the **un-raised ground projection**
  (the object's XY without the altitude lift) — i.e. the model's outline stamped
  on the ground in the shadow color. This is the exact "plot color 1, no z-test"
  step. CONFIRMED.
- **Altitude→pixel factor — [RE-FALLBACK].** The raised sprite is offset by the
  locomotor's pixel offset (ILocomotion `+0x2C`, added by `VXD_4DAE20` per
  `voxel-render.md` §1), which the jumpjet loco derives from the object's Z. The
  exact Z-leptons→screen-pixels scale in the jumpjet path was **not isolated**.
  Adopt the spec §5 / `voxel-render.md` §6 fallback: the up/z term ≈ **36.74 px
  per cell-of-Z**, i.e. `altitude_px ≈ (altitude/256) · 36.74` → **~107 px at
  `JumpjetHeight=750`**. Marked **[RE-FALLBACK]**.
- **Shadow anchor.** The shadow is drawn at the object's cell/screen anchor
  (the same tactical world→pixel point SHP/voxel units use, `voxel-render.md`
  §1 "blit/anchor"), i.e. directly under the raised model at ground level, in
  color 1. The dark-silhouette-at-ground-projection approximation the spec
  proposes is faithful to this. CONFIRMED (convention), [RE-FALLBACK] only for
  the exact px factor that separates sprite from shadow.

## §4 Air-to-ground attack — fire from cruise, unlimited ammo

- **Fire-from-cruise, not descend-to-attack.** Three independent facts pin
  this: (a) `BalloonHover=yes` — the unit never descends/lands; (b) the weapon
  `[BlimpBomb] OmniFire=yes` sets `WeaponType+0x12B` (`FUN_0077151B`), which
  bypasses the turn-to-face aim gate (`voxel-render.md` §5's
  rotation-complete gate is skipped) — the Kirov fires while cruising over the
  target, no turret, no turn; (c) the delivery point `PrimaryFireFLH=-50,0,-140`
  has **H = −140 leptons (below the hull)**, so the muzzle/bomb origin is under
  the airship and the bomb drops. GetFLH is `FUN_006F3030` (`voxel-render.md`
  §6: F forward / L lateral / H up in leptons, turret space). CONFIRMED.
- **Projectile.** `[BlimpBombP]` is `Vertical=yes` (straight down),
  `Speed=20`, `DetonationAltitude=20000`, `Shadow=no` — it falls on its initial
  down vector. CB-2 may model it as an **instant single-target-cell delivery**
  (the established projectile simplification, `voxel-render.md` §7) — record as
  a deviation.
- **Splash is real (2 cells).** `[BlimpHE] CellSpread=2 PercentAtMax=.5` →
  victim radius `ftol(2·256) = 512` leptons (`combat.md` §7,
  `FUN_00489190`). Unlike the Rhino's 0.3-cell AP warhead, this **hits a ~2-cell
  area** — the single-target simplification does NOT apply; run the spread
  applicator. Damage 250 × `Verses[armor]`, truncate-toward-zero (`combat.md`
  §3).
- **Ammo default = unlimited — CONFIRMED.** `Ammo` is `type+0x684`; the ctor
  `FUN_0070FE90` initializes it to `-1` (it stores `0xFFFFFFFF` into dword index
  `0x1A1` of the object, i.e. byte offset `0x684`), the
  INI loader (`0x713B0D`, `ReadInteger "Ammo"`) uses the current value as the
  default, and `[ZEP]` has no `Ammo=` key — so it stays **-1 = unlimited**
  (rules comment: `Ammo … [-1 means unlimited] (def=-1)`). The "must land to
  rearm" / airfield-dock branch is provably never taken for the Kirov.

## §5 Exemplar-stat formulas (values → engine constants)

(W7 re-derivation, §1a/§1b. All rates per logic frame; 15 frames per second.)

| Rules value | Formula (recovered) | Engine constant |
|-------------|---------------------|-----------------|
| `[ZEP] JumpjetSpeed=5` | raw leptons/frame, `ftol(CurSpeed)` along the facing (`FUN_0054CDA0` @0x54D1FB) | **5 leptons/frame** (≈ 0.29 cells/s) |
| `[SHAD]/[SCHP]/[JUMPJET] JumpjetSpeed=30` | same | 30 leptons/frame (≈ 1.76 cells/s) |
| `JumpJetTurnRate=2` (ZEP) | `FUN_004C9100`: `min(rate,127)<<8` u16/frame (Link `FUN_0054AA60`) | 512 u16/frame |
| `ROT=10` (ZEP) | not used by the jumpjet loco (it is the HoverLocomotion rate source) | — |
| `JumpJetAccel=10` (ZEP), `12` (SHAD) | CurSpeed += Accel/frame up to JumpjetSpeed; −1.5·Accel when above target | ZEP reaches 5 in one frame |
| `JumpjetHeight=750` | leptons of Z (target cruise altitude) | 750 leptons; → **~107 px** via [RE-FALLBACK] §3 |
| `JumpjetClimb=6` / `JumpjetCrash=12` | ascend/descend rate (float leptons/frame) | 6 / 12 |
| `[ORCA] Speed=14` | `type+0x678 = min(255, Speed·256/100)` × CurrentSpeed (`FUN_004CFD40`) | 35 leptons/frame at full throttle |
| `[ORCA] ROT=3` | `FUN_004C95A0`: `ROT<<8` u16/frame | 768 u16/frame |
| `[General] FlightLevel=1500` | climb `min(d,20)`, descend/land `clamp(d/20,20,50)` per frame | 75 frames to cruise |
| `SlowdownDistance` (default 500) | throttle `min(1, dist/500)` when not attacking | — |
| `BlimpBomb Speed=20` | projectile `min(255,clamp·256/100)` (`voxel-render.md` §7) | 51 leptons/frame (or instant-hit, §4) |
| `BlimpHE Damage=250, CellSpread=2` | `combat.md` §3/§7 damage × Verses (trunc), 512-lepton spread | multi-cell splash |

## §6 Determinism + no-RNG flight path

- **No RNG on the flight path — CONFIRMED.** The only stochastic-looking element
  is the hover wobble, and it is a **pure deterministic sine of the frame
  counter**: `FUN_00513B40` (called each Process) computes the bob from
  `sin(DAT_00A8DCEC · k)` (`FUN_004CABD0` = sin, `DAT_00A8DCEC` = current frame)
  scaled by the `[JumpjetControls]` `WobblesPerSecond`/`WobbleDeviation` rules
  globals and the `BalloonHover*` Rules fields (`+0x5CC`/`+0x16B8`/`+0x5F0`).
  The game's R250 generator (`FUN_0065C220`/`FUN_0065C1C0`, `combat.md` §5) is
  **not called** anywhere in Process or the wobble. `JumpjetNoWobbles=yes`
  (ZEP) disables the bob outright, so the Kirov flies a perfectly deterministic
  path.
- **Integration / step ordering.** Airborne units integrate through the same
  per-unit AI path as ground units — the unit's `Locomotor->Process()`
  (`FUN_00514130`) is invoked in the unit step; there is no separate global
  aircraft pass in the original. For the engine, a `step_flight` pass gated by
  `is_aircraft` (mirroring CB-1's `step_cargo`) reproduces this deterministically
  as long as it runs in a fixed iteration order. PROBABLE (the exact call-site
  ordering vs `step_combat` was not isolated; no RNG dependency means order is
  the only determinism lever, and it is under our control).

## Located functions

| Function | Role | How found |
|----------|------|-----------|
| `FUN_006BB020` | game-init locomotor **factory registration**: registers each CLSID (incl. JJ `0x7E8AC0`, Fly `0x7E8A80`) into the class map `0xB0ABF8` | GUID data-xref `0x6BC818`; disasm |
| `0x7F2CA8` / `FUN_006C38C0` / `FUN_006C3990` | JJ **class-factory vtable** / factory ctor / `CreateInstance` (`new 0x78` → ctor) | factory ctor installs `0x7f2ca8`; slot 3 |
| `FUN_00513A40` | **[W7: HoverLocomotionClass, not Jumpjet]** **JumpjetLocomotionClass ctor** — installs ILocomotion vtbl `0x7E9CFC`, IPersistStream `0x7E9DC8`; FacingClass at `+0x30` | CreateInstance callee; disasm of vtable stores |
| `0x7E9CFC` | **JumpjetLocomotionClass ILocomotion vtable** (50 slots) | ctor; slot layout mirrors Drive `0x7E6EB0` |
| `FUN_00514130` | **[W7: HoverLocomotionClass, not Jumpjet]** **JumpjetLocomotionClass::Process** (slot 16, +0x40) — straight-line translate along facing + timed turn + altitude/wobble + exhaust particles | vtable slot; content (atan2, cos/sin translate) |
| `FUN_00513D60` | **Draw_Matrix** (slot 9, +0x24) — facing rotation via `FUN_0055A3C0` + §2 quantizer; tilt matrix for crashing (obj `+0x328/+0x32c` pitch/roll) | vtable slot; content matches Drive Draw_Matrix shape |
| `FUN_005140C0` | tilt-aware render matrix helper (slot 10, +0x28) | vtable slot |
| `FUN_00513AD0` | **[W7: HoverLocomotionClass, not Jumpjet]** **Link_To_Object** (slot 3, +0xC) — FacingClass rate `= ROT(type+0x71C) << 1` | vtable slot; disasm `0x513AF4` reads `+0x71C` |
| `FUN_00516E90` | IPersistStream **GetClassID** (returns guid `0x7E8A40`) | slot 3 of `0x7E9DC8` |
| `FUN_00513B40` | **[W7: HoverLocomotionClass, not Jumpjet]** **BalloonHover wobble/bob** — `sin(frame·k)` + `[JumpjetControls]` globals; no RNG | Process callee; content |
| `FUN_00755680` | **shadow renderer** — plot color 1, 2 px/column, no z-test, ground projection | `voxel-render.md` seed; content confirmed |
| `FUN_006F3030` | **GetFLH** — `PrimaryFireFLH=-50,0,-140` delivery (H below hull → bomb falls) | `voxel-render.md` §6 |
| `FUN_00711510` | **TechnoTypeClass::LoadFromINI** — jumpjet block `+0xD70..+0xD95`, `Ammo`→`+0x684`, `ROT`→`+0x71C`, `Speed`→`+0x678` | INI-key string xrefs |
| `FUN_0070FE90` | **TechnoTypeClass ctor** — jumpjet-field defaults (= `[JumpjetControls]`), `Ammo`=-1 (`+0x684`) | displacement scan; content |
| `FUN_00673E10` | **`[JumpjetControls]` rules loader** — TurnRate/Speed/Climb/CruiseHeight/Accel/Wobbles/Deviation → Rules `+0x40C..+0x438` | `"CruiseHeight"` string xref |
| `FUN_0077151B` | WeaponTypeClass::LoadFromINI — `OmniFire`→`WeaponType+0x12B` | `"OmniFire"` string xref |
| `FUN_005203E0` / `FUN_00520C70` / `FUN_004D55B0` | mission/AI code that special-cases jumpjets via `GetClassID == 0x7E8AC0` | GUID xrefs; content |
| `FUN_00489190` | spread-damage applicator (`CellSpread·256` leptons) — for `BlimpHE` 2-cell splash | `combat.md` §7 |

## Confidence table

| Item | Finding | Evidence | Confidence |
|------|---------|----------|:----------:|
| §1 | **SUPERSEDED (W7, §1a: HoverLocomotion)** Jumpjet loco = `FUN_00513A40` class, ILocomotion vtbl `0x7E9CFC`, Process `FUN_00514130`; CLSID `0x7E8AC0` | factory→ctor→vtable chain; Process content | **CONFIRMED** |
| §1 | straight-line, unpathed flight (no A*/passable) | Process: atan2 + cos/sin translate, no cell walk | **CONFIRMED** |
| §1 | **SUPERSEDED (W7, §1a: HoverLocomotion)** turn rate = **ROT×2** (Link_To_Object), NOT `JumpJetTurnRate` | `FUN_00513AD0` `[type+0x71C]<<1`; no `+0xD70` read in loco | **CONFIRMED** (ROT source); **PROBABLE** (TurnRate vestigial) |
| §1 | flight speed = `JumpjetSpeed` (`+0xD74`), distinct from `Speed` (`+0x678`); `12 leptons/frame` | separate INI fields; obj `GetMaxSpeed` `+0x538`; §5 formula | **PROBABLE** (branch not isolated; ZEP both=5) |
| §1 | airborne unit reserves no ground cell | no cell-mark in Process + `MovementZone=Fly` | **PROBABLE** |
| §2 | altitude = object Z (`+0xA4`) leptons; `JumpjetHeight`=target cruise Z (leptons) | Process reads `+0xA4`; `+0xD80` int | **CONFIRMED** (storage), **PROBABLE** (integrator) |
| §2 | Climb `+0xD78`/Crash `+0xD7C` floats (6/12); Accel `+0xD84`; takeoff+travel overlap | offsets + ctor defaults; Process does both per tick | **CONFIRMED** (offsets), **PROBABLE** (overlap) |
| §2 | BalloonHover never-land hold | `BalloonHover` bool read; hover-only vertical | **PROBABLE** |
| §3 | shadow = color 1, 2 px, no z-test, ground projection | `FUN_00755680` content | **CONFIRMED** |
| §3 | altitude→pixel factor | loco pixel offset (ILocomotion `+0x2C`) not unwound | **[RE-FALLBACK]** `(alt/256)·36.74` |
| §4 | fire-from-cruise (BalloonHover + OmniFire + H=−140), no descend | 3 independent facts; GetFLH `FUN_006F3030` | **CONFIRMED** |
| §4 | `Ammo` default −1 unlimited; no rearm branch | ctor `+0x684=-1`; loader; no ZEP key | **CONFIRMED** |
| §4 | `BlimpHE CellSpread=2` = real ~512-lepton splash | `combat.md` §7 `FUN_00489190` | **CONFIRMED** |
| §6 | no RNG on flight path; wobble = `sin(frame)` | `FUN_00513B40` content; no `FUN_0065C220` call | **CONFIRMED** |
| §6 | `JumpjetNoWobbles=yes` disables bob | bool `+0xD8C`; wobble gate | **CONFIRMED** |
| §6 | deterministic step integration | loco Process in unit step; no RNG | **PROBABLE** |

## Deviations we take

- **[PLAN DIFF — §1 turn rate, RE-GATE STOP].** The plan's `flight_turn` from
  `JumpJetTurnRate` is WRONG for this build: the turn rate is **ROT × 2**
  (Link_To_Object `[type+0x71C]<<1`), and `JumpJetTurnRate` is not consumed by
  the loco. Task 5's `resolve_combat_unit` should derive `flight_turn` from
  **ROT** (`(ROT<<1)·256` u16/frame class, timed shortest-arc like the Drive
  loco). If the plan keeps `JumpJetTurnRate`, record it as a deliberate
  divergence — but ROT is the faithful source. (For ZEP: ROT=10 gives a brisk
  turn; JumpJetTurnRate=2 would be much slower — the difference is visible.)
- **[PLAN DIFF — §1 speed source].** Flight uses **`JumpjetSpeed` (`+0xD74`)**,
  a field distinct from base `Speed`. Task 5 must read `JumpjetSpeed` (fall back
  to `Speed` when absent). Conversion is the shared
  `min(255,clamp·256/100)` → `JumpjetSpeed=5 → 12 leptons/frame` (≈ 0.70
  cells/s at 15 Hz), i.e. the vanilla ≈0.15·speed scale — the same
  known-divergence note as `voxel-render.md` §5 applies if the engine keeps its
  faster infantry-era scale. ZEP has `Speed==JumpjetSpeed==5`, so the demo is
  insensitive; pick `JumpjetSpeed` for fidelity.
- **[DEVIATION — §3 altitude→pixel, [RE-FALLBACK]].** Keep
  `altitude_dy = (altitude/256)·36.74` (≈107 px at 750) and the
  dark-silhouette-in-shadow-color at the ground projection. The exact loco
  pixel-offset formula was not unwound; this is the most likely long-term
  divergence to revisit.
- **[DEVIATION — §4 projectile].** Model `BlimpBombP` as an **instant delivery
  to the target cell** (the `voxel-render.md` §7 precedent) rather than
  simulating the falling `Vertical=yes` bomb — but **keep the `BlimpHE`
  CellSpread=2 splash** (do not collapse it to single-target; run
  `combat.md` §7). Record the instant-hit as a divergence (vanilla bomb takes
  travel time and can miss a moving target — out of scope).
- **[DEVIATION — §3 shadow shape].** A filled dark silhouette/ellipse at the
  ground projection approximates `FUN_00755680`'s exact color-1 per-column plot;
  faithful enough at cell resolution.
- **[DEFERRED — fixed-wing / FlyLocomotionClass].** The `[AircraftTypes]`
  helicopters/planes use `FlyLocomotionClass`, CLSID
  **`{4A582746-9839-11d1-B709-00A024DDAFD1}`** (`DAT_007E8A80`; registered by
  the same factory `FUN_006BB020`). They carry real `Ammo` (`[ORCA] Ammo=1`) and
  `Dock=` airfields → a land-to-rearm cycle. **Mapped, not implemented** this
  phase; the Kirov jumpjet needs neither.
- **[DEFERRED — no jumpjet wobble/pitch tilt].** `JumpjetWobbles`/`PitchSpeed`
  visual bob & nose-pitch and the crash tilt (`Crashable`, Draw_Matrix
  `obj+0x328/32c`) are render-level polish; ZEP `JumpjetNoWobbles=yes` removes
  the bob. Omit for now — determinism/render level, no gameplay effect.
- **[OUT OF SCOPE — air defense].** Ground-to-air targeting and air-to-air are
  out of this phase; the Kirov is treated as attacking ground only, and (per the
  phase scope) is not targeted by ground units.
- **[NOTE — determinism].** No RNG touches the flight path (wobble is
  `sin(frame)`), so the only determinism requirement is a fixed `step_flight`
  iteration order (insert between `step_cargo` and `step_turrets`, gated by
  `is_aircraft`), mirroring CB-1's `step_cargo`.

## W7 crash damage and airfield loss (wave-7 air/naval fix)

- **Fixed-wing impact** [CONFIRMED-RE FlyLoco FUN_004CD520 @0x4CD6E8]: Strength
  0 (shot down) → FireDeathWeapon FUN_0070CAC0 (DeathWeapon, else the primary,
  Damage × DeathWeaponDamageModifier, its warhead's DamageArea) @0x4CD729;
  Strength left (a crash with no airfield / a lost carrier) → ReceiveDamage of
  its own Strength and DamageArea(Strength, C4Warhead = Rules+0xFA8) @0x4CD7C9.
- **Jumpjet impact** [CONFIRMED-RE FUN_00744F30 message 0x117C]: BalloonHover
  (Kirov) → FireDeathWeapon; other Crashable units (Night Hawk, Siege Chopper)
  → FUN_007376C0, Explosion anims only, no damage. The infantry Rocketeer
  (FUN_0052283B) just plays its death sequence.
- **No dock** [CONFIRMED-RE Enter_Idle_Mode FUN_004176F0]: an AirportBound
  (+0xE0D) plane that finds no dock crashes (vtbl+0x3DC FUN_004DEAC0), campaign
  included; only a loaner (+0x3D4: reinforcement transports FUN_0065D770
  @0x65D9E1, paradrop/SW planes 0x65D632..0x65E550) retreats off the map.
- **Paradrop missions** (W8-05-07) [CONFIRMED-RE]: Mission_ParadropApproach
  FUN_004158E0 (vtbl+0x260, returns 3) flies at the target; within
  ParadropRadius (Rules+0x54C) it switches to Mission_ParadropOverfly (0x1B)
  and decrements the pass counter +0x6D3 (5 from the ctor FUN_00413D20
  @0x413D74). Overfly FUN_00415960 (vtbl+0x264, returns 5): no target or no
  passenger -> Mission Retreat; inside the radius and IsInPlayfield
  (FUN_00578020) -> one jump FUN_00415C60; out of the radius -> Approach (0x1A)
  while +0x6D3 > 0, else Retreat. The jump: RemoveFirstPassenger, the point
  128 leptons (DAT_007E1808) off the plane's coords at facing +0x3FFF (count
  left even) or -0x3FFF (odd), angle scale DAT_007E1810 = -2pi/65534; the
  trooper's Can_Enter_Cell (vtbl+0x1AC) must pass and FindInfantrySubposition
  FUN_00481090 must find a free spot, then SpawnParachuted FUN_005F5150
  (OnBridge on a bridge cell); success sets +0x6D3 = 5, failure puts the
  trooper back aboard (retried at the next Overfly tick).
- **Pad repair**: see naval.md W7 (FUN_0044B780, every ReloadRate).
- **RadarInvisible** (+0x22F) hides the unit on the radar of non-allied
  players (FUN_00655690 @0x65583F). **HoverAttack** (+0x390) lifts a landed
  unit that has a target (FUN_004D4CE0 / FUN_004D4F90).
