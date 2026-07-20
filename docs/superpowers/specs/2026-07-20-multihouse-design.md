# Multi-house / Alliances — Design Spec

**Status:** DRAFT (awaiting user review)
**Depends on:** the Capture phase (`docs/superpowers/specs/2026-07-19-capture-design.md` — provides `kNeutralHouse=2`, the per-house-array widening to 3, and the house 0=player / 1=enemy / 2=neutral convention) + the Shroud phase (`docs/superpowers/specs/2026-07-19-shroud-design.md` — provides the per-house `explored_`/`visible_` grids and `seen_by_mask` that this phase makes alliance-aware) + the base World/render/combat engine (`combat.cpp` auto-acquire scan, `pal_for_house`, `spawn_base`).
**Calibration authority:** `docs/re/multihouse.md` (the `rulesmd.ini` + OpenRA recovery hunt — ALREADY COMPLETE; real rules at `D:\ra2-re\out\shroud_evidence\rulesmd.ini`). This spec is built on recovered original-game behavior + the controller's binding design decisions (D1–D8), not design guesswork.

## Goal

Generalize the engine's hardwired 2–3 house world into a proper **N-player** model (up to 8 combatant houses + non-combatant houses) with **alliances / teams** and **last-team-standing victory**, calibrated to RA2/YR. Today the engine assumes exactly the fixed trio 0=player / 1=enemy / 2=neutral, treats `house != self_house` as the sole definition of "enemy" (so allies and neutrals would wrongly be hostile), derives side/color from bare house-id branches, sizes six per-house arrays as fixed `[3]`, and has no victory/defeat logic at all. This phase replaces those hardwired assumptions with:

- a **`HouseRec` table** (`houses_`) describing every house's side, color, country, alliance team and combatant flag, defaulted so today's world is reproduced exactly;
- a pairwise **alliance predicate** (`is_enemy` / `are_allied`) that all inlined enmity sites route through — auto-acquire, splash "enemies-only", capture/infiltrate/repair own-vs-enemy tests, and AI quarry/attack selection;
- **AllyReveal** shared visibility (allies see what their partners reveal) wired into the shroud layer;
- **last-team-standing victory/defeat** (ShortGame = last-building);
- **per-house side/color rendering + spawn** driven from the table (8 MP colors);
- the six fixed `[3]` per-house arrays **vectorized** to `house_count()`.

**Byte-identity promise.** Every generalization defaults to *exactly today's behavior*. Enmity falls back to `is_enemy(a,b) == (a != b)` unless alliances are explicitly configured; side/color fall back to today's `house 1 → Soviet/red, else Allied/blue` derivation; the vectors default to size 3 with indices 0/1/2 computed identically; new per-frame logic (AllyReveal sharing, victory checks) is firewalled behind `houses_configured_` / `victory_enabled_` bools that are OFF in every existing gate. `world_hash` is not touched. All 15 existing byte-identity gates stay bit-identical; the feature adds a 16th, `--demo-multihouse`.

## Global Constraints

Copied verbatim from the controller's design direction. These bind every task.

- All 15 existing byte-identity gates MUST stay bit-identical: combat logs=78, tank logs=144, base 5273b5e71ca009e1, econ 7fed96cb98b68017, prod f0f4e1207ecd713c, ai-base cd1f7c6a29933f0a, ai-team 312e92ed862a5e74, ai-trigger 8334a6ca1531cabc, transport fe500f946e913645, aircraft a69642564cf64773, naval 6aee2c82bc024e86, superweapon 189496664f5eca68, techtree e73adae61ff6f71f, capture 58b87b5bc63f7046, shroud d302852aa9517661. Adds the 16th: `--demo-multihouse`.
- `world_hash` MUST NOT be touched (folds only Unit{cell_x,cell_y,hp,alive}+Building{type,cx,cy,hp,state}; house is NOT folded — confirmed). Resizing per-house arrays cannot perturb any hash provided per-index values at 0/1/2 are computed identically.
- New per-frame logic (victory checks, AllyReveal sharing) is firewalled (`*_enabled_` bool → early return) — OFF in every existing gate.
- Enmity generalization defaults to EXACTLY today's behavior: `is_enemy(a,b) == (a != b)` unless alliances are explicitly configured. The gates never configure alliances → byte-identical.
- Demo gate = two-run compute-and-self-compare, NO pinned literal (README pins), wedge guards → exit 1.
- No assets/GPL/Ghidra raw output committed. Recovered CONSTANTS (country list, 8 colors, cash/UnitCount, victory rule) ARE committable.
- Commit trailer: `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`.

