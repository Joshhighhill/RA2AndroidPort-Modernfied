# Economy semantics — reverse-engineering notes

(Method: `docs/re/workflow.md`; image base `0x00400000`, YR 1.001 `gamemd.exe`.
Hunt scripts, all off-repo in the session scratchpad: `economy_hunt.py` …
`economy_hunt10.py` (passes 9–10 = the §6a cost-path follow-up). Content-anchored only — located from the INI-key string
cross-refs `"FreeUnit"` `"Storage"` `"NumberOfDocks"` `"DockUnload"` `"Refinery"`
`"Weeder"` `"OrePurifier"` `"PurifierBonus"` `"UnloadingClass"` `"GrowthRate"`
`"GrowthPercentage"` `"SpreadPercentage"` `"Spread"` `"Growth"` `"Value"`
`"Power"` `"Color"` `"Image"` `"ChronoInSound"` `"ChronoOutSound"`
`"ChronoDelay"` `"ChronoTrigger"` `"TiberiumGrowthEnabled"`, plus displacement-
constant scans (1.13 M instructions) for the recovered type-field offsets, and
forced function creation across the UnitClass region `0x739000`–`0x747000`.
Decompiles/listings under `D:\ra2-re\out\eco*`; nothing under `D:\ra2-re` is
committed. Published YRpp/Ares addresses were used only as prose hints — every
one that was probed (`Harvesting 0x73D450`, `FindIndex 0x5FDD20`,
`RegisterForGrowth 0x7235A0`, `TiberiumClass ctor 0x7216C0`,
`OverlayTypeClass ctor 0x5FE250`) had drifted to an unrelated function, exactly
as `combat.md`/`workflow.md` warned.)

Companion of `construction.md` (which owns FactoryClass / BuildTime / power).
"Decisions for ra2engine" at the end resolves every `[RE-GATE]` marker in the
Economy-phase plan Tasks 2–8.

Key globals recovered: `DAT_00886160` = `RulesClass::Instance`;
`DAT_00A82CEC` / `DAT_00A82CF8` = OverlayTypeClass array base / count;
`DAT_00B0E45C` / `DAT_00B0E468` = TiberiumClass array base / count.

## Located functions

