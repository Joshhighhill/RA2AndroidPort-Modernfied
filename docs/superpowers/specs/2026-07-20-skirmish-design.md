# Skirmish Shell + Main Menu — Design Spec

**Status:** DRAFT (awaiting user review)
**Depends on:** the Multi-house / Alliances phase (`docs/superpowers/specs/2026-07-20-multihouse-design.md` — provides `HouseRec`, `set_house`, `house_count`, `enable_ai_house`, `enable_victory`, `enable_shroud`/`enable_ally_reveal`, `is_enemy`/`are_allied`/`house_team`, the 8 MP colors, and `simulate_multihouse` as the launcher template) + the base World/render/combat engine (`spawn_base`, `add_combat_unit`, `add_building`, `pal_for_house`, `grant_superweapon`) + the existing `run_interactive` playable loop (`main.cpp:2901`).
**Calibration authority:** `docs/re/skirmish.md` (the `rulesmd.ini` recovery hunt — ALREADY COMPLETE; real rules at `D:\ra2-re\out\shroud_evidence\rulesmd.ini`, `[MultiplayerDialogSettings]` line 3017). This spec is built on recovered original-game behavior + the controller's binding design decisions (D1–D7), not design guesswork.

## Goal

Replace the hardcoded single scenario with a **config-driven skirmish launch** and a minimal **scene/state framework**, so the sim can be started from a `SkirmishConfig` the player assembles in a lobby instead of the fixed inline scenario baked into `run_interactive`. This phase delivers two things:

- **A launcher — `SkirmishConfig` → `launch_skirmish` → `World`** — the testable core. A plain config struct (map, per-slot side/color/country/team/AI/difficulty, cash, unit count, tech level, toggles) is turned into a ready-to-run `World` by a free function modeled 1:1 on `simulate_multihouse` + the `run_interactive` setup block: `build_world` → per-slot `set_house` → `enable_shroud`/`enable_ally_reveal`/`enable_victory` → per-house `spawn_base` + starting units at the map waypoints → `enable_ai_house` for AI slots → `grant_superweapon` per slot. Calibrated to RA2's recovered skirmish lobby options and defaults.
- **A scene framework — main menu → skirmish setup → game.** A tiny `Scene` base class + a scene-loop driver wired at the `main()` seam. `GameScene` is the current `run_interactive` loop body, lifted and initialized from a `SkirmishConfig` via `launch_skirmish`. `MainMenuScene` (Skirmish + Quit live; the rest visible-but-stub) and `SkirmishSetupScene` (the recovered lobby options) drive the launcher. A minimal in-house rect-font (A–Z, extending the existing digit strip) renders the menu text.

**Byte-identity promise.** The launcher, the scenes, the font, and the config struct are **PURELY ADDITIVE**. The 16 existing byte-identity gates set up their worlds DIRECTLY (`build_world`/`spawn_base`/`set_house`/`add_combat_unit` inline) and NEVER call `run_interactive`, the menu, the scene driver, or `launch_skirmish` — so none of this new code can alter a single gate byte. Every existing `run_demo_*`/`simulate_*`/`build_world`/`spawn_base`/`spawn_squad` signature is unchanged; `world_hash` is not touched. All 16 existing gates stay bit-identical; the feature adds a 17th, `--demo-skirmish`, which steps the same fixed `dt = 1/15` and self-compares (no pinned literal).

## Global Constraints

Copied verbatim from the controller's design direction. These bind every task.

- All 16 existing byte-identity gates MUST stay bit-identical: combat logs=78, tank logs=144, base 5273b5e71ca009e1, econ 7fed96cb98b68017, prod f0f4e1207ecd713c, ai-base cd1f7c6a29933f0a, ai-team 312e92ed862a5e74, ai-trigger 8334a6ca1531cabc, transport fe500f946e913645, aircraft a69642564cf64773, naval 6aee2c82bc024e86, superweapon 189496664f5eca68, techtree e73adae61ff6f71f, capture 58b87b5bc63f7046, shroud d302852aa9517661, multihouse 39412ae996cce8ae. Adds the 17th: `--demo-skirmish`.
- `world_hash` untouched. The 16 gates set up worlds DIRECTLY (build_world/spawn_base/set_house/add_combat_unit inline) and NEVER call run_interactive/menu/config → the launcher + scenes + font + config struct are PURELY ADDITIVE and cannot alter a single gate byte. Keep every existing run_demo_*/simulate_*/build_world/spawn_base/spawn_squad signature unchanged.
- Demo gate = two-run compute-and-self-compare, NO pinned literal (README pins), wedge guards → exit 1. --demo-skirmish steps a FIXED dt=1/15 (like all gates); the interactive GameScene keeps variable dt (not a gate).
- No assets/GPL/Ghidra raw output committed. Recovered CONSTANTS (option defaults/ranges, 8-player cap) ARE committable.
- Commit trailer: `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`.

