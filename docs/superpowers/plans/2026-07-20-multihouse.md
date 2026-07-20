# Multi-house / Alliances Phase Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Generalize the engine's hardwired 2–3 house world into a proper **N-player** model (up to 8 combatant houses + non-combatant houses) with **alliances / teams** and **last-team-standing victory**, calibrated to RA2/YR. Replace the fixed trio 0=player / 1=enemy / 2=neutral and the `house != self_house` enmity test with (1) a **`HouseRec` table** (`houses_`) describing every house's side/color/country/alliance-team/combatant flag, defaulted to reproduce today's world exactly; (2) a pairwise **alliance predicate** (`is_enemy` / `are_allied`) that every inlined enmity site routes through (auto-acquire, splash "enemies-only", capture/infiltrate/repair own-vs-enemy, AI quarry/attack); (3) **AllyReveal** shared visibility wired into the shroud layer; (4) **last-team-standing victory/defeat** (ShortGame = last-building); (5) **per-house side/color rendering + spawn** driven from the table (8 MP colors); and (6) the six fixed `[3]` per-house arrays **vectorized** to `house_count()`. Demonstrated by the **16th permanent byte-identity gate `--demo-multihouse`** (`multihouse_hash`). The 15 existing gates stay bit-identical.

**Architecture:** Task 1 adjudicates the RE-GATE (the hunt `docs/re/multihouse.md` is COMPLETE — Task 1 re-pins countries / 8 colors / MaxPlayers / victory rule via `ra2extract` + OpenRA reference and closes the four `[RE-FALLBACK]` items, NOT a fresh hunt). The single source of truth becomes a `std::vector<HouseRec> houses_` on `World` (sized `house_count()`, default 3, populated with the exact 0/1/2 defaults) behind a `houses_configured_` firewall bool (D1). While `houses_configured_ == false` — which is every existing gate — `house_side`/`house_color`/`is_enemy`/`are_allied` route through the CURRENT derivation (`house 1 → Soviet/red, else Allied/blue`; `is_enemy(a,b) == (a != b)`), so the engine is byte-identical. The six fixed `[3]` per-house arrays become `std::vector`s sized `house_count()` with every `< 3` guard widened to `< house_count()` and `take_income`'s `> 1` bound widened to `>= house_count()` (D2); with `house_count() == 3` the indices 0/1/2 are computed by identical code paths and **none feeds `world_hash`**. Two new `const` predicates `is_enemy`/`are_allied` (D3) replace every inlined `house != self_house` / `house == attacker.house` site (the D-list, with confirmed file:line anchors below). AllyReveal (D4) OR-merges allied houses' `explored_`/`visible_` grids at the end of the firewalled `step_visibility`. A greenfield firewalled `step_victory()` pass (`victory_enabled_`, D5) marks defeated combatant houses and resolves `winning_team()`. `pal_for_house` gains an 8-color path via `house_color` and `spawn_base`/unit-side route through `house_side` (D6), with defaults reproducing the exact house-1-is-Soviet building TYPES that feed `world_hash`. `seen_by_mask` stays `uint8_t` (8-combatant cap, D7). `world_hash` and `combat.cpp`'s scan-body arithmetic are **NOT touched**; the new state is hashed ONLY by `multihouse_hash` (D8).

**Tech Stack:** C++17, doctest, CMake+Ninja (desktop `build/`, Android `android/` gradle, NDK `D:\android-ndk-r27c`, JDK-21), Ghidra headless (`analyzeHeadless.bat`, `JAVA_HOME=C:\Program Files\Java\jdk-21`, project `D:\ra2-re`) for the confirmatory RE-GATE only, SDL2 (render/UI layer), `ra2assets::Rules`/`ra2assets::Ini` as the rulesmd substrate, the existing `engine/world.{hpp,cpp}` firewall/step-pass pattern (`step_capture`/`capture_any_`, `step_visibility`/`shroud_any_`), the `engine/visibility.{hpp,cpp}` grids from the Shroud phase, `engine/combat.cpp`'s `valid_target`/`apply_spread_damage`, `engine/iso_renderer.cpp`'s `pal_for_house`, `main.cpp`'s `spawn_base`/`build_world`/`world_hash`, and the `run_demo_shroud`/`shroud_fold` gate template. Alliance predicates + victory + the HouseRec table are host-doctestable on a hand-built `World` (no install).

## Global Constraints

