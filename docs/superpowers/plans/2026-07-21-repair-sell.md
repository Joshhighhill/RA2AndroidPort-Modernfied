# Building Repair & Sell — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the sidebar's two remaining inert buttons work, exactly as `docs/re/repair-sell.md` recovered them. **Repair** = a per-building toggle that heals **+8 HP** on a **global 14-frame phase** (`frame % trunc(RepairRate*900) == 0`), charging `max(1, trunc((Cost / (Strength / RepairStep)) * RepairPercent))` with **BOTH divisions integer**, auto-stopping at full health and silently clearing itself when the owner cannot afford the next step. **Sell** = a timed **mission**, not an instant action: the buildup animation plays **MIRRORED** for the buildup duration, and only on completion do the **flat** refund (`trunc(trunc(Cost*m2*m1) * RefundPercent)`, **no HP scaling**) and the survivors (`clamp(refund / SurvivorDivisor[side], 1, 5)`) land. Both buttons become mutually-exclusive cursor modes with the recovered arm/refuse/cancel semantics. Demonstrated by the **20th permanent byte-identity gate `--demo-repair`**. All 19 existing gates stay bit-identical.

**Architecture:** One new pure TU `engine/repair_sell.{hpp,cpp}` (world-free — includes only `building.hpp`; registered in BOTH cmake files, host-doctested) holding `RepairSellRules` plus nine free functions: `repair_interval_frames`, `repair_due`, `repair_step_cost`, `sell_refund`, `survivor_divisor`, `survivor_count`, `can_be_click_repaired`, `can_be_sold`, `mirrored_buildup_frame`. Four inert fields are appended to `Building` (`being_repaired`, `selling`, `sell_time`, `sold`) and four to `BuildingTypeDef` (`unsellable`, `click_repairable`, `crewed`, `soylent`), populated by a new generic `Rules::section_flag` + the existing `section_double` inside `load_building_assets`. `World` gains `set_repair_sell_rules` / `set_repair` / `toggle_repair` / `begin_sell` / `set_survivor_crew` / `building_repairing` / `building_selling` / `repair_frame` and **one firewalled pass** `step_repair_sell(dt)`, inserted into `World::step` after the dead-building blocking-release loop and before `step_combat`, guarded by `repair_sell_any_` (false in every existing demo) with an **unconditional** `repair_frame_++` above the guard so the cadence is an absolute frame phase. `SidebarLayout` gains `repair_btn`/`sell_btn` rects at the coordinates `main.cpp` already blits at; `Input` gains `BaseMode{None,Repair,Sell}` with the recovered setter semantics, the §C1 cancel chain, and two new `InputActions` reports; `GameScene` wires the buttons, the crews, the mirrored-buildup draw and the explosion suppression; and a new `repair_fold`/`simulate_repair`/`run_demo_repair`/`--demo-repair` gate mirrors the `--demo-maps` harness. Everything new defaults inert, and `world_hash` is not modified.

**Tech Stack:** C++17, doctest, CMake+Ninja (desktop `build/`, Android `android/` gradle, NDK `D:\android-ndk-r27c`, **JDK-21**), SDL2 (render/UI only), `ra2assets` (`Ini`, `Rules`, `Art`, `MixFile`, `ShpFile`), `ra2engine` (`World`, `Building`, `BuildingTypeDef`, `SidebarLayout`, `Input`, `exit_cell`, `kill_building`), `build/ra2extract.exe` for the RE-GATE only. Authority = `docs/re/repair-sell.md`; spec = `docs/superpowers/specs/2026-07-21-repair-sell-design.md`.

## Global Constraints

(Copied from the spec's §Global Constraints — every task's requirements implicitly include this section.)

- **All 19 existing byte-identity gates MUST stay bit-identical:** combat logs=78, tank logs=144, base `5273b5e71ca009e1`, econ `7fed96cb98b68017`, prod `f0f4e1207ecd713c`, ai-base `cd1f7c6a29933f0a`, ai-team `312e92ed862a5e74`, ai-trigger `8334a6ca1531cabc`, transport `fe500f946e913645`, aircraft `a69642564cf64773`, naval `6aee2c82bc024e86`, superweapon `189496664f5eca68`, techtree `e73adae61ff6f71f`, capture `58b87b5bc63f7046`, shroud `d302852aa9517661`, multihouse `39412ae996cce8ae`, skirmish `e9ad3f1fd88495f9`, roster `333cd43d8b5ece6a`, maps `6e516005f120a1bb`. Adds the **20th: `--demo-repair`**.
- **The firewall, four parts, non-negotiable** (this phase mutates `Building::hp` and `Building::state`, and `world_hash` folds BOTH):
  1. new `Building` / `World` state defaults inert;
  2. `if (!repair_sell_any_) return;` guards the one new pass, and `repair_sell_any_` is opened ONLY by `set_repair(..., on)` / `begin_sell` — which no existing demo calls;
  3. **`world_hash` (`main.cpp:5665`) is NOT modified**; the new state is hashed only by `repair_fold`;
  4. `step_repair_sell` is a pure INSERTION at `world.cpp:2859` (after the blocking-release loop, before `step_combat`) — no existing pass moves relative to any other.