## Calibration summary (recovered ground truth — the behavior contract)

Every claim below is tagged as in `docs/re/skirmish.md` ([CONFIRMED] = real `rulesmd.ini`/decompile; [ANCHORED] = OpenRA/documented RA2, structure only; [RE-FALLBACK] = design choice). Task 1 re-adjudicates the [RE-FALLBACK] items (see the last section) before code. All numeric defaults are the **shipped** `[MultiplayerDialogSettings]` values (rulesmd.ini:3018–3042), NOT the code `def=` glossary (rulesmd.ini:2990–3015) — the shipped values are what the retail game presents (skirmish.md §preamble, §5).

### Lobby OPTION list — shipped defaults + ranges (§1)

Numeric / slider options:

| Option | Shipped default | Range (min…max, step) | Rules anchor | Tag |
|---|---|---|---|---|
| **Starting Credits (cash)** | 10000 | 5000 … 10000, step 100 | `Money=10000`/`MinMoney=5000`/`MaxMoney=10000`/`MoneyIncrement=100` (:3018–3021) | [CONFIRMED] |
| **Unit Count** (starting units, MCV extra) | 10 | 0 … 10 | `UnitCount=10`/`MinUnitCount=0`/`MaxUnitCount=10` (:3022–3024) | [CONFIRMED] |
| **Tech Level** | 10 | 1 … 10 | `TechLevel=10` (:3025) | [CONFIRMED] value; 1–10 range [ANCHORED] |
| **Game Speed** | 1 | 0 … 6 (0=fastest, 6=slowest) | `GameSpeed=1` (:3026) + comment :2999 | [CONFIRMED] |
| **AI Players** (count) | 0 (skirmish forces ≥1) | 0 … 7 | `AIPlayers=0` (:3028) + comment :3001 | [CONFIRMED] value; 0–7 cap [ANCHORED] |
| **AI Difficulty** | 0 = Easy | Easy=0 / Normal=1 / Brutal(Hard)=2 | `AIDifficulty=0` (:3027) + comment :3000 | [CONFIRMED] |

Toggle / checkbox options (all default as shipped):

| Option | Shipped default | Rules anchor | Retail status | Tag |
|---|---|---|---|---|
| **Bridge Destruction** | on | `BridgeDestruction=yes` (:3029) | Active | [CONFIRMED] |
| **Crates** | on | `Crates=yes` (:3034) | Active | [CONFIRMED] |
| **Short Game** (defeat on last building) | on | `ShortGame=yes` (:3039) | Active | [CONFIRMED] |
| **MCV Redeploys** | on | `MCVRedeploys=yes` (:3041) | Active | [CONFIRMED] |
| **Allies Allowed** (teams/alliances) | off (= FFA) | `AlliesAllowed=no` (:3038) | Active | [CONFIRMED] |
| **Ally Change Allowed** | on | `AllyChangeAllowed=yes` (:3042) | Active | [CONFIRMED] |
| **Fog of War** | off | `FogOfWar=no` (:3040) | Active | [CONFIRMED] |
| **Ore/Tiberium Grows** | on | `TiberiumGrows=yes` (:3033) | Active | [CONFIRMED] |
| **Shroud** (map starts shrouded) | on (always) | `Shroud=yes` (:3031) | "not yet supported" as a toggle → always-on | [CONFIRMED] |
| **Superweapons** | on | not a rules key; retail lobby checkbox (≠ `[IQ] SuperWeapons=4` :3159) | Active | [ANCHORED] toggle / [CONFIRMED] IQ key is separate |
| **Build Off Ally** | on | not a rules key; documented YR lobby toggle | Active | [ANCHORED] |

### The 5 DESUPPORTED keys — HIDE from the lobby (§1)

`Bases`, `ShadowGrow`, `CaptureTheFlag`, `HarvesterTruce`, `MultiEngineer` are explicitly marked **DESUPPORTED** in the retail YR trailing comments (rulesmd.ini:3003–3010): the keys still parse but the features were cut. A faithful skirmish shell does NOT offer these as live toggles; they remain fixed at their shipped values and are never user-facing. This is the difference between the rules *schema* and the retail *lobby*. [CONFIRMED keys; features dead]