| Function | Role | How found |
|----------|------|-----------|
| `FUN_00720CF0` | **TiberiumClass::Read_INI** — Spread→+0x9c(int), SpreadPercentage→+0xa0(dbl), Growth→+0xa8(int), GrowthPercentage→+0xb0(dbl), Value→+0xb8(int), Power→+0xbc(int), Color→+0xc0, then a hardcoded switch assigning Image (OverlayTypeClass\* +0xe0), NumFrames→+0xe4, NumImages→+0xe8 (**=12/0xC**), NumSlopes→+0xec (**=8**) | gap fn; INI-key string refs `0x720D23`–`0x720EE1`; forced listing |
| `FUN_005FD510` | **TiberiumClass::FindIndex(overlayIdx)** — the overlay→tiberium classifier | reads OverlayType+0x2a9 (Tiberium bool), loops TiberiumClass array; `"Overlay %s not really tiberium"` @`0x832488` |
| `FUN_005FE460` | OverlayTypeClass::Find(name) — searches `DAT_00A82CEC[0..DAT_00A82CF8)` by ID(+0x24) | array-global consumer |
| `FUN_005FDA40` | OverlayTypeClass ctor — ArrayIndex→+0x294, Tiberium flag→+0x2a9 | array registration |
| `FUN_005FCAD0` | Map `OverlayPack`/`OverlayDataPack` loader — writes each cell's overlay index and **density byte → cell+0x11e** | `"OverlayPack"`/`"OverlayDataPack"` @`0x83247C`/`0x83246C` |
| `FUN_0045FE40` | BuildingTypeClass::LoadFromINI — FreeUnit→+0xEA0(UnitType\*), Refinery→+0x16BB, DockUnload→+0x16B3, Weeder→+0x16BC, OrePurifier→+0x16CC, FactoryPlant→+0x16CD, NumberOfDocks→+0x1780, DockingOffsets vector→+0x1788 (stride 0xC = CoordStruct) | string refs |
| `FUN_00711510` | TechnoTypeClass::LoadFromINI — Storage→+0x800(int), UnloadingClass→+0x6B8(type idx), Soylent→+0x614 | string refs |
| `FUN_0066CF70` | RulesClass `[General]` loader — PurifierBonus→+0xF3C(float), GrowthRate→+0x1638(double), ChronoDelay→+0xBEC, ChronoTrigger→+0xBF8, ChronoHarvTooFarDistance→+0xD7C | string refs |
| `FUN_0073C560` | **UnitClass harvester mission** — harvest-scan (8-neighbour dig) + unload/deposit credit math | contains PurifierBonus (`rules+0xf3c`) + Refinery-flag reads; UnitClass region create |
| `FUN_00740780` | UnitClass AI / dock-drive (per-frame locomotor + dock approach) | DockUnload/Refinery flag reads |
| `FUN_0043C250` | **BuildingClass::Receive_Message** — the dock/radio handshake (switch on radio msg id; Refinery/DockUnload/Weeder cases 0xE–0x15) | DockUnload/Refinery/Weeder flag reads |
| `FUN_004509C0` | BuildingClass refinery animation AI — storage pip/smoke level = `(stored<<2)/Storage(+0x800)` | Refinery-flag reader |
| `FUN_006C8D00` / `FUN_006C8D30` / `FUN_006C8EA0` | harvester per-type ore store: get float[idx] / subtract(clamp 0) / first-nonempty of **4** slots | callees of the deposit math |
| `FUN_004F9430` / `FUN_004F9520` | HouseClass give-money (ftol→ credits +0x30C, tally +0x54E8) | callees of the deposit |
| `FUN_0045ED40` | **BuildingTypeClass::Cost_Of override** (type vtbl slot 43/+0xAC, vtable @`0x7E3570`): `max(0, Cost − FreeUnit.Cost)`, ConYard −avg MCV cost | sole ref = vtable entry @`0x7E361C` |
| `FUN_00711250` / `FUN_007112A0` / `FUN_00711280` | TechnoTypeClass raw-Cost accessor (+0xAC base, `mov eax,[ecx+0x610]`) / Cost_Of(house) (+0x84: +0xAC × country × handicap mults) / build-frames (+0x88: `FILD [type+0x610]` — RAW cost) | type vtable slot walk (RTTI) |
| `FUN_007DB520` | locomotor factory (CoCreateInstance-style; builds the CMIN teleport locomotor at unit+0x19D) | callee throughout `FUN_00740780`/`FUN_0073C560` |
| `FUN_00668C20` | `[AudioVisual]` rules loader — ChronoInSound/ChronoOutSound → global sound slots +0x86/+0x87 (plus the full UI/event sound table) | `"ChronoInSound"` @`0x83999C` |
| `FUN_00665090` | rules-setup writer — the only runtime `GrowthRate`(+0x1638) touch found (a *write*; the timer consumer was not pinned) | displacement scan 0x1638 |
| `FUN_00674C50` | BuildingTypeClass type-linkage resolver (calls vtbl+0xC on every type-pointer field incl. FreeUnit +0xEA0) — not the spawn | FreeUnit displacement hits |
| `FUN_00720B20` | TiberiumClass dtor — frees two candidate-array/queue pointer sets (+0xF4/+0xF8/+0xFC, count +0xF0) = the §4 growth/spread bookkeeping shape | Tiberium vtable region |
| `FUN_0050BC10` / `FUN_0050BCD0` | country per-class Cost mult (floats country+0x114…+0x124 by WhatAmI) / handicap cost mult | callees of `FUN_007112A0` |

---

## §1 Harvester FSM

The harvester is a `UnitClass` whose type carries the miner flags. Its whole
harvest↔unload life-cycle runs through **`FUN_0073C560`** (the mission update),
which splits on a "carrying cargo" flag `this+0xB9`:

