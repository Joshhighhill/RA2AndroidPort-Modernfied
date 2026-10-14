# Unit production semantics — reverse-engineering notes

(Method: `docs/re/workflow.md`; image base `0x00400000`, YR 1.001 `gamemd.exe`.
Hunt scripts, all off-repo in the session scratchpad: `prod_hunt.py` …
`prod_hunt5.py`; decompiles/listings under `D:\ra2-re\out\prod*`. Nothing under
`D:\ra2-re` is committed. Content-anchored only — located from the FactoryClass
anchors already pinned in `construction.md` §located-functions, from the INI-key
string cross-refs `"BuildCat"` `"ExitCoord"` `"BaseNormal"` `"IsBaseDefense"`
`"MultipleFactory"` and the EVA strings `"EVA_PrimaryBuildingSelected"`
`"EVA_NewRallyPointEstablished"`, and from the Ares hook-address list
(`KickOutUnit` dispatch `0x443CCA`/`0x444119`/`0x444131`/`0x44531F`,
`HouseClass::BeginProductionOf 0x4FA5A8`) used **only as prose hints** — every
address was verified by content (the AbstractType switch, the field offsets, the
INI-key string it references). Published YRpp/Ares absolute addresses drifted as
`workflow.md` §4 warns: `SetPrimaryFactory 0x500850` landed inside unrelated
house-init code, `StartProduction 0x4CA5A0`→`CountTotal`, `SetObject
0x4C9E10`→the rate-update fn; each was relocated by content. Per-unit numeric
constants below are read from the real `rulesmd.ini` extracted from
`ra2md.mix` with the repo's `ra2extract`.)

Companion of `construction.md` (which owns the FactoryClass 54-step charge model,
BuildTime math, power, prerequisite AND-semantics) and `economy.md` (FreeUnit /
Cost_Of). This doc resolves the `[RE-GATE]` markers in the Unit-Production plan
Tasks 3–7 and answers spec §1 items 1–6. **See "Deviations / plan diffs" — item 1
diverges from the plan's expected queue model and is a RE-GATE STOP.**

Key globals: `DAT_00886160` = `RulesClass::Instance` (`MultipleFactory` @+0x57C
= 0.8; `MaximumQueuedObjects` @+0xF0 = 29). `DAT_0089E5F8` = the 8-direction
`(dx,dy)` short table (shared with the harvester dig ring, `economy.md` §1).

## Located functions

| Function | Role | How found |
|----------|------|-----------|
| `FUN_004FA170` | **HouseClass::BeginProductionOf** — sidebar click → per-category primary FactoryClass pointer (create if null via `operator_new(0x74)`+`FUN_004C97D0` ctor), then `FactoryClass::Set`. The queue/tab router. | Ares hint `0x4FA5A8`; AbstractType switch; error strings `"Request to Begin Production of…"` |
| `FUN_00500510` | HouseClass per-category **byte-flag** getter (parallel to the pointer array): switch(AbsType){Air→+0x53D0, Inf→+0x53D1, Veh→+0x53D2, Ship→+0x53D3, Bld(cat≠5)→+0x53D4, Def(cat==5)→+0x53D8} | YRpp `GetPrimaryFactory 0x500510`; AbstractType switch |
| `FUN_00500730` | HouseClass **factory count per product class** (drives MultipleFactory): Air +0x5378, Inf +0x537C, Veh +0x5380, **Bld +0x5384 (single slot, NO struct/def split)**, Ship +0x5388 | construction.md; callee of Time_To_Build |
| `FUN_006F3D00` | TechnoClass::Time_To_Build — confirms the MultipleFactory loop: `if 0.0 < Rules+0x57C: n=FUN_00500730(class); repeat (n−1)× t=ftol(t×MultipleFactory)`; wall branch (`WhatAmI==6 && type+0x1571`) | construction.md |
| `FUN_004C9B90` | **FactoryClass::Set / DemandProduction** — BuildingType path abandons current first (`FUN_004C9F10`); else queue-append capped at `Rules+0xF0`; sets Object +0x58 (CreateObject vtbl+0x8C), balance +0x60 = `type->Cost_Of(house)` (vtbl+0x84) | construction.md; consumer confirmed |
| `FUN_004C9A40` / `FUN_004C9DC0` / `FUN_004C9F10` | FactoryClass AI per-step / rate update / Abandon+refund | construction.md |
| `FUN_00443BC0` | **BuildingClass::KickOutUnit** — emits a finished unit: building-center cell → facing → walk `DAT_0089E5F8` 8-ring via `FUN_0047C430` (cell-occupant test) to first free cell; apply type `ExitCoord` (+0xEC8/+0xECC/+0xED0, gated by door flags +0x16E4/5/6); place + Move mission (vtbl+0x1E8) | Ares hooks `0x443CCA`/`0x444119`/`0x444131` inside it; force-created at `0x443BC0` |
| `FUN_00500020` | produced-unit **default destination** — nearest free cell from the building's own coords (words 0x27/0x28/0x29) via `FUN_0056CD60`+`FUN_0056D750` (the exit-adjacent gather cell) | callee of KickOutUnit |
| `FUN_004437D0` | right-click order handler incl. **SetRallypoint** (`0x443860` is 0x90 B in; Ghidra didn't split) — computes target cell from building coords, dispatches a game event, plays `"EVA_NewRallyPointEstablished"` (ref @`0x4439D4`) | EVA string ref |
| `FUN_00448171…` | tail of **TogglePrimaryFactory** — `"EVA_PrimaryBuildingSelected"` (ref @`0x448171`); reads building IsPrimary byte (+0x3D3), gates on producer `Factory` type (+0xEB8) | EVA string ref |
| `FUN_0045FE40` | BuildingTypeClass::LoadFromINI — **BuildCat→+0xE08** (int enum), **ExitCoord→+0xEC8** (CoordStruct), **BaseNormal→+0x154F**, **IsBaseDefense→+0x1706**, `Factory`(AbstractType)→+0xEB8 | string-ref listings (`prod4_loadini_sites.txt`) |
| `FUN_00505180` | prerequisite check (every token ANDed; negative id → `[General] Prerequisite<Group>` vector) | construction.md §10 |

## §1.1 Queue model / tab → queue mapping

**Production queues are per-house, per-category `FactoryClass` objects — there are
SIX category slots, NOT three.** `HouseClass::BeginProductionOf`
(`FUN_004FA170`) switches on the producible's `AbstractType` and reads (or lazily
creates) the primary `FactoryClass*` for that category, stored on the HouseClass:

| category | House `FactoryClass*` slot | classifier |
|---|---|---|
| Aircraft | +0x53AC | `AbsType ∈ {Aircraft=2, AircraftType=3}` |
| Infantry | +0x53B0 | `AbsType ∈ {Infantry=15, InfantryType=16}` |
| Vehicles | +0x53B4 | `AbsType ∈ {Unit=1, 40}` **and `Naval==0`** |
| Ships | +0x53B8 | `AbsType ∈ {Unit=1, 40}` **and `Naval==1`** |
| **Structures** | +0x53BC | `AbsType ∈ {Building=6, BuildingType=7}` **and `BuildCat != Combat(5)`** |
| **Defenses** | +0x53CC | `AbsType ∈ {Building=6, BuildingType=7}` **and `BuildCat == Combat(5)`** |

(+0x53C0/+0x53C4/+0x53C8 are the three unused primary slots between Structures
and Defenses — matches the YRpp `Primary_ForBuildings, Unused1/2/3,
Primary_ForDefenses` layout at 4-byte stride.) A parallel per-category **byte**
array at +0x53D0…+0x53D8 (`FUN_00500510`) tracks per-category state with the same
switch. The MultipleFactory **count** array (`FUN_00500730`, +0x5378…+0x5388)
uses the same categories **except buildings collapse to one slot (+0x5384)** —
i.e. MultipleFactory for buildings counts all ConYards together, no struct/def
split.

Each slot is an **independent FactoryClass with its own queue** (its own +0x44
queue array / +0x50 count / +0x58 Object / 54-step Production). Clicking a
structure and a defense therefore builds **concurrently** from the *same*
Construction Yard: they resolve to +0x53BC and +0x53CC, two different
FactoryClass objects. `FUN_004C9B90`'s "abandon current before starting a new
one" special-case fires only within the *same* factory (a second structure
abandons the first structure; it never touches the defense factory).

