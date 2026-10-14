# Enemy AI base-planning semantics — reverse-engineering notes

(Method: `docs/re/workflow.md`; image base `0x00400000`, YR 1.001 `gamemd.exe`.
Hunt scripts, all off-repo in the session scratchpad: `ai_hunt1.py` …
`ai_hunt5.py`; decompiles/listings under the scratchpad `out/` — nothing under
`D:\ra2-re` or the scratchpad is committed. **Content-anchored only.** Every
function below was located by anchoring on the literal `[AI]` / `[IQ]` /
difficulty INI key strings the binary loads (`"PowerSurplus"`, `"BaseSizeAdd"`,
`"RefineryRatio"`, `"SovietBaseDefenses"`, `"BuildConst"`…`"BuildRadar"`,
`"ConcreteWalls"`, `"BuildDelay"`, `"AISafeDistance"`, `"BaseBias"`,
`"MaxIQLevels"`, `"Production"`, the section names `"Normal"`/`"Difficult"`),
recovering the `RulesClass` field offset each writes, then instruction-scanning
the whole `.text` (1,130,658 insns) for those displacement offsets and
xref-walking to the consumers. No YRpp/Ares address was used as evidence.
Numeric constants are read from the real `rulesmd.ini` extracted from
`ra2md.mix`.)

Companion of `construction.md` (FactoryClass / BuildTime / power / placement
adjacency / prerequisites) and `production.md` (queue router). This doc is the
calibration authority for AI-1 base-planning spec §1 items 1–6 and resolves the
`[RE-GATE APPLIED]` blocks in the phase plan Tasks 2–7. **See "Deviations / plan
diffs (for the RE-GATE review)" for the STOP items** — the Soviet defense set,
the dead `[AI]` build lists, and the plan-list-vs-priority model all diverge
from the pre-hunt assumptions.

Key globals: `DAT_00886160` = `RulesClass::Instance`. `DAT_00a82bd4` /
`DAT_00a82be0` = BuildingTypeClass array base / count. `DAT_00a7f194` /
`DAT_00a7f1a0` = HouseClass array base / count. `DAT_00a8a1a0` = game mode
(0 = campaign; non-zero = skirmish/MP). `FUN_0065c220` = the lockstep AI RNG
(`rand(lo,hi)`), `FUN_007c4d00` = the raw RNG draw.

## Located functions

| Function | Role | How found |
|----------|------|-----------|
| `FUN_00672520` | **RulesClass::Read `[AI]`** — loads every `[AI]` key to a RulesClass field (offset map below); build-lists parsed to BuildingType vectors | anchor: `"PowerSurplus"`/`"BuildConst"`/… string refs (all 33 keys land here) |
| `FUN_0066ccb0` | **DifficultyClass::Read** — reads one difficulty section into a 0x50-byte struct (FirePower/Groundspeed/Airspeed/Armor/ROF/Cost/BuildTime/RepairDelay/BuildDelay/BuildSlowdown/DestroyWalls/ContentScan) | anchor: `"BuildDelay"`/`"RepairDelay"`/`"DestroyWalls"`/`"ContentScan"` refs |
| `FUN_00668630` | **RulesClass::Process** — the master dispatcher; calls `Read_AI`/`Read_IQ`/`[General]`, then `DifficultyClass::Read` three times: **Easy→Rules+0x1538, Normal→Rules+0x1588, Difficult→Rules+0x15d8** | disasm of the three `0x66CCB0` call sites (LEA EDX,[EDI+off]) |
| `FUN_00673c80` | **RulesClass::Read `[IQ]`** — MaxIQLevels→+0x1434, SuperWeapons→+0x1438, Production→+0x143c, … SellBack→+0x145c | anchor: `"MaxIQLevels"`/`"Production"` refs |
| `FUN_0066cf70` | `[General]` loader — **AISafeDistance→Rules+0xd74 (int), BaseBias→Rules+0x1420 (double)** | anchor: `"AISafeDistance"`/`"BaseBias"` refs (as construction.md) |
| `FUN_005052d0` | **HouseClass — build the ordered base-node plan** into the house's node vector (house+0x5704). Filters all BuildingTypes by ownership/side/tech/prereq, orders via the `[AI]` category anchors, appends per-side base-defense placeholders | instruction-scan hit on Rules build-list offsets 0x8ac…0x98c + `FUN_00505000`/`FUN_00505180` calls |
| `FUN_0050a3e0` | **HouseClass — init/sync the base plan.** If node count (house+0x5714)==0 calls `FUN_005052d0`; records each already-placed building's cell into its node (+4); sets house IQ (house+0x24c) = `Rules.MaxIQLevels` | scan hit on house+0x5704/+0x5708/+0x5714 + calls `FUN_005052d0` |
| `FUN_004fe200` | **HouseClass — per-node build dispatcher.** One-AI-structure-at-a-time gate (house+0x564c); peeks the head node; resolves defense/wall placeholders; **power-adequacy preemption** (inserts a Power node before a drainer); sets house+0x564c = node type to start | scan hit on house+0x564c + node vector; calls `FUN_00506d10`/`FUN_0050c160` |
| `FUN_004fdb30` | **HouseClass — economy expansion** (refinery vs war-factory add): uses `BuildRefinery`/`BuildWeapons` primaries, compares effective cost vs credits (house+0x30c), inserts the node, calls `FUN_004fe200` | scan hit on Rules 0x8e4/0x938 + node vector |
| `FUN_004fd320` | **HouseClass — AI building tick** (no direct xref → invoked through the house AI-timer/vtable). IQ-gates super-weapon use (`Rules.IQ.SuperWeapons` +0x1438 ≤ house IQ +0x24c); dispatches `FUN_004fdb30`; **returns reschedule delay `0x69 + rand(1,7)` = 105–112 frames** | caller-walk up from `FUN_004fdb30`; `FUN_0065c220(1,7)` tail |
| `FUN_00506d10` | **base-defense placeholder resolver** — calls the three per-side pickers + the AIForcePrediction weighted distribution (air/armor/infantry by enemy force in credits) | caller of `FUN_005079a0`; `AIForcePredictionFudge` |
| `FUN_005079a0` / `FUN_00507b90` / `FUN_00507d80` | **base-defense list selectors by house Side** — read HouseType+0xbc (Side) → `Rules+0x954` (Allied) / `+0x970` (Soviet) / `+0x98c` (Third); filter each candidate by ownership bit (type+0x6cc), TechLevel (type+0x634 ≤ house+0x1d4), and `FUN_00505180` prerequisite | instruction-scan hit on Rules 0x954/0x970/0x98c |
| `FUN_0050c160` | **wall-ring planner** — reads `ConcreteWalls` (Rules+0xa54/+0xa60), finds a base-defense building (type+0x1765) and inserts wall nodes around its foundation perimeter (foundation W/H via `FUN_0045ec80`/`FUN_0045ec90`) | scan hit on node vector; `Rules+0xa54` |
| `FUN_00505000` | get the first *ownable* BuildingType id from an `[AI]` build-list vector (the "primary for this category") | callee throughout `FUN_005052d0`/`FUN_004fdb30`/`FUN_004fe200` |
| `FUN_00505180` | **prerequisite check** — every token ANDed; negative id → `[General] Prerequisite<Group>` (construction.md §10). **Shared with the player.** | construction.md; called by every AI selector |
| `FUN_0042eaa0` / `FUN_0042e7a0` / `FUN_0042e700` | base-node accessors (peek head / get-at / valid-test) over house+0x5704 | callees of `FUN_004fe200`/`FUN_004fdb30` |
| `FUN_0050bcd0` / `FUN_0050bc10` / `FUN_0050bec0` | per-house **handicap** cost mult (house+0x5390..+0x53a0 by WhatAmI) / country cost mult / country BuildTime mult | economy.md/construction.md; consumers of the difficulty→house handicap copy |