## Calibration summary (recovered ground truth — the behavior contract)

Every claim below is tagged as in `docs/re/multihouse.md` ([CONFIRMED] = real `rulesmd.ini`/decompile; [ANCHORED] = OpenRA/documented RA2, structure only; [RE-FALLBACK] = design choice). Task 1 re-adjudicates the [RE-FALLBACK] items (see the last section) before code.

### Player / house model + counts (§1)
- **14 country TYPE ids** over 3 combatant sides + non-combatant [CONFIRMED, `[Countries]` line 959 / `[Sides]` line 981]: Americans, Alliance, French, Germans, British (side `GDI` = **Allied**); Africans, Arabs, Confederation, Russians (side `Nod` = **Soviet**); YuriCountry (`ThirdSide` = **Yuri**); GDI, Nod (passive); Neutral (`Civilian`), Special (`Mutant`). The `GDI`/`Nod` strings are a Tiberian-Sun legacy naming; on-screen sides are Allied / Soviet / Yuri.
- A country provides **Side** (tech/art), **Prefix/Suffix** art tags, a **default Color**, `Multiplay=yes` (lobby-selectable) or `MultiplayPassive=true` (non-combatant), `SmartAI`. Per-country bonus/veteran multipliers are **COMMENTED OUT** in this rules revision → countries are mechanically near-identical (per-country bonuses = a CARRY-FORWARD). [CONFIRMED]
- A **house/slot** is a runtime instance binding a country (→ side/tech), a color, a start waypoint, a team/alliance id, human-vs-AI + handicap. Neutral/Special are `MultiplayPassive=true` — always instantiated, non-combatant, never win/lose, never auto-targeted. [CONFIRMED passive flag; slot model ANCHORED]
- **MaxPlayers = 8** — no `MaxPlayers` rules key; the 8 MP colors are the natural cap. [ANCHORED — 8]

### Team colors (§2)
- **8 selectable MP colors**, values are **HSV** (not RGB; rules comment line 3357) [CONFIRMED, `[Colors]` line 3367]: Gold(Yellow), DarkRed(Red), Orange, Magenta(Pink), Purple, DarkBlue(Blue), DarkSky(Periwinkle/Aqua), DarkGreen(Light green). Default per-slot color = the country's `Color=` (Allied→Gold, Soviet→DarkRed, civilian→Grey), overridden by the lobby pick; picks are unique per slot. [CONFIRMED defaults / ANCHORED lobby-unique]
- The HSV triple is a **remap-palette spread**: the engine tints the art's reserved house-color band to the owner's hue at blit time — the mechanism our `pal_for_house` implements. [ANCHORED]

### Alliance model (§3)
- Alliance is a **pairwise relationship: Ally / Enemy / Neutral** [ANCHORED, OpenRA `Player.cs:249`]. **Self is always Ally to self.** Alliances are **mutual** in settled play. **Neutral ≠ Enemy** — Civilian/Mutant (and unallied-but-non-hostile) houses are Neutral, never auto-attacked. This third state is what the engine currently lacks.
- Enmity gates on **relationship, not house identity**: a unit auto-acquires / auto-fires **only if the target is an ENEMY** — never self, ally, or neutral. Replace `house != self_house` with `RelationshipWith(target) == Enemy`. [ANCHORED, `AutoTarget.cs:342/420`]
- **FFA is the default**: `AlliesAllowed=no` ⇒ every player is enemy of every other; no pre-formed team. **Team games**: lobby-assigned teammates start **pre-allied** (mutual Ally), all other teams Enemy. `AllyChangeAllowed=yes` ⇒ in-game alliance requests permitted. [CONFIRMED defaults / ANCHORED team pre-ally]
- **`AllyReveal=yes`**: allied houses **share revealed shroud/sight** — a cell revealed by any house is revealed for its allies. [CONFIRMED line 751]
- Allies do **not** share unit control or economy (each keeps its own credits/queues/command). Force-fire bypasses the relationship gate; splash/area damage is location-based and **still hurts allies and own units**. [ANCHORED]