- **Calibrate-first.** Every constant cites `docs/re/repair-sell.md` by § in a code comment. Task 1 pins everything the hunt left PROBABLE/[RE-FALLBACK] BEFORE code depends on it; anything still unpinned keeps an explicit `[RE-FALLBACK: …]` comment naming what was adjudicated instead.
- **Module split:** pure arithmetic → `engine/repair_sell.cpp` (no SDL, no `World`), registered in BOTH `cmake/ra2_sources.cmake` and `CMakeLists.txt`. `World` mutation entry points → `engine/world.cpp` (already on `test_world`'s line). Geometry → `engine/sidebar.cpp`. Gesture state → `engine/input.cpp`. Rules parsing → `include/ra2assets/rules.hpp` + `src/rules.cpp`. UI/render/gate → `engine/main.cpp`.
- **No assets committed.** Real-asset tests guard on `RA2_INSTALL_DIR` and skip cleanly (`const char* dir = std::getenv("RA2_INSTALL_DIR"); ... if (buf.empty()) { MESSAGE("skipped: no install"); return; }`).
- **Demo gate** = two-run compute-and-self-compare (run 1 rendered hidden-window + start/end PNG, run 2 headless fresh `World`), `identical = (witness && hash)`, wedge guards → exit 1, **NO pinned hash literal in code** (README pins it).
- Commit trailer: `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.

**Gate-runtime discipline (operational, binds every build/test/demo step):**

- **Run demo gates ONE per FOREGROUND call.** Each gate is ≈2 min / ~3 GB; chaining several in one call exceeds the 600 s tool timeout, auto-backgrounds, and reads as a false hang. Every gate step below is a FOREGROUND, synchronous, blocking run waited for INLINE (timeout up to 600000 ms). **NEVER batch 3-4 gates. NEVER "wait for a monitor".**
- **Run exes from PowerShell, NOT Git Bash** (Git Bash gives exit 127). `RA2_INSTALL_DIR` default `D:\Red Alert 2`. NEVER pipe a live native exe through `Select-Object -First N` (stops draining stdout → false hang).
- Pure modules go in **BOTH** `cmake/ra2_sources.cmake` (→ `ra2engine` + Android `libmain.so`) AND `CMakeLists.txt` (→ `ra2assets_tests`). This phase adds `engine/repair_sell.cpp` — register it in both.
- Repo on master, solo, no remote. NDK `D:\android-ndk-r27c`; **`JAVA_HOME` must be `C:\Program Files\Java\jdk-21`** — the machine default jdk-11 is rejected by the Android Gradle plugin.
- Scratchpad for all temp files: `C:\Users\Omar\AppData\Local\Temp\claude\d--Red-Alert-2\d44bece6-5e7c-465a-9fd5-820d808b4426\scratchpad\`.

**Exact commands (use verbatim):**
- Build: `cmake --build D:\ra2-android\build --target ra2assets_tests ra2engine`
- Tests: `D:\ra2-android\build\ra2assets_tests.exe --test-case="<filter>*"`
- Gates (INDIVIDUALLY, one per call): `D:\ra2-android\build\ra2engine.exe --demo-X`

**The 19-gate re-verify block (referenced by Tasks 4/5/8 — run ONE per FOREGROUND call):**

```powershell
cmake --build D:\ra2-android\build --target ra2engine
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2engine.exe --demo-combat
D:\ra2-android\build\ra2engine.exe --demo-tank
D:\ra2-android\build\ra2engine.exe --demo-base
D:\ra2-android\build\ra2engine.exe --demo-econ
D:\ra2-android\build\ra2engine.exe --demo-prod
D:\ra2-android\build\ra2engine.exe --demo-ai-base
D:\ra2-android\build\ra2engine.exe --demo-ai-team
D:\ra2-android\build\ra2engine.exe --demo-ai-trigger
D:\ra2-android\build\ra2engine.exe --demo-transport
D:\ra2-android\build\ra2engine.exe --demo-aircraft
D:\ra2-android\build\ra2engine.exe --demo-naval
D:\ra2-android\build\ra2engine.exe --demo-superweapon
D:\ra2-android\build\ra2engine.exe --demo-techtree
D:\ra2-android\build\ra2engine.exe --demo-capture
D:\ra2-android\build\ra2engine.exe --demo-shroud
D:\ra2-android\build\ra2engine.exe --demo-multihouse
D:\ra2-android\build\ra2engine.exe --demo-skirmish
D:\ra2-android\build\ra2engine.exe --demo-roster
D:\ra2-android\build\ra2engine.exe --demo-maps
```

Expected, all nineteen pins EXACT: `deterministic=yes logs=78`; `deterministic=yes logs=144`; `hash=5273b5e71ca009e1`; `econ_hash=7fed96cb98b68017`; `prod_hash=f0f4e1207ecd713c`; `ai_base_hash=cd1f7c6a29933f0a`; `ai_team_hash=312e92ed862a5e74`; `ai_trigger_hash=8334a6ca1531cabc`; `transport_hash=fe500f946e913645`; `aircraft_hash=a69642564cf64773`; `naval_hash=6aee2c82bc024e86`; `superweapon_hash=189496664f5eca68`; `techtree_hash=e73adae61ff6f71f`; `capture_hash=58b87b5bc63f7046`; `shroud_hash=d302852aa9517661`; `multihouse_hash=39412ae996cce8ae`; `skirmish_hash=e9ad3f1fd88495f9`; `roster_hash=333cd43d8b5ece6a`; `maps_hash=6e516005f120a1bb`.

### File Structure

| File | Responsibility |
|---|---|
| `docs/re/repair-sell.md` (modify, Task 1) | Calibration authority (hunt COMPLETE). Task 1 appends `## §D Adjudication (RE-GATE)` pinning the 9 open items. |
| `engine/repair_sell.hpp` + `engine/repair_sell.cpp` (create, Task 2) | PURE, world-free: `RepairSellRules` + the nine functions. Register in BOTH cmake files. |
| `engine/building.hpp` (modify, Task 2) | 4 new inert `Building` fields + 4 new inert `BuildingTypeDef` fields. |
| `tests/test_repair_sell.cpp` (create, Task 2) | The §A2/§B1/§B3/§B5 arithmetic + predicate pins. |
| `tests/test_building.cpp` (modify, Task 2) | New fields default inert; `kill_building` leaves them alone. |
| `include/ra2assets/rules.hpp` + `src/rules.cpp` (modify, Task 3) | `bool section_flag(id, key, fallback)`. |
| `tests/test_rules.cpp` (modify, Task 3) | `section_flag` + the real-install `[General]` repair/sell key pins. |
| `engine/main.cpp` (modify, Tasks 3/6/7) | `RepairSellRules` on `BuildingAssets` + the 4 per-type resolves (T3); sidebar hit-tests, pressed frames, mode wiring, crews, mirrored-buildup draw, explosion suppression (T6); `repair_fold`/`simulate_repair`/`run_demo_repair`/`--demo-repair` (T7). |
| `engine/world.hpp` + `engine/world.cpp` (modify, Tasks 4/5) | `repair_rules_`/`repair_sell_any_`/`repair_frame_`/`survivor_crew_`; `set_repair_sell_rules`, `set_repair`, `toggle_repair`, `building_repairing`, `repair_frame`, `step_repair_sell` (repair half, T4); `begin_sell`, `set_survivor_crew`, `building_selling`, `complete_sell` (T5); one line in `repair_building`. |
| `tests/test_world.cpp` (modify, Tasks 4/5) | Firewall no-op, cadence, charge, auto-stop, out-of-credits (T4); sell mission, flat refund, survivors, instant branch (T5). |
| `engine/sidebar.hpp` + `engine/sidebar.cpp` (modify, Task 6) | `kRepairBtnX/kSellBtnX/kSideBtnY/kSideBtnW/kSideBtnH`, `repair_btn`/`sell_btn`, `repair_btn_at`/`sell_btn_at`. |
| `tests/test_sidebar.cpp` (modify, Task 6) | Button-rect pin against `main.cpp:2972-2973`'s literals. |
| `engine/input.hpp` + `engine/input.cpp` (modify, Task 6) | `BaseMode`, `set_repair_mode`, `set_sell_mode`, `base_mode`, `cancel_one_mode`, `set_owned_buildings`, the intercept block, 4 new `InputActions` fields. |
| `tests/test_input.cpp` (modify, Task 6) | Mode semantics, exclusion, cancel priority, own-house click gating. |
| `cmake/ra2_sources.cmake`, `CMakeLists.txt` (modify, Task 2) | `engine/repair_sell.cpp` → `RA2_ENGINE_SOURCES` AND `ra2assets_tests`; `tests/test_repair_sell.cpp` → `ra2assets_tests`. |
| `README.md` (modify, Task 8) | Repair & Sell section + 20th-gate row + CLI bullet; "nineteen" → "twenty". |
| `.superpowers/sdd/progress.md` (modify, Task 8) | Ledger entry. |

**Live-code reconciliation (grepped against the post-maps tree `3022385` at write time — every anchor below was verified, not remembered):**

- `struct Building` — `engine/building.hpp:60`; `enum class State { Buildup, Idle, Dead }` `:74`; `buildup_time` `:76`; `buildup_total` `:77`; `die_time` `:78`; `alive` `:79`; `blocking_released` `:83`; last field `grants_superweapon` `:98`; closes `:99`.
- `struct BuildingTypeDef` — `engine/building.hpp:16`; `strength` `:19`; `cost` `:21`; `buildup` `:40`; last field `grants_map_reveal` `:56`; closes `:57`.
- `buildup_frame` — decl `building.hpp:143`, def `building.cpp:69-76`. `building_frame` — decl `:134`, def `building.cpp:62-67`. `step_buildings` — decl `:155`, def `building.cpp:78-95`. `kill_building` — decl `:159`, def `building.cpp:97-102`.
- `class World` — `engine/world.hpp:512`; `buildings_` `:1183`; `passable_` `:1184`; `occ_` `:1192`; `grid_w_/grid_h_` `:1193/:1194`; `types_` `:1204`; `house_credits_` `:1217`; private block ends `:1218`, class closes `:1219`.
- `World::step(double)` — decl `world.hpp:988`, def `world.cpp:2799`; `step_buildings(buildings_, dt);` `world.cpp:2850`; the dead-building blocking-release loop `world.cpp:2851-2859`; `step_combat(...)` `world.cpp:2861`.
- `World::step_capture` firewall shape — `world.cpp:1521-1525`. `World::step_superweapon` — `world.cpp:1331-1335`.
- `World::repair_building(int, int)` — decl `world.hpp:876`, def `world.cpp:1420-1429`.
- `World::add_building(...)` — decl `world.hpp:654-656`, def `world.cpp:812-835`.
- `World::add_combat_unit(int, int, int, const UnitCombat&)` — decl `world.hpp:537`, def `world.cpp:539`. `World::walkable`/`unit_at_cell` preconditions documented `world.hpp:529-536`.
- `World::launch_unit(...)` — decl `world.hpp:694`, def `world.cpp:877-915` (the `exit_cell` + `free_cell` lambda pattern).
- `exit_cell(const Building&, const BuildingTypeDef&, free_fn)` — `engine/factory.hpp:33`, `engine/factory.cpp:6-47`.
- `World::Side { Allied, Soviet, Yuri, Neutral }` — `world.hpp:780`; `HouseRec` `:785-791`; `house_count()` `:794`; `house_side(int)` `:797`; `ai_enabled(int)` `:729`; `are_allied(int,int)` `:819`; `house_credits(int)` `:932-934`; `grant_credits_for_test` `:979-981`; `clear_selection()` `:545`; `building_at_cell(int,int)` `:661`.
- `foundation_cells(cx, cy, fw, fh)` — `building.hpp:104`.
- `Rules::general_double` — decl `include/ra2assets/rules.hpp:131`, def `src/rules.cpp:220-228`. `Rules::section_double` — decl `:136`, def `src/rules.cpp:230-237`. `Rules::is_construction_yard` — def `src/rules.cpp:208` (the `is_truthy(ini_->get(id, key, ""))` pattern).
- `load_building_assets(...)` — `main.cpp:1484`; `build_time_min` `:1551`; `ba.condition_yellow` `:1554`; the per-type resolve loop `:1604-1685`; `buildup_total` computation `:1642-1655`; sidebar art `opt(...)` block `:1711-1728` (`sb_repair`/`sb_sell` at `:1722-1723`, `has_repair_sell` `:1724`).
- `struct BuildingAssets` — `main.cpp:~380`; `sb_repair, sb_sell` `:387`; `has_repair_sell` `:390`; `condition_yellow` `:403`.
- `draw_sidebar(...)` — `main.cpp:2942-3099`; the repair/sell blit `main.cpp:2971-2974`.
- `draw_building(...)` — `main.cpp:1960`; buildup-vs-main SHP selection `:2002-2005`; call site `:2275`.
- `draw_placement_cursor(...)` — `main.cpp:3101-3130`; call site `:3558-3559`.
- `SidebarLayout` — `engine/sidebar.hpp:49-74`; `compute` def `sidebar.cpp:7-60` (returns at `:59`); `cameo_at` `:62`; `tab_at` `:69`; `SbRect` `sidebar.hpp:13-17`; `kSide1Y=158`/`kSide1H=69` `:26-27`.
- `class Input` — `engine/input.hpp:74`; `InputActions` `:28-45` (last field `sw_cy` `:44`); `arm_superweapon/cancel_superweapon/sw_targeting` `:99-101`; `placement_active()` `:91`; `set_owned_buildings` is greenfield; `take_actions()` `:162-166`; privates `place_type_` `:230`, `sw_targeting_` `:235`, `selected_building_` `:243`, `view_w_` `:254`.
- `Input::handle` — `input.cpp:293`; the `sw_targeting_` block `:307-342`; the `placement_active()` block `:349-384`; the ordinary switch `:386-701`; `resolve_ground_cell` def `input.cpp:~150`; `commit_factory_click` `:196-229`.
- `GameScene` — `main.cpp:3232`; ctor `:3234-3321`; sidebar init `:3262-3267`; `handle` `:3326-3401` (SW slot `:3331-3339`, tab `:3341-3347`, cameo `:3349-3391`, catch-all `return` `:3393`, Escape branch `:3396-3399`, `input_.handle(ev)` `:3400`); `update` `:3405-3550` (`SDL_GetMouseState` `:3413`, placement commit `:3495-3513`, audio pump `:3548`); `render` `:3554-3575`; members `:3596-3642`; `kWinW/kWinH` `:3582-3583`.
- Gate harness: `fnv_mix` `main.cpp:5657-5663`; `world_hash` `:5665-5681`; `maps_fold` `:10292-10314`; `struct DemoMapsResult` `:10316`; `kDemoMapsFile` `:10321`; `simulate_maps` `:10328-10472`; `run_demo_maps` `:10480-10552`; the `--demo-maps` dispatch arm `main.cpp:10797-10798`.
- World builders: `build_world(const Assets&, const UnitAssets&, int count)` `main.cpp:1459`; `wire_economy(World&, const BuildingAssets&, const Rules&)` `main.cpp:1410-1416`; `spawn_base(World&, const BuildingAssets&, int house, int cx, int cy, bool with_refinery=false, const VehicleAssets* harv_va=nullptr, bool harv_chrono=false, int harv_storage=0, bool allied_full=false)` `main.cpp:1879`; `roster_index(const BuildingAssets&, const std::string&)`; `resolve_combat_unit(const Rules&, const std::string&)` (`.combat`, used at `main.cpp:9608-9609`); `pick_start_cell(const MapFile&)` `world.hpp:419`; `centered_camera(const MapFile&, int, int)` `main.cpp:2266`; `capture_png(SDL_Renderer*, const std::string&, int, int)`; `render_world(...)`; `env_install_dir()`; `load_ini_from_ra2md(dir, name)`.
- **Greenfield confirmed (repo-wide grep found NONE):** `repair_sell.hpp`, `RepairSellRules`, `repair_interval_frames`, `repair_due`, `repair_step_cost`, `sell_refund`, `survivor_divisor`, `survivor_count`, `can_be_click_repaired`, `can_be_sold`, `mirrored_buildup_frame`, `being_repaired`, `selling`, `sell_time`, `sold`, `unsellable`, `click_repairable`, `crewed`, `soylent`, `section_flag`, `set_repair_sell_rules`, `set_repair`, `toggle_repair`, `begin_sell`, `set_survivor_crew`, `step_repair_sell`, `repair_sell_any_`, `repair_frame_`, `BaseMode`, `set_repair_mode`, `set_sell_mode`, `cancel_one_mode`, `set_owned_buildings`, `repair_btn`, `sell_btn`, `repair_fold`, `DemoRepairResult`, `simulate_repair`, `run_demo_repair`.

---

### Task 1: RE-GATE adjudication (NOT a fresh hunt) → docs/re/repair-sell.md §D

**Files:**
- Modify: `docs/re/repair-sell.md` (append a `## §D Adjudication (RE-GATE)` section)
- Scratch only (never committed): extracted `rulesmd.ini` / `MOUSE.SHP` dumps under the session scratchpad

**Interfaces:**
- Consumes: the COMPLETED hunt `docs/re/repair-sell.md` (§Ground truth, §A1–A7, §B1–B6, §C1–C2, §Confidence summary, §Deviations we will take); `D:\ra2-android\build\ra2extract.exe` against `D:\Red Alert 2`; the repo's own `ra2assets` for `Ini`/`Rules`/`ShpFile`; `docs/re/construction.md` §10 (`FactoryClass::Abandon`) and §11a (the button rects).
- Produces: pinned values for the 9 items below. Each consuming point in Tasks 2–7 is written PRE-ADJUDICATION with an inline FALLBACK and marked `[RE-GATE: repair-sell.md §D item N]`; the controller flips them to `[RE-GATE APPLIED: …]` after this task lands.

**The 9 items to adjudicate** (everything marked CONFIRMED in §A1–A6, §B1–B3, §B5, §C1 is already closed — do **NOT** re-open it):

1. **Survivor crew type (§B3, PROBABLE).** `Rules+0xF70` was not string-anchored; the 25 % branch is gated on building purpose-class `7`. Adjudicate **CUT this phase** (deviation #5's "defensible first cut"): every survivor is the house's basic infantry. Pin the two ids off the real `rulesmd.ini` (expected `E1` Allied / `E2` Soviet — the same pair `main.cpp:9608-9609` already uses) and record that the 25 % branch is an RNG draw that would break the new gate's determinism.
2. **Service-Depot case (§B4, PROBABLE).** Check whether ANY of the clone's `kRoster[24]` ids sets `UnitReload=yes` in the real `rulesmd.ini`. Expected: **none → out of scope**, recorded, with `vtbl+0x274` left unidentified.
3. **AI reuse (§A7, PROBABLE).** Adjudicate **OUT of scope**. Record the blocker list explicitly: the six unanchored offsets (`Rules+0x1444`, `Rules+0x1758`, `Owner+0x245`, `Owner+0x1D0`, `Owner+0x1D4`, `this->[0x3D1]`, `Type->[0xEB8]`) and the `Random(0,50)` draw that would perturb `--demo-ai-base`/`--demo-ai-team`/`--demo-ai-trigger`. State what a future hunt must string-anchor first.
4. **Mid-production / primary-factory fate on sell (§B4, [RE-FALLBACK]).** Adjudicate the **FALLBACK**: a sold building goes through the clone's existing building-death path, so selling a factory behaves exactly as destroying one — the one thing certainly true of the original, since both end in `~FactoryClass` (`construction.md` §10, `FUN_004C9930`: the destructor abandons **with refund**). Record precisely what the clone does today when a building with a live queue and/or `primary == true` dies, and state whether `begin_sell`/`complete_sell` must add anything (expected: nothing). The `[RE-FALLBACK]` label stays; what changes is that the chosen behavior is stated and justified.
5. **`PlayerPtr->[0x2F0]` = owned-building count (§C1, PROBABLE).** One attempt to confirm from the existing listings. **FALLBACK:** implement the arm-refusal as "the player owns ≥ 1 alive, non-Dead building", tagged `[RE-FALLBACK: PlayerPtr+0x2F0 identity]`.
6. **Mind-control sell refusal (`+0x1D0`, §B5 item 5, PROBABLE).** Adjudicate **OUT of scope** (no mind-control mechanic exists); record that `can_be_sold` deliberately omits the term so the phase adding mind control knows to add it.
7. **`UndeploysInto` option globals (§B6, PROBABLE).** Adjudicate **OUT of scope** with the spec's §Scope justification, and record explicitly that the clone takes the **plain-refund branch**, which the recovered code itself takes whenever `UndeploysInto == 0` / not-MP / non-human owner / option off — a reachable configuration of the original, not a deviation.
8. **Wrench / dollar cursor frames (§C2, [RE-FALLBACK]).** One attempt: dump `MOUSE.SHP`, record its frame count and dimensions, and see whether a wrench and a dollar sign are identifiable. If yes, record the indices and close §C2. **FALLBACK:** §C2 stays open; record the shipped substitute (pressed sidebar frame 1 + hovered-building outline, spec §B12).
9. **Re-measure this phase's own numbers against this install** (so the gate's wedge bounds are measured, not remembered): for every id in `kRoster[24]`, the real `Cost` / `Strength`, the resulting `repair_step_cost`, the flat `sell_refund` (human), and the Allied/Soviet `survivor_count`; plus which rostered ids set `Crewed=`, `Unsellable=`, `ClickRepairable=` or `Soylent=`. Expected: `Unsellable` none, `ClickRepairable` none (all default yes), `Soylent` none; and the §A2/§B3 rows for `GACNST` (3000/1000 → cost 3, refund 1500, Allied 3, Soviet 5), `GAPOWR` (800/1000 → cost 1, refund 400, `Crewed=no` → 0), `GAWEAP` (2000/1000 → cost 2, refund 1000, Allied 2, Soviet 4) reproduced **exactly**. Any discrepancy must be investigated before a line of code is written.

**Steps:**

- [ ] **Step 1: Re-read the authority.** Read `docs/re/repair-sell.md` end to end. Note the anchors this plan depends on: §Ground truth's verbatim `[General]` block; §A2's cadence + both traps + the `stepCost` listing + the worked table; §A3's before-the-spend check; §A4's `-1/0/1` convention + the five off-ramps + `Can_Be_Click_Repaired`; §A5's `<=` threshold; §A6's separate engineer path; §B1's three-branch refund + the human-only `RefundPercent`; §B2's mission + the mirror formula + the 54-tick duration; §B3's divisor formula + worked table; §B5's six refusals + the **CONFIRMED-by-absence** paragraph; §C1's five mode flags + setter shape + cancel chain; §C2's [RE-FALLBACK]; and all ten items of §Deviations. **Do NOT re-hunt any of these.**

- [ ] **Step 2: Extract the ground-truth INI + the cursor art (FOREGROUND, blocking, timeout 600000 ms).**

```powershell
$SC = "C:\Users\Omar\AppData\Local\Temp\claude\d--Red-Alert-2\d44bece6-5e7c-465a-9fd5-820d808b4426\scratchpad"
cmake --build D:\ra2-android\build --target ra2extract
D:\ra2-android\build\ra2extract.exe "D:\Red Alert 2\ra2md.mix" --raw rulesmd.ini "$SC\rulesmd.ini"
D:\ra2-android\build\ra2extract.exe "D:\Red Alert 2\ra2.mix"   --raw mouse.shp   "$SC\mouse.shp"
```

  If a nested path is needed, walk the candidates the way `docs/re/maps.md` §Reproduce shows (`ra2md.mix` → `localmd.mix` etc.). Record the actual command that worked.

- [ ] **Step 3: Measure item 9.** With `$SC\rulesmd.ini`, for each of the 24 roster ids (`GACNST NACNST GAPOWR NAPOWR GAPILE NAHAND GAWEAP NAWEAP GAREFN NAREFN GARADR NARADR GATECH NATECH GAAIRC NANRCT ATESLA NASAM CAOILD …` — take the authoritative list from `main.cpp:1536`'s `kRoster`), read `Cost`, `Strength`, `Crewed`, `Unsellable`, `ClickRepairable`, `Soylent`, `UnitReload` and compute:

```
per      = Strength / 8                              (integer)
n        = per ? Cost / per : Cost                   (integer)
stepCost = max(1, trunc(n * 0.15))
refund   = trunc(trunc(Cost * 1.0 * 1.0) * 0.50)
survA    = Crewed ? clamp(refund / 500, 1, 5) : 0
survS    = Crewed ? clamp(refund / 250, 1, 5) : 0
```

  A short scratch script (Python or a C++ scratch tool linking `build/libra2assets.a`) is fine; the OUTPUT is what matters. Cross-check the three §A2/§B3 rows and STOP if any disagrees.

- [ ] **Step 4: Inspect `MOUSE.SHP` (item 8).** Report its frame count and per-frame dimensions with `ra2extract`; dump a handful of frames to PNG if the tool supports it, and state plainly whether a wrench / dollar-sign frame is identifiable. Do **not** guess an index.

- [ ] **Step 5: Answer items 1–7 from the documents.** Item 2 from the extracted INI (`UnitReload=`); item 4 from `docs/re/construction.md` §10 **plus** a read of the clone's current building-death path (`building.cpp:97-102`, `world.cpp:2851-2859`, and whatever `production.cpp`/`factory.cpp` do when a `primary` factory dies); items 1, 3, 5, 6, 7 from `repair-sell.md` itself plus the spec's §Scope justifications.

- [ ] **Step 6: Write `## §D Adjudication (RE-GATE)` into `docs/re/repair-sell.md`.** One numbered subsection per item, each with the pinned value AND the evidence (command + output excerpt) that pins it, or the FALLBACK with an explicit `[RE-FALLBACK]` tag. Item 9 MUST include the full 24-row measured table (id, Cost, Strength, stepCost, refund, survA, survS, Crewed/Unsellable/ClickRepairable/Soylent) — Tasks 2 and 7 copy their expected numbers from it verbatim. Item 8 MUST either give the two frame indices or state that §C2 stays open.

- [ ] **Step 7: Commit the adjudication.**

```powershell
git -C "D:\ra2-android" add docs/re/repair-sell.md
git -C "D:\ra2-android" commit -m "$(printf 'docs(re): repair-sell RE-GATE adjudication - pin survivor crew (E1/E2, 25%% special CUT), Service-Depot/AI-reuse/UndeploysInto/mind-control OUT of scope with blocker lists, factory-abandon fallback, owned-building predicate, MOUSE.SHP finding, and the measured 24-row stepCost/refund/survivor table [RE: repair-sell.md]\n\nCo-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>')"
```

- [ ] **Step 8 (CONTROLLER, not the implementer): RE-GATE amendment.** Diff §D against every `[RE-GATE: repair-sell.md §D item N]` marker in Tasks 2–7, amend the plan BEFORE dispatching Task 2, and flip each marker to `[RE-GATE APPLIED: …]` with the confirmed value inline. In particular: paste item 9's measured `GACNST`/`GAPOWR`/`GAWEAP` numbers into Task 2's test expectations and Task 7's wedge guards.

---

### Task 2: `engine/repair_sell.{hpp,cpp}` — the pure arithmetic + the inert fields

**Files:**
- Create: `engine/repair_sell.hpp`, `engine/repair_sell.cpp`
- Create: `tests/test_repair_sell.cpp`
- Modify: `engine/building.hpp` (4 new `Building` fields after `grants_superweapon` at `:98`; 4 new `BuildingTypeDef` fields after `grants_map_reveal` at `:56`)
- Modify: `tests/test_building.cpp` (inert-default case)
- Modify: `cmake/ra2_sources.cmake` (add `${RA2_ROOT}/engine/repair_sell.cpp` after `engine/maplist.cpp` at `:77`), `CMakeLists.txt` (add a `target_sources` line after the `test_visibility.cpp` line at `:213`)

**Interfaces:**
- Consumes: `ra2engine::Building`, `ra2engine::BuildingTypeDef` (`engine/building.hpp`).
- Produces — the four `Building` fields (appended after `bool grants_superweapon = false;`, `building.hpp:98`):

```cpp
    // --- Repair & Sell phase [RE: repair-sell.md §A2/§B2] ---
    // ALL FOUR are inert by default and are NOT read by world_hash (which folds
    // only type/cx/cy/hp/state), so every existing gate is byte-identical. They
    // are set ONLY by World::set_repair / World::begin_sell, which no existing
    // demo calls.
    bool   being_repaired = false;  // [§A2 bldg+0x6E8] the per-building wrench toggle
    bool   selling        = false;  // [§B2] mission 0x13 (Selling) is running
    double sell_time      = 0.0;    // seconds elapsed in the MIRRORED buildup [§B2]
    bool   sold           = false;  // died via sell, not combat (suppresses the explosion)
```

  and the four `BuildingTypeDef` fields (appended after `bool grants_map_reveal = false;`, `building.hpp:56`):

```cpp
    // --- Repair & Sell phase [RE: repair-sell.md §A4/§B1/§B3/§B5] ---
    // Absent from every stock building section EXCEPT ClickRepairable, whose
    // recovered default is YES [§A4]. Resolved by load_building_assets.
    bool unsellable       = false;  // Unsellable=      [§B5] (set by NO stock RA2 building)
    bool click_repairable = true;   // ClickRepairable= [§A4] DEFAULT YES
    bool crewed           = false;  // Crewed=          [§B3] gates survivor spawn
    int  soylent          = 0;      // Soylent=         [§B1] absolute refund override
```

- Produces — `engine/repair_sell.hpp`:

```cpp
#pragma once
// Pure Repair & Sell arithmetic and predicates, recovered from YR-1.001
// gamemd.exe -- docs/re/repair-sell.md (§ refs inline). World-free and
// SDL-free: this TU sees only Building/BuildingTypeDef, exactly like
// production.cpp and factory.cpp, so every number below is host-doctestable.
//
// THE THREE TRAPS this file exists to make impossible to re-introduce:
//   1. the repair interval is trunc(RepairRate*900) = 14, NOT 15 (__ftol
//      truncates 14.4) and it is a GLOBAL frame phase, not a per-building timer;
//   2. stepCost uses TWO INTEGER divisions -- the naive
//      cost*percent*damage_fraction form is ~17% wrong on a Construction Yard;
//   3. the sell refund has NO health term at all.
#include <cstdint>

#include "building.hpp"

namespace ra2engine {

// The [General] values this phase reads. The DEFAULTS ARE this install's real
// rulesmd.ini values [RE: repair-sell.md §Ground truth / §A1], so a World that
// never calls set_repair_sell_rules already behaves like the original.
// NOTE [§Ground truth, CONFIRMED]: URepairStep and URepairPercent DO NOT EXIST
// in this build -- there is exactly one RepairStep (buildings + vehicles), one
// IRepairStep (infantry) and one RepairPercent. SurvivorRate is present in the
// INI but DEAD (never read by the engine); the divisors are the whole story.
struct RepairSellRules {
    int    repair_step    = 8;      // RepairStep    (Rules+0x16CC) [§A1]
    double repair_percent = 0.15;   // RepairPercent (Rules+0x16D0) [§A1]
    double repair_rate    = 0.016;  // RepairRate    (Rules+0x16E0) [§A1] minutes/step
    double refund_percent = 0.50;   // RefundPercent (Rules+0x1738) [§A1]
    int    allied_survivor_divisor = 500;   // Rules+0x14F8 [§B3]
    int    soviet_survivor_divisor = 250;   // Rules+0x14FC [§B3]
    int    third_survivor_divisor  = 750;   // Rules+0x1500 [§B3]
};

// interval = trunc(RepairRate * 900.0) frames [RE: §A2, CONFIRMED].
// TRAP: 0.016 * 900 = 14.4 and __ftol TRUNCATES -> 14, not 15 (a 7% speed
// difference). Floored at 1 so a zero/negative rate can never divide by zero.
int repair_interval_frames(double repair_rate);

// The tick fires iff Frame % interval == 0 [RE: §A2, CONFIRMED]. This is a
// GLOBAL PHASE: every repairing building of every house steps on the SAME
// frames, and a building that starts repairing waits 0..interval-1 frames for
// its first step. It does NOT get its own timer.
bool repair_due(std::uint64_t frame, int interval);

// stepCost = max(1, trunc((Cost / (Strength / RepairStep)) * RepairPercent))
// [RE: §A2, CONFIRMED -- FUN_00711470, whole function].
// BOTH divisions are INTEGER and truncating; only then does the RepairPercent
// multiply happen, and __ftol truncates that too. Worked from the real rules
// (RepairStep=8, RepairPercent=0.15): GAPOWR 800/1000 -> 1 (125 to full),
// GACNST 3000/1000 -> 3 (375 to full), GAWEAP 2000/1000 -> 2 (250 to full).
// Do NOT "simplify" to cost*percent*damage_fraction: that yields 450 for a
// GACNST, ~17% wrong. Depends only on the TYPE, so callers may cache it.
int repair_step_cost(int cost, int strength, int repair_step, double repair_percent);

// refund [RE: §B1, CONFIRMED -- FUN_00711300 GetRefund + FUN_0070A220]:
//   Soylent != 0     -> trunc(Soylent * country_mult)          (RefundPercent NOT applied)
//   human owner      -> trunc(trunc(Cost*house_mult*country_mult) * RefundPercent)
//   AI owner         -> trunc(Cost*house_mult*country_mult)    (UNSCALED -- a real asymmetry)
// THERE IS NO HEALTH TERM. A building at 1 HP refunds exactly what a full-
// health one does. The two multipliers are the FactoryPlant/OrePurifier/
// difficulty modifiers, 1.0 in a default skirmish; they are parameters so a
// later phase can supply them without changing this signature.
int sell_refund(int cost, int soylent, double refund_percent, bool owner_is_human,
                double country_mult = 1.0, double house_mult = 1.0);

// The per-SIDE survivor divisor [RE: §B3, CONFIRMED]: side_ix is
// World::Side's underlying value -- 0 Allied (500), 1 Soviet (250),
// 2 Yuri/Third (750). Anything else (Neutral) returns 0 == "no survivors".
int survivor_divisor(const RepairSellRules& r, int side_ix);

// survivors = clamp(refund / divisor, 1, 5), integer division of the REFUND
// [RE: §B3, CONFIRMED -- FUN_00451320]. Zero when the type is not Crewed (or
// the instance has no crew). The divisor is DOUBLED for a captured building.
// SurvivorRate is never read (dead key). Worked: GACNST refund 1500 -> Allied
// 3, Soviet 5 (clamped from 6), Yuri 2; GAPOWR (Crewed=no) -> 0.
int survivor_count(int refund, int divisor, bool crewed, bool has_been_captured);

// Can_Be_Click_Repaired [RE: §A4, CONFIRMED -- FUN_00452620]: alive, not dead,
// not still building (the "in limbo / being built" test), the TYPE opts in via
// ClickRepairable (default yes), and there is damage to undo. OWNERSHIP is NOT
// tested here -- World::set_repair does that (mirroring the network event).
bool can_be_click_repaired(const Building& b, const BuildingTypeDef& t);

// Can_Be_Sold [RE: §B5, CONFIRMED -- FUN_00449410 + Sell_Back's own C4 test].
// Refused for, and ONLY for: Unsellable types; buildings still under
// construction (BState == 0 / mission Construction); buildings already selling
// (mission 0x13); and buildings with C4 planted.
//
// THERE IS NO LAST-CONSTRUCTION-YARD RULE AND NO CAPTURED-BUILDING RULE.
// Both were searched for in the binary and neither exists (§B5, "CONFIRMED by
// absence"): HasBeenCaptured affects only the survivor divisor and the AI
// repair trigger, never sellability. DO NOT ADD EITHER.
//
// c4_applied is an explicit parameter because the clone has no C4 mechanic
// until roadmap P4 -- every caller passes false today, and P4 wires it in one
// line without touching this predicate. Ownership is enforced by
// World::begin_sell (the SELL event's own check, §C1).
bool can_be_sold(const Building& b, const BuildingTypeDef& t, bool c4_applied);

// The sell animation's frame [RE: §B2, CONFIRMED -- BuildingClass::Shape_Number
// FUN_0043EF10]: during mission 0x13 the index is MIRRORED inside the BState-0
// buildup range, `(start + count - stage) - 1`, i.e. the buildup runs BACKWARDS
// over the same frame count at the same rate. Deliberately the exact mirror of
// building.cpp's buildup_frame, reading sell_time instead of buildup_time, so
// the two provably share one period (buildup_total / image_frames).
int mirrored_buildup_frame(const Building& b, int buildup_shp_frame_count);

}  // namespace ra2engine
```

- Produces — `engine/repair_sell.cpp`:

```cpp
#include "repair_sell.hpp"

#include <algorithm>

namespace ra2engine {

int repair_interval_frames(double repair_rate) {
    // [RE: repair-sell.md §A2] FLD Rules.RepairRate; FMUL 900.0; __ftol.
    // __ftol truncates toward zero -- 0.016*900 = 14.4 -> 14.
    const int n = static_cast<int>(repair_rate * 900.0);
    return n > 1 ? n : 1;   // floor at 1: a zero rate must not divide by zero
}

bool repair_due(std::uint64_t frame, int interval) {
    if (interval <= 0) return false;
    return frame % static_cast<std::uint64_t>(interval) == 0;
}

int repair_step_cost(int cost, int strength, int repair_step, double repair_percent) {
    // [RE: repair-sell.md §A2, FUN_00711470] -- transcribed instruction for
    // instruction; deviation #2 forbids re-deriving it.
    //   IDIV  Strength / RepairStep        (integer)
    //   IDIV  Cost / that                  (integer)
    //   FMUL  * RepairPercent ; __ftol     (truncating)
    //   CMP/JG -> clamp to >= 1
    const int per = (repair_step != 0) ? strength / repair_step : 0;
    const int n = (per != 0) ? cost / per : cost;
    const int c = static_cast<int>(static_cast<double>(n) * repair_percent);
    return c > 1 ? c : 1;
}

int sell_refund(int cost, int soylent, double refund_percent, bool owner_is_human,
                double country_mult, double house_mult) {
    // [RE: repair-sell.md §B1, FUN_00711300] Soylent is an ABSOLUTE credit
    // override and BYPASSES RefundPercent. No stock RA2 building sets it (all
    // 20 Soylent= lines are Yuri-Grinder infantry/vehicles), but deviation #3
    // says to read the key rather than hard-code the Cost path.
    if (soylent != 0) return static_cast<int>(static_cast<double>(soylent) * country_mult);
    // v = ftol(Cost * HouseCostMult * CountryCostMult)
    const int v = static_cast<int>(static_cast<double>(cost) * house_mult * country_mult);
    // RefundPercent is applied ONLY when the owning house is human-controlled
    // (TEST AL,AL / JZ at 0x00711399). An AI house receives the UNSCALED value.
    if (!owner_is_human) return v;
    return static_cast<int>(static_cast<double>(v) * refund_percent);
}

int survivor_divisor(const RepairSellRules& r, int side_ix) {
    // [RE: repair-sell.md §B3] switch on Owner->SideIndex; default -> 0
    // survivors. World::Side is {Allied=0, Soviet=1, Yuri=2, Neutral=3}.
    switch (side_ix) {
        case 0: return r.allied_survivor_divisor;
        case 1: return r.soviet_survivor_divisor;
        case 2: return r.third_survivor_divisor;
        default: return 0;
    }
}

int survivor_count(int refund, int divisor, bool crewed, bool has_been_captured) {
    // [RE: repair-sell.md §B3, FUN_00451320]
    if (!crewed) return 0;              // !Type->Crewed (or NoCrew) -> 0
    int div = divisor;
    if (div == 0) return 0;
    if (has_been_captured) div *= 2;    // captured buildings yield half as many
    const int n = refund / div;         // INTEGER division of the REFUND
    if (n < 1) return 1;                // floor 1 (a Crewed building always
                                        // yields at least one survivor)
    if (n > 5) return 5;                // cap 5
    return n;
}

bool can_be_click_repaired(const Building& b, const BuildingTypeDef& t) {
    // [RE: repair-sell.md §A4, FUN_00452620]
    if (!b.alive) return false;
    if (b.state == Building::State::Dead) return false;
    if (b.hp <= 0) return false;                       // Health == 0
    if (b.state == Building::State::Buildup) return false;  // in limbo / being built
    if (!t.click_repairable) return false;             // ClickRepairable, default yes
    if (b.hp >= b.max_hp) return false;                // nothing to repair
    return true;
}

bool can_be_sold(const Building& b, const BuildingTypeDef& t, bool c4_applied) {
    // [RE: repair-sell.md §B5, FUN_00449410 + Sell_Back's C4 test]
    if (!b.alive) return false;
    if (b.state == Building::State::Dead) return false;
    if (t.unsellable) return false;                    // Unsellable=yes
    if (b.state == Building::State::Buildup) return false;  // BState == 0 / Construction
    if (b.selling) return false;                       // already mission 0x13
    if (c4_applied) return false;                      // C4 planted
    // NO last-ConYard rule. NO captured rule. See the header comment.
    return true;
}

int mirrored_buildup_frame(const Building& b, int buildup_shp_frame_count) {
    // [RE: repair-sell.md §B2] frame = (start + count - stage) - 1, i.e. the
    // buildup range played BACKWARDS. Same image_frames/period derivation as
    // building.cpp's buildup_frame (building-render.md §5), reading sell_time.
    const int image_frames = buildup_shp_frame_count / 2;  // 2nd half = shadows
    if (image_frames < 1) return 0;
    if (b.buildup_total <= 0.0) return 0;   // no buildup art -> instant sell
    const double period = b.buildup_total / image_frames;
    int f = static_cast<int>(b.sell_time / period);
    f = std::clamp(f, 0, image_frames - 1);
    return image_frames - 1 - f;
}

}  // namespace ra2engine
```

**world_hash safety:** a brand-new TU plus eight struct fields that `world_hash` (`main.cpp:5665`, folds `Unit{cell_x,cell_y,hp,alive}` + `Building{type,cx,cy,hp,state}`) does not read. Nothing on any gate path calls any of the nine functions after this task. `world_hash` untouched.

**Steps:**

- [ ] **Step 1: Write the failing tests.** Create `tests/test_repair_sell.cpp`. **Every numeric expectation below is Task 1 §D item 9's MEASURED value** `[RE-GATE: repair-sell.md §D item 9]` — if the adjudication reported different `Cost`/`Strength` for a roster id, use the measured numbers and note the change in the task report.

```cpp
#include "doctest.h"
#include "repair_sell.hpp"
using namespace ra2engine;

// Every pin below is a literal recovered from YR-1.001 gamemd.exe / this
// install's rulesmd.ini -- docs/re/repair-sell.md (§ refs inline).

namespace {
// A minimal type/instance pair for the predicate cases.
BuildingTypeDef mk_type(int cost, int strength) {
    BuildingTypeDef t;
    t.cost = cost;
    t.strength = strength;
    return t;
}
Building mk_bldg(int hp, int max_hp) {
    Building b;
    b.hp = hp;
    b.max_hp = max_hp;
    b.state = Building::State::Idle;
    b.alive = true;
    return b;
}
}  // namespace

TEST_CASE("repair: interval is trunc(RepairRate*900) = 14, NOT 15 [RE: §A2]") {
    // 0.016 * 900 = 14.4 and __ftol TRUNCATES. Getting 15 here is a 7% error.
    CHECK(repair_interval_frames(0.016) == 14);
    CHECK(repair_interval_frames(0.02) == 18);
    CHECK(repair_interval_frames(1.0 / 900.0) == 1);
    CHECK(repair_interval_frames(0.0) == 1);      // floored, never divides by zero
    CHECK(repair_interval_frames(-1.0) == 1);
}

TEST_CASE("repair: the tick is a GLOBAL frame phase [RE: §A2]") {
    const int iv = repair_interval_frames(0.016);   // 14
    CHECK(repair_due(0, iv));
    CHECK_FALSE(repair_due(1, iv));
    CHECK_FALSE(repair_due(13, iv));
    CHECK(repair_due(14, iv));
    CHECK(repair_due(28, iv));
    CHECK(repair_due(140, iv));
    CHECK_FALSE(repair_due(15, iv));                // the 15-frame trap
    CHECK_FALSE(repair_due(10, 0));                 // guard: interval 0
}

TEST_CASE("repair: stepCost uses TWO integer divisions [RE: §A2 worked table]") {
    // GAPOWR 800/1000: 1000/8 = 125; 800/125 = 6; 6*0.15 = 0.90 -> trunc 0 -> clamp 1
    CHECK(repair_step_cost(800, 1000, 8, 0.15) == 1);
    // GACNST 3000/1000: 125; 3000/125 = 24; 24*0.15 = 3.60 -> 3
    CHECK(repair_step_cost(3000, 1000, 8, 0.15) == 3);
    // GAWEAP 2000/1000: 125; 2000/125 = 16; 16*0.15 = 2.40 -> 2
    CHECK(repair_step_cost(2000, 1000, 8, 0.15) == 2);
    // The naive float form is ~17% wrong on a ConYard: 3000 * 0.15 = 450 credits
    // to full, but the real total is (1000/8) steps * 3 = 375.
    const int steps_to_full = 1000 / 8;                       // 125 steps of 8 HP
    CHECK(steps_to_full * repair_step_cost(3000, 1000, 8, 0.15) == 375);
    CHECK(steps_to_full * repair_step_cost(800, 1000, 8, 0.15) == 125);
    CHECK(steps_to_full * repair_step_cost(2000, 1000, 8, 0.15) == 250);
    // max(1, .) clamp and the divide-by-zero guards.
    CHECK(repair_step_cost(1, 1000, 8, 0.15) == 1);
    CHECK(repair_step_cost(500, 4, 8, 0.15) == 75);           // Strength < RepairStep -> per == 0
    CHECK(repair_step_cost(500, 1000, 0, 0.15) == 75);        // RepairStep == 0 -> per == 0
}

TEST_CASE("sell: the refund is FLAT and Soylent bypasses RefundPercent [RE: §B1]") {
    // Human owner, no Soylent: trunc(trunc(3000*1*1) * 0.5) = 1500.
    CHECK(sell_refund(3000, 0, 0.50, true) == 1500);
    CHECK(sell_refund(800, 0, 0.50, true) == 400);
    CHECK(sell_refund(2000, 0, 0.50, true) == 1000);
    CHECK(sell_refund(999, 0, 0.50, true) == 499);            // trunc(499.5)
    // AI owner: RefundPercent is NOT applied (a real, recovered asymmetry).
    CHECK(sell_refund(3000, 0, 0.50, false) == 3000);
    // Soylent overrides AND bypasses RefundPercent, for human and AI alike.
    CHECK(sell_refund(3000, 700, 0.50, true) == 700);
    CHECK(sell_refund(3000, 700, 0.50, false) == 700);
    // The cost multipliers are applied INSIDE the inner trunc.
    CHECK(sell_refund(3000, 0, 0.50, true, /*country*/1.0, /*house*/0.5) == 750);
}

TEST_CASE("sell: survivor divisors and the clamp [RE: §B3 worked table]") {
    RepairSellRules r;
    CHECK(survivor_divisor(r, 0) == 500);      // Allied
    CHECK(survivor_divisor(r, 1) == 250);      // Soviet
    CHECK(survivor_divisor(r, 2) == 750);      // Yuri / Third
    CHECK(survivor_divisor(r, 3) == 0);        // Neutral -> no survivors
    CHECK(survivor_divisor(r, -1) == 0);
    // GACNST: refund 1500, Crewed=yes
    CHECK(survivor_count(1500, 500, true, false) == 3);
    CHECK(survivor_count(1500, 250, true, false) == 5);   // clamped from 6
    CHECK(survivor_count(1500, 750, true, false) == 2);
    // GAWEAP: refund 1000, Crewed=yes
    CHECK(survivor_count(1000, 500, true, false) == 2);
    CHECK(survivor_count(1000, 250, true, false) == 4);
    CHECK(survivor_count(1000, 750, true, false) == 1);
    // GAPOWR: Crewed=no -> ZERO regardless of refund
    CHECK(survivor_count(400, 500, false, false) == 0);
    // floor 1: a Crewed building always yields at least one
    CHECK(survivor_count(400, 500, true, false) == 1);
    CHECK(survivor_count(0, 500, true, false) == 1);
    // a captured building DOUBLES the divisor
    CHECK(survivor_count(1500, 500, true, true) == 1);    // 1500 / 1000
    CHECK(survivor_count(0, 0, true, false) == 0);        // divisor 0 guard
}

TEST_CASE("repair: Can_Be_Click_Repaired [RE: §A4]") {
    BuildingTypeDef t = mk_type(3000, 1000);
    Building b = mk_bldg(500, 1000);
    CHECK(can_be_click_repaired(b, t));
    SUBCASE("full health is refused") { b.hp = 1000; CHECK_FALSE(can_be_click_repaired(b, t)); }
    SUBCASE("zero health is refused")  { b.hp = 0;    CHECK_FALSE(can_be_click_repaired(b, t)); }
    SUBCASE("under construction is refused") {
        b.state = Building::State::Buildup;
        CHECK_FALSE(can_be_click_repaired(b, t));
    }
    SUBCASE("dead is refused") { b.state = Building::State::Dead; CHECK_FALSE(can_be_click_repaired(b, t)); }
    SUBCASE("tombstoned is refused") { b.alive = false; CHECK_FALSE(can_be_click_repaired(b, t)); }
    SUBCASE("ClickRepairable=no is refused") {
        t.click_repairable = false;
        CHECK_FALSE(can_be_click_repaired(b, t));
    }
    SUBCASE("ClickRepairable defaults to YES") { CHECK(mk_type(1, 1).click_repairable); }
}

TEST_CASE("sell: Can_Be_Sold -- and the two rules that DO NOT EXIST [RE: §B5]") {
    BuildingTypeDef t = mk_type(3000, 1000);
    Building b = mk_bldg(1000, 1000);
    CHECK(can_be_sold(b, t, false));
    SUBCASE("Unsellable=yes is refused") { t.unsellable = true; CHECK_FALSE(can_be_sold(b, t, false)); }
    SUBCASE("under construction is refused") {
        b.state = Building::State::Buildup;
        CHECK_FALSE(can_be_sold(b, t, false));
    }
    SUBCASE("already selling is refused") { b.selling = true; CHECK_FALSE(can_be_sold(b, t, false)); }
    SUBCASE("C4 planted is refused") { CHECK_FALSE(can_be_sold(b, t, true)); }
    SUBCASE("dead is refused") { b.state = Building::State::Dead; CHECK_FALSE(can_be_sold(b, t, false)); }
    // CONFIRMED BY ABSENCE (§B5): neither of these is a refusal in the binary.
    SUBCASE("a FULL-HEALTH CONSTRUCTION YARD IS sellable (no last-ConYard rule)") {
        BuildingTypeDef cy = mk_type(3000, 1000);
        cy.conyard = true;
        Building only = mk_bldg(1000, 1000);
        CHECK(can_be_sold(only, cy, false));
    }
    SUBCASE("a DAMAGED building is sellable, and refunds the same as a full one") {
        Building hurt = mk_bldg(1, 1000);
        CHECK(can_be_sold(hurt, t, false));
        CHECK(sell_refund(t.cost, t.soylent, 0.50, true) == 1500);   // no HP term
    }
}

TEST_CASE("sell: the buildup plays MIRRORED [RE: §B2]") {
    Building b = mk_bldg(1000, 1000);
    b.buildup_total = 3.6;                       // BuildupTime .06 -> 54 ticks @15Hz
    const int shp_frames = 20;                   // 10 image frames + 10 shadows
    const int image_frames = shp_frames / 2;
    b.sell_time = 0.0;
    CHECK(mirrored_buildup_frame(b, shp_frames) == image_frames - 1);   // starts at the LAST frame
    b.sell_time = 3.6;
    CHECK(mirrored_buildup_frame(b, shp_frames) == 0);                  // ends at the FIRST
    // Equal and opposite to buildup_frame at every stage.
    for (int k = 0; k < image_frames; ++k) {
        const double t = (b.buildup_total / image_frames) * k + 1e-9;
        b.sell_time = t;
        b.buildup_time = t;
        CHECK(mirrored_buildup_frame(b, shp_frames) ==
              image_frames - 1 - buildup_frame(b, shp_frames));
    }
    SUBCASE("no buildup art -> frame 0 (the instant-removal branch)") {
        b.buildup_total = 0.0;
        CHECK(mirrored_buildup_frame(b, shp_frames) == 0);
        CHECK(mirrored_buildup_frame(b, 0) == 0);
    }
}
```

  Also append to `tests/test_building.cpp`:

```cpp
TEST_CASE("building: Repair & Sell fields are inert by default [RE: repair-sell.md §A2/§B2]") {
    ra2engine::Building b;
    CHECK_FALSE(b.being_repaired);
    CHECK_FALSE(b.selling);
    CHECK(b.sell_time == doctest::Approx(0.0));
    CHECK_FALSE(b.sold);
    ra2engine::BuildingTypeDef t;
    CHECK_FALSE(t.unsellable);
    CHECK(t.click_repairable);          // the recovered DEFAULT YES
    CHECK_FALSE(t.crewed);
    CHECK(t.soylent == 0);
    // kill_building must not disturb any of them (sell sets `sold` itself).
    b.being_repaired = true;
    ra2engine::kill_building(b);
    CHECK(b.being_repaired);
    CHECK_FALSE(b.sold);
}
```

- [ ] **Step 2: Register the new TU and see the tests FAIL to compile.**

  `cmake/ra2_sources.cmake` — add after `${RA2_ROOT}/engine/maplist.cpp` (line 77):

```cmake
    ${RA2_ROOT}/engine/repair_sell.cpp
```

  `CMakeLists.txt` — add after the `test_visibility.cpp` line (213):

```cmake
# Repair & Sell phase [RE: repair-sell.md]: pure repair/sell arithmetic + the
# refusal predicates (14-frame global cadence, the two-integer-division
# stepCost, the FLAT refund, the survivor divisors, the mirrored buildup
# frame). World-free -- it sees only Building/BuildingTypeDef, like
# production.cpp. The World mutation entry points live in engine/world.cpp
# (already on test_world's line), so world.cpp is NOT added here.
target_sources(ra2assets_tests PRIVATE tests/test_repair_sell.cpp engine/repair_sell.cpp)
```

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
```

  Expect a compile failure (`repair_sell.hpp` does not exist). That is the RED state.

- [ ] **Step 3: Implement** `engine/building.hpp`'s eight new fields, `engine/repair_sell.hpp` and `engine/repair_sell.cpp`, exactly as given in **Interfaces**.

- [ ] **Step 4: Build and run the new cases.**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests ra2engine
D:\ra2-android\build\ra2assets_tests.exe --test-case="repair:*"
D:\ra2-android\build\ra2assets_tests.exe --test-case="sell:*"
D:\ra2-android\build\ra2assets_tests.exe --test-case="building:*"
```

  All green. Record the assertion counts.

- [ ] **Step 5: Cheap byte-identity canary** (nothing on a gate path changed, but `building.hpp` is included everywhere — prove it):

```powershell
D:\ra2-android\build\ra2engine.exe --demo-base       # expect hash=5273b5e71ca009e1
```

- [ ] **Step 6: Commit.**

```powershell
git -C "D:\ra2-android" add engine/repair_sell.hpp engine/repair_sell.cpp engine/building.hpp tests/test_repair_sell.cpp tests/test_building.cpp cmake/ra2_sources.cmake CMakeLists.txt
git -C "D:\ra2-android" commit -m "$(printf 'feat(repair-sell): pure repair/sell arithmetic - 14-frame global cadence, two-integer-division stepCost, FLAT refund, survivor divisors, mirrored buildup, refusal predicates [RE: repair-sell.md 2/A4/B1/B2/B3/B5]\n\nCo-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>')"
```

---

### Task 3: Rules plumbing — `section_flag` + the four per-type keys + `RepairSellRules`

**Files:**
- Modify: `include/ra2assets/rules.hpp` (declare `section_flag` beside `section_double`), `src/rules.cpp` (define it beside `section_double` at `:230`)
- Modify: `tests/test_rules.cpp`
- Modify: `engine/main.cpp` (`struct BuildingAssets` — one new member beside `condition_yellow` at `:403`; `load_building_assets` — the `RepairSellRules` resolve beside `:1551-1554`, and four lines in the per-type loop at `:1604-1685`)

**Interfaces:**
- Consumes: `ra2assets::Ini`, the existing `is_truthy` helper used by `Rules::is_construction_yard` (`src/rules.cpp:208`), `Rules::section_double` (`src/rules.cpp:230`), `Rules::general_double` (`src/rules.cpp:220`), `ra2engine::RepairSellRules` (Task 2).
- Produces — `include/ra2assets/rules.hpp`, declared immediately after `section_double`:

```cpp
    // Generic per-section BOOLEAN key with an explicit fallback. Same shape as
    // section_double, but yes/1/true-style values via the same is_truthy the
    // dedicated flag readers (is_construction_yard, spy_sat) already use.
    // Needed because the Repair & Sell phase reads THREE per-type flags whose
    // absent-key defaults DIFFER: Unsellable -> false, Crewed -> false,
    // ClickRepairable -> TRUE [RE: repair-sell.md §A4/§B3/§B5].
    bool section_flag(const std::string& id, const std::string& key, bool fallback) const;
```

- Produces — `src/rules.cpp`, immediately after `section_double`'s body:

```cpp
bool Rules::section_flag(const std::string& id, const std::string& key, bool fallback) const {
    const std::string v = ini_->get(id, key, "");
    if (v.empty()) return fallback;
    return is_truthy(v);
}
```

- Produces — `engine/main.cpp`, one new member on `struct BuildingAssets` beside `condition_yellow` (`:403`):

```cpp
    // Repair & Sell phase [RE: repair-sell.md §A1/§B3]: the four [General]
    // scalars + the three survivor divisors, resolved ONCE here and handed to
    // World::set_repair_sell_rules. The struct's own defaults ARE this
    // install's real values, so a caller that never resolves them still
    // behaves like the original.
    ra2engine::RepairSellRules repair_sell;
```

  and the resolve, inserted immediately after `ba.condition_yellow = ...;` (`main.cpp:1554`):

```cpp
    // rulesmd [General] repair/sell scalars [RE: repair-sell.md §A1/§B3,
    // CONFIRMED]. general_double reads [General] then [AudioVisual] and uses
    // strtod, which stops at '%', so "RepairPercent=15%" reads back as 15.0 --
    // hence the /100.0, exactly as ConditionYellow above.
    // TRAP [§A2]: RepairRate is MINUTES per step; the frame interval is
    // trunc(RepairRate*900) = 14 at .016, NOT 15. repair_interval_frames owns
    // that truncation -- do not pre-compute it here.
    // NOTE [§Ground truth]: URepairStep / URepairPercent DO NOT EXIST in this
    // build, and SurvivorRate is a dead key. Do not add reads for them.
    ba.repair_sell.repair_step    = static_cast<int>(rules.general_double("RepairStep", 8.0));
    ba.repair_sell.repair_percent = rules.general_double("RepairPercent", 15.0) / 100.0;
    ba.repair_sell.repair_rate    = rules.general_double("RepairRate", 0.016);
    ba.repair_sell.refund_percent = rules.general_double("RefundPercent", 50.0) / 100.0;
    ba.repair_sell.allied_survivor_divisor =
        static_cast<int>(rules.general_double("AlliedSurvivorDivisor", 500.0));
    ba.repair_sell.soviet_survivor_divisor =
        static_cast<int>(rules.general_double("SovietSurvivorDivisor", 250.0));
    ba.repair_sell.third_survivor_divisor =
        static_cast<int>(rules.general_double("ThirdSurvivorDivisor", 750.0));
```

  and four lines appended to the per-type resolve loop, immediately after `d.produce_cash_delay = ...;` (`main.cpp:~1635`):

```cpp
        // Repair & Sell [RE: repair-sell.md §A4/§B1/§B3/§B5]. Unsellable /
        // Crewed / Soylent are ABSENT from every stock building section
        // (verified: no RA2 building sets Unsellable, and all 20 Soylent= lines
        // are Yuri-Grinder infantry/vehicles) -> the defaults keep every prior
        // type byte-identical. ClickRepairable's recovered absent-key default
        // is YES, so its fallback is `true`, not `false`.
        d.unsellable       = rules.section_flag(id, "Unsellable", false);
        d.click_repairable = rules.section_flag(id, "ClickRepairable", true);
        d.crewed           = rules.section_flag(id, "Crewed", false);
        d.soylent          = static_cast<int>(rules.section_double(id, "Soylent", 0.0));
```

**world_hash safety:** `Rules::section_flag` is a new accessor with no existing caller. The four per-type reads land in `BuildingTypeDef` fields nothing reads until Task 4, and `BuildingTypeDef` is not hashed (`world_hash` folds `Building{type,cx,cy,hp,state}` — the *index*, not the type record). The seven `general_double` calls read INI text already parsed and in memory: no new file or mix access, no ordering change, no new failure mode (each has a fallback). `BuildingAssets` is not hashed. `load_building_assets` **is** on every gate path, so this task ends with a three-gate spot check.

**Steps:**

- [ ] **Step 1: Write the failing tests.** Append to `tests/test_rules.cpp` (match the file's existing synthetic-INI + guarded-real-install style):

```cpp
TEST_CASE("rules: section_flag honours per-key fallbacks [RE: repair-sell.md §A4]") {
    const std::string text =
        "[GACNST]\n"
        "Cost=3000\n"
        "Strength=1000\n"
        "Crewed=yes\n"
        "[GAPOWR]\n"
        "Cost=800\n"
        "Strength=1000\n"
        "Crewed=no\n"
        "ClickRepairable=no\n";
    const ra2assets::Ini ini = ra2assets::Ini::parse(text);
    const ra2assets::Rules r = ra2assets::Rules::load(ini);
    CHECK(r.section_flag("GACNST", "Crewed", false));
    CHECK_FALSE(r.section_flag("GAPOWR", "Crewed", false));
    // absent key -> the CALLER's fallback, which differs per key
    CHECK(r.section_flag("GACNST", "ClickRepairable", true));    // absent -> default YES
    CHECK_FALSE(r.section_flag("GAPOWR", "ClickRepairable", true));
    CHECK_FALSE(r.section_flag("GACNST", "Unsellable", false));
    CHECK_FALSE(r.section_flag("NOSUCH", "Crewed", false));
    CHECK(r.section_flag("NOSUCH", "ClickRepairable", true));
}

TEST_CASE("rules: the real rulesmd repair/sell scalars [RE: repair-sell.md §Ground truth]") {
    const char* dir = std::getenv("RA2_INSTALL_DIR");
    const std::string base = dir ? dir : "D:\\Red Alert 2";
    // (Use whatever helper this file already uses to load rulesmd.ini from the
    // install; skip cleanly when it is unavailable.)
    if (/* rulesmd unavailable */ false) { MESSAGE("skipped: no install"); return; }
    // ... load `rules` from the real rulesmd.ini ...
    CHECK(rules.general_double("RepairStep", -1.0) == doctest::Approx(8.0));
    CHECK(rules.general_double("RepairPercent", -1.0) == doctest::Approx(15.0));   // "15%" -> 15
    CHECK(rules.general_double("RepairRate", -1.0) == doctest::Approx(0.016));
    CHECK(rules.general_double("RefundPercent", -1.0) == doctest::Approx(50.0));   // "50%" -> 50
    CHECK(rules.general_double("AlliedSurvivorDivisor", -1.0) == doctest::Approx(500.0));
    CHECK(rules.general_double("SovietSurvivorDivisor", -1.0) == doctest::Approx(250.0));
    CHECK(rules.general_double("ThirdSurvivorDivisor", -1.0) == doctest::Approx(750.0));
    // URepairStep / URepairPercent DO NOT EXIST in this build (§Ground truth).
    CHECK(rules.general_double("URepairStep", -1.0) == doctest::Approx(-1.0));
    CHECK(rules.general_double("URepairPercent", -1.0) == doctest::Approx(-1.0));
    // Per-type flags [RE-GATE: repair-sell.md §D item 9 -- use the MEASURED row]
    CHECK(rules.section_flag("GACNST", "Crewed", false));
    CHECK_FALSE(rules.section_flag("GAPOWR", "Crewed", false));
    CHECK_FALSE(rules.section_flag("GACNST", "Unsellable", false));
    CHECK(rules.section_flag("GACNST", "ClickRepairable", true));   // absent -> yes
    CHECK(rules.section_double("GACNST", "Soylent", 0.0) == doctest::Approx(0.0));
}
```

  (Follow `tests/test_rules.cpp`'s existing pattern for loading the real `rulesmd.ini` — reuse whatever helper it already has rather than inventing a new one, and keep the `MESSAGE("skipped: …"); return;` guard.)

- [ ] **Step 2: Run and see them FAIL.**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
D:\ra2-android\build\ra2assets_tests.exe --test-case="rules: section_flag*"
```

  Expect a compile error (no `section_flag`). RED.

- [ ] **Step 3: Implement** `Rules::section_flag` (header decl + `src/rules.cpp` body), the `BuildingAssets::repair_sell` member, the seven-line `[General]` resolve and the four per-type lines, exactly as given.

- [ ] **Step 4: Build and run.**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests ra2engine
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2assets_tests.exe --test-case="rules:*"
```

  Green. **If the real-install case reports anything other than 8 / 15 / .016 / 50 / 500 / 250 / 750, STOP and report** — the hunt's ground truth would be contradicted and the whole phase's arithmetic is downstream of it.

- [ ] **Step 5: Three-gate spot check, ONE PER FOREGROUND CALL** (`load_building_assets` is on every gate path):

```powershell
D:\ra2-android\build\ra2engine.exe --demo-base       # expect hash=5273b5e71ca009e1
```
```powershell
D:\ra2-android\build\ra2engine.exe --demo-econ       # expect econ_hash=7fed96cb98b68017
```
```powershell
D:\ra2-android\build\ra2engine.exe --demo-roster     # expect roster_hash=333cd43d8b5ece6a
```

- [ ] **Step 6: Commit.**

```powershell
git -C "D:\ra2-android" add include/ra2assets/rules.hpp src/rules.cpp tests/test_rules.cpp engine/main.cpp
git -C "D:\ra2-android" commit -m "$(printf 'feat(rules): section_flag + the repair/sell [General] scalars and per-type keys (Unsellable/ClickRepairable=yes-default/Crewed/Soylent) [RE: repair-sell.md ground-truth/A1/A4/B1/B3/B5]\n\nCo-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>')"
```

---

### Task 4: The `World` repair tick + the firewalled pass (19-GATE RE-VERIFY)

**Files:**
- Modify: `engine/world.hpp` (public API after `repair_building` at `:876`; private state before the closing `};` at `:1219`; `#include "repair_sell.hpp"`)
- Modify: `engine/world.cpp` (the new method bodies beside `repair_building` at `:1420-1429`; one line INSIDE `repair_building`; the pass insertion at `:2859`)
- Modify: `tests/test_world.cpp`

**Interfaces:**
- Consumes: `ra2engine::RepairSellRules`, `repair_interval_frames`, `repair_due`, `repair_step_cost`, `can_be_click_repaired` (Task 2); `Building`, `BuildingTypeDef`; `World::types_` (`world.hpp:1204`, populated by `set_economy`), `World::house_credits_` (`:1217`), `World::are_allied` (`:819`).
- Produces — `engine/world.hpp`, public, inserted immediately after `bool repair_building(int b_idx, int by_house);` (`:876`):

```cpp
    // --- Repair & Sell phase [RE: repair-sell.md] -------------------------
    // The four [General] scalars + the three survivor divisors. The struct's
    // defaults ARE this install's real values, so a World that never calls
    // this already behaves like the original; main.cpp overrides from rulesmd.
    void set_repair_sell_rules(const RepairSellRules& r) { repair_rules_ = r; }
    const RepairSellRules& repair_sell_rules() const { return repair_rules_; }

    // BuildingClass::Repair(control) [RE: repair-sell.md §A4, vtbl+0x19C].
    // control: -1 toggle / 0 off / 1 on -- the classic Westwood convention.
    // Returns true iff the flag CHANGED. Refuses (false, no change) unless:
    //   * b_idx names an alive, non-Dead building,
    //   * b.house == by_house (the network REPAIR/SELL event is authoritative
    //     about ownership, §C1 -- you cannot repair someone else's building),
    //   * types_ covers b.type (set_economy must have run; repair needs Cost
    //     and Strength to price a step), and
    //   * turning ON additionally passes can_be_click_repaired (§A4).
    // Turning ON opens repair_sell_any_. Turning OFF is always allowed on a
    // live building (it is off-ramp 1/2/5 -- see step_repair_sell for 3/4).
    bool set_repair(int b_idx, int by_house, int control);
    bool toggle_repair(int b_idx, int by_house) { return set_repair(b_idx, by_house, -1); }
    bool building_repairing(int b_idx) const {
        return b_idx >= 0 && b_idx < static_cast<int>(buildings_.size()) &&
               buildings_[b_idx].being_repaired;
    }
    // The GLOBAL repair cadence counter [RE: §A2]. It advances EVERY frame,
    // above the firewall, so `frame % interval == 0` is an ABSOLUTE phase --
    // all repairing buildings step together and a newly-armed one waits
    // 0..interval-1 frames, exactly like the original's `Frame % interval`.
    std::uint64_t repair_frame() const { return repair_frame_; }
    // The firewalled per-frame Repair & Sell pass. Public so a gate/test can
    // drive it directly; normally called from World::step.
    void step_repair_sell(double dt);
```

  and, in the private block immediately before the class's closing `};` (`world.hpp:1218-1219`):

```cpp
    // --- Repair & Sell phase [RE: repair-sell.md] -------------------------
    // ALL inert by default. repair_sell_any_ is opened ONLY by set_repair
    // (turning ON) and begin_sell, neither of which ANY existing demo calls --
    // so step_repair_sell costs every existing gate one increment and one
    // branch, and none of this state is read by world_hash.
    RepairSellRules repair_rules_;              // recovered defaults [§A1]
    bool            repair_sell_any_ = false;   // the firewall
    std::uint64_t   repair_frame_ = 0;          // the global cadence phase [§A2]
```

  `engine/world.hpp` also gains `#include "repair_sell.hpp"` beside its existing `#include "building.hpp"`.

- Produces — `engine/world.cpp`, inserted immediately after `World::repair_building`'s closing brace (`:1429`):

```cpp
bool World::set_repair(int b_idx, int by_house, int control) {
    if (b_idx < 0 || b_idx >= static_cast<int>(buildings_.size())) return false;
    Building& b = buildings_[b_idx];
    if (!b.alive || b.state == Building::State::Dead) return false;
    // Ownership [RE: repair-sell.md §C1] -- the network REPAIR event resolves
    // the target and the SELL event checks Owner->[0x30] == ev.HouseIndex.
    // OWN house only: unlike the engineer path (repair_building), the wrench is
    // not an allied action.
    if (b.house != by_house) return false;
    if (b.type < 0 || b.type >= static_cast<int>(types_.size())) return false;
    const bool cur = b.being_repaired;
    bool next = cur;
    if (control < 0) next = !cur;              // -1 toggle
    else if (control == 0) next = false;       //  0 off
    else next = true;                          //  1 on
    if (next == cur) return false;
    if (next) {
        // [RE: §A4, FUN_00452620] Only turning ON is gated -- turning OFF must
        // always work (off-ramps 2 and 5 call Repair(0) unconditionally).
        if (!can_be_click_repaired(b, types_[b.type])) return false;
        if (b.selling) return false;           // [§B2] a selling building cannot repair
        b.being_repaired = true;
        repair_sell_any_ = true;               // open the firewall
    } else {
        b.being_repaired = false;
    }
    return true;
}

void World::step_repair_sell(double dt) {
    // The cadence counter advances UNCONDITIONALLY, above the firewall, so the
    // phase is ABSOLUTE [RE: repair-sell.md §A2 -- `Frame % interval`, not a
    // per-building timer]. Incrementing a counter nothing hashes is byte-safe.
    const std::uint64_t frame = repair_frame_++;
    // FIREWALL (spec §Global Constraints): until the first repair/sell order
    // ever, this whole pass is one branch -- the only cost every existing demo
    // pays. world_hash is untouched either way.
    if (!repair_sell_any_) return;

    // --- Sell missions land in Task 5; the repair tick is below. ---
    (void)dt;

    // [RE: §A2] interval = trunc(RepairRate * 900) = 14 at .016 -- a GLOBAL
    // phase shared by every repairing building of every house.
    const int interval = repair_interval_frames(repair_rules_.repair_rate);
    if (!repair_due(frame, interval)) return;

    // Ascending building index -- deterministic, and it matches the original's
    // object-order walk (one shared Available_Money() consulted per building,
    // so an early building CAN drain the pool and switch a later one off the
    // same frame; §A3).
    for (Building& b : buildings_) {
        if (!b.alive || b.state == Building::State::Dead) continue;
        if (!b.being_repaired) continue;
        if (b.selling) { b.being_repaired = false; continue; }   // [§B2 entry: Repair(0)]
        if (b.type < 0 || b.type >= static_cast<int>(types_.size())) {
            b.being_repaired = false;
            continue;
        }
        const BuildingTypeDef& t = types_[b.type];
        // [RE: §A2, FUN_00711470] BOTH divisions integer. Depends only on the
        // type, so this is a pure function of (Cost, Strength, rules).
        const int cost =
            repair_step_cost(t.cost, t.strength, repair_rules_.repair_step,
                             repair_rules_.repair_percent);
        if (b.house < 0 || b.house >= house_count()) { b.being_repaired = false; continue; }
        // [RE: §A3, CONFIRMED] The affordability check is BEFORE the spend:
        // too poor -> the flag is CLEARED, no partial step, no free repair, no
        // EVA line. The player must click the wrench again after refilling.
        if (house_credits_[b.house] < static_cast<double>(cost)) {
            b.being_repaired = false;
            continue;
        }
        house_credits_[b.house] -= static_cast<double>(cost);
        // [RE: §A2] +RepairStep HP flat (8), clamped to Strength. The original
        // advances Health AND EstimatedHealth; the clone has one hp field.
        b.hp += repair_rules_.repair_step;
        if (b.hp >= b.max_hp) {
            b.hp = b.max_hp;
            b.being_repaired = false;          // AUTO-STOP at full health [§A2]
        }
    }
}
```

  and **one line inside `World::repair_building`** (the engineer path, `world.cpp:1420-1429`), immediately after `b.hp = b.max_hp;`:

```cpp
    b.being_repaired = false;   // off-ramp 5 [RE: repair-sell.md §A4/§A6]:
                                // Repair_To_Full (vtbl+0x40C) calls Repair(0).
                                // A no-op wherever the flag was never set, so
                                // --demo-capture stays byte-identical.
```

  and the **pass insertion** in `World::step`, immediately after the dead-building blocking-release loop's closing brace (`world.cpp:2859`) and before `const std::vector<MoveRequest> reqs = step_combat(...)` (`:2861`):

```cpp
    // Repair & Sell [RE: repair-sell.md §A2/§B2]: the wrench tick + the sell
    // missions. A pure INSERTION beside step_cargo/step_capture -- it runs
    // AFTER step_buildings (so a building that flipped Buildup->Idle this frame
    // is already repairable, and a building sold LAST frame has already had its
    // foundation released above) and BEFORE step_combat (so a completed sale is
    // Dead before targeting runs). `if (!repair_sell_any_) return;` inside is
    // the only cost every existing demo pays -- no demo issues a repair or sell
    // order [spec §Global Constraints].
    step_repair_sell(dt);
```

**world_hash safety:** `world_hash` (`main.cpp:5665`) is not modified. The pass is a pure insertion that mutates nothing while `repair_sell_any_ == false`, and the only way to open that flag is `set_repair(..., on)` or `begin_sell` — neither of which exists on any gate path (grep `toggle_repair|set_repair|begin_sell` across `main.cpp` after this task: the only hits are these definitions). `repair_frame_` is incremented unconditionally but is read by nothing outside the pass and is not hashed. The one new line in `repair_building` writes `false` over a field that is already `false` on every `--demo-capture` path. **This task inserts a pass into `World::step`, so it ends with the FULL 19-gate re-verify.**

**Steps:**

- [ ] **Step 1: Write the failing tests.** Append to `tests/test_world.cpp` (reuse whatever `World` fixture the file already uses to get a map + a roster; the sketch below names the APIs, not the fixture):

```cpp
TEST_CASE("world: step_repair_sell is a strict no-op with the firewall closed [RE: repair-sell.md]") {
    // A stock world nobody ever ordered to repair must be untouched by the new
    // pass -- this is the byte-identity guarantee in test form.
    /* build a world with a roster via set_economy and one damaged building */
    const int before_hp = world.buildings()[b].hp;
    const double before_credits = world.house_credits(0);
    for (int k = 0; k < 100; ++k) world.step(1.0 / 15.0);
    CHECK(world.buildings()[b].hp == before_hp);
    CHECK(world.house_credits(0) == doctest::Approx(before_credits));
    CHECK_FALSE(world.building_repairing(b));
    CHECK(world.repair_frame() == 100);        // the counter still advanced
}

TEST_CASE("world: set_repair refuses per the recovered gates [RE: repair-sell.md §A4/§C1]") {
    /* b = an own (house 0) damaged Idle building; e = a house-1 building */
    CHECK_FALSE(world.set_repair(-1, 0, 1));                 // bad index
    CHECK_FALSE(world.set_repair(e, 0, 1));                  // NOT your building
    CHECK(world.set_repair(b, 0, 1));                        // arms
    CHECK(world.building_repairing(b));
    CHECK_FALSE(world.set_repair(b, 0, 1));                  // already on -> no change
    CHECK(world.toggle_repair(b, 0));                        // -1 toggles OFF
    CHECK_FALSE(world.building_repairing(b));
    CHECK(world.toggle_repair(b, 0));                        // -1 toggles ON
    /* full-health building f */
    CHECK_FALSE(world.set_repair(f, 0, 1));                  // nothing to repair
    /* Buildup-state building u */
    CHECK_FALSE(world.set_repair(u, 0, 1));                  // under construction
}

TEST_CASE("world: the repair tick is a 14-frame GLOBAL phase, +8 HP, charged [RE: §A2]") {
    /* b: GACNST-like, cost 3000 strength 1000, hp 500; house 0 credits 10000 */
    REQUIRE(world.set_repair(b, 0, 1));
    const int step_cost = ra2engine::repair_step_cost(3000, 1000, 8, 0.15);   // 3
    CHECK(step_cost == 3);
    std::vector<std::uint64_t> hit_frames;
    int prev_hp = world.buildings()[b].hp;
    double prev_credits = world.house_credits(0);
    for (int k = 0; k < 60; ++k) {
        const std::uint64_t f = world.repair_frame();
        world.step(1.0 / 15.0);
        if (world.buildings()[b].hp != prev_hp) {
            hit_frames.push_back(f);
            CHECK(world.buildings()[b].hp == prev_hp + 8);           // +8 flat
            CHECK(world.house_credits(0) == doctest::Approx(prev_credits - step_cost));
            prev_hp = world.buildings()[b].hp;
            prev_credits = world.house_credits(0);
        } else {
            CHECK(world.house_credits(0) == doctest::Approx(prev_credits));  // no idle drain
        }
    }
    REQUIRE(hit_frames.size() >= 4);
    for (std::size_t i = 1; i < hit_frames.size(); ++i)
        CHECK(hit_frames[i] - hit_frames[i - 1] == 14);               // NOT 15
    for (std::uint64_t f : hit_frames) CHECK(f % 14 == 0);            // an ABSOLUTE phase
}

TEST_CASE("world: repair auto-stops at full health [RE: §A2]") {
    /* b at max_hp - 4 (less than one 8-HP step) */
    REQUIRE(world.set_repair(b, 0, 1));
    for (int k = 0; k < 30; ++k) world.step(1.0 / 15.0);
    CHECK(world.buildings()[b].hp == world.buildings()[b].max_hp);    // clamped, never over
    CHECK_FALSE(world.building_repairing(b));                        // cleared itself
}

TEST_CASE("world: out of credits STOPS repair silently, with no partial step [RE: §A3]") {
    /* b: cost 3000 strength 1000 (stepCost 3), hp 500 */
    world.grant_credits_for_test(0, 2.0);                            // less than one step
    REQUIRE(world.set_repair(b, 0, 1));
    const int hp0 = world.buildings()[b].hp;
    for (int k = 0; k < 30; ++k) world.step(1.0 / 15.0);
    CHECK(world.buildings()[b].hp == hp0);                           // NO partial repair
    CHECK(world.house_credits(0) == doctest::Approx(2.0));           // and NO free repair
    CHECK_FALSE(world.building_repairing(b));                        // the flag was cleared
    // Refilling does NOT resume by itself -- the player must click again (§A3).
    world.grant_credits_for_test(0, 10000.0);
    for (int k = 0; k < 30; ++k) world.step(1.0 / 15.0);
    CHECK(world.buildings()[b].hp == hp0);
}

TEST_CASE("world: the engineer path clears the wrench flag (off-ramp 5) [RE: §A4/§A6]") {
    REQUIRE(world.set_repair(b, 0, 1));
    CHECK(world.repair_building(b, 0));            // instant, free, separate path
    CHECK(world.buildings()[b].hp == world.buildings()[b].max_hp);
    CHECK_FALSE(world.building_repairing(b));
}
```

- [ ] **Step 2: Run and see them FAIL.**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
D:\ra2-android\build\ra2assets_tests.exe --test-case="world: *repair*"
```

  Expect a compile error (no `set_repair`). RED.

- [ ] **Step 3: Implement** the `world.hpp` include + public API + private state, the two `world.cpp` bodies, the one-line addition inside `repair_building`, and the `step_repair_sell(dt)` insertion at `world.cpp:2859` — exactly as given.

- [ ] **Step 4: Build and run the new cases.**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests ra2engine
D:\ra2-android\build\ra2assets_tests.exe --test-case="world: *repair*"
D:\ra2-android\build\ra2assets_tests.exe --test-case="world:*"
```

  All green, no regressions in the existing `world:*` cases.

- [ ] **Step 5: RE-VERIFY ALL 19 GATES EXACT — ONE PER FOREGROUND CALL.** This is the commit that inserts a pass into `World::step`; it is the phase's highest-risk touch. Run the 19-gate block from **Global Constraints**, one `ra2engine.exe --demo-X` per tool call, timeout 600000 ms (~2 min each; **NEVER batch — a multi-gate call auto-backgrounds and reads as a false hang**). Every printed pin must match the expected list exactly. **If ANY gate moves, STOP** — the firewall leaked, and the fix is to find what opened `repair_sell_any_`, not to re-pin a hash.

- [ ] **Step 6: Commit.**

```powershell
git -C "D:\ra2-android" add engine/world.hpp engine/world.cpp tests/test_world.cpp
git -C "D:\ra2-android" commit -m "$(printf 'feat(repair): the wrench toggle + the firewalled repair tick - global 14-frame phase, +8 HP, integer stepCost charge, auto-stop at full, silent stop when broke [RE: repair-sell.md 2/A3/A4]\n\nAll 19 byte-identity gates re-verified EXACT after inserting step_repair_sell\ninto World::step.\n\nCo-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>')"
```

---

### Task 5: The sell mission — mirrored buildup, flat refund, survivors, removal

**Files:**
- Modify: `engine/world.hpp` (public API beside Task 4's block; private `survivor_crew_`/`survivor_crew_set_`)
- Modify: `engine/world.cpp` (`begin_sell`, `set_survivor_crew`, the private `complete_sell`, and the sell half of `step_repair_sell`)
- Modify: `tests/test_world.cpp`

**Interfaces:**
- Consumes: `sell_refund`, `survivor_divisor`, `survivor_count`, `can_be_sold` (Task 2); `World::set_repair` (Task 4); `kill_building` (`building.hpp:159`); `foundation_cells` (`building.hpp:104`); `World::add_combat_unit` (`world.hpp:537`), `walkable`, `occ_.is_free`, `house_side` (`:797`), `ai_enabled` (`:729`), `house_credits_` (`:1217`), `passable_` (`:1184`), `grid_w_`/`grid_h_`.
- Produces — `engine/world.hpp`, appended to Task 4's public block:

```cpp
    // BuildingClass::Sell_Back(-1) [RE: repair-sell.md §B2/§B5, vtbl+0x1A0].
    // Starts mission 0x13 (Selling). Returns true iff the mission STARTED (or,
    // for a type with no buildup art, completed instantly -- §B5 item 6).
    // Refuses per can_be_sold PLUS ownership (b.house == by_house -- the SELL
    // event is authoritative, §C1) PLUS types_ coverage. Calls
    // set_repair(b_idx, by_house, 0) FIRST -- selling always cancels repair
    // (§B2 entry, 0x00449B81). Opens repair_sell_any_.
    //
    // c4_applied is passed through to can_be_sold; the clone has no C4 mechanic
    // until roadmap P4, so every caller passes false today.
    bool begin_sell(int b_idx, int by_house, bool c4_applied = false);
    bool building_selling(int b_idx) const {
        return b_idx >= 0 && b_idx < static_cast<int>(buildings_.size()) &&
               buildings_[b_idx].selling;
    }
    // The crew UnitCombat spawned as a survivor for a given side [RE: §B3].
    // World is ASSET-FREE, so the CALLER resolves the infantry (E1 Allied /
    // E2 Soviet) and registers it here -- the order_spy_infiltrate precedent
    // (pre-resolved params stored so the pass needs no roster).
    // side_ix is World::Side's underlying value. UNREGISTERED (the default for
    // every existing World) -> that side spawns ZERO survivors, so nothing
    // spawns anywhere until a caller opts in.
    // [RE-GATE APPLIED: repair-sell.md §D item 1] the 25%-chance special crew
    // type for building purpose-class 7 is CUT: Rules+0xF70 was never
    // string-anchored, and it is an RNG draw that would break gate determinism.
    void set_survivor_crew(int side_ix, const UnitCombat& c);
```

  private, appended to Task 4's private block:

```cpp
    std::vector<UnitCombat>    survivor_crew_;      // by side_ix; empty = none registered
    std::vector<std::uint8_t>  survivor_crew_set_;  // parallel "was registered" flags
    // Finish a sell mission: free the footprint, spawn survivors, pay the FLAT
    // refund, then kill + mark sold. See world.cpp for the recovered ordering.
    void complete_sell(int b_idx);
```

- Produces — `engine/world.cpp`, beside Task 4's bodies:

```cpp
void World::set_survivor_crew(int side_ix, const UnitCombat& c) {
    if (side_ix < 0 || side_ix > 3) return;
    if (static_cast<int>(survivor_crew_.size()) <= side_ix) {
        survivor_crew_.resize(side_ix + 1);
        survivor_crew_set_.resize(side_ix + 1, 0);
    }
    survivor_crew_[side_ix] = c;
    survivor_crew_set_[side_ix] = 1;
}

bool World::begin_sell(int b_idx, int by_house, bool c4_applied) {
    if (b_idx < 0 || b_idx >= static_cast<int>(buildings_.size())) return false;
    Building& b = buildings_[b_idx];
    // Ownership [RE: repair-sell.md §C1] -- the SELL event checks
    // Owner->[0x30] == ev.HouseIndex: you cannot sell someone else's building
    // even if the cursor lets you click it.
    if (b.house != by_house) return false;
    if (b.type < 0 || b.type >= static_cast<int>(types_.size())) return false;
    // [RE: §B5, CONFIRMED] Unsellable / under construction / already selling /
    // C4. NO last-ConYard rule and NO captured rule -- both were searched for
    // in the binary and neither exists. DO NOT ADD EITHER.
    if (!can_be_sold(b, types_[b.type], c4_applied)) return false;
    repair_sell_any_ = true;
    // [RE: §B2, 0x00449B81] Mission_Selling's FIRST act is Repair(0).
    set_repair(b_idx, by_house, 0);
    b.selling = true;
    b.sell_time = 0.0;
    // [RE: §B5 item 6] A type with no buildup art takes the INSTANT-removal
    // branch at 0x00447115 instead of the mission (the wall/fence case).
    if (b.buildup_total <= 0.0) {
        complete_sell(b_idx);
    }
    return true;
}

void World::complete_sell(int b_idx) {
    Building& b = buildings_[b_idx];
    const BuildingTypeDef& t = types_[b.type];
    const int house = b.house;

    // (1) Free the footprint FIRST. World::step's own release loop only fires
    // on a building that was ALREADY Dead when the loop ran, i.e. next frame --
    // but the survivors must stand on this footprint THIS frame (§B3: they exit
    // onto the building's own foundation cells), so we run the same three lines
    // here and latch blocking_released so the loop never repeats them.
    for (const Cellxy& cell : foundation_cells(b.cx, b.cy, b.fw, b.fh)) {
        if (cell.x < 0 || cell.y < 0 || cell.x >= grid_w_ || cell.y >= grid_h_) continue;
        passable_[static_cast<std::size_t>(cell.y) * grid_w_ + cell.x] = 1;
    }
    b.blocking_released = true;

    // (2) The refund [RE: §B1, CONFIRMED]. FLAT -- no health term anywhere.
    // RefundPercent applies ONLY to a human-controlled house; an AI house gets
    // the UNSCALED value. The two cost multipliers are the FactoryPlant/
    // OrePurifier/difficulty modifiers, 1.0 in this engine.
    const bool human = !ai_enabled(house);
    const int refund = sell_refund(t.cost, t.soylent, repair_rules_.refund_percent, human);

    // (3) Survivors [RE: §B3, CONFIRMED]: clamp(refund / divisor, 1, 5), zero
    // when the type is not Crewed. They exit onto the foundation cells in
    // foundation_cells order; a survivor whose cell is blocked is DELETED, not
    // displaced (that is the recovered behaviour, not a shortcut).
    // has_been_captured is FALSE: the clone's capture_building records no
    // captured flag on the building, so the x2 divisor is implemented and
    // unit-tested but never triggered here.
    // [RE-FALLBACK: captured-flag plumbing -- add it when capture records one.]
    const int side_ix = static_cast<int>(house_side(house));
    const int n = survivor_count(refund, survivor_divisor(repair_rules_, side_ix), t.crewed,
                                 /*has_been_captured=*/false);
    if (n > 0 && side_ix >= 0 && side_ix < static_cast<int>(survivor_crew_.size()) &&
        survivor_crew_set_[side_ix]) {
        const UnitCombat& crew = survivor_crew_[side_ix];
        int placed = 0;
        for (const Cellxy& cell : foundation_cells(b.cx, b.cy, b.fw, b.fh)) {
            if (placed >= n) break;
            // occ_.is_free (not unit_at_cell) so a mid-hop unit's reserved next
            // cell is respected -- the launch_unit free_cell rule.
            if (!walkable(cell.x, cell.y) || !occ_.is_free(cell.x, cell.y)) continue;
            add_combat_unit(cell.x, cell.y, house, crew);
            ++placed;
        }
    }

    // (4) Pay, then remove. `buildings_` may have been reallocated by
    // add_combat_unit? No -- add_combat_unit touches units_, not buildings_ --
    // but re-fetch anyway so this block is robust to future changes.
    Building& bb = buildings_[b_idx];
    if (house >= 0 && house < house_count())
        house_credits_[house] += static_cast<double>(refund);
    kill_building(bb);          // Dead + die_time 0; step_buildings tombstones it
    bb.sold = true;             // suppresses the death explosion (it was sold, not destroyed)
    bb.selling = false;
    bb.being_repaired = false;
}
```

  and the **sell half** of `step_repair_sell`, replacing the `// --- Sell missions land in Task 5; the repair tick is below. ---` / `(void)dt;` placeholder from Task 4:

```cpp
    // --- Sell missions [RE: repair-sell.md §B2]: the buildup runs BACKWARDS
    // over the same frame count at the same rate, and ONLY on completion do the
    // credits land and the survivors spawn. Ascending index, deterministic.
    // Indexed (not range-for) because complete_sell spawns units and we re-read
    // buildings_ inside it.
    for (int i = 0; i < static_cast<int>(buildings_.size()); ++i) {
        Building& b = buildings_[i];
        if (!b.alive || !b.selling) continue;
        if (b.state == Building::State::Dead) { b.selling = false; continue; }
        b.sell_time += dt;
        // sell_duration == buildup_total: image_frames * max(1,
        // trunc(BuildupTime*900/image_frames)) / 15 -- the SAME value
        // main.cpp already computed for construction (building-render.md §5).
        if (b.sell_time >= b.buildup_total) complete_sell(i);
    }
```

  (and the `(void)dt;` line is deleted, since `dt` is now used).

**world_hash safety:** `begin_sell` is the only other opener of `repair_sell_any_`, and nothing on a gate path calls it (grep `begin_sell` in `main.cpp` after this task: only Task 6's `GameScene` and Task 7's gate). `complete_sell` is private and reachable only from `begin_sell`/`step_repair_sell`. `world_hash` untouched. **This task changes `World::step`'s behavior on the open-firewall path, so it ends with a second full 19-gate re-verify.**

**Steps:**

- [ ] **Step 1: Write the failing tests.** Append to `tests/test_world.cpp`:

```cpp
TEST_CASE("world: begin_sell refuses per the recovered gates [RE: repair-sell.md §B5/§C1]") {
    CHECK_FALSE(world.begin_sell(-1, 0));                    // bad index
    CHECK_FALSE(world.begin_sell(e, 0));                     // NOT your building (house 1)
    /* u = a Buildup-state own building */
    CHECK_FALSE(world.begin_sell(u, 0));                     // under construction
    CHECK(world.begin_sell(b, 0));                           // starts the mission
    CHECK(world.building_selling(b));
    CHECK_FALSE(world.begin_sell(b, 0));                     // already selling
    CHECK_FALSE(world.begin_sell(b, 0, /*c4_applied=*/true));
    // CONFIRMED BY ABSENCE (§B5): a full-health, only Construction Yard sells.
    CHECK(world.begin_sell(cy, 0));
}

TEST_CASE("world: selling cancels repair first [RE: §B2 entry]") {
    REQUIRE(world.set_repair(b, 0, 1));
    REQUIRE(world.begin_sell(b, 0));
    CHECK_FALSE(world.building_repairing(b));
}

TEST_CASE("world: the sell mission pays ONLY on completion, FLAT [RE: §B1/§B2]") {
    /* b: GACNST-like, cost 3000, hp 1 of 1000 (nearly dead), buildup_total 3.6,
       house 0 = human (ai_enabled(0) == false), credits 1000 */
    const double c0 = world.house_credits(0);
    REQUIRE(world.begin_sell(b, 0));
    int frames = 0;
    while (world.building_selling(b) && frames < 200) {
        world.step(1.0 / 15.0);
        ++frames;
        if (world.building_selling(b))
            CHECK(world.house_credits(0) == doctest::Approx(c0));   // NOTHING until the end
    }
    CHECK(frames > 1);                                              // it is a MISSION, not instant
    CHECK(frames == doctest::Approx(3.6 * 15.0).epsilon(0.10));     // the buildup duration
    // FLAT refund: 1 HP of 1000 refunds exactly what full health would.
    CHECK(world.house_credits(0) == doctest::Approx(c0 + 1500.0));
    CHECK(world.buildings()[b].state == ra2engine::Building::State::Dead);
    CHECK(world.buildings()[b].sold);
    CHECK_FALSE(world.buildings()[b].selling);
    // the footprint is walkable again
    CHECK(world.walkable(world.buildings()[b].cx, world.buildings()[b].cy));
}

TEST_CASE("world: survivors spawn on the foundation [RE: §B3]") {
    /* Allied house 0, GACNST-like with crewed = true, refund 1500 -> 3 */
    world.set_survivor_crew(static_cast<int>(ra2engine::World::Side::Allied), gi_combat);
    const int u0 = static_cast<int>(world.units().size());
    REQUIRE(world.begin_sell(b, 0));
    for (int k = 0; k < 200 && world.building_selling(b); ++k) world.step(1.0 / 15.0);
    CHECK(static_cast<int>(world.units().size()) == u0 + 3);
    SUBCASE("no registered crew -> no survivors") { /* fresh world, no set_survivor_crew */ }
    SUBCASE("Crewed=no -> no survivors") { /* type with crewed == false -> u0 unchanged */ }
}

TEST_CASE("world: an AI owner gets the UNSCALED refund [RE: §B1]") {
    /* house 1 is AI-enabled; b1 = a house-1 cost-3000 building */
    const double c0 = world.house_credits(1);
    REQUIRE(world.begin_sell(b1, 1));
    for (int k = 0; k < 200 && world.building_selling(b1); ++k) world.step(1.0 / 15.0);
    CHECK(world.house_credits(1) == doctest::Approx(c0 + 3000.0));   // NOT 1500
}

TEST_CASE("world: a type with no buildup art sells INSTANTLY [RE: §B5 item 6]") {
    /* w: buildup_total == 0.0 */
    const double c0 = world.house_credits(0);
    CHECK(world.begin_sell(w, 0));
    CHECK_FALSE(world.building_selling(w));                          // completed inside begin_sell
    CHECK(world.buildings()[w].state == ra2engine::Building::State::Dead);
    CHECK(world.house_credits(0) > c0);
}
```

- [ ] **Step 2: Run and see them FAIL.**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
D:\ra2-android\build\ra2assets_tests.exe --test-case="world: *sell*"
```

  RED (no `begin_sell`).

- [ ] **Step 3: Implement** the `world.hpp` API + private state, `set_survivor_crew`, `begin_sell`, `complete_sell`, and the sell half of `step_repair_sell` — exactly as given.

- [ ] **Step 4: Build and run.**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests ra2engine
D:\ra2-android\build\ra2assets_tests.exe --test-case="world:*"
```

  All green.

- [ ] **Step 5: RE-VERIFY ALL 19 GATES EXACT — ONE PER FOREGROUND CALL.** Same block, same discipline as Task 4 Step 5. `--demo-capture` deserves particular attention (it is the gate that exercises `repair_building`).

- [ ] **Step 6: Commit.**

```powershell
git -C "D:\ra2-android" add engine/world.hpp engine/world.cpp tests/test_world.cpp
git -C "D:\ra2-android" commit -m "$(printf 'feat(sell): sell as a timed MISSION - mirrored buildup, FLAT refund (no HP scaling, human/AI asymmetry, Soylent override), survivors on the foundation, instant branch for buildup-less types [RE: repair-sell.md B1/B2/B3/B5]\n\nAll 19 byte-identity gates re-verified EXACT.\n\nCo-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>')"
```

---

### Task 6: The two buttons as mutually-exclusive cursor modes

**Files:**
- Modify: `engine/sidebar.hpp` (5 constants after `kPowerPitch` at `:42`; 2 rects + 2 predicates on `SidebarLayout`), `engine/sidebar.cpp` (fill them in `compute`; the two predicates)
- Modify: `tests/test_sidebar.cpp`
- Modify: `engine/input.hpp` (`BaseMode`, 6 public methods, 4 `InputActions` fields, 2 privates), `engine/input.cpp` (the setters, `cancel_one_mode`, the intercept block)
- Modify: `tests/test_input.cpp`
- Modify: `engine/main.cpp` (`draw_sidebar` pressed frames; `GameScene::handle` hit-tests + Escape; `GameScene::update` mode consumption + owned-building count + crew registration; `draw_building` mirrored buildup + explosion suppression; the hovered-building outline)

**Interfaces:**
- Consumes: `SbRect` (`sidebar.hpp:13`), `World::building_at_cell` (`world.hpp:661`), `World::clear_selection` (`:545`), `Input::resolve_ground_cell`/`placement_active`/`sw_targeting_`, `World::toggle_repair`/`begin_sell` (Tasks 4/5), `mirrored_buildup_frame` (Task 2), `BuildingAssets::sb_repair`/`sb_sell`/`has_repair_sell`/`repair_sell` (Task 3), `resolve_combat_unit` (`main.cpp:9608`).
- Produces — `engine/sidebar.hpp`, after `kPowerPitch` (`:42`):

```cpp
// Repair / Sell buttons [RE: construction.md §11a, Allied/sidec01]: repair.shp
// (2 frames) at bar-relative (20, 166) 64x31, gadget id 101; sell.shp (2
// frames) at (84, 166) 64x31, gadget id 102. Frame 1 is the PRESSED state,
// drawn while the mode is armed. These are exactly the coordinates
// main.cpp's draw_sidebar already blits at, so the rects provably sit under
// the drawn art. (Soviet/Yuri offsets 33/85 at y 165 are recorded in §11a but
// not implemented -- the player is Allied.)
constexpr int kRepairBtnX = 20;
constexpr int kSellBtnX = 84;
constexpr int kSideBtnY = 166;
constexpr int kSideBtnW = 64, kSideBtnH = 31;
```

  and on `SidebarLayout`, after `SbRect scroll_dn, scroll_up;` (`:57`):

```cpp
    // Repair / Sell buttons [RE: construction.md §11a; behaviour
    // repair-sell.md §C1 -- both TOGGLE mutually-exclusive cursor modes].
    SbRect repair_btn, sell_btn;
```

  and beside `tab_at` (`:72`):

```cpp
    bool repair_btn_at(int px, int py) const { return repair_btn.contains(px, py); }
    bool sell_btn_at(int px, int py) const { return sell_btn.contains(px, py); }
```

- Produces — `engine/sidebar.cpp`, inserted in `compute` immediately after the `lay.side1 = ...;` line (`:21`):

```cpp
    // Repair / Sell buttons sit ON the side1 band [RE: construction.md §11a --
    // repair at bar-relative 20, sell at 84, both y 166, 64x31].
    lay.repair_btn = {x0 + kRepairBtnX, kSideBtnY, kSideBtnW, kSideBtnH};
    lay.sell_btn = {x0 + kSellBtnX, kSideBtnY, kSideBtnW, kSideBtnH};
```

- Produces — `engine/input.hpp`. Four fields appended to `InputActions` after `sw_cy` (`:44`):

```cpp
    // Repair & Sell [RE: repair-sell.md §C1]: a left-click in Repair/Sell mode
    // that landed on an OWN (house 0) alive building. main.cpp routes these to
    // World::toggle_repair / World::begin_sell. Default inert -- no existing
    // flow arms either mode, so no existing demo/frame reports them.
    bool repair_clicked = false;
    int repair_building = -1;
    bool sell_clicked = false;
    int sell_building = -1;
```

  public, after `int placement_type() const` (`:102`):

```cpp
    // --- Repair & Sell cursor modes [RE: repair-sell.md §C1] --------------
    // The original's DisplayClass carries FIVE mutually-exclusive one-byte mode
    // flags (RepairMode +0x11B0, SellMode +0x11B1, PowerToggleMode +0x11B2,
    // PlanningMode +0x11B3, PlaceBeaconMode +0x11B4) and EVERY setter clears
    // the other four. The last three have no feature behind them in this engine
    // (no per-building power toggle, no waypoint planning, no beacons), so they
    // are recorded but not modeled: the exclusion set here is
    // {pending placement, sw_targeting, Repair, Sell}.
    enum class BaseMode { None, Repair, Sell };

    // SetRepairMode / SetSellMode (FUN_004AC7D0 / FUN_004AC570), transcribed:
    //   1. control -1 toggle / 0 off / 1 on; no-op if the value is unchanged;
    //   2. REFUSED outright while a placement is pending (placement WINS);
    //   3. clears every other mode (here: the sibling mode + sw_targeting);
    //   4. arming requires the player to own at least one building, and calls
    //      Reset_Cursor (FUN_0048DBA0) -- which DESELECTS EVERYTHING.
    // The mode is NOT one-shot: it stays armed until cancelled, so several
    // buildings can be repaired/sold in a row.
    void set_repair_mode(int control);
    void set_sell_mode(int control);
    BaseMode base_mode() const { return mode_; }

    // The ESC / right-click chain (FUN_004AAC40): cancels EXACTLY ONE thing per
    // press, in the recovered priority order
    //   pending placement -> RepairMode -> SellMode -> PowerToggleMode
    //   -> pending superweapon -> PlanningMode -> PlaceBeaconMode -> (nothing).
    // The three unmodeled modes are simply absent from the chain. Returns true
    // iff something was cancelled.
    bool cancel_one_mode();

    // PlayerPtr->[0x2F0] > 0 [RE: §C1, PROBABLE]: arming ANY mode is refused
    // when the player owns no buildings, and the sidebar greys the buttons by
    // the same predicate. main.cpp refreshes this once per frame.
    // [RE-GATE: repair-sell.md §D item 5 -- FALLBACK if the field's identity is
    // not confirmed: "the player owns >= 1 alive, non-Dead building", which is
    // behaviourally identical for the only case that matters (zero -> dead
    // buttons).]
    void set_owned_buildings(int n) { owned_buildings_ = n; }
    int owned_buildings() const { return owned_buildings_; }
```

  private, after `bool sw_targeting_ = false;` (`:235`):

```cpp
    // Repair & Sell cursor modes [RE: repair-sell.md §C1]. Default None +
    // 0 owned -> both modes are unreachable until main.cpp arms one.
    BaseMode mode_ = BaseMode::None;
    int owned_buildings_ = 0;
```

- Produces — `engine/input.cpp`, the setters + cancel chain (place them beside `cancel_placement`, `input.cpp:169`):

```cpp
namespace {
// The shared body of SetRepairMode/SetSellMode [RE: repair-sell.md §C1] --
// both setters are byte-for-byte the same shape in the binary.
Input::BaseMode next_mode(Input::BaseMode cur, Input::BaseMode want, int control) {
    const bool on = (cur == want);
    bool next_on = on;
    if (control < 0) next_on = !on;
    else if (control == 0) next_on = false;
    else next_on = true;
    return next_on ? want : Input::BaseMode::None;
}
}  // namespace

void Input::set_repair_mode(int control) { set_mode_(BaseMode::Repair, control); }
void Input::set_sell_mode(int control) { set_mode_(BaseMode::Sell, control); }

void Input::set_mode_(BaseMode want, int control) {
    const BaseMode next = next_mode(mode_, want, control);
    if (next == mode_) return;                       // no-op if unchanged
    // PLACEMENT WINS: every setter early-returns while PendingObject != 0.
    if (placement_active()) return;
    // Every setter clears the other modes.
    sw_targeting_ = false;
    if (next == BaseMode::None) {
        mode_ = BaseMode::None;
        return;
    }
    // Arming requires at least one owned building; otherwise the button is dead.
    if (owned_buildings_ <= 0) {
        mode_ = BaseMode::None;
        return;
    }
    mode_ = next;
    // Reset_Cursor (FUN_0048DBA0): arming a mode DESELECTS EVERYTHING.
    world_.clear_selection();
    selected_building_ = -1;
    actions_.actor = -1;
}

bool Input::cancel_one_mode() {
    // [RE: repair-sell.md §C1] EXACTLY ONE thing per press, in priority order.
    if (placement_active()) {
        actions_.place_cancelled = true;
        cancel_placement();
        return true;
    }
    if (mode_ == BaseMode::Repair) { mode_ = BaseMode::None; return true; }
    if (mode_ == BaseMode::Sell) { mode_ = BaseMode::None; return true; }
    // (PowerToggleMode would sit here -- not modeled.)
    if (sw_targeting_) { sw_targeting_ = false; return true; }
    // (PlanningMode, PlaceBeaconMode would sit here -- not modeled.)
    return false;
}
```

  (declare `void set_mode_(BaseMode want, int control);` in the private section of `input.hpp` beside `cancel_placement`'s siblings.)

  and the **intercept block** in `Input::handle`, inserted immediately after the `placement_active()` block's closing `}` (`input.cpp:384`) and before `switch (ev.kind) {` (`:386`):

```cpp
    // Repair / Sell cursor mode [RE: repair-sell.md §C1]. While armed, a left
    // Down+Up (no drag) on an OWN alive building reports the click (main.cpp
    // routes it to World::toggle_repair / World::begin_sell); a right-click
    // cancels ONE mode; PointerMove just tracks the cursor so the hovered-
    // building outline follows it. The whole gesture model is swallowed so no
    // selection/attack/move leaks through. Quit still propagates.
    //
    // The mode is NOT one-shot -- unlike sw_targeting_ it stays armed after a
    // click, so several buildings can be repaired/sold in a row.
    //
    // Placed AFTER the placement block because placement WINS (set_mode_ also
    // refuses while placing, so the two can never both be active); the relative
    // order with the sw_targeting_ block above is behaviourally irrelevant for
    // the same reason (arming either mode clears sw_targeting_).
    if (mode_ != BaseMode::None) {
        switch (ev.kind) {
            case InputEvent::Quit:
                quit_ = true;
                break;
            case InputEvent::PointerMove:
                cur_x_ = ev.x;
                cur_y_ = ev.y;
                break;
            case InputEvent::PointerDown:
                if (ev.button == InputEvent::Right) {
                    cancel_one_mode();
                } else {
                    l_down_ = true;
                    dragging_ = false;
                    down_x_ = cur_x_ = ev.x;
                    down_y_ = cur_y_ = ev.y;
                }
                break;
            case InputEvent::PointerUp:
                if (ev.button == InputEvent::Left && l_down_ && !dragging_) {
                    int cx = 0, cy = 0;
                    resolve_ground_cell(ev.x, ev.y, cx, cy);
                    const int bi = world_.building_at_cell(cx, cy);
                    // OWN house only [RE: §C1 -- the SELL event verifies
                    // ownership; you cannot sell someone else's building even
                    // if the cursor lets you click it]. A click on nothing, or
                    // on an enemy building, is swallowed with NO report.
                    if (bi >= 0 && world_.buildings()[bi].alive &&
                        world_.buildings()[bi].state != Building::State::Dead &&
                        world_.buildings()[bi].house == 0) {
                        if (mode_ == BaseMode::Repair) {
                            actions_.repair_clicked = true;
                            actions_.repair_building = bi;
                        } else {
                            actions_.sell_clicked = true;
                            actions_.sell_building = bi;
                        }
                    }
                }
                l_down_ = false;
                dragging_ = false;
                break;
            default:
                break;
        }
        return;
    }
```

- Produces — `engine/main.cpp`:

  **(a)** `draw_sidebar` — replace the block at `main.cpp:2971-2974` with the layout rects + the pressed frame. `draw_sidebar` gains one trailing defaulted parameter `int armed_mode = 0` (`0` none, `1` repair, `2` sell) so every existing call site is unchanged:

```cpp
    // Repair / Sell buttons [RE: construction.md §11a rects; behaviour
    // repair-sell.md §C1]. Frame 1 is the PRESSED art, drawn while that mode is
    // armed -- the 2-frame button art is the feedback channel the engine
    // actually has (§C2's wrench/dollar cursor frames are [RE-FALLBACK] and
    // there is no SHP-cursor subsystem to put them in).
    if (ba.has_repair_sell) {
        blit_shp(ren, tc, "sb:rep", ba.sb_repair, armed_mode == 1 ? 1 : 0, ba.sb_pal,
                 lay.repair_btn.x, lay.repair_btn.y);
        blit_shp(ren, tc, "sb:sell", ba.sb_sell, armed_mode == 2 ? 1 : 0, ba.sb_pal,
                 lay.sell_btn.x, lay.sell_btn.y);
    }
```

  (`GameScene::render`'s `draw_sidebar` call at `main.cpp:3570-3572` passes `input_.base_mode() == Input::BaseMode::Repair ? 1 : (input_.base_mode() == Input::BaseMode::Sell ? 2 : 0)`.)

  **(b)** `GameScene::handle` — inserted immediately before the catch-all `return;` at `main.cpp:3393`:

```cpp
                // Repair / Sell buttons [RE: repair-sell.md §C1]: BOTH TOGGLE
                // (SidebarClass::Action ids 0x8065 / 0x8066 both pass -1).
                if (lay_.repair_btn_at(ev.x, ev.y)) {
                    if (audio_.enabled) audio_.mixer->play("PlaceBuilding");
                    input_.set_repair_mode(-1);
                    return;
                }
                if (lay_.sell_btn_at(ev.x, ev.y)) {
                    if (audio_.enabled) audio_.mixer->play("PlaceBuilding");
                    input_.set_sell_mode(-1);
                    return;
                }
```

  and the Escape branch at `main.cpp:3396-3399` becomes:

```cpp
        // Escape (mapped to Quit) cancels ONE armed thing instead of quitting,
        // in the recovered priority order [RE: repair-sell.md §C1].
        if (ev.kind == ra2engine::InputEvent::Quit && input_.cancel_one_mode()) return;
```

  **(c)** `GameScene` ctor — after the existing member init, register the two crews once:

```cpp
        // Survivor crews [RE: repair-sell.md §B3]: World is asset-free, so the
        // basic infantry per side is resolved HERE and registered. The 25%
        // special crew type is CUT [RE-GATE APPLIED: repair-sell.md §D item 1].
        world_.set_repair_sell_rules(ba_.repair_sell);
        world_.set_survivor_crew(static_cast<int>(ra2engine::World::Side::Allied),
                                 resolve_combat_unit(rules_, "E1").combat);
        world_.set_survivor_crew(static_cast<int>(ra2engine::World::Side::Soviet),
                                 resolve_combat_unit(rules_, "E2").combat);
```

  **(d)** `GameScene::update` — beside the existing per-frame recomputes, and after `const ra2engine::InputActions act = input_.take_actions();`:

```cpp
        // [RE: repair-sell.md §C1] arming a mode requires >= 1 owned building.
        {
            int owned = 0;
            for (const ra2engine::Building& b : world_.buildings())
                if (b.alive && b.house == 0 && b.state != ra2engine::Building::State::Dead) ++owned;
            input_.set_owned_buildings(owned);
            if (owned == 0 && input_.base_mode() != ra2engine::Input::BaseMode::None) {
                input_.set_repair_mode(0);
                input_.set_sell_mode(0);
            }
        }
        // Repair / Sell clicks [RE: repair-sell.md §A4/§B2]. The mode STAYS
        // armed (it is not one-shot), so the player can work through a base.
        if (act.repair_clicked && world_.toggle_repair(act.repair_building, 0)) {
            if (audio_.enabled) audio_.mixer->play("PlaceBuilding");
        }
        if (act.sell_clicked && world_.begin_sell(act.sell_building, 0)) {
            if (audio_.enabled) audio_.mixer->play("PlaceBuilding");
        }
```

  **(e)** `draw_building` (`main.cpp:2002-2005`) — the SHP/frame selection becomes:

```cpp
    // [RE: repair-sell.md §B2] A SELLING building plays the buildup range
    // MIRRORED (Shape_Number's `start + count - stage - 1`) -- the buildup runs
    // BACKWARDS over the same frame count at the same rate.
    const bool buildup = (b.state == ra2engine::Building::State::Buildup && art.has_buildup);
    const bool selling = (b.selling && art.has_buildup);
    const ra2assets::ShpFile& shp = (buildup || selling) ? art.buildup : art.shp;
    const int frame = selling ? ra2engine::mirrored_buildup_frame(b, art.buildup.count())
                     : buildup ? ra2engine::buildup_frame(b, art.buildup.count())
                               : ra2engine::building_frame(b, art.shp.count(), condition_yellow);
```

  and wherever the caller draws the death explosion for a `State::Dead` building, that draw is guarded with `&& !b.sold` (a **sold** building is `Limbo()`d and deleted in the original — it does not explode).

  **(f)** the hovered-building outline — beside the `draw_placement_cursor` call (`main.cpp:3558-3559`), a sibling that reuses the same diamond idiom:

```cpp
// Repair / Sell mode feedback [RE: repair-sell.md §C2 -- the wrench/dollar
// MOUSE.SHP frame indices are [RE-FALLBACK] and this engine has no SHP-cursor
// subsystem, so the armed mode is shown by outlining the hovered OWN building's
// foundation instead: green = repair, yellow = sell].
void draw_mode_cursor(SDL_Renderer* ren, const ra2engine::World& world,
                      const ra2engine::Input& input, const ra2engine::Camera& cam,
                      int mx, int my, int W, int H);
```

  called as `if (!(sidebar_up_ && lay_.contains(mx_, my_))) draw_mode_cursor(ren, world_, input_, cam_, mx_, my_, kWinW, kWinH);` — implemented by resolving the hovered cell the same way `draw_placement_cursor` does, taking `world.building_at_cell`, and stroking `foundation_cells(b.cx, b.cy, b.fw, b.fh)` with the existing 5-point diamond in `(0,220,0,160)` for Repair or `(220,200,0,160)` for Sell; a no-op when `base_mode() == None` or the hovered building is not house 0.

**world_hash safety:** `SidebarLayout` gains two rects at the coordinates `main.cpp` was already blitting at (pinned by a test), so **no drawn pixel moves**. `Input`'s new state defaults `None`/`0` and its intercept block is unreachable until `set_*_mode` is called — which only `GameScene::handle` does, and no gate constructs a `GameScene`. `draw_sidebar`'s new parameter is defaulted, so every existing call site is unchanged. `draw_building`'s new branch requires `b.selling`, which no gate sets. `world_hash` untouched.

**Steps:**

- [ ] **Step 1: Write the failing tests.** Append to `tests/test_sidebar.cpp`:

```cpp
TEST_CASE("sidebar: repair/sell button rects [RE: construction.md §11a]") {
    auto lay = SidebarLayout::compute(1280, 720, {1, 2, 3});
    const int x0 = 1280 - 168;
    // The EXACT coordinates main.cpp's draw_sidebar already blits at
    // (lay.bar.x + 20, 166) and (lay.bar.x + 84, 166), 64x31, on the side1 band.
    CHECK(lay.repair_btn.x == x0 + 20);
    CHECK(lay.repair_btn.y == 166);
    CHECK(lay.repair_btn.w == 64);
    CHECK(lay.repair_btn.h == 31);
    CHECK(lay.sell_btn.x == x0 + 84);
    CHECK(lay.sell_btn.y == 166);
    CHECK(lay.sell_btn.w == 64);
    CHECK(lay.sell_btn.h == 31);
    // Both sit inside the side1 band (158..227) and never overlap each other,
    // the tab row (y 197) or the cameo grid.
    CHECK(lay.repair_btn.y >= lay.side1.y);
    CHECK(lay.repair_btn.y + lay.repair_btn.h <= lay.side1.y + lay.side1.h);
    CHECK(lay.repair_btn.x + lay.repair_btn.w <= lay.sell_btn.x);
    CHECK(lay.repair_btn_at(x0 + 21, 167));
    CHECK_FALSE(lay.repair_btn_at(x0 + 21, 165));
    CHECK(lay.sell_btn_at(x0 + 85, 167));
    CHECK_FALSE(lay.sell_btn_at(x0 + 21, 167));       // repair rect, not sell
    CHECK_FALSE(lay.sell_btn_at(0, 0));
    // 800x600 (the binary's native size) puts them at the same absolute y.
    auto s = SidebarLayout::compute(800, 600, {1});
    CHECK(s.repair_btn.x == 800 - 168 + 20);
    CHECK(s.repair_btn.y == 166);
}
```

  and to `tests/test_input.cpp` (reuse the file's existing `World`+`Camera`+`Input` fixture):

```cpp
TEST_CASE("input: repair/sell modes are mutually exclusive and TOGGLE [RE: repair-sell.md §C1]") {
    /* fixture: world with >= 1 house-0 building; in.set_owned_buildings(1) */
    CHECK(in.base_mode() == Input::BaseMode::None);
    in.set_repair_mode(-1);
    CHECK(in.base_mode() == Input::BaseMode::Repair);
    in.set_repair_mode(-1);                       // both buttons TOGGLE
    CHECK(in.base_mode() == Input::BaseMode::None);
    in.set_repair_mode(1);
    in.set_sell_mode(1);                          // arming Sell clears Repair
    CHECK(in.base_mode() == Input::BaseMode::Sell);
    in.set_sell_mode(0);
    CHECK(in.base_mode() == Input::BaseMode::None);
    SUBCASE("arming clears sw_targeting") {
        in.arm_superweapon();
        in.set_repair_mode(1);
        CHECK(in.base_mode() == Input::BaseMode::Repair);
        CHECK_FALSE(in.sw_targeting());
    }
    SUBCASE("PLACEMENT WINS -- arming is refused while placing") {
        in.begin_placement(0, 2, 2);
        in.set_repair_mode(1);
        CHECK(in.base_mode() == Input::BaseMode::None);
        CHECK(in.placement_active());
    }
    SUBCASE("refused with no owned buildings") {
        in.set_owned_buildings(0);
        in.set_sell_mode(1);
        CHECK(in.base_mode() == Input::BaseMode::None);
    }
    SUBCASE("arming DESELECTS everything (Reset_Cursor)") {
        world.units()[0].selected = true;
        in.set_repair_mode(1);
        CHECK_FALSE(world.units()[0].selected);
    }
}

TEST_CASE("input: cancel_one_mode follows the recovered priority chain [RE: §C1]") {
    in.set_owned_buildings(1);
    in.begin_placement(0, 2, 2);
    in.set_repair_mode(1);                        // refused (placement wins)
    CHECK(in.cancel_one_mode());                  // 1st press: the placement
    CHECK_FALSE(in.placement_active());
    in.set_repair_mode(1);
    in.arm_superweapon();                         // (arm order: SW last)
    CHECK(in.cancel_one_mode());                  // 2nd: Repair before SW
    CHECK(in.base_mode() == Input::BaseMode::None);
    CHECK(in.sw_targeting());
    CHECK(in.cancel_one_mode());                  // 3rd: the superweapon
    CHECK_FALSE(in.sw_targeting());
    CHECK_FALSE(in.cancel_one_mode());            // nothing left
}

TEST_CASE("input: a mode click reports OWN buildings only, and stays armed [RE: §C1]") {
    in.set_owned_buildings(1);
    in.set_sell_mode(1);
    /* click over an own house-0 building at (bx, by) */
    const InputActions a = in.take_actions();
    CHECK(a.sell_clicked);
    CHECK(a.sell_building == b_own);
    CHECK_FALSE(a.repair_clicked);
    CHECK(in.base_mode() == Input::BaseMode::Sell);          // NOT one-shot
    SUBCASE("an enemy building reports NOTHING") { /* click over a house-1 building */ }
    SUBCASE("empty ground reports NOTHING") { /* click far from any building */ }
    SUBCASE("a right-click cancels the mode") {
        /* feed a right PointerDown */
        CHECK(in.base_mode() == Input::BaseMode::None);
    }
    SUBCASE("no selection/move leaks through while armed") {
        /* click over an own UNIT while in Sell mode -> nothing selected */
    }
}
```

- [ ] **Step 2: Run and see them FAIL.**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
D:\ra2-android\build\ra2assets_tests.exe --test-case="sidebar: repair*"
```

  RED.

- [ ] **Step 3: Implement** the sidebar constants/rects/predicates, the `Input` mode API + intercept block, and the six `main.cpp` edits (a)–(f), exactly as given.

- [ ] **Step 4: Build and run.**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests ra2engine
D:\ra2-android\build\ra2assets_tests.exe --test-case="sidebar:*"
D:\ra2-android\build\ra2assets_tests.exe --test-case="input:*"
```

  All green, no regressions in the existing `input:*` cases (`test_input.cpp` is the largest gesture-model suite in the tree — a regression there means the intercept block was inserted in the wrong place).

- [ ] **Step 5: Two-gate canary, ONE PER FOREGROUND CALL** (`draw_sidebar`/`draw_building`/`Input` are on interactive paths only, but `SidebarLayout::compute` and `draw_building` are compiled into every gate binary):

```powershell
D:\ra2-android\build\ra2engine.exe --demo-base       # expect hash=5273b5e71ca009e1
```
```powershell
D:\ra2-android\build\ra2engine.exe --demo-prod       # expect prod_hash=f0f4e1207ecd713c
```

- [ ] **Step 6: Interactive visual check (controller).** Launch `D:\ra2-android\build\ra2engine.exe`, start a skirmish, and confirm: clicking **Repair** shows the pressed button frame and outlines the hovered own building green; clicking a damaged building starts the wrench (HP bar climbs, credits tick down every ~14 frames); clicking it again stops it; ESC cancels the mode; clicking **Sell** shows the yellow outline, and selling a building plays its buildup **backwards** for ~3.6 s before the credits land and infantry walk out. Report what was seen.

- [ ] **Step 7: Commit.**

```powershell
git -C "D:\ra2-android" add engine/sidebar.hpp engine/sidebar.cpp engine/input.hpp engine/input.cpp engine/main.cpp tests/test_sidebar.cpp tests/test_input.cpp
git -C "D:\ra2-android" commit -m "$(printf 'feat(ui): the Repair and Sell buttons work - mutually-exclusive toggle cursor modes (placement wins, arming deselects, refused with no buildings), the recovered cancel chain, pressed button frames, mirrored sell buildup, no explosion on a sold building [RE: repair-sell.md C1/C2 + construction.md 11a]\n\nCo-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>')"
```

---

### Task 7: `--demo-repair` — the 20th byte-identity gate

**Files:**
- Modify: `engine/main.cpp` (add `repair_fold`, `struct DemoRepairResult`, `simulate_repair`, `run_demo_repair` immediately after `run_demo_maps` at `main.cpp:10552`; add the `--demo-repair` dispatch arm immediately after the `--demo-maps` arm at `main.cpp:10797-10798`)

**Interfaces:**
- Consumes: `build_world(a, ua, 0)` (`main.cpp:1459`), `wire_economy(world, ba, rules)` (`:1410`), `spawn_base(world, ba, house, cx, cy, with_refinery, harv_va, harv_chrono, harv_storage, allied_full)` (`:1879`), `roster_index(ba, id)`, `resolve_combat_unit(rules, id)` (`:9608`), `pick_start_cell(a.map)` (`world.hpp:419`), `World::set_repair_sell_rules`/`set_survivor_crew`/`toggle_repair`/`begin_sell`/`building_repairing`/`building_selling`/`repair_frame`/`grant_credits_for_test`/`add_building`, `repair_step_cost`/`sell_refund`/`survivor_count`/`survivor_divisor` (Task 2), `fnv_mix` (`:5657`), `capture_png`, `centered_camera` (`:2266`), `render_world`, `env_install_dir`, `load_ini_from_ra2md`, and the `run_demo_maps` harness shape (`:10480-10552`).
- **Gate exemplars:** `GACNST` (Cost 3000 / Strength 1000 → `stepCost` 3, refund 1500, Allied survivors 3, `Crewed=yes`) and `GAPOWR` (800 / 1000 → `stepCost` 1, refund 400, `Crewed=no` → 0 survivors) — **`[RE-GATE: repair-sell.md §D item 9]`; if the adjudication measured different values, substitute the measured ones here and in the wedge guards.**
- Produces — `engine/main.cpp`:

```cpp
// --- --demo-repair helpers ---------------------------------------------------

// FNV fold of the Repair & Sell state [RE: repair-sell.md]. Folds EXACTLY the
// new state plus what it moves -- per-building hp/flags/state, per-house
// credits, the unit roll (survivors) and the global cadence counter.
// world_hash is UNTOUCHED (it folds only Unit{cell,hp,alive} +
// Building{type,cx,cy,hp,state}); this gate owns its own fold, like every
// previous phase.
uint64_t repair_fold(uint64_t h, const ra2engine::World& world) {
    for (int i = 0; i < (int)world.buildings().size(); ++i) {
        const ra2engine::Building& b = world.buildings()[i];
        h = fnv_mix(h, (uint64_t)(int64_t)b.hp);
        h = fnv_mix(h, (uint64_t)(b.being_repaired ? 1 : 0));
        h = fnv_mix(h, (uint64_t)(b.selling ? 1 : 0));
        // sell_time is a double; quantize to ms so the fold is exact.
        h = fnv_mix(h, (uint64_t)(int64_t)std::llround(b.sell_time * 1000.0));
        h = fnv_mix(h, (uint64_t)(b.sold ? 1 : 0));
        h = fnv_mix(h, (uint64_t)(b.alive ? 1 : 0));
        h = fnv_mix(h, (uint64_t)(int)b.state);
    }
    for (int hh = 0; hh < world.house_count(); ++hh)
        h = fnv_mix(h, (uint64_t)(int64_t)std::llround(world.house_credits(hh)));
    h = fnv_mix(h, (uint64_t)world.units().size());
    for (const ra2engine::Unit& u : world.units()) {
        h = fnv_mix(h, (uint64_t)(int64_t)u.cell_x);
        h = fnv_mix(h, (uint64_t)(int64_t)u.cell_y);
        h = fnv_mix(h, (uint64_t)(u.alive ? 1 : 0));
        for (char c : u.image) h = fnv_mix(h, (uint64_t)(unsigned char)c);
    }
    h = fnv_mix(h, world.repair_frame());
    return h;
}

struct DemoRepairResult { std::vector<std::string> witness; uint64_t hash = 0; bool ok = true; };

// One scripted run of the Repair & Sell demo (headless or rendered --
// bit-identical; NO RNG is reachable in any path this drives: the 25%-chance
// special crew type is CUT, so survivors are always the house's basic
// infantry). Drives the WHOLE feature: the 14-frame global cadence + the
// integer stepCost charge, the auto-stop, the silent out-of-credits stop, every
// recovered refusal (AND the two that DO NOT exist), and the sell mission with
// its mirrored buildup, FLAT refund and survivor spawn. PURELY ADDITIVE: no
// other gate calls it; world_hash untouched; NO pinned hash literal (Task 8
// pins the observed value in the README).
DemoRepairResult simulate_repair(Assets& a, UnitAssets& ua, BuildingAssets& ba, const Rules& rules,
                                 const std::vector<const VehicleAssets*>& vlist,
                                 bool render, SDL_Renderer* ren, ra2engine::TextureCache* tc,
                                 const ra2engine::Camera* cam, int W, int H,
                                 const std::string& start_png, const std::string& end_png) {
    DemoRepairResult r;
    uint64_t h = 0xcbf29ce484222325ull;             // FNV-1a offset basis

    // --- Fixed world: an Allied base (house 0, human) far from a Soviet base
    // (house 1, AI-flagged for the refund-asymmetry note) so no combat can
    // perturb the scripted HP numbers.
    ra2engine::World world = build_world(a, ua, 0);       // count=0 -> no pre-spawned units
    wire_economy(world, ba, rules);                      // populates types_ (repair needs Cost/Strength)
    world.set_repair_sell_rules(ba.repair_sell);         // the real rulesmd scalars
    world.set_house(0, {ra2engine::World::Side::Allied, 5, "Americans", 0, true});
    world.set_house(1, {ra2engine::World::Side::Soviet, 1, "Russians", 1, true});
    // Survivor crews [RE: §B3] -- World is asset-free, the caller registers them.
    world.set_survivor_crew(static_cast<int>(ra2engine::World::Side::Allied),
                            resolve_combat_unit(rules, "E1").combat);
    world.set_survivor_crew(static_cast<int>(ra2engine::World::Side::Soviet),
                            resolve_combat_unit(rules, "E2").combat);

    const int gw = world.grid_w(), gh = world.grid_h();
    auto clampi = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };
    const ra2assets::MapCell* pc = ra2engine::pick_start_cell(a.map);
    const int bx = clampi(pc ? pc->x : 20, 8, gw - 12);
    const int by = clampi(pc ? pc->y : 20, 8, gh - 12);
    spawn_base(world, ba, /*house=*/0, bx, by, /*with_refinery=*/false, /*harv_va=*/nullptr,
               /*harv_chrono=*/false, /*harv_storage=*/0, /*allied_full=*/true);
    spawn_base(world, ba, /*house=*/1, clampi(bx + 40, 8, gw - 12), clampi(by + 30, 8, gh - 12));
    world.grant_credits_for_test(0, 10000.0);
    world.grant_credits_for_test(1, 10000.0);

    // Locate the exemplars by ROSTER ID (never by index or position).
    auto find_own = [&](const char* id) -> int {
        const int t = roster_index(ba, id);
        for (int i = 0; i < (int)world.buildings().size(); ++i) {
            const ra2engine::Building& b = world.buildings()[i];
            if (b.alive && b.house == 0 && b.type == t) return i;
        }
        return -1;
    };
    auto find_enemy_any = [&]() -> int {
        for (int i = 0; i < (int)world.buildings().size(); ++i)
            if (world.buildings()[i].alive && world.buildings()[i].house == 1) return i;
        return -1;
    };
    const int cnst = find_own("GACNST");
    const int powr = find_own("GAPOWR");
    const int foe = find_enemy_any();
    const int t_cnst = cnst >= 0 ? world.buildings()[cnst].type : -1;
    const int t_powr = powr >= 0 ? world.buildings()[powr].type : -1;
    const bool have_exemplars = (cnst >= 0 && powr >= 0 && foe >= 0);

    // The EXPECTED numbers, computed from the SAME pure functions the sim uses
    // (so a wrong constant fails the wedge, not the hash).
    const int cnst_cost = have_exemplars
        ? ra2engine::repair_step_cost(ba.roster[t_cnst].cost, ba.roster[t_cnst].strength,
                                      ba.repair_sell.repair_step, ba.repair_sell.repair_percent)
        : 0;
    const int cnst_refund = have_exemplars
        ? ra2engine::sell_refund(ba.roster[t_cnst].cost, ba.roster[t_cnst].soylent,
                                 ba.repair_sell.refund_percent, /*owner_is_human=*/true)
        : 0;
    const int cnst_surv = have_exemplars
        ? ra2engine::survivor_count(
              cnst_refund,
              ra2engine::survivor_divisor(ba.repair_sell,
                                          static_cast<int>(ra2engine::World::Side::Allied)),
              ba.roster[t_cnst].crewed, /*has_been_captured=*/false)
        : 0;
    const int expect_interval = ra2engine::repair_interval_frames(ba.repair_sell.repair_rate);

    auto render_frame = [&]() {
        if (!render) return;
        SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
        SDL_RenderClear(ren);
        render_world(ren, *tc, a, ua, world, *cam, W, H, nullptr, &vlist);
    };

    render_frame();
    if (render) capture_png(ren, start_png, W, H);
    h = repair_fold(h, world);
    {
        char b[256];
        std::snprintf(b, sizeof b,
                      "[rs] stage0 setup: cnst=%s powr=%s foe=%s interval=%d step_hp=%d "
                      "step_cost=%d refund=%d survivors=%d",
                      cnst >= 0 ? "yes" : "no", powr >= 0 ? "yes" : "no", foe >= 0 ? "yes" : "no",
                      expect_interval, ba.repair_sell.repair_step, cnst_cost, cnst_refund,
                      cnst_surv);
        r.witness.emplace_back(b);
    }

    // --- Stage 1: the 14-frame GLOBAL cadence + the integer stepCost charge.
    // Damage the ConYard by an exact multiple of the step so the arithmetic is
    // unambiguous, arm the wrench, and step ONE frame at a time recording the
    // frames on which hp actually moved.
    std::vector<uint64_t> hit_frames;
    int step_gaps_ok = 1, heal_ok = 1, charge_ok = 1;
    if (cnst >= 0) {
        const int max_hp = world.buildings()[cnst].max_hp;
        world.buildings()[cnst].hp = max_hp - 8 * 6;      // 6 steps of damage
        if (!world.toggle_repair(cnst, 0)) r.ok = false;
        int prev_hp = world.buildings()[cnst].hp;
        double prev_cr = world.house_credits(0);
        for (int k = 0; k < 90; ++k) {
            const uint64_t f = world.repair_frame();
            world.step(1.0 / 15.0);
            const int hp = world.buildings()[cnst].hp;
            const double cr = world.house_credits(0);
            if (hp != prev_hp) {
                hit_frames.push_back(f);
                if (hp != prev_hp + ba.repair_sell.repair_step && hp != max_hp) heal_ok = 0;
                if (std::llround(prev_cr - cr) != cnst_cost) charge_ok = 0;
            }
            prev_hp = hp;
            prev_cr = cr;
        }
        for (size_t i = 1; i < hit_frames.size(); ++i)
            if (hit_frames[i] - hit_frames[i - 1] != (uint64_t)expect_interval) step_gaps_ok = 0;
        for (uint64_t f : hit_frames)
            if (f % (uint64_t)expect_interval != 0) step_gaps_ok = 0;    // an ABSOLUTE phase
    }
    h = repair_fold(h, world);
    {
        char b[256];
        std::snprintf(b, sizeof b,
                      "[rs] stage1 repair: steps=%d first=%llu gap=%d heal8=%s charge=%s "
                      "hp=%d/%d credits=%.0f",
                      (int)hit_frames.size(),
                      (unsigned long long)(hit_frames.empty() ? 0 : hit_frames[0]),
                      expect_interval, heal_ok ? "yes" : "no", charge_ok ? "yes" : "no",
                      cnst >= 0 ? world.buildings()[cnst].hp : -1,
                      cnst >= 0 ? world.buildings()[cnst].max_hp : -1, world.house_credits(0));
        r.witness.emplace_back(b);
    }

    // --- Stage 2: AUTO-STOP at full health.
    const bool autostop_ok =
        cnst >= 0 && world.buildings()[cnst].hp == world.buildings()[cnst].max_hp &&
        !world.building_repairing(cnst);
    {
        char b[192];
        std::snprintf(b, sizeof b, "[rs] stage2 autostop: full=%s flag_cleared=%s",
                      (cnst >= 0 && world.buildings()[cnst].hp == world.buildings()[cnst].max_hp)
                          ? "yes" : "no",
                      (cnst >= 0 && !world.building_repairing(cnst)) ? "yes" : "no");
        r.witness.emplace_back(b);
    }
    h = repair_fold(h, world);

    // --- Stage 3: out of credits STOPS repair silently, with NO partial step.
    int broke_hp0 = -1, broke_hp1 = -1;
    bool broke_ok = false;
    if (powr >= 0) {
        world.buildings()[powr].hp = world.buildings()[powr].max_hp - 8 * 4;
        const int powr_cost =
            ra2engine::repair_step_cost(ba.roster[t_powr].cost, ba.roster[t_powr].strength,
                                        ba.repair_sell.repair_step, ba.repair_sell.repair_percent);
        world.grant_credits_for_test(0, (double)powr_cost - 1.0);   // one credit short
        if (!world.toggle_repair(powr, 0)) r.ok = false;
        broke_hp0 = world.buildings()[powr].hp;
        for (int k = 0; k < 40; ++k) world.step(1.0 / 15.0);
        broke_hp1 = world.buildings()[powr].hp;
        broke_ok = (broke_hp1 == broke_hp0) && !world.building_repairing(powr) &&
                   std::llround(world.house_credits(0)) == (long long)powr_cost - 1;
    }
    h = repair_fold(h, world);
    {
        char b[224];
        std::snprintf(b, sizeof b,
                      "[rs] stage3 broke: hp=%d->%d unchanged=%s flag_cleared=%s credits=%.0f",
                      broke_hp0, broke_hp1, (broke_hp0 == broke_hp1) ? "yes" : "no",
                      (powr >= 0 && !world.building_repairing(powr)) ? "yes" : "no",
                      world.house_credits(0));
        r.witness.emplace_back(b);
    }

    // --- Stage 4: the refusals -- exactly the recovered set, and NOT the two
    // that do not exist. A fresh Buildup-state building is placed and tested
    // BEFORE any further stepping so it is still under construction.
    world.grant_credits_for_test(0, 10000.0);
    bool refuse_enemy = false, refuse_buildup = false, refuse_double = false, allow_conyard = false;
    if (foe >= 0) refuse_enemy = !world.begin_sell(foe, 0);          // NOT your building
    {
        const int t = roster_index(ba, "GAPOWR");
        int ux = -1, uy = -1;
        for (int dx = 2; dx <= 10 && ux < 0; ++dx)
            for (int dy = 2; dy <= 10 && ux < 0; ++dy) {
                const ra2engine::BuildingTypeDef& d = ba.roster[t];
                const ra2engine::PlacementCheck chk = ra2engine::placement_check(
                    bx + dx, by + dy, d.fw, d.fh, 0, d.adjacent, world.passable(), world.grid_w(),
                    world.grid_h(), world.occupancy(), world.buildings(), /*bypass=*/false);
                if (chk.ok) { ux = bx + dx; uy = by + dy; }
            }
        if (ux >= 0) {
            const ra2engine::BuildingTypeDef& d = ba.roster[t];
            const int ui = world.add_building(t, ux, uy, d.fw, d.fh, 0, d.strength, d.armor_ix,
                                              ba.art[t].buildup_total, /*instant=*/false, d.sight,
                                              d.grants_map_reveal);
            refuse_buildup = !world.begin_sell(ui, 0);               // under construction
        }
    }
    if (cnst >= 0) {
        // CONFIRMED BY ABSENCE (§B5): the ONLY, FULL-HEALTH Construction Yard
        // IS sellable. There is no last-ConYard rule.
        allow_conyard = world.begin_sell(cnst, 0);
        refuse_double = !world.begin_sell(cnst, 0);                  // already selling
    }
    h = repair_fold(h, world);
    {
        char b[256];
        std::snprintf(b, sizeof b,
                      "[rs] stage4 refusals: enemy=%s under_construction=%s already_selling=%s "
                      "full_health_conyard_sellable=%s",
                      refuse_enemy ? "refused" : "ALLOWED",
                      refuse_buildup ? "refused" : "ALLOWED",
                      refuse_double ? "refused" : "ALLOWED",
                      allow_conyard ? "yes" : "NO");
        r.witness.emplace_back(b);
    }

    // --- Stage 5: the sell MISSION -- credits land ONLY on completion, FLAT.
    const double cr_before = world.house_credits(0);
    const int units_before = (int)world.units().size();
    int sell_frames = 0;
    bool paid_early = false;
    while (cnst >= 0 && world.building_selling(cnst) && sell_frames < 300) {
        world.step(1.0 / 15.0);
        ++sell_frames;
        if (world.building_selling(cnst) &&
            std::llround(world.house_credits(0)) != std::llround(cr_before))
            paid_early = true;
    }
    const long long paid = cnst >= 0
        ? std::llround(world.house_credits(0)) - std::llround(cr_before) : 0;
    const int spawned = (int)world.units().size() - units_before;
    const bool removed = cnst >= 0 && world.buildings()[cnst].sold &&
                         world.buildings()[cnst].state == ra2engine::Building::State::Dead &&
                         !world.buildings()[cnst].selling;
    const bool footprint_free =
        cnst >= 0 && world.walkable(world.buildings()[cnst].cx, world.buildings()[cnst].cy);
    h = repair_fold(h, world);
    {
        char b[256];
        std::snprintf(b, sizeof b,
                      "[rs] stage5 sell: frames=%d paid_early=%s paid=%lld expect=%d "
                      "survivors=%d expect=%d removed=%s footprint_free=%s",
                      sell_frames, paid_early ? "YES" : "no", paid, cnst_refund, spawned,
                      cnst_surv, removed ? "yes" : "no", footprint_free ? "yes" : "no");
        r.witness.emplace_back(b);
    }
    render_frame();
    if (render) capture_png(ren, end_png, W, H);

    r.hash = h;
    // Wedge guards -> exit 1 unless each invariant holds (the gate can never
    // pass vacuously). Each cites the § it defends.
    if (!have_exemplars)        { r.ok = false; r.witness.emplace_back("[rs] WEDGE: GACNST/GAPOWR/enemy exemplars not spawned"); }
    if (expect_interval != 14)  { r.ok = false; r.witness.emplace_back("[rs] WEDGE: repair interval is not 14 frames (trunc(RepairRate*900), §A2)"); }
    if (ba.repair_sell.repair_step != 8) { r.ok = false; r.witness.emplace_back("[rs] WEDGE: RepairStep is not 8 (§A1)"); }
    if (cnst_cost != 3)         { r.ok = false; r.witness.emplace_back("[rs] WEDGE: GACNST stepCost is not 3 -- the two-integer-division form was lost (§A2)"); }
    if (hit_frames.size() < 5)  { r.ok = false; r.witness.emplace_back("[rs] WEDGE: the wrench never stepped"); }
    if (!step_gaps_ok)          { r.ok = false; r.witness.emplace_back("[rs] WEDGE: repair steps are not on the absolute 14-frame phase (§A2)"); }
    if (!heal_ok)               { r.ok = false; r.witness.emplace_back("[rs] WEDGE: a repair step did not heal exactly RepairStep HP (§A2)"); }
    if (!charge_ok)             { r.ok = false; r.witness.emplace_back("[rs] WEDGE: a repair step did not charge exactly stepCost (§A2)"); }
    if (!autostop_ok)           { r.ok = false; r.witness.emplace_back("[rs] WEDGE: repair did not auto-stop at full health (§A2)"); }
    if (!broke_ok)              { r.ok = false; r.witness.emplace_back("[rs] WEDGE: out-of-credits did not stop silently with hp untouched (§A3)"); }
    if (!refuse_enemy)          { r.ok = false; r.witness.emplace_back("[rs] WEDGE: an enemy building was sellable (§C1 ownership)"); }
    if (!refuse_buildup)        { r.ok = false; r.witness.emplace_back("[rs] WEDGE: a building under construction was sellable (§B5)"); }
    if (!refuse_double)         { r.ok = false; r.witness.emplace_back("[rs] WEDGE: an already-selling building was sellable again (§B5)"); }
    if (!allow_conyard)         { r.ok = false; r.witness.emplace_back("[rs] WEDGE: the only full-health Construction Yard was REFUSED -- an invented last-ConYard rule (§B5, CONFIRMED by absence)"); }
    if (sell_frames < 2)        { r.ok = false; r.witness.emplace_back("[rs] WEDGE: sell completed instantly -- it is a MISSION (§B2)"); }
    if (paid_early)             { r.ok = false; r.witness.emplace_back("[rs] WEDGE: credits landed before the mission completed (§B2)"); }
    if (paid != (long long)cnst_refund) { r.ok = false; r.witness.emplace_back("[rs] WEDGE: the refund was not the FLAT recovered value (§B1 -- no HP scaling)"); }
    if (spawned != cnst_surv)   { r.ok = false; r.witness.emplace_back("[rs] WEDGE: the survivor count does not match clamp(refund/divisor,1,5) (§B3)"); }
    if (!removed)               { r.ok = false; r.witness.emplace_back("[rs] WEDGE: the sold building was not removed/marked sold (§B2)"); }
    if (!footprint_free)        { r.ok = false; r.witness.emplace_back("[rs] WEDGE: the sold building's foundation is still blocked"); }
    char hb[64];
    std::snprintf(hb, sizeof hb, "[rs] repair_hash=%016llx", (unsigned long long)r.hash);
    r.witness.emplace_back(hb);
    return r;
}

// --demo-repair: the Repair & Sell integration gate (20th permanent
// byte-identity gate). Runs simulate_repair rendered (hidden window + start/end
// screenshots) then headless from a FRESH World, and byte-compares the two
// witness logs + hashes. NO hardcoded hash literal -- compute-and-self-compare;
// Task 8 pins the observed value in the README gate table.
int run_demo_repair() {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "d44bece6-5e7c-465a-9fd5-820d808b4426\\scratchpad\\";
    const std::string start_png = scratch + "demo_repair_start.png";
    const std::string end_png = scratch + "demo_repair_end.png";

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    Assets a = load_assets(dir, ra2);
    UnitAssets ua = load_unit_assets(dir, ra2);
    ra2assets::MixFile language =
        ra2assets::MixFile::open(ra2assets::read_file(dir + "language.mix"));
    Ini rules_ini = load_ini_from_ra2md(dir, "rulesmd.ini");
    Rules rules = Rules::load(rules_ini);
    Ini art_ini = load_ini_from_ra2md(dir, "artmd.ini");
    Art art = Art::load(art_ini);
    BuildingAssets ba = load_building_assets(ra2, language, rules, art);
    if (!ba.ok) {
        std::cerr << "[demo-repair] FAILED: building assets unavailable\n";
        return 1;
    }
    std::vector<const VehicleAssets*> vlist;        // no voxel units in this gate

    DemoRepairResult r1;
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
        SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-repair)", SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
        if (!win) throw std::runtime_error(SDL_GetError());
        SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
        if (!ren) throw std::runtime_error(SDL_GetError());
        {
            ra2engine::TextureCache tc(ren);
            ra2engine::Camera cam = centered_camera(a.map, W, H);
            r1 = simulate_repair(a, ua, ba, rules, vlist, /*render=*/true, ren, &tc, &cam, W, H,
                                 start_png, end_png);
        }
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }
    const DemoRepairResult r2 = simulate_repair(a, ua, ba, rules, vlist, /*render=*/false, nullptr,
                                                nullptr, nullptr, W, H, "", "");

    const bool identical = (r1.witness == r2.witness) && (r1.hash == r2.hash);
    std::cerr << "[demo-repair] run1 lines=" << r1.witness.size()
              << " run2 lines=" << r2.witness.size() << "\n";
    for (const std::string& s : r2.witness) std::cout << s << "\n";
    char mh[64];
    std::snprintf(mh, sizeof mh, "%016llx", (unsigned long long)r2.hash);
    std::cout << "[demo-repair] deterministic=" << (identical ? "yes" : "no")
              << " repair_hash=" << mh << "\n";
    if (!identical || !r1.ok || !r2.ok) return 1;
    return 0;
}
```

  Dispatch arm, inserted immediately after the `--demo-maps` arm (`main.cpp:10797-10798`):

```cpp
            } else if (arg == "--demo-repair") {
                return run_demo_repair();
```

  (add `#include "repair_sell.hpp"` beside the existing `#include "building.hpp"` in `engine/main.cpp` if it is not already transitively visible via `world.hpp`.)

**world_hash safety:** a brand-new gate function plus a new dispatch arm. It is the only caller of `simulate_repair`/`repair_fold`; no existing gate's code path is touched. `world_hash` untouched.

**Steps:**

- [ ] **Step 1: Implement** `repair_fold`, `DemoRepairResult`, `simulate_repair`, `run_demo_repair` (insert after `run_demo_maps`, `main.cpp:10552`) and the dispatch arm, exactly as given.

- [ ] **Step 2: Build.**

```powershell
cmake --build D:\ra2-android\build --target ra2engine
```

- [ ] **Step 3: Run the new gate (FOREGROUND, blocking, timeout 600000 ms).**

```powershell
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2engine.exe --demo-repair
```

  Expect `deterministic=yes repair_hash=<observed>`, **zero** `WEDGE` lines, exit code 0, and six witness stages showing `interval=14 step_hp=8 step_cost=3 refund=1500 survivors=3`; `gap=14 heal8=yes charge=yes`; `full=yes flag_cleared=yes`; `unchanged=yes flag_cleared=yes`; `enemy=refused under_construction=refused already_selling=refused full_health_conyard_sellable=yes`; `paid_early=no paid=1500 expect=1500 survivors=3 expect=3 removed=yes footprint_free=yes`. **Record the observed `repair_hash` in the task report** — Task 8 pins it in the README (never in code).

- [ ] **Step 4: Eyeball the screenshots.** `demo_repair_start.png` / `demo_repair_end.png` in the scratchpad: the start shot must show an intact Allied base; the end shot must show the Construction Yard **gone**, with a small clutch of infantry standing where it was. If the ConYard is still drawn, the sell never completed — STOP and report.

- [ ] **Step 5: Re-verify the two most sensitive neighbours, ONE PER FOREGROUND CALL** (the new arm sits in the same dispatch ladder, and the gate is the first caller of the whole feature):

```powershell
D:\ra2-android\build\ra2engine.exe --demo-maps       # expect maps_hash=6e516005f120a1bb
```
```powershell
D:\ra2-android\build\ra2engine.exe --demo-capture    # expect capture_hash=58b87b5bc63f7046
```

- [ ] **Step 6: Commit.**

```powershell
git -C "D:\ra2-android" add engine/main.cpp
git -C "D:\ra2-android" commit -m "$(printf 'feat(gate): --demo-repair, the 20th byte-identity gate (14-frame cadence + integer stepCost + auto-stop + broke-stop + the recovered refusals + the sell mission with its FLAT refund and survivors) [RE: repair-sell.md 2/A3/A4/B1/B2/B3/B5]\n\nCo-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>')"
```

---

### Task 8: Finalize

**Files:** `README.md`, `.superpowers/sdd/progress.md`

**Steps:**

- [ ] **Step 1: Full build + full suite.**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests ra2engine
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2assets_tests.exe
```

  Expect every case green including the new `repair:*`, `sell:*`, `rules: section_flag*`, `sidebar: repair*`, `input:*` mode and `world: *repair*` / `world: *sell*` cases, with no regressions. Record the total count (it was **510/510** at the end of the maps phase).

- [ ] **Step 2: Re-verify ALL 19 existing gates EXACT — ONE PER FOREGROUND CALL.** Run the 19-gate block from **Global Constraints**, one `ra2engine.exe --demo-X` per tool call, timeout 600000 ms (~2 min each; **NEVER batch — a multi-gate call auto-backgrounds and reads as a false hang**). Every printed pin must match the expected list exactly.

- [ ] **Step 3: Re-run the 20th gate and pin its hash.**

```powershell
D:\ra2-android\build\ra2engine.exe --demo-repair
```

  Confirm `deterministic=yes`, no WEDGE lines, exit 0, and that `repair_hash` equals what Task 7 observed. This value goes in the README only.

- [ ] **Step 4: arm64 build.**

```powershell
$env:JAVA_HOME = "C:\Program Files\Java\jdk-21"
cd D:\ra2-android\android
.\gradlew.bat assembleDebug
```

  (`JAVA_HOME` **must** be jdk-21 — the machine default jdk-11 is rejected by the Android Gradle plugin.) Expect `BUILD SUCCESSFUL`. Then confirm the new symbols linked into `libmain.so` with `nm`/`llvm-nm` from the NDK: `repair_step_cost`, `sell_refund`, `survivor_count`, `mirrored_buildup_frame`, `step_repair_sell`.

- [ ] **Step 5: README.** Add a **"Building repair & sell"** section covering, with the recovered numbers:
  - **Repair** is a per-building toggle on a **global 14-frame phase** — `interval = trunc(RepairRate × 900)` and `.016 × 900 = 14.4` truncates to **14, not 15**; every repairing building of every house steps on the same frames, and a newly-armed one waits 0–13 frames for its first step.
  - Each step heals a flat **+8 HP** (`RepairStep`) and charges `max(1, trunc((Cost / (Strength / RepairStep)) × RepairPercent))` with **BOTH divisions integer** — GAPOWR 1/step (125 to full), GACNST 3/step (375 to full), GAWEAP 2/step (250 to full). Note explicitly that the naive `Cost × RepairPercent × damage_fraction` form gives 450 for a ConYard and is **~17 % wrong**.
  - Repair **auto-stops** at full health and **stops silently** when the owner cannot afford the next step (checked *before* the spend — no partial step, no free repair, no EVA line); the flag is cleared, so the player must click the wrench again.
  - **`URepairStep` / `URepairPercent` do not exist** in this build, and `SurvivorRate` is a dead key.
  - **Sell is a MISSION, not an instant action:** the buildup animation plays **MIRRORED** (`start + count − stage − 1`) for the buildup duration (`BuildupTime=.06` → 54 ticks ≈ 3.6 s, the same time the building took to appear), and only on completion do the credits land and the survivors spawn. Selling always calls `Repair(0)` first.
  - The refund is **FLAT**: `trunc(trunc(Cost × m2 × m1) × RefundPercent)`, `RefundPercent=50%` — **no HP scaling**, so a building at 1 HP refunds exactly what a full-health one does. `Soylent` overrides and bypasses `RefundPercent`. `RefundPercent` applies **only to human-controlled houses**; an AI house gets the unscaled value.
  - **Survivors** = `clamp(refund / SurvivorDivisor[side], 1, 5)`, divisors **500 / 250 / 750**, zero when `Crewed=no`; they exit onto the building's own foundation cells and a blocked survivor is deleted, not displaced.
  - **Refusals are exactly:** `Unsellable`, under construction, already selling, C4 applied — and state plainly that **there is no last-Construction-Yard rule and no captured-building rule**; both were searched for in the binary and neither exists, and the gate asserts that the only full-health ConYard IS sellable.
  - **The two buttons are mutually-exclusive toggle cursor modes:** placement wins, arming clears the selection, arming is refused when the player owns no buildings, and ESC/right-click cancels exactly one thing in the recovered priority order.
  - **Carry-forwards:** `UndeploysInto`/MCV redeploy (needs the deploy feature; the clone takes the plain-refund branch, a reachable configuration of the original), AI auto-repair/auto-sell-below-`ConditionRed` (PROBABLE + six unanchored offsets + an RNG draw that would perturb the AI gates), upgrade peel-off, Service Depot unit-sell, unit selling, garrison release, the 25 % special crew type, the 21-slot damaged-anim table, C4/mind-control refusals, the three unmodeled mode flags, and the **`MOUSE.SHP` wrench/dollar cursor frames (§C2 [RE-FALLBACK])**.

  Add the CLI bullet:

  > `ra2engine.exe --demo-repair` — the Building Repair & Sell integration gate: drives the wrench on a real Allied base (measuring the 14-frame global cadence, the +8 HP heal and the exact integer `stepCost` charge frame by frame), proves the auto-stop and the silent out-of-credits stop, exercises every recovered sell refusal **and** the two that do not exist, then runs a full sell mission and asserts the credits land only on completion, flat, with the recovered survivor count; 19 wedge guards armed (`docs/re/repair-sell.md`).

  Add the gate-table row:

  > `| --demo-repair | repair_hash=<observed> | deterministic=yes; repair = a GLOBAL 14-frame phase (trunc(RepairRate*900), NOT 15) x +8 HP x stepCost=max(1,trunc((Cost/(Strength/RepairStep))*RepairPercent)) with BOTH divisions integer (GACNST 3/step, 375 to full); auto-stop at full; silent stop when broke (no partial step, no free repair); refusals = Unsellable / under construction / already selling / C4 only -- NO last-ConYard rule and NO captured rule (the gate asserts the only full-health ConYard IS sellable); sell = a MISSION with the buildup played MIRRORED, paying a FLAT trunc(Cost*RefundPercent)=1500 refund ONLY on completion plus clamp(refund/500,1,5)=3 Allied survivors on the foundation; 19 wedge guards armed (docs/re/repair-sell.md) |`

  Update every "nineteen"/"19 gates" mention to "twenty"/"20 gates".

- [ ] **Step 6: Ledger.** Append a phase-complete entry to `.superpowers/sdd/progress.md` in the house style: what landed (the pure `repair_sell` TU, the eight inert struct fields, `Rules::section_flag` + the seven `[General]` scalars, the firewalled `step_repair_sell` pass with the wrench tick, the sell mission with the mirrored buildup / flat refund / survivors, the sidebar button rects + the two cursor modes, the 20th gate), the evidence (suite count, 19 gates bit-identical one-per-call, arm64 green, `repair_hash`), the RE-GATE outcomes from Task 1 §D, and the CARRY-FORWARDS list from the spec's §Scope.

- [ ] **Step 7: Commit.**

```powershell
git -C "D:\ra2-android" add README.md .superpowers/sdd/progress.md
git -C "D:\ra2-android" commit -m "$(printf 'docs(repair-sell): finalize - README repair/sell section + 20th gate row + ledger [RE: repair-sell.md]\n\nCo-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>')"
```

## Self-Review notes

- **The single highest-risk task is Task 4** — it inserts a pass into `World::step`, the hot path all 19 gates run. Its whole byte-identity argument rests on `repair_sell_any_` being unopenable from any gate path, which is why Task 4 re-verifies all 19 gates inside the task rather than only at finalize, and why Task 5 does it again (it adds the second opener, `begin_sell`). A reviewer should grep `toggle_repair|set_repair|begin_sell` across `engine/main.cpp` after Task 5 and confirm the only hits are `GameScene` (interactive) and `run_demo_repair`.
- **Task 5's `complete_sell` deliberately duplicates three lines of `World::step`'s blocking-release loop.** That is not an oversight: the release loop only fires on a building that was *already* `Dead` when it ran, i.e. the frame *after* the sale, but §B3 requires survivors to stand on the building's own foundation the moment it goes. `blocking_released` is latched so the loop never repeats the work.
- **`Building::State::Selling` was deliberately NOT added.** A fourth enumerator would silently change the meaning of every `state != Idle` test in the tree (power, targeting, placement, victory) and introduce a new value class into `world_hash`'s `state` fold. Keeping `state == Idle` while `selling == true` is both simpler and closer to the original, where the object is alive and functioning until `Limbo()` at mission state 2 — so a selling building keeps supplying power right up to the sale, exactly as it does in the game.
- **The gate's expected numbers are computed from the same pure functions the sim uses**, then wedge-guarded against the recovered literals (`interval == 14`, `RepairStep == 8`, `GACNST stepCost == 3`). That way a broken constant trips a *named* wedge line rather than silently moving the hash, and a hash move can only mean a genuine behavioral change.
- **Deliberate scope cuts** (justified in the spec §Scope, adjudicated in Task 1 §D): `UndeploysInto`/MCV redeploy and the AI's auto-repair/auto-sell. The first is cut because the clone has no deploy machinery at all and because the plain-refund branch the clone takes is a *reachable configuration of the original*, not an invention. The second is cut because its decision chain rests on six offsets that were never string-anchored — implementing it would be exactly the empirical guessing the calibrate-first directive forbids — and because its `Random(0,50)` draw would perturb the three AI byte-identity gates.
- **Where repair-sell.md left no pinned value** (all routed through Task 1): the survivor crew type (`Rules+0xF70`, PROBABLE → the 25 % special is CUT, basic infantry always), the Service-Depot radio query (`vtbl+0x274`, PROBABLE → out of scope), the AI reuse (§A7, PROBABLE → out of scope), the fate of in-progress production and the primary-factory flag on sell (§B4, **[RE-FALLBACK]** → the building-death path, justified by `~FactoryClass`), `PlayerPtr+0x2F0`'s identity (PROBABLE → "owns ≥ 1 building"), the mind-control refusal (`+0x1D0`, PROBABLE → omitted, recorded), the `UndeploysInto` option globals (PROBABLE → out of scope), and the **wrench/dollar `MOUSE.SHP` frame indices (§C2, [RE-FALLBACK] → likely stays open; the pressed button frame + hovered-building outline ship instead)**. No task may consume any of these before Task 1 lands and the controller applies the RE-GATE amendment.
- **`test_input.cpp` is the largest suite in the tree (52 KB).** Task 6 inserts a new intercept block at the top of `Input::handle`; if any pre-existing `input:*` case regresses, the block went in the wrong place (it must be *after* the placement block and *before* the ordinary gesture switch), not the tests' fault.
