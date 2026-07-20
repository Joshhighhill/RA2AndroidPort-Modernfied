# ra2assets + ra2engine

A portable C++17 library that decodes Red Alert 2 / Yuri's Revenge game data
(`ra2assets`: MIX archives, PAL palettes, SHP sprites, VXL/HVA voxel models
(vehicle meshes + animation transforms, decode only), LZO/LCW map packs, INI,
rules, `.map` cell grids, TMP theater tiles, CSF string tables, BAG/IDX audio
decoded from Westwood IMA-ADPCM to PCM), plus `ra2engine` — an SDL2
front-end that renders a real stock map with real theater terrain and lets you
select and command a squad of units (RA2-classic controls: left-click / box /
Shift to select, right-click to group-move) with A\* pathfinding,
grid-occupancy collision avoidance, and walk animation — and fight: Allied
GIs vs Soviet Conscripts with house-colored palettes, HP bars, fire/death
animations, and a damage model (Damage × Verses, ROF, range, guard behavior)
recovered from the original `gamemd.exe` via Ghidra and driven by your real
`rulesmd.ini` (see `docs/re/combat.md`). Standing/walking/firing/dying frames
and all 8 facings are resolved from your real `artmd.ini` sequence data
(`ra2assets::Art`, see `docs/re/infantry-anim.md`), and group attack orders
fan attackers out to individual firing positions instead of queuing them
single-file. The fight is audible, too: weapon-fire sound effects, unit voice
acknowledgements (select/move/attack barks), death screams, and the EVA
"Unit lost" announcement, all driven by your real `soundmd.ini`/`evamd.ini`
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
so a vehicle on a ramp still renders level); no crush and no debris;
projectiles are instant-hit with single-target damage — vanilla shells fly
102 leptons/frame (not modeled; travel time is skipped) but `[AP]
CellSpread=.3` only ever reaches 76 leptons, in practice just the direct
target anyway, so the single-target damage half of this is exact even
though the flight-time half is a real simplification; the engine keeps its
already-established absolute speed scale of `0.75·Speed` cells/sec rather
than vanilla's `0.15·Speed` (the tank:GI speed ratio is vanilla-faithful
either way — a whole-engine time-base recalibration to the vanilla absolute
scale is a recorded future candidate, not done piecemeal here); the
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
the pre-built Soviet base; units never auto-acquire buildings as combat
targets (explicit attack orders only). Plus, from the construction-phase
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
feeds nothing back. The interactive enemy Soviet house now runs all three AI
layers at once: the AI-1 base brain, AI-2 autocreate teams, and AI-3's
adaptive trigger mix, all through `make_ai_team_config(..., enable_triggers)`.
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
- **No ground AA / air-to-air** — ground units never auto-acquire or target
  the aircraft (`valid_target`'s `!v.is_aircraft` clause); the aircraft
  attacks only ground targets. Ground-AA-vs-air is a documented
  carry-forward [`aircraft.md` §4; spec §Out].
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
  `Speed`) converted via the engine's existing `kCellsPerSecPerSpeed`
  vehicle-speed scale, and `climb_rate`/`descend_rate` are the recovered
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
static radar frame. All render/minimap work is cosmetic and folds into no gate
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
then per-slot spawns a base (`spawn_base`) + `unit_count` starting units + an MCV,
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
Movies, Credits) drawn as greyed stubs; Skirmish opens the setup screen (per-slot
side/color/AI-difficulty, cash/unit-count/tech sliders, alliance teams); Start
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
again at this finalize, and `--demo-skirmish` pins `skirmish_hash=e9ad3f1fd88495f9`.
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

## Building

Prerequisites: CMake 3.20+, MinGW-w64 gcc, Ninja, Python 3.13. The engine also
pulls SDL2 via CMake `FetchContent` (pinned `release-2.30.9`), built from source
on first configure.

Configure and build with CMake + Ninja:
```bash
cmake -S . -B build -G Ninja
cmake --build build
```

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

