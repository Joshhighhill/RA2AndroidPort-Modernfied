# P5 Veterancy + Crates — Design Spec

Calibration authority: `docs/re/veterancy.md` (every value/formula below cites
its § there). Roadmap: `docs/superpowers/ROADMAP-completion.md` P5, gate
`--demo-veterancy`.

## Goal

Replace the capture phase's minimal "rank-1 = spawn-baked ×1.1 damage / ×1.5 HP"
with the original's real veterancy system — kill-based experience
(`VeteranRatio`), veteran/elite ranks capped at `VeteranCap`, per-type
`VeteranAbilities`/`EliteAbilities`, the exact firepower/armor/ROF/speed integer
chains, elite weapons, elite self-heal, chevron rendering — and add multiplayer
crates (`[CrateRules]`/`[Powerups]`: spawn, lifetime/relocation, weighted pickup,
the conversion rules, and the eight effects that can occur with stock weights).

## Global constraints

1. **Byte-identity firewall.** Every existing gate builds a World that never
   calls `enable_veterancy` / `enable_crates`. With `vet_any_ == false`:
   * `add_combat_unit` keeps the legacy spawn-baked rank-1 path (capture gate);
   * no kill credit, no rank change, no self-heal, no stat refresh;
   * `combat.cpp`'s new receive-damage hook is behind `v.vet.recv` (false by
     default; only `enable_veterancy` / a vet-enabled spawn sets it) — the
     existing `damage_vs(u.w_damage, …)` path is literally unchanged;
   * the only writes on the legacy path are `killed_by` stamps on a unit/
     building at the moment it dies — read by nothing outside the firewalled
     pass and by no hash.
   Crates additionally require `crates_any_` (opened only by `enable_crates`).