### Superweapons / Build Off Ally (§1)

Both are lobby toggles NOT present in `[MultiplayerDialogSettings]`. `Superweapons` is the retail "allow superweapons" checkbox — do NOT conflate with `[IQ] SuperWeapons=4` (:3159), which is the AI IQ level at which the computer auto-fires SWs. `Build Off Ally` (share build radius with allies) has no rules key in this revision. Both default on. [ANCHORED toggles / CONFIRMED the IQ key is a different thing]

### Player cap (§1)

8-player hard cap. No `MaxPlayers` rules key exists; `[Maximums] Players=8` (rulesmd.ini:3056, `; ipx layer limits this to 8 maximum`) is the object-heap cap and corroborates the limit. Per-match start-location count comes from the map's `[Waypoints]` (0..7), not rules. [ANCHORED 8]

### Main-menu structure (§2)

Documented retail YR menu. Only **Skirmish** (+ the shared MP lobby data model) is in scope this phase; everything else is a menu stub. Skirmish IS a local Multiplayer match with AI-filled slots and no netcode — one code path serves both. [ANCHORED — documented menu, not a rules artifact]

| Menu entry | Scope for this clone |
|---|---|
| **Skirmish** | IN SCOPE (phase target) |
| **Exit / Quit** | IN SCOPE (trivial) |
| Campaign / Single Player | Stub |
| Multiplayer → LAN / Internet / Network / Modem | Stub (shares §1/§3 data model, minus AI-fill + netcode) |
| Load Game | Stub |
| Options | Stub |
| Map Editor (external tool) | Out of scope |
| Movies / Credits | Stub |

### Launch flow → engine mapping (§3)

A skirmish match instantiates: **house slots** = (human slots, index 0 = the human) + (`AIPlayers` AI slots, ≥1 forced) + the two fixed non-combatant houses **Neutral** and **Special (Mutant)**; combatant slots ≤ 8. Each combatant slot binds a country (→ Side via `[Sides]`: GDI→Allied, Nod→Soviet, ThirdSide→Yuri), a color (one of 8 MP colors, unique per slot), a start waypoint (map `[Waypoints]`), an alliance team id, and human-vs-AI + AI difficulty handicap. Each slot spawns an MCV at its start waypoint plus `UnitCount` (=10) combat units around it, with `Money` (=10000) starting credits. [Flow: ANCHORED; engine API + rules-backed counts: CONFIRMED where noted]

| Lobby datum | Engine call | Notes |
|---|---|---|
| Slot country → Side | `set_house(h, {.side, .country})` | Side from `[Sides]`. [CONFIRMED map] |
| Slot MP color | `HouseRec.color_ix = 0..7` | Render-unhashed. [CONFIRMED list] |
| Team (`AlliesAllowed`) | `HouseRec.alliance_team` | FFA default → team = own id → `is_enemy == a!=b`; team game → shared id. [CONFIRMED default / ANCHORED teams] |
| Neutral + Special | `set_house(h, {.combatant=false})` | `MultiplayPassive`; never win/lose/auto-targeted. [CONFIRMED] |
| `AIPlayers` count | one `enable_ai_house(h, cfg)` per AI slot | ≥1 for skirmish. [CONFIRMED count] |
| `AIDifficulty` (0/1/2) | AI cfg handicap band | Easy/Normal/Difficult. [CONFIRMED] |
| Start waypoint | spawn MCV + `UnitCount` units | Waypoints from map. [ANCHORED] |
| `Money` / `UnitCount` | seed credits + spawn count | [CONFIRMED values] |

Firewall note: `set_house` sets `houses_configured_ = true`; the skirmish shell is the first real caller. FFA with each team = own id yields `is_enemy(a,b) == (a != b)`, identical to the unconfigured default — so the FFA skirmish baseline is byte-consistent with the current gates (multihouse.md §Adjudication Item 4).

### Game speed — presentation-only + GameSpeedBias carry-forward (§4)

- Original `GameSpeed` = 7 discrete cadence levels 0..6, **inverted** (0=fastest, 6=slowest); shipped default **1**. It throttles the inter-frame delay of the logic loop (wall-clock pacing), NOT per-frame movement magnitude. [CONFIRMED value / ANCHORED mechanism]
- Our engine uses a fixed logic step `dt = 1/15` (~66.7 ms, ~15 logic fps) throughout the sim/demo loops (11 sites in `main.cpp` + `world.cpp:1667`) and has **no runtime game-speed control**. [CONFIRMED — code]
- **Decision:** because our step is deterministic and fixed, `GameSpeed` is a **presentation-time pacing knob** (frames emitted per wall-second) that does NOT change the deterministic logic step or the world hash → it is NOT stored in `SkirmishConfig`. [Deviation documented / RE-FALLBACK for how we expose it]
- `[General] GameSpeedBias=1.6` (rulesmd.ini:60, "multiplier to overall game object movement speed") IS a logic-affecting constant, but our movement constants are already calibrated and gated; retroactively applying a bias would break the movement gates. It is a **CARRY-FORWARD** — do NOT apply it this phase. [CONFIRMED value / carry-forward]

