# Wave-5 unit and building abilities: reverse-engineering notes

Binary: YR 1.001 `gamemd.exe` (image base `0x00400000`, SHA1
`7BE4E5508655F8AF482AB3FF5965D196E1B6B6C7`). Project copies `D:\ra2-re\w5-abil*`
(outside the repo). Every offset below was reached from an INI-key string
reference in this binary; YRpp/Ares were used for naming only.

Code: `engine/abil_w5.hpp` (data), `engine/abil_w5_rules.cpp` (rulesmd ->
type data), `engine/world_w5.cpp` (the World pass), plus the touched sites
named in each section.

## §1 Spy infiltration dispatch and stolen tech (R-042) — CONFIRMED

`BuildingClass::Infiltrate(house)` = `FUN_004571E0` (sole caller the enter
executor `FUN_00519449` @ `0x519DF3`). Returns at once when the building's
owner is the spy's house. Then it tests the building TYPE in this order (the
first match wins):

| Order | Test | Anchor | Effect |
|---|---|---|---|
| 1 | `Radar` (+0x16A4) set | loader `FUN_0045FE40` stores `"Radar"` at +0x16A4 | `FUN_0050BB30` -> reset the OWNER's shroud (`FUN_005775E0`) unless latched (+0x577A) |
| 2 | `Power` (+0xEE0) >= 1 | | `FUN_0050BAB0(Rules->SpyPowerBlackout)` |
| 3 | type is in Rules list +0x920 (count +0x92C) | `FUN_00672520` reads `"BuildTech"` into the vector at Rules+0x91C | by `AIBasePlanningSide` (+0x6D0, `FUN_00711510` param[0x1B4]): 0 -> house+0x2BE, 1 -> house+0x2BD, else house+0x2BC (YRpp `Side0/1/2TechInfiltrated`) |
| 4 | `SuperWeapon` (+0x16F0) != -1 | | `FUN_006CD620`: reset THAT superweapon's charge |
| 5 | `Storage` (+0x800) > 0 | `FUN_00711510` param[0x200] = `"Storage"` | steal `SpyMoneyStealPercent` of the victim's money |
| 6 | `Factory` (+0xEB8) == 0x28 (`UnitType`) | loader stores `"Factory"` at +0xEB8 | house+0x2C0 (`WarFactoryInfiltrated`) |
| 7 | `Factory` == 0x10 (`InfantryType`) | | house+0x2BF (`BarracksInfiltrated`) |

`[AI] BuildTech=NATECH,GATECH,YATECH` in this install, so a spy in an Allied
Battle Lab (AIBasePlanningSide=0) gives `Side0TechInfiltrated`, a Soviet one
`Side1`. A unit with `RequiresStolenAlliedTech/SovietTech/ThirdTech` is
buildable only while the house holds every matching flag (YRpp
`HouseClass::HasAllStolenTech`): CCOMAND (Allied tech), CIVAN (Soviet tech),
PTROOP (Yuri tech, unreachable in an Allied/Soviet game).

Consequences in the clone:
- `fill_building_abilities` resolves `BldTypeW5::spy_purpose` in this exact
  order from rulesmd, so the gesture layer and the AI use the binary's
  dispatch. The old id list mapped `GACHRO` (no such type; the Chronosphere
  is `GACSPH`) and treated the Battle Labs as radars.
- The Naval Yard is `Factory=UnitType`, so it sets the same flag as a War
  Factory. **Corrected (wave 7, W7-01-15):** that flag makes only non-Naval,
  Trainable vehicles veteran (UnitClass Init 0x734490 @0x734717); ships and
  aircraft never read it. Naval yards are not `Spyable` in stock rulesmd
  anyway. The flags are tested when every infantry / vehicle the house
  creates is initialised (W7-08-21: paradrops, reinforcements, crates too).
- The SW reset now resets only the infiltrated building's superweapon.
- `RequiresStolen*` types stay in the producible rosters (player and AI) with
  TechLevel 0 until `World::apply_stolen_tech_gate` sees the stolen bits.
  The stolen bits are saved (`WorldW5::stolen_tech`).

## §2 Prism Tower forwarding (R-043) — CONFIRMED

