# ra2assets + ra2engine

> **Players:** read [`docs/INSTALL.md`](docs/INSTALL.md). It covers the game
> files you need, desktop and Android install, mouse and touch controls, and
> troubleshooting. Version: `VERSION` (`ra2engine --version`).
>
> **Legal.** This repository contains an engine only (license: [`LICENSE`](LICENSE);
> third-party code: [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)). It does
> not include and does not distribute any Command & Conquer: Red Alert 2 /
> Yuri's Revenge game data. The engine runs only on game files that the player
> supplies from their own legally obtained copy. "Command & Conquer" and
> "Red Alert" are trademarks of Electronic Arts Inc. This project is not
> affiliated with, endorsed by or sponsored by Electronic Arts. The art,
> logos, sounds and text shown at run time come from the player's own files.

A portable C++17 library that decodes Red Alert 2 / Yuri's Revenge game data
(`ra2assets`: MIX archives, PAL palettes, SHP sprites, VXL/HVA voxel models
(vehicle meshes + animation transforms, decode only), LZO/LCW map packs, INI,
rules, `.map` cell grids, TMP theater tiles, CSF string tables, BAG/IDX audio
decoded from Westwood IMA-ADPCM to PCM), plus `ra2engine` — an SDL2
front-end that renders a real stock map with real theater terrain and lets you
select and command a squad of units (the ORIGINAL controls, recovered from
`gamemd.exe`: left-click / box / Shift to select, left-click to order, right-click to deselect or cancel -- see "Real-play controls") with A\* pathfinding,
grid-occupancy collision avoidance, and walk animation — and fight: Allied
GIs vs Soviet Conscripts with house-colored palettes, HP bars, fire/death
animations, and a damage model (Damage × Verses, ROF, range, guard behavior)
recovered from the original `gamemd.exe` via Ghidra and driven by your real
`rulesmd.ini` (see `docs/re/combat.md`). Standing/walking/firing/dying frames
and all 8 facings are resolved from your real `artmd.ini` sequence data
(`ra2assets::Art`, see `docs/re/infantry-anim.md`), and group attack orders
fan attackers out to individual firing positions instead of queuing them
single-file. The fight is audible, too: weapon-fire sound effects, unit voice
acknowledgements (select/move/attack barks) for every unit, death screams,
impact/explosion sounds, in-game music and the full EVA (construction
complete, unit ready, low power, base under attack, insufficient funds, ...
-- see "Presentation in real play"), all driven by your real `soundmd.ini`/`evamd.ini`
event definitions and the `audio.bag`/`audio.idx` sample archive decoded by
the in-house IMA-ADPCM decoder (no XCC/third-party tooling) plus a from-scratch
WAV reader for the EVA clips. Playback semantics — linear distance
attenuation over the isometric screen-pixel Chebyshev metric, per-event
simultaneous-instance limits (approximated by reject-at-trigger; only
`Control=interrupt` steals a busy voice), uniform `Control=random` variant
selection, and the 5-level Priority scale — were recovered from
`gamemd.exe` via Ghidra (see `docs/re/audio-playback.md`, which also
records the deliberate divergences) and are mixed by a small dependency-free
`ra2engine::Mixer`, played out through SDL2 on desktop/Android. The library
stays pure and dependency-free; only the engine links SDL2.

Vehicles fight too, rendered as real voxels instead of sprites: the Rhino
tank's hull, turret, and barrel sections are decoded from your real VXL/HVA
meshes (`ra2assets::Vxl`/`Hva`) and rasterized by a from-scratch software
voxel rasterizer (`ra2engine::rasterize_model`, see
`docs/re/voxel-render.md`) into 32 pre-rendered facing sprites per section,
cached once at load (`build_facing_set`) and blitted per frame — the same
"cached facings" strategy the original uses — lit by the recovered normal
tables and diffuse/specular lighting against the unit's own `voxels.vpl`
(`ra2assets::Vpl`) and remapped through the house-colored unit palette.
Locomotion follows the recovered Drive model: hull and turret both turn in
place at the type's `ROT` degrees/frame with shortest-arc, exact-landing
`FacingClass` semantics (`step_turrets`), and each vehicle drives at its own
`Speed`-derived rate. Documented divergences from vanilla (see
`docs/re/voxel-render.md` §5, §7): no slope tilt (facings are cached flat,
so a vehicle on a ramp still renders level); no debris (crush: see Gap fix C);
projectiles are instant-hit with single-target damage — vanilla shells fly
102 leptons/frame (not modeled; travel time is skipped) but `[AP]
CellSpread=.3` only ever reaches 76 leptons, in practice just the direct
target anyway, so the single-target damage half of this is exact even
though the flight-time half is a real simplification; the engine keeps its
**[superseded by gap fix C, G-42]** recovered absolute speed scale
`trunc(Speed·2.56)·GameSpeedBias(1.6)·15/256` cells/sec (docs/re/gamespeed.md;
it was a feel-tuned `0.75·Speed`); the
turret's ROF+5-frame idle-return hold is omitted (it re-centers
immediately); and `MoveSound` (engine/track sound effects) is deferred.

Buildings round out the base game: an 8-building Allied+Soviet roster
(Construction Yard, Power Plant, Barracks-class infantry producer, and War
Factory, both houses — `GACNST/GAPOWR/GAPILE/GAWEAP` and
`NACNST/NAPOWR/NAHAND/NAWEAP`) renders from the same real SHP art pipeline as
units, resolved via the recovered
`NewTheater` letter-substitution + `'G'`-fallback rule (a `name[1]` munge
tested against `{g,n,c,y}×{a,t}`, then a hardcoded `'G'` retry on miss,
searched across every loaded mix, no per-art scoping — see
`docs/re/building-render.md` §1); multi-cell foundations occupy their full
W×H cell rectangle and block pathfinding/placement like any other obstacle.
The whole Construction Yard loop works end to end: a real RA2 sidebar
(credits readout, power bar, four tab buttons, and the cameo build-progress
clock drawn with the game's own `gclock2.shp` — one frame per production
step, 55 frames = idle + 54 steps, instead of a fill bar) drives a
54-step `FactoryClass`-faithful build economy at
`frames = trunc(Cost × BuildSpeed × 0.9)` (15 Hz, e.g. 504 frames for an
800-cost Power Plant), a placement cursor validated by the recovered
`Adjacent+1` expanded-rect proximity rule, buildup animations paced at
`BuildupTime × 900` ticks (rate = frames/image_frames, played once), and a
low-power build penalty clamped to `clamp(1 − modifier×(1 − output/drain),
.5, .8)`. Attack-capable buildings fight too, with turret-aim gating shared
with vehicles. A pre-built Soviet base exercises the Soviet roster and the
non-local-player placement-check bypass the original itself uses for
scripted placement, and `--demo-base` is a deterministic headless gate over
the whole loop. Calibration authorities:
`docs/re/building-render.md` (art/foundation/draw-order/damage-state/buildup/
palette semantics) and `docs/re/construction.md` (build-time math, power
model, placement validity, queue rules, sidebar metrics).

Documented divergences from vanilla: no idle `ActiveAnims`/production
animations; repair and sell are not implemented (their sidebar buttons are
inert); no rubble frames; no shroud; the radar is a static frame; sidebar
tabs beyond the Building tab are inert; the credits tally uses a built-in
digit strip rather than the game's own (unproven-for-this-string) font path;
the READY cameo state shows a flashing green tint (no in-game font this phase;
the divergent credits digit strip covers 0–9 only);
`GAWEAP` (Soviet War Factory) is unbuildable by the player this phase — its
`PROC` prerequisite is correctly enforced and unmet, exercised instead via
the pre-built Soviet base; units auto-acquire enemy ARMED structures (base
defenses) but plain buildings only on explicit attack orders (gap fix C, G-46). Plus, from the construction-phase
RE-gate: power output does **not** scale with building health (vanilla
scales output by hp ratio — `construction.md` §8 optional-fidelity item,
omitted here); `Bib=yes` bib art/row is omitted (`GAWEAP` renders fine
without its bib — `building-render.md` §2); tab buttons are drawn frame-0 +
a shade overlay rather than their full per-state frame set (the §11
`[RE-FALLBACK]`, since the frame-index-to-state mapping beyond frame 0 was
never fully pinned); and the main SHP's second damaged frame (frame 2, a
distinct debris/lighting variant) is unused, since it's only reachable
through the anim-stage machinery a static building never exercises.

The Economy phase closes the loop from raw terrain to spendable credits. Ore
and gems are decoded straight off the real map's `[OverlayPack]`/
`[OverlayDataPack]` (no synthetic placement) and classified into an
`ra2engine::OreField` by the recovered range-based overlay-index rule — an
overlay index belongs to a tiberium type when it falls in that type's
`[base, base+NumImages=12)` flat-image range or the following `NumSlopes=8`
slope range (`docs/re/economy.md` §5); the runtime index is the 0-based
order-of-appearance position in `[OverlayTypes]` (bases 27/102/127/147 for
GEM01/TIB01/TIB2_01/TIB3_01 on the real stock map), density is the map's
per-cell byte, 0..11. `HARV`/`CMIN` miners run the recovered harvester FSM
(`ra2engine::step_harvest`, `engine/harvest.cpp`): an 8-neighbour dig banks
bale value into cargo until full — the engine models a **2-kind** store
(`Unit::bales_ore`/`bales_gems`), collapsing the original binary's
**4-slot** per-tiberium-type store (one slot each for GEM/TIB/TIB2/TIB3,
`docs/re/economy.md` §1) into a single ore bucket — then the
miner drives (`HARV`) or Chrono-teleports (`CMIN`, a `TeleportLocomotionClass`
analogue bracketed by `ChronoInSound`/`ChronoOutSound`) to the refinery's
single dock, where a chunked per-tick payout transfers `Σ bales × Value` into
house credits. Refineries also spawn their `FreeUnit` (HARV/CMIN) at
grand-opening with its cost pre-billed into the refinery's own price
(`charge = Cost − FreeUnit.Cost`, e.g. GAREFN 2000 − 1400 = 600, while its
1260-frame build time still derives from the raw, un-subtracted 2000 —
`economy.md` §6a), and once built the refinery satisfies the `PROC`
prerequisite group, unlocking `GAWEAP`/`NAWEAP` (the War Factory, correctly
gated off in the Buildings phase). Ore itself is alive: a
`GrowthRate`-cadenced timer raises density toward the cap (11) on a slice of
eligible cells and spreads new tiberium onto adjacent empty, passable cells
from fully-grown sources. Calibration authority: `docs/re/economy.md`
(harvester FSM, dock/unload math, chrono locomotion, growth/spread, overlay
tables, FreeUnit cost split); companion of `docs/re/construction.md`.

Documented divergences from vanilla (Economy phase): ore growth/spread
candidate selection uses an engine-local **seeded xorshift32** RNG in place
of the original's lockstep-synchronised `Randomizer` — same rates
(`GrowthRate`, `Growth`/`Spread`, `*Percentage`, cap 11), not bit-identical
(the original's netgame-synchronised sequence is unreachable single-machine);
the `UnloadingClass` art swap (e.g. `HARV`→`HORV`, `CMIN`→`CMON`) while docked
is not implemented — miners keep their normal body during unload; the
refinery dock cell is adjudicated as the foundation's south-center cell
(`dock_cell_x`/`dock_cell_y`, `engine/harvest.hpp`) since GAREFN/NAREFN define
no `DockingOffset` in artmd — **[RE-FALLBACK]**; the per-gulp/unload/
chrono-warp/ore-scan pacing constants (`kGulpSecs`, `kUnloadSecsPerBale`,
`kChronoWarpSecs`, `kOreScanRadius`) are adjudicated, not recovered —
**[RE-FALLBACK]**; classification (`ore.cpp`'s `kBases`) models all four
overlay-index bases (27 GEM01, 102 TIB01, 127 TIB2_01, 147 TIB3_01 — all
`OreKind::Ore` except GEM), and the real stock map DOES reference index 127
(`economy.md` §5's own Task 2 dump observed it directly) — but only the
generic `TIB01..TIB20`/`GEM01..GEM12` art files are loaded (`load_ore_art`,
`engine/main.cpp`), so TIB2/TIB3 cells render through the shared
`TIB01..TIB20` set (whichever slot their in-block offset selects) instead of
dedicated `TIB2_*`/`TIB3_*` art, which is never loaded; `--demo-base`'s
roster witness now reports `roster=10` (GAREFN/NAREFN
appended to the buildable roster) — an intended content change, the gated
`hash=5273b5e71ca009e1` is unchanged; the ConYard `BaseUnit` cost-subtraction
rule recovered in `economy.md` §6a (Cost −= avg MCV cost when the type is
`BaseUnit[0].DeploysInto`) is not implemented, since Construction Yards are
never player-purchasable in this engine; and ore renders flat-cells-only —
the 8 `NumSlopes` slope frames per tiberium type are unused (no sloped-tile
ore rendering).

The Unit Production phase brings the sidebar's other three tabs to life
alongside Buildings (tab0). The player's Construction Yard now drives **four
concurrent production queues** (`ra2engine::ProductionQueues`), one per
sidebar tab: Structures (tab0), Defenses (tab1), Infantry (tab2), Vehicles
(tab3, sharing the slot with Ships/Aircraft, out of scope here). Structures
and Defenses are **separate** `FactoryClass` queues keyed on `BuildCat==Combat`
(not `IsBaseDefense` — GAWALL/GTGCAN are `BuildCat=Combat` but
`IsBaseDefense=None`, so the classifier can't be the flag), so a power plant
and a pillbox build **at the same time** from the same ConYard
(`docs/re/production.md` §1.1). The Allied defense roster (tab1) is GAPILL
(pillbox); the Allied unit roster (tab3/tab2) is GI (`E1`), Guardian GI
(`GGI`), Grizzly tank (`MTNK`), and IFV (`FV`) — whichever the loaded
`rulesmd.ini`/art resolve. Each unit queue reuses the identical 54-step
`FactoryClass` charge economy as buildings (`build_frames = trunc(Cost ×
BuildSpeed × 0.9)`), with **no FreeUnit subtraction for units** (charge ==
Cost, unlike refineries — `production.md` §1.5), and is gated by the matching
producer building (Barracks for infantry, War Factory for vehicles) via the
same prerequisite AND-mechanism buildings use.

A finished unit is emitted by the queue's **primary factory** — the first-built
building of that category by default, switchable with a second left-click on
the already-selected factory (right-click is reserved for setting rally, see
Deviations below). A second factory of the same category speeds up its class's
build time via the recovered **MultipleFactory = 0.8** compounding (one extra
same-class factory → ×0.8 build time; buildings share ONE ConYard count slot
across Structures+Defenses for this multiplier, `production.md` §1.1/§1.2).
On completion the unit is placed at its **exit cell** — an 8-ring first-free
walk outward from the factory's foundation anchor (`ra2engine::exit_cell`,
`production.md` §1.3) — then given a Move order to its **rally point**: the
cell set by the player, or by default the exit-adjacent gather cell nearest
the building's own coordinate (`FUN_00500020`, `production.md` §1.4).
Calibration authority: `docs/re/production.md` (queue model, primary +
MultipleFactory, exit cell, rally); companion of `docs/re/construction.md`
and `docs/re/economy.md`.

Deviations we take (adjudicated divergences from the recovered `production.md`
ground truth — not bugs, do not "fix" in review):
- **Queue depth is single-slot.** RA2 queues up to `MaximumQueuedObjects = 29`
  items per factory (`production.md` §1.5); this engine keeps one item
  building + one ready per queue — a deliberate simplification.
- **Exit facing is deterministic**, not the lockstep game RNG the original
  uses (`production.md` §1.3): south-first, then a fixed 8-ring walk order to
  the true nearest free perimeter cell. Same reachable set as the original,
  not bit-identical to its netgame RNG sequence. The `ExitCoord` sub-cell lean
  (+0xEC8) is also dropped at our cell-resolution grid.
- **Rally default = the exit-adjacent gather cell** (nearest free cell from
  the building's own coordinate, `FUN_00500020`), matching the recovered
  default — not the map center.
- **Primary-building auto-first-on-placement** (a newly placed factory becomes
  primary only if the house has none yet for its category) is the standard
  RA2 rule; the auto-first-primary-on-placement path itself was recovered
  **PROBABLE**, not pinned 1:1 in the disassembly.
- **Primary-set input trigger differs**: the original toggles primary with a
  right-click on the selected factory; this engine's right-click sets the
  rally point instead, so primary-set is a second **left-click** on the
  already-selected factory ([RE-FALLBACK adjudicated], Task 7).
- **GATOW (Patriot Missile System) is absent** from this asset install, so its
  SHP is dropped by the art guard; tab1 (Defenses) shows only GAPILL. This is
  an environmental gap in the install, not an engine limitation.
- **No leave-factory door animation.** The unit drives out under a Move order
  to its exit/rally cell; no per-building door-open/close animation frames are
  replicated.

The AI-1 phase gives the enemy a brain. A house-generalized **per-house AI
registry** (`World::enable_ai_house`) marks any house as AI-controlled with its
own difficulty; the interactive scene enables house 1 (the pre-built Soviet
base) at difficulty **Normal**. Each AI tick runs a pure, side-effect-free
brain, `engine/ai_base` (`AiBaseState`, `build_base_plan`, `next_build`,
`find_build_location`, `difficulty_mods`), that: builds an **ordered base-node
plan** once via a topological (Kahn) prerequisite sort over the house's ownable
BuildingTypes — the recovered mechanism that lets a Refinery land ahead of the
War Factory/Radar that require it without the dispatcher ever needing to skip a
blocked head node (`docs/re/ai-base.md` §2/§2a); **walks the plan head-first**,
applying a **power-preemption** override (force-insert a Power node before any
node that would drive drain over output − reserve) and a separate **economy
expansion** override (add a Refinery/War Factory outright when affordable);
picks a placement cell with a **deterministic outward scan** from existing
anchors, respecting `AIBaseSpacing`; applies **per-difficulty handicaps**
(Easy/Normal/Difficult cost/build-time/build-delay multipliers, §4); and
**rebuilds in place** at a node's retained cell when its building is destroyed,
ahead of any tail expansion (§5). Critically, none of this is new machinery —
`World::step_ai_houses` drives the AI house through the **exact same**
production queues, `can_start`/prerequisite checks, `placement_check`
placement validity, and building-placed path the human player uses; the only
AI-specific carve-out is the recovered `Adjacent=` proximity-check bypass
non-human houses get (`ai-base.md` §3). `step_ai_houses` is the **last**
statement of `World::step()` and is gated by a single early-return
(`if (ai_any_)`) with AI fields excluded from `world_hash`, so with no AI house
enabled the five legacy gates stay bit-identical — verified across every task
of this phase and re-verified here. Calibration authority: `docs/re/ai-base.md`
(house AI tick/build clock, urgency ordering + `[AI]` constants, prereq-sort
plan builder, placement scan, per-difficulty handicaps, rebuild semantics);
companion of `docs/re/construction.md` and `docs/re/production.md`.

Deviations we take for AI-1 (adjudicated divergences from the recovered
`ai-base.md` ground truth — not bugs, do not "fix" in review):
- **Engine-local seeded xorshift32** (`AiBaseState::rng`, fixed seed) drives
  the AI's build-clock jitter and node-position tie-breaks in place of the
  original's lockstep game RNG (`FUN_0065c220`/`FUN_007c4d00`) — same
  distributions/counts, not bit-identical to the netgame sequence
  (`ai-base.md`, "Deviations / plan diffs" — RNG).
- **Single-slot Structure/Defense queues** (one item building + one ready per
  queue) stand in for the original's 29-deep `MaximumQueuedObjects` queue —
  carried forward unchanged from the Production phase and reused as-is for the
  AI's one-structure-at-a-time build clock (the recovered house+0x564c gate,
  `ai-base.md` §1).
- **Topological prereq-sort + force-emit cycle-breaker** is our model of the
  recovered plan builder (`FUN_005052d0`): `build_base_plan` prereq-sorts the
  node list (Kahn-style, ConYard → Power → Barracks → Refinery → War
  Factory/Radar → prereq-sorted remainder → defenses+walls) and force-emits one
  node per pass with no progress to guarantee termination; `next_build` acts on
  the plan's head node only and never skips a blocked head, mirroring the
  recovered head-only dispatcher `FUN_004fe200` (`ai-base.md` §2/§2a).
- **NABNKR and TESLA are absent from this asset install**, so the Soviet
  defense catalog is art-guard-dropped down to its two available members —
  effective Soviet base defenses are **NALASR/NAFLAK** only, out of the
  recovered full set `SovietBaseDefenses = NALASR,NABNKR,TESLA,NAFLAK`
  (`ai-base.md` §2). This is an environmental install gap, not an engine
  limitation (the GATOW lesson from the Production phase).
- **`AIBaseSpacing=1`** is honored as the inter-building cell gap the
  placement scan targets, and AI placement **bypasses the player's
  `Adjacent=` proximity check** end to end — both recovered CONFIRMED
  (`ai-base.md` §3); per-cell validity (bounds/terrain/occupancy) is still the
  same shared grid the player's cursor uses.
- **Wall placement uses the generic center-out scan, not the defense
  foundation-perimeter ring.** The original rings `ConcreteWalls` cells around
  each base defense's foundation perimeter (`FUN_0050c160`, `ai-base.md` §3);
  our AI-1 emits one wall node per planned defense but, because a wall node is
  not a `catalog.defenses` entry, places it via the same nearest-first
  Chebyshev center-out scan (`find_build_location`, `is_defense=false`) as any
  non-defense node — a `[RE-FALLBACK]` deviation on the wall ring geometry
  (the wall COUNT and per-defense pairing are kept).
- **Per-difficulty handicaps are recovered values, applied at `start()`**
  (charge ×= `Cost` mult, build speed ×= `BuildTime` mult) rather than at the
  binary's exact handicap call sites, with identical numbers: `BuildTime`
  0.8/1.0/1.0 and `BuildDelay` 0.03/0.03/0.1 minutes for Easy/Normal/Difficult
  (`ai-base.md` §4). `BuildSlowdown` (no/yes/yes per difficulty) is recovered
  and recorded but **unused** in AI-1 — no consumer of it exists yet in this
  phase.
- Remaining **`[RE-FALLBACK]`**s carried from the hunt, all adjudicated and
  cited in `ai-base.md`: the exact outward placement-scan order (nearest-first
  Chebyshev ring, first-free tie-break, §3); the lobby-selection→house
  difficulty-index arithmetic (index 0/1/2 = Easy/Normal/Difficult as loaded,
  §4); the per-difficulty base-defense-count array's index binding (hardest→25,
  normal→22, easiest→6, §2); and the `BuildDelay` minutes→frames conversion
  (minutes×60 seconds, §1).

The AI-2 phase gives the enemy offense: it forms and marches **offensive
squads** on top of the AI-1 base brain. `engine/aimd_parse` reads the real
`aimd.ini` (index sections + numbered entries, whole-team art-guard-drop) into
the recovered **TaskForce / ScriptType / TeamType** data model — the same
`Ini::section()` ordered-list reader every other loader uses, no new file
format. The pure brain, `engine/ai_team` (`AiTeamState`/`TeamInstance`,
`step_team`), runs each team through a four-state lifecycle —
**Recruiting → Gathering → Executing → Disbanding** — driven by a **ScriptType
interpreter** (`classify_op`) over the recovered ~90-opcode table: the
implementable core (attack-quarry/attack-move/attack-house/-building/-units,
move-waypoint/-cell, guard-area, timed-guard, regroup, scatter, jump, disband)
is fully dispatched; every opcode belonging to a subsystem this engine doesn't
model yet (e.g. AIWaitFor-triggers, non-implemented base-tab opcodes) is
interpreted to an **inert skip** (logged, `pc++`) rather than dropped from the
table, so the interpreter's opcode coverage matches the recovered table
1:1. Team **selection** (`select_autocreate_team`) is autocreate-only —
`IsBaseDefense=no`, optional `House=` restriction, `Priority` ordering, `Max`
cap (`Max=0`=uncapped), a `TotalAITeamCap`, and a `CreditReserve` gate, one
spawn per autocreate tick — and **fill** is two-source: `recruit_idle` claims
an already-IDLE unit of a needed type first, and only the shortfall
(`next_produce_type`) is queued for production. Critically, none of this is
new production machinery: `step_ai_teams` drives teams **only** through the
EXISTING Infantry/Vehicle production queues, `launch_unit`, and
`order_unit_move`/attack orders — the same paths the human player and the AI-1
base brain already use. Teams gather at a seeded-angle ring around the enemy
base (`AISafeDistance`) before executing their ScriptType, and march on
whatever the interpreter's quarry/target opcodes resolve (attack-house
defaults to the player). `World::step_ai_teams(dt)` runs after
`step_ai_houses` inside the existing `if (ai_any_)` firewall, gated further by
a per-house `team.enabled` flag (default `false`), and new AI-team fields are
excluded from `world_hash` — so with team support off (the default for every
existing gate and every non-team house), the six legacy gates stay bit-
identical, re-verified across every task of this phase and re-verified
here. Calibration authority: `docs/re/ai-team.md` (struct layouts + aimd.ini
format §1, lifecycle state machine §2, full opcode table §3,
selection/weighting §4, quarry/target selection §5, production/reserve
interplay §6); companion of `docs/re/ai-base.md` and `docs/re/production.md`.
**AITriggerTypes-driven weighted team creation is out of scope for AI-2 and
lands in AI-3** — this phase's `select_autocreate_team` only ever spawns from
the always-eligible autocreate pool.

Deviations we take for AI-2 (adjudicated divergences from the recovered
`ai-team.md` ground truth — not bugs, do not "fix" in review; see
`docs/re/ai-team.md`'s "Deviations / plan diffs" section for the full,
cited list):
- **Engine xorshift32 for AI tie-breaks** (`AiTeamState::rng`, fixed seed)
  stands in for the original's lockstep game RNG — the AI-1 carry-forward,
  applied to team RNG draws too.
- **Synthetic waypoint→cell table** (`make_ai_waypoints`, a deterministic
  ring around the player base) stands in for the map's `[Waypoints]` section,
  which stays unparsed until a later phase.
- **Random-angle gather ring + engine free-cell snap**: the gather cell is a
  seeded-xorshift32 angle on a circle of radius `AISafeDistance` around the
  enemy base centre (the recovered rule, `ai-team.md` §2), snapped to a free
  cell by the engine's own move/path machinery rather than the original's
  `FUN_0056d750`. The "gathered"/arrival predicate uses the recovered
  `Stray=2.0`/`CloseEnough=2.25` cell radii.
- **Quarry class-filter targeting with proxy value + xorshift32 jitter**:
  targets are scored `distance² × value × seeded-RNG` (`ai-team.md` §5);
  `value` is a proxy of 1 pre-map, and the jitter uses the engine xorshift32
  in place of the lockstep RNG **[RE-FALLBACK]**.
- **BuildSlowdown modelled as an over-queue damp** (a difficulty byte, Easy
  off / Normal+Difficult on: don't queue more of a category already
  queued/on-hand) — **not** a build-time multiplier (`ai-team.md` §6).
- **Single-slot Infantry/Vehicle queues** (one unit building at a time) vs
  the original's 29-deep queue — carried forward from the Production phase;
  the single slot maps the recovered one-at-a-time team-production gate.
- **MoveToCell (0xb) and quarry kinds 5/6/7 (factories / base-defenses /
  base-threats) resolved coarsely**: MoveToCell resolves to the enemy base
  centre (no packed-cell unpack pre-map) and quarry 5/6 resolve as any
  building **[RE-FALLBACK]** — plus every remaining `[RE-FALLBACK]` recorded
  in `ai-team.md`, cited by §.
- **DissolveUnfilledTeamDelay is multiplay-only** — the default
  skirmish/campaign path never auto-dissolves an unfilled team on the timer
  (`ai-team.md` §2); a `Full=no` team launches as soon as it has a member.

The AI-3 phase gives the enemy judgement: it composes on AI-2's team
machinery by deciding **which** team to create and **when**, adaptively,
instead of only ever drawing from the always-eligible autocreate pool. The
recovered flat-line `[AITriggerTypes]` + `[AITriggerTypesEnable]` data model is
an extension of the same reader every other table uses
(`engine/aimd_parse`'s `parse_ai_triggers`): one full `AITriggerType` per
`<id>=Name,field,...` key in the CONFIRMED 18-field order, its
`condition_object` resolved through the same two caller-supplied
unit/building roster resolvers `parse_ai_triggers` already takes into a
`{TechnoKind, roster index}` pair, its comparator decoded from the
**value-first** on-disk `Conditions[4]` blob (count/value then operator —
only slot `[0]` is ever meaningful), and the whole trigger art-guard-dropped
when its object or its `Team1` (the REQUIRED, attribution-key team) fails to
resolve; `[AITriggerTypesEnable]`'s per-id flag lands as `is_enabled`. The
pure brain, `engine/ai_trigger` (`AiTriggerState`, `select_trigger`,
`adjust_weight`), dispatches the FULL recovered `ConditionType` enum
(`kCondNone`..`kCondCivilianHouseOwns`, −1..7) over a per-pass `TriggerView`
snapshot — `EnemyHouseOwns`/`OwnerHouseOwns`/`CivilianHouseOwns` count the
condition object against the comparator, `HouseCredits` compares the OWNER
house's real credits (not "enemy credits"), and every other value (the
superweapon-charge pair, the two unused inline house-metrics) evaluates
FALSE rather than crashing — through the recovered enable-filter matrix
(base-defense reject, `is_enabled`, skirmish-only, the **INVERTED**
per-difficulty `enabled[2 − difficulty]` consult, owner-house, side,
tech-level). An eligible trigger set is drawn by the recovered
**cumulative-sum walk over `round(current weight)`** — a 5000-weight
trigger anywhere in the pool restricts the draw to the mandatory 5000 tier —
off a dedicated, seeded xorshift32 stream (`AiTriggerState::rng`, separate
from `AiTeamState::rng`), returning the fired trigger's id plus **both**
Team1 and Team2 to create. `World`'s trigger-eval step rides inside AI-2's
existing TeamDelays check pass, right beside `select_autocreate_team` — a
SECOND creation path into the same `st.teams` list, gated by the
per-house `triggers_enabled` firewall (default `false`) so the seven legacy
gates stay bit-identical whether or not the new step ever runs. The only
additive change to AI-2 itself is `TeamInstance::completed_success`, set
`true` exclusively by the `kOpDisband` script action (success is
**script-completion**, not survivor count — the recovered definition);
World's disband handler scans the trigger table for every trigger whose
`Team1` equals the disbanding team's TeamType (there is no team→trigger
back-pointer) and feeds each one's `completed_success` outcome through
`adjust_weight`'s additive **+20 success / −50 failure** rule plus a
track-record term, clamped to `[min_weight, max_weight]` — so a shared
Team1 moves every trigger that names it, and a Team2-only-created team
feeds nothing back. (Historical note: this paragraph used to claim the
interactive enemy ran all three layers; that stopped being true at `f77e68e`,
the Scene-framework refactor, which dropped the `aimd.ini` load and the team
config from the real game. It is true again since gap fix G-60 — see "Real-play
AI" below.)
Calibration authority: `docs/re/ai-trigger.md` (field layout + comparator
packing §1/§2, the full ConditionType dispatch §2, weighted selection +
weight dynamics §3/§4, enable filters + cadence §5, composition/attribution
§6); companion of `docs/re/ai-team.md` and `docs/re/ai-base.md`. **AI-3 is
the trigger-selection/adaptive-weight layer composing on AI-2's team
machinery — it does not add superweapons, aircraft, or naval play.**

Deviations we take for AI-3 (adjudicated divergences from the recovered
`ai-trigger.md` ground truth — not bugs, do not "fix" in review; see
`docs/re/ai-trigger.md`'s "Deviations we take" section for the full, cited
list):
- **Engine xorshift32 in a SEPARATE, dedicated `AiTriggerState::rng` stream**
  for the weighted draw vs the original's lockstep game RNG
  (`FUN_0065c220(1,Σw)`) — same distribution, not bit-identical to the
  netgame sequence; enabling triggers never perturbs the AI-2 draw sequence
  (protects `ai_team_hash`) [ai-trigger.md deviations].
- **ct=4 HouseCredits implemented as a REAL `credits OP N` compare** — every
  stock use is `N=0, op>=` (effectively always-true), but our economy
  exposes the owner house credits, so a mod-supplied non-zero `N` works
  [ai-trigger.md §2/deviations].
- **Inert conditions**: ct=5 (IronCurtainCharged) and ct=6
  (ChronoSphereCharged) evaluate FALSE until a superweapon subsystem exists;
  ct=2/ct=3 are recovered-but-unused inline house-metrics left unimplemented
  (FALSE); ct=7 (CivilianHouseOwns) evaluates FALSE unless a neutral/civilian
  house exists (none in this engine) [ai-trigger.md §2/deviations].
- **Enemy aggregation**: the original's `ConditionMet` receives the house's
  ONE current enemy (`house+0x5600`); our `World` aggregates ALL rival
  houses for the enemy-owns count (the `ai-team.md` convention)
  [ai-trigger.md deviations].
- **One additive AI-2 flag**: the spec's "zero AI-2 changes" is relaxed to
  `TeamInstance::completed_success` (set at `kOpDisband`) — required because
  the recovered success definition is script-completion (`team+0x84`), not
  survivors-at-disband [ai-trigger.md §4].
- **Trigger cadence CONFIRMED shared**: no separate AITrigger clock exists —
  eval runs inside AI-2's team-management (TeamDelays) pass; the original's
  per-house probability gate (`house+0x565c`=100) is always-true by default
  and modeled as always-run [ai-trigger.md §5].
- **Base-defense split simplified [RE-FALLBACK]**: base-defense triggers are
  rejected (offensive scope; ALL stock triggers are `IsForBaseDefense=0`)
  because the recovered defensive-need inputs (`Rules+0x1394[diff]`,
  `house+0x566c`) are not tracked [ai-trigger.md §5].
- **Team-buildable predicate deferred [RE-FALLBACK]**: the original's
  `FUN_0041ff00` buildable check is deferred to AI-2's recruit/produce
  machinery — an unfillable team idles and disbands as a failure
  [ai-trigger.md §5/§6].
- **Unit-object conditions resolve within the AI house's own roster scope**
  (enemy-owns-UNIT triggers naming types outside it art-guard-drop);
  building objects use the full shared building roster.
- **Skirmish-only engine**: triggers with `IsForSkirmish=0` are never
  eligible (all stock are 1); the campaign enable path (non-inverted) is not
  implemented — plus every remaining `[RE-FALLBACK]` recorded in
  `ai-trigger.md`, cited by §.
- **Known cosmetic issue**: `--demo-ai-trigger`'s END screenshot renders
  black (a long-run SDL backbuffer/device-loss effect over the demo's
  ~13500 un-presented frames; screenshot-only — it does **not** affect the
  hash, the witness chain, or any gate). The START screenshot renders
  correctly; fixing this is deferred to a future rendering-polish task.

The CB-1 phase gives units a ride: transports that load, carry, and unload
passengers. The pure module, `engine/cargo` (`can_load`, `cargo_total_size`,
`next_eject`, `eject_cell`), recovers the original's capacity/eject model:
capacity is a passenger **COUNT** — `can_load` gates on the cargo list's
current size being less than the transport's `Passengers=`, plus a
per-passenger `Size <= SizeLimit` cap — while `cargo_total_size` is a
separate, **pip-only** Σ-Size metric (the original `GetTotalSize` has no
capacity caller at all, so the count gate and the pip-total are deliberately
different quantities fed by the same cargo list). `next_eject` is **LIFO**
(`cargo.back()`) — the original container is a head-pushed/head-popped stack,
so the last unit to board is the first to leave. `eject_cell` is the
facing±90° scatter picker recovered from the original unload: it alternates
sides per eject and falls back to the nearest free cell of the immediate
neighbourhood (row-major, r=1 then r=2) when the scatter cell itself is
blocked, returning `{-1,-1}` only when fully boxed in. `factory.cpp`/
`exit_cell` are **untouched** — the original unload does not reuse the
`KickOutUnit` ring, so the pre-hunt plan to extract a shared ring helper was
dropped at RE-GATE.

The World integration layer, `World::order_enter_transport` /
`World::step_cargo`, wires this into the existing sim with no new movement,
combat, targeting, selection, render, or AI-recruitment machinery:
`order_enter_transport` batches the ordered movers onto the transport's cell
through the existing `assign_formation` group-move path; boarding itself
happens at Chebyshev adjacency ≤ 1 with capacity re-checked at the moment of
arrival (so a late arrival can be refused even if it wasn't when the order was
given, and the refusal is witnessed); a boarded unit is released from ALL
occupancy and carried **OFF-MAP** via one added clause on every existing
corpse-skip site (unpickable, undrawn, untargetable, non-colliding) — its
coordinates go **STALE by design** while carried (stamped fresh only on
eject or death), matching the original's own passenger-limbo semantics.
`step_cargo` is firewalled behind `cargo_any_` and runs between `step_units`
and `step_turrets`, ejecting **one passenger per logic frame** in LIFO order
via `eject_cell`, claiming the destination cell's occupancy before combat
targeting runs that same frame; a fully boxed-in transport HOLDs and retries
next frame, mirroring the original's put-back-and-retry (and `launch_unit`'s
own precedent). Transport death **tombstones its cargo** — confirmed at
three original sweep sites with no survival branch; `OpenTopped` grants no
death-survival (it only governs fire-out while alive, a documented
carry-forward, not CB-1 code).

The wiring layer adds the HTK Flak Track as the primary transport (art-guarded,
with the FV IFV as a script-scaled fallback if HTK art ever regresses), a
right-click-on-own-transport enter gesture, a `D` key to unload the selected
transport's cargo, a loaded-pip row that reuses the HP-bar chrome (filled by
the pip-only `cargo_total_size`, not the COUNT capacity), and
Enter/LeaveTransportSound routed through the existing chrono-sound-style
guarded audio hook. Byte-identity discipline follows the same strategy every
prior phase used: `world_hash` is **untouched** (strategy (a) — it reads only
cell/hp/alive, none of which a non-cargo unit's carry state perturbs), and the
eight legacy gates were re-verified byte-identical both at the additive
Task-2 commit and again at this finalize. Calibration authority:
`docs/re/transport-cargo.md` (capacity/eject recovery, boarding trigger,
carry-guard sweep, unload flow, death disposition, deviations).

Deviations we take for CB-1 (adjudicated divergences from the recovered
`transport-cargo.md` ground truth — not bugs, do not "fix" in review;
mirrors the hunt doc's own "Deviations we will take", adjudicated at
RE-GATE `0cef283`):
- **Boarding at Chebyshev adjacency** — the original boards on CO-LOCATION
  (the passenger paths onto the transport's own tile, `FUN_004D8ED0` case
  0x12); our one-unit-per-cell occupancy has no "walk onto an occupied cell"
  notion, so adjacency ≤ 1 is the adjudicated deviation (same reachable set,
  deterministic) [`transport-cargo.md` §3].
- **Deterministic ordering with NO RNG** — the original's container ops
  consult no RNG (the suspected `FUN_007C4D00` is ftol, not a draw); the
  last slot goes to whichever boarder's arrival is processed first, so our
  ascending-unit-index boarding tie-break is the faithful analogue
  [`transport-cargo.md` §6].
- **Eject fine-placement fallback** — the eject DIRECTION (facing ±90°,
  sides alternating per eject) and the one-per-tick cadence are the
  RECOVERED behavior; `FUN_00481090`'s exact fine placement is unpinned, so
  a blocked scatter cell falls back to the first free cell of the immediate
  neighbourhood (deterministic row-major, r=1 then r=2). The `KickOutUnit`
  `DAT_0089E5F8` 8-ring is NOT shared with unload — it stays `exit_cell`-only
  [`transport-cargo.md` §4].
- **No DeployTime deploy-start delay** — `DeployTime` is a real countdown
  timer in the original (`FUN_004A5150`) but its scale/application point is
  unpinned; CB-1 begins ejecting immediately at one-per-tick (documented
  carry-forward) [`transport-cargo.md` §4].
- **Same-house loading only** — the original gate is `IsAlliedWith` (same
  house OR same side OR ally bitmask, `FUN_004F98B0`); allies are unmodeled
  in CB-1 (one house per side), and `can_load`'s house clause is where allied
  loading would attach [`transport-cargo.md` §2].
- **No nested loaded transports** — `can_load` rejects a passenger with
  non-empty cargo (data-unreachable with HTK `SizeLimit=2` < vehicle `Size=3`)
  [spec Out-of-scope; `transport-cargo.md` deviations].
- **Passenger coords are STALE while carried** (stamped on eject/death); the
  original limbos carried passengers the same way — they live only in the
  passenger list [`transport-cargo.md` notes].
- **Transport-death default**: cargo dies with the transport — CONFIRMED,
  three sweep sites, no survival branch; `OpenTopped` grants NO death
  survival (it governs fire-out while alive — an RE-documented
  carry-forward for the fire-out phase, not CB-1 code) [`transport-cargo.md`
  §5].
- **Sub-cell eject lean dropped** at our cell resolution (the established
  `ExitCoord` divergence); the ±90° scatter side survives as the CELL choice
  + the outward eject facing [`transport-cargo.md` §4].
- **Enter cursor art / full pip row rendering are cosmetic**: default cursor;
  minimal tick-rect indicator filled with the SIZE-SUM (`cargo_total_size` —
  the original `GetTotalSize` is pip-only, capacity is the COUNT)
  [`transport-cargo.md` §1].
- **AI use of transports stays inert**: ScriptType opcodes 0x2e/0x2f remain
  inert in the AI-2 interpreter; flipping them implementable atop this
  subsystem is a recorded carry-forward.

The pre-existing scratch-path/`--out-dir` debt shared with prior gates (demo
screenshots write to the harness scratchpad rather than a configurable
output directory) is unchanged by this phase — carried forward, not CB-1
scope.

## W5-A: campaign scripting and real-play mission proofs (branch `w5-camp`)

Engine fixes (all in the campaign path unless noted):
- **Campaign economy armed** (`CampaignRuntime` ctor -> `wire_economy`): campaign Worlds had no ore growth, no
  harvester FSM and no refinery income for any house; every mission ran broke.
- **Wall diagonal gaps** (`pathfind` leak mask, `World::block_wall_cell`, [RE-FALLBACK, map evidence]): two wall
  overlays touching only at a corner leave a gap units walk through (ALL12's Tesla Reactor fence).
- **PlayerControl houses take attack / board orders** (`World::order_attack*`, board): SOV09's Yuris ignored attack clicks.
- **Mind control capacity** (R-036, RE-FALLBACK): a controller holding a victim does not auto-capture another.
- **EnteredBy before the trigger loops**: a capture on a house-loop frame no longer lets "X Destroyed" fire first (ALL02).
- **Recruit rule** [RE: TeamClass::Can_Add FUN_006E9B80]: only a strictly higher-Priority team takes members of a
  live recruitable team; loaded transports are never recruited (SOV02's MCV boat).
- **Campaign crate RNG** seeded from the mission name (deterministic play proofs).
- **Trigger actions** 35 PreferredTarget (AI SW quarry, FUN_00509710/0050CF90), 40 ResizeView, 44 IonStorm, 51 Reshroud,
  61/62 TurnOff/On (World::set_building_power), 70 DestroyTag, 77/78 ratios (recorded), 89/117 presentation;
  9 FireSale = house AI state 4 (sell all + hunt every AI pass, FUN_004FD320).
- **Events** 23 TeamLeavesMap, 31 AllBridgesDestroyed, 34 ComesNearWaypoint, 53/54 SpyAsHouse/Infantry
  (3/18/35/45/46 documented as unmodelled; unused by the 24 maps).
- **Team scripts** 10 Follow, 18 ChangeTeam, 19/41 Panic/Unpanic, 42 ForceFacing, 50 Flash, 55 IronCurtainMe.
- **Placed objects**: Facing and Veterancy applied (SubCell by W5-B).
- **Computer houses** [RE: FUN_00500960, FUN_00686560, FUN_004FD320, FUN_0041E740]: map IQ gates SW autofire
  ([IQ] SuperWeapons), aimd.ini merged unless `[Basic] IgnoreGlobalAITriggers=yes`, CampaignMoneyDelta for
  PlayerControl houses; the difficulty binding (house+0x184 = 2 - slider) confirmed.
- Save format **v22**.

Real-order proofs (`scripts/play/m-*-real.play`, generators in `scripts/play/gen/`): **m-sov01-real** (SOV01 WON:
bridge hut repair + Pentagon), **m-all03-real** (ALL03 WON: Psychic Beacon), **m-all12-real** (ALL12 opening:
SEAL C4 through the fence gap, Chrono reinforcements), **m-sov02-mcv** (cheat-free MCV chain).

## CB-2 — Aircraft & Flight Locomotor

The CB-2 phase gives units flight: a pure `engine/flight` module plus a World
integration that routes, moves, and fights an airborne unit without ever
touching ground A*/occupancy. The pure module (`engine/flight.{hpp,cpp}`,
no RNG, no World/Occupancy dependency) recovers the original's altitude and
heading model: `step_altitude` climbs or descends toward a target altitude by
an exact per-frame rate with no overshoot (clamped at the bound); `cruise_step`
is a straight-line horizontal advance toward a goal cell that never consults
`passable_`/`occ_` — an airborne unit takes no path, it flies; `heading_toward`
turns by the shortest arc toward a target bearing at a capped per-frame rate,
and is a no-op when the unit is `OmniFire` (a turretless flyer needs no facing
to shoot); `altitude_to_screen_dy` converts the unit's current altitude into
the raised-draw pixel offset used at render time; and `advance_flight_state`
drives the `Grounded → TakingOff → Cruising → Descending` state machine off
altitude thresholds alone.

The World integration layer wires this into the existing sim with the same
firewall discipline every prior phase used: `order_unit_move` grows an
aircraft branch that assigns a flight goal (`flight_goal_x/y` + `TakingOff`)
instead of routing through A*; `World::step_flight` is firewalled behind
`aircraft_any_` and runs between `step_cargo` and `step_turrets` — takeoff
releases the unit's held ground cell exactly once (climb and horizontal
travel then overlap, not gated on reaching altitude first) and an airborne
unit reserves no occupancy for the remainder of its flight; the corpse-skip
site gains `|| u.is_aircraft` (an airborne unit is never eligible for ground
movement) and `valid_target` gains `&& !v.is_aircraft` (ground units — and
their auto-acquire — never target the aircraft, suppressing air-to-air and
ground-AA-vs-air). Even while airborne, `cell_x/cell_y` keep tracking the
rounded ground projection of `fx/fy` — used for the ground shadow and for
combat range checks — but that projection claims no occupancy slot; it is
the CB-1 "coordinates stale while carried" analogue, here live but
unreserved.

Air-to-ground combat needed no new targeting or damage machinery: the
aircraft's move-to-attack-range request routes through the same flight-goal
mechanism (not A*), and the vehicle aim gate is bypassed for `OmniFire` units
so a turretless flyer fires from cruise without turning to face. The bomb
itself is delivered instant-to-target-cell (the falling-bomb travel time is a
documented carry-forward), but its `BlimpHE CellSpread=2 PercentAtMax=.5`
damage is a REAL ~512-lepton 2-cell splash via `apply_spread_damage` (linear
100%→50% falloff) — not collapsed to a single-target hit — gated on
`CellSpread >= 1` in `try_fire` so every pre-existing sub-1-cell warhead
(tank AP `CellSpread=.3`, infantry small arms) keeps its exact, byte-identical
single-target path unchanged.

Render adds one raised voxel draw (`altitude_to_screen_dy` folded into the
existing `draw_voxel_sprite` as a trailing default-arg `extra_dy`, so every
pre-existing call site is untouched) plus a new ground shadow — a dark
translucent diamond plotted at the un-raised ground cell, CB-2's
approximation of the original's `FUN_00755680` silhouette. The exemplar
(`[ZEP]`, the Kirov Airship) is art-guarded with an ORCA/BEAG fallback if its
art ever regresses.

Byte-identity discipline follows the same strategy every prior phase used:
`world_hash` is **untouched** (strategy (a) — flight fields are inert
defaults for every non-aircraft unit and are never read by the hash); the
nine legacy gates were re-verified byte-identical both at the first additive
CB-2 commit and again at this finalize. Calibration authority:
`docs/re/aircraft.md` (Jumpjet/Locomotor recovery, flight state machine,
occupancy guard sweep, splash-damage adjudication, deviations).

Deviations we take for CB-2 (adjudicated divergences from the recovered
`aircraft.md` ground truth — not bugs, do not "fix" in review; mirrors the
hunt doc's own "Deviations we take", adjudicated at the Task 1 RE-GATE):
- **Instant projectile FLIGHT, but REAL 2-cell splash damage** — the bomb is
  delivered instant-to-target-cell (the `BlimpBombP Vertical=yes`
  falling-bomb travel-time is the documented carry-forward, `voxel-render.md`
  §7), but the `BlimpHE CellSpread=2 PercentAtMax=.5` DAMAGE is implemented
  as a genuine ~512-lepton 2-cell splash (`apply_spread_damage`, linear
  100%→50% falloff, `combat.md` §7), NOT collapsed to single-target. The
  applicator is gated on `CellSpread>=1` so every existing sub-1-cell
  warhead (tank AP `CellSpread=.3`, infantry) keeps its exact single-target
  path [`aircraft.md` §4 → `combat.md` §7].
- **Spread falloff curve is a linear approximation** — `combat.md` §7 pins
  the retaliation choice but the exact `CellSpread` damage-falloff curve
  (`FUN_00489190`) was not fully unwound; the adjudicated linear
  100%→`PercentAtMax` falloff over `round(CellSpread)` cells stands in until
  pinned [`aircraft.md` §4 → `combat.md` §7].
- **Ground-shadow approximation** — a dark translucent filled diamond at the
  un-raised ground cell approximates the original's `FUN_00755680` (which
  plots the model's column silhouette in color 1 with no z-test); the exact
  silhouette/color is not pixel-matched [`aircraft.md` §3].
- **Altitude→pixel factor [RE-FALLBACK]** — the raised-draw offset is
  `(altitude/256)·36.74` px (voxel-render.md §6's up/z term); the hunt did
  NOT unwind the loco pixel-offset formula, so this stays the one flagged
  [RE-FALLBACK] to revisit [`aircraft.md` §3].
- **Ground AA** — [superseded by gap fix C, G-44] an airborne aircraft is a
  target for AA=yes weapons (HTK/APOC/FLAKT/GGI secondaries, NASAM/NAFLAK
  defenses); the per-target weapon choice picks the slot (docs/re/weapons.md).
- **No airfield/Ammo rearm cycle** — the exemplar (`[ZEP]`, no `Ammo=`,
  `BalloonHover=yes`) is continuously armed and never lands; the
  `FlyLocomotionClass`/`Ammo`/airfield-dock mechanism for `[AircraftTypes]`
  (`ORCA`/`BEAG`/Harrier) is mapped-for-later, not CB-2 code [`aircraft.md`
  §4; spec §Out].
- **BalloonHover — descend/land unused** — the flyer takes off once and
  holds cruise; the `Descending`→`Grounded` state exists so the machine is
  complete but the exemplar never exercises it; a full land-cycle is a
  carry-forward [`aircraft.md` §2; spec §3.4].
- **No jumpjet wobble / pitch tilt** — `JumpjetNoWobbles=yes` removes the
  sinusoidal wobble (kept out for determinism); the airborne sprite renders
  level (`PitchSpeed`/`PitchAngle` dropped — the ground voxel "no slope
  tilt" precedent) [`aircraft.md` §6; spec §Out].
- **Aircraft coords are LIVE but claim no cell** — `cell_x/cell_y` track the
  ground projection of `fx/fy` while airborne (for the shadow + combat
  range) but reserve no `occ_` slot; the deliberate analogue of CB-1's
  "coords stale while carried" [`aircraft.md` §1; spec §4].
- **Speed/turn/climb conversions** — `flight_turn` is sourced from the
  unit's `ROT` (× 2, the `Link_To_Object` FacingClass rate) — **not**
  `JumpJetTurnRate`, which the hunt confirmed is vestigial for turn in this
  build (`aircraft.md` §1); `flight_speed` is `JumpjetSpeed` (fallback base
  `Speed`) converted via the recovered `speed_to_cps` scale (gap fix C, G-42), and `climb_rate`/`descend_rate` are the recovered
  per-frame `JumpjetClimb`/`JumpjetCrash` values — all converted to
  per-second rates (×15) for the dt-based `step_flight`, the
  `rot_u16_per_sec` precedent [`aircraft.md` §1/§2/§5].
- **AI use of aircraft stays inert** — the AI never fields aircraft in CB-2;
  aircraft-related ScriptType opcodes stay inert in the AI-2 interpreter (a
  recorded carry-forward) [spec §Out].

## CB-3 — Naval & Water Movement

The CB-3 phase gives units a second movement domain: water. The engine had no
notion of a water cell before this phase, so the foundational piece is
`LandType` — the net-new water-cell identification recovered by classifying
each cell's owning theater tile-set against the theater `[General]` indices
(`WaterSet`, `ShorePieces`, `CliffSet`, `WaterCliffs`, `RoughTile`,
`ClearTile`) via `ra2engine::Theater` + the pure
`ra2assets::land_type_from_tile_set`/`Theater::land_type_of_cell`, walked
per-map-cell by `main.cpp`'s `resolve_land_types`. The pre-hunt `.tmp`
`TerrainType` byte was FALSIFIED for this purpose (it carries values 9–15 and
never Water on the real map) and is explicitly NOT used anywhere in the
CB-3 path.

On top of `LandType`, water passability is pure and binary:
`MovementDomain { Land, Water }` plus `build_water_passability` — a cell is
passable to a Float ship iff its `LandType` is `Water` AND it is materialized
in the Land grid (the intersection with `passable_`); the Land grid itself
(`passable_`, `build_passability`) is literally unchanged by any of this.

World integration adds `grid_for(domain)`, which selects the existing Land
grid for the DEFAULT `Land` domain — byte-identical to every prior phase —
and the new `water_passable_` grid for `Water`; `set_water_terrain` is
opt-in and is never called by any existing (pre-CB-3) demo. `order_unit_move`
and `step_units`'s re-path both gained a domain-selected grid clause (ternary
on `movement_domain`, provably `Land` for every non-naval unit), and the
combat-approach branch gained a naval path that routes through the water
grid. Occupancy is unchanged, and the water/land grids stay disjoint by
construction — CB-3 does NOT retrofit ground-from-water blocking onto the
Land grid.

Naval combat needed no new acquire/targeting/damage machinery: the Sea
Scorpion (`[HYD]`) acquires and fires `FlakTrackGun` through the EXISTING
`step_combat`/`try_fire` path; only its approach differs, routing to a WATER
A* path instead of the ground grid. Render is likewise a non-event: ships are
voxels at altitude 0, drawn through the EXISTING `draw_vehicle_unit` path — no
new render code, no shadow, no altitude offset (unlike CB-2's airborne
units). The exemplar is art-guarded, with a DRED/DEST fallback if `[HYD]`'s
art ever regresses.

Byte-identity discipline follows the same strategy every prior phase used:
`world_hash` is **untouched** (strategy (a) — naval fields are inert defaults
for every non-naval unit and are never read by the hash); the ten legacy
gates were re-verified byte-identical both at the first additive CB-3 commit
(Task 4) and again at this finalize. Calibration authority: `docs/re/naval.md`
(naval locomotor CLSID + movement-shape recovery, LandType→passability model,
water occupancy/repath, naval combat + targeting, exemplar-stat formulas,
determinism/no-RNG, map-water availability).

Deviations we take for CB-3 (adjudicated divergences from the recovered
`naval.md` ground truth — not bugs, do not "fix" in review; mirrors the
hunt doc's own "Deviations we take", adjudicated at the Task 1 RE-GATE):
- **Binary water passability, not speed-weighted terrain cost** — a cell is
  water-passable to a Float ship iff `LandType==Water` AND it is materialized
  (the game's `Ground[LandType][SpeedType] == 0.0` gate is binary, confirmed);
  the `[Clear]`/`[Water]`/… speed-% → A* cost model is a documented
  carry-forward [`naval.md` §2].
- **LandType via the theater tile-set classification (NOT the `.tmp`
  byte)** — a cell's land type is decoded by classifying its owning
  `[TileSetNNNN]` set against the theater `[General]` set indices
  (`WaterSet=21`, `ShorePieces=12`, `CliffSet=10`, `WaterCliffs=15`,
  `RoughTile=13`, `ClearTile=0`) via `ra2engine::Theater`/
  `land_type_from_tile_set`; the pre-hunt `.tmp TerrainType`-byte source was
  FALSIFIED (it carries 9–15, zero Water) and is NOT used [`naval.md` §2].
- **Ground-from-water blocking NOT retrofitted** — CB-3 does NOT change the
  Land grid `passable_` to block ground units from water (that would perturb
  the ten gates and is unnecessary — no existing demo paths a ground unit
  through the lake); the water/land inverse is expressed by the separate
  water grid the naval domain reads [`naval.md` §2; spec §Out].
- **Instant single-target hit; splash reuses the existing
  `apply_spread_damage`** — `FlakTrackGun` damage is delivered
  instant-to-target-cell (the established projectile simplification,
  `combat.md`), which drops the vanilla `FlakScatter`/`Inaccurate`
  ballistic-scatter RNG. `FlakTWH CellSpread=1.0` (≥ 1.0, `PercentAtMax=1.0`
  ⇒ no falloff) flows through CB-2's EXISTING `apply_spread_damage` one-ring
  splash — NO new code, faithful. Only the `FlakTProj Speed=50` projectile
  flight itself is a carry-forward [`naval.md` §4/§6]. `[HYD]` auto-acquires
  both land and naval targets (no targeting keys) — faithful, not a
  deviation.
- **Naval duel target** — the demo's target is a second Sea Scorpion on
  water, keeping every combat-approach cell on water (deterministic;
  sidesteps the naval-to-shore firing-cell question); naval-to-shore
  anti-surface fire is RE-documented but the demo does not depend on it
  [`naval.md` §4].
- **Pure-water ships** — CB-3 models the Float water-movement domain only;
  the submarine cloak/submerge (`[SUB]` second CLSID), amphibious/hovercraft
  land↔water transition (`SpeedType=Hover`/`Amphibious`, Beach/FloatBeach
  crossing), and spawner naval delivery (`[DRED]`/`[DEST] Spawns=`) are
  DEFERRED and mapped-for-later by the hunt [`naval.md` §1; spec §Out].
- **No Naval Yard production** — CB-3 spawns the exemplar directly (like
  CB-1/CB-2); production-queue + naval-yard-on-water placement are
  carry-forwards [spec §Out].
- **AI use of naval units stays inert** — the AI does not field ships in
  CB-3; naval-related ScriptType opcodes stay inert in the AI interpreter (a
  recorded carry-forward) [spec §Out].
- **Naval fields are LIVE but excluded from `world_hash`** — `is_naval`/
  `movement_domain` are hashed only by `naval_hash` (strategy (a));
  `cell_x/cell_y` are hashed by `world_hash` and are live for a naval unit,
  but no existing demo has one, so those coords are default-driven exactly
  as before [`naval.md` §6; spec §8].

## CB-4 — Superweapons

The CB-4 phase adds a recharge-timer superweapon framework that is
AI-INDEPENDENT by construction: a new per-house registry `sw_houses_`/
`sw_any_` on `World` works for the player house 0 — which is never present in
`ai_houses_` — as readily as for any AI house. The data model is the pure,
header-inline `Superweapon` lifecycle (`type`, `recharge_secs`, `timer`,
`ready`, with `tick`/`is_ready`/`on_fired`/`charge_fraction`), embedded per
house in `HouseSuperweapon` alongside the resolved effect parameters
(`eff_damage`, `eff_cell_spread`, `eff_percent_at_max`, `eff_verses`).
`grant_superweapon` is opt-in and is never called by any existing (pre-CB-4)
demo, so `sw_any_` stays false and `step_superweapon` — inserted into
`World::step` right after `step_flight` — early-returns as a firewalled
no-op for every one of them.

Target selection and fire mirror the existing placement-mode input pattern:
the `N` keybind or a cameo click ARMS targeting, and the next map click FIRES
via `fire_superweapon`, reusing the existing screen→cell pick
(`input.cpp`'s placement block). `fire_superweapon` is refused unless the SW
is READY, and a successful fire calls `on_fired()`, which resets the timer to
0 and clears `ready` — the SW then recharges from scratch.

The fired EFFECT reuses CB-2's area-damage applicator rather than inventing a
new damage system: `fire_superweapon` builds a transient, never-added-to-
`units_` virtual detonator `Unit` (house + target fx/fy + the four resolved
warhead fields) and calls the EXISTING `apply_spread_damage` against it —
`Unit` itself is untouched and `combat.cpp`'s behavior is unchanged. Because
`apply_spread_damage` lived in `combat.cpp`'s anonymous namespace (internal
linkage), it was promoted to the `ra2engine` namespace and declared in
`combat.hpp` so `world.cpp` can call it — a linkage-only change; the function
body and its existing caller (`try_fire`) are byte-for-byte unchanged. The
`[NukeSpecial]`/`[NukePayload]`/`[NUKE]` values (`RechargeTime`, `Damage`,
`CellSpread`, `PercentAtMax`, `Verses`) are resolved from `Rules` by
`resolve_superweapon`. The detonation is enemies-only (the applicator's
`w.house == attacker.house` skip spares the firing house) and instant
(applies the frame `fire_superweapon` is called, no missile flight). It
splashes both UNITS (via the reused applicator) and BUILDINGS in the blast
radius — the hunt RE-GATE-confirmed (§5.3) that the real `[NUKE]` warhead
damages every in-radius object with `Strength>0`, so `fire_superweapon` also
runs an SW-only building-splash loop that mirrors the unit falloff exactly
(round(`CellSpread`) radius, linear 1.0→`PercentAtMax` scale, `damage_vs`
truncation, enemies-only) and destroys in-radius enemy structures via
`kill_building`.

The sidebar SW UI draws a `NukeIcon` cameo plus a `gclock2.shp` recharge
clock (`gframe = round(charge_fraction·54)`) with a flashing-green READY
tint, art-guarded and drawn only when house 0 has a granted SW — so no
existing screenshot changes.

Byte-identity discipline follows the same strategy every prior phase used:
`Unit` and `world_hash` are **untouched** (strategy (a) — the SW timer/ready/
type live in a separate per-house record and are never read by the hash);
the combat edit is linkage-only (`apply_spread_damage`'s body and its
`try_fire` call are unchanged); the eleven legacy gates were re-verified
byte-identical both at the first additive CB-4 commit (Task 3) and again at
this finalize. Calibration authority: `docs/re/superweapon.md` (superweapon
data model + `[SuperWeaponTypes]` id map, recharge-timer lifecycle,
target/fire, the `[NukePayload]`/`[NUKE]` fired effect + building-splash
RE-GATE, sidebar UI art, determinism/no-RNG).

Deviations we take for CB-4 (adjudicated divergences from the recovered
`superweapon.md` ground truth — not bugs, do not "fix" in review; mirrors the
hunt doc's own "Deviations we take", adjudicated at the Task 1 RE-GATE):
- **`apply_spread_damage` promoted to external linkage (combat.cpp behavior
  unchanged)** — the nuke REUSES the exact CB-2 area-damage applicator;
  because it lived in `combat.cpp`'s anonymous namespace it was moved to the
  `ra2engine` namespace + declared in `combat.hpp` so `world.cpp` can call
  it. Linkage-only: the function body + its existing caller (`try_fire`) are
  byte-for-byte unchanged and all twelve gate outputs are bit-identical.
  This is the one departure from the spec's "`combat.cpp` literally
  untouched" wording — the alternative (reimplementing the falloff loop) is
  the "new damage system" §5/§8 forbid [`superweapon.md` §2; spec §5/§8].
- **Instant area-damage detonation; missile flight deferred** — the effect
  applies at the target cell the frame `fire_superweapon` is called (the
  established instant/simplified-delivery precedent); the
  `GiantNukeUp`/`GiantNukeDown` missile flight, `DetonationAltitude`, and the
  `NUKEANIM` mushroom cloud are carry-forwards [`superweapon.md` §5; spec
  §Out].
- **Enemies-only splash** — the reused applicator's
  `w.house == attacker.house` skip spares the firing house; the real nuke
  damages friendlies too. The enemies-only splash is kept (a documented
  divergence); friendly-fire splash is a carry-forward [`superweapon.md` §5;
  spec §Out].
- **Building splash (RE-GATE CONFIRMED)** — the hunt confirmed the `[NUKE]`
  warhead damages every in-radius object with `Strength>0` (buildings
  included; `WallAbsoluteDestroyer`/`Wood`), so `fire_superweapon` runs an
  SW-only building-splash loop mirroring the unit falloff exactly
  (round(`CellSpread`) radius, linear 1.0→`PercentAtMax` scale, `damage_vs`
  truncation, enemies-only, `kill_building` on hp<=0). Reached only by the
  SW call, so the gates stay byte-identical; friendly-fire on buildings is
  the same documented enemies-only divergence as the unit splash
  [`superweapon.md` §5.3; spec §Out].
- **`RadLevel=500` lingering radiation dropped** — CB-4 applies the one-shot
  `[NUKE]` splash only; the radiation field / over-time damage is out
  [`superweapon.md` §2; spec §Out].
- **SW granted directly, not via a Nuke Silo** — CB-4 grants the SW to a
  house directly (like CB-1/2/3 spawned their exemplars directly); wiring
  "build the Nuke Silo → unlock the SW" through the prerequisite/production
  system is a carry-forward [spec §Out].
- **Only the `MultiMissile`/Nuke `Type` implemented** — the full SW `Type`
  enum (Iron Curtain, Chronosphere, Lightning Storm, Psychic Dominator, …) is
  RE-documented and mapped (`SuperweaponType` + `superweapon_type_from_id`)
  so each deferred kind starts from a mapped point; CB-4 ships only the
  area-damage detonation effect [`superweapon.md` §4; spec §Out].
- **AI does not use superweapons; the AI-3 inert SW-charge conditions stay
  inert** — `AIDefendAgainst=yes` / the AI SuperDefense system stays inert;
  the `ai-trigger.md` ct=5/ct=6 IronCurtainCharged/ChronoSphereCharged
  conditions are NOT wired to this framework in CB-4 (CB-4 grants only the
  Nuke, which neither names) — a carry-forward [spec §Out].
- **SW state is LIVE but excluded from `world_hash`** — the SW timer/ready/
  type is hashed only by `superweapon_hash` (strategy (a)); `world_hash` and
  `Unit` are untouched; no existing demo grants a SW, so no existing hash
  changes [`superweapon.md` §5; spec §8].

## CB-5 — Tech-Tree / Prerequisites

The final content-breadth phase completes the **tech-tree / prerequisite
progression**: buildability gated by the house's owned buildings through the
real RA2 prerequisite system — including the group tokens (`POWER`/`FACTORY`/
`BARRACKS`/`RADAR`/`TECH`) that any building in the category satisfies — with a
**tech building (Battle Lab `GATECH`)** unlocking an advanced unit (**Prism Tank
`SREF`**, `Prerequisite=GAWEAP,GATECH`, `TechLevel=8`). The buildability
predicate (`prerequisites_met`/`can_start`/`can_start_unit` in
`engine/production.cpp`) already existed and already handled group tokens from
the Unit Production phase — CB-5 REUSES it verbatim (predicate bodies untouched)
and only (a) full-parses the player unit roster's real `Prerequisite=` via
`rules.prerequisite(id)`, (b) appends `GATECH` to the building roster + `SREF` to
the player unit roster (both art-guarded), (c) extends the sidebar cameo
disabled-shade to unit tabs via `can_start_unit` (render-only), and (d) adds the
`--demo-techtree` gate.

Byte-identity discipline (this phase touches the production/buildability path, so
the strategy is load-bearing): the player-unit full-parse resolves the real
prereqs — `[E1]=GAPILE`, `[GGI]=GAPILE`, `[MTNK]=GAWEAP`, `[FV]=GAWEAP`, all
single plain building tokens (no group token) — **identically** to the retired
hardcoded tokens, and `--demo-prod` bypasses `can_start_unit` via a direct
`start_unit` call anyway, so `prod_hash` is inert to player-unit data; the
**Soviet AI unit roster is kept UNCHANGED** (the `APOC→NAWEAP` proxy — full-
parsing it would make APOC require the unsatisfiable `NATECH` and move
`ai_team_hash`); `GATECH` is an Allied `GA*` id absent from the Soviet AI catalog
(`make_ai_config` names `NA*`), so `ai_base_hash` is safe; the roster grows
`18→19` (content-only — the `--demo-base` `roster=` witness grows but the
instance-based `base_hash` is unchanged); `world_hash` is untouched and the new
tech-tree state is hashed only by `techtree_hash`. All twelve legacy gates were
re-verified byte-identical at every buildability-touching commit and again at
this finalize. Calibration authority: `docs/re/techtree.md` (the prereq
mechanism `FUN_00505180` + `[General] Prerequisite<Group>` membership + the
recovered `GATECH`/`SREF`/player-unit values; no RNG).

Deviations we take for CB-5 (adjudicated divergences from the recovered
`techtree.md` ground truth — not bugs, do not "fix" in review):
- **`GATECH` placed DIRECTLY in the demo** — `GATECH`'s own prerequisite is
  radar-gated (`GAWEAP,RADAR,GACNST`), but `--demo-techtree` places it via the
  `place_player_building`/`spawn_base` base-build path, which bypasses the
  buildability predicate (the same path `--demo-base`/`--demo-prod` use), so the
  demo base needs no radar. The `SREF` proof depends only on `GATECH` being
  OWNED so the `TECH`/specific-building prereq is satisfied, not on how it was
  placed [`techtree.md` §4].
- **Art is art-guarded, not required** — `GATECH`/`SREF` load through the normal
  art path (which may resolve via the NewTheater generic-art munge); on this
  install both resolve. If an install were missing the Allied SHP set, `GATECH`
  would render as the placeholder (buildings are never dropped from the roster on
  art failure) and `--demo-techtree` would synthesize an art-independent `SREF`
  probe def — the gate proves buildability art-independent either way
  [`techtree.md` §5; spec §6].
- **Prereq gating only** — engineer/spy CAPTURE (building-ownership transfer),
  building UPGRADES (`PowersUp`/`Upgrades=`), neutral tech-structure capture,
  `BuildLimit`/one-of restrictions, and negative (`Prerequisite.Negative`)
  prereqs are RE-documented but carry-forwards [spec §Out].
- **AI does not consume the new tech unit** — the Soviet AI roster stays on the
  proxy; wiring the AI to build `GATECH`/`SREF` is a carry-forward [spec §Out].

**This completes the content-breadth arc** (CB-1 Transports → CB-2 Aircraft →
CB-3 Naval → CB-4 Superweapons → CB-5 Tech-Tree). Thirteen permanent
byte-identity gates guarded the engine at the close of that arc.

## Engineer / Spy Capture

The first content-mechanic phase after the content-breadth arc adds the two RA2
"a unit enters an enemy/neutral building" mechanics — **engineer capture**
(building-ownership transfer) and **spy infiltration** (per-building effects) —
plus the spy's **disguise** and its **dog detection**. Calibrated from
`gamemd.exe` + `rulesmd.ini` (`docs/re/capture.md`); the hunt overturned two
pre-hunt assumptions before any code (see the RE-GATE below).

**Ownership transfer (Approach A).** Ownership in this engine is per-frame
derived — `power_totals` and `prerequisites_met` scan `buildings_` filtered by
`b.house` on every call — so `World::capture_building` just flips `b.house`
(matching `BuildingClass::ChangeOwner` `FUN_004481ab`) and power / buildability /
the player sidebar follow for free. The only extra fix-ups are the things
`ChangeOwner` does beyond the flag: move a superweapon grant
(`transfer_superweapon`, via the inert `Building::grants_superweapon` bool), AI
base-node resync on BOTH sides (the AI-1 standing-building↔node mechanism), a
primary-factory flag reset, and `ProduceCashStartup` granted only when captured
**from the neutral house**.

**Engineer.** `ENGINEER` captures an enemy/neutral building at ANY health (no
threshold — `EngineerCaptureLevel` is absent in YR-1.001 → ctor default 1.0; the
Ares-era `EngineerDamage`/`EngineerAlwaysCaptureTech` keys do not exist in this
build) and is always consumed; entering your own damaged building repairs it to
full. The gesture is **deferred** — the engineer paths to a footprint-adjacent
cell and the act fires on arrival in the firewalled `step_capture` (the
`order_enter_transport` pattern), not instantly on click.

**Neutral house + tech structures.** A neutral owner (house id 2; per-house
arrays widened to 3 with index 2 inert) holds capturable tech structures. The
`CAOILD` oil derrick pays its owner `ProduceCashAmount=20` every
`ProduceCashDelay=100` frames (plus the one-time `ProduceCashStartup=1000` on
capture-from-neutral) via the firewalled `step_tech_structures`. `CATHOSP`/
`CAMACH`/`CAPOWR`/`CAAIRP` are recovered but their passive effects are minimal/
deferred (`CAPOWR`'s +200 power arrives free via `power_totals` once owned).

**Spy infiltration** (`BuildingClass::Infiltrate` `FUN_004571e0`, early-returns
if the spy already owns it; also deferred to arrival): refinery → steal
`SpyMoneyStealPercent=0.5` × the victim's credits; power plant → `SpyPowerBlackout
=1000`-frame blackout (applied by the pure `apply_blackout` wrapper at the power
read-sites — `power_totals` / `production.cpp` bodies stay byte-for-byte
untouched); superweapon → recharge-timer reset (reuses CB-4's lifecycle); factory
→ the SPY's own house gets a produced-veteran flag; radar / Battle Lab → reveal
flags, **inert [RE-FALLBACK]** (no shroud in the engine yet).

**Minimal veterancy.** `UnitCombat`/`Unit` gain a `veteran_rank` (default 0 →
byte-identical). A unit produced by a house holding the produced-veteran flag
spawns rank 1 with the recovered multipliers **spawn-baked** into `w_damage`/`hp`
(rulesmd `[General]` `VeteranCombat=1.1` firepower, `VeteranArmor=1.5` armor;
rank 1, `VeteranCap=2`) — `combat.cpp` is literally untouched, so the combat/tank
gates stay bit-identical.

**Disguise / detection.** A spy carries its side-default `PermaDisguise`
(`AlliedDisguise=E1` / `SovietDisguise=E2`) and renders as that enemy infantry in
the disguised house's palette (render-only). Per the RE-GATE, a spy has
`PermaDisguise=yes` so it **never** drops its disguise by firing or being attacked
(reveal-on-fire gates on `CanDisguise && !PermaDisguise`) — `commit_attack_target`
is untouched. The ONLY thing that defeats the disguise is a `DetectDisguise` unit
(the dogs `[ADOG]/[DOG]/[YDOG]`) within range, in the firewalled detection scan.
A still-disguised spy is not auto-acquired (an inert `|| disguised` clause on the
`combat.cpp` auto-acquire scan, byte-identical when no disguise exists); a dog
reveal restores targetability automatically.

Byte-identity discipline: a `capture_any_` firewall (every new step pass
early-returns until the first capture/infiltrate/disguise), `veteran_rank=0`
default, all new `Unit`/`Building` fields + per-house arrays inert and excluded
from `world_hash`; `production.cpp` and `combat.cpp` bodies untouched. The 13
prior gates were re-verified bit-identical at every ownership-touching commit and
again at finalize; the 14th gate `--demo-capture` (`capture_hash=58b87b5bc63f7046`)
hashes only the new state.

RE-GATE (`docs/re/capture.md`, calibrate-first save #9) corrected two pre-hunt
assumptions before code: (1) engineer captures at ANY health (no damage-only
threshold — an Ares-era invention), and (2) the spy is PermaDisguise-EXEMPT from
reveal-on-fire (my design had it revealing on attack; the real spy stays
disguised, dog-only), plus pinned the veteran multipliers (×1.1/×1.5, not the
×1.25 fallback).

Deviations we take for Capture (adjudicated divergences from recovered
`capture.md` ground truth — not bugs):
- **Power blackout via `apply_blackout` wrapper, not by editing `power_totals`** —
  keeps `production.cpp` byte-for-byte untouched; identity when not blacked out
  [`capture.md` §B2; spec §8].
- **Veteran multipliers spawn-baked in `world.cpp`, `combat.cpp` untouched** —
  a rank-1 unit spawns with scaled firepower + HP; observably equivalent for a
  minimal veteran; keeps combat 78 / tank 144 bit-identical [`capture.md` §B5].
- **Radar / Battle-Lab reveal inert** — recognized but no-op; real reveal needs a
  shroud phase [`capture.md` §B6; spec §Out].
- **Neutral tech structures demo-spawned, not map-parsed** — `mapsmd03` isn't
  parsed for pre-placed tech buildings; the gate spawns a `CAOILD` [`capture.md`
  §D2; spec §Out].
- **`--demo-capture` is pure-state / art-independent** — the gate loads only
  GI/CONS art, so `ENGINEER`/`SPY`/`DOG`/`CAOILD` render as placeholders (their
  SHPs exist in the install but the demo's minimal asset load doesn't pull them);
  the hash folds sim state only, like `--demo-superweapon` [RE-FALLBACK].
- **Carry-forwards:** engineer/spy CAPTURE is done; still deferred are MakeupKit
  click-to-re-disguise, mirage disguise + fake-blink RNG, full veterancy
  (kills-based promotion / elite / chevrons), real radar/shroud reveal,
  map-parsed neutral roster + neutral AI, garrisonable buildings, and AI use of
  engineers/spies [spec §Out].

## Shroud / Fog-of-War

The Shroud phase draws the veil back over the map: a per-house, three-state
visibility model — **SHROUDED** (never seen, drawn black), **FOGGED** (explored
but not currently in sight, drawn dim), **VISIBLE** (in a unit's current sight,
drawn full) — carried on two independent per-house grids, `explored_[3]` and
`visible_[3]`. Both are **empty until enabled** (the `water_passable_`
precedent from CB-3): `World::enable_shroud(true)` is opt-in, allocates the
grids, and sets the `shroud_any_` firewall flag; no pre-existing demo ever
calls it, so every prior gate sees the grids unallocated and the visibility
step short-circuited.

Reveal is **near-circular**: `reveal_disc`/`reveal_source` mark every cell whose
`dx²+dy² <= sight²` around a source, where `sight` is the unit/building's
`Sight=` clamped to 10 (`rules.sight`); a building reveals from **each of its
footprint cells**, so a multi-cell base lights a fatter disc than any single
unit. Fog does **not** regrow: `step_visibility` recomputes `visible_` from
scratch each pass but only ever **adds** to `explored_` (it is sticky), matching
the shipped `FogOfWar=no` / `ShroudGrow=no` defaults — once you've seen a cell it
stays lit (dim) forever, and cells drop back only from VISIBLE to FOGGED, never
to SHROUDED.

`World::step_visibility` is firewalled the same way every prior phase's new pass
is: it runs **after `step_units`** (so movement is already applied) and
**before combat**, and its first statement is `if (!shroud_any_) return;`. It
rebuilds each enabled house's `visible_` grid from its live units/buildings,
folds the newly-seen cells into `explored_`, and stamps every unit's
`seen_by_mask` — a per-`Unit` bitmask of which houses currently see it.

Targeting consults that mask **inertly**: `Unit::seen_by_mask` defaults to
`0xFF` (seen by everyone), and combat's auto-acquire scan gained **one extra
clause** at `combat.cpp:362` skipping a target the scanning house cannot see —
byte-identical when shroud is off (mask stays `0xFF`), and structurally the twin
of the Capture phase's `|| disguised` clause. `combat.cpp`'s scan arithmetic is
otherwise untouched, so the combat=78 / tank=144 log gates stay exact.

Render adds a per-cell overlay — **black** over SHROUDED, a translucent
**dim** `shade_rect` over FOGGED, **full** over VISIBLE — consulted from the
local player's (house 0) grids, and **hides enemy mobile objects** sitting in
house 0's shroud/fog (a unit you can't see isn't drawn). The radar minimap is
greenfield: it paints only what house 0 has explored, replacing the CB-era
static radar frame. (Real play now draws shroud/fog with SHROUD.SHP/FOG.SHP
and an isometric radar -- see "Presentation in real play".) All render/minimap work is cosmetic and folds into no gate
hash.

The spy/radar reveal path resolves the Capture phase's INERT reveal item: the
local house owning a **SpySat**-type building (`grants_map_reveal`) triggers
`reveal_all_house` — `explored_` filled to the whole grid — and **latches**
(`map_reveal_[3]`), so a subsequent re-shroud can't take that house's map back.
A spy infiltrating an enemy **radar** (`InfiltratePurpose::RadarLab`, the real
Capture-phase enum name) calls `reshroud_house(victim)`, clearing the victim's
`explored_` map to 0 — **unless** the victim is SpySat-latched, in which case
the sabotage is a no-op.

Byte-identity discipline follows the established strategy: `world_hash` is
**untouched** (the new `Unit`/`Building`/`World` fields are appended after the
hashed set and never read by it — they are hashed only by the new
`shroud_hash`); every new field is inert by default and excluded from
`world_hash`; the `shroud_any_` firewall keeps `step_visibility` a no-op until a
house enables shroud. The fourteen legacy gates were re-verified bit-identical
at every shroud-touching commit and again at this finalize, and the fifteenth
gate `--demo-shroud` (`shroud_hash=d302852aa9517661`) hashes only the new
visibility state. Calibration authority: `docs/re/shroud.md` (three-state model,
Sight reveal math, fog-no-regrow, targeting consult, spy/SpySat reveal,
deviations).

Deviations we take for Shroud (adjudicated divergences from the recovered
`shroud.md` ground truth — not bugs, do not "fix" in review):
- **Per-house visibility grids, not the original's local-player cell bits** —
  RA2 stores cell shroud for the local player only (`DAT_00a82cb4`); we
  generalize to per-house `explored_[3]`/`visible_[3]` (faithful per-MP-client,
  required for the enemy-side targeting consult) [`shroud.md` §1/D1; spec
  §Architecture A].
- **Targeting consult via a per-unit `seen_by_mask`, not a per-cell lookup in
  the scan** — the enemy auto-acquire skip reads a `Unit` bitmask stamped by
  `step_visibility`, so `step_combat`'s signature is unchanged and the clause is
  an inert `0xFF` default mirroring `disguised` [`shroud.md` §6; spec
  §Architecture E].
- **Fog dim factor = translucent-black `shade_rect` (alpha pinned at the
  RE-GATE), cosmetic/unhashed** — approximates `BlendedFog=yes`; render is not
  part of any gate hash [`shroud.md` §5 RE-GATE item 1].
- **SpySat reveal driven by a `grants_map_reveal` type flag** — if `GASPYSAT`
  is absent from the roster the mechanism ships inert until such a building
  exists (RE-GATE item 2) [`shroud.md` §7A].
- **GapGenerator, FogOfWar regrow, height sight bonus, cloak/`SensorsSight`
  detection, AI fog-aware scouting, and `AllyReveal` map-sharing are
  carry-forwards** — documented, not implemented this phase [`shroud.md`
  §4/§3/§6/§8; spec §Scope/non-goals].
- **Shroud state is LIVE but excluded from `world_hash`** — the new
  `Unit`/`Building`/`World` fields are hashed only by `shroud_hash`; no existing
  demo enables shroud, so no existing hash changes [`shroud.md` §1; spec §Data
  model].

## Multi-house / Alliances

The Multi-house phase generalizes the engine from its hardwired two-combatants-
plus-neutral world to N houses bound into alliance-teams. All new state lives on
a per-house `HouseRec` table (`houses_`, a `std::vector<HouseRec>` carrying each
house's `Side`, MP color index, `alliance_team`, and combatant flag) gated by a
single `houses_configured_` firewall: until a caller populates the table, every
side/color/enmity/victory query **falls back to today's derivation** (house 0 =
Allied, house 1 = Soviet, house 2 = Neutral; the alliance sentinel resolves to
each house's own id, i.e. free-for-all), so no pre-existing demo sees any change.
`enum Side { Allied, Soviet, Yuri, Neutral }` names the four roster classes;
`house_side`/`house_color`/`house_count`/`house_team`/`house_combatant`/
`set_house` are the accessors.

Enmity is now a **pairwise predicate** rather than an inlined `house != self`:
`is_enemy(a,b)` and `are_allied(a,b)` read the table. With the firewall off
`is_enemy(a,b) == (a != b)` — byte-identical to the old test. A **neutral /
non-combatant house is never an enemy** and a house is **always its own ally**;
two combatant houses are allied iff they share an `alliance_team` (the sentinel
value → own id, so distinct-id houses are FFA enemies by default). Every inlined
enmity site now routes through the predicate: combat auto-acquire (`valid_target`,
signature widened to take the scanning house + world), nuke/superweapon splash
target selection, repair-vs-attack and capture own-vs-enemy decisions, disguise
detection, and the AI's quarry/attack selection. **Area/splash damage still hurts
allies** — `apply_spread_damage` keeps its `w.house == attacker.house` own-house-
only spare rather than routing to `is_enemy`, faithful to RA2 friendly-fire.

**AllyReveal** shares the shroud **map** (not targeting): at the end of
`step_visibility` each configured house's `explored_`/`visible_` grids are
snapshotted and OR-merged with its allies' (deterministic, order-independent),
firewalled behind `houses_configured_` && `shroud_any_` so the shroud gate stays
byte-identical. The auto-acquire `seen_by_mask` still reflects each house's **own**
sight, so shared vision reveals the map without silently widening targeting.

**Last-team-standing victory** is greenfield and firewalled behind
`victory_enabled_`: `step_victory` marks a combatant house `house_defeated` when
it has no units and no buildings (ShortGame = last-**building**), never marks
neutral/non-combatant houses, and resolves `winning_team` to the sole surviving
alliance-team (sentinel until one remains). Neutral never wins or loses. With
victory disabled (every legacy demo) the pass is a no-op.

**Per-house side and color** drive spawns and rendering: `house_side` selects the
spawned Building/unit **TYPES** (`spawn_base` builds a Soviet or Allied ConYard +
base per side), and `house_color` + `pal_for_color` map a house's MP color index
to one of the **8 multiplayer colors** (HSV spread → RGB remap of the unit
palette), entered for render via `house_palette`. The defaults reproduce exactly
today's world — house 1 = Soviet/red, everyone else Allied/blue — so the spawned
`Building{type}` is byte-identical and `world_hash` is unaffected.

Six formerly-`[3]` per-house arrays were vectorized to `std::vector` sized
`house_count()` (`blackout_frames_`, `produced_veteran_`, `explored_`, `visible_`,
`map_reveal_`, `house_credits_`), with the `<3` accessor guards, the `take_income`
bound, and the `step_visibility` per-house loops widened accordingly. The one
exception is the per-unit `seen_by_mask` stamp, which stays a `uint8_t` capped at
**8 combatants** (RA2 `MaxPlayers=8`); neutral/non-combatant houses are excluded
from that targeting bitmask.

Byte-identity discipline is unchanged from prior phases: `world_hash` is
**untouched** (all new house/victory/vectorized state is appended after the hashed
set and read only by the new `multihouse_hash`); every new field is inert by
default; the `houses_configured_` and `victory_enabled_` firewalls keep the new
behavior off until a caller opts in; and the fifteen legacy gates were re-verified
bit-identical at every multi-house-touching commit and again at this finalize.
Calibration authority: `docs/re/multihouse.md`.

Deviations we take for Multi-house / Alliances (adjudicated divergences from the
recovered `multihouse.md` ground truth — not bugs, do not "fix" in review):
- **Enmity via a pairwise `is_enemy`/`are_allied` predicate, not `house != self_house`** —
  every inlined enmity site routes through the table-driven relationship; the
  default (`houses_configured_` off) returns `a != b`, exactly today [`multihouse.md`
  §3/§4; spec §Architecture C].
- **Area/splash damage still hurts allies (only the attacker's own house is spared)** —
  `apply_spread_damage` keeps its `w.house == attacker.house` own-house spare
  rather than routing to `is_enemy`, faithful to RA2 friendly splash [`multihouse.md`
  §4].
- **Neutral / non-combatant is a THIRD relationship (never an enemy, never auto-acquired, but still capturable)** —
  the engine gained the Neutral state it lacked; `step_capture` captures neutral
  (not-allied) buildings while repairing own/allied [`multihouse.md` §3].
- **AllyReveal shares the shroud MAP, not targeting** — allied `explored_`/`visible_`
  grids OR-merge for render/reveal, but `seen_by_mask` (auto-acquire) still
  reflects each house's OWN sight [`multihouse.md` §3; spec §Architecture D].
- **Last-team-standing victory is greenfield + firewalled** — `step_victory` runs
  only when `victory_enabled_`; ShortGame=last-building; neutral/non-combatant
  never win or lose [`multihouse.md` §5; spec §Architecture E].
- **`seen_by_mask` stays `uint8_t` (8-combatant cap == MaxPlayers)** — not widened;
  neutral/non-combatant houses are excluded from the targeting bitmask
  [`multihouse.md` §1; spec §Architecture G].
- **Country/lobby default colors are the derived house1=Soviet/red else Allied/blue
  defaults, not per-country lobby picks** — the calibrated defaults reproduce
  today's world; per-country color assignment from a lobby is a carry-forward
  [`multihouse.md` §2 RE-GATE].
- **`draw_unit`/`draw_building` carry a per-house color forward via `house_palette`,
  not a per-object recolor** — rendering remaps the unit palette by the house's MP
  color index (cosmetic, unhashed); no gate hash depends on it [`multihouse.md`
  §2].
- **Per-country bonuses, the Yuri roster, in-game alliance-request UI, shared
  control/economy, AI diplomacy, AIDifficulty handicaps, and multi-human input are
  carry-forwards** — documented, not implemented this phase [`multihouse.md`
  §1/§3/§7; spec §Scope/non-goals].
- **Multi-house state is LIVE but excluded from `world_hash`** — `houses_`/
  predicates/victory/vectorized arrays are hashed only by `multihouse_hash`; no
  existing demo configures alliances, so no existing hash changes [`multihouse.md`
  §1; spec §Data model].

## Skirmish Shell + Main Menu

The Skirmish Shell phase turns the hardwired scenario into a config-driven local
skirmish, fronted by a real main menu. The data model is two plain structs
(`engine/skirmish.hpp`): a `SkirmishSlot` — `{ World::Side side; int color_ix;
std::string country; int alliance_team; bool combatant; bool is_ai; AiDifficulty
diff; }`, whose first five fields map 1:1 into the LIVE `World::HouseRec` while
`is_ai`/`diff` drive `enable_ai_house` — and a `SkirmishConfig` carrying the
shipped `[MultiplayerDialogSettings]` defaults recovered in Task 1:
`starting_cash = 10000` (slider range 5000–10000), `unit_count = 10` (0–10, the
extra starting units beyond the MCV), `tech_level = 10` (1–10), `shroud = true`
(always-on this phase), `crates = false`, `superweapons = true`,
`short_game = true`, and a single static `map_id = "mapsmd03"`; AI difficulty is
the recovered 0/1/2 = Easy/Normal/Difficult index. `alliance_team = -1` is the
FFA sentinel (a slot allied only with itself). GameSpeed is deliberately NOT a
field (see Deviations).

The launch path splits along the same pure-core / asset-wrapper line the
Multi-house and Economy phases used. The pure, host-testable core lives in
`engine/skirmish.cpp`: `configure_skirmish_houses(World&, const SkirmishConfig&)`
applies the config's house table + toggles + per-house credit seed to a `World`
using ONLY existing `World` APIs — `set_house(i, HouseRec{side, color_ix,
country, alliance_team, combatant})` per slot, then `enable_shroud` /
`enable_ally_reveal` / `enable_victory(true, short_game)`, a
`grant_credits_for_test(i, starting_cash)` seed per combatant, and
`enable_ai_house(i, {difficulty})` per AI slot (a minimal config that makes
`ai_enabled(i)==true`); it never spawns anything, so it is fully unit-tested on a
hand-built `World`. `default_skirmish_config(const Rules&)` returns the 1v1
quick-start (slot 0 Allied human, slot 1 Soviet AI Normal, slot 2 Neutral
non-combatant; `starting_cash = rules.mp_money()`) reproducing exactly the world
the old `run_interactive` built. The asset-dependent `launch_skirmish(cfg,
Assets&, UnitAssets&, BuildingAssets&, Rules&)` stays in `main.cpp` (it needs the
main-local asset statics): it `build_world`s, calls `configure_skirmish_houses`,
then per-slot spawns a base (`spawn_base`) + `unit_count` starting units (gates
only — real play now starts from an MCV, see "Real play: MCV, skirmish start,
game over"),
re-calls `enable_ai_house` with the full roster-backed `make_ai_config` per AI
slot (last-call-wins overwrite of the minimal config), and grants each combatant
its superweapon — the Allied Construction Yard being the only base type spawned
this phase.

The front-end is a small `Scene` framework: an abstract `Scene`
(`handle`/`update`/`render`/`next`/`wants_quit`) driven by `run_scene_driver`,
with three subclasses — `MainMenuScene`, `SkirmishSetupScene`, and `GameScene`
(a verbatim lift of the old `run_interactive` body with the scenario replaced by
`launch_skirmish`). The menu → setup → game flow: the main menu shows Skirmish
and Quit live with the other entries (Campaign, Multiplayer, Load, Options,
Movies, Credits) drawn as greyed stubs; Skirmish opens the setup screen (now the
faithful dialog-0x102 page, see "Skirmish setup page (dialog 0x102)"); Start
builds a `SkirmishConfig` and hands it to `GameScene`, which calls
`launch_skirmish`. The framework is wired only at the two NON-gate `main()`
seams — the gate-dispatch ladder (all seventeen `--demo-*` arms) runs and returns
FIRST, so no gate ever constructs a `Scene`. Text is drawn by the in-house
`engine/rectfont` module: an A–Z (+0–9) 5×7 glyph table extending the existing
`kDigits`/`draw_number` bit-per-pixel technique, exposed as `glyph_rows(char)`
and `draw_text(...)`, with no external font dependency (honoring
`iso_renderer.hpp`'s "REUSE only, no new font systems").

Byte-identity discipline is the same as every prior phase: `world_hash` is
**untouched** — the launcher adds no `World` field, and scenes/font/config are a
pure UI/config layer none of the seventeen hashes read. No gate configures a
skirmish through the menu; the new `--demo-skirmish` gate drives a FIXED
`SkirmishConfig` straight through `launch_skirmish` (not the interactive scenes),
so the sixteen legacy gates were re-verified byte-identical at Tasks 3/4/6 and
again at this finalize, and `--demo-skirmish` pins `skirmish_hash=fde3f587e747f765`.
Calibration authority: `docs/re/skirmish.md` (lobby settings + ranges §1, menu
structure §2, config→launch architecture §3, GameSpeed/pacing §4, UI/rect-font
§UI).

Deviations we take for the Skirmish Shell (adjudicated divergences from the
recovered `skirmish.md` ground truth — not bugs, do not "fix" in review):
- **Config-driven launch replaces the hardcoded scenario** — `SkirmishConfig` →
  `launch_skirmish` → `World`; `run_interactive` is now a thin
  `default_skirmish_config()` → `GameScene` wrapper; the launcher only calls
  existing `World` APIs and is purely additive [`skirmish.md` §3; spec
  §Architecture B].
- **GameSpeed is presentation-only** — our fixed deterministic `dt = 1/15` step
  is unchanged; `GameSpeed` is NOT stored in `SkirmishConfig` and cannot alter
  `world_hash`; the setup screen omits it this phase; `GameSpeedBias=1.6`
  movement reconciliation is a carry-forward (movement gates already calibrated)
  [`skirmish.md` §4].
- **The 5 DESUPPORTED lobby keys are hidden** — `Bases`/`ShadowGrow`/
  `CaptureTheFlag`/`HarvesterTruce`/`MultiEngineer` parse in the rules schema but
  were cut in retail YR; the setup screen never surfaces them [`skirmish.md` §1].
- **Single static map (`mapsmd03`)** — no MIX map enumeration path exists; a map
  browser + `.mpr`/`.map`/`.yrm` parsing is a carry-forward; the map's
  start-waypoint count caps the slot count [`skirmish.md` §1/§3].
- **Minimal in-house rect-font (no font system)** — `engine/rectfont` extends the
  existing `kDigits` 5×7 technique with an A–Z glyph table + `draw_text`; no
  external font dependency (honors `iso_renderer.hpp:227` "REUSE only, no new font
  systems") [`skirmish.md` §UI].
- **Allied Construction Yard only at spawn** — `launch_skirmish` spawns the Allied
  ConYard base type for every slot this phase; a per-country/side base-unit spawn
  table is a carry-forward [`skirmish.md` §3].
- **The menu is interactive, not hashed** — `MainMenuScene`/`SkirmishSetupScene`/
  `GameScene` are UI, exercised by boot-alive + build + screenshots, NOT by a
  determinism gate; only the config→launch path (`--demo-skirmish`) is hashed
  [`skirmish.md` §2].
- **Skirmish-first menu; the rest are stubs** — Campaign/Multiplayer-net/Load/
  Options/Movies/Credits are greyed menu stubs; Skirmish is a local MP match with
  AI-filled slots and no netcode [`skirmish.md` §2].
- **Carry-forwards (documented, not implemented):** map browser / MIX
  enumeration; campaign; multiplayer networking; load/save; options persistence;
  map editor; movies/credits; GameSpeed pacing control + GameSpeedBias movement
  reconciliation; full country roster / Yuri side; multi-human input; in-game
  alliance-request UI; per-house on-screen color rendering; skirmish-preset
  save/load [`skirmish.md` §1/§2/§3/§4; spec §Scope/non-goals].
- **Skirmish state is additive + excluded from `world_hash`** —
  `SkirmishConfig`/scenes/font are UI/config-layer; the launcher adds no `World`
  field; no gate configures a skirmish, so no existing hash changes
  [`skirmish.md` §1; spec §Data model].

## Broaden Unit/Building Roster

The Broaden Roster phase widens the playable cast by 16 calibrated content types,
all art-present on this install, with stats/prereqs recovered verbatim from
`docs/re/roster.md §2`. Twelve are combat units held in a new `kRosterUnits`
table — 7 Soviet (`DRON`, `V3`, `ZEP`, `SHK`, `DESO`, `APOC`, `TERROR`) and 5
Allied (`GHOST`, `CLEG`, `JUMPJET`, `MGTK`, `ORCA`) — and four are buildings
appended to `kRoster` at indices `[20..23]`: `NANRCT` (Soviet Nuclear Reactor),
`ATESLA` (Allied Prism Tower, art `GAPRIS`), `NASAM` (Allied Patriot SAM) and
`GAAIRC` (Allied Airforce Command HQ). `APOC` is promoted from a hardcoded
`kSoviet` proxy token into a real content type (dual prereq `NAWEAP,NATECH`)
while the old `kSoviet` proxy entry is left byte-for-byte alone.

Everything resolves through the existing generic paths — the four buildings via
`load_building_assets`' resolve loop over the grown `kRoster`, the twelve units
via `resolve_combat_unit` reading `ra2assets::Rules` — and both honor each type's
`Image=` art override (e.g. `APOC`→`MTNK`, `TERROR`→`TRST`, `MGTK`→`RTNK`,
`ORCA`→`FALC`, `GHOST`→`SEAL`, `ATESLA`→`GAPRIS`), so a type renders under its
declared art without any per-type code. `spawn_base` gains a trailing
default-false `bool allied_full`: when set (and non-Soviet) it lays down a
symmetric Allied full base (ConYard + power + barracks + war factory + airforce
command); every determinism gate leaves it false, and only the `--demo-roster`
gate passes `true`. Wiring it into interactive skirmish (so an Allied human/AI
player gets the full base) is a **deferred carry-forward (Task 5)** — doing so via
the shared `launch_skirmish` spawn would drift `skirmish_hash`, so it needs an
`allied_full` field on `SkirmishConfig` (default false for the gate); until then
interactive Allied houses still get ConYard-only.

The change is strictly APPEND-ONLY: `kRoster` grew `[20]`→`[24]` (every prior
index preserved), `kRosterUnits` is a brand-new table, and `kUnits`/`kSoviet`
(the player/AI-proxy tables) are untouched. `world_hash` is **untouched** and
folds instances by index, so all seventeen legacy gate hashes stay bit-identical
— only the `roster=N` content-count witness re-pins from `20` to `24` on
`--demo-base` and `--demo-techtree` (hashes `5273b5e71ca009e1` /
`e73adae61ff6f71f` unchanged). Each type's unique active ability (see Deviations)
is content-only-deferred this phase: stats, weapon, armor, prereq, art,
buildability and basic combat land now; the special mechanic is a future
mini-phase. The new eighteenth determinism gate `--demo-roster` pins
`roster_hash=333cd43d8b5ece6a` (INSTALL-SPECIFIC — see the gate table note; now
`9c57c02eb36e9872` since gap A's `Image=` fix, see the gate table).

Deviations we take for Broaden Roster (adjudicated divergences from the recovered
`roster.md` ground truth — not bugs, do not "fix" in review):
- **Append-only roster growth** — 4 buildings at `kRoster[20..23]` + a new
  `kRosterUnits` table; every existing index preserved; `world_hash` folds
  instances by index, so all 17 gate hashes stay bit-identical and only the
  `roster=N` witness re-pins 20→24 [`roster.md` §2/§4].
- **`kUnits`/`kSoviet` untouched; APOC promoted separately** — the 12 units live
  in `kRosterUnits`, not the player/AI-proxy tables; APOC becomes a real content
  type (dual prereq `NAWEAP,NATECH`) while the `kSoviet` APOC proxy is left
  byte-for-byte alone, so `--demo-prod`/ai-base/ai-team hashes are unperturbed.
  NATECH-gated live buildability of APOC is a carry-forward (NATECH not rostered
  this phase) [`roster.md` §2/§5].
- **Symmetric Allied base behind a default-false flag** — `spawn_base` gains a
  trailing `bool allied_full`; only the `--demo-roster` gate passes it `true`
  (every other caller, incl. `launch_skirmish` and the interactive base, leaves it
  false). Wiring it into interactive skirmish is a deferred carry-forward (Task 5,
  see below) [`roster.md` §3].
- **Each type's unique active ability is content-only-deferred** — stats/weapon/
  armor/prereq/art/buildability + basic combat land now; the special mechanic is
  a future mini-phase:

```
  DRON burrow-into-vehicle parasite; V3 rocket-projectile flight (V3ROCKET);
  ZEP bomb-drop + slow jumpjet flight; SHK power-up-Tesla-coil + EMP;
  DESO radiation field + deploy-fire; APOC dual weapon (AA MammothTusk);
  TERROR suicide detonation; GHOST C4 demolition + amphibious;
  CLEG teleport-erase (ChronoBeam) + target freeze; JUMPJET jumpjet air movement
  (basic flight allowed this phase iff the Jumpjet CLSID resolves, T1 item 2);
  MGTK tree-disguise when still; ORCA transport + dock/rearm (needs GAAIRC dock).
```

- **Carry-forwards (documented, not implemented):** all the above unique
  abilities; ORCA/JUMPJET true jumpjet/air flight (basic flight only when the
  CLSID resolves); NATECH-gated APOC buildability; the `ATESLA` render (art
  `GAPRIS` absent on this install, so no instance is placed — the gate still
  asserts it resolves as a building); GASPYSAT + the shroud-T7 reveal-ordering
  fix; GAYARD naval yard + DEST/AEGIS naval pair; ROBO/GAROBO (art absent); Yuri
  faction (NATBNK/YA*, Side::Yuri); TTNK/GTGCAN drop-in extras (bounded batch of
  16); per-country bonuses; the remaining ~70 cast members; multi-human input;
  the **Task-5 skirmish Allied-base wiring is DEFERRED** — the `allied_full`
  mechanism exists on `spawn_base` but is not wired into interactive skirmish
  (would drift `skirmish_hash`; needs a `SkirmishConfig.allied_full` field
  defaulted false for the gate), so interactive Allied houses are ConYard-only for
  now; all prior-phase carry-forwards [`roster.md` §5; spec §Scope/non-goals].

## Faithful Main Menu

The Faithful Main Menu phase reskins the front-end to render from the real RA2
shell art actually present on this install, driven by three new, self-contained
decoders. It is **interactive/visual only and strictly additive** — no
determinism gate renders the menu, no gate exercises the new decoders, and all
eighteen gate hashes plus `world_hash` are bit-identical to the prior phase.

> **True-design rebuild (supersedes the earlier composition below).** The menu
> was subsequently rebuilt to the faithful RA2 shell recovered from `game.exe`
> (dialog resource `0xE2` and its shell asset loaders; see
> `docs/re/mainmenu_true.md`). This **supersedes the earlier `mnscrnl` +
> `cache.mix/palette.pal` olive-remapped, right-aligned composition** described
> in the paragraphs that follow — the shell now composes on a 640×480 logical
> canvas (scaled + letterboxed into the window) from the recovered assets,
> palettes, and dialog-template pixel rects:
>
> - **Backdrop:** `neutral.mix/mnscrns.shp` (472×448) under `cache.mix/shell.pal`
>   — the dark-**red** US-East-Coast radar map with the beveled CRT frame baked
>   in — at (0,0,472,448), with `neutral.mix/lwscrns.shp` as the 32 px bottom bar.
> - **Full metal sidebar chrome** (168 px column): `sdtp.shp` frame 0 (the
>   console with the mini-monitor bezel) at (472,0,168,199) under `shell.pal`;
>   `sdbtm.shp` V-console at (472,415,168,65); and a per-button `sdbtnbkgd.shp`
>   metal slice under **`cache.mix/shell2.pal`** (the correct chrome palette;
>   `shell.pal` on these pieces was a red-speckle source — `mainmenu_true.md`
>   §7).
> - **Animated WARNING monitor:** `neutral.mix/sdwrnanm.shp` (92×53, **91
>   frames**) under `shell2.pal`, drawn into the dialog-`0xE2` monitor hole
>   (control 0x71C, rect 508,47,92,53) on top of `sdtp` — the red **WARNING**
>   screen + rising graph + launch flash. It **animates live** (~1.1 s loop):
>   `ShellScene::update(dt)` advances the frame index each tick.
> - **Buttons:** the real sidebar buttons are **`neutral.mix/sdbtnanm.shp`**
>   (156×42, 17 frames) under **`neutral.mix/sdbtnanm.pal`**, blitted **1:1
>   (not stretched)** — frame **2** normal / **3** orange-glow hover / **4**
>   yellow-hot pressed / **10** empty socket for button-less rows. Recovered
>   from the owner-draw painter (game.exe 0x5F0400): geometry is x=484
>   (=screenRight−156), y=199+42·row, pitch 42, one 156×42 control per row —
>   this **supersedes** the earlier (wrong) stretched `mnbttn.shp` (which is in
>   fact the *popup-dialog* button). Pressed labels shift (+2,+5) px.
> - **Gold labels** rendered with the authentic `12metfnt.fnt` Westwood font
>   (`draw_fnt_text`), centered per button, plus the "Main Menu" header
>   (477,2,162,16) and the version line (477,461).
> - **Left "CRT screen" is a VQA that is absent everywhere in this install** —
>   the real menu loops `Ra2ts_s.vqa` / `Ra2ts_l.vqa` (radar sweep + Statue of
>   Liberty + logo) in a movie control, but an exhaustive magic-byte scan of
>   every mix (base/YR/expansion/maps/nested — `--scanmagic`) found **zero
>   VQAs**; `MOVIES01/02.MIX` are 4-byte `"MYTH"` dummies. So **no fake logo is
>   drawn** (cropping `logo.pcx` — the tanks title art — is not the menu
>   design); the `mnscrns` red radar map is the faithful stand-in, matching what
>   retail RA2 shows when the movie is missing. A supplied `ra2ts_*.vqa` can be
>   blitted into the CRT rect for the exact original look.
> - **Multi-page tree** (one reusable `draw_shell` per page, `ShellScene`):
>   **Main** → **Single Player** (New Campaign / Load Saved Game greyed,
>   **Skirmish** live → `SkirmishSetupScene`, Back) / **Movies & Credits**
>   (greyed stubs + Back) / **Options** (Sound / Keyboard / Display greyed +
>   Back) / Internet + Network greyed / **Exit Game**. Render rects and hit-test
>   rects derive from a **single** `layout()` in logical space (click rects ==
>   draw rects).
>
> Byte-identity is unchanged by this rebuild: only `main.cpp`'s menu region and
> new menu helpers changed — no `--demo-*`/`simulate_*`/`run_demo_*`/`world_hash`
> or any decoder was touched — so all eighteen gate hashes remain bit-identical
> (re-verified one-per-run at finalize), the ra2assets suite passes 482/482 in
> both install and no-install modes, and the arm64 APK still links the menu
> symbols (`draw_shell` / `ShellScene` / `draw_fnt_text` / `load_menu_art`).
> True-design carry-forwards: the exact Options CSF labels; Campaign and Load
> pages as greyed stubs; VQA (`ra2ts_*`) left-screen playback (needs a VQA
> decoder + the movie file, absent here); the `sdwrntmp` page-transition
> power-on and header shimmer; and the 800×600 ("large" `*L` asset) button
> redistribution — all deferred.

### Bink CRT animation

The CRT "screen" is **not** a static radar stand-in — it plays the **real RA2
menu animation** (logo + rotating red radar sweep + Statue of Liberty scene),
decoded live by a **from-scratch Bink1 video decoder** in `ra2assets`
(`include/ra2assets/bink.hpp` + `src/bink.cpp`, new self-contained TU with
`tests/test_bink.cpp`). This **corrects the earlier "left CRT VQA is absent /
static" assumption above**: the animation was never a VQA — it is a **Bink
video** (`language.mix/ra2ts_s.bik`), present in the install all along, and it
plays because the shell tries the `.bik` name **before** the (stubbed) `.vqa`.
The `--scanmagic` VQA sweep found nothing precisely because the asset is Bink,
not FORM/WVQA.

The decoder is a complete Bink1 (`BIKi`, revision `'i'`) implementation built
against `docs/re/bink.md` (spec + tables) and `docs/re/bink_proof_decoder.py`
(byte-exact reference):

- **Container** (`BinkFile`) — 44-byte LE header (`BIKi` magic, dimensions,
  frame count, fps ratio, video flags, audio-track count) + the
  `num_frames+1` u32 absolute frame-offset index (bit0 = keyframe flag, masked
  off for the real offset).
- **LSB-first bit reader** + **Huffman** (`read_tree` on-the-fly trees) and the
  full **bundle** protocol (block-types, colors, patterns, motion X/Y, intra/
  inter DC, run) with per-row RLE refills.
- **All block types** (SKIP, SCALED, MOTION, RUN, RESIDUE, INTRA, FILL, INTER,
  PATTERN, RAW) + the **Bink integer IDCT** + intra/inter dequant + motion
  compensation from the previous frame, honoring every exact-arithmetic trap
  (32-bit wrapping `MUL`/dequant with `>>11`, no-clamp pixel adds mod 256,
  scan-vs-raster quant indexing, quantizer read after coefficients, DC
  dequantized, 32-bit align after each plane, the skipped leading per-packet
  u32, plane order **Y, V, U**, COLORS bytes used raw).
- **BT.601 limited-range** YUV420→RGBA (`c=298·(Y−16)`, `R=c+409·e`,
  `G=c−100·d−208·e`, `B=c+516·d`, RGB clamped, nearest-neighbour chroma
  upsample, `A=255`).

**`language.mix/ra2ts_s.bik`** is **472×450, 431 frames @ 15 fps**. The decoder
is validated against an ffmpeg oracle: **431/431 frames byte-exact** vs
`ffmpeg -pix_fmt yuv420p` (dev-time direct `.yuv` memcmp + committed FNV pins on
frames 0 and 130 for CI without the reference), and **RGBA within ±2 per
channel** vs the `bikframes/f%04d.png` reference (swscale dithers the
limited-range conversion; `bink.md` notes the ±2). The 13 bink test cases pass
in both install and no-install modes (real-asset cases skip cleanly when
`RA2_INSTALL_DIR` is unset).

**Playback** (`engine/main.cpp` menu region, interactive-only): `load_menu_art`
opens `language.mix`, reads `ra2ts_s.bik`, and holds a heap
`BinkFile`+`BinkVideoDecoder` in the menu-art store. `ShellScene::update(dt)`
advances a 15 fps accumulator, decoding **one frame per step** and rewinding to
the keyframe (frame 0) at the end to loop. `draw_shell` uploads the current
RGBA frame into **one reused streaming `SDL_Texture`** (re-uploaded only when the
frame index changes) and blits it 1:1 into the CRT rect (logical `0,0,472,448`)
over `mnscrns`, under the sidebar console. A null guard falls back to the static
`mnscrns` backdrop on a partial install (no `.bik`). This is strictly additive —
`draw_shell` gained a defaulted trailing parameter; no `--demo-*` /
`simulate_*` / `run_demo_*` / `world_hash` or any existing decoder was touched,
so all eighteen gate hashes stay bit-identical (re-verified one-per-run at
finalize) and the arm64 APK links the Bink symbols (`BinkFile` /
`BinkVideoDecoder`).

**Carry-forwards:** Bink **audio** tracks are not parsed (RA2's `ra2ts_s.bik`
has 0 audio tracks, so none needed here); **pause-on-focus-loss** and
**per-page CRT restart** (the animation currently free-runs across page
transitions); and **runtime decode performance** — decoding is currently
one-frame-per-tick on the main thread (fine at 472×450/15 fps, but a background
decode thread or frame cache would be the scale-up path).

**Three decoder additions** (each ADDITIVE + host-tested; none touched by any
gate):
- **PCX** (`ra2assets::decode_pcx`, `src/pcx.cpp`) — 8-bit RLE PCX with the
  769-byte trailer palette, for the `logo.pcx` / `title.pcx` shell art and the
  loadscreens (`mainmenu.md` §6). The trailer palette is written full-8-bit
  directly (not the 6-bit `Palette::load` VGA expansion).
- **Westwood FNT** (`ra2assets::load_fnt` / `Font`, `src/fnt.cpp`) — variable-
  width bitmap font (per-glyph width table + palettized rows), for authentic
  bevelled menu labels; consumed by `draw_fnt_text` (`mainmenu.md` §3). This
  font is now **LIVE** (`fnt_ok`/`font=y`): the decoder was corrected to the
  real Westwood on-disk format — glyphs are **8bpp** (one byte per pixel, a
  low-nibble intensity ramp; advance = glyph `width`), **not** 4bpp-packed
  (2px/byte, 2·width advance), and per-glyph pixel offsets are read
  **relative to the header `image_ofs`**, which was the one true bug. The
  correction was anchored by a byte-oracle over the glyph table and a legible
  render (`menufix.md` CORRECTION). Menu labels now paint in the authentic
  gold bevelled face instead of the previous rect-font fallback.
- **Stereo IMA-ADPCM** (the `channels==2` branch in `ra2assets::load_wav_ima`,
  `src/wav.cpp`) — MS-IMA interleaved L/R decode for the looping menu theme
  `grinder.wav` (RIFF/IMA, 2ch, 22050 Hz — `mainmenu.md` §4). The `channels==1`
  mono path is **byte-unchanged** (mono synthetic + real-asset pins pass
  unchanged); the mixer sink downmixes the stereo track to mono.

**Faithful menu composition** (real assets, all present per `mainmenu.md` §2):
`mnscrnl.shp` (632×568, uncompressed single-frame SHP) as the uniformly-scaled,
letterboxed East-Coast situation-map background; `mnbttn.shp` (126×25, 3
state-frames up/down/hover) as the right-side vertical button stack; `logo.pcx`
/ `title.pcx`; `metal12.fnt` labels; `bkgdlg.shp` + `sidec01/sidec02` dialog
chrome on the skirmish-setup screen; the looping `grinder.wav` menu track. The
background palette is pinned to **`cache.mix/palette.pal`** (the decisive
render-sweep winner in `mainmenu.md` §7 item 1 — the only palette that renders a
coherent East-Coast relief map; `fsscrn.pal` is rejected as the dark red/blue
speckle). The button stack and logo are **right-aligned** in a fixed column
mirroring the shell layout, and the backdrop's harsh red land-shading is
corrected: `palette.pal` indices **230–239** (the theater highlight/remap band,
formerly a pure-red ramp that tinted the whole map) are **remapped to a muted
olive ramp at menu-background render time**, yielding the military-map olive
relief the original shows.

**Byte-identity discipline:** the menu is interactive-only, the three decoders
live in new/guarded code paths, and mono-WAV decode is byte-unchanged, so
`world_hash` and all eighteen gate hashes are bit-identical and **no new gate was
added** — verification is a `--menu-shot` screenshot (a hidden-window single-
frame capture of `MainMenuScene` + `SkirmishSetupScene` to PNG) plus the
ra2assets host tests, not a determinism pin. The subsequent fidelity upgrade
(authentic 8bpp FNT live, olive-remapped backdrop, right-aligned composition)
touches only these interactive render/decoder paths — re-verified at finalize,
all eighteen gate hashes plus the ra2assets suite (both install and no-install
modes) remain bit-identical, and the arm64 APK still links the FNT decoder.

**Deviations we take** (adjudicated against `mainmenu.md`, not bugs):
- **Approximate button/logo coordinates** — the exact per-button pixel rects are
  hard-coded in `gamemd.exe` (not data-driven), so the right-aligned layout is
  approximated from the SHP sizes (backdrop fit ≈1.267 into 1280×720, buttons
  ~200×40 at x≈980); a Ghidra pass on the shell dialog init to recover
  pixel-exact rects is a carry-forward (`mainmenu.md` §5 / §7 item 2).
- **Backdrop olive remap** — `palette.pal`'s 230–239 band is the theater
  highlight/remap slots, on disk a pure-red ramp; rendering it verbatim tinted
  the whole land red, so those indices are remapped to a muted olive ramp at
  menu-background render, matching the original's military-map relief. The
  pixel-exact shell shading is likely embedded in the exe, so a Ghidra pass to
  refine the 230–239 band remains a carry-forward (`mainmenu.md` §7 item 1
  caveat).
- **FNT glyph-decode gap** — the authentic 8bpp font is live; only where an
  individual glyph fails to resolve does text fall back to the engine's
  rect-font rather than dropping characters (`mainmenu.md` §3 / §6).
- **Letterbox composition** — the backdrop is scaled uniformly and centered with
  side bars filled by the palette background color (olive land-base, idx 254),
  mirroring the shell's border fill (`mainmenu.md` §7 item 2).
- **Mono sink downmix** — the stereo menu track is downmixed to the mixer's mono
  22050 Hz sink; menu audio idles the device when muted (`--menu-shot` / no-audio
  runs open no device) (`mainmenu.md` §4).

**Carry-forwards (documented, not implemented):** the two remaining fidelity
refinements are **pixel-exact button rects via a Ghidra shell-dialog-init pass**
and **exact shell-palette shading (the 230–239 band) recovered from the exe**;
also Campaign /
Network / Internet / Load / Options / Sneak Peeks as greyed stubs (LIVE =
Skirmish + Quit, `mainmenu.md` §7 item 3); the Allied/Soviet loadscreens
(`aloadlg.pcx` / `rloadlg.pcx`) between menu and game; the intro movie; animated
menu elements; the Yuri faction; and all prior-phase carry-forwards.

## Skirmish setup page (dialog 0x102)

The placeholder lobby (blue boxes over the in-game options panel, one global
AI count/difficulty, Team and Tech Level rows) is replaced by a reproduction of
RA2's Skirmish page, calibrated to `docs/gap/07-skirmish-page.md` (layout spec
of dialog `0x102`/`0x6B`, CSF keys, `game.exe` 1.006 behaviour). Files:

- `engine/skirmish_lobby.{hpp,cpp}` (pure, unit-tested in
  `tests/test_skirmish_lobby.cpp`): `LobbyState` = name + 8 fixed rows
  `{ai, country, colour}` + Game Speed/Credits/Unit Count + the five checkboxes;
  RA2.INI `[Skirmish]` (`GameMode ScenIndex GameSpeed Credits UnitCount
  ShortGame SuperWeaponsAllowed BuildOffAlly MCVRepacks CratesAppear
  Slot01..07=type,country,color`, types None 1 / Brutal 4 / Medium 5 / Easy 6)
  and `[MultiPlayer] Handle/Color/ColorEx/Side/SideEx` with the original's
  spellings (`ScenFile=` is an extra clone key for the map round trip);
  exclusive colours; rows past the map's max closed; validation
  (`TXT_SCENARIO_TOO_SMALL`, `TXT_NEED_AT_LEAST_TWO_PLAYERS`); Random
  resolution as `FUN_00672BB0` (country uniform over the playable countries,
  colour uniform over the free ones); `lobby_to_config` builds the
  `SkirmishConfig` (FFA, rules TechLevel, hidden Neutral house, lobby colour ->
  house colour table, per-row level -> `AiDifficulty` with class = 2 - choice).
- `engine/shell_widgets.inc`: the shared shell control skins (`ShellPainter`:
  trackbar/checkbox/combobox/edit; `ShellList`: dropped list/listbox). The
  Options page draws through it too (pixel-identical `--menu-shot`).
- `engine/skirmish_page.inc`: the scene. `draw_shell` with SDTP frame 1 and no
  WARNING monitor/Bink; Start Game / Customize Battle / Back in sidebar rows
  1/2/5; the `[PreviewPack]` thumbnail with its baked start markers in the
  monitor well, "Battle" + map name under it; STT tooltips on the status line;
  flags from `local.mix`; Customize Battle (Game Type + Game Map lists, Use
  Map, Random Map greyed, Cancel). Start validates, saves RA2.INI (also
  `[Options] GameSpeed`, as `0x6829F0` writes OptionsClass+0) and launches;
  Back/Esc save and return to Single Player.

Consumed in game: Game Speed drives the logic tick rate for a lobby game
(`cfg.game_speed`); MCV Repacks sets `World::set_mcv_redeploys` (sold ConYard
undeploys); Superweapons off also removes `SuperWeapon=` structures from the
human sidebar and the AI plan (the `[AI] BuildTech` tech centres stay);
Build Off Ally is shown greyed and persisted untouched (RA2-classic).
`MPBattleMD.ini` only sets `[MultiplayerDialogSettings] AlliesAllowed=yes`, so
the Battle mode override has nothing to apply. Gates never enter the lobby;
`--demo-skirmish` / `--demo-maps` hashes are unchanged.

`--play` targets on the page: `button:<label>` (+ `start` / `back`),
`edit:name`, `combo:ai<N>` / `combo:country<N>` / `combo:color<N>`, while a list
is open `pick:<label>` / `item:<i>` / `list:up` / `list:down`, `check:<short|
mcv|crates|superweapons>`, `slider:<speed|credits|units>=<value>`, and on
Customize Battle `map:<file>`. `scripts/play/sk-lobby.play` drives every
control and asserts each choice through the game probe.

## Map Browser / Multi-Map Skirmish

Until this phase the engine loaded exactly **one** map, hardcoded as
`mapsmd03.mix` entry `0x133a0cb2`. The hunt (`docs/re/maps.md`) established
what that map actually is: **`sov07tmd.map`, a Yuri's Revenge Soviet campaign
mission** — so every "skirmish" ever played in this clone was fought on a
mission map, with both sides fanned out from one derived cell because the
maps' own spawn data was never parsed. This phase replaces all of that with the
real multiplayer map set.

**Enumeration is manifest-driven, not a directory scan.** The original does not
list "every map in the archive"; it reads a `.pkt` manifest
(`langmd.mix/missionsmd.pkt`) whose `[MultiMaps]` index names each map and whose
per-map section carries `Description`, `GameMode`, `MinPlayers`/`MaxPlayers` and
`CD`. `ra2assets::PktFile` decodes it. `ra2engine::build_map_catalog` then
applies the recovered pipeline in order: parse → **existence filter** → **mode
filter** → **CSF display names** → stable sort. The existence filter is
mandatory, not defensive: RA2's own `missions.pkt` lists **58 maps that do not
exist**, so a manifest-only list would offer unloadable maps. On this install
the YR manifest yields **161 active entries, all present**, of which **91 are
`GameMode=standard`** — the Battle list, the only mode that admits AI players,
and therefore the skirmish list. Names come from the game's string table
(`ra2md.csf`, 5,208 labels): `Description=` is a *label*, not a name, and no
stock map sets `[Basic] Name=` (0 of 281), so an unresolved label falls back to
the raw base name.

**Player starts are real.** `[Waypoints]` keys **0–7** are the multiplayer
start positions, packed as `x = v % 1000`, `y = v / 1000` in the same cell space
as the terrain records — triple-sourced (OpenRA, XCC, and this install's own
`[Header]` preview waypoints, which are an exact affine transform of the decoded
cells *only* under this decode order; the key count equals
`NumberStartingPoints` on **267/267** multiplayer maps). `MapFile` exposes them
as new fields, and `launch_skirmish` places combatant *k* at
`start_waypoints()[k]` when `SkirmishConfig::use_map_starts` is set. The map's
waypoints also overlay the AI's synthetic waypoint ring, retiring that
`[RE-FALLBACK]` for every index a map actually defines.

**All five multiplayer theaters render.** `ra2assets::TheaterChain` carries each
theater's tile mix, art mix, control INI, iso/theater/unit palettes and tile
extension. `load_assets` resolves any theater through that table instead of
throwing on everything but temperate, and takes a `MapSelection` that defaults
to the historical entry — which is what keeps every gate path unchanged.

Two RE-GATE findings (`maps.md` §9) overturned assumptions that would have
produced a broken loader:

- **The `*md` tile archives are overlays, not tile sets.** `isotemmd.mix`,
  `isosnomd.mix`, `isourbmd.mix`, `isoubnmd.mix` and `isodesmd.mix` each resolve
  **0** of a real map's tiles and lack even `Clear01` at index 0. The **base**
  archive is authoritative; opening only the `md` variant — the natural reading
  of the naming convention — renders an empty map.
- **Only SNOW has dedicated building art** (letter `'A'`, *arctic*). There is no
  `gtcnst`/`gucnst`/`gncnst`/`gdcnst`/`glcnst` anywhere; every other theater
  resolves through the generic `'G'` fallback the engine already implements, so
  the per-theater art letter is nearly irrelevant.

Chains proven by constructing each theater and resolving every distinct tile of
a real map of that theater: TEMPERATE **270/270**, SNOW **164/164**, URBAN
**116/116**, DESERT **103/103**, NEWURBAN **154/155**. LUNAR is excluded — zero
multiplayer maps use it. Note also that SNOW/URBAN take their control INI from
the *expansion* archive (`snowmd.ini`/`urbanmd.ini` in `localmd.mix`) while their
tiles come from the base one — an asymmetry detected by filename suffix, so the
expansion archive is opened only when a theater needs it and the default path's
startup cost is unchanged.

The skirmish setup screen now carries a **map selector** over the 91-map list
(arrows step by 1, the row's outer edges by 10) showing each map's CSF name and
player count. Because the in-house rect-font is A–Z plus 0–9 only, punctuation
would silently vanish, so the position is spelled `1 OF 91` and the step arrows
are drawn as triangles.

The **19th determinism gate**, `--demo-maps`, drives the whole path headlessly:
catalog → load `xmp31s2.map` ("May Day (2)", SNOW 70x83) **by name** → real
starts → 300 fixed frames, run once rendered and once headless and byte-compared.
It pins `maps_hash=ab66922dab1b24f6` and carries six wedge guards (empty
catalog, unlisted gate map, wrong theater/size/starts, starts not used, too few
houses, sim did not advance) so it can never pass vacuously. Its
`starts_used=yes` witness is the load-bearing check that houses really sit on
the map's waypoints rather than silently falling back to the old fan-out.
INSTALL-SPECIFIC, like `--demo-roster`: the hash folds the catalog.

All eighteen prior gates were re-verified **bit-identical** after every
behaviour-touching commit and again at finalize; the ra2assets suite is
510/510; the arm64 APK builds with the new symbols linked.

**Deviations we take** (adjudicated against `maps.md`, not bugs):
- **Slot → start binding is deterministic ascending** (combatant *k* →
  `start_waypoints()[k]`). The original randomizes the assignment; a
  byte-identity gate needs determinism [`maps.md` §9.5, `[RE-FALLBACK]`].
- **`LocalSize` is ignored** — the full `[Map] Size` rect renders; clipping the
  camera to the playable sub-rect is a later pass [`maps.md` §9.8].
- **The meaning of waypoints > 7** was never determined (in-map script
  references were ruled out); they are used only as positional nodes, which
  cannot contradict anything the hunt pinned [`maps.md` §4.3].
- **The default map is never repointed** — gates keep loading the campaign map
  they always did; map choice is a parameter that defaults to current behavior
  [`maps.md` §7 rule 2].

**Carry-forwards (documented, not implemented):** `[PreviewPack]` lobby
thumbnails (decodable — Format5 + **LZO**, *not* LCW as folklore holds: LCW
fails 267/267 while LZO decodes 267/267 — but a second UI subsystem for no
simulation value); **pre-placed `[Units]`/`[Infantry]`/`[Structures]`/
`[Terrain]`**, which 85 of 267 multiplayer maps carry — *blocked on
calibration*, since the object record's field tail is only PROBABLE from a
third-party reader and no owner-string → house mapping has been recovered;
chain-aware building art (SNOW should use `snow.mix`'s dedicated `'A'` art
rather than the generic set); the RA2 `MULTI.MIX` map set; non-`standard` game
modes; LUNAR; loose `.mpr`/`.yrm` files; and a scrollable/filterable browser
with previews in place of the stepper.


## Building Repair & Sell

The sidebar's two remaining inert buttons now work, exactly as
`docs/re/repair-sell.md` recovered them (§D is the RE-GATE adjudication and its
24-row table measured against this install is the authority for every number
below).

**Repair is a per-building toggle on a GLOBAL 14-frame phase.** The cadence is
`frame % trunc(RepairRate × 900) == 0`, and `.016 × 900 = 14.4` truncates to
**14, not 15**. It is an *absolute* phase, not a per-building timer: every
repairing building of every house steps on the same frames, and a newly armed
one waits 0–13 frames for its first step. Each step heals a flat **+8 HP**
(`RepairStep`) and charges
`max(1, trunc((Cost / (Strength / RepairStep)) × RepairPercent))` with **both
divisions integer**: GACNST 3/step (375 to full), GAWEAP 2/step (250), GAPOWR
1/step (`Strength` is **750**, not 1000, so 94 steps to full). The naive
`Cost × RepairPercent × damage_fraction` form gives 450 for a ConYard, about
17% too much. Repair **auto-stops** at full health and **stops silently** when
the owner cannot afford the next step. The check runs *before* the spend, so
there is no partial step, no free repair and no EVA line. The flag is cleared,
so the player has to click the wrench again. `URepairStep`/`URepairPercent` do
not exist in this build.

**Selling is a mission, not an instant action.** The buildup animation plays
**mirrored** (`start + count − stage − 1`) over the same frame count, at the
same rate the building took to appear (the engine's existing buildup duration,
`image_frames × max(1, trunc(BuildupTime×900/image_frames)) / 15`. That is 30
frames for GACNST and 50 for GAREFN on this install). Credits land and
survivors spawn only on completion. Selling always calls `Repair(0)` first. A
type with no buildup art takes the instant-removal branch. A sold building does
not explode.

- **The refund is flat:** `trunc(trunc(Cost × m2 × m1) × RefundPercent)`,
  `RefundPercent=50%`, with **no HP scaling**. A building at 1 HP refunds the
  same as one at full health. `Soylent` overrides and bypasses `RefundPercent`,
  and that override matters here because **GAREFN/NAREFN set `Soylent=300`**, so
  a refinery refunds 300, not 1000. `RefundPercent` applies **only to
  human-controlled houses**. An AI house gets the unscaled value.
- **Survivors** = `clamp(refund / SurvivorDivisor[side], 1, 5)`, with divisors
  **500/250/750** and zero when `Crewed=no`. They are `[General] AlliedCrew=E1` /
  `SovietCrew=E2`, and they exit onto the building's own foundation cells. A
  survivor whose cell is blocked is deleted, not displaced. The 25% special-crew
  branch is cut: it is un-anchored and it is an RNG draw.
- **Refusals are exactly:** `Unsellable` (reachable: **CAOILD**), under
  construction, already selling, and C4 applied. **There is no
  last-Construction-Yard rule and no captured-building rule.** Both were searched
  for in the binary and neither exists. The gate asserts that the only
  full-health ConYard *is* sellable.

**The two buttons are mutually exclusive toggle cursor modes** (§C1): placement
wins, arming clears the selection, arming is refused when the player owns no
and right-click cancels exactly one thing per press in the recovered order
placement → Repair → Sell → superweapon (correction, fixB hunt: that chain `FUN_004AAC40` is the right-mouse-up handler; ESC is the Options hotkey and opens the options dialog). The mode is not one-shot. While a mode
placement → Repair → Sell → superweapon. The mode is not one-shot. While a mode
is armed, the button shows its pressed frame and the pointer is the real
`MOUSE.SHP` cursor (`cache.mix/mousepal.pal`). The cursor uses the wrench
150–169/170–189/190 or the dollar 129–138/139–148/149, at one cursor frame per 4
game frames. Those indices and the centre hotspot come from the binary's own
cursor table at `0x0082C038`. Which animated member means "over a valid target"
is PROBABLE (§D8). The interactive game now arms the full economy through
`launch_skirmish` (`wire_economy`, gap G-20); the old lookup-only firewall is gone. It also
bridges the player's wallet into the World pool while a repair runs.

The **20th determinism gate**, `--demo-repair`, pins
`repair_hash=30e49667c9c6e52d`.

**Carry-forwards (documented, not implemented):** (`UndeploysInto`/MCV
redeploy is now implemented by gap F: a human skirmish ConYard sold with
`MCVRedeploys=yes` packs into its MCV; campaign/AI keep the plain refund, §B6); AI auto-repair/auto-sell (eight unanchored
offsets plus a `Random(0,50)` draw that would perturb the AI gates, §D3); the
Service Depot docked-unit sell (`GAAIRC` sets `UnitReload=yes` but nothing can
dock, §D2); the mind-control sell veto (§D6); the captured-building ×2 survivor
divisor (capture records no captured flag yet); upgrade peel-off; unit selling;
garrison release; the 25% special crew; C4; the three unmodeled mode flags
(power toggle, planning, beacons); and the Soviet/Yuri sidebar button offsets.
`[RE-FALLBACK]` stands for the fate of in-progress production on sell (§D4): a
sold factory takes the existing building-death queue-cancel path.

## Options + Persistence

The main menu's **Options** button used to open a guessed page of three greyed
stubs (Sound / Keyboard / Display). The hunt (`docs/re/options.md`, game.exe
`FUN_00542460` → dialog **`0xD5`**, DLGPROC `0x00542590`) showed the real thing is
**one combined page** with four sections — Display, Game, UI, Audio — plus two
side buttons, **Keyboard** (dialog `0xA3`) and **Network** (the IPX page `0xD7`),
and **Main Menu**. There is no separate Sound or Display page on this path, and no
game-speed control (that lives only on the in-game page `0xBBB`).

**The page** is drawn from the dialog template's own rects (MapDialogRect 6x13 on
the 640x480 shell, letterboxed by the shared `ShellXform`): *Visual Details*
(0..1), *Set Game Resolution* (combobox), *Difficulty* (0..2), *Tooltips* /
*Target Lines* / *See Hidden Objects* checkboxes, *Scroll Rate* (0..6, shown
reversed: `pos = 6 - ScrollRate`), and *Music / Sound / Voice Volume* (0..10 with
a numeric readout). Controls use the shell skin engine's real art and colours:
`trakgrip.pcx` thumbs, `trofl/trofm/trofr.pcx` readout wells, `cue_i/cce_i.pcx`
checkbox lamps, red (255,0,0) frames, yellow text, dark-red when disabled (the
volume sliders are disabled without an audio device, exactly as
`EnableWindow(audio_present)`). The left screen shows the static MNSCRNS map — the
dialog has no CRT movie control. Sliders click-jump/drag with the skin proc's own
mapping; volume drags apply live; leaving via **Main Menu** commits the controls
(`FUN_00542280`: `DetailLevel = pos ? 2 : 0`, `ScrollRate = 6 - pos`,
`volume = pos * 0.1`) and saves.

**The settings file** is the original's `RA2.INI` `[Options]`/`[Video]`/`[Audio]`
keys with the `OptionsClass` constructor's defaults (`GameSpeed=3`,
`Difficulty=1`, `ScrollRate=3`, `DetailLevel=2`, `SoundVolume=0.7`,
`VoiceVolume=1.0`, `ScoreVolume=0.5`, ...), the loader's clamps and the Westwood
INIClass value rules (`$hex` / `...h` ints, first-letter booleans, `%f` doubles
with `%` scaling; written back as `%d` / `yes|no` / `%f` in the original's save
order). It lives at `SDL_GetPrefPath("ra2engine","RA2")/RA2.INI`
(`%APPDATA%\ra2engine\RA2\` on Windows, the app's internal files dir on Android)
— never the install dir. Saving is read-modify-write through a case- and
order-preserving `IniDoc`, so foreign sections and comments survive byte-for-byte
(`tests/test_settings.cpp` pins the round trip).

**What the settings drive:**
- **Volumes → three mixer buses**, as the original routes them: SoundVolume → the
  sound-effect pool, VoiceVolume → the EVA speech slot, ScoreVolume → music (the
  menu theme). Linear gain (`Mixer::set_bus_gain`, permille).
- **Scroll Rate → edge scroll.** The guessed constant 40 px/frame pan is replaced
  by the recovered algorithm: distance table `{448,384,320,256,192,128,64,32,16}`
  × `[General] ScrollMultiplier` (.07), index `8 - accel` floored at
  `ScrollRate + 1`, `accel` climbing once per 16 ms tick while scrolling — a slow
  start (1 px) accelerating to 13 px/tick at the default rate (26 at fastest, 2 at
  slowest).
- Everything else (Difficulty, Visual Details, Target Lines, Hidden Objects,
  Tooltips, the `[Video]` keys, ...) is persisted faithfully but has no consumer
  in the clone yet.

**Gates are untouched by construction:** the file is read only by `run_menu` (the
desktop default path and the Android entry); no gate calls it or constructs a
menu/game scene, `--menu-shot` renders with built-in defaults, and every mixer bus
defaults to identity gain with the pre-P8 arithmetic path preserved bit-for-bit.

**Deferred:** the Keyboard page (`0xA3`, Keyboard.ini `[Hotkey]` remapping UI -- the
bindings themselves ARE live since G-04, read from KeyboardMD.ini; see "Real-play controls") and the Network/IPX page are drawn greyed;
resolution switching (fixed-window/Android port — the combobox shows the current
mode, inert); the in-game options page and its GameSpeed slider; slider tick and
volume-preview sounds; the tooltip status line; page power-on transitions.


## Projectile Flight (real ballistics)

Until this phase every weapon was instant-hit: `combat.cpp::try_fire` applied
damage the frame the ROF timer expired. Calibrated against the `BulletClass`
hunt (`docs/re/projectile.md`) and its implement-phase RE-GATE addendum (§12),
non-`Inviso` weapons now fire a real projectile that flies on the recovered
integrator and only then calls the **unchanged** damage layer:

- **Ballistic** (`ROT==0`: `[Cannon]` Rhino/Grizzly/Apocalypse, `[FlakTProj]`
  Flak Track, `[Ballistic]` Destroyer). Launch speed is **not** `Speed=` — it is
  `ftol(sqrt(d·Gravity·1.2))` recomputed per shot (`FUN_00771E90` →
  `FUN_0048AAA0`), capped at half the distance, with **`Gravity=6` read from
  `[AudioVisual]`** (it does not exist in `[General]`). The launch angle comes
  from the **unwound** arcing solver (`FUN_0048A8E0`, §12.1: an exact quadratic
  in the horizontal speed squared, `θ = acos(√t/v)`, plus the wrapper's
  `d+1` derivative test that recovers downward shots and a flat 45° whenever the
  target is above the muzzle), quantised through the 16-bit binary angle like
  `Fire`. Each logic frame: `V.z −= 6; pos = ftol(pos + V)` (semi-implicit Euler,
  integer-lepton positions). A Rhino shell at max range flies ~16 frames.
- **Vertical** (`[BlimpBombP]`, the Kirov bomb): leaves at **1 lepton/frame**
  (§12.3), gains `Acceleration=1` per frame up to the weapon `Speed` (51),
  direction frozen — **~37 frames (~2.5 s) from cruise height**, the single
  largest error of the old instant model.
- **Termination** (§7 + §12.2, `LevelHeight=104` now CONFIRMED from its static
  initialiser): ground impact; arrival in the true target's cell below 208;
  collision with the first hostile unit in the cell within 127 leptons (snaps onto
  it unless `Inaccurate`); off-map removal without damage; out-of-speed; the
  `IsBlocked` −416 floor. `Explode` snaps within 32 leptons.
- **Detonation** reuses `apply_spread_damage` (CellSpread ≥ 1) or the extracted
  `deliver_unit_hit` / `deliver_building_hit` — the old `try_fire` tails moved
  verbatim. A target that moved away is a **miss**; a target that died mid-flight
  leaves the shell to detonate on its cell (§8).
- **Inviso weapons stay instant.** All infantry small arms are `[InvisibleLow]`,
  a one-frame teleport in the original (§6.3) — the old path is already faithful.

**Inaccuracy and the RNG decision.** `[FlakTProj]` scatter (§5.2,
`Random(0, BallisticScatter)·dist/Range` around a uniform angle) and the §5.1
`Random(BS/2, BS)` form are drawn from a **dedicated projectile RNG**: the
project's xorshift32 with a fixed seed, consumed only by scatter draws in launch
(ascending attacker) order, wrapped in an **exact** reproduction of
`Randomizer::Random(lo,hi)`'s range mapping (`FUN_0065C220`: swap, top-bit
mask, reject > range — inclusive and unbiased). The *distribution* is the
original's; the *stream* is ours (the original's single lockstep
`Scenario->Random` is unreachable). It is deliberately **not** shared with the
ore or AI streams, so firing a Flak Track can never perturb ore growth or AI
picks — each system's reproducibility stays independent.

**The firewall.** Combat sits on nearly every gate path, so the flight behaviour
is gated three ways: (1) `World::projectiles_enabled_` is opened only by
`World::enable_projectiles`, called by the interactive `GameScene` and
`--demo-projectile` and by nothing else; (2) `step_combat` gained a trailing
`std::vector<ProjectileLaunch>* launches = nullptr` — `World::step` passes it
only when enabled, and with `nullptr` every shot takes the pre-phase code
(the moved damage tails are statement-for-statement identical); (3)
`step_projectiles` is a pure insertion before `step_combat` that returns
immediately when disabled. Projectile state, events and RNG are not read by
`world_hash`, and the projectile draw list is empty in every existing demo.

**Gate:** `--demo-projectile` (`projectile_hash=44cdd50b67f276e9` since gap fix C; originally `1ff09b8a3d3eecdd`) runs five
scenarios, rendered then headless, and self-compares: *firewall* (no
`enable_projectiles` → the Rhino's hit lands the frame it fires), *arc* (launch
speed 90 == the §0.2 formula of the FLH muzzle→target distance, 13 flight
frames, apex 175 > muzzle 100, the 90 damage lands only at detonation), *miss*
(the Conscript walks off; the shell detonates harmlessly on its old cell),
*kirov* (speed 1 from z=610 = 750−140 FLH, 37 frames, the 2-cell splash hits
both Conscripts), *flak* (5 scattered `[FlakTProj]` shots, identical across
runs). The mid-flight PNG shows both Rhinos' shells at the top of their arcs.

**Deviations / cuts** (spec `docs/superpowers/specs/2026-10-06-projectile-design.md`):
- **Homing (`ROT>0`) stays instant** — `FUN_005B1B20` steering is only partly
  unwound and `MissileSpeedVar`/`MissileROTVar`/`Acceleration` defaults are not
  pinned; implementing it now would be guessing.
- `[FlakProj]` AA scatter (§5.3), the Inviso cliff/wall line trace, the lofted
  root (needs an obstacle LOS model), `Bouncy`/`Airburst`/`Cluster`/`Shrapnel`
  (no roster weapon sets them) — out of scope.
- The combat plane is flat (floor 0, §11.6); libm `acos`/`sin`/`cos` replace the
  game's trig tables; yaw is an exact direction vector (the 16-bit yaw
  quantisation is < 0.2 lepton at these ranges).
- The non-arcing pitch's sign toward a lower target and the unidentified
  `TechnoType+0x6A4` gate on the speed-1 launch are PROBABLE (§12.3).
- Projectile art (`120MM`/`ZBOMB` SHPs): drawn in real play by the G-83 FX
  layer (see "Presentation in real play"); the gates still draw rimmed dots.

## Map Triggers / Scripting (P9)

The campaign substrate. A map's own scripting — `[Triggers]`, `[Events]`,
`[Actions]`, `[Tags]`, `[CellTags]`, `[VariableNames]` (with `[TeamTypes]` for
team references) — is parsed and run with `gamemd.exe`'s semantics, recovered
in `docs/re/map-triggers.md` from the ReadINI functions, `TagClass::RaiseEvent`,
`TriggerClass::RegisterEvent`, `TEventClass::HasOccurred` (the 62-kind predicate
switch), `TActionClass::Execute` (the 146-way action switch) and the two raise
loops. This is a different system from the AI-3 `[AITriggerTypes]` engine.

**What the original does (and we now do):**
- **Two raise loops.** Every frame `LogicClass::AI` walks the *logic* tag list
  in order and per tag tries kinds `27/28/36/37` (only when a variable changed),
  `13`, `51`, then `14` (only when the mission timer just expired), stopping at
  the first raise that fires. Every **8th** frame each house walks its own tag
  list **last-to-first** raising kind `8` (AnyEvent). Tags join the lists at
  scenario start from their triggers' attach flags.
- **AND of events, latched only for repeating tags.** A trigger fires when all
  its events hold in one evaluation; only Persistent tags latch one-shot object
  events. Events are stored **reversed** (ReadINI prepends), which decides which
  timer event wins when a trigger has two.
- **Persistence.** Volatile fires once and **kills its whole tag** (sibling
  triggers in the chain die with it); SemiPersistant fires when the last attached
  object's events fire; Persistent fires every time and re-arms its timers.
- **Timers.** ElapsedTime = `Value·15` frames, RandomDelay =
  `(Value/2 + rand[0,Value])·15`, ElapsedScenarioTime = `Value ≤ frame/15`.
  Timers re-arm on construction, on EnableTrigger (even if already enabled), on
  a Persistent fire, and **whenever a referenced global/local changes** — so a
  `LocalCleared + ElapsedTime` trigger restarts its countdown on every change.
- **Index spaces.** House values index the rules `[Countries]` list with the
  map's new countries appended (on `sov07tmd`, `_Lose` waits on `DestroyedAll 15`
  = `Player`); BuildingType values index rules `[BuildingTypes]`
  find-or-allocated (rulesmd lists `NAPSYA` twice, so the map's `300` is
  `YACNST`; `24`/`54` are `NAIRON`/`NAMISL`, matching the trigger names).

**Coverage, ranked by the 44 stock campaign maps** (4,341 triggers, 4,857
events, 13,871 actions; usage counts in parentheses):

| status | events | share | actions | share |
|---|---|---:|---|---:|
| **engine** — evaluated/executed in `MapTriggerSystem` | 13 (2022), 36 (551), 47 (342), 37 (298), 8 (103), 51 (59), 14 (8), 27/28 (4/4) | 69.8% | 53 (2706), 54 (1836), 56 (354), 57 (171), 12 (84), 3 (81), 74 (76), 2 (55), 1 (43), 27 (19), 23 (18), 28 (13), 0 (12), 24 (6), 22 (3), 75 (3), 68 (2), 25 (2) | 39.5% |
| **host** — resolved by the engine, answered/executed by the `TriggerHost` | 32 (121), 57 (116), 11 (105), 61 (67), 19 (37), 10 (29), 9 (16), 17 (15), 20 (13), 21 (12), 30 (12), 16 (11), 15 (9), 58 (7), 60 (7), 52 (3), 22 (1) | 12.0% | 4 (1208), 7 (941), 17 (615), 80 (425), 5 (403), 107 (84), 6 (66), 125 (40), 16 (11) | 27.3% |
| **API** — object/cell kinds, implemented behind `raise_tag`/`raise_cell`, not yet raised by the world | 1 (295), 48 (293), 4 (114), 6 (35), 44 (32), 33 (28), 29 (17), 31 (14), 7 (14), 59 (11), 26 (9), 25 (4), 41 (4), 40 (3), 39 (1), 43 (1), 2 (1) | 18.0% | — | — |
| **recorded** — presentation only (speech, sound, text, movie, camera, radar, input lock, flashes), logged with parameters | — | — | 99 (1289), 21 (570), 11 (451), 55 (372), 19 (310), 112 (84), 100 (79), 48 (76), 41 (68), 113 (58), 46 (51), 116 (49), 47 (46), 104 (45), 114 (37), 115 (34), 131 (30), 103 (19), 10 (6), 20 (4) | 26.5% |
| **unimplemented** — logged / evaluates false | 55 (4), 56 (4), 5 (1) | 0.2% | 14 (186), 108 (130), 63 (100), 38 (93), 36 (64), 32 (51), 101 (38), 9 (29), 81 (25), 37 (21), 60 (18), 88 (18), 135 (16), 121 (12), 40 (11), 111 (10), and 26 kinds with ≤ 9 uses each | 6.6% |

The gate host answers house events from the live `World` (owned buildings /
vehicles / infantry by house, per-BuildingType counts, credits, power ratio,
`Factory=` buildings, techno counts) and executes reinforcements (7/80/107),
reveal (16/17) and CreateBuilding (125); it does not track destroyed-object
counters (15/16) or last-built types (19–22), and records CreateTeam /
DestroyTeam / AllToHunt without executing them.

**The 20th determinism gate**, `--demo-triggers`, runs the default gate map
(`sov07tmd.map`, YR Soviet 7 — the map every gate already loads) **with its own
126 triggers** for 2,700 frames: the map's 10 houses configured from `[Houses]`,
its roster-known `[Structures]`/`[Units]`/`[Infantry]` placed, then the trigger
system ticked at the top of every frame. It fires 18 triggers (14 from the logic
loop, 4 from the house loop, 2 driven by a variable change — the crypt
LocalSet at frame 0 firing two `Lights Out` triggers through raise kind 27 at
frame 1), places TaskForce reinforcements at their team waypoints, and records
the speech/lock-input lines. Rendered and headless runs are byte-compared;
`trigger_hash=20347f4dfa462eb4`, `lines=76`. Wedge guards: no logic fire, no
variable fire, no house fire, no structure placed.

**Firewall.** `engine/map_trigger.{hpp,cpp}` is World-free; the only code that
constructs a `MapTriggerSystem` is `run_demo_triggers`. `World`, `MapFile`,
`load_assets` and every other gate path are untouched, so the 19 prior gates are
unaffected by construction.

**Deviations we take** (`map-triggers.md` §7): RandomDelay draws the original's
inclusive range from our own seeded generator; presentation actions are recorded
not rendered; object/cell events are not raised by world objects yet; map
TeamTypes have no TeamClass, so reinforcements are placed directly at the
waypoint (no transport delivery, no script) and CreateTeam is recorded; the
game-over flag that stops raises after an outcome is not modelled.

**Carry-forwards (P10):** the campaign object loader (tags on placed objects,
`InstanceCount`, cell-entry raises), TeamClass execution for map teams,
destroyed/last-built house counters, MakeAlly/MakeEnemy, ChangeHouse,
CreateCrate, Apply100Damage, outcome → mission end.
## Veterancy + Crates (P5)

Calibration authority: `docs/re/veterancy.md` (Ghidra hunt of YR 1.001
`gamemd.exe` + this install's `rulesmd.ini`); spec
`docs/superpowers/specs/2026-10-06-veterancy-crates-design.md`, plan
`docs/superpowers/plans/2026-10-06-veterancy-crates.md`.

**Veterancy** replaces the capture phase's minimal spawn-baked rank-1:

- Experience is the original's `float` (`Techno+0x150`): each kill adds
  `victimCost / (killerCost * VeteranRatio)` (computed in double, cap compared on
  the unrounded sum, stored as float); veteran victims count x2, elite x3,
  allied 0; `DontScore` victims and `Trainable=no` killers give nothing;
  destroyed buildings credit their roster Cost. Veteran = `[1,2)`, elite `>= 2`
  (`VeteranCap=2`).
- Per-type `VeteranAbilities`/`EliteAbilities` (elite inherits the veteran list)
  gate the exact integer chains: FIREPOWER `ftol(d*1.1)`, STRONGER
  `ftol(d/1.5)` then `max(d,1)` BEFORE the Verses, ROF `ftol(r*0.6)`, FASTER
  `ftol(s*1.2)`, SIGHT (dead: `VeteranSight=0.0`). Elite swaps to `ElitePrimary`.
- Elite `SELF_HEAL` (and `SelfHealing=yes` types such as HARV/APOC) gain +1 HP on
  the global `ftol(RepairRate*900)=14`-frame phase.
- Chevrons: YR `PIPS.SHP` frame 14 (veteran) / 15 (elite).
- The x87 runs at 53-bit precision (CRT `_controlfp(_PC_53)`), so plain IEEE
  double arithmetic reproduces every boundary (e.g. `5*0.6 == 3.0`).

**Crates** (lobby CRATES toggle): exact `ScenarioClass::Random` port (R250 +
psdes seeding) for all crate draws; `clamp(players, CrateMinimum, CrateMaximum)`
initial crates; lifetime `ftol((A-B)*rand+B)` with `A=Regen*1800`,
`B=Regen*450`, relocation on expiry; pickup = cumulative-weight `[Powerups]`
pick, the multiplayer conversion rules (e.g. a second Armor crate on an armored
unit becomes Money), replacement placed BEFORE the effect; effects Money
`Random(2000,2900)`, Unit (CrateGoodie pool), HealBase, Reveal, Armor x1.5 /
Firepower x2.0 (every Techno of any house within `CrateRadius`=768 leptons),
Speed x1.2 (foot units), Veteran (+1 level).

**Firewall:** everything runs only after `World::enable_veterancy` /
`enable_crates`, called solely by `GameScene` (the interactive game) and
`--demo-veterancy`. `launch_skirmish` (shared with `--demo-skirmish`) is
untouched; a legacy World keeps the spawn-baked rank-1 path verbatim.

**Cut / carried forward:** ROF `Random(0,2)` rearm jitter (found in the binary,
contradicting combat.md section 5 -- would move every combat gate); promotion
EVA/sound and the elite flash render; SCATTER/CLOAK/SENSORS/... abilities; the
zero-weight powerups; the forced-MCV crate; spawner/occupant kill re-routing;
aircraft FASTER; `NearbyLocation`/`MapRect` are ring-search/grid-bound
fallbacks; the Unit crate draws once from the injected pool.

## Roster Breadth + Remaining Producers (P6)

Calibration authority: `docs/re/roster2.md` (Ghidra hunt of YR 1.001
`gamemd.exe` + this install's `rulesmd.ini`/`artmd.ini`); spec
`docs/superpowers/specs/2026-10-06-roster2-design.md`, plan
`docs/superpowers/plans/2026-10-06-roster2.md`.

- **Roster** (APPEND-ONLY): `kRoster` grows 24 -> 39 with GAYARD, NAYARD, GAWALL,
  GADEPT, NADEPT, NATECH, NAIRON, NAMISL, GACSPH, GAGAP, GAOREP, GTGCAN, AMRADR,
  GAWEAT, NAINDP (GAWEAT/NAINDP art absent here -> placeholder, never placed).
  Indices >= 24 honour rules `Image=` (AMRADR draws as GAAIRC); 0..23 resolve
  exactly as before. A new `kRoster2Units` table adds 24 bodies (naval DEST AEGIS
  CARRIER LCRF SUB HYD DRED SAPC; TNKD TTNK DTRUCK SHAD BFRT ROBO; BEAG; IVAN SNIPE
  TANY FLAKT SENGINEER BORIS CCOMAND PTROOP CIVAN). YR-only voxels fall back to
  `ra2md.mix/localmd.mix`. Abilities (spawners, Ivan bomb, demo truck, ...) are not
  in scope; SQD/DLPH/SCHP are dropped (no complete voxel art), GASPYSAT/GAROBO deferred.
- **Walls**: placed walls connect through a 4-bit N/E/S/W mask (`CellClass::Wall_Update`,
  bit0=N (0,-1), bit1=E, bit2=S, bit3=W); frame = `stage*16 + mask`. Placing a wall
  runs the original wall-line fill: per facing, walk up to `GuardRange` (5) cells;
  reaching an own same-type wall over placeable cells fills the gap for FREE. Wall
  build time = `ftol(build_frames * WallBuildSpeedCoefficient(3.0))` (100 -> 189).
- **Naval Yard**: `WaterBound=yes` -> SpeedType Float placement (every foundation
  cell Water); `Factory=UnitType` buildings split by `Naval` into the naval / land
  categories (a ship never comes from GAWEAP); the produced ship exits onto the first
  free water ring cell; the yard footprint is cut out of the water grid.
- **Airforce Command**: pad k = building centre + art `DockingOffsetk` (GAAIRC: 4
  pads); Harrier/Black Eagle (`Ammo=1`) take off, fly straight to the target, fire
  (`Ammo--`), return to their pad, land, and rearm +1 ammo per
  `ftol(ReloadRate*900)` = 270 frames.

**Deviations / carry-forward:** walls stay 1x1 Buildings (the AI-base gate builds
NAWALL that way) with an HP-derived stage; the wall 3x time is a pure helper, not yet
in the live Defense queue; the naval exit uses the existing ring walk, not
`NearByLocation`'s spiral; `NumberImpassableRows` not modelled; Fly-locomotor
climb = 100 leptons/frame [RE-FALLBACK]; the airfield sortie is a self-contained
state machine (not yet driven from the sidebar/World flight); naval/aircraft
sidebar queues are not wired into the interactive game.

## Campaign Missions (P10)

The RA2 **Allied** and **Soviet** campaigns, played through the real game
path. Recovered in `docs/re/campaign.md` from `gamemd.exe` (the battle/mission/
mapsel loaders, the `[Basic]` reader, the three object-section readers,
`Flag_To_Win`/`Flag_To_Lose` + the BorrowedTime expiry in `HouseClass::AI`,
`Do_Win`, the MapSel stage selectors and the team-script jump table); RA2's
`game.exe` carries the same strings without the `MD` suffix, and the RA2 data
files (`battle.ini`/`mission.ini`/`mapsel.ini` in `ra2.mix/local.mix`, the maps
in `Maps01.mix`/`MAPS02.MIX`) are consumed with that logic.

**Play it:** Main Menu > Single Player > New Campaign > Allied / Soviet, or
`ra2engine --campaign ALL01t.MAP` (any mission). The loading screen shows the
mission's `LS800` art, its name and `LSLoadBriefing` text and the difficulty;
in the mission **F1** shows the briefing, objectives (`TextTrigger` lines)
appear top-left, the mission timer top-centre; after the outcome the
BorrowedTime (`SavourDelay` .1 min = 90 frames) runs out and the result screen
offers Continue (next mission), Retry or Main Menu. Options > Abort Mission
returns to the main menu. Campaign games save/load (P7) with the mission
identity in the header and the trigger runtime + campaign state in `TRIG`/`HOST`.

**What the original does (and we now do):**
- **Progression is MapSel, not `NextScenario`.** `[Basic] NextScenario=` is a TS
  leftover (`GDI2A.map` on every RA2 map). The next mission is the
  `mapsel.ini` stage chain of the player's side (`[GDI]` / `[Nod]`), the
  current stage found by matching its `Scenario=` to the current map; each RA2
  stage has exactly one successor: `ALL01t → … → ALL12s`, `SOV01t → … → sov12s`;
  `EndOfGame=yes` ends the campaign.
- **RA2 index spaces.** The maps' trigger house/building values index the RA2
  `rules.ini` `[Countries]`/`[BuildingTypes]` (YR's `rulesmd.ini` inserts
  `YuriCountry` and shifts them): on `all01t` `Win 13` = `Player`.
- **Objects** placed with the recovered field orders (`[Infantry]` sub-cell/
  mission/facing/tag…, `[Units]` facing/mission/tag…, `[Structures]`
  facing/tag/…), strength = `ftol(Strength×health/256)` snapping to full above
  `Strength−3`; a failed placement deletes the object; tagged objects attach to
  the tag's single `TagClass` (InstanceCount).
- **Triggers in campaign mode**: object/cell events are raised from the world
  (cell entry 1/59 via `[CellTags]`, destroyed 48/7, attacked 6, discovered 4,
  selected 33); ChangeHouse (every object whose tag holds the firing trigger),
  AllChangeHouse, MakeAlly/MakeEnemy (both directions), DestroyAttachedObject,
  Apply100Damage, destroyed/last-built counters, CreateTeam / Reinforcement /
  DestroyTeam / AllToHunt.
- **Map teams** run the map's own `[ScriptTypes]` with the script-action
  numbering confirmed on the executor's jump table (`3` move to waypoint, `0`
  attack, `5` guard, `6` jump (1-based), `7`/`23` win/lose, `12/38/39/40`
  variables, `20` change house, `37` delete members, `46/47` enemy building …).

**Status:** Allied 1 (`all01t`, URBAN) and Soviet 1 (`sov01t`, URBAN) run
start → win through their own triggers (the gate's victory runs reach `WON`
and name `ALL02s`/`sov02t`); every other mission loads and runs through the
same path (`--campaign-survey`: all 24 Allied/Soviet missions (URBAN ×9, SNOW ×8, TEMPERATE ×7) construct and run 450 frames with their own triggers and teams, none failing early; their start → win was not individually played through).

**The campaign gate**, `--demo-campaign`: Allied 1 and Soviet 1 for 1,800
frames each with a scripted player walking the missions' cell-tag trip-wires,
then a scripted-kill **victory run** of each (BadGuy1's buildings / the four
Pentagon sections destroyed at frame 150 — the missions' own triggers must
carry it to `WON` after the BorrowedTime), each rendered + headless and
byte-compared; then a **save/load continuity** segment (Allied 1 saved at
frame 300 — World + `TRIG` + `HOST` — loaded into a fresh runtime, both run to
600, World and trigger state identical). `campaign_hash=f10f9d4e6bd883cd`,
`lines=870`.

**Firewall.** `CampaignRuntime` (`engine/campaign_runtime.inc`) is constructed
only by a campaign `GameScene` (non-empty `SkirmishConfig::campaign_map`) and
`--demo-campaign`; `MapTriggerSystem` campaign mode defaults OFF (`--demo-triggers`
is identical with and without this phase's trigger changes). World gained only
additive per-unit order wrappers; the voxel sprite set index is clamped for
houses ≥ 2 (houses 0/1 unchanged). Save format → v2 (`campaign_map/side/
difficulty` in `CONF`, MapTriggerSystem's two new members, `CampaignHostSave`).

**Deviations / cuts** (`campaign.md` §9): no campaign movies (the `MOVIES0x.MIX`
are 4-byte stubs on this install); result screen instead of the unrecovered
score tally; reinforcements appear at the team waypoint (no transport/paradrop
delivery) and `CreateTeam` recruits idle units; transport/deploy/iron-curtain/
chrono script actions are skipped; Allied 1's dreadnought shelling of the
Statue is held (Patriot interception not modelled); symmetric alliance teams;
enemy base build-out (ProductionBegins/AI triggers for map houses) not run;
aircraft objects not placed; object facing/sub-cell/veterancy/Sleep not
applied; MCV deploy now exists (gap F, see "Real play: MCV, skirmish start,
game over"); unit stats
from `rulesmd.ini` (RA2 `rules.ini` only for index spaces); map-embedded rules
overrides now applied (W2-CA-04, below); civilian building art uses the temperate candidate walk.

**Carry-forwards:** team transport delivery + paradrops, missile interception,
map rules overrides, enemy base AI from map `[<House>]` IQ/nodes, bridges.
(MCV deploy, the player's TechLevel cap, trigger crates (108), reshroud (101),
fire sale (9) and events 29/44/25/26 landed in gap F.)

### Campaign wave-2 fixes (branch `w2fix-campaign`, W2-CA-01..19)

Audit: `docs/gap/wave2/01-campaign.md` (§5 has the row-by-row status and the
per-mission table). Every one of the 24 RA2 missions now reaches its own `Win`
action through its trigger chain; before/after real-play winnability
6 YES / 5 PROBABLE / 2 DOUBTFUL / 11 NO -> 16 YES / 8 PROBABLE / 0 / 0.

* **EnteredBy on buildings (W2-CA-01)** -- World records campaign-only
  `EnteredEvent`s (engineer capture/repair, spy infiltrate, garrison, bridge
  hut); the runtime raises 1 on the building's tag (+2 for spies) and 48 on the
  consumed infantry [RE `FUN_00519449`].
* **PlayerControl houses (W2-CA-02)** -- `[<House>] PlayerControl=yes` ->
  `World::controllable(h)`; selection and orders accept those units
  [RE `FUN_0050B510` ControlledByHuman reads `house+0x1ED`].
* **Rules-driven campaign roster + map rules overlay (W2-CA-03/04)** -- the
  campaign GameScene's rules are rulesmd with the map's rules sections laid
  over them; the sidebar is the skirmish rp path with the player house's
  country/TechLevel and the classic RA2 type list.
* **Side fallback (05), events 55/56 (06), team release (09), pairwise
  MakeAlly/MakeEnemy (10), attack-waypoint hold only on loss-guarding targets
  (11), SellBuilding/EvictOccupiers (13), damage events 38-43 and 22 (14).**
* **Map AI (W2-CA-07)** -- action 74 runs the map's own AITriggerTypes/
  TeamTypes; **sea/edge landings (W2-CA-08)** land at the nearest free cell
  connected to the team's first move-waypoint; **aircraft (W2-CA-16)** placed
  through the W2-AN flight model; **mind control (W2-CA-15)**; script actions
  Deploy/Unload/Scatter (17).
* **Mission audio (W2-CA-12)** -- PlaySpeech/PlaySoundEffect/
  PlaySoundAtWaypoint/PlayMusicTheme and `[Basic] Theme=` through the
  presentation audio (RA2 `eva.ini` Mis_* lines).
* **Play scripts at mission N (W2-CA-19)** -- `set campaign <MAP> <GDI|Nod>`
  starts a `--play` script at that mission; `cheat camp_*` (killb/killu/
  killdef/spawn/move/capture/occupy/attackb/attacku/deploy). New scripts:
  `m-all02`, `m-all04`, `m-all08`, `m-sov03`, `m-sov05`, `m-sov06`, `m-sov09`.
* Save format **v12** (PlayerControl, pair alliances, entered events,
  mind-control links).

Still cut: real transport/paradrop delivery (RE-C1), `Mission=` Sleep/Harmless
(W2-CA-18), actions 88/90/102/18/42/35/..., script actions 14/55/42/58,
events 31/5.

## Building

Prerequisites: CMake 3.20+, MinGW-w64 gcc, Ninja, Python 3.13. The engine also
pulls SDL2 via CMake `FetchContent` (pinned `release-2.30.9`), built from source
on first configure.

Configure and build with CMake + Ninja:
```bash
cmake -S . -B build -G Ninja
cmake --build build
```

An unset `CMAKE_BUILD_TYPE` now means **RelWithDebInfo** (`-O2 -g`; W2-GA-P2 —
the old default was unoptimised `-O0`, 2.5–3.5x slower in terrain draw, IMA
decode and voxel rasterising). For a shipping build pass
`-DCMAKE_BUILD_TYPE=Release` (`-O3`, no debug info); `-DCMAKE_BUILD_TYPE=Debug`
gets `-O0` back. Every gate hash is identical across these build types
(re-verified on the wave-2 gfx branch).

Run tests (PowerShell — the compiled test exe must be launched from PowerShell,
not Git Bash, on this Windows setup; set `RA2_INSTALL_DIR` to exercise the
real-asset tests, otherwise they skip cleanly):
```powershell
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
& .\build\ra2assets_tests.exe
```

## Engine (desktop)

`ra2engine` reads your own legally-owned game files at run time via
`RA2_INSTALL_DIR` (default `D:\Red Alert 2`); no assets are bundled or
committed. It loads a real temperate stock map (`mapsmd03.mix` entry
`0x133a0cb2`, 103×110), the temperate theater tiles/palette
(`ra2.mix → isotemp.mix`, `isotem.pal`), and the GI infantry
(`conquer.mix/gi.shp` + `cache.mix/unittem.pal`, resolved via `rulesmd.ini`
unit `E1`).

Build and run (PowerShell):
```powershell
cmake --build build --target ra2engine
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
& .\build\ra2engine.exe
```

Startup: MIX files are block-read (`ra2assets::read_file`) and `ra2.mix` is
opened once per run; cold start dropped from ~79 s to ~0.87 s on the host
(measured via `Measure-Command { ra2engine --screenshot }`). The same sources
build into the Android APK via `cmake/ra2_sources.cmake` — no jni changes.

Interactive controls (the ORIGINAL scheme, see "Real-play controls" and
`docs/INSTALL.md` section 4): left-click a unit to select it; left-drag a box
to select many; Shift+left-click to add/remove one; double-click a unit to
select all of its type on screen; with a selection, LEFT-click ground to move
(units fan out to distinct cells and route around each other) and LEFT-click
an enemy to attack it; right-click deselects or cancels one armed mode;
right-drag, the arrow keys or the screen edge scroll. Esc opens the options
menu (it never quits; close the window or use Exit Game). Touch: see
`docs/INSTALL.md`. Ctrl+Shift+F (or `--fps`) shows the perf overlay.

Headless/verification CLI modes (render one or more frames and exit):
- `ra2engine.exe --screenshot <out.png>` — render one frame of the map+unit to a PNG.
- `ra2engine.exe --demo-move <cellX> <cellY>` — select the unit, path to the cell, run the sim to arrival, write `task12_start.png`/`task12_end.png` to the scratchpad.
- `ra2engine.exe --demo-retarget <x1> <y1> <x2> <y2>` — order a move, retarget mid-walk, confirm arrival at the second target.
- `ra2engine.exe --demo-group <n> <cellX> <cellY>` — spawn `n` GI units, select
  all, group-move to the cell, run to arrival; writes `demo_group_start.png`/
  `demo_group_end.png` to the scratchpad and exits non-zero if any two units
  ever shared a cell.
- `ra2engine.exe --demo-combat [seed]` — 5 GIs vs 5 Conscripts fight to
  elimination with a deterministic player-micro policy; the sim runs twice
  (rendered + headless) and the per-hit damage logs must be byte-identical
  (combat is fully deterministic — the original's combat paths use no RNG,
  see `docs/re/combat.md`; the seed is accepted for interface stability and
  unused). Writes start/end screenshots to the scratchpad; exits non-zero on
  any overlap, dangling target, log divergence, or a bloodless battle.
- `ra2engine.exe --demo-audio` — headless-safe proof that the audio pipeline
  actually produces sound: loads `rulesmd.ini`/`soundmd.ini`/`evamd.ini` and
  the `audio.bag` archive, plays `GIAttack` and renders straight into a plain
  buffer bypassing SDL entirely (fails if every sample comes back zero); if a
  real audio device opens, it also plays an audible ~4 s scripted sequence
  (`GIAttack` → `GISelect` → `GIDie` → EVA "Unit lost") through it. Device
  absence is not a failure (CI/headless boxes have none — only check 1 must
  pass); prints a `[demo-audio] clips=<n> device=yes|no mix=nonzero` summary
  and exits 0.
- `ra2engine.exe --demo-tank` — the voxel-vehicle counterpart of
  `--demo-combat`: 5 GIs + 3 Rhino tanks per side fight to elimination; the
  sim runs twice (rendered + headless) and the per-hit damage logs must be
  byte-identical. Writes start/end screenshots to the scratchpad; exits
  non-zero on any overlap, dangling target, or log divergence.
- `ra2engine.exe --demo-base` — the Buildings & Construction integration
  gate: builds a Construction Yard → Power Plant → Barracks-class producer,
  confirms `GAWEAP` stays prerequisite-blocked (`PROC` unmet) against a
  pre-built Soviet base, and hashes the whole world state (FNV-1a 64-bit).
  Rendered and headless runs must produce identical witness lines and hash.
- `ra2engine.exe --demo-econ` — the Economy integration gate: places a
  refinery (charging the FreeUnit-subtracted cost, building at the raw-Cost
  frame count), watches its `CMIN` FreeUnit complete a chunked unload, watches
  an AI refinery's `HARV` unload for income, confirms `GAWEAP` unblocks once
  `PROC` is met, and crosses one ore-growth timer firing — then hashes the
  world state (ore field + credits included). Rendered and headless runs must
  be byte-identical.
- `ra2engine.exe --demo-prod` — the Unit Production integration gate: queues a
  Grizzly (`MTNK`) in the Vehicle queue and a GI (`E1`) in the Infantry queue
  concurrently from the same base, confirms the tank exits at its computed
  8-ring exit cell and drives to its rally, places a second War Factory,
  proves `set_primary` routes the next tank to the new primary and that its
  build time is exactly the recovered MultipleFactory ×0.8 of the
  single-factory build — then hashes the world + queue state (`prod_hash`).
  Rendered and headless runs must be byte-identical.
- `ra2engine.exe --demo-ai-base` — the AI-1 base-planning integration gate: a
  lone Soviet Construction Yard (house 1, difficulty Normal, fixed seed) runs
  the recovered per-house AI tick end-to-end — building power → barracks →
  refinery → war factory → radar (the topological prereq-sort order,
  `docs/re/ai-base.md` §2/§2a) through the same production queues and
  placement validity the player uses — for a fixed frame budget, then hashes
  the world/AI state (`ai_base_hash`). Runs twice and both runs must produce
  identical witness lines and hash.
- `ra2engine.exe --demo-ai-team` — the AI-2 offensive-teams integration gate:
  a Soviet AI house (house 1, a pre-built base, `team.enabled=true`)
  autocreates a team from the real `aimd.ini`, recruits/produces its
  TaskForce through the existing queues, gathers near the base, and marches
  the squad at the player under the ScriptType interpreter
  (`docs/re/ai-team.md`) — then hashes the team/production/credit state
  (`ai_team_hash`). Runs twice and both runs must produce identical witness
  lines and hash.
- `ra2engine.exe --demo-ai-trigger` — the AI-3 adaptive-trigger integration
  gate: a Soviet AI house (house 1, a pre-built base, Normal difficulty,
  `triggers_enabled=true`) parses the real `aimd.ini` `[AITriggerTypes]`
  table, runs a deterministic runtime scan (no hardcoded id) for an
  intrinsically-eligible exemplar, then lets the trigger loop run for a fixed
  frame budget: condition met → trigger selected by the recovered
  cumulative-sum draw over `round(current weight)` → Team1 (and Team2, when
  set) created through the AI-2 path → the created team's script-completion
  disband outcome adjusts every trigger sharing that Team1's identity by the
  clamped `+20`/`−50` track-record rule (`docs/re/ai-trigger.md`) — then
  hashes the trigger/team/credit state (`ai_trigger_hash`). Runs twice and
  both runs must produce identical witness lines and hash.
- `ra2engine.exe --demo-transport` — the CB-1 Transports/Cargo integration
  gate: spawns one HTK Flak Track (art-guarded; FV IFV fallback with the
  script scaled to capacity 1) plus capacity+1 GIs, orders all GIs aboard
  (capacity is a passenger COUNT — one board per arrival, the surplus GI is
  REFUSED at the full transport; the exact-fill boundary and the refusal
  both witnessed), carries them to a distant cell (no passenger rendered or
  occupying a cell), unloads one passenger per logic frame in LIFO order
  (last in, first out — the original's head-pushed passenger stack) onto
  facing±90° scatter cells that alternate sides per eject (`eject_cell`;
  nearest-free-neighbour fallback, boxed-in HOLD), then runs a fixed
  resume window — and hashes cargo membership + positions at four
  checkpoints (`transport_hash`). Runs twice (rendered + headless); both
  runs must produce identical witness lines and hash; wedge guards exit
  non-zero if no board, no eject, no capacity refusal, or a non-LIFO eject
  order ever happens.
- `ra2engine.exe --demo-aircraft` — the CB-2 Aircraft & Flight integration
  gate: spawns one Kirov Airship (`[ZEP]`, art-guarded; ORCA/BEAG fallback)
  for house 0 and TWO adjacent hostile ground GIs (one cell apart) for house 1,
  then scripts TAKEOFF (releases its ground cell, ascends to cruise altitude 750
  while climb and horizontal travel overlap, transitions to Cruising), FLY
  (cruises in a straight line at altitude to a distant cell, reserving no cell —
  folded into the hash), ATTACK (flies to within weapon range and drops ONE bomb
  from cruise whose `BlimpHE CellSpread=2` 2-cell splash kills BOTH adjacent
  targets), and RESUME (holds cruise — BalloonHover) — and hashes the flight
  path + altitude + attack state + a zero-airborne-occupancy audit at four
  checkpoints (`aircraft_hash`). Runs twice (rendered + headless); both runs
  must produce identical witness lines and hash; wedge guards exit non-zero
  unless a takeoff AND an airborne cruise with `occ_held=0` AND one bomb's
  2-cell splash killing BOTH targets (2 kills) all actually happen.
- `ra2engine.exe --demo-naval` — the CB-3 Naval & Water-Movement integration
  gate: resolves the map's per-cell water mask (`set_water_terrain`), spawns one
  Sea Scorpion (`[HYD]`, art-guarded; DRED/DEST fallback) for house 0 on a lake
  cell and one hostile Sea Scorpion for house 1 on water within range, then
  scripts MOVE (sails across the water via A* on the water grid — every path
  cell is a Water cell, folded into the hash), LAND-BLOCK (ordered onto a land
  cell, refuses with an empty/stay-put path — the inverse of a ground unit),
  ATTACK (approaches to within weapon range over water and fires `FlakTrackGun`
  until the target dies), and RESUME (holds station on the water) — and hashes
  every unit's cell/alive/hp + each naval unit's domain/path-size/target + the
  count of water cells traversed at four checkpoints (`naval_hash`). Runs twice
  (rendered + headless); both runs must produce identical witness lines and
  hash; wedge guards exit non-zero unless the ship MOVED on >=1 water cell AND
  was BLOCKED from a land cell AND at least one hit landed.
- `ra2engine.exe --demo-superweapon` — the CB-4 Superweapons integration gate:
  grants house 0 the Soviet Nuclear Missile (`[NukeSpecial]`, `Type=MultiMissile`)
  via `grant_superweapon` with the recovered `[NukePayload]`/`[NUKE]` params
  (Damage=600, CellSpread=10, PercentAtMax=.02), spawns a cluster of enemy units
  (house 1) around a target cell within the nuke radius plus one own unit in-radius
  and one far enemy out-of-radius, then scripts CHARGE (steps the real recharge
  timer to READY), FIRE (`fire_superweapon` at the cell — returns true, resets the
  timer), DETONATE (the reused `apply_spread_damage` kills the in-radius enemies,
  spares the firing house, leaves the far enemy untouched), and RECHARGE (the SW
  re-charges from 0) — and hashes every unit's cell/alive/hp + the firing house's
  SW quantized-timer/ready/type + the target cell at four checkpoints
  (`superweapon_hash`). Runs twice (rendered + headless); both runs must produce
  identical witness lines and hash; wedge guards exit non-zero unless the SW
  reached READY AND fired AND >=1 in-radius enemy was killed.
- `ra2engine.exe --demo-techtree` — the CB-5 Tech-Tree integration gate: builds a
  fresh Allied base (`GACNST`/`GAPOWR`/`GAPILE`/`GAWEAP`, no radar) and resolves
  the probe unit roster (synthetic-`SREF` fallback if art is dropped), then folds
  the buildable-set snapshot (`can_start_unit` for `MTNK`/`SREF`, `can_start` for
  `GATECH`/`GAWEAP`) at BASELINE — where `SREF` is UNBUILDABLE (`GATECH` unmet)
  and `MTNK` is buildable — places the Battle Lab `GATECH` DIRECTLY (bypassing its
  own radar-gated prerequisite) so the `TECH` group is satisfied, and re-folds at
  UNLOCK, where `SREF` is now BUILDABLE — then hashes the world/building state +
  the buildable-set at three checkpoints (`techtree_hash`). Runs twice (rendered +
  headless) and both runs must produce identical witness lines and hash (no pinned
  literal — a compute-and-self-compare); wedge guards exit non-zero unless `SREF`
  is UNBUILDABLE before `GATECH` AND BUILDABLE after AND `MTNK` is buildable at
  baseline.
- `ra2engine.exe --demo-capture` — the Engineer/Spy Capture integration gate:
  builds a player ConYard + an enemy base fragment (refinery, power plant, an
  SW-building granted the Nuke, factory) + a neutral `CAOILD` oil derrick, then
  scripts an `ENGINEER` capturing an enemy building (ownership 1→0, an
  owned-count/power/buildability delta follows) and the neutral `CAOILD` (2→0,
  +1000 startup, then +20 every 100 frames), a `SPY` infiltrating the refinery
  (cash 0.5×), power plant (blackout 1000f), SW-building (recharge reset), and
  factory (produced units get `veteran_rank=1`, ×1.1 firepower / ×1.5 armor), and
  a disguised spy revealed by a `DOG` (`DetectDisguise`). Hashes building
  ownership + both houses' credits + blackout timers + SW ready + unit veteran/
  disguise state at fixed checkpoints (`capture_hash`). Runs twice (rendered +
  headless); both runs must produce identical witnesses + hash (no pinned literal);
  nine wedge guards exit non-zero unless capture flips ownership AND cash-steal
  moves credits AND blackout arms AND SW resets AND the dog reveals the spy. The
  gate is pure-state / art-independent (loads only GI/CONS art → engineer/spy/dog/
  derrick render as placeholders, like `--demo-superweapon`).
- `ra2engine.exe --demo-shroud` — the Shroud / Fog-of-War integration gate:
  enables the per-house visibility layer, then scripts a player unit's initial
  reveal (near-circular Sight disc), movement growing the explored map, fog
  stickiness after the source leaves (explored persists, visible clears), an
  enemy unit becoming unseen (seen_by_mask) when it sits in house 0's shroud, a
  spy sabotaging an enemy radar (reshroud_house clears the victim's explored map
  to 0), and a SpySat revealing the whole map (reveal_all_house -> explored ==
  grid). Hashes per-house explored/visible cell counts + a seen_by_mask sample at
  fixed checkpoints (shroud_hash). Runs twice (rendered + headless); both runs
  must produce identical witnesses + hash; wedge guards exit non-zero unless the
  initial reveal marks >0 cells AND movement grows explored AND fog persists AND
  the fogged enemy is unseen AND the sabotage clears the victim's map AND
  reveal-all fills explored.
- `ra2engine.exe --demo-multihouse` — the Multi-house / Alliances integration
  gate: configures >=3 combatant houses into 2 alliance-teams (houses 0+2 vs 1)
  plus a neutral house owning a structure, enables shroud + AllyReveal + victory,
  then scripts allied units NOT auto-acquiring each other but acquiring the enemy
  team (is_enemy/are_allied), AllyReveal sharing a scout's sight with its ally,
  one team eliminated -> house_defeated true, and last-team-standing ->
  winning_team resolves. Hashes per-house alive building+unit counts + a per-pair
  relationship sample + defeated flags + winning_team at fixed checkpoints
  (multihouse_hash). Runs twice (rendered + headless); both runs must produce
  identical witnesses + hash; wedge guards exit non-zero unless allies don't
  fight AND the enemy is acquired AND AllyReveal shares sight AND the eliminated
  house is marked defeated AND the surviving team wins.
- `ra2engine.exe --demo-skirmish` — the Skirmish Shell integration gate: builds
  a FIXED SkirmishConfig (3 combatant slots in 2 alliance-teams, houses 0+2 vs 1,
  plus a neutral non-combatant), launches it via launch_skirmish, enables shroud +
  victory + superweapons, then steps N fixed dt=1/15 frames. Hashes per-house
  alive building+unit counts + ai_enabled flags + credits + a per-pair
  relationship sample + winning_team at fixed checkpoints (skirmish_hash). Runs
  twice (rendered + headless); both runs must produce identical witnesses + hash;
  wedge guards exit non-zero unless launch_skirmish produced the expected
  house_count AND the AI slot is enabled AND victory+shroud are enabled AND the
  sim advanced deterministically.
- `ra2engine.exe --demo-roster` — the Broaden Unit/Building Roster integration
  gate: resolves all 16 new types from Rules/kRoster (12 units via
  resolve_combat_unit, 4 buildings from kRoster), asserts cost/tech/strength +
  a key weapon/stat per type, then spawns them into a fresh World (the 4
  buildings via the Allied full base spawn_base(allied_full=true) + explicit
  add_building; the 12 units via add_combat_unit) and steps N fixed dt=1/15
  frames. Hashes every spawned instance (like world_hash) + a per-new-type
  resolved sample + the resolved count (roster_hash). Runs twice (rendered +
  headless); both runs must produce identical witnesses + hash; wedge guards
  exit non-zero unless every new type resolves AND spawns AND the Allied full
  base placed >1 building AND a spot stat matches rules (APOC strength 800,
  ATESLA a building).
- `ra2engine.exe --demo-abilities` is the P4 Unit special abilities gate: eight
  fresh-World stages (drone x2, desolator, legionnaire, C4, Ivan, terrorist, IFV),
  run rendered + headless and byte-compared, wedge-guarded on every recovered
  number in `docs/re/abilities.md`.
- `ra2engine.exe --demo-repair` is the Building Repair & Sell integration gate.
  On a real Allied base it drives the wrench and measures, frame by frame, the
  14-frame global cadence, the +8 HP heal and the exact integer `stepCost`
  charge. It then proves the auto-stop and the silent out-of-credits stop,
  exercises every reachable sell refusal (enemy, under construction, CAOILD
  `Unsellable`, already selling) **and** the one rule that does not exist (the
  only full-health ConYard is sellable). Finally it runs two sell missions: the
  ConYard (FLAT 1500 only on completion, 3 E1 survivors on the freed
  foundation) and the refinery (`Soylent=300`, 1 survivor). Runs twice
  (rendered + headless) and byte-compares; 25 wedge guards
  (`docs/re/repair-sell.md`).

- `ra2engine.exe --demo-roster2` — the P6 Roster Breadth gate: witnesses the 15
  appended buildings and the `kRoster2Units` bodies, closes a wall line (0/3/2
  free fills, connection masks + frames), places GAYARD on water (land refused),
  builds a DEST on the naval category and launches it to a water exit, and flies
  ORCA + BEAG from GAAIRC pads through strike/RTB/land/rearm (270 frames). Runs
  twice (rendered + headless) and byte-compares; wedge guards armed.

`ra2engine.exe` also accepts `--no-audio` (skips audio init entirely — same
silent fallback as a failed device open, just without touching the mix chain
at all) alongside the interactive run.

### Determinism gates (permanent)

These headless demos (the table below plus `--demo-maps`, described in its
own section; 31 `--demo-*` gates as of wave 4) are byte-identity regression gates: any change to
their printed output signals a defect in whatever change caused it, not an
intentional update (content changes that legitimately alter output, e.g. a
new roster entry, must be called out explicitly — see the Economy divergences
above for `--demo-base`'s `roster=10`, and note `--demo-base`'s roster grows
again with each subsequent roster addition — `roster=40` as of W5-FIX-C (GASPYSAT; `roster=39` was P6 Roster
Breadth (the 15 P6 buildings; `roster=24` was the Broaden Roster phase — the
`NANRCT`/`ATESLA`/`NASAM`/`GAAIRC` buildings; `roster=20` was
the Engineer/Spy Capture phase — the `CAOILD` oil derrick; `roster=19` was the
CB-5 Battle Lab `GATECH`) — with the gated hash unaffected each time. The first
SEVENTEEN are the legacy gates re-verified unchanged by every subsequent phase;
`--demo-roster` is the Broaden Unit/Building Roster gate and `--demo-roster2`
the P6 Roster Breadth gate.

**Current verification policy (since wave 4).** Gates are slow (up to ~2-3 min
and ~3 GB each), so they are NOT run on every commit: a step runs only its
scoped unit tests (`ra2assets_tests --test-case="<module>*"`); a branch runs
the full suite, every `--play` script (one at a time) and every gate ONCE at
the end, before merging. A gate may move only where the change says so (and
is then re-pinned in this table with the reason); every other gate must print
its pin.

| Demo | Gate | Key witnesses |
|------|------|----------------|
| `--demo-combat` | `deterministic=yes logs=79` (W7 re-pin, was logs=104: W7 speed model (docs/gap/wave7/11-locomotion.md): Walk/Hover move raw leptons (no GameSpeedBias), Drive/Ship drive the original track tables (curved bends, x1.59/1.67 geometry), Accelerates ramp, ConditionYellow x.75, uniform-cost A* with occupancy costs, PathDelay/BlockagePathDelay/CloseEnough blocked moves; shots=73 deaths=6) `deterministic=yes logs=104` (net-l0 re-pin, was logs=86: original FastMath + whole-frame rearm timers (A/B: libm-only build prints logs=86, the timers move it)) `deterministic=yes logs=86` (last-fix re-pin, was logs=100, now shots=80 deaths=6 again: an AUTO-acquired target that vanishes mid-approach no longer sends the unit back to its guard anchor: it finishes the walk to its firing cell and guards there (combat.cpp step (d), traced: FootClass::Approach_Target FUN_004D55B0 sets a CELL Destination, which FootClass::PointerExpired FUN_004D9870 keeps -- only TarCom is cleared); the fv-a A/B value) `deterministic=yes logs=100` (fv-a re-pin, was logs=86 shots=80 deaths=6, now shots=92 deaths=8: a vanished AUTO-acquired target ends the unit's approach and it walks back to its guard anchor (R-211, combat.cpp step (d)); A/B: the same tree without the step-(d) change prints logs=86) `deterministic=yes logs=86` (W6 fix #2 re-pin, was logs=78: W6-02-11 threat radius = weapon range + 1, never Sight -- A/B: the same tree with the old max(range, Sight) radius prints logs=78) `deterministic=yes logs=78` (W5-B re-pin, was logs=87 shots=82 deaths=5: every hit through DamageArea/Modify_Damage -- no floor of 1 for units after the Verses, 3-D range) | `shots=72 deaths=6` — re-pinned by gap fix C (was logs=78 shots=73): recovered movement scale x GameSpeedBias (G-42) + original ROF `Random(0,2)` rearm jitter (G-50) |
| `--demo-tank` | `deterministic=yes logs=120` (W7 re-pin, was logs=124: W7 speed model (docs/gap/wave7/11-locomotion.md): Walk/Hover move raw leptons (no GameSpeedBias), Drive/Ship drive the original track tables (curved bends, x1.59/1.67 geometry), Accelerates ramp, ConditionYellow x.75, uniform-cost A* with occupancy costs, PathDelay/BlockagePathDelay/CloseEnough blocked moves; shots=110 deaths=10, hit90_rhino/hit22_infantry yes) `deterministic=yes logs=124` (net-l0 re-pin, was logs=134: FastMath (A/B 137) then whole-frame rearm timers (124)) `deterministic=yes logs=134` (last-fix re-pin, was logs=125, now shots=125 deaths=9 `hit90_rhino=yes hit22_infantry=yes` again: an AUTO-acquired target that vanishes mid-approach no longer sends the unit back to its guard anchor: it finishes the walk to its firing cell and guards there (combat.cpp step (d), traced: FootClass::Approach_Target FUN_004D55B0 sets a CELL Destination, which FootClass::PointerExpired FUN_004D9870 keeps -- only TarCom is cleared); the fv-a A/B value) `deterministic=yes logs=125` (fv-a re-pin, was logs=134 shots=125 deaths=9, now shots=114 deaths=11, `hit90_rhino=yes hit22_infantry=yes` unchanged: a vanished AUTO-acquired target ends the unit's approach and it walks back to its guard anchor (R-211, combat.cpp step (d)); A/B: the same tree without the step-(d) change prints logs=134) `deterministic=yes logs=134` (W6 fix #2 re-pin, was logs=163: W6-02-11 threat radius -- A/B: the old max(range, Sight) radius prints logs=163) `deterministic=yes logs=163` (W5-B re-pin, was logs=143 shots=135 deaths=8: DamageArea splash of the AP shells, shell close-enough re-aim, 3-D range; now `shots=153 deaths=10 hit90_rhino=yes hit22_infantry=yes`) (W2-SK-25 re-pin, was logs=129: guard-range acquisition/pursuit) | `shots=135 deaths=8 hit90_rhino=yes hit22_infantry=yes` (was shots=118 deaths=11) — re-pinned by gap fix C (was logs=144 shots=135 deaths=9): G-42 speed scale, G-50 jitter + min-1 damage |
| `--demo-base` | `hash=ce7b20e9b4ae09c9` (W7 re-pin, was `58ab59378652acf9`: W7 speed model (docs/gap/wave7/11-locomotion.md): Walk/Hover move raw leptons (no GameSpeedBias), Drive/Ship drive the original track tables (curved bends, x1.59/1.67 geometry), Accelerates ramp, ConditionYellow x.75, uniform-cost A* with occupancy costs, PathDelay/BlockagePathDelay/CloseEnough blocked moves; same building kill) `hash=58ab59378652acf9` (R3 re-pin, was `c4d64097b53d49d6`: an explicit attack on a building now holds at weapon range like an attack on a unit (Mission_Attack, infantry-anim.md §7) -- the GIs stop short of the NAPOWR and it dies at t=101.2 (was 101.3); A/B: the same tree with the old world.cpp prints `c4d64097b53d49d6`) `hash=c4d64097b53d49d6` (W2-SK re-pin, was `63d559e59abbda5a`: W2-SK-25 guard-range pursuit -- NAPOWR destroyed at t=102.5 (was 102.7)) `hash=63d559e59abbda5a` (final-verification re-pin, was `5273b5e71ca009e1`: moved at gap C 477ab70, bisected to 1d074af G-42 recovered speed scale x GameSpeedBias -- GI `move_cps` 3 -> 0.9375, NAPOWR now dies at t=102.7 instead of 87.9) | `roster=39` (content-only growth, hash unaffected), GAPOWR `frames=504 step_ticks=9`, credits `9200→8700`, `GAWEAP blocked (PROC unmet)` |
| `--demo-econ` | `econ_hash=df2fa14df9f1eb96` (W7 re-pin, was `ba42a1b045c5d3d7`: W7 speed model (docs/gap/wave7/11-locomotion.md): Walk/Hover move raw leptons (no GameSpeedBias), Drive/Ship drive the original track tables (curved bends, x1.59/1.67 geometry), Accelerates ramp, ConditionYellow x.75, uniform-cost A* with occupancy costs, PathDelay/BlockagePathDelay/CloseEnough blocked moves; the miners drive at the new speeds; the CMIN drives back to the ore (W7-11-13); same economy witnesses) `econ_hash=ba42a1b045c5d3d7` (W2-SK re-pin, was `fcf98e42955f4ff3`: W2-SK-01 original Riparius growth clock (2200 frames, `growth density=3->4`) and W2-SK-02/03/04 scan radius 48, dock queue, reachable ore search) `econ_hash=fcf98e42955f4ff3` (final-verification re-pin, was `7fed96cb98b68017`: moved at gap C 477ab70, bisected to 1d074af G-42 speed scale -- CMIN/HARV `speed_cps` 3 -> 0.9375, miner positions in the hash; every economy witness unchanged) | GAREFN `frames=1260 step_ticks=23 charge=600`; CMIN unload `+500` credits; enemy `income=1000`; `GAWEAP can_start=yes (PROC met)` |
| `--demo-prod` | `prod_hash=f0f4e1207ecd713c` | `deterministic=yes`; MTNK frames/charge, exit cell, concurrent GI, 2nd-factory primary routing |
| `--demo-ai-base` | `ai_base_hash=fba198cfddeccb39` (W6-04 re-pin, was `b61f283f7fd56db9`: W6-04-21: the Battle Bunker (NABNKR, in [AI] SovietBaseDefenses) is art-present now, so the Soviet plan places it) `ai_base_hash=b61f283f7fd56db9` (W2-SK re-pin, was `834b3a2b86f29574`: W2-SK-23 walls ring the defences (no AIBaseSpacing gap) and W2-SK-07 dead plan nodes re-link; same build order) `ai_base_hash=834b3a2b86f29574` (W2-AN re-pin, was `6a094915a28b9314`: the AI plan now places the tech centre / superweapon structures (and, in real play, the Naval Yard) right after the refineries instead of at the plan tail, W2-AN-21; master 844033b still prints the old hash) (gap D re-pin, see "Real-play AI") | `deterministic=yes`; lone Soviet ConYard builds out power → barracks → refinery → war factory → radar → defenses (the recovered topological prereq-sort order, `docs/re/ai-base.md` §2/§2a) at fixed difficulty/seed/frames |
| `--demo-ai-team` | `ai_team_hash=3ee3f158351f8778` (W7 re-pin, was `ai_team_hash=5b70028c394f93dd`: W7 speed model (docs/gap/wave7/11-locomotion.md): Walk/Hover move raw leptons (no GameSpeedBias), Drive/Ship drive the original track tables (curved bends, x1.59/1.67 geometry), Accelerates ramp, ConditionYellow x.75, uniform-cost A* with occupancy costs, PathDelay/BlockagePathDelay/CloseEnough blocked moves) `ai_team_hash=5b70028c394f93dd` (w7fix-units re-pin, was `fba117f5eaa814fd`: lines 21 -> 22: W7-04-03 BuildTimeMultiplier -- the AI's HTNK (1.5) now builds in 810 frames, so team 0ACDAEFC's first member joins at f3309 (was 3039); A/B witness diff against master shows only the shifted frames) `ai_team_hash=fba117f5eaa814fd` (net-l0 re-pin, was `01e2a343d31e75f3`, lines 21 unchanged: the TeamDelays check clock counts whole frames, so the first team is created on frame 2500 exactly (was 2501, float drift)) `ai_team_hash=01e2a343d31e75f3` (R3 re-pin, was `fdd60988623ca3ef`, lines 21 unchanged: the squad's attack on its target building holds at weapon range (R3 building hold), so team 0ACDAEFC reaches script steps 6/7 at f5083/5084 (was 5040/5041); A/B: the same tree with the old world.cpp prints `fdd60988623ca3ef`) `ai_team_hash=fdd60988623ca3ef` (W2-SK re-pin, was `c28ba20195c2d4dc`: W2-SK-06 production for every Recruiting/Gathering team shortfall in parallel factories, muster at the own base until filled or 60 s, size-scaled gather radius; W2-SK-11 one target per team) `ai_team_hash=c28ba20195c2d4dc` (gap D re-pin) | `deterministic=yes`; a Soviet AI house with a pre-built base autocreates a team from the real aimd.ini, recruits/produces its TaskForce, gathers near the base, and marches the squad at the player under the ScriptType interpreter (`docs/re/ai-team.md`) |
| `--demo-ai-trigger` | `ai_trigger_hash=1ec2b81dccb660ad` (W7 re-pin, was `ai_trigger_hash=8fa579e042f3ba4b`: W7 speed model (docs/gap/wave7/11-locomotion.md): Walk/Hover move raw leptons (no GameSpeedBias), Drive/Ship drive the original track tables (curved bends, x1.59/1.67 geometry), Accelerates ramp, ConditionYellow x.75, uniform-cost A* with occupancy costs, PathDelay/BlockagePathDelay/CloseEnough blocked moves) `ai_trigger_hash=8fa579e042f3ba4b` (w7fix-units re-pin, was `42f3533c1bbcab4e`: lines 13 -> 16: W7-04-03 BuildTimeMultiplier slows the AI's HTNK/TTNK production (same path as --demo-ai-team), so the TaskForce teams fill and finish later) `ai_trigger_hash=42f3533c1bbcab4e` (net-l0 re-pin, was `fa813aec707b3719`, lines 18 -> 13: FastMath (A/B `86ad7daba599c572`) then the whole-frame TeamDelays clock) `ai_trigger_hash=fa813aec707b3719` (R3 re-pin, was `3699768f334e348c`, lines 17 -> 18: with the R3 building hold the AI teams' building attacks end sooner -- trigger 0ECCF29C succeeds at f8781 (was 9325), 0C8EEAEC at f9518 (was 9656) and once more at f13491, final adjustments 4 -> 5, live teams 6 -> 5; A/B: the same tree with the old world.cpp prints `3699768f334e348c`) `ai_trigger_hash=3699768f334e348c` (W6-04 re-pin, was `287a87c0aefdfec7`: W6-04-17/18/21: two more aimd triggers name now-buildable YR types -- `aimd triggers parsed=10 -> 12`; lines=17) `ai_trigger_hash=287a87c0aefdfec7` (W5-E2 re-pin of a W5-B move: the w5-combat build already prints this hash with lines=17, and the change is not from W5-E2. Was `42b3dc0e306a6e1e`, re-pinned earlier as below) `ai_trigger_hash=42b3dc0e306a6e1e` (W2-SK re-pin, was `090383d7d78f5913`: W2-SK-06 team production/muster and W2-SK-08 trigger counts read the ONE current enemy house) `ai_trigger_hash=090383d7d78f5913` (W2-AN re-pin, was `17388180b9427561`, lines 21 -> 20: the AI unit roster gained the navies and jumpjets (W2-AN-16) and the plan order moved (W2-AN-21), so different TaskForce triggers win the weighted draw -- e.g. `0ECCF29C-G` at f=7502; same machinery) (gap D re-pin) | `deterministic=yes`; a Soviet AI house (Normal difficulty) with an intrinsically-eligible ct=4 HouseCredits exemplar (`0D2C41EC-G`, weight band `[10..70]`, runtime-scanned, not hardcoded) fires by the recovered cumulative-sum draw over `round(current weight)`, creates its Team1 through the AI-2 path, and the created teams' script-completion disband outcomes adjust every trigger sharing that Team1 by the clamped `+20`/`−50` track-record rule (witnessed weight movement `40→60`, `50→70`, `60→70` across the run) (`docs/re/ai-trigger.md`) |
| `--demo-transport` | `transport_hash=afcdd96989108c45` (W7 re-pin, was `transport_hash=54da5c3b71c51007`: W7 speed model (docs/gap/wave7/11-locomotion.md): Walk/Hover move raw leptons (no GameSpeedBias), Drive/Ship drive the original track tables (curved bends, x1.59/1.67 geometry), Accelerates ramp, ConditionYellow x.75, uniform-cost A* with occupancy costs, PathDelay/BlockagePathDelay/CloseEnough blocked moves) `transport_hash=54da5c3b71c51007` (final-verification re-pin, was `fe500f946e913645`: moved at gap C 477ab70, bisected to 1d074af G-42 speed scale -- HTK `speed_cps` 6 -> 1.875, boards at f=31..111 (was 9..44), carry arrives f=249 (was 98); same 5 boards / 1 refusal / 5 ejects) | `deterministic=yes`; variant=HTK (Passengers=5 SizeLimit=2 on this install; FV fallback documented if art ever regresses); 5 boards to the exact-fill COUNT boundary + 1 refusal at full; carry with zero carried-unit draw/occupancy folded into the hash; 5 LIFO ejects one-per-frame onto facing±90° scatter cells (`eject_cell`); wedge guards armed (`docs/re/transport-cargo.md`) |
| `--demo-aircraft` | `aircraft_hash=a69642564cf64773` | `deterministic=yes`; variant=ZEP (Kirov, `JumpjetHeight=750`, `ROT=10`, `JumpjetSpeed=5` on this install; ORCA/BEAG fallback documented if art regresses); takeoff to alt=750 with the ground cell released (climb/travel overlap), straight-line cruise at altitude with zero occupancy held (folded into the hash), one `BlimpHE CellSpread=2` bomb splash killing BOTH adjacent ground targets (2 kills), hold-at-cruise resume; raised voxel draw + ground shadow; wedge guards armed (`docs/re/aircraft.md`) |
| `--demo-naval` | `naval_hash=36c7b3ed1259f7e5` (W7 re-pin, was `167b4335dcf60738`: W7 speed model (docs/gap/wave7/11-locomotion.md): Walk/Hover move raw leptons (no GameSpeedBias), Drive/Ship drive the original track tables (curved bends, x1.59/1.67 geometry), Accelerates ramp, ConditionYellow x.75, uniform-cost A* with occupancy costs, PathDelay/BlockagePathDelay/CloseEnough blocked moves; ok=Y/Y, the HYD arrives f=405 on its tracks) `naval_hash=167b4335dcf60738` (W6 fix #2 re-pin, was `d95a2692eec41986`: W6-02-11 -- the idle HYD now acquires the shore enemy at weapon range + 1 and closes in during the land-block window; the gate's land-block verdict now means "no path and never on land" instead of "did not move", `ok=Y/Y`) `naval_hash=d95a2692eec41986` (W5-B re-pin, was `bdf8f5e8a0852d66`: a ship's approach heads for the nearest free water cell beside its target; hits through DamageArea) `naval_hash=bdf8f5e8a0852d66` (W2-SK re-pin, was `6aee2c82bc024e86`: W2-SK-25 guard-range acquisition (the ship engages from Sight); land order still refused, hits=200, target dead) `naval_hash=6aee2c82bc024e86` | `deterministic=yes`; variant=HYD (Sea Scorpion, `Speed=8`, `ROT=6`, `FlakTrackGun` Damage=25/ROF=25/Range=5, Warhead `FlakTWH` CellSpread=1.0 on this install; DRED/DEST fallback documented if art regresses); sails across the `mapsmd03` water region (468 Water cells, 252-cell main body, bbox [71,11]–[95,40], runtime-classified from the theater `[General]` tile-sets) via A* on the water grid (every path cell Water, folded into the hash), refuses a land-cell order (empty/stay-put — the water/land inverse), approaches over water and fires FlakTrackGun to a kill, holds station on resume; voxel ship at altitude 0 (existing draw path); wedge guards armed (`docs/re/naval.md`) |
| `--demo-superweapon` | `superweapon_hash=e6969fed3e772bd3` (W2-AN re-pin, was `189496664f5eca68`: the nuke now flies 200 up + 200 down frames before the blast (W2-AN-05, new `[sw] flight frames=` witness) and the blast hits the firing house too (W2-AN-06: `own_spared=no`)) | `deterministic=yes`; grants the Soviet Nuclear Missile (`[NukeSpecial]` `Type=MultiMissile`, `RechargeTime` -> recharge_secs, `[NukePayload]` Damage=600, `[NUKE]` CellSpread=10/PercentAtMax=.02/Verses); charges to READY, fires at the target cell, detonates via the REUSED `apply_spread_damage` (enemies-only, instant), spares the firing house, leaves out-of-radius units untouched, re-charges from 0; NukeIcon cameo + gclock2 recharge clock (art-guarded, placeholder if absent); wedge guards armed (`docs/re/superweapon.md`) |
| `--demo-techtree` | `techtree_hash=e73adae61ff6f71f` | `deterministic=yes`; fresh Allied base (no radar), `roster=39 sref_slot=4`; BASELINE `SREF unbuildable (GATECH unmet) can_start=0 ; MTNK can_start=1` → places Battle Lab `GATECH` DIRECTLY (bypasses its radar-gated prereq) → UNLOCK `SREF buildable (GATECH met) can_start=1`; reuses the existing `can_start_unit`/`can_start` predicate verbatim; wedge guards armed (`docs/re/techtree.md`) |
| `--demo-capture` | `capture_hash=b3166ecd8847c267` (w7fix-units re-pin, was `58b87b5bc63f7046`: lines 10 -> 9 with exit 0: W7-01-02 [RE 0x6F71D0] a DetectDisguise dog no longer CLEARS the spy's disguise (no radius, no flag clear) -- it ignores it in its own scan; checkpoint 9 folds the still-set flag; the witness now reads dog_revealed=dog-sees-through) `capture_hash=58b87b5bc63f7046` | `deterministic=yes`; engineer capture (`ChangeOwner` flip + SW move + AI base-node resync + startup-from-neutral) of an enemy building AND a neutral `CAOILD` (Startup 1000, Amount 20 / Delay 100f); spy infiltrate (cash-steal 0.5×, power-blackout 1000f, SW recharge reset, factory→produced `veteran_rank=1` ×1.1 firepower/×1.5 armor, radar/lab inert); disguise + dog `DetectDisguise` reveal (spy stays disguised on attack — PermaDisguise); nine wedge guards armed (`docs/re/capture.md`) |
| `--demo-shroud` | `shroud_hash=d302852aa9517661` | `deterministic=yes`; per-house visibility (near-circular Sight reveal, dx2+dy2<=sight2, cap 10), fog no-regrow (explored sticky), fogged-enemy untargetable (seen_by_mask), spy->enemy-radar reshroud (RadarLab), SpySat reveal-all; wedge guards armed (`docs/re/shroud.md`) |
| `--demo-multihouse` | `multihouse_hash=39412ae996cce8ae` | `deterministic=yes`; N-player HouseRec table + alliances (is_enemy/are_allied, FFA default a!=b), AllyReveal shared sight, last-team-standing victory (ShortGame=last-building), per-house side/color (8 MP colors); wedge guards armed (`docs/re/multihouse.md`) |
| `--demo-skirmish` | `skirmish_hash=fde3f587e747f765` | `deterministic=yes`; config-driven launch (SkirmishConfig -> launch_skirmish -> World), house table + toggles + AI-enable + per-house credit seed, 3 combatant slots in two alliance-teams (0+2 allied vs 1, no neutral); wedge guards armed (`docs/re/skirmish.md`) |
| `--demo-roster` | `roster_hash=f0ab5da966934761` (W7 re-pin, was `e68a5b05a8a64547`: W7 speed model (docs/gap/wave7/11-locomotion.md): Walk/Hover move raw leptons (no GameSpeedBias), Drive/Ship drive the original track tables (curved bends, x1.59/1.67 geometry), Accelerates ramp, ConditionYellow x.75, uniform-cost A* with occupancy costs, PathDelay/BlockagePathDelay/CloseEnough blocked moves; 18 witness lines) `roster_hash=e68a5b05a8a64547` (net-l0 re-pin, was `7d252f859ed1a698`: original FastMath (A/B: the libm-only build prints this value)) `roster_hash=7d252f859ed1a698` (W6-04 re-pin, was `2ead1c5b7f8b7b93`: bisected to the W6-04-01 Burst commit afaccad: the Apocalypse fires its 120mmx / MammothTusk Burst=2 volleys; same 18 witness lines) `roster_hash=2ead1c5b7f8b7b93` (W2-SK re-pin, was `68732c5f60b6ef23`: W2-SK-25 guard-range acquisition/pursuit among the spawned roster; same witness lines) `roster_hash=68732c5f60b6ef23` (W2-AN re-pin, was `9c57c02eb36e9872`: same witness lines; the hashed unit state changes -- ORCA/BEAG resolve as fixed-wing aircraft (W2-AN-02) and the new per-unit naval/air type data) (earlier: `9c57c02eb36e9872` (final-verification re-pin, was `333cd43d8b5ece6a`: moved at gap A 5948a41 -- `load_building_assets` honours rules `Image=` for every roster index, so TESLA draws as NATSLA and ATESLA as GAPRIS; ATESLA is now art-present and spawned, `total_buildings=8` (was 7)) | `deterministic=yes`; 16 new types (7 Soviet + 5 Allied units in kRosterUnits + NANRCT/ATESLA/NASAM/GAAIRC in kRoster), resolved generically (Image= overrides honored) + spawned (Allied full base + explicit add_building + add_combat_unit); APPEND-ONLY, kUnits/kSoviet untouched; wedge guards armed (`docs/re/roster.md`). **NOTE — this hash is INSTALL-SPECIFIC:** it folds which roster types are art-present. Before gap A the ATESLA art lookup used the id (`ATESLA`, absent) instead of `Image=GAPRIS`, so no instance was placed; since 5948a41 GAPRIS resolves and an `ATESLA` instance spawns. |
| `--demo-repair` | `repair_hash=30e49667c9c6e52d` | `deterministic=yes lines=8`; repair = a GLOBAL 14-frame phase (`trunc(RepairRate*900)`, NOT 15) × +8 HP × `stepCost=max(1,trunc((Cost/(Strength/RepairStep))*RepairPercent))` with BOTH divisions integer (GACNST 3/step, GAPOWR 1/step); `steps=6 first=0 gap=14 heal8=yes charge=yes`; auto-stop at full; silent stop when broke (`hp=718->718`, no partial step, no free repair); refusals `enemy/under_construction/unsellable_caoild/already_selling=refused`, `full_health_conyard_sellable=yes` (NO last-ConYard rule); ConYard sell = a 30-frame MISSION, `paid_early=no paid=1500 survivors=3`, footprint freed; GAREFN `Soylent` sale `paid=300 survivors=1`; 25 wedge guards armed (`docs/re/repair-sell.md`) |
| `--demo-projectile` | `projectile_hash=d1cd26e6fe1a6d4c` (W7 re-pin, was `235153e0d6b918fc`: W7 speed model (docs/gap/wave7/11-locomotion.md): Walk/Hover move raw leptons (no GameSpeedBias), Drive/Ship drive the original track tables (curved bends, x1.59/1.67 geometry), Accelerates ramp, ConditionYellow x.75, uniform-cost A* with occupancy costs, PathDelay/BlockagePathDelay/CloseEnough blocked moves; same witnesses) `projectile_hash=235153e0d6b918fc` (net-l0 re-pin, was `1225c466bde4764d`: FastMath (A/B `6bebc8975b7b7b8d`) then whole-frame timers) `projectile_hash=1225c466bde4764d` (W5-B re-pin, was `e46dbf715c54d2d3`: detonations through DamageArea (the Rhino shell's AP CellSpread .3) and the close-enough re-aim) `projectile_hash=e46dbf715c54d2d3` (W2-SK re-pin, was `44cdd50b67f276e9`: W2-SK-25 guard-range acquisition shifts the launch frames) `projectile_hash=44cdd50b67f276e9` (gap fix C re-pin, was `1ff09b8a3d3eecdd`: G-42 speed + G-50 rearm jitter shift frames; every witness value unchanged) | `deterministic=yes`; P2 Projectile flight: firewall (instant path when not enabled, `events=0`), Rhino arc (`speed=90` == `ftol(sqrt(d*6*1.2))`, 13 frames, apex 175, damage only at detonation), miss (target walked off -> `victim=-1 damage=0`), Kirov bomb (`speed=1` from `z=610`, 37 frames, both splash targets hit), Flak Track scatter (5/5 shots scattered by the dedicated RNG) (`docs/re/projectile.md` §12) |
| `--demo-triggers` | `trigger_hash=3e88854b5f342a9a` (W6-04 re-pin, was `d43ff81dac21635b`: W6-04-17/18/21: sov07tmd's YR structures (Battle Bunkers ...) resolve with their YR art -- `placed structures=83 -> 91`, `buildings=83 -> 91`; every trigger fire/action line identical, lines=76) `trigger_hash=d43ff81dac21635b` (W5-FIX-C re-pin, was `b8cd267e98f5e002`: `placed structures=82 -> 83` -- the roster gained GASPYSAT (index 39), so `place_map_objects` resolves one more of `sov07tmd`'s `[Structures]`; every trigger fire/action line identical, lines=76) `trigger_hash=b8cd267e98f5e002` | `deterministic=yes lines=76`; **re-pinned by gap F (G-95)**: bisected on the first-parent merges -- P5 84b8217 prints the old `20347f4dfa462eb4` with `placed structures=66`, P6 7e754d5 prints `02f4226456db131d` with 70, gap A 5948a41 (and master fddbcb3, and gF) print `b8cd267e98f5e002` with 82; the witness diffs ONLY in that placed-structure count and the end-of-run building total (the bigger rosters let `place_map_objects` resolve more of `sov07tmd`'s `[Structures]`), every trigger fire/action line is identical; `sov07tmd.map`'s own 126 triggers for 2700 frames: 18 fires (logic 14, house 4, variable 2), TaskForce reinforcements at team waypoints, recorded presentation; wedge guards armed (`docs/re/map-triggers.md`) |
| `--demo-veterancy` | `veterancy_hash=a8e3fc10a1787f12` (W7 re-pin, was `veterancy_hash=7d5ed5c6a9363115`: W7 speed model (docs/gap/wave7/11-locomotion.md): Walk/Hover move raw leptons (no GameSpeedBias), Drive/Ship drive the original track tables (curved bends, x1.59/1.67 geometry), Accelerates ramp, ConditionYellow x.75, uniform-cost A* with occupancy costs, PathDelay/BlockagePathDelay/CloseEnough blocked moves) `veterancy_hash=7d5ed5c6a9363115` (net-l0 re-pin, was `3b814501fb6dbd48`: whole-frame rearm timers (the libm-only build keeps the old pin)) `veterancy_hash=3b814501fb6dbd48` (gap fix C re-pin, was `1f02e4c09dba0964`: kill frames shift with G-42/G-50; ranks/dmg/ROF witnesses identical) | `deterministic=yes lines=31`; real E1 hero kills 12 weaponless E2: veteran at kill 6 (dmg 15->16, ROF 20->12), veteran victim counts x2, elite at kill 12 (M60E 25->27); elite first hit taken 10 vs rookie 15 (VeteranArmor); +4 HP self-heal over 56 frames; crates: 1 initial crate, typed Money/Armor/Armor->Money/Firepower/Speed/Veteran/HealBase/Reveal/Unit + one weighted random pick, expiry relocation; wedge guards armed (`docs/re/veterancy.md`) |
| `--demo-roster2` | `roster2_hash=2e52b1c0eb61c0e0` (W7 re-pin, was `roster2_hash=9e15ca1af94e8d9c`: W7 speed model (docs/gap/wave7/11-locomotion.md): Walk/Hover move raw leptons (no GameSpeedBias), Drive/Ship drive the original track tables (curved bends, x1.59/1.67 geometry), Accelerates ramp, ConditionYellow x.75, uniform-cost A* with occupancy costs, PathDelay/BlockagePathDelay/CloseEnough blocked moves) `roster2_hash=9e15ca1af94e8d9c` (W6-04 re-pin, was `fb2ce78432058b4a`: W6-04-17/18/21: a roster building the RA2 archives lack takes its YR art (NAINDP `art=n -> y`) and GAROBO joins the roster -- lines 74 -> 75) `roster2_hash=fb2ce78432058b4a` (final-verification re-pin, was `b23b7d9924a99c85`: moved twice inside gap C 477ab70 -- b3ade7e Ini inline-`;` comment cut (RC-5) makes `GAWEAT Image=GAWETH ;GEF ...` resolve, GAWEAT art-present 3x3 (14 art-present, was 13) -> `72ceaad0c333e4bc`; then 1d074af G-42 speed scale -- ORCA/BEAG `speed_lpf` 179.2 -> 56.0, fire f=77/81, land f=152/160, `Reloaded` f=422/430 (+270)) | `deterministic=yes lines=74`; 15 appended buildings (14 art-present) + 24 unit bodies; wall fills `0/3/2` (free), corner mask `E|S=6`, wall `build_frames=189 (x3.0 of 63)`; naval factories `GAYARD,NAYARD` vs land `GAWEAP,NAWEAP`; GAYARD `water_ok=1 land_refused=1 water_blocked=1`; DEST `total_frames=630 step_ticks=11 ready_after=594` -> water exit; ORCA/BEAG pads 0/1, fire f=77/81, land f=152/160, `Reloaded` f=422/430 (+270); INSTALL-SPECIFIC like `--demo-roster` (`docs/re/roster2.md`) |
| `--demo-abilities` | `abilities_hash=e4f2a823a5f6b755` (W7 re-pin, was `abilities_hash=bc6054f3c1704e13`: W7 speed model (docs/gap/wave7/11-locomotion.md): Walk/Hover move raw leptons (no GameSpeedBias), Drive/Ship drive the original track tables (curved bends, x1.59/1.67 geometry), Accelerates ramp, ConditionYellow x.75, uniform-cost A* with occupancy costs, PathDelay/BlockagePathDelay/CloseEnough blocked moves) `abilities_hash=bc6054f3c1704e13` (w7fix-units re-pin, was `375f7d8fc5bd5313`: W7-03-02 [RE 0x735A60 Turret=0 branch]: the turretless Terror Drone turns its hull before its jump, so infect / first tick / exit come 2 frames later (15 -> 17, exit 435 -> 437); every other stage witness unchanged) `abilities_hash=375f7d8fc5bd5313` (W6-04 re-pin, was `65639c87586d62df`: bisected to the W6-04-03/12 commit 392cf1f: Ivan / Terrorist / death-weapon / eruption blasts go through the faithful DamageArea (ring cells, 3-D, sub-cell offsets, AffectsAllies, ProneDamage); the 9 witness lines are identical, the hashed per-unit HP after the blasts moves) `abilities_hash=65639c87586d62df` (post-merge re-pin, was `732afedaff7cab7f`: bisected on the first-parent merges -- p2b 1a02ca6 prints the old hash, the P3 merge 78a42eb the new one; P3's C4 rule ([Sapper] CellRangefinding=yes: the plant range is measured from the sapper's CELL to the building centre + (w+h)*64, and the sapper walks to the nearest cell in that reach) makes Tanya plant one frame later, `stage5 c4: plant=74 blast=101` -> `plant=75 blast=102`, same 27-frame delay; every other witness line identical) `abilities_hash=732afedaff7cab7f` (gap fix C re-pin, was `e510c5b98c089804`: G-42 speed, G-50 jitter; witnesses identical) | `deterministic=yes lines=9`; P4: drone vs Rhino (first tick immediate, 8 x 50 every 60 frames, drone pops out), drone vs GI (1 tick = 125, exits same frame), Desolator deploy (4 eruptions, center 500 -> 940 after Add, first hit 52 at 5 cells, 16-frame phase, immune), CLEG (10-cell teleport immediate + 53 frozen, GI erased in 157 frames without dying), Tanya C4 (27 frames, sale refused while armed), Ivan (blast on plant+451, neighbor 400->175), Terrorist (1 suicide -> 1 TerrorBomb), IFV (GI -> mode 2 ROF 15, empty -> 0, Desolator -> mode 9 dmg 175); wedge guards armed (`docs/re/abilities.md`) |
| `--demo-saveload` | `saveload_hash=fe867e2d791c1d16` (W7 re-pin, was `saveload_hash=9681c8c5f7739226`: W7 speed model (docs/gap/wave7/11-locomotion.md): Walk/Hover move raw leptons (no GameSpeedBias), Drive/Ship drive the original track tables (curved bends, x1.59/1.67 geometry), Accelerates ramp, ConditionYellow x.75, uniform-cost A* with occupancy costs, PathDelay/BlockagePathDelay/CloseEnough blocked moves) `saveload_hash=9681c8c5f7739226` (w7fix-combat merge re-pin, was `323c419c66e7fdb4`: save v39 (UnitTgt mission model, UnitTypeTgt wave-7 combat keys, World W7 combat rules / house IQ / hit log) and the armed skirmish leg follows the W7 mission model (threat scan, retaliation Override_Mission, area guard, crush warning): skirmish witnesses `launches=10 produced=8 vet=1 abil=91`; A/B: the same tree with `W5_OFF=w7` (W7 combat rules unarmed) prints `launches=9 produced=8 vet=0 abil=99` and `25e6e3c61b69dbe9`, the v39 format change on the pre-W7 fight; `identical=yes`) `saveload_hash=323c419c66e7fdb4` (w7 pres merge re-pin: save v38 adds InfAnimState erupt/air + World inf-death/fragment state; both round trips match) `saveload_hash=bef10af68ca85223` (w7 ai merge re-pin: save v37 adds Unit::ai + AiHouseConfig auto-deploy/to-protect; both round trips match) `saveload_hash=31a22c246813d378` (w7 units+airnav merge re-pin: save v36 = units v35 + airnav v35 fields; airnav alone was `b5145603a8e9460d`) `saveload_hash=d65ddfd87cfca094` (w7fix-units re-pin, was `6f2350c85771f15d`: save format v35 (UnitTypeW7 / UnitW7 / BldW7, WeaponSlot NeverUse, BldTypeW5 CanC4, URepairRate, World side disguises) and the wave-7 unit changes; identical=yes) `saveload_hash=6f2350c85771f15d` (net-l0 re-pin, was `665ba021ee0c7414`: save format v34 (int frame timers + session seed) and the sim changes above) `saveload_hash=665ba021ee0c7414` (NET L1 re-pin, was `2d2119b4eb5cafd2`, save format v32 -> v33, `identical=yes`: Unit::selected is no longer serialized (the selection is the local player's UI state) -- one byte less per unit; A/B: the same tree with a dummy `bool` written in its place prints `2d2119b4eb5cafd2`; every witness line identical) `saveload_hash=2d2119b4eb5cafd2` (L2 re-pin, was `aeb199b4ed5ea5dd`, save format v31 -> v32, `identical=yes`: the hash folds the save BYTES and the format changed -- World gains the per-house economy (econ_ / human_mask_ / house_outcome_ / victory_cheered_), loses wallet_hint_, HouseAiRec loses credits/queues, SceneSave loses the wallet/queues; the gate's player production moved from its SceneSave copy into World::step_player_economies. Every simulation witness is unchanged against master 8b1a76a (skirmish shells_at_save=2 launches=9 produced=8 vet=0 crates=0 abil=142 rad_sites=1; triggers fired_at_save=17 fired_total=18 log_lines=72; scene identical_at_load=yes identical_after_120=yes); the other 26 gates print master's values byte-for-byte) `saveload_hash=aeb199b4ed5ea5dd` (last-fix re-pin, was `981125da21baf72f`, save format unchanged (v31), `identical=yes`: an AUTO-acquired target that vanishes mid-approach no longer sends the unit back to its guard anchor: it finishes the walk to its firing cell and guards there (combat.cpp step (d), traced: FootClass::Approach_Target FUN_004D55B0 sets a CELL Destination, which FootClass::PointerExpired FUN_004D9870 keeps -- only TarCom is cleared); A/B: the owner-change fix alone does not move it -- the same tree with the old campaign_runtime.inc prints `aeb199b4ed5ea5dd` too) `saveload_hash=981125da21baf72f` (fv-b re-pin, was `1f8d95592a4c2057`: save v31 -- WeaponSlot and UnitTypeW6 gain `bomb_disarm` (the warhead BombDisarm= flag behind the GetFireError gate FUN_006FB5D0: a DefuseKit only fires at an object carrying an Ivan bomb); A/B: the same tree with the format back at v30 and those two `io` calls removed prints `1f8d95592a4c2057`; `identical=yes`) `saveload_hash=1f8d95592a4c2057` (fv-a re-pin, was `a388ee3e3c074c8d`, save format unchanged (v30), `identical=yes`, skirmish `match=yes`: the skirmish leg's fight differs (`abil=142 -> 101`, A/B hash `1bb3a5150022979c -> 05589f9cfdd35266`) because a vanished AUTO-acquired target ends the unit's approach and it walks back to its guard anchor (R-211, combat.cpp step (d)); A/B: the same tree without the step-(d) change prints `a388ee3e3c074c8d`) `saveload_hash=a388ee3e3c074c8d` (R-210 re-pin, was `d555a9af89492b8d`: save v30 -- Unit `on_deck` (the high-bridge deck layer), BldTypeW6 `gate` (Gate=yes) and World's derived deck grids `deck_land_`/`deck_amph_`/`deck_ver_`; A/B: the same tree with the format back at v29 and those `io` calls removed prints `d555a9af89492b8d`; `identical=yes`) `saveload_hash=d555a9af89492b8d` (W6 fix #2 re-pin, was `62eabfa5cbd454c0`: save v29 -- Unit/UnitCombat/Building `tg` (rules id, Insignificant/LegalTarget/ImmuneToPsionics/GuardRange, IsInPlayfield, area guard), World usable area + per-house handicap; `identical=yes`) `saveload_hash=62eabfa5cbd454c0` (W6-04 re-pin, was `0a724e5f98ec7f2d`: save v28 -- wave-6 #4 state: WeaponSlot/WeaponStats/WeaponSpec Burst, OmniFire, Sonic, wall keys, shrapnel, MinimumRange; Unit/Building w6 (burst index, wall stage, defence structure target, powered-unit flag); map wall overlay stages; IFV elite weapons; `identical=yes`) `saveload_hash=0a724e5f98ec7f2d` (W6-FIX-PRES re-pin, was `c1e6c6ffc71db33d`: save v27 -- MapTriggerSystem gains `timer_text_`, the trigger action 103 TimerText label the bottom-right mission timer shows (W6-05-04); before the master merge the same change moved v24 `4cf40c63a14f07e7` -> v25 `bb9d7ad67e98f3cb`, and rebuilding that branch with the format back at v24 and the field's `io` removed printed `4cf40c63a14f07e7` again; `identical=yes`) `saveload_hash=c1e6c6ffc71db33d` (w6fix-ai re-pin, was `4cf40c63a14f07e7`: save v25/v26 -- the skirmish AI state is now serialized (BuildingTypeDef wall/insignificant, HouseAiRec anger table + hate-delay clock, AiHouseConfig virtual purifiers / AI rules knobs, raise-money targets); w6fix-ai alone already prints this hash, and the merge with w6fix-rel (v26) does not move it; `identical=yes`) `saveload_hash=4cf40c63a14f07e7` (post-merge re-pin, was `ea91172edde017b6`: save v24 -- Unit gains `w_mind_control` (the campaign mind-control shot captures and deals no damage, re-armed each frame by CampaignRuntime; serialized because every Unit member is); `identical=yes`) `saveload_hash=ea91172edde017b6` (P4 re-pin, was `e75c88820eab9881`: save v23 -- BldTypeW5 gains the building death weapon (explodes / delay_kill_eligible / DeathWeapon spec), WeaponSpec the warhead DelayKill fields, World the crushable-wall state crush_wall_ (+ derived crush grids); `identical=yes`) `saveload_hash=e75c88820eab9881` (W5-A re-pin, was `c27c4a1e77e57c09`: save v22 -- the campaign host section gains the TurnOff/On flags, AI superweapon quarries, FireSale houses, teams that left the map and the ResizeView area; World serializes the wall leak mask and SW quarry; AiHouseConfig fire_superweapons) `saveload_hash=c27c4a1e77e57c09` (W5-E2 re-pin, was `1a73891b550afe52`: save format v21 writes `Building::power_off` for every building; the W5-B build prints the old pin, this branch the new one. Earlier: W5-B re-pin, was `420ac604a50be366`: save format v20 adds the sub-cell occupancy, Unit spot/sub offset/speed/crush fields, land-speed/crush/elevation rules, AffectsAllies, projectile SubjectTo*; the real-GameScene segment arms sub-cells; `identical=yes`) `saveload_hash=420ac604a50be366` (W5-FIX-C re-pin, was `3a946fb75996d946`: save format v18 adds the wave-5 ability state -- World `w5_` + gap scratch, Unit/Building/BuildingTypeDef/UnitTypeDef `w5`, WeaponSpec projectile fields; `identical=yes`) `saveload_hash=3a946fb75996d946` (W4-FIX-SEQ re-pin, was `0949ea204c76a2ab`: save format v16 adds AnUnit `para_alt`, SwParams `parachute_fall`, the host's `u_mission` and team airdrop flag, World player-order queue / MultiplayPassive houses / high-bridge cells / scripted-team AI demand, and the campaign segment's one-sided alliance matrix; `identical=yes`) `saveload_hash=0949ea204c76a2ab` (W4-FIX-PROJ re-pin, was `b029b2825cf11e6f`: save format v15 adds the per-type RocketParams in SwParams, the RocketLocomotion flight + rocket kind/speed/NoShadow/elite/end state on AnType/AnUnit, the homing projectile state on Projectile/ProjectileSpec, ProjectileRules MissileROTVar/MissileSafetyAltitude and World::proj_seq_; `identical=yes`) `saveload_hash=b029b2825cf11e6f` (W3-B re-pin, was `ef78bd7f13287dbc`: save format v14 adds the infantry sequence/fear state `Unit::ia`, the warhead `InfDeath`/`ProneDamage` on Unit/UnitCombat/WeaponSlot/WeaponStats, and World's live-infantry switches + per-image sequence tables; the real-GameScene segment arms the infantry machine; `identical=yes`, A==B everywhere) `saveload_hash=ef78bd7f13287dbc` (W2-SK re-pin on top of W2-CA, was `c19113168b76bae7`: save format v13 adds the W2-SK state -- ore mines + original growth timing, harvest scan/skip/income, wallet hints, depot/capture/spy/factory type flags, shrapnel/Mirage unit fields, AI enemy house / MultipleFactory; W2-SK-25 guard-range pursuit makes the opening fight close in sooner, so the skirmish segment saves from frame 10 (shells_at_save=2); A==B everywhere) `saveload_hash=c19113168b76bae7` (W2-CA re-pin, was `8291b85b9957b34a`: save format v12 adds HouseRec `player_control`, World `ally_pairs_`/`entered_events_`/`track_entered_` and the campaign host's mind-control links; `identical=yes`) (earlier: `8291b85b9957b34a` W2-AN re-pin, was `653c22b544488e64` on master 844033b: save format v10 adds the W2-AN state -- Unit/UnitCombat `an`, Building `iron_left`, World `an_` + per-type SW lists + AI catalog `naval_yard` + `nav_factory_types`; `identical=yes`. Earlier: W2-GA-03 re-pin, was `7fde8a2b895cf6ae`: save v9 serialises the FogOfWar flag + fog-off targeting in the skirmish scenario; witnesses unchanged. Earlier: G16 re-pin on top of D+F `4876e19f292871e3`: save format v7 adds the garrison/bridge state -- Unit occupier/occupy weapons/garrison_of/ground target, Building occupants, World depth rules + compact bridge grids; the gate map has low bridges; `identical=yes`, two runs equal) | `identical=yes`; **combined re-pin (D then F)**: save format v6 carries both gap D's AI fields (D pinned `2511ec3d0deec668` at v5) and gap F's MCV deploy fields (Unit deploys_into/deploy_mission/goal/deployed_away, UnitCombat deploys_into, BuildingTypeDef deploy_facing, World deploy state), so the save bytes and A/B digests change; every round trip still matches (skirmish, triggers, GameScene identical at load and after 120 frames); P7: skirmish saved at frame 121 (2 shells in flight, 14 launches, 8 produced, 151 ability events, 1 radiation site) -> `SAVEE0E7.SAV` 1.29 MB -> fresh scenario -> frame 900 `A==B`; sov07tmd triggers saved at 900 (17 fires) -> 1900 (18 fires) `A==B`; real GameScene save->load identical at load and after 120 frames (`docs/re/saveload.md`) |
| `--demo-campaign` | `campaign_hash=6c2c94bbc12a44f9` (W7 re-pin, was `campaign_hash=d2b95f1406f2bd34`: W7 speed model (docs/gap/wave7/11-locomotion.md): Walk/Hover move raw leptons (no GameSpeedBias), Drive/Ship drive the original track tables (curved bends, x1.59/1.67 geometry), Accelerates ramp, ConditionYellow x.75, uniform-cost A* with occupancy costs, PathDelay/BlockagePathDelay/CloseEnough blocked moves) `campaign_hash=d2b95f1406f2bd34` (net-l0 re-pin, was `359c517cac6223c4`, lines 785: FastMath + whole-frame timers; also matches on Android x86_64 after the CampaignRuntime dangling-reference fix) `campaign_hash=359c517cac6223c4` (last-fix re-pin, was `7216e37c22014b1a`, lines 785 unchanged, both runs identical: two causes, A/B-separated. (1) an AUTO-acquired target that vanishes mid-approach no longer sends the unit back to its guard anchor: it finishes the walk to its firing cell and guards there (combat.cpp step (d), traced: FootClass::Approach_Target FUN_004D55B0 sets a CELL Destination, which FootClass::PointerExpired FUN_004D9870 keeps -- only TarCom is cleared) -- the same tree with the old campaign_runtime.inc prints `7502dcacb1020bfa`, the pre-fv-a pin; (2) every unit owner change (mind control, its release, team/trigger ChangeHouse, AllChangeHouse) goes through World::set_unit_owner = TechnoClass::SetOwningHouse FUN_00700940: target and move dropped, Guard where it stands -- the no-input ALL01t run still ends LOST at f698 with the same 28 fires, but with 11 player units left (was 10); the full witness diff is that one `[camp] end ALL01t` line) `campaign_hash=7216e37c22014b1a` (fv-a re-pin, was `7502dcacb1020bfa`, lines 785 unchanged, both runs identical, saveload `world_same=1 triggers_same=1`: SOV01t's "Unlock Input 1" fires at f1063 (was 981) and ALL01t raises 141 -> 142, because a vanished AUTO-acquired target ends the unit's approach and it walks back to its guard anchor (R-211, combat.cpp step (d)); A/B: the same tree without the step-(d) change prints `7502dcacb1020bfa`) `campaign_hash=7502dcacb1020bfa` (R-210 re-pin, was `c3b3a0f84eb3a355`, lines 785 unchanged, every trigger fire/action line identical: an Amphibious reinforcement whose waypoint lies at sea now enters on the water there instead of on the nearest land cell -- in ALL01 the Soviet SAPC transports at wp (50,96) / (76,96) (they used to start ashore and reach the sea only by dropping a cliff, which the R-210 height gate refuses); A/B: the same tree without that placement change prints `c3b3a0f84eb3a355`, the height gate / deck layer / gates alone do not move this gate) `campaign_hash=c3b3a0f84eb3a355` (W6 fix #2 re-pin, was `660e518dd801e8a5`, lines 795 -> 785: combined W6 fix #2 campaign changes -- the W6-02-11 radius (A/B: old radius prints `bd711f09f612f899`) plus legal_threat/IsInPlayfield, team tags on recruits, rules-id tracking, full-strength teams, recruit filters, Patrol attack-move and the campaign handicap/AICM start money; ALL01 no-input still ends LOST (f608), SOV01 RUNNING at f1800, both victory runs WON; PROBABLE combined cause, not bisected line by line) `campaign_hash=660e518dd801e8a5` (W6-04 re-pin, was `abd6a675da8f57e9`: two wave-6 #4 causes, each bisected: W6-04-01 Burst (afaccad; ALL01 `raises=145 -> 143`) and W6-04-17/18/21 (the YR-art roster buildings resolve, so the maps' NABNKR/NAINDP structures are placed); lines 795 unchanged) `campaign_hash=abd6a675da8f57e9` (W6-FIX-PRES re-pin, was `dd71d9faceb255e1`: the full gate output diffs against master 76fd8ce in ONE witness line only -- `[camp] saveload ALL01t ... trig=16771B` -> `trig=16779B`, the 8-byte length prefix of the newly saved MapTriggerSystem `timer_text_` (W6-05-04, save v27); every trigger, team and world line is identical, lines=795) `campaign_hash=dd71d9faceb255e1` (W5-A re-pin, was `0847d9e93d9ac946`, lines 793 -> 795: the campaign World now runs the economy (wire_economy: ore growth, harvesters, refinery income), entered events are raised before the frame's trigger loops, team recruiting follows Can_Add priority/loaded-transport rules, placed objects take their Facing/Veterancy, and the new events/actions (23/31/34/53/54, 35/40/44/51/61/62/70/77/78, FireSale state) run; PROBABLE combined cause, not bisected line by line) `campaign_hash=0847d9e93d9ac946` (W5-B re-pin, was `f65155e11b36a00a`, lines 793 unchanged: the mission fights resolve through DamageArea / 3-D range; both maps still reach their scripted outcomes) `campaign_hash=f65155e11b36a00a` (W5-FIX-C re-pin, was `d602a4c7109a9756`, lines 793 unchanged: the campaign worlds now carry the GASPYSAT roster entry and the wave-5 type data (prism/tesla/tech-building flags, stolen-tech roster gate); PROBABLE cause -- not bisected line by line) `campaign_hash=d602a4c7109a9756` (W4-FIX-SEQ re-pin on top of W4-FIX-CTL, was `9261ef609a25c962`, lines 784 -> 793: trigger 63 Apply100Damage now breaks the bridges (`FUN_006dfa00` -> `FUN_00586bb0`): ALL01 `Break Bridges` `-> ok [bridge cells broken=4/5]` and SOV01 `Break Bridge` `-> ok [bridge cells broken=1/3]` (were `skipped`); campaign houses use their own one-sided `Allies=` lists, team transports/drop planes/garrison scripts follow the recovered TeamClass rules (the gate's World registers no PDPLANE, so Droppod teams keep the waypoint fallback, `plane=-1`); ALL01 no-input still ends on the map's own `Mission Failed` chain (Tanya into the Liberty strip -> paratroopers -> V3 -> `Enable Failure` on her death), now at f=608 (was 728), SOV01 RUNNING at f=1800, both victory runs WON) `campaign_hash=9261ef609a25c962` (W4-FIX-CTL re-pin, was `d6f8eda4e434b348`, lines 765 -> 784: W4-MC-01 makes Tanya amphibious (`MovementZone=AmphibiousDestroyer`), so in the no-input ALL01 run her scripted team now swims into the Liberty Island trigger cells -- raises 45 -> 291, fired 22 -> 27, she dies and the map's own Lose trigger ends ALL01 at f=728 (`outcome=LOST`, was RUNNING at f=1800); SOV01 and both victory runs identical. A/B: the same tree with the old exact `Amphibious` match prints `d6f8eda4e434b348`) `campaign_hash=d6f8eda4e434b348` (W2-SK re-pin on top of W2-CA, was `5d033b6aa019beaa`, lines 739 -> 765: W2-SK-25 guard-range acquisition/pursuit and W2-SK-05 nearest-reachable moves change the mission fights; both maps still reach their scripted outcomes) `campaign_hash=5d033b6aa019beaa` (W2-CA re-pin, was `760c46ae2eed28b4`, lines 737 -> 739: ALL01's aircraft reinforcements (cargo plane / FALC teams at wp 83/85/87/89) are now placed (W2-CA-16) so `Transport Retreat 1` fires at f=665 and 4 more teams exist (13 vs 9, raises 48 -> 45); SOV01's start team recruits all 18 members incl. the SMCV (W2-CA-09: no `!moving()` filter, finished teams release) and its script's Deploy action (W2-CA-17) unfolds the SMCV -> `Con Yard Exists` fires (house loop 1); every other line identical; master-equivalent w2fix-air build prints the old hash) (earlier: `760c46ae2eed28b4` W2-AN re-pin, was `b1d3c453c416e4fb`: ALL01 death raises 45 -> 48 at f=1800 -- its ships now fight with the naval rules (sub cloak + sonar, NavalTargeting, Destroyer ASW spawner, W2-AN-12/13/14); every other line identical) (earlier: `campaign_hash=b1d3c453c416e4fb` (combined gap D + gap F re-pin) | `deterministic=yes lines=737`; **combined re-pin**: gap D (`17830b0f7c233223`, lines 737: campaign enemy houses build out from ProductionBegins/AI triggers) plus gap F: actions 108 executed (logged `CreateCrate ... -> skipped` in the gate, whose World has no crate system), ALL01 reinforcement ships on water instead of the nearest land cell (fewer EnteredBy raises), death raises in the recovered order 7, 48, 29; P10: Allied 1 + Soviet 1 (1800 frames, scripted walker over the cell-tag trip-wires) and their scripted-kill victory runs reaching `WON` through the maps' own triggers (next = `ALL02s.MAP` / `sov02t.MAP`), each rendered + headless; save/load continuity (Allied 1 saved at f300 with World+TRIG+HOST, identical at f600); wedge guards armed (`docs/re/campaign.md`) |

**Gap owner A re-pins (real-play economy/production, docs/gap G-20..G-30).**
--demo-skirmish e9ad3f1fd88495f9 -> fde3f587e747f765 and --demo-maps
6e516005f120a1bb -> ab66922dab1b24f6: launch_skirmish (the World GameScene
plays on) now arms the economy + water grid, no longer grants a frame-0 Nuke,
and AI houses hold the lobby Money once (the liveness signature now also
reads the AI wallet, since the drained pool seed no longer moves). 
--demo-saveload b71d77c2b4abb7b8 -> 270b4fce163438f7: same launch_skirmish
change plus six production queues (save format v3). Also expected to move,
not re-run here: roster/techtree/base/AI/trigger gates (building art now
honours Image= for every roster index -- ATESLA/TESLA became art-present)
and gates whose harvesters belong to house 2+ (paid to their own pool).
--probe-realplay (not hashed) drives the real GameScene and asserts the
fixed behaviour: wallet, harvest income, roster per side/country/tech,
strip, CMIN/E2/Rhino/Tanya/Destroyer production, Naval Yard on water, wall
fill + 3x time, SW by building + IsPowered pause, radar power gate, Ore
Purifier bonus.

## Save / Load (P7)

Calibration authority: `docs/re/saveload.md` (string scan + YRpp layout map +
a headless Ghidra pass over `LoadOptionsClass` / `ScenarioClass::SaveGame`);
spec `docs/superpowers/specs/2026-10-06-saveload-design.md`, plan
`docs/superpowers/plans/2026-10-06-saveload.md`.

**Guarantee.** `load(save(state))` continues bit-identically: the World
(units incl. cargo/flight/naval/capture/disguise/veterancy/ability state,
buildings incl. repair/sell/C4/bomb/erase state, houses/alliances/victory,
credits, ore + ore RNG, shroud, occupancy/passability, AI base/team/trigger
state and its resolved config, superweapons, projectiles in flight +
`ProjRng`, crates + crate RNG, radiation field, every firewall bool and pending
event buffer), the map-trigger runtime (`MapTriggerSystem`: tags, triggers,
timers, latches, locals/globals, mission timer, RNG, log) and the GameScene
record (camera, sidebar tab, the player's production queues, wallet, rolling
credits, FreeUnit tracker). Transient input (armed repair/sell/placement)
resets on load, as in the original.

**Format (decision).** The original writes an OLE compound file of raw class
memory images -- not reproducible. We keep its *naming* (`SAVE%04X.SAV`,
random number retried until free), its header-first newest-first listing,
the 80-char description and the UI flow, and use our own versioned binary:
`RA2SAVE\0` magic, `u32` format version (`save::kFormatVersion`, a mismatch
is listed but refused = the original's `IsWrongVersion`), a length-prefixed
header (description, scenario kind, map file/id/signature, frame, time), then
tagged sections `CONF`/`WRLD`/`TRIG`/`SCNE`/`HOST` (unknown tags skipped).
Scalars are fixed-width little-endian; `long` is always 8 bytes; doubles are
bit patterns. Files live in `<SDL pref dir>/Saved Games/`.

**Growth.** `engine/save_fields.hpp` holds ONE visitor per struct, one line per
field, shared by save, load and a counting archive; classes with private
state carry one `friend struct SaveAccess;` line. Three tripwires in
`tests/test_saveload.cpp` fail when state is added without a visitor line:
aggregate arity == visited count for every serialized struct (mutation-checked),
a source scan proving every `World` / `MapTriggerSystem` member is visited,
and pinned `sizeof`s (MinGW x64) for the private-state classes. A rich-world
round trip (shells in flight) must continue byte-identically for 90 frames.

**UI.** In game, the sidebar **Options** button (or F10) opens the options menu
(dialog 0xB5): Load / Save / Delete / Abort / Resume; the game is paused while
it is open. The shared Load/Save/Delete dialog lists saves newest first; Save
adds `<New Saved Game>` + a description edit (default the scenario name,
empty refused), overwrite and delete confirm yes/no. The main menu's
**Load Saved Game** (Single Player page) is now live.

**Gate `--demo-saveload`.** Skirmish (economy, projectiles, veterancy, crates,
P4 abilities, AI, the player's production pool) saved at the first frame
>= 90 with shells in flight (121) and continued to 900; the sov07tmd trigger
scenario saved at 900 and continued to 1900 (straddling the 1800-frame taunt);
each save goes through a real `.SAV` file into a FRESH scenario, and the
full-state hash (FNV over the serialized World + scene/trigger bytes, mixed
with `world_hash`) must match. A third segment drives the real `GameScene`
`save_game` -> load constructor and compares the complete state at load and
after 120 more frames.

**Cuts.** The interactive GameScene does not run map triggers yet (P10), so
in-game saves have no `TRIG` section; audio state is not saved; no
"Saving game..." bar graph; no free-disk check; Abort Mission exits the game
(no MenuArt reachable from GameScene); visuals are our rect-font widgets.

## Unit Special Abilities (batch 1)

Phase P4 gives the specialists their identity, calibrated from the binary
(`docs/re/abilities.md`): **Terror Drone** -- the shot is an infection; the
drone rides the victim off-map, ticking every `ROF=60` frames (first tick
immediate), `50 x Verses` to vehicles, an instant kill (damage = Health,
defenses ignored) to infantry; it pops back out when the host dies, unless the
host was suppressed (`2*damage - SuppressionThreshold` frames). **Desolator**
-- `D` deploys; `RadEruptionWeapon` fires at its own cell every 60 frames and
creates/`Add`s a RadSite (spread = `ftol(CellSpread)` = 10 cells, linear
falloff, level 500 accumulating, cells decay `amt/LevelSteps` every 90 frames);
foot units take `ftol(ftol(min(lvl,500))*0.2) x [RadSite] Verses` on the
absolute 16-frame phase. **Chrono Legionnaire** -- every move is a teleport
(immediate relocation, then frozen `max(dist/48, 16)` frames); its beam erases
(`Strength*10` budget minus the attached legionnaires' Damage per frame; a GI in
157 frames; removed, not killed). **Tanya / Navy SEAL** -- ordered onto a
building they walk to Sapper range and plant C4 (`ftol(C4Delay*900)` = 27
frames, then a lethal ignore-defense blast); a C4'd building cannot be sold (the
`can_be_sold` C4 hook is now live). **Crazy Ivan** -- plants one bomb per
target, detonating on `plant+451` or when the target dies (`IvanDamage=450`,
`IvanWH` 1.5 cells, all houses, buildings too); his own death does nothing.
**Terrorist** -- a `Suicide` weapon: firing kills him and his `TerrorBomb`
death weapon blasts (also on any death); no passive acquire, no retaliation.
**IFV** -- `Gunner=yes`: the weapon is `Weapon[IFVMode of the passenger]`
(GI -> `CRM60`, Desolator -> `CRRadBeamWeapon`, Ivan -> suicide `CRNuke`, CLEG ->
erasing `CRNeutronRifle`), back to `HoverMissile` when empty.

**Firewall:** profiles reach units only through
`World::register_ability_profile` (bound by image), called by `GameScene` and
`--demo-abilities` only (never `launch_skirmish`); `step_abilities` returns at
once otherwise. Every edit to an existing pass is a guard on a default-inert
field (`frozen`, `abil.fire_kind`, `abil.c4`, `abil.teleporter`,
`abil.no_passive_acquire/no_retaliate`, `Building::c4_applied`). Ability shots
are intercepted before the P2 projectile launch (abilities are instant).

**Cut / carried forward:** drone removal by Service Depot / IFV repair (no unit
repair system), the squid, C4 on ships, bombs on friendlies, mind control,
radiation lighting, effect anims/sounds, the erase-target damage immunity and
the engine-wide min-1 damage clamp (`abilities.md` section H). Distances are
2-D. Suppression is measured per frame.

## Android (arm64-v8a) cross-compile

This proves the library is portable: the same sources in `src/` build for
Android via the NDK's own CMake toolchain file, with no source changes.
This section is the library-only cross-compile check (confirming
`libra2assets.so` is a real AArch64 ELF shared object). The playable app is
the APK: see "Android app (APK)" below; it runs on the `ra2x86` emulator and
on phones (`docs/INSTALL.md` section 5 explains how to report real-phone
frame rates).

Prerequisites: Android NDK r27c for Windows
(https://dl.google.com/android/repository/android-ndk-r27c-windows.zip),
unzipped so that `$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake`
exists, e.g.:

```powershell
Invoke-WebRequest -Uri "https://dl.google.com/android/repository/android-ndk-r27c-windows.zip" -OutFile "D:\android-ndk-r27c-windows.zip"
Expand-Archive -Path "D:\android-ndk-r27c-windows.zip" -DestinationPath "D:\"
$env:ANDROID_NDK_HOME = "D:\android-ndk-r27c"
```

Configure and build the library targets (the test/CLI executables reference
`RA2_INSTALL_DIR` game-install paths meant for the host and aren't part of
this cross-build):

```powershell
cmake -S . -B build-android -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE="$env:ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" `
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 `
  -DRA2_BUILD_SHARED=ON
cmake --build build-android --target ra2assets ra2assets_so
```

`RA2_BUILD_SHARED=ON` additionally builds `ra2assets_so` (installed as
`libra2assets.so`), a shared target compiled from the same sources as the
static `ra2assets` library, so the resulting `.so` actually contains the
library's real code (verifiable with `llvm-nm`/`llvm-readelf`), proving the
sources are portable to Android/arm64 as an ELF shared object too, not just
as a static archive. Verify the result is AArch64:

```powershell
python -c "
import struct
with open('build-android/libra2assets.so','rb') as fh:
    m = fh.read(20)
assert m[:4] == b'\x7fELF'
machine = struct.unpack('<H', m[18:20])[0]
print('machine=0x%x' % machine, 'AArch64' if machine == 0xB7 else 'NOT-ARM64')
"
```

Expected output: `machine=0xb7 AArch64`.

## Android app (APK — arm64-v8a + x86_64)

`android/` is a self-contained SDL2 Android project that packages the whole
engine (all of `engine/*.cpp` + the `ra2assets` library from `src/`) into an
installable **arm64-v8a** APK. `SDLActivity` loads `libSDL2.so` + `libmain.so`
and calls `SDL_main` (our `engine/main.cpp`), which on Android opens the same
main menu as the desktop build (`run_menu`: Single Player -> New Campaign /
Skirmish / Load). Touch input is described in the controls section (gap
owner B); this section only covers building and packaging.

The native `.so` list and the source lists come from the same
`cmake/ra2_sources.cmake` the desktop build uses, so the two cannot drift.
Game assets are **never** bundled (copyright); they are read at run time from
the app's external files dir (see "Assets on device" below).

### Prerequisites

- **Android SDK** (command-line tools) with `platforms;android-34`,
  `build-tools;34.0.0`, `platform-tools`, `cmake;3.22.1`. Install to e.g.
  `D:\android-sdk` and set `ANDROID_HOME`/`ANDROID_SDK_ROOT` to it.
- **NDK r27c** (e.g. `D:\android-ndk-r27c`).
- **JDK 17+** for Gradle (this project uses **JDK 21**). Gradle 8.9 + Android
  Gradle Plugin 8.5.2 are pinned; the bundled `gradlew` wrapper auto-downloads
  Gradle, so no system Gradle is needed.
- The desktop CMake configure must have run once so SDL2 (release-2.30.9) is
  present at `build/_deps/sdl2-src` (the jni build reuses it; override with
  `-DSDL2_SRC_DIR=` if it lives elsewhere).

Create `android/local.properties` (gitignored — machine-specific) pointing at
your SDK + NDK. The NDK key is **`ra2.ndkPath`**, not `ndk.dir`: AGP reads
`ndk.dir` itself and fails with `[CXX1100]` when both are set
(`app/build.gradle`):

```properties
sdk.dir=D:/android-sdk
ra2.ndkPath=D:/android-ndk-r27c
```

The SDK also needs `platforms;android-35` (compileSdk/targetSdk 35, W2-AR-11):
`sdkmanager "platforms;android-35"`.

### Build the APK

```powershell
$env:JAVA_HOME = "C:\Program Files\Java\jdk-21.0.12"   # the installed JDK 21 dir (was jdk-21)
$env:ANDROID_HOME = "D:\android-sdk"
# A git worktree has no desktop build/_deps; point the jni build at any
# SDL 2.30.9 source tree instead:
# $env:RA2_SDL2_SRC_DIR = "D:\ra2-android\build\_deps\sdl2-src"
Set-Location D:\ra2-android\android
.\gradlew.bat assembleDebug
```

Output: `android/app/build/outputs/apk/debug/app-debug.apk`, with both
arm64-v8a and x86_64 native libraries. **Both build types compile the native
code optimised** (W2-AR-13). `debug` is RelWithDebInfo (`-O2 -g`) and keeps
SDL's internal asserts, which the engine logs without blocking. `release`
(`.\gradlew.bat assembleRelease`) is Release (`-O3`) with SDL asserts
compiled out. The release APK is signed with the key in
`android/keystore.properties` (gitignored: `storeFile`, `storePassword`,
`keyAlias`, `keyPassword`); without that file it falls back to the debug key,
which is fine for side-loading but not for Play.

Every `.so` is linked with 16 KB-compatible `LOAD` segments
(`-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON`). Play requires this for
targetSdk 35. Check it with
`llvm-readelf -l lib/arm64-v8a/libmain.so`: every `LOAD` must show
`align 0x4000`. versionName comes from `VERSION`, and versionCode is the git
commit count.
Verify it contains AArch64 native libs (no device needed):

```powershell
python - <<'PY'
import zipfile, struct
apk = r"android/app/build/outputs/apk/debug/app-debug.apk"
with zipfile.ZipFile(apk) as z:
    for name in ("lib/arm64-v8a/libmain.so", "lib/arm64-v8a/libSDL2.so"):
        m = z.read(name)[:20]
        machine = struct.unpack('<H', m[18:20])[0]
        print(name, "ELF" if m[:4] == b'\x7fELF' else "NOT-ELF",
              "machine=0x%x" % machine, "AArch64" if machine == 0xB7 else "NOT-ARM64")
PY
```

Expected: both `machine=0xb7 AArch64`.

### Running it (device or emulator)

The APK ships **both** `arm64-v8a` (real phones) and `x86_64` (emulator on an
x86 host / x86 Chromebooks). It has been run on an Android 11 (API 30) x86_64
emulator: it loads the real temperate map and GI unit and renders them
full-screen. Game files are **never** bundled (copyright) — they are read at
run time from the app's own external files dir.

1. **Install** the APK (on a real phone connect it with USB debugging on):
   ```powershell
   adb install -r android\app\build\outputs\apk\debug\app-debug.apk
   ```
2. **Supply your own game files** (the list is in `docs/INSTALL.md`). The app
   opens on **SetupActivity**, which lists the missing files and offers
   **Import game files**: a system folder picker (Storage Access Framework).
   It needs no permissions and works on Android 5 to 15. The files are copied,
   with their names normalised to the retail case, into
   `Android/data/org.ra2.engine/files/ra2/`, the folder the engine reads.
   **On Android 11+ `adb push` into `Android/data` is refused** for the shell
   user (W2-AR-04), so stage the files in a shared folder and import them:
   ```powershell
   adb shell mkdir -p /sdcard/Download/RA2
   foreach ($f in "ra2.mix","ra2md.mix","language.mix","langmd.mix","multimd.mix",
                  "mapsmd03.mix","theme.mix","expand01.mix","Maps01.mix","MAPS02.MIX","ra2.csf") {
     adb push "D:\Red Alert 2\$f" /sdcard/Download/RA2/
   }
   ```
   Then tap **Import game files**, choose `Download/RA2` and allow access.
   MTP (USB file transfer from a PC straight into
   `Android/data/org.ra2.engine/files/ra2/`) also works. If you `adb root`
   and push into that folder on an emulator, the files get the wrong SELinux
   label (`storage_file`) and the app cannot read them.
3. **Launch** the app (`RA2 Engine`) from the launcher, or with
   `adb shell am start -n org.ra2.engine/.SetupActivity`. With all required
   files present it goes straight to the main menu (immersive fullscreen).
   Settings (`RA2.INI`) and `Saved Games/` live in the app's internal storage
   (`/data/data/org.ra2.engine/files/`; on a debug build you can reach them with
   `adb shell run-as org.ra2.engine ls files`).

   **Diagnosing failures:** `adb logcat -s SDL SDL/APP RA2Setup` shows the
   asset folder (`[assets] android asset dir = ...`), the audio device
   (`[audio] ... sdl_devices=1`), the per-stage load timings (`[load] ...`),
   lifecycle events (`[lifecycle] ...`) and any `fatal:` error. A fatal error
   is also shown on screen.

   **Note on x86_64 emulator:** it uses software GL (SwiftShader) and slow
   emulated storage, so first load takes ~1 minute; a real arm64 phone is far
   faster. The emulator confirms asset-loading + logic + layout, but final GPU
   behavior is only authoritative on a real device.

## Preflight Check

Verify the game install and binary before RE/oracle work:
```bash
RA2_INSTALL_DIR="D:\Red Alert 2" python scripts/preflight.py
```

The script checks that `gamemd.exe` exists and matches the expected SHA1 hash for YR 1.001.

## Gap fix C — combat, units, movement in REAL play (G-40..G-52, RC-5)

Audit: `docs/gap/03-combat-units.md`, IDs in `docs/gap/GAP-ANALYSIS.md`. New RE:
`docs/re/gamespeed.md` (RE-T1), `docs/re/ini-reader.md` (RC-5),
`docs/re/weapons.md` (RE-W1/W3), `docs/re/defenses.md` (RE-W2, W5 partial).
Real-play proof: `ra2engine --probe-combat` builds the menu's GameScene
(launch_skirmish + every interactive wiring) headless and checks each fix with
the scene's own rosters (ALL PASS), plus `tests/test_gap_combat.cpp`.

- **RC-5 Ini**: entry lines are cut at the first `;` and empty keys/values are
  dropped, exactly as `INIClass::Load` (FUN_00525790); Rules bools follow
  `Get_Bool` (first char 1/T/Y, 0/F/N). Local strippers removed.
- **G-41 weapon slots**: TechnoType slot rule (TurretCount<1 → Primary/Secondary,
  else Weapon1..N) — the Prism Tank fires Comet (elite SuperComet with its own
  projectile); FV keeps Weapon1 only.
- **G-44 anti-air**: per-target weapon choice (primary if it can engage, else
  secondary; AA/AG from the projectile; 0 % Verses cannot fire). Airborne
  aircraft are targets for AA weapons; flak splash stays in its layer.
- **G-40 / G-29 base defenses**: `World::step_building_combat` — acquire the
  nearest engageable enemy, Turret=yes slews at ROT, fire on ROF through the
  shared projectile/damage layer; Powered=yes defenses are offline while the
  house is low on power (FUN_004555c0). Victims retaliate. `World::take_fire_events`.
- **G-46**: idle units engage enemy base defenses in range.
- **G-42 speed**: `trunc(Speed·2.56)` leptons/frame × GameSpeedBias 1.6 at 15
  frames per engine second (GI 0.94, Rhino 1.41 c/s); every unit carries its own
  speed. RE-T1: GameScene runs `fps(GameSpeed)/15` engine seconds per wall
  second (GameSpeed 1 = 60 fps = 4x; `engine/gamespeed.hpp`).
- **G-43 passability**: Water/Rock(cliffs)/Wall cells are closed to ground
  units in skirmish and campaign (`World::set_land_terrain`); shore stays open
  [RE-FALLBACK: tile-set granularity].
- **G-45 GI/GGI deploy**: deployed = immobile firing post firing the
  DeployFireWeapon slot (Para / MissileLauncher) via normal combat; the
  Desolator keeps its eruption.
- **G-47 crush**: Crusher vehicles squash enemy Crushable infantry they drive
  into. Stop / Guard / Scatter are World orders (`unit_orders.cpp`, B's S/G/X).
- **G-50 decided with evidence**: ROF `Random(0,2)` rearm jitter (veterancy.md
  V7, FUN_006FC460) and the min-1 damage clamp (ReceiveDamage 0x700DA0) are the
  original's and are applied everywhere (fixed-seed `World::combat_rng_`).
  Gates re-pinned: combat, tank, projectile, abilities, veterancy (table).
- **Open**: garrisons (G-48), bridges (G-49), amphibious (G-51), InfDeath /
  prone (G-52), splash vs buildings/own units, Crush=1.8/AutoCrush steering,
  SelectWeapon body (RE-W1 fallback rule), homing steering (RE-W8).


## Presentation in real play (agent E, G-08/09/16/80..91)

Everything below runs in the game the menu launches (`GameScene`); the gates
keep their old renders, and no simulation value or gate hash moves (the FX,
EVA and music layers only observe World/queue state; `--demo-skirmish`
re-verified `skirmish_hash=fde3f587e747f765`). Code lives in additive files
`engine/pres_*.inc`; the RE behind it is `docs/re/presentation.md`.

- **EVA (G-80)**: `evamd.ini` parsed, side-correct clips (Allied/Russian/Yuri,
  `langmd.mix/audiomd.mix` then `language.mix/audio.mix`), the recovered queue
  (one line at a time, 500 ms gap, INTERRUPT flush, QUEUE FIFO + de-dupe,
  transient pending slot by priority). Triggers: Building/Training on start,
  ConstructionComplete, UnitReady (RadarEvent 6, 2 cells), Canceled,
  InsufficientFunds (< 100 credits with production running, every SpeakDelay),
  LowPower (once per episode, needs a yard), OurBaseIsUnderAttack + siren
  (RadarEvent 3: 8 cells / 400 frames), UnitLost (RadarEvent 7),
  NewConstructionOptions, StructureSold, BuildingCaptured, Repairing,
  UnitPromoted, SW ready/launched, YouAreVictorious/YouHaveLost.
- **Voices and sounds (G-81)**: select/move/attack/die + weapon `Report=` for
  every unit of every roster (incl. A's rules-driven roster), anim `Report=` for
  impacts/explosions, BuildingDieSound / BuildingDamageSound / SellSound /
  promotion sounds / RadarOn / RadarOff.
- **Music (G-82)**: `thememd.ini` `Normal=yes` tracks in `[Themes]` order
  (BrainFreeze first), `Repeat=` honoured, streamed from `thememd.mix` on the
  Score bus.
- **Effects (G-83)**: weapon `Anim=` muzzle flashes (8-facing infantry lists,
  FLH for vehicles), warhead `AnimList` impacts (`[min(dmg, n*25-1)/25]`),
  building `Explosion=` anims, projectile `Image=` art in flight, nuke take-off
  and NUKEANIM; `[Anim]` Rate (900/Rate frames), Start/End/Loop, Next, Report.
- **Shroud/fog (G-84)**: SHROUD.SHP + FOG.SHP post-pass with the binary's
  256-entry neighbour LUT (0x7F3194); enemy buildings, ore, crates and shells
  respect visibility.
- **Buildings (G-85)**: ActiveAnim/Two/Three/Four (+Damaged), BibShape,
  DamageFireTypes fires, 50 % shadow half.
- **House colours (G-16 render)**: SHP units, buildings and voxels in each
  house's lobby colour (voxel sets rasterized lazily per colour).
- **Theater art (G-86)**: buildings, ore, crates and the unit palette follow the
  map's theater (snow/urban); YR-only DESERT/NEWURBAN stay temperate.
- **Map dressing (G-87)**: `[Terrain]` trees/rocks, walls/fences/bridges
  overlays, `[Smudge]`, neutral `[Structures]` (tech buildings join the World,
  civilian buildings are dressing); they block ground movement.
- **Cursors (G-08 art)**: MOUSE.SHP from the 0x82C038 descriptor table, driven
  by B's `Input::action_at` through DisplayClass::ConvertAction; edge-scroll
  arrows; 16 ms animation ticks.
- **Radar (G-09 art)**: isometric minimap inside radar.shp's open frame
  (tile radar colours, ore, dressing, shroud/fog, house blips, viewport box),
  power-up animation; B's radar click uses its exact inverse.
- **HUD/platform (G-88/89/90/91)**: 12metfnt credits/banners, black clear,
  selected-own path lines only (Target Lines), rules health colours, infantry
  shadows + anim tables in skirmish, Soviet `sidec02` sidebar, RA2.INI
  `[Video]` window size + Alt+Enter fullscreen.

**Acceptance harness**: `ra2engine --scene-shot <stem> [--map M] [--secs a,b]
[--script none|fight|tanks|build] [--side soviet] [--color N] [--select]
[--mouse x,y] [--peek terrain|overlay|civ] [--give ID] [--audio]` drives the
real GameScene at 15 Hz, writes `<stem>_<sec>.png` and prints the `[pres]`
event log (EVA requests/plays, sounds, music, impacts, cursor ids).

**Open (RE-FALLBACK in code)**: infantry shadow = palette index 12 at 50 %
(binary uses the second-half frame; GI's is empty), damage-fire spawn
placement, BlendedFog compose, radar colour source, YR-only theater art,
nuke flight (damage is instant, G-25), music shuffle setting.
## Real-play controls (agent B, G-01/04/05/06/07/08/09/10/12/13/70/71)

Recovered from `gamemd.exe` (hunt "fixB", scripts `D:\ra2-re\scripts\fixB_*.py`)
and verified in the REAL scene tree by the `--play` scripts `controls.play` and
`ctl-*.play` (MainMenu -> Skirmish lobby -> GameScene with scripted input). The
older `--controls-selftest` was retired in wave 5 (R-183: written for the pre-MCV
start, and it wrote the user's own RA2.INI). Not a gate.

- **Mouse (G-05)**: the original's tactical WndProc `FUN_00692AB0` dispatches
  WM_LBUTTONUP to Left_Up `FUN_004AB8C0`: with a selection a LEFT click issues
  the selected objects' action (move / attack / enter / harvest / dock /
  capture / infiltrate / rally); a left click on an own unit re-selects (Shift
  toggles, double-click selects the type on screen); with nothing selected a
  ground click does nothing. RIGHT click (no drag) cancels one armed mode
  (placement -> repair -> sell -> superweapon) else deselects everything
  (`FUN_004AAC40` -> `FUN_0048DBA0`); right-drag scrolls [RE-FALLBACK 1:1].
  Ctrl = force-fire, Alt = force-move. Clicking the selected deployable unit
  itself = deploy/unload (ACTION_SELF). Attack targets are `is_enemy` houses
  only (G-06). The legacy right-click-orders model survives only as
  `Input::Scheme::Legacy` for old unit tests.
- **Esc (G-01)**: never quits (only closing the window does). In game it opens
  the options dialog (`[Hotkey] Options=27`, exec `FUN_00646AE0`); in dialogs
  and shell pages it is Back. Android BACK = Esc.
- **Hotkeys (G-04)**: the shipped `KeyboardMD.ini [Hotkey]` table (85 bindings,
  VK + 256 Shift / 512 Ctrl / 1024 Alt), overridden by the install's file:
  S stop, G guard, X scatter, D deploy, T type-select (screen, then map), H
  centre base, K/L repair/sell, Q/W/E/R sidebar tabs, 1..0 select group,
  Ctrl+n create, Shift+n add, Alt+n centre (n twice within 800 ms centres),
  N/M next/previous unit, F1-F4 / Ctrl+F1-F4 view bookmarks, numpad 8/2 strip
  scroll. Commands without a feature (beacons, planning, alliances, taunts...)
  are consumed. N no longer arms the superweapon (it is NextObject).
- **Build queues (G-07)**: StripClass action `FUN_006AA390`: left = start /
  queue another unit (cap `MaximumQueuedObjects`) / resume a held item / place
  a ready building; right = hold, then abandon with refund (Shift = abandon
  all); count badge and HOLD marker on cameos.
- **Building selection (G-10)**: any own building is selectable (bracket + HP
  bar); rally/primary for factories.
- **Cursor state (G-08)**: `Input::action_at` / `GameScene::cursor_action()`
  give the action under the pointer for the cursor art.
- **Radar (G-09)**: a click on the active radar re-centres the view, or orders
  the selection there.
- **Sidebar strip (G-12)**: r-up/r-dn and SidebarUp/Down scroll by one row.
- **Logic tick (G-13)**: GameScene steps the World in fixed 1/15 s ticks
  [RE-FALLBACK until the per-GameSpeed rate is pinned].
- **Touch (G-70/G-71)**: tap = left click, drag = band box, long-press or
  two-finger tap = right click, two-finger drag = pan; SDL's synthetic mouse
  events are disabled, and touches never edge-scroll.

## Real-play verification harness (`--play`, G0)

`ra2engine --play <script.play> [--out <dir>]` plays the REAL game -- main menu
-> Single Player -> lobby / New Campaign -> `GameScene` -- from a small text
script, in a hidden window, at a fixed 1/15 s logic tick, and checks what the
player would see. It is the acceptance bar for every gap fix ("real play"),
not a byte-identity gate: no existing gate hash depends on it.

* **Input** goes through the scenes' real `handle()` as scripted `InputEvent`s
  (built on agent B's `EventSource` / `DriverHooks` / `run_scene_driver(src,
  fixed_dt, hooks)` groundwork; keys via the production `key_event_from_sdl`).
  Targets are semantic: `click button "Single Player"`, `click start`,
  `click cameo:GAPOWR`, `click unit 0:E1 +5 +0`, `click building 0:GACNST`,
  `place auto`, `key esc`, `key 1 ctrl`.
* **State** comes from each scene's probe (G's typed `Scene::play_probe` merged
  with B's integer `Scene::probe`): credits, units/buildings per house and type,
  harvester states, sidebar contents and `can_build`, queues, armed modes,
  menu/dialog state, victory (`outcome` = what the session shows,
  `world_outcome` = the World's own verdict), campaign objectives/triggers.
* **Checks**: `assert`, `require`, `until N <expr>`, `if <expr> then <cmd>`,
  `let`; every check can carry `@<G-ID>:<owner>` so a failure names the gap
  issue it belongs to. A missing probe key reports `missing-probe:<key>`
  instead of crashing (scripts written for a later phase degrade gracefully).
* **Output**: `PLAY PASS/FAIL` lines, PNG shots, `play_state` every 15 frames,
  `play_result=PASS|FAIL`, a deterministic `play_hash`, `<out>/result.txt`;
  exit code 0 / 1 (failures) / 2 (script error) / 3 (exception). RA2.INI and
  saved games are isolated under `<out>/pref/` (`RA2_PREF_DIR`).

Scripts live in `scripts/play/` (format and probe keys: `scripts/play/README.md`):
`sk-a` / `sk-s` (Allied / Soviet skirmish playable core), `sk-a-lose`,
`sk-end` (end of a skirmish), `m-all01` / `m-sov01` (campaign mission 1),
`shell-smoke` (menus, Esc, save/load, Abort/Exit), `controls` (agent B's
controls self-test as a script) and `probe-*` self-checks. Run them ONE at a
time (`pwsh scripts/play/run-all.ps1`); ~15 s start-up, then ~400 frames/s with
`set render_every 300`. Expected results per tree and the G-ID / owner of every
failing step: `docs/gap/PLAYTEST-STATUS.md`.
## Real play: MCV, skirmish start, game over (gap F: G-02/14/15/17/23/72-75)

Everything below runs in the **real** game (`run_menu` -> lobby Start /
New Campaign -> `GameScene`); the byte-identity gates keep their own setup.
Recovery notes: `docs/re/mcv-deploy.md` (gamemd.exe listings).

- **MCV deploy (G-23).** `AMCV`/`SMCV` (`DeploysInto=`) deploy by clicking the
  selected MCV (ACTION_SELF), the D key, or `World::order_deploy_mcv` (the
  order D's AI and campaign scripts call). Recovered rule: the ConYard's origin
  is the MCV cell + AdjacentCell[NW] (-1,-1) for a foundation wider/taller than
  2; every foundation cell must be clear (no adjacency test); the MCV turns to
  the building's `DeployFacing` (default 0x80) and then unfolds with the
  buildup animation, health ratio carried. An illegal spot makes the MCV drive
  to the first legal cell of the recovered 36-offset ring; a failed unfold says
  `EVA_CannotDeployHere`. A human's skirmish ConYard sold with
  `MCVRedeploys=yes` packs back into its MCV (`UndeploysInto`).
- **Skirmish start (RE-E2).** No pre-built base: each combatant gets its
  country's `BaseUnit` MCV on its start plus the recovered `UnitCount` force
  (`FUN_006880F0`: budget = (UnitCount-1) x average Cost of all
  `AllowedToStartInMultiplayer` types; vehicles while under 2/3 of it, then
  infantry; Owner/TechLevel filtered; scattered at distance >= 4). Starts:
  first house random, each next the farthest free start. AI houses unfold at
  once. ShortGame defeat needs no building **and** no MCV. The camera opens on
  the player's MCV. [RE-FALLBACK: a map-seeded LCG instead of the session RNG.]
- **Game over (G-02).** Victory / defeat flag `EVA_YouAreVictorious` /
  `EVA_YouHaveLost` with `TXT_VICTORIOUS` / `TXT_LOST`, a defeated opponent
  `EVA_PlayerDefeated` + `TXT_PLAYER_DEFEATED`; after BorrowedTime the result
  page (time, kills/losses) offers REMATCH or MAIN MENU. [RE-FALLBACK: the MP
  score screen layout is not recovered.]
- **Confirms (G-14).** Exit Game asks `GUI:ExitAreYouSure` (OK/Cancel, dialog
  0x120); Abort Mission asks `GUI:AskAbortMission` (Abort / Restart in a
  campaign / Resume Mission, dialog 0x121).
- **Lobby (G-17).** The player cell cycles the nine playable countries (CSF
  names); the side follows the country's `Side=`; Yuri is gone; AI players
  are clamped to the map's MaxPlayers-1. The colour index stays in
  `HouseRec.color_ix` for the remap.
- **TechLevel (G-15).** The sidebar offers only types with `TechLevel <=` the
  lobby slider (skirmish) or the mission's `[<Player>] TechLevel=` (ALL01/SOV01
  = 3).
- **Campaign (G-73/74/75).** Events 29 (after 48 on every death), 44
  AttackedByHouse (attacker inferred from target assignments [RE-FALLBACK]),
  25/26 crossing lines (row/column cell tags on cell entry) are raised;
  actions 108 CreateCrate, 101 reshroud, 9 FireSale [RE-FALLBACK: sells at
  once] run; ships arrive on water, ground reinforcements at sea land on the
  nearest shore [RE-FALLBACK: no transport delivery].

**Verify:** `ra2engine --gf-play skirmish|soviet|win|lose|confirm|campaign:<MAP>[:GDI|Nod[:frames]] [outdir]`
drives the real scene tree at a fixed 1/15 s tick and prints `gf_play PASS/FAIL`
lines plus PNGs; `ra2assets_tests -tc="mcv:*"` covers the World deploy rules.


## Real-play AI (gap fix D, G-60..G-69)

The audit (`docs/gap/04-ai.md`) found the computer opponent firewalled into the
`--demo-ai-*` gates: commit `f77e68e` (Scene framework) had dropped the
`aimd.ini` load and the team config from the real game, so `step_ai_teams`
skipped every real AI house and the player could not lose. Every skirmish AI
slot now runs all three layers in real play (`engine/ai_skirmish.inc`,
`arm_skirmish_ai`, called from `GameScene`'s ctor and its load-failed restart).

| G-ID | What changed | Evidence |
|---|---|---|
| G-60 / G-62 | AI-2 teams + AI-3 triggers armed for every AI slot (both sides' ground roster, real rulesmd prerequisites, Owner-filtered per house); a team whose TaskForce the house can neither build nor recruit idle is not created (`FUN_0041ff00`, ai-trigger.md §5); team moves give up after 10 s without progress [RE-FALLBACK] | `--ai-soak`, `sk-a` L117, `sk-s` L94-L95, `sk-a-lose` L19-L20 |
| G-61 | ONE ScriptType table for AI-2 and the campaign: the 65-entry executor at `0x6E94E4` (`engine/script_actions.hpp`); guard = arg x15 frames, jump 1-based, 49 = success flag + continue, 53/54 gather/regroup, 46/47/58 building targets, YRpp QuarryType | `docs/re/ai-team.md` §3 (rewritten), c13/c14 logs |
| G-63 | lobby AI difficulty -> `house+0x184 = 2 - lobby` (`FUN_004f6dd0` AssignHandicap; Brutal runs on `[Easy]`); FirePower/ROF handicaps applied (Armor/Groundspeed: no reader found, not applied) | `D:a2-reixDout` |
| G-64 | side-correct catalogs from the `[AI]` Build* lists filtered by AIBasePlanningSide/Owner/RequiredHouses/ForbiddenHouses; country checked against `[Sides]` (NEW-4) | `sk-s` L88-L90 |
| G-65 | AI repair (`FUN_00450620`: money >= CreditReserve, one start per RepairDelay window) and sell (money < CreditReserve, health < ConditionRed) | `--ai-soak --damage-ai-at 4` |
| G-66 | AI plans BuildTech + its AIBuildThis superweapon structures; a charged SW fires at the enemy's highest-AIIonCannon*Value target (`FUN_00509710` / `FUN_0050ca10`) | World test, `--ai-soak` (NAMISL by min 40) |
| G-67 | threat centroid / rival size / AI-3 enemy counts use enemy houses only | — |
| G-68 | miners rebought to refineries x HarvestersPerRefinery (`[IQ] Harvester=2`) | `--ai-soak` (8 miners on 4 refineries) |
| G-69 | campaign computer houses build/rebuild their map base nodes once map action 3 "Production begins" fires (AI-2/AI-3 for map houses still open, RE-A5) | `RA2_SURVEY_MAP=sov03u.map --campaign-survey` |

`ra2engine --ai-soak [--minutes N] [--diff easy|normal|brutal] [--ai-side
soviet|allied] [--map file|default] [--damage-ai-at MIN] [--ai-vs-ai] [--png
path]` drives the real `GameScene` with a passive human and prints, per game
minute, the AI's structures, miners, credits, units, teams, triggers and the
damage dealt to the human base. 15 min on `xmp31s2`, Soviet AI Normal: 21
structures, 8 miners, 18 units trained, 10 teams, first hit on the human base
at ~6 min, human defeated. Allied AI Brutal: GA* base only (AMRADR for the
Americans), hits the human base by min 12.

**Gate re-pins (gap D).** `--demo-ai-base` `cd1f7c6a29933f0a` -> `6a094915a28b9314`:
the hash folds the AI plan, which gains the G-66 tail nodes NATECH/NAIRON/NAMISL
(`plan=54` -> `57`; the build/placement witness is unchanged). `--demo-ai-team`
`312e92ed862a5e74` -> `c28ba20195c2d4dc` and `--demo-ai-trigger`
`8334a6ca1531cabc` -> `17388180b9427561`: the stock scripts now run under the
real action table (G-61: 54/53 moves instead of "attack units"/"attack
building", 49 continues instead of disbanding, guard x15 frames), teams must be
buildable (G-60), and gap C's combat/movement changes on master. The earlier
master drift of `--demo-ai-trigger` (`8334a6ca` -> `1002c3f8`) is explained:
P6's kRoster growth 24 -> 39 (`0800106`) made NATECH resolvable, so two more
stock AITriggerTypes ("Soviet Apoc Hard/Medium", condition object NATECH)
survive parsing (8 -> 10) and enter the weighted draw -- faithful. Verified by
building `4b3520c` (8334a6ca, parsed=8) and `0800106` (1002c3f8, parsed=10).
`--demo-saveload` `270b4fce163438f7` -> `2511ec3d0deec668`: save format v5 (new
AI config/state fields serialized) and the side-aware AI. `--demo-campaign`
`f10f9d4e6bd883cd` (870 lines) -> `17830b0f7c233223` (737 lines): the campaign
interpreter shares the G-61 table, so `Guard` lines last arg x15 frames instead
of x90 (A/B: with x90 restored this tree gives `8a0f07758cb0393f`, 730 lines --
the remainder is master-side, gap C/E). `--demo-skirmish` / `--demo-maps`
unchanged.

## World depth: garrisons and bridges (G16, G-48 / G-49)

RE'd from gamemd.exe (`docs/re/world-depth.md`, every rule with its address)
and verified in the real game by `scripts/play/world-depth.play` (PASS).

* **Garrisons.** Infantry with `Occupier=yes` (GI, Conscript, ...) clicked onto a
  `CanBeOccupied=yes` building that is theirs or neutral walk in (up to
  `MaxNumberOccupants`); the building turns to the first occupant's house and
  back to the neutral house when emptied. It fires the current occupant's
  `OccupyWeapon` (`EliteOccupyWeapon` when elite) round-robin over the
  occupants: ROF / occupant count / `OccupyROFMultiplier`, damage x
  `OccupyDamageMultiplier`, range = min(foundation W, H) / 2 +
  `OccupyWeaponRange`. Deploy (`D`, or clicking the selected garrison) unloads
  them; selling or losing the building lets them out. Skirmish maps' civilian
  garrisons and bridge repair huts are now real neutral World buildings.
* **Low bridges** (wood `LOBRDG*`, concrete `LOBRDB*`). Ground units cross the
  river on them; ships do not pass under. A `Wall=yes` warhead exploding on a
  section breaks it with probability `damage / BridgeStrength` (two stages:
  damaged, then destroyed -- the 3-cell cross-section turns back into water and
  a ground unit on it dies). Ctrl+click on the ground force-fires at a cell.
  An Engineer entering a bridge repair hut (`CABHUT`) restores the bridge and
  is consumed. Some maps (May Day) ship their crossings already blown.
* The map dressing now indexes `[OverlayTypes]` by order of appearance (the
  runtime index the map stores); every non-ore overlay used to draw with the
  art of a neighbouring entry.
* Not yet: high (tile) bridges' on/under layer and destruction, the Assaulter
  (YR) garrison clearing, `DistributedFire`.
* Dev modes: `--depth-survey` (bridges / huts / garrisonable structures per MP
  map), `--depth-dump <map> x0 y0 x1 y1` (runtime overlay indices).


## In-game HUD and shell pages (wave-2 W2-UI fixes)

These fixes close the UI rows of `docs/gap/wave2/04-ui-shell.md`. That doc has
the row statuses, the RE answers and the screenshot paths. RE came from
`game.exe` 1.006, with the scripts in `D:\ra2-re\scripts\w2fix_*.py` and the
output in `D:\ra2-re\w2fix-uiout\`.

* **Sidebar.**
  * The chrome draws under its own mix's `sidebar.pal` (`sidec01.mix` or
    `sidec02.mix`). Cameos keep `cameo.pal`.
  * The build clock is the translucent `GCLOCK2` frame. A cameo you cannot
    build is darkened by `DARKEN.SHP`.
  * "Ready" and "On Hold" (CSF), and the queue count, are drawn in `game.fnt`
    in the UI colour over a 0xAF black box.
  * Tabs show lit / normal / blank frames.
  * The Soviet sidebar uses its own constant set.
  * The radar boots at a 4-tick frame timer and folds back when it loses power.
    The minimap sits in the 140x108 viewport at (16, 49).
* **Tooltips.** Hovering a cameo shows "UIName $Cost" with every space turned
  into a newline. Sidebar buttons show their CSF tips. The `[Options]
  ToolTips` setting applies.
* **One HUD font.**
  * `ra2assets::UFont` decodes the "fonT" `local.mix/game.fnt`.
  * The UI colour is RGB(164,210,255) for Allied and (255,255,0) for Soviet.
  * Messages go to a top-left list with a timeout: "Mission Saved", "... has
    been defeated!", "Low Power - Production Slowed" and objectives. The timer,
    the end banners and the F1 briefing use the same font.
* **Selection.**
  * A selected unit shows the `PIPBRD` board and up to 17 `PIPS` (8 for
    infantry), green / yellow / red.
  * A selected building shows the diagonal `PIPS` line along the top-left edge
    of its foundation.
* **In-game options are full-screen shell pages.** Esc or F10 opens them.
  * Game Options (0xB5).
  * Game Controls (0xBBB): Game Speed applies live, plus Scroll Rate and the
    check boxes.
  * Sound (0xB8): volumes, a jukebox over RA2 `theme.ini`, Play / Stop and
    Shuffle.
  * Abort (0xB6): Restart appears in a campaign only.
  * Load, Save and Delete (0xB7 / 0x2B4 / 0x2B5).
  * Diplomacy (0x73), opened by the sidebar button.
  * Confirm boxes are the 0x120 popup, using `pudlgbga` for Allied and
    `pudlgbgs` for Soviet.
* **Campaign.**
  * Page 0x94 shows the Allied, Boot Camp and Soviet emblems as wells, the
    campaign list, the difficulty slider and Load.
  * The loading screen fills the window, uses `game.fnt` and starts the mission
    by itself.
  * The result pages are shell pages: the skirmish score page and Campaign
    Results.
* **Shell.**
  * The bottom row of Single Player and of Movies & Credits is "Main Menu"
    (`button:Back` still works in scripts).
  * The status line shows the STT help text. Every button press plays
    `MenuClick`.
  * Pages power on with a staggered `SDBTNANM` animation.
  * No button is lit until the pointer is over it.
  * View Credits scrolls `credits.txt`.
  * Hotkeys come from RA2 `Keyboard.ini`. `KeyboardMD.ini` is used only if
    `Keyboard.ini` is missing.
* **New play scripts:** `ui-hud`, `ui-pages`, `ui-campaign`, `ui-result`,
  `ui-shell`.
* **Still open:**
  * the in-game Keyboard page;
  * the 800x600 "L" shell assets;
  * the rally-point art;
  * the credits tally colour and roll rate;
  * the tab flash on a new cameo;
  * the score formula.

## In-world graphics, audio and render performance (wave-2 W2-GA fixes)

The per-row status, the RE answers and the before/after perf numbers are in
`docs/gap/wave2/05-gfx-audio-perf.md` §6. In short:

- **Terrain and visibility.** Never-explored cells (cliff and slope extras
  included) are not drawn, so the layout no longer leaks through the shroud.
  Fog of war is a scenario flag that is **off** by default (RE-Q3:
  `IsLocationFogged` is a stub, and FogOfWar comes from the MP options).
  `World::object_visible_at` is the single query for drawing, picking,
  auto-acquire and the radar: with fog off, an explored cell shows everything
  in it. Omitted IsoMapPack5 cells are clear ground (RE-Q5). Ore and gems use
  the theater's own art. The map's `[Lighting]` is applied (RE-Q7).
- **Map dressing and units.** Ore mines draw in the unit palette and animate.
  Trees and civilian buildings cast shadows. High-bridge decks are drawn.
  Rocks and bridges use the iso palette. The Guardian GI art comes from
  conqmd.mix. SHP vehicles (Terror Drone) use the RE-Q14 frame rule. Ground
  vehicles cast a voxel shadow. Defence turrets are drawn (`TurretAnim`
  voxels). The war factory opens its door, and the superweapon buildings show
  their charge anims. Tesla bolts and Prism beams are drawn, explosions leave
  craters and scorch marks, and radiation glows.
- **Audio.** The RA2 `theme.ini` / `theme.mix` playlist uses the recovered
  `Next_Song` and RA2.INI shuffle/repeat. Music is **streamed** on a worker
  thread (`engine/music_stream.*`). Output is a stereo SDL callback sink with
  the RE-Q11 pan law, live re-attenuation and a soft limiter. VShift, FShift,
  ALL, ATTACK, DECAY and LOOP are supported. Added sounds: move-start, crush
  and water impact. Each superweapon type now has its own Ready, Detected and
  Launched EVA lines, and there are ore-miner and ally under-attack lines.
- **Performance.** The terrain draw list is cached per map (XMP27T8: 39 ms →
  0.1 ms). The shroud, ore and radiation passes use visible-row iteration.
  `MapFile::at` is O(1). MIX archives are read by file offset with nested
  views (no whole-archive reads, peak memory 724 → ~330 MB). `TextureCache`
  has an LRU budget with pinning. Voxel colour sets are prewarmed on a worker
  thread. The renderer uses vsync with deadline pacing. The default build is
  RelWithDebInfo.
- **Harness.** `--scene-shot --script tanks|fight` works with the MCV start.
  New options: `--view X,Y` and `--reveal`. New play scripts:
  `scripts/play/gfx-vis.play` (base, units, door, Patriot) and
  `scripts/play/gfx-fx.play` (bolts, beams, craters).

Gate re-pin: `--demo-saveload` `7fde8a2b895cf6ae` → `653c22b544488e64`.
Causes: save format v9 serialises the FogOfWar flag (one more byte in each
save, which changes both scenario digests), and the skirmish scenario now
targets explored cells with fog off. Every witness count is unchanged:
N=123, 13 launches, 8 produced, 149 ability events, 1 rad site. Every other
gate is byte-identical, at `-O0` and at `-O2`; `--demo-shroud` opts into
FogOfWar for its fogged-enemy stage.

## Aircraft, naval and superweapons in real play (wave-2 W2-AN fixes)

Branch `w2fix-air`; gap rows `docs/gap/wave2/02-air-naval-sw.md` §5. Code:
`engine/world_an.cpp` (naval/air pass), `engine/world_sw.cpp` (superweapons v2),
`engine/air_naval.hpp` (per-type / per-unit state), `engine/w2air_main.inc`
(rules resolution), `engine/w2air_play.inc` (scene set-up, SW targeting, `--play`
keys). Save format **v10**.

* **Naval.** Ships move where clicked (domain-aware click/cursor, per-domain group
  formation). Subs submerge (Underwater+Cloakable; surface for CloakDelay after
  firing) and are found only by Sensors sonar; NavalTargeting/LandTargeting/
  CanPassiveAquire follow the rules.ini table. Amphibious LCRF/SAPC cross land and
  water, load on shore and unload only on land. Spawner weapons: Carrier Hornets,
  Destroyer ASW Osprey, Dreadnought and V3 missiles (MinimumRange). The Naval Yard
  repairs docked ships; heavy ships sink. Dolphin/Squid are SHP ships.
* **Aircraft.** Harrier/Black Eagle are built onto Airforce Command pads and fly
  sorties (take off, attack pass, Ammo, return, land, 270-frame rearm). Jumpjets
  lift off on exit, obey Stop and crash when killed in the air; the Night Hawk lands
  to load/unload.
* **Superweapons.** One record and one sidebar slot (own cameo) per type: Nuke
  (200+200-frame flight RE'd from `FUN_006cba10`, hits everyone incl. airborne,
  RadLevel 500), Iron Curtain, Chronosphere (two picks, chrono-kill of Organic
  non-Teleporter units RE'd from `TechnoType+0xD97/+0xCD4`), Lightning Storm,
  American paratroopers. EVA/FX run off a World event feed, so AI launches show.
  The Superweapons checkbox removes only DisableableFromShell SWs (radars stay).
* **AI.** Builds a Naval Yard on water (`BuildNavalYard`, `AINavalYardAdjacency`),
  fields ships and jumpjets, plans its tech centre and superweapons after the
  refineries, and fires every SW type.
* **Acceptance:** `naval.play`, `air.play`, `sw.play` (PASS, deterministic),
  `--probe-realplay` (0 fails, MCV start fixed), `--ai-soak --map xbayopigs.map
  --cash 50000` (new `--cash`; NAYARD/NATECH/NAMISL/NAIRON, 4 ships, 2 Kirovs, 3 SW
  launches). Gates re-pinned with causes: ai-base, ai-trigger, superweapon, roster,
  saveload, campaign (table above); the rest unchanged.
* **Still open:** CAAIRP Tech Airport paradrop, jumpjet air-speed bias (Q3), yard
  repair cost, sub cloak shimmer, AI fixed-wing aircraft, Squid grab.

## Full skirmish matches: economy, ground units, ground AI, scale (wave-2 W2-SK fixes)

Branch `w2fix-skirmish`, rows W2-SK-01..27 of
`docs/gap/wave2/03-skirmish-match.md` (§7 holds the row-by-row status, the
pinned RE and the before/after match numbers). Save format **v11**.

- **Economy that lasts.** Ore mines (TIBTRE01-03) spawn ore exactly as
  `TerrainClass::AI` does (0x71B9D0: AnimationProbability start,
  AnimationRate stages, a forced spread at half the SHP frames, density 3). Ore
  grows and spreads on the original per-type clocks (Riparius Growth/Spread
  2200 frames; FUN_007221A0 / FUN_007216E0). Harvesters scan
  TiberiumShortScan=6 then TiberiumLongScan=48 for reachable, unoccupied ore.
  Returning miners queue at the refinery's QueueingCell instead of sealing the
  pad, and an idle empty miner leaves the dock.
- **Units.** V3 rocket payload (200 on V3WH); Demolition Truck blast;
  one-bite Attack Dogs; Service Depot repair (and drone removal); Capturable=;
  spy theft from real wallets, per-category factory veterancy, Spyable=; oil
  derrick income and the real neutral house; Prism Tank beam split; Mirage
  disguise-when-still; Spy disguise.
- **AI.** Production for every team shortfall in parallel factories,
  MultipleFactory, muster-until-filled; one EnemyHouse; engineers, spies,
  transports and deploys from the stock scripts; MCV rebuy when the
  prerequisites allow; base-defence response; wall rings; team dissolve.
- **Scale.** Bucketed auto-acquire (identical results), guard-range pursuit,
  generation-stamped A* with budgets and nearest-reachable goals, event-queue
  drains, dead-slot storage release.

New play scripts: `scripts/play/economy-60min.play` (an hour on hillbtwn:
mines, regrowth and refinery income every 15 minutes) and
`scripts/play/unit-specials.play` (depot, engineer capture/refusal, spy theft,
dog, V3, Mirage, spy disguise, Prism split, Demolition Truck). New probe keys:
`uhp(H,TYPE)`, `urank(H,TYPE)`, `udisguised(H,TYPE)`, `ore_cells`,
`ore_mines`, `mine_spawned`, `ai_credits(H)`, `income(H)`. New cheats:
`damage_unit H:TYPE[:n] DMG`, `place TYPE H building H:T +DX +DY`. `--ai-soak`
gained `--kill-ai-conyard-at MIN`, per-house columns and a team dump for every
AI house.

## Android, robustness and release readiness (wave-2 W2-AR fixes)

Details and evidence: `docs/gap/wave2/06-android-robustness.md` section 7.
The player guide is `docs/INSTALL.md`.

- **One audio device** (W2-AR-01). `AudioOutput` is a refcounted handle on a
  single SDL device. The menu and the game swap the mixer it plays, so there
  are no AAudio assertions on Android.
- **SDL assertions never block** (W2-AR-30). A handler logs them and
  continues. They are compiled out of optimised builds.
- **Loading screen and teardown before build** (W2-AR-02/-10/-05).
  - Scene changes into a game are `DeferredScene`s. The driver destroys the
    old scene first, then builds the game under a progress screen fed by
    `load_progress()` checkpoints.
  - A build that throws (for example a missing campaign `.mix`) becomes an
    error page with a MAIN MENU button.
  - Emulator start went from 9 min to 4.5 s, and the load-a-save peak from
    1.4 GB to 405 MB.
- **Finding the game files** (W2-AR-03/-22).
  - Desktop search order: `--data <dir>`, `RA2_INSTALL_DIR`,
    `RA2.INI [Engine] DataDir`, the exe folder, the registry `InstallPath`,
    `D:\Red Alert 2`.
  - Missing required files open a **GAME FILES NEEDED** page (RETRY /
    BROWSE / EXIT; IMPORT on Android).
  - New flags: `--version`, `--help`, and `--quick-skirmish` (dev: the
    default 1v1 through the loading screen).
  - Evidence hooks: `RA2_AUTOSHOT_DIR=<dir>` saves the loading / error /
    missing-files screens, and `RA2_QUIT_AFTER_LOAD_MS=<ms>` quits that long
    after the first load.
- **Android app**:
  - `SetupActivity` launcher with a SAF importer.
  - Immersive fullscreen, and a game canvas that follows the panel aspect.
  - Touch canvas of about 480 px on phones, for bigger cameos and buttons.
  - Touch command bar: +SEL / FIRE / MOVE / STOP / GRD / SCAT / DPLY /
    teams 1-5.
  - Two-tap placement.
  - Back on the root menu asks to exit.
  - IME text rects.
  - Cancelled touches never click.
  - Background: pause + `AUTOSAVE.SAV`, then a resume prompt after a kill.
  - Texture cache clear/trim on device reset or low memory.
  - The process ends after quit.
- **Saves are format v11**: v10 plus a CRC-32 trailer. Corrupt files are
  listed as "Damaged" and refused.
- **Release build**:
  - Both APK build types are optimised.
  - 16 KB page-aligned `.so` files; targetSdk 35.
  - `keystore.properties` signing.
  - Version from `VERSION` (window title / `--version`: `0.9.0+g<hash>`).
  - `LICENSE`, `THIRD_PARTY_NOTICES.md`.
  - `-Wall -Wextra` with 0 warnings; `-ffp-contract=off` on both builds.

## Art and asset loading (wave-3 W3-FIX-A)

Details and evidence: `docs/gap/wave3/03-asset-anim-systems.md` (status
section "W3-FIX-A"), the RE note `docs/re/asset-search.md`, and the `art-*`
`--play` scripts in `scripts/play/`.

- **One MIX search list per theater** (`ra2assets::AssetFS`,
  `src/asset_fs.cpp`; engine glue `engine/asset_search.inc`). The original
  resolves every file with one global lookup over all opened archives, first
  opened first (RA2 `game.exe` `FUN_005952f0` appends, `FUN_00595aa0` walks
  head to tail). The recovered RA2 load order: `expand99..00`, `ra2`,
  `cache`, `local`, `ecache*`/`elocal*`, `conquer`, then per theater
  `generic` (not for SNOW), `<theater>`, `<ext>` (`tem/sno/urb`),
  `iso<theater>`, `isogen`, `neutral`. The YR `ra2md.mix` archives follow as
  a fallback tail [RE-FALLBACK] so YR-only art (the civilian buildings of the
  default lobby map `hillbtwn`) draws without overriding RA2 art. Buildings
  (roster, campaign, garrison extras), map dressing, building and
  `[Animations]` anims (incl. `Theater=yes` debris/waterfall art), infantry
  and SHP-vehicle bodies, and voxels all resolve through it.
- **Theater-aware campaign buildings**: `camp::Types::building` uses the
  map's theater (was hard-coded TEMPERATE): snow missions get snow roofs and
  the snow Kremlin.
- **Landmarks**: the Statue of Liberty, White House, the Washington
  memorials, the Chicago landmarks, the Gateway Arch, St Basil's, the Eiffel
  Tower and Arc de Triomphe (`isogen.mix`) and the ALL10 ammo crates
  (`sno.mix`) are drawn.
- **Art `Palette=`**: `<name><theater ext>.pal` (`Palette=lib` ->
  `liburb.pal`), drawn without house remap (`FUN_00604540`).
- **`InvisibleInGame=yes`** buildings (the lamp posts) are not drawn; they
  keep their cells and tags (`FUN_00459a00`: Invisible + RadarVisible=no).
- **Building light sources**: every live building with `LightIntensity`
  lights the cells within `LightVisibility` leptons (linear falloff, intensity
  and tint added per cell, `FUN_00479440`), so the night missions get street
  lighting; NEGLAMP darkens. Drawn as a per-cell light map multiplied over the
  playfield (above 1.0 clamps [RE-FALLBACK]).
- **Campaign SHP units**: Terror Drones, Dolphins and Giant Squid exist as
  map units and TaskForce members (they were sent to the voxel loader).
- **Water-bound map structures**: map-placed Naval Yards (ALL04/07/11) are
  placed on the water grid.
- **Map-placed BalloonHover aircraft** (the SOV11/12 Kirovs) start at their
  hover height.
- **Census**: `ninja art_census; art_census "D:/Red Alert 2/"` resolves
  every placed object with the engine's real resolver: campaign 28,394 /
  28,399 (99.98 %, was 92.8 %), RA2-theater lobby maps 91,959 / 91,963.
- `--play` verbs for these checks (play harness only): `cheat reveal`,
  `cheat art_dump`, `cheat view_building TYPE [n]`, `cheat view_unit IMAGE
  [n]`, `cheat view_dressing IMAGE [n]`; probe keys `art.missing`,
  `art.invisible`, `art.dressing_missing`, `img.<IMAGE>`, `img.<IMAGE>.air`.
- **Gates**: all 27 determinism gates print their pinned values (no
  re-pin). The playable building roster deliberately resolves the RA2
  archives only, so the YR-only `NABNKR`/`NAINDP` stay art-absent and
  unbuildable as before (with the YR tail they gained art and moved
  `--demo-ai-base`, `--demo-ai-trigger`, `--demo-triggers`, `--demo-roster2`
  and `--demo-saveload`).

## Animation systems (wave-3 W3-FIX-B)

Details and evidence: `docs/gap/wave3/03-asset-anim-systems.md` (status
section "W3-FIX-B"), the RE notes `docs/re/infantry-anim.md` §8-§14 and
`docs/re/presentation.md` §8, and the `anim-*` `--play` scripts.

- **Anim clock on the logic tick.** `Presentation::logic_tick` runs inside
  GameScene's fixed 15 Hz loop and drives `g_pres_tick`; AnimClass anims,
  building slot anims, terrain/tile anims, the radar anim, beams, infantry
  sequences and the vehicle walk stage all advance per game frame (was per
  render frame, ~4x too fast at 60 Hz and half speed on a 30 Hz phone; it
  now scales with GameSpeed like the original). Fire/death effects,
  projectile impacts, defence muzzles and building deaths are spawned on the
  tick of their event. Same-tick screenshots of a 60 Hz and a 30 Hz run are
  pixel-identical (`anim_check.py --cross`).
- **AnimClass draw keys** (shared `pres_fx.inc`): `Translucent=yes` fades by
  life (25/50/75 % at 0.2/0.4/0.6 of End), `Translucency=` levels, `Shadow=`
  darken frames, `Flat`/ground-layer anims under objects, `AltPalette` through
  the owner's (else ColorSchemes[0]) unit-palette remap, `RandomRate`,
  `StartSound`.
- **Infantry DoType machine** (`engine/inf_anim.cpp`, live game only): all
  42 art sequences with the original DoControls rates, idle fidgets on the
  IdleActionFrequency timer, fear -> Down/Prone/FireProne/Up with the
  warhead's `ProneDamage`, civilian Panic + scatter (Fraidycat), deaths by the
  warhead's `InfDeath` (Die1/Die2 or INFDIE/FLAMEGUY/ELECTRO/YURIDIE/
  NUKEDIE/...), `DeadBodies` corpses, GI/Guardian GI Deploy/Deployed/
  DeployedFire/Undeploy, Swim/Wet set in water, Cheer on trigger 113 and on
  victory. Save format v14.
- **Building slots** (`engine/pres_bldanim.inc`): all 21 art slots with
  Damaged/Garrisoned variants; construction-yard crane on placement, shipyard
  production, service-depot Special -> SpecialTwo -> SpecialThree -> Idle,
  refinery storage level (one ActiveAnim) and ore dump, Tesla/Prism charge
  per shot, ground and roof factory doors, CanBeOccupied 4-state and Gate
  frames, LeaveRubble rubble, DestroyAnim ruins, civilian dressing anims.
- **Terrain**: theater tile anims (`Tile<NN>Anim` of the tile set's
  SetName section: ALL07's waterfalls, tunnel tops).
- **Vehicles**: HVA frames by the walk stage (Night Hawk rotors), SHP-vehicle
  WalkRate/IdleRate, `TurretOffset`, `UnloadingClass` while dumping, a
  random `Explosion=` entry, 8-way muzzle anims by turret facing.
- `--play` additions: `set hz N` / `set dt S` for the whole run and
  `RA2_PLAY_HZ` to re-run a script at another render rate, `set align N`
  (inputs/cheats on aligned logic ticks), `{tick}` in shot names,
  `<out>/shots.txt` (screen point of the `view` target per shot), probe keys
  `anim_tick`, `iseq(<Sequence>)`, `iprone`, `corpses`, `death_anims`,
  `veh_explosions`; cheats `infkill H:TYPE[:n] INFDEATH`, `cheer H`.
  Scripts: `anim-buildings.play`, `anim-infantry.play`, `anim-vehicles.play`,
  `anim-terrain.play`; checker `scripts/play/anim_check.py <out>` (each shot
  pair changes) and `--cross <out60> <out30>` (0 px).
- **Gates**: 26 of 27 print their pinned values; `--demo-saveload` re-pinned
  to `b029b2825cf11e6f` (save format v14, see the table). The infantry
  machine, fear and ProneDamage are armed only by GameScene, so every
  headless gate keeps its sim.

## Effects and mission visuals (wave-3 W3-FIX-C)

Details and evidence: `docs/re/effects.md` (the recovered behaviour), the
status sections "W3-FIX-C" of `docs/gap/wave3/01..03`, and the `fx-*`
`--play` scripts. All of it is presentation: it observes the World, owns its
own RNG, steps once per World frame (`vet_frame`), and is never hashed or
saved.

- **Trigger visual actions run** (campaign): PlayAnimAt (the RA2
  deduplicated `[Animations]` index: objective arrows, ALL12/SOV12 chrono
  flashes, SOV01 FLAMEGUY), ParticleAnim, MoveCameraToWaypoint (the recovered
  pan-speed table) and CenterCamera, CreateRadarEvent (the minimap zoom
  square, also drawn for the EVA radar events), FlashTeam /
  FlashBuildingsOfType, MakeHouseCheer (CheerSound; the infantry Cheer is
  W3-B's), SetTabTo, DoExplosionAt (the warhead's impact anim).
  They reach the scene through `TriggerHost::present` (the trigger log, its
  hash and the save layout are unchanged). LightningStrikeAt (90) and the
  trigger lightning storm (102) are World actions (SOV03/SOV10 storms now
  strike and damage).
- **Particle systems** (`engine/pres_effects.inc`): Smoke / Spark / Railgun /
  Gas behaviours from `[ParticleSystems]`/`[Particles]`; damaged units and
  buildings smoke from `DamageParticleSystems` at `DamageSmokeOffset` on the
  health-condition edges, the smoke follows its owner and stops above
  ConditionYellow.
- **Debris**: destroyed vehicles and buildings throw `MetallicDebris` /
  `DebrisAnims` Bouncer anims (bounce physics, TrailerAnim smoke, ExpireAnim)
  and `DebrisTypes` voxel debris (TIRE, AXLE).
- **Trails**: projectile `Trailer=` every SpawnDelay frames, `UseLineTrail`
  line trails (IFV / Patriot / Harrier missiles), the V3/Dreadnought rocket
  `V3TAKOFF`/`V3TRAIL` puffs.
- **Water**: ship wakes (`Wake=`, every 8 frames while sailing), splashes
  (`SplashList` for Conventional warheads on water).
- **Ore twinkles** (`OreTwinkle`, chance 1/`OreTwinkleChance` per ore cell)
  and object **AmbientSound** loops (ore mines, fountains, cows, ships...).
- Effect keys are read from the RA2 `rules.ini` first (YR's rulesmd drops
  `LargeRedSmoke` and comments out building smoke).
- `--play` hooks: `cheat camp_fire NAME` (fire a map trigger now),
  `cheat camp_log SUBSTR`; probe keys `fx_systems`, `fx_particles`,
  `fx_debris`, `fx_twinkles`, `fx_wakes`, `fx_lines`, `fx_v3_puffs`,
  `fx_splashes`, `fx_anims`, `radar_pings`, `ambient_live`, `sw_strikes`,
  `sw_bolts`, `projectiles`, `proj_last`. Scripts: `fx-particles`,
  `fx-naval`, `fx-m-all01`, `fx-m-all12`, `fx-m-sov01`, `fx-m-sov10`.
- Deliberately cosmetic [RE-FALLBACK]: Gas particle damage (no RA2 weapon
  makes Gas particles) and debris impact damage (DBRIS*LG, PIECE).
- **Gates**: all 31 `--demo-*` gates print master's pins after the merge with
  W3-FIX-A/B (no re-pin: the effects are unhashed, the trigger log lines are
  unchanged and LightningStrikeAt/Storm only run in SOV03/SOV10). Play
  scripts: 60/63 pass; `probe-bridge`, `probe-flow` and `robust-missing` fail
  identically on master (pre-existing). The fx scripts also pass at
  `RA2_PLAY_HZ=30` (effects step on the World frame, not the render frame).

## Object draw positions (wave-4 W4-FIX-POS)

Details and evidence: `docs/gap/wave4/01-positioning.md` (the recovered draw
formulas and the "W4-FIX-POS status" section), `docs/re/building-render.md` §2
(rewritten) and the `pos-*` `--play` scripts. Every SHP is a centred blit; only
the anchor differs per object kind (shared helpers in `engine/iso_project.*`):

- **Buildings and everything attached to them** draw with the canvas centred on
  the **top vertex of the foundation's origin cell**
  (`ra2engine::building_draw_point`, original `FUN_00459ee0` /
  RA2 `FUN_00453c70` = `Location - (128,128)`): the main image and shadow, bib,
  ActiveAnim/Idle/Production/Super slots, doors, damage fires (`FUN_0043c050`),
  rubble, the DestroyAnim ruin, voxel turrets (`TurretAnimX/Y`) and the
  civilian/landmark map dressing. The old foundation-centre anchor drew every
  building `((fw-fh)*15, (fw+fh)*7.5)` px low (the Statue of Liberty 45 px, the
  White House 60 px). The depth key stays the foundation centre.
- **FX anims use the real terrain level** (impacts, building explosions,
  superweapon anims, defence muzzles/beams, the DestroyAnim ruin); anims apply
  their art `YDrawOffset`.
- **Overlays, ore, gems, crates**: the cell centre plus the original's Y offset
  (`overlay_y_offset`: -12 Wall/Tiberium/Crate, -1 Railroad, -16 high-bridge
  cells and -31 for OverlayData 9..17). Bridges join their roads, walls meet
  their towers, ore sits in its own cell.
- **Smudges**: the cell's top vertex. Ore-mine (`SpawnsTiberium`) shadows are
  lifted 16 px with the image.
- `RA2_POS_DBG=1` outlines each building foundation and marks its draw point;
  `cheat view_key PREFIX [n]` centres on a `terr:`/`civ:`/`ovl:`/`smudge:` map
  object. Scripts: `pos-all01`, `pos-all03`, `pos-all08`, `pos-all12`,
  `pos-sov05` (screenshots to read against `docs/gap/wave4/01-evidence/ref`).

## Missiles, rockets and the ALL01 opening (wave-4 W4-FIX-PROJ)

Details and evidence: `docs/gap/wave4/03-all01-opening.md` and
`docs/gap/wave4/04-projectiles.md` (each opens with a fix-status table),
`docs/re/projectile.md` §6.4 (homing, now CONFIRMED and implemented). Save
format v15.

- **Per-rocket-type warheads** (`engine/rocket.{hpp,cpp}`, FUN_00662A70): V3,
  DMisl and CMisl each explode with their own `[CombatDamage]` *Warhead*
  (elite *EliteWarhead* + EliteDamage when the launcher was elite). A
  Dreadnought missile now does the original's 300 x 2% = 6 to ALL01's Statue
  instead of V3WH's 150000, so the **ALL01 opening shelling hold is lifted**:
  the four Dreadnoughts shell the Statue from t=0 and the Liberty Island
  Patriots shoot the missiles down.
- **RocketLocomotion** (FUN_00661D00): PauseFrames on the launcher, TiltFrames
  PitchInitial->PitchFinal, climb with +Acceleration to the block Altitude (the
  map's `[General] DMislAltitude=1550` in ALL01), LazyCurve=yes arc (V3) or
  TurnRate level-off and dive (DMisl), BodyLength nose-contact detonation. A
  rocket on its launcher is not a target; a rocket shot down dies the
  Explodes=no death (its Explosion= anim in the air, no warhead damage). The
  hull draws at its pitch, `NoShadow=yes` is honoured, spent rockets are
  DontScore (not in `units_lost`).
- **Homing projectiles fly** (`ProjKind::Homing`, `integrate_homing`): the
  Patriot, IFV, Harrier / Black Eagle Maverick, Apocalypse tusks, Aegis,
  Typhoon torpedo (Level), Hornet / Osprey bombs -- the recovered steering
  (MissileROTVar weave, terrain-following cruise, course lock, give-up,
  MissileSafetyAltitude), `Acceleration` default 3.
- **Projectile art** comes from the weapon that fired (unit slot or base
  defence): art `Rotates`, `Voxel=yes` bullets (ZBOMB, SUBT) as voxels, the
  unit palette unless `AnimPalette=yes` (tank shells are visible again),
  `Shadow=yes` blob, line trails/Trailers on real flights.
- The Statue's DestroyAnim ruin draws in its art `Palette=lib`; no crater or
  scorch smudge on water. The projectile pass reads a per-frame cell index
  instead of scanning every unit per projectile.
- Scripts: `all01-opening` (gate: 4 min no input), `all01-v3-statue`,
  `missiles`, `missiles-intercept`. Probe keys `rockets_launched/shot/hit(h)`,
  `rockets_live(h)`, `rocket_alt(h)`, `rocket_phase(h)`, `homing_live`,
  `homing_top`, `homing_near`, `homing_speed`, `bhp(h,ID)`.

## Mission control and touch (wave-4 W4-FIX control, branch `w4fix-ctl`)

Fixes the "I can't control the mission" report (`docs/gap/wave4/02-mission-control.md`,
`06-android-missions.md`). RE notes: `docs/re/movement-zones.md`, `docs/re/mission-view.md`.

- **Amphibious units swim (W4-MC-01).** Every `Amphibious*` MovementZone
  (`AmphibiousDestroyer`: Tanya, SEAL, PCOMMANDO, JFISH, CPHAL; `AmphibiousCrusher`: APC;
  `Amphibious`: LCRF/SAPC/HVR) and `SpeedType=Amphibious` use the land+water domain
  (gamemd MovementZone table @0x81AA98). Only a naval factory launches onto water. In ALL01
  Tanya swims to Liberty Island and the 11 GIs become the player's.
- **LockInput / UnlockInput (W4-MC-02)** lock pointer, keys, touch, edge/arrow scroll and
  the sidebar and hide the cursor (FUN_00683c80 / DAT_00a8dd04). Esc / Back, F1 and the
  options button still work. Not saved; a lock is released after 1800 frames or at mission end.
- **Opening view (W4-MC-03)** on `Waypoint[[Basic] HomeCell]` (FUN_004acd80); ALL01 opens
  on the GIs at wp98.
- **`[Map] LocalSize` (W4-MC-04)**: the campaign camera is clamped like the TacticalClass
  clamp FUN_006d7bb0; off-board foundations cannot be placed; probe counts exclude off-board
  and shrouded units. Skirmish maps keep the old free camera.
- **Scripted pans (W4-MC-05)** stop when the player scrolls after the unlock.
- **Touch (W4-AM-01..09/11)**: drag = scroll, tap = select/order, double tap = type, hold+drag
  = box, hold+lift = cancel mode (never deselects), two-finger drag = scroll, two-finger tap =
  deselect; hold ring; mode banner with [X]; no tooltips from taps; left-edge command column
  (closed by default, auto-closes, BRF briefing button); radar drag; strip swipe; sidebar
  near-miss snapping; phone canvas 0.75 dp/px. See `docs/INSTALL.md` §4.
- **Side-keyed sidebar textures (W4-AM-04)**; **APK version = VERSION+g<hash>** shown on
  the main menu (W4-AM-12).
- `--play`: `touch tap|dtap|hold|drag|boxdrag|pan2`, the `onscreen` click flag, probe keys
  `input_locked`, `ctl_units`, `ctl_on_screen`, `view_cx/cy`, `touch_*`, `tooltip_up`.
  Control scripts: `ctl-all01` (real clicks), `ctl-touch-all01` (real gestures), `ctl-all02`,
  `ctl-sov01`, `ctl-sides`.
- **Verification** (merged with master `f3f1ea1`): unit suite 822/822; 74 of 77 `--play` scripts pass,
  one at a time (`probe-bridge`, `probe-flow`, `robust-missing` fail as on master); `all01-opening`
  passes with the rockets live and the input lock. Gates: every `--demo-*` gate prints its pin except
  `--demo-campaign`, re-pinned to `9261ef609a25c962` (above). Save format unchanged (v15: the lock is
  not saved). Emulator (`ra2x86`): `docs/gap/wave4/06-evidence-fix/`.

## Mission sequences: team delivery, scripts, high bridges (wave-4 W4-FIX-SEQ, branch `w4fix-seq`)

Fixes for `docs/gap/wave4/05-mission-sequences.md` (status table + verdict there). Every rule is cited to
gamemd.exe in `docs/re/campaign.md` §9 and `docs/re/world-depth.md` §B3.

- **Transports carry their cargo (W4-SQ-01).** A `Full=yes` reinforcement creates its passengers INSIDE
  the last-created transport (`FUN_0065d770`). Script 8 Unload waits for empty transports, then its arg
  decides who stays in the team (0 all, 1 transports, 2 unloaded units, 3 none, `FUN_006ee670`);
  14 Load / 43 WaitLoaded are implemented. SOV02's landing force stays the player's (only the empty
  SAPCs change house) and the MCV chain works; ALL05's spies are delivered once; ALL04/06/08/11/SOV04/10
  craft arrive loaded. The Deploy key unloads PlayerControl houses' transports too.
- **Paradrops by plane (W4-SQ-06).** `Droppod=yes` teams arrive in a PDPLANE from the TransportWaypoint
  or the house edge and drop under parachutes (art `Paradrop=` sequence, `ParachuteMaxFallRate`); the
  team script starts once they are down. ALL09's LZ beat at 9 s (was 119 s), SOV10's at 30 s.
- **VeteranLevel (W4-SQ-04)**, **script 46/47/58 BuildingTypes arg + garrisons (W4-SQ-02)** (ALL04's 85
  Conscripts occupy 9 civilian buildings; MultiplayPassive owners lend their buildings),
  **Sleep/Harmless + Wakeup 81/82/83 (W4-SQ-05)**, **no-dock aircraft leave the map (W4-SQ-03)**,
  **RevealZone (18) and HouseDiscovered (5)**, **computer houses train what their scripted teams miss
  (W4-SQ-12, RE-FALLBACK)**.
- **Destructible high bridges (W4-SQ-10 / A1-06).** High-bridge decks carry ground units while intact,
  trigger 63 Apply100Damage calls the bridge destroy dispatch on its waypoint (`FUN_006dfa00` ->
  `FUN_00586bb0`), the span collapses (units on it die, the deck stops drawing), a bridge hut repairs it.
  SOV01 and ALL01 break their bridges at f0. Units on a deck still draw at ground level.
- **One-sided alliances.** Campaign houses see each other through their own `Allies=` list (the player's
  side unchanged). Root cause of SOV09's early loss (W4-MC-08): the Russian drop groups were on the
  Americans' team and hunted the Yuris. A player order now releases a unit from its script team
  (W4-MC-06, RE-FALLBACK).
- **Save format v16**: AnUnit `para_alt`, SwParams `parachute_fall`, team airdrop flag, host `u_mission`,
  World player-order queue / passive houses / high-bridge cells / scripted-team AI demand.
- **Play scripts** (real orders): `m-sov02-real`, `m-sov02-mcv` (one cheat for the beach assault),
  `m-all04-real`, `m-sov09-real`; `m-all04` / `m-sov09` follow the new delivery. Unit test
  `tests/test_high_bridges.cpp`.
- **Open:** ALL12 no-input is lost at ~17 s (3 elite SEALs drop among 16 guards + 8 dogs that no longer
  fight the prisoners) -- verdict DOUBTFUL, needs a SEAL-vs-Conscript combat check; 61/62 TurnOff/On,
  35 PreferredTarget, events 23/34; units drawn at ground level on high-bridge decks; the hut click in
  SOV01 resolves the wrong cell (building pick vs anchor) so the repair is proven by the unit test.
- **Verification** (merged with master `eb87581`): unit suite 826/826; 78 of 81 `--play` scripts pass (one at a time; on
  the first sweep `m-all04` / `m-sov09` failed and were updated for the loaded craft / paradropped
  Yuris); `probe-bridge`, `probe-flow`, `robust-missing` fail as on master. Gates: every
  `--demo-*` gate prints its pin except `--demo-campaign` and `--demo-saveload`, re-pinned above.

## Android, performance and robustness (wave-5 W5-F, branch `w5-android`)

- **Perf overlay and log (R-170).** Three-finger tap, Ctrl+Shift+F, `--fps`, `RA2_PERF=1`,
  RA2.INI `[Engine] ShowFPS=yes` or `adb shell setprop debug.ra2.perf 1`: FPS, logic ticks/s
  and ms per frame (update / render / present / worst) at the bottom-left, plus a `[perf]` log
  line every 5 s. `docs/INSTALL.md` section 5 is the "how to measure on a real phone" recipe.
- **ALL03 opening (R-171).** Profiled with the new dev sampler (`RA2_PROFILE=<file>`,
  `tools/profile_report.py`): 86 % of the time was A* (`find_path_nearest`, 20000-node budget)
  re-run every logic frame for attackers whose approach cell is unreachable. `pathfind.cpp` now
  memoises A* results per grid, validated against a byte copy of the grid (a pure function, so
  the result is identical by construction). `probe-perf-all03` (1800 frames): ~11 -> ~100
  logic frames/s headless, `play_hash` unchanged.
- **Phone render cost.** The light map (`pres_light.inc`) is redrawn only when the camera,
  canvas, base colour or lit set changes. Each redraw cost two render-target switches (62 % of
  an emulator frame in `glCheckFramebufferStatus`). Lit shots are pixel-identical.
  Emulator (`ra2x86`, SwiftShader, ALL01 opening 1080p): **14.0 -> ~17 fps**; the engine's
  render is now ~2 ms and the rest is the host GL pipe. No interpolation (R-172): the original
  draws once per logic frame (`docs/re/gamespeed.md`).
- **Android.** Audio focus (R-163: a call mutes, "may duck" ducks to 20 %, regaining restores);
  the importer opens at `Download/RA2`, explains the sub-folder rule, also accepts the parent
  folder, disables Exit/Back during the copy and cleans `*.part` files (R-160/161); a mid-game
  panel resize re-lays the canvas (R-162); the soft keyboard keeps SAVE visible and the default
  description is selected (R-152); campaign list rows snap to the nearest row on touch (R-151).
  `docs/PHONE-README.txt` replaces the stale package README (one-finger drag now scrolls).
- **Robustness.** `load_world` range-checks every cross-reference (R-181); the sidebar
  backlog / hold state is saved (`QBKL` section, **save format v17**, R-180); `ra2game.exe` is
  a WIN32 build of the same objects (no console, log in `%APPDATA%\ra2engine\RA2\ra2engine.log`,
  R-182); `--record <f>` / `--replay <f> [--hidden]` (or `RA2_RECORD`) reproduce a session
  (R-184; a recorded `ctl-sov01` replays to the same probe hash).
- **Scripts (R-183).** `probe-bridge` and `probe-flow` updated; `robust-missing` skips on a
  full install (`if data_missing_optional == 0 then end`, `if ... then end` is now allowed);
  `--controls-selftest` retired (pre-MCV flow, wrote the user's RA2.INI).
- Evidence: `docs/gap/wave5/06-evidence/`.
- **Verification** (merged with master `f0fbd7b`): unit suite 829/829; **82/82** `--play`
  scripts pass (one at a time; `probe-bridge`, `probe-flow`, `robust-missing` included);
  all 27 `--demo-*` hash gates print their pins (no re-pin; `--demo-saveload` does not hash
  the format version or the new optional section).

## Combat and movement core (wave-5 W5-B, branch `w5-combat`)

Every rule below was recovered from `gamemd.exe` first (private Ghidra copy
`D:\ra2-re\w5-combat`; notes in `docs/re/combat.md` §8-§10) and is armed for
the real game by `engine/w5_combat_arm.inc` (`w5_arm_combat`, GameScene ctor /
load path and the save-load gate World). Unit-test Worlds keep the old model
unless a test opts in. `W5_OFF=sub,land,crush,elev,threat` disables single
parts for diagnosis.

- **Splash, falloff, friendly fire (R-060).** One applicator, `damage_area`
  (DamageArea `FUN_00489190`), serves instant hits (detonating at the victim's
  coordinates), every projectile detonation and base-defense shots. Cells
  scanned = the original ring table (`DAT_007EC3D0` counts, 369 offsets
  emulated out of the initializer at 0x561540), radius `ftol(CellSpread*256)`,
  3-D distance, aircraft in the air at half distance. Buildings take one hit
  per foundation cell in range (distance 0 in the impact cell, else from the
  cell centre). All houses are hit; `AffectsAllies=no` spares the firer's own
  and allied houses (TechnoClass::ReceiveDamage). Falloff is Modify_Damage
  `FUN_00489090`, disassembled: linear in float to `PercentAtMax` at the edge,
  then `ftol(dmg*Verses)`, `MaxDamage=10000`; only buildings are floored at 1.
- **Three infantry per cell (R-061).** `Occupancy` holds the spots 2 (right),
  3 (left), 4 (down) per cell (`FindInfantrySubposition FUN_00481090`, spot
  coordinates `DAT_0089D960`, preference rows `DAT_0081BC94/BCA8`). Infantry
  reserve a spot in each cell they step into (keeping their lane), squads pack
  three to a cell (`assign_infantry_formation`), map `[Infantry]` SubCell
  places on its own spot, an infantry cannot enter a vehicle's cell and a
  vehicle cannot enter a cell with infantry. `Unit::spot` and the lepton offset
  `Unit::sub_x/sub_y` (interpolated while walking) are exposed for render:
  `iso_renderer` draw/selection/health bars and the main.cpp pips/chevrons use
  them, aim and splash use them.
- **Land speed (R-062).** GroundType table (`FUN_00673A40`) from rulesmd
  `[LandType]` Foot..FloatBeach, TMP terrain-type byte -> LandType
  (`DAT_008278F4`), ore cells = Tiberium; the Drive/Ship locomotor multiplier
  (`FUN_004B2550`): cost capped at 1, 0 -> .5, Tracked/Wheeled Uphill/Downhill,
  x.75 at ConditionRed. Infantry (Walk locomotor) do not read the table.
- **Crush (R-050).** `ShouldCrush FUN_00742720` / `Mission_Attack
  FUN_007402F0`: a Crusher within `[CombatDamage] Crush=1.8` cells of a
  crushable target drives onto it instead of firing -- computer houses always,
  the player with `PlayerAutoCrush=yes` or an `AutoCrush=yes` type. Every
  crushable enemy infantry in the cell is squashed; idle friendly infantry
  blocking a vehicle scatter.
- **3-D range (R-063).** `FUN_006F6750`: 3-D distance, [ElevationModel] bonus
  (`FUN_006F6490`), buildings measured to the centre + (w+h)*64, the
  SubjectToCliffs line trace (`FUN_004CC020/280`); flying shooters 2-D.
- **Idle threat (R-064, partial).** Evaluate_Object `FUN_006F71D0`: computer
  units may auto-target unarmed buildings; the player's only armed ones.
- **Prerequisites (R-065).** Counted from placement (`Unlimbo FUN_006F61D0 ->
  RegisterGain FUN_005028A0`), build-up included.
- **Flak and shells (R-057, partial).** FlakProj impact scatter
  (projectile.md §5.3) on the instant path; the shell "close enough" re-aim
  onto an in-cell target (`BulletClass::Update` tail). Walls are not traced.
  Sight from height: the original has no radius bonus (`FUN_0070A240`).

Play scripts: `w5-subcell`, `w5-splash`, `w5-crush`, `w5-terrain`, `w5-squid` (all PASS);
probe keys in `engine/w5_play.inc` (`max_cell_inf`, `cells3`, `uhpn/ucx/ucy/
uspot/ulx/uly/uspeed/uland/utgt/umov/uwait`, `bhp/bmaxhp`; points `ore_near`,
`clear_near`). Save format v20.

Also: SHP ships (Dolphin, Giant Squid) get their ROT (the aim gate held their
fire), ship approach heads for a free water cell beside the target (the Squid
grab now fires), sub-cell aware click picking and `--play` unit targets, the
presentation hook `pres_subcell_offset` reads `Unit::sub_x/sub_y`. The A*
result cache (pathfind.cpp) needs no change: W5-B adds no path costs, and the
traffic-aware re-path grid it hashes already folds the sub-cell occupancy.

Gates re-pinned (causes in the table): `--demo-combat` logs 87 -> 78,
`--demo-tank` logs 143 -> 163, `--demo-naval`, `--demo-projectile`,
`--demo-campaign`, `--demo-saveload`; every other gate prints its pin; all
deterministic. Script updates (each commented in the script): `sk-a`
(guards + fielded-units count), `economy-60min` (more scaffolding guards),
`naval` (both GIs board, sinking a frame later, the Dolphin's dive),
`m-sov09-real` (re-issued pull-back), `ctl-all02` (+1 -3 open ground).
Open: `m-sov02-mcv` -- with the sub-cell fight timing, SOV02's 'Transports
Off' team (CreateTeam) fires one frame after 'Land the MCV' and recruits that
team's MCV boat (its TeamType is AreTeamMembersRecruitable); the clone's
`recruit()` ignores team priority, so the boat is stolen and sent away with
the MCV aboard. Campaign team recruiting is agent A's area.
- **Verification (W5-B)** (merged with master `72b2da3`): unit suite 843/843;
  110/111 `--play` scripts pass one at a time (5 new `w5-*`); the one failure
  is `m-sov02-mcv` (above). Gates as listed; save format v20.
## Audio and shell UI (wave-5 W5-E, branch `w5-audio`)

RE on `gamemd.exe` with the private Ghidra copy `D:\ra2-re\w5-audio` (jobs j1..j50);
details in `engine/pres_audio_events.inc`, `keyboard_page.inc`, `movie_player.inc`,
`skirmish_load.inc` and the comments at each call site.

- **Unit / world sounds (R-130).** The click voice follows the original's
  `ClickedMission` (`FUN_006FF0A0`): VoiceHarvest, VoiceEnter (dock, transport, garrison),
  VoiceCapture (engineer), VoiceDeploy (D), VoiceSpecialAttack (C4 on a building), each with
  the original fallback chain. VoiceFeedback on the NowYellow damage state, 30 %
  (`FUN_00700DA0`). Sinking / Crashing voices and sounds (`FUN_004DA440`), TurretRotateSound
  while a turret turns (Grand Cannon, `FUN_006F9380`), Deploy / UndeploySound (GI, MCV),
  BuildingGarrisonedSound, BuildingRepairedSound (engineer), Crate*Sound / HealCrateSound,
  CheerSound (AllToCheer), RepairBridgeSound, and CreditTicks: the credit counter now rolls
  `|diff|>>3` (1..143) per tick and ticks up / down (`CreditClass` `FUN_004A2510`).
- **EVA (R-131).** PrimaryBuildingSelected, NewRallyPointEstablished, OnHold, UnableToComply,
  SelectTarget, StructureGarrisoned / Abandoned (RadarEvent 15), UnitRepaired (RadarEvent 8),
  BridgeRepaired (RadarEvent 14), TechBuildingLost + the type's `CaptureEvaEvent` /
  BuildingCaptured (RadarEvent 10, `FUN_004481AB`), crate Unit{Armor,Speed,FirePower}Upgraded,
  BuildingOffLine / OnLine (the power-toggle mode: see W5-E2 below).
- **sound.ini (R-132).** PREDELAY / AMBIENT predelay from `Delay=` (lo = 33 ms for AMBIENT,
  re-drawn on every loop pass), Type=LOCAL and Type=SHROUD (silent over unrevealed cells), the
  0.05 audibility floor, and the corrected attenuation: a source anywhere on screen is at full
  volume, the linear falloff starts at the screen edge (`FUN_0074F8E0`).
- **Keyboard page (R-140, R-150).** Options 0x5CE -> dialog 0xA3 from the main menu and the
  in-game Game Controls page: categories / command names / descriptions from the CommandClass
  vtables, capture a key, Assign (moves a used key), Reset All, Back saves
  `<settings>/Keyboard.ini` and the running game reloads it. New hotkeys: CombatantSelect,
  HealthNav (RA2 `Keyboard.ini`), VeterancyNav, Follow, AllToCheer, CenterOnRadarEvent,
  ScreenCapture. PlaceBeacon / ToggleAlliance / PageUser / taunts are network-only in the
  original; PlanningMode is not modelled.
- **Score pages (R-141).** The Score column is the original's (`MPScore.cpp` `FUN_005C92C0`:
  victim cost x (1 + rank) per kill, nothing for allied / DontScore victims, the survivor's
  bonus), rows sorted by it. The Campaign Results page draws the 0xBC7 graph well (player vs
  the strongest opponent: Kills / Losses / Built / Score) and First / Second Place.
- **Briefing and movies (R-034, U-5 ready).** The briefing is a modal shell page (Restate
  dialog 0x10C, `FUN_0065EF60`) on F1 and on the sidebar's second tab in a campaign; the game is
  paused while it is open. The map's Intro / Brief movies play before a mission, Win / Lose
  before its result, trigger actions 10 / 100 / 116 in the mission -- through the Bink decoder
  when a `.bik` exists (`docs/INSTALL.md` "Movies"); the install's stubs are skipped.
- **Options (R-146).** Visual Details drives the anim `DetailLevel=` filter (low detail drops
  wakes, twinkles, power-plant lights); the non-volume sliders tick, Sound / Voice play a
  preview; the resolution combobox works on desktop (window + RA2.INI `[Video]`).
  "See Hidden Objects": see W5-E2 below.
- **Shell (R-173 part).** Skirmish loading screen `ls800<country>.shp` + `MPLS<country>.PAL`
  (YR `loadmd.mix`, `FUN_005529F0`) with the player list; the menu CRT movie keeps its place
  across shell pages and pauses when the window loses focus.
- **Saving.** A "Saving game..." frame before the write, a free-space check ("NOT ENOUGH DISK
  SPACE"), and the music state in a new `AUDI` section (**save format v19**).
- **Evidence.** `--play` now writes `<out>/pres.log` and exposes `snd(KEY)`, `why(<trigger>)`,
  `eva(NAME)`, `evareq(NAME)` probe counters. New scripts: `audio-events`, `keyboard`,
  `score-pages`, `briefing-movies`, `options-w5`, `save-w5`; shots in `docs/gap/wave5/E-evidence/`.



## Audio and shell UI, part 2 (wave-5 W5-E2, branch `w5-audio2`)

RE on `gamemd.exe` with the private Ghidra copy `D:\ra2-re\w5-audio` (jobs j51..j90).

- **Power toggle (R-150, R-131).** `DisplayClass::SetPowerToggleMode` (`FUN_004AC730`,
  +0x11B2) exists, but YR 1.001 has no caller that arms it (the sidebar Action handler knows only
  repair 0x8065 and sell 0x8066). The engine models what it does: in the mode, a click on an own
  building with `Power<0` powers it down or up (`GoOffline` / `GoOnline`, `FUN_00452350` /
  `FUN_00452250`, flag +0x660). The drain leaves the house's power totals, a powered-down
  defence goes offline, and EVA BuildingOffLine / BuildingOnLine plays. The mode is reached via
  the `--play` cheat `power_mode`; `World::set_building_power` is also the hook for trigger
  actions TurnOff / TurnOn (`FUN_006DCE20`). `Building::power_off` is saved (**save format v21**).
  PlanningMode stays open.
- **Infiltration EVA (R-131).** Follows the spy infiltration in `FUN_004571E0`. The victim
  hears RadarSabotaged, PowerSabotaged, CashStolen or TechnologyStolen. The spy's owner hears
  BuildingInfRadarSabotaged, BuildingInfCashStolen, or BuildingInfiltrated, with
  EnemyBasePoweredDown or NewTechnologyAcquired where they apply.
- **Sequence sounds (R-133).** The `<Sequence>Sounds= frame sound [frame sound]` line
  (`FUN_00523A30`, at most two pairs) plays when `stage % count == frame`. Only the YR T-Rex
  (`TrexSequence`) uses it.
- **See Hidden Objects (R-146).** `TechnoClass::AI` -> `FUN_0070E600` attaches the
  `[AudioVisual] Behind=` anim (BEHIND). It applies to a `CanBeHidden` non-aircraft object that
  a structure drawn after it covers. The anim is only drawn when the option is on (Options+0x1F,
  `FUN_00423AE0`). Infantry use their sub-cell offsets.
- **Movie and BAG audio (R-134).** Stereo IMA-ADPCM BAG clips are decoded: per chunk, an L
  header and an R header, then 4-byte L/R sub-blocks. A new Bink Audio decoder handles RDFT and
  DCT, mono and stereo. It matches ffmpeg's decode of crafted files sample for sample
  (`tests/test_bink_audio.cpp`; [RE-FALLBACK: the published bitstream, not binkw32.dll]). A
  movie's first audio track plays through the mixer's music slot. The theme pauses for the
  movie and comes back after it. The install's own `.bik` files carry no audio, so the menu CRT
  stays silent because of the data.
- **Bink off the main thread (R-173).** `engine/bink_async.hpp`: a worker owns the decoder.
  It decodes frames in order up to the one the clock asks for. The main thread only uploads the
  newest finished picture. It is used by the menu CRT and by the movie player. Under `--play`
  the worker blocks on each frame, so captures stay deterministic. A damaged frame ends the
  movie instead of crashing it.
- **Shell leave transition (R-143).** `FUN_00606980` with the leave flag off works in 30 ms
  ticks. Row i starts i+1 ticks late. A button row slides its face back into the socket (SDBTNANM
  5..10). A row without a button plays the plate band (11..16). The page switch waits for the
  animation, and input is ignored meanwhile. Under `--play` the switch stays immediate unless a
  script says `set leave_band 1`, because several skirmish scripts depend on the frame the game
  starts on (`air` fails with a 6-frame shift even on master). The main-menu header shimmer is
  still open.
- **Evidence.** New scripts: `power-toggle`, `spy-eva`, `seq-sounds`, `see-hidden`,
  `shell-leave`. Shots are in `docs/gap/wave5/E2-evidence/`. Unit tests: `test_world_power_w5`,
  `test_bink_audio` (3 fixtures), `test_bink_async`, and the stereo case in `test_audio`.
- **Verification (W5-E2).** Merged with master `7c9bb36`. Unit suite: 853/853. `--play`: 115/116 scripts pass, run one at a time (5 new). The one failure is the known `m-sov02-mcv` (W5-B, above). Gates: 25 of 27 print their pins. `--demo-saveload` was re-pinned because of the v21 `power_off` field. `--demo-ai-trigger` was re-pinned because of a W5-B change: the w5-combat build prints the same hash. Save format **v21**.

## Presentation: layers, base-building chrome, state effects, lighting (wave-5 W5-PRES, branch `w5-pres`)

RE notes: `docs/re/w5-presentation.md` (gamemd.exe addresses, confidence per item). All of it
is render-only (`g_fx` / `g_map_dressing` / `g_light` exist only in GameScene), so no gate
hash moves.

- **Layers (R-100/R-101).** The original draws high-bridge decks in the terrain pass with a
  Z-buffer; the clone has none, so decks (`BRIDGE1/2`, `BRIDGEB1/B2`) join the ground depth
  sort at `x+y+0.99`: a ship or a unit under an overpass is covered, a ground unit on a deck
  over water draws BridgeHeight (4 levels) up and sorts after its deck. Airborne aircraft and
  jumpjets draw after the whole ground layer (the original's Air/Top layers).
  `pres_compat.inc` picks up `World::high_bridge_destroyed` (broken decks vanish), the
  paratrooper state and agent B's sub-cell offset by SFINAE.
- **Base-building chrome (R-102/R-144/R-039).** Opaque `PLACE.SHP` cells (frame 0 valid,
  frame 2+ramp blocked, PALETTE.PAL green/red; no building ghost in vanilla); the selected
  primary factory shows the CSF `TXT_PRIMARY` box in its house colour; the rally point is the
  original's marching-ants dashed line in the house colour; trigger FlashCameo flashes a cameo.
- **Turrets (R-121).** Defence turrets draw the sim's ROT-slewed facing (no snapping; a building
  turret keeps its aim, as the original).
- **State effects (R-120).** Per-object `ObjTint` (colour mod + additive re-draw + alpha) in
  the unit / voxel / building blits: the Iron Curtain brightness envelope (flash, dim pulses,
  flash out -- IronCurtainColor=0 means no red), the elite promotion and FlashTeam flash
  (2-frame 2.0x blinks), chrono warp (50 % translucent, WARPOUT at both ends, CHRONOSK).
- **Parachute (R-122).** PARACH over falling paratroopers: opening, glide loop, collapse.
- **Lighting (R-104).** Light above 1.0 brightens up to 2.0x (MUL pass) instead of clamping;
  units, buildings and turrets take their own height light (and +4 levels on a bridge deck).
  The W5-F light-map cache key includes the excess colours.
- **Remap (R-103).** Indices 16..31 come from the original ColorScheme generator (sin/cos
  table, integer HSV), not the brightness-scaled HSV heuristic.
- **Anims (R-123).** PingPong, Reverse, Normalized, RandomRate clamp (a fixed 900/b, as the
  original), IsMeteor bounce, Spawns/SpawnCount on impact. Ore art: cell-hash variant and the
  TIB slope images on ramps.
- **Sidebar (R-142).** Credits roll `clamp(|d|>>3,1,143)` per logic frame in the side colour
  (Allied light blue, Soviet yellow); tabs 0/1 flash (frames 3/4) while a building is ready.
- **Air.** Airborne voxels cast their own silhouette on the ground; jumpjet wobble per
  `JumpjetWobbles/Deviation/NoWobbles` (every RA2 jumpjet but SHAD sets NoWobbles).
- **Mirage (R-047).** An enemy's still Mirage draws as its `DefaultMirageDisguises` tree from
  agent C's `Unit::w5.mirage_tree` (enemy viewers only; verified with w5-abil merged).
- **Lobby (R-105).** The skirmish lobby hides the 11 YR-only DESERT/NEWURBAN/LUNAR maps.
- Scripts: `pres-bridge`, `pres-bridge-on`, `pres-place`, `pres-state`, `pres-light`,
  `pres-para`, `pres-air`, `pres-mirage` (cheats in `engine/pres_play_cheats.inc`).
- Open: aircraft bank/pitch (only a few Fly aircraft), the jump-weapon projectile draw (R-054,
  needs the sim to limbo the firer), the SOV05 Paris Tower visual (R-041), building-anim
  YSort as separate sort items, TIB2/TIB3 art (the OreField keeps no tiberium type), broken
  high-bridge ramp art.

## Unit and building abilities (wave-5 W5-FIX-C, branch `w5-abil`)

Calibration: `docs/re/abilities-w5.md` (Ghidra on YR 1.001 `gamemd.exe`). Code:
`engine/abil_w5.hpp` (all new state: `BldTypeW5`, `UnitTypeW5`, `BldW5`, `UnitW5`, `WorldW5`),
`engine/abil_w5_rules.cpp` (rulesmd -> type data, hooked at the end of `fill_building_weapons`),
`engine/world_w5.cpp` (the `step_w5` pass, gap shroud, AI pass), plus `building_combat.cpp`
(prism / tesla), `cargo.*`, `world_veterancy.cpp`, `world_depth.*`, `realplay_economy.inc`.
Everything is armed by `wire_w5` in the real `GameScene` only.

- **Stolen tech (R-042).** `BuildingClass::Infiltrate` (`FUN_004571E0`) order: Radar -> Power ->
  [AI] BuildTech by AIBasePlanningSide -> SuperWeapon -> Storage -> Factory. A spy in an enemy
  Battle Lab unlocks the `RequiresStolen*Tech` infantry (CIVAN / CCOMAND) for player and AI; the
  `GACHRO` typo is gone (rules-driven purposes); a SW spy resets only that building's SW; a Naval
  Yard spy gives veteran vehicles and ships (both `Factory=UnitType`).
- **Prism forwarding (R-043).** One idle own prism recruited per frame within the master's
  Secondary range (max 8), DelayedFireDelay 28, damage `(Damage*((150N+100)*256/100))>>8`, supporter
  reload 45. **Tesla charging (R-045).** Idle Tesla Troopers next to an own coil charge it: 2 keep it
  online without power, 3 (or 1 at full power) switch it to OPCoilBolt. **Gap Generator (R-044).**
  An enemy powered gap shrouds a radius-10 circle for human houses, re-applied every 120 frames,
  not lifted by a SpySat; own units still see.
- **Tech buildings (R-056).** `Capturable=yes NeedsEngineer=yes` map structures are real neutral
  World buildings: CAAIRP grants ParaDrop, CAPOWR +200 power, CATHOSP / CAMACH heal owned
  infantry 20 HP / 50 frames and vehicles 5 HP / 75 frames (per building). GASPYSAT is in the
  roster (index 39) and reveals the map while built and powered.
- **Transports.** OpenTopped (Battle Fortress) passengers fire out (+2 range, x1.2 damage); allies
  board each other's transports (R-052); unloading waits DeployTime x 900 frames (R-051).
- **Specials.** Engineers defuse Ivan bombs (R-046); Boris calls a 2-MiG (elite 4) BPLN airstrike
  on a building he designates within 12 cells (R-046); the IFV swap carries the projectile's
  AA/AG and flight, and an Engineer IFV repairs damaged allied vehicles (R-048); the Mirage picks a
  `DefaultMirageDisguises` tree (`Unit::w5.mirage_tree`, drawn by the presentation, R-047);
  ParasitePlus paralysis / culling on the P4 parasite for the Giant Squid (R-049, see open);
  Tanya / SEAL C4 on ships.
- **Veterancy / crates.** SCATTER (a ranked idle unit hit steps aside), CRUSHER / EXPLODES / C4
  switches, aircraft FASTER, kill credit for spawner-launched aircraft/missiles and for the firing
  garrison occupant, the forced-MCV crate (veterancy.md §3.3 rule 1).
- **AI.** Builds ORCA / BEAG onto its airfield pads and recruits docked planes for its Harrier /
  Black Eagle TaskForces (R-080); garrisons a nearby empty civilian building and sends idle
  engineers at neutral tech buildings (R-081, RE-FALLBACK choice rule); spies into Battle Labs
  unlock CCOMAND / CIVAN for its teams.
- **Save format v18** (`WorldW5`, `UnitW5`, `BldW5`, type data, WeaponSpec projectile fields).
- **Play scripts:** `abil-stolen-tech`, `abil-prism-chain`, `abil-tesla-charge`, `abil-gap`,
  `abil-tech-buildings`, `abil-opentopped`, `abil-defuse`, `abil-ifv-repair`, `abil-boris`,
  `abil-ai-air`. Unit test `tests/test_abil_w5.cpp`.
- **Open:** the Giant Squid never fires at a ship in play (the vehicle aim gate in `combat.cpp`,
  agent B); C4-on-ships has no play acceptance (no reliable enemy-ship spawn cell in a script);
  the Armor/Groundspeed difficulty handicaps stay unapplied (AssignHandicap traced, no runtime
  reader found, R-082); AI superweapon defence and AI trigger condition types 2/3/5/6 were not
  re-hunted (the helper RE agents were stopped); NACLON / NAPSIS are Yuri-only under the YR rules
  (prereqs YATECH/YACNST), so they stay out of the Allied/Soviet roster; the Soviet Spy Plane is
  YR-only (not classic).
- **Verification** (merged with master `01094b8`): unit suite 832/832; all 92 `--play` scripts pass
  (one at a time); gates: every `--demo-*` gate prints its pin except `--demo-triggers` (GASPYSAT
  roster growth: one more placed structure), `--demo-saveload` (format v18) and `--demo-campaign`
  (wave-5 type data in the campaign worlds), re-pinned above; `--demo-base`/`--demo-techtree`
  witness `roster=40` (hash unaffected).
- **Verification (W5-E)** (merged with master `92112e4`): unit suite 839/839; **106/106** `--play` scripts pass (one at a time; 6 new); all 27 `--demo-*` gates print their current pins (no re-pin: `--demo-saveload` stays `420ac604a50be366` -- it does not hash the format version or the optional `AUDI` section). Save format **v19**. Also fixed: `load_world`'s range check rejected a garrisoned unit's `cargo_of` sentinel, so a skirmish saved after the AI garrisoned a building could not be loaded (`save-w5`).

- **Verification** (merged with master `53fd609`): unit suite 835/835; **100/100** `--play`
  scripts pass one at a time (the 8 new `pres-*` included; `probe-bridge` / `world-depth` now
  scroll 2 rows to xmp31s2 in the 80-map lobby); `--demo-combat` / `--demo-projectile` /
  `--demo-veterancy` print their pins -- no gate moved (every change is render-only).

## Shell, save/load, Android lifecycle and release (wave-6 W6-FIX-REL, branch `w6fix-rel`)

Fixes `docs/gap/wave6/06-shell-release.md` (W6-06-02..16) and W6-01-10; per-item status in that
report's section 6.

- **Windows package (W6-06-02).** MinGW runtime linked statically, VERSIONINFO resource,
  `pwsh scripts/package-win.ps1 [-Configure]` -> `dist/ra2engine-<version>-win64/` + `.zip`
  (exes, `SDL2.dll`, INSTALL.md, LICENSE, THIRD_PARTY_NOTICES.md); the script checks the
  imports and runs `--version` from the folder with PATH limited to Windows.
- **Android lifecycle (W6-01-10, W6-06-09/10).** An activity destroyed without finishing (a
  config change Android cannot hand over) keeps `AUTOSAVE.SAV` (`nativeKeepAutosave`); wider
  `configChanges`. The startup Resume page is a shell page (`engine/resume_scene.inc`), MAIN
  MENU asks first; a won mission writes RA2.INI `[Engine] ResumeCampaign=` until the next
  mission starts. `--play` gained `lifecycle background|foreground|relaunch`.
- **Importer (W6-06-05/06).** Launcher shortcut + Options > Game Files re-open it at any time;
  it copies patches (`expand/ecache/elocal##.mix`), `thememd.mix` and movies.
- **Save/load (W6-06-03/04).** No superweapon EVA replay after a load; damage smoke rebuilt;
  trigger particle systems saved (section `PFX `). **Save format v25.**
- **Campaign shell (W6-06-07/08/11).** EndOfGame -> FinalMovie -> credits -> menu; PostScore /
  PreMapSelect movies; Results rows by side, map objects not Built, no places after a loss/tie;
  Back on Results / loading screens.
- **RA2.INI (W6-06-12)**, **Play Movies (W6-06-13)**, **SDL pin + fresh-checkout Android build
  (W6-06-14)**, **versionCode from VERSION + release-signing guard (W6-06-15)**, **About page +
  notices in the APK (W6-06-16)**.
- **Play scripts:** `lifecycle-w6`, `campaign-end-w6`, `save-w6-fx`, `about-w6`, `movies-w6`
  (needs `RA2_MOVIE_DIR` with a movie file).
- **Gates:** none moved (`--demo-saveload` prints `4cf40c63a14f07e7` with format v25).

## Touch play (wave-6 W6FIX-TOUCH, branch `w6fix-touch`)

Fixes `docs/gap/wave6/01-touch.md` (all items but W6-01-10, the Android
config-change autosave, which is the release agent's). RE note:
`docs/re/object-pick.md`. Player-facing gesture table: `docs/INSTALL.md` §4 and
`docs/PHONE-README.txt`.

- **Building pick (W6-01-01).** [RE: FUN_00691d00 -> FUN_006d98f0] the original
  picks the nearest object of the tactical's drawn-object list (`DAT_00b0be30`)
  within `dx^2 + 0.5 dy^2 < 200`; buildings are registered at their origin
  cell's centre (FUN_006d9250 over the BuildingClass vector `0xA8DAA8`), units
  at their draw point (FUN_006d9460); else the cell's first object. So a mouse
  picks a building by its foundation (plus that hotspot) -- `Input::pick_building`
  now does exactly that for the mouse. On touch [engine deviation] the drawn art
  (main frame + ActiveAnim / IdleAnim / SuperAnim slots, front-most first) is
  tested first, and a unit's generous 65x63 selection box only claims a tap that
  lands on its BODY (the original's hotspot ellipse) or on nothing better. Select,
  sell, repair, capture, spy, C4, attack, garrison, rally and primary all use the
  pick. Hit maps (`docs/gap/wave6/01-evidence-fix/`, one fresh skirmish per type,
  `hitmap_per_type.ps1`): art samples that select the building, before -> after:
  Prism Tower 32 % -> **100 %**, Tesla Coil 36 % -> **100 %**, Battle Lab 31 % ->
  **100 %**, Power Plant 58 -> 94 %, Air Force Command 62 -> 98 %, Barracks
  70 -> 100 %, Weather Device 75 -> 100 %, War Factory 79 -> 100 %, Pillbox
  86 -> 100 %. The Prism Tower's head selects the tower with GIs at its foot
  (`ctl-touch-w6`).
- **Sell / repair / superweapon confirm (W6-01-02 / -07).** On touch the first tap
  marks the building (foundation outline, banner "TAP AGAIN TO SELL <NAME>") or
  the SW target (the 3x3 / CellSpread area, "TAP THE SAME SPOT AGAIN TO FIRE");
  a second tap on the same building / cell commits through the normal Input path.
  The Weather Device's dome no longer sells the ConYard behind it.
- **Crowd rule (W6-01-06)** [engine deviation, touch; the original's rule is the
  cursor: an own unit under the pointer shows Select]: with two or more units
  selected, a tap on a unit of the selection is a move order to its cell (the army
  stays selected); a tap beside an own unit (inside its selection box, outside its
  body) is a ground tap; a tap on the body of an unselected own unit is a
  deliberate select; enemies keep the generous box. A deploy by tap needs a lone
  selection (or DPLY).
- **Double tap (W6-01-05)** selects every on-screen unit of the type and never
  deploys: a touch tap on the lone selected deployer (ACTION_SELF) waits out the
  500 ms double-tap window (`Input::touch_tick`) before it deploys; a two-finger
  tap resets the double-tap tracking.
- **Cameo hold (W6-01-03 / -04).** A hold shows the name + price ABOVE the finger
  and no longer suspends / abandons; with something in production a popup beside
  the cameo offers PAUSE / RESUME, CANCEL (one) and ALL (Shift + right click).
- **Radar tap (W6-01-08)** always centres the view on touch; a radar move order
  needs MOVE armed. The mouse keeps the original (selection -> move).
- **Command column (W6-01-09 / -11)**: 8 rows; page 1 adds **BOX** (sticky: a
  one-finger drag boxes at once, two fingers scroll) and **MORE**; page 2 has
  TYPE, HOME, EVT, NEXT, PREV, FOLW, teams 6-0 and view bookmarks V1-V3 (hold =
  SetView, tap = View; BACK returns). The hold is
  450 ms (was 600 ms; Android's long-press is 400-500 ms).
- **HUD text (W6-01-12)**: messages, objectives and tooltips draw 1.5x on a touch
  canvas. **Options button (W6-01-13)** joins the sidebar near-miss snap.
  **Edge swipes (W6-01-14)**: `RA2Activity` excludes the left-edge strip by the
  command column and the right-edge strip by the cameo strip from system gestures
  (Android 10+, 200 dp each); the cutout mode stays the default (the window sits
  beside a notch). **Perf overlay (W6-01-16)** moved right of the command column.
- **`--play`**: `set canvas WxH` (W6-01-15, the phone canvas, e.g. `974x548`);
  the edge-band trap is documented in `scripts/play/README.md`; probe keys
  `touch_aim`, `touch_aim_sw`, `cameo_menu`, `touch_box`, `touch_page2`,
  `selected_building_type`; targets `art:<ID>`, `cmenu:<BUTTON>`, the page-2
  `touch:<label>`s. New script `ctl-touch-w6` (on the 974x548 canvas). Unit tests
  `tests/test_input_touch_w6.cpp` and the BOX case in `tests/test_touch_gestures.cpp`.
- **Save format** unchanged (v24): every new state is UI-only and not saved.
- **Verification** (branch on `409e807`, scoped per the wave-6 rules): unit tests `W6-01*`,
  `touch:*`, `Input*` 56/56; `--play` one at a time: `ctl-touch-w6` (new), `ctl-touch-all01`,
  `ctl-all01`, `ctl-all02`, `ctl-sov01` (touch step adapted to W6-01-06), `ctl-sides`,
  `controls`, `keyboard`, `m-all01`, `m-sov01` all PASS; the per-type hit-map runs. Gates:
  `--demo-superweapon` prints its pin `e6969fed3e772bd3` (the only gate whose headers changed:
  a const accessor); no other gate constructs Input / TouchGestures / GameScene, so no re-pin.

## Wave-6 fix #3: skirmish AI (branch `w6fix-ai`)

Calibrated to gamemd.exe (RE: `docs/re/ai-base.md` §7; audit and status: `docs/gap/wave6/03-skirmish-ai.md` §6).

- **Victory / targets:** `Wall=` and `Insignificant=` structures never keep a house alive and are never
  AI quarries or superweapon targets (walls are overlays in the original).
- **Enemy:** anger table fed by damage (`FUN_00700da0` → `FUN_005045b0`), `AIHateDelays` nearest-house
  seed, arg-max Enemy, cleared on defeat/alliance. In a skirmish with no lobby teams every AI allies with
  every other AI against the human (`FUN_00501460`, `MPBattleMD.ini AlliesAllowed=yes`).
- **Economy / difficulty:** `AIVirtualPurifiers` (4,2,0) harvest bonus; `MultiplayerAICM` (400,0,0)
  starting money; raise-money recovery (`FUN_004fdb30`); `AIExtraRefineries` + defence placeholders
  spread through the plan (`FUN_005052d0`) with `AIPickWallDefensePercent` walls.
- **Superweapons:** the house AI fires only Nuke/Storm/paradrop-class types at the Enemy
  (`FUN_00509710`/`FUN_0050ca10`); the Iron Curtain and Chronosphere fire through the ct=5/6 AITriggers
  and script actions 55/56/57. The invented house-level IC/Chrono auto-fire and the invented
  civilian-garrison rule are gone.
- **Probes / scripts:** `--ai-soak --ruin-ai-at MIN` (wreck the AI economy); `scripts/play/sk-walls.play`,
  `scripts/play/sk-1v3.play` (new); `sk-a.play` guards its ConYard when the AI razed the refinery;
  `abil-ai-air.play` now asserts no AI garrison.
- **Save format v25 (v26 after merging with w6fix-rel).** No gate re-pin: `--demo-ai-base` `b61f283f7fd56db9`, `--demo-ai-team`
  `fdd60988623ca3ef`, `--demo-ai-trigger` `287a87c0aefdfec7`, `--demo-skirmish` `fde3f587e747f765`,
  `--demo-multihouse` `39412ae996cce8ae`, `--demo-superweapon` `e6969fed3e772bd3` all unchanged.

## Missions: target legality, difficulty, teams (wave-6 fix #2, branch `w6fix-missions`)

Calibration: docs/gap/wave6/02-missions.md (Ghidra on `gamemd.exe`, private copy `D:\ra2-re\w6fix-missions`).

- **Target legality (W6-02-07/08).** `World::legal_threat_unit/_building` mirror the Evaluate_Object rejects (`FUN_006F71D0`): LegalTarget=no, IsInPlayfield off, and in a campaign an Insignificant unit or an Insignificant building of a MultiplayPassive house. Used by the idle auto-acquire, the campaign team targeting and the AI quarry pickers; the AI SW picker scores 0 outside the usable area. IsInPlayfield is per object (`tg.in_playfield`), set at placement, gained on moving in, recomputed on ResizeView. ALL12's Moscow is no longer razed.
- **Difficulty (W6-02-01..05).** Every campaign house gets its handicap at scenario start (player + PlayerControl = slider, others 2 - slider); ROF reaches units and defence buildings; BuildTime is never applied; AI team clock seeded from frame 0; MultiplayerAICM start money (`FUN_006862D0`) for campaign houses.
- **Threat radius (W6-02-11).** GuardRange, else larger weapon range + 1 (+AirRangeBonus); area guard x2 (max 16); Sight is never read. ImmuneToPsionics blocks mind control (W6-02-12).
- **Teams/triggers.** Team tags on recruits (W6-02-14), built units tracked by rules id (W6-02-15), full-strength team start + Reinforce refill (W6-02-16), recruit filters (W6-02-17), Patrol = attack-move (W6-02-18).
- **Play:** `m-trn02-real` (Boot Camp 2) WON by real orders; SOV01 / SOV07 / SOV12 / ALL12-opening scripts adapted; ALL05, ALL06, ALL10, ALL11, SOV09 scripts now fail (bisected causes in 02-missions.md).
- **Save format v29.** Gates re-pinned: combat, tank, naval, saveload, campaign (table); every other gate prints its pin. Unit suite 878/878.
## Networking L1: the command layer (branch `net-l1`)

Every player action now reaches the World as a **Command** (`engine/command.{hpp,cpp}`),
modelled on the original game's `EventClass` (111 bytes: Type, HouseIndex, execution Frame,
payload; `docs/gap/net/01-original-protocol.md` §3). The event numbering is the original's
(`EVENT_EMPTY 0x00` .. `EVENT_ABANDON_ALL 0x2E`) and the fields are the original payload
fields, so the wire codec can map a Command to an `EventClass` record 1:1.

- **Queue and scheduling.** The UI builds a Command; `CommandScheduler` stamps the issuing
  house, the execution frame (`now + delay`, delay 0 in single player) and a per-house
  sequence; `CommandQueue` hands the due commands to `apply_command()` at the **start** of
  each logic tick, ordered by (frame, issuing house, sequence) [RE: `Queue_AI` ->
  `Execute_DoList FUN_0064BDF0` before the frame's logic]. In single player the frame's
  late commands (deploy / repair / sell / place / superweapon clicks, issued after the tick
  loop) are flushed right after the loop: the same World mutation order as applying them at
  the next tick's start, since nothing touches the World in between. A lockstep session
  must not flush early.
- **Selection is UI state.** `Unit::selected` is gone. The local player's selection lives in
  `Input` (`engine/selection.hpp`); it is never saved or hashed. Orders carry explicit,
  ascending unit ids (the `Whom` list). The World's `*_selected` orders became `group_*`
  orders taking `(issuer house, ids)`; ownership is checked at apply time with
  `World::can_command(issuer, house)` (own house, plus the PlayerControl houses for the
  campaign player). `Selection::prune` drops units the sim used to deselect (boarded,
  garrisoned, consumed, erased, Spawned=yes). Save format **v33**.
- **Campaign event 33 "Selected by player"** reads the selection through a
  `SELECT_NOTIFY` command (clone extension `0x40`, never sent to an original peer), so a
  replay sees the same selection.
- **Production / placement** apply through `CommandHost` (`game_scene_commands.inc`
  `cmd_produce` / `cmd_suspend` / `cmd_abandon` / `cmd_place`) onto L2's World economy
  (`econ_produce` / `econ_suspend` / `econ_abandon` / `econ_place`). The cameo click only
  decides which event to send (the original `StripClass` action `FUN_006AA390`). Before each
  apply the scene sets `World::set_order_issuer(cmd.house)` (L2), so every ownership check
  answers for the command's house.
- **Victory cheer**: L2 made it a sim rule (`step_house_outcomes`); nothing per frame.
- **Wire bridge** (`engine/net/command_bridge.{hpp,cpp}`): `to_net` / `from_net` (a Whom run
  goes out as one EventClass record per object and is re-grouped on receipt, so a group move
  computes the same formation on every peer; the Chronosphere's two picks pair up; the SW index
  is the original's binary index) and the `to_event` / `from_event` / `to_events` entry points
  `engine/net/event_map.hpp` proposed.

| Player action | Command (original event) | Payload |
|---|---|---|
| Move (click, force-move Alt, radar, touch tap) | `MEGAMISSION 0x04`, Mission `Move` (2) | Whom = selection, Destination = cell |
| Attack unit / building / ground (Ctrl force-fire) | `MEGAMISSION`, `Attack` (1) | Target = unit / building / cell |
| Harvest ore | `MEGAMISSION`, `Harvest` (10) | Whom = the miners, Target = cell (the rest of a mixed selection: a `Move`) |
| Dock at refinery | `MEGAMISSION`, `Enter` (7) | Target = own refinery |
| Enter transport / garrison a building | `MEGAMISSION`, `Enter` (7) | Target = transport / building |
| Engineer capture / repair / defuse | `MEGAMISSION`, `Capture` (8) | Whom = the engineer, Target = building / bombed unit |
| Spy infiltrate | `MEGAMISSION`, `Sabotage` (17) | Whom = the spy, Target = building |
| Guard (G) | `MEGAMISSION`, `AreaGuard` (11) | Whom = selection |
| Stop (S) | `IDLE 0x06` | Whom = selection |
| Scatter (X) | `SCATTER 0x07` | Whom = selection |
| Deploy / unload (D, click the selected unit, touch) | `DEPLOY 0x09` | Whom = garrison building + selection |
| Place building | `PLACE 0x0B` | RTTI BuildingType, HeapID = roster index, cell |
| Start / resume / queue | `PRODUCE 0x0E` | RTTI, HeapID, IsNaval |
| Hold | `SUSPEND 0x0F` | same |
| Cancel one | `ABANDON 0x10` | same |
| Cancel all (Shift) | `ABANDON_ALL 0x2E` | same |
| Primary factory | `PRIMARY 0x11` | Whom = factory |
| Rally point | `ARCHIVE 0x1E` | Whom = factory, Target = cell |
| Superweapon | `SPECIAL_PLACE 0x12` | SW type, cell (+ the Chronosphere's destination, sent by the codec as the ChronoWarp `SPECIAL_PLACE`) |
| Repair toggle | `REPAIR 0x15` | Whom = building |
| Sell | `SELL 0x16` (walls by cell: `SELLCELL 0x17`) | Whom = building |
| Power toggle | `POWERON 0x01` / `POWEROFF 0x02` | Whom = building |
| AllToCheer | `ALLCHEER 0x2D` | -- |
| Alliance toggle | `ALLY 0x03` | HouseID (apply implemented; the hotkey stays network-only, as in the original) |

**Not routed** (by design or out of scope): selection, control groups, camera, sidebar tab
and scrolling (UI state); the options / pause menu, briefing, in-game movies and game speed
(local; `OPTIONS` / `GAMESPEED` events are for the lockstep session); Abort / Restart
Mission (scene change; `EXIT` / `REMOVEPLAYER` belong to the session layer); `--play`
`cheat ...` commands and the `camp_*` campaign cheats (debug, mutate the World directly, so a
script that uses them does not replay); `PageUser`, `PlaceBeacon`, taunts and chat (not
implemented; network-only in the original).

**Record / replay (R-184 reworked).** `--record <f>` (or `RA2_RECORD`) writes the game's
config (`SkirmishConfig`), crate seed and canvas, then every applied command with its tick
(`C ...` lines, `command_to_line`), then `END <tick> <state hash>`. `--replay <f>` rebuilds the
game from that config (no menus, no input), re-applies the log tick by tick and prints
`[replay] end tick=N state_hash=H recorded=R match=yes|no`. The state hash covers the World
(`save_world`), the scene economy (wallet, queues, backlog / hold, building states) and the
campaign trigger runtime; camera, sidebar tab and the credits display are left out. A game
resumed from a save is not recorded. The raw input recorder stays as `--record-input`
(`RA2REC` files still replay with `--replay`).

**Replay proof** (`--play <script> --record f.cmd`, then `--replay f.cmd`; release build, merged
with master `018abfb`): skirmish `sk-1v3` (9624 ticks) `47aa5884637bf89a` = `47aa5884637bf89a`;
campaign `m-sov01-real` (9033 ticks, 78 commands) `6758d38967a57e8c`, `m-all03-real` (20958 ticks)
`9686935520863c85`, `m-all04-real`, `m-all05-real` (299 commands), `m-sov02-real`, `m-all01`,
`m-sov01` and every `ctl-*` script without cheats, `controls`, `keyboard`: `match=yes`. `sk-a`,
`ctl-touch-w6` and `mh-2human` use `cheat` lines (and `mh-2human` a `set humans` debug switch),
which mutate the World outside the command log, so their replays differ by design.

**Verification (net-l1).** Gates: 26 of 27 print their pins; `--demo-saveload` re-pinned
(v33, table above, A/B-proven). Scoped unit tests (`test_command*`, `test_net*`, `test_input*`,
`test_world.cpp`, `test_saveload*`, `test_house_econ*`, `test_mp*`): all pass. `--play`, one
at a time: `ctl-all01`, `ctl-all02`, `ctl-sides`, `ctl-sov01`, `ctl-touch-all01`, `ctl-touch-w6`,
`controls`, `keyboard`, `m-all01`, `m-sov01`, `sk-a`, `sk-1v3`, `mh-2human`, and the real-order
mission wins `m-sov01-real`, `m-all03-real`, `m-all04-real`, `m-sov02-real`, `m-all05-real`:
18/18 PASS.

## Networking L2: per-house economy and the local player (branch `net-l2`)

The human economy moved from `GameScene` into the `World`
(`engine/world_econ.cpp`). Every house that keeps money in the sim has one
`HouseEcon` (wallet, the six FactoryClass queues, the queued-behind backlog,
the hold bits); AI houses use the same record (their wallet and queues left
`HouseAiRec`). `World::step` ends with `step_player_economies` (production,
income drain, unit exit, queued refill for every human house) and
`step_house_outcomes` (per-house outcome, win-time cheer). Asset-derived
rules (`PlayerEconRules`: unit roster, factory types, placement extras,
buildable mask, BuildLimit, rules scalars) are installed by the host after
construction and after every load; they are not saved. Save format v32.

Command API (the original's events, validated when they execute
[RE: EventClass::Execute FUN_004C6BD0]): `econ_produce` (PRODUCE 0x0E,
FUN_004FA170), `econ_suspend` (SUSPEND 0x0F, FUN_004FA730), `econ_abandon`
(ABANDON 0x10 / ABANDON_ALL 0x2E, FUN_004FA830), `econ_place` (PLACE 0x0B,
FUN_004FAF00), `econ_set_primary`, `econ_set_rally`, `econ_power`, plus the
existing `toggle_repair(b, house)` / `begin_sell(b, house)`. Read side:
`econ(h)`, `credits(h)`, `econ_queued_count`, `econ_held`,
`econ_placement`, `econ_can_build`, `house_outcome(h)`,
`take_econ_launches()`. `controllable()` answers for
`set_order_issuer(house)`; `can_command(issuer, h)` is the explicit form.

The scene plays `local_house_` (presentation reads `g_local_house`, input
reads `World::order_issuer()`). Debug switch for several human houses:
`--humans 0,1 --local-house 1` on the command line, or `set humans 0,1` /
`set local_house 1` in a `--play` script. `scripts/play/mh-2human.play`
drives house 1 through the sidebar while house 0 builds through World
commands (`cheat econ_produce H TYPE`, `cheat econ_place H`,
`cheat deploy_house H`), then the houses fight.

Fix-black (network joiner saw a black map): the terrain pass
(`iso_renderer.cpp draw_terrain`) skipped every cell HOUSE 0 had not
explored, so any client whose local house is not 0 drew a black playfield
with only its objects on top. `draw_terrain` now takes the viewing house
(`g_local_house`), as do the remaining presentation `object_visible_at`
reads (unit/building/FX/radar/sound reveal) and the SW sidebar slot.
Probes `terrain_drawn`, `local_explored`, `local_explored_on_screen`,
`cam_own_dist`, `cam_cell_x/y`, `own_cx/cy`; `own_on_screen` now counts the
local house's objects through the local house's sight. Asserted by
`mh-2human.play`, `mh-local1-view.play` and the four `net-*-host/join.play`
(`@fix-black`); evidence in `docs/gap/net/evidence/`.

Behaviour notes (deliberate, calibrated to the original): one Balance per
house -- repairs and the Service Depot charge the wallet directly (the old
scene's repair wallet bridge is gone); the unit queue holds up to
`MaximumQueuedObjects` waiting copies (was one fewer); ABANDON of the current
unit starts the next queued one at once (FUN_004CA4C0); a held building may
be replaced by PRODUCE of another building (FactoryClass::Set abandons it);
campaign `Credits` trigger conditions read the real wallet (was the
player's start credits / the AI income pool).

## Networking: original-format transport and lockstep (agent T, branch `net-t`)

`engine/net/` speaks gamemd's own multiplayer wire format, so a future bit-exact simulation can play
against real `gamemd` peers without new network code. The full format, with addresses, is in
[docs/re/net-protocol.md](docs/re/net-protocol.md).

* **Transport:** non-blocking IPv4 UDP (Winsock / BSD, works on the Android NDK). Every datagram carries
  the original rotate-add checksum (`0x7B0DD0`). An optional CnCNet tunnel header (V2 or V3) is added
  for relayed play.
* **Private channel 0xAA04:** a byte-exact port of ConnectionClass. It has DATA_ACK, DATA_NOACK and ACK
  packets, retransmit, timeouts, and exactly-once in-order delivery (`0x48BF50`, `0x48C230`, `0x48C4B0`).
* **Global channel 0x1236:** a byte-exact port of the lobby, chat and kick-vote channel, deduped on
  (address, id).
* **EventClass codec:** all 47 YR event types plus the spawner's 0x30. It covers FRAMEINFO and
  FRAMESYNC, and the MEGAMISSION run-length coding with both unit-list caches, quirks included. It is a
  port of `0x64B0D0` / `0x64B840`.
* **Lockstep scheduler:** a non-blocking port of `Queue_AI_Multiplayer`. It covers:
  * frame stamping on FrameSendRate boundaries;
  * Wait_For_Players with the original timeouts;
  * the master's TIMING;
  * the CnCNet "Protocol 0" latency levels;
  * per-frame CRC exchange and out-of-sync detection;
  * REMOVEPLAYER on timeout, and EXIT.

  The frame CRC is a caller hook.
* **Android:** the manifest declares INTERNET, ACCESS_NETWORK_STATE, ACCESS_WIFI_STATE and
  CHANGE_WIFI_MULTICAST_STATE.

Tests:

```
ra2assets_tests --test-case="net*"                    # 32 cases, ~1 s (golden bytes, loopback peers)
ra2netpeer --selftest --frames 600                    # two peers, real 127.0.0.1 sockets
pwsh scripts/net_two_process.ps1 -Exe <build>\ra2netpeer.exe   # two OS processes, LAN
```

## Networking: playing a match (NET-PLAY, branch `net-play`)

Network and Internet games are real now: the lobby's **Start Game** builds the match and every
machine runs the same simulation in lockstep, the way the original does (one EventClass stream,
the frame scheduler, the per-frame CRC compare). Pieces: `engine/net/net_game.*` (the driver),
`engine/net_play.inc` + `engine/game_scene_net.inc` (the game side), `engine/mp/mp_net.*` (the
shared endpoint), the hand-off in `engine/mp_pages.inc` (`mp_start_session`).

**How to play**

* **LAN (same Wi-Fi / network):** *Network* -> *Host Game* on one machine; on the others
  *Network* -> pick the game in the list (or *Join by IP* with the address the host's lobby
  shows, e.g. `192.168.1.20:50020`). Pick country / colour / team / start, the joiners press
  *Ready*, the host presses *Start Game*.
* **Internet:** run your own `ra2lobbyd` ([docs/SERVER.md](docs/SERVER.md)); everyone enters its
  address under *Internet* -> *Connect*. The host creates a room (public, or private with a
  room code), the others join from the list or with the code. Game traffic goes through the
  server's CnCNet-V3 tunnel, so no port forwarding is needed.
* **In the game:** Enter (or the *Chat* button, touch) opens the chat line, Enter / *Send*
  sends it to everyone (the original chat packet 0x0C on the global channel); lines show in
  the message list in the sender's colour. The options menu does **not** pause a network game
  (save / load are disabled). *Abort* leaves: an EXIT is scheduled like any order, the game
  keeps running until it executes on every machine at the same frame, then you go to the menu
  and the others see "<name> has left the game.".
* **Waiting / reconnect:** when a player's packets stop, the game waits ("Waiting for
  players..."). After the original's dialog timeout a dialog names the player we wait for with
  a countdown and offers *Drop player* / *Quit game*; at the original's total timeout (LAN 2400
  ticks = 38 s, Internet 7200 ticks) the slowest player is dropped automatically. A dropped or
  leaving player's house is **defeated** (everything it owns blows up) -- or, when the host
  ticked *AI takes over* (options string `ait=1`), a Normal AI takes the house over with its
  money, queues and units. Frame 0 waits up to 3 minutes for slow loaders (phones).
* **Android:** sending the app to the background tells the others (AWAY, clone global packet
  0x41): their game shows "<name> paused the game -- waiting N s" for 90 s instead of opening
  the drop dialog; coming back within that time sends BACK and play simply continues. A
  network game is not autosaved. **Not done yet:** rejoining after the app was killed or the
  network changed (reconnect by state snapshot, 05-transport-infra.md §7.2 step 4 / §7.3).
* **Windows vs Android:** the simulation is not yet bit-identical across CPU / maths libraries
  (that is net L0: deterministic maths). Until L0 is merged a match must be **same platform
  only**: the join handshake carries a platform tag (1 Windows, 2 Android arm64, 3 Android
  x86_64) and the host refuses a mixed-platform joiner ("The host plays on another platform")
  unless the lobby option *Cross-platform (experimental)* (`xp=1`) is on.

**What runs where.** The lobby and the match share ONE endpoint (`MpNet`: engine/net's
`UdpDatagramIO` + `NetManager`; the lobby is the 0x1236 global channel, the match adds one
private channel per remote human on the same socket / tunnel id). `GO` now carries the peer
table, so every machine reaches every other human (the match is a full mesh, the lobby a star).
`mp_start_session` builds the World from `session_to_skirmish` with the session seed (crates),
the lobby's resolved start waypoints and the local house; the GameScene then:

* sends local orders through `NetGame::issue` (EventClass records, `Frame = now + MaxAhead`);
  they come back from `Execute_DoList` with everyone else's on their frame and are applied at
  that tick's start in the original's order -- local orders never short-cut the queue;
* ticks at a fixed **15 Hz** (the lobby speed is not used; the TIMING fps cap is GameSpeed 4)
  and only when `NetGame::step` says Run; ticks are never dropped (a backlog runs up to 4 per
  frame). Start values from the original (net-protocol.md §8): LAN FrameSendRate 5 /
  MaxAhead 10, Internet 5 / 15; the host's TIMING events resize both from the measured round
  trip (RA2.INI `[Net] FrameSendRate=` / `MaxAhead=` override the start values). At 15 Hz
  MaxAhead 10 means an order lands ~0.7 s after the click (the original at speed 4 behaves
  the same);
* drains the World's per-frame presentation event queues (fire / ability / garrison /
  projectile) at tick end, because those queues are part of the hashed save bytes.

**Sync check.** The FRAMEINFO CRC is a full-state hash: FNV-1a over `save_world(world)` (every
World member, bit-exact doubles) plus the scene's FreeUnit tracker, folded to 32 bits. Measured
on the desktop (Release, 2 players, frames 0-3000 of the test match): **3.6 ms average, 6.1 ms
max** per hash. So it runs every **N = 15 frames** (once a second; RA2.INI `[Net] HashEvery=`
overrides) and the last value is fed in between -- every machine feeds identical CRCs each
frame, a divergence is caught at the first compared frame after the next hash (in the test,
diverged at 600 -> compared frame 615, both machines stopped at 630, i.e. within 2 s; the two
TXT files named `econ_`, the member the test changed). The original's 256-frame grace after the start
applies; a different frame-0 state (different rules / map / roster) is "Scenarios do not match"
at once. On a mismatch the game shows the original's TXT_OUT_OF_SYNC message, writes
`Saved Games/DESYNC_F<frame>_H<house>.SAV` (this machine's full save) and `.TXT` (frames, both
CRCs, the last 256 fed CRCs and an FNV of **every World member** -- diff the two machines' TXT
files and the differing member is named), and ends the match.

Two determinism fixes this needed (found by the frame-0 check): the crate unit pool was built
from the local country's roster (now the side-independent AI roster in a match), and a remote
player's PRODUCE resolved its queue from the local roster (now the issuing house's roster).

**Tests**

```
ra2assets_tests --test-case="net play*,mp*"          # two real Worlds in lockstep over a lossy loopback:
                                                     # 2000 frames identical every frame; desync at 400 caught
                                                     # (frame 405, detected 420); EXIT at a common frame; chat;
                                                     # resign_house
pwsh scripts/play/net-match.ps1                      # two processes, LAN lobby, 3000-frame match
                                                     # (+ chat and an Android-style AWAY / BACK from the joiner)
pwsh scripts/play/net-match.ps1 -Online              # same through a local ra2lobbyd + the V3 tunnel
pwsh scripts/play/net-match.ps1 -DesyncAt 600        # joiner diverges at 600: both stop out of sync + dumps
pwsh scripts/play/mp-lan-loopback.ps1                # lobby agreement (now ends in the running match)
pwsh scripts/play/mp-online-loopback.ps1
```

## Networking L0: a deterministic, cross-platform simulation (branch `net-l0`)

Lockstep needs every peer to compute the same World from the same commands, on
Windows (MinGW gcc, UCRT) and Android (NDK clang, bionic; x86_64 and arm64).
L0 removes the platform-dependent parts of the simulation and adds a gate that
proves it (`docs/gap/net/03-sim-bitexact.md`, `docs/gap/net/04-engine-lockstep.md` N4-01/07/08/11/12/18).

* **No libm in the simulation.** `engine/orig_math.{hpp,cpp}` ports gamemd's
  FastMath routines and `_ftol` instruction by instruction (sqrt `0x4CAB60`,
  sin/cos/tan, asin/acos, atan/atan2, `_ftol 0x7C4D00`) with their tables dumped
  once into `engine/gen/orig_math_tables.inc`. `tests/test_orig_math.cpp` checks
  each routine bit for bit against the original machine code run in an emulator
  and fails the build's test run if a sim source calls a banned libm function.
* **Every sim double->int conversion is `om::ftol`.** A C++ cast is undefined out
  of range and differs between x86 and arm64; `_ftol` wraps mod 2^32. A deleted
  integral overload made the compiler find every floating cast (696 candidates,
  113 real ones in 45 sim files).
* **Timers count whole frames.** Rearm, Iron Curtain, Chronosphere and surfacing
  timers, the AI TeamDelays / AutocreateTime / repair clocks and the ore growth /
  spread clocks were running sums of 1/15 s compared with k/15, which expire a
  frame late (247 of 300 rearm lengths). They now count frames (save v34). The
  AI base build-pass clock stays a seconds sum on purpose: it is IEEE-exact on
  every platform, and counting frames re-timed the AI build order and wedged
  `--demo-ai-team`; matching the original's period is left to the AI calibration
  work (simdiff build-order rung).
* **One session seed.** `SkirmishConfig::session_seed` (saved) is the network
  lobby's session seed (`defer_net_game_scene` sets it for both peers); `World::set_session_seed` derives every sim stream from it
  (projectiles, combat, ore, bridges, each AI house's base / team RNG, the
  campaign trigger RNG) and seeds the crate `GameRandom` (`Scenario->Random`,
  [RE: InitRandom `0x52F960`]) with the seed itself. Seed 0, used by every
  single-player game, keeps the historical seeds, so the `--play` scripts are
  unaffected. The skirmish crate seed is no longer the wall clock.
  Checked with the net-h harness: `--ai-soak --seed 0x01234567 --diff-dump`
  frame 0 shows the generator state of a freshly seeded `RandomClass`
  (`rng 0 103 3D76A7D2`; the original is at 83/186 by frame 0 because of its
  load-time draws, not modelled yet).
* **Path memo keyed by content.** The `thread_local` A* memo keyed the leak /
  level / deck grids by address; after a resync `load_world` or a reused address
  it could serve a path built on other terrain. It now keeps byte copies of
  every input grid.
* **No dropped ticks.** The logic loop carries a backlog over kMaxLogicSteps to
  the next frames instead of zeroing it (a single frame counts at most 1 s).
* **A real bug the gate found.** `CampaignRuntime`'s poll loops held `Unit&` /
  `Track&` across trigger raises whose actions create objects, so the vectors
  reallocated under live references. Android read `hp=0` from the freed block
  at ALL01 frame 410 and raised an extra AttackedByAnybody; Windows read stale
  bytes. `CampaignRuntime::frame` now reserves headroom
  (`World::reserve_object_headroom`), capacity only.

### Cross-platform gate

`-DRA2_ANDROID_CLI=ON` with the NDK toolchain builds `ra2engine` as a plain
Android executable (static SDL, no activity) that runs the desktop command line
under SDL's dummy drivers. `tools/xplat_gate.ps1` runs every `--demo-*` gate on
both and compares all hashes and witness counts, and with `-SoakMinutes N` a
seeded `--ai-soak --ai-vs-ai --hash-every 15`: fnv1a(`save_world`) every 15
frames -- the basis of the match sync hash (`GameScene::net_state_hash`) -- plus
the final `state_hash`.

```
cmake -S . -B build-android-cli -G Ninja -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_TOOLCHAIN_FILE=%NDK%/build/cmake/android.toolchain.cmake ^
      -DANDROID_ABI=x86_64 -DANDROID_PLATFORM=android-24 -DRA2_ANDROID_CLI=ON
cmake --build build-android-cli --target ra2engine
adb shell mkdir -p /data/local/tmp/l0/ra2          # then adb push the game files there once
pwsh tools/xplat_gate.ps1 -Exe build\ra2engine.exe -AndroidExe build-android-cli\ra2engine -Out <dir> -SoakMinutes 30
```

Result (Windows MinGW vs Android 11 x86_64 emulator): **27 of 27 gates match**,
and a 30-minute seeded AI-vs-AI soak matches at all 1800 sync points and in the
final state (`state_hash=8660aff5a906e564`, 2.8 MB of World). Command logs are
portable too: `--record` on Windows, `--replay` on the emulator gives the same
state hash (`sk-1v3` 9004 ticks `048443cfc7b9168d`, `m-sov01-real` 26990
ticks / 174 commands `b67d6b754d0a2e97`). An arm64 phone run is still to do
(N4-18): the same executable built with `-DANDROID_ABI=arm64-v8a`.

**Gates.** Re-pinned with causes in the table: combat, tank, roster,
projectile, veterancy, campaign (FastMath and the frame timers, A/B against a
libm-only build), ai-team and ai-trigger (the frame TeamDelays clock),
saveload (v34). The other 18 print their pins.

**Mission scripts.** The original math and frame timers change combat
outcomes, and three recorded real-order mission wins no longer won:
`m-sov01-real` (already with the FastMath commit alone), `m-all04-real` (the
timers) and `m-sov09-real`. Re-recorded against the new combat (generators
only, no engine regression found; docs/gap/wave4/05-mission-sequences.md "MR1
status"): `m-sov01-real` WON f6090 (Normal), `m-all04-real` WON f9030
(Normal), `m-sov09-real` WON f13275 (Easy); each `--record`/`--replay` pair
ends with the same state hash. `m-all06-real`, failing on master, wins again.
`m-all08` (failing since R-212 `40a3296`, before the networking work) passes
again with the player calling Tanya back. Failing on master too:
`m-all11-real`, `m-sov07-real`, `m-sov12-real`. The two-process

timers) and `m-sov09-real`. They need re-recording against the new combat.
`m-all06-real`, failing on master, wins again. Failing on master too:
`m-all08`, `m-all11-real`, `m-sov07-real`, `m-sov12-real`. **MR2** (branch `mr2`): `m-sov07-real` and `m-sov12-real` were broken by
net-l2 (bisected `c39089c` PASS / `868b18d` FAIL): `PlayerEconRules::buildup_total`
held ints, truncating the Buildup seconds of every human-placed structure;
fixed (`std::vector<double>`, unit test), both WIN again unchanged.
`m-all11-real` dodges BadGuy2's first nuke and builds two more war factories
(generator; WON f14933, Easy). The replay state hash no longer mixes the UI
field `place_cat` (ALL11's replay reported `match=no` with an identical World);
`RA2_REC_CHECK=N` / `RA2_REC_DUMP=T` find the first diverging tick of a replay.
Results and hashes: docs/gap/wave4/05-mission-sequences.md (MR2). The two-process
pairs pass through their drivers: `net-match.ps1` (LAN and `-Online`; with
`-DesyncAt 600` both peers stop out of sync with dumps), `mp-lan-loopback.ps1`,
`mp-online-loopback.ps1`.
