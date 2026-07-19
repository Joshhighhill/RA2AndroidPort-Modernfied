# Shroud / Fog-of-War — Design Spec

**Status:** DRAFT (awaiting user review)
**Depends on:** the Capture phase (`docs/superpowers/specs/2026-07-19-capture-design.md` — provides the neutral house id 2, the per-house-array widening to 3, and the currently-inert `InfiltratePurpose::Radar` this phase wires up) + the base World/render/combat engine (per-cell grids `passable_`/`water_passable_`, `iso_renderer` draw path, `combat.cpp` auto-acquire scan).
**Calibration authority:** `docs/re/shroud.md` (the Ghidra + `rulesmd.ini` recovery hunt — ALREADY COMPLETE; raw dumps in `D:\ra2-re\out\shroud_*`). This spec is built on recovered original-game behavior + the controller's binding design decisions (D1–D8), not design guesswork.

## Goal

Add RA2's shroud / fog-of-war visibility layer: the map begins black (SHROUDED); a house's own units and buildings reveal a near-circular radius of `Sight` cells around themselves; once terrain is seen it stays visible-but-dimmed (FOGGED) and — matching the shipped `FogOfWar=no` default — **never re-shrouds by tick**; cells currently inside a friendly source's sight are VISIBLE. On top of this the spec wires the two confirmed reveal-events recovered from the binary: owning an operational SpySat/GPS reveals the whole map, and a spy infiltrating an enemy **radar** re-shrouds that victim's entire map (the "radar sabotage" that resolves the item left INERT in the Capture phase, `docs/re/capture.md` §B6). The visibility model is generalized to a **per-house** layer (the original tracks only the local player; §1 CONFIRMED gate on `DAT_00a82cb4`), which is faithful for multi-client MP and required for the enemy-side targeting consult.

**Byte-identity promise.** Shroud is OFF by default. Every new field defaults to a value that makes it inert, is excluded from `world_hash`, and the new `step_visibility` World pass is firewalled behind `if (!shroud_any_) return;`. All 14 existing byte-identity gates stay bit-identical; the feature adds a 15th, `--demo-shroud`.

## Global Constraints

Copied verbatim from the controller's design direction. These bind every task.

- All 14 existing byte-identity gates MUST stay bit-identical: combat logs=78, tank logs=144, base 5273b5e71ca009e1, econ 7fed96cb98b68017, prod f0f4e1207ecd713c, ai-base cd1f7c6a29933f0a, ai-team 312e92ed862a5e74, ai-trigger 8334a6ca1531cabc, transport fe500f946e913645, aircraft a69642564cf64773, naval 6aee2c82bc024e86, superweapon 189496664f5eca68, techtree e73adae61ff6f71f, capture 58b87b5bc63f7046. Adds the 15th: `--demo-shroud`.
- `world_hash` (main.cpp FNV over Unit{cell_x,cell_y,hp,alive} + Building{type,cx,cy,hp,state}) MUST NOT be touched. New fields default INERT and are excluded from world_hash.
- New World::step pass `step_visibility` is firewalled `if (!shroud_any_) return;` — all 14 gates never enable shroud, so it's a strict no-op there.
- Any targeting change is an INERT extra clause (mirror the existing `|| units[j].disguised` clause at combat.cpp:362): default value makes the clause never trigger when shroud is off.
- Demo gate = two-run compute-and-self-compare, NO pinned literal in code (README pins the hash), wedge guards → exit 1.
- No assets/GPL/Ghidra raw output committed. Recovered CONSTANTS (Sight values, radii, cap 10) ARE committable.
- Commit trailer: `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>` (project git-history convention).

## Calibration summary (recovered ground truth — the behavior contract)

Every claim below is tagged as in `docs/re/shroud.md` ([CONFIRMED] = real rules/decompile; [ANCHORED] = OpenRA/documented RA2, structure only; [RE-FALLBACK] = design choice). The plan's Task 1 RE-GATE re-adjudicates the [RE-FALLBACK] items (see the last section) and re-pins the Sight values via `ra2extract` before code.