### Victory / defeat (§5)
- **`ShortGame=yes` (default)**: a house is **defeated when its last BUILDING is destroyed**; `ShortGame=no` ⇒ defeated only with **zero buildings AND zero units**. [CONFIRMED line 3039 + def; mechanism ANCHORED, `PlayerExtensions.cs:19`]
- **Win = every non-allied combatant house is defeated** (last-**TEAM**-standing — surviving allies all win). Neutral/Special never count toward victory or defeat. [ANCHORED, `ConquestVictoryConditions.cs:63`]

### Start setup (§6) + AI (§5/§7)
- Starting cash **10000** (slider 5000–10000), **UnitCount=10** starting units + a separate MCV at the start waypoint, TechLevel 10. [CONFIRMED values; MCV+units placement ANCHORED]
- House count = human slots + `AIPlayers` (≥1 for skirmish) + fixed Neutral + Special. `AIDifficulty` 0/1/2 selects a handicap band (human/AI handicapped independently). [CONFIRMED]

## Current-engine reality (what we build on)

From the survey — file:line anchors. These are the sim invariants and the policy branches this phase generalizes.

- **House convention:** `kNeutralHouse=2` (capture.hpp:15); convention doc world.hpp:1050; `Unit::house` default 0 (world.hpp:105). All gates use ≤3 houses, indices literally 0/1/2; only capture uses house 2 (neutral oil derrick).
- **SIX fixed `[3]` per-house arrays:** `blackout_frames_[3]` (world.hpp:1032), `produced_veteran_[3]` (1038), `explored_[3]` (1052), `visible_[3]` (1053), `map_reveal_[3]` (1060), `house_credits_[3]` (1116). Accessor `<3` guards: `house_credits()` 868, `blackout_frames()` 871, `produced_veteran()` 878, `grant_credits_for_test()` 915. `take_income` bound `house<0||house>1` (world.cpp:408). NONE feed `world_hash`.
- **Grow-on-demand precedent already exists:** `ai_houses_` (world.hpp:953, resize world.cpp:1008), `sw_houses_` (world.hpp:997, resize world.cpp:1041); `make_ai_base_view` already uses a dynamic `rival_counts` vector (world.cpp:1628-1633).
- **NO `is_enemy`/`are_allied` predicate.** Inlined enmity sites to route through it: `valid_target` combat.cpp:59-63 (`v.house != self_house` — THE linchpin: gates auto-acquire + stale-drop + explicit-order), `apply_spread_damage` combat.cpp:145 (`w.house==attacker.house continue`, enemies-only), nuke building-splash world.cpp:1106, `capture_building` 1155, `infiltrate_building` 1187, `repair_building` 1220, `step_capture` arrival 1352, disguise detect 1487, `ai_quarry_exists` 1971/1975, `ai_pick_quarry_unit` 1991, `ai_pick_quarry_building` 2010, `ai_order_attack` 2058, `ai_order_attack_building` 2069.
- **`seen_by_mask` is `uint8_t`** (world.hpp:245; combat.cpp:372 `1u<<u.house`; step_visibility write world.cpp:1570) → an 8-house bitmask cap (== MaxPlayers).
- **Player fixed at house 0** in the input/order layer (input.cpp; world.cpp order methods 431-765). **Enemy fixed at house 1** in demo/setup + side derivation: `spawn_base` `house==1?"NACNST":"GACNST"` (main.cpp:1808), unit `house==1?"E2":"E1"` (3072), enemy scans `h.house!=1` (3906/4050), `enable_ai_house(1,...)` (3000/5620/5883/6228). These are UI/scenario POLICY, not sim invariants — the gates pin 0=player/1=enemy/2=neutral, so leaving defaults keeps the gates unchanged.
- **`pal_for_house(unit_pal, house)`** (world.cpp:290-304): ONLY blue (house!=1) / red (house==1). Side is NOT stored per-house — derived from house id or roster GA/NA prefix.
- **NO victory/defeat logic anywhere** (greenfield); demos end on fixed frame counts.
- **`world_hash`** (main.cpp:4300-4316) folds NO house → confirmed hash-safe to resize the arrays.

