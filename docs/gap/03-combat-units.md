# Gap analysis 03 — Combat, units and movement in REAL play

Audit date: 2026-10-06, master @ `f9b93ba` (after the P1–P10 merges). Read-only;
no source changed, nothing committed. `ra2engine.exe` was NOT run (the gate
batch had not reached `ALL DONE`); every claim below is traced in the
interactive code path (`GameScene`, `engine/main.cpp:4343-5259`) and in
`World::step` (`engine/world.cpp:3058`), with the original's behaviour cited
from `docs/re/*.md` or read directly from this install's `rulesmd.ini`
(extracted copy `D:\ra2-re\out\shroud_evidence\rulesmd.ini`).

"REAL play" = `Main Menu -> Skirmish -> GameScene` (Allied human vs Soviet AI,
`launch_skirmish`, `main.cpp:11557`) and `New Campaign -> GameScene` with a
`CampaignRuntime` (`campaign_runtime.inc`). The byte-identity gates
(`--demo-*`) are NOT real play.

## How the interactive World is wired (what is and is not enabled)

| Call | Reached in skirmish? | Reached in campaign? | Where |
|---|---|---|---|
| `enable_projectiles` | yes | yes | `main.cpp:4392` (ctor), `:4986` (load-fail rebuild) |
| `wire_abilities` -> `register_ability_profile` x {DRON,DESO,CLEG,TANY,GHOST,IVAN,TERROR,FV + every IFVMode infantry} | yes | yes | `main.cpp:4452`, `:1004-1020` |
| `set_survivor_crew` | yes | yes | `main.cpp:4459-4471` |
| `enable_p5_features` -> `enable_veterancy` (+`enable_crates` iff lobby CRATES) | yes | yes | `main.cpp:4501`, `:4003-4023` |
| `set_water_terrain` (water grid) | **NO** — `launch_skirmish` never calls it | yes (`build_campaign_world`) | `main.cpp:11557-11633`; `campaign_runtime.inc:1297-1301` |
| `make_ai_team_config` (AI unit production + attack teams) | **NO** — `make_ai_config` leaves `team.enabled=false` | n/a (map teams instead) | `main.cpp:2178-2258`; only `:9227`/`:9476` (gates) build a team config |
| `load_roster_units` (12 P-roster units) / `load_roster2_units` (24 P6 units) | **NO** — `[[maybe_unused]]`, "Called ONLY by --demo-roster / --demo-roster2" | NO | `main.cpp:1582`, `:1613` |
| Player unit roster | `load_unit_roster` = **E1, GGI, MTNK, FV, SREF** | Allied: same 5; Soviet: `load_soviet_unit_roster` = **E2, HTNK, APOC, HTK** | `main.cpp:1528-1545`, `:1559-1566`, `:4373` |
| Player building tabs | `GA*` prefix only (even for a Soviet-side human slot) | `GA*` / `NA*` by campaign side | `main.cpp:5078-5087`, `:3227-3244` |
| Unit factories wired to the sidebar | `GAPILE` / `GAWEAP` only (no GAYARD / GAAIRC queue) | + `NAHAND` / `NAWEAP` for Soviet campaign | `main.cpp:4425-4426`, `:5177-5185` |
| Keyboard | arrows (scroll), **D** (unload + deploy), **N** (SW), F1/F10. No stop/guard/scatter/force-fire/attack-move/queue keys; `input.cpp` handles no `InputEvent::Key` at all | same | `main.cpp:4622-4675`, `grep InputEvent::Key engine/input.cpp` -> none |

## Feature table

Status: WORKS / GATE-ONLY / PARTIAL / WRONG / BROKEN / MISSING. Severity:
BLOCKER = game unplayable or core loop broken, MAJOR, MINOR. Effort S/M/L.

### Movement and pathfinding