### Three-state visibility model (§1)
- State set is exactly **{SHROUDED, FOGGED, VISIBLE}**. [ANCHORED — OpenRA `ShroudCellType`; consistent with the CONFIRMED cell bit-pairs.]
  - **SHROUDED** — never seen; black.
  - **FOGGED / explored** — terrain seen once, drawn dimmed; mobile/enemy objects there are HIDDEN.
  - **VISIBLE** — currently inside a friendly source's sight this frame; full brightness, all objects shown.
- The binary stores this **per cell** in `CellClass` via **two independent bit-pairs** (§1 CONFIRMED offsets): `cell+0x12C` bits `0x18` = explored (mapped) and `cell+0x140` bits `0x03` = visible-this-frame; reset routine `FUN_005775e0` clears both, reveal routine `FUN_005778c0` sets both. "Explored" (fog) is NOT a superset of "Visible" — the two counters are resolved independently, which is what yields the three states. [offsets CONFIRMED; bit→state mapping ANCHORED]
- Both routines only touch cells when the target house equals the local/rendered player `DAT_00a82cb4` (early-out otherwise). RA2 keeps cell-level shroud for the **local player's viewpoint**; per-house reveal *events* are separate house flags (§7). [CONFIRMED]

### Sight table (§2) — real values, hard cap 10
- `Sight=` is a `TechnoTypeClass` field at offset `0x5E8`, read in whole **cells**. [CONFIRMED]
- **Hard cap = 10 cells.** `rulesmd.ini [General]` `VeteranSight` comment: *"going past ten is a hard code Vegas crash"*; `RevealTriggerRadius=9 ; … 10 is maximum`. Clamp all sight to 10. [CONFIRMED]
- Roster values (verbatim from the extracted `rulesmd.ini`, §2):

  | Source | Code | `Sight=` |
  |---|---|---|
  | GI | `E1` | 5 |
  | Conscript | `E2` | 5 |
  | Guardian GI | `GGI` | 6 |
  | Attack Dog | `DOG` / `ADOG` | 9 |
  | Grizzly Tank | `MTNK` | 8 |
  | Rhino Tank | `HTNK` | 8 |
  | IFV | `FV` | 8 |
  | Engineer | `ENGINEER` | 4 |
  | Spy | `SPY` | 9 |
  | Prism Tank | `SREF` | 8 |
  | Harvester | `HARV` | 4 |
  | MCV | `AMCV` / `SMCV` | 6 |
  | Construction Yard | `GACNST` / `NACNST` | 8 |
  | Power Plant | `GAPOWR` / `NAPOWR` | 4 |
  | Refinery | `GAREFN` / `NAREFN` | 6 |
  | Barracks (Allied / Soviet) | `GAPILE` / `NAHAND` | 5 / 6 |
  | War Factory | `GAWEAP` / `NAWEAP` | 4 |
  | Service Depot | `GADEPT` | 5 |
  | Radar (Allied AFC / Soviet Tower) | `GAAIRC` / `NARADR` | 5 / 10 |
  | Battle Lab | `GATECH` / `NATECH` | 6 |
  | Oil Derrick | `CAOILD` | 6 |
  | Gap Generator | `GAGAP` | 5 |
  | SpySat Uplink | `GASPYSAT` | 5 |
  | Psychic Sensor | `NAPSIS` | 10 |

- There is **no global default `Sight=`** in `[General]`; every combat type sets its own (all our roster types do). [CONFIRMED]

### Reveal algorithm (§3)
- Radius = `Sight` cells around each friendly unit/building. [CONFIRMED value/units]
- Distance metric = **near-circular**: enumerate the diamond of candidate cells (`FindTilesInAnnulus`) then keep a cell only if its center is within Euclidean `Sight` cells (`HorizontalLengthSquared <= sight²`). The result (radius = Sight, near-circular) is what must be faithful; the exact in-engine rasterization is not address-pinned. [ANCHORED]
- **Buildings reveal from each occupied footprint cell**, not a single center point. [ANCHORED]
- Recomputed on add-to-world, on move, and when range changes each tick. [ANCHORED]
- **Initial reveal at game start:** each house's starting objects reveal around themselves; the rest of the map is SHROUDED. [ANCHORED]