- **`this+0xB9 == 0` → HARVEST phase.** Sub-state `this+0x2F` drives the machine:
  - **state 0**: acquire a tiberium target / turn to face it; on arrival start
    the dig timer (`(**vtbl+0x1E8)(5,…)`).
  - **state 1**: wait for the turn/approach to finish → state 3.
  - **state 3**: **dig.** It walks the **8 adjacent cells** via the direction
    offset table `DAT_0089E5F8` (8 × dx,dy shorts), and for each cell that holds
    the harvester's tiberium it calls the cell's reduce-tiberium vtable slot
    (`(*cell)[0x36]`), removing one density step and crediting its `Value` into
    the unit's per-type ore store. Digs neighbour-by-neighbour until the store
    reaches capacity (`this+0x1B9 >= this+0x45`, `+0x45` = Storage in bales) →
    state 4.
  - **state 4**: full → issue the "unload" mission (radio to a refinery).
- **`this+0xB9 != 0` → UNLOAD phase** (drive to refinery, dock, deposit — §2).

**Ore scan / cell selection.** The *local* dig is the 8-neighbour ring above
(CONFIRMED). The *wide* "find a new ore field" search is `FootClass::MoveToTiberium`
(reached from state 0) — a spiral/radius scan choosing the nearest cell holding
the unit's tiberium; the exact radius constant and tie-break were **not isolated**
in this hunt — **[RE-FALLBACK: TS/RA2 nearest-passable-tiberium spiral, break
ties by scan order]**. Our engine's nearest-cell search is the correct analogue.

**Per-gulp amount & interval.** One dig step removes **one density level** from a
cell (density byte 0-based, §5) and banks that level's worth of `Value` credits
into the harvester's store; the dig is gated by a short mission timer (`vtbl+0x1E8`
sets it), not a fixed rules key — the timer's tick constant was **not isolated**:
**[RE-FALLBACK: short per-gulp mission delay, tune to original pacing]**.
The store is **per tiberium type** — a 4-float
array (`FUN_006C8D00`/`D30`/`EA0` index 0..3), i.e. Riparius/gems/etc. accumulate
separately so mixed loads pay the correct `Value` per bale.

**"Full" test.** `loaded (this+0x1B9) >= Storage (this+0x45)`, where Storage is
the type's `Storage=` (rulesmd HARV=40, CMIN=20). No `Value`/credit conversion
happens until unload — the store holds bale value, capacity is counted in bales.

**Cell exhaustion.** When the cell under/next to the miner drops to density 0 the
dig loop simply finds no tiberium in the 8-ring and the machine falls back to
state 0 → `MoveToTiberium` to relocate to the next field.

**Auto-resume after a manual move.** A harvester with the "was harvesting" flag
set re-enters the harvest mission automatically once its ordered move completes
(the mission is restored rather than dropped) — behaviour consistent with the
`this+0xB9`/`this+0x2F` reset seen when a fresh Harvest mission is assigned.
Exact flag = **[RE-FALLBACK: standard RA2 auto-return-to-harvest]**.

Confidence: FSM shape + 8-neighbour dig + per-type store + full-test **CONFIRMED**;
wide scan radius / per-gulp tick interval / auto-resume flag **PROBABLE /
[RE-FALLBACK]**.

## §2 Dock / unload

