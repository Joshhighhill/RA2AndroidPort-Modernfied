# Veterancy + Crates — reverse-engineering notes

(Method: `docs/re/workflow.md`; image base `0x00400000`, YR 1.001 `gamemd.exe`,
SHA1 `7BE4E5508655F8AF482AB3FF5965D196E1B6B6C7`. Hunt scripts
`D:\ra2-re\scripts\vet_hunt1.py … vet_hunt14.py`, `vet_dis.py`, `vet_fpu.py`;
dumps in `D:\ra2-re\out\p5\`. The Ghidra project was COPIED to
`D:\ra2-re\p5proj\ra2p5` so this hunt could run headless without contending for
the shared project lock (other phase agents were using `D:\ra2-re\ra2`). The
rules ground truth is the same extracted `rulesmd.ini` the repair-sell hunt used
(`D:\ra2-re\out\shroud_evidence\rulesmd.ini`, 742 958 B, extracted with
`ra2extract "ra2md.mix" --raw "localmd.mix/rulesmd.ini"`). Nothing off-repo is
committed.)

**Content-anchored only.** Every function below was reached from (a) the
RulesClass `[General]` loader `FUN_0066cf70` field offsets (`VeteranRatio`
… `VeteranCap` at `Rules+0x668..0x698`), scanned as FP operands across the whole
`.text`; (b) the TechnoTypeClass loader `FUN_00711510` offsets (`Trainable`
`+0xc8e`, `VeteranAbilities` `+0x29c`, `EliteAbilities` `+0x2ae`,
`SelfHealing` `+0xd14`, weapon arrays `+0x898` / elite `+0xa94`), scanned as byte
operands; (c) INI key strings (`CrateRules`, `Powerups`, `PIPS.SHP`,
`EliteFlashTimer`, `CrateGoodie`, …) and their xrefs. YRpp addresses were used
only as hints and were **wrong for this build every time they were tried**
(`HasAbility 0x70D0D0` → `0x70C550`; `CombatDamage 0x6F3970` is a ctor here;
`GetPrimaryWeapon 0x70E1A0` is a timer helper) — the workflow §4 drift lesson,
again.

This is the calibration authority for the **P5 Veterancy + Crates** phase.

---

## §0 — Ground truth (this install's `rulesmd.ini`, verbatim)

```
[General]
VeteranRatio=3.0       ; must destroy this multiple of self-value to become a veteran [per level]
VeteranCombat=1.1       ; multiplier to damage
VeteranSpeed=1.2        ; multiplier to max speed
VeteranSight=0.0        ; multiplier to sight !!!going past ten is a hard code Vegas crash!!!
VeteranArmor=1.5        ; ... (in reality, damage is divided by this
VeteranROF=0.6          ; ROF delay multiplier
VeteranCap=2            ; maximum veteran level that can be obtained
InitialVeteran=no
RepairRate=.016         ; (also the self-heal cadence -- §V8)

[AudioVisual]
EliteFlashTimer=150     ;gs Frames that a newly Elite unit will flash for

[CrateRules]
CrateMaximum=255
CrateMinimum=1
CrateRadius=3.0
CrateRegen=3
SilverCrate=HealBase   SoloCrateMoney=5000   UnitCrateType=none
WoodCrate=Money        WaterCrate=Money      FreeMCV=yes
CrateImg=CRATE  WoodCrateImg=CRATE  WaterCrateImg=WCRATE  HealCrateSound=HealCrate

[Powerups]          ; weight, anim, water-ok, argument
Armor=10,ARMOR,yes,1.5      Firepower=10,FIREPOWR,yes,2.0   HealBase=10,HEALALL,yes
Money=20,MONEY,yes,2000     Reveal=10,REVEAL,yes            Speed=10,SPEED,yes,1.2
Veteran=20,VETERAN,yes,1    Unit=20,<none>,no
Invulnerability=0 IonStorm=0 Gas=0 Tiberium=0 Pod=0 Cloak=0 Darkness=0
Explosion=0 ICBM=0 Napalm=0 Squad=0          ; all weight 0
```

Per-type keys (examples; `[E1]`): `Cost=200`, `Primary=M60`, `ElitePrimary=M60E`,
`VeteranAbilities=STRONGER,FIREPOWER,ROF,SIGHT,FASTER`,
`EliteAbilities=SELF_HEAL,STRONGER,FIREPOWER,ROF`. Vehicles typically omit `ROF`
at veteran (`[MTNK] VeteranAbilities=STRONGER,FIREPOWER,SIGHT,FASTER`). 81 types
set `Trainable=no` (dogs, engineers, …); `[HARV]`/`[APOC]` set `SelfHealing=yes`.
The `[Country]` `CostUnitsMult`/`CostInfantryMult` keys are all commented out,
so `GetActualCost(house) == Cost` on this install.

## §1 — Summary

| # | Item | Evidence | Confidence |
|---|------|----------|:----------:|
| V1 | **Veterancy is a `float` at `Techno+0x150`**; ranks: veteran ⇔ `1.0f <= v < 2.0f`, elite ⇔ `v >= 2.0f`, rookie otherwise; negative = "rookie(-0.25)" | `IsVeteran 0x74EDB0`, `IsElite 0x74EE30`, `GetRank 0x74EE50` (consts `[0x7E1AC8]=1.0f`, `[0x7E27B4]=2.0f`); setters `0x74EEA0/0x74EEB0/0x74EED0/0x74EE80` | **CONFIRMED** |
| V2 | **Experience add** `v = min(VeteranCap, v + victimValue / (ownerCost * VeteranRatio))` — computed in double (PC_53), compared to the cap on the UNROUNDED sum, stored as `float` | `FUN_0074ED70` disassembly | **CONFIRMED** |
| V3 | **Kill credit** (`FUN_007021D0`, the techno death/RegisterDestruction path; buildings included): `victimValue = victimType.GetActualCost(victimHouse)`, **×2 if the victim was veteran, ×3 if elite, 0 if the killer's house is allied with the victim**; skipped entirely if the victim type has `DontScore` (`+0xc9f`); credited to the killer iff the killer's type is `Trainable` (`+0xc8e`) with `ownerCost = killerType.GetActualCost(victimHouse)` | disassembly `0x7021D0..0x702480` | **CONFIRMED** |
| V4 | **Ability test** `HasAbility(a)` = `(veteran && Vet[a]) || (elite && (Vet[a] || Elite[a]))` — elite INHERITS the veteran list | `FUN_0070C550` | **CONFIRMED** |
| V5 | **Firepower**: at fire, `d = ftol(houseFP * FirepowerMultiplier * Damage)`; if `HasAbility(FIREPOWER)` `d = ftol(d * VeteranCombat)` | `FUN_006FD210` `0x6FD800..0x6FD8A3` | **CONFIRMED** |
| V6 | **Armor**: in `TechnoClass::ReceiveDamage` (`FUN_00700DA0`), for non-negative, non-ignore-defenses damage: `d = ftol(d / (houseArmor * ArmorMultiplier))`; if victim `HasAbility(STRONGER)` `d = ftol(d / VeteranArmor)`; **then `d = max(d, 1)`**; the warhead Verses are applied AFTER (base `ObjectClass` path; the estimator `FUN_006FD040` shows the identical order ending in `ModifyDamage 0x489090`) | `0x700DA0..0x700E83`; `FUN_006FD040` | **CONFIRMED** (order: CONFIRMED by the estimator mirror) |
| V7 | **ROF**: `r = ftol(ROF * houseROF + Random(0,2))`; if `HasAbility(ROF)` `r = ftol(r * VeteranROF)` | `FUN_006FC460` `0x6FC55E..0x6FC60C` | **CONFIRMED** — see §6 **contradiction** with combat.md §5 |
| V8 | **Speed**: `s = ftol(maxSpeed * houseSpeed * SpeedMultiplier)`; if `HasAbility(FASTER)` `s = ftol(s * VeteranSpeed)`; `s = ftol(s * SpeedPercentage)` | `FUN_004DB0B0` | **CONFIRMED** |
| V9 | **Sight**: `if (VeteranSight != 0.0 && HasAbility(SIGHT)) sight = ftol(sight * VeteranSight)` — with this install's `VeteranSight=0.0` the branch is **dead** (sight never changes) | `0x70A349..0x70A371` (`[0x7E1800]=0.0`) | **CONFIRMED** (inert) |
| V10 | **Elite weapons**: `GetWeapon(i)`: if elite and `EliteWeapon[i].WeaponType != null` → elite slot, else normal slot. `ElitePrimary` absent ⇒ null (ctor) ⇒ normal weapon | `FUN_0070D570` → `0x716B80`/`0x716B60`; ctor `FUN_0070FE90` | **CONFIRMED** |
| V11 | **Self-heal**: `ShouldSelfHealOneStep` (vtable `+0x294`) = `(type.SelfHealing || HasAbility(SELF_HEAL)) && Frame % ftol(RepairRate*900) == 0 && Health != Strength && Health != 0`; the caller (`TechnoClass::Update`) then does `Health += 1` | `FUN_0070B300`; caller `FUN_006F9380` `0x6F9C6D` | **CONFIRMED** |
| V12 | **Promotion events**: `Update` compares the stored last rank (`+0x13c`, initialised `-1`) with `GetRank`; on change (not from -1): to **elite** → (player-owned: sound + EVA) and `flash = EliteFlashTimer` (150 frames, `+0xf0`); to **veteran** → (player-owned: sound + EVA) | `FUN_006F9380` L106-133 | **CONFIRMED** |
| V13 | **Chevron**: `DrawVeterancyPips` (vtable `+0x454`) draws `PIPS.SHP` frame **14 (veteran) / 15 (elite)** (19 for a negative "rookie"), at the object draw point **+(5,2)** for infantry, **+(10,6)** for everything else, flags `0xE00`; drawn every frame (not only when selected) unless disguised-as / cloaked | `0x709E10..0x709EDB`; caller `0x6F48E2` | **CONFIRMED** (palette PROBABLE: `PALETTE.PAL` by YRpp-offset analogy) |
| V14 | `Trainable` defaults **yes**; abilities default **all false**; `ElitePrimary` defaults null | TechnoType ctor `FUN_0070FE90` (`+0xc8e=1`, `+0x29c..+0x2bf` zeroed) | **CONFIRMED** |
| V15 | **Produced veterans** (house-type `VeteranUnits/Infantry/Aircraft` lists, or the spied-factory house latch `house+0x2c0`) are created with `SetVeteran` = `Veterancy=1.0f`, and only if the type is `Trainable` | `FUN_00734490` L90-111 (units), `FUN_00413F80` (infantry), `FUN_00442BC0` (buildings) | **CONFIRMED** (mechanism); corrects capture.md §B6 — see §6 |
| V16 | **FPU precision**: the CRT sets `_controlfp(_PC_53, _MCW_PC)` at startup (`0x7CD8AF`); no game code changes the precision → every x87 op above rounds to IEEE double, so plain C++ `double` arithmetic (one op per statement, no FMA contraction) reproduces it bit-for-bit | `vet_fpu.py` scan | **CONFIRMED** |
| C1 | **Crate RNG** = `ScenarioClass::Random` (`[0xA8A198]+0x218`), a 250-word **R250** generator (taps 0/103) seeded by a 4-round **psdes** hash (Numerical Recipes constants `c1={baa96887,1e17d32c,03bcdc3c,0f33d1b2}`, `c2={4b0f3b58,e874f0c3,6955c5a6,55a7ca46}`); `Random(min,max)` = rejection-sample `next() & mask(bitlen(max-min))` until `<= max-min` | `FUN_0065C110` (seed), `FUN_0065C1C0` (next), `FUN_0065C220` (ranged), tables `0x83863C`/`0x83868C` | **CONFIRMED** |
| C2 | **Crate count at game start** = `clamp(players, CrateMinimum, CrateMaximum)` placed with `PlaceRandomCrate`, only when the Crates game option is on | `FUN_006862D0` L50-62 | **CONFIRMED** (player count = session humans, PROBABLE) |
| C3 | **PlaceRandomCrate** (`FUN_0056B870`): needs a free slot in `Crates[256]`; up to **1000** tries of `cell = (MapRect.X + Random(0,W-1), MapRect.Y + Random(0,H-1))` → `NearbyLocation(cell, water ? Float : Track)` → `PlaceCrate(cell)` | `FUN_0056B870`, `FUN_004A16D0`, `FUN_004A1800` | **CONFIRMED** (NearbyLocation internals: [RE-FALLBACK]) |
| C4 | **Crate lifetime** `= ftol((A - B) * (Random(0,0x7FFFFFFE) * (1/0x7FFFFFFE)) + B)` frames, `A = CrateRegen*1800`, `B = CrateRegen*450` (CrateRegen=3 → 1350..5400 frames); **on expiry the crate is removed and a new random crate placed** | `0x4A1760..0x4A17D5`; `UpdateCrates FUN_0056B710` | **CONFIRMED** |
| C5 | **Pickup** (`CellClass::CollectCrate FUN_00481910`): powerup = cumulative-weight pick `Random(1, ΣWeight)`; the multiplayer **conversion rules** (§3.3) may turn it into Money; then the crate is removed **and a replacement placed BEFORE the effect runs**; then the effect | `FUN_00481910` | **CONFIRMED** |
| C6 | **Effects** (multiplayer): Money `+Random(arg, arg+900)` to the collector's house; HealBase: every building of the collector's house to full; Reveal: whole map for the house; Armor/Firepower: **every Techno of ANY house** within `CrateRadius` with mult `== 1.0` gets `mult *= arg`; Speed: same, **Foot** only, not aircraft; Veteran: every `Trainable` Techno in radius gains `arg` levels (rookie→veteran→elite); Unit: a random `CrateGoodie=yes` vehicle type | `FUN_00481910` cases | **CONFIRMED** |
| C7 | `CrateRadius` is read in **leptons** (`ReadLeptons`, `3.0` cells → `768`); the area test is `ftol(sqrt(dx²+dy²+dz²)) < radius` | `0x66B4C3`, `0x482853` | **CONFIRMED** |
| C8 | `CrateGoodie` defaults **no** (UnitType ctor `0x745F39` zeroes `+0xe0d`) | `FUN_00745F00` | **CONFIRMED** |

## §2 — Veterancy details

### V2 — `VeterancyStruct::Add(ownerCost, victimCost)` (`0x74ED70`)

```
FILD  victimCost            ; ST0 = victim
FILD  ownerCost             ; ST0 = owner, ST1 = victim
FMUL  Rules.VeteranRatio    ; ST0 = owner*ratio              (double, PC_53)
FDIVP                       ; ST0 = victim/(owner*ratio)
FADD  dword [this]          ; + (float)v
FST   dword [this]          ; store (float) -- overwritten below
FLD ST0 ; FLD Rules.VeteranCap ; FCOMPP     ; compare cap vs the DOUBLE sum
cap <= sum ? store (float)cap : store (float)sum
```

So `ownerCost == 0` divides by zero → `+inf` → capped to `VeteranCap` (2.0): a
free unit that kills anything goes straight to elite. Reproduced as-is.

Worked example (this install): E1 (Cost 200) kills an E2 (Cost 100):
`100 / (200*3.0) = 0.1667` → six E2 kills = 1.0 = veteran. Killing a
veteran E2 counts double (`200/600`).

### V3 — the kill-credit branch (`0x7021D0`)

```
value = victimType->GetActualCost(victim->Owner)        ; vtbl +0x84 on the type
if victimType.DontScore: return                        ; +0xc9f
if killer:
    if killer->Owner->IsAlliedWith(victim): value = 0  ; FUN_004F98B0
    elif victim.IsVeteran: value *= 2
    elif victim.IsElite:   value *= 3
    trainee = (killer is a spawned/occupant with a Trainable owner) ? that owner
            : killerType.Trainable ? killer : (gunner/transport cases) : none
    trainee.Veterancy.Add(traineeType->GetActualCost(victim->Owner), value)
```

The owner-cost lookup is passed the VICTIM's house (a Westwood quirk;
irrelevant here because no country cost multiplier is active). The spawner /
occupant / gunner re-routing branches target systems this engine does not have
(spawners, garrisons, IFV gunners) — out of scope.

### V5/V6 — damage pipeline order (the exact integer chain)

```
fire:     d0 = ftol(1.0 * FirepowerMultiplier * Damage)
          if HasAbility(FIREPOWER): d0 = ftol(d0 * VeteranCombat)
receive:  d1 = ftol(d0 / (1.0 * ArmorMultiplier))       ; only if d0 >= 0 and !IgnoreDefenses
          if victim.HasAbility(STRONGER): d1 = ftol(d1 / VeteranArmor)
          d1 = max(d1, 1)
verses:   d2 = ModifyDamage(d1, warhead, armor)          ; = engine damage_vs (combat.md §3)
```

The estimator `FUN_006FD040` (used for target evaluation) spells out the same
chain in one function and calls `FUN_00489090` (Verses) **last**, which pins the
order. `houseFP`/`houseArmor` (`HouseClass+0x188`, `FUN_0050BB50`) are 1.0 in
skirmish (no country bonuses active in this rules file).

**Note the `max(d,1)` clamp**: it applies to EVERY hit through
`TechnoClass::ReceiveDamage`, not just veterans. The current engine's
`damage_vs` chain lacks it; adding it globally would change existing gates, so
the clone applies it only on the firewalled veterancy path (§5 of the spec).

### V7 — ROF

```
r = ftol(ROF * house.ROFMultiplier + Random(0,2))   ; [0xA8A198]+0x218 = Scenario RNG
if HasAbility(ROF): r = ftol(r * VeteranROF)
(then the occupant/IsInAir and Rules+0xf44/0xf50 multipliers -- unrelated systems)
```

E1 M60 `ROF=20` → veteran `ftol(20*0.6) = 12` (ignoring the jitter).

### V8 — Speed

`s = ftol(type.Speed * houseSpeed * SpeedMultiplier)`, FASTER →
`ftol(s * 1.2)`, then `ftol(s * SpeedPercentage)`. E1 Speed 4 → veteran
`ftol(4.8) = 4` (**no visible gain** for 4-speed infantry); MTNK Speed 7 →
`ftol(8.4) = 8`.

### V11 — self-heal

`ftol(RepairRate * 900.0)` with `RepairRate=.016` → `ftol(14.4) = 14`: on every
global frame divisible by 14, every techno whose type is `SelfHealing=yes`
(`[HARV]`, `[APOC]`) or which HAS `SELF_HEAL` (all elite infantry/vehicles in
this file via `EliteAbilities=SELF_HEAL,...`) and is damaged-but-alive gains
**+1 HP**. The same 14-frame global phase as the repair wrench
(`docs/re/repair-sell.md §A2`). Limbo (transported) objects return before the
check.

### V12/V13 — promotion feedback + chevrons

Promotion feedback is per-frame rank-change detection in `TechnoClass::Update`
(stored previous rank `+0x13c`, initial `-1` so a unit spawned veteran does not
announce). Elite additionally arms a 150-frame flash. Chevrons are `PIPS.SHP`
frames 14/15 drawn at a fixed offset from the object's draw point.

## §3 — Crates details

### 3.1 RNG (`RandomClass`, C1)

```
seed(s):  idx1 = 0; idx2 = 103
          for i in 0..249:  (lw, rw) = (i, s)          ; psdes, 4 rounds k=0..3
              u = c1[k] ^ lw;  hi = (int32)u >> 16 (SAR);  lo = u & 0xffff
              t = ~(hi*hi) + lo*lo                       (int32 wrap)
              n = ((((int32)t >> 16) | (t << 16)) ^ c2[k]) + hi*lo) ^ rw
              rw = lw; lw = n
              table[i] = lw
next():   table[idx1] ^= table[idx2]; r = table[idx1]; idx1,idx2 = (+1) mod 250
ranged(a,b): if a==b return a; order; range = b-a; m = bitlen(range)
             do r = next() & ((1<<m)-1) while (int)r > range; return a + r
```

### 3.2 Placement / lifetime (C2–C4)

See the table. `4A1800` (the placement predicate) requires the cell to accept an
overlay (`FUN_00577E90(cell,1)`) and have none; on water it uses `WaterCrateImg`.

### 3.3 Pickup conversion rules (multiplayer branch, in order)

1. If the house owns no buildings, has `> 1500` credits, owns no MCV and the
   MCV-redeploy option is on → **forced Unit (MCV)**. *(CUT: no MCV/deploy.)*
2. `Unit`: house unit count `> 50` → Money; cell LandType Water(2)/Beach(6) → Money.
3. `Cloak`: collector already cloaked → Money. `Squad`: infantry `> 100` or water → Money.
4. `Armor`: collector `ArmorMultiplier != 1.0` → Money.
5. `Speed`: collector `SpeedMultiplier != 1.0` or aircraft → Money.
6. `Firepower`: collector `FirepowerMultiplier != 1.0` or unarmed → Money.
7. `Veteran`: collector not `Trainable` → Money; collector elite → Money.
8. Water cell and `!Powerups[p].Naval` → Money.
9. `Squad` is always converted to Money afterwards.

### 3.4 Effects (C6) — exact

| Powerup | Effect |
|---|---|
| Money | `credits += Random(ftol(arg), ftol(arg)+900)` (2000..2900) |
| Unit | `UnitCrateType` if set, else re-roll `UnitTypes[Random(0,n-1)]` until `CrateGoodie=yes` (skipping `BaseUnit` MCVs); unlimbo at the crate cell, else `NearbyLocation` |
| HealBase | every building of the collector's house → full strength |
| Reveal | the whole map revealed for the collector's house |
| Armor / Firepower | every Techno (any house!) with `ftol(dist) < CrateRadius` and multiplier exactly 1.0 → `mult *= arg` (1.5 / 2.0) |
| Speed | same, Foot only and not aircraft, `SpeedMultiplier *= 1.2` |
| Veteran | every `Trainable` Techno in radius: `arg` times { veteran→elite; rookie→veteran; negative→rookie } |

## §4 — What maps where in the clone

| Original | Clone |
|---|---|
| `Techno+0x150` float | `Unit::veterancy` (float) |
| `VeteranAbilities/EliteAbilities` bool[18] | 18-bit masks on `UnitCombat`/`Unit` |
| `FirepowerMultiplier/ArmorMultiplier/SpeedMultiplier` | `Unit::firepower_mult/armor_mult/speed_mult` |
| `ScenarioClass::Random` | `ra2engine::GameRandom` (exact R250/psdes port), crate-only instance |
| Frame | a World frame counter advanced every `step` |

## §5 — Out of scope / cut (with reason)

* **ROF `Random(0,2)` jitter** (V7): would require a sync RNG draw in
  `combat.cpp` and changes every combat gate; reported, not applied (§6).
* **Promotion sound/EVA, elite flash** (V12): audio events have no EVA queue
  wired for this; the flash is cosmetic. The rank-change EVENT is recorded.
* **SCATTER, CLOAK, SENSORS, EXPLODES, C4, RADAR_INVISIBLE, FEARLESS,
  TIBERIUM_*, GUARD_AREA, CRUSHER abilities**: no underlying systems.
* **Zero-weight powerups** (Cloak, Explosion, Napalm, Squad, Darkness, ICBM,
  Invulnerability, IonStorm, Gas, Tiberium, Pod): unreachable by the weighted
  pick with stock weights; only reachable from map-placed typed crates, which
  this engine does not load.
* **Forced-MCV crate** (rule 1): needs MCV deploy machinery.
* **Spawner/occupant/gunner kill re-routing** (V3): no such systems.
* **`NearbyLocation`** internals: [RE-FALLBACK] nearest-ring search over the
  engine's passability grids; `MapRect` approximated by the grid bounds.

## §6 — Contradictions / corrections to earlier docs

1. **combat.md §5 "CONFIRMED no ROF jitter/RNG"** — `FUN_006FC460` (reached
   from the `Rules+0x690 VeteranROF` FMUL, and the only function multiplying by
   it) draws `ScenarioClass::Random(0,2)` and FIADDs it into the ROF before
   `_ftol`; the between-burst delay is `Random(3,5)`. The rearm timer
   therefore jitters by 0..2 frames in the original. Not changed here (would
   move every combat gate); flagged for a future combat-fidelity pass.
2. **capture.md §B5/§B6** — the house bytes `+0x2bc..+0x2c0` that the
   infiltration branch sets are read back at unit/building CREATION
   (`FUN_00734490` checks `house+0x2c0` → `SetVeteran`), i.e. at least `+0x2c0`
   is the spied-factory veteran latch, not a radar marker. The produced unit
   gets `Veterancy = 1.0f` (veteran rank via the normal rank thresholds), not
   spawn-baked ×1.1 damage / ×1.5 HP. The clone keeps the legacy spawn-baking
   when veterancy is disabled (capture gate byte-identity) and uses the real
   `Veterancy=1.0f` when it is enabled.
3. **capture.md "VeteranArmor = toughness/HP"** — the binary never scales max
   HP; it divides incoming damage (V6). Same firewall as (2).
