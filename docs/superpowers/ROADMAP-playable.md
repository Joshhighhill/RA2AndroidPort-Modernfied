# RA2 Clone — Playable Roadmap (G-phases)

**Standing goal (unchanged):** finish the Red Alert 2 (Allied vs Soviet, no Yuri) clone
at `D:\ra2-android` as a game a person can play. `ROADMAP-completion.md` (P1–P10) built
the subsystems; this roadmap wires them into the game the user launches and proves it
with interactive verification. Issue IDs (G-xx), root causes (RC-x) and RE hunts
(RE-xx) refer to `docs/gap/GAP-ANALYSIS.md`.

**Method (amended):** RE hunt or RE-doc citation → spec → plan → subagent-driven
execution in a worktree → **interactive acceptance under the Phase-0 harness** →
gate batch once at the end of the phase → re-pin per the policy below → README +
ledger. Three rules carry over from the project memory and the gap analysis:

1. **Calibrate to the original first.** Every behaviour comes from `rulesmd.ini`/
   `artmd.ini`/`aimd.ini`/`KeyboardMD.ini`, `docs/re/*.md`, or a Ghidra hunt on
   `gamemd.exe`. Where a hunt is still open the phase ships the documented best guess
   tagged `RE-FALLBACK` in code and in the RE doc, never an empirical tweak.
2. **Playable core first.** Phases are ordered so the four core playthroughs
   (below) turn green as early as possible; polish and breadth come after.
3. **No gate-only firewalls.** A fix is done when the *real* `GameScene` exhibits it.
   The gates keep determinism; hashes are re-pinned with justification (policy §G).

**Model assignment (unchanged):** hunts/specs/plans/reviews → fable; implementation
→ opus; mechanical single-file tasks → sonnet.

## The four core playthroughs (acceptance currency)

Every phase's acceptance is expressed as `--play` scripts (Phase 0). The four that
define "playable core" are checked in as `scripts/play/*.play` and are expected to
FAIL until the phase that owns each assertion lands:

| Script | Setup | Done when |
|---|---|---|
| `SK-A` | Main menu → Skirmish → human Allied vs 1 Soviet AI (Normal), stock MP map, 10000, MCV start, crates off | Deploy MCV, build power/refinery/barracks/factory, train GI + Grizzly + Prism + Engineer + Chrono Miner, defend with a Pillbox, attack with a group, reach **WON** through `World::step_victory`; a second variant with no player input reaches **LOST** on Brutal. |
| `SK-S` | Same, human Soviet vs 1 Allied AI | Soviet sidebar/roster/skin; Conscript/Rhino/Flak Track/War Miner/Tesla Coil; the Allied AI builds GA\* only; **WON**. |
| `M-ALL01` | New Campaign → Allied → ALL01t | Tanya + GIs through the mission's own triggers to **WON** by player actions (not a scripted kill); TechLevel 3 cap visible. |
| `M-SOV01` | New Campaign → Soviet → Sov01t | Deploy the SMCV, build, reach **WON** via the map's triggers. |

Plus a shell smoke set (`shell-esc`, `lobby-roundtrip`, `pause-save-load-resume`).

## Phase queue

Each phase is sized for one agent in one worktree. "Hotspots" lists the regions of
`engine/main.cpp` (15,905 lines; every phase touches it) and other files the phase
edits, so parallel phases can be checked for overlap before they start. Line numbers
are master `f9b93ba`.

### G0 — Verification harness (`--play`)

- **Scope:** RC-2. No gameplay change. Adds a scripted-input driver for the *real*
  scene tree (`ShellScene` → `SkirmishSetupScene`/campaign scenes → `GameScene`) with
  fixed-tick stepping, screenshot capture, a state probe, assertions and a hash.
- **Why now:** nothing later can be accepted without it; it also turns the four core
  scripts into a living definition of done and gives the interactive path its first
  determinism gate.