**The tab classifier is the item's `AbstractType` (roster membership) + `Naval`
+ `BuildCat`, NOT `IsBaseDefense`.** Confirmed in `FUN_004FA170`:
it loads `BuildingType+0xE08` (= `BuildCat`, parsed at LoadFromINI `0x45FE74`)
and branches: `BuildCat == 5 → +0x53CC (Defenses) else +0x53BC (Structures)`.
Verified against real `rulesmd.ini`: every defensive structure (GAPILL, GTGCAN,
NALASR, ATESLA, NASAM, GAWALL, NAWALL) has `BuildCat=Combat`; power/refinery/
tech/ConYard structures have `BuildCat = Power/Resource/Tech/None`.
`IsBaseDefense` (+0x1706) is a *separate* AI-targeting flag — GTGCAN and GAWALL
are `BuildCat=Combat` with `IsBaseDefense=None`, so it cannot be the queue key.
The infantry/vehicle split is roster membership: an item's `AbstractType` comes
from which list it was registered in (`[InfantryTypes]` → Infantry,
`[VehicleTypes]` → Unit); the producing building is one whose `Factory=` matches
that AbstractType (`GAWEAP Factory=UnitType`, `GAPILE Factory=InfantryType`,
`GACNST Factory=BuildingType`) and whose `Naval` matches.

