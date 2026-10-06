# Projectile Flight (real ballistics) — Design Spec

**Status:** APPROVED for implementation (controller-dispatched phase P2)
**Roadmap slot:** `docs/superpowers/ROADMAP-completion.md` **P2 — "Projectile flight (real ballistics)"** ("Every weapon is currently instant-hit. The single most-visible fidelity gap in normal play." Gate: `--demo-projectile`).
**Depends on:** Combat (`engine/combat.{hpp,cpp}` — `step_combat`, `try_fire`, `damage_vs`, `apply_spread_damage`), Voxel Vehicles (turret facing, `PrimaryFireFLH`), CB-2 Aircraft (`Unit::altitude`, the Kirov `BlimpBomb`), Buildings (`foundation_cells`, `kill_building`), Multi-house (`World::is_enemy`).
**Calibration authority:** `docs/re/projectile.md` (the `BulletClass`/`[Projectile]` hunt over YR 1.001 `gamemd.exe`, SHA1 `7BE4E5508655F8AF482AB3FF5965D196E1B6B6C7`) **plus its §12 implement-phase RE-GATE addendum written by this phase** (Task 1), which closes three of the hunt's open items from the existing decompiles + three short headless Ghidra passes (`D:\ra2-re\scripts\p2impl_{1,2,3}.py`).

## Goal

Today `combat.cpp::try_fire` applies damage the instant the ROF timer expires. This phase makes every **non-`Inviso`, non-homing** weapon fire a real projectile that flies on the recovered integrator and detonates on the recovered termination rules, and only *then* calls the existing — unchanged — damage layer:

1. **Ballistic** (`ROT==0`, `!Vertical`, `!Inviso`): `[Cannon]` (Rhino/Grizzly/Apocalypse), `[FlakTProj]` (Flak Track ground gun), `[Ballistic]` (Destroyer). Launch speed `ftol(sqrt(d·Gravity·1.2))`, the unwound arcing-angle solver, `V.z -= Gravity; pos += V` per logic frame.
2. **Vertical** (`[BlimpBombP]`, the Kirov bomb): launched at 1 lepton/frame along the aim vector, accelerating `+Acceleration`/frame to the weapon's `Speed` cap, direction frozen.
3. **Inaccuracy** (`Inaccurate`+`Arcing`): the two recovered `Fire`-time scatter formulas, drawn from a **deterministic projectile RNG** (design decision §D3).

Infantry small arms (`[InvisibleLow]`) are `Inviso` and **stay on the instant path** — the hunt proved Inviso is a 1-frame teleport, i.e. the current model is already faithful (projectile.md §6.3, §11.3).

## Global Constraints

- **All existing byte-identity gates MUST stay bit-identical** (combat logs=78, tank logs=144, base, econ, prod, ai-base, ai-team, ai-trigger, transport, aircraft, naval, superweapon, techtree, capture, shroud, multihouse, skirmish, roster, maps, + repair if merged). This phase adds **`--demo-projectile`** (`projectile_hash`).
- **Firewall (the established four-part rule).** Combat is on almost every gate path, so this phase's firewall is stricter than any previous one:
  1. **Inert-by-default state.** `UnitCombat::proj` / `Unit::w_proj` (`ProjectileSpec`) default to `kind = Instant`. They *are* resolved from rules for every unit (resolve_combat_unit) — but **nothing reads them unless the World flag is open**.
  2. **`World::projectiles_enabled_` (false by default).** Opened ONLY by `World::enable_projectiles(const ProjectileRules&)`, called ONLY by `GameScene` (the interactive game) and `--demo-projectile`. No existing demo calls it.
  3. **`step_combat` signature gains a trailing `std::vector<ProjectileLaunch>* launches = nullptr`.** `World::step` passes `projectiles_enabled_ ? &launches : nullptr`. With `nullptr` every branch of `try_fire` and the building-attack path is the pre-phase code verbatim (the new branch is `if (launches && u.w_proj.flies())`). The damage-delivery tail of `try_fire` is extracted into a helper `deliver_unit_hit` that is byte-for-byte the old body (same statements, same order) so the instant path and the detonation path share it.
  4. **`step_projectiles` is a pure insertion** in `World::step` immediately before `step_combat`, starting `if (!projectiles_enabled_) return;`. The projectile list, the projectile RNG and the event log are new World members **not read by `world_hash`**. Rendering draws `world.projectiles()`, which is empty in every existing demo → no existing screenshot changes.
