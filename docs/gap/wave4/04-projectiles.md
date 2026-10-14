# Wave 4 / 04: Missiles, rockets and projectile flight (gap analysis)

Read-only audit of `master` 4647b17. It covers every weapon in the Allied and Soviet rosters plus the base defenses, and asks one question per weapon: does what flies on screen and in the sim match the original?

How the audit was done:
- **Reverse engineering.** Ghidra headless on a private project copy (`D:\ra2-re\w4-proj`), scripts `D:\ra2-re\scripts\w4p_1..7.py`, outputs in `D:\ra2-re\w4-projout\`. The main target is `gamemd.exe`. The RA2 `game.exe` was used to cross-check RA2 against YR.
- **Rules.** RA2 `rules.ini`/`art.ini` (from `ra2.mix/local.mix`) and YR `rulesmd.ini`/`artmd.ini`.
- **Play runs.** Five `--play` runs in the worktree `D:\ra2-wt\w4-proj` (Release build `build-rel`).
- **Evidence.** Images and scripts are in `docs/gap/wave4/04-evidence/` (local only).

The ALL01 report ("no rockets flying") has a root cause, and it is the projectile system:
- **Every missile with `ROT>0` is still resolved instantly.** This covers the Patriot, IFV, Harrier, Black Eagle, Apocalypse tusks, Aegis, Typhoon torpedo, Hornet and Osprey bombs, and the Tech Outpost.
- **Nothing flies for these weapons.** The unit weapons get a 1-frame fading line at most. Base-defense missiles (the Patriot) draw nothing at all: there is no projectile and no line.
- **What does fly:** the V3/Dreadnought rocket units, ballistic shells and Kirov bombs.

---

## Fix status (wave-4 fix agent 3, branch `w4fix-proj`)

| ID | Status | Notes |
|---|---|---|
| W4-PJ-01 homing instant | **FIXED** | `ProjKind::Homing`, `integrate_homing` (§1.1 as recovered; Acceleration default 3 from the ctor) |
| W4-PJ-02 base-defence missiles not drawn | **FIXED** | defence bullets fly (`launch_defense_projectile`) and draw from the building's weapon slot |
| W4-PJ-03 secondary-weapon art ignored | **FIXED** | the projectile draws from the weapon that fired it (unit slot / defence) |
| W4-PJ-04 V3/DMisl flight model | **FIXED** | `engine/rocket.cpp` (FUN_00661D00 / 00661B30 / 00662A70) |
| W4-PJ-05 voxel bullets | **FIXED** | `Voxel=yes` art (ZBOMB, SUBT) rasterised at the velocity yaw/pitch |
| W4-PJ-06 building shells fallback square | **FIXED** | building shooters resolve their weapon art |
| W4-PJ-07 Rotates from rules | **FIXED** | read from the art `[Image]` section |
| W4-PJ-08 Trailer / line trails | **FIXED (falls out of 01)** | the wave-3 trail system now runs on real flights; the fake `instant_line` only remains for weapons that are still instant |
| W4-PJ-09 rocket shadow | **FIXED** | `NoShadow=yes` honoured |
| W4-PJ-10 interception gate | **FIXED** | `all01-opening.play`, `missiles-intercept.play` |
| W4-PJ-11 spent rockets in units_lost | **FIXED** | `DontScore` units excluded (probe + score screen) |
| W4-PJ-12 V3 single shot | **FIXED (probable cause)** | a rocket that died in flight never freed its launcher's spawn slot; now it does |
| W4-PJ-13 cannon shell invisible | **FIXED** | bullet SHPs use the unit palette unless `AnimPalette=yes`; `Shadow=yes` blob. The 120MM shell is the original's 4x4 dot. The "ring mark on the launch frame" was not reproduced |
| W4-PJ-14 jump projectiles | open | dogs / drones stay abilities |
| W4-PJ-15 FlakProj scatter / Inviso trace | open | – |
| W4-PJ-16 O(P x U) projectile pass | **FIXED** | per-frame cell index of ground occupants |

## 1. Flight models recovered from the binary

### 1.1 Homing missile steering (`BulletClass::Update` homing branch, `ROT >= 1`). PINNED (projectile.md §6.4 was PROBABLE)

Addresses are in `gamemd.exe`. RA2 `game.exe` has the same code at `0x45FBC4`, where `BulletType::ROT` sits at `+0x2AC` instead of `+0x2DC`.

**Wobble and turn limit** (`0x466B9F`–`0x466CF1`):

```
V    = Rules->MissileROTVar                         ; Rules+0x598 (double), =0.25
                                                    ; string-anchored: FUN_0066CF70 @0x66E66C