## Current-engine reality (what we build on)

From the survey — file:line anchors. These are the seams the launcher and scene framework attach to; none is a sim invariant this phase alters.

- **No scene/state framework, no config struct.** `main()` at `main.cpp:8492`: Android jumps to `run_interactive(false)` at 8501; desktop does an argv scan + dispatch ladder 8523–8610; the 16 gate arms are at 8527–8609; the fallthrough is `return run_interactive(no_audio)` at 8610.
- **`run_interactive` (`main.cpp:2901`) is the only playable mode** and hardcodes the ENTIRE scenario inline: assets 2902–2958 (`load_soviet_unit_roster` hardcoded 2930), `build_world(a,ua,5)` 2964 (5 GI house0 + 5 CONS house1), `wire_economy` 2967, `spawn_tank_squads(...,3)` 2968, transport 2972–2985, `spawn_base` house0 (2988) + house1 with_refinery (3015), `enable_ai_house(1, Normal, rules.mp_money())` 2998–3013, `grant_superweapon(0,Nuke)` 3021, engineer/spy/dog 3035–3132, `player_credits = rules.mp_money()` 3194. Map hardcoded `mapsmd03` (load_assets 138, entry 0x133a0cb2). Loop 3213–3547 (variable `dt=(now-prev)/1000` at 3353; `render_world` 3514; `draw_sidebar` 3536; present + `SDL_Delay(16)`).
- **Multi-house hooks (all present in `world.hpp` from the multihouse phase):** `HouseRec` 785, `Side` enum 780, `set_house` 810, `house_count` 794, `enable_ai_house` 728, `enable_victory(on, short_game=true)` 834 + `house_defeated` 835 / `winning_team` 836, `enable_shroud` 951 + `enable_ally_reveal` 826, `are_allied` 819 / `is_enemy` 820 / `house_team` 805 / `house_side`/`color`/`palette` 797–802, `grant_superweapon` 763, `add_combat_unit`/`add_building`.
- **`simulate_multihouse` (`main.cpp:8214`) is the EXACT launcher template:** `build_world(a,ua,0)` → per-house `set_house` → `enable_shroud`/`ally_reveal`/`victory` → per-house `add_combat_unit`/`add_building`.
- **UI primitives — no font system** (`iso_renderer.hpp:227` "REUSE only, no new font systems"). Only a 5x7 digit strip `kDigits[10][7]` (`main.cpp:2516`) + `draw_number` 2524 / `draw_number_centered` 2544. Rect fills via `SDL_RenderFillRect`; `blit_shp` 2552; `draw_sidebar` 2666. Hit-testing: `SbRect::contains` (`sidebar.hpp:14`); `SidebarLayout::cameo_at`/`tab_at` (`sidebar.hpp:71–73`). Mouse `InputEvent{PointerDown/Up/Move,x,y,button,shift,t_ms}` (`platform.hpp:4`); sidebar consume-in-rect-before-world pattern (`main.cpp:3227`).
- **Map list:** single hardcoded `mapsmd03`; NO enumeration path (grep negative). MP maps `.mpr`/`.map`/`.yrm` not referenced. A map browser = net-new MIX scan (CARRY-FORWARD; ship a static single-entry list for now).
- **Gate harness:** `run_demo_multihouse` 8384 + `simulate_multihouse` 8214 (rendered hidden-window + PNG, headless fresh `World`, `identical=(witness && hash)`, `multihouse_fold` 8195/8274, wedge guards, NO pinned literal). Every gate sets up worlds DIRECTLY — the config launcher is purely additive.

## Architecture

Each component states what it does, its interface, its dependencies, and — critically — HOW it stays additive / byte-identical (the 16 gate arms 8527–8609 never call any of it; the `build_world`/`spawn_base`/`run_demo_*` signatures are unchanged).

### A. `SkirmishConfig` + `SkirmishSlot` struct + `default_skirmish_config()` (D1)