- **Calibrate-first.** Every constant cites `projectile.md` by §. Items the hunt + RE-GATE leave PROBABLE / `[RE-FALLBACK]` keep that label in a code comment.
- **Module split:** pure physics (launch solver, integrators, scatter, RNG range-mapping) → new `engine/projectile.{hpp,cpp}` (no SDL, no World; registered in `cmake/ra2_sources.cmake` + `CMakeLists.txt` test line with `tests/test_projectile.cpp`). World integration (launch → flight → collision → detonation → damage) → `engine/world.cpp`. Rules parsing → `Rules::weapon_projectile` / `weapon_speed` in `src/rules.cpp`. Spec resolution + rendering + gate → `engine/main.cpp`.
- **Testing rule (user directive for this phase):** scoped test cases only during TDD (`--test-case="projectile*"` etc.); only the new gate is run to pin its hash; the full suite + all gates are run once by the controller at the end.

## Calibration summary (the behavior contract)

### C0. Rules ground truth (projectile.md §0/§2, CONFIRMED)

| Key | Section | Value | Use |
|---|---|---|---|
| `Gravity` | **`[AudioVisual]`** (NOT `[General]`) | 6 | per-frame `V.z -=` |
| `BallisticScatter` | `[CombatDamage]` | 1.0 → `ftol(1.0·256)` = 256 lept | scatter radius |
| weapon `Speed` | `[weapon]` | `ftol(min(255, clamp(S,0,100)·256/100))` lept/frame | **only** Vertical/homing cap |
| `Arcing`,`Inviso`,`Inaccurate`,`FlakScatter`,`Vertical`,`Floater`,`ROT`,`Acceleration`,`DetonationAltitude` | `[Projectile]` | per type | classification + flight |

### C1. Launch (`TechnoClass::Fire` `FUN_006FD210`, projectile.md §4 + §12)

```
src  = firer coords + FLH  (F along the turret/aim yaw, H up; aircraft add altitude)
tgt  = target coords (unit (fx,fy)·256, z=0 | nearest building foundation cell)
d2   = ftol(sqrt(Δx²+Δy²))                                  # 2-D, pre-scatter
spd  = ROT==0 ? ftol(sqrt(d2·g·1.2)) : WeaponSpeed          # g halved if Floater
Δ   += scatter (§C3)                                        # Inaccurate && Arcing
d3   = ftol(sqrt(Δx²+Δy²+Δz²)); if (d3/2 < spd) spd = d3/2  # integer /2
if (Vertical || ROT>0) spd = 1                              # §12.3
pitch: Arcing    -> solver(spd, horiz=ftol(|Δxy|), Δz, g)  (§12.1), refuse shot if no root
       otherwise -> 0, or sign(Δz)·atan2(|Δz|−20, horiz) when |Δz| > 200   [PROBABLE sign]
pitch quantised through the 16-bit binary angle: p16 = ftol((θ−π/2)·(−65534/2π)),
       φ = (p16 − 0x3FFF)·(−2π/65534)
V    = (dir_xy·spd·cosφ, spd·sinφ)                          # |V| = spd
```

### C2. Flight (`BulletClass::Update` `FUN_004666D0`, projectile.md §6/§7 + §12)

Positions are **integer leptons**, velocity **doubles**; each frame `pos = ftol(pos + V)` per axis.

- **Ballistic:** `slow = |V| < 8.0`; `V.z -= g`; `pos += V`.
- **Vertical:** `s = ftol(|V|)`; `if (s < SpeedCap) V = normalize(V)·(s + Acceleration)`; `pos += V`.

Termination, evaluated on the new position in this order (first match wins):

