# Building Repair & Sell — Design Spec

**Status:** DRAFT (awaiting user review)
**Roadmap slot:** `docs/superpowers/ROADMAP-completion.md` **P1 — "Repair & Sell"** ("The sidebar's two remaining inert buttons; core base management the player reaches for constantly. Tight scope." Gate: `--demo-repair`).
**Depends on:** the Buildings phase (`engine/building.{hpp,cpp}` — `Building`, `BuildingTypeDef`, `step_buildings`, `buildup_frame`, `kill_building`; `engine/sidebar.{hpp,cpp}` — `SidebarLayout`), the Economy phase (`World::house_credits_`), Unit Production (`engine/factory.cpp::exit_cell`, `power_totals`), the Engineer/Spy Capture phase (`engine/capture.{hpp,cpp}` — the pure-engine-TU precedent; `World::repair_building`, the **separate** engineer instant-repair path), Multi-House (`World::Side`, `house_side`, `ai_enabled`, `are_allied`), and the Skirmish/Maps phases (the gate harness).
**Calibration authority:** `docs/re/repair-sell.md` (the Repair + Sell hunt over YR 1.001 `gamemd.exe`, SHA1 `7BE4E5508655F8AF482AB3FF5965D196E1B6B6C7` — **ALREADY COMPLETE**, 953 lines). Every constant, formula, refusal rule and cadence in this spec cites it by §. Where repair-sell.md is `PROBABLE` or `[RE-FALLBACK]`, this spec carries the label forward verbatim and names what the plan's **Task 1 RE-GATE** adjudicates instead.
**Secondary authorities (already-recovered machinery this phase REUSES, never re-derives):** `docs/re/construction.md` §11a (the exact sidebar composition — the repair/sell button rects and gadget ids), §10 (`FactoryClass::Abandon` refunds `Cost_Of − balance`; the factory destructor abandons on destruction); `docs/re/building-render.md` §4 (`Shape_Number`), §5 (buildup frame count + period); `docs/re/capture.md` §A5 (engineer instant repair — a **different** code path that must stay separate).

## Goal