A plain data struct with no engine coupling beyond the `World::Side` and `AiDifficulty` enums. The single input to the launcher.

```cpp
struct SkirmishSlot {
    World::Side  side;
    int          color_ix;
    std::string  country;
    int          alliance_team;   // -1 sentinel -> own id (FFA)
    bool         combatant;
    bool         is_ai;
    AiDifficulty diff;            // meaningful only when is_ai
};
struct SkirmishConfig {
    std::string map_id = "mapsmd03";   // only one map exists; browser = carry-forward
    std::vector<SkirmishSlot> slots;   // index 0 = the human player
    double starting_cash = 10000.0;    // recovered default; slider 5000-10000
    int    unit_count    = 10;         // 0-10
    int    tech_level    = 10;         // 1-10
    bool   shroud        = true;
    bool   crates        = false;      // wired to the World crate seam if present, else inert
    bool   superweapons  = true;
    bool   short_game    = true;
    // GameSpeed is PRESENTATION-only, NOT stored here (does not affect the sim/hash).
};
```

`default_skirmish_config()` reproduces the current `run_interactive` 1v1 for a "quick start": slot 0 = Allied human, slot 1 = Soviet AI at Normal, `starting_cash = rules.mp_money()` (10000), plus the fixed Neutral + Special non-combatant slots. This is the config the thin-wrapper `run_interactive` (component D) and the desktop default path feed into `launch_skirmish`.

**Additive/inert:** a pure data struct in a new header. Nothing in the 16 gates references it; it never feeds `world_hash`. Depends on: `World::Side`, `AiDifficulty` (both pre-existing).

### B. `launch_skirmish(config, assets...)` → `World` — the testable CORE (D2)

A new free function modeled 1:1 on `simulate_multihouse` + the `run_interactive` setup block. Given a `SkirmishConfig` and the loaded assets, it produces a ready-to-run `World`:

1. `build_world(a, ua, 0)` — start with zero pre-seeded units (the launcher spawns them).
2. For each slot `i`: `set_house(i, HouseRec{.side, .color_ix, .country, .alliance_team, .combatant})`.
3. `enable_shroud(config.shroud)`, `enable_ally_reveal(...)`, `enable_victory(true, config.short_game)`.
4. For each combatant slot: `spawn_base(...)` + spawn `config.unit_count` starting combat units (plus the MCV) at the slot's map start waypoint; seed `config.starting_cash` credits.
5. For each AI slot: `enable_ai_house(i, make_ai_config(slot.diff, config.starting_cash))`.
6. For each slot, if `config.superweapons`: `grant_superweapon(i, ...)`.
7. Return the `World`.

This is the byte-identity-relevant deliverable and is fully **host-testable** (config → correct `house_count`/teams/AI-enabled/victory/shroud/credits). The menu UI drives it but is not required for correctness.

**Additive/inert:** a new free function that only calls EXISTING `World` APIs (`build_world`, `set_house`, `enable_*`, `spawn_base`, `add_combat_unit`, `enable_ai_house`, `grant_superweapon`). It is never called by any of the 16 gates. It introduces no new `World` state and does not touch `world_hash`. Depends on: the multihouse `World` API (component-list above), the roster/asset loaders.

### C. `--demo-skirmish` gate — the 17th (D3)

A new gate arm added after 8609 (before the 8610 fallthrough), mirroring `run_demo_multihouse`. It builds a FIXED `SkirmishConfig` in code (e.g. 3 combatant slots: 1 human + 1 AI + a small allied/enemy setup, plus Neutral/Special non-combatants), calls `launch_skirmish` → `World`, steps N frames at a **FIXED `dt = 1/15`** (like every gate), folds a `skirmish_fold` (house_count, per-house alive counts, a teams/relationship sample, `ai_enabled` flags, victory/shroud enabled, cash), and does a two-run rendered+headless self-compare (`identical = witness && hash`). Wedge guards → exit 1. **NO pinned literal** (README pins the hash). `world_hash` + all 16 prior `*_hash` untouched.

**Additive/inert:** a new gate adds no code to any existing gate's path; it is the only caller of `launch_skirmish` inside the harness. Depends on: `launch_skirmish` (B), the fold/self-compare harness (from the multihouse gate).

### D. Scene/state framework + `GameScene` (D4)

A tiny polymorphic scene interface + a scene-loop driver, wired ONLY at the `main()` seam.

```cpp
struct Scene {
    virtual void   handle(const InputEvent&) {}
    virtual void   update(double dt) {}
    virtual void   render(Platform&, TextureCache&) {}
    virtual Scene* next() { return nullptr; }   // non-null => transition
    virtual ~Scene() {}
};
```