### Fog does NOT regrow (§4)
- Shipped defaults: `FogOfWar=no`, `ShroudGrow=no`, `ShroudRate=4`, `FogRate=.01`. [CONFIRMED]
- `ShroudGrow=no` ⇒ once explored, terrain stays revealed (dimmed) permanently; the shroud never creeps back. `FogOfWar=no` ⇒ the regrowing fog that re-hides mobile objects in explored areas is off. Only the black shroud over unexplored terrain is always active. [CONFIRMED]
- GapGenerator (`[GAGAP]` `GapGenerator=yes`, radius 10) is a shroud *source* that re-covers enemies' explored terrain; per-cell "generated shroud" count mechanism. [flags CONFIRMED / mechanism ANCHORED] — **carry-forward, not this phase.**

### Spy / radar reveal semantics (§7) — resolves the Capture-phase INERT item
- **A. Own operational SpySat/GPS → reveal whole map.** `FUN_00508d80(house)` scans the house's buildings; if a powered building whose type has the SpySat/GPS flag (`Type+0x16A5`) exists it calls `FUN_005778c0` (clear shroud = reveal entire map) and sets latch `house+0x577A=1`; losing that source calls `FUN_005775e0` (re-shroud whole map). Allied `GASPYSAT` (`SpySat=yes`). [CONFIRMED]
- **B. Spy infiltrating enemy Radar → re-shroud the victim.** In infiltrate dispatch `FUN_004571e0`, when the infiltrated type is a radar (`Type+0x16A4 != 0`) it calls `FUN_0050bb30(victim)` → `if (victim+0x577A==0) FUN_005775e0(victim)` = reset the radar owner's entire map to shrouded, unless that house has an active SpySat/GPS latch. The classic radar-sabotage: victim loses their explored map and must re-scout. [CONFIRMED]

## Current-engine reality (what we build on)

- **Per-cell grids** already exist: `passable_` and the opt-in `water_passable_` (world.hpp:1035), both sized `grid_w_*grid_h_` (world.hpp:1038-1039), indexed `y*grid_w_+x`. `water_passable_` is EMPTY until naval opts in (`set_water_terrain`) — the exact precedent for our grids. Built in `build_passability` (world.cpp:208-230).
- **Per-house [3] arrays** are the established shape: `house_credits_[3]` (world.hpp:1058, accessor range-checks `house>=0 && <3`), `blackout_frames_[3]` (world.hpp:992), `produced_veteran_[3]` (world.hpp:998). Houses: 0=player, 1=enemy, 2=neutral (`kNeutralHouse` — no combat/AI, left inert). The Capture phase already widened these to 3.
- **Firewall precedent:** `capture_any_` (world.hpp:962, opened at world.cpp:1270), `cargo_any_`, `aircraft_any_`, `naval_any_`, `sw_any_`, `ai_any_`; no-op body pattern e.g. `step_capture` (world.cpp:1311). `World::step` pass order at world.cpp:2479-2760.
- **`world_hash`** (main.cpp:4224-4240, `fnv_mix` 4216) folds ONLY `Unit{cell_x,cell_y,hp,alive}` + `Building{type,cx,cy,hp,state}`. Inert-field doc pattern on Unit at world.hpp:160-161,189-190,208-210,250.
- **Render:** `render_world` (main.cpp:2065-2166); `draw_terrain` painter loop (iso_renderer.cpp:97-201, `cell_to_screen`+cam 128-134, off-screen cull 136-138); `draw_one_unit` (main.cpp:2094-2112 → `draw_unit` iso_renderer.cpp:203); `shade_rect(ren,rect,alpha,r,g,b)` translucent fill (main.cpp:2551-2557, already used for sidebar dimming). Radar box is a **static logo blit** ("sb:radar" into `lay.radar`, main.cpp:2607-2610, sidebar.hpp:53, 168x110) — no minimap exists (greenfield).
- **Combat:** `valid_target` (combat.cpp:59-69), auto-acquire scan (combat.cpp:334-378) with the skip clause at line 362 `|| units[j].disguised`; return-fire guard in `try_fire` (combat.cpp:100). `step_combat(units,buildings,occ,dt)` (world.cpp:2536) reads per-Unit fields.
- **Sight:** NO sight field anywhere today. `UnitCombat` (unit_combat.hpp:24-89), copied to `Unit` in `World::add_combat_unit` (world.hpp:527); `BuildingTypeDef` (building.hpp:16-50); rules accessors in `include/ra2assets/rules.hpp`.

