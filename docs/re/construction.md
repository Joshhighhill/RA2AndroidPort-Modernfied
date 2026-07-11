# Construction / production semantics — reverse-engineering notes

(Method: `docs/re/workflow.md`; image base `0x00400000`, YR 1.001 `gamemd.exe`.
Hunt scripts, all off-repo at `D:\ra2-re\scripts\`: `building_hunt.py` …
`building_hunt11.py`, plus the §11a sidebar-composition passes
`sidebar_hunt.py` … `sidebar_hunt5.py`. Anchored from `"BuildSpeed"`, `"LowPowerPenaltyModifier"`,
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
| `FUN_006A4720` / `FUN_006A47C0` | SidebarClass geometry writers: ALL layout globals (side1 top 0x9E, tab pitch 0x1D, col pitch 0x3F, row pitch 0x32, strip origin, scroll-button row) per side style | writers of `0x885F10`–`0x885F1C` / `0xB0A44C`–`0xB0A484` (xref classify, sidebar_hunt.py) |
| `FUN_006A49A0` / `FUN_006AB3C0` | SidebarClass one-time gadget init / layout applier (repair, sell, tabs, R-UP/R-DN, cameo cell gadget rects) | same global cluster reads |
| `FUN_00652730` / `FUN_006528D0` / `FUN_00652B40` | RadarClass One_Time (top.shp y=16, radar y=48, minimap 140×108 @(16,49)) / Init_For_House (optbtn/diplobtn gadgets) / Draw_It (TOP.SHP + RADAR.SHP anim + minimap) | `"RadarClass: One_Time"` @`0x8382D4`, `"RadarClass: Init_For_House"` @`0x838324`; readers of `0xB0E950`/`0xB0E9D8` |
| `0x6CFF90` (gap) / `FUN_004A2280` | TabClass::Draw_It (CREDITS.SHP at (0,0)) / credits tally print (`"%d"` centered at (84, 2)) | reader of `"CREDITS.SHP"` shape global `0xB0EA78`; callee |
| `FUN_0072EAF0` / `FUN_0072ED40` / `FUN_0072E620` | UI-shell chrome loader (per-side name table @`0x843C8C`: SIDEBTTN/CREDITS/TOP/RADAR(Y)/SIDE1/2/2B/3/ADDON/LENDCAP/RENDCAP) / rect builder (stacks credits+top+radar+side1+side2×n+side3+addon) / shell draw | data-pointer scan for `"TOP.SHP"` @`0x84405C` etc. |
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

## §11a Sidebar composition (exact) — supersedes §11's y-offset guesses

(Second hunt, scripts `sidebar_hunt.py`…`sidebar_hunt5.py`, dumps
`D:\ra2-re\out\sb*_*`. Every number below is a literal read from gamemd.exe —
the geometry writers `FUN_006A4720` + `FUN_006A47C0`, the one-time gadget init
`FUN_006A49A0`, the gadget layout applier `FUN_006AB3C0`,
`RadarClass::One_Time FUN_00652730`, `RadarClass::Init_For_House
FUN_006528D0`, `RadarClass::Draw_It FUN_00652B40`, `TabClass::Draw_It
FUN_006CFF90` (gap fn @`0x6CFF90`), credits tally `FUN_004A2280`, the shell
rect builder `FUN_0072ED40` and chrome loader `FUN_0072EAF0` — cross-checked
against pixel measurements of the decoded sidec01.mix art.)

**Coordinate space.** SidebarClass/RadarClass/TabClass composite into a
dedicated sidebar surface (`DAT_00886280`) whose origin is the bar's top-left:
x0 = `DAT_00885F10` = tactical-view right edge = **screen_w − 168**, y0 =
**window top (0)**. Gadget rects are stored in screen coords and re-based by
`−DAT_00885F10` at draw. `DAT_00885F18` = bar width = **168**
(`DAT_007F4BF8`). `DAT_00885F14` = side1 top = literal **0x9E = 158**. The
tactical view (`FUN_0072A030`) is `{0, 0, screen_w−168, screen_h−32}` — 32 =
`DAT_007F4BFC` = the bottom message-bar reservation; the sidebar column itself
runs to the true screen bottom (`DAT_00885F1C` = screen_h − 158, the space
below side1's top edge).

**Side style.** `[session+0x34B8]` (0 = Allied/sidec01, 1 = Soviet/sidec02,
2 = Yuri/sidec03 — mode 2 loads `RADARY.SHP` instead of `RADAR.SHP`) selects
two constant sets in `FUN_006A4720`/`FUN_006A47C0`. Allied values first,
(Soviet/Yuri in parens) — Soviet and Yuri share one set.

**Layout table** — bar-relative x (add screen_w−168), absolute y, at any
resolution; `N` = visible cameo rows, `strip_h` = 50·N:

| piece | source (sidec01.mix) | x | y | w×h | drawn by |
|---|---|---:|---:|---|---|
| credits backdrop | `credits.shp` | 0 | 0 | 168×16 | TabClass::Draw_It |
| credits tally text | font (see §11) | 84 (bar center), style `0x4108` | 2 | — | `FUN_004A2280`, format `"%d"` |
| top cap | `top.shp` | 0 | 16 | 168×32 | RadarClass::Draw_It |
| options button | `optbtn.shp` (2 fr) | 11 (14) | 20 (21) | 72×18 | gadget id 242 |
| diplomacy button | `diplobtn.shp` (2 fr) | 83 (86) | 20 (21) | 72×18 | gadget id 243 |
| **radar area** | `radar.shp` (33 fr) | 0 | 48 | 168×110 | RadarClass::Draw_It |
| radar minimap viewport | (engine-rendered) | 16 | 49 | 140×108 | radar gadget id 223 |
| side1 | `side1.shp` | 0 | 158 | 168×69 | SidebarClass::Draw_It |
| repair button | `repair.shp` (2 fr) | 20 (33) | 166 (165) | 64×31 | gadget id 101 |
| sell button | `sell.shp` (2 fr) | 84 (85) | 166 (165) | 64×31 | gadget id 102 |
| tab i = 0..3 | `tab0i.shp` (5 fr) | 26 + 29·i (20 + 32·i) | 197 | 28×27 | gadget ids 203+i |
| side2 × N rows | `side2.shp` | 0 | 227 + 50·k | 168×50 | SidebarClass::Draw_It |
| side3 bottom cap | `side3.shp` | 0 | 227 + strip_h | 168×26 | SidebarClass::Draw_It |
| addon tail | `addon.shp` | 0 | 227 + strip_h + 26 | 168×63 | SidebarClass::Draw_It (clipped at screen bottom) |
| scroll DOWN | `r-dn.shp` (3 fr) | 39 | 227 + strip_h + 7 | 46×25 | gadget id 201 |
| scroll UP | `r-up.shp` (3 fr) | 39+46 = 85 (39+45 = 84) | same | 46×25 | gadget id 200 |
| power pip column | `powerp.shp` 12×2 (5 fr) | 5 (0) | 227 + 3·k | 12×2 per pip | PowerClass::Draw_It |

Anchors: 158 = 16 + 32 + 110 (credits + top + radar heights) — the binary's
literal `0x9E` and the shell's `credits.h + top.h + radar.h` stacking agree.
Strip top `DAT_00B0A468` = `DAT_00885F14 + 0x45` = 158 + 69 = **227** (0x45 =
69 = side1_h — the strip begins exactly at side1's bottom edge). Scroll-button
row y = `DAT_00B0A47C` = strip_top + strip_h + 7.

- **Row count**: `N = floor((DAT_00885F1C − 227 − side3_h − 7 + 158)/50)`
  with side3_h counted as 26 (18 for Soviet/Yuri) ⇒
  `N = floor((screen_h − 234 − side3_h)/50)` = **6 rows (12 cameos) at
  800×600** Allied (also 6 for Soviet/Yuri). The pre-game shell
  (`FUN_0072ED40`) uses `floor((screen_h − 253)/50)` — same result at standard
  sizes. side2 tiles once per row; side3 caps the stack; addon is drawn after
  side3 and clips off the bottom of the screen (art has decorative machinery +
  the scroll-arrow socket tops).
- **Radar area content**: pre-radar the engine draws `radar.shp` **frame 0**
  (the side logo — Allied eagle) into the 168×110 rect; on radar
  activation/deactivation it steps frames 1..31 as a boot animation
  (state machine at RadarClass+0x14AC/+0x14FC, 4-tick frame timer), and frame
  **32** is the active backdrop behind the live minimap, which renders in the
  inset 140×108 viewport at (16, 49) (viewport x = `(168−0x90)/2 + 4` = 16
  Allied, `(168−0x91)/2 + 5` = 16 Soviet/Yuri; y = member +0x11F4 = 0x31 = 49;
  w/h = members +0x11F8/+0x11FC = 0x8C/0x6C set in `RadarClass::One_Time`).
  `radar.shp` lives in sidec01.mix (168×110×33, verified with ra2extract);
  `radary.shp` is the Yuri variant.
- **Cameo grid (S2)**: origin = (`DAT_00B0A464` − x0, strip_top + 1) =
  **(22, 228)**; column pitch `DAT_00B0A46C` = **63** (64), row pitch
  `DAT_00B0A470` = **50** (both styles); 2 columns × N rows, row-major;
  cell = 60×48. Scrolling is by whole rows (strip cell index =
  `col + (row + TopRow)·2`). The EMPTY sockets are **baked into side2.shp**:
  each 168×50 tile carries two 60×48 socket recesses whose interiors sit at
  side2-local x ≈ 26..81 and 89..144 (pixel-measured) — i.e. the cameo rects
  (22..81, 85..144) cover socket + inner bevel, with the 3-px ridge at
  x 82..84 left visible between columns. `side2b.shp` is the socket-less
  variant used by the pre-game shell. side1 carries no sockets (its lower
  band is the tab row art); there is NO per-cell blank-cameo SHP.
- **Cameo cell overlays** (strip draw `0x6A8BD0`, all relative to cell x,y):
  cameo SHP frame 0 at (x, y); 60×48 translucent shade fill (surface vtbl
  +0x54) over non-current/disabled cells; `DARKEN.SHP` at (x, y) flags 0x401
  when unbuildable / 0x404 flashing; `gclock2.shp` frame `stage+1` at (x, y)
  flags 0x404 while producing; progress/status text at (x, y+36); quantity
  count right-aligned at (x+60, y+1) text flag 0x242; hold text centered at
  (x+30, y+1) flag 0x142.
- **Power bar (S3)**: pip column x = **5** bar-relative (0 for Soviet/Yuri),
  first pip y = `DAT_00885F14 + 0x45` = **227** (= strip top), pitch **+3 px
  downward**, capacity = `(strip_h + 3)/3` (= 101 pips at 800×600). Drawn
  top→bottom: empty pips (frame 0), one flash pip (frame 4, blink gate
  `(tick & 1)==0`) at the empty/used boundary while draining, then green
  (frame 1), yellow (frame 2), red (frame 3) at the bottom; counts per §8.
  There is no separate drain indicator besides the frame-4 flash pip.
- **Credits tally (S4)**: TabClass::Draw_It blits `credits.shp` at (0, 0)
  then `FUN_004A2280` prints the amount with format `"%d"` at bar-relative
  (84, 2) — x = bar_width/2 — with text style `0x4108` (centered with shadow,
  by observed behavior; exact flag decode [RE-FALLBACK: visual]); color from
  the UI palette entry pair `DAT_00B0E98C/D`. Observer mode prints a mission
  timer instead. (We keep the §11 digit-strip divergence — only position
  matters: centered on the credits strip, 2 px from the top.)
- **S5 — assets our engine hasn't loaded yet** (all verified present in
  `ra2.mix/sidec01.mix` via ra2extract): `radar.shp` 168×110×33 (radar
  frame/boot anim/logo placeholder — REQUIRED for the radar area),
  `side3.shp` 168×26×1 (bottom cap), `addon.shp` 168×63×1 (bottom tail),
  `side2b.shp` 168×50×1 (shell-only, optional), `r-up.shp`/`r-dn.shp`
  46×25×3 (strip scroll buttons), `repair.shp`/`sell.shp` 64×31×2,
  `optbtn.shp`/`diplobtn.shp` 72×18×2 (top-cap buttons, optional),
  `sidebttn.shp` 125×25×3 (shell/top-bar variant, optional),
  `lendcap.shp` 28×32×3 / `rendcap.shp` 28×32×1 (bottom message-bar caps,
  optional). Soviet = sidec02.mix, Yuri = sidec03.mix same names (+
  `radary.shp`).

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
| §11a exact (x, y) of every chrome piece, tab pitch 29, cameo origin (22, 228) + pitches 63/50, row-count formula, scroll-button rects | literal constants in `FUN_006A4720`/`FUN_006A47C0`/`FUN_006A49A0`/`FUN_006AB3C0`/`FUN_00652730` + shell rect builder `FUN_0072ED40`; agree with pixel-measured socket rects in side1/side2 art | **High** |
| §11a radar area = radar.shp 168×110 at (0, 48); frame 0 = pre-radar logo; minimap viewport 140×108 at (16, 49) | `RadarClass::One_Time`/`Init_For_House`/`Draw_It` literals + radar.shp dims | **High** |
| §11a which of optbtn/diplobtn is left vs right | gadget geometry certain; name→gadget binding via register args not dumped | **Medium** (geometry High) |
| §11a credits tally centered at (84, 2), format `"%d"` | `FUN_004A2280` decompile | **High** (style-flag bit meanings: Medium) |
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
- **§11/§11a layout (Tasks 6/7e)** — `SidebarLayout::compute`
  (engine/sidebar.cpp) must be rewritten to the recovered constants
  (bar-relative x, y from window top; `bx = view_w − 168`):
  - `lay.bar = {bx, 0, 168, view_h}` (unchanged).
  - `lay.credits = {bx, 0, 168, 16}` — credits strip is the TOPMOST piece
    (currently placed below the radar: wrong). Tally text centered at
    (bx+84, 2).
  - top.shp cap at `{bx, 16, 168, 32}` — it is a bevel between credits and
    radar, NOT the radar frame (our current `lay.radar = top.shp @ y0` is the
    visible bug: the real radar area was missing entirely).
  - `lay.radar = {bx, 48, 168, 110}` — blit `radar.shp` frame 0 (side logo)
    until a radar building exists; minimap renders inside `{bx+16, 49, 140,
    108}`. **New asset to load: sidec01.mix/radar.shp** (168×110×33).
  - side1.shp at `{bx, 158, 168, 69}`; repair gadget `{bx+20, 166, 64, 31}`,
    sell `{bx+84, 166, 64, 31}` (art repair.shp/sell.shp, optional this
    phase).
  - `lay.tabs[i] = {bx + 26 + 29*i, 197, 28, 27}` — tab row x-pitch is
    **29** (NOT `bar.w/4` = 42, and not centered per-slot), y = 197.
  - cameo strip: `rows = (view_h − 260) / 50` (= `(view_h − 234 −
    side3_h(26))/50`, 6 at 600); side2.shp tiled at `{bx, 227 + 50*k, 168,
    50}`; **cameo cell (col, row) = `{bx + 22 + 63*col, 228 + 50*row, 60,
    48}`** — column pitch 63 (NOT cameo_w+1 = 61), row pitch 50 (NOT 49),
    origin (22, 228) (NOT power_w+2 = 14, tabs_bottom+2). These land the
    icons exactly in the sockets baked into side2.shp.
  - side3.shp at `{bx, 227 + 50*rows, 168, 26}`, then addon.shp right below
    (clips off-screen; both cosmetic but cheap — load from sidec01.mix).
  - scroll buttons: r-dn.shp gadget `{bx+39, 227 + 50*rows + 7, 46, 25}`,
    r-up.shp at `{bx+85, same y}` (down-arrow left, up-arrow right — binary
    order). Scrolling moves whole rows.
  - `lay.power`: pip column at `{bx+5, 227}`, pip = powerp.shp 12×2, pitch
    3 px downward, `total_pips = (50*rows + 3)/3` — replaces both the
    "below tabs, full height" rect and §11's "≈0x45 below sidebar-top"
    approximation (0x45 = 69 = side1_h exactly; pips start at the strip top).
  - `SidebarArtDims` additions: `radar_h = 110`, `side3_h = 26`,
    `tab_pitch = 29`, `cameo_origin = (22, 228−227=1 within strip)`,
    `col_pitch = 63`, `row_pitch = 50`, `power_x = 5`, `scroll = 46×25`.
    (Soviet/Yuri sidec02/03 variants differ slightly — tab pitch 32, col
    pitch 64, pip x 0, side3_h 18 — not needed while we ship Allied chrome.)
  - Build clock unchanged: `gclock2.shp` frame `1 + stage` overlay; DARKEN
    for unbuildable; quantity right-aligned at cell x+60, y+1.
- **§11 credits digits (Task 6)**: the game's own tally font is not trivially
  extractable (GAME.FNT format decode is real work and unproven for this
  string) — **use the built-in 5×7 digit strip** (documented divergence), as
  the plan's default. The rolling-tally animation is display-only and stands.