- **Design (concrete):**
  - `struct EventSource { virtual bool poll(InputEvent&) = 0; }`; `Platform`
    implements it; a new `ScriptedSource` replays `(frame, InputEvent)` pairs.
    `run_scene_driver(plat, tc, scene, EventSource&, double fixed_dt = 0)` — when
    `fixed_dt > 0` it ignores wall-clock and steps `update(1/15)` per iteration
    (`main.cpp:3966-3985`).
  - Hidden window (`SDL_WINDOW_HIDDEN`, software renderer fallback) so it runs in CI;
    screenshots via `SDL_RenderReadPixels` + the existing `stbi_write_png` helper
    (`main.cpp:2892`).
  - Script commands resolve *semantically* so they survive layout changes:
    `click button <ShellName>`, `click cameo <ID>`, `click cell x y [right]`,
    `drag cell x1 y1 x2 y2`, `key <name> [ctrl|shift|alt]`, `place auto`,
    `select unit <house:type[:n]>`, `shot <name>.png`, `wait N`,
    `assert <expr>`, `until N assert <expr>`, `cheat kill_house h`,
    `cheat spawn <type> h x y`, `end N expect WON|LOST|RUNNING`. Cameo/button
    rects come from `SidebarLayout`/`ShellScene::layout`; cells from the camera.
  - `GameScene::probe()` (append-only, public) returns credits(h), units(h,type),
    buildings(h,type,state), hp, selection, cam, active tab, armed mode, outcome,
    queue states, the EVA/voice play log and the trigger-fire log. The probe is
    printed every 15 frames as `play_state f=N …` and hashed to `play_hash`.
  - `no_audio=true`, crates off or seeded (crates seed from the wall clock,
    `main.cpp:4009-4022`), `GameSpeed` fixed.
  - Ships `scripts/play/` with the four core scripts + the shell smoke set, and a
    `tests/test_play_script.cpp` for the parser/resolver.
- **RE needed:** no.
- **Acceptance (interactive):** `ra2engine --play scripts/play/shell-esc.play`
  reaches the lobby, presses Esc, and the run reports `FAIL: process would quit at
  f=…` (documenting G-01) instead of exiting; `SK-A` runs to its first failing
  assertion (`credits > 10000 by f=1800`, documenting G-20) and writes
  `start.png`/`f0900.png`; two consecutive runs of `SK-A` print identical
  `play_hash` (fixed tick proven deterministic).
- **Gate policy:** adds the `--play` gate family; pins a `play_hash` only for scripts
  that pass. No existing gate moves.
- **Hotspots:** `run_scene_driver` (3966-3985), `platform.hpp` `InputEvent`/`Platform`
  (new `EventSource`), `GameScene` tail (new `probe()`), `main()` arg parsing
  (15780-15800), new `engine/play_script.cpp`.

### G1b — Parser hygiene (runs in parallel with G0)

- **Scope:** RC-5; the `Ini`-level half of G-41.
- **Why now:** every later phase reads rules; two more per-site patches would be two
  more bugs. Fixing it early means later re-pins are attributable.
- **Design:** `Ini` strips an inline `;` comment at parse time (the game's reader
  treats `;` as a comment anywhere); `strip_inline_comment` (`src/rules.cpp:48`),
  `ability_flag` (`main.cpp:904`) and `p6_flag` (`main.cpp:2050`) become thin
  wrappers; `Rules::weapon(unit, slot)` returns `Primary` else `Weapon1` (Secondary
  semantics stay in G4). Add a `--dump-rules` dev mode listing every (section, key,
  value) so before/after can be diffed.