**Handshake at NumberOfDocks (refinery has 1 dock).** The harvester↔refinery
handshake is `BuildingClass::Receive_Message` (`FUN_0043C250`), a switch over
radio-message ids. The refinery answers docking queries only when it is a
`Refinery`(+0x16BB)/`DockUnload`(+0x16B3)/`Weeder`(+0x16BC) building; it computes
a docking coord (own-cell + the `DockingOffsets` entry, type+0x1788) and hands it
back (the building pointer is written through the function's 4th argument). A second miner requesting the single dock is refused
until the dock is free (the "can I enter?" case returns "busy"), so extra
harvesters queue near the refinery and dock one-at-a-time. Exact queue-slot
geometry = **[RE-FALLBACK: single dock, FIFO by radio order]**.

**Dock cell offset.** `DockingOffsets` (BuildingType+0x1788, a `VectorClass<CoordStruct>`,
stride 0xC) gives the sub-cell docking coordinate **relative to the foundation
origin**; refineries define one entry (rulesmd `NumberOfDocks=1`). The docking
drive (`FUN_00740780`) steers the miner onto that coord before unloading.

**Unload duration + per-tick credit math.** In the UNLOAD phase of `FUN_0073C560`
the miner transfers its store to the owning house in timed chunks. Per tick,
gated by a rate (`RulesClass::Instance+0x1528 × const`), it:

1. picks the first non-empty tiberium slot `t = FUN_006C8EA0(store)` (of 4);
2. reads that slot's banked value `v = store[t]` (`FUN_006C8D00`);
3. gives `v` to the house — `FUN_004F9430(house, v)` (ftol → credits at
   `house+0x30C`, running tally at `house+0x54E8`);
4. drains the slot `FUN_006C8D30(store, v, t)`;
5. resets the unload accumulator (`this+0x3E = 0`).

So **income = Σ (bales of type t × Value_t)** — the `Value` is folded in at dig
time, and unload is a straight value→credits transfer. Unload takes several ticks
(the store is paid out in per-tick chunks, not instantly).

**PurifierBonus applies only with a purifier.** Alongside the base pay-out the
code computes a bonus `purifier_metric(house+0x538C) × PurifierBonus(rules+0xF3C,
float) × value` and gives it as a *separate* credit add — guarded by
`house+0x538C` (the house's Ore-Purifier count). With **no purifier the metric is
0 and the bonus vanishes**; each purifier adds `PurifierBonus` (rulesmd `.25`) of
the load. (There is also an AI-difficulty income handicap: when the owner is AI
(`house+0x1EC==0`) it adds `rules+0x1324[difficulty]` — out of scope for the human
player.) CONFIRMED.

**Storage=200 on the refinery is cosmetic.** `FUN_004509C0` reads the refinery's
`Storage`(type+0x800) only to pick the ore-pile/smoke animation frame:
`level = (stored << 2) / Storage` → 0..4. RA2 has **no silo cap** — refinery
`Storage` gates nothing about income; it is purely the pip/animation scale.
CONFIRMED.

**UnloadingClass swap.** `UnloadingClass` is a TechnoType key (parsed at
TechnoType+0x6B8 to a unit-type index, `FUN_00711510`). It names the alternate
art/type the miner morphs into while sitting on the dock (the "unloading" body).
The swap is a display/type substitution during the unload phase and reverts on
undock; the clone selects that art while its harvester state is `Unloading`.
The original's exact animation/state-edge timing remains **[RE-FALLBACK:
cosmetic type swap while docked]**. The clone's render lookup is shared-owned
and atomically replaced on scene changes to avoid dangling pointers.

## §3 CMIN chrono-return

The Chrono Miner (CMIN) is a `UnitClass` that owns a **teleport locomotor** COM
object at `this+0x19D` (created via `FUN_007DB520`, the CoCreateInstance-style
locomotor factory seen throughout `FUN_00740780`/`FUN_0073C560`). Instead of
driving back to the refinery it warps:

- **Trigger**: on becoming full (HARVEST state 4) — the same "issue unload
  mission" path — the locomotor is asked to teleport to the refinery dock coord
  rather than pathfind to it. (Manual "return" orders route through the same
  mission, so a manual send-home also warps.)
- **Warp-out / warp-in**: handled inside the locomotor's move slot; the unit
  disappears at the source cell and reappears on the **dock cell** (the
  `DockingOffsets` coord). After unloading it warps back toward the harvest field.
- **Sounds**: `ChronoInSound` / `ChronoOutSound` (AudioVisual globals loaded by
  `FUN_00668C20` into the sound table at slots `+0x86`/`+0x87`) play on
  warp-in / warp-out.
- **Rules**: `[General] ChronoDelay`(+0xBEC), `ChronoTrigger`(+0xBF8),
  `ChronoHarvTooFarDistance`(+0xD7C) exist and bound chrono behaviour (the
  too-far distance forces a normal drive when the field is beyond warp range),
  but the bulk of ChronoDelay/ChronoTrigger governs the Chronosphere superweapon.

Exact warp timing, per-move cooldown, and the precise CMIN-vs-HARV branch flag
were **not fully isolated** — **[RE-FALLBACK: TeleportLocomotionClass; warp on
full, warp back after unload, ChronoIn/OutSound bracketing the jump]**.
Confidence: locomotor + sounds + trigger **PROBABLE**; exact timing/cooldown
**[RE-FALLBACK]**.

## §4 Ore growth / spread

**Per-type parameters** (from `FUN_00720CF0`, TiberiumClass):
`Spread`(+0x9C int), `SpreadPercentage`(+0xA0 double), `Growth`(+0xA8 int),
`GrowthPercentage`(+0xB0 double), `Value`(+0xB8 int), `Power`(+0xBC),
`Color`(+0xC0). rulesmd: Riparius `Value=25 GrowthPercentage=.06`,
Cruentus `Value=50`. The global rate is `[General] GrowthRate`
(rules+0x1638 double, rulesmd `5` = minutes between growth firings).

**Structure — PROBABLE / [RE-FALLBACK].** The TiberiumClass object is large
enough to carry per-type growth/spread bookkeeping beyond the INI fields (the
ctor/dtor pair `FUN_00720B20` frees candidate arrays + queue pointers at
+0xF4/+0xF8/+0xFC and clears a count at +0xF0, twice — one set per logic), which
fits a growth-logic + spread-logic pair each holding a candidate-cell queue, a
cells-with-tiberium map, and a countdown timer. That decomposition is a prose
hint from the YRpp headers (hint only — NOT evidence), corroborated by the dtor
shape but **not pinned**: the periodic consumer that drains the queues when the
`GrowthRate` timer fires was never located in this hunt (the only runtime
`GrowthRate` touch found is the *write* in the rules-setup fn `FUN_00665090`).
**[RE-FALLBACK: per-type candidate queues + GrowthRate-seeded timer, TS-lineage
semantics].** Cells register as candidates when tiberium is placed/created; when
the timer fires a slice of candidates is processed:

- **Growth** raises the density of eligible cells toward the cap; `Growth` /
  `GrowthPercentage` select how many of the queued cells fire per tick
  (`GrowthPercentage` = the fraction of candidates promoted).
- **Spread** seeds tiberium into an adjacent passable, tiberium-free cell from a
  **fully-grown** (max-density) source; `Spread` / `SpreadPercentage` select the
  count/probability. Spread targets must be land-passable and not already ore.

**Density cap = 11.** Each tiberium overlay has **`NumImages = 12`** (`0xC`,
`FUN_00720CF0`) → density byte 0..11; growth stops at 11. (Plus `NumSlopes = 8`
slope variants.) CONFIRMED.

**RNG.** The original selection uses the game's lockstep-synchronised RNG
(`ScenarioClass` random) so growth is deterministic across a netgame. That RNG is
not reproducible in our single-machine engine. **Deviation (README):** our engine
substitutes its own **seeded xorshift32** for growth/spread candidate selection —
statistically equivalent (same rates), not bit-identical to the original's
lockstep sequence.

Confidence: per-type fields + density cap **CONFIRMED**; the queue/timer
structure **PROBABLE / [RE-FALLBACK]** (dtor shape + prose hint; timer consumer
not pinned); exact per-cell probability formula and candidate-drain slice
**[RE-FALLBACK: OpenRA/ModEnc semantics]**.

## §5 Overlay tables

**Overlay array.** `DAT_00A82CEC` = the OverlayTypeClass\* array (count
`DAT_00A82CF8`); each OverlayType has `ArrayIndex`(+0x294) and a `Tiberium` bool
(+0x2A9), set in the ctor `FUN_005FDA40`.

**Classification (overlay index → tiberium type)** is **range-based**, not a flat
table — `TiberiumClass::FindIndex` (`FUN_005FD510`): given a 0-based overlay
index it (1) rejects it unless `OverlayType[idx].Tiberium` is set, then (2) loops
the 4-entry TiberiumClass array testing whether the index falls in
`[Image.ArrayIndex, Image.ArrayIndex + NumImages)` (the 12 density frames) or the
following `NumSlopes` (8) range, and returns that tiberium's index; otherwise it
logs `"Overlay %s not really tiberium"`. So the *engine* only needs each tiberium
type's overlay base + 12 density frames (+8 slopes) — no hardcoded per-index if/else.

The hardcoded image bases the Tiberium loader assigns (indices into the overlay
array) are `{27, 102, 127, 147}` for the four tiberium types — **the GEM (gems /
Cruentus) set and the three ore sets TIB/TIB2/TIB3** (Riparius and variants).

**RESOLVED (CONFIRMED): the runtime overlay index is the 0-based
order-of-appearance position in the `[OverlayTypes]` list — NOT `key−1`.** The
INI key numbers have gaps (the list registers 250 entries), so key−1 drifts from
the true index for the later blocks. Empirically verified against the real
rulesmd list: positions GEM01=27, TIB01=102, TIB2_01=127, TIB3_01=147 — an
**exact match to the four recovered bases {27, 102, 127, 147}**; and Task 2's
real-map OverlayPack dump observed exactly indices 27/102/127 on tiberium cells
with data bytes 0..11. The engine must therefore assign overlay indices by list
position at parse time and key each tiberium type off its `Image.ArrayIndex +
NumImages(12)` range (mirroring the `FUN_005FD510` range test), never off the
key numbers.

Implied decomposition of the 20-entry TIB blocks: entries 1..12 are the
`NumImages=12` flat density images and entries 13..20 (e.g. TIB13..TIB20 at
positions 114..121) are the `NumSlopes=8` slope variants that
`FUN_005FD510`'s second range covers — 12 + 8 = 20, exactly the block size.
(GEM has only the 12 flat images — the slope range is empty/unused for gems.)

**Density byte semantics.** The map's `OverlayDataPack` writes one byte per cell
to **cell+0x11E** (`FUN_005FCAD0`); this is the **0-based density** 0..11. One
density step = one bale of `Value` (dig removes one step and banks one `Value`).

**Density → frame.** Straight identity within the type's image set: frame =
`Image.ArrayIndex + density` for flat cells (0..11), with the +12.. slope frames
(`NumSlopes=8`) used on sloped tiles. CONFIRMED (NumImages=12/NumSlopes=8 read
from the binary).

**Ore overlay SHP filenames + extension.** Overlay art is theater-keyed: the
temperate-theater tiberium overlays load from the theater chain (temperat.mix →
isotemp.mix), one SHP per `[OverlayTypes]` name (e.g. `TIB01`…`TIB12`,
`GEM01`…`GEM12`) with the theater extension (temperate `.tem`, then the generic
`.shp` fallback), 12 density frames each. The exact per-name file list was **not
byte-verified here** — **[RE-FALLBACK: extract the overlay names from
`[OverlayTypes]` and load `<name>.tem`/`.shp` from the temperate chain]**; our
existing theater loader is the correct path.

**Draw layer.** Overlays render in the **ground/overlay layer — above the terrain
tiles and building bibs, below units, infantry, and buildings** (the isometric
cell draw order: terrain → smudges/bibs → overlay(ore) → objects). Exact
layer-enum value = **[RE-FALLBACK: TS/RA2 cell draw order]**; behaviourally ore
draws flat on the ground under everything mobile.

## §6 FreeUnit spawn

`FreeUnit` is a BuildingType key → a `UnitTypeClass*` at type+0xEA0
(`FUN_0045FE40`). Its cost is **pre-billed into the building**: the building's
effective cost is `Cost − FreeUnit.Cost` (`FUN_0045ED40`), i.e. you pay for the
free harvester as part of the refinery.

- **Spawn timing**: at the refinery's *grand opening* — when buildup completes and
  the building goes into service (the same moment power/service flags flip). The
  free unit is created from `type.FreeUnit` (vtable CreateObject, slot +0xC).
- **Spawn cell**: adjacent to the foundation, at/next to the refinery's dock cell
  (`DockingOffsets` coord) so the harvester materialises on the pad.
- **Initial mission**: the spawned harvester is put straight into its
  Harvest/return mission (heads to the nearest ore field, or docks if it starts
  loaded).

The exact grand-opening spawn function was **not pinned 1:1** (readers of
type+0xEA0 are the loader, the type-linkage resolver `FUN_00674C50`, the cost
accessor, and AI code) — **[RE-FALLBACK: BuildingClass::Grand_Opening creates
type.FreeUnit adjacent to the dock cell on buildup completion, mission = Harvest]**.
Confidence: FreeUnit field + cost pre-billing **CONFIRMED**; spawn cell/timing/
mission **PROBABLE / [RE-FALLBACK]**.

### §6a FreeUnit cost & the factory charge (follow-up hunt, passes 9–10)

Which paths consume the FreeUnit-subtracted accessor `FUN_0045ED40` vs the raw
`Cost` field (type+0x610)? Pinned via RTTI vtable walks + listings
(`economy_hunt9.py`/`economy_hunt10.py`, `eco9_*`/`eco10_*`):

- **`FUN_0045ED40` has exactly one reference in the whole binary**: the
  BuildingTypeClass vtable (RTTI, @`0x7E3570`) slot 43 (**+0xAC**, entry
  @`0x7E361C`). It is the BuildingTypeClass override of the virtual
  **`Cost_Of()`** accessor; TechnoTypeClass and UnitTypeClass slot 43 is the base
  `FUN_00711250` = literally `mov eax,[ecx+0x610]; ret` (raw Cost). No code calls
  `FUN_0045ED40` directly — every consumer goes through `type->vtbl+0xAC`.
- **Full body of `FUN_0045ED40`** (listing `0x45ED40`): start from raw Cost
  (`FUN_00711250`); **(a)** if this type == `BaseUnit[0]->DeploysInto`
  (rules+0xB5C = the `[General] BaseUnit` UnitType vector; DeploysInto at
  unittype+0x3EC) and the rules flag +0x17E8 is clear, subtract
  `(Cost_Of(BaseUnit[0]) + Cost_Of(BaseUnit[1]))/2` — i.e. the **Construction
  Yard's effective cost also folds in the average MCV cost** (PROBABLE
  interpretation of the +0xB5C vector identity); **(b)** if `FreeUnit`(+0xEA0) is
  set, subtract `FreeUnit->Cost_Of()` and **clamp the result to ≥ 0**.