---

## §1 House AI tick & build clock

**Entry / scheduler.** The per-house base-build logic runs in `FUN_004fd320`
(HouseClass AI-building tick). It has **no direct call xref** — it is invoked
through the house's AI-update timer (indirect vtable/callback dispatch), exactly
the hang-point the later scheduler sub-phases (AI-2/AI-3) attach to. The routine
**returns its own next-call delay**: the tail is
it returns `RandomRanged(1,7)` (`FUN_0065c220`) plus `0x69` → **105–112 frames
(≈ 7 s at 15 Hz)** between base-build passes. Inside, super-weapon autofire is
IQ-gated (`Rules.IQ.SuperWeapons` = Rules+0x1438 = 4 ≤ house IQ house+0x24c),
and the base-build/economy dispatch runs (`FUN_004fdb30` → `FUN_004fe200`).

**The "start a building now" gate** is in `FUN_004fe200`:

1. `if (house+0x564c != -1) return;` — **house+0x564c holds the BuildingType
   the AI is currently constructing/placing; while it is set, no new structure
   starts.** This is the *one-AI-structure-at-a-time* clock (it is cleared back
   to `-1` when the building is placed — seen in `FUN_004fdb30`, which resets
   house+0x564c/+0x5650/+0x5654/+0x5658/+0x564c to `0xffffffff`).
2. Peek the head base node (`FUN_0042eaa0`).
3. **Prerequisite + tech + ownership** must pass (shared `FUN_00505180`).
4. **Power adequacy** must hold (see §6): if starting this node would drive
   `house_drain(+0x53a8)+type_drain(+0xee4) > house_output(+0x53a4) −
   reserve(+0x160b4)`, a Power-plant node is inserted *ahead* of it first.
5. Set `house+0x564c = node.type` — production of the head node begins through
   the normal FactoryClass path (`HouseClass::BeginProductionOf`,
   production.md §1.1); the finished structure is then placed at the node's
   stored cell.

**Build cadence between structures.** Two timers stack: the ~7 s tick reschedule
above, and the per-house build/repair timer seeded from the **difficulty
`BuildDelay` / `RepairDelay`** (DifficultyClass +0x40 / +0x38; the house's
build-delay timer lives at house+0x5648/+0x5640, decremented against the global
frame counter `DAT_00a8dcec` in `FUN_004fd320`). The exact minutes→frames
conversion was not isolated to a single literal — **[RE-FALLBACK: BuildDelay
seconds = BuildDelay_minutes×60; Easy/Normal 0.03 min ≈ 1.8 s, Difficult
0.1 min ≈ 6 s]**. **Confidence: gate + tick-reschedule CONFIRMED; the BuildDelay
frame conversion [RE-FALLBACK].**

## §2 What-to-build urgency ordering + `[AI]` constants

**The AI does NOT re-evaluate a flat priority list every tick — it builds a
pre-computed *ordered base-node plan* (`FUN_005052d0`), with two dynamic
overrides layered on top (power preemption; economy expansion).**

**Static plan order** (`FUN_005052d0`, reading the `[AI]` category anchors via
`FUN_00505000(Rules+off)`). **NB: the order below is the construction *sequence*
inside `FUN_005052d0`, NOT the final list order — steps 3–5 are a topological
prerequisite sort (see §2a), so the effective plan order can differ from the
raw sequence (e.g. a Refinery ends up ahead of the War Factory even though War
Factory is "hoisted" first).**

1. **ConYard** — `BuildConst` (Rules+0x8ac) — searched in the candidate list,
   appended to the output plan, marked used.
2. **Power** — `BuildPower` (Rules+0x8c8) — primary appended to the output plan.
3. **Barracks** — `BuildBarracks` (Rules+0x900) — *hoisted to candidate index 0*
   (swapped, not appended): it is merely **tried first** by the topo sort.
4. **War Factory** — `BuildWeapons` (Rules+0x938) — *hoisted to candidate
   index 1*: tried second by the topo sort. **Its prereq includes the refinery
   group (token −6 → Rules+0x8e4), so the topo sort SKIPS it until a refinery
   has been appended, then appends it after — see §2a.**
5. the **prerequisite-sorted remainder** of the house's ownable BuildingTypes,
   emitted by the topo-sort loop (§2a): each candidate is appended only once
   `FUN_00505180` reports its prereqs are satisfied by the plan-so-far
   (dependency-ordered; upgrade "plug" buildings `GAPLUG` skipped by name).
   **The first Refinery enters here — it has no building prereq, so it is
   appended before the War Factory/Radar that depend on it.**