Interactive controls: left-click a unit to select it (green marker); left-drag
a box to select many; Shift+left-click to add/remove one; double-click a unit
to select all of its type on screen; right-click ground to move the selection
(units fan out to distinct cells and route around each other); **right-click a
hostile Conscript to attack it** — units move into weapon range, hold, and
fire; idle units auto-acquire hostiles in range and return fire; pan with the
arrow keys or by pushing the pointer to a screen edge; Esc/close quits.

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

`ra2engine.exe` also accepts `--no-audio` (skips audio init entirely — same
silent fallback as a failed device open, just without touching the mix chain
at all) alongside the interactive run.

### Determinism gates (permanent)

These seventeen headless demos are byte-identity regression gates: any change to
their printed output signals a defect in whatever change caused it, not an
intentional update (content changes that legitimately alter output, e.g. a
new roster entry, must be called out explicitly — see the Economy divergences
above for `--demo-base`'s `roster=10`, and note `--demo-base`'s roster grows
again with each subsequent roster addition — `roster=20` as of the Engineer/Spy
Capture phase (the `CAOILD` oil derrick; `roster=19` was the CB-5 Battle Lab
`GATECH`) — with the gated hash unaffected each time). The first SIXTEEN are the
legacy gates re-verified unchanged by every subsequent phase; `--demo-skirmish`
is the new Skirmish Shell + Main Menu gate.