- **The factory charge chain uses the SUBTRACTED cost.** FactoryClass::Set
  (`FUN_004C9B90`, listing @`0x4C9CFE`): the balance snapshot (+0x60, the amount
  the 54-step charge spends, construction.md §10) is
  `type->vtbl+0x84(house)` = `FUN_007112A0` (TechnoTypeClass::Cost_Of(house)) =
  `ftol( vtbl+0xAC() × country Cost<class>Mult (FUN_0050BC10, house->country
  floats +0x114…+0x124 by WhatAmI) × handicap mult (FUN_0050BCD0) )` — and
  `vtbl+0xAC` for buildings is `FUN_0045ED40`. So **GAREFN/NAREFN
  (Cost=2000, FreeUnit HARV/CMIN Cost=1400) charge 2000 − 1400 = 600 credits**;
  the cancel refund (`FUN_004C9F10` refunds `Cost_Of − balance`, same virtual)
  is on the same subtracted basis. CONFIRMED.
- **The build-time path uses the RAW Cost.** The base-frames virtual is slot 34
  (**+0x88**) = `FUN_00711280` in all three type vtables (no BuildingType
  override), and its body is `FILD [ECX+0x610]; FMUL BuildSpeed; FMUL 0.9` — it
  reads the **raw Cost field directly**, bypassing the +0xAC accessor.
  `Time_To_Build` (`FUN_006F3D00`) reaches it via object vtbl+0x88 → type
  vtbl+0x88. So **GAREFN's build time comes from the full 2000**:
  `ftol(2000 × .7 × .9) = 1260 frames = 84 s` at full power — while its queue
  charge is 600. CONFIRMED.