| # | Test | Applies to | Source |
|---|---|---|---|
| 1 | `z < floor` → detonate at `z = floor` | Ballistic | §7 "Ground impact", `FUN_004666D0` |
| 2 | `z > DetonationAltitude` or `height < 0` | Vertical | §6.2 |
| 3 | new cell == target cell **and** `height < 2·LevelHeight` (= **208**) | Ballistic | §7 + §12.2 (LevelHeight=104 CONFIRMED) |
| 4 | first live unit in the new cell, enemy of the owner, 3-D distance ≤ **127** → detonate, **snap onto it unless `Inaccurate`** | both | §5.4/§7 |
| 5 | new cell off the map → **removed silently, no damage** | both | §7 |
| 6 | `slow`, or `|V| < 10` and `height ≤ 9` | Ballistic | §7 |
| 7 | `height ≤ −4·LevelHeight` (= −416) | both | `IsBlocked` §12.2 |

`floor` is 0 (the clone's combat plane is flat — projectile.md §11.6; `combat.md` §6).

### C3. Inaccuracy (projectile.md §5, CONFIRMED formulas)

Uniform angle: `r = Random(0,0x7FFFFFFE)`, `a16 = (int16)ftol((r/2³¹·2π − π/2)·(−65534/2π))`, `θ = (a16 − 0x3FFF)·(−2π/65534)`.
- **§5.1** `Inaccurate && Arcing && !(FlakScatter && !Inviso)`: `r = Random(BS/2, BS)`, `Δ += (ftol(cosθ·r), ftol(−sinθ·r))`.
- **§5.2** `Inaccurate && Arcing && FlakScatter && !Inviso` (`[FlakTProj]`): `r = Random(0,BS)·ftol(|Δ|₃)/WeaponRangeLeptons` (integer), same offset.
- §5.3 (`[FlakProj]`, the Inviso AA gun) is AA — **out of scope** (no AA in the clone).

### C4. Detonation (projectile.md §7/§8)

- `Explode` snap: if `!Inaccurate` and the original target unit is alive and within **32** leptons (3-D) → impact := target coords.
- Damage — the **existing** layer, only rescheduled:
  - warhead `CellSpread >= 1` → `apply_spread_damage(units, occ, attacker, center@impact)` (unchanged applicator).
  - else single-target `deliver_unit_hit` (the exact old `try_fire` tail: `damage_vs`, idle retaliation, death) to the **victim**: the collided unit (rule 4), else the original target if it is alive and its cell == the impact cell, else **nobody (a miss)**.
  - building target: if the impact cell is in the (alive) building's foundation → the old building-hit code (`damage_vs` vs `armor_ix`, `kill_building`, clear other attackers).
- **Target dies mid-flight:** a ballistic/vertical shell never re-aims; it keeps flying to the stored target cell and detonates there (§8 — "retarget onto the cell"). The victim lookup simply finds nothing alive.

## Design decisions

### D1. Firewall = World flag + nullable launch sink (see Global Constraints)
Chosen over "a new combat function" because it keeps ONE combat loop (no fork to drift) and makes the old path provably identical: with `launches == nullptr` the only new instructions are one pointer test per shot.

### D2. Frame clock
Projectiles integrate per **logic frame** (15 Hz) — the hunt proved there is no `dt` (semi-implicit Euler, one step per frame). `World::step(dt)` accumulates `dt` into `proj_accum_` and runs `while (proj_accum_ >= 1/15 − 1e-9)` whole frames. The gate steps at exactly `1/15`, so 1 step = 1 frame. A projectile launched in frame N first moves in frame N+1 (matching the hunt's "detonates on the next Update").

### D3. RNG for inaccuracy (the sync-RNG question)
The original draws from `Scenario->Random` — the single lockstep RNG (CONFIRMED, `[0xA8A198]+0x218`). The clone cannot reproduce that stream (different generator, different call history). Decision:
- A **dedicated** `ProjRng` (xorshift32, fixed seed `0x9E3779B9`) owned by World, consumed **only** by projectile scatter draws, in launch order (ascending attacker index — `step_combat`'s loop order), so a run is bit-reproducible on every platform.
- **Faithful range mapping:** `Random(lo,hi)` reproduces `FUN_0065C220` exactly — swap if `hi<lo`, find the top set bit of `hi−lo`, draw masked words and **reject** values `> hi−lo` (unbiased, inclusive) — so the *distribution* is the original's; only the *bit source* differs.
- **Why dedicated, not shared with `ore_rng_`/AI rng:** sharing would let the mere act of firing a Flak Track perturb ore growth and AI picks, coupling unrelated systems' determinism; a separate stream keeps each system's reproducibility independent and keeps the firewall airtight (no existing RNG stream is touched). Recorded as **[DEVIATION — RNG stream]** (projectile.md §11.1 extended).

### D4. Coordinates
Clone cell space → leptons: `x = fx·256`, `y = fy·256`, the cell of a lepton point is `floor(x/256 + 0.5)` (a unit at integer `fx` sits at its cell's lepton centre, matching `dist_leptons`). The yaw is a direction vector in this space (no 16-bit yaw quantisation — at these ranges the original's quantisation is < 0.2 lepton). `FLH` forward uses the unit's `turret_facing` (vehicles; the aim gate guarantees it points at the target when firing) via `facing_u16_from_delta`'s convention `(sin a, −cos a)`.

## Scope

**IN:** ballistic + vertical flight, the arcing solver, launch-speed rule, speed cap, scatter §5.1/§5.2, termination rules 1–7, Explode snap, collision snap, dead-target behaviour, building targets, deterministic RNG, simple in-game rendering (a shell dot + ground shadow), `--demo-projectile`.

**CUT / DEFERRED (with reasons):**
- **Homing (`ROT>0`: Torpedo, Medusa, Maverick, MammothTusk…)** — stays instant behind the firewall. The steering function `FUN_005B1B20` is only partially unwound, `MissileSpeedVar`/`MissileROTVar` consumers are not located and the `Acceleration` absent-key default is unpinned (projectile.md §5.5/§6.4 PROBABLE/RE-FALLBACK). Implementing it now would be guessing.
- **`[FlakProj]` (Inviso AA scatter §5.3)** — no AA in the clone.
- **Inviso cliff/wall line trace** (`FUN_00587AD0`) — the clone has no cliff/wall LOS model.
- **The lofted root** (`FUN_0070C9C0` "blocked → high arc") — always the low root; needs an obstacle LOS model.
- **Bouncy/Airburst/Cluster/Shrapnel/Degenerates/Floater/Level/AA fuse** — no roster weapon sets them (projectile.md §11.5).
- **Projectile art** (`Image=120MM`, `ZBOMB` SHPs) — a cosmetic follow-up; a drawn dot + shadow shows the arc.
- **Terrain height in combat** — `floor = 0` (§11.6).

## Gate: `--demo-projectile` (`projectile_hash`)

Two-run compute-and-self-compare (rendered hidden window + headless, fresh Worlds), `dt = 1/15`, five deterministic scenarios on the default map, each in its own fresh World:

1. **firewall** — a World *without* `enable_projectiles`: a Rhino's first hit lands in the same frame it fires (instant path intact).
2. **arc** — Rhino vs a stationary enemy at 5 cells: witness launch speed == `ftol(sqrt(d·6·1.2))`, flight frames ≥ 2, apex > 0, damage only at detonation, `hit=90` vs heavy.
3. **miss** — the target is moved 4 cells away right after the launch: the shell detonates in the old cell, **no damage**.
4. **kirov** — a Kirov bombs two adjacent Conscripts: launch speed 1, accelerating to 51, flight ≥ 20 frames, the 2-cell splash kills both.
5. **flak** — a Flak Track fires 6 shots at a Conscript: scatter offsets drawn, non-zero, identical across runs.

`projectile_hash` = FNV fold of every projectile event (frame, kind, attacker, x, y, z, victim, damage) + `world_hash` at each scenario end. Exit 1 on any mismatch or failed witness. No hash literal in code (README pins it).
