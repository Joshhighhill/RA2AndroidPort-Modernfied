# Combat Phase — Design

**Date:** 2026-07-10
**Status:** Design approved; pending implementation plan.
**Depends on:** Phase A (multi-unit selection & group movement, complete —
13 commits `6ba12c0..0c6b8da`); Foundation Batch RE infrastructure (Ghidra
project at `D:\ra2-re`, headless workflow, `docs/re/` conventions).

## Goal

Units fight: right-click a hostile to attack, idle units auto-acquire and
return fire, damage follows the ORIGINAL game's math, units die and are
removed. GI (Allied, player) vs Conscript (Soviet, hostile) on the real map.

## Scope decisions (settled during brainstorming)

1. **Fidelity: Ghidra-calibrated combat.** All combat DATA comes from the real
   `rulesmd.ini` (Strength, Armor, weapon Damage/ROF/Range/Warhead, warhead
   Verses). The hardcoded FORMULAS (damage application, Verses indexing, ROF
   cooldown, range check, guard/return-fire rules) are recovered from
   `gamemd.exe` via Ghidra and cross-checked against ModEnc/YRpp — the same
   calibration discipline as the CSF/VXL/audio decoders. **Movement stays
   Phase A's model** (no lepton/facing/sub-cell retrofit; that is a separate
   possible future fidelity phase).
2. **Combatants: GI vs Conscript.** One new infantry type (Soviet Conscript),
   resolved from rulesmd exactly as `E1 -> GI` was (id -> `Image=` -> SHP in
   `conquer.mix`). New `house` concept on units (0 = player/Allied,
   1 = hostile/Soviet); hostility = different house. Selection and orders
   apply only to house-0 units; box-select ignores hostiles. No vehicles (no
   voxel rendering this phase).
3. **Behaviors: RA2-style engage.** Right-click an enemy = attack order (move
   into range, fire until it dies). Idle/guarding units auto-acquire hostiles
   in range, return fire when hit, pursue briefly, then resume. Death plays
   the SHP die sequence, then the unit is removed. No veterancy, no
   attack-move/stance commands (see Non-goals).

## Architecture (pure combat core, extend in place)