2. **Enabled only by**: `GameScene` (the interactive game — veterancy always,
   crates iff the lobby's existing CRATES toggle) and `--demo-veterancy`.
   `launch_skirmish` (shared with `--demo-skirmish`) is NOT touched.
3. **Exact arithmetic.** The CRT runs x87 at 53-bit precision (veterancy.md
   V16), so every formula is reproduced with plain `double` ops, one operation
   per statement (no FMA contraction), `ftol` = truncation toward zero.
4. Pure modules (`veterancy.{hpp,cpp}`, `crate.{hpp,cpp}`) are World/SDL-free and
   doctest-covered; World owns the mutation passes.
5. Scoped testing during implementation; only `--demo-veterancy` is run (to pin
   its hash). The full suite + all gates are run once by the controller.

## Calibration summary (the behavior contract)

| Id | Rule | veterancy.md |
|---|---|---|
| R1 | `veterancy` is a `float`; veteran ⇔ `1.0f <= v < 2.0f`; elite ⇔ `v >= 2.0f` | V1 |
| R2 | kill: `v = (float)min(Cap, (double)victim/(owner*Ratio) + v)` (cap compared on the double sum) | V2 |
| R3 | victim value = victim Cost ×2 if veteran, ×3 if elite, 0 if allied; `DontScore` victims credit nothing; only `Trainable` killers gain; owner cost = killer Cost | V3 |
| R4 | `HasAbility(a) = (vet && V[a]) || (elite && (V[a] || E[a]))` | V4 |
| R5 | fire damage `d = ftol(FPmult*Damage)`, FIREPOWER → `ftol(d*VeteranCombat)` | V5 |
| R6 | receive `d = ftol(d/ArmorMult)`, STRONGER → `ftol(d/VeteranArmor)`, `max(d,1)`, THEN Verses | V6 |
| R7 | ROF `r = ftol(ROF)`, ROF ability → `ftol(r*VeteranROF)` (jitter NOT applied — §Scope) | V7 |
| R8 | speed `s = ftol(Speed*SpeedMult)`, FASTER → `ftol(s*VeteranSpeed)` | V8 |
| R9 | sight ×VeteranSight only if `VeteranSight != 0` (dead on this install) | V9 |
| R10 | elite uses `ElitePrimary` when the type defines one | V10 |
| R11 | self-heal +1 HP on global frames `% ftol(RepairRate*900)` (=14) for `SelfHealing=yes` or SELF_HEAL holders, damaged and alive, not carried | V11 |
| R12 | rank-change events (first observation from rank -1 is silent); elite arms `EliteFlashTimer` | V12 |
| R13 | chevrons: `PIPS.SHP` (YR `conqmd.mix`) frame 14 veteran / 15 elite at draw point +(5,2) infantry / +(10,6) others, always drawn | V13 |
| R14 | `Trainable` default yes, abilities default none, `CrateGoodie` default no | V14, C8 |
| R15 | spy-factory produced unit (vet-enabled world) = `veterancy = 1.0f` iff Trainable — not HP/damage baking | V15 |
| R16 | crate RNG = exact R250/psdes `RandomClass` port, its own instance | C1 |
| R17 | initial crates `clamp(players, CrateMinimum, CrateMaximum)` | C2 |
| R18 | placement: 1000 tries of random grid cell → nearest valid cell; lifetime `ftol((A-B)*(Random(0,0x7FFFFFFE)*k)+B)`, A=Regen·1800, B=Regen·450; expiry → remove + place new | C3, C4 |
| R19 | pickup: `Random(1,ΣW)` cumulative pick (typed crates skip it) → conversion rules → remove + place replacement → effect | C5, §3.3 |
| R20 | effects: Money `Random(arg,arg+900)`; HealBase; Reveal; Armor/Firepower (any house, radius, mult==1.0); Speed (foot, not aircraft); Veteran (+arg levels, Trainable); Unit (CrateGoodie pool) | C6, C7 |

## Current-engine seams

* `combat.cpp::try_fire` / `apply_spread_damage` / the building branch compute
  `damage_vs(u.w_damage, …)` and start death at `hp <= 0` — the kill site and
  the receive-damage site.
* `World::add_combat_unit` — copies `UnitCombat` onto `Unit`; holds the legacy
  rank-1 baking.
* `World::step` — the firewalled passes are pure insertions (`step_capture`,
  `step_repair_sell` precedent).
* `resolve_combat_unit` (main.cpp) — the one place `UnitCombat` is built from
  rules; extended with the P5 per-type keys (inert defaults for hand-built
  `UnitCombat`s).
* `render_world` — draws units then HP bars; chevrons draw beside the HP bar,
  crates in the overlay pass. Not hashed.

## Architecture

### A. `engine/veterancy.{hpp,cpp}` — pure

```cpp
enum class Ability { Faster, Stronger, Firepower, Scatter, Rof, Sight, Cloak,
  TiberiumProof, VeinProof, SelfHeal, Explodes, RadarInvisible, Sensors,
  Fearless, C4, TiberiumHeal, GuardArea, Crusher };          // 18, original order
using AbilityMask = std::uint32_t;
AbilityMask parse_abilities(const std::string& csv);         // case-insensitive tokens
struct VeterancyRules { double ratio=3.0, combat=1.1, speed=1.2, sight=0.0,
  armor=1.5, rof=0.6, cap=2.0, repair_rate=0.016; int elite_flash=150; };
struct WeaponStats { int damage=0, rof=0; double range=0; std::vector<int> verses;
  double cell_spread=0, percent_at_max=1.0; };
int  vet_rank(float v);                 // 0 rookie, 1 veteran, 2 elite (negative -> 0)
bool is_veteran(float), is_elite(float);
float veterancy_add(float v, int owner_cost, int victim_cost, const VeterancyRules&);
bool has_ability(float v, AbilityMask vet, AbilityMask elite, Ability a);
int  kill_value(int victim_cost, float victim_vet, bool allied);
int  fire_damage(int damage, double fp_mult, bool firepower, double combat);
int  receive_damage(int d, double armor_mult, bool stronger, double vet_armor);
int  rof_frames(int rof, bool rof_ability, double vet_rof);
int  speed_value(int speed, double speed_mult, bool faster, double vet_speed);
int  sight_value(int sight, bool sight_ability, double vet_sight);
int  self_heal_interval(double repair_rate);      // ftol(rate*900)
bool should_self_heal(std::uint64_t frame, int interval, bool eligible, int hp, int max_hp);
float promote_levels(float v, int levels);       // the Veteran-crate loop
struct UnitVet { … per-unit state (below) … };
```

`UnitVet` (embedded in `Unit` as `vet`, all inert defaults): `float veterancy`,
`int last_rank=-1`, masks, `trainable=true`, `dont_score`, `self_healing`,
`cost`, `rules_speed`, `base` + `elite` `WeaponStats`, `has_elite`,
`base_speed_cps`, `base_sight`, `firepower_mult/armor_mult/speed_mult = 1.0`,
`recv=false`, `vet_armor=1.5`, `killed_by=-1`, `credited=false`,
`elite_flash=0`.

### B. `engine/crate.{hpp,cpp}` — pure

```cpp
class GameRandom { explicit GameRandom(std::uint32_t seed);
  std::uint32_t next(); int ranged(int lo, int hi); };   // exact RandomClass port
enum class Powerup { Money, Unit, HealBase, Cloak, Explosion, Napalm, Squad,
  Darkness, Reveal, Armor, Speed, Firepower, ICBM, Invulnerability, Veteran,
  IonStorm, Gas, Tiberium, Pod };                        // 19, original order
struct PowerupDef { int weight=0; std::string anim; bool naval=false; double arg=0; };
struct CrateRules { int minimum=1, maximum=255; int radius_leptons=768;
  double regen=3.0; std::array<PowerupDef,19> powerups; };
PowerupDef parse_powerup(const std::string& value);     // "10,ARMOR,yes,1.5" (+% -> *0.01)
int pick_powerup(GameRandom&, const std::array<PowerupDef,19>&);   // Random(1,ΣW)
int crate_lifetime_frames(GameRandom&, double regen);
int initial_crate_count(int players, const CrateRules&);
```

### C. `Unit` / `UnitCombat` / `Building` fields

* `UnitCombat` (+ inert defaults): `cost`, `trainable=true`, `dont_score`,
  `self_healing`, `crate_goodie`, `vet_abilities`, `elite_abilities`,
  `rules_speed`, `has_elite`, `elite` (`WeaponStats`).
* `Unit`: `UnitVet vet`.
* `Building`: `bool vet_recv=false; double armor_mult=1.0; int killed_by=-1;
  bool vet_credited=false;`.

### D. `combat.cpp` — three minimal hooks

```cpp
int raw = u.w_damage;
if (v.vet.recv) raw = receive_damage(raw, v.vet.armor_mult, <STRONGER>, v.vet.vet_armor);
const int dmg = damage_vs(raw, u.w_verses, v.armor_ix);
...
if (v.hp <= 0) { v.vet.killed_by = attacker_idx; ... }        // inert stamp
```

The same in `apply_spread_damage` (receive mods applied to the full damage
before the existing falloff, attacker index recovered by pointer arithmetic
when the attacker lives in `units`, else -1 — the nuke's virtual detonator) and
in the building branch (`b->vet_recv` → `ftol(d/armor_mult)`, `max(1)`).
STRONGER is evaluated from `v.vet` via `has_ability`.

### E. `World`

* `enable_veterancy(VeterancyRules, std::vector<int> building_costs)` — opens
  `vet_any_`, initialises every existing unit (`vet.recv`, base snapshot,
  `last_rank`), and every building's `vet_recv`.
* `add_combat_unit` — always copies the type data into `u.vet` and snapshots
  base stats (inert); when `vet_any_`: `recv=true`, and a `veteran_rank>0`
  request becomes `veterancy=1.0f` (if Trainable) instead of baking; stats
  refreshed. `add_building` sets `vet_recv` when `vet_any_`.
* `step_veterancy()` (after `step_combat`, firewalled): (1) credit every newly
  dead unit/building to its `killed_by` (R2/R3); (2) rank-change detection →
  `VetEvent{unit, old, new}` + refresh stats + elite flash; (3) self-heal on the
  global 14-frame phase (R11). A `vet_frame_` counter advances every `step`.
* `refresh_vet_stats(Unit&)` — recompute `w_damage/w_rof/w_range/w_verses/
  w_cell_spread/w_percent_at_max/speed_cps/sight` from base/elite + mults + rank.
* `enable_crates(CrateConfig)` (requires vet) — rules, seed, player count,
  unit pool; places the initial crates. `place_crate(x,y,powerup=-1)` (typed
  crate seam — the original's `PlaceCrate(cell,type)`), `crates()`,
  `take_crate_events()`.
* `step_crates()` (after `step_units`, firewalled): collection by any alive,
  non-dying, non-carried, non-aircraft unit standing on a crate cell (ascending
  index), then lifetime expiry → relocation.
* `take_vet_events()`.

### F. `main.cpp`

* `resolve_combat_unit`: read `Cost`, `Trainable`, `DontScore`, `SelfHealing`,
  `CrateGoodie`, `VeteranAbilities`, `EliteAbilities`, `Speed` (int), and
  `ElitePrimary` (damage/rof/range/warhead verses/CellSpread/PercentAtMax).
  `Rules::section_string` added for the list/string keys.
* `resolve_veterancy_rules(rules)`, `resolve_crate_rules(rules)`.
* `GameScene` ctor body: `enable_veterancy(...)`; if `cfg.crates`:
  `enable_crates(...)` with a time-based seed, players = human slots, pool =
  rostered `CrateGoodie=yes` vehicles.
* `render_world`: chevrons (PIPS.SHP frames 14/15, YR `conqmd.mix`, palette
  `palette.pal` → fallback unit palette) for vet/elite units (or legacy
  `veteran_rank`), crates (`crate` theater overlay, ore palette).
* `--demo-veterancy` (the new gate).

## The gate `--demo-veterancy`

Rendered + headless runs from fresh Worlds, witness + `veterancy_hash`
byte-compared (the capture-gate pattern). Real `E1`/`E2` via
`resolve_combat_unit` + real `[General]`/`[CrateRules]`/`[Powerups]`.

1. **Promotion by kills**: an E1 hero vs a column of weaponless E2 targets fed
   one at a time; witness every kill's veterancy, the veteran and elite frames,
   the stat refresh (16 dmg / 12 ROF at veteran; M60E 27 dmg / 12 ROF elite).
2. **Armor**: an armed enemy E2 shoots the elite hero and a rookie control —
   hit sizes differ by VeteranArmor.
3. **Self-heal**: the damaged elite hero regains +1 HP per 14 frames.
4. **Crates**: `enable_crates(seed)`; witness the initial crate + its lifetime;
   typed crates under collectors exercise Money/HealBase/Reveal/Armor/
   Firepower/Speed/Veteran/Unit and the conversion rules (second Armor crate on
   an already-armored collector → Money); a random crate exercises the weighted
   pick; time-skip past a lifetime proves relocation.
Wedge guards make every stage mandatory.

## Scope / non-goals (cut, with reason)

* ROF `Random(0,2)` jitter — would put a sync-RNG draw in `combat.cpp` and move
  every combat gate; reported as a contradiction of combat.md §5 (veterancy.md §6).
* Promotion sound/EVA and the elite flash render — no EVA queue; cosmetic. The
  events are emitted for a later audio hook.
* SCATTER/CLOAK/SENSORS/EXPLODES/C4/… abilities — no systems.
* Zero-weight powerups; forced-MCV crate; spawner/occupant kill re-routing;
  aircraft FASTER (flight speed); `NearbyLocation` internals ([RE-FALLBACK]
  ring search); `MapRect` (grid bounds); crate pickup animations/sounds;
  water crates only when a water grid exists.
* Unit crate draws from the injected pool with one `Random(0,n-1)` (the
  original re-rolls over all UnitTypes) — [RE-FALLBACK].

## Testing strategy

Doctest: `test_veterancy.cpp` (pure formulas incl. the 5×0.6 / 25×1.1 boundary
cases, Add with zero owner cost, abilities parse, rank thresholds),
`test_crate.cpp` (RNG determinism + range, psdes seed table, pick cumulative,
lifetime bounds, powerup parse), `test_world.cpp` additions (kill credit
end-to-end, legacy path unchanged when disabled, stat refresh, self-heal phase,
crate effects, non-default rules injection proves wiring). Gate: `--demo-veterancy`.