## Architecture

Each component states what it does, its interface, its dependencies, and — critically — HOW it stays inert / byte-identical when alliances are not configured (which is every existing gate).

### A. HouseRec table + `houses_` vector + `houses_configured_` firewall (D1)

The single source of truth for every house (including player 0). New types + state on `World`:

```cpp
enum class Side { Allied, Soviet, Yuri, Neutral };
struct HouseRec {
    Side        side          = Side::Allied;
    int         color_ix      = 0;      // index into the 8 MP colors (or a default)
    std::string country       = "";
    int         alliance_team = -1;     // sentinel; resolved to own id on init (self-only = FFA)
    bool        combatant     = true;
};
std::vector<HouseRec> houses_;          // sized to house_count()
bool                  houses_configured_ = false;   // the firewall
```

Interface: `house_side(h)`, `house_color(h)`, `set_house(h, rec)`, `house_count()`, plus the alliance accessors (component C). `set_house` (or an `enable_multihouse`/configure entry) sets `houses_configured_ = true`.

**Defaults reproduce today EXACTLY** (used when `houses_` is unset/default *or* `houses_configured_` is false):
- house 0 = `{Allied, blue, "", team=0, combatant=true}`
- house 1 = `{Soviet, red, "", team=1, combatant=true}`
- house 2 = `{Neutral, blue (today's `pal_for_house` returns blue for `!=1`), "", team=2, combatant=false}`
- `alliance_team` sentinel `-1` resolves to **the house's own id** → every house is its own team → FFA.

**Inert rule:** `houses_configured_ == false` routes `house_side`/`house_color`/enmity through the CURRENT derivation (house 1 → Soviet/red, else Allied/blue; `is_enemy == a!=b`) → byte-identical. The table is never read by `world_hash`. Depends on: nothing beyond `World`.

### B. Vectorized six `[3]` → `std::vector` sized `house_count()` (D2)

Convert `blackout_frames_`, `produced_veteran_`, `explored_`, `visible_`, `map_reveal_`, `house_credits_` from `[3]` to `std::vector<>` sized `house_count()` (default 3 → indices 0/1/2 identical). Widen every accessor `<3` guard to `< (int)size()`. Widen `take_income` bound `house>1` → `>= house_count()`.

Interface: unchanged accessor signatures; the backing store is a vector allocated to `house_count()` at construction/configure.

**Inert rule:** with `house_count() == 3` the vectors are the same length as the old arrays, and per-index values at 0/1/2 are computed by identical code paths. NONE of the six feeds `world_hash`; the capture/shroud folds read **per-index** values which are unchanged. Depends on: `house_count()` (A). This is the FIRST task to re-verify all 15 gates (T3).

### C. Alliance predicate `is_enemy` / `are_allied` + route ALL enmity sites (D3)

Two `const` predicates on `World`:

```cpp
bool World::are_allied(int a, int b) const {
    if (a == b) return true;                       // self always allied to self
    if (!houses_configured_) return false;         // today: only same-house is "friendly"
    return combatant(a) && combatant(b) && team(a) == team(b);
}
bool World::is_enemy(int a, int b) const {
    if (!houses_configured_) return a != b;        // EXACTLY today's behavior
    if (a == b) return false;
    if (!combatant(a) || !combatant(b)) return false;   // neutral/non-combatant is NEVER an enemy
    return !are_allied(a, b);
}
```