6. **Extra Refineries** — `BuildRefinery` (Rules+0x8e4): the topo sort already
   placed the first refinery; the builder then finds it in the plan and inserts
   the *remaining* per-house refinery count at RNG positions after it.
7. **Base defenses** — per-side count of placeholders (§3 / below), RNG-spaced
   through the tail; **walls** ring each defense (`FUN_0050c160`).

**Dynamic overrides:**
- **Power preemption (`FUN_004fe200`)** — before any power-draining node, if
  power would go short, a Power node is force-inserted first. This is the real
  "power emergency" behaviour and it matches the pre-hunt "power first".
- **Economy expansion (`FUN_004fdb30`)** — separately adds a refinery or war
  factory when the house can afford it (effective cost vs credits house+0x30c),
  with sell-to-raise-cash fallback.

## §2a Prereq ordering vs dispatcher skip — the plan is topologically sorted

**Resolved question:** in `rulesmd`, `NAWEAP` (War Factory) requires `PROC`
(refinery) and `NARADR` (Radar) requires `NAREFN`. From a lone ConYard a
strictly-ordered "build the head node, wait if its prereq isn't met" walk would
deadlock at the War Factory node (never reaching the later Refinery that unblocks
it). The real AI never stalls. **The recovered mechanism is (B): the PLAN BUILDER
prereq-sorts the nodes — a prerequisite building is emitted into the plan before
any node that depends on it. The dispatcher does NOT skip; it relies on the plan
already being correctly ordered.** CONFIRMED from decompiles of `FUN_005052d0`,
`FUN_004fe200`, and `FUN_00505180`.

**Evidence 1 — `FUN_00505180` (prereq check) tests against a *supplied* building
list, not the house's built structures.** Signature
`FUN_00505180(BuildingType, &vector, count)`: it walks the type's prereq tokens
(`type+0x63c`, count `type+0x648`); for a non-negative token it searches the
*provided* list (`vector+4` items, `count` entries) for that type; a negative
token expands to a prereq *group* and searches the same list:
**−1→Power(Rules+0x8c8), −2→Factory(+0x938), −3→Barracks(+0x900),
−4→Radar(+0xa34), −5→Tech(+0x91c), −6→Refinery/PROC(+0x8e4).** Any unsatisfied
token ⇒ returns 0. So the caller controls *which* set of buildings counts as
"already present."

**Evidence 2 — `FUN_005052d0` runs a topological (Kahn) sort feeding that check
the plan-so-far.** After appending ConYard + Power and hoisting Barracks/War
Factory to candidate indices 0/1, the main loop repeatedly sweeps the unused
candidates and appends any whose prereqs are already met **by the output plan
built so far**:

```
met = prereqs_met(candidateType, &outputPlanVector, outputCount)   // FUN_00505180
if name != "GAPLUG" and met: append candidateType to plan; mark used; progress = true
...
if (!progress) { /* cycle-breaker: force-append the last unused candidate */ }
```

`&outputPlanVector`/`outputCount` are the *plan under construction*, so the War
Factory (candidate idx 1, prereq group −6 = refinery) fails the check and is
**skipped on that pass**, while the Refinery (no building prereq) passes and is
appended; a later pass then appends the War Factory now that the Refinery is in
the plan. Result: **Refinery precedes War Factory / Radar in the final plan.**
The `!progress` branch force-appends one node to guarantee termination (no
infinite loop even on an unsatisfiable/cyclic set).

**Evidence 3 — `FUN_004fe200` (dispatcher) does NOT skip prereq-blocked nodes.**
It peeks only the **head** node (`FUN_0042eaa0(-1)`, a plain index accessor —
not a "find first buildable" scan), resolves defense/wall placeholders, applies
the **power** preemption gate (`drain+node ≤ output−reserve`, else insert a Power
node ahead), then sets `house+0x564c = head.type` and returns. **There is no
`FUN_00505180` gate on the build node and no iterate-to-next-buildable loop** —
the lone `FUN_00505180` call in the dispatcher is inside the *power-plant choice*
(basic vs advanced power), not a head-node gate. The dispatcher therefore assumes
the head node is buildable in sequence, which is exactly what the topo-sorted
plan guarantees.

**Concrete implication for our engine:** `build_base_plan` MUST prereq-sort the
node plan (topological / Kahn order) so a Refinery is emitted before the War
Factory and Radar that require it — do NOT rely on `next_build` skipping a
prereq-blocked node. `next_build` should mirror `FUN_004fe200`: act on the head
node, apply only the power-preemption insert, and start it; it must **not** scan
past a blocked head, because the original does not. Keep the "hoist Barracks/War
Factory to the front of the candidate set" as a *preference* that the prereq
sort overrides (War Factory cannot precede its refinery). Include the
cycle-breaker (force-emit one node if a full pass makes no progress) to match
termination behaviour.

**Confidence: (B) topological prereq-sort in the plan builder — CONFIRMED**
(`FUN_005052d0` topo loop + `FUN_00505180` list-parameterised prereq check +
`FUN_004fe200` head-only, skip-free dispatcher).

**Recovered `[AI]` constants (committable; from real `rulesmd.ini`, offsets from
`FUN_00672520`):**

