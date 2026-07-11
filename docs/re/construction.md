# Construction / production semantics — reverse-engineering notes

(Method: `docs/re/workflow.md`; image base `0x00400000`, YR 1.001 `gamemd.exe`.
Hunt scripts, all off-repo at `D:\ra2-re\scripts\`: `building_hunt.py` …
`building_hunt11.py`. Anchored from `"BuildSpeed"`, `"LowPowerPenaltyModifier"`,
`"MaximumQueuedObjects"`, `"Prerequisite*"`, `"Adjacent"`, `"POWERP.SHP"`,
`"SIDE1.SHP"`, `"TAB%02d.SHP"`, `"GCLOCK2.SHP"` string refs and the
`.?AVFactoryClass@@` / `.?AVSidebarClass@@` / `.?AVPowerClass@@` /
`.?AVHouseClass@@` RTTI → vtables. Decompiles + forced listings under
`D:\ra2-re\out\bh*_*`; nothing under `D:\ra2-re` is committed. Content-anchored
only; published YRpp/Ares addresses were never used as evidence. Empirical SHP
dimensions measured with the repo's `ra2extract` against the real install.)

Companion of `building-render.md`; calibration authority for §7 BuildTime math,
§8 power model + power bar, §9 placement validity, §10 queue rules,
§11 sidebar metrics. "Decisions for ra2engine" at the end resolves every
`[RE: construction.md §N]` marker in the plan's Tasks 2–7.

## Located functions

| Function | Role | How found |
|----------|------|-----------|
| `FUN_00711280` | TechnoTypeClass base time (vtbl +0x88): `ftol(Cost × BuildSpeed × 0.9)` | TechnoTypeClass vtable (RTTI) slot walk; listing |
| `FUN_006F3D00` | TechnoClass::Time_To_Build — full formula incl. low-power + MultipleFactory (forced listing `bh6_ttb_6F3D00_listing.txt`) | consumer of rules +0x570/+0x574/+0x578/+0x57C |
| `FUN_004FCC50` | HouseClass power fraction: `output<drain ? (output ? output/drain : 0.0) : 1.0`; house output @+0x53A4, drain @+0x53A8 | callee of Time_To_Build; listing |
| `FUN_0050BEC0` | house/country BuildTime multiplier (HouseTypeClass `BuildTime<class>Mult`) | callee of Time_To_Build |
| `FUN_00500730` | HouseClass factory count per product class (buildings @house+0x5384) | callee of Time_To_Build |
| `FUN_0066CF70` | RulesClass `[General]` loader: `BuildSpeed`→+0x1748(dbl), `BuildupTime`→+0x1518, `MinLowPowerProductionSpeed`→+0x570(f), `Max…`→+0x574, `LowPowerPenaltyModifier`→+0x578, `MultipleFactory`→+0x57C, `MaximumQueuedObjects`→+0xF0, `WallBuildSpeedCoefficient`→+0x758, `Prerequisite{Power,Factory,Barracks,Radar,Tech,Proc}`→ int vectors +0x358…+0x3E4 (stride 0x1C), `PrerequisiteProcAlternate`→+0x400 | string refs |
| `FUN_004C9B90` | FactoryClass::Set/Start: cost snapshot (`balance` @+0x60 = `Cost_Of(house)`), building products abandon any current item first; queue push for others | FactoryClass code window sweep (RTTI vtable @`0x7E78D0`) |
| `FUN_004C9DC0` | FactoryClass rate update: `step_ticks = clamp(Time_To_Build/54, 1, 255)`; blocked check `balance/(54−stage)` vs available money | same sweep |
| `FUN_004C9A40` | FactoryClass::AI — the per-step tick: stage++, spend `balance/(54−stage)`, stall when broke, complete at stage 54 | same sweep |
| `FUN_004C9F10` | FactoryClass::Abandon: refund `Cost_Of − balance` (all spent), clears per-class in-production markers | `"Abandoning production of %s"` string |
| `FUN_004C9930` / `FUN_004CA690` | FactoryClass dtors — abandon (with refund) on destruction | vtable slot 0 |
| `FUN_004265B0` | TimerStruct remaining (`duration − (now − start)`, 0 when expired) | callee of AI |
| `FUN_004F95B0` / `FUN_004F9770` | HouseClass Spend / Refund money | callees of AI/Abandon |
| `FUN_0050B550` / `FUN_0050B510` | house "is human-controlled" checks (campaign/MP variants) | callees of Start |
| `FUN_00505180` | prerequisite check: every token ANDed; negative ids → rules BuildingType group vectors | consumer of type prereq vector +0x63C/+0x648 |
| `FUN_004A8DC0` | Passes_Proximity_Check — the `Adjacent=` rule (only enforced for the local human player) | consumer of BuildingTypeClass +0xEB4 |
| `FUN_0044E7B0` / `FUN_0044E880` | BuildingClass::Power_Output / Power_Drain (output scales with health) | consumers of type +0xEE0/+0xEE4 |
| `FUN_0063F5C0` | PowerClass bar draw: pip column, 3 px pitch, powerp frames 0..4 | consumer of `"POWERP.SHP"` @`0x835D94` |
| `FUN_0063F940` / `FUN_0063F400` / `FUN_0063F2F0` | PowerClass AI / pip-target split / capacity scale (listings) | PowerClass vtable (RTTI) |
| `FUN_006A4ED0` | SidebarClass art init: SIDE1/2/3.SHP, ADDON.SHP, `TAB%02d.SHP` ×4, R-UP/R-DN, REPAIR/SELL, GCLOCK2, DARKEN | consumer of `"SIDE1.SHP"` @`0x83E9B0` |
| `0x6A8BD0` (gap, 4210 B) | cameo StripClass draw: 2 columns, 60×48 shade rect, GCLOCK2 stage overlay, quantity/READY text | window sweep 0x6A7400–0x6AB900; `"DARKEN.SHP"` raw ref `0x6A77F3` |
| `FUN_00434A50` | loads `GAME.FNT` at startup (old-style bitmap font present in RA2) | `"GAME.FNT"` @`0x817B98` |
| `FUN_0045FE40` | BuildingTypeClass::LoadFromINI: `Adjacent`→+0xEB4 (int), `Power`→ split +0xEE0 output / +0xEE4 drain, `BaseNormal`→+0x154F, `EligibleForAllyBuilding`-style flag +0x1550 | string refs (see building-render.md) |

## §7 BuildTime math

**Base formula** — `TechnoTypeClass` vtbl +0x88 (`FUN_00711280`), 5
instructions:

```
frames = ftol( Cost × Rules.BuildSpeed × 0.9 )
```

- `Cost` = the type's `Cost=` int (type+0x610, key string @`0x824470`).
- `BuildSpeed` = rules `[General]` double (+0x1748), rulesmd `.7` — "minutes to
  produce a 1000-credit item".
- `0.9` = literal double @`0x7F3E80` = 900 frames-per-minute ÷ 1000 credits.
- `ftol` truncates (MSVC `__ftol`).

So `frames = trunc(Cost/1000 × BuildSpeed × 900)` at 15 Hz — the pre-hunt
expectation confirmed exactly. GAPOWR: `800 × .7 × .9 = 504` frames = 33.6 s.
GACNST: `3000 × .7 × .9 = 1890` frames = 126 s. There is **no per-type
`BuildTime=` key**: the `"BuildTime"` string in the binary is only the
HouseTypeClass (country) multiplier and the `BuildTimeMultiplier=` techno key
(type+0x608, default 1.0, absent from our 8) — the Cost path is the only path.

**Full `Time_To_Build`** (`FUN_006F3D00`, applied when the factory computes its
step rate):

```
t = ftol( base × country_BuildTime<class>Mult )      (FUN_0050BEC0; 1.0 default)
t = ftol( t × type.BuildTimeMultiplier )             (+0x608, 1.0 default)
speed = 1 − LowPowerPenaltyModifier × (1 − power_fraction)     (§8)
speed = max(speed, MinLowPowerProductionSpeed)                 (.5)
if power_fraction < 1: speed = min(speed, MaxLowPowerProductionSpeed)  (.8)
if speed == 0: speed = 0.01                          (float @0x7F3E34)
t = ftol( t / speed )
n = house factory count for this product class      (FUN_00500730)
if MultipleFactory > 0: repeat (n−1)×: t = ftol(t × MultipleFactory)   (.8 each)
walls only: t = ftol(t × WallBuildSpeedCoefficient)  (rules+0x758, 3.0)
```

Note the low-power clamp order: with `LowPowerPenaltyModifier=1` and fraction
`f`, `speed = clamp(f, .5, .8)` when `f<1`, else `1.0` — i.e. even 99% power
caps building speed at 0.8×.

## §8 Power model + low-power penalty

- **Per-building contribution** (`FUN_0044E7B0`/`FUN_0044E880`): the rules
  `Power=` key is split at load: positive → `PowerBonus` (type+0xEE0), negative
  → `PowerDrain` (type+0xEE4, stored positive). A building contributes only
  when in service (its online/`HasPower` flag set — buildings still in the
  buildup state or offline contribute nothing) and:
  - `Power_Output = trunc(PowerBonus × Health_Ratio)` — **output scales with
    current health**; plus upgrade-plug bonuses (out of scope).
  - `Power_Drain = PowerDrain` — drain does NOT scale with health.
- **House totals**: output at house+0x53A4, drain at +0x53A8 (read by the power
  fraction fn and the sidebar). **Low-power condition: `output < drain`.**
- **Power fraction** (`FUN_004FCC50`):
  `f = (output < drain && drain != 0) ? (output ? output/drain : 0.0) : 1.0`.
- **Penalty**: see §7 — `speed = clamp(1 − mod×(1−f), Min, Max-if-low)`. This
  is applied inside `Time_To_Build`, i.e. to ALL production; the factory
  re-computes its step rate from it (§10), so power changes mid-build take
  effect at the next step.
- **Power bar** (PowerClass; art `powerp.shp` 12×2 px, 5 frames — empirically
  frame 0 = dark/empty pip, 1 = green, 2 = yellow, 3 = red, 4 = grey/white
  flash pip, from decoding the frames against `sidebar.pal`):
  - Column of pips at 3 px vertical pitch; `total_pips = (bar_height+3)/3`;
    the y origin is 0x45 px below the sidebar-top global, drawn top→down:
    empty pips first, then green, yellow, red at the bottom.
  - **Capacity scale** (`FUN_0063F2F0`): `total_power = Σ output + Σ drain`
    over the player's buildings;
    `pips_used = total_pips − clamp(ftol(total_pips × 400/(total_power+400)), 0, total_pips−1)`
    (400.0 = double @`0x7EC8C8`) — a soft scale that fills the bar as the
    base's total power grows.
  - **Color split** (`FUN_0063F400`): `surplus = output − drain`; if
    `surplus < 0` → no green/yellow; else `yellow_amt = min(surplus, 100)`,
    `green_amt = surplus − yellow_amt`, `red_amt = drain`; the used pips are
    divided proportionally (`amt / (drain + yellow_amt + green_amt)`), i.e.
    **red = the drained portion, yellow = the first 100 surplus, green =
    surplus beyond 100**.
  - **Outage flash**: on any output/drain change the bar redraws and a
    10-cycle, 3-tick-period flash counter runs (frame-4 pip / redraw flash)
    while `drain > output`.

## §9 Placement validity

**Adjacency** — `Passes_Proximity_Check` (`FUN_004A8DC0`), for candidate
foundation rect `(x, y, w, h)` (h including the Bib row) and `Adjacent = N`
(BuildingTypeClass +0xEB4, default 2 for our set):

- Scan the rectangle expanded by `N+1` cells on every side:
  `x−(N+1) … x+w+N`, `y−(N+1) … y+h+N`, skipping cells inside the candidate
  foundation itself.
- For each scanned cell, look up the cell's occupying **building** (cell →
  building link, i.e. the neighbor's whole foundation counts).
- Pass if any such building satisfies: `owner == placing house` AND its type
  has **`BaseNormal=yes`** (+0x154F). (With the MP "ally build" option, an
  allied owner + `EligibleForAllyBuilding` flag +0x1550 also passes.)
- Equivalent formulation: **the number of empty cells between the two
  foundation rects must be ≤ N** (Chebyshev gap ≤ N; the code's `N+1` is the
  scan reach from the foundation edge, one cell of which is the neighbor's own
  occupied cell).
- Buildings must be placed on the map to have cell links — a building under
  its buildup animation already occupies its cells (placement claims them), so
  it counts; queued/unplaced items do not. There is no "fully constructed"
  requirement.
- **The check is only enforced for the local human player** (early `return 1`
  unless the house is the local player and it's a real placement) — scripted /
  AI placement bypasses adjacency natively, which is exactly the plan's
  validity-bypass flag for the pre-built enemy base.

**Per-cell requirements**: every foundation cell must be individually legal —
in-bounds, buildable terrain, and free of any object (the placement grid runs
the same cell loop as occupancy claiming; the recovered occupy lists are the
full W×H rect, `building-render.md` §2). The exact terrain-class predicate
(which SpeedType/land types count as buildable) was not isolated in this hunt —
[RE-FALLBACK: TS-lineage semantics per OpenRA/ModEnc: clear/buildable land, no
water/cliffs/overlay obstacles] — our engine's existing passability +
occupancy grid is the correct analogue.

**Per-cell feedback**: the placement cursor art is `place.shp`
(`conquer.mix`, 60×59 canvas, 38 frames — green/red cell diamonds and band
variants), loaded next to the grid globals by the display init
(`FUN_004A8760`). Cells draw green when the individual cell passes and red
otherwise; adjacency failure makes the whole grid invalid.

## §10 Queue rules

FactoryClass (RTTI vtable @`0x7E78D0`; object fields: +0x24 stage counter,
+0x2C timer, +0x38 rate, +0x44/+0x50 queue array/count, +0x58 product object,
+0x5C IsBlocked, +0x60 balance, +0x6C owner house, +0x70 IsDone):

- **Production is 54 discrete steps** (`0x36` literal throughout).
  `step_ticks = clamp(Time_To_Build / 54, 1, 255)` (`FUN_004C9DC0`), re-derived
  when conditions (power) change — GAPOWR at full power: `504/54 = 9` ticks per
  step, 54×9 = 486 ≈ 504 frames (the clamp+trunc make actual duration
  `54 × trunc(TTB/54)`).
- **Credits are deducted incrementally per step** (`FUN_004C9A40` AI): each
  step costs `balance / (54 − stage)` (integer division, remainder folded into
  later steps; final step spends the whole remaining balance). Money is spent
  via HouseClass::Spend (`FUN_004F95B0`).
- **Stall at 0 credits**: if `available < step_cost` the stage is decremented
  back (no progress, nothing spent), `IsBlocked` (+0x5C) set, and the step
  retries on the next timer expiry. Production also never starts if
  `available < first step cost` path in the rate check.
- **Cancel refund**: Abandon (`FUN_004C9F10`) refunds `Cost_Of − balance` — 
  **everything spent so far**, in one lump (`FUN_004F9770`).
- **One building at a time**: FactoryClass::Set/Start special-cases building
  products (`WhatAmI == BuildingClass`): any current item is abandoned
  (refunded) before the new one starts — the building factory never queues a
  second item. Non-building factories push into the FactoryClass queue capped
  by rules `[General] MaximumQueuedObjects` (+0xF0, rulesmd 29).
- **READY hold**: at stage 54 the item is complete (`IsDone` +0x70 = 1), the
  remaining balance is spent, and the AI early-outs from then on — the
  finished building is held indefinitely until placed.
- **ConYard death mid-build**: the FactoryClass destructor abandons production
  — the item is cancelled and all spent credits are refunded.
- **Prerequisites** (`FUN_00505180`; parse in TechnoTypeClass::LoadFromINI →
  int vector at type+0x63C/count +0x648): every token must pass (AND):
  - token ≥ 0: a BuildingType index — the house must own at least one **live,
    placed building** of exactly that type (checked against the house's
    owned-building-type list);
  - token < 0: a group id (−1…−6) — satisfied if the house owns ≥ 1 building
    whose type is a member of the corresponding rules `[General]` list
    (`PrerequisitePower / Factory / Barracks / Radar / Tech / Proc`, parsed to
    six BuildingType vectors; the check switches on the negative id to one of
    six RulesClass pointer-vectors @+0x8C8…+0xA34). The name→group binding
    (POWER→PrerequisitePower etc.; token strings live @`0x81B024`–`0x81B1D8`)
    is semantically forced by the key names; the exact negative-id numbering
    is [RE-FALLBACK: ModEnc] and behaviorally irrelevant.
  - `PrerequisiteProcAlternate` (rules+0x400, `SMIN`) additionally counts as a
    PROC in the sidebar-side buildable logic (loader confirmed; not part of
    `FUN_00505180` itself).
  - **GAWEAP (`Prerequisite=PROC,GAPILE,GACNST`) is therefore unbuildable
    without a refinery** — PROC is ANDed like every other token. Confirmed.

## §11 Sidebar metrics + credits font

Art dimensions (empirical, `ra2extract`, `sidec01.mix` unless noted):

| file | canvas | frames |
|---|---|---:|
| `top.shp` | 168×32 | 1 |
| `side1.shp` | 168×69 | 1 |
| `side2.shp` | 168×50 | 1 |
| `credits.shp` | 168×16 | 1 |
| `tab00.shp` / `tab01.shp` | 28×27 | 5 |
| `powerp.shp` | 12×2 | 5 |
| `gclock2.shp` | 60×48 | **55** |
| `language.mix/cameo.mix/powricon.shp`, `brrkicon.shp` | 60×48 | 1 |
| `conquer.mix/place.shp` | 60×59 | 38 |

- **Total sidebar width = 168 px** (every chrome piece is 168 wide) — expected
  value confirmed.
- **Stacking** (SidebarClass init `FUN_006A4ED0` + strip draw `0x6A8BD0`):
  top.shp cap (with the radar area) at the top, tab strip of **4 tabs**
  (`TAB%02d.SHP` for tab index 0–3, one 28×27 multi-frame SHP per tab — NOT
  one strip image divided by 4), then the cameo area tiled from
  side1/side2/side3 + ADDON.SHP, credits.shp as the credits readout backdrop.
  The exact y-coordinates are runtime-computed from the art heights, not
  binary constants; the strip struct array (4 × 0x60 bytes @`0xB06BB8`) holds
  per-tab state.
- **Tab frames**: each `tabNN.shp` has 5 frames = the button's states
  (the draw indexes them by tab activation/flash state; per-frame mapping
  beyond frame 0 = idle / lit-state variants was not fully pinned —
  [RE-FALLBACK: visual inspection]; our engine needs only "normal" and
  "active/greyed").
- **Cameo grid**: 60×48 cells in **2 columns** (the strip draw loops
  `column < 2`), rows advancing by a runtime row-pitch global (cameo height +
  gutter). Overlays recovered from the draw: 60×48 shade fill for
  disabled/darkened states (also DARKEN.SHP), quantity number at `x+60−…`,
  "on hold" text, and the **build clock = GCLOCK2.SHP frame overlay** — 55
  frames = idle + 54 sweep stages, indexed by `stage+1`, i.e. the clock sweep
  has exactly one frame per FactoryClass production step (§10).
- **Power meter**: `powerp.shp` pips (12×2) in a single column at the left
  edge of the sidebar strip area, 3 px pitch, starting 0x45 px below the
  sidebar-top global; frame colors/semantics per §8.
- **Credits font**: the credits tally is drawn by the top bar (TabClass) text
  path with the game's font system. `GAME.FNT` (a TD-lineage bitmap .fnt) IS
  present and loaded at startup (`FUN_00434A50` → font loader
  `FUN_00433800`), but the credits string was not positively traced to it (RA2
  text rendering has multiple font paths, e.g. `GRFXTXT.SHP` glyph sheets for
  some UI). Extracting and decoding `.fnt`/glyph-sheet formats is NOT trivial
  within this phase — recommendation stands: **built-in digit strip**
  (documented divergence).

## Confidence

| Item | Evidence | Confidence |
|------|----------|:----------:|
| §7 base frames = trunc(Cost × BuildSpeed × 0.9) | `FUN_00711280` listing + constants 0.9/900.0 read from binary | **High** |
| §7 no per-type BuildTime key (Cost path only) | all `"BuildTime"` refs are country mult / class mults; techno key is `BuildTimeMultiplier` | **High** |
| §7 full multiplier chain (country, type, power, MultipleFactory, walls) | `FUN_006F3D00` forced listing, all constants resolved | **High** |
| §8 Power= split output/drain; output × health; service gating | LoadFromINI + `FUN_0044E7B0`/`FUN_0044E880` | **High** (health-scaling multiply inferred from `Health_Ratio()`+`ftol` pair with decompiler-dropped float op: **Medium-High**) |
| §8 low-power condition + penalty formula + clamps | `FUN_004FCC50` + TTB listing + rulesmd values (.5/.8/1) | **High** |
| §8 power bar pip geometry, frames, capacity scale (400), color split (100-surplus band) | PowerClass draw/AI/scale decompiles + listings + frame-color decode | **High** (exact rounding-remainder distribution: Medium) |
| §9 Adjacent scan rect, BaseNormal filter, human-only enforcement | `FUN_004A8DC0` decompile | **High** |
| §9 per-cell terrain predicate | not isolated; occupy lists + engine analogue | **Low** ([RE-FALLBACK: OpenRA/ModEnc prose]) |
| §10 54 steps, rate clamp, incremental spend, stall, refund-all, READY hold | FactoryClass fns decompiled | **High** |
| §10 one-building-at-a-time; MaximumQueuedObjects=29 for unit queues | Start/Add path + rules key | **High** |
| §10 prerequisite AND semantics, groups via [General] lists | `FUN_00505180` + loader | **High** (group-id numbering: [RE-FALLBACK: ModEnc], behaviorally forced) |
| §11 168 px width, 4 separate tab SHPs, 2-column 60×48 cameo grid, gclock2 55-frame clock | art dims + SidebarClass/strip decompiles | **High** |
| §11 exact pixel y-offsets of each chrome piece | runtime-computed; not binary literals | **Medium** (compute from art heights) |
| §11 credits font identity | GAME.FNT loaded but tally not traced to it | **Low** ([RE-FALLBACK: divergence decision]) |

## Decisions for ra2engine

Resolving every `[RE: construction.md §N]` marker in the plan (Tasks 2–7):

- **§7 build_frames (Tasks 5/7, tests)**: implement
  `frames = (int)(cost/1000.0 * build_speed * 900.0)` — the plan's formula is
  exactly right (binary computes `Cost × BuildSpeed × 0.9` with trunc). Pins:
  GAPOWR (800) → **504**, GACNST (3000) → **1890**. No BuildTime overrides for
  our 8 — confirmed, Cost fallback is the only path. Country/type multipliers
  are all 1.0 for our roster: omit.
- **§8 power totals (Task 5)**: `power_output/drain` recomputed from live
  buildings; buildings in Buildup state contribute NOTHING (service flag
  gating) — plan's pre-hunt assumption confirmed. Optional fidelity: output
  scales with `hp/max_hp` (drain doesn't); include if cheap, else document.
- **§8 penalty (Tasks 5/7)**: REPLACE the plan's bare
  `penalty = modifier × shortage_ratio` with the full recovered rule:
  `speed = 1 − LowPowerPenaltyModifier×(1 − output/drain)` when `drain>output`
  (0 output → speed 0), then `speed = max(speed, MinLowPowerProductionSpeed=.5)`
  and (low power only) `speed = min(speed, MaxLowPowerProductionSpeed=.8)`;
  full power → 1.0. Amend Task 5's `build_speed_multiplier` accordingly.
- **§8 power-bar thresholds (Task 7e)**: NOT "1 pip = 100 power". Correct
  model: `total_pips = bar_h/3` (3 px pitch, 12×2 pips);
  `pips_used = total_pips − trunc(total_pips × 400/(Σout+Σdrain+400))`;
  split used pips proportionally into red = drain, yellow = min(surplus,100),
  green = surplus−100 (drawn bottom red → yellow → green, empty above);
  flash (grey pip frame 4 / redraw) at ~5 Hz-equivalent (3-tick period) while
  `drain > output`. powerp frames: 0 empty, 1 green, 2 yellow, 3 red, 4 flash.
- **§9 adjacency (Tasks 3/7)**: expanded-rect test confirmed: candidate
  foundation expanded by `Adjacent+1` cells; pass if any own building
  (BaseNormal=yes — true for all our 8 except checking rulesmd; GACNST etc.
  default yes) occupies a scanned cell ⇒ **empty-cell gap ≤ Adjacent**.
  Buildings in Buildup count (they occupy cells); enemy/neutral buildings
  never do. Scripted spawns: the original itself bypasses the check for
  non-local-human houses — the plan's bypass flag mirrors the binary.
- **§9 per-cell validity (Tasks 3/7)**: in-bounds + passable terrain +
  unoccupied via the existing Occupancy grid (binary analogue; exact terrain
  predicate is the [RE-FALLBACK] above). Per-cell green/red feedback with
  overall-red on adjacency failure matches the original's place-grid behavior.
- **§10 queue (Task 5)**: one-slot BuildingType queue confirmed (starting a new
  building abandons the current). Credits deducted **incrementally over 54
  steps**, `step_cost = remaining/(54−stage)`; stall (no progress, no charge)
  while broke; cancel refunds **all** spent credits; READY holds indefinitely;
  ConYard death cancels with full refund of spent amount. For fidelity use 54
  steps with `step_ticks = max(1, trunc(frames/54))`; a per-tick linear
  approximation is acceptable if the 54-step gate is kept for witnesses.
- **§10 prerequisites (Task 5)**: every token ANDed; plain id = own live
  placed building of that id; group tokens POWER/BARRACKS/PROC/FACTORY/RADAR/
  TECH = any id in the `[General] Prerequisite<Group>` list. GAWEAP
  prerequisite-blocked without a refinery — confirmed; keep it greyed.
- **§11 layout (Tasks 6/7e)**: sidebar width 168; stack from art heights
  (top 32 → tabs 27 → cameo strips; credits.shp 168×16 readout). `tab_w`:
  **four separate 28×27 SHPs (`tab00`–`tab03`), NOT one strip divided by 4** —
  amend Task 6's `tab00 width/4` note; each has 5 state frames (use frame 0 +
  a lit/greyed variant). Cameo grid: 2 columns of 60×48. Build clock: the
  original uses `gclock2.shp` (sidec01.mix, 60×48, 55 frames = 1 idle + 54
  steps — one frame per production step). Since it ships in the already-loaded
  sidec01.mix and decodes with the existing SHP path, **use gclock2.shp frames
  (frame = 1 + stage) instead of the pre-hunt vertical fill bar** — cheaper
  AND exact. Power meter: pip column at the strip's left edge, 3 px pitch,
  starting ≈0x45 px below the tab area (compute from art, don't hardcode).
- **§11 credits digits (Task 6)**: the game's own tally font is not trivially
  extractable (GAME.FNT format decode is real work and unproven for this
  string) — **use the built-in 5×7 digit strip** (documented divergence), as
  the plan's default. The rolling-tally animation is display-only and stands.