The proven Phase-A shape: plain-data `Unit`, pure sim functions compiled into
both `ra2assets_tests` and `ra2engine`, SDL confined to renderer/platform.
Boundaries stay ECS-migratable (a later phase's concern).

| Piece | Change |
|---|---|
| `src/rules.cpp` / `include/ra2assets/rules.hpp` | `Rules` gains combat getters: `strength(id)`, `armor(id)` (armor-class string), `primary(id)` (weapon name); weapon lookups `weapon_damage/rof/range/warhead/report(weapon)`; warhead lookup `verses(warhead) -> 11 percentages`. Pure INI reads, same thin-by-reference design. |
| **new** `engine/combat.hpp/.cpp` | **Pure**: `step_combat(units, occ, dt, rng, ...)` — target validation/acquisition, range gating, ROF cadence, damage application, death transitions, pursuit hooks. Plus the Ghidra-recovered damage formula as a standalone testable function. |
| **new** `engine/game_rng.hpp/.cpp` | **Pure**: seedable reimplementation of the game's `Random2Class` generator IF the recovered mechanics use randomness (ROF jitter etc.); engine seeds per session, tests pin a fixed seed. Omitted if RE proves the used paths deterministic. |
| `engine/world.hpp/.cpp` | `Unit` gains `hp`, `house`, `armor_class`, resolved weapon stats, `target` (index or -1), `rof_cooldown`, `dying` anim state. `World::step` = `step_units` then `step_combat`. New `World::order_attack(target_idx)` for the selection. Death removal frees occupancy, clears selection, drops all `target` references to the dead unit. |
| `engine/input.cpp` | Right-click resolves `unit_under` FIRST: hostile under cursor -> `order_attack`; otherwise the existing group move. Ground move-target becomes **level-aware** (folds in the Phase-A final-review recommendation, since this code path is being touched). |
| `engine/iso_renderer.*` | House-remap palettization (unit palette indices 16-31 recolored per house; `TextureCache` key gains a house suffix); HP bars (green/yellow/red) over selected and damaged units; fire + die SHP sequences (single facing, walk-anim precedent). |
| `engine/main.cpp` | Spawns the GI squad (house 0) + a Conscript squad (house 1) apart on the map; `--demo-combat` headless mode. |

Deterministic sim: fixed index order, event-timestamp input, seeded RNG —
a battle replays bit-identically from the same seed.

## Ghidra calibration plan

Each item lands in `docs/re/combat.md` with a High/Med/Low confidence row
(csf/vxl/audio precedent). Recovered from `gamemd.exe` (YR 1.001, preflight
SHA1-checked), anchored by strings/constants; YRpp names guide but addresses
drift (~0x1000, known from csf work).

1. **Damage application** — the `ReceiveDamage`-path math: order of
   operations for `Damage x Verses[armor]`, integer vs float, exact
   rounding/truncation, minimum-damage / 0%-Verses special cases.
2. **Verses indexing** — confirm the 11 armor-class order (`none, flak,
   plate, light, medium, heavy, wood, steel, concrete, special_1,
   special_2`) and the `%` list parsing/storage.
3. **ROF semantics** — `ROF=` in 15 Hz logic frames; the cooldown reset rule
   and whether random jitter is added (if yes -> `game_rng`).
4. **Range semantics** — `Range=` in cells, checked in leptons (256/cell);
   the exact distance metric. Our continuous `fx/fy` converts exactly
   (x256), so the check is implemented bit-faithfully WITHOUT retrofitting
   movement.
5. **Guard / return fire / acquire** — guard range, retaliation target
   choice, pursuit-then-return behavior.

**Fallback rule:** any mechanic the binary won't give up cleanly is
implemented per ModEnc-documented behavior, marked **Low** confidence and
flagged honestly in the doc (vxl span-walk precedent).

**GPL/licensing boundary (unchanged):** ModEnc/YRpp/XCC are READ off-repo for
cross-checking only; nothing is copied into the repo. No game assets, binary,
or Ghidra project committed.

## Verification (no XCC-style oracle exists for combat)

Three legs:

1. **Hand-computed vectors as unit tests** (the ADPCM approach): from the
   recovered formula + real rulesmd numbers, compute by hand the exact damage
   per shot, shots-to-kill, and frames between shots for GI-vs-Conscript both
   directions; assert them in synthetic tests.
2. **Cross-check** against ModEnc + YRpp (documented in `docs/re/combat.md`).
3. **Guarded real-asset tests**: load the real rulesmd.ini through
   `Ini`/`Rules` and pin the resolved GI/Conscript/weapon/warhead values;
   skip cleanly (MESSAGE + return) without the install.

## Rendering & UX

- **House colors:** unit-palette indices 16-31 are the house remap ramp;
  recolor per house at palettize time (authentic mechanism, cheap). Cache key
  includes house.
- **Combat feedback:** SHP fire-sequence frames while shooting (single
  facing); HP bar above selected and damaged units (green > 2/3, yellow >
  1/3, red below); SHP die sequence on death, then removal. No projectile or
  tracer visuals (infantry fire reads through the fire anim + HP drop).
- **Input:** right-click = attack if a hostile is under the cursor
  (`unit_under`, already level-aware), else group move (made level-aware this
  phase).
- **`--demo-combat [seed]`**: spawn both squads in range, sim to one side's
  elimination (capped); print a per-shot damage log + survivor count; write
  start/end screenshots to the scratchpad. The optional seed feeds `game_rng`
  IF the recovered mechanics use randomness (defaults to a fixed value);
  if RE proves the paths deterministic, the argument is accepted and unused.
  Either way the battle is bit-deterministic for a given invocation, so the
  expected outcome is asserted exactly.

## Testing

- **Pure `combat` module:** acquisition priority (explicit order >
  return-fire > auto-acquire, deterministic nearest-with-index-tie-break);
  exact range gating at the lepton boundary; exact ROF frame cadence;
  hand-computed damage vectors; death invariants (occupancy freed, selection
  cleared, no dangling `target` indices) asserted every step; pursuit
  break-off.
- **`Rules` extension:** synthetic INI text tests + guarded real-rulesmd
  value pins.
- **Input:** right-click-enemy vs right-click-ground dispatch.
- **World integration:** two squads battle to elimination deterministically
  on a hand-built grid; per-frame invariants.
- **Demo + visual gate:** `--demo-combat` screenshots inspected; full suite
  green with AND without the install; arm64 NDK build clean.

## Success criteria

1. Real rulesmd.ini stats drive GI and Conscript (guarded pins green).
2. `docs/re/combat.md` documents the recovered damage/Verses/ROF/Range/guard
   mechanics with a confidence table; hand-computed vectors pass.
3. Right-click attack, auto-acquire, return fire, and pursuit work
   (unit-tested + demo-confirmed).
4. Deaths are clean: die animation, removal, occupancy freed, no dangling
   targets (invariant-tested every frame).
5. House-remap rendering distinguishes sides; HP bars render.
6. `--demo-combat` is bit-deterministic for a fixed seed and its outcome is
   asserted; screenshots confirm visually; full suite + arm64 green.

## Non-goals (deferred)

Veterancy, attack-move/stance commands, projectile ballistics (vehicles'
phase), voxel rendering, buildings, fog of war, AI beyond guard behavior,
and **weapon sound playback** — the audio decoder is ready, but the SDL
mixer/playback layer is its own piece (strong candidate for the next phase).
Movement fidelity retrofit (leptons/facings/sub-cells) remains a separate
opt-in future phase.