Route EVERY inlined enmity site (the D-list in Current-engine reality) through these: `valid_target` (auto-acquire + stale-drop + explicit-order), `apply_spread_damage` "enemies-only", nuke building-splash, capture/infiltrate/repair own-vs-enemy, `step_capture` arrival, disguise detect, and all five AI quarry/attack sites become alliance-aware.

**Byte-identity:** with `houses_configured_ == false`, `is_enemy(a,b) == (a != b)` and `are_allied(a,b) == (a == b)` — exactly today. The gates never configure alliances; the neutral house owns no combat units, so its enmity is never exercised in the gates. Depends on: `houses_` (A).

### D. AllyReveal shared visibility — firewalled (D4)

When alliances are configured AND the shroud layer is enabled AND `AllyReveal` is on, allied houses share revealed terrain. After `step_visibility` computes each house's per-house `explored_`/`visible_` grids, deterministically OR-merge allied houses' grids into each other (equivalently, `visible_at`/`explored_at` consult allies). The merge is order-independent (an OR reduction over the ally set), so it is deterministic.

**Inert rule:** firewalled behind `houses_configured_` + the shroud `shroud_any_` flag → OFF in every gate. The shroud gate is FFA / no alliances, so no sharing occurs and `shroud d302852aa9517661` is unchanged. Depends on: the Shroud phase grids (`explored_`/`visible_`), `are_allied` (C).

### E. Victory / defeat last-team-standing — firewalled `victory_enabled_` (D5)

A greenfield, faithful victory pass gated by `bool victory_enabled_ = false`. A combatant house is **DEFEATED** when: ShortGame ⇒ zero alive buildings; else zero alive buildings AND zero alive units. **WIN** = exactly one alliance-team of combatant houses remains undefeated (last-team-standing); Neutral / non-combatant houses never win or lose. A firewalled `step_victory()` (inserted in the `World::step` pass order, mirroring `step_capture`) runs only when `victory_enabled_`.

Interface: `house_defeated(h)` → bool, `winning_team()` → int (-1 if none yet).

**Byte-identity:** default OFF → `step_victory` early-returns → the gates run their fixed frame counts exactly as today. No new state feeds `world_hash`. Depends on: `houses_` (A), the alive building/unit counts already tracked by `World`.

### F. Per-house side/color rendering + spawn (D6)

Route `pal_for_house` through `house_color(h)` (the 8 MP colors) and `spawn_base` / unit-side selection through `house_side(h)`. Add the 8-color HSV→remap palette generation (the extra 6 colors are used only when a house is explicitly assigned one).

**Defaults reproduce today:** `house_side(1) = Soviet`, `house_side(else) = Allied`; `spawn_base` building TYPES stay identical for the gates (house 1 → Soviet `NACNST`/`E2` set, else Allied `GACNST`/`E1`). `pal_for_house` color is unhashed, but defaults stay today's blue (`!=1`) / red (`==1`).

**Byte-identity:** the spawned building/unit **types** — which DO feed `world_hash` via `Building{type}` — are unchanged because `house_side` defaults reproduce the exact house-1-is-Soviet branch. Color/palette is render-only and unhashed. Depends on: `houses_` (A).

### G. `seen_by_mask` stays `uint8_t` — 8-combatant-house cap (D7)

Keep `seen_by_mask` as `uint8_t` (8-house bitmask == MaxPlayers). Do NOT widen (widening risks perturbing the shroud fold). Combatant house ids are 0..7; neutral/special houses (id ≥ 8 if present, or flagged non-combatant) are excluded from the targeting bitmask because they never auto-acquire and are never auto-acquired. For this phase the gate uses ≤ ~4 combatant houses + a neutral.

**Inert rule:** no representational change → no hash risk; documentation-only cap plus the combatant-flag exclusion. Depends on: the Shroud phase.

