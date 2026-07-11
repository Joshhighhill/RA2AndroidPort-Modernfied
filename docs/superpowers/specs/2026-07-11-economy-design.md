# Economy Phase Design — Ore, Miners, Refineries

**Date:** 2026-07-11
**Status:** Approved (brainstorm sections 1–5 approved individually)
**Depends on:** Buildings & Construction phase (complete; production queue,
sidebar, placement, `--demo-base` gate hash=5273b5e71ca009e1)

## Goal

Close the money loop: ore fields from the real map, harvesters that collect
and unload it, refineries that convert it to credits, and the PROC
prerequisite unlocking the War Factory. After this phase: place a refinery →
a free miner spawns → credits flow → GAWEAP becomes buildable.

## Approved scope decisions

| Decision | Choice |
|---|---|
| Miners | **Both, real behavior** — Soviet HARV (War Miner) drives home; Allied CMIN (Chrono Miner) chrono-teleports home |
| Ore source | **Real map overlay** — parse `[OverlayPack]`/`[OverlayDataPack]` from the map already loaded (mapsmd03.mix entry 0x133a0cb2) |
| Ore growth/spread | **In scope** — full 5-minute timer, density growth, adjacent spread |
| Miner control | **Auto + manual** — auto-harvest loop plus right-click orders (harvest here / return / move-then-resume) |
| Unit production | **Out of scope** — GAWEAP unlocks but produces nothing; miners come only from refinery `FreeUnit` |
| Economy sinks | Unchanged — existing production queue spends/refunds from the same credit pool |

## Ground-truth constants already read from rulesmd.ini

- `[NAREFN]`: Cost=2000, Power=-50, Storage=200, Adjacent=2, `FreeUnit=HARV`, `NumberOfDocks=1`, Bib=yes, Prerequisite=POWER,NACNST
- `[GAREFN]`: same economics, `FreeUnit=CMIN`, Prerequisite=POWER,GACNST
- `[HARV]` (War Miner): Storage=40, Speed=4, Turret + 20mmRapid, `Dock=NAREFN,GAREFN`, `UnloadingClass=HORV`
- `[CMIN]` (Chrono Miner): Storage=20, no weapon, ChronoIn/OutSound=ChronoMinerTeleport, `UnloadingClass=CMON`
- `[Riparius]` (ore): Value=25, Growth=2200, GrowthPercentage=.06, Spread=2200, SpreadPercentage=.06
- `[Cruentus]` (gems): Value=50, no growth/spread
- `[General]`: GrowthRate=5 (minutes), TiberiumGrows=yes, TiberiumSpreads=yes, PurifierBonus=.25, HarvesterUnit=HARV,CMIN

## §1 Architecture & calibration authority

**Task 1 is a Ghidra hunt** (content-anchored only, gamemd.exe YR 1.001 at
D:\ra2-re, headless analyzeHeadless.bat, JAVA_HOME=C:\Program Files\Java\jdk-21)
producing `docs/re/economy.md` with confidence tables. It must pin:

1. **Harvester FSM** (HarvesterClass/TiberiumClass): ore-scan radius and
   cell-selection/tie-break rule; per-gulp pickup amount and tick interval;
   "full" threshold vs `Storage`; behavior when the current cell exhausts.
2. **Dock/unload**: queue/waiting behavior at `NumberOfDocks=1`; unload
   duration and credits-per-tick math (how load × `Value` (± PurifierBonus
   when a purifier exists — none this phase) drains into the credit pool);
   whether refinery `Storage=200` gates income at all in RA2.
3. **CMIN chrono-return**: trigger condition (full load), warp mechanics
   (out/in placement, timing, cooldown), warp back to the ore field after
   unload.
4. **Ore growth/spread**: what one 5-minute timer firing does; how
   `GrowthPercentage=.06` selects cells; density cap; spread adjacency and
   passability rules; RNG usage.
5. **Overlay tables**: overlay index ranges for ore/gems (hardcoded in the
   binary + `[OverlayTypes]` ordering); density data byte → SHP frame
   mapping; ore overlay SHP names in the theater chain and their draw layer.
6. **FreeUnit spawn**: where the free miner appears relative to the refinery
   foundation, and when (buildup completion).