The driver owns the active `Scene*`, pumps input → `handle`, ticks `update(dt)`, calls `render`, and on a non-null `next()` swaps scenes. `GameScene` is the current `run_interactive` loop body (3214–3546) **lifted verbatim**, but initialized from a `SkirmishConfig` via `launch_skirmish` instead of the hardcoded 2902–3194 setup block. It keeps the interactive **variable dt** (it is NOT a gate). `run_interactive` stays as a **thin wrapper** — `default_skirmish_config()` → `GameScene` — so anything that referenced `run_interactive` still works. The desktop fallthrough (8610) and the Android entry (8501) call the scene driver starting at `MainMenuScene`.

**Additive/byte-identical:** the 16 gate arms (8527–8609) are UNTOUCHED — they never enter the scene driver. Only the two non-gate entry points (Android 8501, desktop fallthrough 8610) change, and only to route through the driver. `GameScene` produces the same interactive experience `run_interactive` did (it IS that loop body). No gate calls `Scene`, the driver, or `GameScene`. Depends on: `InputEvent`/`Platform`/`TextureCache` (existing), `launch_skirmish` (B).

### E. Rect-font A–Z + `draw_text` (D5)

Extend the existing rect-font approach (`kDigits[10][7]`) with an A–Z (+ space, basic punctuation) glyph table `kGlyphs` and a `draw_text(renderer, str, x, y, scale, color)` helper that draws each glyph as filled rects (the same technique as `draw_number`). Reused for menu labels/buttons. No external font dependency — this is the minimal in-house extension the `iso_renderer.hpp:227` "REUSE only, no new font systems" note permits.

**Additive/inert:** new const glyph data + one drawing helper, used only by the menu scenes. No gate renders text through it; render output is unhashed regardless. Depends on: the existing rect-draw primitives.

### F. `MainMenuScene` + `SkirmishSetupScene` (D6)

- **`MainMenuScene`:** title + buttons — Skirmish [active], Quit [active]; Campaign / Multiplayer / Load / Options visible-but-stub (greyed). Button hit-test via `SbRect::contains`. Click Skirmish → `next()` returns a `SkirmishSetupScene`; Quit exits the driver.
- **`SkirmishSetupScene`:** exposes the recovered lobby options with shipped defaults — player count / AI count + difficulty, per-slot side + color + alliance team, cash slider (5000–10000), unit count (0–10), tech level, toggles (crates / superweapons / short-game). **Hides the 5 DESUPPORTED keys.** Map = the single `mapsmd03` (browser carry-forward). "Start" → assemble a `SkirmishConfig` → `next()` returns `GameScene(launch_skirmish(config))`. "Back" → `MainMenuScene`.
- Faithful to the RA2 skirmish lobby **option set**, not pixel-exact.

**Additive/inert:** new scenes reachable only through the driver from the two non-gate entry points; no gate constructs or renders them. Depends on: `draw_text` (E), `SbRect`/`InputEvent` (existing), `SkirmishConfig` (A), `launch_skirmish` (B).

## Data model (exact struct additions)

| Field / type | On | Default | In `world_hash`? | Why additive/inert |
|---|---|---|---|---|
| `struct SkirmishSlot { Side side; int color_ix; std::string country; int alliance_team; bool combatant; bool is_ai; AiDifficulty diff; }` | (new type) | — | No | Pure config data; consumed by `launch_skirmish` into existing `set_house`/`enable_ai_house` calls only. |
| `struct SkirmishConfig { … }` | (new type) | see D1 | No | Pure config data; never folded; no gate references it. |
| `std::vector<SkirmishSlot> slots` | `SkirmishConfig` | `default_skirmish_config()` → Allied human + Soviet-Normal AI + Neutral/Special | No | Maps 1:1 to `set_house` calls the launcher already makes. |
| `struct Scene { virtual handle/update/render/next; virtual ~Scene(); }` | (new type) | — | No | UI-layer polymorphic base; driven only at the `main()` seam, never by a gate. |

**None of it enters `world_hash`.** `world_hash` folds only `Unit{cell_x,cell_y,hp,alive}` + `Building{type,cx,cy,hp,state}` — untouched. `SkirmishConfig`/`SkirmishSlot`/`Scene` are NOT `World` state and are never folded. The launcher only calls existing `World` APIs — it adds no new field to `World`, so it cannot perturb any hash. The scenes and font are render/UI-layer only and unhashed.

## Behavior details (each tied to the recovered §)