Anchors: `PrismType` 0x83ABEC .. `PrismSupportHeight` 0x83AB84 read by the
Rules [General] reader `FUN_0066CF70` (0x670B6D..0x670C35) into Rules
+0x498..+0x4AC (PrismSupportModifier parsed as a percent -> int 150).

- `BuildingClass::Mission_Attack` `FUN_0044ACF0`, prism block 0x44B2F8: while
  `SupportingPrisms < PrismSupportMax`, recruit the NEAREST own (Owner->Buildings,
  same house only) PrismType building that is alive, reload timer expired,
  `DelayBeforeFiring == 0`, not drained, not in its Attack mission, within the
  master's Secondary (PrismSupport) range. ONE supporter per frame; the slave
  gets `DelayBeforeFiring = DelayedFireDelay` (art, GAPRIS 28), stage Slave.
  When none is left the tower itself becomes Master with the same delay.
  Single-level star: slaves never recruit (multi-level chains are Ares-only).
- `PrismUpdate` `FUN_004503E0` (every frame from BuildingClass::AI): count the
  delay down; a Master fires and sets
  `bullet->DamageMultiplier = (PrismSupportModifier*N + 100)*256/100`
  (unsigned); `BulletClass::Detonate` 0x469973 deals `(Damage*mult) >> 8`.
  Damage 120: N=1 300, 2 480, 3 660 ... 8 1560. A Slave fires its support beam
  (`FireLaser` `FUN_0044ABD0`: duration PrismSupportDuration 15, ReloadTimer =
  PrismSupportDelay 45). `PrismSupportHeight` is never read (dead key).
- Clone: `building_combat.cpp` (recruit / countdown / `w5_prism_fire`).
  [RE-FALLBACK] every tower acquires in the same frame here, so a tower inside
  the reach of an own prism that is already recruiting stands down (in the
  original the per-object target-scan stagger lets the first one recruit).

## §3 Tesla Coil charging (R-045) — CONFIRMED

- `Overpowerable` -> BuildingType +0x1575; warhead `ElectricAssault` -> +0x158.
- Detonate's ElectricAssault branch (0x4693B0): a building target hit by an
  infantry-owned bullet -> `BuildingClass::Overpower(inf)` `FUN_00452810` (a
  set of troopers, moved to the end on each hit); no damage is dealt.
- BuildingClass::AI 0x44007F: troopers whose Target is no longer this coil
  leave the set; `IsOverpowered = n >= 3 || (n >= 1 && power% == 100 && HasPower)`.
- `IsPowerOnline` `FUN_004555C0`: at low power a Powered building is offline
  unless `Overpowerers >= 2`.
- `SelectWeapon` 0x6F2B40: an overpowered building fires its Secondary
  (OPCoilBolt 300 / Range 8).
- Idle troopers scan their 8 neighbour cells in Guard/AreaGuard for an own
  Overpowerable building and attack it with AssaultBolt (ROF 25).
- Clone: `world_w5.cpp` step (3) + `building_combat.cpp` slot/power gates.

## §4 Gap Generator (R-044) — CONFIRMED