## Architecture

Each component states what it does, its interface, its dependencies, and — critically — HOW it stays inert / byte-identical when shroud is off.

### A. Per-house visibility layer (D1)
Two independent per-house grids on `World`, each sized `grid_w_*grid_h_`, indexed `y*grid_w_+x` (same index as `passable_`):
- `std::vector<std::uint8_t> explored_[3];` — sticky "terrain seen once" (FOG). Set on reveal; **NEVER cleared by tick** (fog-no-regrow §4). Cleared only by `reshroud_house` (spy-radar sabotage §7B).
- `std::vector<std::uint8_t> visible_[3];` — "currently in sight this frame". Recomputed every visibility tick (cleared, then re-set from live sources).

Three states derive per cell per house: `!explored` → SHROUDED; `explored && !visible` → FOGGED; `visible` → VISIBLE. House index 0=player, 1=enemy, 2=neutral (leave inert). **Inert rule:** grids are EMPTY (size 0) until `shroud_any_` opens and allocates them; accessors on an empty grid return "all visible / all explored" so every consult is a no-op with shroud off. Depends on: `grid_w_`/`grid_h_`, and the Capture phase's 3-wide per-house array convention.

### B. Firewalled `step_visibility` pass (D3)
A new `World::step_visibility()` inserted **after `step_units`** (movement done) and **before `step_combat`** (combat reads visibility), in the world.cpp:2479-2760 pass order. Body opens with `if (!shroud_any_) return;` (mirrors `step_capture` world.cpp:1311). Per tick: for each house h in 0..2, clear `visible_[h]`; for each ALIVE unit/building of house h, reveal cells within `sight` (algorithm C); setting a cell visible also sets `explored_[h]` (sticky); then stamp each unit's `seen_by_mask` (component E). **Inert rule:** the firewall returns immediately for all 14 gates (none enable shroud), so the pass never runs and never touches world state there.

### C. Reveal algorithm (D3)
A pure helper `reveal_source(house, cx, cy, sight)` (near-circular): enumerate the diamond of candidate cells around `(cx,cy)`, keep a cell `(x,y)` iff `(x-cx)² + (y-cy)² <= sight²`, and for each kept in-bounds cell set `visible_[house]` and `explored_[house]` (§3 ANCHORED). Buildings call it **once per occupied footprint cell** (§3). Sight is clamped to 10 at data-load (D2). Fog does NOT regrow: `explored_` is never cleared here (§4). Depends on: the grids (A), the Sight field (D). Lives in a pure module (e.g. `engine/visibility.{hpp,cpp}`) tested host-side; the mutating entry is the `World` pass. Inert only via the firewall (never invoked with shroud off).

### D. Sight data (D2)
`int sight = 0;` added to `UnitCombat` (unit_combat.hpp) and mirrored on `Unit` (world.hpp), copied in `World::add_combat_unit`; `int sight = 0;` added to `BuildingTypeDef` (building.hpp). Populated from rules via a new `rules.sight(id)` accessor reading `Sight=` and hard-clamping to 10 (the "Vegas crash" cap). **Inert rule:** default `0` reveals only the source's own cell (or nothing), and is meaningless while the grids are empty; excluded from `world_hash` (a plain stat like `strength`/`weapon_range`, which `world_hash` already ignores).