(Copied VERBATIM from the spec `docs/superpowers/specs/2026-07-20-multihouse-design.md` §Global Constraints — every task's requirements implicitly include this section.)

- All 15 existing byte-identity gates MUST stay bit-identical: combat logs=78, tank logs=144, base 5273b5e71ca009e1, econ 7fed96cb98b68017, prod f0f4e1207ecd713c, ai-base cd1f7c6a29933f0a, ai-team 312e92ed862a5e74, ai-trigger 8334a6ca1531cabc, transport fe500f946e913645, aircraft a69642564cf64773, naval 6aee2c82bc024e86, superweapon 189496664f5eca68, techtree e73adae61ff6f71f, capture 58b87b5bc63f7046, shroud d302852aa9517661. Adds the 16th: `--demo-multihouse`.
- `world_hash` MUST NOT be touched (folds only Unit{cell_x,cell_y,hp,alive}+Building{type,cx,cy,hp,state}; house is NOT folded — confirmed). Resizing per-house arrays cannot perturb any hash provided per-index values at 0/1/2 are computed identically.
- New per-frame logic (victory checks, AllyReveal sharing) is firewalled (`*_enabled_` bool → early return) — OFF in every existing gate.
- Enmity generalization defaults to EXACTLY today's behavior: `is_enemy(a,b) == (a != b)` unless alliances are explicitly configured. The gates never configure alliances → byte-identical.
- Demo gate = two-run compute-and-self-compare, NO pinned literal (README pins), wedge guards → exit 1.
- No assets/GPL/Ghidra raw output committed. Recovered CONSTANTS (country list, 8 colors, cash/UnitCount, victory rule) ARE committable.
- Commit trailer: `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`.

**Gate-runtime discipline (operational, binds every build/test/demo step):**

- **Run demo gates ONE per foreground call.** Each AI gate is ≈2 min / ~3 GB; a multi-gate chain in one call exceeds the 600 s tool timeout, auto-backgrounds, and reads as a false hang. Every build/test/demo step below orders a FOREGROUND, synchronous, blocking run, waited-for INLINE (timeout up to 600000 ms). NEVER "wait for a monitor". NEVER batch 3-4 gates.
- **Run exes from PowerShell, NOT Git Bash** (Git Bash gives exit 127). `RA2_INSTALL_DIR` default `D:\Red Alert 2`. NEVER pipe a live native exe through `Select-Object -First N` (stops draining stdout → false hang).
- Pure modules registered in BOTH `cmake/ra2_sources.cmake` AND `CMakeLists.txt` (project convention).
- Repo on master, solo, no remote. NDK `D:\android-ndk-r27c` for arm64 (JDK-21).

**Exact commands (use verbatim):**
- Build: `cmake --build D:\ra2-android\build --target ra2assets_tests ra2engine`
- Tests: `D:\ra2-android\build\ra2assets_tests.exe --test-case="<filter>*"`
- Gates (INDIVIDUALLY, one per call): `D:\ra2-android\build\ra2engine.exe --demo-X`

**The 15-gate re-verify command block (referenced by Tasks 3/4/5/7/9 — run ONE per foreground call):**

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
```

Expected, all fifteen pins EXACT: `deterministic=yes logs=78`; `deterministic=yes logs=144`; `hash=5273b5e71ca009e1`; `econ_hash=7fed96cb98b68017`; `prod_hash=f0f4e1207ecd713c`; `ai_base_hash=cd1f7c6a29933f0a`; `ai_team_hash=312e92ed862a5e74`; `ai_trigger_hash=8334a6ca1531cabc`; `transport_hash=fe500f946e913645`; `aircraft_hash=a69642564cf64773`; `naval_hash=6aee2c82bc024e86`; `superweapon_hash=189496664f5eca68`; `techtree_hash=e73adae61ff6f71f`; `capture_hash=58b87b5bc63f7046`; `shroud_hash=d302852aa9517661`.

### File Structure

| File | Responsibility |
|---|---|
| `docs/re/multihouse.md` (modify, Task 1) | Calibration authority (hunt COMPLETE). Task 1 adds a "§Adjudication (RE-GATE)" subsection re-pinning countries / 8 MP colors / MaxPlayers / victory rule via `ra2extract` and flips the four `[RE-FALLBACK]` items (8-color HSV→RGB conversion / precise ShortGame defeat / neutral default relationship / FFA-default enmity == a!=b) → adjudicated. |
| `engine/world.hpp` + `engine/world.cpp` (modify, Tasks 2/3/4/5/6/7) | `enum class Side`, `struct HouseRec`, `houses_`/`houses_configured_`; `house_side`/`house_color`/`house_count`/`house_team`/`house_combatant`/`set_house`; the six `[3]`→`std::vector` conversions + widened guards + `take_income` bound; `is_enemy`/`are_allied` + all enmity-site routing; AllyReveal merge in `step_visibility`; `victory_enabled_`/`short_game_`/`house_defeated_`/`winning_team_` + `enable_victory`/`house_defeated`/`winning_team`/`step_victory`; `house_palette`/`pal_for_color`. |
| `engine/combat.cpp` (modify, Task 4) | Route `valid_target` (line 63) + `apply_spread_damage` (line 145) through `World::is_enemy`. |
| `engine/iso_renderer.cpp` (modify, Task 7) | `pal_for_color` (8 MP-color HSV→remap spread) beside `pal_for_house`. |
| `engine/main.cpp` (modify, Tasks 7/8) | `spawn_base` / unit-side via `house_side`; render call sites via `house_palette`; `multihouse_fold` + `simulate_multihouse` + `run_demo_multihouse` + the `--demo-multihouse` dispatch. |
| `tests/test_world.cpp` (append, Tasks 2/3/4/5/6) | Host doctests: HouseRec defaults + accessors; `is_enemy`/`are_allied` truth table; vectorized-array parity; AllyReveal sharing; victory last-team-standing. |
| `README.md` (modify, Task 9) | Multi-house section + 16th-gate row + CLI bullet + deviations; "fifteen" → "sixteen". |

**Module-placement decision:** ALL multi-house engine state and logic lives on the existing `World` class in `engine/world.{hpp,cpp}` — there is **no new pure module** this phase (the alliance predicate + victory + table are `World` methods, host-doctestable on a hand-built `World` via `test_world.cpp`, which already compiles `engine/world.cpp` into `ra2assets_tests`). Therefore **no `cmake/ra2_sources.cmake` / `CMakeLists.txt` source-registration change is required** — verify at finalize (Task 9) that no new `.cpp`/test file was introduced; if a task DOES add one, it MUST be registered in BOTH `cmake/ra2_sources.cmake` (→ `ra2engine`) AND `CMakeLists.txt` (→ `ra2assets_tests`) per project convention. `pal_for_color` is a free function added beside `pal_for_house` in `engine/iso_renderer.cpp` (already compiled into both targets) — no cmake change.

**Live-code reconciliation (grepped against the post-Shroud tree at write time — folded into the tasks below):**
- The six `[3]` arrays are at the design-direction's lines EXACTLY: `blackout_frames_[3]` (world.hpp:1032), `produced_veteran_[3]` (1038), `explored_[3]` (1052, already `std::vector<std::uint8_t>[3]`), `visible_[3]` (1053, already `std::vector<std::uint8_t>[3]`), `map_reveal_[3]` (1060, a `bool[3]` SpySat latch added by Shroud), `house_credits_[3]` (1116). **Note:** `explored_`/`visible_` are ALREADY `std::vector<std::uint8_t>[3]`, so vectorizing them means the OUTER `[3]` → `std::vector<std::vector<std::uint8_t>>` (T3).
- The accessor `< 3` guards shifted **+1** from the design-direction: `house_credits()` **869** (design said 868), `blackout_frames()` **872** (871), `produced_veteran()` **879** (878), `grant_credits_for_test()` **916** (915). `take_income` bound `house < 0 || house > 1` is at world.cpp:**408** (matches).
- Every world.cpp enmity site matches the design-direction lines EXACTLY: nuke building-splash **1106** (`b.house == house`), `capture_building` **1149** (same-owner guard **1155**), `infiltrate_building` **1181** (self-spy **1187**), `repair_building` **1216** (own-only **1220**), `step_capture` arrival **1352** (`b.house != eng_house`), disguise detect **1487** (`s.house == d.house`), `ai_quarry_exists` **1971/1975**, `ai_pick_quarry_unit` **1991**, `ai_pick_quarry_building` **2010**, `ai_order_attack` **2058**, `ai_order_attack_building` **2069**. combat.cpp `valid_target` **59-63** (`v.house != self_house`), `apply_spread_damage` **145** (`w.house == attacker.house`).
- `pal_for_house` at world.cpp:**290** (`if (house == 1) return out;` red for house 1, blue-ish spread else). `spawn_base` at main.cpp:**1783**, building type `house == 1 ? "NACNST" : "GACNST"` at **1808** (this TYPE feeds `world_hash`). Unit side `house == 1 ? "E2" : "E1"` at **3072** (image name — render-only, NOT hashed). `world_hash` at main.cpp:**4300** (unchanged this phase).
- `step_visibility` at world.cpp:**1540** with per-house loops `for (int h = 0; h < 3; ++h)` at **1544/1550/1574**, the `seen_by_mask` stamp loop at **1569** (`m |= 1u << h` — MUST stay ≤ 8 for the `uint8_t` mask), and `b.house < 3` at **1547**. `World::step` calls `step_visibility()` at **2574** and `step_capture(dt)` at **2588**; `step_combat` at **2630**.
- No pre-existing `Side`/`HouseRec`/`houses_`/`is_enemy`/`are_allied`/`victory_enabled_`/`winning_team`/`house_defeated` symbols (all greenfield — grep found none).

---

### Task 1: RE-GATE adjudication (NOT a fresh hunt) → docs/re/multihouse.md

**Files:**
- Modify: `docs/re/multihouse.md` (add a "§Adjudication (RE-GATE)" subsection; flip the four `[RE-FALLBACK]` items in the spec's last section to adjudicated, each with a justification)
- Scratch: extracted `rulesmd.ini` + `ra2extract` probes + any targeted OpenRA/decompile cross-checks under the session scratchpad (never committed)

**Interfaces:**
- Consumes: the COMPLETED hunt `docs/re/multihouse.md` (§1 countries/sides/MaxPlayers, §2 `[Colors]` HSV list + 8 MP colors, §3 alliance/relationship + FFA default + AllyReveal, §5 ShortGame victory), `D:\ra2-android\build\ra2extract.exe` against `D:\Red Alert 2`, the OpenRA reference at `D:\ra2-re\reference\OpenRA` (`Player.cs:249`, `AutoTarget.cs:342/420`, `PlayerExtensions.cs:19`, `ConquestVictoryConditions.cs:63`), and (only if a decompile cross-check is needed) the Ghidra project at `D:\ra2-re` (FOREGROUND, generous timeouts, do NOT background).
- Produces: pinned values for the four `[RE-FALLBACK]` items, folded into the plan by the controller in a RE-GATE amendment step (each consuming point in Tasks 2/6/7/8 is written PRE-ADJUDICATION with a fallback inline and marked `[RE-GATE: multihouse.md §N]`).

**The four items to adjudicate** (everything else in §1–§8 is already CONFIRMED/ANCHORED — do NOT re-open it):

1. **8-color HSV→RGB conversion (§2, D6/T7).** The eight MP colors are recovered as HSV triples (Gold=43,239,255; DarkRed=0,230,255; Orange=25,230,255; Magenta=221,102,255; Purple=201,201,189; DarkBlue=153,214,212; DarkSky=131,200,230; DarkGreen=81,200,210). Adjudicate the concrete HSV→RGB remap-spread conversion the engine uses to tint the art's reserved house-color band (confirm it matches the existing `pal_for_house` spread rule), so the 8-color output is deterministic. Render is unhashed, so any faithful conversion reproducing the recovered hues is acceptable. FALLBACK: standard `HSV(h∈0..255,s,v)→RGB` (RA2 hue is 0..255, not 0..360) applied per remap-band index to spread Value across indices 16..31 like `pal_for_house` does, documented `[RE-FALLBACK]` as cosmetic/unhashed.
2. **Precise ShortGame defeat condition (§5, D5/T6).** Confirm `ShortGame=yes` (shipped default) ⇒ defeated on **last building**, vs `ShortGame=no` ⇒ zero-buildings-AND-units, and confirm the "required building" set (all structures counting, per `PlayerExtensions.cs:19` `RequiredForShortGame`). Adjudicate that the gate drives `ShortGame=yes` (last-building). FALLBACK: `defeated = short_game ? (alive_buildings == 0) : (alive_buildings == 0 && alive_units == 0)`, all alive non-Dead buildings count.
3. **Neutral default relationship (§3, D1/D3/T4).** Confirm a neutral / `MultiplayPassive` house is relationship-**Neutral** by default (never auto-attacked, never attacks) — matching `Player.cs:249`'s `NonCombatant ? Neutral` branch — i.e. non-combatant ⇒ never an enemy of anyone (both directions). FALLBACK: `is_enemy(a,b) == false` whenever `!house_combatant(a) || !house_combatant(b)`.
4. **FFA-default enmity == `a != b` (§3, D3/T4/T8).** Confirm that with `AlliesAllowed=no` (FFA) every combatant pair is Enemy and this equals `is_enemy(a,b) == (a != b)` among combatants — i.e. the configured-FFA path (every house its own `alliance_team`) and the unconfigured default path AGREE, so the gate's alliance-configured FFA baseline is byte-consistent with today's `a != b` behavior. FALLBACK: confirmed consistent — `alliance_team` sentinel `-1` resolves to own id ⇒ no two combatants share a team ⇒ `are_allied(a,b)==false` for `a!=b` ⇒ `is_enemy(a,b)==(a!=b)`.

**Steps:**

- [ ] **Step 1: Re-read the hunt + method.** Read `docs/re/multihouse.md` end-to-end (calibration authority) and note the confirmed anchors: `[Countries]` line 959 (14 entries), `[Sides]` line 981 (GDI=Allied/Nod=Soviet/ThirdSide=Yuri), `[Colors]` line 3367 + HSV comment 3357 (8 `MP`-tagged colors), `[MultiplayerDialogSettings]` `AlliesAllowed=no`/`AllyChangeAllowed=yes`/`ShortGame=yes`/`AllyReveal=yes` (line 751)/`Money=10000`/`UnitCount=10`/`TechLevel=10`. Do NOT re-hunt these.

- [ ] **Step 2: Extract the real rulesmd + re-pin the constants (FOREGROUND, blocking, timeout 600000 ms).**

```powershell
cmake --build D:\ra2-android\build --target ra2extract
D:\ra2-android\build\ra2extract.exe "D:\Red Alert 2\ra2md.mix" --raw localmd.mix/rulesmd.ini "C:\Users\Omar\AppData\Local\Temp\claude\d--Red-Alert-2\d44bece6-5e7c-465a-9fd5-820d808b4426\scratchpad\rulesmd.ini"
```

If `localmd.mix/rulesmd.ini` misses, walk the nested candidates (`expandmd01..05.mix`, `expandmd99.mix`, `cachemd.mix`, `conqmd.mix`, then top level). Record IN SCRATCH, verbatim: the `[Countries]` list, `[Sides]` mapping, the 8 `MP`-flagged `[Colors]` HSV triples, `MultiplayPassive=true` on `[Neutral]`/`[Special]`, and the `[MultiplayerDialogSettings]` victory/ally/start keys. Confirm NO `MaxPlayers` key (8-color cap). Cross-check the four items against OpenRA (`Player.cs:249` for item 3, `PlayerExtensions.cs:19`/`ConquestVictoryConditions.cs:63` for item 2).

- [ ] **Step 3: Adjudicate the four items.** For each, write the pinned value (or the FALLBACK with an explicit `[RE-FALLBACK]` tag) into a new "§Adjudication (RE-GATE)" subsection in `docs/re/multihouse.md`, each with a rulesmd-line / OpenRA-file justification. Item 1 MUST state the concrete HSV→RGB rule (or "cosmetic, unhashed — any faithful conversion"). Item 2 MUST state the ShortGame defeat predicate + the counting set. Item 3 MUST state non-combatant ⇒ never-enemy (both directions). Item 4 MUST state FFA-default == `a!=b` consistency.

- [ ] **Step 4: Commit the adjudication.**

```powershell
git -C "D:\ra2-android" add docs/re/multihouse.md
git -C "D:\ra2-android" commit -m "$(printf 'docs(re): multihouse RE-GATE adjudication - re-pin countries/8 MP colors/MaxPlayers=8/ShortGame victory via ra2extract, fix 8-color HSV->RGB conversion (cosmetic/unhashed), ShortGame last-building defeat, neutral=never-enemy, FFA-default==a!=b; flip [RE-FALLBACK]->adjudicated [RE: multihouse.md]\n\nCo-Authored-By: Claude Fable 5 <noreply@anthropic.com>')"
```

- [ ] **Step 5 (CONTROLLER, not the implementer): RE-GATE amendment.** After the adjudication lands, the controller diffs it against every `[RE-GATE: multihouse.md §N]` block in Tasks 2/6/7/8, amends the plan (Fable trailer) BEFORE dispatching Task 2, and flips each marker to `[RE-GATE APPLIED: multihouse.md §N]` with the confirmed value inline. None should be left `[RE-FALLBACK]` needing human escalation.

---

### Task 2: HouseRec table + houses_ vector + houses_configured_ firewall (pure-ish + host tests)

**Files:**
- Modify: `engine/world.hpp` (`enum class Side`, `struct HouseRec`, `houses_`/`houses_configured_` members, the accessor decls), `engine/world.cpp` (populate `houses_` with the 3 defaults at construction; the accessor bodies)
- Test: `tests/test_world.cpp` (append)

**Interfaces:**
- Consumes: the `World` constructor / init path (where `grid_w_`/`grid_h_` are set — the `houses_` default population goes here, or in an idempotent lazy `ensure_houses_default()` called by every accessor), the grow-on-demand precedent `ai_houses_`/`sw_houses_` (world.hpp:953/997, resize world.cpp:1008/1041).
- Produces — `engine/world.hpp` (new public types, near the top of the `World` public section, before the capture/shroud API ~world.hpp:790):

```cpp
    // --- Multi-house / Alliances phase [RE: multihouse.md §1/§2, D1] ---------
    // On-screen sides (the [Sides] legacy GDI=Allied / Nod=Soviet / ThirdSide=Yuri
    // are resolved to these). Neutral = Civilian/Special (non-combatant).
    enum class Side { Allied, Soviet, Yuri, Neutral };
    // One record per house (including player 0). Defaults reproduce today's world
    // 0=player Allied / 1=enemy Soviet / 2=neutral EXACTLY. NEVER read by
    // world_hash. color_ix indexes the 8 MP colors (or a default). alliance_team
    // sentinel -1 resolves to the house's own id (self-only team = FFA).
    struct HouseRec {
        Side        side          = Side::Allied;
        int         color_ix      = 0;      // 0..7 into the 8 MP colors
        std::string country       = "";
        int         alliance_team = -1;     // sentinel; -1 -> own id on read
        bool        combatant     = true;
    };
    // Number of houses (default 3). The six per-house vectors + house_defeated_
    // are sized to this. Grows via set_house.
    int  house_count() const { return houses_.empty() ? 3 : (int)houses_.size(); }
    // Side / color for house h. With houses_configured_ FALSE -> today's exact
    // derivation (house 1 -> Soviet/red, else Allied/blue) -> byte-identical.
    Side house_side(int h) const;
    int  house_color(int h) const;
    // Alliance team for h (sentinel -1 -> own id). Non-combatant houses keep their
    // own team but never ally/enemy anyone (see is_enemy/are_allied, Task 4).
    int  house_team(int h) const;
    bool house_combatant(int h) const;
    // Configure house h. Grows houses_ (and the per-house vectors, once T3 lands)
    // to h+1 with defaults if needed, sets houses_[h]=rec, and OPENS the firewall
    // (houses_configured_ = true). No existing gate calls this -> byte-identical.
    void set_house(int h, HouseRec rec);
    bool houses_configured() const { return houses_configured_; }
```

- Produces — `engine/world.hpp` (new private members, in a Multi-house block near the capture/shroud private members ~world.hpp:1060):

```cpp
    // --- Multi-house / Alliances phase [RE: multihouse.md §1/D1] ---
    // The single source of truth for every house. Populated with 3 defaults at
    // construction (0=Allied/team0/comb, 1=Soviet/team1/comb, 2=Neutral/team2/
    // non-comb) so house_count()==3 and today's world is reproduced. NOT hashed.
    std::vector<HouseRec> houses_;
    bool houses_configured_ = false;   // the side/color/enmity/AllyReveal firewall;
                                       // false in every existing gate.
```

- Produces — `engine/world.cpp` (populate defaults + accessor bodies). Populate `houses_` where the map dims are established (or lazily via `ensure_houses_default()` invoked at the top of every accessor + `set_house`; the lazy form avoids ordering fragility and is idempotent):

```cpp
void World::ensure_houses_default() {
    if (!houses_.empty()) return;
    houses_.resize(3);
    houses_[0] = HouseRec{Side::Allied,  /*color*/ kMpColorBlue,  "", 0, true};
    houses_[1] = HouseRec{Side::Soviet,  /*color*/ kMpColorRed,   "", 1, true};
    houses_[2] = HouseRec{Side::Neutral, /*color*/ kMpColorBlue,  "", 2, false};
    // houses_configured_ stays FALSE -> accessors use today's derivation.
}

World::Side World::house_side(int h) const {
    if (!houses_configured_) return (h == 1) ? Side::Soviet : Side::Allied;  // today's derivation
    if (h < 0 || h >= (int)houses_.size()) return Side::Allied;
    return houses_[h].side;
}

int World::house_color(int h) const {
    if (!houses_configured_) return (h == 1) ? kMpColorRed : kMpColorBlue;   // today's red/blue
    if (h < 0 || h >= (int)houses_.size()) return kMpColorBlue;
    return houses_[h].color_ix;
}

int World::house_team(int h) const {
    if (h < 0 || h >= (int)houses_.size()) return h;   // unknown -> own id
    const int t = houses_[h].alliance_team;
    return (t < 0) ? h : t;                             // sentinel -> own id (FFA)
}

bool World::house_combatant(int h) const {
    if (h < 0 || h >= (int)houses_.size()) return true;   // unknown -> combatant (inert)
    return houses_[h].combatant;
}

void World::set_house(int h, HouseRec rec) {
    if (h < 0) return;
    ensure_houses_default();
    if (h >= (int)houses_.size()) {
        const int old = (int)houses_.size();
        houses_.resize(h + 1);
        for (int i = old; i <= h; ++i) houses_[i] = HouseRec{};   // default combatant
        grow_house_arrays((int)houses_.size());   // T3: resize the six vectors + house_defeated_
    }
    houses_[h] = rec;
    houses_configured_ = true;   // open the firewall
}
```

  `kMpColorBlue`/`kMpColorRed` are named `constexpr int` MP-color indices (into the 8 colors — `DarkBlue`=index of the blue MP color, `DarkRed`=the red one) declared near `pal_for_house`; they are consulted only when `houses_configured_` (render-only, unhashed), so any two distinct valid 0..7 values are acceptable. **`ensure_houses_default()` and `grow_house_arrays(int)` are private helper decls added in world.hpp**; `grow_house_arrays` is a stub in this task (`{}`) and resizes the six vectors in Task 3 (declared here so `set_house` compiles). Call `ensure_houses_default()` at the top of `house_side`/`house_color`/`house_team`/`house_combatant` too (const — make `houses_` `mutable` OR populate eagerly in the constructor; prefer eager population in the constructor body and drop the lazy calls if the constructor is a clean single site — verify at implement time).

**world_hash safety:** this task adds NEW state (`houses_`, `houses_configured_`) that `world_hash` (main.cpp:4300, folds only `Unit{cell_x,cell_y,hp,alive}` + `Building{type,cx,cy,hp,state}`) does NOT read. `world_hash` is NOT touched.

**Steps:**

- [ ] **Step 1: Write the failing World test.** Append to `tests/test_world.cpp` (host-only, hand-built):

```cpp
TEST_CASE("World multihouse: HouseRec defaults reproduce today's 0/1/2 world [RE: multihouse.md §1]") {
    ra2engine::World world = make_test_world();
    CHECK(world.house_count() == 3);
    // Defaults (houses_configured_ == false -> today's derivation):
    CHECK(world.house_side(0) == ra2engine::World::Side::Allied);
    CHECK(world.house_side(1) == ra2engine::World::Side::Soviet);   // the hashed spawn branch
    CHECK(world.house_side(2) == ra2engine::World::Side::Allied);   // !=1 -> Allied
    CHECK(world.house_combatant(0));
    CHECK(world.house_combatant(1));
    CHECK_FALSE(world.houses_configured());
    // alliance_team sentinel resolves to own id (FFA) before configure:
    CHECK(world.house_team(0) == 0);
    CHECK(world.house_team(1) == 1);
    CHECK(world.house_team(2) == 2);
}

TEST_CASE("World multihouse: set_house opens the firewall + grows the table [RE: multihouse.md §1/D1]") {
    ra2engine::World world = make_test_world();
    // Configure houses 0+3 onto team 7 (FFA otherwise), house 3 is new (grows).
    world.set_house(0, {ra2engine::World::Side::Allied, 5, "Americans", 7, true});
    world.set_house(3, {ra2engine::World::Side::Soviet, 1, "Russians", 7, true});
    CHECK(world.houses_configured());
    CHECK(world.house_count() == 4);
    CHECK(world.house_side(3) == ra2engine::World::Side::Soviet);
    CHECK(world.house_team(0) == 7);
    CHECK(world.house_team(3) == 7);          // shared team
    CHECK(world.house_side(0) == ra2engine::World::Side::Allied);
}
```

- [ ] **Step 2: Run to verify failure FOREGROUND, blocking (timeout 600000 ms).**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
D:\ra2-android\build\ra2assets_tests.exe --test-case="*multihouse*"
```

Expected: compile FAILS (`Side`/`HouseRec`/`house_side`/`set_house` not members of `World`).

- [ ] **Step 3: Land the types + members + accessors + defaults** exactly as in Interfaces (add `#include <string>`/`<vector>` to world.hpp if not present; declare `ensure_houses_default`/`grow_house_arrays` private).

- [ ] **Step 4: Build + run FOREGROUND, blocking (timeout 600000 ms), expected PASS. Build BOTH targets:**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests ra2engine
D:\ra2-android\build\ra2assets_tests.exe --test-case="*multihouse*"
```

Expected: both targets build; matched cases pass, 0 failed.

- [ ] **Step 5: Full suite FOREGROUND, blocking (timeout 600000 ms), expected 0 failed:**

```powershell
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2assets_tests.exe
```

- [ ] **Step 6: Commit.** (No gate re-verify needed yet: no gate calls `set_house`, `houses_configured_` is false, and no existing code path reads the new accessors — the table is inert. The FIRST gate re-verify is Task 3.)

```powershell
git -C "D:\ra2-android" add engine/world.hpp engine/world.cpp tests/test_world.cpp
git -C "D:\ra2-android" commit -m "$(printf 'feat(engine): HouseRec table + houses_ vector + houses_configured_ firewall - enum Side{Allied,Soviet,Yuri,Neutral}, house_side/house_color/house_team/house_combatant/house_count/set_house; defaults reproduce today 0=Allied/1=Soviet/2=Neutral, alliance_team sentinel -> own id (FFA); inert with firewall off [RE: multihouse.md §1]\n\nCo-Authored-By: Claude Fable 5 <noreply@anthropic.com>')"
```

---

### Task 3: Vectorize the six [3] per-house arrays → std::vector sized house_count() (FIRST 15-gate re-verify)

**Files:**
- Modify: `engine/world.hpp` (the six member declarations + the four accessor `< 3` guards + the `grow_house_arrays` body decl), `engine/world.cpp` (`grow_house_arrays` body; size the six vectors at construction/`ensure_houses_default`; widen `take_income` bound at 408; widen the `step_visibility` per-house loops at 1544/1547/1550/1569/1574; widen `visible_at`/`explored_at`/`reveal_source`/`reshroud_house`/`reveal_all_house`/`enable_shroud` `3`/`>= 3` guards)
- Test: `tests/test_world.cpp` (append — parity)

**Interfaces:**
- Consumes: `house_count()` (Task 2), the six arrays at their confirmed lines, the accessor guards (world.hpp:869/872/879/916), `take_income` (world.cpp:408), `step_visibility` (world.cpp:1540-1576), `visible_at`/`explored_at`/`reveal_source`/`reshroud_house`/`reveal_all_house`/`enable_shroud` (Shroud phase).
- Produces — `engine/world.hpp` (the six declarations change type; keep the same names + comments):

```cpp
    // T3 [RE: multihouse.md §D2]: [3] -> std::vector sized house_count() (default
    // 3 -> indices 0/1/2 identical). NONE feeds world_hash. Sized in
    // ensure_houses_default / grow_house_arrays.
    std::vector<int>          blackout_frames_;   // was int[3]          (world.hpp:1032)
    std::vector<std::uint8_t> produced_veteran_;  // was bool[3]         (1038)
    std::vector<std::vector<std::uint8_t>> explored_;  // was vector<u8>[3] (1052)
    std::vector<std::vector<std::uint8_t>> visible_;   // was vector<u8>[3] (1053)
    std::vector<std::uint8_t> map_reveal_;        // was bool[3] latch    (1060)
    std::vector<double>       house_credits_;     // was double[3]        (1116)
```

- Produces — `engine/world.cpp` (`grow_house_arrays` sizes all six to `n`, preserving existing values; called from `ensure_houses_default` with `n=3` and from `set_house` on growth):

```cpp
void World::grow_house_arrays(int n) {
    if ((int)blackout_frames_.size()  < n) blackout_frames_.resize(n, 0);
    if ((int)produced_veteran_.size() < n) produced_veteran_.resize(n, 0);
    if ((int)explored_.size()         < n) explored_.resize(n);   // empty grids (shroud off)
    if ((int)visible_.size()          < n) visible_.resize(n);
    if ((int)map_reveal_.size()       < n) map_reveal_.resize(n, 0);
    if ((int)house_credits_.size()    < n) house_credits_.resize(n, 0.0);
}
```

  and in `ensure_houses_default` (Task 2), after `houses_.resize(3)`, call `grow_house_arrays(3);` — so all six are size 3 by construction, identical length to the old `[3]` arrays.

- Produces — `engine/world.hpp` accessor guards (widen `< 3` → `< house_count()`; the four accessors at 869/872/879/916):

```cpp
        return (house >= 0 && house < house_count()) ? house_credits_[house] : 0.0;   // 869
        return (house >= 0 && house < house_count()) ? blackout_frames_[house] : 0;   // 872
        return house >= 0 && house < house_count() && produced_veteran_[house];       // 879
        if (house >= 0 && house < house_count()) house_credits_[house] = amount;      // 916
```

- Produces — `engine/world.cpp` `take_income` (world.cpp:408) — widen `> 1` to `>= house_count()`:

```cpp
double World::take_income(int house) {
    if (house < 0 || house >= house_count()) return 0.0;   // was: house > 1
    const double v = house_credits_[house];
    house_credits_[house] = 0.0;
    return v;
}
```

  **Byte-identity note:** the econ fold's caller (`rec.credits += take_income(h)`, world.cpp:1764) drives `h` over AI/player houses; house 2 (neutral) accrues NO income, so `house_credits_[2] == 0.0` always ⇒ `take_income(2)` returns 0.0 under BOTH the old `> 1` guard and the new `>= house_count()` (==3) guard, and zeroing 0.0 is a no-op. Byte-identical. NONE of the six feeds `world_hash`.

- Produces — `engine/world.cpp` `step_visibility` + shroud-accessor guards: widen the per-house loops that touch the six vectors from `< 3` / `>= 3` to `< house_count()` / `>= house_count()`, EXCEPT keep the `seen_by_mask` bit-stamp bounded to 8 (the `uint8_t` mask, D7):

```cpp
    // step_visibility (world.cpp:1544/1547/1550/1574): the map_reveal_ + grid loops
    for (int h = 0; h < house_count(); ++h) map_reveal_[h] = false;                // 1544
    ... if (b.house >= 0 && b.house < house_count() && b.grants_map_reveal) ...     // 1547
    for (int h = 0; h < house_count(); ++h) if (!visible_[h].empty()) std::fill(...);// 1550
    for (int h = 0; h < house_count(); ++h) if (map_reveal_[h]) reveal_all_house(h);// 1574
    // seen_by_mask stamp (1569): bounded to 8 combatant houses (uint8_t cap, D7):
    for (int h = 0; h < house_count() && h < 8; ++h)
        if (visible_at(h, u.cell_x, u.cell_y)) m |= (std::uint8_t)(1u << h);
```

  and widen `visible_at`/`explored_at` (`house >= 3` → `>= house_count()`), `reveal_source`/`reshroud_house`/`reveal_all_house` (`h >= 3` → `>= house_count()`), and `enable_shroud`'s `for (int h = 0; h < 3; ++h)` → `< house_count()`.

**Steps:**

- [ ] **Step 1: Write the failing parity test.** Append to `tests/test_world.cpp` (host-only):

```cpp
TEST_CASE("World multihouse: six per-house arrays vectorized, parity at 0/1/2 [RE: multihouse.md §D2]") {
    ra2engine::World world = make_test_world();
    CHECK(world.house_count() == 3);
    // house_credits read/write parity at 0/1/2 + out-of-range reject.
    world.grant_credits_for_test(0, 100.0);
    world.grant_credits_for_test(1, 250.0);
    world.grant_credits_for_test(2, 7.0);
    CHECK(world.house_credits(0) == doctest::Approx(100.0));
    CHECK(world.house_credits(1) == doctest::Approx(250.0));
    CHECK(world.house_credits(2) == doctest::Approx(7.0));
    CHECK(world.house_credits(3) == doctest::Approx(0.0));   // out of range -> 0
    CHECK(world.house_credits(-1) == doctest::Approx(0.0));
    // take_income zeros only the taken index; neutral has no income path.
    CHECK(world.take_income(0) == doctest::Approx(100.0));
    CHECK(world.house_credits(0) == doctest::Approx(0.0));
    CHECK(world.take_income(3) == doctest::Approx(0.0));     // >= house_count() -> 0
}

TEST_CASE("World multihouse: per-house arrays grow with set_house [RE: multihouse.md §D2]") {
    ra2engine::World world = make_test_world();
    world.set_house(4, {ra2engine::World::Side::Soviet, 1, "Russians", 4, true});
    CHECK(world.house_count() == 5);
    world.grant_credits_for_test(4, 42.0);                   // index 4 now valid
    CHECK(world.house_credits(4) == doctest::Approx(42.0));
    CHECK(world.take_income(4) == doctest::Approx(42.0));    // widened bound
}
```

  (Adapt `grant_credits_for_test`/`house_credits`/`take_income` to their real signatures — they exist at world.hpp:916/869 and world.cpp:407.)

- [ ] **Step 2: Run to verify failure FOREGROUND, blocking (timeout 600000 ms).**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
D:\ra2-android\build\ra2assets_tests.exe --test-case="*vectorized*"
```

Expected: FAIL — `house_credits(4)`/`take_income(4)` still reject (bounds not widened / index 4 out of the old `[3]`).

- [ ] **Step 3: Land the six type conversions + `grow_house_arrays` + the widened guards + `take_income` bound + the `step_visibility`/shroud-accessor loops** exactly as in Interfaces.

- [ ] **Step 4: Build + run FOREGROUND, blocking (timeout 600000 ms), expected PASS. Build BOTH targets:**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests ra2engine
D:\ra2-android\build\ra2assets_tests.exe --test-case="*vectorized*"
D:\ra2-android\build\ra2assets_tests.exe --test-case="*multihouse*"
D:\ra2-android\build\ra2assets_tests.exe --test-case="*shroud*"
```

Expected: both targets build; all matched cases pass (the shroud host cases still pass — the grids are now `std::vector<std::vector<...>>` but sized 3 and indexed identically), 0 failed.

- [ ] **Step 5: Full suite FOREGROUND, blocking (timeout 600000 ms), expected 0 failed:**

```powershell
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2assets_tests.exe
```

- [ ] **Step 6: FIRST 15-gate re-verify FOREGROUND, blocking (timeout 600000 ms EACH; ONE per call).** This is the RISKIEST task — the six arrays' backing store changed shape. Prove all fifteen pins byte-identical using the **15-gate re-verify command block** (Global Constraints). With `house_count() == 3` the vectors are the same length as the old `[3]` arrays and per-index values at 0/1/2 are computed by identical code; none feeds `world_hash`; the capture/econ/shroud folds read per-index values which are unchanged. **ANY drift = a mis-sized vector (a vector not allocated to size 3 before first use) OR an accessor reading beyond `size()` (a `< 3` guard mis-widened, or a `house_count()` returning 0/wrong)** — diagnose with systematic-debugging before proceeding. Pay special attention to `--demo-econ` (7fed96cb98b68017, `house_credits_`/`take_income`), `--demo-capture` (58b87b5bc63f7046, `house_credits_`/`produced_veteran_`/`blackout_frames_`), and `--demo-shroud` (d302852aa9517661, `explored_`/`visible_`/`map_reveal_`). Fix before proceeding.

- [ ] **Step 7: Commit.**

```powershell
git -C "D:\ra2-android" add engine/world.hpp engine/world.cpp tests/test_world.cpp
git -C "D:\ra2-android" commit -m "$(printf 'feat(engine): vectorize the six per-house [3] arrays -> std::vector sized house_count() (blackout_frames_/produced_veteran_/explored_/visible_/map_reveal_/house_credits_), widen <3 accessor guards + take_income bound + step_visibility loops (seen_by_mask capped 8); default size 3 -> indices 0/1/2 identical, none hashed; 15 gates re-verified [RE: multihouse.md §D2]\n\nCo-Authored-By: Claude Fable 5 <noreply@anthropic.com>')"
```

---

### Task 4: is_enemy / are_allied + route ALL enmity sites (combat=78 / tank=144 + all 15 gates re-verify)

**Files:**
- Modify: `engine/world.hpp` (`is_enemy`/`are_allied` decls), `engine/world.cpp` (bodies + route the world.cpp enmity sites), `engine/combat.cpp` (`valid_target` line 63 + `apply_spread_damage` line 145)
- Test: `tests/test_world.cpp` (append — truth table)

**Interfaces:**
- Consumes: `houses_configured_`/`house_combatant`/`house_team` (Task 2), the enmity sites at their confirmed lines (Live-code reconciliation above). `combat.cpp`'s `valid_target` takes `(const std::vector<Unit>&, int self_house, int idx)` — it has NO `World&`, so the predicate must be reachable as a **free function** too. Add a free `bool is_enemy_houses(...)` OR pass the needed state; the cleanest is a free function mirroring the member (see below) that `combat.cpp` calls and `World::is_enemy` delegates to.
- Produces — `engine/world.hpp` (public, near `house_combatant`):

```cpp
    // --- Multi-house / Alliances phase [RE: multihouse.md §3/§4, D3] ---
    // Pairwise relationship predicates. With houses_configured_ FALSE ->
    // is_enemy(a,b) == (a != b) and are_allied(a,b) == (a == b) -> EXACTLY today's
    // behavior -> byte-identical in every gate. Neutral/non-combatant is NEVER an
    // enemy of anyone (both directions) and never allied to a house it doesn't
    // share a team with. Self is always allied to self. NOT read by world_hash.
    bool are_allied(int a, int b) const;
    bool is_enemy(int a, int b) const;
```

- Produces — `engine/world.cpp` (bodies):

```cpp
bool World::are_allied(int a, int b) const {
    if (a == b) return true;                       // self always allied to self
    if (!houses_configured_) return false;         // today: only same-house is friendly
    if (!house_combatant(a) || !house_combatant(b)) return false;   // neutral: not an ally
    return house_team(a) == house_team(b);
}

bool World::is_enemy(int a, int b) const {
    if (!houses_configured_) return a != b;        // EXACTLY today's behavior
    if (a == b) return false;
    if (!house_combatant(a) || !house_combatant(b)) return false;   // neutral NEVER an enemy
    return !are_allied(a, b);
}
```

- Produces — `engine/combat.cpp`: because `valid_target`/`apply_spread_damage` are free functions in the combat TU (no `World&`), thread a `const World&` (or a small `const HouseRel&` view) so they can consult `is_enemy`. Preferred minimal change: give `valid_target` and `apply_spread_damage` a `const World& world` parameter (the combat TU already includes `world.hpp` types) and route through `world.is_enemy(self_house, v.house)`:

```cpp
// valid_target (combat.cpp:59-63) -- replace `v.house != self_house`:
bool valid_target(const std::vector<Unit>& units, const World& world, int self_house, int idx) {
    const Unit& v = units[idx];
    return v.alive && v.die_time < 0.0 &&
           /* ...existing predicates... */ &&
           world.is_enemy(self_house, v.house);   // was: v.house != self_house
}
```

```cpp
// apply_spread_damage (combat.cpp:145) -- "enemies only" firing-house spare.
// NOTE: splash is location-based and STILL hurts allies + own units in RA2
// (spec §Behavior). The existing line spares ONLY the ATTACKER's own house
// (w.house == attacker.house), NOT allies -- that is faithful and MUST be kept.
// Do NOT route this to is_enemy (that would wrongly spare allies from splash).
if (w.house == attacker.house) continue;           // UNCHANGED -- own-house spare only
```

  **`apply_spread_damage` reconciliation:** the design-direction lists line 145 as an enmity site, but the recovered §4 behavior is that **splash still hurts allies** — only the attacker's OWN house is spared. Keeping `w.house == attacker.house` (own-house spare) is the faithful behavior; routing it through `is_enemy` would incorrectly protect allies. **This site is therefore documented-and-left-unchanged** (the routing applies to the auto-acquire/direct-hit sites, not area splash). Note this deviation in the commit + Task 9 README.

- Produces — `engine/world.cpp`: route each DIRECT-HIT / targeting / capture enmity site through `is_enemy`/`are_allied`. Each shows before → after:

| Site (world.cpp line) | Before | After |
|---|---|---|
| nuke building-splash **1106** | `if (b.house == house) continue;` | **UNCHANGED — `if (b.house == house) continue;`** (controller RE-GATE amendment). A nuke is location-based AREA damage: per recovered §4 splash/area damage STILL hurts allies (and neutral), sparing ONLY the firing house — exactly like the unit splash `apply_spread_damage` (combat.cpp:145), which is also kept unchanged. Do NOT route this through `is_enemy` (that would wrongly spare allies + neutral from the blast). Byte-identical for the gates (unconfigured → `is_enemy` reduces to `a!=b`, so `!is_enemy(house,b.house)` == `b.house==house`); the two forms diverge ONLY once alliances are configured, where "spare own only" is the faithful one. Keep both splash sites consistent. |
| `capture_building` **1155** | `if (new_house == old_house) return false;` | UNCHANGED — a same-owner ChangeOwner no-op, not an enmity test. Left as-is. |
| `infiltrate_building` **1187** | `if (victim == spy_house) return false;` | UNCHANGED — self-spy no-op (identity, not enmity). Left as-is. |
| `repair_building` **1220** | `if (b.house != by_house) return false;` | route to allies: `if (!are_allied(by_house, b.house)) return false;` — you may repair OWN and ALLIED buildings (self-ally holds, so own still repairs). |
| `step_capture` arrival **1352** | `if (b.house != eng_house) { ...capture... } else { ...repair... }` | `if (is_enemy(eng_house, b.house) || (!are_allied(eng_house, b.house) && b.house != eng_house)) { capture } else { repair }` — capture ENEMY **and NEUTRAL** buildings (neutral is not an enemy but IS capturable — the oil-derrick case); repair OWN/ALLIED. Simplify: `const bool own_or_ally = are_allied(eng_house, b.house); if (!own_or_ally) capture; else repair;` (neutral is not allied → captured; own/ally → repaired). |
| disguise detect **1487** | `if (!s.disguised || !s.alive || s.house == d.house) continue;` | `if (!s.disguised || !s.alive || !is_enemy(d.house, s.house)) continue;` — a disguised spy is detected only by an ENEMY observer (own/ally/neutral don't "detect" as a threat). |
| `ai_quarry_exists` **1971** | `u.house != house && quarry_unit_match(...)` | `is_enemy(house, u.house) && quarry_unit_match(...)` |
| `ai_quarry_exists` **1975** | `b.house != house` | `is_enemy(house, b.house)` |
| `ai_pick_quarry_unit` **1991** | `u.house == house` (skip own) | `!is_enemy(house, u.house)` (skip self + ally + neutral) |
| `ai_pick_quarry_building` **2010** | `b.house == house` (skip own) | `!is_enemy(house, b.house)` |
| `ai_order_attack` **2058** | `tgt.house == u.house` (guard) | `!is_enemy(u.house, tgt.house)` — AI never orders an attack on self/ally/neutral. |
| `ai_order_attack_building` **2069** | `tgt.house == u.house` (guard) | `!is_enemy(u.house, tgt.house)` |

  **`valid_target` is the linchpin** (combat.cpp:320 stale-drop + 371 auto-acquire scan + explicit-order path all call it). Auto-acquire + stale-drop MUST become alliance-aware (skip allies/neutral). **Explicit orders MUST stay permissive** — force-fire on an ally/own target bypasses the relationship gate (spec §Behavior). Confirm the explicit `order_attack` path does NOT run through `valid_target`'s enmity clause for a player-issued target (if it does, add a `force` bypass param defaulting false for auto-acquire, true for explicit orders); if the current explicit-order path already skips `valid_target` (issues the target directly), no change is needed — verify at implement time and do NOT over-restrict.

**Byte-identity:** with `houses_configured_ == false` (every gate), `is_enemy(a,b) == (a != b)` and `are_allied(a,b) == (a == b)` — exactly today. `is_enemy(house, x) ≡ (house != x)` at nuke-splash/AI sites reproduces the old `== house`/`!= house` branches. At `step_capture` arrival, `!are_allied(eng, b.house) ≡ (eng != b.house)` reproduces `b.house != eng_house`. At `repair_building`, `!are_allied(by, b.house) ≡ (by != b.house)` reproduces `b.house != by_house`. At disguise, `!is_enemy(d.house, s.house) ≡ (d.house == s.house)` reproduces `s.house == d.house`. All byte-identical. `world_hash` NOT touched.

**Steps:**

- [ ] **Step 1: Write the failing truth-table test.** Append to `tests/test_world.cpp`:

```cpp
TEST_CASE("World multihouse: is_enemy/are_allied truth table [RE: multihouse.md §3/§4]") {
    ra2engine::World world = make_test_world();
    // (a) Default (houses_configured_ == false): is_enemy == a!=b, are_allied == a==b.
    for (int a = 0; a < 3; ++a)
        for (int b = 0; b < 3; ++b) {
            CHECK(world.is_enemy(a, b)   == (a != b));   // the byte-identity contract
            CHECK(world.are_allied(a, b) == (a == b));
        }
    // (b) Configured FFA: every house its own team.
    world.set_house(0, {ra2engine::World::Side::Allied, 5, "", 0, true});
    world.set_house(1, {ra2engine::World::Side::Soviet, 1, "", 1, true});
    world.set_house(2, {ra2engine::World::Side::Neutral, 3, "", 2, false});
    CHECK(world.is_enemy(0, 1));                          // cross-team combatants
    CHECK(world.is_enemy(1, 0));
    CHECK_FALSE(world.is_enemy(0, 0));                    // self never enemy
    CHECK_FALSE(world.are_allied(0, 1));
    // (c) Neutral / non-combatant: never enemy of anyone, both directions.
    CHECK_FALSE(world.is_enemy(0, 2));
    CHECK_FALSE(world.is_enemy(2, 0));
    CHECK_FALSE(world.is_enemy(1, 2));
    CHECK_FALSE(world.are_allied(0, 2));                  // not allied to a combatant
    CHECK(world.are_allied(2, 2));                        // self-ally still holds
    // (d) Configured team: 0+3 allied vs 1.
    world.set_house(3, {ra2engine::World::Side::Soviet, 6, "", 0, true});   // team 0 with house 0
    world.set_house(0, {ra2engine::World::Side::Allied, 5, "", 0, true});   // ensure team 0
    CHECK(world.are_allied(0, 3));                        // teammates
    CHECK_FALSE(world.is_enemy(0, 3));                    // allies never fight
    CHECK(world.is_enemy(0, 1));                          // cross-team enemy
    CHECK(world.is_enemy(3, 1));
}
```

- [ ] **Step 2: Run to verify failure FOREGROUND, blocking (timeout 600000 ms).**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
D:\ra2-android\build\ra2assets_tests.exe --test-case="*truth table*"
```

Expected: compile FAILS (`is_enemy`/`are_allied` not members).

- [ ] **Step 3: Land the predicates + route the world.cpp sites + the combat.cpp `valid_target` threading** exactly as in Interfaces (keep `apply_spread_damage` own-house spare UNCHANGED; keep explicit-order permissiveness). Update every `valid_target(units, ...)` caller (combat.cpp:320/371) to pass the `World&`.

- [ ] **Step 4: Build + run FOREGROUND, blocking (timeout 600000 ms), expected PASS:**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests ra2engine
D:\ra2-android\build\ra2assets_tests.exe --test-case="*truth table*"
D:\ra2-android\build\ra2assets_tests.exe
```

Expected: both targets build; matched case passes; full suite 0 failed.

- [ ] **Step 5: combat / tank byte-identical re-verify FIRST FOREGROUND, blocking (timeout 600000 ms EACH; ONE per call).** The predicate is now on the hot auto-acquire path — re-verify the TWO combat-math gates FIRST:

```powershell
cmake --build D:\ra2-android\build --target ra2engine
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2engine.exe --demo-combat
D:\ra2-android\build\ra2engine.exe --demo-tank
```

Expected: `deterministic=yes logs=78` and `deterministic=yes logs=144` EXACT. If either drifts, `is_enemy(a,b)` is not returning `(a != b)` with the firewall off (most likely `houses_configured_` got set, or a site's before→after is not equivalent to its old branch) — diagnose with systematic-debugging.

- [ ] **Step 6: FULL 15-gate re-verify FOREGROUND, blocking (timeout 600000 ms EACH; ONE per call).** Every enmity site was touched (including the AI quarry/attack sites → `--demo-ai-base`/`--demo-ai-team`/`--demo-ai-trigger`, and the capture/repair/disguise sites → `--demo-capture`). Run the full **15-gate re-verify command block** (Global Constraints). ANY drift = a non-equivalent before→after at some site. Fix before proceeding.

- [ ] **Step 7: Commit.**

```powershell
git -C "D:\ra2-android" add engine/world.hpp engine/world.cpp engine/combat.cpp tests/test_world.cpp
git -C "D:\ra2-android" commit -m "$(printf 'feat(engine): is_enemy/are_allied predicates + route all enmity sites (valid_target, nuke splash, repair/step_capture own-vs-enemy, disguise detect, AI quarry/attack) through them; splash keeps own-house-only spare (allies still hurt); default is_enemy==a!=b -> 15 gates byte-identical [RE: multihouse.md §3/§4]\n\nCo-Authored-By: Claude Fable 5 <noreply@anthropic.com>')"
```

---

### Task 5: AllyReveal shared visibility (firewalled; shroud gate re-verify)

**Files:**
- Modify: `engine/world.hpp` (`ally_reveal_` bool + `enable_ally_reveal` decl), `engine/world.cpp` (the OR-merge at the end of `step_visibility`)
- Test: `tests/test_world.cpp` (append)

**Interfaces:**
- Consumes: `houses_configured_`/`are_allied`/`house_count` (Tasks 2/4), the per-house `explored_`/`visible_` vectors + `step_visibility` (Task 3), the `shroud_any_` firewall.
- Produces — `engine/world.hpp` (in the Multi-house block):

```cpp
    // AllyReveal [RE: multihouse.md §3, D4]: allied houses share revealed shroud.
    // Firewalled behind houses_configured_ + shroud_any_ -> OFF in every gate.
    // Default true (matches rules AllyReveal=yes) but only ACTIVE when alliances
    // are configured AND shroud is enabled. NOT hashed.
    void enable_ally_reveal(bool on) { ally_reveal_ = on; }
    bool ally_reveal_ = true;
```

- Produces — `engine/world.cpp`: at the END of `step_visibility` (after the `seen_by_mask` stamp at 1571 and the SpySat reveal-all at 1574-1575), OR-merge allied houses' grids. Snapshot the source grids first so the merge is order-independent (deterministic):

```cpp
    // AllyReveal [RE: multihouse.md §3, D4]: OR-merge allied houses' explored_/
    // visible_ (shroud sharing) AFTER per-house sight + seen_by_mask are computed,
    // so targeting still uses each house's OWN sight (seen_by_mask), while
    // rendering/reveal reflects shared sight. Firewalled: OFF unless alliances are
    // configured AND AllyReveal is on. Snapshot-then-merge = order-independent.
    if (houses_configured_ && ally_reveal_) {
        const int n = house_count();
        const std::vector<std::vector<std::uint8_t>> exp_src = explored_;   // snapshot
        const std::vector<std::vector<std::uint8_t>> vis_src = visible_;
        for (int a = 0; a < n; ++a) {
            if (a >= (int)explored_.size() || explored_[a].empty()) continue;
            for (int b = 0; b < n; ++b) {
                if (a == b || !are_allied(a, b)) continue;
                if (b >= (int)exp_src.size() || exp_src[b].size() != explored_[a].size()) continue;
                for (size_t i = 0; i < explored_[a].size(); ++i) {
                    explored_[a][i] |= exp_src[b][i];
                    if (!visible_[a].empty() && vis_src[b].size() == visible_[a].size())
                        visible_[a][i] |= vis_src[b][i];
                }
            }
        }
    }
```

**Inert rule:** the merge is guarded by `houses_configured_` (false in every gate) AND is inside `step_visibility` (already firewalled by `shroud_any_`, false in every gate). The shroud gate (`--demo-shroud`) is FFA / no alliances (`houses_configured_ == false`), so the merge never runs and `shroud_hash` is unchanged. `world_hash` NOT touched.

**Steps:**

- [ ] **Step 1: Write the failing test.** Append to `tests/test_world.cpp`:

```cpp
TEST_CASE("World multihouse: AllyReveal shares explored/visible between allies [RE: multihouse.md §3]") {
    ra2engine::World world = make_test_world();               // >= 16x16 hand-built
    // Houses 0 and 1 allied (team 0); house 1 has no vision source of its own here.
    world.set_house(0, {ra2engine::World::Side::Allied, 5, "", 0, true});
    world.set_house(1, {ra2engine::World::Side::Allied, 6, "", 0, true});   // ally of 0
    world.set_house(2, {ra2engine::World::Side::Soviet, 1, "", 2, true});   // enemy
    world.enable_shroud(true);
    world.enable_ally_reveal(true);
    ra2engine::UnitCombat c; c.image = "E1"; c.strength = 100; c.armor = "none"; c.sight = 5;
    world.add_combat_unit(8, 8, /*house=*/0, c);              // only house 0 has a scout
    world.step_visibility();
    CHECK(world.visible_at(0, 8, 8));                         // house 0 sees its scout
    CHECK(world.visible_at(1, 8, 8));                         // ally house 1 SHARES it
    CHECK(world.explored_at(1, 8, 8));
    CHECK_FALSE(world.visible_at(2, 8, 8));                   // enemy does NOT share
}

TEST_CASE("World multihouse: AllyReveal inert when unconfigured (shroud gate contract) [RE: multihouse.md §3]") {
    ra2engine::World world = make_test_world();
    world.enable_shroud(true);                                // shroud on, alliances NOT configured
    ra2engine::UnitCombat c; c.image = "E1"; c.strength = 100; c.armor = "none"; c.sight = 5;
    world.add_combat_unit(8, 8, /*house=*/0, c);
    world.step_visibility();
    CHECK(world.visible_at(0, 8, 8));
    CHECK_FALSE(world.visible_at(1, 8, 8));                   // no sharing (houses_configured_ false)
}
```

- [ ] **Step 2: Run to verify failure FOREGROUND, blocking (timeout 600000 ms).**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
D:\ra2-android\build\ra2assets_tests.exe --test-case="*AllyReveal*"
```

Expected: FAIL (`enable_ally_reveal` missing, or the ally does not share yet).

- [ ] **Step 3: Land `ally_reveal_`/`enable_ally_reveal` + the snapshot-merge in `step_visibility`** exactly as in Interfaces.

- [ ] **Step 4: Build + run FOREGROUND, blocking (timeout 600000 ms), expected PASS:**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests ra2engine
D:\ra2-android\build\ra2assets_tests.exe --test-case="*AllyReveal*"
D:\ra2-android\build\ra2assets_tests.exe --test-case="*shroud*"
D:\ra2-android\build\ra2assets_tests.exe
```

Expected: both targets build; matched cases pass; full suite 0 failed.

- [ ] **Step 5: shroud gate re-verify FOREGROUND, blocking (timeout 600000 ms; ONE per call).** The merge lives inside `step_visibility` guarded by `houses_configured_` — the shroud gate never configures alliances, so it must be a strict no-op:

```powershell
cmake --build D:\ra2-android\build --target ra2engine
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2engine.exe --demo-shroud
```

Expected: `shroud_hash=d302852aa9517661` EXACT. If it drifts, the merge is running with `houses_configured_ == false` (the guard is wrong) — fix. Then run the remaining fourteen pins from the **15-gate re-verify command block** (a step_visibility edit could in principle touch a hashed path — prove all fifteen). ANY drift = defect.

- [ ] **Step 6: Commit.**

```powershell
git -C "D:\ra2-android" add engine/world.hpp engine/world.cpp tests/test_world.cpp
git -C "D:\ra2-android" commit -m "$(printf 'feat(engine): AllyReveal shared visibility - snapshot-then-OR-merge allied houses explored_/visible_ at the end of step_visibility (deterministic, order-independent); firewalled behind houses_configured_ + shroud_any_ -> shroud gate byte-identical; 15 gates re-verified [RE: multihouse.md §3]\n\nCo-Authored-By: Claude Fable 5 <noreply@anthropic.com>')"
```

---

### Task 6: Victory / defeat last-team-standing (firewalled victory_enabled_)

**Files:**
- Modify: `engine/world.hpp` (`victory_enabled_`/`short_game_`/`house_defeated_`/`winning_team_` members + `enable_victory`/`house_defeated`/`winning_team`/`step_victory` decls), `engine/world.cpp` (bodies + the `World::step` insertion)
- Test: `tests/test_world.cpp` (append)

**Interfaces:**
- Consumes: `house_count`/`house_combatant`/`house_team` (Task 2), `buildings_`/`units_` alive state, the `World::step` pass tail (world.cpp end of `step`, after `step_combat` at 2630 + death resolution), the `step_capture` firewall precedent.
- Produces — `engine/world.hpp` (public + private, in the Multi-house block):

```cpp
    // --- Victory / defeat [RE: multihouse.md §5, D5] ---
    // Enable the greenfield last-team-standing victory pass. short_game=true
    // (ShipGame default) -> a combatant house is defeated when its last building
    // falls; false -> zero buildings AND zero units. Default OFF -> step_victory
    // early-returns -> gates run their fixed frame counts. NOT hashed.
    void enable_victory(bool on, bool short_game = true);
    bool house_defeated(int h) const;   // sticky; false if h out of range / neutral
    int  winning_team() const;          // the sole surviving combatant team, or -1
    void step_victory();                // firewalled per-frame pass (public for the gate)
```

```cpp
    // Victory state (private). house_defeated_ sized house_count(); sticky.
    bool victory_enabled_ = false;
    bool short_game_      = true;
    std::vector<std::uint8_t> house_defeated_;
    int  winning_team_    = -1;
```

- Produces — `engine/world.cpp` (bodies):

```cpp
void World::enable_victory(bool on, bool short_game) {
    victory_enabled_ = on;
    short_game_      = short_game;
    if (on) { house_defeated_.assign(house_count(), 0); winning_team_ = -1; }
}

bool World::house_defeated(int h) const {
    return h >= 0 && h < (int)house_defeated_.size() && house_defeated_[h] != 0;
}

int World::winning_team() const { return winning_team_; }

void World::step_victory() {
    if (!victory_enabled_) return;                           // firewall (spec §Global)
    const int n = house_count();
    if ((int)house_defeated_.size() != n) house_defeated_.resize(n, 0);
    // Count alive buildings/units per house.
    std::vector<int> blds(n, 0), unts(n, 0);
    for (const Building& b : buildings_)
        if (b.alive && b.state != Building::State::Dead &&
            b.house >= 0 && b.house < n) ++blds[b.house];
    for (const Unit& u : units_)
        if (u.alive && u.die_time < 0.0 && u.house >= 0 && u.house < n) ++unts[u.house];
    // Mark defeated combatant houses (sticky). Neutral never defeated.
    for (int h = 0; h < n; ++h) {
        if (!house_combatant(h) || house_defeated_[h]) continue;
        const bool defeated = short_game_ ? (blds[h] == 0)
                                          : (blds[h] == 0 && unts[h] == 0);
        if (defeated) house_defeated_[h] = 1;
    }
    // WIN = exactly one alliance-team of combatant houses survives (last-team-
    // standing). While >= 2 combatant teams survive -> -1. Neutral never counts.
    int surviving_team = -1; bool multiple = false;
    for (int h = 0; h < n; ++h) {
        if (!house_combatant(h) || house_defeated_[h]) continue;
        const int t = house_team(h);
        if (surviving_team == -1) surviving_team = t;
        else if (t != surviving_team) { multiple = true; break; }
    }
    winning_team_ = (!multiple && surviving_team != -1) ? surviving_team : -1;
}
```

- Produces — `engine/world.cpp`: the `World::step` insertion — as the LAST statement in `step()` (after `step_combat` at 2630 and its death/MoveRequest resolution, so defeats reflect this frame's deaths), mirroring the `step_capture` firewall:

```cpp
    // Multi-house [RE: multihouse.md §5]: evaluate last-team-standing victory
    // AFTER combat has resolved this frame's deaths. Firewalled behind
    // victory_enabled_ (false in every existing gate -> a strict no-op).
    step_victory();
```

**Byte-identity:** default `victory_enabled_ == false` → `step_victory` early-returns → the gates run their fixed frame counts exactly as today. No new state feeds `world_hash`. `world_hash` NOT touched.

**Steps:**

- [ ] **Step 1: Write the failing test.** Append to `tests/test_world.cpp`:

```cpp
TEST_CASE("World multihouse: last-team-standing victory (ShortGame) [RE: multihouse.md §5]") {
    ra2engine::World world = make_test_world();
    // Teams: 0+2 (team 0) vs 1 (team 1); house 3 neutral non-combatant.
    world.set_house(0, {ra2engine::World::Side::Allied,  5, "", 0, true});
    world.set_house(1, {ra2engine::World::Side::Soviet,  1, "", 1, true});
    world.set_house(2, {ra2engine::World::Side::Allied,  6, "", 0, true});
    world.set_house(3, {ra2engine::World::Side::Neutral, 3, "", 3, false});
    world.enable_victory(true, /*short_game=*/true);
    // Give each combatant a building (type 0 -> a ConYard-ish structure) + neutral one.
    world.add_building(0, 2,  2,  1,1, /*house=*/0, 100, 0, 0.0, true);
    world.add_building(0, 20, 2,  1,1, /*house=*/1, 100, 0, 0.0, true);
    world.add_building(0, 2,  20, 1,1, /*house=*/2, 100, 0, 0.0, true);
    world.add_building(0, 20, 20, 1,1, /*house=*/3, 100, 0, 0.0, true);
    world.step_victory();
    CHECK_FALSE(world.house_defeated(0));
    CHECK_FALSE(world.house_defeated(1));
    CHECK(world.winning_team() == -1);            // 2 combatant teams alive
    CHECK_FALSE(world.house_defeated(3));          // neutral never defeated
    // Destroy house 1's only building -> team 1 defeated -> team 0 wins.
    for (auto& b : world.buildings_mut())          // (adapt: a test hook / direct kill)
        if (b.house == 1) { b.alive = false; b.state = ra2engine::Building::State::Dead; }
    world.step_victory();
    CHECK(world.house_defeated(1));
    CHECK_FALSE(world.house_defeated(0));
    CHECK_FALSE(world.house_defeated(2));
    CHECK(world.winning_team() == 0);             // last team standing = team 0
}
```

  (Adapt the building-kill to the file's existing test hooks — if there is no `buildings_mut()`, use the same kill path other `test_world.cpp` cases use, e.g. applying lethal damage then a `step`, or a `kill_building_for_test` helper. The load-bearing asserts are `house_defeated`/`winning_team`.)

- [ ] **Step 2: Run to verify failure FOREGROUND, blocking (timeout 600000 ms).**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
D:\ra2-android\build\ra2assets_tests.exe --test-case="*last-team-standing*"
```

Expected: compile FAILS (`enable_victory`/`house_defeated`/`winning_team`/`step_victory` missing).

- [ ] **Step 3: Land the victory members + bodies + the `World::step` insertion** exactly as in Interfaces.

- [ ] **Step 4: Build + run FOREGROUND, blocking (timeout 600000 ms), expected PASS:**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests ra2engine
D:\ra2-android\build\ra2assets_tests.exe --test-case="*last-team-standing*"
D:\ra2-android\build\ra2assets_tests.exe
```

Expected: both targets build; matched case passes; full suite 0 failed.

- [ ] **Step 5: 15-gate re-verify FOREGROUND, blocking (timeout 600000 ms EACH; ONE per call).** `step_victory` is now in the `World::step` pass order — PROVE the `victory_enabled_` firewall makes it a strict no-op. Run the full **15-gate re-verify command block** (Global Constraints). ANY drift = `step_victory` not firewalled (or `enable_victory` called somewhere it should not be). Fix before proceeding.

- [ ] **Step 6: Commit.**

```powershell
git -C "D:\ra2-android" add engine/world.hpp engine/world.cpp tests/test_world.cpp
git -C "D:\ra2-android" commit -m "$(printf 'feat(engine): last-team-standing victory/defeat - firewalled step_victory (victory_enabled_) marks combatant houses defeated (ShortGame=last-building) + resolves winning_team (sole surviving alliance-team); neutral never wins/loses; default OFF -> 15 gates byte-identical [RE: multihouse.md §5]\n\nCo-Authored-By: Claude Fable 5 <noreply@anthropic.com>')"
```

---

### Task 7: Per-house side/color rendering + spawn (gate re-verify: rendered + type-affecting)

**Files:**
- Modify: `engine/iso_renderer.cpp` (add `pal_for_color` beside `pal_for_house`), `engine/world.hpp`/`engine/world.cpp` (`house_palette` helper), `engine/main.cpp` (`spawn_base` building type via `house_side`; unit-side via `house_side`; render call sites via `house_palette`)
- Test: `tests/test_world.cpp` (append — spawn-type + palette default parity)

**Interfaces:**
- Consumes: `house_side`/`house_color`/`houses_configured` (Task 2), `pal_for_house` (world.cpp/iso_renderer.cpp:290), `spawn_base` (main.cpp:1783, building type at 1808), the unit-side branch (main.cpp:3072), the 8 MP-color HSV triples + the adjudicated HSV→RGB conversion (Task 1 item 1).
- Produces — `engine/iso_renderer.cpp` (new free function beside `pal_for_house`):

```cpp
// [RE-GATE APPLIED: multihouse.md §2 -- 8 MP colors HSV->remap spread]. Generate
// a house-color remap from one of the 8 MP color indices (0..7). Spreads the
// color's hue across the art's reserved house-color band (indices 16..31) like
// pal_for_house, at varying Value. color_ix out of 0..7 -> unit_pal unchanged.
// Render-only; NOT read by world_hash. HSV triples (RA2 hue 0..255):
//   0 Gold=43,239,255  1 DarkRed=0,230,255  2 Orange=25,230,255
//   3 Magenta=221,102,255  4 Purple=201,201,189  5 DarkBlue=153,214,212
//   6 DarkSky=131,200,230  7 DarkGreen=81,200,210
ra2assets::Palette pal_for_color(const ra2assets::Palette& unit_pal, int color_ix);
```

  (Implement the HSV→RGB conversion adjudicated in Task 1 item 1; spread Value/Saturation across indices 16..31 mirroring `pal_for_house`'s band loop. If Task 1 confirmed "cosmetic — any faithful conversion", use a standard `HSV(h∈0..255,s,v)→RGB` per band step.)

- Produces — `engine/world.hpp`/`engine/world.cpp` (a `World` helper the renderer calls — routes default vs configured):

```cpp
    // house_palette [RE: multihouse.md §2/D6]: the render entry. With
    // houses_configured_ FALSE -> pal_for_house(unit_pal, house) EXACTLY (today's
    // blue/red) -> byte-identical / pixel-identical. Configured -> the 8-MP-color
    // spread from house_color(house). Render-only; NOT hashed.
    ra2assets::Palette house_palette(const ra2assets::Palette& unit_pal, int house) const;
```

```cpp
ra2assets::Palette World::house_palette(const ra2assets::Palette& unit_pal, int house) const {
    if (!houses_configured_) return pal_for_house(unit_pal, house);   // today's derivation
    return pal_for_color(unit_pal, house_color(house));
}
```

- Produces — `engine/main.cpp` `spawn_base` (main.cpp:1808) — route the building TYPE (which feeds `world_hash`) through `house_side`:

```cpp
    // Multi-house [RE: multihouse.md §1/D6]: side drives the ConYard TYPE. Default
    // house_side(1)==Soviet, else Allied -> reproduces `house == 1 ? NACNST : GACNST`
    // -> the spawned Building{type} (hashed) is byte-identical in every gate.
    const bool soviet = (world.house_side(house) == ra2engine::World::Side::Soviet);
    place(roster_index(ba, soviet ? "NACNST" : "GACNST"), cx, cy, /*bypass=*/true);
```

  Apply the same `house_side`-based Soviet/Allied selection to EVERY side-branched id inside `spawn_base` (the full Soviet vs Allied base set), so a configured Soviet house spawns the Soviet set and an Allied house the Allied set. Defaults reproduce today exactly.

- Produces — `engine/main.cpp` unit-side (main.cpp:3072) — route through `house_side` (image name — render-only, NOT hashed, but keep consistent):

```cpp
    // Multi-house [RE: multihouse.md §1]: default house_side(1)==Soviet reproduces
    // `house == 1 ? "E2" : "E1"`. Image name is render-only (NOT in world_hash).
    return resolve_combat_unit(rules,
        world.house_side(house) == ra2engine::World::Side::Soviet ? "E2" : "E1").combat.image;
```

  (If no `World&` is in scope at 3072, keep the literal `house == 1` branch — it is byte-identical to the default `house_side` and image is unhashed; note it in the commit. Prefer routing where a `World&` is available.)

- Produces — `engine/main.cpp` render call sites: replace `pal_for_house(pal, u.house)` (and building equivalents) with `world.house_palette(pal, u.house)` where a `World&` is in scope. With `houses_configured_ == false` this returns `pal_for_house` exactly → pixel-identical.

**Byte-identity:** the spawned building **TYPES** (`Building{type}`, hashed) are unchanged because `house_side` defaults reproduce the house-1-is-Soviet branch. Colors/palette + unit image names are render-only and unhashed; `house_palette` returns `pal_for_house` verbatim with the firewall off. `world_hash` NOT touched.

**Steps:**

- [ ] **Step 1: Write the failing test.** Append to `tests/test_world.cpp` (host-only — exercises `house_side` defaults + a configured override; the actual spawn-type parity is covered empirically by the base/capture gates in Step 5):

```cpp
TEST_CASE("World multihouse: house_side drives spawn side; defaults reproduce today [RE: multihouse.md §1/D6]") {
    ra2engine::World world = make_test_world();
    // Default (firewall off): house 1 -> Soviet, else Allied (the hashed branch).
    CHECK(world.house_side(0) == ra2engine::World::Side::Allied);
    CHECK(world.house_side(1) == ra2engine::World::Side::Soviet);
    CHECK(world.house_side(2) == ra2engine::World::Side::Allied);
    // Configured: house 0 can be Soviet, house 1 Allied (fully table-driven).
    world.set_house(0, {ra2engine::World::Side::Soviet, 1, "Russians", 0, true});
    world.set_house(1, {ra2engine::World::Side::Allied, 5, "Americans", 1, true});
    CHECK(world.house_side(0) == ra2engine::World::Side::Soviet);
    CHECK(world.house_side(1) == ra2engine::World::Side::Allied);
}
```

- [ ] **Step 2: Run to verify failure/pass FOREGROUND, blocking (timeout 600000 ms).** (This test passes once Task 2 landed `house_side`; its role here is to lock the defaults before the spawn/render routing changes. If it already passes, proceed — the byte-identity proof is Step 5.)

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
D:\ra2-android\build\ra2assets_tests.exe --test-case="*house_side drives spawn*"
```

- [ ] **Step 3: Land `pal_for_color` + `house_palette` + route `spawn_base` types via `house_side` + the unit-side + render call sites** exactly as in Interfaces.

- [ ] **Step 4: Build + run FOREGROUND, blocking (timeout 600000 ms), expected PASS:**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests ra2engine
D:\ra2-android\build\ra2assets_tests.exe --test-case="*house_side drives spawn*"
D:\ra2-android\build\ra2assets_tests.exe
```

Expected: both targets build; matched case passes; full suite 0 failed.

- [ ] **Step 5: 15-gate re-verify FOREGROUND, blocking (timeout 600000 ms EACH; ONE per call).** `spawn_base` now routes the ConYard/base TYPE through `house_side` — the byte-identity-critical change (`Building{type}` feeds `world_hash`). Run the full **15-gate re-verify command block** (Global Constraints), with special attention to the gates that spawn/capture bases and render: `--demo-base` (5273b5e71ca009e1, rendered + spawn_base types), `--demo-capture` (58b87b5bc63f7046, base + capture rendered), `--demo-superweapon` (189496664f5eca68, rendered), `--demo-techtree` (e73adae61ff6f71f, spawn_base + GATECH types), `--demo-ai-base` (cd1f7c6a29933f0a). ANY drift = `house_side` default not reproducing the house-1-is-Soviet branch (a mis-mapped default), OR a render-path signature change touched a hashed path. Fix before proceeding.

- [ ] **Step 6: Commit.**

```powershell
git -C "D:\ra2-android" add engine/iso_renderer.cpp engine/world.hpp engine/world.cpp engine/main.cpp tests/test_world.cpp
git -C "D:\ra2-android" commit -m "$(printf 'feat(engine): per-house side/color - pal_for_color (8 MP-color HSV spread) + house_palette render entry; spawn_base ConYard/base TYPE + unit side via house_side; defaults reproduce house1=Soviet/red else Allied/blue so spawned Building{type} byte-identical; 15 gates re-verified [RE: multihouse.md §1/§2]\n\nCo-Authored-By: Claude Fable 5 <noreply@anthropic.com>')"
```

---

### Task 8: --demo-multihouse — the 16th permanent byte-identity gate

**Files:**
- Modify: `engine/main.cpp` (`multihouse_fold` + `DemoMultihouseResult` + `simulate_multihouse` + `run_demo_multihouse` + the `--demo-multihouse` dispatch)

**Interfaces:**
- Consumes: `fnv_mix`/`world_hash` (main.cpp — UNTOUCHED; `multihouse_fold` is a NEW accumulator), `build_world`/`capture_png`/`render_world`/`Camera`/`TextureCache`/`load_assets`/`env_install_dir` (the `run_demo_shroud` template, main.cpp), `World::{set_house,enable_shroud,enable_ally_reveal,enable_victory,step,step_visibility,step_victory,add_combat_unit,add_building,is_enemy,are_allied,house_defeated,winning_team,visible_at,explored_at,units,buildings,grid_w,grid_h}`.
- Produces — `engine/main.cpp`: `multihouse_fold` (folds per-house alive building+unit counts + a per-pair relationship sample + defeated flags + `winning_team`):

```cpp
// FNV fold of the multi-house state [spec §Testing]: for each house h -> alive
// building count, alive unit count, house_defeated flag; for each ordered pair
// (a,b) -> is_enemy(a,b) sampled; plus winning_team(). A NEW accumulator;
// world_hash + every prior *_hash UNTOUCHED. Folded at the demo's fixed checkpoints.
uint64_t multihouse_fold(uint64_t h, const ra2engine::World& world) {
    const int n = world.house_count();
    for (int hh = 0; hh < n; ++hh) {
        uint64_t nb = 0, nu = 0;
        for (const ra2engine::Building& b : world.buildings())
            if (b.alive && b.state != ra2engine::Building::State::Dead && b.house == hh) ++nb;
        for (const ra2engine::Unit& u : world.units())
            if (u.alive && u.die_time < 0.0 && u.house == hh) ++nu;
        h = fnv_mix(h, nb);
        h = fnv_mix(h, nu);
        h = fnv_mix(h, (uint64_t)(world.house_defeated(hh) ? 1 : 0));
    }
    for (int a = 0; a < n; ++a)
        for (int b = 0; b < n; ++b)
            h = fnv_mix(h, (uint64_t)(world.is_enemy(a, b) ? 1 : 0));
    h = fnv_mix(h, (uint64_t)(int64_t)world.winning_team());
    return h;
}

struct DemoMultihouseResult { std::vector<std::string> witness; uint64_t hash = 0; bool ok = true; };
```

- Produces — `engine/main.cpp`: `simulate_multihouse` (the `simulate_shroud` template). Scenario: **≥3 combatant houses in 2 alliance-teams (houses 0+2 allied on team 0 vs house 1 on team 1) + a neutral house (3) owning a structure.** Configure via `set_house`; enable shroud + AllyReveal + victory. Drive stages, folding at fixed checkpoints, recording the wedge bools:
  1. **Configure + spawn** (checkpoint A): `set_house` for 0/1/2/3; `enable_shroud(true)`, `enable_ally_reveal(true)`, `enable_victory(true, true)`. Spawn each combatant a ConYard-ish building + a combat unit; give the neutral (3) a structure. Position house-0 and house-2 units so an ally is in sight of a house-0 scout, and house-1 units within enemy range.
  2. **Allies-don't-fight / enemy-acquired** (checkpoint B): `step` several frames; assert a house-0 unit auto-acquires a house-1 (enemy) unit but NEVER a house-2 (ally) or house-3 (neutral) unit. Record `allies_dont_fight = (no house-0 unit ever targeted a house-2/house-3 unit)` and `enemy_acquired = (some house-0 unit targeted a house-1 unit)`.
  3. **Ally-reveal-shared** (checkpoint C): `step_visibility`; assert a cell revealed only by house 0's scout is `visible_at(2, ...)` (ally shares) but NOT `visible_at(1, ...)` (enemy). Record `ally_reveal_shared`.
  4. **Defeat-detected** (checkpoint D): destroy house 1's last building (apply lethal damage + `step` until Dead, or the file's kill hook); `step_victory`; assert `house_defeated(1)`. Record `defeat_detected`.
  5. **Team-wins** (checkpoint E): with team 1 eliminated and team 0 (houses 0+2) surviving, assert `winning_team() == 0` and neutral (3) never defeated / never wins. Record `team_wins`.
  Fold `h = multihouse_fold(h, world);` after each checkpoint; witness lines prefixed `[mh] ...` mirroring `[shroud] ...`. Keep the exact `fnv` basis `0xcbf29ce484222325ull`:

```cpp
DemoMultihouseResult simulate_multihouse(Assets& a, UnitAssets& ua,
                                         const std::vector<const VehicleAssets*>& vlist,
                                         bool render, SDL_Renderer* ren, ra2engine::TextureCache* tc,
                                         const ra2engine::Camera* cam, int W, int H,
                                         const std::string& start_png, const std::string& end_png) {
    DemoMultihouseResult r;
    ra2engine::World world = build_world(a, ua, 0);
    world.set_house(0, {ra2engine::World::Side::Allied,  5, "Americans", 0, true});
    world.set_house(1, {ra2engine::World::Side::Soviet,  1, "Russians",  1, true});
    world.set_house(2, {ra2engine::World::Side::Allied,  6, "British",   0, true});   // ally of 0
    world.set_house(3, {ra2engine::World::Side::Neutral, 3, "Civilian",  3, false});  // neutral
    world.enable_shroud(true);
    world.enable_ally_reveal(true);
    world.enable_victory(true, /*short_game=*/true);
    uint64_t h = 0xcbf29ce484222325ull;
    // (stages 1-6 in full: spawn, step loops, checkpoint folds, witness lines, wedge bools)
    r.hash = h;
    // Wedge guards [spec §Testing]: exit 1 unless each invariant holds.
    if (!allies_dont_fight)  { r.ok = false; r.witness.emplace_back("[mh] WEDGE: an allied/neutral unit was auto-acquired"); }
    if (!enemy_acquired)     { r.ok = false; r.witness.emplace_back("[mh] WEDGE: the enemy team was never auto-acquired"); }
    if (!ally_reveal_shared) { r.ok = false; r.witness.emplace_back("[mh] WEDGE: AllyReveal did not share the scout's sight"); }
    if (!defeat_detected)    { r.ok = false; r.witness.emplace_back("[mh] WEDGE: eliminated house was not marked defeated"); }
    if (!team_wins)          { r.ok = false; r.witness.emplace_back("[mh] WEDGE: last team standing did not win"); }
    char hb[64];
    std::snprintf(hb, sizeof hb, "[mh] multihouse_hash=%016llx", (unsigned long long)r.hash);
    r.witness.emplace_back(hb);
    if (render) { capture_png(ren, end_png, W, H); }
    return r;
}
```

  (Write the stage bodies out in FULL when implementing — no placeholder ellipses in committed code; each stage is a direct `World::` call sequence + a `multihouse_fold` checkpoint + a witness line, following `simulate_shroud` exactly. The five wedge bools are defined in the stage descriptions above.)

- Produces — `engine/main.cpp`: `run_demo_multihouse` (the `run_demo_shroud` template — rendered run + headless run from a FRESH `World`, byte-compare witness logs + hashes, **NO hardcoded hash literal**; tail line `deterministic=<y/n> lines=<N> multihouse_hash=<H>`; `return bad ? 1 : 0`).
- Produces — `engine/main.cpp`: the dispatch (append after the `--demo-shroud` arm):

```cpp
            } else if (arg == "--demo-multihouse") {
                return run_demo_multihouse();
```

**Steps:**

- [ ] **Step 1: Add `multihouse_fold` + `DemoMultihouseResult` + `simulate_multihouse` + `run_demo_multihouse` + the dispatch** exactly as in Interfaces (write the stage bodies in full — no ellipses).

- [ ] **Step 2: Build + run the new gate FOREGROUND, blocking (timeout 600000 ms; do NOT background, do NOT pipe through Select-Object):**

```powershell
cmake --build D:\ra2-android\build --target ra2engine
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2engine.exe --demo-multihouse
```

Expected: exit 0; tail line `deterministic=yes lines=<N> multihouse_hash=<H>`; witnesses show allies not fighting, the enemy acquired, ally-reveal sharing, house 1 defeated, and team 0 winning. NO wedge lines. If a wedge fires, diagnose with systematic-debugging — do NOT weaken the guard.

- [ ] **Step 3: Wedge-guard proof (negative check, FOREGROUND).** Temporarily break one stage so its invariant fails (e.g. skip destroying house 1's building so `defeat_detected`/`team_wins` stay false), rebuild, run — expected exit 1 with the matching `[mh] WEDGE: ...` line. REVERT, rebuild, re-run, expected exit 0. Do not commit the temporary edit.

- [ ] **Step 4: ALL SIXTEEN demos FOREGROUND, blocking (timeout 600000 ms each; ONE per call)** — the fifteen legacy pins byte-identical + the new gate deterministic. Run the **15-gate re-verify command block** (Global Constraints), then:

```powershell
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2engine.exe --demo-multihouse
```

Expected: fifteen legacy pins EXACT + `--demo-multihouse deterministic=yes` exit 0.

- [ ] **Step 5: Full suite FOREGROUND, blocking (timeout 600000 ms).** `D:\ra2-android\build\ra2assets_tests.exe` — expected 0 failed.

- [ ] **Step 6: Commit.**

```powershell
git -C "D:\ra2-android" add engine/main.cpp
git -C "D:\ra2-android" commit -m "$(printf 'feat(engine): --demo-multihouse gate (16th) - multihouse_fold (per-house building/unit counts + relationship sample + defeated flags + winning_team) + simulate_multihouse + run_demo_multihouse; 2 alliance-teams (0+2 vs 1) + neutral, allies-dont-fight/enemy-acquired/ally-reveal-shared/defeat-detected/team-wins wedge guards; two-run self-compare, NO pinned literal [RE: multihouse.md]\n\nCo-Authored-By: Claude Fable 5 <noreply@anthropic.com>')"
```

- [ ] **Step 7 (CONTROLLER, not the implementer): pin + eyeball.** Record the observed `multihouse_hash` (it goes into Task 9's README gate row — never a literal in code); eyeball `demo_multihouse_*.png` in the scratchpad (two teams' base-color palettes distinct, neutral structure present); confirm which demo art was present (the README documents any `[RE-FALLBACK]`).

---

### Task 9: Finalize — full suite both modes, sixteen gates, arm64 nm-verify, README

**Files:**
- Modify: `README.md`; `android/app/jni/CMakeLists.txt` ONLY if it lists sources beyond `cmake/ra2_sources.cmake` (verify — no new `.cpp` was added this phase, so nothing should be needed).

**Steps:**

- [ ] **Step 1: Full test suite, both modes, FOREGROUND, blocking (timeout 600000 ms):**

```powershell
cmake --build D:\ra2-android\build --target ra2assets_tests
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
D:\ra2-android\build\ra2assets_tests.exe
$env:RA2_INSTALL_DIR = "C:\nonexistent"; D:\ra2-android\build\ra2assets_tests.exe; $env:RA2_INSTALL_DIR = "D:\Red Alert 2"
```

Expected: 0 failed in both; the hand-built `World multihouse:*` cases run in BOTH modes (host-only, no install needed).

- [ ] **Step 2: ALL SIXTEEN demos FOREGROUND, blocking (timeout 600000 ms each; ONE per call)** — the fifteen legacy pins EXACT (values in Global Constraints) + `--demo-multihouse deterministic=yes` exit 0 with `multihouse_hash` equal to Task 8 Step 7's controller-pinned value. Run the **15-gate re-verify command block**, then `--demo-multihouse`.

- [ ] **Step 3: arm64 NDK build FOREGROUND, blocking (timeout 600000 ms):**

```powershell
$env:JAVA_HOME = "C:\Program Files\Java\jdk-21"
cd D:\ra2-android\android
.\gradlew.bat assembleDebug
```

Expected: `BUILD SUCCESSFUL`. Then nm-verify the new multi-house symbols in the arm64 `libmain.so`:

```powershell
& "D:\android-ndk-r27c\toolchains\llvm\prebuilt\windows-x86_64\bin\llvm-nm.exe" `
  (Get-ChildItem -Recurse -Filter libmain.so D:\ra2-android\android\app\build | Where-Object { $_.FullName -match "arm64" } | Select-Object -First 1).FullName `
  | Select-String -Pattern "is_enemy|are_allied|house_side|house_color|step_victory|set_house|house_count|winning_team|house_defeated|house_palette"
```

(The `Select-Object`/`Select-String` here filter an already-buffered file listing / bounded nm output — NOT the forbidden pattern of piping a LIVE native exe's stdout through `Select-Object -First N`; llvm-nm writes finite output and runs to completion.) Expected: at least one match for `is_enemy`, `are_allied`, `house_side`, `step_victory`, `set_house`, `house_count`.

- [ ] **Step 4: README — Multi-house / Alliances section.** Add a "Multi-house / Alliances" section after the Shroud / Fog-of-War section documenting: the `HouseRec` table (`houses_`) + `houses_configured_` firewall (side/color/enmity fall back to today's derivation when off); the pairwise `is_enemy`/`are_allied` predicate (`is_enemy(a,b)==(a!=b)` by default; neutral/non-combatant NEVER an enemy; self always ally; team = shared `alliance_team`, sentinel → own id = FFA) and the enmity sites it routes (auto-acquire `valid_target`, nuke splash, repair/capture own-vs-enemy, disguise detect, AI quarry/attack) — noting that **area splash still hurts allies** (only the attacker's own house is spared, faithful to RA2); AllyReveal shared visibility (snapshot-OR-merge of allied grids in `step_visibility`, firewalled); last-team-standing victory/defeat (`step_victory`/`victory_enabled_`, ShortGame=last-building, neutral never wins/loses, `winning_team`/`house_defeated`); per-house side/color (`house_side` drives spawn TYPES, `house_color`+`pal_for_color` the 8 MP colors, defaults reproduce house1=Soviet/red else Allied/blue); the six `[3]`→`std::vector` vectorization sized `house_count()`; the `seen_by_mask` `uint8_t` 8-combatant cap (MaxPlayers=8); and the byte-identity discipline (`world_hash` untouched; new state inert + excluded; `houses_configured_`/`victory_enabled_` firewalls; 15 gates re-verified at every touching commit and finalize). Update the "Determinism gates (permanent)" intro from "fifteen headless demos" to "sixteen".

Add the CLI bullet after the `--demo-shroud` bullet:

```
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
```

Add the gate row (hash = Task 8 Step 7's controller-pinned observation):

```
| `--demo-multihouse` | `multihouse_hash=<pinned value from Task 8 Step 7>` | `deterministic=yes`; N-player HouseRec table + alliances (is_enemy/are_allied, FFA default a!=b), AllyReveal shared sight, last-team-standing victory (ShortGame=last-building), per-house side/color (8 MP colors); wedge guards armed (`docs/re/multihouse.md`) |
```

Add to the "Deviations we take" pattern (each cited to multihouse.md):
  - **Enmity via a pairwise `is_enemy`/`are_allied` predicate, not `house != self_house`** — every inlined enmity site routes through the table-driven relationship; the default (`houses_configured_` off) returns `a != b`, exactly today [`multihouse.md` §3/§4; spec §Architecture C].
  - **Area/splash damage still hurts allies (only the attacker's own house is spared)** — `apply_spread_damage` keeps its `w.house == attacker.house` own-house spare rather than routing to `is_enemy`, faithful to RA2 friendly splash [`multihouse.md` §4].
  - **Neutral / non-combatant is a THIRD relationship (never an enemy, never auto-acquired, but still capturable)** — the engine gained the Neutral state it lacked; `step_capture` captures neutral (not-allied) buildings while repairing own/allied [`multihouse.md` §3].
  - **AllyReveal shares the shroud MAP, not targeting** — allied `explored_`/`visible_` grids OR-merge for render/reveal, but `seen_by_mask` (auto-acquire) still reflects each house's OWN sight [`multihouse.md` §3; spec §Architecture D].
  - **Last-team-standing victory is greenfield + firewalled** — `step_victory` runs only when `victory_enabled_`; ShortGame=last-building; neutral/non-combatant never win or lose [`multihouse.md` §5; spec §Architecture E].
  - **`seen_by_mask` stays `uint8_t` (8-combatant cap == MaxPlayers)** — not widened; neutral/non-combatant houses are excluded from the targeting bitmask [`multihouse.md` §1; spec §Architecture G].
  - **Per-country bonuses, the Yuri roster, in-game alliance-request UI, shared control/economy, AI diplomacy, AIDifficulty handicaps, and multi-human input are carry-forwards** — documented, not implemented this phase [`multihouse.md` §1/§3/§7; spec §Scope/non-goals].
  - **Multi-house state is LIVE but excluded from `world_hash`** — `houses_`/predicates/victory/vectorized arrays are hashed only by `multihouse_hash`; no existing demo configures alliances, so no existing hash changes [`multihouse.md` §1; spec §Data model].

- [ ] **Step 5: Commit.**

```powershell
git -C "D:\ra2-android" add README.md
git -C "D:\ra2-android" commit -m "$(printf 'docs: Multi-house / Alliances README - 16th gate row (--demo-multihouse), CLI bullet, deviations; fifteen->sixteen\n\nCo-Authored-By: Claude Fable 5 <noreply@anthropic.com>')"
```

(If Step 3's verify found `android/app/jni/CMakeLists.txt` needed a change, include it in this commit and say so in the message body.)

---

## Self-Review (performed at write time)

1. **Spec coverage:**
   - Spec §Calibration summary + §[RE-FALLBACK] items → **Task 1** re-pins countries / 8 colors / MaxPlayers / victory via `ra2extract` + OpenRA and adjudicates the four [RE-FALLBACK] items (8-color HSV→RGB / ShortGame defeat / neutral default relationship / FFA-default==a!=b), NOT a fresh hunt, + the controller RE-GATE amendment (Step 5).
   - Spec §Architecture A + §Data model (`Side`/`HouseRec`/`houses_`/`houses_configured_`, `house_side`/`house_color`/`house_count`/`set_house`, alliance_team sentinel→own id) → **Task 2**.
   - Spec §Architecture B + §Data model (six [3]→`std::vector` sized `house_count()`, widened guards + `take_income` bound) → **Task 3** (FIRST 15-gate re-verify).
   - Spec §Architecture C + §Behavior "Enmity rule" (§3/§4) → **Task 4** (`is_enemy`/`are_allied` + all enmity sites; combat=78/tank=144 + all 15 gates).
   - Spec §Architecture D + §Behavior "AllyReveal sharing" (§3) → **Task 5** (firewalled snapshot-merge; shroud gate re-verify).
   - Spec §Architecture E + §Behavior "Victory condition" (§5) → **Task 6** (firewalled `step_victory`, `house_defeated`/`winning_team`).
   - Spec §Architecture F + §Behavior "Side/color defaults" (§1/§2) → **Task 7** (`pal_for_color`/`house_palette` + `spawn_base`/unit-side via `house_side`; rendered + type-affecting gate re-verify).
   - Spec §Architecture G (seen_by_mask cap) → covered structurally in **Task 3** (seen_by_mask stamp bounded to 8) + documented in Task 9.
   - Spec §Architecture H + §Testing "New 16th gate" (§D8) → **Task 8** (two-run self-compare, NO pinned literal, five wedge guards, `multihouse_fold`).
   - Spec §Byte-identity promise + §Global Constraints → Global Constraints (copied verbatim, 15 hashes) + enforced structurally (`world_hash` untouched, `houses_configured_`/`victory_enabled_` firewalls, `is_enemy` default a!=b, spawn TYPE defaults, vectors size-3) + empirically (15-gate re-verify at Tasks 3/4/5/6/7 and the 16-demo runs at Tasks 8/9).
   - Spec §Scope/non-goals → each carry-forward listed in the Task 9 deviations; untouched by any task.
   - Spec §Testing pure-host cases → hand-built `test_world.cpp` cases (HouseRec defaults, is_enemy/are_allied truth table, vectorized parity, AllyReveal, victory); both-modes finalize sweep; arm64 assembleDebug + nm-verify.
2. **Placeholder scan:** every code step shows complete code; the ONE deliberate ellipsis is in Task 8's `simulate_multihouse` skeleton, explicitly flagged "write the stage bodies out in FULL when implementing — no placeholder ellipses in committed code" (each stage = a named `World::` call sequence + a `multihouse_fold` checkpoint + a witness line, with the five wedge-bool definitions spelled out in the stage descriptions). The `multihouse_hash` value is extract-then-pin by the controller (Task 8 Step 7 → Task 9), deliberately never a literal in code. Task 1's four items carry inline FALLBACKs marked `[RE-FALLBACK]`.
3. **Type consistency (checked against LIVE code):**
   - `World::Side World::house_side(int) const`, `int World::house_color(int) const`, `int World::house_count() const`, `int World::house_team(int) const`, `bool World::house_combatant(int) const`, `void World::set_house(int, HouseRec)` (Task 2) — used with these exact signatures by Tasks 3/4/5/6/7/8.
   - `bool World::is_enemy(int,int) const`, `bool World::are_allied(int,int) const` (Task 4) — member + the free-function threading into `combat.cpp`'s `valid_target(const std::vector<Unit>&, const World&, int, int)` (signature widened; all callers at combat.cpp:320/371 updated). Consistent.
   - `bool World::house_defeated(int) const`, `int World::winning_team() const`, `void World::step_victory()`, `void World::enable_victory(bool,bool)` (Task 6) — used by the gate (Task 8). `house_defeated_` (`std::vector<std::uint8_t>`) / `winning_team_` (`int`) / `short_game_` (`bool`) consistent.
   - The six vectorized members `std::vector<int> blackout_frames_`, `std::vector<std::uint8_t> produced_veteran_`, `std::vector<std::vector<std::uint8_t>> explored_`/`visible_`, `std::vector<std::uint8_t> map_reveal_`, `std::vector<double> house_credits_` (Task 3) — reconciled: `explored_`/`visible_` were ALREADY `std::vector<std::uint8_t>[3]` from the Shroud phase, so vectorizing = the OUTER `[3]`; `map_reveal_` was a `bool[3]` SpySat latch. `grow_house_arrays(int)` resizes all six + `house_defeated_`.
   - `ra2assets::Palette pal_for_color(const ra2assets::Palette&, int)` (iso_renderer.cpp) + `ra2assets::Palette World::house_palette(const ra2assets::Palette&, int) const` (Task 7) — `house_palette` delegates to `pal_for_house` (unchanged) when firewall off. Consistent.
   - `uint64_t multihouse_fold(uint64_t, const World&)` + `run_demo_multihouse()` (Task 8) — mirror `shroud_fold`/`run_demo_shroud`; `fnv_mix`/`world_hash` UNTOUCHED.
   - **`world_hash` UNTOUCHED** — no field it reads (`Unit{cell_x,cell_y,hp,alive}`, `Building{type,cx,cy,hp,state}`, main.cpp:4300) is added/reordered; new fields are appended; the ONLY hashed-input change is `spawn_base`'s Building TYPE selection (Task 7), whose default reproduces the exact house-1-is-Soviet branch → byte-identical. **`combat.cpp` scan arithmetic UNTOUCHED** — `valid_target` swaps ONE boolean (`v.house != self_house` → `world.is_enemy(self_house, v.house)`), equivalent when the firewall is off; `apply_spread_damage` is left unchanged.
4. **Line-number / type reconciliations found and fixed (folded into the tasks):**
   - **Accessor `< 3` guards shifted +1** from the design-direction (Shroud inserted a line): `house_credits()` 869 (not 868), `blackout_frames()` 872 (not 871), `produced_veteran()` 879 (not 878), `grant_credits_for_test()` 916 (not 915). Task 3 uses the live lines.
   - **`explored_`/`visible_` are ALREADY `std::vector<std::uint8_t>[3]`** (Shroud), and **`map_reveal_` is a `bool[3]` SpySat latch** (Shroud) — not plain scalars. Task 3 vectorizes the OUTER `[3]` for the two grids and converts the latch to `std::vector<std::uint8_t>`. Called out explicitly.
   - **`apply_spread_damage` (combat.cpp:145) is NOT routed through `is_enemy`** — the design-direction listed it as an enmity site, but recovered §4 says splash still hurts allies; only the attacker's own house is spared. Task 4 documents-and-keeps the `w.house == attacker.house` spare. This is the single deliberate departure from the design-direction's site list, justified by the calibration.
   - **`capture_building` (1155) and `infiltrate_building` (1187) guards are identity no-ops, not enmity tests** — left unchanged; the real own-vs-enemy capture decision is at `step_capture` arrival (1352), which IS routed (capture not-allied incl. neutral, repair own/allied). Reconciled in the Task 4 site table.
   - **`step_visibility` per-house loops** (world.cpp:1544/1547/1550/1569/1574) widen to `house_count()` in Task 3, EXCEPT the `seen_by_mask` stamp (1569) which stays bounded to 8 (the `uint8_t` cap, D7).
   - **`take_income` bound** confirmed at world.cpp:408 (`house > 1`); widened to `>= house_count()` with the byte-identity argument (neutral house 2 accrues no income → `take_income(2)` returns 0.0 either way).
   - All world.cpp enmity-site lines + combat.cpp `valid_target`/`apply_spread_damage` + `pal_for_house`/`spawn_base`/`world_hash` match the design-direction exactly (verified by grep) — the design-direction was authored against the post-Shroud tree.
5. **FOREGROUND discipline:** every build/test/demo step orders a foreground, synchronous, blocking run (up to 600000 ms), exes from PowerShell (never Git Bash), the sixteen gate demos run ONE per call (never a chain), and no step pipes a live exe through `Select-Object -First N` (the bounded llvm-nm pipeline is annotated as finite-output).

> **Known reconciliation flagged to the controller (NOT a stale placeholder):** (a) the design-direction lists `apply_spread_damage` (combat.cpp:145) among the enmity sites, but the recovered §4 behavior (splash hurts allies) means it is kept as an own-house-only spare and NOT routed through `is_enemy` — Task 4 documents this as the one intentional departure. (b) The Shroud phase shifted the four per-house accessor guard lines +1 and made `explored_`/`visible_` `std::vector<std::uint8_t>[3]` and `map_reveal_` a `bool[3]` — Task 3 vectorizes accordingly. Both surfaced in the Live-code reconciliation block + the Task 3/4 Interfaces.