| key | value | RulesClass off | notes |
|---|---|---|---|
| `PowerSurplus` | **50** | +0x1134 (int) | build power until surplus ≥ this |
| `PowerEmergency` | **75%** (0.75) | +0x10b0 (double) | sell to raise power below this fraction. **NB: the key is `PowerEmergency`, not `PowerEmergencyFraction`.** |
| `BaseSizeAdd` | **3** | +0x1130 (int) | AI base ≤ largest human base + this |
| `AIBaseSpacing` | **1** | +0x1460 (int) | cell spacing between AI-placed buildings (see §3) |
| `RefineryRatio` / `RefineryLimit` | **.16 / 4** | +0x1128 (dbl) / +0x1124 (int) | refinery share / hard cap |
| `BarracksRatio` / `BarracksLimit` | **.16 / 2** | +0x1118 / +0x1120 | |
| `WarRatio` / `WarLimit` | **.1 / 2** | +0x1108 / +0x1110 | |
| `DefenseRatio` / `DefenseLimit` | **.4 / 40** | +0x10f8 / +0x1100 | |
| `AARatio` / `AALimit` | **.14 / 10** | +0x10e8 / +0x10f0 | |
| `TeslaRatio` / `TeslaLimit` | **.16 / 10** | +0x10d8 / +0x10e0 | |
| `HelipadRatio` / `HelipadLimit` | **.1 / 2** | +0x10c8 / +0x10d0 | |
| `AirstripRatio` / `AirstripLimit` | **.12 / 0** | +0x10b8 / +0x10c0 | |
| `InfantryReserve` / `InfantryBaseMult` | **50000 / 1** | +0x1138 / +0x113c | |
| `CreditReserve` | **100** | +0x1758 | repair won't start below this |
| `ComputerBaseDefenseResponse` | **3** | +0xb18 | over-response to base attacks |
| `AISafeDistance` | **20** cells | +0xd74 | gather distance from enemy base centre (team, not placement) |
| `BaseBias` | **2** | +0x1420 (dbl) | **threat-target value multiplier when enemy is near own base — a TARGETING bias, not a placement bias** |