- **Launch flow** (§3, D2): for each combatant slot the launcher calls `set_house(i, {side, color_ix, country, alliance_team, combatant})`; FFA default (`alliance_team = own id`) → `is_enemy(a,b) == (a != b)`; team game → shared team ids. Neutral + Special are `combatant=false`. One `enable_ai_house(i, cfg)` per AI slot (≥1 forced for skirmish). Each combatant spawns an MCV + `unit_count` (=10) units at the map start waypoint with `starting_cash` (=10000) credits. Victory is `enable_victory(true, short_game)`; shroud is `enable_shroud(shroud)` (always-on in retail). [CONFIRMED counts / ANCHORED flow]
- **Option defaults** (§1, D1): the config defaults are the shipped `[MultiplayerDialogSettings]` values — cash 10000 (5000–10000/100), unit count 10 (0–10), tech level 10, superweapons on, short game on, shroud on. AI difficulty 0=Easy is the shipped default; the setup screen may pick 0/1/2. The 5 DESUPPORTED keys are never surfaced. [CONFIRMED]
- **GameSpeed presentation-only** (§4): `GameSpeed` is NOT in `SkirmishConfig` and never reaches the launcher. Our deterministic `dt = 1/15` step is unchanged; the option, if exposed at all, would only pace frame emission at wall-clock time and cannot alter the logic step or `world_hash`. `GameSpeedBias=1.6` is a carry-forward (not applied). [CONFIRMED / deviation documented]
- **Menu → setup → game scene flow** (§2, D4/D6): the driver starts at `MainMenuScene`. Skirmish → `SkirmishSetupScene` (recovered options, DESUPPORTED hidden, single `mapsmd03`). Start → build `SkirmishConfig` → `GameScene(launch_skirmish(config))`. Back returns to the menu; Quit exits. Only Skirmish + Quit are live; Campaign/Multiplayer/Load/Options are greyed stubs. [ANCHORED menu]

## Scope / non-goals (carry-forwards — document, do NOT implement)

Explicitly OUT of scope this phase (documented, not implemented):

| Carry-forward | Recovered ref | Why deferred |
|---|---|---|
| **Map browser / MIX map enumeration + `.mpr`/`.map`/`.yrm` parsing** | §1/§3 [ANCHORED] | No enumeration path exists; a browser is a net-new MIX scan. Ship the single static `mapsmd03`. |
| **Campaign / single-player** (Allied/Soviet/Yuri ops) | §2 [ANCHORED] | Scripted missions; menu stub only. |
| **Multiplayer networking** (LAN / Internet / Network / Modem) | §2 [ANCHORED] | Shares the §1/§3 data model minus netcode + AI-fill; no networking this phase. |
| **Load / Save game** | §2 [ANCHORED] | No serialization; menu stub. |
| **Options / settings persistence** | §2 [ANCHORED] | Engine has its own config; no persisted lobby presets. |
| **Map Editor** (FinalAlert 2, external) | §2 [ANCHORED] | Out of scope (external tool). |
| **Movies / Credits** | §2 [ANCHORED] | Menu stubs. |
| **GameSpeed control** (presentation pacing) | §4 [CONFIRMED / RE-FALLBACK] | Our step is fixed 1/15; how (or whether) to expose a pacing knob is deferred — it never affects the sim/hash. |
| **`GameSpeedBias=1.6` movement reconciliation** | §4 [CONFIRMED value] | Movement constants already calibrated + gated; applying a retroactive bias would break the movement gates. |
| **Full country roster / Yuri side** | §1 [CONFIRMED list] | Allied/Soviet art only; `Side::Yuri` defined but unpopulated (inherited from multihouse). |
| **Multi-human input** (more than one local player) | §1/§3 [ANCHORED] | Player stays house 0 in the input layer; a real menu makes this more visible but it stays deferred. |
| **In-game alliance-request UI** (`AllyChangeAllowed`) | §1/§3 [CONFIRMED key] | Alliances configured programmatically via `alliance_team`; no runtime request UI. |
| **Per-house on-screen color** (rendering the 8 MP colors in-game) | §1/§3 [CONFIRMED colors] | `color_ix` is carried into `HouseRec` but full per-house color rendering stays a multihouse carry-forward. |
| **Saving/loading skirmish presets** | §2 [ANCHORED] | No preset persistence. |

## Testing strategy