### H. `--demo-multihouse` gate (D8)

A new 16th gate. Scenario: ≥3 combatant houses configured into 2 alliance-teams (e.g. houses 0+2 allied vs house 1) plus a neutral house owning a structure. See the Testing strategy for the exercised behaviors, wedge guards, and fold. Two-run rendered+headless self-compare, NO pinned literal (README pins the hash), wedge guards → exit 1. `world_hash` + all 15 prior `*_hash` untouched.

**Inert rule:** a new gate adds no code to the existing gates' paths; enabling alliances is scoped to this gate only.

## Data model (exact field / struct additions)

| Field / type | On | Default | In `world_hash`? | Why inert by default |
|---|---|---|---|---|
| `enum class Side { Allied, Soviet, Yuri, Neutral }` | (new type) | — | No | A tag; only read via `house_side` behind `houses_configured_`. |
| `struct HouseRec { Side side; int color_ix; std::string country; int alliance_team; bool combatant; }` | (new type) | see below | No | Never folded; consulted only when configured. |
| `std::vector<HouseRec> houses_` | `World` | size `house_count()`; [0]={Allied,blue,team 0,comb}, [1]={Soviet,red,team 1,comb}, [2]={Neutral,blue,team 2,non-comb} | No | Defaults reproduce today's 0/1/2 world exactly. |
| `bool houses_configured_` | `World` | `false` | No | The enmity/side/color/AllyReveal firewall; false → current derivation. |
| `bool victory_enabled_` | `World` | `false` | No | `step_victory` early-returns → gates run fixed frame counts. |
| `blackout_frames_` `[3]`→`std::vector` | `World` | size 3 | No | Same length/values at 0/1/2; not folded. |
| `produced_veteran_` `[3]`→`std::vector` | `World` | size 3 | No | Same length/values at 0/1/2; not folded. |
| `explored_` `[3]`→`std::vector` | `World` | size 3 | No | Same length/values at 0/1/2; shroud fold reads per-index. |
| `visible_` `[3]`→`std::vector` | `World` | size 3 | No | Same length/values at 0/1/2; shroud fold reads per-index. |
| `map_reveal_` `[3]`→`std::vector` | `World` | size 3 | No | Same length/values at 0/1/2; not folded. |
| `house_credits_` `[3]`→`std::vector` | `World` | size 3 | No | Same length/values at 0/1/2; not folded (econ fold reads per-index). |
| `seen_by_mask` (unchanged `uint8_t`) | `Unit` | `0xFF` (from Shroud phase) | No | Kept 8-bit; NOT widened → no shroud-fold risk. |

`alliance_team` uses sentinel `-1` at construction, resolved to the house's own id on configure (self-only team = FFA default). All new state is excluded from `world_hash` because `world_hash` folds only `Unit{cell_x,cell_y,hp,alive}` + `Building{type,cx,cy,hp,state}` (main.cpp:4300-4316) — untouched. The six vectors are hash-safe because resizing them cannot perturb any hash (house is not folded) provided per-index values at 0/1/2 are computed identically, which the default size-3 path guarantees.

## Behavior details (each tied to the recovered §)

