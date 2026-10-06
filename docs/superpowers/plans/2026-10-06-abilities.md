# Unit Special Abilities (batch 1) — Implementation Plan

**Spec:** `docs/superpowers/specs/2026-10-06-abilities-design.md` · **Authority:** `docs/re/abilities.md`
**Branch / worktree:** `p4-abilities` @ `D:\ra2-wt\p4-abilities` (own `build/`).
**Testing rule (user):** build + run ONLY the scoped test cases for code touched (`ra2assets_tests -tc="*abilit*"` etc.); run only the new `--demo-abilities` gate to pin its hash. The controller runs the full suite + all gates once at the end.

## Task 1 — pure math TU (TDD)
`engine/ability_types.hpp` (AbilityKind / WeaponSpec / AbilityProfile / AbilityRules with this install's values as defaults), `engine/abilities.{hpp,cpp}`:
- `rad_spread_leptons(spread)` = `spread*256+128` (§A1)
- `rad_cell_amount(d_leptons, spread_leptons, level)` (§A2)
- `rad_level_steps(duration, delay)` / `rad_add_current(level, time_left, duration)` / `rad_deactivate_amount(amt, steps, time_left, delay)` (§A1/§A3)
- `rad_damage(cell_level, rules)` = `ftol(ftol(min(lvl,max))*factor)` (§A4)
- `teleport_delay_frames(dist, rules)` (§C1), `warp_remaining_init(strength)` (§C2), `c4_delay_frames(c4_delay_minutes)` (§D1), `bomb_due(det_frame, frame)` (§E), `suppression_frames(dmg, threshold)` (§B3), `ifv_weapon_index(mode, count)` (§G), `area_scale(d, radius, pmax)`.
Tests: `tests/test_abilities.cpp` — every worked number in `abilities.md` (53/16-frame teleports, 157-frame GI erase, 27-frame C4, plant+451, 2*dmg−5, CRM60 index 2, rad 100 dmg at the center, falloff halves at 5 cells, decay steps 500/90=5).
Register in `CMakeLists.txt` (tests) + `cmake/ra2_sources.cmake` (engine).

## Task 2 — World state + firewall + inert edits
`Unit`/`Building` fields (inert), `World::register_ability_profile`, `set_ability_rules`, `abilities_any_`, `ability_frame_`, `step_abilities` (empty pass) appended to `World::step`; the guarded edits in `step_units` / `step_combat` / `try_fire` / dispatch / `order_unit_move` / `begin_sell`. Test: a World with no profile behaves identically (fire still deals damage, no frozen skip).

## Task 3 — the pass (TDD per ability, `engine/world_abilities.cpp`)
Bind + IFV sync; fire-event consumption; teleport; deploy + radiation; parasite; temporal; bombs + C4; death loop + area damage. One host test per behavior on a hand-built map (no assets).

## Task 4 — rules resolution + interactive wiring (`main.cpp`)
`resolve_weapon_spec`, `resolve_ability_profile`, `resolve_ability_rules` from the real INI keys; `GameScene` registers the cast + every `[InfantryTypes]` entry with `IFVMode`; `D` deploys the selected desolators; the sidebar sell refusal reads `c4_applied`.

## Task 5 — `--demo-abilities` gate
`abilities_fold`, `simulate_abilities`, `run_demo_abilities`; wedge guards on every recovered number; run once to pin the hash.

## Task 6 — finalize
README section + gate row; ledger entry in `D:\ra2-android\.superpowers\sdd\progress.md`.