| Feature | Original behaviour (cite) | Clone in REAL play (cite) | Status | Sev | Fix sketch | Effort |
|---|---|---|---|---|---|---|
| Infantry walking (A*, occupancy, re-path) | FootClass A*, one cell at a time; 3 infantry per cell via sub-cells (`infantry-anim.md §7`) | `step_units` (`world.cpp:3407-3612`): A* over `passable_`, 1 unit/cell, seniority-staggered re-path at 0.30 s, give-up after 2.0 s (`world.cpp:156,161` — engine-tuned, not RE'd) | PARTIAL | MINOR | Sub-cell occupation (3/cell) is a known, accepted simplification | L |
| Vehicle drive (turn-in-place then roll, ROT) | `voxel-render.md §5`: hull turns at `ROT<<8` u16/frame, rotation-complete gate | `step_units` hull gate (`world.cpp:3452-3466`), `kRotU16PerRotPerSec=256*15` (`world.hpp:391`) | WORKS | — | — | — |
| **Absolute movement speed** | `Speed` -> `min(255, Speed*256/100)` leptons/frame; Speed 6 = 15 lept/frame = **0.879 cells/s @15 Hz** (`voxel-render.md §5`) | `kCellsPerSecPerSpeed = 0.75` (`main.cpp:675`): Speed 6 -> **4.5 cells/s**, Speed 4 infantry -> 3.0 cells/s. ROF *is* converted at 15 Hz (`combat.cpp:15,95`), so units cover ~5x more ground per shot than the original. README "Voxel vehicles" para admits the 0.75-vs-0.15 scale | WRONG | MAJOR | Pin the logic-frame rate for the game-speed setting (open RE Q1), then set `kCellsPerSecPerSpeed = (256/100)*fps/256` (0.15 @ 15 Hz) and rescale infantry `move_cps` the same way; re-pin movement gates | M |
| **Cliffs / water / impassable terrain for ground units** | `Ground[LandType][SpeedType]==0` blocks: Foot/Track/Wheel cannot enter Water, Rock/cliff (`naval.md §2`, SpeedType table) | `build_passability` marks **every materialized cell walkable** ("ALL materialized ground cells are treated walkable (water / cliffs / land-type refinement comes later)", `world.hpp:470-473`, `world.cpp:208-231`); nothing refines it in `build_world` (`main.cpp:1769`) or `launch_skirmish`. Tanks and GIs drive across lakes and cliff faces | MISSING | BLOCKER | Reuse `resolve_land_types` (already used for the water grid) to zero `passable_` for Water/Rock(cliff)/WaterCliffs cells for Land-domain units; add Beach/Road distinctions later | M |
| Bridges (units under/over, bridge cells) | Bridge overlays are their own height layer (`projectile.md §12.2 BridgeHeight=416`); `campaign.md` lists bridges as a carry-forward | No bridge model at all; bridge cells are ordinary walkable ground | MISSING | MAJOR | Overlay parse + bridge land type + separate passability layer | L |
| Speed by terrain (Road 1.x, Rough, Beach %) | Flat-terrain `Speed` x `[LandType] SpeedType%` downstream of the binary passability gate (`naval.md §2 item 2`) | None — `cps = speed_cps or move_cps` only (`world.cpp:3470`) | MISSING | MINOR | Per-cell multiplier from the `[Clear]/[Road]/...` table once LandType is on the Land grid | S |
| Crushing infantry | `Crusher=yes` vehicles kill `Crushable=yes` infantry on contact; `Crush=1.8`, `AutoCrush=2`, `PlayerAutoCrush=no` (`rulesmd.ini [General]`, `[MTNK] Crusher=yes`, `[E1] Crushable=yes`) | No crush code anywhere (`grep -i crush engine/` hits only the ability-name table) | MISSING | MAJOR | On vehicle cell entry: if occupant is enemy `Crushable` infantry -> kill (squish death), allow entering the occupied cell | M |
| Formation / group move | Each unit gets its own destination near the clicked cell; original uses the same cell + sub-cells | `assign_formation` spiral (`group_move.cpp:44-80`) via `order_move_selected` (`world.cpp:806`) | WORKS (approx.) | — | — | — |
| Attack-approach fan-out | `FootClass::ApproachTarget` picks a firing position, never the victim's cell (`infantry-anim.md §7`) | `step()` batches approach requests per target cell -> `assign_formation` (`world.cpp:3151-3200`) + hold-at-range pass (`world.cpp:3233-3277`) | WORKS (approx.) | — | — | — |
| Aircraft — VTOL (Jumpjet: ZEP Kirov, JUMPJET Rocketeer) | `aircraft.md §1-§4`: straight-line unpathed flight, ROTx2 heading, BalloonHover, fire from cruise | `order_unit_move` aircraft branch (`world.cpp:745-770`) + `step_flight` (`world.cpp:1223-1267`) exist, but **no Jumpjet unit is producible or placed in play**: ZEP/JUMPJET live only in `load_roster_units` (gate-only), campaign skips `[Aircraft]` objects (`campaign_runtime.inc:678`) and `Types::unit` has no aircraft path (`:187`) | GATE-ONLY | MAJOR | Add ZEP/JUMPJET to the player rosters (needs NAWEAP/GAPILE prereq full-parse — already supported), render path already exists | S |
| Aircraft — fixed-wing (ORCA Harrier, BEAG Black Eagle: Fly locomotor, Ammo=1, dock/rearm) | `roster2.md §C` items 9-11: take off from GAAIRC pad, fly to target, fire, `Ammo--`, return, rearm 270 frames | `is_aircraft` is set **only for the Jumpjet CLSID** (`main.cpp:739-742`); a Fly-locomotor type would resolve as a ground vehicle. The sortie machine is a pure helper (`roster2.hpp`, "No SDL, no World ... Nothing here is reachable from an existing gate"), driven only by `--demo-roster2` (`main.cpp:13395-13410`); README P6: "the airfield sortie ... not yet driven from the sidebar/World flight" | GATE-ONLY | MAJOR | Give `World` a fixed-wing flight state (dock pad, ammo, RTB) driven from `Sortie`; add an Aircraft queue (GAAIRC/AMRADR factory) to the sidebar | L |
| Naval locomotion (Float) | `naval.md §1-§3`: Drive-shaped A* over the Float-passable (Water) grid | `grid_for(Water)` path exists (`world.cpp:775-777`), but skirmish never builds the water grid (`launch_skirmish` lacks `set_water_terrain`) and no ship is producible (GAYARD/NAYARD factory not wired: "naval/aircraft sidebar queues are not wired into the interactive game", README P6). Campaign: water grid built, ships only if a map places them (bodies only) | GATE-ONLY | MAJOR | Call `set_water_terrain` in `launch_skirmish`; add Naval queue (`ba.naval` split already exists, `main.cpp:2056-2058`) + GAYARD water placement from `roster2.cpp` | M |
| Amphibious (LCRF, SAPC; Hover/Amphibious SpeedType land<->water) | `naval.md §1`: Hover/Amphibious cross Beach and both domains | `MovementDomain {Land, Water}` only; "Amphibious is reserved for a later phase" (`unit_combat.hpp:12-15`) | MISSING | MAJOR | Third domain = union grid; eject passengers only on land | M |
| Sub submerge / cloak | `naval.md §1` deferred | not present | MISSING | MINOR | later | M |

### Weapons and combat

| Feature | Original behaviour (cite) | Clone in REAL play (cite) | Status | Sev | Fix sketch | Effort |
|---|---|---|---|---|---|---|
| Weapon source (Primary only) | `Primary=`, `Secondary=`, and for `WeaponCount` types `Weapon1..N` (SREF: `Primary` is **commented out**, `WeaponCount=1 Weapon1=Comet`, `rulesmd.ini:6859-6873`) | `resolve_combat_unit` reads only `rules.primary()` = `Primary=` (`main.cpp:695-700`, `src/rules.cpp:109`); no `Secondary`, no `Weapon1` fallback | WRONG | BLOCKER (see SREF) | Resolve `Primary` else `Weapon1`; carry `Secondary` as a second `WeaponStats` and pick per target (AA/AG, Verses) | M |
| **Prism Tank (SREF) can fire** | `Comet` 100 dmg / ROF 100 / Range 10 (`rulesmd.ini [Comet]`) | SREF has no `Primary=` -> `damage=0 rof=0 range=0`: auto-acquire never matches (`d <= 0`), explicit attack approaches forever (`combat.cpp:470,491`). One of only five buildable Allied units is a 1200-credit paperweight | BROKEN | BLOCKER | the `Weapon1` fallback above | S |
| Range check | 3-D Euclidean (`combat.md §6`) | 2-D Euclidean (`combat.cpp:43-54`); no Z in sim | PARTIAL | MINOR | accept until a height model exists | — |
| ROF cooldown | `RearmTimer = ftol(ROF*houseROF + Random(0,2))` — **0..2 frame jitter from the sync RNG** (`veterancy.md V7 / §6 item 1`, `FUN_006FC460`; supersedes `combat.md §5`) | `u.rof_cool = u.w_rof / 15` with comment "CONFIRMED no jitter" (`combat.cpp:93-95`, `:362`) — **known unapplied RE finding** | WRONG | MINOR | Add a world RNG draw (R250 port exists: `GameRandom`) `+ random(0,2)` frames at every rearm; re-pin every combat gate | S |
| Minimum damage of 1 | `ReceiveDamage` clamps a non-negative defended hit to `>= 1` (`combat.md §3`; `abilities.md §H`) | `damage_vs` truncates to 0 (`combat.cpp:255-264`); only the vet-receive path applies `max(d,1)` BEFORE Verses (`world_veterancy` chain, `veterancy.md V5/V6`) — **known unapplied RE finding**: any product < 1 (e.g. 3 dmg x 25 % = 0.75) does nothing, where the original deals 1 | WRONG | MINOR | `return std::max(1, damage*verses/100)` when `damage>0` and `verses>0`; re-pin combat gates | S |
| Verses / warhead / armor | `Damage x Verses[armor]`, truncate (`combat.md §1-§4`) | `damage_vs` (`combat.cpp:255`), armor table (`combat.cpp:28-33`) | WORKS | — | — | — |
| 0 % Verses = cannot target / no retaliation | `GetFireError` returns 5 on `Verses[armor]==0`; `CanRetaliate` refuses (`combat.md §2`) | Auto-acquire scan checks only range (`combat.cpp:465-473`); explicit orders never check Verses; retaliation never checks Verses (`combat.cpp:146`) | MISSING | MINOR | filter `verses[armor]==0` in acquire/order/retaliate | S |
| Splash (CellSpread >= 1) | `FUN_00489190`; exact falloff curve not unwound (`combat.md §7`, `voxel-render.md §7`) | `apply_spread_damage` linear 100 % -> PercentAtMax, enemies only, no buildings (`combat.cpp:211-245`) | PARTIAL | MINOR | include buildings + own units (original damages everything in radius) | S |
| Projectile flight — ballistic (Cannon, FlakTProj) | `projectile.md §4/§6.1/§12` | `enable_projectiles` on in GameScene (`main.cpp:4392`); launch/integrate/detonate `world.cpp:3613-3900` | WORKS | — | — | — |
| Projectile flight — vertical (Kirov bomb) | `projectile.md §6.2` | implemented, but no Kirov in play (see aircraft) | GATE-ONLY | — | roster | — |
| Projectile — homing (`ROT>0`: HoverMissile, AAHeatSeeker, MammothTusk, RedEye2) | `projectile.md §6.4` PROBABLE | stays instant (README P2 "Homing (ROT>0) stays instant") — the IFV's HoverMissile hits instantly | PARTIAL | MINOR | finish §6.4 hunt | M |
| Elite weapons (`ElitePrimary`) | `veterancy.md V10` | `refresh_vet_stats` swaps `WeaponStats` on elite (`world_veterancy.cpp:47-55`), but `w_proj` is NOT swapped (`EliteWeapon1`/`ElitePrimary` projectile stays the base one) and SREF's `EliteWeapon1=SuperComet` is never read | PARTIAL | MINOR | resolve elite projectile + `EliteWeapon1` | S |
| Secondary weapon usage (APOC `MammothTusk` AA, HTK `FlakTrackAAGun`, GGI `MissileLauncher`) | per-target weapon choice by AA/AG + Verses | no Secondary anywhere (see "Weapon source") | MISSING | MAJOR | as above | M |
| **Anti-air targeting (ground AA vs aircraft, air-to-air)** | Flak Track / Flak Trooper / IFV / Patriot / Flak Cannon / Aegis shoot `AA=yes` weapons at airborne units (`rulesmd`: `[HTK] Secondary=FlakTrackAAGun`, `[NASAM] Primary=RedEye2`) | `valid_target` hard-excludes `is_aircraft` (`combat.cpp:59-77`); `apply_spread_damage` skips aircraft (`combat.cpp:220`). Nothing in the game can ever shoot an aircraft — "Ground-AA-vs-air is a documented carry-forward" (README CB-2) | MISSING | MAJOR (BLOCKER once aircraft are producible) | lift the exclusion; weapon pick by `AA`/`AG` flags; projectiles use target altitude (`unit_lz` already exists `world.cpp:3627`) | M |
| **Base defenses fire (GAPILL Vulcan2, NASAM RedEye2, ATESLA PrismShot, TESLA CoilBolt, NALASR Vulcan, NAFLAK FlakWeapon, GTGCAN, NABNKR garrison)** | Buildings with `Primary=` acquire and fire like units (`rulesmd.ini` sections above; `[GAPILL] Primary=Vulcan2 ROT=10`) | `BuildingTypeDef` has **no weapon fields** (`building.hpp:17-69`); `World::step` has no building-combat pass; `combat.cpp:308-315`: "buildings never retaliate, no weapons in this roster". Whatever defenses are buildable (next row) and the AI's NALASR/NABNKR/TESLA/NAFLAK (`main.cpp:2205`) **never shoot**. (`GATOW` in `kRoster` has no `[GATOW]` section in `rulesmd.ini` at all, so it is art-dropped) | MISSING | BLOCKER | Resolve `Primary/Secondary/ROT/Turret` into `BuildingTypeDef`; add a building acquire/aim/fire pass sharing `try_fire`/`launch_projectile` with a building as attacker (a `Unit`-shaped shooter record per building is the smallest change); power-gated (`Power<0` defenses go offline on low power) | M |
| Which defenses the player can even build (sidebar side filter) | YR rulesmd gives every building `Owner=<all countries>`; the side comes from the prereq (`Prerequisite=BARRACKS,GACNST` vs `...,NACNST`, `rulesmd.ini [NASAM]/[ATESLA]/[GTGCAN]/[TESLA]`) | `building_buildable_tab` filters by the **id prefix** `GA`/`NA` (`main.cpp:3227-3236`); `Owner=`/prereq-side is never read (`grep Owner src/rules.cpp` -> none). Allied tab 1 = GAPILL, GAWALL, GAGAP, GACSPH — **no Patriot (NASAM), no Prism Tower (ATESLA), no Grand Cannon (GTGCAN)**; the Soviet campaign tab 1 shows **NASAM (the Allied Patriot)** and no **TESLA** (Tesla Coil) | WRONG | MAJOR | derive side from the ConYard token in `Prerequisite` (GACNST/NACNST) or from `[General] BaseUnit`/side lists; drop the prefix test | S |
| Walls (GAWALL/NAWALL) | placed walls become overlays, line-fill up to `GuardRange` for free, 3x build time (`roster2.md TL;DR 1-5`) | 1x1 Buildings with the connection-frame render (`main.cpp:2403`); `wall_line_fill`/`wall_build_frames` are called only from `--demo-roster2` (`main.cpp:13280-13315`), README P6: "not yet in the live Defense queue" | PARTIAL | MINOR | call the two helpers from the placement confirm (`main.cpp:4783-4800`) | S |
| Units auto-acquire buildings | Idle units in guard engage enemy structures in range (needs confirmation — open Q3) | "units NEVER auto-acquire buildings" (`combat.cpp:309-310`); only explicit right-click (`input.cpp:745-751`) or AI team quarry | PARTIAL | MAJOR | add buildings (nearest foundation cell) to the acquire scan behind a threat score | S |
| Guard radius / stray leash | `GetGuardRange` not located; `Stray=2.0 GuardModeStray=2.0` (`combat.md §7` Low) | `kGuardStray = 2.0` cells past weapon range (`combat.cpp:24,526`) | PARTIAL | MINOR | open Q4 | — |
| Retaliation | threat comparison vs current archive target (`combat.md §7`) | idle victim retaliates against attacker only if `target==-1 && !moving` (`combat.cpp:146-157`) | PARTIAL | MINOR | — | S |
| Explicit orders available to the player | move, attack unit, attack building, force-fire (Ctrl), attack-move (Ctrl+Shift), stop (S), guard (G), scatter (X), deploy (D), waypoint queue (Z) | right-click move/attack/enter/capture/infiltrate (`input.cpp:522-760`), D (unload+deploy), N (SW). No stop/guard/scatter/force-fire/attack-move/queue | PARTIAL | MAJOR | keyboard handler in `Input` (S/G/X, Ctrl modifier) | M |
| **GI / Guardian GI deploy (D)** | `[E1] Deployer=yes DeployFire=yes Secondary=Para` (25 dmg, ROF 15, Range 5, `rulesmd.ini:3498-3499` "DeployFireWeapon = Index of weapon to fire while deployed (def=1)"): deployed GI is immobile and shoots **Para at targets in range 5**; GGI deployed fires `MissileLauncher` (AA/AT) | `resolve_ability_profile`: `deployer = Deployer && DeployFire`, `deploy = Secondary` (`main.cpp:951-953`) — the **Desolator** model: `order_deploy` freezes the unit (`world_abilities.cpp:63-88`), `step_combat` skips frozen units (`combat.cpp:298`), and every `Para.ROF` frames the deploy weapon is applied as **area damage at the GI's own cell** with `[SSA] CellSpread=0` -> hits nobody (`world_abilities.cpp:479-490`). Pressing D on selected GIs makes them immobile and silent until D again | WRONG | MAJOR | For `DeployFire` types without `RadLevel`: deployed = immobile + use `Secondary` as the active weapon through the normal `step_combat` path (do not freeze) | S |
| Garrisonable buildings (civilian structures, NABNKR) | `Occupants`, `OccupantAnim=UCFLASH` (`[Para]`), `CanOccupyFire` | nothing (`grep -i garrison engine/` -> none) | MISSING | MAJOR | occupant list on `Building`, enter order, fire-out with occupant weapon | L |
| Infantry prone / panic / scatter when fired upon | infantry drop prone under fire (`Down/Crawl/FireProne`), `Fraidycat`/panic (`infantry-anim.md §6`) | not modelled; infantry stand (`infantry-anim.md §6` carry-forward); only stand/walk/fire/die sequences in `UnitAnimSeq` (`world.hpp:348-350`) | MISSING | MINOR | prone flag on hit + Crawl/FireProne sequences from `artmd.ini` | M |
| Death animations — infantry | per-warhead `InfDeath` 1-6 (gunfire, explode, flying, burn, electric, ...) (`rulesmd.ini [SA] InfDeath=1`, `[AP] InfDeath=3`, `[Electric] InfDeath=5`) | always `Die1` (`world.cpp:3305-3320`), 1.0 s `kDieAnimSecs` (not RE'd, `combat.cpp:25`) | PARTIAL | MINOR | map `InfDeath` -> Die1..Die5 sequences | S |
| Death animations — vehicles | `Explosion=` list, debris, crater | first `Explosion=` SHP at 15 fps (`main.cpp:2461-2480`); "no crush and no debris" (README) | PARTIAL | MINOR | — | S |
| Death animations — buildings | rubble + `TWLT070` | explosion anim, no rubble frames (README buildings para) | PARTIAL | MINOR | — | S |
| Veterancy (XP, FIREPOWER/STRONGER/ROF/FASTER, elite, self-heal, chevrons) | `veterancy.md §2` | `enable_veterancy` in GameScene (`main.cpp:4008`), credit/rank/refresh (`world_veterancy.cpp:17-146`), chevrons drawn (`main.cpp:2878`) | WORKS | — | promotion EVA/flash are cut (README P5) | — |
| Crates | `veterancy.md §3` | `enable_crates` iff lobby CRATES (`main.cpp:4009-4022`), seeded from wall clock | WORKS | — | — | — |
| Transports (board / carry / LIFO eject) | `transport-cargo.md §1-§6` | right-click own transport boards (`input.cpp:636-660`), D unloads (`main.cpp:4663`), `step_cargo` (`world.cpp:1039-1222`). FV (`Passengers=1`) is the only Allied transport in play; HTK in Soviet campaign | WORKS | — | no "enter" cursor; `DeployTime` delay not modelled | S |
| IFV weapon swap | `abilities.md §G` | `step_abilities (2)` syncs weapon to passenger `IFVMode` (`world_abilities.cpp:461-477`) — E1/GGI/E2 have IFVMode so GI-in-IFV -> `CRM60` works in play | WORKS | — | — | — |
| Terror Drone / Desolator / Chrono Legionnaire / SEAL+Tanya C4 / Ivan / Terrorist | `abilities.md §A-§F` | profiles registered (`main.cpp:1006`), passes implemented (`world_abilities.cpp`), but **none of these units is producible** in skirmish or the Soviet/Allied campaign sidebars (gate-only rosters); only a campaign map that pre-places one exercises it | GATE-ONLY | MAJOR | roster wiring (below) | S |
| Superweapon (nuke) | `superweapon.md` | granted per slot when lobby SW on (`main.cpp:11627-11631`), N / cameo fires | WORKS (not re-audited here) | — | — | — |

### Roster / production reachability (the root of "nothing is working")

| Feature | Original behaviour | Clone in REAL play | Status | Sev | Fix sketch | Effort |
|---|---|---|---|---|---|---|
| **Allied build list** (infantry: GI, Guardian GI, Engineer, Attack Dog, Rocketeer, SEAL, Spy, Chrono Legionnaire, Tanya, Sniper; vehicles: Grizzly, IFV, Mirage, Prism, Tank Destroyer, Chrono Miner, MCV, Nighthawk, Battle Fortress, Robot Tank, Amphib. Transport; naval: Destroyer, Aegis, Carrier, Dolphin; air: Harrier/Black Eagle) | all `TechLevel>0` types gated by prereq + `Owner` | `kUnits` = **E1, GGI, MTNK, FV, SREF** (`main.cpp:1528-1545`), SREF non-functional (above). Everything else sits in `load_roster_units`/`load_roster2_units`, both `[[maybe_unused]]` and never called outside the two gates (`main.cpp:1582, 1613`) | GATE-ONLY | BLOCKER | Build the player roster from `[InfantryTypes]/[VehicleTypes]/[AircraftTypes]` filtered by `Owner` side + `TechLevel>0` + cameo (the loader already full-parses prereqs); split queues by `Naval`/aircraft; keep the gate tables untouched | M |
| **Soviet build list** (Conscript, Engineer, Dog, Flak Trooper, Tesla Trooper, Ivan, Desolator, Terrorist, Boris; Rhino, Flak Track, V3, Apocalypse, Tesla Tank, Terror Drone, Demo Truck, War Miner, MCV, Amphib. Transport, Siege Chopper; Sub, Sea Scorpion, Dreadnought, Squid; Kirov) | as above | Only the Soviet **campaign** human gets a Soviet roster: `kSoviet` = **E2, HTNK, APOC, HTK** with proxy prereqs (APOC needs only NAWEAP, `main.cpp:1559-1566`). A Soviet-side **skirmish** human gets a `NACNST` but the Allied `GA*` building tabs and the Allied unit roster (`main.cpp:4373`, `:5079`, `:4425`) | WRONG | BLOCKER | side-aware roster/tab/factory selection from `SkirmishConfig.slots[0].side` (the P10 `soviet_campaign()` switch generalised) | S |
| Engineer / Spy / Dog producible | `[ENGINEER] TechLevel=1 Prerequisite=GAPILE` etc. | not in any player roster (`input.cpp` gestures key off image `"ENGINEER"`/`"SPY"`, `input.cpp:41-67`, which never spawn in play) | GATE-ONLY | MAJOR | roster wiring | S |
| MCV / deploy | `AMCV/SMCV` deploy into ConYard | none; skirmish starts with a pre-placed ConYard (`spawn_base`, `main.cpp:2266`), campaign "MCV deploy absent" (README P10) | MISSING | MAJOR | deploy order -> `add_building(GACNST)` at the MCV cell | M |
| Skirmish AI fields units / attacks | `ai-team.md`: TaskForces, autocreate | `make_ai_config` (`main.cpp:2178-2258`) builds only the base brain; `AiTeamConfig.enabled` stays false, `make_ai_team_config` is called only by `--demo-ai-team`/`--demo-ai-trigger` (`main.cpp:9227, 9476`). The skirmish AI builds structures (incl. inert defenses) but **never trains a unit or attacks**; the only enemy units are its `unit_count` starting conscripts | GATE-ONLY | BLOCKER (cross-ref AI audit) | `cfg.team = make_ai_team_config(ba, ur_ai, rules, aimd_ini, ..., enable_triggers=true)` in `launch_skirmish` | S |
| Allied skirmish start base | MCV only (deploy) | ConYard only; "interactive Allied houses still get ConYard-only" (README roster), Soviet AI gets ConYard+power+barracks+war factory (`main.cpp:2298-2316`) | PARTIAL | MINOR | fine once MCV exists; or pass `allied_full` | S |

## Per-unit roster matrix (RA2 Allied + Soviet build lists; no Yuri faction)

Legend — Buildable: **Sk** = Allied skirmish sidebar, **Cmp** = Soviet campaign sidebar, "map" = only if a campaign map pre-places it (body resolved generically by `Types::unit`, `campaign_runtime.inc:154-190`), "gate" = `--demo-roster`/`--demo-roster2` only. Moves/Attacks/Ability describe what the code would do for an instance that exists in play.

| unit id | buildable in play? | moves? | attacks correctly? | ability works? | notes |
|---|---|---|---|---|---|
| E1 GI | Sk | yes | yes (M60, instant Inviso) | **D = freeze, no Para fire** (WRONG) | IFV passenger -> CRM60 works |
| GGI Guardian GI | Sk | yes | M60 only; no `MissileLauncher` (no Secondary) | D freezes; no AA | — |
| ENGINEER | gate/map | yes | n/a | capture gesture exists (`input.cpp:715`) | never producible |
| ADOG / DOG | gate/map | yes | bite = `Primary` resolves; no leap | DetectDisguise wired | never producible |
| SPY | gate | yes | n/a | infiltrate gesture exists | never producible |
| JUMPJET Rocketeer | gate | flies (Jumpjet CLSID) | A-G only | — | cannot be shot at (no AA) |
| GHOST Navy SEAL | gate/map | yes | MP5 | C4 on buildings wired | no amphibious |
| CLEG Chrono Legionnaire | gate/map | teleports | erase wired | yes | — |
| TANY Tanya | gate/map | yes | yes | C4 wired; no C4 on ships | — |
| SNIPE Sniper | gate | yes | Primary | — | body only |
| CCOMAND / PTROOP (stolen tech) | gate | yes | Primary | PTROOP mind control cut | body only |
| MTNK Grizzly | Sk | yes | 105mm ballistic | — | no crush |
| FV IFV | Sk | yes | HoverMissile (instant, homing not modelled) | Gunner swap works; no AA | Passengers=1 transport works |
| MGTK Mirage | gate | yes | Primary | no disguise | — |
| SREF Prism Tank | Sk | yes | **NO — damage/rof/range 0** (`Primary` commented out, `Weapon1=Comet`) | — | BROKEN |
| TNKD Tank Destroyer | gate | yes | Primary | — | body only |
| BFRT Battle Fortress / ROBO Robot Tank (YR) | gate | yes | Primary | no fire-out | ROBO art absent |
| CMIN Chrono Miner | free with GAREFN | warps home | n/a | economy | not audited here |
| AMCV / SMCV | gate (multihouse) | yes | n/a | **no deploy** | — |
| SHAD Nighthawk / ORCA Harrier / BEAG Black Eagle | gate | **no real flight**: Fly locomotor -> treated as a ground voxel; `Sortie` only in --demo-roster2 | Primary | no dock/rearm | campaign skips `[Aircraft]` |
| LCRF / SAPC Amphibious Transport | gate | water only (no Amphibious domain) | n/a | transport logic generic | — |
| DEST Destroyer / AEGIS / CARRIER / DRED / SUB / HYD Sea Scorpion | gate (map in campaign) | water grid only (campaign has it; skirmish does not) | HYD FlakTrackGun yes; DEST/DRED/CARRIER spawners cut; SUB no submerge; AEGIS has no AA target | — | no naval yard queue |
| DLPH Dolphin / SQD Squid | none | — | — | — | no voxel on install; dropped |
| E2 Conscript | Cmp (Soviet campaign) | yes | M1Carbine | — | skirmish AI's starting units |
| SENGINEER / FLAKT Flak Trooper / SHK Tesla Trooper / IVAN / DESO Desolator / TERROR / BORIS / CIVAN | gate/map | yes | Primary (FLAKT no AA; SHK no coil charge) | DESO deploy, IVAN bomb, TERROR suicide wired | never producible |
| HTNK Rhino | Cmp | yes | 120mm ballistic | — | AI's `kSoviet` unit (teams disabled in skirmish) |
| HTK Flak Track | Cmp | yes | FlakTrackGun (scatter RNG); **no AA** | Passengers=5 transport works | — |
| APOC Apocalypse | Cmp (NAWEAP only — NATECH ignored) | yes | 120mmx; **no MammothTusk AA** | SelfHealing yes | — |
| V3 / ZEP Kirov / DRON Terror Drone / TTNK Tesla Tank / DTRUCK / SCHP | gate (DRON map) | V3/TTNK/DTRUCK yes; ZEP flies; SCHP dropped | V3 rocket homing instant; ZEP bomb vertical OK | DRON infect wired; DTRUCK none | never producible |
| HARV War Miner | free with NAREFN | yes | n/a | economy | — |

## Top 10 blockers

1. **Player rosters are 5 (Allied) / 4 (Soviet campaign) units; everything else is gate-only** — `load_roster_units`/`load_roster2_units` are `[[maybe_unused]]` (`main.cpp:1582,1613`). No Engineer, Dog, Rocketeer, SEAL, Tanya, Mirage, Nighthawk, any ship, any aircraft, any Soviet specialist.
2. **Base defenses never fire** — `BuildingTypeDef` has no weapon, no building combat pass (`building.hpp`, `combat.cpp:308-315`). GAPILL/NASAM/TESLA/NAFLAK/NALASR are props.
3. **Skirmish AI never trains units or attacks** — `make_ai_config` leaves the team brain off (`main.cpp:2178`); team config exists only in gates (`main.cpp:9227,9476`).
4. **Prism Tank cannot fire** — `Primary=` is commented out in `rulesmd.ini [SREF]`; the clone never reads `Weapon1` (`src/rules.cpp:109`, `main.cpp:695`).
5. **Ground units walk over water and cliffs** — `build_passability` treats every materialized cell as walkable (`world.cpp:208-231`); `launch_skirmish` never builds the water grid either.
6. **Soviet-side skirmish human gets the Allied roster and GA* tabs** (`main.cpp:4373, 5079, 4425`).
7. **No anti-air at all** — `valid_target` excludes aircraft (`combat.cpp:62`); no Secondary weapons; AA weapons on HTK/APOC/GGI/NASAM/NAFLAK are unused.
8. **GI/GGI deploy freezes the squad instead of switching to Para/MissileLauncher** (`main.cpp:951-953`, `world_abilities.cpp:63-88,479-490`).
9. **Movement speed is ~5x the recovered scale relative to ROF** (`kCellsPerSecPerSpeed=0.75` vs 0.146 cells/s per Speed point at 15 Hz, `voxel-render.md §5`).
10. **No crush, no garrison, no MCV deploy, no bridges, no prone** — the infantry/vehicle interaction layer of RA2 is absent (`grep -i crush|garrison engine/` -> nothing).

## Open RE questions (need a Ghidra hunt; none run in this audit)

1. **Logic frame rate at each `GameSpeed` setting.** All recovered per-frame constants (Speed leptons/frame, ROF frames, 15 lept/frame = 0.879 cells/s at 15 Hz) hinge on the real tick rate the player experiences at the default/Fastest speed (the 15 Hz assumption comes from the `*900 = minutes` constant). Needed before fixing blocker 9 — find the game-speed -> frame-delay table (`OptionsClass` GameSpeed, `Frame_Limiter`).
2. **`DeployFire` targeting**: confirm that a deployed `Deployer=yes DeployFire=yes` infantry uses `DeployFireWeapon`(=1, Secondary) through the normal `TechnoClass::Fire` target path while `Mission_Unload`/deployed, and whether it may still rotate to face (`InfantryClass::AI` deploy branch; `rulesmd.ini:3498-3499`).
3. **Do idle (guard) units auto-acquire enemy buildings?** `TechnoClass::Greatest_Threat`/`Target_Something_Nearby` — which `ThreatType` flags include structures for an idle unit vs `GuardArea=2` units; needed to decide the "units never auto-acquire buildings" row.
4. **`TechnoClass::GetGuardRange` body and the `Stray`/`GuardModeStray` consumer** (`combat.md §7` Low): the leash the clone approximates with `kGuardStray=2.0`.
5. **Crush mechanics**: `UnitClass::Per_Cell_Process`/`Drive` crush branch — who may enter an infantry-occupied cell (`Crusher`, `Crushable`, `OmniCrusher`), `Crush=1.8` approach distance and `PlayerAutoCrush=no` semantics.
6. **Weapon selection** (`TechnoClass::SelectWeapon`, `WeaponCount`/`Weapon1..N` vs `Primary/Secondary`): the AA/AG and Verses-based choice, and how SREF's `IsChargeTurret` multi-turret abuse maps to `Weapon1` — needed for blockers 4 and 7.
7. **Building combat**: `BuildingClass::AI` fire path for `Primary=` defenses — acquire range (`GuardRange`?), turret `ROT`, low-power behaviour (`Power<0` defenses offline), and whether `Turret=no` prism/tesla need facing.
8. **Spread-damage falloff curve** inside `ReceiveDamage` (`combat.md §7`, `voxel-render.md §7` Medium) — the clone's linear 100 % -> PercentAtMax is an adjudicated stand-in.
9. **Homing projectile steering** (`projectile.md §6.4`, `FUN_005B1B20`) and the `MissileSpeedVar`/`MissileROTVar` defaults — the IFV, V3, Patriot and Apocalypse AA all depend on it.
10. **`InfDeath` -> sequence mapping and prone trigger** (`InfantryClass::ReceiveDamage` -> `Do_Action(DO_GUN_DEATH...)`, `IsProne` set on `FearLevel`): for the death-animation and prone rows.