Every constant in engine code cites this doc. **[RE-FALLBACK]**: anything the
hunt cannot pin becomes a plan-amendment gate (present options to the human;
amend the plan and commit BEFORE dependent dispatch) — never an
uncalibrated guess. Numeric constants recovered from the binary are
committable; GPL sources (XCC/OpenRA/YRpp at D:\ra2-re) are read off-repo,
prose conclusions only; YRpp/Ares hardcoded addresses untrusted —
content-anchored hunts only.

**Layering** (unchanged from prior phases):

```
ra2assets (pure decode, no SDL)      engine (pure sim, doctest)          main.cpp (SDL wiring)
──────────────────────────────      ──────────────────────────          ─────────────────────
MapFile += overlay accessors         engine/ore.hpp    OreField grid     overlay render pass
  [OverlayPack] Format80 decode        growth/spread timers              ore SHP loads (theater)
rules.cpp += tiberium/economy        engine/harvest.hpp Miner FSM        FreeUnit spawn on placement
  accessors                            scan→eat→return→dock→unload       input: harvest/return orders
                                       credits → existing credit pool    --demo-econ gate + hash
```

**Data flow:** map `[OverlayPack]` → `OreField` (per-cell type+density,
owned by `World`) → rendered under units, consumed by the miner FSM →
unload converts bales to credits in the same pool production spends from →
refinery alive ⇒ PROC token satisfied (already implemented in
`production.cpp`) ⇒ GAWEAP buildable.

## §2 ra2assets: overlay parsing + rules accessors

**MapFile overlay decoding.** `[OverlayPack]` = concatenated base64 →
LCW/Format80 (decoder already in `mappack.cpp`) → 262,144-byte array of
overlay type indices, addressed `cell = x + y*512`, `0xFF` = none.
`[OverlayDataPack]` decodes identically to the per-cell data byte (ore
density/frame). New accessors: `map.overlay_at(x, y)` and
`map.overlay_data_at(x, y)` → `int` (−1 when absent/none). Missing sections
are not an error (matches `[IsoMapPack5]` handling). Unit tests pin literal
values (known ore cells + densities) extracted once from the real map;
tests skip cleanly (MESSAGE + return) without `RA2_INSTALL_DIR`
(default `D:\Red Alert 2`).

**rules.cpp economy accessors** (same pattern as `cost()`/`power()`):
per-tiberium `value/growth/spread/growth_percentage/spread_percentage`;
per-type `storage()`, `harvester()`, `dock()`, `free_unit()`,
`number_of_docks()`; `[General]` `growth_rate`.

**Overlay-index → tiberium-type table**: recovered ranges land as
committable numeric constants in a small pure table (location per the hunt;
likely ra2assets).

**Ore overlay art**: theater-chain SHPs (exact names, frame layout pinned by
the hunt); drawn with the terrain palette; existing theater name resolver
handles extension munging.

## §3 engine/ore: the ore field

Pure module owning per-cell ore state.

- **State:** `OreField` grid (same dimensions/addressing as the occupancy
  grid) of `{type, density}`; type = none/ore/gems; density = data byte.
  Built once from `MapFile` overlay via the recovered index table. Non-ore
  overlays (walls, bridges) are ignored this phase — not in `OreField`,
  not rendered.
- **Consumption:** `take_bale(cx, cy)` decrements density; at zero the cell
  reverts to empty (render stops). Gulp semantics per the hunt.
- **Growth + spread:** free-running timer at the recovered rate
  (`GrowthRate=5` min converted at the recovered tick base). Per firing:
  growth increments density on eligible cells per the recovered
  `GrowthPercentage` selection; spread seeds new ore into adjacent empty,
  passable cells per `SpreadPercentage` and the recovered adjacency rule.
  Deterministic via the engine's existing seeded PRNG. [RE-FALLBACK] if the
  per-firing selection cannot be fully pinned.
- **Tests:** build from synthetic overlay array; deplete to zero; growth
  fires at exactly the recovered interval; spread only into passable
  empties; same seed → same field after N ticks.

## §4 engine/harvest: the miner state machine

Pure module, layered on existing Unit/pathfinding/occupancy like
`combat.cpp`.

