# Unit Special Abilities (batch 1) — Design Spec

**Status:** APPROVED for implementation (controller brief, P4)
**Roadmap slot:** `docs/superpowers/ROADMAP-completion.md` **P4 — "Unit special abilities (batch 1)"** (Terror Drone, Desolator, Chrono Legionnaire, SEAL/Tanya C4, Crazy Ivan / Terrorist, IFV modes). Gate: `--demo-abilities`.
**Calibration authority:** `docs/re/abilities.md` (this phase's hunt). Every constant and rule below cites it by §; `PROBABLE` items carry the label into code comments.
**Depends on:** combat (`combat.cpp` targeting/pursuit/`damage_vs`), transports (`cargo.cpp`, `World::step_cargo`, `transport-cargo.md`), buildings (`kill_building`, `foundation_cells`), Repair & Sell (`can_be_sold`'s `c4_applied` hook), group-move (`nearest_free_cells`).

## Goal

Give the six Soviet/Allied specialists their identity, exactly as recovered:

| Unit | Behavior | § |
|---|---|---|
| Terror Drone `DRON` | fire = infect (limbo inside the victim); tick every `ROF=60` frames, first tick immediate; infantry victim dies on the first tick (ignores armor), vehicles take `50 × Verses`; drone pops out when the host dies (destroyed if suppressed) | B |
| Desolator `DESO` | deploy (immobile) → `RadEruptionWeapon` at its own cell every 60 frames → RadSite (spread 10, level 500, accumulating `Add`), cells decay by `amt/LevelSteps` every 90 frames, foot units on irradiated cells take `ftol(min(lvl,500)*0.2) × RadSite Verses` every 16 frames (absolute phase) | A |
| Chrono Legionnaire `CLEG` | every move is a teleport: immediate relocation, frozen for `max(dist/48, 16)` frames; fire = attach a temporal beam: `WarpRemaining = Strength*10`, minus the sum of attached legionnaires' Damage every frame, erase (no death, no blast) at ≤ 0 | C |
| Tanya `TANY` / SEAL `GHOST` | ordered onto a building: walk to `Sapper` range 1.5, plant C4 (`ftol(C4Delay*900)` = 27 frames), lethal ignore-defense blast; a C4'd building cannot be sold | D |
| Crazy Ivan `IVAN` | fire = plant a bomb (one per target); detonates on `plant+451` (strict) or when the target dies; `DamageArea` 450 / `IvanWH` 1.5 cells, all houses, buildings too; Ivan's own death does nothing | E |
| Terrorist `TERROR` | `Suicide` weapon: firing kills him, the death weapon `TerrorBomb` 225 / `TerrorBombWH` 2 cells blasts at his position; also on any death; no passive acquire, no retaliation | F |
| IFV `FV` | `Gunner=yes`: weapon = `Weapon[IFVMode of the passenger]` (0-based, 0..17), back to 0 when empty; kind-bearing IFV weapons (`CRNuke`/`CRTerrorBomb` suicide, `CRNeutronRifle` temporal) behave as above | G |

## Global constraints

- **Every existing byte-identity gate stays bit-identical.** Firewall (the established four-part rule):
  1. All new `Unit`/`Building`/`World` state is **inert by default** (`AbilityProfile{}` = no ability, `frozen=false`, `c4_applied=false`, `bomb_frame=-1`, …).
  2. Profiles reach units ONLY through `World::register_ability_profile(image, profile)`, which opens `abilities_any_`; the single new pass `step_abilities` early-returns on `!abilities_any_`. **No existing gate registers a profile.** The interactive `GameScene` registers them in its ctor (never in `launch_skirmish`, which the skirmish/maps gates call).
  3. `world_hash` is not modified; the new state is folded only by the new gate's `abilities_fold`.
  4. Every edit to an existing pass is a guard on a default-inert field: `frozen` (step_units / step_combat / the move-request dispatch), `abil.fire_kind` (the fire interception in `try_fire` + the building fire branch), `abil.no_passive_acquire` / `abil.no_retaliate`, `abil.c4` (building approach range), `abil.teleporter` (`order_unit_move`), `b.c4_applied` (`begin_sell` / sidebar sell refusal). With the defaults each guard is false → the old path runs unchanged.
- One frame = one `World::step` call (the repair-sell `repair_frame_` precedent). The ability frame counter advances above the firewall.
- New pure TU `engine/abilities.{hpp,cpp}` (World-free math, host-doctested) + `engine/ability_types.hpp` (data) + `engine/world_abilities.cpp` (the World pass + orders, keeps `world.cpp` churn minimal). Registered in both cmake lists.
- Rules resolution lives in `main.cpp` (`resolve_ability_profile`, reading the real INI keys through the existing `Rules`/`Ini` accessors) — no `src/rules.cpp` change.

## Data model

```cpp
enum class AbilityKind : uint8_t { None, Parasite, Temporal, IvanBomb, Suicide };
struct WeaponSpec { int damage, rof; double range; std::vector<int> verses;
                    double cell_spread, percent_at_max; int rad_level; AbilityKind kind; bool valid; };
struct AbilityProfile {
  AbilityKind fire_kind;            // primary weapon's kind (intercepts combat fire)
  bool c4; double c4_range;         // C4=yes + Secondary range
  bool teleporter;
  bool explodes; WeaponSpec death;  // Explodes / DeathWeapon (else primary)
  bool deployer; WeaponSpec deploy; // DeployFire secondary (RadEruption)
  bool immune_radiation, parasiteable(true), warpable(true);
  bool no_passive_acquire, no_retaliate;
  int  suppression_threshold;
  int  ifv_mode;                    // as a passenger
  bool gunner; std::vector<WeaponSpec> ifv_weapons;   // as an IFV
  bool infantry;
};
struct AbilityRules { /* [Radiation], Chrono*, C4Delay, Ivan*, RadSite/IvanWH/C4 warhead specs */ };
```

Runtime fields on `Unit` (all inert): `abil`, `abil_bound`, `frozen`, `warp_frames_left`, `ability_fired_unit/building`, `parasite_on_me`, `parasite_next_tick`, `suppress_until`, `last_hp`, `temporal_target`, `warp_remaining`, `bomb_frame`, `bomb_owner`, `bomb_house`, `deployed`, `deploy_next_fire`, `ifv_current`, `death_done`.
On `Building`: `c4_applied`, `c4_frame`, `c4_by`, `bomb_frame`, `bomb_owner`, `bomb_house`, `warp_remaining`, `death_done`.

## The pass (`World::step_abilities`, appended at the end of `World::step`)

Order (all per frame, ascending index): (1) bind profiles; (2) IFV gunner weapon sync; (3) consume combat fire events (infect / temporal attach / bomb plant / C4 plant / suicide); (4) teleport freeze countdown; (5) deployed eruptions; (6) parasite suppression + ticks; (7) temporal warp; (8) bombs + C4 timers; (9) RadSite AI then the radiation cadence; (10) the death loop (death weapons, bomb chain, drone ejection, temporal release), repeated until no new deaths (bounded).

## Gate `--demo-abilities`

Two runs (rendered hidden window + headless, fresh worlds), witness lines + `abilities_hash` compared, wedge guards → exit 1, no pinned literal in code. Stages: drone vs Rhino (60-frame ticks, 50 dmg vs heavy), drone vs GI (instant kill, drone pops out); desolator deploy (16-frame rad cadence, 90-frame decay, GI death); CLEG teleport (53-frame freeze for 10 cells) + erase GI in 157 frames; Tanya C4 (27 frames, sell refused while armed); Ivan bomb (`plant+451`, area 450); terrorist suicide (225 / 2 cells); IFV: GI → `CRM60`, unload → `HoverMissile`, Desolator → `CRRadBeamWeapon`.

## Cuts (from `abilities.md` §Deviations)
Projectile flight, Service-Depot/IFV-repair drone removal, squid, C4 on ships, friendly-target bombs, mind control, radiation lighting, effect anims/sounds, erase immunity, min-1 damage clamp.