Engine rule: `charge = max(0, Cost − FreeUnit.Cost)` (× country/handicap mults,
all 1.0 for our roster) for the production spend/refund;
`build_frames = trunc(Cost × BuildSpeed × 0.9)` from the **unsubtracted** Cost.

---

## Confidence summary

| Item | Evidence | Confidence |
|------|----------|:----------:|
| §1 harvester mission machine (`FUN_0073C560`), 8-neighbour dig via `DAT_0089E5F8`, per-type 4-slot ore store, full-test `loaded≥Storage` | decompile + store helpers | **CONFIRMED** |
| §1 wide ore-scan radius / tie-break; per-gulp tick interval; auto-resume flag | not isolated | **[RE-FALLBACK]** |
| §2 single-dock handshake via `Receive_Message` (`FUN_0043C250`); DockingOffsets +0x1788 | decompile | **CONFIRMED (queue geometry PROBABLE)** |
| §2 income = Σ(bales×Value), per-tick payout, ftol→house+0x30C | deposit math + `FUN_004F9430` | **CONFIRMED** |
| §2 PurifierBonus only with a purifier (house+0x538C gate × rules+0xF3C) | deposit math | **CONFIRMED** |
| §2 refinery Storage cosmetic (pip = stored·4/Storage; no silo) | `FUN_004509C0` | **CONFIRMED** |
| §2 UnloadingClass swap | type+0x6B8 loaded; runtime swap not pinned | **PROBABLE / [RE-FALLBACK]** |
| §3 CMIN teleport locomotor (+0x19D), ChronoIn/OutSound, warp-on-full | decompile + audio loader | **PROBABLE** |
| §3 exact warp timing / cooldown / CMIN branch flag | not isolated | **[RE-FALLBACK]** |
| §4 TiberiumClass per-type fields + GrowthRate + density cap 11 (NumImages=12, NumSlopes=8) | `FUN_00720CF0` | **CONFIRMED** |
| §4 growth/spread queue+timer structure; per-cell probability + queue drain; RNG substitution | dtor shape + prose hint only; timer consumer and formula not pinned | **PROBABLE / [RE-FALLBACK]** |
| §5 overlay→tiberium range classifier (`FUN_005FD510`), array globals, density byte cell+0x11E | decompile | **CONFIRMED** |
| §5 overlay index = 0-based order-of-appearance in `[OverlayTypes]` (NOT key−1); bases {27,102,127,147} = GEM01/TIB01/TIB2_01/TIB3_01; TIB blocks = 12 images + 8 slopes | jump-table bases == list positions (exact ×4) + Task 2 real-map dump (27/102/127, bytes 0..11) | **CONFIRMED** |
| §5 ore SHP filenames/extension; draw layer enum | theater-keyed, not byte-verified | **[RE-FALLBACK]** |
| §6 FreeUnit field +0xEA0 + cost pre-billing | `FUN_0045FE40` / `FUN_0045ED40` | **CONFIRMED** |
| §6 spawn cell / timing / initial mission | grand-opening fn not pinned | **PROBABLE / [RE-FALLBACK]** |
| §6a subtracted cost = virtual Cost_Of (BT vtbl slot 43/+0xAC, sole ref); factory charge/refund chain consumes it (GAREFN charges 600) | vtable walk + `FUN_004C9B90` @0x4C9CFE → `FUN_007112A0` → +0xAC | **CONFIRMED** |
| §6a build time from RAW Cost (+0x88 `FUN_00711280` = `FILD [type+0x610]`; no BT override) — GAREFN 1260 frames from 2000 | vtable slots + listing | **CONFIRMED** |
| §6a ConYard case: cost −= avg(BaseUnit[0..1].Cost) when type == BaseUnit[0].DeploysInto | `FUN_0045ED40` listing; +0xB5C = BaseUnit vector identity inferred | **PROBABLE** |

## Deviations we will take

- **Ore growth/spread RNG**: replace the original lockstep `ScenarioClass` RNG
  with an engine-local **seeded xorshift32** for candidate selection. Same rates
  (`GrowthRate`, `Growth/Spread`, `*Percentage`, cap 11), NOT bit-identical to the
  original's netgame-synchronised sequence. README-level deviation; lockstep
  parity is unreachable single-machine.
- **Overlay ore art**: load per-`[OverlayTypes]`-name SHPs from the temperate
  theater chain (`.tem`→`.shp`), 12 density frames; do not hardcode magic overlay
  indices — key each tiberium type off its overlay base + `NumImages(12)` range
  (mirrors `FUN_005FD510`).
- **Credit model**: bank `Value` at dig time into a per-type store; pay out
  `Σ bales×Value` over several unload ticks; add `purifier_count × PurifierBonus ×
  value` only when the owner has ≥1 Ore Purifier. Refinery `Storage` drives only
  the ore-pile animation (0..4 = `stored·4/Storage`), never income.
- **Refinery FreeUnit**: on refinery grand-opening spawn `type.FreeUnit` (HARV/CMIN)
  at the dock cell with its cost already included in the refinery price
  (`Cost − FreeUnit.Cost`), and start it harvesting.