- **Pure host tests** (doctests, no install required) for `launch_skirmish` + `default_skirmish_config`:
  - **Config → World correctness:** a config with N combatant slots + M AI slots yields the expected `house_count()` (N humans + M AI + Neutral + Special); each AI slot has `ai_enabled(i) == true` and humans `false`; `enable_victory` / `enable_shroud` reflect `config.short_game` / `config.shroud`; each combatant house is seeded with `config.starting_cash` credits and `config.unit_count` units (+ MCV).
  - **Teams / relationships:** FFA config (each `alliance_team = own id`) → `is_enemy(a,b) == (a != b)` among combatants; a team config (shared ids) → teammates `are_allied` and NOT enemies, cross-team combatants ARE enemies; Neutral/Special (`combatant=false`) are never enemies of anyone.
  - **`default_skirmish_config()` reproduces the 1v1:** slot 0 Allied human, slot 1 Soviet AI Normal, cash `rules.mp_money()`, plus Neutral/Special — i.e. the current `run_interactive` scenario, so the thin-wrapper path is behavior-preserving.
- **New 17th gate `--demo-skirmish`** (mirror `run_demo_multihouse`): a FIXED in-code `SkirmishConfig` → `launch_skirmish` → step N frames at fixed `dt = 1/15` → two-run compute-and-self-compare (run1 rendered+PNG, run2 headless fresh `World`), `identical = (witness && hash)`, **NO pinned literal in code** (README pins the hash), wedge guards → exit 1. Wedge guards: config produced the expected `house_count`; the AI house is enabled; victory + shroud enabled; the sim advanced / credits changed deterministically. Fold: `house_count`, per-house alive counts, a teams/relationship sample, `ai_enabled` flags, victory/shroud enabled, cash. `world_hash` + all 16 prior `*_hash` untouched.
- **The 16 existing gates stay byte-identical** — first full re-verify at the 17th-gate commit (T3 — additive, should be trivially green; T2 is host-tests only), again at the scene-framework commit (T4, all 16 — the `run_interactive`→wrapper lift is the highest-risk touch), and at finalize (T7), **one gate per call** (each AI gate ≈2 min; batching exceeds the tool timeout — see the gate-runtime discipline). Confirms the launcher/scenes/font/config are fully additive.
- **Menu UI scenes are NOT hash-gateable** (interactive, variable-dt). They are validated by: (a) a clean **build**; (b) the gate's `launch_skirmish` path exercising the launcher core deterministically; (c) **finalize screenshots** of `MainMenuScene` / `SkirmishSetupScene` / a launched `GameScene` for visual confirmation. Symbol presence (`launch_skirmish`, `Scene`) is confirmed via `nm` on the arm64 `assembleDebug` build in finalize.

## [RE-FALLBACK] items for Task 1 RE-GATE to adjudicate

Task 1 is a **confirmatory RE-GATE adjudication** (re-pin option defaults/ranges via `ra2extract` + targeted reference; flip `[RE-FALLBACK]`/`[ANCHORED]` → adjudicated), NOT a fresh hunt. The items to close before code:

1. **In-scope option subset for the setup screen** (§1, D6): confirm exactly which recovered lobby options `SkirmishSetupScene` exposes, given the 5 DESUPPORTED keys are hidden and Superweapons / Build Off Ally are [ANCHORED] non-rules toggles. The spec assumes the live set = Credits, Unit Count, Tech Level, AI Players + Difficulty, Crates, Short Game, MCV Repack, Bridge Destruction, Superweapons, Multiplayer Alliances (+ per-slot side/color/team) — adjudicate this is the faithful retail lobby subset and that Build Off Ally / Bridge Destruction / MCV Repack / TiberiumGrows / FogOfWar are surfaced-or-fixed as the retail lobby does.
2. **GameSpeed handling** (§4): confirm GameSpeed is presentation-only with **no sim effect** on our fixed `dt = 1/15` step — i.e. it is correctly excluded from `SkirmishConfig` and cannot alter `world_hash`. Adjudicate whether the setup screen shows a (non-functional / cosmetic) GameSpeed control at all, or omits it entirely for this phase.
3. **Map-list scope** (§1/§3): confirm the single-entry `mapsmd03` list is the correct phase scope (vs a MIX-scan browser), and that the map's start-waypoint count drives the slot cap for the setup screen. The spec assumes one static map; the browser is a carry-forward.
4. **The `--demo-skirmish` fixed config shape** (D3): adjudicate the exact fixed `SkirmishConfig` the 17th gate builds — slot count, human/AI split, alliance layout (FFA vs a small team setup), Neutral/Special inclusion, and the frame count N — so the gate deterministically exercises `house_count`, AI-enable, victory+shroud, and credit change without a pinned literal.