The sidebar has drawn a **Repair** button and a **Sell** button since the Buildings phase and **both are inert** (`main.cpp:2971-2973` blits `repair.shp`/`sell.shp` frame 0; nothing hit-tests them — `GameScene::handle`'s catch-all `return` at `main.cpp:3393` swallows the click). This phase makes them work, exactly as the hunt recovered them:

1. **Repair** — a **per-building toggle** (`BuildingClass::Repair`, `FUN_00446F40`, vtbl +0x19C) that heals **+8 HP** on a **global 14-frame phase**, charging an integer-arithmetic `stepCost` per step, auto-stopping at full health and silently stopping when the owner runs out of money.
2. **Sell** — a **timed mission** (`Mission::Selling` = `0x13`, `FUN_00449B70`), not an instant action: the building's **buildup animation plays MIRRORED** for the buildup duration, and only on completion do the credits land and the survivors spawn. The refund is **flat** (no HP scaling).

Both buttons become **mutually-exclusive cursor modes** with the recovered arm/refuse/cancel semantics. Demonstrated deterministically and guarded by the **20th permanent byte-identity gate `--demo-repair`** (`repair_hash`). The 19 existing gates stay bit-identical.

The hunt overturned three things a naive implementation would have gotten wrong, and this spec is built around them:

- **`URepairStep` / `URepairPercent` do not exist in this build.** One `RepairStep=8` (buildings + vehicles), one `IRepairStep=20` (infantry), one `RepairPercent=15%`. (§Ground truth, CONFIRMED.)
- **`stepCost` uses TWO integer divisions**, not the "obvious" `Cost × RepairPercent × damage_fraction`. The naive form is off by ~17 % on a Construction Yard (450 vs the real 375 credits to full). (§A2, CONFIRMED.)
- **The refund does not scale with health.** A building at 1 HP refunds exactly what a full-health one does. (§B1, CONFIRMED.)

And it *declines* to add two rules that do not exist: **there is no last-Construction-Yard guard and no captured-building guard** on selling (§B5, "CONFIRMED by absence"). Inventing either is exactly what the calibrate-first process exists to prevent.

## Global Constraints

These bind every task in the plan.

- **All 19 existing byte-identity gates MUST stay bit-identical:** combat logs=78, tank logs=144, base `5273b5e71ca009e1`, econ `7fed96cb98b68017`, prod `f0f4e1207ecd713c`, ai-base `cd1f7c6a29933f0a`, ai-team `312e92ed862a5e74`, ai-trigger `8334a6ca1531cabc`, transport `fe500f946e913645`, aircraft `a69642564cf64773`, naval `6aee2c82bc024e86`, superweapon `189496664f5eca68`, techtree `e73adae61ff6f71f`, capture `58b87b5bc63f7046`, shroud `d302852aa9517661`, multihouse `39412ae996cce8ae`, skirmish `e9ad3f1fd88495f9`, roster `333cd43d8b5ece6a`, maps `6e516005f120a1bb`. This phase adds the **20th: `--demo-repair`**.
- **Firewall discipline (the established four-part rule, non-negotiable). This phase needs it more than any previous one, because repair mutates `Building::hp` and sell mutates `Building::state` — and `world_hash` folds BOTH.**
  1. **New `World`/`Building` state is inert by default** — `Building::being_repaired = false`, `Building::selling = false`, `sell_time = 0.0`, `sold = false`. No existing spawn path, demo script or AI path ever sets any of them.
  2. **A `repair_sell_any_` early-return guard** on the single new step pass. It is opened ONLY by `World::toggle_repair` / `World::set_repair(…, true)` / `World::begin_sell`, none of which any existing demo calls. In every existing gate the pass costs one integer increment plus one branch.
  3. **New fields are excluded from `world_hash`** — `world_hash` (`main.cpp:5665`) folds only `Unit{cell_x,cell_y,hp,alive}` + `Building{type,cx,cy,hp,state}` and is **NOT modified**. The new state is hashed only by the phase's own `repair_fold`.
  4. **The new pass is a pure INSERTION** into `World::step` immediately after the existing dead-building blocking-release loop (`world.cpp:2859`) and before `step_combat` (`world.cpp:2861`) — no existing pass moves relative to any other.
- **Calibrate-first, no empirical guessing.** Every constant cites `docs/re/repair-sell.md` by § in a code comment. Everything the hunt left **PROBABLE** or **[RE-FALLBACK]** is pinned by the **Task 1 RE-GATE** *before* code depends on it; anything that stays unpinned keeps an explicit `[RE-FALLBACK: …]` comment naming what was adjudicated instead.
- **Module split:** pure sim arithmetic → a new testable `engine/repair_sell.{hpp,cpp}` TU (no SDL, no `World` — it includes only `building.hpp`, exactly the `production.cpp`/`factory.cpp` tier), registered in **BOTH** `cmake/ra2_sources.cmake` (→ `ra2engine` + Android) and `CMakeLists.txt` (→ `ra2assets_tests`). The `World` mutation entry points live in `engine/world.cpp` (already on `test_world`'s line). Button geometry → `engine/sidebar.{hpp,cpp}` (pure, already host-doctested). Cursor-mode gesture state → `engine/input.{hpp,cpp}` (pure, already host-doctested). Rules parsing → `include/ra2assets/rules.hpp` + `src/rules.cpp`. Rendering / sound / sidebar chrome → `engine/main.cpp`.
- **No assets committed.** Real-asset tests guard on `RA2_INSTALL_DIR` and skip cleanly (`MESSAGE("skipped: …"); return;`).
- Build: `cmake --build D:\ra2-android\build --target ra2assets_tests ra2engine`. doctest. Run exes from **PowerShell**, not Git Bash (Git Bash gives exit 127). Never pipe a live native exe through `Select-Object -First N`.
- **Demo gate** = two-run compute-and-self-compare (run 1 rendered hidden-window + start/end PNG, run 2 headless from a fresh `World`), `identical = (witness && hash)`, wedge guards → exit 1, **NO pinned hash literal in code** (the README pins it).
- **Gate runtime:** each gate is ≈2 min / ~3 GB. Run them **ONE PER FOREGROUND CALL**. Batching exceeds the 600 s tool timeout, auto-backgrounds, and reads as a false hang.
- Commit trailer for implementers: `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.

## Calibration summary (recovered ground truth — the behavior contract)

Confidence labels are repair-sell.md's own. **CONFIRMED** = read out of this binary and/or this install's real `rulesmd.ini`; **PROBABLE** = strong but single-source or not string-anchored; **[RE-FALLBACK]** = not determined.

### C0. The real rules values on this install (§Ground truth, CONFIRMED)

| Key | Rules offset | Value | Used for |
|---|---|---|---|
| `RepairStep` | `+0x16CC` | **8** | HP healed per repair tick |
| `RepairPercent` | `+0x16D0` | **0.15** | the `stepCost` multiplier |
| `RepairRate` | `+0x16E0` | **0.016** | minutes between repair steps |
| `RefundPercent` | `+0x1738` | **0.50** | sell payout fraction |
| `ConditionYellow` | `+0x1700` | **0.50** | the damaged-art threshold (already implemented) |
| `ConditionRed` | `+0x1708` | **0.25** | health-bar colour + the AI sell test (out of scope) |
| `AlliedSurvivorDivisor` | `+0x14F8` | **500** | survivors, side Allied |
| `SovietSurvivorDivisor` | `+0x14FC` | **250** | survivors, side Soviet |
| `ThirdSurvivorDivisor` | `+0x1500` | **750** | survivors, side Yuri (not a playable side here) |
| `BuildupTime` | `+0x1518` | **0.06** | the mirrored sell animation's duration |

> **`URepairStep` and `URepairPercent` DO NOT EXIST** — neither key is in `rulesmd.ini` and neither string is in `gamemd.exe`. **`SurvivorRate=.4` is present but DEAD** — the engine never reads it; the divisors are the whole story (§B3). Both CONFIRMED.

Note on parsing: `Rules::general_double` (`src/rules.cpp:220`) reads `[General]` then `[AudioVisual]` and uses `strtod`, which stops at `%` — so `RepairPercent=15%` reads back as `15.0` and the caller divides by 100, exactly as `ConditionYellow` already does (`main.cpp:1554`).

### C1. The repair cadence is a GLOBAL frame phase (§A2, CONFIRMED)

```
interval = trunc(RepairRate × 900.0)          // 0.016 × 900 = 14.4 -> 14, NOT 15
tick fires  iff  Frame % interval == 0
```

Two traps, both explicitly called out by the hunt: `__ftol` **truncates** (14, not 15 — a 7 % speed difference), and it is `Frame % interval`, so **all repairing buildings of all houses step on the same frames**. A building that starts repairing does not get its own timer and may wait 0–13 frames for its first step. At 15 Hz that is one step every **0.9333 s**.

### C2. Per repair step: +8 HP and an integer-arithmetic charge (§A2, CONFIRMED)

```
stepHP   = Rules.RepairStep                                     // 8, flat, building-independent
stepCost = max(1, trunc((Cost / (Strength / RepairStep)) * RepairPercent))
                        ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
                        BOTH divisions integer, truncating
```

`Health` **and** `EstimatedHealth` both advance and both clamp to `Type->Strength`. Worked from the real rules:

| Building | Cost | Strength | `Strength/8` | `Cost/that` | `×0.15` | **stepCost** | total to full |
|---|---:|---:|---:|---:|---:|---:|---:|
| `GAPOWR` | 800 | 1000 | 125 | 6 | 0.90 | **1** | 125 |
| `GACNST` | 3000 | 1000 | 125 | 24 | 3.60 | **3** | 375 |
| `GAWEAP` | 2000 | 1000 | 125 | 16 | 2.40 | **2** | 250 |

The naive `Cost × RepairPercent × damage_fraction` form gives 450 for `GACNST` — **17 % wrong**. The hunt's deviation #2 forbids re-deriving it. (These three rows are re-measured against this install by the Task 1 RE-GATE so the gate's expected numbers are measured, not remembered.)

### C3. Out of credits mid-repair: stop, silently (§A3, CONFIRMED)

The affordability check is **before** the spend. `credits < stepCost` ⇒ the per-building repair flag is **cleared**, no partial step, no free repair, **no EVA line** (`EVA_InsufficientFunds` is referenced only from the service-depot and a house path, never here). The player must click the wrench again after refilling. `Available_Money()` includes stored ore in the original; in the clone `house_credits_[h]` is the single pool, which is the same thing.

### C4. Repair is a per-building toggle with five off-ramps (§A4, CONFIRMED)

`Repair(control)` uses the classic Westwood `-1 = toggle / 0 = off / 1 = on` convention. Arming it on a damaged building plays a click sound + `EVA_Repairing` for the local player and sets `NeedsRepairs`. It turns **off** in exactly five ways: (1) clicking the same building again, (2) `Repair(0)` at the head of `Mission_Selling`, (3) reaching full health, (4) running out of money, (5) the engineer full-repair path calling `Repair(0)`.

**Which buildings accept the wrench** — `Can_Be_Click_Repaired` (`FUN_00452620`): `Health != 0`, `Type->ClickRepairable` (**default yes**), not in limbo / under construction, and a valid selection target.

The click queues a **network event**; `EventClass::Execute` resolves the target and calls `target->Repair(-1)`.

### C5. Damage state and the art swap (§A5, CONFIRMED)

`IsDamaged = (HealthRatio <= Rules.ConditionYellow)` — **`<=`, not `<`**. Repairing back above 50 % flips it to 0, re-issues every live building anim from the *undamaged* name table, and **destroys** the persistent damage smoke/fire anim. `ConditionRed` is **not** used for building art.

**The clone already satisfies the observable half of this for free.** `building_frame(b, shp_frame_count, condition_yellow)` (`building.cpp:62`) computes `hp/max_hp <= condition_yellow ? 1 : 0` **per draw**, with no latched flag anywhere in the sim. Repairing above the threshold therefore restores the healthy frame automatically, with **zero new code**. The 21-anim-slot table and the smoke/fire anim have no analogue in the clone's render model (see §Scope).

### C6. The engineer path is separate and must stay separate (§A6, CONFIRMED)

`TechnoClass::Repair_To_Full` (vtbl +0x40C) is an **instant, free, cadence-less** heal to `Strength` that additionally calls `Repair(0)`. It is already implemented as `World::repair_building(b_idx, by_house)` (`world.cpp:1420`). It shares only the damage-anim update. **Keep the two implementations separate**; the only new coupling is one line — `repair_building` must also clear the new `being_repaired` flag (off-ramp 5).

### C7. The refund formula — flat, with a human/AI asymmetry (§B1, CONFIRMED)

```
refund = trunc( trunc(Cost × HouseCostMult × CountryCostMult) × RefundPercent )   // human-controlled owner
refund = trunc( Cost × HouseCostMult × CountryCostMult )                          // AI-controlled owner
refund = trunc( Soylent × CountryCostMult )                                       // Soylent != 0; RefundPercent NOT applied
```

- **No health term exists anywhere** in `GetRefund`, `Refund_Amount` or the payout site. `GACNST` at 1 HP refunds `trunc(3000 × 0.5) = 1500`, same as at full health.
- `Soylent` (`type+0x614`) is an **absolute credit override** bypassing `RefundPercent`. No RA2 *building* in this `rulesmd.ini` sets it (all 20 `Soylent=` lines are on Yuri-Grinder infantry/vehicles) — but per deviation #3 the clone reads the key rather than hard-coding the `Cost` path.
- Both cost multipliers are **1.0** in a default skirmish (they are the `FactoryPlant`/`OrePurifier`/difficulty modifiers, which the clone does not have). They are carried as defaulted parameters so a future phase can supply them without changing the signature.
- `RefundPercent` is applied **only when the owning house is human-controlled**; AI houses receive the **unscaled** value. This is in the binary (`TEST AL,AL / JZ` at `0x00711399`), not a decompiler artefact. **The clone HAS AI houses** (`World::ai_enabled`), so deviation #3's escape clause does not apply — we implement the asymmetry and unit-test both branches, even though only the player can click Sell this phase.

### C8. Sell is a MISSION: the buildup, mirrored (§B2, CONFIRMED)

`Sell_Back(control)` queues **mission `0x13`**. `Shape_Number` (`FUN_0043EF10`) then mirrors the frame index **inside the BState-0 buildup range**:

```
frame = (buildup_start + buildup_count - stage) - 1
```

i.e. the buildup animation runs **backwards**, over the same frame count, at the same rate `building-render.md` §5 already pinned and the clone already implements:

```
image_frames  = buildup_shp_frames / 2
frame_period  = max(1, trunc(Rules.BuildupTime × 900 / image_frames))   ticks
sell_duration = image_frames × frame_period                            ticks @ 15 Hz
```

With `BuildupTime=.06` → `0.06 × 900 = 54` ticks ≈ **3.6 s** — exactly as long as the building took to appear. **The clone already stores exactly this number** as `BuildingArt::buildup_total` seconds (`main.cpp:1642-1655`), copied onto every placed building as `Building::buildup_total` (`world.cpp:812`). Sell reuses that value verbatim; no new duration math exists.

The recovered state machine:

| status | what happens |
|:--:|---|
| entry | **`Repair(0)`** — selling always cancels repair (`0x00449B81`) |
| 0 | deployed-vehicle undeploy; Service-Depot special case (§B4); **upgrade peel-off** (§B4); clear the damage-fire anims; status ← 1 |
| 1 | wait on the reverse buildup; release garrison occupants; **spawn survivors**; play the sell sound; status ← 2 |
| 2 | `EVA_StructureSold`; then either **undeploy into `UndeploysInto`** (§B6) or **`Credits += Refund_Amount()`**, return stored ore, `Limbo()`, `delete this` |

### C9. Survivors (§B3, formula CONFIRMED; crew type PROBABLE)

```
survivors = clamp( refund / SurvivorDivisor[side], 1, 5 )    // integer division of the REFUND
                                                            // 0 when !Type->Crewed or NoCrew
                                                            // divisor DOUBLED when HasBeenCaptured
```

| Building | Cost | refund | Allied (÷500) | Soviet (÷250) | Yuri (÷750) |
|---|---:|---:|---:|---:|---:|
| `GACNST` (`Crewed=yes`) | 3000 | 1500 | 3 | 5 (clamped from 6) | 2 |
| `GAPOWR` (`Crewed=no`) | 800 | 400 | **0** | 0 | 0 |
| `GAWEAP` (`Crewed=yes`) | 2000 | 1000 | 2 | 4 | 1 |

Survivors exit onto the building's own **foundation cells**; a survivor whose cell is blocked is **deleted, not displaced**. **Which infantry** — `Crew_Type` (`FUN_0044EB10`) returns the house's basic infantry (`E1`/`E2`/`INIT`), except a **25 % chance per survivor** of a special `InfantryType` at `Rules+0xF70` for building purpose-class `7`. The identity of `Rules+0xF70` is **PROBABLE** (engineer) — it was **not string-anchored**, and deviation #5 explicitly says to skip it until it is.

### C10. When selling is refused (§B5, CONFIRMED)

Refused for, and **only** for:

1. `Unsellable=yes` types (**none in stock RA2 `rulesmd.ini` — verified**);
2. buildings still **under construction** (`BState == 0`, or mission `0x12 Construction`);
3. buildings **already selling** (mission `0x13`);
4. buildings with **C4 planted** (`+0x6DF`);
5. objects with `+0x1D0` non-zero (**PROBABLE**: mind-controlled);
6. buildings **without a buildup** (`HasBuildUp == 0`) unless `Type->FirestormWall` — those take the **instant-removal branch** at `0x00447115` instead. In stock RA2 that is the wall/fence case.

> **There is no "last Construction Yard" special case and no "captured building" special case.** Both were searched for and neither exists: `HasBeenCaptured` affects only the survivor divisor and the AI repair trigger, never sellability. **CONFIRMED by absence.** Neither may be added.

The SELL **event** is additionally authoritative about **ownership** — you cannot sell someone else's building even if the cursor lets you click it (§C1).

### C11. The two buttons are mutually-exclusive cursor modes (§C1, CONFIRMED)

`DisplayClass` carries five adjacent one-byte mode flags — `RepairMode +0x11B0`, `SellMode +0x11B1`, `PowerToggleMode +0x11B2`, `PlanningMode +0x11B3`, `PlaceBeaconMode +0x11B4` — and **every setter clears the other four**. Each setter has the identical shape:

1. `-1/0/1` control convention; a no-op if the new value equals the current one;
2. **refused outright while a building placement is pending** (`DisplayClass+0x11A8 != 0`) — **placement wins**;
3. arming calls `Reset_Cursor` (`FUN_0048DBA0`), which **deselects everything currently selected**;
4. arming requires `PlayerPtr->[0x2F0] > 0` (**PROBABLE**: the count of owned buildings) — with no buildings the buttons are dead, and the sidebar greys them by the same predicate.

Sidebar `SidebarClass::Action` (`FUN_006A6E10`): button `0x8065` → `SetRepairMode(-1)`, `0x8066` → `SetSellMode(-1)` — **both toggle**. Cancel (`FUN_004AAC40`, ESC / right-click) cancels **exactly one thing per press**, in this priority order:

```
pending placement -> RepairMode -> SellMode -> PowerToggleMode
                  -> pending superweapon -> PlanningMode -> PlaceBeaconMode -> (just reset the cursor)
```

The button rects are already pinned by `construction.md` §11a (Allied/sidec01): **repair `repair.shp` (2 frames) at bar-relative (20, 166), 64×31, gadget id 101**; **sell `sell.shp` (2 frames) at (84, 166), 64×31, gadget id 102** — and those are literally the coordinates `main.cpp:2972-2973` already blits at.

### C12. Cursor art — **[RE-FALLBACK]** (§C2)

The mouse-cursor descriptor table is at `0x0082C038` (86 entries of `{startFrame, count, rate, miniStart, miniCount, hotX, hotY}`), but **the specific wrench and dollar-sign frame indices were not isolated** — the function mapping `RepairMode`/`SellMode` → cursor index was not found. Ruled out: `FUN_0048DBA0`, `FUN_00730DD0`, `FUN_00430FE0`, `FUN_004AC220`. The art is `MOUSE.SHP`.

### C13. The AI's own use of the same two entry points — **PROBABLE** (§A7)

The first half of `FUN_00450620` is the AI hand-holder: `Repair(1)` on damaged buildings when the house has money, `Sell_Back(1)` on buildings below `ConditionRed` when it does not. It reuses the **same two vfns** as the player. **Out of scope this phase** — see §Scope.

## Current-engine reality (the seams this attaches to)

File:line anchors, grepped against the post-maps tree (commit `3022385`) at write time — every anchor below was verified, not remembered.

**Sim side**

- **`struct Building`** — `engine/building.hpp:60`. Fields: `type, cx, cy, fw, fh, house, hp, max_hp, armor_ix, sight, grants_map_reveal, state (enum State{Buildup,Idle,Dead}), buildup_time, buildup_total, die_time, alive, blocking_released, primary, rally_x, rally_y, has_rally, grants_superweapon`. **No `being_repaired`, no `selling`, no `bstate`.**
- **`struct BuildingTypeDef`** — `engine/building.hpp:16`. Has `id, fw, fh, strength, armor_ix, cost, charge, power, adjacent, tech_level, conyard, prerequisite, image, buildup, cameo, explosion, produce_cash_*, sight, grants_map_reveal`. **Missing entirely: `Unsellable`, `ClickRepairable`, `Crewed`, `Soylent`.** A repo-wide grep for those four keys returns **zero** hits outside comments.
- **`kill_building(Building& b)`** — decl `building.hpp:159`, def `building.cpp:97-102`: `if (!b.alive || b.state == Dead) return; b.hp = 0; b.state = Dead; b.die_time = 0.0;`. It touches **nothing else** — passability is restored by `World::step`'s `blocking_released` loop (`world.cpp:2851-2859`), power is per-frame–derived, targeting is cleared by the *callers* (`combat.cpp:281-293`), and tombstoning happens in `step_buildings` at `kBuildingDieSecs = 1.0`.
- **`step_buildings(std::vector<Building>&, double dt)`** — `building.cpp:78-95`. Advances `buildup_time` → `Idle`, and `die_time` → `alive=false`.
- **`buildup_frame(const Building&, int buildup_shp_frame_count)`** — `building.cpp:69-76`: `image_frames = count/2; period = buildup_total/image_frames; f = clamp(trunc(buildup_time/period), 0, image_frames-1)`. **The exact function to mirror.**
- **`building_frame(const Building&, int shp_frame_count, double condition_yellow)`** — `building.cpp:62-67`: returns `1` when `hp/max_hp <= condition_yellow`, else `0`. Per-draw; no latch (see §C5).
- **`power_totals(buildings, types, house)`** — `production.cpp:15-26`. Filters `b.alive && b.house == house && b.state == Idle`. A selling building that stays `Idle` keeps its power — which is what the original does (the building is alive until `Mission_Selling` state 2).
- **`class World`** — `world.hpp:512`. `std::vector<Building> buildings_` (`:1183`), `std::vector<double> house_credits_` (`:1217`), `std::vector<BuildingTypeDef> types_` (`:1204`, populated by `set_economy`), `Occupancy occ_` (`:1192`), `passable_` (`:1184`), `grid_w_/grid_h_` (`:1193/:1194`), `houses_` (`:1141`). **There is NO global frame counter and NO general-purpose RNG in `World`** — the only RNG is `OreRng ore_rng_` (`:1200`).
- **`World::step(double dt)`** — decl `world.hpp:988`, def `world.cpp:2799-3068`. Pass order: `step_units` (2800) → `step_visibility` (2805) → `step_cargo` (2812) → `step_capture` (2819) → `step_flight` (2826) → `step_superweapon` (2833) → `step_tech_structures` (2838) → `step_turrets` (2843) → `step_buildings` (2850) → **dead-building blocking release loop (2851-2859)** → `step_combat` (2861) → MoveRequest dispatch → economy (2924) → hold-at-range → anim override → level patch → `if (ai_any_) {…}` (3059) → `step_victory()` (3067).
- **Firewall boilerplate to copy** — `World::step_capture` (`world.cpp:1521`): `if (!capture_any_) return;`; `World::step_superweapon` (`world.cpp:1331`): `if (!sw_any_) return;`.
- **`World::repair_building(int b_idx, int by_house)`** — `world.cpp:1420-1429`. The **engineer** instant heal: allied-or-own, not full HP, `b.hp = b.max_hp`, opens `capture_any_`. Orthogonal to the wrench.
- **`World::add_building(...)`** — `world.cpp:812-835`; sets `b.buildup_total` and clears `passable_` for the foundation.
- **`World::add_combat_unit(cell_x, cell_y, house, const UnitCombat&)`** — decl `world.hpp:537`, def `world.cpp:539`. Precondition: `walkable(x,y) && unit_at_cell(x,y) < 0`.
- **`World::launch_unit(factory_idx, factory_def, const UnitCombat&)`** — `world.cpp:877-915`. The exemplar "spawn a unit next to a building" path; uses `exit_cell(f, def, free_cell)` with `free_cell = walkable && occ_.is_free`.
- **`exit_cell(const Building&, const BuildingTypeDef&, free_fn)`** — `factory.hpp:33`, `factory.cpp:6-47`: deterministic ring walk from `(cx + fw/2, cy + fh/2)`, south first.
- **`World::Side { Allied, Soviet, Yuri, Neutral }`** — `world.hpp:780`; `house_side(int)` `:797`; `ai_enabled(int)` `:729`; `are_allied(a,b)` `:819`; `house_credits(int)` `:932`; `grant_credits_for_test` `:979`; `clear_selection()` `:545`.
- **Credits mutation has no helper** — every existing site is a raw `house_credits_[h] += / -=` inside a `World` member (`world.cpp:1380`, `1397-1398`, `1680`).
- **`world_hash`** — `main.cpp:5665`, with `fnv_mix` at `main.cpp:5657`. Folds `Unit{cell_x,cell_y,hp,alive}` + `Building{type,cx,cy,hp,state}`.

**Rules side**

- **`class Rules`** — `include/ra2assets/rules.hpp:17`, a by-reference wrapper over an `Ini`; **it stores no `[General]` fields**. `Rules::general_double(key, fallback)` — `src/rules.cpp:220-228` (`[General]` then `[AudioVisual]`, `strtod`). `Rules::section_double(id, key, fallback)` — `src/rules.cpp:230-237`. `Rules::is_construction_yard(id)` — `src/rules.cpp:208`, `is_truthy(ini_->get(id, "ConstructionYard", ""))` — **the exact pattern for a generic flag reader**. There is **no** generic `section_flag`.
- **`load_building_assets(ra2, language, rules, art)`** — `main.cpp:1484`; roster `kRoster[24]` at `:1536`; the per-type resolve loop at `:1604-1685`; `build_time_min = rules.general_double("BuildupTime", 0.06)` at `:1551`; `ba.condition_yellow = rules.general_double("ConditionYellow", 50.0) / 100.0` at `:1554`; `buildup_total` computed at `:1642-1655`.

**UI / input side**

- **`SidebarLayout`** — `sidebar.hpp:49-74`; `compute(view_w, view_h, types)` `sidebar.cpp:7-60`; `cameo_at` `:62`; `tab_at` `:69`; `SbRect` with `contains` `sidebar.hpp:13`. **No repair/sell rect member exists.**
- **Repair/Sell art** — `BuildingAssets::sb_repair/sb_sell` `main.cpp:387`, `has_repair_sell` `:390`, loaded from `sidec01.mix` at `main.cpp:1722-1724`, drawn **frame 0 only, no hit-test** at `main.cpp:2971-2973`:
  ```cpp
  if (ba.has_repair_sell) {
      blit_shp(ren, tc, "sb:rep", ba.sb_repair, 0, ba.sb_pal, lay.bar.x + 20, 166);
      blit_shp(ren, tc, "sb:sell", ba.sb_sell, 0, ba.sb_pal, lay.bar.x + 84, 166);
  }
  ```
- **`class Input`** — `input.hpp:74`. Armed-mode precedent: `sw_targeting_` `:235` with `arm_superweapon()/cancel_superweapon()/sw_targeting()` `:99-101` and the intercept block `input.cpp:307-342`; placement `place_type_` `:230` with `begin_placement/cancel_placement/placement_active` `:89-91` and the intercept block `input.cpp:349-384`. `InputActions` `:28-45` (accumulate-then-consume; `take_actions()` `:162`). `selected_building_` `:243`, `selected_building()` `:154`. `World::building_at_cell(x,y)` used at `input.cpp:204`. `resolve_ground_cell(px,py,cx,cy)` `:181`. **There is no mode enum of any kind.**
- **`GameScene`** — `main.cpp:3232`; ctor `:3234`; `handle` `:3326-3401` (sidebar pre-dispatch: SW slot `:3331` → tab `:3341` → cameo `:3349` → catch-all `return` `:3393`; Escape-cancels-placement `:3396`; `input_.handle(ev)` `:3400`); `update` `:3405-3550` (placement commit `:3495-3513`, `act` from `take_actions()`); `render` `:3554-3575` (`draw_sidebar` `:3570`); members `:3596-3642`.
- **Sound** — `audio_.mixer->play("PlaceBuilding")` at `main.cpp:3510` is the template; registration at `main.cpp:585-586`; EVA clip precedent `main.cpp:609-621` + `play_eva` at `:3533`.
- **Gate harness** — `fnv_mix` `main.cpp:5657`; `maps_fold` `:10292`; `struct DemoMapsResult` `:10316`; `simulate_maps` `:10328-10472`; `run_demo_maps` `:10480-10552`; dispatch arm `--demo-maps` `main.cpp:10797-10798`.
- **`draw_building`** — `main.cpp:1960`, buildup-vs-main SHP selection at `:2002-2005`; called at `:2275`.
- **Greenfield confirmed (repo-wide grep found NONE):** `repair_sell.hpp`, `RepairSellRules`, `repair_step_cost`, `repair_interval_frames`, `sell_refund`, `survivor_count`, `mirrored_buildup_frame`, `can_be_sold`, `can_be_click_repaired`, `being_repaired`, `toggle_repair`, `begin_sell`, `step_repair_sell`, `repair_sell_any_`, `set_survivor_crew`, `section_flag`, `BaseMode`, `set_repair_mode`, `set_sell_mode`, `repair_fold`, `run_demo_repair`.

## Architecture

Seven components. Each states what it does, its interface, its dependencies, and **how it stays additive / byte-identical**.

### A. `engine/repair_sell.{hpp,cpp}` — the pure arithmetic + predicates

New TU, world-free (includes only `building.hpp`), registered in **BOTH** cmake files.

```cpp
namespace ra2engine {

// The four [General] rules values this phase needs, resolved ONCE by the
// caller from rulesmd.ini. Defaults ARE this install's real values [RE:
// repair-sell.md §Ground truth], so a World that never calls
// set_repair_sell_rules still behaves exactly like the original.
struct RepairSellRules {
    int    repair_step    = 8;      // RepairStep    [§A1]
    double repair_percent = 0.15;   // RepairPercent [§A1]
    double repair_rate    = 0.016;  // RepairRate    [§A1] (minutes between steps)
    double refund_percent = 0.50;   // RefundPercent [§A1]
    int    allied_survivor_divisor = 500;   // [§B3]
    int    soviet_survivor_divisor = 250;   // [§B3]
    int    third_survivor_divisor  = 750;   // [§B3]
};

int  repair_interval_frames(double repair_rate);           // trunc(rate*900), floored at 1  [§A2]
bool repair_due(std::uint64_t frame, int interval);        // frame % interval == 0          [§A2]
int  repair_step_cost(int cost, int strength, int repair_step, double repair_percent);  // [§A2]
int  sell_refund(int cost, int soylent, double refund_percent, bool owner_is_human,
                 double country_mult = 1.0, double house_mult = 1.0);                   // [§B1]
int  survivor_divisor(const RepairSellRules& r, int side_ix);                           // [§B3]
int  survivor_count(int refund, int divisor, bool crewed, bool has_been_captured);      // [§B3]
bool can_be_click_repaired(const Building& b, const BuildingTypeDef& t);                // [§A4]
bool can_be_sold(const Building& b, const BuildingTypeDef& t, bool c4_applied);         // [§B5]
int  mirrored_buildup_frame(const Building& b, int buildup_shp_frame_count);            // [§B2]

}  // namespace ra2engine
```

`repair_step_cost` is transcribed **verbatim from deviation #2** — two integer divisions, then a truncating multiply, then `max(1, …)`. `sell_refund` is the three-branch recovered form. `mirrored_buildup_frame` is `buildup_frame` with `image_frames - 1 - f` and reading `b.sell_time`.

**Additive/inert:** a brand-new TU referenced by nothing on any gate path until Task 4 wires `step_repair_sell`.

### B. `Building` / `BuildingTypeDef` — four + four new fields

```cpp
// on Building (building.hpp:60), appended after grants_superweapon:
bool   being_repaired = false;  // [§A2 bldg+0x6E8] the per-building wrench toggle
bool   selling        = false;  // [§B2] mission 0x13 is running on this building
double sell_time      = 0.0;    // seconds elapsed in the MIRRORED buildup
bool   sold           = false;  // completed via sell (suppresses the death explosion)

// on BuildingTypeDef (building.hpp:16), appended after grants_map_reveal:
bool unsellable       = false;  // Unsellable=       [§B5]  (none in stock rules)
bool click_repairable = true;   // ClickRepairable=  [§A4]  DEFAULT YES
bool crewed           = false;  // Crewed=           [§B3]
int  soylent          = 0;      // Soylent=          [§B1]  absolute refund override
```

**Deliberately NOT added:** a `Building::State::Selling` enumerator. Adding a fourth state would change what every `state != Idle` / `state == Dead` test in the tree means (power, targeting, placement, victory) and would make `world_hash` see a new value class. In the original the building is **alive and functioning** through the whole sell mission — it is only `Limbo()`d at state 2 — so keeping `state == Idle` while `selling == true` is both simpler and more faithful. Power therefore keeps flowing until the sale completes, matching `Mission_Selling`.

**Additive/inert:** all eight default to today's behavior; `world_hash` reads none of them.

### C. `Rules::section_flag` + the `load_building_assets` resolve

```cpp
// include/ra2assets/rules.hpp (beside section_double)
bool section_flag(const std::string& id, const std::string& key, bool fallback) const;
```
`src/rules.cpp`: `const std::string v = ini_->get(id, key, ""); return v.empty() ? fallback : is_truthy(v);` — the `is_construction_yard` pattern, generalized. One accessor serves `Unsellable`, `ClickRepairable` and `Crewed`; `Soylent` uses the existing `section_double`.

In `load_building_assets`'s resolve loop (`main.cpp:1604-1685`), four appended lines, plus one `RepairSellRules` resolved next to `build_time_min`/`condition_yellow` (`main.cpp:1551-1554`) and stored on `BuildingAssets`.

**Additive/byte-identical:** `Unsellable`, `Crewed` and `Soylent` are absent from every stock building section → `false`/`false`/`0`; `ClickRepairable` is absent → its recovered default `true`. None of the four is read by anything until Task 4. `general_double` on four new keys reads INI text already in memory — no new file/mix access, no ordering change.

### D. `World` — state, entry points, and one new firewalled pass

```cpp
// --- Repair & Sell [RE: repair-sell.md] ---------------------------------
void set_repair_sell_rules(const RepairSellRules& r);
// Repair(control) [§A4]: -1 toggle / 0 off / 1 on. Refuses (returns false)
// unless b_idx is an alive, non-Dead, non-selling building owned by by_house
// whose type is click-repairable and whose hp is in (0, max_hp). Opens
// repair_sell_any_ when it turns the flag ON.
bool set_repair(int b_idx, int by_house, int control);
bool toggle_repair(int b_idx, int by_house) { return set_repair(b_idx, by_house, -1); }
// Sell_Back(-1) [§B2/§B5]: start mission 0x13. Refuses per can_be_sold +
// ownership. Calls set_repair(b_idx, by_house, 0) FIRST [§B2 entry]. A type
// with no buildup art takes the INSTANT branch [§B5 item 6]. Opens
// repair_sell_any_.
bool begin_sell(int b_idx, int by_house);
// The crew UnitCombat for a side [§B3]. World is asset-free, so the CALLER
// resolves E1/E2 and registers it (the order_spy_infiltrate precedent).
// UNREGISTERED (the default) -> that side spawns ZERO survivors.
void set_survivor_crew(int side_ix, const UnitCombat& c);
bool building_repairing(int b_idx) const;
bool building_selling(int b_idx) const;
std::uint64_t repair_frame() const;      // the global cadence counter [§A2]
void step_repair_sell(double dt);        // public so the gate can drive it directly
```

Private state: `RepairSellRules repair_rules_;`, `bool repair_sell_any_ = false;`, `std::uint64_t repair_frame_ = 0;`, `std::vector<UnitCombat> survivor_crew_;` + `std::vector<std::uint8_t> survivor_crew_set_;`.

`step_repair_sell(dt)`:

```
frame = repair_frame_++;            // ALWAYS advances -> an ABSOLUTE frame phase [§A2]
if (!repair_sell_any_) return;      // FIREWALL

// --- sell missions (mirrored buildup), ascending building index
for each alive building b with b.selling:
    b.sell_time += dt;
    if (b.sell_time < b.buildup_total) continue;
    complete_sell(b);               // see §Behavior B6

// --- repair tick: a GLOBAL frame phase, all buildings together [§A2]
interval = repair_interval_frames(repair_rules_.repair_rate);   // 14
if (!repair_due(frame, interval)) return;
for each alive, non-Dead building b with b.being_repaired, ascending index:
    if (b.selling) { b.being_repaired = false; continue; }       // [§B2 entry]
    cost = repair_step_cost(type.cost, type.strength, rules.repair_step, rules.repair_percent);
    if (house_credits_[b.house] < cost) { b.being_repaired = false; continue; }   // [§A3]
    house_credits_[b.house] -= cost;
    b.hp = min(b.hp + rules.repair_step, b.max_hp);              // [§A2]
    if (b.hp >= b.max_hp) b.being_repaired = false;              // [§A2 auto-stop]
```

Inserted into `World::step` at `world.cpp:2859` — after `step_buildings` + its blocking-release loop (so a building that flipped Buildup→Idle *this* frame is already repairable, and a building sold *last* frame has already had its foundation freed) and before `step_combat` (so a completed sale is `Dead` before targeting runs).

**Additive/byte-identical:** every existing gate leaves `repair_sell_any_ == false`, so the pass is one increment plus one branch, mutating nothing. `world_hash` is untouched. `repair_building` (the engineer path) gains exactly one line — `b.being_repaired = false;` (off-ramp 5) — which is a no-op wherever the flag was never set.

### E. `SidebarLayout` — the two button rects

```cpp
// sidebar.hpp, beside the existing constants [RE: construction.md §11a]
constexpr int kRepairBtnX = 20;   // bar-relative x, Allied/sidec01
constexpr int kSellBtnX   = 84;
constexpr int kSideBtnY   = 166;
constexpr int kSideBtnW   = 64, kSideBtnH = 31;

// on SidebarLayout:
SbRect repair_btn, sell_btn;                     // gadget ids 101 / 102
bool repair_btn_at(int px, int py) const;
bool sell_btn_at(int px, int py) const;
```

`compute` fills them as `{x0 + kRepairBtnX, kSideBtnY, kSideBtnW, kSideBtnH}` / `{x0 + kSellBtnX, …}` — **the same two coordinates `main.cpp:2972-2973` already blits at**, so the rects provably sit under the drawn art. `draw_sidebar` then uses `lay.repair_btn.x/y` instead of its hardcoded literals and picks **frame 1 while the mode is armed** (`repair.shp`/`sell.shp` each carry 2 frames — §11a).

**Additive/byte-identical:** two new members on a struct nothing hashes; the drawn pixel positions are unchanged by construction. Pinned by a `tests/test_sidebar.cpp` case against the literals currently in `main.cpp`.

### F. `Input` — the two cursor modes

```cpp
// [RE: repair-sell.md §C1] Two of the original's FIVE mutually-exclusive
// DisplayClass mode flags. PowerToggle/Planning/PlaceBeacon have no feature
// behind them in this engine and are NOT modeled (see §Scope); the exclusion
// set is {placement, sw_targeting, Repair, Sell}.
enum class BaseMode { None, Repair, Sell };

void     set_repair_mode(int control);   // -1 toggle / 0 off / 1 on  [§C1]
void     set_sell_mode(int control);
BaseMode base_mode() const;
// The §C1 cancel chain, ONE thing per press: placement -> Repair -> Sell ->
// sw_targeting -> (nothing). Returns true if something was cancelled.
bool     cancel_one_mode();
// PlayerPtr->[0x2F0] > 0 [§C1, PROBABLE]: arming ANY mode is refused when the
// player owns no buildings. main.cpp refreshes this once per frame.
void     set_owned_buildings(int n);
```

Setter semantics, transcribed from `SetSellMode` (§C1):
1. compute `next` from `control` (`-1` → toggle, `0` → off, `1` → on); `next == cur` → return;
2. `if (placement_active()) return;` — **placement wins**;
3. clear the other mode **and** `sw_targeting_ = false`;
4. arming: `if (owned_buildings_ <= 0) { mode_ = None; return; }` else `mode_ = <the mode>; world_.clear_selection(); selected_building_ = -1;` (this is `Reset_Cursor`).

`InputActions` gains `bool repair_clicked; int repair_building = -1; bool sell_clicked; int sell_building = -1;`.

`Input::handle` gains one intercept block, placed **immediately after** the `placement_active()` block and before the ordinary gesture switch (mutual exclusion makes the relative order of the mode blocks behaviorally irrelevant; this position mirrors §C1's cancel priority). A left Down+Up (no drag) resolves the ground cell via `resolve_ground_cell`, then `world_.building_at_cell(cx,cy)`; an alive, non-Dead, **house-0** building reports `repair_clicked`/`sell_clicked` + its index. A click on nothing, or on someone else's building, is swallowed with no report (§C1: "you cannot sell someone else's building even if the cursor lets you click it"). A right-click calls `cancel_one_mode()`. **The mode is NOT one-shot** — the original stays armed until cancelled, so several buildings can be repaired/sold in a row.

**Additive/inert:** `mode_ == None` and `owned_buildings_ == 0` by default; no demo constructs an `Input` in a mode, and the two new intercept blocks are unreachable without an explicit `set_*_mode` call. `Input` is already host-doctested (`tests/test_input.cpp`).

### G. `GameScene` wiring + `--demo-repair`

- `GameScene::handle` — before the sidebar catch-all `return` (`main.cpp:3393`), two hit-tests: `lay_.repair_btn_at(ev.x, ev.y)` → `input_.set_repair_mode(-1)`; `lay_.sell_btn_at(…)` → `input_.set_sell_mode(-1)`; each plays the existing click sound and `return`s. The Escape branch at `:3396` is extended to `input_.cancel_one_mode()` (which already handles placement first).
- `GameScene::update` — refresh `input_.set_owned_buildings(<count of alive house-0 buildings>)` once per frame beside the existing per-frame recomputes; consume `act.repair_clicked` → `world_.toggle_repair(act.repair_building, 0)` and `act.sell_clicked` → `world_.begin_sell(act.sell_building, 0)`, each with an audio cue. Register `E1`/`E2` crews once in the ctor via `world_.set_survivor_crew`.
- `GameScene::render` / `draw_building` — a selling building draws its **buildup SHP mirrored** (`mirrored_buildup_frame`) instead of the main SHP; a `sold` building draws **no explosion**. A `being_repaired` building draws the existing damaged/healthy frame unchanged.
- **`--demo-repair`** — the 20th gate, mirroring `run_demo_maps` exactly (see §Testing).

**Additive/inert:** all UI; no gate constructs a `GameScene`.

## Data model (exact additions)

| Field / type | On | Default | In `world_hash`? | Why additive/inert |
|---|---|---|---|---|
| `struct RepairSellRules` + 9 free functions | (new TU `engine/repair_sell.hpp`) | — | No | New pure TU; nothing on a gate path references it. |
| `being_repaired`, `selling`, `sell_time`, `sold` | `ra2engine::Building` | `false/false/0.0/false` | **No** | `world_hash` folds only `type,cx,cy,hp,state`. No spawn path sets them. |
| `unsellable`, `click_repairable`, `crewed`, `soylent` | `ra2engine::BuildingTypeDef` | `false/true/false/0` | No | Keys absent from every stock section; read by nothing until Task 4. |
| `bool section_flag(id,key,fallback)` | `ra2assets::Rules` | — | No | New accessor; no existing caller. |
| `RepairSellRules repair_sell` | `struct BuildingAssets` (`main.cpp:397`) | recovered values | No | `BuildingAssets` is not hashed. |
| `repair_rules_`, `repair_sell_any_`, `repair_frame_`, `survivor_crew_`, `survivor_crew_set_` | `ra2engine::World` (private) | recovered / `false` / `0` / empty | No | The firewall bool is opened only by the three new entry points. |
| `set_repair_sell_rules`, `set_repair`, `toggle_repair`, `begin_sell`, `set_survivor_crew`, `building_repairing`, `building_selling`, `repair_frame`, `step_repair_sell` | `ra2engine::World` (public) | — | No | New methods; `step_repair_sell` is the only one `World::step` calls, and it early-returns. |
| `repair_btn`, `sell_btn`, `repair_btn_at`, `sell_btn_at` + 5 constants | `ra2engine::SidebarLayout` | rects at the already-blitted coords | No | Geometry only; drawn pixels unchanged. |
| `BaseMode`, `set_repair_mode`, `set_sell_mode`, `base_mode`, `cancel_one_mode`, `set_owned_buildings`, `mode_`, `owned_buildings_` | `ra2engine::Input` | `None` / `0` | No | Unreachable without an explicit setter call. |
| `repair_clicked`, `repair_building`, `sell_clicked`, `sell_building` | `ra2engine::InputActions` | `false/-1/false/-1` | No | Only the new intercept blocks set them. |

**None of it enters `world_hash`.** The new `--demo-repair` gate uses its **own** `repair_fold`, exactly as every previous phase did.

## Behavior details (each tied to a recovered §)

- **B1 — Repair cadence (§A2, CONFIRMED).** `interval = trunc(RepairRate × 900)`, floored at 1 → **14** frames at `.016`. `repair_frame_` increments **unconditionally at the top of `step_repair_sell`, before the firewall**, so the phase is absolute (a building armed at frame 100 first steps at frame 112, waiting 12 frames) exactly as `Frame % interval` does — not a per-building timer. All repairing buildings of all houses step on the same frames.
- **B2 — Repair step (§A2, CONFIRMED).** `+8 HP` flat, clamped to `max_hp`; charge `repair_step_cost` (two integer divisions). The clone has one HP field, so `Health`/`EstimatedHealth` collapse to `b.hp`. Order per building: affordability check → spend → heal → auto-stop test.
- **B3 — Out of credits (§A3, CONFIRMED).** Checked **before** spending; on failure the flag is cleared and the loop moves on — no partial step, no EVA. Because the check is per building inside one global tick, an early building can drain the pool and a later one is simply switched off the same frame; that is what the original does (one shared `Available_Money()` consulted per building in object order).
- **B4 — Auto-stop and the five off-ramps (§A4, CONFIRMED).** (1) toggle via `set_repair(-1)`; (2) `begin_sell` calls `set_repair(…, 0)` first; (3) `hp >= max_hp` after a step; (4) out of credits; (5) `repair_building` (engineer) clears it.
- **B5 — Which buildings accept the wrench (§A4, CONFIRMED).** `can_be_click_repaired`: `b.alive`, `state != Dead`, `state != Buildup` (the "in limbo / being built" test), `t.click_repairable`, `hp > 0`, `hp < max_hp`. Ownership is checked by the `World` entry point, not the predicate.
- **B6 — The sell mission (§B2, CONFIRMED).** `begin_sell` refuses per §C10, calls `set_repair(0)`, then sets `selling = true; sell_time = 0.0`. Each frame `sell_time += dt`; at `sell_time >= buildup_total` the sale **completes in one frame**, in this order:
  1. **restore `passable_`** for the foundation cells and set `blocking_released = true` (the same three lines `World::step`'s release loop runs — done here so survivors can stand on the footprint this frame);
  2. **spawn survivors** on the foundation cells in `foundation_cells` order, `n = survivor_count(refund, divisor(side), t.crewed, /*has_been_captured=*/false)`, skipping (i.e. **deleting**, per §B3) any cell that is not `walkable && occ_.is_free`; a side with no registered crew spawns none;
  3. **`house_credits_[b.house] += refund`** (`sell_refund`, §C7 — flat, `Soylent`-aware, human/AI-aware);
  4. **`kill_building(b); b.sold = true; b.selling = false;`**.

  Steps 1–4 collapse the original's states 1 and 2 into the single frame the animation ends on; at 15 Hz the observable difference is nil, and it keeps the pass deterministic and single-pass. The clone has no stored-ore-per-building, so "return stored ore" is a no-op.
  A type with **no buildup art** (`t.buildup.empty()` / `buildup_total <= 0`) takes the **instant branch** (§B5 item 6): the completion block runs immediately inside `begin_sell`.
- **B7 — The refund is FLAT (§B1, CONFIRMED).** No HP term. `Soylent != 0` overrides and bypasses `RefundPercent`. `RefundPercent` applies only when `!ai_enabled(owner)`; an AI owner gets the unscaled value.
- **B8 — Survivors (§B3, CONFIRMED formula).** `clamp(refund / divisor, 1, 5)`, zero when `!crewed`. Divisor by `house_side`: Allied 500, Soviet 250, Yuri 750, Neutral → 0 survivors. `has_been_captured` is passed as **`false`** — the clone's `capture_building` does not record a captured flag on the building, so the ×2 divisor is implemented in the pure function and unit-tested but never triggered; `[RE-FALLBACK: captured-flag plumbing]` is carried in the code comment. The **25 % special crew type is CUT** (§Scope).
- **B9 — Power while selling (§B2, design consequence).** The building keeps `state == Idle` for the whole mission, so `power_totals` keeps counting it until the frame it dies — matching the original, where the object is alive until `Limbo()`.
- **B10 — Refusals (§B5, CONFIRMED).** `can_be_sold`: `b.alive && state != Dead && state != Buildup && !b.selling && !t.unsellable && !c4_applied`. `c4_applied` is a parameter, always `false` today (no C4 mechanic until roadmap P4) — written as a parameter so the P4 phase wires it without touching the predicate. **No last-ConYard rule. No captured rule.** Ownership (`b.house == by_house`) is enforced by `World::begin_sell`, mirroring the SELL event's ownership check.
- **B11 — Modes (§C1, CONFIRMED).** Both sidebar buttons toggle. Arming is refused while a placement is pending and when the player owns no buildings; arming clears the unit selection and the factory selection; arming either mode clears the other and `sw_targeting_`. ESC / right-click cancels exactly one thing, in the recovered priority order.
- **B12 — Cursor feedback (§C2, [RE-FALLBACK] carried).** The engine has **no SHP cursor system at all** (no `MOUSE.SHP` load, no `SDL_SetCursor`, no frame constants — the OS arrow is used everywhere). Rather than invent frame indices for an unresolved `[RE-FALLBACK]` *and* build a cursor subsystem, feedback is delivered through channels the engine already has: the sidebar button draws its **pressed frame 1** while armed (2-frame art, §11a), and the hovered own-building's foundation is outlined via the existing `draw_placement_cursor` idiom — green while Repair is armed, yellow while Sell is armed. §C2 stays open and is re-stated in the README carry-forwards.

## Scope / non-goals (carry-forwards — documented, NOT implemented)

Deliberate cuts, each with its justification.

| Carry-forward | repair-sell.md ref | Why deferred |
|---|---|---|
| **`UndeploysInto` — selling a Construction Yard undeploys it to an MCV** | §B6 **CONFIRMED (rule) / PROBABLE (the two option globals)** | **CUT deliberately.** The *rule* is confirmed, but three things block it. (1) The clone has **no deploy/undeploy machinery whatsoever** — no `UndeploysInto` field, no MCV deploy gesture, no unit→building transform; delivering one branch of sell would mean building the entire deploy feature, which is its own phase. (2) The two gating globals `0x00A8A1A0` (MP session) and `0x00A8A288` (the MCV-redeploy lobby option) are **PROBABLE, not string-anchored**, and the option needs a `[MultiplayerDialogSettings] MCVRedeploys` read plus a lobby toggle. (3) The undeployed unit's health scaling and veterancy carry are both **PROBABLE**. Critically, **cutting it is not an invention**: the recovered code itself takes the plain-refund branch whenever `UndeploysInto == 0`, the session is not MP, the owner is not human, **or** the option is off — so the clone's behavior is a *reachable configuration of the original*, not a deviation from it. Re-open together with MCV deploy (roadmap P6-ish). |
| **AI auto-repair / auto-sell below `ConditionRed`** | §A7 **PROBABLE** | **CUT deliberately.** Two independent reasons. (1) The recovered decision chain depends on **six offsets that were never string-anchored** — `Rules+0x1444` (an IQ index), `Rules+0x1758` (a money threshold), `Owner+0x245/+0x1D0/+0x1D4`, `this->[0x3D1]`, `Type->[0xEB8]` — so implementing it would mean *guessing* what those rules keys are, which is precisely what the calibrate-first directive forbids. (2) The sell branch calls **`Random(0, 50)`**, injecting a new RNG draw into the AI step passes; the three AI gates (`ai-base`, `ai-team`, `ai-trigger`) are byte-identity gates over exactly those passes. Needs its own RE pass to string-anchor the IQ/threshold keys first. Until then the AI simply never repairs or sells — which is a strictly smaller behavior, never a wrong one. |
| **Upgrade peel-off (one upgrade refunded + removed per sell click)** | §B4 **CONFIRMED** | No upgrade buildings exist in the clone's 24-type roster (no power turbines, no `Upgrades[3]`). The rule is recorded; implementing it today would be dead code with no way to test it. Deviation #8 says exactly this. |
| **Service Depot: selling a docked unit instead of the depot** | §B4 **PROBABLE** | No Service Depot in the roster, no `UnitReload` field, no unit-docking mechanic; and the radio-message query `vtbl+0x274` was not isolated. Adjudicated (not silently dropped) by Task 1 item 2. |
| **Selling UNITS (the SELL event's unit branch)** | §C1 **CONFIRMED** | Requires a depot or a Grinder to stand on — neither exists. The ownership + "standing on a building" gate is recorded for whichever phase adds one. |
| **Garrison occupant release on sell** | §B3(a) **CONFIRMED** | No garrisonable buildings, no `UnitAbsorb`/`SecretLab`, no `OccupantCount`. Already a carry-forward from the Capture phase. |
| **The 25 % special crew type (`Rules+0xF70`)** | §B3 **PROBABLE** | The identity of `Rules+0xF70` was **not string-anchored**, and deviation #5 says to skip it until it is. It would also inject an RNG draw into the sell path, which the new gate hashes — determinism first. Every survivor is the house's basic infantry (`E1`/`E2`), the "defensible first cut" the hunt names. |
| **The 21-slot damaged-anim table + the smoke/fire anim destruction** | §A5 **CONFIRMED** | The clone's building render model is a **2-frame healthy/damaged SHP** (`building_frame`), with no per-building anim slots and no persistent damage anim to destroy. The *observable* rule (`HealthRatio <= ConditionYellow` flips the art, and repairing above it flips it back) is already implemented and is exercised by the new gate. The 21-slot table has nothing to attach to. |
| **`ConditionRed` (health-bar colour)** | §A5/C0 | Not read anywhere in the clone today; its only in-scope consumer would be the AI sell test, which is cut. Recorded, unused. |
| **C4-planted and mind-control sell refusals** | §B5 items 4 (CONFIRMED) / 5 (**PROBABLE**) | Neither mechanic exists (C4 arrives with SEAL in roadmap P4; there is no mind control). `can_be_sold` takes `c4_applied` as an explicit parameter so P4 wires it in one line; the mind-control test is PROBABLE (`+0x1D0` was not identified) and is adjudicated by Task 1 item 6 rather than guessed. |
| **`PowerToggleMode` / `PlanningMode` / `PlaceBeaconMode`** | §C1 **CONFIRMED** | Three of the five mutually-exclusive flags have **no feature behind them in this engine** (no per-building power toggle, no waypoint planning, no beacons). Modeling them as enum values nothing can ever set would be dead state. The recovered five-flag exclusion + cancel chain is implemented over the set the engine actually has — `{placement, sw_targeting, Repair, Sell}` — and the full chain is recorded in the code comment so a later phase slots its mode into the documented position. |
| **`MOUSE.SHP` wrench / dollar cursor frames** | §C2 **[RE-FALLBACK]** | The frame indices are genuinely unresolved *and* the engine has no SHP-cursor subsystem to put them in. Building one to display an unknown frame index is the worst of both worlds. See §B12 for the feedback that ships instead. Task 1 item 8 tries once to pin the indices; if it fails the `[RE-FALLBACK]` stays open in §C2. |
| **Mid-production abandon + primary-factory re-eval on sell** | §B4 **[RE-FALLBACK]** | `Mission_Selling` calls two undecompiled functions when the building has a live `Factory`. The most likely mechanism is the `FactoryClass` **destructor's** abandon+refund (`construction.md` §10, `FUN_004C9930`, CONFIRMED there) — but §B4 states plainly that "the link from `Mission_Selling` to `Abandon` was not proven". Task 1 item 4 adjudicates; the FALLBACK is to reuse whatever the clone's existing building-death path already does for a factory, so sell and destruction behave identically (which is the one thing that is certainly true of the original, since both end in `~FactoryClass`). |
| **Network-event routing of the repair/sell click** | §C1 **CONFIRMED** | The original queues a REPAIR/SELL `EventClass` and acts on execution. The clone has no netcode (an explicit roadmap non-goal), so the gesture calls the `World` entry point directly. The event's **ownership check** is preserved (it is the load-bearing part) — `World::begin_sell`/`set_repair` verify `b.house == by_house`. |
| **EVA lines (`EVA_Repairing`, `EVA_StructureSold`) and the click sounds** | §A4/§B2 | The engine loads exactly one EVA clip today (`ceva064.wav`). Adding two more is a small, self-contained audio task with no simulation effect; the existing `mixer->play(<name>)` cue is used instead so the buttons are audible. Recorded so the audio phase can add the real lines. |

## Testing strategy

- **Pure host doctests (no install required):**
  - **`tests/test_repair_sell.cpp`** (new) — `repair_interval_frames(0.016) == 14` (the truncation trap, plus `0.02 → 18`, `0.0 → 1`); `repair_due` on the exact 14-multiple boundaries; `repair_step_cost` reproducing the §A2 table **exactly** (GAPOWR 800/1000 → 1, GACNST 3000/1000 → 3, GAWEAP 2000/1000 → 2) **and** a case proving the naive float form differs (GACNST total to full = 375, not 450); the `max(1, …)` clamp; `strength < repair_step` (division-by-zero guard); `sell_refund` human (`3000 → 1500`) vs AI (`3000 → 3000`), `Soylent` override bypassing `RefundPercent`, truncation of an odd cost; `survivor_divisor` per side incl. Neutral → 0; `survivor_count` reproducing the §B3 table (GACNST Allied 3 / Soviet 5-clamped / Yuri 2; GAPOWR `!crewed` → 0), the `<1 → 1` floor, the `>5 → 5` cap, the captured ×2 divisor; `can_be_click_repaired` / `can_be_sold` over every refusal in §A4/§B5 **plus explicit negative cases proving a full-health ConYard and a captured building ARE sellable** (the "CONFIRMED by absence" guard); `mirrored_buildup_frame` running `image_frames-1 → 0` as `sell_time` sweeps `0 → buildup_total`, and equal-and-opposite to `buildup_frame` at every stage.
  - **`tests/test_building.cpp`** (extend) — the four new `Building` fields default inert; `kill_building` still leaves them alone.
  - **`tests/test_rules.cpp`** (extend) — `section_flag` present/absent/`fallback`/truthy-forms.
  - **`tests/test_sidebar.cpp`** (extend) — `repair_btn`/`sell_btn` pin: at 1280×720, `x0 + 20` / `x0 + 84`, `y == 166`, `64×31` — **the literal-for-literal match against `main.cpp:2972-2973`** — plus hit-testing inside/outside and non-overlap with `side1`/`tabs`.
  - **`tests/test_input.cpp`** (extend) — mode toggle `-1/0/1`; arming refused while `placement_active()`; arming refused with `owned_buildings_ == 0`; arming Repair clears Sell **and** `sw_targeting_`; arming clears the unit selection and `selected_building_`; `cancel_one_mode` priority (placement first, then Repair, then Sell, then sw, then `false`); a left click in Repair mode over an own building reports `repair_clicked` + index; over an **enemy** building reports nothing; over empty ground reports nothing; the mode **stays armed** after a click.
  - **`tests/test_world.cpp`** (extend) — `set_repair`/`toggle_repair`/`begin_sell` refuse on a wrong-house / Dead / Buildup / already-selling / `unsellable` building and open `repair_sell_any_` only on success; `step_repair_sell` is a strict no-op with the firewall closed (drive 100 frames on a stock world and assert nothing moved); a scripted repair run heals exactly `+8` on frames `14, 28, 42…` and charges exactly `stepCost` each time; auto-stop at `max_hp`; out-of-credits clears the flag with hp unchanged; `begin_sell` clears `being_repaired`; a scripted sell run leaves credits **unchanged** until the completion frame, then adds exactly the flat refund, spawns the expected survivor count on foundation cells, sets `sold`, and frees the foundation; an instant (no-buildup) sell completes inside `begin_sell`.
- **Real-asset test (guarded on `RA2_INSTALL_DIR`):** the four new `[General]` keys and the four new per-type keys resolve off the real `rulesmd.ini` to the §C0 values (`RepairStep=8`, `RepairPercent=15%→0.15`, `RepairRate=.016`, `RefundPercent=50%→0.50`, divisors 500/250/750), and `GACNST` reports `Crewed=yes`, `GAPOWR` `Crewed=no`, and **no rostered type reports `Unsellable=yes`** (§B5's "none in stock rules — verified").
- **New 20th gate `--demo-repair`** — mirrors `run_demo_maps` structurally (two-run rendered/headless self-compare, wedge guards, no pinned literal). Five scripted stages on a fixed Allied base:
  1. **repair cadence + cost** — damage a `GACNST` to a known hp, `toggle_repair`, step frames one at a time, and witness the exact frames on which hp moved and by how much, plus the credit delta;
  2. **auto-stop** — keep stepping to full health and witness the flag clearing itself;
  3. **out of credits** — damage a second building, seed the house with less than one `stepCost`, arm, step past a tick boundary, and witness `hp` unchanged + flag cleared;
  4. **refusals** — witness `begin_sell` refused on a Buildup-state building, on an enemy-owned building, and on an already-selling one, **and accepted** on a full-health Construction Yard (the no-last-ConYard-rule proof);
  5. **the sell mission** — `begin_sell` a partially damaged `GACNST`, step through the buildup duration witnessing credits unchanged, then witness the completion frame: credits `+1500` **flat** (not HP-scaled), 3 Allied survivors on foundation cells, building Dead + `sold`, foundation passable again.

  `repair_fold` folds, at each checkpoint: per building `{hp, being_repaired?1:0, selling?1:0, llround(sell_time*1000), sold?1:0, alive?1:0, (int)state}`, per house `llround(house_credits)`, the unit count and each unit's `{cell_x, cell_y, image}`, and `repair_frame()`. **Wedge guards → exit 1:** the repair interval measured from the witnessed frames must be exactly 14; the per-step heal must be exactly 8; the per-step charge must equal `repair_step_cost`; auto-stop must have fired; the out-of-credits stop must have left hp untouched; the full-health ConYard must have been sellable; credits must be unchanged on every pre-completion sell frame and must jump by exactly the flat refund on the completion frame; the survivor count must be exactly 3; the sold building must be Dead with `sold == true`.
  **No RNG is reachable** in any of it (the 25 % crew special is cut), so the gate is bit-deterministic.
- **The 19 existing gates stay byte-identical.** Full re-verify at the first `world.cpp`-touching commit (Task 4 — the highest-risk touch, since it inserts a pass into `World::step`), again after the sell mission lands (Task 5), and at finalize — **ONE GATE PER FOREGROUND CALL**.
- **UI is not hash-gateable.** The sidebar buttons and mode cursor are validated by (a) a clean build, (b) the pure `test_sidebar`/`test_input` pins, (c) a finalize screenshot showing the pressed button frame while a mode is armed, and (d) an interactive check: arm Repair on a damaged building and watch the HP bar climb and credits tick down; arm Sell on a building and watch the buildup play **backwards** before the credits land.
- **arm64:** `android; $env:JAVA_HOME = "C:\Program Files\Java\jdk-21"; .\gradlew.bat assembleDebug` must stay green (the machine default jdk-11 is rejected by the Android Gradle plugin) and link the new symbols (`repair_step_cost`, `sell_refund`, `survivor_count`, `mirrored_buildup_frame`).

## [RE-FALLBACK] / PROBABLE items for the Task 1 RE-GATE to pin

Task 1 is a **confirmatory RE-GATE adjudication** using `ra2extract` + the repo's own `libra2assets.a` against the real install, plus the already-captured Ghidra listings — **not a fresh hunt**. Everything in §A1–A6, §B1–B3, §B5 and §C1 marked CONFIRMED is closed; do **not** re-open it. Task 1 writes a `## §D Adjudication (RE-GATE)` section into `docs/re/repair-sell.md` and must close these **before any code depends on them**:

1. **The survivor crew type (§B3, PROBABLE).** `Rules+0xF70` was not string-anchored, and the 25 % branch is gated on building purpose-class `7`. Adjudicate explicitly: **CUT this phase** — every survivor is the house's basic infantry (`E1` Allied / `E2` Soviet), the hunt's own "defensible first cut" (deviation #5) — and record that the 25 % special is an RNG draw that would break the new gate's determinism. Pin the two infantry ids against the real `rulesmd.ini` sides so the gate's expected units are measured.
2. **The Service-Depot case (§B4, PROBABLE).** Confirm from the existing listing whether the depot branch is reachable for any type in the clone's 24-building roster (it needs `UnitReload=yes`). Expected outcome: **no rostered type sets it → out of scope, recorded**, with the `vtbl+0x274` radio query left unidentified.
3. **The AI reuse (§A7, PROBABLE).** Adjudicate **OUT of scope**, and record the concrete blocker list: the six unanchored offsets, and the `Random(0,50)` draw that would perturb the three AI byte-identity gates. State what a future hunt must string-anchor first (the `[General]` IQ key behind `Rules+0x1444` and the money threshold behind `Rules+0x1758`).
4. **Mid-production / primary-factory fate on sell (§B4, [RE-FALLBACK]).** Adjudicate the **FALLBACK**: a sold building goes through the clone's existing building-death path, so a factory being sold behaves exactly as a factory being destroyed — which is the one thing certainly true of the original, since both end in `~FactoryClass` (`construction.md` §10: the destructor abandons with refund). Record precisely what the clone's death path does today for a building with a live production queue and a `primary` flag, and whether `begin_sell` must add anything. The `[RE-FALLBACK]` label stays; what changes is that the chosen behavior is now stated and justified.
5. **`PlayerPtr->[0x2F0]` = the owned-building count (§C1, PROBABLE).** The arm-refusal predicate. Try once to confirm the field from the existing listings; **FALLBACK:** implement it as "the player owns ≥ 1 alive building", tagged `[RE-FALLBACK: PlayerPtr+0x2F0 identity]`. Either way the behavior is the same for the only case that matters (zero buildings ⇒ the buttons are dead).
6. **The mind-control sell refusal (`+0x1D0`, §B5 item 5, PROBABLE).** Adjudicate **OUT of scope** (no mind-control mechanic exists) and record that `can_be_sold` deliberately omits it, so the P4/P5 phase that adds mind control knows to add the term.
7. **The `UndeploysInto` option globals (§B6, PROBABLE).** Adjudicate **OUT of scope** with the §Scope justification recorded, and note that the clone takes the *plain-refund* branch — a reachable configuration of the original, not a deviation.
8. **The wrench / dollar cursor frames (§C2, [RE-FALLBACK]).** One attempt: dump `MOUSE.SHP` with `ra2extract`, record its frame count, and see whether a wrench and a dollar sign are visually identifiable. If they are, record the indices and close §C2. **FALLBACK:** leave §C2 open and record the §B12 substitute (pressed button frame + hovered-building outline) as the shipped feedback.
9. **Re-measure the phase's own numbers against this install** (so the gate's wedge bounds are measured, not remembered): for **every** type in the clone's `kRoster[24]`, the real `Cost`/`Strength` and the resulting `repair_step_cost`, the flat `sell_refund`, and the Allied/Soviet `survivor_count`; plus which rostered types set `Crewed=`, `Unsellable=`, `ClickRepairable=` or `Soylent=`. Expected: `Unsellable` on none, `ClickRepairable` on none (all default yes), `Soylent` on none. The §A2/§B3 worked rows for `GACNST`/`GAPOWR`/`GAWEAP` must be reproduced exactly, or the discrepancy investigated before any code is written.