**Recovered `[AI]` build-list ids (the ids the Soviet AI actually draws from —
this pins Task 6's roster/catalog). Read from the real `[AI]` section:**

| list | RulesClass off | ids | role |
|---|---|---|---|
| `BuildConst` | +0x8ac | **GACNST,NACNST,YACNST** | ConYard |
| `BuildPower` | +0x8c8 | **NAPOWR,GAPOWR,YAPOWR** | power plant |
| `BuildRefinery` | +0x8e4 | **NAREFN,GAREFN,YAREFN** | refinery |
| `BuildBarracks` | +0x900 | **NAHAND,GAPILE,YABRCK** | barracks |
| `BuildTech` | +0x91c | **NATECH,GATECH,YATECH** | tech |
| `BuildWeapons` | +0x938 | **GAWEAP,NAWEAP,YAWEAP** | war factory |
| `BuildRadar` | +0xa34 | GAAIRC,NARADR,AMRADR,NAPSIS | radar |
| `AlliedBaseDefenses` | +0x954 | GAPILL,ATESLA,NASAM | Allied defenses |
| **`SovietBaseDefenses`** | **+0x970** | **NALASR,NABNKR,TESLA,NAFLAK** | **Soviet defenses ← Task 6** |
| `ThirdBaseDefenses` | +0x98c | YAGGUN,YAPSYT,NATBNK | Yuri defenses |
| `ConcreteWalls` | +0xa54 | **GAWALL,NAWALL,GAFWLL** | walls (ringed around defenses) |
| `BuildNavalYard` | +0xaa4 | NAYARD,GAYARD,YAYARD | naval |
| `BuildDummy` | +0xac0 | GAPILL,NALASR,YAGGUN | placeholder |

**DEAD lists — `BuildDefense`, `BuildPDefense`, `BuildAA`, `BuildHelipad` are
commented out in rulesmd** with the note *"gs these 4 do nothing"*. The loader
`FUN_00672520` still parses `BuildDefense`→+0x9c4 etc., but base-defense choice
is made from the `*BaseDefenses` **side lists** + the AIForcePrediction weighting
(`FUN_00506d10`, `AIForcePredictionFudge = 5,25,80`), NOT from `BuildDefense`.

**Base-defense COUNT (per side, per difficulty)** — the explicit count arrays
(read from rulesmd; superseding the obsolete `*BaseDefenseCoefficient` formula),
indexed by the house difficulty index (house+0x184), consumed in
`FUN_005052d0`/`FUN_004fe200` at Rules+0xd84/+0xda0/+0xdbc (counts) and Rules+0xdd8
(per-difficulty insertion %):

- `AlliedBaseDefenseCounts = 25,20,6`
- **`SovietBaseDefenseCounts = 25,22,6`**
- `ThirdBaseDefenseCounts  = 25,22,6`

The rulesmd comment tags these `25,20,6` inconsistently ("h,m,e" in one place,
"easy,normal,hard" in the sentence). Given `25 ≫ 6` and gameplay (a *hard* AI
plans more defenses), the array is **[hardest…easiest]** by index. The exact
index→difficulty binding for these count arrays is **[RE-FALLBACK: hard→25,
normal→22, easy→6]** — see §4 on the difficulty-index inversion.

**Confidence: plan order + power preemption + build-list ids + Soviet defense set
+ scalar constants CONFIRMED; the ratio/limit *runtime* use (the modern YR AI
leans on the plan+counts, and several ratios are legacy) PROBABLE; the
count-array index binding [RE-FALLBACK].**

## §3 Where-to-place — base-node planner & scan

**The AI's planned base is a `DynamicVectorClass` of 0x10-byte nodes at
`HouseClass+0x5704`** (items ptr +0x5708, count +0x5714, capacity +0x570c;
`+0x5711`/`+0x5718` are the vector's grow flags). **Each node = 16 bytes:**

| off | field |
|---|---|
| +0 | BuildingType **array index**, or a negative **marker**: `-1` = generic base-defense placeholder, `-2`/`-3`/`-4` = special (wall/gate/skip) placeholders |
| +4 | the **placed cell / packed coordinate** (X,Y shorts) — 0 until placed |
| +8 | flags byte |
| +0xC | 0 |

- Built once by `FUN_005052d0` when the vector is empty; re-filled the same way
  after a full teardown (`FUN_0050a3e0` calls it iff count==0).
- `FUN_0050a3e0` then walks the house's already-placed buildings (house+0x6c
  array / +0x78 count) and **writes each live building's cell into its matching
  node's +4** — so the plan is re-synced to reality every init.

**Placement of a fresh node** (node cell +4 == 0):
- **Base-defense placeholders** (`-1`) resolve through `FUN_00506d10` → the
  per-Side list selector (§2) → a weighted pick, then the cell is chosen and the
  concrete BuildingType id is written into the node.
- **Walls** (`FUN_0050c160`): for each planned base defense, wall nodes are laid
  on the **foundation perimeter ring** of that defense (foundation width/height
  from `FUN_0045ec80`/`FUN_0045ec90`), using `ConcreteWalls[side]`.
- Positional jitter / tie-breaks use the lockstep RNG `FUN_0065c220`
  (`rand(0,99)` gates the per-difficulty defense-insertion %; `rand(lo,hi)`
  chooses insert positions).
- **`AIBaseSpacing = 1`** is the inter-building cell gap the placer targets
  (the commented `WantsExtraSpace` would try `AIBaseSpacing+1` then settle for
  `AIBaseSpacing`).

**The exact deterministic outward cell-scan** (ring shape/direction/tie-break
for a brand-new expansion building with no anchor node) was **not isolated to a
single primitive** in this hunt — the recovered path derives most cells from
existing-building anchors + foundation-perimeter rings + RNG jitter, not a clean
spiral. **[RE-FALLBACK: nearest-first Chebyshev ring outward from the ConYard,
respecting `AIBaseSpacing`, first-free-cell tie-break — Task 3's fallback.]**

**Validity reuse:** AI placement is exempt from the player's `Adjacent=`
proximity check — `Passes_Proximity_Check` (`FUN_004a8dc0`, construction.md §9)
early-returns success for non-local-human houses. The per-cell validity
(in-bounds / buildable terrain / unoccupied) is the same grid the player's
green/red cursor uses. **Confidence: node storage + sync + wall ring + Side
selection + Adjacent bypass CONFIRMED; the outward expansion scan order
[RE-FALLBACK].**

## §4 Per-difficulty handicaps

**Three difficulty sections `[Easy]` / `[Normal]` / `[Difficult]`** (NOT
`[Medium]`/`[Hard]`) are loaded by `FUN_00668630` in that order into three
0x50-byte `DifficultyClass` instances:

- **Easy → RulesClass+0x1538**
- **Normal → RulesClass+0x1588**
- **Difficult → RulesClass+0x15d8**

**DifficultyClass layout** (`FUN_0066ccb0`; add the base above):

| field | struct off | Easy | Normal | Difficult |
|---|---|---|---|---|
| FirePower | +0x00 | 1.0 | 1.0 | 1.0 |
| Groundspeed | +0x08 | 1.0 | 1.0 | 1.0 |
| Airspeed | +0x10 | 1.0 | 1.0 | 1.0 |
| Armor | +0x18 | **1.2** | 1.0 | **0.8** |
| ROF | +0x20 | **0.8** | 1.0 | **1.2** |
| **Cost** | +0x28 | **1.0** | **1.0** | **1.0** |
| **BuildTime** | +0x30 | **0.8** | **1.0** | **1.0** |
| RepairDelay | +0x38 | 0.02 | 0.02 | 0.05 |
| **BuildDelay** | +0x40 | **0.03** | **0.03** | **0.1** |
| BuildSlowdown | +0x48 (byte) | no | **yes** | **yes** |
| DestroyWalls | +0x49 (byte) | no | no | no |
| ContentScan | +0x4a (byte) | yes | yes | (default no) |

(Defaults baked into the loader: BuildTime/Cost/Armor/ROF = 1.0
`0x3ff00000`; RepairDelay = 0.02 `0x3f947ae1…`; BuildDelay = 0.03
`0x3f9eb851…`; DestroyWalls default **yes**.)

**Group → house mapping + inversion.** Each house carries a **difficulty index
at house+0x184** (0/1/2) that selects both a `DifficultyClass` and the
per-difficulty count/percent arrays (§2). The rulesmd header states the rule
outright: *"The computer and the player are handicapped by individual settings.
Thus the computer may be playing at 'difficult' level while the player may be
playing at 'easy' level."* i.e. **the human's chosen game difficulty and the AI
house's applied group are independent and typically inverted** (a player on
"Easy" faces an AI handicapped toward the weaker group, and vice-versa). The
skirmish setup writes house+0x184 from the lobby AI-difficulty selection; the
exact lobby-value→index arithmetic was **not pinned** (many writers of
house+0x184 across setup code) — **[RE-FALLBACK: index 0=Easy,1=Normal,
2=Difficult as loaded; the enemy Soviet base runs at the lobby-selected AI
difficulty, default Normal].**

**Application.** [W6-02-04 correction: the difficulty `BuildTime` (house+0x1B8, written by AssignHandicap @0x4F6FFA) is read ONLY by the house CRC @0x502C31 -- `Time_To_Build` `FUN_006F3D00` applies only the country mult `FUN_0050BEC0`; `BuildDelay` (+0x1C8) is CRC-only too. The clone sets `build_time_mul` = 1.0.] The country BuildTime mult feeds
`Time_To_Build` (construction.md §7) via the per-house handicap copy; `Cost`
(all 1.0) feeds the cost accessor via the handicap mult `FUN_0050bcd0` (house
fields +0x5390..+0x53a0). **No starting-credit multiplier lives in the
difficulty sections** (starting credits are a lobby/MPlayer setting, not a
`[Easy]/[Normal]/[Difficult]` key). **IQ gates** (`[IQ]`, `FUN_00673c80`):
`MaxIQLevels=5`, and skirmish AI houses have their IQ set to the max
(`FUN_0050a3e0`: house+0x24c = Rules.MaxIQLevels), so all IQ-gated behaviours
(Production=5, SuperWeapons=4, Harvester=2, RepairSell=1, …) are active for the
enemy base.

**Committable difficulty multiplier set (the pre-hunt 1.0 placeholders):**

| constant | Easy | Normal | Difficult |
|---|---|---|---|
| `kDifficultyCost` | 1.0 | 1.0 | 1.0 |
| `kDifficultyBuildTime` | **0.8** | 1.0 | 1.0 |
| `kDifficultyBuildDelay` (min) | **0.03** | 0.03 | **0.1** |
| `kDifficultyRepairDelay` (min) | 0.02 | 0.02 | 0.05 |

**Confidence: section names/order, struct layout, all numeric values, and the
"handicaps are independent/invertible" rule CONFIRMED; the exact lobby→index
arithmetic and any starting-credit multiplier [RE-FALLBACK] (none in the
difficulty sections).**

## §5 Rebuild semantics

The base plan is a persistent node list; **destroyed buildings are rebuilt
in-place, not re-planned as fresh expansion:**

- A node's cell (+4) is **retained** when its building dies — `FUN_0050a3e0`
  only *writes* cells for live buildings and never clears a node on death, so
  the node keeps its last coordinate and the dispatcher re-queues that same
  BuildingType at that cell.
- `FUN_004fdb30`/`FUN_004fe200` only **remove** a node from the vector when its
  building is successfully (re)built and placed, or when the node is invalid
  (compaction via `FUN_007c8e90` memmove). A node whose building is gone stays
  in the list and is re-attempted on the next tick.
- **Priority: rebuild-before-expansion.** Because rebuild nodes remain at their
  original list position (ahead of tail expansion/defense nodes), the head-node
  dispatcher reaches them first; new expansion/defense nodes are appended at the
  tail. There is **no explicit rebuild counter or per-node rebuild limit** in
  the recovered path (the only caps are the global `*Limit` counts and the
  `BaseSizeAdd` base-size ceiling).

**Confidence: node retention + rebuild-in-place + rebuild-before-tail-expansion
CONFIRMED (from the node lifecycle in `FUN_0050a3e0`/`FUN_004fe200`/
`FUN_004fdb30`); the exact same-cell reuse when the old cell is now blocked is
PROBABLE ([RE-FALLBACK: retry stored cell, else re-scan per §3]).**

## §6 Powered / prerequisite gating for AI builds

The AI obeys the **same** prerequisite and power rules as the player — no new
divergent path:

- **Prerequisites**: every AI selector (`FUN_005052d0`, `FUN_005079a0`,
  `FUN_00506d10`, `FUN_004fe200`) calls the shared **`FUN_00505180`**
  (construction.md §10 — token AND, negative id → `[General] Prerequisite<Group>`
  BuildingType list). Candidates also pass ownership (type+0x6cc bit) and
  **TechLevel** (type+0x634 ≤ house tech level house+0x1d4).
- **Power**: `FUN_004fe200` gates a node on
  `house_drain(+0x53a8) + type_drain(+0xee4) ≤ house_output(+0x53a4) −
  reserve(+0x160b4)` and force-inserts a Power node otherwise — using the same
  house power totals (+0x53a4/+0x53a8) the sidebar and the low-power build-speed
  clamp read (construction.md §8, `FUN_004fcc50`). The low-power production
  slowdown therefore applies to AI production unchanged (it lives in
  `Time_To_Build`, which is category-agnostic).

**Confidence: CONFIRMED** (shared `FUN_00505180` + shared power totals; no
AI-specific prereq/power path exists).

---

## Confidence summary

| Item | Evidence | Confidence |
|------|----------|:----------:|
| §1 build gate = one structure at a time (house+0x564c), prereq+power gated | `FUN_004fe200` decompile | **CONFIRMED** |
| §1 AI tick self-reschedules 105–112 frames (`0x69+rand(1,7)`) | `FUN_004fd320` tail | **CONFIRMED** |
| §1 BuildDelay/RepairDelay seed the per-house build/repair timer; min→frame conversion | difficulty +0x40/+0x38; timer house+0x5648 | **PROBABLE / [RE-FALLBACK]** |
| §2 ordered base-node PLAN (not per-tick priority) built by `FUN_005052d0` from `[AI]` anchors | decompile + `FUN_00505000(Rules+off)` | **CONFIRMED** |
| §2a plan is **topologically prereq-sorted** (mechanism B): builder emits a prereq before its dependents (Refinery before War Factory/Radar); dispatcher acts on head only, does NOT skip | `FUN_005052d0` topo loop + `FUN_00505180` list-param prereq + `FUN_004fe200` head-only | **CONFIRMED** |
| §2 power preemption before drainers; economy expansion separate | `FUN_004fe200` / `FUN_004fdb30` | **CONFIRMED** |
| §2 `[AI]` scalar constants (PowerSurplus 50, PowerEmergency 75%, BaseSizeAdd 3, AIBaseSpacing 1, ratios/limits) | `FUN_00672520` offsets + rulesmd | **CONFIRMED** |
| §2 build-list ids incl. **SovietBaseDefenses = NALASR,NABNKR,TESLA,NAFLAK**; walls = ConcreteWalls | `FUN_00672520` + rulesmd + `FUN_005079a0`/`FUN_0050c160` | **CONFIRMED** |
| §2 `BuildDefense/PDefense/AA/Helipad` are DEAD ("do nothing"); defenses via *BaseDefenses + AIForcePrediction | rulesmd comment + `FUN_00506d10` | **CONFIRMED** |
| §2 per-side/per-difficulty defense COUNTS 25/22/6; index binding | rulesmd + `FUN_004fe200` Rules+0xd84.. | **PROBABLE (index [RE-FALLBACK])** |
| §3 base plan = node vector house+0x5704, 0x10-byte nodes (type/marker, cell, flags) | `FUN_0050a3e0`/`FUN_004fe200`/`FUN_005052d0` | **CONFIRMED** |
| §3 Side→defense-list; wall perimeter ring; RNG jitter; AIBaseSpacing=1 | `FUN_005079a0`/`FUN_0050c160` + rulesmd | **CONFIRMED** |
| §3 exact outward expansion scan order | not isolated | **[RE-FALLBACK]** |
| §3 Adjacent bypass for AI; per-cell validity shared | `FUN_004a8dc0` (construction.md §9) | **CONFIRMED** |
| §4 sections Easy/Normal/Difficult → Rules+0x1538/+0x1588/+0x15d8; struct layout; all values | `FUN_00668630` disasm + `FUN_0066ccb0` + rulesmd | **CONFIRMED** |
| §4 difficulty index house+0x184; human/AI groups independent & invertible | rulesmd header + array indexing | **CONFIRMED (lobby→index arithmetic [RE-FALLBACK])** |
| §4 no starting-credit multiplier in difficulty sections; IQ maxed for skirmish AI | loader keys + `FUN_0050a3e0` IQ set | **CONFIRMED** |
| §5 rebuild-in-place at retained cell; rebuild-before-tail-expansion; no rebuild limit | node lifecycle | **CONFIRMED (blocked-cell reuse PROBABLE)** |
| §6 shared prereq (`FUN_00505180`) + shared power totals; no AI-specific path | selectors + power gate | **CONFIRMED** |

## Deviations / plan diffs (for the RE-GATE review)

- **[PLAN DIFF — Task 6, STOP] The Soviet base-defense set is
  `SovietBaseDefenses = NALASR, NABNKR, TESLA, NAFLAK`** (Rules+0x970, chosen by
  house Side==1), **NOT the spec's "NAPILL/tesla + walls".** NAPILL is not
  Soviet; the Soviet pillbox-equivalent is **NALASR**. Walls are a **separate**
  mechanism — `ConcreteWalls = GAWALL,NAWALL,GAFWLL` (Rules+0xa54), ringed around
  each base defense by `FUN_0050c160`. Task 6's catalog/roster must be replaced
  with `{NALASR, NABNKR, TESLA, NAFLAK}` + `NAWALL` for walls. The
  `BuildDefense` list is DEAD.

- **[PLAN DIFF — Task 4 `next_build`] The engine uses an ordered base-node PLAN,
  not a flat re-evaluated priority.** The pre-hunt "power-emergency → economy →
  prereq → defense → expansion" is *approximately* right for the dynamic layer
  (power IS preempted before drainers; economy expansion IS a separate trigger),
  but the recovered static order is a **topological prereq-sort** (§2a): ConYard →
  Power first, then Barracks/War Factory are *tried* first but the sort emits each
  node only after its prereqs are already in the plan, so the effective order is
  **ConYard → Power → Barracks → Refinery → War Factory → (prereq-sorted rest,
  extra Refineries) → defenses(+walls)** — the Refinery lands ahead of the War
  Factory/Radar that require it, which is what stops the plan deadlocking.
  **`build_base_plan` must prereq-sort; `next_build` must NOT skip a blocked head
  (the dispatcher `FUN_004fe200` acts on the head node only).** If Task 4
  conflated "power emergency" and "power surplus", split them:
  `PowerSurplus=50` (keep building power until surplus ≥ 50) vs the
  drainer-preemption gate (`drain+node ≤ output−reserve`) vs `PowerEmergency=75%`
  (sell to recover below 75% power). Amend before dispatch.

- **[PLAN DIFF — Task 2 constants]** Fill the placeholders with:
  `kAiPowerSurplus=50`, `kAiPowerEmergencyFraction=0.75` (key is `PowerEmergency`
  not `PowerEmergencyFraction`), `kAiBaseSizeAdd=3`, `kAiBaseSpacing=1`,
  `kAiRefineryRatio=0.16`/`kAiRefineryLimit=4`, `kAiDefenseRatio=0.4`/
  `kAiDefenseLimit=40`, `kAiDefenseCountsSoviet={25,22,6}` (per difficulty).
  `kAiBuildDelaySecs` is **difficulty-dependent** (Easy/Normal 0.03 min ≈ 1.8 s,
  Difficult 0.1 min ≈ 6 s) — not a single constant. There is no
  `HarvestersPerRefinery` in `[AI]`; the refinery's free harvester is the
  `FreeUnit` mechanism (economy.md §6) — Task 2 should source harvester supply
  from `FreeUnit`, not an `[AI]` ratio.

- **[PLAN DIFF — Task 2 difficulty set]** The 1.0 placeholders resolve to:
  Cost = **1.0/1.0/1.0** (Easy/Normal/Difficult); BuildTime = **0.8/1.0/1.0**;
  BuildDelay(min) = **0.03/0.03/0.1**; RepairDelay(min) = **0.02/0.02/0.05**.
  Sections are named **`Easy`/`Normal`/`Difficult`** (not Medium/Hard) → index
  0/1/2 at RulesClass+0x1538/+0x1588/+0x15d8. **Group↔house difficulty is
  independent and typically inverted** (human "Easy" ⇒ weak AI); model the AI
  house's group as the lobby-selected AI difficulty, default `Normal`.

- **[PLAN OK — Task 3 placement fallback]** Node storage recovered (house+0x5704
  vector, 0x10-byte nodes with a cell at +4); the exact outward expansion scan is
  not pinned, so Task 3's nearest-first Chebyshev ring (respecting
  `AIBaseSpacing=1`, first-free-cell tie-break) stands as the calibrated
  fallback. Threat/defense placement uses walls-ring-defense + RNG position, not
  a `BaseBias` placement term — **`BaseBias=2` is a targeting multiplier, not a
  placement bias**; drop any placement use of it.