- **Enmity rule** (§3/§4, D3): `is_enemy(a,b)` is true iff `a != b`, both are combatant, and they are not on the same alliance team. Same-house → never enemy (self is Ally). Allied → never enemy. Neutral / non-combatant → **never** enemy (and never auto-acquires). Auto-acquire, splash "enemies-only", capture/infiltrate/repair own-vs-enemy, and AI quarry/attack all consult this. Force-fire (explicit order) is NOT gated by `is_enemy` — `valid_target`'s explicit-order path stays permissive; splash/area damage remains location-based and can hurt allies. [ANCHORED]
- **FFA vs team** (§3, D1/D3): default `alliance_team = own id` ⇒ FFA ⇒ `is_enemy(a,b) == (a != b)` among combatants (matches `AlliesAllowed=no`). A team game sets shared `alliance_team` ids so teammates are `are_allied` and never fight, all other teams Enemy. [CONFIRMED FFA default / ANCHORED team pre-ally]
- **AllyReveal sharing** (§3, D4): with alliances configured + shroud on + AllyReveal, a cell revealed by any house is revealed for its allies (deterministic OR-merge of allied `explored_`/`visible_`). [CONFIRMED `AllyReveal=yes`]
- **Victory condition** (§5, D5): a combatant house is defeated on last-building (ShortGame) or zero-buildings-and-units (full); a house/team wins when every non-allied combatant house is defeated (last-team-standing); Neutral/Special never win or lose. [CONFIRMED ShortGame / ANCHORED team-win]
- **Side / color defaults** (§1/§2, D6): `house_side` defaults house 1 → Soviet, else Allied (drives `spawn_base` building/unit types); `house_color` defaults today's blue (`!=1`) / red (`==1`); the 8 HSV MP colors are available for explicitly-assigned houses. [CONFIRMED colors / CONFIRMED default side mapping]
- **`seen_by_mask` cap** (§1/D7): combatant houses 0..7 fit the `uint8_t` targeting bitmask (== MaxPlayers 8); neutral/non-combatant houses are excluded from it (they never auto-acquire). Not widened. [ANCHORED MaxPlayers=8]

## Scope / non-goals (carry-forwards — document, do NOT implement)

Explicitly OUT of scope this phase (documented, not implemented):

| Carry-forward | Recovered ref | Why deferred |
|---|---|---|
| **Per-country bonuses / special units** (paradrop GIs, Tesla, Tank Destroyer, …; cost/income/veteran mults) | §1 [CONFIRMED mults OFF] / [ANCHORED special units] | Multipliers are commented out in this rules revision; belongs to the roster phase. |
| **Yuri side roster** (the `ThirdSide` unit/building set) + full 14-country wiring | §1 [CONFIRMED] | We have Allied/Soviet art only; `Side::Yuri` is defined but unpopulated. |
| **In-game alliance-request UI / `AllyChangeAllowed` runtime toggling** | §3 [CONFIRMED key] | Mechanism may be present but no UI; alliances configured programmatically. |
| **Shared control / shared economy / spectators** | §3 [ANCHORED] | Allies share only sight + no friendly auto-fire + combined victory this phase. |
| **AI diplomacy** (AI forming/breaking alliances) | §7 [ANCHORED] | AI honors `is_enemy`/`are_allied` for targeting, but does not form alliances. |
| **Multi-human input** (more than one local player) | §1 [ANCHORED] | Player stays house 0 in the input layer; multi-house is configured programmatically / by the future skirmish shell (Phase 3). |
| **AIDifficulty handicap tables** (Easy/Normal/Difficult ROF/Armor/Cost/BuildTime) | §7 [CONFIRMED] | No handicap-multiplier plumbing this phase. |
| **Shroud-phase T7 carry-forwards** (`reveal_all_house` ordering / SpySat-loss re-shroud) | shroud §7/§8 | Stays deferred to the roster phase. |

## Testing strategy

- **Pure host tests** (doctests, no install required):
  - `is_enemy` / `are_allied` truth table:
    - `houses_configured_ == false` (default): `is_enemy(a,b) == (a != b)` for all pairs; `are_allied(a,b) == (a == b)` — the byte-identity contract.
    - configured FFA (every house own team): `is_enemy` among combatants == `a != b`; self never enemy.
    - configured team: teammates `are_allied` and NOT enemies; cross-team combatants ARE enemies.
    - neutral / non-combatant: never an enemy of anyone (both directions), never allied to a combatant it doesn't share a team with; self-ally still holds.
  - `HouseRec` defaults: house 0 Allied/blue/team0/combatant, house 1 Soviet/red/team1/combatant, house 2 Neutral/blue/team2/non-combatant; `alliance_team` sentinel resolves to own id.
  - Victory last-team-standing: a house with zero buildings (ShortGame) is `house_defeated`; with one surviving team `winning_team()` returns that team; while ≥2 combatant teams survive `winning_team()` == -1; Neutral never defeated / never wins.
  - Vectorized-array parity: at `house_count()` 0/1/2 the six vectors read/write identical values to the old `[3]` arrays; accessor guards permit 0..size-1 and reject out-of-range.
