# Projectile Flight (P2) — Implementation Plan

Spec: `docs/superpowers/specs/2026-10-06-projectile-design.md`. Calibration: `docs/re/projectile.md` (+ §12 written in Task 1).
Branch `p2-projectile` (worktree `D:\ra2-wt\p2-projectile`, build dir `D:\ra2-wt\p2-projectile\build`, Ninja + WinLibs MinGW, `FETCHCONTENT_SOURCE_DIR_SDL2` → the main tree's `_deps/sdl2-src`).

**Testing rule (user directive):** each task runs ONLY its scoped doctest filter. Only `--demo-projectile` is ever run as a gate (to pin its hash). The full suite + every existing gate is run once at the end by the controller.

Build: `cmake --build D:\ra2-wt\p2-projectile\build --target ra2assets_tests ra2engine`. Run exes from **PowerShell**.

---

## Task 1 — RE-GATE addendum (docs only)

Write `docs/re/projectile.md` §12 from the existing decompiles (`D:\ra2-re\out\proj\`) + the three short headless passes `p2impl_{1,2,3}.py` (outputs `D:\ra2-re\out\p2impl\`):

- **§12.1 arcing solver unwound** (`FUN_0048A8E0`): with `v`=speed, `d`=horiz, `z`=Δz, `g`: `if z>0 → π/4`; `A = v² − z·g`; `D = v⁴ − 2v²zg − g²d²`; `den = 2(z²/d² + 1)`; `t = (A ± √D)/den` (`+` = selector 0); `θ = acos(√t / v)` — `t` is the **horizontal** speed squared (corrects §4.1's "vertical"). Wrapper `FUN_0048A7E0`: lofted → minus root; else plus root and a numeric-derivative sign test: `θ(d+1) < θ(d)` ⇒ negate. Derivation shows it is the exact textbook quadratic in `vh²`. `FUN_004CACD0` = table `acos` (`π/2 − asin_tbl`).
- **§12.2 LevelHeight = 104 CONFIRMED** (static init `0x466190`: `ftol(tan(90°−60°)·√(2·256²)·0.5)` via the 4096-entry tan table at `0x85C024`), BridgeHeight `0x89CDD4 = ftol(4·104+0.5) = 416`. ⇒ arrival `height < 208`, `IsBlocked` `height ≤ −416`, ground "object in cell" band 150 (`0x7E38D8`).
- **§12.3 Vertical/homing launch speed = 1** (`Fire` `0x6FDxxx`: `if (ROT>0 || Vertical) { if (TechnoType+0x6A4 == 0) speed = 1; bullet.Speed = Weapon.Speed; }`) — corroborates §6.5's "1, +1/frame"; the `+0x6A4` field is unidentified (PROBABLE: 0 for every roster type).
- **§12.4** `MoveTo` stores `TargetCoords = Target->GetTargetCoords()` (the TRUE target, not the scattered aim) — so a scattered shell can miss the arrival test.
- **§12.5** ground branch caveat: the decompile's `Owner==0` split around the `z<floor` block is not fully disentangled; the implementation follows the hunt's §7 CONFIRMED row ("ground impact → detonate").
- **§12.6** `FUN_0065C220` range mapping (swap, top-bit mask, reject > range) recorded for the RNG decision.

Commit: `docs(re): projectile RE-GATE addendum ...`.

## Task 2 — pure projectile core (`engine/projectile.{hpp,cpp}`, TDD)

Types: `ProjKind {Instant, Ballistic, Vertical}`, `ProjectileSpec`, `ProjectileRules {gravity, ballistic_scatter}`, `ProjRng` (xorshift32 + `random(lo,hi)` with the §12.6 mapping + `angle()`), `Projectile` (int pos, double vel, target, attacker…), `LaunchResult`.
Functions:
- `int weapon_speed_leptons(int ini_speed)` — `ftol(min(255, clamp(S,0,100)·256/100))`.
- `int arcing_launch_speed(int dist2d, double g)` — `ftol(sqrt(d·g·1.2))`.
- `bool arc_solve_core(bool lofted, int v, int d, int z, double g, double& theta)` and `bool arc_pitch(int v, int d, int z, double g, double& phi)` (wrapper + 16-bit quantisation).
- `ScatterOffset` §5.1 / §5.2.
- `LaunchResult compute_launch(spec, rules, src, tgt, weapon_range_lept, rng)`.
- `void integrate_ballistic(Projectile&, double g)` / `void integrate_vertical(Projectile&)`.
- `int lepton_to_cell(int l)`.

Tests (`tests/test_projectile.cpp`, `TEST_CASE("projectile: ...")`): speed table (102 @ 1472, 74 @ 768, 96 @ 1280, 121 @ 2048), weapon speed (20→51, 40→102, 100→256→255), solver flat-ground `sin2θ = 1/1.2` (≈28.22°), `z>0 → π/4`, no-root refusal, downward sign test, RNG range mapping (inclusive, bounds, swap, lo==hi), scatter magnitude bounds, integrator one-step exactness (`V.z -= 6`, `ftol`), vertical acceleration to cap, a full flat flight lands within one cell of the target in ≈16 frames at 1472.

Register in `cmake/ra2_sources.cmake` and the `CMakeLists.txt` test line. Scoped run: `ra2assets_tests --test-case="projectile*"`.

## Task 3 — rules plumbing + spec resolution

- `Rules::weapon_projectile(w)` (`Projectile=`), `Rules::weapon_speed(w)` (`Speed=` raw int).
- `UnitCombat::proj` (`ProjectileSpec`, default Instant) ; `Unit::w_proj` copied in `add_combat_unit` / `launch_unit`.
- `main.cpp::resolve_projectile_spec(rules, weapon, range)` called from `resolve_combat_unit` (no change to the `[unit]` log line); `load_vehicle_assets` copies `va.flh` into `va.combat.proj.flh`.
- `resolve_projectile_rules(rules)` → `{Gravity from [AudioVisual] (6), BallisticScatter from [CombatDamage] (1.0→256)}`.
- Tests: `TEST_CASE("rules: weapon_projectile/weapon_speed ...")` (synthetic ini) — scoped `--test-case="rules*projectile*"`.

## Task 4 — World integration (the firewalled flight pass)

- `combat.hpp`: `struct ProjectileLaunch { int attacker, target_unit, target_building; }`; `step_combat(..., std::vector<ProjectileLaunch>* launches = nullptr)`; `deliver_unit_hit(...)` extracted verbatim from `try_fire`; `deliver_building_hit(...)` extracted verbatim from the building branch. New branch only when `launches && u.w_proj.flies()`.
- `World`: `enable_projectiles(const ProjectileRules&)`, `projectiles_enabled()`, `projectiles()`, `take_projectile_events()`, private `projectiles_`, `proj_rng_`, `proj_rules_`, `proj_accum_`, `proj_frame_`, `step_projectiles(dt)` (pure insertion before `step_combat`), `launch_projectile(...)`, `detonate_projectile(...)`.
- Tests (`TEST_CASE("projectile world: ...")` in `tests/test_world.cpp`): firewall closed ⇒ same-frame instant hit and zero events; open ⇒ damage arrives after ≥2 frames; target moved away ⇒ miss; dead-target ⇒ detonates without damage; Vertical bomb from altitude; collision snap onto an interposed enemy; building target damage. Scoped run `--test-case="projectile world*"`.

## Task 5 — interactive game + rendering

- `GameScene` ctor: `world_.enable_projectiles(resolve_projectile_rules(rules_))`.
- `render_world`: draw each live projectile (3×3 shell + 2×2 ground shadow) using the FLH-flash projection (`36.74 px` per cell of z).

## Task 6 — `--demo-projectile` gate

`simulate_projectile` (5 scenarios per spec §Gate) + `run_demo_projectile` (two-run self-compare, witnesses, `projectile_hash`). Run once to pin the hash.

## Task 7 — finalize

README section (behaviour, firewall, RNG decision, deviations, gate hash); append the phase record to `D:\ra2-android\.superpowers\sdd\progress.md` (main tree, gitignored).