### E. `seen_by_mask` inert targeting clause (D4)
`std::uint8_t seen_by_mask = 0xFF;` added to `Unit` (DEFAULT `0xFF` = "seen by all houses" = INERT; excluded from `world_hash`, doc-tagged per world.hpp:160-161). `step_visibility` overwrites it: bit h set iff the unit's cell is `visible_` to house h. The auto-acquire scan (combat.cpp:362) gains ONE extra clause mirroring `|| units[j].disguised`:
```
|| !(units[j].seen_by_mask & (1u << u.house))
```
**Inert rule:** with the default `0xFF`, `(0xFF & (1<<u.house))` is always non-zero so `!(...)` is always false → the clause never triggers → combat=78 and tank=144 stay byte-identical. `step_visibility` (behind the firewall) is the only writer, so with shroud off the mask never changes from `0xFF`. `valid_target` is NOT gated (explicit orders on known units stay allowed, §6, matching the disguise precedent); optionally the same guard may be added to the return-fire path (`try_fire` combat.cpp:100) with the same default-inert property.

### F. Rendering overlay (D5) — local player = house 0
An overlay pass in the render path (main.cpp `render_world` / the `draw_terrain` loop): for each drawn cell, consult house 0's grids — SHROUDED → paint black; FOGGED (`explored && !visible`) → draw terrain then dim it with a translucent-black `shade_rect`-style fill; VISIBLE → full brightness. Additionally suppress mobile/enemy objects in non-visible cells (skip `draw_one_unit` when the unit's cell is not visible to house 0). Reuses `cell_to_screen`+cam projection and the existing off-screen cull. **Inert rule:** with shroud off the grids are empty → accessor returns all-visible → no black, no dim, no suppression → pixel-identical render (and render is not part of any gate hash anyway). Dim factor is [RE-FALLBACK] — pick a blend approximating `BlendedFog`.

### G. Radar minimap (D6) — greenfield
A new `draw_radar_minimap` into `lay.radar` (168x110, main.cpp:2607-2610 replaces the static "sb:radar" logo when a radar is owned): iterate the grid, paint only cells EXPLORED by house 0 (terrain color via `pal_for_house`/terrain palette), overlay unit blips only for cells VISIBLE to house 0, gated on house 0 owning an operational radar building (`Radar=yes`); a SpySat/GPS reveal shows the whole minimap. **Inert rule:** render-only, no gate impact; with shroud off it retains today's static-logo behavior (or draws nothing new).

### H. Spy / radar reveal wiring (D7) — resolves the Capture-phase INERT item
Two `World` helpers plus wiring:
- `reshroud_house(h)` — clear both `explored_[h]` and `visible_[h]` (fully re-shrouded). **Wire** the Capture phase's currently-inert `InfiltratePurpose::Radar` (spy infiltrates enemy radar, dispatched from `infiltrate_building` / `step_capture` arrival) → `reshroud_house(victim)` UNLESS the victim is SpySat/GPS-protected (§7B latch).
- `reveal_all_house(h)` — set `explored_[h]` fully (and `visible_` while the source persists). Driven from house h owning a building whose type has `SpySat=yes` (Allied `GASPYSAT`, §7A). If `GASPYSAT` is absent from the roster, implement the mechanism (a `BuildingTypeDef` flag `grants_map_reveal`) and leave it inert until such a building exists.

**Inert rule:** both helpers only run under `step_visibility`/the wired spy path, all behind `shroud_any_`; with shroud off the Radar purpose stays the inert no-op it is today. GapGenerator (shroud SOURCE) is a carry-forward (per-cell generated-shroud count) — noted, not implemented.

### I. Enablement (D8)
`World::enable_shroud(bool)` (or first reveal) sets `shroud_any_` and allocates the grids (`explored_[h]`/`visible_[h]` → `grid_w_*grid_h_`, zero-filled). Default OFF → every existing gate inert. A "reveal map / no-shroud" option = `reveal_all_house` at start (§8). Initial reveal at game start = each house's starting objects reveal around themselves (§3, the add-to-world reveal, naturally produced by the first `step_visibility`). FogOfWar-regrow is NOT implemented (matches shipped `FogOfWar=no`) — carry-forward.