- **[PLAN OK — Task 4/5 rebuild]** Rebuild-before-expansion at the retained node
  cell, no rebuild limit — matches the pre-hunt assumption; keep.

- **[DEVIATION — RNG]** The original uses the lockstep game RNG
  (`FUN_0065c220`/`FUN_007c4d00`) for defense-insertion %, node positions, and
  the AIForcePrediction weighting. Our single-machine engine substitutes a
  **seeded xorshift32** for these tie-breaks — same distributions (same
  `AIForcePredictionFudge=5,25,80`, same counts), not bit-identical to the
  netgame sequence.

- **[DEVIATION — single-slot structure queue]** The AI builds one structure at a
  time (house+0x564c gate), reusing the same single-slot BuildingType FactoryClass
  queue the player uses (production.md §1.1 / construction.md §10) — keep as-is.

## §7 Wave-6 additions (w6fix-ai; gamemd.exe, private Ghidra copy `D:\ra2-re\w6fix-ai`)

Decompiles and scans: `D:\ra2-re\w6fix-ai-out\*.txt` (scripts in `D:\ra2-re\w6fix-ai-scripts`), plus the
audit's `docs/gap/wave6/03-evidence/logs/ghidra_dec*.txt`.

- **Enemy and anger.** `FUN_005045b0(delta, house)` adds anger to the (house, anger) list at
  house+0x5608 (count +0x5614), then sets house+0x5600 (the Enemy) to the house with the highest
  anger (> 0, the first on a tie) that is not defeated (+0x1f5), not this house, and not allied.
  `TechnoClass::ReceiveDamage` (`FUN_00700da0` @0x7012CD–0x7012FD) feeds it
  `ftol(type->vt+0xAC() * damage / type+0xA0)` (read as Cost × damage / Strength) toward the
  attacker's owner. In a multiplayer session `FUN_004fd320` adds 1 anger to the **nearest** other
  living, non-passive house (allies included) on each AI tick while the Enemy is -1 and the
  `AIHateDelays` timer has run out (house+0x5640/0x5648, armed by `FUN_006862d0` from Rules+0x1178 =
  30/50/70 frames at house+0x184). A defeated Enemy loses its anger and the Enemy becomes -1.
  `FUN_004f9990` (MakeAlly) removes the anger toward the ally; `FUN_004f9db0` (MakeEnemy) adds 1.