ph   = ((bullet->ID + Frame) % 15) * (1/15) * 2pi   ; 0x7E38F8 = 1/15, 0x7E2CC0 = 2pi
turn = ftol( (V*sin(ph) + V + 1.0) * Type->ROT )    ; 0x7E0718 = 1.0, FIMUL [type+0x2DC]
if (|bullet - target| < 0x100)  turn = ftol(turn * 1.5)   ; 0x7E38F0 = 1.5 (inside one cell)
if (CourseLock)                 turn = 0
limit16 = low_byte(turn) << 8                       ; MOV CH,AL: ROT in 1/256 circle per frame
dist = FUN_005B1B20(&tgt, &Velocity, targetIsAir, Airburst(+0x294),
                    VeryHigh(+0x299), Level(+0x29D), &limit16)
```

What this means:
- **This resolves the old `MissileROTVar` [RE-FALLBACK].** `MissileROTVar` is the amplitude of a deterministic 15-frame sine on the turn rate. The turn rate therefore varies between ROT×1.0 and ROT×1.5 (mean ×1.25), and goes ×1.5 again inside 256 leptons.
- **`MissileSpeedVar` (`Rules+0x590`) has no consumer.** No instruction in the bullet module reads it; it is parsed only. Status: PROBABLE.
- **`MissileSafetyAltitude` is now string-anchored** at `Rules+0x5A0` (`0x66E68C`). That upgrades projectile.md §8 from PROBABLE to CONFIRMED.

**Steering, `FUN_005B1B20`.** All work is done in 16-bit facing space:
- **No target.** If the target coordinates are null, pitch is turned toward climb (`0x2000`) by the limit.
- **Yaw.** Yaw is turned toward `atan2(target − bullet)` by at most `limit16`, snapping when within the limit (`FUN_005B23C0`). The horizontal speed is preserved.
- **Pitch, cruise mode.** This applies when the target is not air, the turn is above 1°, and either `Airburst` is set or the distance condition holds:
  - The missile follows the terrain.
  - Its cruise height is `min(dist_cells, 5) × LevelHeight` above the cell floor, or 10 levels for `Airburst`/`VeryHigh`.
  - Height is nudged ±18 leptons per frame when it is off by more than 20 leptons.
  - Pitch moves toward climb `0x2000`, dive `0x4800` or level `0x4000` at `limit/2`.
- **Pitch, direct mode.** Against air targets, pitch aims straight at the target.
- **`Level=yes`.** All pitch changes are skipped; this is the torpedo case.
- **Return value.** The function returns the distance with the z component quartered, or with z zeroed for `Airburst`. `Update` detonates when `dist ≤ 0.5·|V|` (`0x7E0738`).

**Speed.** The rest of the branch is as projectile.md §6.4/§12.3 describes, now CONFIRMED from the decompile:
- Launch at speed 1, then `|V| += Acceleration` up to the weapon `Speed` (leptons per frame). When `CourseLockDuration=0`, the increment alternates 0/1 per frame while course-locked.
- `|V| -= Acceleration/2` when above `Speed`.
- Course lock holds the heading for `CourseLockDuration` frames.
- The 60-frame "stopped closing" give-up rule applies.

### 1.2 RocketLocomotion (V3ROCKET, DMISL, YR CMISL). PINNED

| Item | Address / value |
|---|---|
| CLSID `{B7B49766-E576-11D3-9BD9-00104B972FE8}` | `0x7E8AD0` (RA2 `game.exe`: `0x7A2010`) |
| constructor (vtables `0x7EFBE8` / `0x7EFB1C`) | `FUN_00662E50` |
| `Process` (state machine) | `FUN_00661D00` |
| nose proximity / ground test | `FUN_00661B30` |
| explode | `FUN_00662A70` |
| per-type `RocketStruct` in Rules | V3 `+0x4B0`, DMisl `+0x4E4`, CMisl `+0x518`. Selected by comparing the owner type with `V3RocketType +0x4E0` / `CMislType +0x548`; anything else is DMisl. |

`RocketStruct` field offsets (read by `FUN_0066CF70` @`0x670C47..0x670DE2`):

| Offset | Field |
|---|---|
| +0 | PauseFrames |
| +4 | TiltFrames |
| +8 | PitchInitial (float) |
| +C | PitchFinal (float) |
| +10 | TurnRate (float) |
| +14 | RaiseRate |
| +18 | Acceleration (float) |
| +1C | Altitude |
| +20 | Damage |
| +24 | EliteDamage |
| +28 | BodyLength |
| +2C | LazyCurve |
| +30 | Type |

States of `FUN_00661D00`. Pitch angles are `field × π/2` (`0x7E1820`).

1. **Pause.** The rocket waits on the launcher for `PauseFrames` (V3 0, DMisl 20). It spawns a launch-smoke anim every 24 frames.
2. **Tilt.** Over `TiltFrames` (60), pitch is interpolated linearly from `PitchInitial·90°` to `PitchFinal·90°`: V3 18.9° → 45°, DMisl 0° → 45°. At the end of the tilt:
   - the rocket leaves the launcher;
   - a smoke anim is spawned.
3. **Ignite / climb.** Each frame `speed += Acceleration` (V3 0.4, DMisl 0.8 leptons/frame²), capped at the type's Speed. The rocket flies along its pitch until its height reaches `Altitude` (768). Then it records the initial horizontal distance to the target.
4. **Cruise.**
   - **`LazyCurve=no` (DMisl).** Pitch decays by `TurnRate` per frame toward level. When the remaining 3-D distance is no more than the height above the target, the rocket goes to state 5.
   - **`LazyCurve=yes` (V3).** `pitch = atan(dz/horiz)·(1−t) + PitchFinal·90°·t` with `t = horiz/horiz0`. This is a smooth descending arc.
   - In both cases yaw is re-aimed at the target every frame.
5. **Dive.** Pitch turns toward `atan(dz/horiz)` (at the target) by at most `TurnRate` per frame.

Every frame the rocket moves by `speed·cos(pitch)` along its facing and `speed·sin(pitch)` in height.

**Detonation** (`FUN_00661B30`). The rocket explodes when any of these holds:
- the nose point (BodyLength ahead along pitch and yaw) reaches the target z;
- it reaches the target z plus the bridge height over a bridge cell;
- its height is below 1;
- its own `Strength` reaches 0. This last one is the interception path.

**Explosion** (`FUN_00662A70`). Warhead `Rules+0xFB0` V3 or `+0xFB4` DMisl, elite `+0xFB8`/`+0xFBC`. The explosion anim comes from `FUN_0048A400`, damage goes through the shared area applicator `FUN_00489190`, and then the rocket is deleted.

### 1.3 Interception. Decided: YES, by design, with no special keys

- **No interception keys exist.** There is no `Interceptor`, `AntiMissile` or `Intercept` string in YR `gamemd.exe`, nor in RA2 `game.exe`. Those keys are Ares, not RA2.
- **The rockets are ordinary airborne targets:**
  - V3ROCKET and DMISL are AircraftTypes with `Strength=50`, armor `special_2`, `Spawned=yes`, `MissileSpawn=yes` (TechnoType `+0xD68`).
  - None of the `MissileSpawn` readers (`0x41430D`, `0x4176F0`, `0x54E040`, `0x6B7230`, `0x6B7400`, `0x7021D0`) is a target-evaluation function. They handle spawn bookkeeping and locomotion.
- **Every AA warhead does 100% against `special_2`:** SAMWH (Patriot, Aegis), FlakWH, FlakGuyWH, HE (tusks, IFV), SSA (Rocketeer) and SA.
- **Conclusion.** Patriots, Flak Cannons and Tracks, Flak Troopers, IFVs, Apocalypse tusks, Rocketeers and the Aegis can shoot V3 and Dreadnought rockets down. The game data clearly intends this. In the code it is PROBABLE: no target filter exists.

### 1.4 RA2 vs YR data differences that matter here

The projectile sections, ROT values, V3Rocket and DMisl blocks are identical between RA2 `rules.ini` and `rulesmd.ini`, except for these:

| Item | RA2 | YR |
|---|---|---|
| DMISL `Speed=` | 20 | 18 |
| Harrier/Black Eagle `Burst=` | 2 (elite 4) | 1 |
| Rhino elite (120mmE) | 85 dmg / ROF 80 | 90 / 65 |
| Flak Track/Hydrofoil ground-gun ROF | 40 | 25 |
| GI Para | 15 dmg | 25 |
| CMisl block (Boomer) | absent | present (YR-only) |

The clone reads `rulesmd.ini` (W2-AN-19), so it carries the YR values.

---

## 2. Weapon matrix

Legend:
- **Clone kinds:** `Instant` means the damage lands on fire (`resolve_projectile_spec`: Inviso → Instant, ROT>0 → Instant); `Ballistic` and `Vertical` are P2 flying projectiles; `Rocket unit` means world_an.cpp missiles.
- **Flies?** is what the screenshots show.

| Weapon (unit) | Original projectile | Clone kind | Flies? (clone) | Art / trail / speed / ROT right? | Interception | Evidence |
|---|---|---|---|---|---|---|
| M60/Para (GI), M1Carbine (Conscript), MP5 (SEAL), DoublePistols (Tanya), AWP (Sniper), Vulcan/Vulcan2 (Sentry Gun, Pillbox), MirageGun, DemoBomb | `InvisibleLow` Inviso (1-frame teleport) | Instant | n/a (correct: nothing flies) | OK. The cliff/wall trace is missing (SubjectToCliffs). | — | projectile.md §6.3 |
| 20mm (Rocketeer), BlackHawkCannon (Night Hawk) | Inviso AA+AG | Instant | n/a | OK | can hit rockets | rules |
| ElectricBolt/TankBolt/CoilBolt (Tesla) | Inviso + IsElectricBolt | Instant + bolt beam | beam drawn | OK (W2-GA-15) | — | W2 |
| PrismShot/PrismSupport (Prism Tower) | Inviso + IsLaser | Instant + laser | beam drawn | OK | — | W2 |
| RadBeam (Desolator), NeutronRifle (Chrono Legionnaire), PsiWave/MindControl (Yuri), SonicZap (Dolphin) | Inviso / special | Instant | n/a | not audited here (no flight) | — | — |
| 120mm/120mmx (Rhino, Apocalypse), 105mm (Grizzly), Robogun, SABOT (Tank Destroyer) | `Cannon` Arcing, `120MM` 4×4 SHP | Ballistic | **sim yes**: ~16-frame arc, both impacts at the right time. **Visually no**: no shell pixel in any of 6 frames at 3× zoom (`cannon_frames.png`). | Speed/arc per §0.2/§12.1 OK. The 4×4 `120MM` dot was not observed. A ring smudge appears mid-path on the launch frame (unexplained). | — | w4-proj-a |
| FlakTrackGun/FlakGuyGun (Flak Track, Flak Trooper, Hydrofoil) | `FlakTProj` Arcing + scatter | Ballistic | sim yes | scatter per §5.2 | — | P2 |
| FlakTrackAAGun/FlakWeapon (Flak Track, Flak Cannon, Hydrofoil AA) | `FlakProj` Inviso + 2×scatter | Instant | n/a (correct) | the §5.3 scatter of the impact point is not modelled for Instant | **shoots rockets**: V3 intercepted (see F) | w4-proj-b |
| 155mm (Destroyer) | `Ballistic` Arcing | Ballistic | sim yes | OK (P2) | — | P2 |
| GrandCannonWeapon (Grand Cannon) | `GrandCannonBall` Arcing | Ballistic (building) | **drawn as a 3-px hollow square** (`gcannon_tusk.png` g1) | `pres_draw_projectile` returns false for building shooters, so the fallback rectangle is drawn instead of `120MM` | — | w4-proj-land G |
| BlimpBomb (Kirov) | `BlimpBombP` Vertical, `ZBOMB` **voxel** | Vertical | **yes**: falls from altitude 618 over ~24 frames and hits the power plant | **art wrong**: `ZBOMB` is a voxel, `projectile_shp` fails, so it draws as a grey 3-px square (`kirov_bomb.png`). Accel/terminal speed per §6.2 OK. | — | w4-proj-b D2 |
| HoverMissile (IFV, Tech Outpost) | `AAHeatSeeker2` **ROT=60**, `DRAGON` 32-facing SHP, UseLineTrail | **Instant** | **no**: a full-length grey line appears on the fire frame and is gone 2 frames later (`ifv_instant_line.png`); no missile sprite | Art, speed (Speed=40 → 102 leptons/frame, accel), ROT and Arm=2 all missing. The line is laid along a fake 64-lepton sine. | — | w4-proj-land B |
| Maverick/Maverick2 (Harrier, Black Eagle) | `AirToGroundMissile` ROT=100, DRAGON | Instant | no (line only) | as above | — | code |
| MammothTusk (Apocalypse AA) | `AAHeatSeeker` ROT=80, DRAGON | Instant | **no, not even a line**: `fire_fx` only looks up slot-0 (Primary) art | missing | can hit rockets | w4-proj-land H, `pres_game.inc:629` |
| RedEye2 (**Patriot**) | `AAHeatSeeker` ROT=80, DRAGON + line trail | Instant (defense) | **nothing at all**: `defense_fire_tick` draws only muzzle, bolt or laser | missing missile, line and speed | **shoots rockets** | `pres_game.inc:873`, w4-proj-b |
| Medusa (Aegis) | `MedusaProjectile` ROT=20, CourseLock=15, `MEDUSA` 64-facing SHP, High=yes | Instant | no (line only) | missing | can hit rockets | code |
| SubTorpedo (Typhoon) | `Torpedo` ROT=12, Level=yes, `SUBT` **voxel**, Trailer=BBBLELRG | Instant | **no**: the destroyer took damage on the order frame (`unit_hp 500 < 600 after 0 frames`); no torpedo, no bubbles | all missing | — | w4-proj-n T |
| HornetBomb/ASWBomb (Hornet, Osprey) | ROT=1 homing, DRAGON | Instant (from the spawned plane) | no | missing | — | code |
| GoodTeeth/BadTeeth/DroneJump/SquidGrab (dogs, Terror Drone, Giant Squid) | ROT=8 "jump" projectiles (ADOGP/DOGP/DRONP SHP) | Instant / ability | no | the drone/dog leap is not drawn | — | code |
| V3Launcher (V3) | Spawner → **V3ROCKET** (RocketLocomotion) | Rocket unit (world_an.cpp) | **yes**: a voxel rocket with V3TRAIL puffs flies (`v3_rocket.png` e2) | **partial**: no tilt/pitch (the hull stays level); constant cruise speed (no `V3RocketAcceleration`); climbs at 12 leptons/frame to 768 then flies flat; **no LazyCurve descent and no dive**: it detonates when it is horizontally over the target while still at altitude, and the damage is applied on the ground; draws a grey shadow square despite `NoShadow=yes` | **YES**: with 2 enemy Patriots + a Flak Cannon the rocket is lost and the target survives; without AA the target dies (control `w4-proj-c`) | w4-proj-land E, -b F2, -c |
| DredLauncher (Dreadnought) | Spawner → **DMISL** ×2 | Rocket unit | yes (W2/W3 `fx-naval`) | same gaps as V3; DMisl pause 20 + tilt 60 frames OK; `LazyCurve=no` level-off/dive missing | rockets are interceptable (Aegis test inconclusive: the cheat placed the Aegis on land) | w4-proj-n M |
| HornetLauncher (Carrier), ASWLauncher (Destroyer) | Spawner → aircraft | spawned planes | yes | (W2-AN-12) | Hornets are targetable aircraft | W2 |
| Nuke (SW) | NukeCarrier/NukePayload bullets | SwStrike | yes (W2-AN-05) | out of scope | — | W2 |

**Other observations:**
- **`units_lost` counts every spent V3/DMisl rocket as a lost unit,** whether or not it was intercepted (seen in both F2 and the control). This is a statistics bug.
- **After its first rocket the V3 did not fire again within 900 frames** (w4-proj-b, second `until`). Possibly the spawn regen/reload or the persistence of the attack order; needs confirmation.
- **Performance.** A 12 Apocalypse vs 12 Flak Track + 12 Rhino melee, rendered every frame for 300 frames, took 7.9 s against 7.8 s for an idle baseline. At most 4 shells were in flight at once.
  - The per-projectile cost is O(units): `first_unit_in_cell` scans all units, `detonate_projectile` snapshots the HP of all units, the live vector is copied each frame, and `observe_projectiles` builds a `std::map` per frame.
  - This is harmless today. It will matter once homing missiles fly: Patriot and Aegis volleys, Hornets, burst-4 elite IFVs.

---

## 3. Gap table

| ID | Gap | Original (pinned) | Clone (evidence) | Severity |
|---|---|---|---|---|
| W4-PJ-01 | **Homing missiles (ROT>0) are instant-hit and invisible** | §1.1 steering: speed 1 → accel to `Speed`, 15-frame ROTVar wobble, course lock, terrain-following cruise height, arrival `dist ≤ |V|/2`, give-up | `projectile.hpp` ROT>0 → `Instant`; IFV shows a 2-frame line; torpedo damage lands on the order frame | **CRITICAL** (this is "no rockets flying") |
| W4-PJ-02 | **Base-defense missiles draw nothing** (Patriot) | same as 01 + DRAGON line trail | `defense_fire_tick` handles bolt/laser/muzzle only | **CRITICAL** (visible in ALL01-style bases) |
| W4-PJ-03 | Secondary-weapon art is ignored (Apocalypse tusks, IFV variants) | per-slot weapon art | `fire_fx` and `pres_draw_projectile` use the Primary/unit image only | MAJOR |
| W4-PJ-04 | **V3/DMisl flight model** | §1.2: tilt from PitchInitial to PitchFinal, Acceleration ramp, climb to 768, LazyCurve arc or level-off + dive at TurnRate, nose-contact detonation, Body pitch drawn | flat climb, constant speed, detonates at altitude over the target; hull never pitches | MAJOR |
| W4-PJ-05 | Voxel projectiles not rendered (`ZBOMB`, `SUBT`) | `[img] Voxel=yes` bullets draw the VXL by facing and pitch | fallback grey square; for the torpedo, nothing | MAJOR (the Kirov bomb is a signature visual) |
| W4-PJ-06 | Building-fired shells use the fallback square (Grand Cannon) | Image=120MM | `pres_draw_projectile` bails when `attacker < 0` | MINOR |
| W4-PJ-07 | `Rotates=` is read from the rules `[Projectile]` instead of the art `[Image]` | `Rotates` read via `0x46C180` with section `ArtINI` (`0x886100`), next to `Trailer` | DRAGON/MEDUSA would always draw frame 0 | MINOR (latent until 01 lands) |
| W4-PJ-08 | Missing torpedo `Trailer=BBBLELRG`, MEDUSA SpawnDelay trail, real point-by-point line trails | `Trailer` spawned every `SpawnDelay` from BulletClass::AI; UseLineTrail laid as the missile moves | wave-3 machinery exists and only runs for flying projectiles; instant lines are faked | MAJOR (falls out of 01) |
| W4-PJ-09 | Rocket shadow drawn for `NoShadow=yes` | no shadow | grey square under V3ROCKET (`v3_rocket.png` e2) | MINOR |
| W4-PJ-10 | Interception exists but is untested and partly accidental | AA can target 50-HP `special_2` rockets (§1.3) | works vs V3 (F2 vs control); not verified for DMisl/Aegis; no regression gate | MINOR (needs a gate) |
| W4-PJ-11 | Spent rockets counted in `units_lost` | `DontScore=yes` | +1 per rocket | MINOR |
| W4-PJ-12 | V3 single-shot on a held attack order (observed once) | rearm: `SpawnRegenRate`/ROF | no second launch in 900 frames | MAJOR if confirmed |
| W4-PJ-13 | Cannon shell (`120MM`) not visible in flight | 4×4 bright dot, Shadow drawn | no pixel change between tanks across 4 sampled frames; a crater ring spawns mid-path at launch | MINOR (verify the blit palette and position) |
| W4-PJ-14 | Jump projectiles (Terror Drone DRONP, dogs, Squid) | ROT=8 bullets with Rotates art | instant/ability | MINOR |
| W4-PJ-15 | `FlakProj` impact scatter (§5.3) and the Inviso cliff/wall trace | `MoveTo` scatter / `FUN_00587AD0` | not modelled on the Instant path | MINOR |
| W4-PJ-16 | Projectile pass is O(P×U) per frame | — | `world.cpp` step_projectiles_frame / detonate | MINOR (fix together with 01) |

---

## 4. Prioritized fix list

1. **W4-PJ-01: homing flight (do first).**
   - Add `ProjKind::Homing` to `resolve_projectile_spec` (ROT>0, not Inviso) and a `integrate_homing` in `projectile.cpp` implementing §1.1 exactly:
     - launch speed 1 (§12.3);
     - accel/decel by `Acceleration` (absent → 0, but CourseLock alternation applies);
     - the `(1 + V + V·sin)·ROT` wobble from a per-bullet id plus the frame counter, ×1.5 inside 256 leptons;
     - the `FUN_005B1B20` yaw/pitch turn in 16-bit facing units;
     - `Level` keeps z fixed;
     - cruise height `min(d_cells,5)·104` (10 levels for VeryHigh/Airburst);
     - the `Arm` fuse.
   - Termination: arrival `dist ≤ 0.5·|V|`, the 60-frame give-up, and `MissileSafetyAltitude` climb-out when an air target dies (§8).
   - Re-target to the cell for ground targets.
   - AA collision: impact within `0x80` of an airborne target.
   - Route Burst shots as separate bullets.
   - Gate it behind the existing `projectiles_enabled_` firewall, with a `--demo-projectile` scenario for Patriot vs Kirov and IFV vs Rhino.
2. **W4-PJ-02 and W4-PJ-03: let the new homing bullets carry their firer.** Unit, building and weapon slot should travel with the bullet. Building launches go through `launch_defense_projectile`, which already exists. Draw the projectile from the weapon actually used (slot), not from the unit image.
3. **W4-PJ-05, -07 and -08: projectile art.**
   - Read `Rotates` from art.
   - Render voxel bullets (ZBOMB, SUBT) via `rasterize_model` at the velocity yaw/pitch.
   - Wire `Trailer`/`SpawnDelay` and real UseLineTrail on the flying bullet, and delete `instant_line`.
4. **W4-PJ-04: RocketLocomotion state machine** (§1.2). Implement pause → tilt → accelerate/climb → LazyCurve or level+dive → nose-contact explode. Draw the hull pitched. Fold in W4-PJ-09 (NoShadow), W4-PJ-11 (DontScore) and W4-PJ-12 (refire).
5. **W4-PJ-10: interception gate.** Turn `w4-proj-b`/`-c` into a regression gate covering V3 vs Patriot+Flak, DMisl vs Aegis on real water, and a control case.
6. **W4-PJ-06, -13, -14, -15, -16: polish.**
   - building-shell art;
   - the 120MM blit check and the mid-path ring;
   - jump projectiles;
   - FlakProj scatter;
   - per-cell occupancy lookup in the projectile pass.

Reproduce. Run these from PowerShell in `D:\ra2-wt\w4-proj`; scripts are in `04-evidence/scripts/`:
- `build-rel\ra2engine.exe --play scripts\play\w4-proj-land.play`
- likewise `w4-proj-a` (cannon frame by frame), `w4-proj-b` (Kirov, interception), `w4-proj-c` (no-AA control), `w4-proj-n` (naval), and `w4-perf-base`/`w4-perf-fight`.