| Demo | Gate | Key witnesses |
|------|------|----------------|
| `--demo-combat` | `deterministic=yes logs=78` | `shots=73 deaths=5` |
| `--demo-tank` | `deterministic=yes logs=144` | `shots=135 deaths=9 hit90_rhino=yes hit22_infantry=yes` |
| `--demo-base` | `hash=5273b5e71ca009e1` | `roster=20` (content-only growth, hash unaffected), GAPOWR `frames=504 step_ticks=9`, credits `9200→8700`, `GAWEAP blocked (PROC unmet)` |
| `--demo-econ` | `econ_hash=7fed96cb98b68017` | GAREFN `frames=1260 step_ticks=23 charge=600`; CMIN unload `+500` credits; enemy `income=1000`; `GAWEAP can_start=yes (PROC met)` |
| `--demo-prod` | `prod_hash=f0f4e1207ecd713c` | `deterministic=yes`; MTNK frames/charge, exit cell, concurrent GI, 2nd-factory primary routing |
| `--demo-ai-base` | `ai_base_hash=cd1f7c6a29933f0a` | `deterministic=yes`; lone Soviet ConYard builds out power → barracks → refinery → war factory → radar → defenses (the recovered topological prereq-sort order, `docs/re/ai-base.md` §2/§2a) at fixed difficulty/seed/frames |
| `--demo-ai-team` | `ai_team_hash=312e92ed862a5e74` | `deterministic=yes`; a Soviet AI house with a pre-built base autocreates a team from the real aimd.ini, recruits/produces its TaskForce, gathers near the base, and marches the squad at the player under the ScriptType interpreter (`docs/re/ai-team.md`) |
| `--demo-ai-trigger` | `ai_trigger_hash=8334a6ca1531cabc` | `deterministic=yes`; a Soviet AI house (Normal difficulty) with an intrinsically-eligible ct=4 HouseCredits exemplar (`0D2C41EC-G`, weight band `[10..70]`, runtime-scanned, not hardcoded) fires by the recovered cumulative-sum draw over `round(current weight)`, creates its Team1 through the AI-2 path, and the created teams' script-completion disband outcomes adjust every trigger sharing that Team1 by the clamped `+20`/`−50` track-record rule (witnessed weight movement `40→60`, `50→70`, `60→70` across the run) (`docs/re/ai-trigger.md`) |
| `--demo-transport` | `transport_hash=fe500f946e913645` | `deterministic=yes`; variant=HTK (Passengers=5 SizeLimit=2 on this install; FV fallback documented if art ever regresses); 5 boards to the exact-fill COUNT boundary + 1 refusal at full; carry with zero carried-unit draw/occupancy folded into the hash; 5 LIFO ejects one-per-frame onto facing±90° scatter cells (`eject_cell`); wedge guards armed (`docs/re/transport-cargo.md`) |
| `--demo-aircraft` | `aircraft_hash=a69642564cf64773` | `deterministic=yes`; variant=ZEP (Kirov, `JumpjetHeight=750`, `ROT=10`, `JumpjetSpeed=5` on this install; ORCA/BEAG fallback documented if art regresses); takeoff to alt=750 with the ground cell released (climb/travel overlap), straight-line cruise at altitude with zero occupancy held (folded into the hash), one `BlimpHE CellSpread=2` bomb splash killing BOTH adjacent ground targets (2 kills), hold-at-cruise resume; raised voxel draw + ground shadow; wedge guards armed (`docs/re/aircraft.md`) |
| `--demo-naval` | `naval_hash=6aee2c82bc024e86` | `deterministic=yes`; variant=HYD (Sea Scorpion, `Speed=8`, `ROT=6`, `FlakTrackGun` Damage=25/ROF=25/Range=5, Warhead `FlakTWH` CellSpread=1.0 on this install; DRED/DEST fallback documented if art regresses); sails across the `mapsmd03` water region (468 Water cells, 252-cell main body, bbox [71,11]–[95,40], runtime-classified from the theater `[General]` tile-sets) via A* on the water grid (every path cell Water, folded into the hash), refuses a land-cell order (empty/stay-put — the water/land inverse), approaches over water and fires FlakTrackGun to a kill, holds station on resume; voxel ship at altitude 0 (existing draw path); wedge guards armed (`docs/re/naval.md`) |
| `--demo-superweapon` | `superweapon_hash=189496664f5eca68` | `deterministic=yes`; grants the Soviet Nuclear Missile (`[NukeSpecial]` `Type=MultiMissile`, `RechargeTime` -> recharge_secs, `[NukePayload]` Damage=600, `[NUKE]` CellSpread=10/PercentAtMax=.02/Verses); charges to READY, fires at the target cell, detonates via the REUSED `apply_spread_damage` (enemies-only, instant), spares the firing house, leaves out-of-radius units untouched, re-charges from 0; NukeIcon cameo + gclock2 recharge clock (art-guarded, placeholder if absent); wedge guards armed (`docs/re/superweapon.md`) |
| `--demo-techtree` | `techtree_hash=e73adae61ff6f71f` | `deterministic=yes`; fresh Allied base (no radar), `roster=20 sref_slot=4`; BASELINE `SREF unbuildable (GATECH unmet) can_start=0 ; MTNK can_start=1` → places Battle Lab `GATECH` DIRECTLY (bypasses its radar-gated prereq) → UNLOCK `SREF buildable (GATECH met) can_start=1`; reuses the existing `can_start_unit`/`can_start` predicate verbatim; wedge guards armed (`docs/re/techtree.md`) |
| `--demo-capture` | `capture_hash=58b87b5bc63f7046` | `deterministic=yes`; engineer capture (`ChangeOwner` flip + SW move + AI base-node resync + startup-from-neutral) of an enemy building AND a neutral `CAOILD` (Startup 1000, Amount 20 / Delay 100f); spy infiltrate (cash-steal 0.5×, power-blackout 1000f, SW recharge reset, factory→produced `veteran_rank=1` ×1.1 firepower/×1.5 armor, radar/lab inert); disguise + dog `DetectDisguise` reveal (spy stays disguised on attack — PermaDisguise); nine wedge guards armed (`docs/re/capture.md`) |
| `--demo-shroud` | `shroud_hash=d302852aa9517661` | `deterministic=yes`; per-house visibility (near-circular Sight reveal, dx2+dy2<=sight2, cap 10), fog no-regrow (explored sticky), fogged-enemy untargetable (seen_by_mask), spy->enemy-radar reshroud (RadarLab), SpySat reveal-all; wedge guards armed (`docs/re/shroud.md`) |
| `--demo-multihouse` | `multihouse_hash=39412ae996cce8ae` | `deterministic=yes`; N-player HouseRec table + alliances (is_enemy/are_allied, FFA default a!=b), AllyReveal shared sight, last-team-standing victory (ShortGame=last-building), per-house side/color (8 MP colors); wedge guards armed (`docs/re/multihouse.md`) |
| `--demo-skirmish` | `skirmish_hash=e9ad3f1fd88495f9` | `deterministic=yes`; config-driven launch (SkirmishConfig -> launch_skirmish -> World), house table + toggles + AI-enable + per-house credit seed, two alliance-teams + neutral; wedge guards armed (`docs/re/skirmish.md`) |

