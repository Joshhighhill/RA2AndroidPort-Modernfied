# Projectile flight (`BulletClass` / `[Projectile]`) — reverse-engineering notes

(Method: `docs/re/workflow.md`; image base `0x00400000`, YR 1.001 `gamemd.exe`,
SHA1 `7BE4E5508655F8AF482AB3FF5965D196E1B6B6C7`. Hunt scripts `proj_1.py … proj_9.py`
off-repo at `D:\ra2-re\scripts\`; decompiles + forced listings at `D:\ra2-re\out\proj\`.
**Content-anchored only.** Every offset below was reached from an INI-key string
reference in *this* binary — the whole `[Projectile]` key pool sits in one tight
block `0x81A004`–`0x81A178` (`Vertical` `0x81A01C`, `FlakScatter` `0x81A0B0`,
`Inaccurate` `0x81A0BC`, `Arcing` `0x81A140`, `Acceleration` `0x81A150`,
`CourseLockDuration` `0x81A160`, `ROT` `0x81A174`, `Arm` `0x81A178`), plus
`Gravity` `0x839344` and `BallisticScatter` `0x839D98` in the Rules key pool, and
the Rules static base `0x886160`. Published YRpp addresses were used only as a
*naming* map — `BulletTypeClass.h` / `BulletClass.h` field order was
independently corroborated at **34 field offsets** in this binary before any of
it was believed.)

Calibration authority for projectile flight, which the clone **does not have at
all** today: `engine/combat.cpp::try_fire` applies damage the instant the ROF
timer expires. This document is the prerequisite for fixing the carry-forward
recorded in `combat.md` §7, `voxel-render.md` §7 ("may be modeled as instant-hit
for this phase"), `aircraft.md` §4 and `naval.md` §4/§6.

**It also overturns two things those documents recorded.** See §0.

---

## §0 — Two overturned assumptions (read this first)

1. **`[General] Gravity=` does not exist. `Gravity` lives in `[AudioVisual]`.**
   The brief (and every wiki summary) says `[General] Gravity`. In the real
   `rulesmd.ini` on this install, line 756, `Gravity=6` is inside
   **`[AudioVisual]`**, and the binary agrees: the read site is
   `FUN_00668C20` @ `0x66AE11` with section pointer
   `PTR_s_AudioVisual_007EFC7C`, storing to `Rules+0x16B8`. Reading it from
   `[General]` returns nothing and you silently get the absent-key default.
   **CONFIRMED.**

2. **A non-homing projectile's launch speed is NOT the weapon's `Speed=` key.**
   `voxel-render.md` §7 records "Weapon Speed … `Speed=40 → 102 leptons/frame`
   … Cannon-class projectiles fly a straight line at that rate." Both halves are
   wrong for `[Cannon]`:

   - `WeaponTypeClass::GetSpeed(distance)` = **`FUN_00771E90`**:

     ```c
     // ECX = WeaponTypeClass*, param = distance in leptons
     Projectile = this->Projectile;               // WeaponType+0xA0
     if (Projectile && Projectile->ROT == 0) {     // BulletType+0x2DC
         g = Rules->Gravity;                       // Rules+0x16B8
         if (Projectile->Floater) g = Gravity*0.5; // FUN_0048AC00
         return FUN_0048AAA0(distance, g);         // <- computed, ignores Speed=
     }
     return this->Speed;                           // WeaponType+0xA8 (homing only)
     ```

     and `FUN_0048AAA0` is 41 bytes of pure arithmetic:

     ```
     0x48AAA8  FILD dword ptr [ESP+8]        ; (double)distance
     0x48AAAC  FMUL qword ptr [ESP+0x10]     ; * Gravity
     0x48AAB0  FMUL qword ptr [0x007E4190]   ; * 1.2      <- DAT_007E4190 = 1.2
     0x48AAB9  CALL 0x004CAB60               ; sqrt
     0x48AABE  CALL 0x007C4D00               ; ftol
     ```

     > **`launch_speed = ftol( sqrt( distance × Gravity × 1.2 ) )` leptons/frame,
     > recomputed per shot, whenever `Projectile->ROT == 0`.**

     `Speed=` is used verbatim **only** for `ROT > 0` (homing) and, separately,
     as the *terminal* speed of a `Vertical` bomb (§2.4). Its 102-leptons/frame
     coincidence for the Rhino is exactly that — a coincidence:
     `ftol(sqrt(1472 × 6 × 1.2)) = ftol(102.95) = 102`, the same number the ini
     path produces, at max range only. At 3 cells the real value is **74**, not
     102. **CONFIRMED** (both functions decompiled and disassembled; constant read
     from the image).

   - `[Cannon]` is `Arcing=true`, so it does not fly a straight line at all
     (§2.2). The straight-line claim was inferred from "no `Arcing`/`ROT` in the
     rules" — but the real `[Cannon]` section *does* carry `Arcing=true`.

A third, softer correction: `naval.md` §6 says `FlakTrackGun`'s shell scatters
"via `TechnoClass::Fire`". That is right, but the *sibling* AA shell
`[FlakProj]` scatters in a **different** function (`BulletClass::MoveTo`) by a
**different** formula — see §5.

---

## §1 — Located functions

| Function | Role | How found |
|----------|------|-----------|
| `FUN_0046BE00` | **`BulletTypeClass::ReadINI`** — every `[Projectile]` key → field offset | forced `createFunction` in the `0x46BCFE–0x46BF4C` analysis gap; all 30 key strings resolve |
| `FUN_0046BF4C` | tail half of the same (Ghidra split it) | string xrefs to `Inaccurate`/`FlakScatter`/`Vertical` |
| `FUN_004664B0` | **`BulletClass::Construct(type,target,owner,damage,WH,speed,bright)`** | YRpp `0x4664C0` (drift −0x10), verified by content: writes `+0x10C/0x110/0x128/0xE0/0x6C/0xAC/0xB0`, `DamageMultiplier(+0x150)=0x100` |
| `FUN_004666D0` | **`BulletClass::Update`** — the whole per-frame integrator, 6422 bytes | only bullet-module reader of `Rules+0x16B8` (`0x4671AB`) |
| `FUN_00468660` | **`BulletClass::MoveTo(coord, velocity)`** — vtable **+0x1F0**; the launch commit | copies 6 dwords (3 doubles) into `+0xE8`; reads `Rules+0x1734` |
| `FUN_00468420` | **`BulletClass::LoseTarget()`** | YRpp `0x468430`; body matches "retarget to the cell" |
| `FUN_00468BA0` | **`BulletClass::IsBlocked(coord)`** — terrain/altitude/AA termination test | called from `Update`'s tail with the new coords |
| `FUN_00468D70` | **`BulletClass::Explode(bool)`** — snap-to-target + `Cluster` loop | YRpp `0x468D80` |
| `FUN_004690A0` | **`BulletClass::Detonate(coord)`** — calls the area-damage applicator | YRpp `0x4690B0`; contains the `FUN_00489190` call |
| `FUN_0046A230` | `BulletClass::Shrapnel()` | called when `Type->ShrapnelWeapon != 0` |
| `FUN_006FD210` | **`TechnoClass::Fire`** — inaccuracy draw, launch velocity, `MoveTo` | only non-bullet caller of `Construct`; reads `Rules+0x1734` and `+0x16B8` |
| `FUN_00771E90` | **`WeaponTypeClass::GetSpeed(distance)`** (§0.2) | `Rules+0x16B8` reader; called at `Fire:0x6FE0xx` before `CreateBullet` |
| `FUN_0048AAA0` | **arcing launch-speed solver** `sqrt(d·g·1.2)` | callee of the above |
| `FUN_0048A7E0` / `FUN_0048A8E0` | **arcing launch-*angle* solver** (wrapper + core) | callee at `Fire:0x6FE1E1`, guarded by `Type->Arcing` |
| `FUN_0070C9C0` | "use the lofted root" predicate (fires the high arc over a blocking obstacle) | callee at `Fire:0x6FE1D6`, its result becomes the solver's root selector |
| `FUN_0048AC00` | `Floater` gravity = `Rules->Gravity × 0.5` (18 bytes) | `Rules+0x16B8` reader, called under `Type->Floater` |
| `FUN_0046B880` | `CoordStruct += CoordStruct` (33 bytes) — the position integrator | called from `Update`'s arcing branch |
| `FUN_004E1040` / `FUN_004E1100` | **`FuseClass::Arm` / `FuseClass::Update`** — the proximity fuse | called from `MoveTo` / `Update` tail |
| `FUN_005B1B20` | homing steering (facing-space turn toward the target, 2101 bytes) | called from `Update`'s `ROT>=1` branch |
| `FUN_0065C220` | **`Randomizer::Random(lo,hi)`** — 250-word lagged-XOR table | already pinned in `naval.md`; instance is `Scenario+0x218` |
| `FUN_00489190` | area-damage applicator (`CellSpread`) — the clone already mirrors this | `voxel-render.md` §7; re-confirmed as `Detonate`'s callee |
| `FUN_00474550` / `FUN_00474740` | ini readers: cells→leptons (`×256.0` @ `0x7E0710`), and `Speed` (`min(255, clamp(0,100)·256/100)`) | forced listings `dis_inicell_474550.txt`, `dis_inispeed_474740.txt` |
| `FUN_004CABD0` / `FUN_004CAC20` | **table** sin / cos (index `= x · 8192/π`, `& 0x1FFF`) — deterministic, not libm | forced listing `dis_sin_4CABD0.txt` |

**Numeric constants read out of the image** (`proj3/5/6/9` logs):

| Address | Value | Use |
|---|---|---|
| `0x7E4190` | **1.2** | arcing launch-speed margin (§0.2) |
| `0x7E0710` | **256.0** | leptons per cell, ini parse |
| `0x7E1810` | **−2π/65534** | 16-bit facing → radians |
| `0x7E1818` | **−65534/2π** | radians → 16-bit facing |
| `0x7E1820` | **π/2** | pitch datum |
| `0x7E2570` | **1/2³¹** | `Random(0,0x7FFFFFFE)` → `[0,1)` |
| `0x7E38E0` | **8.0** | minimum speed; below it a non-homing bullet detonates |
| `0x7E0738` | **0.5** | homing arrival test `dist ≤ 0.5·speed` |
| `0x7E34A8` | **10.0** | "came to rest" speed threshold |
| `0x7E0728` | **60.0**, `0x7E38E8` **59/60** | "no longer closing" smoothing/give-up |
| `0x7E38F8` | **1/15**, `0x7E2CC0` **2π** | homing wobble phase (15-frame period) |
| `0x8213C0` | **8192/π** (float) | sin/cos table index scale |
| `0x3FE921D9F4D37C12` | **π/4** | hard-coded arc angle when the target is *above* the muzzle |

---

## §2 — Ground truth: the real `rulesmd.ini` on this install

Extracted with `build/ra2extract.exe "D:\Red Alert 2\ra2md.mix" --raw
localmd.mix/rulesmd.ini` (742,958 bytes — the nested-fallback chain
`engine/main.cpp::load_ini_from_ra2md` documents). Comments are verbatim.

```ini
[AudioVisual]
Gravity=6               ; gravity constant for ballistic projectiles

[CombatDamage]
BallisticScatter=1.0 ; maximum scatter distance (cells) for inaccurate ballistic projectiles (Note: Flak Cannon uses this)
                     ; -> FUN_00474550 => ftol(1.0 * 256.0) = 256 leptons, Rules+0x1734
                     ; HomingScatter (Rules+0x1730) is parsed but ABSENT from this ini.

[General]
MissileSpeedVar=.25     ; speed flucuation percentage that guided missiles have
MissileROTVar=.25       ; rate of turn fluctuation percentage that guided missiles have
MissileSafetyAltitude=750 ; altitude a missile fired at an air target that dies will fly to before detonating
```

The `[Projectile]` sections referenced by every weapon the clone currently
fires (all keys present are shown; absent keys take the type default):

```ini
[InvisibleLow]      ; GI M60, Conscript M1Carbine, GI-deployed Para
Inviso=yes
Image=none
SubjectToCliffs=yes
SubjectToElevation=yes
SubjectToWalls=yes

[Cannon]            ; Rhino 120mm, Grizzly 105mm      "; straight high-speed ballistic shot"
Image=120MM
Arcing=true
SubjectToCliffs=yes
SubjectToElevation=yes
SubjectToWalls=yes

[BlimpBombP]        ; Kirov BlimpBomb
Image=ZBOMB
Arm=10
Shadow=no
Acceleration=1
Vertical=yes ;can't turn or do much of anything.  Just stays on the vector of its initial shooting (up or down)
DetonationAltitude=20000 ; Needs this to prevent premature explosionation since uses same system as nuke

[FlakTProj]         ; Flak Track FlakTrackGun (anti-surface)
Image=120MM
Arcing=true
Inviso=no
AA=no
AG=yes
Shadow=no
Inaccurate=yes  ; Bullets do not snap onto targets when "close enough".
FlakScatter=yes ; This weapon scatters its shots.
SubjectToCliffs=no
SubjectToElevation=yes
SubjectToWalls=yes

[FlakProj]          ; Flak Track FlakTrackAAGun (anti-air), shared with Flak Cannon
Image=none
Inviso=yes
AA=yes
AG=no
Shadow=no
Ranged=yes      ; Not homing, but ranged -- check fuse, explode if near target coords
Inaccurate=yes  ; Bullets do not snap onto targets when "close enough".
FlakScatter=yes ; This weapon scatters its shots.
SubjectToCliffs=no
SubjectToElevation=yes
SubjectToWalls=no

[Ballistic]         ; Destroyer 155mm     "; arcing ballistic projectile"
Image=120MM
Arcing=true
SubjectToCliffs=no
SubjectToElevation=yes
SubjectToWalls=no

[InvisibleHigh]     ; V3Launcher, DredLauncher  (both Spawner=yes -- see §7)
Inviso=yes
Image=none
SubjectToElevation=yes

[Torpedo]           ; Typhoon Sub SubTorpedo   (homing)
Arm=2
Shadow=no
Ranged=yes
Image=SUBT
ROT=12 ;4
AA=no
AG=yes
Level=yes

[MedusaProjectile]  ; Aegis Cruiser Medusa     (homing, course-locked)
Arm=1
High=yes
Shadow=no
AA=yes
AG=no
Image=MEDUSA
CourseLockDuration=15
ROT=20
Scalable=yes
SubjectToCliffs=no
SubjectToElevation=no
SubjectToWalls=no
```

Weapons (`Speed` is a **weapon** key, never a `[Projectile]` key):

```ini
[M60]          Damage=15  ROF=20  Range=4     Projectile=InvisibleLow Speed=100 Warhead=SA
[M1Carbine]    Damage=15  ROF=25  Range=4     Projectile=InvisibleLow Speed=100 Warhead=SA
[Para]         Damage=25  ROF=15  Range=5     Projectile=InvisibleLow Speed=100 Warhead=SSA
[120mm]        Damage=90  ROF=65  Range=5.75  Projectile=Cannon       Speed=40  Warhead=AP
[105mm]        Damage=65  ROF=60  Range=5     Projectile=Cannon       Speed=40  Warhead=AP
[BlimpBomb]    Damage=250 ROF=50  Range=1.5   Projectile=BlimpBombP   Speed=20  Warhead=BlimpHE  CellRangefinding=yes OmniFire=yes
[FlakTrackGun] Damage=25  ROF=25  Range=5     Projectile=FlakTProj    Speed=50  Warhead=FlakTWH
[FlakTrackAAGun] Damage=35 ROF=25 Range=10    Projectile=FlakProj     Speed=100 Warhead=FlakWH
[155mm]        Damage=60  ROF=110 Range=8     Projectile=Ballistic    Speed=10  Warhead=ARTYHE  MinimumRange=0 Lobber=no
[SubTorpedo]   Damage=100 ROF=120 Range=7     Projectile=Torpedo      Speed=25  Warhead=APSplash
[Medusa]       Damage=100 ROF=15  Range=12    Projectile=MedusaProjectile Speed=120 Warhead=SAMWH OmniFire=yes
[V3Launcher]   Damage=1   ROF=150 Range=18    Projectile=InvisibleHigh Speed=10 Spawner=yes MinimumRange=5
```

`artmd.ini` FLH (§4): `[HTNK] PrimaryFireFLH=150,0,100`, `[GTNK] 150,0,100`,
`[HTK] 65,0,220` (`TurretOffset=-80`), `[ZEP] -50,0,-140`, `[GI] 80,0,105`
(`SecondaryFireFLH=80,0,90`), `[CONS] 60,0,100`, `[DEST] 280,0,120`,
`[V3] -160,0,75`.

---

## §3 — `BulletTypeClass` layout — **CONFIRMED**

`FUN_0046BE00` reads every key into the offsets below. YRpp's declaration order
in `BulletTypeClass.h` matches this binary field-for-field with **no drift** —
34 offsets, all corroborated by their own key string. This is the strongest
class-layout confirmation in the project after `MixFileClass`.

| Off | Key | Type | Off | Key | Type |
|-----|-----|------|-----|-----|------|
| `+0x294` | `Airburst` | bool | `+0x2A7` | `Bouncy` | bool |
| `+0x295` | `Floater` | bool | `+0x2A8` | `AnimPalette` | bool |
| `+0x296` | `SubjectToCliffs` | bool | `+0x2A9` | `FirersPalette` | bool |
| `+0x297` | `SubjectToElevation` | bool | `+0x2AC` | `Cluster` | int |
| `+0x298` | `SubjectToWalls` | bool | `+0x2B0` | `AirburstWeapon` | ptr |
| `+0x299` | `VeryHigh` | bool | `+0x2B4` | `ShrapnelWeapon` | ptr |
| `+0x29A` | `Shadow` | bool | `+0x2B8` | `ShrapnelCount` | int |
| **`+0x29B`** | **`Arcing`** | bool | **`+0x2BC`** | **`DetonationAltitude`** | int |
| `+0x29C` | `Dropping` | bool | **`+0x2C0`** | **`Vertical`** | bool |
| `+0x29D` | `Level` | bool | `+0x2C8` | `Elasticity` | **double** |
| **`+0x29E`** | **`Inviso`** | bool | **`+0x2D0`** | **`Acceleration`** | int |
| **`+0x29F`** | **`Proximity`** | bool | `+0x2D4` | `Color` | ptr |
| **`+0x2A0`** | **`Ranged`** | bool | `+0x2D8` | `Trailer` (art) | ptr |
| `+0x2A1` | `NoRotate` (= `!Rotates`) | bool | **`+0x2DC`** | **`ROT`** | int |
| **`+0x2A2`** | **`Inaccurate`** | bool | **`+0x2E0`** | **`CourseLockDuration`** | int |
| **`+0x2A3`** | **`FlakScatter`** | bool | `+0x2E4/0x2E8` | `SpawnDelay` / scaled | int |
| `+0x2A4` | `AA` | bool | `+0x2EC` | `Scalable` | bool |
| `+0x2A5` | `AG` | bool | **`+0x2F0`** | **`Arm`** | int |
| `+0x2A6` | `Degenerates` | bool | `+0x2F4/5/6/7` | `AnimLow/High/Rate/Flat` | byte |

Note: **`AN` and `AS` are commented out in this rules file and have no reader in
this build** — only `AA` and `AG` are parsed. `Level` and `VeryHigh` exist and
are read by the homing solver. `Proximity` is parsed but **never read** by
`Update`, `MoveTo`, `Explode`, `Detonate` or `IsBlocked` — the actual proximity
behaviour is driven by `Ranged` + `Arm` (§5). **PROBABLE** (`Proximity` may be
read by an AI/targeting path outside the bullet module; it was not found in the
bullet module, and `Ranged` alone reproduces the documented behaviour).

`BulletClass` instance layout is likewise YRpp-exact and corroborated by
`Construct`: `+0x6C` damage (aliases `ObjectClass::Health`), `+0x9C..0xA7`
`Location`, `+0xAC` `Type`, `+0xB0` `Owner`, `+0xE0` `Bright`,
**`+0xE8/0xF0/0xF8` `Velocity` (3 `double`)**, `+0x105` `CourseLock`,
`+0x108` `CourseLockCounter`, `+0x10C` `Target`, **`+0x110` `Speed`**,
`+0x114` `InheritedColor`, `+0x118`/`+0x120` closing-rate counter/accumulator,
`+0x128` `WH`, `+0x12C/0x12D` anim frame/counter, `+0x130` `WeaponType`,
`+0x134` `SourceCoords`, `+0x140` `TargetCoords`, `+0x14C` `LastMapCoords`,
`+0x150` `DamageMultiplier` (initialised to **`0x100` = 256**), `+0x154`
`NextAnim`, `+0x158` `SpawnNextAnim`.

**Units.** Everything is in **leptons**, 256 leptons per cell — proved twice in
`MoveTo` alone: `LastMapCoords = (X + (X>>31 & 0xFF)) >> 8` per axis (a
round-toward-zero `/256`), and the `×256.0` in the cells→leptons ini reader.
Velocities are **leptons per logic frame** (15 fps, `combat.md` §5).

---

## §4 — Launch (`TechnoClass::Fire`, `FUN_006FD210`) — **CONFIRMED** except the angle algebra

Order of operations, in code order:

1. **Muzzle point.** `FLH` is resolved through `TechnoClass` vtable `+0xB0` into
   `SourceCoords`; `voxel-render.md` §6 already pins the FLH → world transform
   (turret rotation + `TurretOffset` + ramp tilt). No new work needed here.
   **CONFIRMED (by reference).**

2. **Speed.** `speed = WeaponType->GetSpeed(distance)` (§0.2) — computed for
   `ROT==0`, the ini `Speed` for `ROT>0`.

3. **Speed cap.** `if (dist3D/2 < speed) speed = dist3D/2;` — a projectile can
   never cross more than half the remaining distance in its first frame, so
   every shot takes **at least 2 frames**. **CONFIRMED.**

4. **Inaccuracy** (§5) perturbs the working delta.

5. **Facing.** `if (Type->ROT == 0 && !Type->Dropping)` the yaw comes from
   `ArcTan(delta)`; otherwise the firer's own `PrimaryFacing` is used (a homing
   missile leaves the tube pointing where the launcher points). **CONFIRMED.**

6. **Base velocity.** `V = (100.0, 0, 0)`, then scaled by `speed/100.0` → a
   vector of magnitude `speed` along +X, then rotated by the yaw:
   `V.x = sin(θ)·|Vxy|`, `V.y = −cos(θ)·|Vxy|`.

7. **Pitch.**
   - **Non-arcing:** pitch datum `0x3FFF` (level). If `|Δz| > 200` leptons the
     shot is elevated: `pitch = ArcTan2(|Δz| − 20.0, horiz)` (the `20.0` is
     dropped when the target is a building whose height is within tolerance).
     **CONFIRMED.**
   - **Arcing:** `g = Rules->Gravity` (`×0.5` if `Floater`), `horiz =
     ftol(sqrt(Δx²+Δy²))`, then
     `lofted = FUN_0070C9C0(...)` (true when something between firer and target
     forces the high arc) and
     `ok = FUN_0048A7E0(lofted, speed, horiz, Δz, g, &pitch)`.
     If `ok == false` **the bullet is destroyed and the shot does not happen.**
     **CONFIRMED.**

8. **Apply pitch:** un-rotate any existing pitch (`V.xy /= cos(cur)`), then
   `V.x *= cos(φ)`, `V.y *= cos(φ)`, `V.z = sin(φ)·|V|`.

9. **`bullet->MoveTo(SourceCoords, V)`** — vtable `+0x1F0`.

### §4.1 The arcing angle solver — **PROBABLE (inputs + shape), [RE-FALLBACK] (exact algebra)**

`FUN_0048A7E0` is a thin wrapper: it calls the core `FUN_0048A8E0` twice, once
with the caller's root-selector and once with `horiz+1` and root `0`, and uses
the comparison of the two results to decide the **sign** of the returned pitch
(a numeric derivative test). The result is converted to the 16-bit pitch units
the velocity code wants by `pitch = ftol((θ − π/2) × (−65534/2π))`.

Two things in the core are exact and load-bearing:

- **`if (Δz > 0) return π/4;`** — literally `0x3FE921D9F4D37C12` stored to the
  out-parameter, no computation. **If the target is higher than the muzzle at
  all, the launch angle is a flat 45°.** **CONFIRMED.**
- The discriminant is tested twice (`< 0` and `== 0`) and both failures return
  `false` → the shot is refused (step 7 above).

The closed form itself was **not unwound within budget**. The FPU trace, which
is transcribed verbatim in `D:\ra2-re\out\proj\dis_arccore_48A8E0.txt`, is:

```
A     = v² − g·z
D     = <discriminant built from v², g, d²>        ; 0x48A958..0x48A974
denom = 2·( X²/d² + 1 )                            ; 0x48A97C..0x48A98C
t     = (A ± sqrt(D)) / denom                      ; ± selected by CL
if (t < 0) fail
θ     = FUN_004CACD0( sqrt(t) / v )                ; inverse trig
```

i.e. `sqrt(t)` is the **vertical** component of the launch velocity and
`sqrt(t)/v` is its sine. `[RE-FALLBACK]` for the exact `D`/`X` terms.

**What the clone can rely on without it.** Because the launch speed is
`v = sqrt(1.2·d·g)`, the textbook flat-ground solution gives
`sin(2θ) = d·g/v² = 1/1.2 = 0.8333` — **independent of distance** — so
`θ ≈ 28.22°` (low root) or `61.78°` (lofted root) for *every* flat-ground arcing
shot in the game, and the flight time is

```
T  =  2·v·sin θ / g  =  0.4226 · sqrt(d_leptons)  frames        (Gravity = 6)
   ≈  6.76 · sqrt(d_cells) frames  ≈  0.45 · sqrt(d_cells) seconds
```

That is a **PROBABLE** derivation (it assumes the textbook solver, which is the
part labelled `[RE-FALLBACK]`), but it is self-consistent with the two
`CONFIRMED` halves and reproduces `Speed=40 → 102 lept/frame` at the Rhino's max
range exactly.

---

## §5 — Inaccuracy: `Inaccurate`, `FlakScatter` — **it is an RNG draw, from the sync-critical RNG**

**This is the answer to the RNG question, and it is unambiguous.** There are
**three** independent scatter mechanisms and all three call
`FUN_0065C220(Randomizer*, lo, hi)`. The `Randomizer*` in every case is
`[0x00A8A198] + 0x218`, and YRpp's `ScenarioClass.h` annotates
`Randomizer Random; //218` — this is **`ScenarioClass::Instance->Random`, the
lockstep-synchronised game RNG**, not a cosmetic one. Verified in the forced
listing `dis_moveto_scatter.txt` (`LEA ECX,[EDX+0x218]` immediately before both
calls). **CONFIRMED.**

Uniform random angle, used identically by all three:

```
r     = Random(0, 0x7FFFFFFE)
ang16 = ftol( (r / 2^31) · 2π − π/2 ) · (−65534/2π)     ; -> 16-bit facing
θ     = ((short)ang16 − 0x3FFF) · (−2π/65534)           ; -> radians
```

### 5.1 `Inaccurate && Arcing && !(FlakScatter && !Inviso)` — in `Fire`

```
r = Random(BallisticScatter/2, BallisticScatter)        ; 128..256 leptons
Δ += ( cos θ · r , −sin θ · r )
```

### 5.2 `Inaccurate && Arcing && FlakScatter && !Inviso` — in `Fire` — this is `[FlakTProj]`

```
dist  = sqrt(Δx² + Δy² + Δz²)
r     = Random(0, BallisticScatter) · ftol(dist) / WeaponRange
Δ    += ( cos θ · r , −sin θ · r )
```

i.e. **the miss grows linearly with how far the target is, reaching up to a full
cell at maximum range and nothing at point-blank.** `BallisticScatter=1.0` →
`ftol(1.0 × 256.0) = 256` leptons.

### 5.3 `FlakScatter && Inviso` — in `MoveTo` — this is `[FlakProj]`

The same distance-scaled formula, but applied to the *impact coordinate* rather
than the firing delta, and using `WeaponType->Range` (`WeaponType+0xB4`) as the
divisor:

```
r    = Random(0, 2 × BallisticScatter) · ftol(dist) / WeaponType->Range
impact = impact + ( cos θ · r , sin θ · r )
```

Note the `<< 1`: the AA shell scatters over **twice** the radius. Forced listing
`dis_moveto_scatter.txt`, `SHL EAX,0x1` at `0x4687B0`.

### 5.4 What `Inaccurate` *itself* does — **CONFIRMED, and it is not what the name suggests**

`Inaccurate` is not a scatter magnitude. It is a **"do not snap"** flag, checked
in exactly three places, all of which are *snap-to-target* corrections that it
suppresses:

- `Update` collision (`FUN_004666D0` @ `0x4679E9`): on touching a victim within
  **127 leptons**, a normal bullet **teleports its detonation point onto the
  victim's exact coordinates**; an `Inaccurate` bullet detonates where it is.
- `Explode` (`FUN_00468D70` @ `0x468D8C`): within **32 leptons** of the target,
  a normal bullet snaps onto it. Suppressed by `Inaccurate` (and by `Airburst`).
- `Update` tail (`0x467E00`): the fuse-triggered "close enough, call it a hit"
  re-aim, suppressed by `Inaccurate` and `Airburst`.

The rules comment is literally right: *"Bullets do not snap onto targets when
'close enough'."* A weapon that is `Inaccurate` but has no scatter source still
behaves differently — it can physically miss.

### 5.5 Also RNG-bearing, for completeness

- `Cluster > 0` (`Explode`): each sub-detonation offsets by
  `Random(0x100, 0x200)` leptons. **CONFIRMED.**
- `[General] MissileSpeedVar=.25` / `MissileROTVar=.25` are parsed
  (`FUN_00668C20`) and described as per-missile fluctuation. Their consumer was
  **not located** in the bullet module. `[RE-FALLBACK]` — assume homing missiles
  additionally jitter ±25% on speed and ROT until this is pinned.

---

## §6 — Per-frame motion (`BulletClass::Update`, `FUN_004666D0`)

The very first branch splits the entire function:

```c
if (Type->ROT < 1) {        /* ballistic / vertical / inviso        */ }
else                        { /* homing                             */ }
```

### 6.1 Non-homing, `!Vertical` — the ballistic integrator — **CONFIRMED**

```c
if (|Velocity| < 8.0) detonate = true;          // DAT_007E38E0

g = (double)Rules->Gravity;                     // Rules+0x16B8   (= 6)
if (Type->Floater) g = Rules->Gravity * 0.5;    // FUN_0048AC00

Velocity.Z -= g;                                // 0x4673F2: FLD; FSUB ST0,ST1; FSTP
Position   += Velocity;                          // FUN_0046B880 (component-wise +=)
```

The forced listing `dis_arcgrav_4673F2.txt` shows exactly that FSUB and the
`CALL 0x0046B880` two instructions later. **Semi-implicit Euler, gravity applied
before the position step, one frame = one integration step, no `dt`.**

Ground/terrain: the new Z is compared against `MapClass::GetCellFloorHeight`
(`FUN_00577AB0`, plus a bridge offset when the cell carries flag `0x100`); on or
below ground → detonate. `Bouncy` + `Elasticity` (`+0x2C8`) drive a reflection
matrix in the same block (out of scope for the clone; **PROBABLE**).

### 6.2 Non-homing, `Vertical` — the falling bomb (`[BlimpBombP]`) — **CONFIRMED**

```c
s = ftol(|Velocity|);
if (s < this->Speed) {                     // bullet+0x110 = weapon Speed (leptons/frame)
    s += Type->Acceleration;               // BulletType+0x2D0
    Velocity = normalize(Velocity) * s;    // direction preserved exactly
}
prev = Position;  Position += Velocity;

if (Position.Z > Type->DetonationAltitude) detonate;   // ascending nukes
else if (this->GetHeight() < 0)            detonate;   // reached the ground
```

`GetHeight()` is vtable `+0x1C8` (height above the ground under the bullet). The
direction is never re-derived — the rules comment *"Just stays on the vector of
its initial shooting (up or down)"* is exactly the code.

**Kirov worked example.** `[ZEP] JumpjetHeight=750`, `[BlimpBombP]
Acceleration=1`, `[BlimpBomb] Speed=20` → terminal `ftol(20·256/100)=51`
leptons/frame; launch speed is 1 (§6.5). Fall distance after `n` frames is
`n(n+1)/2`, so `n = 39` frames to clear 750 leptons ⇒ **≈ 2.6 s of flight**
before the Kirov's bomb does anything. That is the single largest gameplay error
in the clone's current instant model.

### 6.3 `Inviso` — the *non*-flying projectile — **CONFIRMED, and it matters**

Every `Inviso=yes` projectile is special-cased in **`MoveTo`**, at launch:

```c
if (Type->Inviso) {
    hit = FUN_00587AD0(source, where, &targetcoords, house);   // cliff/wall line trace
    if (hit == (0,0,0)) {                                       // unobstructed
        SetLocation(targetcoords);       // teleport straight to the impact point
    } else {
        SetLocation(hit);                // stop at the blocking cliff/wall
    }
    this->Speed = 0;
    Velocity   *= (0.0 / |Velocity|);    // -> exactly (0,0,0)
}
```

On the next `Update` the `|V| < 8.0` test and the "already in the target cell"
test both fire, and the bullet detonates. So an `Inviso` projectile has a flight
time of **exactly one logic frame (~67 ms)**.

> **All infantry small arms (`[InvisibleLow]`) and the Flak Track's AA gun
> (`[FlakProj]`) are `Inviso`.** The clone's current instant delivery is
> therefore *already faithful to within one frame* for every infantry weapon.
> The two behaviours it is missing for them are (a) the cliff/wall line trace,
> and (b) `[FlakProj]`'s scatter (§5.3).

### 6.4 Homing (`ROT >= 1`) — **CONFIRMED** (wave 4; implemented)

> **Wave-4 update (`docs/gap/wave4/04-projectiles.md` §1.1, implemented in
> `engine/projectile.cpp` `integrate_homing`, `ProjKind::Homing`).** The P2
> "ROT>0 -> Instant" cut is retired: every non-Inviso, non-Vertical bullet with
> `ROT>=1` flies. The weave is `turn = ftol((V·sin(ph) + V + 1)·ROT)` with
> `V = [General] MissileROTVar` (Rules+0x598) and `ph = ((id+frame)%15)/15·2π`
> (not a bare cosine), ×1.5 inside 256 leptons (0x7E38F0), byte-truncated and
> `<< 8` into 1/256-circle units. The give-up smoothing factor is
> `0x7E38E8 = 0.98333` (59/60) with the threshold `0x7E0728 = 60.0`.
> `Acceleration` absent = **3** (the BulletTypeClass constructor stores 3 at
> `+0x2D0` and `+0x2E4`, gamemd `0x46BB2D`). `FUN_005B1B20`: yaw turned at the
> target by the limit; pitch straight at an air target, else the
> terrain-following cruise (`min(dist_cells,5)` levels, 10 for
> Airburst/VeryHigh, ±18-lepton nudge beyond 20, climb `0x2000` / dive `0x4800`
> / level `0x4000` at limit/2); `Level=yes` skips pitch. Remaining
> `[RE-FALLBACK]`s: the `Arm` fuse and `Proximity` are not modelled; a Level
> bullet skips the height<1 test (the clone's water line is z = 0); a defence
> weapon with no art FLH launches at H = 100.

The pre-wave-4 reconstruction follows for reference:

```c
// course lock: hold the launch heading for CourseLockDuration frames
if (Type->CourseLockDuration == 0) { if (Speed > 39 || Speed <= |V| + 0.5) CourseLock = 0; }
else if (++CourseLockCounter >= Type->CourseLockDuration) CourseLock = 0;

a = Type->Acceleration;
if (CourseLock && CourseLockDuration == 0) a = ((frame & 1) == 0);   // 0/1 alternating
if (|V| < Speed) |V| = min(Speed, |V| + a);
else if (|V| > Speed) |V| = max(0, |V| − a/2);
Velocity = normalize(Velocity) * |V|;

tgt = Target ? Target->GetCoords() : (0,0,0);
turn = ftol( Type->ROT · cos( ((id + frame) % 15) · 2π/15 ) );   // 15-frame weave
if (CourseLock) turn = 0;
dist = FUN_005B1B20(&tgt, &Velocity, targetIsBuilding, Type->Airburst,
                    Type->VeryHigh, Type->Level, turn << 8);
if (dist <= |V| · 0.5 || GetHeight() < 1) detonate;               // arrival
if (tgt == (0,0,0) && GetHeight() >= Rules->MissileSafetyAltitude) detonate;

// "am I still closing?" — 60-frame warm-up, then exponential smoothing at 59/60
closing = prev_dist − dist;
if (++n < 60) acc += closing;
else { acc = acc·(59/60) + closing;
       if (acc >= 0 && acc < 60 && !Airburst && !VeryHigh) detonate; }   // gave up
```

`FUN_005B1B20` works entirely in 16-bit facing space (`0x2000` = 45°, values
centred on `0x3FFF`, `×(−2π/65534)` to radians) and turns the current heading
toward the desired heading by at most the passed limit. The `<< 8` on the turn
limit says **`ROT` is in 1/256-of-a-circle units per frame** (so `ROT=12` ≈
16.9°/frame). Marked **PROBABLE**: the turn-limit plumbing through
`FUN_005B1B20`'s register args was not fully disentangled, and the `cos` weave
multiplication was reconstructed from the surrounding FPU ops rather than read
as a single `FMUL`. The *wobble is deterministic* (driven by the frame counter,
not the RNG) — that part is **CONFIRMED**.

### 6.5 Speeds actually produced, for the clone's weapons

`ftol(min(255, clamp(Speed,0,100)·256/100))` (used for `ROT>0`/`Vertical`) vs
`ftol(sqrt(d·6·1.2))` (used for `ROT==0`):

| Weapon | Projectile | Class | Launch speed (lept/frame) | ≈ flight time |
|---|---|---|---|---|
| `M60` / `M1Carbine` / `Para` | `InvisibleLow` | **Inviso** | n/a (teleport) | **1 frame (67 ms)** |
| `120mm` (Rhino) | `Cannon` | **arcing** | `sqrt(1.2·6·d)`: **102** @ 5.75 cells, **74** @ 3 cells | 16.2 fr / 1.08 s @ max |
| `105mm` (Grizzly) | `Cannon` | **arcing** | **96** @ 5 cells | 15.1 fr / 1.01 s |
| `FlakTrackGun` | `FlakTProj` | **arcing + scatter** | **96** @ 5 cells (*not* 128) | 15.1 fr / 1.01 s |
| `FlakTrackAAGun` | `FlakProj` | **Inviso + scatter** | n/a (teleport onto a scattered point) | **1 frame** |
| `BlimpBomb` (Kirov) | `BlimpBombP` | **vertical** | 1, `+1`/frame, cap **51** | **39 fr / 2.6 s** from 750 |
| `155mm` (Destroyer) | `Ballistic` | **arcing** | **121** @ 8 cells (*not* 25) | 19.1 fr / 1.27 s |
| `SubTorpedo` | `Torpedo` | **homing** `ROT=12` | 1 → cap **64** | distance-dependent |
| `Medusa` (Aegis) | `MedusaProjectile` | **homing** `ROT=20`, lock 15 fr | 1 → cap **255** | distance-dependent |
| `V3Launcher` / `DredLauncher` | `InvisibleHigh` | **Spawner** — spawns a `V3ROCKET`/`DMISL` *unit*, not a bullet | n/a | n/a |

---

## §7 — Termination and detonation — **CONFIRMED**

A bullet dies from exactly one of these, all evaluated in `Update`:

| Cause | Test | Notes |
|---|---|---|
| **Ground impact** | new `Z <= GetCellFloorHeight(cell)` (+ bridge offset) | ballistic |
| **Altitude** | `Z > Type->DetonationAltitude` | `Vertical` ascending |
| **Ground (vertical)** | `GetHeight() < 0` | `Vertical` descending |
| **Arrived at the target cell** | new cell == `TargetCoords >> 8` **and** `!Vertical` **and** `GetHeight() < 2·k` | the common case |
| **Collision** | a `TechnoClass` within **`0x7F` = 127 leptons** of the new position, not the owner, not an ally | snaps onto it unless `Inaccurate` |
| **Fuse** | `FuseClass::Update` returns 1 or 2 | only when `ROT >= 1 \|\| Type->Ranged`; ignored entirely when `Type->Dropping` |
| **Blocked** | `IsBlocked()` (`FUN_00468BA0`): `SubjectToCliffs`/`SubjectToWalls` line trace; `GetHeight() <= −4·k`; `Level` on a sloped cell; `AA` within `0x80` of an airborne target | |
| **Out of speed** | `\|V\| < 8.0`, or (`\|V\| < 10.0` and `GetHeight() <= 9`) | |
| **Homing gave up** | smoothed closing rate stalls (§6.4) | |
| **Off map** | `IsOnMap(coord) == false` | **removed silently — no detonation** |
| **Damage decay** | `Degenerates`: `damage--` each frame while `damage > 5` | not a kill, but relevant |

**The fuse** (`FuseClass`, `FUN_004E1040` arm / `FUN_004E1100` update) is the
`Ranged`/`Arm` machinery:

```c
// Arm(start, targetcoords, arm, max):   at MoveTo time
armFrame = CurrentFrame;  armDelay = Type->Arm;         // BulletType+0x2F0
fuseTarget = targetcoords;  initialDist = |start − targetcoords|;

// Update(current):   each frame from BulletClass::Update
if (CurrentFrame − armFrame < armDelay) return 0;        // not yet armed
d = |current − fuseTarget| / 2;
if (d < 0x20)                       return 1;            // DETONATE  (real dist < 64 leptons)
if (d < 0x100 && lastD < d)         return 2;            // overshot -> DETONATE
lastD = d;                          return 0;
```

`Arm` is therefore a **frame delay before the fuse can trigger**, not a distance
— `[BlimpBombP] Arm=10` means the Kirov's bomb is inert for its first 10 frames.

**Detonation** is `Explode(bool)` → `Detonate(coord)`:

- `Explode` does the ≤32-lepton snap (§5.4), then, if `Cluster > 0`, calls
  `Detonate` repeatedly with `Random(0x100,0x200)` offsets.
- `Detonate` (`FUN_004690A0`) computes the effective damage as
  **`(DamageMultiplier × damage) >> 8`** (`+0x150 × +0x6C`, default multiplier
  256 = ×1), fires `Shrapnel()` when `ShrapnelWeapon` is set, and then calls
  **`FUN_00489190(owner, WH, …, house)`** — *the exact area-damage applicator the
  clone already reimplements faithfully as
  `engine/combat.cpp::apply_spread_damage` (`voxel-render.md` §7,
  `combat.md` §7).*

> **The whole warhead/`CellSpread` layer is already correct in the clone.** A
> projectile layer must only change *when* and *where* that applicator is
> invoked — never how.

---

## §8 — Target dies mid-flight — **CONFIRMED**

`BulletClass::LoseTarget` (`FUN_00468420`):

```c
CoordStruct c = Target->GetCoords();
if (!globalFlag && !Target->IsInAir() && cellOf(c) != INVALID_CELL) {
    this->Target = MapClass::GetCell(c);   // retarget onto the CELL
    return;
}
this->Target = nullptr;
```

> **A ground target that dies mid-flight is replaced by the *cell* it died in.
> The projectile keeps flying and detonates there.** It does not vanish, and it
> does not stop.

Only when the target was **airborne** (or its coords are off-map) does `Target`
become `nullptr`. A homing missile in that state has `tgt == (0,0,0)`, and §6.4's
`GetHeight() >= Rules int at +0x5A0` branch takes over: it climbs to that
altitude and self-detonates. The **identification of `Rules+0x5A0` as
`MissileSafetyAltitude` is an inference** (the field was reached from the code,
not from its key string) — **PROBABLE** — but it is the only `[General]` key
whose documented meaning is this branch, and the rules comment matches verbatim
(*"this is the altitude a missile fired at an air target that dies will fly to
before detonating"*). A ballistic shell with a null target simply completes its
arc into the ground.

Note the aliasing: `Target` (`+0x10C`) is an `AbstractClass*` that may point at
either a `TechnoClass` **or** a `CellClass`. `GetTargetCoords()` works either
way. A reimplementation needs the same "target may be a cell" union.

---

## §9 — Confidence summary

| # | Finding | Evidence | Confidence |
|---|---|---|:---:|
| 0.1 | `Gravity=6` lives in **`[AudioVisual]`**, → `Rules+0x16B8` | rules line 756 + `FUN_00668C20` @`0x66AE11` w/ `PTR_s_AudioVisual` | **CONFIRMED** |
| 0.2 | Non-homing launch speed = `ftol(sqrt(d·Gravity·1.2))`, **not** `Speed=` | `FUN_00771E90` + `FUN_0048AAA0` decompile + disasm; `DAT_007E4190 = 1.2` | **CONFIRMED** |
| 3 | `BulletTypeClass` / `BulletClass` layouts (34 + 20 offsets) | `FUN_0046BE00` key→offset map; `Construct` writes | **CONFIRMED** |
| 3 | 256 leptons/cell; velocities in leptons/**frame** | `>>8` cell derivation ×2; `×256.0` @`0x7E0710` | **CONFIRMED** |
| 4 | Launch order: FLH → speed → `min(speed, dist/2)` → scatter → yaw → pitch → `MoveTo` | `FUN_006FD210` decompile + `dis_fire_*.txt` | **CONFIRMED** |
| 4 | Arcing shot is **refused** (bullet destroyed) when the solver has no solution | a stack bool (solver success flag) is 0 → dtor path | **CONFIRMED** |
| 4.1 | `Δz > 0` ⇒ launch angle is a hard-coded **π/4** | literal `0x3FE921D9F4D37C12` @`0x48A8F7` | **CONFIRMED** |
| 4.1 | Exact arcing-angle algebra (`D`, `X` terms of the quadratic) | FPU trace captured, not unwound | **[RE-FALLBACK]** |
| 4.1 | Flat-ground arc ⇒ `sin 2θ = 1/1.2`, `T ≈ 0.4226·sqrt(d)` frames | derived from two CONFIRMED halves + textbook solver | **PROBABLE** |
| 5 | Inaccuracy **requires an RNG draw**, from `Scenario->Random` (sync-critical) | `LEA ECX,[EDX+0x218]` ×2 + YRpp `//218`; `FUN_0065C220` | **CONFIRMED** |
| 5.1–5.3 | Three distinct scatter formulas (`Random(BS/2,BS)`; `Random(0,BS)·d/range`; `Random(0,2·BS)·d/range`) | `dis_moveto_scatter.txt`, `fire_6FD210.c` | **CONFIRMED** |
| 5.4 | `Inaccurate` = **suppress snap-to-target**, at 32 lept / 127 lept / fuse re-aim | three read sites | **CONFIRMED** |
| 5.5 | `MissileSpeedVar` / `MissileROTVar` consumers | parsed, consumer not located | **[RE-FALLBACK]** |
| 6.1 | Arcing integrator: `V.z -= Gravity; pos += V` (semi-implicit Euler, no `dt`) | `dis_arcgrav_4673F2.txt` FSUB + `FUN_0046B880` | **CONFIRMED** |
| 6.1 | `Floater` ⇒ half gravity | `FUN_0048AC00` (18 bytes) | **CONFIRMED** |
| 6.2 | `Vertical`: `\|V\| += Acceleration` capped at `Speed`; direction frozen; ends at `GetHeight()<0` or `> DetonationAltitude` | `Update` vertical branch + `dis_update_gravity.txt` | **CONFIRMED** |
| 6.3 | `Inviso` projectiles **teleport at launch**, `V=0`, detonate 1 frame later | `MoveTo` Inviso branch | **CONFIRMED** |
| 6.4 | Homing: accel/decel, course lock, 15-frame deterministic weave, arrival at `dist ≤ 0.5·\|V\|`, "stopped closing" give-up | `Update` homing branch; `FUN_005B1B20` unwound in wave 4 (04-projectiles.md §1.1) | **CONFIRMED** (implemented: `integrate_homing`) |
| 6.4 | `ROT` is 1/256-circle per frame (`turn << 8` into 16-bit facing) | `<<8` at the steering call | **PROBABLE** |
| 7 | Termination causes (11 listed), collision radius `0x7F`, fuse ≤ `0x20`-of-half-distance | `Update` tail + `FUN_004E1100` + `FUN_00468BA0` | **CONFIRMED** |
| 7 | `Arm` is a **frame delay**, not a distance | `FuseClass::Arm/Update` | **CONFIRMED** |
| 7 | `Detonate` → `FUN_00489190`, the applicator the clone already mirrors | `bul_4690A0.c` L106 | **CONFIRMED** |
| 7 | Effective damage = `(DamageMultiplier × damage) >> 8` | `Detonate` | **CONFIRMED** |
| 8 | Dead **ground** target ⇒ retarget to its **cell**, keep flying (does **not** vanish) | `FUN_00468420` decompile | **CONFIRMED** |
| 8 | Dead **air** target ⇒ `Target = nullptr`, climb to `Rules+0x5A0`, self-detonate | `FUN_00468420` + `Update` null-target branch | **CONFIRMED** (behaviour) / **PROBABLE** (`+0x5A0` = `MissileSafetyAltitude=750`, inferred not string-anchored) |
| 3 | `Proximity` has no reader in the bullet module (`Ranged` + `Arm` do the work) | negative search across all 58 module functions | **PROBABLE** |
| — | `Bouncy`/`Elasticity` reflection; `Airburst`/`Cluster`/`Shrapnel` spawning | located, not unwound | **PROBABLE** |

### What was ruled out

- `FireFLH` as a *bullet* concern: the string does not exist in the binary at
  all (0 hits). FLH is `PrimaryFireFLH`/`SecondaryFireFLH` in `artmd.ini`,
  resolved by `TechnoClass` before the bullet exists (`voxel-render.md` §6).
- `[Projectiles]` as a list section: it does not exist in `rulesmd.ini`.
  Projectile types are declared implicitly by `Projectile=` on weapons.
- `AN` / `AS`: commented out in the rules and **not parsed** by this build.
- `HomingScatter` (`Rules+0x1730`): parsed by `FUN_0066B5F0`, but **absent from
  this `rulesmd.ini`** — it never takes a non-default value on this install.
- `Speed=` as the ballistic launch speed: actively disproven (§0.2). The
  `[155mm] Speed=10` → 25 lept/frame reading is *provably impossible* — a
  25-lept/frame shell under `Gravity=6` cannot reach 104 leptons, let alone the
  weapon's 2048-lepton range.

---

## §10 — What the clone does today

`engine/combat.cpp::try_fire` (called from `step_combat` step (f) once
`rof_cool <= 0` and `vehicle_aimed`):

```c
u.rof_cool  = u.w_rof / kLogicFps;
u.fire_flash = kFireFlashSecs;
if (u.w_cell_spread >= 1.0) apply_spread_damage(units, occ, u, v);   // Kirov / nuke
else { v.hp -= damage_vs(u.w_damage, u.w_verses, v.armor_ix); ... }  // everything else
```

So: **zero flight time, zero miss chance, damage lands on the victim the same
call the weapon fires, and the impact point is the victim itself.** Everything
downstream of the impact — `damage_vs`, `Verses`, `CellSpread` falloff,
`PercentAtMax`, retaliation, death/tombstone — is faithful and must not change.

A projectile layer therefore needs exactly one structural change: `try_fire`
stops calling the damage path and instead emits a `Projectile` record; a new
`step_projectiles(dt)` advances them and calls the *existing*
`apply_spread_damage` / `damage_vs` on detonation. `UnitCombat`/`Unit` need the
new per-weapon fields (`projectile class`, `arcing`, `vertical`, `rot`,
`acceleration`, `inviso`, `inaccurate`, `flak_scatter`, `ranged`, `arm`,
`ini_speed`), all defaulting inert so the existing byte-identical gates keep
passing until a weapon opts in.

---

## §11 — Deviations we will take

1. **[DEVIATION — RNG]** Faithful inaccuracy **is** an RNG draw from the
   lockstep RNG (§5, CONFIRMED). The clone substitutes a seeded `xorshift32` for
   the Westwood 250-word lagged-XOR `Randomizer`. We will make the scatter draws
   through the clone's seeded PRNG, in a **fixed unit-index order** inside
   `step_projectiles`, so the sim stays bit-reproducible. The *distribution* is
   faithful; the *stream* is not. This affects only `[FlakTProj]`, `[FlakProj]`
   and `Cluster` weapons — every other weapon in the clone's roster is
   deterministic in vanilla too.

2. **[DEVIATION — arcing angle]** Until `FUN_0048A8E0`'s algebra is unwound, use
   the textbook low-root solution with the two CONFIRMED special cases (`Δz > 0`
   ⇒ 45°; no solution ⇒ refuse the shot). With the CONFIRMED
   `v = sqrt(1.2·d·g)` this lands within a frame of the derived §4.1 timings and
   is self-consistent. Flag any future mismatch here, not in the integrator.

3. **[NO DEVIATION — infantry]** Do **not** build a flying projectile for
   `[InvisibleLow]`. `Inviso` genuinely teleports (§6.3); a 1-frame delay is the
   faithful model. Keep `try_fire`'s current path for `Inviso` weapons and only
   add the cliff/wall line trace when `SubjectToCliffs`/`SubjectToWalls` is set.

4. **[RETIRED in wave 4 — homing]** The homing branch is implemented as
   recovered (§6.4 wave-4 update), including the `MissileROTVar` weave.
   `MissileSpeedVar` (Rules+0x590) has no consumer in the bullet module and is
   correctly ignored.

5. **[DEVIATION — Bouncy / Airburst / Cluster / Shrapnel / Degenerates]** Out of
   scope. None of the clone's current weapons set them
   (`Cluster`/`ShrapnelWeapon`/`Bouncy`/`Degenerates` are all absent from
   `[InvisibleLow]`, `[Cannon]`, `[BlimpBombP]`, `[FlakTProj]`, `[FlakProj]`,
   `[Ballistic]`, `[Torpedo]`, `[MedusaProjectile]`), so the gap is provably
   unreachable for the existing roster.

6. **[DEVIATION — 3-D]** The clone's world has no Z for ground units
   (`combat.md` §6 already records this). Arcing shots will carry their own
   projectile-local Z (the whole point), but `Δz` between firer and target will
   be **0** for same-plateau ground combat, which makes §4.1's flat-ground
   solution the *only* branch reached — the `Δz > 0 ⇒ 45°` case is dead code
   until terrain height reaches combat. Record it now so it is not "discovered"
   as a bug later.

7. **[DEVIATION — `DamageMultiplier`]** Always 256 (×1) for every weapon in the
   roster; hard-code the ×1 rather than carrying the field.

---

## §12 — Implement-phase RE-GATE addendum (P2 implement phase)

Closed from the existing hunt decompiles (`D:\ra2-re\out\proj\`) plus three
short headless passes `D:\ra2-re\scripts\p2impl_{1,2,3}.py` (outputs
`D:\ra2-re\out\p2impl\`). Same binary, same content-anchored method.

### 12.1 The arcing-angle solver is unwound — **CONFIRMED** (supersedes §4.1's `[RE-FALLBACK]`)

`FUN_0048A8E0(sel /*CL*/, v /*EDX*/, d, z, g, double* out)` (stack args mapped
from the wrapper's pushes at `0x48A7FF..0x48A808`):

```
if (z > 0)            { *out = π/4; return 1; }        // 0x3FE921D9F4D37C12
A   = v² − z·g
D   = v⁴ − 2·v²·z·g − g²·d²
den = 2·(z²/d² + 1)
if (D < 0 || den == 0) return 0;                        // shot refused
t   = (A + √D)/den   if sel == 0                        // "plus" root
      (A − √D)/den   otherwise                          // "minus" root
if (t < 0) return 0;
*out = acos( √t / v );                                  // FUN_004CACD0
return 1;
```

Derivation: with `vh = v·cosθ`, `T = d/vh`, `z = d·tanθ − g·d²/(2vh²)` →
`vh⁴(z²/d²+1) − vh²(v² − zg) + g²d²/4 = 0`, whose discriminant is exactly `D`.
So **`t` is the HORIZONTAL speed squared** and `√t/v = cos θ` (§4.1 called it
the vertical component — corrected here). `FUN_004CACD0` is a table `acos`:
`FLD x; FADD 1.0f; FMUL −2048.0f; ftol; ECX = 0x858014 − 4·idx; FSUBR π/2` i.e.
`π/2 − asin_tbl[...]`. The clone uses libm `acos` (precision-only deviation).

Wrapper `FUN_0048A7E0` (`dis_arcsolve_48A7E0.txt`):
- `sel != 0` (lofted): `pitch16 = ftol((θ − π/2)·(−65534/2π))`.
- `sel == 0`: also calls the core with **`d+1`** (`INC ECX` at `0x48A855`) and
  root 0; `FCOMP` → if `θ(d+1) < θ(d)` the angle is **negated** (`FCHS` at
  `0x48A877`) before the same conversion. Since `acos` loses the sign, this
  numeric-derivative test recovers a *downward* low-root shot (target far
  enough below the muzzle). **CONFIRMED** (disassembly).

### 12.2 `LevelHeight = 104`, `BridgeHeight = 416` — **CONFIRMED**

`0x89CDE0` (read by `Update`'s arrival test and `IsBlocked`) is written once by
the static initialiser at `0x466190`:

```
0x466190 FLD [0x89CD48]   ; = 90.0 · (π/180)      (init 0x466170: 0x7E3668 · 0x7E0730)
0x466196 FSUB [0x89CD70]  ; − 60.0 · (π/180)      (init 0x466150: 0x7E3668 · 0x7E0728)
0x4661A2 CALL 0x4CAC70    ; tan, 4096-entry table @0x85C024, scale 651.8986 = 4096/2π
0x4661A7 FMUL [0x89CD68]  ; √(2·256²) = 362.04    (init 0x4660C0: pow(256,2)·2, sqrt)
0x4661B0 FMUL [0x7E0738]  ; 0.5
0x4661BB MOV [0x89CDE0], ftol(...)   → ftol(tan(30°)·181.02) = 104
```

and `0x89CDD4 = ftol(4·[0x89CDE0] + 0.5) = 416` (`0x466230`), the bridge
offset `Update` adds on bridge cells (cell flag `0x100`). Constants read from
the image: `0x7E3668 = π/180`, `0x7E0730 = 90.0`, `0x7E0728 = 60.0`,
`0x7E0738 = 0.5`, `0x7E7970 = 651.8986`, `0x7E38D8 = 150.0`. Consequences:

- Arrival test (§7): `cell == TargetCell && !Vertical && GetHeight() < 2·104 = 208`.
- `IsBlocked` (`FUN_00468BA0`): `GetHeight() <= −4·104 = −416` → blocked.
- Ground band: `floor <= z < floor + 150` with a non-owner, non-allied object in
  the cell → detonate (the `0x7E38D8` test in the ballistic block).

### 12.3 Vertical and homing projectiles launch at speed 1 — **CONFIRMED** (field PROBABLE)

`Fire` after the `d3/2` cap: `if (Projectile->ROT > 0 || Projectile->Vertical)
{ if (TechnoType+0x6A4 == 0) speed = 1; bullet->Speed(+0x110) = Weapon->Speed(+0xA8); }`.
This is where §6.5's "launch speed 1, +1/frame, cap 51" comes from. `+0x6A4`
is not string-anchored (PROBABLE: zero for every type in the clone's roster —
the same field also selects the alternate yaw code path that is never taken
for a 0 value). `MoveTo` additionally normalises a `ROT>0` bullet's velocity to
magnitude 1.0.

### 12.4 `TargetCoords` is the TRUE target — **CONFIRMED**

`MoveTo` (`FUN_00468660`) sets `+0x140/0x144/0x148 = Target->GetTargetCoords()`
— not the scattered aim point. A scattered `[FlakTProj]` shell is aimed off the
target but its arrival test still looks for the real target's cell, so it
usually ends on the ground-impact / collision rules instead.

### 12.5 Caveat: the ground-impact block — **PROBABLE**

The `z < floor` block in `Update`'s ballistic branch splits on
a local holding `this+0xB0` (Owner): with an owner and no bridge flag the decompile
jumps past the detonate assignment. That reading would make owned shells
burrow to the `IsBlocked` −416 floor before detonating, which contradicts the
hunt's own §7 "Ground impact … CONFIRMED" row and observed play (missed tank
shells burst on the ground). The branch was not disentangled from the FPU
stack in budget; the clone follows §7 (detonate on `z < floor`, z clamped) and
keeps the −416 `IsBlocked` rule as a backstop.

### 12.6 `Randomizer::Random(lo, hi)` range mapping — **CONFIRMED**

`FUN_0065C220`: if `lo == hi` return `lo`; swap so `lo < hi`; `range = hi − lo`;
`b` = index of the top set bit of `range`; repeat { draw the next table word,
mask to `b+1` bits } while `value > range`; return `lo + value`. Inclusive and
unbiased (rejection sampling). The clone reproduces this mapping exactly on top
of its own bit source (see the P2 spec, decision D3).