- **New 16th gate `--demo-multihouse`** (mirror `run_demo_capture`): scenario with ≥3 combatant houses in 2 alliance-teams (e.g. 0+2 allied vs 1) + a neutral house owning a structure. Two-run compute-and-self-compare (run1 rendered+PNG, run2 headless fresh `World`), `identical=(witness && hash)`, **NO pinned literal in code** (README pins the hash), wedge guards → exit 1. Exercises: (a) allied units do NOT auto-acquire each other but DO acquire the enemy team; (b) AllyReveal — an ally sees what its partner reveals; (c) a house eliminated → `house_defeated` true; (d) last-team-standing → `winning_team()` resolves. **Wedge guards** (→ exit 1): `allies-dont-fight`, `enemy-acquired`, `ally-reveal-shared`, `defeat-detected`, `team-wins`. Fold: per-house alive building+unit counts + a per-pair relationship sample + defeated flags + `winning_team()`, at fixed checkpoints. `world_hash` + all 15 prior `*_hash` untouched.
- **The 15 existing gates stay byte-identical** — re-verify at the vectorization commit (T3, first full re-verify), the enmity-routing commit (T4, combat=78 / tank=144 + all gates), the AllyReveal commit (T5, shroud gate), and finalize (T9), **one gate per call** (each AI gate ≈2 min; batching exceeds the tool timeout — see the gate-runtime discipline). Confirms `houses_configured_ == false` / `victory_enabled_ == false` keep the enmity predicate, side/color derivation, AllyReveal, victory pass, and vectors fully inert.

## [RE-FALLBACK] items for Task 1 RE-GATE to adjudicate

Task 1 is a **confirmatory RE-GATE adjudication** (re-pin values via `ra2extract` + targeted OpenRA/decompile reference; flip `[RE-FALLBACK]`/`[ANCHORED]`→adjudicated), NOT a fresh hunt. The items to close before code:

1. **Exact 8-color HSV→RGB values** (§2, D6): the eight MP colors are recovered as HSV triples (Gold=43,239,255; DarkRed=0,230,255; Orange=25,230,255; Magenta=221,102,255; Purple=201,201,189; DarkBlue=153,214,212; DarkSky=131,200,230; DarkGreen=81,200,210). Adjudicate the concrete HSV→RGB remap-spread conversion the engine uses to generate each house's palette band (confirm it matches the existing `pal_for_house` spread rule), so the 8-color output is deterministic. Render is unhashed, so any faithful conversion is acceptable if it reproduces the recovered hues.
2. **The precise defeat condition** (§5, D5): confirm `ShortGame=yes` (shipped default) ⇒ defeated on **last building**, vs `ShortGame=no` ⇒ zero-buildings-AND-units. Adjudicate which the gate drives (the spec assumes the shipped `ShortGame=yes` last-building rule) and confirm the "required building" set (ConYard/structures counting) via rules/OpenRA `PlayerExtensions.cs`.
3. **Neutral default relationship** (§3, D1/D3): confirm whether a neutral / `MultiplayPassive` house is relationship-**Neutral** by default (never auto-attacked, never attacks) or merely non-combatant with an unspecified relationship. The spec treats non-combatant ⇒ never an enemy of anyone; adjudicate that this matches `Player.cs:249`'s `NonCombatant ? Neutral` branch.
4. **Exact FFA-default enmity for the gate** (§3, D3): confirm that with `AlliesAllowed=no` (FFA) every combatant pair is Enemy and this equals `is_enemy(a,b) == (a != b)` among combatants — i.e. the configured-FFA path and the unconfigured default path agree — so the gate's alliance-configured FFA baseline is byte-consistent with today's `a != b` behavior.