## Data model (exact field additions)

| Field | On | Default | In `world_hash`? | Why inert by default |
|---|---|---|---|---|
| `int sight` | `UnitCombat` (unit_combat.hpp) | `0` | No | A plain stat; `world_hash` folds only cell/hp/alive. Sight 0 reveals ~nothing. |
| `int sight` | `Unit` (world.hpp) — mirror | `0` | No | Copied in `add_combat_unit`; same reasoning. |
| `int sight` | `BuildingTypeDef` (building.hpp) | `0` | No | Type-level stat; not hashed. |
| `std::uint8_t seen_by_mask` | `Unit` (world.hpp) | `0xFF` | No | `0xFF` = seen by all → the combat.cpp:362 clause never fires. Doc-tag per world.hpp:160-161. |
| `std::vector<std::uint8_t> explored_[3]` | `World` | empty (size 0) | No | Empty grid → accessor returns all-explored → no consult effect. |
| `std::vector<std::uint8_t> visible_[3]` | `World` | empty (size 0) | No | Empty grid → accessor returns all-visible. |
| `bool shroud_any_` | `World` | `false` | No | The firewall flag; false → `step_visibility` returns immediately. |

Sight values are populated at data-load from `rules.sight(id)` using the §2 table (clamped to 10). All new state is excluded from `world_hash` because `world_hash` folds only `Unit{cell_x,cell_y,hp,alive}` + `Building{type,cx,cy,hp,state}` (main.cpp:4224-4240) — untouched.

## Behavior details (each tied to the recovered §)