- **Paranoid computer alliance (RE-Q W6-a: YES).** `FUN_00501460` allies every living computer
  house with every other computer house and declares war on every human, gated on Rules+0x14b5 OR
  a game-mode virtual (`*DAT_00a8a1a4`+4), AND Scenario+0x11e0 == 0. Rules+0x14b5 is
  `[MultiplayerDialogSettings] AlliesAllowed` (loader `FUN_006718e0` @0x671B9B, key 0x82F8BC):
  rulesmd says `no`, but `MPBattleMD.ini` (Battle, the only mode with AI players per
  `mpmodesmd.ini`) sets `AlliesAllowed=yes`. Scenario+0x11e0 is set by `FUN_00687950` @0x687C49
  when any lobby slot picked a team. Callers: base set-up on a ConYard (`FUN_0050a3e0` @0x50A5D4),
  an AI MCV deploy (`FUN_00504fa0`), an AI defeat with `Paranoid=yes` (Rules+0x17e0,
  `FUN_004fbed0`), a human allying (`FUN_004f9990`).
- **Starting money.** `FUN_006862d0` @0x686492–0x6864B3 gives every computer, non-passive house
  `GiveMoney(ftol(Rules+0x1308[house+0x184] * 0.01 * money))` (`FUN_004f9770`: house+0x30c +=);
  `MultiplayerAICM=400,0,0` → a Brutal AI starts with 5× the lobby money.