## Android (arm64-v8a) cross-compile

This proves the library is portable: the same sources in `src/` build for
Android via the NDK's own CMake toolchain file, with no source changes.
There is no app and no on-device run here — artifact inspection only
(confirming `libra2assets.so` is a real AArch64 ELF shared object).

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

## Android app (APK — arm64-v8a)

`android/` is a self-contained SDL2 Android project that packages the whole
engine (all of `engine/*.cpp` + the `ra2assets` library from `src/`) into an
installable **arm64-v8a** APK. `SDLActivity` loads `libSDL2.so` + `libmain.so`
and calls `SDL_main` (our `engine/main.cpp`), which on Android launches
straight into the interactive scene — tap to select the unit, tap to move
(A\* + walk animation), drag to pan. No menus.

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
your SDK + NDK:

```properties
sdk.dir=D:/android-sdk
ndk.dir=D:/android-ndk-r27c
```

### Build the APK

```powershell
$env:JAVA_HOME = "C:\Program Files\Java\jdk-21"
$env:ANDROID_HOME = "D:\android-sdk"
Set-Location D:\ra2-android\android
.\gradlew.bat assembleDebug
```

Output: `android/app/build/outputs/apk/debug/app-debug.apk` (arm64-v8a only).
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
2. **Place your own game files** where the app reads them. The engine reads the
   mix chain from `SDL_AndroidGetExternalStoragePath()/ra2/`, i.e.
   `/storage/emulated/0/Android/data/org.ra2.engine/files/ra2/`. Push at least
   `mapsmd03.mix`, `ra2.mix`, and `ra2md.mix` (the loader opens the
   theater/config/palette/unit mixes nested inside `ra2.mix`/`ra2md.mix`, so
   push the full set you own).

   **The files must be readable by the app** — this is the #1 cause of "it
   opens then instantly closes". `Android/data/<pkg>/` is scoped storage, so:
   - **Real phone:** copy the files in with a file manager / MTP, or
     `adb push` **as the normal (non-root) shell user** — both route through
     MediaProvider and give the files the correct SELinux label
     (`media_rw_data_file`) that the app is allowed to read.
   - **Do NOT `adb root` then push** into that folder: root-created files get
     the `storage_file` label and the app's sandbox is SELinux-denied `read`
     (you'll see `avc: denied { read } ... tcontext=...storage_file` in
     logcat). If you already did this, either re-push as non-root, or on an
     emulator only, work around it with `adb root; adb shell setenforce 0`.
   ```powershell
   # non-root push (correct label); create the dir first if needed
   adb shell mkdir -p /sdcard/Android/data/org.ra2.engine/files/ra2
   adb push "D:\Red Alert 2\mapsmd03.mix" /sdcard/Android/data/org.ra2.engine/files/ra2/
   adb push "D:\Red Alert 2\ra2.mix"      /sdcard/Android/data/org.ra2.engine/files/ra2/
   adb push "D:\Red Alert 2\ra2md.mix"    /sdcard/Android/data/org.ra2.engine/files/ra2/
   ```
3. **Launch** the app (`RA2 Engine`) from the launcher, or:
   ```powershell
   adb shell am start -n org.ra2.engine/.RA2Activity
   ```
   It opens into the real temperate map; tap-select and tap-move work with
   touch. Capture a device screenshot with `adb exec-out screencap -p > shot.png`.

   **Diagnosing failures:** the engine logs to Android logcat under the
   `SDL/APP` tag. On startup it prints `[assets] android asset dir = <path>`,
   and any fatal error is logged as `fatal: <reason>` (e.g.
   `cannot open file: .../ra2.mix`) before it exits — so
   `adb logcat -s SDL/APP:V` tells you exactly what went wrong instead of the
   app vanishing silently.

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
