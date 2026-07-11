# Buildings & Construction — full ConYard loop with the real RA2 sidebar

**Date:** 2026-07-11 · **Status:** approved (design sections 1–5 approved in session)
**Calibration authority:** `docs/re/building-render.md` + `docs/re/construction.md`
(NEW — Task 1 of the plan produces both; rendering + construction semantics
recovered from gamemd.exe, per the standing calibrate-first directive and the
user's explicit "dont forget to use ghidra" instruction for this phase)

## Goal

Buildings as a first-class entity: the Allied and Soviet core sets rendered
from the real SHP art with multi-cell foundations, and the full ConYard
construction loop — real RA2 sidebar (credits, power meter, tabs, cameo grid
with build clock), BuildTime countdown funded by a fixed credit pool,
placement cursor with foundation/adjacency validity, buildup animations,
power grid with low-power build penalty.

## Scope decisions (approved)

- **Full ConYard loop, no economy.** Fixed starting credit pool (rules
  `[MultiplayerDialogSettings]` Money= default), cost deducted per the
  recovered schedule, real BuildTime countdown. Refinery/harvesting is a
  later phase.
- **Roster: Allied + Soviet core sets (8 buildings).** Player (Allied):
  GACNST, GAPOWR, GAPILE, GAWEAP. Enemy (Soviet): NACNST, NAPOWR, NAHAND,
  NAWEAP.
- **Buildings-only production.** The ConYard's BuildingType queue is the one
  production line. Barracks/War Factory exist, satisfy prerequisites, and
  draw power, but do not train/build units — unit production (factory exit
  paths, rally points) is its own next phase.
- **Pre-built enemy base.** The Soviet base spawns complete via the same
  placement function (validity-bypass flag for scripted spawns). Only the
  player runs the construction loop; AI base-building is a later AI phase.
- **Full RA2 sidebar, core functions live:** real side art, working credits
  tally, power meter, tab strip (Building tab live, others greyed), cameo
  grid with clock sweep + READY flash. Radar area shows the static
  no-radar frame; Repair/Sell/Waypoint buttons drawn but inert.

## Ground truth already extracted (this session, from the real install)

- rulesmd.ini `[GACNST]`: ConstructionYard=yes, Strength=1000,
  Armor=concrete, Adjacent=2, Factory=BuildingType, Cost=3000, Power=0,
  Explosion=TWLT070,S_BANG48,S_BRNL58,S_CLSN58,S_TUMU60.
- rulesmd.ini `[GAPOWR]`: BuildCat=Power, Prerequisite=GACNST, Strength=750,
  Armor=wood, TechLevel=1, Adjacent=2, Cost=800, **Power=200**.
- rulesmd.ini `[GAWEAP]`: WeaponsFactory=yes, Prerequisite=PROC,GAPILE,GACNST,
  Factory=UnitType, Strength=1000, Armor=wood, Cost=2000, **Power=-25**,
  Bib=yes. (PROC prerequisite handling: see Placement/queue rules below.)
- artmd.ini `[GACNST]`: Remapable=yes, **Foundation=4x4**, Height=4,
  Buildup=GACNSTMK, NewTheater=yes (+ ActiveAnim/ProductionAnim keys — idle
  anims are OUT of scope, documented divergence).
- artmd.ini `[GAPOWR]`: **Foundation=2x2**, Buildup=GAPOWRMK, Cameo=POWRICON.
- artmd.ini `[GAPILE]`: **Foundation=3x2**, Buildup=GAPILEMK, Cameo=BRRKICON.
- **Asset locations verified with ra2extract** (the NewTheater discovery):
  temperate building art is the **generic-letter 'G' fallback** —
  `ra2.mix/generic.mix/ggcnst.shp` (63,760 B), `ggpowr.shp` (16,416 B),
  `ggweap.shp` (43,008 B), `ggpile.shp` (25,712 B) all exist; snow-specific
  art (letter 'A': `gacnst.shp` 63,736 B, `gapowr.shp`, `gapile.shp`) lives
  in `ra2.mix/snow.mix`; buildup anims are theater-specific in
  `ra2.mix/isotemp.mix` (`gtcnstmk.shp`, 428,080 B). No `gtcnst.shp` exists
  anywhere — temperate main art IS the generic art.
- **Sidebar art verified:** `ra2.mix/sidec01.mix` (Allied) and `sidec02.mix`
  (Soviet) both contain `top.shp`, `side1.shp`, `side2.shp`, `tab00.shp`,
  `tab01.shp`, `powerp.shp`, `credits.shp`. Cameos:
  `language.mix/cameo.mix/powricon.shp` (2,912 B), `brrkicon.shp`, at
  native 60×48.

## Task 1 — Ghidra hunt (BEFORE any implementation)

Content-anchored hunt in gamemd.exe (headless analyzeHeadless at D:\ra2-re;
string anchors: building art names, theater letters, `generic.mix`,
`BuildSpeed`, `Adjacent`, `Foundation`, sidebar art names). Hardcoded
YRpp/Ares addresses remain untrusted. Two RE docs with confidence tables;
anything unrecoverable gets `[RE-FALLBACK]` (ModEnc/OpenRA read off-repo,
prose only) and the plan-amendment gate runs before dependent tasks dispatch.

**`docs/re/building-render.md`:**
1. NewTheater filename resolution — second-letter substitution (T=temperate,
   A=snow, U=urban), the 'G' generic fallback, exact lookup order and mix
   search set. Must explain the verified on-disk layout above.
2. Foundation semantics — `Foundation=WxH` → occupied cells relative to the
   placement origin cell; SHP canvas registration/anchor point on the
   foundation at our 30×15 half-cell iso metric.
3. Multi-cell draw order — how a 4x4 building sorts against units in front/
   behind (YSort/ZAdjust art keys), enough to slot into our painter's sort
   without seams.
4. Damage states — SHP frame layout (healthy + damaged halves),
   `[AudioVisual]` ConditionYellow/ConditionRed thresholds, when damaged art
   applies; whether rubble frames exist in the layout.
5. Buildup anim — frame rate, play-once phase, transition to idle frame;
   placement sound (BuildingSlam or per-building).
6. Palette + remap — confirm unit-theater palette (`unittem.pal`) and SHP
   rows 16–31 house remap for Remapable=yes.

**`docs/re/construction.md`:**
7. BuildTime math — Cost → duration formula ([General] BuildSpeed=.7,
   frames-per-credit at 15 Hz), and the low-power penalty multiplier on
   building production.
8. Power model — output vs drain accumulation, the low-power condition, its
   effects list, and the power-meter thresholds (green/yellow/red).
9. Placement validity — Adjacent=N measurement (from foundation edge or
   center; which own buildings count), cell passability requirements,
   green/red per-cell feedback rules.
10. Queue rules — credits deducted incrementally vs up-front; cancel refund
    amount; one-item-at-a-time per queue in 1.001; READY hold behavior.
    Prerequisite semantics for lists (GAWEAP needs PROC — with no refinery
    this phase, GAWEAP is expected unbuildable unless the hunt shows
    otherwise; the demo script and tests treat GAWEAP as
    prerequisite-blocked and it appears greyed in the sidebar. Its art/power
    still exercise via the pre-built Soviet NAWEAP.)
11. Sidebar metrics — layout constants (cameo grid position/spacing, strip
    stacking, power meter geometry, credits slot) from the side art SHP
    dimensions + binary constants; the credits font (language.mix font
    files) — is it trivially extractable?

## Architecture (approved: dedicated modules mirroring the unit pattern)

1. **ra2assets additions** (pure, shared via `cmake/ra2_sources.cmake`):
   - `Rules`: `foundation(id)` → {w,h} from `Foundation=WxH`, `power(id)`,
     `adjacent(id)`, `prerequisite(id)` (list), `cost(id)`,
     `is_construction_yard(id)`, `factory_kind(id)`, `[AudioVisual]`
     ConditionYellow/ConditionRed, `[MultiplayerDialogSettings]` Money.
   - `Art`: `buildup(image)`, `cameo(id)`, `remapable` (exists), plus
     YSort/ZAdjust keys only if the hunt says the sort needs them.
   - **Theater name resolver** (pure helper): image + theater → ordered
     candidate filenames (`gtcnst.shp` → `ggcnst.shp` → literal
     `gacnst.shp`) and mix search set, per the recovered rule; unit-tested
     against the verified real locations.
2. **`engine/building.{hpp,cpp}`** (pure, headless-testable):
   - `struct Building { int id_index; int cx, cy; int fw, fh; double hp,
     max_hp; int house; enum State { Buildup, Idle, Dead }; double
     buildup_t; }` (exact fields per plan).
   - Foundation occupancy: placement claims all fw×fh cells in the existing
     `Occupancy` grid (blocks pathfinding like units); released on death.
   - Frame selection (pure): buildup frames at the recovered rate → healthy
     frame 0 → damaged half when hp/max_hp ≤ ConditionYellow → death plays
     first `Explosion=` anim (vehicle death pattern), occupancy released.
   - Placement validity (pure): every foundation cell in-bounds, passable,
     unoccupied, AND within Adjacent= range per the recovered measurement;
     scripted spawns use the same function with a bypass flag.
   - Draw anchoring from foundation size + SHP canvas per the recovered
     registration rule; buildings enter the existing painter's sort with the
     recovered sort key.
3. **`engine/production.{hpp,cpp}`** (pure, no SDL):
   - `ProductionState { int credits; double power_output, power_drain;
     BuildQueue queue; }` — one BuildingType queue.
   - `can_start(id)`: prerequisites among own live buildings + affordable +
     queue free. `tick(dt)`: progress at recovered BuildTime rate × low-power
     multiplier when drain > output; completion → READY (held until placed).
   - `cancel()`: refund per recovered rule. Credits deducted incrementally
     or up-front per what the binary does.
   - Power recomputed from live buildings on every change; feeds the build
     penalty and meter. ConYard death → queue cancelled, cameos grey.
4. **`engine/sidebar.{hpp,cpp}`**:
   - Pure `SidebarLayout` (rects computed from art sizes + recovered
     metrics) with hit-testing; unit-tested headless. Draw calls in main.cpp.
   - Anchored right edge, native art scale: radar frame (static), credits
     readout (rolling tally), tab strip (Building live, others greyed),
     2-column cameo grid ordered by TechLevel, power meter (`powerp.shp`
     pips, output vs drain, recovered color thresholds, low-power flash).
   - Cameo states: available / darkened (unaffordable or prereq missing) /
     clock-sweep building / READY flash. Click routing: sidebar rect
     hit-tests first in input.cpp; sidebar clicks never reach world orders.
   - Credits digits: game font if trivially extractable from language.mix,
     else a small built-in digit strip (documented divergence; plan decides
     after the hunt).
   - Game-field viewport shrinks by the sidebar width (viewport-rect change
     only).
5. **Placement mode (input.cpp + world):** READY cameo click enters
   placement; hovered origin cell renders the fw×fh grid green/red per-cell;
   left-click valid → Buildup state starts (buildup anim + placement sound),
   occupancy claimed, queue frees; right-click/Esc returns to READY (item
   kept).
6. **main.cpp wiring:** load 8 buildings' SHPs + buildups + cameos + sidebar
   art through the theater resolver and existing TextureCache (house remap
   rows 16–31 blue/red like infantry); per-asset guarded loads; player
   starts with ConYard + existing squad, Soviet base pre-built across the
   map; buildings targetable by existing combat (armor concrete/wood rows
   already in the Verses tables).

## Demo / regression

- New `--demo-base`: scripted command stream drives the sidebar (start
  GAPOWR → wait → place → GAPILE → …) against the pre-built Soviet base;
  deterministic, headless-capable, rendered-vs-headless byte-compare.
  Witnesses: power totals after each placement, credits after each
  deduction, build durations in frames from the recovered formula,
  occupancy cell counts (player base total 16+4+6: pre-placed ConYard +
  built GAPOWR + built GAPILE).
- **`--demo-combat` and `--demo-tank` byte-identity remain untouched hard
  gates** (no buildings spawn there).
- Interactive mode gets the full loop: sidebar, placement cursor, combat
  against buildings.

## Error handling

Every asset load individually guarded: a failed building SHP drops that
building from the buildable set (one log line); failed sidebar art degrades
to no-sidebar (construction unavailable); malformed INI values throw at the
load boundary, caught and logged. Never crash.

## Testing (approved)

- **ra2assets:** foundation/power/prereq/cameo/Money parsing pinned against
  real rulesmd values (install-guarded, clean skips without
  `RA2_INSTALL_DIR`); theater-resolution candidate lists pinned to the
  verified real locations (ggcnst→generic.mix, gacnst→snow.mix,
  gtcnstmk→isotemp.mix).
- **building.cpp (pure):** foundation claim/release cell math; frame
  selection across hp thresholds; buildup timing; placement validity truth
  table (out-of-bounds / occupied / impassable / adjacency edges at the
  recovered measurement).
- **production.cpp (pure):** BuildTime pins hand-computed from recovered
  constants for Cost=800 and Cost=3000; low-power multiplier; credits
  deduction schedule; cancel refund; prereq gating (GAWEAP blocked without
  PROC); ConYard-death queue cancel.
- **sidebar:** layout rects from known art sizes; hit-test mapping.
- **Integration:** `--demo-base` determinism + witnesses; suite green with
  AND without install; arm64 NDK compiles all new sources into libmain.so.

## Documented divergences (README)

No idle ActiveAnims/ProductionAnims; no repair/sell (buttons inert); no
rubble frames (unless trivially in the frame layout); no shroud; static
radar frame; sidebar tabs beyond Building inert; possibly the credits font;
bib art (Bib=yes) per the hunt — include only if a one-liner, else divergence;
GAWEAP unbuildable by the player this phase (PROC prerequisite; exercised
via the pre-built Soviet NAWEAP).

## Constraints (standing, binding)

GPL-clean (XCC/OpenRA/YRpp/ModEnc read off-repo, prose conclusions only;
numeric tables extracted from the binary ARE committable); no game assets,
gamemd.exe, or Ghidra project committed; tests skip cleanly (MESSAGE+return)
without `RA2_INSTALL_DIR` (default `D:\Red Alert 2`); pure modules shared via
`cmake/ra2_sources.cmake`; run test exes from PowerShell not Git Bash; work
on `master` (solo, no remote), frequent commits; commit trailer
`Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`; demos write scratch
output to the session scratchpad (existing debt: `--out-dir` flag).
