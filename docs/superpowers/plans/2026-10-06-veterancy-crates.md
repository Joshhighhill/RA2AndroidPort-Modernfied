# P5 Veterancy + Crates — Implementation Plan

Spec: `docs/superpowers/specs/2026-10-06-veterancy-crates-design.md`.
Calibration: `docs/re/veterancy.md`. Branch `p5-veterancy` (worktree
`D:\ra2-wt\p5-veterancy`, own build dir `build/`).

## Global Constraints

* Byte-identity firewall (spec §Global constraints 1–2): nothing new runs unless
  `enable_veterancy` / `enable_crates` was called; `launch_skirmish` untouched.
* Exact `double` arithmetic, one op per statement, `ftol` = truncate.
* TDD per task; **scoped** test runs only (`ra2assets_tests -tc=...` /
  `-ts=...`); never the full suite or the existing gates (controller does that).
* Commit after each task; messages end with the Co-Authored-By trailer.

## File Structure

| File | Change |
|---|---|
| `engine/veterancy.{hpp,cpp}` | NEW pure module (Task 1) |
| `engine/crate.{hpp,cpp}` | NEW pure module (Task 2) |
| `engine/unit_combat.hpp` | + P5 type fields (Task 3) |
| `engine/world.hpp` | `Unit::vet`, `Building` 4 fields, World API (Tasks 3–5) |
| `engine/building.hpp` | 4 inert fields (Task 3) |
| `engine/world.cpp` | add_combat_unit/add_building, enable_veterancy, step_veterancy, refresh_vet_stats, crates (Tasks 3–5) |
| `engine/combat.cpp` | 3 hooks (Task 4) |
| `include/ra2assets/rules.hpp`, `src/rules.cpp` | `section_string` (Task 6) |
| `engine/main.cpp` | resolve_* + GameScene wiring + render + gate (Tasks 6–8) |
| `cmake/ra2_sources.cmake`, `CMakeLists.txt` | sources + tests |
| `tests/test_veterancy.cpp`, `tests/test_crate.cpp`, `tests/test_world.cpp` | tests |

### Task 1: `engine/veterancy.{hpp,cpp}` — pure formulas

Tests first (`test_veterancy.cpp`): rank thresholds (0.99999994f rookie, 1.0f
vet, 1.9999999f vet, 2.0f elite); `veterancy_add` (E1 200 vs E2 100 → 1/6 per
kill; cap at 2.0 compared on the double sum; owner 0 → cap); `kill_value`
(×2/×3/allied 0); `has_ability` (elite inherits vet list; elite-only ability
not active at veteran); `parse_abilities` (case/whitespace/unknown tokens);
`fire_damage(15,1.0,true,1.1)=16`, `(25,…)=27`; `rof_frames(20,true,.6)=12`,
`(5,true,.6)=3` (PC_53 double rounding — NOT 2); `receive_damage` (min 1,
`/1.5` trunc, armor mult); `speed_value(4,1,true,1.2)=4`, `(7,…)=8`;
`sight_value` dead at 0.0; `self_heal_interval(.016)=14`; `should_self_heal`
phase/hp gates; `promote_levels` (rookie→vet→elite, negative→rookie, elite stays).

### Task 2: `engine/crate.{hpp,cpp}` — RNG + crate math

Tests (`test_crate.cpp`): `GameRandom` determinism for a seed, `ranged` bounds
over 10k draws incl. `lo==hi`, swapped bounds, full-ish ranges; seed table
word 0 equals a hand-computed psdes value; `pick_powerup` cumulative mapping
(weights 10/20 with forced draws via a stub sequence) and zero-weight never
picked; `parse_powerup("10,ARMOR,yes,1.5")`, `"20,<none>,no"`, `%` handling;
`crate_lifetime_frames` within `[B, A]` for regen 3; `initial_crate_count`.

### Task 3: data plumbing — `UnitCombat`, `UnitVet`, `Building`, `add_combat_unit`

Copy type data into `u.vet` + base snapshot always (inert); legacy rank-1
baking only when `!vet_any_`. World tests: a disabled World spawns a
`veteran_rank=1` unit exactly as before (dmg ×1.1 round, HP ×1.5); fields are
copied.

### Task 4: `enable_veterancy`, combat hooks, `step_veterancy`, `refresh_vet_stats`

Tests: kills credit the killer (`veterancy` after N kills), allied kills give 0,
untrainable killers gain nothing, `DontScore` victims nothing; promotion
refreshes `w_damage/w_rof` and swaps to the elite weapon; receive path divides
by VeteranArmor and clamps to 1; self-heal +1 at frame multiples of 14 only; a
NON-DEFAULT `VeterancyRules` (ratio 1.0, combat 2.0) changes the observable
(wiring proof); disabled World: no credit, no change (firewall); building kill
credits building cost.

### Task 5: crates in World

`enable_crates`, `place_crate`, `step_crates` (collection then expiry),
effects + conversion rules, `CrateEvent`s. Tests per effect, the Armor→Money
conversion, expiry relocation, initial count, determinism for a seed.

### Task 6: rules plumbing + `resolve_combat_unit`

`Rules::section_string`; `resolve_combat_unit` P5 keys + `ElitePrimary`;
`resolve_veterancy_rules`, `resolve_crate_rules`. Test: `section_string` on
`test_rules`; asset-backed resolve checked inside the gate.

### Task 7: GameScene wiring + rendering

Enable veterancy (+ crates iff `cfg.crates`) in the GameScene ctor body; load
`PIPS.SHP` (YR first) + `crate` overlay; draw chevrons (frames 14/15) and
crates. Build only (render not hashed).

### Task 8: `--demo-veterancy` gate

`simulate_veterancy` (spec §gate) + `run_demo_veterancy`; run it, pin the hash.

### Task 9: Finalize

README section; ledger entry in `D:\ra2-android\.superpowers\sdd\progress.md`.