- **Reveal metric** (§3, D3): near-circular — diamond of candidate cells filtered by Euclidean `(dx²+dy²) <= sight²`; buildings reveal from each footprint cell; sight clamped to 10 (§2 cap).
- **Initial reveal** (§3/§8, D8): each house's starting objects reveal around themselves at game start; the rest is SHROUDED. Produced by the first `step_visibility` tick over add-to-world objects.
- **No fog regrow** (§4, D1/D3): `explored_` is set on reveal and never cleared by the tick; only `visible_` is recomputed each tick. Matches shipped `ShroudGrow=no`/`FogOfWar=no`.
- **Spy-radar sabotage** (§7B, D7): spy infiltrates an enemy **radar** → `reshroud_house(victim)` (clears victim's explored+visible), unless the victim holds an active SpySat/GPS reveal. Wires the Capture phase's inert `InfiltratePurpose::Radar`.
- **SpySat reveal-all** (§7A, D7): a house owning an operational `SpySat=yes` building → `reveal_all_house`; losing it re-shrouds (matches `FUN_00508d80` latch semantics). Mechanism implemented even if `GASPYSAT` is absent from roster (flag `grants_map_reveal`, inert until present).
- **Targeting skip** (§6, D4): the auto-acquire scan rejects candidates whose cell is not VISIBLE to the scanning house, via the inert `seen_by_mask` clause. Explicit orders and `valid_target` are NOT gated (§6).
- **Rendering consultation** (§5, D5): SHROUDED → black; FOGGED → dimmed terrain, mobile/enemy objects suppressed; VISIBLE → full. Dim factor approximates `BlendedFog` [RE-FALLBACK].

## Scope / non-goals (carry-forwards)

Explicitly OUT of scope this phase (documented, not implemented):

| Carry-forward | Recovered ref | Why deferred |
|---|---|---|
| **GapGenerator** (shroud SOURCE re-shrouding enemies) | §4 `[GAGAP]` radius 10 | Needs a per-cell "generated-shroud count" a viewer must out-reveal; more complex than a boolean grid. |
| **FogOfWar regrow** (re-fogging explored areas over time) | §4 `FogOfWar=no`/`FogRate` | Off in shipped defaults; not needed for calibration. |
| **Height sight bonus** (+1 sight per `LeptonsPerSightIncrease`=2000 leptons) | §3 | No unit elevation model yet. |
| **Cloak / `SensorsSight` detection** (reveal cloaked/subterranean) | §2/§6 `SensorsSight=15` | Separate detection layer; no cloak model yet. |
| **AI use of the reveal layer** (AI scouting / fog-aware targeting) | §6 | AI targets from full knowledge for now; only enemy-side auto-acquire consult lands. |
| **AllyReveal map-sharing** (allies share revealed map) | §8 `AllyReveal=yes` | No alliance model beyond 3 fixed houses. |

## Testing strategy

- **Pure host tests** (doctests on the `engine/visibility.*` module + `rules.sight`), no install required:
  - `rules.sight(id)` returns the §2 table values and clamps to 10.
  - `reveal_source` near-circular footprint: a source with sight `s` at `(cx,cy)` marks exactly the cells with `dx²+dy² <= s²` (spot-check radii 4/5/8/9/10 against expected cell counts); out-of-bounds cells skipped.
  - Building footprint reveal covers the union of per-cell reveals.
  - `explored_` sticky across ticks (fog no-regrow): a cell revealed on tick N stays explored after the source leaves; `visible_` clears.
  - `reshroud_house` clears both grids; `reveal_all_house` sets `explored_` fully.
  - `seen_by_mask` stamping: bit h set iff cell visible to h; default `0xFF`.
- **New 15th gate `--demo-shroud`** (mirror `run_demo_capture` main.cpp:7762-7809): two-run compute-and-self-compare (run1 rendered+PNG, run2 headless fresh `World`), `identical=(witness && hash)`, **NO pinned literal in code** (README pins the hash), wedge guards → exit 1. Sequence exercises: initial reveal around a starting unit; a unit moving to reveal new cells (explored grows); fog stickiness (source leaves, explored persists, visible clears); an enemy unit becoming untargetable when it steps into the scanning house's shroud (via `seen_by_mask`); spy→enemy-radar `reshroud_house` clears the victim's map; a SpySat `reveal_all_house`. **Wedge guards** (→ exit 1): initial reveal must mark >0 cells explored; movement must grow explored; fog must stay explored after the source leaves; the spy-radar sabotage must clear the victim's explored count to 0; reveal-all must set explored to full. Hash folds the explored/visible cell counts per house + a `seen_by_mask` sample at fixed checkpoints.
- **The 14 existing gates stay byte-identical** — re-verify at the first visibility-layer commit (T3), the targeting-clause commit (T4), and finalize (T9), **one gate per call** (each AI gate ≈2 min; batching exceeds the tool timeout — see the gate-runtime discipline). Confirms `shroud_any_=false` keeps `step_visibility`, the render overlay, the targeting clause, and all new fields fully inert.

## [RE-FALLBACK] items for Task 1 RE-GATE to adjudicate

Task 1 is a **confirmatory RE-GATE adjudication** (re-pin values via `ra2extract` + targeted decompile; flip `[RE-FALLBACK]`→adjudicated), NOT a fresh hunt. The items to close:

1. **Fog dim factor** (§5, D5): the exact blend/darkening applied to FOGGED terrain — pick a translucent-black `shade_rect` alpha approximating `BlendedFog=yes` (blend rather than dither). Currently [RE-FALLBACK]; adjudicate a concrete alpha (or confirm the blend is purely cosmetic and any reasonable value is acceptable since render is unhashed).
2. **`GASPYSAT` roster presence** (§7A, D7): confirm via `ra2extract` whether the `GASPYSAT` (`SpySat=yes`) building is in our roster/art. If present, drive `reveal_all_house` from it directly; if absent, the mechanism ships inert behind the `grants_map_reveal` flag until such a building exists.
3. **Exact reveal rasterization** (§3, ANCHORED/[RE-FALLBACK]): the in-engine near-circular footprint is not address-pinned. Confirm the Euclidean-filtered-diamond (`dx²+dy² <= sight²`) reproduces RA2's per-source reveal footprint closely enough; adjudicate the boundary/rounding convention (center-in-range) so cell counts are deterministic and stable for the gate.
