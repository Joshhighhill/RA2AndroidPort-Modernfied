# Effects and mission visuals: recovered behaviour (wave 3, W3-FIX-C)

Ghidra on `gamemd.exe` (private project copy `D:\ra2-re\w3fix-c`, outputs
`D:\ra2-re\w3fix-c-out\`, scripts `dec.py`/`vt.py`/`range.py`/`disp.py`/
`dis.py`/`rulesuse.py`/`refs.py`). Addresses are YR 1.001; field offsets were
checked against the INI readers. RA2 `game.exe` was not disassembled: where the
RA2 data differs from YR the choice is stated. Implementation:
`engine/pres_effects.inc` (particles, debris, trails, wakes, twinkles),
`engine/campaign_visuals.inc` (trigger visuals), `engine/pres_game.inc` hooks.

Confidence: **High** = read in the decompile; **Medium** = read, one detail
inferred; **RE-FALLBACK** = not recovered, the clone's stand-in is stated.

## 1. Particle systems

`ParticleSystemClass` (vtable `0x7EEB9C`, size 0x100), ctor `FUN_0062d6f0(type,
coord, target, owner, targetCoords, house)`, Update = vtable[23] `FUN_0062f800`.
`ParticleClass` (vtable `0x7EE954`, size 0x138), ctor `FUN_0062b080(type, coord,
coord2, system)`. Type fields: `ParticleSystemTypeClass` HoldsWhat +0x294,
Spawns +0x298, SpawnFrames +0x29C, Slowdown +0x2A0, ParticleCap +0x2A4,
SpawnRadius +0x2A8, SpawnCutoff +0x2AC, SpawnTranslucencyCutoff +0x2B0,
BehavesLike +0x2B4, Lifetime +0x2B8, SparkSpawnFrames +0x300.

| Item | Behaviour | Conf. |
|---|---|---|
| Update | switch BehavesLike: 0 Smoke `FUN_0062e7e0`, 1 Gas `FUN_0062e170`, 2 Fire `FUN_0062f440`, 3 Spark `FUN_0062e2e0`, 4 Railgun `FUN_0062ecd0`; then `Lifetime--`, at 0 the system is removed; a `TimeToDie` system with no particles is removed | High |
| System ctor | `Accum = (float)SpawnFrames`, `Lifetime`, `SparkSpawnFrames` copied; with an owner, `SpawnDistanceToOwner = Location - owner.Coords` (the system follows it) | High |
| Smoke system | each frame: follow the owner; run every particle; every `(int)Accum` frames spawn `HoldsWhat` at `Location + (Random % (SpawnRadius+1), same, +10)`; past `SpawnTranslucencyCutoff` the new puff's translucency +25; its speed `-= (Accum - SpawnFrames) * k`, floor 2.0; `Accum += Slowdown`; `Accum > SpawnCutoff` -> TimeToDie | High (k: RE-FALLBACK 1.0) |
| Particle ctor | Speed = Velocity, RemainingEC = MaxEC + Random % MaxEC (Railgun: + Random % 10), RemainingDC = MaxDC, state = StartStateAI, translucency = Translucency | High |
| Particle AI `FUN_0062c8e0` | behaviour step, then RemainingEC--; 0 -> delete | High |
| Smoke particle `FUN_0062b7f0`/`FUN_0062cd40` | even frames: with 1/8 chance a +-1 step of a +-2-lepton x or y drift; odd frames: move (velocity matrix, cell/bridge collision) + drift + wind (WindEffect, Rules wind table); the state (= image frame) advances every StateAIAdvance (+0/1) frames; at EndStateAI: DeleteOnStateLimit -> delete, else 0 | Medium; the velocity matrix is RE-FALLBACK: the clone rises `Speed` every odd frame, `Speed -= Deacc` (floor 1) |
| Spark system | while SparkSpawnFrames > 0: on the last frame or with SpawnSparkPercentage, `Cap/2 + Random % (Cap/2)` sparks, velocity `(Random % XVelocity, Random % YVelocity, MinZVelocity + Random % ZVelocityRange)` (+SpawnDirection); then SparkSpawnFrames--, at 0 TimeToDie | High |
| Spark particle `FUN_0062c180` | `vz -= Rules Gravity`, move, ground hit ends it; colour walks ColorList at ColorSpeed | Medium (drawn as a 2x2 dot) |
| Railgun | particles along the shot at ParticlesPerCoord on a SpiralRadius spiral, Laser line | RE-FALLBACK (perturbation terms not unwound) |
| Gas / Fire | Gas particles carry `Damage=` + `Warhead=`; RA2 ships no weapon that makes them (Gas2 is used only by the unused `Vortex` / `ChemLauncher`), FireStreamSys is unused | High (data); drawn like smoke |

**Damage smoke** (`TechnoClass::ReceiveDamage` `FUN_00700da0`): when the health
ratio is at or below `ConditionYellow` (Rules+0x1700) and the damage result is
NowYellow (2) or NowRed (3), and the object has no smoke system (+0x310), one
random `BehavesLike=Smoke` entry of `DamageParticleSystems` (TechnoType +0x778)
is created at `Location + DamageSmokeOffset` (+0x7B0; `DamSmkOffScrnRel` +0x7BC
converts a screen offset, `FUN_00716c60`), owned by the object. A
ReceiveDamage that leaves the ratio above ConditionYellow removes it.
`TechnoClass::AI` `FUN_006f9380` adds Spark-behaving entries only for
`DamageSparks` (+0xC8F, false unless set in code -- the cyborg infantry), so
the `SparkSys` entries of RA2 vehicles/buildings are inert. **High**. The clone
observes health-condition edges (a worsening edge spawns, ratio > yellow
removes); it does not see a hit that keeps the condition.

**Trigger action 88 ParticleAnim** (Execute case 0x58): `ParticleSystemTypes[Value]`
at the waypoint cell centre on the ground, no owner. **High**.

**Data source.** The original is RA2: the effect keys (`DamageParticleSystems`,
`DamageSmokeOffset`, debris keys, `[ParticleSystems]`/`[Particles]` sections)
are read from the RA2 `rules.ini` (expand01) first, rulesmd for YR-only
sections. YR comments out `GACNST DamageParticleSystems` and drops
`LargeRedSmoke`/`BigRedSmokeSys`; `FLGRYSMK1.SHP` exists only in `ecache01.mix`.

## 2. Debris

`TechnoClass::ReceiveDamage` death branch: skipped when the object is below
height 11 on a water cell. `n = Random(MinDebris, MaxDebris-1)` (+0x5C0/+0x5BC).
With `DebrisTypes` (+0x324 count): cycling the list, each takes
`Random % (DebrisMaximums[i]+1)` (capped at n) `VoxelAnimClass`es
(`FUN_007481e0`). Then, without `DebrisAnims` (+0x5D4) and without
DebrisTypes, n anims from `[General] MetallicDebris` (Rules+0x140); with
DebrisAnims, n random DebrisAnims; both at Location + 20 z. **High**. Buildings
take the same path (RA2 buildings: MinDebris 5 / MaxDebris 15).

**Bouncer anims** (`AnimClass` ctor `FUN_00421ec0`, `BounceClass::Initialize`
`FUN_00439760`, `Update` `FUN_00439a80`): velocity x,y = `Random % 2*MaxXYVel -
MaxXYVel`, z = `MinZVel + Random % (MaxZVel - MinZVel)`; gravity **1.4**
leptons/frame^2 (constant `0x3FF6666660000000`); on the ground the velocity
reflects off the slope normal and scales by Elasticity; too slow -> Impact ->
`ExpireAnim`. `AnimClass::AI` `FUN_00423ae0` spawns `TrailerAnim` every
`TrailerSeperation` frames (`frame % sep == 0`). **High**; the Impact speed
threshold is RE-FALLBACK (4 leptons/frame). The debris `Damage=` (DBRIS*LG 20
HE, PIECE 5) on impact is **not** applied: the clone keeps debris cosmetic.
Voxel debris (TIRE / AXLE) tumble by a quaternion in the original; the clone
spins the voxel about its vertical axis (16 yaws) [RE-FALLBACK].

## 3. Trails

- **Projectile `Trailer=`** (BulletTypeClass art reader `FUN_0046bf4c`: Trailer
  +0x2D8, SpawnDelay +0x2E4 default **3**): `BulletClass::AI` `FUN_004666d0`
  spawns it when `Frame % SpawnDelay == 0`. **High**.
- **`UseLineTrail`** (DRAGON: IFV, Patriot, Harrier missiles): a polyline of
  the last 32 positions in `LineTrailColor`, each point fading by
  `LineTrailColorDecrement` per frame [Medium: LineTrail class not
  disassembled]. The clone resolves the IFV's HoverMissile instantly, so its
  trail is laid along the shot at fire time [RE-FALLBACK].
- **V3 rocket / Dreadnought missile** (RocketLocomotionClass `FUN_00661d00`,
  anim names at `0x8389A4`/`0x8389A8`): `V3TAKOFF` every 24 frames (delay 2,
  z -10) while on the launcher, then `V3TRAIL` every 3 frames in flight.
  **High**. (The skirmish V3 tank folds its rocket into the shot, W2-SK-13, so
  only spawned `V3ROCKET`/`DMISL` units trail.)

## 4. Water and ore

- **Wake** ([General] `Wake=`, Rules+0x94; ship locomotors `FUN_0069f2a0`
  and siblings): every 8 frames (`Frame & 7 == 0`) while moving, not in limbo,
  on a water cell. **High**. Drawn in a ground pass (WAKE1 is `Flat`).
- **Splash** (`MapClass::SelectDamageAnimation` `FUN_0048a400`): a
  `Conventional` warhead landing on a water cell, not on a bridge and below
  cell height + 2 levels, shows `SplashList[min(damage, n*35-1)/35]` instead of
  its AnimList. **High**.
- **Ore twinkle** (scenario start `FUN_00684670`): each ore cell gets
  `OreTwinkle` (Rules+0x1870) with chance `1/OreTwinkleChance` (+0x186C);
  TWNK1 has `LoopCount=-1`, `RandomLoopDelay=120,300`, `HideIfNoOre`. **High**.
- **AmbientSound** (ObjectTypeClass +0x1F4; `ObjectClass::AI` `FUN_005f3680`):
  while not in limbo the object keeps the sound playing through its sound
  controller (+0x3C, `FUN_0074f800`) at its coords. **High**. The clone keeps
  one positional voice per object and restarts it when it ends.

## 5. Trigger visual actions (`TActionClass::Execute` `FUN_006dce20`)

| Kind | Function | Behaviour | Conf. |
|---|---|---|---|
| 41 PlayAnimAt | `FUN_006e1800` | `AnimTypes[Value]` at the waypoint cell centre at ground height (+ bridge height), loops 1, flags 0x600 | High |
| 42 DoExplosionAt | `FUN_006e1900` | `WeaponTypes[Value]`: its warhead's impact anim (SelectDamageAnimation), screen shake, `DamageArea` with the warhead | High; the WeaponTypes order is RE-FALLBACK |
| 48 MoveCameraToWaypoint | `FUN_006d1990` | Tactical pan to the waypoint; progress per frame = table `0x84188C[Value]` = {0.0015, 0.003, 0.0075, 0.03, 0.06} | High (linear interpolation: Medium) |
| 55 CreateRadarEvent | `FUN_0065f4b0` | `RadarEvent::Create(type = Value, cell)` -> the minimap zoom square (presentation.md §1.2) | High |
| 63 Apply100Damage | `FUN_006dfa00` | five `DamageArea(100, C4Warhead)` at the waypoint and +-0x55 leptons diagonally; **no anim of its own** | High |
| 88 ParticleAnim | case 0x58 | see §1 | High |
| 90 LightningStrikeAt | `FUN_00539e70` | a `WeatherConClouds` cloud at the cell, registered with the storm's cloud lists; its bolt strikes the cell | Medium |
| 102 | case 0x66 | unless `LightningStorm::IsActive`, `LightningStorm::Start(cell, triggerHouse)` | High |
| 104 FlashTeam | `FUN_006f1490` | every member `Flash(Value)` | High |
| 112 CenterCamera | `FUN_006d55e0` | the view jumps to the waypoint | High |
| 113 MakeHouseCheer | `FUN_0050c6e0` | every infantry of the house runs its Cheer (vtable +0x388), plus `[AudioVisual] CheerSound` | High |

### 5.1 PlayAnimAt / ParticleAnim index spaces

The AnimTypeClass and ParticleSystemTypeClass arrays are find-or-allocated in
list order (`FUN_00668630`), so duplicate names take no slot. Census of all 26
RA2 maps: PlayAnimAt uses 479 (bridge huts, airport, GI hints), 471 ("Arrows -
Garrison"), 253 (every "Chrono" trigger, ALL12 + SOV12) and 238 (SOV01 burning
building); ParticleAnim uses 2, 11 ("DamagedIFVSparks"), 12.

| Index | ra2.mix/local.mix rules.ini, dedup | expand01 rules.ini, dedup | expand01 raw |
|---|---|---|---|
| 479 | **ARRWSE** (points at the hut below the waypoint) | ARRWNE | ARRWS |
| 471 | **ARRWDEST** | CAWA19_AG | NAMISL_FD |
| 253 | **CHRONOTG** | CHRONOTG | WAKE1 |
| 238 | **FLAMEGUY** | FLAMEGUY | DEATH_E |

Only the local.mix list fits every use. ParticleAnim 12 exists only in the
expand01 `[ParticleSystems]` (BigRedSmokeSys; local.mix has 12 entries). The
clone therefore reads PlayAnimAt against the local.mix `[Animations]` and
ParticleAnim against the expand01 `[ParticleSystems]`, both deduplicated
[RE-FALLBACK: chosen by the census; game.exe not disassembled]. (The wave-3
audit's "SOV12 ship wakes" were this misreading: SOV12's 8 PlayAnimAt are
CHRONOTG chrono flashes.)

### 5.2 Not visible in the original either

- 63 Apply100Damage (ALL01 "Break Bridges" x10 at the two bridge approaches):
  no anim; `DamageArea` with `C4Warhead=Super` (Wall=no) cannot hit low
  bridges, and high bridges (overlay 74..99, 205..230) are destroyed with
  `Random(1, BridgeStrength) < 100`. The clone has no destructible high
  bridges (W2-GA-07), so its "bombardment" stays the single-cell damage
  deviation of campaign_runtime.inc (unchanged; the ALL01 gate depends on it).

### 5.3 RE-FALLBACKs

- 42: the WeaponTypes array is allocated as TechnoTypes read their INI; that
  read order was not recovered. The clone uses the RA2 rules.ini file order of
  Primary/Secondary/Elite*/Occupy/DeathWeapon keys.
- 90: the cloud's manifest delay before the bolt is skipped (the bolt lands
  at once).
- 104/131: the clone pulses an additive white diamond over the footprint on
  alternate 2-frame beats; the original redraws the object brightened.
- 113: the infantry Cheer is the World's (`World::make_house_cheer`, W3-B);
  the scene adds CheerSound.
- 115 FlashCameo: not drawn.