The 4 sidebar tabs are a UI grouping over these categories: tab0 = Structures,
tab1 = Defenses (BuildCat=Combat), tab2 = Infantry, tab3 = Units (Vehicles +
Ships + Aircraft — three separate queues shown together). **Confidence:
CONFIRMED** (AbstractType switch + BuildCat offset + rulesmd corroboration).

## §1.2 Primary factory & MultipleFactory

**Which building emits a finished item — the "primary" building.** The
FactoryClass queue is house-level; the *physical* building a completed unit
walks out of is the **primary building** of that factory type — the one with its
IsPrimary flag (BuildingClass +0x3D3) set. Right-clicking a factory building with
it selected toggles that flag (TogglePrimaryFactory, tail at `FUN_00448171`,
plays `"EVA_PrimaryBuildingSelected"`, gated on the building's producer `Factory`
AbstractType +0xEB8) and writes the house's per-category primary pointer. **The
default primary is the first-built factory of that type** (a newly placed factory
becomes primary iff the house has none yet for its category); building a second
does not steal primary until the player right-clicks it. **Confidence: PROBABLE**
(toggle + IsPrimary byte + house pointer write seen; the auto-first-primary-on-
placement path was not isolated 1:1 — standard RA2 rule).

**MultipleFactory.** `rulesmd.ini [General] MultipleFactory = 0.8` (read directly;
loaded to `Rules+0x57C` per construction.md). `Time_To_Build` (`FUN_006F3D00`)
confirms the compounding exactly: `n = FUN_00500730(product_class)` (count of the
house's factories of that class); `if (0.0 < MultipleFactory) { for k in 1..n−1:
t = ftol(t × MultipleFactory) }`. So each *extra* same-class factory multiplies
build time by 0.8 (1 factory → ×1.0, 2 → ×0.8, 3 → ×0.64, …) — i.e. it *speeds
up* production, applied AFTER the power/country/type multipliers and BEFORE the
wall coefficient. Matches construction.md §7 and the pre-hunt expectation.
**Confidence: CONFIRMED** (loop in listing + 0.8 from rulesmd).

## §1.3 Exit cell + leave-factory motion

A finished vehicle/infantry is emitted by **BuildingClass::KickOutUnit**
(`FUN_00443BC0`), dispatched off the building's WhatAmI switch (Unit=1 and
Infantry=15 fall through to the generic ground path; Aircraft=2 and
Building=6 have their own blocks). The exit cell is **computed foundation-
relative at runtime — it is NOT a fixed literal offset and NOT a per-unit
`Factory=` exit-coord list** (a unit's `Factory=` key is the producer's
AbstractType, not a coordinate). The computation:

1. take the building's reference cell (`vtbl+0x48`, its center/anchor);
2. choose an exit facing (from the building facing / a small RNG via
   `FUN_007C4D00`/`FUN_0065C220`);
3. walk the **8-neighbour `(dx,dy)` table `DAT_0089E5F8`** outward, testing each
   cell's occupant with `FUN_0047C430`, until a cell not occupied by the building
   itself is found (loop `while occupant == this_building`);
4. add the producing **BuildingType's `ExitCoord`** sub-cell lean
   (+0xEC8 X / +0xECC Y / +0xED0 Z), applied only at the specific door cell
   positions gated by flags +0x16E4/+0x16E5/+0x16E6 (per-facing door offsets).

The unit is then placed at that cell and given a **Move mission** (`vtbl+0x1E8`,
mission id 2 = Move) to the gather/rally cell; it drives/walks out under engine
control and only becomes player-idle once it reaches that cell. **Structures do
NOT exit — they READY-hold until placed** (construction.md §10, IsDone +0x70);
vehicles/infantry are kicked out immediately on completion. **Confidence:
CONFIRMED (structural)** for the 8-ring + ExitCoord + Move-out; the exact
facing/RNG door-cell pick is **PROBABLE** (see deviation).

## §1.4 Rally point

Right-clicking a destination cell with a primary factory building selected routes
through the order handler `FUN_004437D0`: it converts the building's coords
(words 0x27/0x28/0x29) + the clicked cell to a target, dispatches it as a game
event, and plays `"EVA_NewRallyPointEstablished"` (`BuildingClass::SetRallypoint`,
YRpp `0x443860`, is 0x90 B into this function; Ghidra merged them). The rally
target is stored on the building.

**Where completed units path:** KickOutUnit places the unit at the exit cell and
issues the Move mission; the move destination is the rally cell **if one is set**,
otherwise the **default exit-adjacent gather cell** returned by `FUN_00500020`
(nearest free cell computed from the building's own coordinate). So with no rally
the unit gathers just outside the factory door; with a rally it drives to the
rally cell after leaving. **Confidence: PROBABLE** (SetRallypoint + EVA + default-
gather fn located; the exact rally-member offset and the precise "rally vs
gather" selection inside KickOutUnit's Move call were not isolated to a single
literal — standard RA2 behavior).

## §1.5 Build economy for units

The unit factory uses the **identical FactoryClass 54-step charge model** as
buildings — `FactoryClass::Set` (`FUN_004C9B90`) is the shared entry for all
categories. It sets Object (+0x58) via the type's CreateObject (vtbl+0x8C) and
snapshots `balance` (+0x60) = `type->Cost_Of(house)` (vtbl+0x84 =
`FUN_007112A0` = `ftol(vtbl+0xAC × country × handicap)`). For **UnitType and
InfantryType, vtbl+0xAC is the base `FUN_00711250` = `mov eax,[ecx+0x610]; ret`
(raw `Cost`)** — the FreeUnit subtraction lives ONLY in the BuildingTypeClass
override (`FUN_0045ED40`, economy.md §6a). So **units have NO FreeUnit charge
subtraction: charge == Cost** (× country/handicap mults, all 1.0 for our roster),
and — like buildings — `build_frames = trunc(Cost × BuildSpeed × 0.9)` off the
same raw Cost (construction.md §7). The 54-step incremental spend, stall-when-
broke, and cancel-refund-all-spent all apply unchanged.

**Queue depth differs from buildings:** the building path abandons the current
item before starting a new one (one-slot), but the non-building path appends to
the FactoryClass queue capped at `Rules+0xF0 = MaximumQueuedObjects = 29`
(`FUN_004C9B90` lines: reject if `Rules+0xF0 <= count`, else push into +0x44 at
+0x50). **Immediate-exit vs ready-hold:** buildings wait for placement (READY
hold); vehicles/infantry are kicked out on completion (§1.3) and the factory
pulls the next queued item. **Confidence: CONFIRMED.**

## §1.6 Per-unit prerequisite / tech gating

Mechanism per construction.md §10 (`FUN_00505180`): every `Prerequisite=` token
is ANDed; a plain id = "own ≥1 live placed building of that type"; a group token
maps to a `[General] Prerequisite<Group>` BuildingType list. Recovered from real
`rulesmd.ini` (committable constants):

**Prerequisite groups** (`[General]`):
- `PrerequisitePower  = GAPOWR,NAPOWR,NANRCT,YAPOWR`
- `PrerequisiteFactory = GAWEAP,NAWEAP,YAWEAP`
- `PrerequisiteBarracks = NAHAND,GAPILE,YABRCK`
- `PrerequisiteRadar  = GAAIRC,NARADR,AMRADR,NAPSIS`
- `PrerequisiteTech   = GATECH,NATECH,YATECH`
- `PrerequisiteProc   = GAREFN,NAREFN,YAREFN`  ; `PrerequisiteProcAlternate = SMIN`

**Producer buildings** (own prereqs + what they build):
- `GAWEAP` (Allied War Factory): `Prerequisite=PROC,GAPILE,GACNST`, `Factory=UnitType`
- `NAWEAP` (Soviet War Factory): `Prerequisite=PROC,NAHAND,NACNST`, `Factory=UnitType`
- `GAPILE` (Allied Barracks): `Prerequisite=POWER,GACNST`, `Factory=InfantryType`
- `NAHAND` (Soviet Barracks): `Prerequisite=POWER,NACNST`, `Factory=InfantryType`

**Producible units** (`Prerequisite=`):
- `E1` (GI) = `GAPILE`; `GGI` (Guardian GI) = `GAPILE`; `E2` (Conscript) = `NAHAND`
- `ENGINEER` / `DOG` / `ADOG` = `Barracks` (group token)
- `MTNK` (Grizzly) = `GAWEAP`; `FV` (IFV) = `GAWEAP`
- `HTK` / `DRON` = `NAWEAP`; `V3`/`TTNK` = `NAWEAP,NARADR`; `APOC`/`ZEP` = `NAWEAP,NATECH`
- `CMIN` = `GAWEAP,PROC`; `HARV` = `NAWEAP,PROC`; `AMCV` = `GAWEAP,GADEPT`
- `SREF` (Prism Tank) = `GAWEAP,GATECH`; `LTNK` (Lasher, Yuri) = `YAWEAP`

So a War Factory (GAWEAP/NAWEAP) gates all vehicles, a Barracks (GAPILE/NAHAND)
gates all infantry, and heavier units AND them with a Radar/Tech/Proc token.
GI = GAPILE and Grizzly = GAWEAP as the brief expected. **Confidence: CONFIRMED**
(mechanism from `FUN_00505180`; tokens read from real rulesmd).

## Confidence summary

| Item | Evidence | Confidence |
|------|----------|:----------:|
| §1.1 six per-category FactoryClass queues; router = AbsType+Naval+BuildCat | `FUN_004FA170` switch + slot offsets + LoadFromINI BuildCat +0xE08 + rulesmd | **CONFIRMED** |
| §1.1 **Structures and Defenses are SEPARATE concurrent queues** (BuildCat==Combat) | +0x53BC vs +0x53CC; every rulesmd defense = BuildCat=Combat | **CONFIRMED** |
| §1.1 IsBaseDefense (+0x1706) is NOT the queue key | GTGCAN/GAWALL Combat but IsBaseDefense=None | **CONFIRMED** |
| §1.2 primary building = IsPrimary (+0x3D3), right-click toggles, default first-built | TogglePrimary tail + EVA + house pointer write | **PROBABLE** (auto-first-primary not isolated 1:1) |
| §1.2 MultipleFactory = 0.8, compounds ×0.8 per extra same-class factory | `FUN_006F3D00` loop + rulesmd | **CONFIRMED** |
| §1.3 exit cell = foundation-relative 8-ring walk + BuildingType `ExitCoord`; not a per-unit list | `FUN_00443BC0` + `DAT_0089E5F8` + ExitCoord +0xEC8 | **CONFIRMED (structural)** |
| §1.3 leave-factory = Move mission to gather/rally; structures READY-hold, units exit on completion | KickOutUnit `vtbl+0x1E8` + construction.md §10 | **CONFIRMED** |
| §1.3 exact facing/RNG door-cell pick | uses game RNG/facing | **PROBABLE** ([RE-FALLBACK] deterministic pick) |
| §1.4 rally set by right-click (EVA_NewRallyPointEstablished); default = exit-adjacent gather cell (`FUN_00500020`) | `FUN_004437D0` + `FUN_00500020` | **PROBABLE** |
| §1.5 units use same 54-step charge; charge == Cost (NO FreeUnit); build_frames from raw Cost | `FUN_004C9B90` +0x60 = Cost_Of; vtbl+0xAC = raw for Unit/Inf | **CONFIRMED** |
| §1.5 unit queue depth = MaximumQueuedObjects (29); immediate-exit vs building READY-hold | `FUN_004C9B90` Rules+0xF0 cap | **CONFIRMED** |
| §1.6 prereq AND-mechanism + per-unit tokens (GI=GAPILE, Grizzly=GAWEAP, groups) | `FUN_00505180` + rulesmd | **CONFIRMED** |

## Deviations / plan diffs (for the RE-GATE review)

- **[PLAN DIFF — item 1, RE-GATE STOP] The plan's "3 queues, tabs 0+1 share the
  BuildingType queue" is WRONG.** The binary has **six** per-category primary
  FactoryClass slots, and **Structures (tab0) and Defenses (tab1) are SEPARATE
  concurrent queues** keyed on `BuildCat==Combat` (+0x53BC vs +0x53CC). You *can*
  build a power plant and a pillbox at once from one ConYard. For the plan's
  War-Factory+Barracks scope the concurrently-runnable queues are **4**:
  Structures, Defenses, Infantry, Vehicles (Ships/Aircraft add two more if ever
  built). **Task 3/6 must be amended before Task 2 is dispatched** — the tab→queue
  map is `{tab0→Structures(BuildCat≠Combat), tab1→Defenses(BuildCat==Combat),
  tab2→Infantry, tab3→Vehicles/Ships/Aircraft}`, and the classifier is
  AbstractType(roster) + Naval + BuildCat, not IsBaseDefense.
- **[PLAN OK — MultipleFactory]** 0.8, compounding ×0.8 per extra same-class
  factory (buildings count all ConYards in one class slot). Matches plan
  expectation; keep.
- **[PLAN NOTE — exit cell]** The plan's "exit cell foundation-relative" holds:
  it is a foundation-relative 8-neighbour walk to the first free cell plus the
  BuildingType `ExitCoord` sub-cell offset — NOT a fixed constant offset and NOT
  a per-unit exit list. Task 4 should model exit_cell as
  `first-free-cell-in-8-ring-from-building-anchor + ExitCoord`, seeded from the
  building's exit facing.
- **[DEVIATION — door-cell RNG]** The original picks the exit facing with the
  lockstep game RNG (`FUN_007C4D00`/`FUN_0065C220`). Our single-machine engine
  substitutes a **deterministic exit-facing** (building's primary door facing,
  then first free cell in the 8-ring) — same reachable set, not bit-identical to
  the netgame sequence.
- **[DEVIATION — leave-factory animation]** We keep the Move-out to the gather/
  rally cell (unit is engine-driven until it arrives, then player-idle) but do not
  replicate any per-building door open/close animation frames beyond the drive.
- **[PLAN NOTE — rally default]** Default gather cell = `FUN_00500020`'s nearest-
  free-cell from the building's own coordinate (exit-adjacent), overridden by the
  rally cell when set. Task 5 should default the produced-unit destination to the
  exit-adjacent free cell, not the map center.
- **[PLAN OK — unit build economy]** charge == Cost (no FreeUnit for units;
  FreeUnit subtraction is BuildingType-only), build_frames from raw Cost, 54-step
  incremental spend, queue depth 29, immediate exit on completion. Matches plan.