- **Harvest bonus.** `FUN_00522a80` / `FUN_0073c560` @0x73D30B: n = own purifiers + (computer,
  not campaign) `AIVirtualPurifiers[house+0x184]` (Rules+0x1320/+0x1324, 4,2,0), bonus
  n × PurifierBonus × value.
- **Plan tail.** `FUN_005052d0`: after the topological sort the plan holds one refinery;
  `AIExtraRefineries[house+0x184]` (Rules+0x1378; 2,1,0) more go in at rand(firstRefinery, n-1)+1
  (`AISlaveMinerNumber`-1 for a slave-miner side); then `<Side>BaseDefenseCounts` -1 placeholders
  each at rand(3, n-1)+1. `FUN_004fe200` resolves a -1 head: rand(0,99) < Rules+0xdd8[house+0x184]
  (`AIPickWallDefensePercent` 50/25/10) and `FUN_0050c160` succeeds → walls; else `FUN_00506d10`
  picks a defence, or the node is removed.
- **Second factories (RE-Q W6-c).** `WarLimit` (Rules+0x1110) and `BarracksLimit` (+0x1120) have no
  runtime reader (displacement scan: only the loader `FUN_00672520` and the BuildingType reader at
  the same offsets of another class). The plan holds each ownable type once; no house code adds a
  second war factory or barracks. `AIAlternateProductionCreditCutoff` (Rules+0x1300) drives the
  house production mode +0x1e4 in `FUN_00509520` (switches when the money drops under 1000). No
  surplus-spending path outside team demand was found.
- **Raise money.** `FUN_004fd7c0` rates "no refinery and cannot afford BuildRefinery" / "refineries
  but no harvester at work and cannot afford HarvesterUnit" as urgent; `FUN_004fdb30` then sells
  base-plan buildings from the tail backwards until money + refunds exceed the cost, sets
  house+0x564c / +0x5650 to it and abandons the house's other production.
- **Superweapons.** `FUN_00509710` switches on the SW type: 0 Nuke (needs an Enemy) and 2 Storm
  (`FUN_00509c20`; not while one rages, needs an Enemy) via `FUN_0050ca10` / `FUN_0050cf90`;
  5/6/8/11 via `FUN_00509af0` (the Enemy's base centre, else the own base, nearest free cell + (2,2));
  7, 9, 10 (Yuri). **No case for 1 IronCurtain, 3 ChronoSphere, 4 ChronoWarp.** `FUN_0050ca10`
  scores only the Enemy's ground objects from the Rules+0x1198.. arrays (ConYard, WarFactory,
  Power, TechCenter, Engineer, Thief, Harvester, MCV, APC, BaseDefense; other building 4, other
  unit 2, aircraft 1; cloaked rand(0, best+10)).
- **Curtain / Sphere through the AI teams.** ct=5/6 (`FUN_0041f0f0` / `FUN_0041f1a0`) are TRUE when
  the SW is present and charged ≥ Rules+0xd70 (`AIMinorSuperReadyPercent`, loader @0x66FA0E).
  Script 55 → `FUN_006ef1d0`, 56 → `FUN_006ef3c0`, 57 → `FUN_006ef690` (executor table 0x6E94E4):
  charged and full power → fire (55: curtain the team; 56/57: Sphere at the leader, Warp at the
  leader's target of building type / quarry <arg>) and complete; charging past the threshold → wait;
  otherwise complete.
- **Score time (RE-Q W6-d).** `MPScore.cpp` formats `Scenario+0x614` (an elapsed TimerClass on the
  `timeGetTime()>>4` clock, `FUN_006c82c0`) divided by 60 through `TXT_TIME_FORMAT_HOURS`
  (`FUN_005c91d0`, capped at 99:59:59; code @0x5C966D–0x5C9743): wall-clock play time (≈ ms/960),
  not game frames.