`GapGenerator`/`GapRadiusInCells` -> TechnoType +0xCD1/+0xCD2.
`TechnoClass::CreateGap` `FUN_006FA690`: power required; for every cell with
`dx*dx+dy*dy < (R+1)^2` (R=10: 349 cells), when the owner is not the local
player and not allied with it, the cell's ShroudCounter is raised and the
cell goes black at once. Unit sight still reveals (RevealArea sets
FlagToShroud on gap cells); `LogicClass::Update` re-shrouds those cells every
120 frames (`FUN_00577B30`). DestroyGap only re-reveals with a SpySat; a
captured gap is destroyed and recreated for the new owner; power loss
destroys it. The radar shows the gap cells black; no radar jamming.
Clone: `World::w5_gap_pass` from `step_visibility` -- every human (non-AI)
house that is not the owner/ally is shrouded in the circle (the original has
only the local player's shroud); a SpySat does not lift it.

## §5 Tech Hospital / Machine Shop — CONFIRMED

`TechnoClass::Update` `FUN_006F9380`: infantry heal on
`Frame % SelfHealInfantryFrames (Rules+0x30) == 0` by
`min(SelfHealInfantryAmount * house+0x164, missing)`; vehicles likewise with
SelfHealUnitFrames/Amount (+0x38/+0x3C) and house+0x168. The house counters
are the sums of owned buildings' `InfantryGainSelfHeal` (+0x1564) /
`UnitsGainSelfHeal` (+0x1568), kept on placement (`FUN_00445EE0`) and capture
(`FUN_004481AB`). This install: 50/20 and 75/5.

Tech buildings (`Capturable=yes NeedsEngineer=yes`) on a map are now real
neutral World buildings (`depth_world_building`): CAAIRP (ParaDropSpecial),
CATHOSP, CAMACH, CAPOWR (Power=200), CAOUTP, CASLAB...

## §6 OpenTopped — PROBABLE (anchors only)

Rules [CombatDamage] `OpenToppedRangeBonus=2` / `OpenToppedDamageMultiplier=1.2`
(`FUN_0066B5F0`). Only BFRT is OpenTopped in rulesmd (the Flak Track is not).
Clone: passengers acquire the transport's target or the nearest visible enemy
within (own range + 2) of the transport and fire with damage x1.2.
[RE-FALLBACK: the per-passenger acquisition order was not unwound.]

## §7 Transport DeployTime — CONFIRMED

`DeployTime` (TechnoType +0x3C8, minutes, double) arms a timer
(`FUN_004A5150`: minutes x 900, ftol) when the transport starts unloading
(`FUN_004190B0` case 0x15); passengers leave once it expires. .022 -> 19 frames.

## §8 Engineer bomb defusal — CONFIRMED

Warhead `BombDisarm` -> WarheadType +0x16E (`FUN_0075C6AF`). Detonate's branch
calls `FUN_00438930` (remove the AttachedBomb) on a target whose bomb pointer
(+0x38) is set. Engineer `Primary=DefuseKit` Range 1.5. Clone:
`World::order_defuse` (input: a lone engineer on a bombed own/allied object).

## §9 Mirage disguise — CONFIRMED

`UnitClass::UpdateDisguise` `FUN_007456F0` 0x745870: `DisguisedAsType =
DefaultMirageDisguises[Random(0, n-1)]` (Rules +0xFFC/+0x1008), a TerrainType.
Clone: `Unit::w5.mirage_tree` (presentation draws it -- agent D).

## §10 Boris airstrike — CONFIRMED (fields) / RE-FALLBACK (flight path)

`AirstrikeClass` ctor `FUN_0041D3B0` copies TechnoType +0x61C AirstrikeTeam,
+0x620 EliteAirstrikeTeam, +0x62C/+0x630 the team types, +0x624/+0x628 the
recharge frames; `FUN_0041D800` "may strike": not busy and Frame >= last +
recharge. Clone: Boris attacking an enemy building within Flare range (12)
spawns AirstrikeTeam BPLN planes from the map edge; they make their attack
run and fly out.

## §11 Giant Squid — PARTIAL

`SquidGrab` warhead `ParasitePlus`: Parasite=yes, Paralyzes, Culling ("kills
instead of damages if victim in Red"). Clone: SQD binds the P4 parasite; a
paralysing parasite pins its host, a culling one kills it at <= 25% HP.
OPEN: in play the squid does not engage a ship (the vehicle aim gate in
combat.cpp never passes for it) -- agent B area.

## §12 AI difficulty handicaps — CONFIRMED (copy) / OPEN (Armor/Groundspeed readers)

`HouseClass::AssignHandicap` `FUN_004F6DD0` copies the difficulty struct
into house +0x188 FirePower, +0x190 Groundspeed x GameSpeedBias, +0x198
Airspeed x bias, +0x1A0 Armor, +0x1A8 ROF, +0x1B0 Cost, +0x1B8 BuildTime
(multiplied by the country's in MP). `TechnoClass::ReceiveDamage` 0x700DF2
divides incoming damage by `FUN_0050BB50(house)` x TechnoType +0x158 --
`FUN_0050BB50` returns the COUNTRY armor multiplier by WhatAmI (HouseType
+0x100.. +0x110), not house+0x1A0. No reader of house+0x1A0/+0x190 as a
double was found in a whole-binary displacement scan, so the Armor and
Groundspeed handicaps stay unapplied (R-082 open, now with evidence).