- **RE needed:** no (the game's INI reader behaviour is documented in `rules.cpp:48`).
- **Acceptance:** unit tests on the literal lines `rulesmd.ini:6870`
  (`Turret=yes ;temp…` → true), `4140` (`Teleporter=yes;` → true), `6686`
  (`Weapon1=HoverMissile ;Normal` → `HoverMissile`), `6873` (SREF → `Comet`);
  the `--dump-rules` diff shows changes *only* on lines that had inline comments,
  attached to the re-pin commit.
- **Gate policy:** any gate whose world read a commented value may move; re-pin
  per fix with the dump diff as justification.
- **Hotspots:** `src/ini.cpp`, `src/rules.cpp/hpp`, `main.cpp:900-915`, `2050-2056`,
  `tests/test_ini.cpp`. No `engine/` logic.

### G1 — Session control

- **Scope:** G-01 (Esc), G-02 (skirmish win/lose), G-14 (Abort/Exit confirm,
  Controls page shell), the fixed 15 Hz accumulator half of G-13.
- **Why now:** without it no playthrough can end and a reflex key loses the session.
- **RE needed:** RE-I8 (victory presentation) and RE-I10/RE-I11 are
  confirm-only; ship per `multihouse.md §5`, `campaign.md §3.4`, `saveload.md §3`
  with `RE-FALLBACK` tags.
- **Acceptance:** `shell-esc` (Esc in menu → confirm, Esc again closes it; in lobby →
  back; in game → `InGameMenu` opens, Esc resumes; mid-placement → placement
  cancelled, still in game); `SK-A` variants with `cheat kill_house 1` → result
  scene within BorrowedTime → main menu; with `cheat kill_house 0` → LOST.
- **Gate policy:** none move.
- **Hotspots:** `platform.cpp:95-120` (key switch), `run_scene_driver`, `GameScene::
  handle` head (4520-4530) and the dormant cancel chain (4607-4611), `update`
  (new block after 4707), `InGameMenu` (4297-4341), `ACT_QUIT` (6726), abort (5046).
  **Merge G1 before G2 starts** — both edit `update` 4685-4710.

### G2 — Economy live

- **Scope:** G-20, G-21.
- **Why now:** the core loop (harvest → spend) is the game; everything after builds
  on money existing.
- **Design:** `wire_economy` inside `launch_skirmish` (complete real world; policy
  §G.4) and in the load-fail rebuild (4983); make `wire_economy` idempotent for
  `sl_build_skirmish` (15359). Single wallet: `World::house_credits_[0]` is
  authoritative, `prod_tick` syncs to/from it each frame, the repair bridge
  (4685-4692) and `mp_money()` seed (4441) go; AI double seed (2253 + `world.cpp:2254`)
  removed.
- **RE needed:** no (economy.md §1-§4; RE-E7 ore timer is a later fidelity hunt).
- **Acceptance:** `SK-A`: `credits(0) == 10000` at f=1; `until 1800 assert credits(0)
  > 10000` with no sell; ore density near the refinery falls; `credits(1)` rises;
  `f0900.png` shows the miner off the dock. Save → load → the miner keeps harvesting.
- **Gate policy:** `skirmish_hash`, `maps_hash` move (the skirmish world now
  harvests) → one justified re-pin; `saveload_hash` may move (idempotent wiring).
- **Hotspots:** `launch_skirmish` (11557-11633), `GameScene` ctor 4441, `update`
  4685-4710, `skirmish.cpp:27`, `make_ai_config` 2253.

### G3 — Side-aware player

- **Scope:** G-03, G-26, G-15 (skirmish half), G-91 (Soviet skin), remove Yuri.
- **Why now:** half the lobby choices are dead; the roster phase (G5) must be
  side-aware from day one.
- **Design:** one `PlayerSide` resolver from `cfg.slots[0].side` feeding the building
  filter (side from the ConYard token in `Prerequisite`, GACNST/NACNST, or `Owner=`
  once RE-E8 lands), `ur_`, `inf_ft_/veh_ft_`, SW type, `sidec0{1,2}` and the
  `SidebarLayout` constant set; `spawn_base` symmetric; `tech_level` passed to
  `can_start*` and the shade.
- **RE needed:** RE-E8 (Owner filter) — ship the prerequisite-token heuristic tagged
  `RE-FALLBACK`.
- **Acceptance:** `SK-S`: probe cameos are all `NA*`, infantry tab contains E2,
  `sidec02` chrome in `start.png`; build NAPOWR → NAHAND → E2 and `units(0,E2)` becomes
  1; TechLevel 1 greys HTNK/MTNK (probe). Yuri absent from the side cycle.
- **Gate policy:** none expected (`building_buildable_tab` is interactive-only; verify).
- **Hotspots:** `building_buildable_tab` (3221-3243) + callers (5078-5088), ctor
  4373/4425-4426, `spawn_base` (2290-2323), lobby (6794-6799), `sidebar.hpp:19-47`,
  `main.cpp:2078`, 11620 (SW type only).

### G4 — Combat foundation

- **Scope:** G-40 (defences fire), G-41 (weapon source/Secondary/elite), G-45 (GI
  deploy), G-06 (`is_enemy` gate), G-46 (acquire buildings), G-50 (ROF jitter, min
  damage 1, Verses 0, splash on buildings), G-29 (power gate for `Powered=yes`).
- **Why now:** combat is the other half of the loop; a Prism Tank that cannot fire
  and pillboxes that cannot shoot make any skirmish a farce.
- **RE needed (blocking, run first):** RE-W1 (`SelectWeapon`), RE-W2
  (`BuildingClass::AI` fire path). RE-W3 and RE-W4 may ship behind documented
  fallbacks (`rulesmd.ini:3498` for DeployFire; "idle units engage structures" as
  PROBABLE).
- **Acceptance:** `SK-A`: `cheat spawn E2 1 x y` ×3 next to a GAPILL → all dead within
  300 frames; Prism ordered on a building → its HP drops; deployed GI kills a target
  5 cells away and does not move; right-click on an ally → no attack order (probe);
  idle Grizzly engages an enemy building in range; APOC uses `MammothTusk` on a Kirov
  in `--demo-roster` (unit test) even though AA lands in G13.
- **Gate policy:** `combat`/`tank` logs and most sim hashes move (jitter, min
  damage) → re-pin citing `veterancy.md V7`, `combat.md §3`.
- **Hotspots:** `combat.cpp`, `building.hpp/.cpp` (weapon fields), `resolve_combat_unit`
  / `resolve_ability_profile` (695-1020), `load_building_assets` (1960-2000),
  `world.cpp` `step` (new building pass), `world_abilities.cpp:63-88, 479-490`,
  `input.cpp:542, 745`.

### G5 — Full roster, MCV deploy, queue semantics

- **Scope:** G-22, G-23, G-07, G-12, G-90 (anim registration for all types).
- **Why now:** the economy (G2) dead-ends without a rebuildable miner, the base
  dead-ends without an MCV, and 12/24 missions need deploy.
- **Design:** `load_player_roster(side)` builds `ur_` from the type lists filtered by
  side/Owner/`TechLevel>0`/cameo, reusing the full-parse loader; gate tables stay.
  AMCV/SMCV + `World::order_deploy_mcv` (`placement_check` with adjacency bypass at
  the MCV cell → `add_building(instant=false)` → erase unit); `SkirmishConfig::
  spawn_mcv` default true for the interactive config; D / double-click / right-click-
  self. Unit queues become vectors capped at `MaximumQueuedObjects`; right-click =
  hold → cancel+refund; count badge; `TopRow` strip scrolling.
- **RE needed (blocking):** RE-E3 (deploy validity/timing/undeploy). RE-E2 (starting
  force) and RE-I2 (strip semantics) ship behind `skirmish.md §3` /
  `construction.md §10` fallbacks.
- **Acceptance:** `SK-A` starts with an MCV; `key D` → ConYard buildup → cameos
  appear; GAWEAP → CMIN in the vehicle tab; click GI ×3 → badge 3; right-click holds,
  second cancels and refunds exactly the spent amount (probe); Engineer/Dog/
  Rocketeer/Mirage buildable; strip scrolls past row 9. `M-SOV01`: deploy the SMCV at
  f=30. Soviet roster in `SK-S` incl. HARV.
- **Gate policy:** `--demo-roster*` untouched; `--demo-skirmish` unchanged while its
  config keeps `spawn_mcv=false`; `--demo-prod` may move if `kUnits` changes (keep
  `kUnits` as the gate's loader).
- **Hotspots:** rosters (1528-1650), ctor 4373/4400-4415, `handle` sidebar clicks
  (4560-4600), `production.hpp` queues, `sidebar.cpp/hpp`, `world.cpp` (new deploy
  order), `launch_skirmish` spawn (11601-11615), placement confirm (4784-4800), D
  handler (4661-4669).

### G6 — Terrain and time calibration

- **Scope:** G-43 (passability), G-42 (speed scale), G-13 (GameSpeed table + knob),
  `set_water_terrain` in skirmish (the water-grid half of G-24).
- **Why now:** tanks crossing lakes and 5× speed break every later acceptance's
  timing; must be fixed before AI timing (G7) is tuned.
- **RE needed (blocking):** RE-T1 (frame rate per GameSpeed).
- **Acceptance:** `SK-A`: order a Grizzly across a lake → path avoids water (probe
  final cell is land; path length > straight line); a Speed-6 unit covers 10 cells in
  ≈170 frames ±10 % (0.879 cells/s); GameSpeed 0..6 changes real seconds per frame,
  not cells per frame; cliffs block infantry.
- **Gate policy:** all movement/combat gates move (speed) → one re-pin wave citing
  `voxel-render.md §5` + RE-T1.
- **Hotspots:** `world.cpp:208-231`/`world.hpp:470` (`build_passability`),
  `main.cpp:675` + infantry `move_cps` (700-760 — **after G4**), `update` accumulator
  (G1's block), `launch_skirmish` (one call — coordinate with G5), `InGameMenu`
  Controls page (G1's shell).

### G7 — AI live (may be split G7a / G7b, sequential)

- **Scope:** G7a: G-60 (teams + `aimd.ini`), G-61 (opcode table), G-62 (triggers),
  G-96 (X-1 bisect). G7b: G-63 (difficulty), G-64 (side-aware catalogs), G-65 (AI
  repair/sell), G-67 (centroid), G-68 (miner rebuy).
- **Why now:** with economy, combat and terrain calibrated the opponent can be made
  to play; until then "AI attacks" cannot be judged.
- **RE needed:** RE-A1 (blocking for the handicap mults; the index inversion is
  CONFIRMED in `ai-trigger.md §5`), RE-A6 (handlers 58/2/10/16), RE-A2 (repair gate),
  RE-A4 optional.
- **Acceptance:** `SK-A`: by f=4500 `units(1)` exceeds the start count (production),
  a house-0 building has taken damage from house 1 (attack), and the AI has rebuilt a
  destroyed power plant; `SK-S`: every house-1 building id is `GA*`; Brutal AI has
  more structures at f=3000 than Easy; the no-input `SK-A` variant ends **LOST**.
- **Gate policy:** `ai_team_hash`, `ai_trigger_hash` re-pinned only after X-1 explains
  the current drift; `skirmish_hash` moves (teams in `launch_skirmish`).
- **Hotspots:** `launch_skirmish` 11618 (**after G5**), `make_ai_config` (2178-2258),
  `make_ai_team_config` (3061-3122), `ai_team.hpp/cpp`, `ai_base.cpp:17-26`,
  `world.cpp` 2119-2123 / 2203-2367 / 2727-3000, ctor (aimd load, `ur_ai_`).

### G8 — Feedback layer

- **Scope:** G-80 (EVA set), G-81 (voices/reports for all units), G-08 (action
  cursors), G-11 (tooltips), G-10 (building selection/HP), G-09 (minimap click +
  buildings + power gate), G-84 (nothing drawn through shroud; edge frames if RE-P3
  lands), G-88 (path lines only for selection).
- **Why now:** the game is now mechanically playable but blind and mute; the user
  cannot tell what a click does or that a unit is ready.
- **RE needed:** RE-P1 (EVA set; ship the ids explicitly cited in `repair-sell.md`,
  `production.md`, `veterancy.md`), RE-I3 (cursor table), RE-I4 (radar), RE-I9
  (tooltip source), RE-P3 (shroud frames — the visibility test ships regardless).
- **Acceptance (screenshot + probe):** cursor frame over an enemy is the attack
  cursor (software cursor captured); hover 500 ms over GAPOWR → tooltip "Power Plant
  $800" in `.fnt`; EVA log shows `ConstructionComplete` at READY, `LowPower` on the
  outage edge once, `BaseUnderAttack` once per cooldown; minimap click moves `cam_`;
  pixels at an unexplored enemy building's cell are black; selecting a Grizzly logs
  its select voice; selecting a building shows its HP bar.
- **Gate policy:** none (presentation/audio unhashed).
- **Hotspots:** `init_audio` (592-646), `update` audio hooks (4815-4836), `render`
  (4844-4880), draw helpers (3519-3900: minimap 3541-3576, sidebar 3579-3745, cursor
  3796-3836), `render_world`/`draw_building` (2346-2890), `iso_renderer.cpp`,
  `handle` 4603 (radar click), `input.hpp` hover state.

### G9 — Hotkeys and the mouse model

- **Scope:** G-04 (KeyboardMD.ini hotkeys, stop/guard/scatter, groups, tabs, T/H/K/L/
  Space), G-05 (original left-click-orders model behind a setting), G-06 force-fire.
- **Why now:** units that cannot be stopped and no control groups make a real fight
  unmanageable; needed before anyone judges the AI fight in G7 fairly.
- **RE needed:** RE-I1 (mouse dispatch — keep the current model default until
  confirmed), RE-I5, RE-I6 (ship Stop→Guard per `repair-sell.md §C1`/ModEnc tagged
  `RE-FALLBACK`).
- **Acceptance:** `key S` halts a moving tank next frame (`moving==false`); `G` sets a
  guard anchor; `X` scatters to a free neighbour; `ctrl+1` then `1` reselects the
  group; `Q/W/E/R` switch tabs; `T` selects same type; `H` centres on the ConYard;
  `K`/`L` arm repair/sell; with the original model on, left-click ground moves and
  right-click deselects.
- **Gate policy:** none.
- **Hotspots:** `platform.cpp` key switch (**after G1**), `input.cpp` (Key handling,
  groups, orders), `update` raw-scancode polling (4624-4675, removed), `world.cpp`
  (`order_stop/guard/scatter_selected`), new `engine/hotkeys.cpp`.

### G10 — Superweapons by building

- **Scope:** G-25, G-66 (AI fires), SW half of G-29; stub launch visual/sound until
  G14's AnimList.
- **Why now:** a free nuke at frame 0 breaks every balance judgement; the AI needs
  it to be a threat.
- **RE needed:** RE-E9 (grant/revoke on place/die), RE-A3 (AI target), RE-E4.
- **Acceptance:** `SK-A`: no SW cameo at start; GAWEAT (Allied) / NAMISL (Soviet) →
  cameo and charge; selling it removes both; low power pauses the clock (probe);
  AI with a silo fires on the player's densest cluster when ready (probe damage
  event); the cameo lives in the strip, not over the minimap.
- **Gate policy:** `--demo-superweapon` unchanged if it grants directly;
  `skirmish_hash` moves (no per-slot grant) → re-pin.
- **Hotspots:** `world.cpp` `add_building`/building death (~883, 1615),
  `step_superweapon` (1388), `launch_skirmish` 11618-11624 (**after G7**), SW cameo
  code 3492-3517 / 3740-3743 / 4531-4540 (**after G8**), `superweapon.cpp`.

### G11 — Campaign mission-1 hardening (parallel-friendly)

- **Scope:** G-15 (campaign TechLevel), G-74 (events 44/29/25/26), G-75 (actions
  108/9/101), G-73 (reinforcement placement incl. sea), G-77 (Sleep/Harmless,
  difficulty mods), G-95 (confirm + re-pin `--demo-triggers`).
- **Why now:** mission 1 of each campaign is in the core; this phase barely touches
  `main.cpp`, so it can run alongside the skirmish work.
- **RE needed:** RE-C5 (event raise points), RE-C4 (108/9/101), RE-C1 (delivery).
- **Acceptance:** `M-ALL01` and `M-SOV01` reach **WON** through the maps' triggers by
  scripted *player* actions (Tanya C4 on the target in ALL01; deploy + build + attack
  in SOV01 — SMCV deploy itself comes from G5); probe: Prism Tower absent at TL3;
  event 44 fires when a scripted enemy attacks; action 108 raises `crates()`;
  reinforcements with a sea waypoint land on the nearest shore, not inland.
- **Gate policy:** `campaign_hash`, `trigger_hash` move → re-pin; G-95's new hash is
  accepted only after the witness-log diff shows the extra objects as the only delta.
- **Hotspots:** `campaign_runtime.inc`, `campaign.cpp`, `map_trigger.cpp`,
  `main.cpp:5177-5185` (tech level, small).

**Core complete** when G0–G11 are merged and the four core scripts plus the shell
smoke set are green on the merged tree, with every moved hash re-pinned under §G.

### Tier 2 (after the core)

| Phase | Scope | RE | Interactive acceptance | Gate policy | Hotspots |
|---|---|---|---|---|---|
| **G12 Campaign enemy AI** | G-69, G-76, G-110 | RE-A5 (blocking), RE-A6 | ALL03/SOV03 scripts: enemy house builds ≥3 structures after `ProductionBegins`, produces, attacks | `campaign_hash` re-pin | `campaign_runtime.inc setup_houses`, `map_trigger.cpp:838`, `make_ai_config` (after G7) |
| **G13 Naval and air** | G-24, G-44, G-51 | RE-W1, RE-W8 | GAYARD placed on water, Destroyer launches to a water cell, Harrier sorties/returns/rearms, Flak Track shoots a Rocketeer, LCRF crosses beach→water | `--demo-naval/aircraft/roster2` may move | `production.hpp QueueCat`, handoff 4710-4730, `roster2.*`, `world.cpp` flight, placement 4787 |
| **G14 Presentation** | G-83 AnimList + projectile art, G-82 music, G-86 theater art, G-87 map dressing + neutral structures in skirmish, G-16 house colours/country, G-88 rest, G-52 | RE-P2, RE-P5, RE-P6, RE-E6 | Impact frame shows the warhead anim; nuke has missile + siren; music probe lists a `Normal=yes` track; snow map shows snow building art; 3-player skirmish shows 3 colours; trees block movement | none (render) except `skirmish_hash` if map objects enter `launch_skirmish` | `render_world`/`draw_*` 2346-2890, loaders 172-335/1079/1909/1936, `launch_skirmish`, new `engine/anim.cpp` |
| **G15 Android input** | G-70, G-71, G-72 | no | Harness `touch_tap/touch_drag` commands drive the gesture layer on desktop; on-device smoke via `gradlew assembleDebug` + `adb` screenshot of `SK-A` first 300 frames; README asset list corrected | none | `platform.cpp` touch 147-171 (after G1, G9), `input.cpp` gesture layer (after G9), `main()` 15748 |
| **G16 World depth** | G-47 crush, G-48 garrison, G-49 bridges, G-27 inert buildings, G-28, G-30, G-31, G-85 building anims | RE-W6, RE-E1, RE-E5, RE-E6, RE-P5 | Rhino squishes a GI; GIs garrison a civilian block and fire out; bridge crossing and destruction raises event 31; walls line-fill at 3× speed; ore purifier bonus visible in the probe | several re-pins | `world.cpp` movement, `building.*`, `campaign_runtime.inc`, draw code |
| **G17 Shell and lobby polish** | G-17, G-78, G-89, G-16 lobby country cycle, G-14 leftovers | RE-C2, RE-C3 | AI count clamps to starts; difficulty seeds from settings; Boot Camp entry; map preview; resizable window | none | lobby 6397-6540, shell 6127-6170, `platform.cpp:21-56` |

## Parallelism and conflict hotspots

`engine/main.cpp` is the shared battlefield; `world.cpp` (3,901 lines) and
`input.cpp`/`platform.cpp` are the next. The table says who may run together.

| Wave | Phases in parallel | Why they do not collide | Serialisation rules |
|---|---|---|---|
| 0 | **G0 ‖ G1b** | G0 edits the driver/`Platform`/`GameScene` tail/`main()`; G1b edits `src/` only. | Both merge before wave 1. |
| 1 | **G1 ‖ G4 ‖ G11** | G1: `platform.cpp` keys, driver, `handle` head, `update` tail, menus, shell. G4: `combat.cpp`, `building.*`, 695-1020, `world.cpp step`. G11: `campaign_*`, `map_trigger.cpp`. | **G1 merges first** (small); G2 must not start until it has. |
| 2 | **G2 ‖ G3 ‖ G9** | G2: `launch_skirmish`, ctor 4441, `update` 4685-4710, `skirmish.cpp`. G3: `building_buildable_tab`, ctor 4373/4425, 5078-5088, lobby, `sidebar.hpp`. G9: `platform.cpp` keys, `input.cpp`, `update` 4624-4675, `world.cpp` orders. | Ctor and `update` hunks are distinct but adjacent: rebase before merge, run `SK-A` on the merged tree. |
| 3 | **G5 ‖ G6 ‖ G8** | G5: rosters, ctor roster lines, `handle` sidebar, `production.hpp`, `sidebar.cpp`, `world.cpp` deploy, `launch_skirmish` spawn. G6: `world.cpp` passability, 675-760, `update` accumulator, `launch_skirmish` water call. G8: draw helpers, `init_audio`, `update` audio, `render`, `iso_renderer.cpp`. | `launch_skirmish` is touched by G5 (spawn) and G6 (one call): G6 adds its line at the top of the function; G5 edits the loop. `sidebar.cpp` (G5) vs `draw_sidebar` (G8): G8 must not change layout structs. |
| 4 | **G7 ‖ G10 ‖ G15** | G7: `make_ai_*`, `ai_*.cpp`, `world.cpp` AI, `launch_skirmish` 11618. G10: `world.cpp add_building`/SW, 11618-11624, SW cameo code. G15: `platform.cpp` touch, `input.cpp` gestures. | G7 and G10 both touch 11618-11624 and `world.cpp step_ai_houses`: **G7 merges first**, G10 rebases. G15 waits for G9's `input.cpp` merge. |
| 5 | **G12 ‖ G13 ‖ G14 ‖ G16 ‖ G17** | Mostly disjoint files; G13/G14 both touch `launch_skirmish` and the draw code. | G13 before G14 in `launch_skirmish`; G14 owns `render_world`. |

Standing conflict hotspots in `main.cpp` (every phase touching one must say so in
its plan): `launch_skirmish` 11557-11633 (G2, G3, G5, G6, G7, G10, G14);
`GameScene` ctor 4345-4506 (G0, G2, G3, G5, G7); `GameScene::update` 4614-4890
(G1, G2, G6, G8, G9); `GameScene::handle` 4520-4612 (G1, G5, G8); sidebar/cursor
draw helpers 3519-3900 (G8, G10, G5 via `sidebar.cpp`); `platform.cpp` (G0, G1, G9,
G15). A phase that needs a hotspot owned by an in-flight phase waits or lands a
minimal, pre-agreed hunk.

## §G — Gate policy for this roadmap

1. Determinism (`deterministic=yes`, rendered == headless) is the invariant; a moved
   hash is a behavioural change to explain, not a defect.
2. One re-pin commit per logical fix: `gate: re-pin <gate> — <G-id> <reason>`, with
   old → new hash and the RE citation in the README gate row and the ledger.
3. Never re-pin an unexplained drift (G-96 → X-1 bisect first; G-95 → witness diff).
4. Prefer completing the real world (`launch_skirmish`) over hash-neutral wiring in
   `GameScene`; the `--play` gate (G0) hashes the real path from now on.
5. Full gate batch once per phase at the end (plus the one affected gate during
   development); gates are ~2 min / 3 GB each — never in parallel, never
   misdiagnosed as hung.
6. The four core `--play` scripts run on every merged tree; their `play_hash` is
   pinned once a script passes and re-pinned under rule 2 thereafter.

## Non-goals (unchanged)

Yuri faction; multiplayer netcode; map editor; mod support.

## Ledger

Durable progress lives in `.superpowers/sdd/progress.md` (gitignored). This file
records the plan; the ledger records what actually happened.