**States:** `Idle → SeekOre → MoveToOre → Harvesting → ReturnToRefinery →
WaitForDock → Unloading → SeekOre`.

- **SeekOre:** best ore cell per the recovered expanding-radius scan +
  tie-break. Nothing found → Idle with periodic rescan.
- **Harvesting:** one gulp per recovered interval; each gulp =
  `take_bale` + load += recovered amount (cap = `Storage`: HARV 40,
  CMIN 20). Cell exhausted → adjacent ore or rescan. Full → return.
- **Return fork:** HARV paths to its `Dock=` refinery's dock cell.
  **CMIN chrono-teleports** per the recovered rule (warp to dock when
  full; warp back to the field after unload) — position-set + the
  ChronoIn/OutSound clips.
- **WaitForDock:** one miner unloads at a time (`NumberOfDocks=1`);
  waiting positions per the hunt.
- **Unloading:** credits drain into the shared pool at the recovered
  per-tick rate over the recovered unload duration. `UnloadingClass`
  (HORV/CMON) renders during unload if the hunt shows a straightforward
  image swap; otherwise [RE-FALLBACK].
- **Manual orders:** right-click ore cell → SeekOre targeting it;
  right-click own refinery → return/unload now; right-click ground →
  move, then auto-resume harvesting (original behavior); HARV attack
  orders still work via the existing combat path.

**Tests:** fill to exactly Storage; dock queue 2 miners/1 dock; credit
total after one full cycle = recovered formula; CMIN warps while HARV
drives; manual-order preemption; determinism.

## §5 Wiring, rendering, demo gate

- **Rendering:** ore overlay in the terrain pass (after tiles, before
  units/buildings; exact layer per the hunt), frame = density mapping,
  terrain palette. Miners are voxels via the existing Rhino pipeline:
  harv.vxl + harvtur.vxl (turreted), cmin.vxl (no turret), HVA-animated,
  house-remapped. HORV/CMON swap during unload per the §4 gate.
- **Refinery spawn:** on buildup completion, `FreeUnit` spawns at the
  recovered exit cell and enters SeekOre. The pre-built enemy Soviet base
  gains NAREFN + a working HARV (visible enemy economy).
- **Input:** §4 manual orders join the right-click resolution chain in
  `input.cpp` after the attack-building branch.
- **HUD:** nothing new — credits readout and power bar exist; credits tick
  up during unload. Starting credits stay `mp_money`; spend/refund
  unchanged.
- **Demo gate:** new `--demo-econ` deterministic scripted run (place
  NAREFN → free HARV spawns → harvests → unloads) with witness lines
  (credits after first unload = recovered formula, `GAWEAP unlocked
  (PROC met)`, pinned ore density after depletion + one growth firing)
  and a final state hash — locked as a permanent byte-identity gate.
- **Existing hard gates stay byte-identical:** `--demo-combat`
  (`shots=73 deaths=5 … logs=78`), `--demo-tank`
  (`shots=135 deaths=9 … logs=144`), `--demo-base`
  (`hash=5273b5e71ca009e1`).
- **Finalize:** full suite both modes (with/without RA2_INSTALL_DIR),
  arm64 NDK build (NDK D:\android-ndk-r27c), README updated with any
  adjudicated deviations.

## Constraints (project-wide, unchanged)

- Run executables from PowerShell, never Git Bash.
- No game assets, gamemd.exe, or Ghidra project committed; GPL sources
  read off-repo, prose conclusions only; recovered numeric constants are
  committable.
- Pure modules registered in `cmake/ra2_sources.cmake`.
- Repo on master, solo, no remote; commits end
  `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`.
- Scratch files to the session scratchpad.

## Testing summary

| Layer | Tests |
|---|---|
| ra2assets | overlay accessors vs literal pins from the real map; skip-clean without install |
| engine/ore | synthetic-field build, depletion, growth interval, spread passability, seed determinism |
| engine/harvest | Storage fill, dock queue, credit formula, CMIN warp vs HARV drive, manual preemption, determinism |
| integration | `--demo-econ` witness lines + state hash (new permanent gate) |
| regression | three existing demo gates byte-identical; full suite both modes; arm64 build |
