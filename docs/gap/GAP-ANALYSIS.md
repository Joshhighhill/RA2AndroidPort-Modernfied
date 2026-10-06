# Gap analysis — why "nothing is working" in real play (synthesis)

Synthesised 2026-10-06 from the six read-only audits of master `f9b93ba` in this
directory (`01-controls-ui.md`, `02-economy-production.md`, `03-combat-units.md`,
`04-ai.md`, `05-render-audio.md`, `06-shell-campaign-android.md`) and the finished
`docs/superpowers/ROADMAP-completion.md` (P1–P10). Citations are `file:line` on
master at that commit; "01#36" means audit 01, feature row 36. No source was changed.
The companion fix queue is `docs/superpowers/ROADMAP-playable.md`.

## Executive summary

All ten roadmap phases shipped and all 20 byte-identity gates were green at merge
time, yet the game a person actually launches (`main()` → `run_menu` → `GameScene`,
`main.cpp:4343`) is not playable: Escape terminates the process from any screen
(`platform.cpp:102` → `main.cpp:3972`); a skirmish starts with a pre-built ConYard,
no harvesting and no ore growth because `launch_skirmish` (`main.cpp:11557-11633`)
never calls `wire_economy`, so the only money is the (doubled) starting cash; the
player can build exactly five Allied unit types (`kUnits`, `main.cpp:1528-1545`) —
one of which, the Prism Tank, has no weapon — while 37 recovered types live in
`[[maybe_unused]]` loaders called only by `--demo-roster*`; picking Soviet in the
lobby yields a Soviet base with an Allied sidebar and factories that produce nothing
(`main.cpp:4373, 4425-4426, 5078-5088`); the AI builds a few structures and then never
trains a unit or attacks because `AiTeamConfig.enabled` is only ever set by the two
AI gates (`main.cpp:9227, 9476`; `world.cpp:2730`); base defences have no weapon
model and never fire (`building.hpp:17-69`, `combat.cpp:308-315`); ground units drive
across water and cliffs (`world.cpp:208-231`); and the skirmish can neither be won
nor lost because `GameScene::update` reads `house_defeated`/`winning_team` nowhere
(`main.cpp:4694`). Every one of these subsystems *exists and is byte-identity
verified* — inside a `--demo-*` gate. The real game is the one caller that was
firewalled out of them, and nothing in the process ever exercised the real game.

## Root causes (systemic first)

| # | Root cause | Evidence | Consequence |
|---|---|---|---|
| RC-1 | **Gate-only firewalling as the method.** Every phase had to keep the existing gate hashes bit-identical, so new behaviour was armed behind a flag/call that only the phase's own gate set. The interactive path was never one of the "callers that open the firewall". | `world.cpp:3219-3231` (`if (!types_.empty())` around ore/harvest); `world.cpp:2730` (`team.enabled`); `world.hpp:523` (`triggers_enabled`); `main.cpp:1582, 1613` (`[[maybe_unused]]` rosters "Called ONLY by --demo-roster"); `main.cpp:11568-11573` comment "must NOT arm ore growth/harvest stepping (that is what would move skirmish_hash)"; `sl_build_skirmish` (`main.cpp:15354-15359`) describes itself as "the interactive game's World plus every firewall the real game opens" and has to add `wire_economy` itself. | Economy, AI teams, AI triggers, 37 units, naval/air production, water grid, map objects, wall line-fill, Sortie, SW-by-building all GATE-ONLY in real play (G-07, G-08, G-10, G-12, G-30, G-31, G-33). |
| RC-2 | **No interactive verification, ever.** There is no harness that drives the real `GameScene` with input and asserts anything; `run_screenshot` renders one frame of a hand-built world, `--menu-shot` renders the shell, and the gates build their own worlds. Audits 01–06 all note the engine was not even launched during the audit. | `main.cpp:2917` (`run_screenshot` builds its own world), `main.cpp:5267` (`run_interactive` is a bare launcher with no assertions), no `tests/test_*scene*` or playthrough test. | Regressions like RC-3 and the Esc quit were invisible; "WORKS" in the README meant "gate passes". |
| RC-3 | **Regression `f77e68e` (scene framework) deleted the interactive wiring and was called "behavior-preserving".** | `git show f77e68e -- engine/main.cpp` removed `wire_economy(world, ba, rules)`, the AI house's `spawn_base(..., with_refinery=true)`, the `aimd.ini` load and `acfg.team = make_ai_team_config(...)` from the old `run_interactive` body (04 §"Regression origin"). | G-07, G-08, G-30 were once live and silently went dark. |
| RC-4 | **Stale README claims.** | README AI-3 paragraph "the interactive enemy Soviet house now runs all three AI layers at once" (pre-`f77e68e`, 04); README:2796/2893 "tap to select, tap to move" on Android (touch is Left-only, 06#35); README:2863-2870 lists three mixes when ten are required (06#39); "Harvesters WORK" style claims in the P-phase sections describe gate worlds. | The user's expectation and the documented state diverged; nobody re-checked. |
| RC-5 | **Shared parser bug: `Ini` keeps inline `;` comments and tolerant-parsing is not centralised.** `src/ini.cpp:66` stores `trim(value)` with the comment attached. `strip_inline_comment` (`src/rules.cpp:48`) and `ability_flag` (`main.cpp:904`, written because "`Teleporter=yes;` carries a trailing ';' on this install") are per-call-site patches; `Rules::section_flag` → `is_truthy` (`rules.cpp:252`, `15-20`) does an exact `"yes"`/`"true"` compare and `Rules::primary` (`rules.cpp:109`) and the `p6_flag` lambda (`main.cpp:2050-2054`) read raw values. Real `rulesmd.ini` lines: `[SREF] Turret=yes ;temp until tank art done` (6870), `Teleporter=yes;` (4140, 4706), `Naval=yes ;GS`, `GTGCAN   ; Grand Cannon`, `Weapon1=HoverMissile ;Normal` (6686). | Any boolean read through `section_flag` on a commented line is silently false; any string read without stripping carries garbage. Combined with the Primary-only weapon resolver (RC-6) it is why the Prism Tank is a paperweight (G-18). Fixing it at the `Ini` level changes values everywhere → gate re-pins (see policy). |
| RC-6 | **Weapon source = `Primary=` only.** `resolve_combat_unit` (`main.cpp:695-700`) reads `rules.primary()`; no `Secondary`, no `WeaponCount/Weapon1..N`, no `ElitePrimary/EliteWeapon1` projectile, no AA/AG choice. `[SREF]` has `; Primary=Comet` commented out and `WeaponCount=1 Weapon1=Comet` (`rulesmd.ini:6859-6873`). | SREF damage/rof/range = 0 (03); no AA anywhere (03); APOC/HTK/GGI secondary weapons unused. |
| RC-7 | **Side derived from the id prefix `"GA"`/`"NA"`, not from rules.** `building_buildable_tab` (`main.cpp:3221-3243`), the AI catalog (`make_ai_config`, `main.cpp:2199-2210`, Soviet-only), `make_ai_team_config` (`trigger_side=2`, `house_name="Russians"`, `main.cpp:3108-3118`), and `can_start` (`production.cpp:65-73`, no Owner check). | GTGCAN/ATESLA/TESLA/NASAM land in the wrong (or no) tab; Allied AI slots build Soviet bases beside their Allied yard; Soviet human gets GA* cameos (G-03, G-20, G-32). |
| RC-8 | **Two wallets and wall-clock simulation.** `GameScene::player_credits_` (`main.cpp:4441`) vs `World::house_credits_[0]` (seeded by `skirmish.cpp:27`) bridged by `take_income` (`main.cpp:4707`, `world.cpp:658-662`); `world_.step(dt)` on wall-clock `dt` (`main.cpp:4691`) with `kCellsPerSecPerSpeed=0.75` (`main.cpp:675`) vs the recovered 0.146 cells/s per Speed point at 15 Hz. | Starting cash doubled (G-09); repair needs a per-frame bridge; everything moves ~5× faster than the ROF scale; no game-speed control; the interactive path is non-deterministic (so it could never have had a gate). |

## Merged issue list

Status: WORKS / GATE-ONLY / PARTIAL / WRONG / BROKEN / MISSING. Severity: BLOCKER /
MAJOR / MINOR. Effort: S (<1 day) / M (1–3 days) / L (>3 days). "Needs RE?" names the
hunt in the RE section below; "no" means the behaviour is already pinned in
`docs/re/*.md` or the rules data.

### A. Session, controls, UI

| ID | Issue | Area | Status | Sev | Root cause | Fix sketch | Effort | Depends on | Needs RE? |
|---|---|---|---|---|---|---|---|---|---|
| G-01 | **Esc exits the program** from menu, lobby, game, save dialog, mid-placement. `platform.cpp:102` maps `SDLK_ESCAPE`→`Quit`; `run_scene_driver` returns on `Quit` before `scene->handle` (`main.cpp:3972`). The cancel chain (`main.cpp:4607-4611`), `InGameMenu` Resume (`:4303`) and `SaveLoadDialog` Back (`:4075`) are dead. Sources: 01#1, 06#1. | Controls | BROKEN | BLOCKER | RC-2 | Add `kKeyEscape`; keep `Quit` for `SDL_QUIT` only; route: dialog→Back/Resume, armed mode→`cancel_one_mode`, in game→open `InGameMenu`, shell→Back/Exit-confirm (`FUN_004AAC40` chain, repair-sell.md §C1). | S | — | RE-I10 (confirm fallthrough to 0xB5; ANCHORED) |
| G-02 | **Skirmish never ends.** `World::step_victory` sets `house_defeated_`/`winning_team_` but `GameScene::update` only checks `campaign_->finished()` (`main.cpp:4694`); `wants_quit` is input/abort only (`:4888`). 01#2, 06#2. | Session | MISSING | BLOCKER | RC-2 | In `update()`: `!campaign_ && victory_enabled()` → `house_defeated(0)`=LOST, `winning_team()==team(0)`=WON; BorrowedTime; result scene (reuse `CampaignResultScene` w/o Continue) → main menu; EVA victorious/lost. | S | G-01 | RE-I8 (score-screen path, MP BorrowedTime) |
| G-03 | **Soviet (or Yuri) human is unplayable.** Base = NACNST set (`main.cpp:2290-2301`) but sidebar prefix "GA" (`:5078-5081`), roster = Allied `kUnits` (`:4373`), factories GAPILE/GAWEAP (`:4425-4426`) → `factory_category_lost`, no unit ever; SW always Nuke (`:11620`); Yuri selectable (`:6794-6799`) with no content. 01#36, 02#19, 03 roster, 06#3. | Lobby/Sidebar | WRONG | BLOCKER | RC-7 | Derive prefix/roster/`inf_ft_/veh_ft_`/SW type/sidebar skin from `cfg.slots[0].side` (generalise the P10 `soviet_campaign()` branch at `:5177-5185`); symmetric `spawn_base`; remove Yuri. | S | — | no |
| G-04 | **Keyboard delivers 7 keys**; no Stop/Guard/Scatter/groups/Q-W-E-R tabs/T/H/K/L/Space; `GameScene::update` polls raw scancodes for arrows/D/N (`main.cpp:4624-4675`); `input.cpp` handles no `Key`. Units cannot be told to halt. 01 F2/#9/#10/#13/#14/#28, 03 orders, 06#34. | Controls | MISSING | MAJOR | RC-2 | Parse `KeyboardMD.ini [Hotkey]` (VK + 256/512/1024 modifiers) with defaults; `Platform::poll` emits all keys + modifiers; `World::order_stop_selected`, `order_guard_selected`, scatter; `std::array<std::vector<int>,10>` groups in `Input`. | M | G-01 | RE-I5 (group storage/center), RE-I6 (Stop→Guard vs Sleep, Guard radius, Scatter cell) |
| G-05 | **Mouse model inverted vs original**: left = select only, right = orders (`input.cpp:294-354, 507-764`); original left-click issues orders with a selection, right-click deselects/cancels/drag-scrolls (`ScrollMethod`, RA2MD.INI). 01#5. Blocks touch (G-70). | Controls | WRONG | MAJOR | design choice | Implement original scheme as default, keep current behind a setting. | M | G-01 | RE-I1 (`DisplayClass` left/right dispatch) |
| G-06 | **Attack gate is `house != 0`, not `is_enemy`** (`input.cpp:542, 745`) → right-click on an ally orders an attack; no force-fire (Ctrl), no attack-move. 01#12, 03 orders. | Controls | WRONG | MAJOR | — | `world_.is_enemy(0,h)` in both gates; Ctrl = force-fire via `order_attack_cell`. | S / M | — | no |
| G-07 | **Build-queue semantics wrong**: left-click on a busy cameo cancels (`main.cpp:4568-4572, 4589-4593`), right-click no-op (`:4603`), unit queues hold one item (`production.hpp:67-70, 146`), no hold, no count badge (`MaximumQueuedObjects=29`). 01#18, 02#11/#30. | Sidebar | WRONG | MAJOR | — | Right-click = hold→cancel+refund; left on busy unit queue = enqueue (vector, cap 29); badge via `draw_number`. | M | — | RE-I2 (`StripClass::Action` exact semantics) |
| G-08 | **No action cursors** — OS arrow everywhere except repair/sell (`main.cpp:3796-3836, 4865-4873`) although `mouse.shp` is loaded (`:4493-4504`). 01#11, 05 cursors. | UI | PARTIAL | MAJOR | — | Per-frame "what would a click do" probe → `ModeCursor` {select, move, nomove, attack, enter, capture, deploy, scroll×8, SW}. | M | G-05 | RE-I3 (cursor table 0x0082C038 index→action) |
| G-09 | **Minimap non-interactive and incomplete**: no click-to-centre, no buildings, flat green terrain, no viewport box, no power gate, SW cameo parked over it (`main.cpp:3541-3576, 3601, 3740-3743`). 01#26/#27, 05 radar. | UI | PARTIAL | MAJOR | — | Hit-test `lay_.radar` → `cam_`; draw buildings + viewport; power gate; move SW cameo into the strip. | S / M | G-29 (power) | RE-I4 (`RadarClass` click/blips/boot anim/colour source) |
| G-10 | **Buildings cannot be selected** (no `selected` field, no HP bar on selection; only factories for rally, `input.cpp:259-292`). 01#31, 05 bhp. | UI | MISSING | MAJOR | — | `selected_building_` + HP bar for any own building. | S | — | no |
| G-11 | **No tooltips / status line** (`ToolTips` persisted, no consumer, README:1954). 01#39, 05. | UI | MISSING | MAJOR | — | Hover timer over `lay_.cameos` → `UIName` (CSF) + `Cost` via `draw_fnt_text`. | S | — | RE-I9 (CSF key family, status line 0x695) |
| G-12 | **Sidebar strip scrolling inert** (`main.cpp:3638-3641`, `sidebar.cpp:44-53`); breaks the moment the roster grows. 01#38. | Sidebar | MISSING | MINOR→MAJOR after G-10/G-16 | — | `TopRow` in `SidebarLayout`; wheel + gadgets 200/201 + `SidebarUp/Down`. | S | — | no |
| G-13 | **No game speed / fixed logic tick**; `world_.step(dt)` on wall-clock (`main.cpp:4691`); no in-game Controls page (0xBBB). 01#24, 06#11, 03 Q1. | Session | MISSING | MINOR (MAJOR for determinism/harness) | RC-8 | Fixed 1/15 s accumulator with a speed multiplier from settings; Controls page in `InGameMenu`. | S | — | RE-T1 (GameSpeed→frame-delay table) |
| G-14 | **In-game menu lacks Controls; Abort and Exit have no confirm** (`main.cpp:5046-5050, 6726`). 01#23, 06#12/#19. | Shell | PARTIAL | MINOR | — | Confirm dialogs (reuse `SaveLoadDialog::ask`); Controls page. | S | G-01 | RE-I11 (0x120/0x121 strings) |
| G-15 | **TechLevel ignored**: lobby slider stored (`main.cpp:6513`) never consumed; `can_start` tests `>0` only (`:3231`); campaign `[<house>] TechLevel` never read (mission 1 can build Prism Towers). 02#15, 06#9/#23. | Production | BROKEN | MINOR (skirmish) / MAJOR (campaign) | — | Pass `cfg_.tech_level` (or the mission house's) into `can_start*` and the sidebar shade. | S | — | RE-A5 (BuildLevel vs TechLevel in campaign) |
| G-16 | **Lobby colour and country have no effect; house colours wrong for ≥3 houses**: `pal_for_color` never called (06#5); voxels red for every house≠0 (`main.cpp:2460`), SHP/buildings blue for every house≠1 (`world.cpp:296-310`); country fixed (`skirmish.cpp:8-12`), `RequiredHouses/ForbiddenHouses` and cost multipliers unread (02#18). 05 house colours. | Lobby/Render | WRONG | MAJOR (3+ players) | — | Rasterise voxel facing sets per `color_ix`; route `draw_unit/draw_building` through `house_palette`; country cycle → side; parse house restrictions. | M | — | RE-E8 (Owner/RequiredHouses), RE-E6 (country mults) |
| G-17 | Lobby AI count may exceed map starts (stack on `starts[k % size]`, `main.cpp:11594`); one shared difficulty. 06#6. | Lobby | PARTIAL | MINOR | — | Clamp to `start_waypoints().size()-1`; per-slot difficulty. | S | — | RE-C2 (start randomisation) |

### B. Economy, production, base

| ID | Issue | Area | Status | Sev | Root cause | Fix sketch | Effort | Depends on | Needs RE? |
|---|---|---|---|---|---|---|---|---|---|
| G-20 | **Economy firewall shut in real play.** `launch_skirmish` never calls `wire_economy`/`set_economy` → `types_` empty → `World::step` skips `ore_.step` and `step_harvest` (`world.cpp:3219-3231`). Free miners spawn and stand still; ore is a static painting; `take_income` returns 0 for player AND AI; AI stalls after ~5 structures. `--demo-saveload`'s `sl_build_skirmish` proves the fix is one call (`main.cpp:15359`). 02#7/#8/#9/#29, 04#1, 03 wiring table. | Economy | GATE-ONLY | BLOCKER | RC-1, RC-3 | `wire_economy(world_, ba_, rules_)` in the `GameScene` ctor after `launch_skirmish` (`main.cpp:4379`) and in the load-fail rebuild (`:4983`) — or inside `launch_skirmish` with a re-pin of `skirmish_hash`/`maps_hash`. | S | — | no (RE-E7 ore timer is a fidelity follow-up) |
| G-21 | **Starting credits doubled; slider ignored for the human.** `player_credits_ = mp_money()` (10000, `main.cpp:4441`) + `house_credits_[0] = starting_cash` (`skirmish.cpp:27`) drained by the first `take_income` (`:4707`). AI also doubled (`main.cpp:2253` + `world.cpp:2254`). 02#2, 06#7. | Economy | WRONG | MAJOR | RC-8 | Single wallet: hold the player's money in `World::house_credits_[0]` and read it from the scene (removes the repair bridge at `:4685-4692`); or at minimum `player_credits_ = cfg.starting_cash` and no house-0 seed. | S / M | — | no |
| G-22 | **Only 5 units buildable** (E1, GGI, MTNK, FV, SREF; `kUnits` `main.cpp:1528-1545`); the 12 `kRosterUnits` + 24 `kRoster2Units` are gate-only (`:1582, 1613`); Soviet campaign gets 4 (`:1559-1566`). No Engineer, Dog, Spy, Tanya, SEAL, Rocketeer, Legionnaire, Mirage, **no CMIN/HARV rebuild, no MCV**, no naval, no air. P4 abilities are registered but no ability unit is producible. 01 F3/#16/#34/#35, 02#17, 03 roster matrix, 04 (AI roster). | Production | GATE-ONLY | BLOCKER | RC-1 | Build the player `ur_` from `[InfantryTypes]/[VehicleTypes]/[AircraftTypes]` filtered by side/Owner + `TechLevel>0` + cameo (the loader already full-parses prereqs); keep the gate tables untouched; split Naval/aircraft to G-24. | M | G-03, G-12 | RE-E8 (Owner filter) |
| G-23 | **No MCV, no deploy.** `grep -i mcv engine/` → nothing beyond a comment (`main.cpp:11588`); skirmish pre-spawns a finished ConYard with adjacency bypass (`:11601`, `2296`); `UnitCount` spawns identical E1/E2 (`:11603-11615`); ConYard loss is permanent; AI never deploys; **12 of 24 campaign missions hand the player an MCV and are therefore unwinnable** (06 §2). 01#37, 02#1, 03, 04#6, 06#8/#24. | Production/Campaign | MISSING | BLOCKER (campaign) / MAJOR (skirmish) | RC-1 | AMCV/SMCV roster entries (`Prerequisite=GAWEAP,GADEPT`); `World::order_deploy_mcv`: `placement_check(bypass adjacency)` with the ConYard foundation at the MCV cell → `add_building(instant=false)` + erase unit; D/double-click UI; `SkirmishConfig::spawn_mcv` (default off for the gate); AI "deploy if no ConYard" step. | M | G-22 | RE-E3 (deploy validity, timing, undeploy, `MCVRedeploys`), RE-E2 (starting force) |
| G-24 | **Naval and aircraft production gate-only**: four `QueueCat`s (`production.hpp:172`), GAWEAP-only handoff (`main.cpp:4425-4426, 4710-4730`); `set_water_terrain` never called in skirmish (water grid absent, `world.hpp:611-618`); GAYARD placeable on land (`:4787-4790`); `is_aircraft` only for the Jumpjet CLSID (`:739-742`); `Sortie` driven only by `--demo-roster2`. 01#17, 02#23/#24, 03 naval/aircraft rows. | Production/Movement | GATE-ONLY / WRONG | MAJOR | RC-1 | `set_water_terrain` in `launch_skirmish`; `WaterBound` placement branch; Ships + Aircraft queues (tab 3 merge); `launch_naval` from roster2; fixed-wing flight state (pad, ammo, RTB) driven from `Sortie`. | L | G-22, G-43 | no (roster2.md §B/§C, naval.md, aircraft.md) |
| G-25 | **Superweapons: Nuke for everyone at frame 0, SW buildings inert, instant invisible detonation.** `launch_skirmish` grants per slot (`main.cpp:11618-11624`); building NAMISL/GACSPH/GAWEAT grants nothing; charge ignores power; Allied gets Nuke not Weather Storm; AI never fires (04#11); no missile anim/sound (05). 01#25/#26, 02#25, 06#10. | Production/Combat | WRONG | MAJOR | RC-1 | Grant/revoke on Buildup→Idle / death of a type with `SuperWeapon=`; side-correct type; `IsPowered` gate; AI fire at densest enemy cluster; delay damage to impact with anims (G-62). | M | G-29, G-62 | RE-E9 (SuperClass create/destroy), RE-A3 (AI target) |
| G-26 | **Sidebar building filter is a two-letter id prefix** (`main.cpp:3221-3243`): excludes GTGCAN, ATESLA, TESLA, AMRADR; Soviet campaign shows NASAM (Allied Patriot); `Owner=`/prereq-side never read (`grep Owner src/rules.cpp` → none). 01#15, 02#16, 03 defences filter. | Production | WRONG | MAJOR | RC-7 | Side from the ConYard token in `Prerequisite` (GACNST/NACNST) or `Owner=` ∩ country; drop the prefix test; hide inert buildings (G-27). | S | — | RE-E8 |
| G-27 | **Placeable-but-dead buildings**: GADEPT (no `UnitRepair`), GACSPH/GAWEAT (G-25), GAGAP, GAOREP (`PurifierBonus` unapplied, `harvest.cpp:418`), NAINDP (`FactoryPlant` mult 1.0, `world.cpp:1582`), CAOILD income only in the capture demo (`main.cpp:10987`). 02#16/#26/#28. | Production | MISSING | MINOR | RC-1 | Count own Idle purifiers in the unload branch; cost mults in `start_unit`; depot heal loop; `register_tech_income` for captured derricks. | S each / M depot | G-20 | RE-E6 |
| G-28 | `BaseNormal=no` not honoured in `placement_check` (GAWALL/GAPILL extend the base). 02#4. | Production | PARTIAL | MINOR | — | `base_normal` in `BuildingTypeDef`; require it on the neighbour. | S | — | RE-E5 |
| G-29 | **Low-power secondary effects missing**: radar, `Powered=yes` defences and SW charge keep working with no power (`main.cpp:3523-3532`, `world.cpp:1388`, no power gate in `combat.cpp`). 02#6. | Production | PARTIAL | MINOR | — | `World::house_low_power(h)` from `power_totals`; gate radar, `Superweapon::tick`, building `try_fire` for `Powered=yes`. | S | G-40 | RE-E4 |
| G-30 | **Walls build at 1× with no line fill** (helpers called only by `--demo-roster2`, `main.cpp:13280-13315`). 02#22, 03 walls. | Production | PARTIAL | MINOR | RC-1 | Call `wall_line_fill`/`wall_build_frames` from the placement confirm (`:4783-4800`). | S | — | no |
| G-31 | Prerequisite "owned" requires `State::Idle`; original may count at placement. 02 Q1. | Production | PARTIAL | MINOR | — | Decide after the hunt. | S | — | RE-E1 |

### C. Combat, units, movement

| ID | Issue | Area | Status | Sev | Root cause | Fix sketch | Effort | Depends on | Needs RE? |
|---|---|---|---|---|---|---|---|---|---|
| G-40 | **Base defences never fire.** `BuildingTypeDef` has no weapon fields (`building.hpp:17-69`); no building-combat pass in `World::step`; `combat.cpp:308-315` "buildings never retaliate". GAPILL/NASAM/TESLA/NAFLAK/NALASR/GTGCAN are props for player and AI. 03. | Combat | MISSING | BLOCKER | RC-1 | Resolve `Primary/Secondary/ROT/Turret` into `BuildingTypeDef`; a `Unit`-shaped shooter record per defence sharing `try_fire`/`launch_projectile`; power-gated (G-29). | M | G-41 | RE-W2 (`BuildingClass::AI` fire path, GuardRange, low-power) |
| G-41 | **Weapon source is `Primary=` only; Prism Tank cannot fire.** `resolve_combat_unit` (`main.cpp:695-700`) → `rules.primary()` (`rules.cpp:109`); `[SREF]` has `; Primary=Comet` commented, `WeaponCount=1 Weapon1=Comet` (`rulesmd.ini:6859-6873`) → damage/rof/range 0; no `Secondary` (APOC MammothTusk, HTK FlakTrackAAGun, GGI MissileLauncher unused); `EliteWeapon1`/elite projectile not swapped (`world_veterancy.cpp:47-55`). Also RC-5: `[SREF] Turret=yes ;temp…` fails `is_truthy`. 03. | Combat | BROKEN | BLOCKER | RC-5, RC-6 | Centralise comment stripping in `Ini` (or one `Rules::value()`); resolve `Primary` else `Weapon1`; carry `Secondary` as a second `WeaponStats` chosen per target (AA/AG, Verses). | M | — | RE-W1 (`SelectWeapon`, `WeaponCount` mapping) |
| G-42 | **Movement ~5× too fast relative to ROF**: `kCellsPerSecPerSpeed=0.75` (`main.cpp:675`) vs 0.146 cells/s per Speed at 15 Hz (voxel-render.md §5); ROF is converted at 15 Hz (`combat.cpp:15,95`). 03, 05. | Movement | WRONG | MAJOR | RC-8 | After RE-T1: `kCellsPerSecPerSpeed = (256/100)·fps/256` and rescale infantry `move_cps`; re-pin movement gates. | M | G-13 | RE-T1 |
| G-43 | **Every materialised cell is walkable**: `build_passability` ignores Water/Rock/cliffs (`world.hpp:470-473`, `world.cpp:208-231`); tanks drive across lakes and cliff faces; no terrain speed multipliers; no bridges. 03. | Movement | MISSING | BLOCKER | — | Reuse `resolve_land_types` (already used for the water grid, `campaign_runtime.inc:1300`) to zero `passable_` for Water/Rock/WaterCliffs for Land units; `[LandType] SpeedType%` later. | M | — | no (naval.md §2 table) |
| G-44 | **No anti-air**: `valid_target` excludes `is_aircraft` (`combat.cpp:59-77`); `apply_spread_damage` skips aircraft (`:220`); AA weapons unused. BLOCKER once aircraft are producible. 03. | Combat | MISSING | MAJOR | RC-6 | Lift the exclusion; weapon pick by `AA/AG`; projectiles use target altitude (`unit_lz`). | M | G-41 | RE-W1, RE-W8 (homing) |
| G-45 | **GI/GGI deploy freezes the squad** instead of switching to Para/MissileLauncher: Desolator model applied to every `Deployer+DeployFire` type (`main.cpp:951-953`, `world_abilities.cpp:63-88, 479-490`; `[SSA] CellSpread=0` hits nobody). 03. | Combat | WRONG | MAJOR | — | For `DeployFire` types without `RadLevel`: immobile + `Secondary` as the active weapon through normal `step_combat`. | S | G-41 | RE-W3 |
| G-46 | Idle units never auto-acquire buildings (`combat.cpp:309-310`). 03. | Combat | PARTIAL | MAJOR | — | Add buildings to the acquire scan behind a threat score. | S | — | RE-W4 |
| G-47 | No crushing (`Crusher`/`Crushable`; `grep -i crush engine/` → names only). 03. | Combat | MISSING | MAJOR | — | On vehicle cell entry kill enemy `Crushable` infantry. | M | G-43 | RE-W6 |
| G-48 | No garrisonable buildings (`Occupants`, NABNKR, `UCFLASH`). 03. | Combat | MISSING | MAJOR | — | Occupant list on `Building`, enter order, fire-out with occupant weapon. | L | G-40 | no (rules keys) |
| G-49 | No bridge model (height layer, passability, overlays, event 31). 03, 05 overlays, 06#28. | Movement/Render | MISSING | MAJOR | — | Overlay parse + bridge land type + separate passability layer. | L | G-43, G-64 | no |
| G-50 | Known unapplied RE findings: ROF jitter `+Random(0,2)` (`combat.cpp:93-95` "CONFIRMED no jitter" is superseded by veterancy.md V7); minimum damage 1 (`combat.cpp:255-264`); Verses 0 % should forbid targeting/retaliation; splash skips buildings/own units. 03. | Combat | WRONG | MINOR | — | Apply; re-pin combat gates with the RE citation. | S | — | no |
| G-51 | No Amphibious domain (LCRF/SAPC; `unit_combat.hpp:12-15`). 03. | Movement | MISSING | MAJOR | — | Third domain = union grid; eject only on land. | M | G-43 | no |
| G-52 | Infantry death anims always `Die1`; no prone/panic; `kDieAnimSecs` not RE'd. 03, 05. | Units | PARTIAL | MINOR | — | `InfDeath`→Die1..5; prone flag under fire. | S / M | — | RE-W9 |

### D. Computer opponent

| ID | Issue | Area | Status | Sev | Root cause | Fix sketch | Effort | Depends on | Needs RE? |
|---|---|---|---|---|---|---|---|---|---|
| G-60 | **AI never trains a unit or attacks.** `make_ai_config` leaves `AiTeamConfig.enabled=false` (`main.cpp:2178-2258`); `make_ai_team_config` is called only by `--demo-ai-team`/`--demo-ai-trigger` (`:9227, 9476`); `step_ai_teams` skips every house (`world.cpp:2730`); no `aimd.ini` load on the interactive path. The player cannot lose. 03 wiring, 04#2/#4. | AI | GATE-ONLY | BLOCKER | RC-1, RC-3 | Load `aimd.ini` via `load_ini_from_ra2md` (`main.cpp:439`); per AI slot `acfg.team = make_ai_team_config(ba, ur_ai_, rules, aimd, diff, player_x, player_y, triggers=true, &a_.map)`; `enable_ai_house`. Pair with G-61. | S | G-20 | RE-A4 (autocreate order; RE-FALLBACK today) |
| G-61 | **AI-2 script opcode table is wrong**: `classify_op` (`ai_team.hpp:58-71`, `ai_team.cpp:10-36`) uses 8/9/11/12/14/53/54/57/58/63 for actions the binary numbers 0..64 (ModEnc; `D:\ra2-re\p10out\c13.log:29-33`, `c14.log`); only 0, 5, 6, 49 agree; stock `aimd.ini` is dominated by 1/2/3/4/46/47/58 which the clone treats as inert. The P10 campaign executor (`campaign.hpp:169-175`, `campaign.cpp`) already has the right table. 04#3. | AI | WRONG | MAJOR | — | Renumber `classify_op` to share the P10 table; implement 58/2/10/16; re-pin `ai_team_hash`/`ai_trigger_hash`; amend ai-team.md §3. | M | G-60 | RE-A6 (handlers 58/2/10/16) |
| G-62 | **AI-3 triggers never enabled interactively** (`world.hpp:523`, set only at `main.cpp:9476`) and the `--demo-ai-trigger` gate DIFFs on master (`1002c3f8fdb3f00a` vs `8334a6ca1531cabc`). 04#5, drift section. | AI | GATE-ONLY (+gate broken) | MAJOR | RC-1 | `enable_triggers=true` in the G-60 wiring; bisect the drift before re-pinning. | S (+M bisect) | G-60 | X-1 (bisect, not RE) |
| G-63 | **AI difficulty inverted / ineffective**: `[Easy]/[Normal]/[Difficult]` indexed as loaded (`main.cpp:2241-2250`, `ai_base.cpp:17-26`) whereas index 0 is the hardest (ai-trigger.md §5 CONFIRMED); no Armor/ROF/Firepower handicap applied to AI units. 04#13. | AI | WRONG | MAJOR | — | `index = 2 − lobby`; apply handicap mults to AI-house objects. | S (after pin) | — | RE-A1 |
| G-64 | **Allied AI slots build a Soviet base**: `make_ai_config` Soviet-only catalog (`main.cpp:2199-2210`), `make_ai_team_config` `trigger_side=2`/"Russians" (`:3108-3118`), `can_start` has no Owner check (`production.cpp:65-73`). Also NABNKR/TESLA art-dropped so AI defences are NALASR/NAFLAK only. 04#7/#12. | AI | WRONG | MAJOR | RC-7 | `make_ai_config(side)` with GA* catalog + `AlliedBaseDefenses`; `make_ai_team_config(side)` with the Allied roster, `trigger_side=1`, house name from the slot; investigate the SHP load failure. | M | G-60, G-26 | no |
| G-65 | **AI never repairs or sells** (P1 cut; repair-sell.md §D3; `kAiPowerEmergency` unconsumed). 04#9/#10. | AI | MISSING | MAJOR | — | In `step_ai_houses`: `toggle_repair` when `credits > CreditReserve` and `RepairDelay` elapsed; sell on power emergency. | M | G-20 | RE-A2 |
| G-66 | **AI never fires superweapons** (`step_superweapon` only ticks; granted Nuke charges forever; NAMISL/NAIRON not in the catalog). 04#11. | AI | MISSING | MAJOR | — | AI tick: `sw_ready(h)` → fire at the densest enemy cluster; SW structures in the plan tail. | M | G-25 | RE-A3 |
| G-67 | AI threat centroid / `rival_base_size` include allies and the neutral house (`world.cpp:2119-2123`). 04#14. | AI | PARTIAL | MINOR | — | Filter with `is_enemy && house_combatant`. | S | — | no |
| G-68 | No AI harvester replacement (IQ `Harvester=2`). 04#16. | AI | MISSING | MINOR | — | Queue HARV/CMIN when `alive miners < refineries`. | S | G-20, G-22 | no |
| G-69 | **Campaign enemy houses have no AI**: actions 3/74/75 recorded only (`map_trigger.cpp:838`); no `enable_ai_house` in `campaign_runtime.inc:500-560`; no `[Base]` nodes, IQ or TechLevel read. 04#20, 06#26. | AI/Campaign | MISSING | MAJOR | — | On action 3 for a non-player house `enable_ai_house(h, make_ai_config(side))` with the map's node list; AI-3 on 74 with the non-inverted enable. | L | G-60, G-64 | RE-A5 |

### E. Rendering, audio, presentation

| ID | Issue | Area | Status | Sev | Root cause | Fix sketch | Effort | Depends on | Needs RE? |
|---|---|---|---|---|---|---|---|---|---|
| G-80 | **EVA = "Unit lost" only** (`main.cpp:495, 643, 4820-4821`); `evamd.ini` never parsed; no construction-complete/unit-ready/insufficient-funds/low-power/base-under-attack/victory/defeat; vet/crate events drained unused (`:4701-4702`). 01#40, 02#27, 05. | Audio | MISSING | MAJOR | — | Parse `evamd.ini` (side-specific clip), lazy-load from `audio.mix`, §8 priority queue; fire from queue READY, refusals, power edge, house-0 damage with cooldown, sell/capture/promotion, SW ready/fire, win/lose. | M | — | RE-P1 |
| G-81 | **Most units are mute**: voices/`Report=` resolved only for E1/E2/HTNK (`main.cpp:592`); `audio_.voices[u.image]` is empty for every other unit (`:4815, 4830`). 05. | Audio | PARTIAL | MAJOR | — | Register `Voice*`/`DieSound`/weapon `Report` for every resolved roster def; key consistently by rules id. | S | G-22 | no |
| G-82 | **No in-game music** (`theme.ini` unread; only the menu Grinder loop, `main.cpp:5392`). 05. | Audio | MISSING | MAJOR | — | Parse `theme.ini`; stream stereo IMA `.wav` on the Score bus; mission `[Basic] Theme`. | M | — | RE-P2 |
| G-83 | **No AnimList**: shells are coloured squares (`main.cpp:2741-2767`), no impact/warhead anims, no muzzle flash art, no explosion sounds, no beams (Tesla/Prism), no SW visuals. 05. | Render | MISSING | MAJOR | — | World-level `AnimList` (pos, SHP, timer, 15 Hz, `Report`) spawned on detonation/death/placement; projectile `Image=`; beam renderer. | M | — | RE-P5 |
| G-84 | **Shroud drawn as bounding rectangles** that overwrite neighbours (`iso_renderer.cpp:148-153, 220-224`); **enemy buildings, ore, crates, projectiles drawn through the shroud** (`draw_building` has no visibility test, `main.cpp:2346-2456`). 05. | Render | WRONG | MAJOR | — | Visibility test in the merge loop (+ last-seen cache); shroud as a post-pass with `shroud.shp` edge frames keyed by the explored mask. | S (visibility) / M (edges) | — | RE-P3 |
| G-85 | **Buildings are static**: no ActiveAnim/IdleAnim/Bib/TurretAnim/damage fire (`grep ActiveAnim engine/` → none); shared `TWLT070` death. 05. | Render | MISSING | MAJOR | — | Parse `artmd.ini` anim keys, stage counter per anim, `*Damaged` switching; turret facing needs G-40. | L | G-40, G-83 | RE-P5 |
| G-86 | **Theater art hardcoded TEMPERATE** for buildings (`main.cpp:1936`), ore (`:286`), unit palette (`:1079, 1909`). 05. | Render | WRONG | MAJOR (non-temperate maps) | — | Pass `a_.chain` into the three loaders. | S | — | no |
| G-87 | **Maps are barren in skirmish**: `[Terrain]` trees, non-ore overlays (walls/fences/bridges), `[Smudge]`, neutral `[Structures]` never spawned (`place_map_objects` only on campaign/gate paths, `main.cpp:12932, 15571`). 05. | Render/World | MISSING | MAJOR | RC-1 | Parse `[Terrain]`/overlays/smudge; call the P9/P10 object placer for Neutral structures in `launch_skirmish`. | M | — | no |
| G-88 | Path lines drawn for every unit incl. enemy (`main.cpp:2791-2795`); HP bar thresholds 2/3,1/3 not `ConditionYellow/Red`; placeholder selection box; clear colour dark blue (`platform.cpp:176`); 5×7 bitmap font in HUD instead of `.fnt`. 05. | Render | WRONG/PARTIAL | MINOR | — | Selected-own-only behind `TargetLines`; rules thresholds; black clear; `draw_fnt_text` in-game. | S | — | no |
| G-89 | Fixed 1280×720, no fullscreen/resize; Options combobox inert. 01#46, 05. | Platform | PARTIAL | MINOR | — | Honour `[Video]`; resizable layout. | S / M | — | no |
| G-90 | Anim table only registered for player-launched units; AI/skirmish-start infantry fall back to `default_anim()` (`world.cpp:30-36`; black silhouettes). 05. | Render | PARTIAL (latent) | MINOR | — | Register every `ua_.anims` entry in both skirmish and campaign. | S | G-22 | no |
| G-91 | Soviet sidebar skin (`sidec02`) never used (`main.cpp:2078`; `sidebar.hpp:19-47`). 01#45, 05. | UI | MISSING | MINOR | — | Second constant set + mix keyed on side. | S | G-03 | no |

### F. Shell, campaign, Android

| ID | Issue | Area | Status | Sev | Root cause | Fix sketch | Effort | Depends on | Needs RE? |
|---|---|---|---|---|---|---|---|---|---|
| G-70 | **Android cannot issue a single order**: `SDL_FINGER*` → Left only (`platform.cpp:147-165`); every order is Right (`input.cpp:507-764`); a ground tap clears the selection; edge scroll reads the frozen synthetic mouse (`main.cpp:4628`) and scrolls forever; D/N/F1/F10/Esc unreachable. 01#3/#4, 06#35/#37/#38. | Android | BROKEN | BLOCKER (Android) | RC-2, RC-4 | Touch gesture layer: tap on own unit = select, tap elsewhere with a selection = order (the original left-click model, G-05), long-press/two-finger = cancel, two-finger drag = pan; gate edge-scroll on a real mouse; on-screen Deploy/SW/Menu buttons. | M | G-01, G-05 | no |
| G-71 | **Every Android tap is a double-click**: SDL synthesises mouse events from touch (`SDL_HINT_TOUCH_MOUSE_EVENTS` default on; no `SDL_SetHint` in `engine/`; no `SDL_TOUCH_MOUSEID` filter) → two `PointerDown/Up` pairs inside the 400 ms/6 px window (`input.cpp:32-33`). 06#36. | Android | BROKEN (likely) | BLOCKER (Android) | RC-2 | `SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0")` before `SDL_Init` or drop `which == SDL_TOUCH_MOUSEID`. | S | — | no |
| G-72 | APK last built 2026-08-12 (before P1–P10; master never compiled for arm64); README lists 3 of the 10 required mixes (06#39/#40); save-description typing needs `SDL_StartTextInput` (01#43). | Android | UNVERIFIED | MAJOR | RC-4 | `gradlew assembleDebug`; fix the README list; in-app "missing file" screen. | S | — | no |
| G-73 | **Campaign reinforcement delivery**: members dumped on the nearest free land cell (`campaign_runtime.inc:792-830`); script 8 Unload/14 Load/9 Deploy/42-43 Chrono/50 Iron Curtain/25 Garrison skipped (`campaign.cpp:442-445`); Unload is used in 23/24 missions. 06#25. | Campaign | PARTIAL | MAJOR | — | Place transport cargo as a group on the nearest shore; 9 Deploy for enemy MCVs with G-23. | M | G-23 | RE-C1 |
| G-74 | **Trigger events never raised**: 29 DestroyedFakesAll, 31 AllBridgesDestroyed, 44 AttackedByHouse (ALL01/Sov01), 25/26 crosses line, 39/40/41, 2/5/22/55/56 → `_Lose`/objective chains in 8+ missions never arm. 06#28. | Campaign | PARTIAL | MAJOR | — | Raise 44 beside 6 with the attacker's house; 29 as 48 for `Fake=`; 25/26 from cell deltas. | M | — | RE-C5 |
| G-75 | **Trigger actions unimplemented**: 108 CreateCrate (17 maps, 109 uses), 9 FireSale, 101 Reshroud, 88/42/90/102 FX, 40/111 view. 06#27. | Campaign | PARTIAL | MINOR→MAJOR | — | 108 via `enable_crates` spawn API; 9 via `begin_sell` for all house buildings; 101 shroud API. | M | — | RE-C4 |
| G-76 | Campaign script handlers 58 (move to friendly building, 49 stock uses), 2 berserk, 10 follow, 16 patrol skipped. 04#19. | Campaign/AI | PARTIAL | MAJOR | — | Implement in the shared executor. | M | G-61 | RE-A6 |
| G-77 | Campaign objects ignore `Mission=Sleep/Harmless`, veterancy, facing (`campaign_runtime.inc:653-700`); no difficulty handicaps in campaign (06#31). 06#29. | Campaign | PARTIAL | MINOR | — | `no_retaliate` for Sleep/Harmless; apply `difficulty_mods`. | S | — | no |
| G-78 | New Campaign page: difficulty picked on the loading screen, ignores saved `[Options] Difficulty`; Boot Camp absent; Movies/Credits greyed; map picker has no preview. 06#4/#14/#15. | Shell | PARTIAL | MINOR | — | Seed from settings; TRN entry; `[PreviewPack]` preview. | S / M | — | RE-C3 |

### G. Gate drift

| ID | Issue | Area | Status | Sev | Root cause | Fix sketch | Effort | Depends on | Needs RE? |
|---|---|---|---|---|---|---|---|---|---|
| G-95 | `--demo-triggers` prints `trigger_hash=02f4226456db131d`, pinned `20347f4dfa462eb4` (README:2631). P10 attributed it to P6's bigger roster placing more map objects on `sov07tmd`; that attribution is not recorded in the repo. | Gates | DIFF | MAJOR (process) | merge interaction | Reproduce on `f9b93ba`, diff the witness logs against the pinned run, confirm the extra objects are the only delta, re-pin with the explanation in the README row. | S | — | no |
| G-96 | `--demo-ai-trigger` prints `ai_trigger_hash=1002c3f8fdb3f00a`, pinned `8334a6ca1531cabc` (README:2618). Cause unknown. 04 §gate health. | Gates | DIFF | MAJOR (process) | unknown | Bisect `1cf5316..f9b93ba` per merged phase; never re-pin an unexplained drift. | M | — | X-1 |

## "Playable core" definition

**Target:** one skirmish on a stock MP map, human **Allied** vs 1 Soviet AI *and* human
**Soviet** vs 1 Allied AI, plus **Allied mission 1 (ALL01t)** and **Soviet mission 1
(Sov01t)**, each playable start-to-finish on desktop with mouse + keyboard, meaning:

1. Esc never kills the session; menus, lobby, pause, save/load, abort and the result
   screen all return to the shell (G-01, G-02, G-14).
2. The economy runs: miners harvest, ore grows/depletes, income arrives in a single
   wallet seeded once from the lobby slider; miners and the MCV can be rebuilt; the
   MCV deploys (G-20, G-21, G-22, G-23).
3. The side you picked is the side you play: correct sidebar, roster, factories,
   defences, SW type and skin for Allied and Soviet (G-03, G-26, G-91); Yuri gone.
4. Units fight correctly: every buildable unit has a working weapon incl. Prism,
   secondary weapons are used, defences fire, GI deploy works, allies are not
   attackable by accident, idle units engage buildings, movement speed matches the
   recovered scale on a fixed 15 Hz tick with a speed knob (G-40, G-41, G-42, G-45,
   G-06, G-46, G-50, G-13).
5. Terrain is terrain: no driving over water or cliffs; water grid present in
   skirmish (G-43).
6. The AI plays: it harvests, trains from `aimd.ini`, attacks, rebuilds, repairs, and
   builds the base of its own side at a difficulty that means what the lobby says
   (G-60, G-61, G-62, G-63, G-64, G-65, G-68).
7. Superweapons come from their buildings and go away with them (G-25).
8. The player can see and hear what is happening: core EVA set, unit voices and
   weapon reports, action cursors, tooltips, building selection, a clickable minimap,
   nothing drawn through the shroud, hotkeys for stop/guard/scatter/groups/tabs
   (G-80, G-81, G-08, G-11, G-10, G-09, G-84, G-04).
9. Mission 1 of each campaign works as designed: TechLevel cap honoured, event 44 and
   action 108 fire, reinforcements arrive (including by sea), the SMCV deploys
   (G-15, G-74, G-75, G-73, G-23).

**Required IDs:** G-01, G-02, G-03, G-04, G-06, G-08, G-09, G-10, G-11, G-12, G-13,
G-14, G-15, G-20, G-21, G-22, G-23, G-25, G-26, G-29, G-40, G-41, G-42, G-43, G-45,
G-46, G-50, G-60, G-61, G-62, G-63, G-64, G-65, G-68, G-73, G-74, G-75, G-80, G-81,
G-84, G-91 — plus the RC-5 parser fix and the Phase-0 harness that proves it.
Everything else (naval/air G-24/G-44/G-51, AnimList G-83, building anims G-85,
crush/garrison/bridges G-47/G-48/G-49, map dressing G-86/G-87, music G-82, campaign
enemy AI G-69, Android G-70–G-72, lobby polish G-16/G-17/G-78) is the next tier.

**Evidence of done** = four scripted playthroughs under the Phase-0 harness (SK-A,
SK-S, M-ALL01, M-SOV01 in `ROADMAP-playable.md`) each reaching WON/LOST through the
real `GameScene`, with the per-phase assertions and screenshot checks green.

## Open RE questions (Ghidra hunts), de-duplicated

Grouped; each lists the issue IDs it unblocks. Items marked (ANCHORED/PROBABLE) have a
documented best guess in `docs/re/` that the fix may ship behind, to be re-pinned after
the hunt.

**T — timing**
- RE-T1 Logic frame rate per `GameSpeed` setting (`OptionsClass` GameSpeed → frame-delay table, `Frame_Limiter`); the edge-scroll accel reset point (options.md §5 RE-FALLBACK). Unblocks G-13, G-42 (and every per-frame constant's conversion). Sources: 01 Q7, 03 Q1.

**W — weapons / combat**
- RE-W1 `TechnoClass::SelectWeapon`: `Primary/Secondary` vs `WeaponCount/Weapon1..N`, AA/AG and Verses-based choice, SREF `IsChargeTurret` mapping. Unblocks G-41, G-44. (03 Q6)
- RE-W2 `BuildingClass::AI` fire path for `Primary=` defences: acquire range, turret `ROT`, `Power<0`/low-power offline, facing for `Turret=no` prism/tesla. Unblocks G-40, G-29. (03 Q7)
- RE-W3 `DeployFire` targeting: does a deployed `Deployer+DeployFire` infantry fire `DeployFireWeapon` through the normal target path, may it rotate. Unblocks G-45. (03 Q2)
- RE-W4 Do idle/guard units auto-acquire structures (`Greatest_Threat`/`Target_Something_Nearby` ThreatType flags)? Unblocks G-46. (03 Q3)
- RE-W5 `GetGuardRange` and the `Stray/GuardModeStray` consumer. Unblocks the `kGuardStray=2.0` stand-in. (03 Q4)
- RE-W6 Crush: `Per_Cell_Process`/`Drive` crush branch, `Crush=1.8`, `PlayerAutoCrush`. Unblocks G-47. (03 Q5)
- RE-W7 Spread-damage falloff curve in `ReceiveDamage`. Unblocks G-50 (splash). (03 Q8)
- RE-W8 Homing steering `FUN_005B1B20`, `MissileSpeedVar/ROTVar`. Unblocks G-44, IFV/V3/Patriot fidelity. (03 Q9)
- RE-W9 `InfDeath` → sequence mapping and the prone trigger. Unblocks G-52. (03 Q10)

**I — input / UI**
- RE-I1 `DisplayClass` left/right dispatch and where `ScrollMethod` is read. Unblocks G-05, G-70. (01 Q1)
- RE-I2 `SidebarClass::StripClass::Action`: left/right/hold/cancel-refund order, count badge, READY flash, which strip hosts SW cameos. Unblocks G-07, G-09. (01 Q2)
- RE-I3 Full cursor descriptor table @`0x0082C038` (frame start/count/rate/hotspot) for select/move/nomove/attack/enter/capture/deploy/scroll/SW. Unblocks G-08. (01 Q3, 05)
- RE-I4 `RadarClass`: click → recentre, radar-event blips, boot anim timing, per-cell colour source, blip sizes. Unblocks G-09. (01 Q4, 05)
- RE-I5 Group hotkeys `TeamCreate/Select/AddSelect/Center` storage and the centre-camera rule. Unblocks G-04. (01 Q5)
- RE-I6 `StopObject`→Guard vs Sleep, `GuardObject` area radius, `ScatterObject` cell choice. Unblocks G-04. (01 Q6)
- RE-I7 In-game options page 0xBBB layout (GameSpeed slider, volumes). Unblocks G-13, G-14. (01 Q7)
- RE-I8 Skirmish victory/defeat presentation: `Do_Win/Do_Lose` MP variant, EVA lines, BorrowedTime when `Session.GameMode != 0`, score screen before the shell. Unblocks G-02. (01 Q8, 06 Q4)
- RE-I9 Tooltip text source (CSF key family + cost format) and status-line 0x695 messages. Unblocks G-11. (01 Q9)
- RE-I10 ESC cancel chain `FUN_004AAC40` falls through to dialog 0xB5 when nothing is armed (ANCHORED). Unblocks G-01 fidelity. (06 Q5)
- RE-I11 Exit Game / Abort Mission confirm dialogs 0x120/0x121 strings; does Abort go through the score screen. Unblocks G-14. (06 Q8)

**E — economy / production**
- RE-E1 Prerequisite "owned" at `Unlimbo` (placement) or grand opening (PROBABLE: placement). Unblocks G-31. (02 Q1)
- RE-E2 Skirmish starting force: `UnitCount` pool per side, MCV cell relative to the start waypoint, facing/mission (ANCHORED). Unblocks G-23. (02 Q2)
- RE-E3 MCV deploy: `Try_To_Deploy`/`DeploysInto` foundation check, adjacency bypass, `DeployTime`, buildup vs instant, `UndeploysInto`, `MCVRedeploys` global (`0x00A8A288` unanchored), AI "deploy BaseUnit when no ConYard". Unblocks G-23, G-73 (script 9). (02 Q3/Q8, 04 Q4, 06 Q1)
- RE-E4 Low-power effect set: predicate (`FUN_004FCC50`) and consumers (powered weapons, radar, `SuperClass` pause, gap generator). Unblocks G-29, G-25. (02 Q4)
- RE-E5 `Passes_Proximity_Check` reads `BaseNormal` (+0x154F; pre-hunt citation). Unblocks G-28. (02 Q5)
- RE-E6 `Cost<class>Mult` (`FUN_0050BC10`), `InfantryCostBonus/UnitsCostBonus/AircraftCostBonus`, rounding against the 54-step charge. Unblocks G-27, G-16. (02 Q6)
- RE-E7 Ore `GrowthRate` timer consumer (candidate-queue drain). Fidelity follow-up for G-20. (02 Q7)
- RE-E8 `RequiredHouses/ForbiddenHouses` offsets and the sidebar `Owner=`/side filter (`HouseTypeClass` side → `Owner` bitmask). Unblocks G-26, G-22, G-16. (02 Q9)
- RE-E9 `SuperClass` creation/destruction tied to `SuperWeapon=` building placement/death; captured silo transfer (capture.md covers the move). Unblocks G-25. (01 Q10)

**A — AI**
- RE-A1 Lobby `AIDifficulty` → `house+0x184`, `HouseClass::AssignHandicap` (which of RulesClass+0x1538/+0x1588/+0x15d8 index 0/1/2 use), how Armor/ROF/Firepower mults apply to AI objects. Unblocks G-63. (04 Q1)
- RE-A2 AI repair gate `Rules->[0x1444]` and the `RepairDelay` timer at house+0x5640. Unblocks G-65. (04 Q2)
- RE-A3 AI SW autofire target selection inside `FUN_004fd320`. Unblocks G-66, G-25. (04 Q3)
- RE-A4 Autocreate selection order among equal-priority teams (all 163 stock teams are `Autocreate=yes`; RE-FALLBACK today). Unblocks G-60 fidelity. (04 Q5)
- RE-A5 Campaign enemy AI: `[Base]`/`BaseNodes` reader, `ProductionBegins` (28/29) → house production flag, the non-inverted campaign AITrigger enable, per-house `TechLevel`/`IQ` → sidebar cap and AI-1/2/3 toggles. Unblocks G-69, G-15 (campaign). (04 Q6, 06 Q2)
- RE-A6 Script handlers 58 (move to friendly building), 2 (berserk), 10 (follow), 16 (patrol). Unblocks G-61, G-76. (04 Q7)

**C — campaign / shell**
- RE-C1 `TeamClass` reinforcement delivery (`FUN_006eff20` CreateTeam / `FUN_0065d320` Reinforcement): transport cargo, edge entry cell, paradrop, chrono-in. Unblocks G-73. (06 Q3)
- RE-C2 Lobby slot → start-location randomisation and colour-conflict resolution. Unblocks G-17. (06 Q6)
- RE-C3 Options `Difficulty` ↔ campaign dialog 0x94 slider. Unblocks G-78. (06 Q7)
- RE-C4 Actions 108 CreateCrate (crate index space), 9 FireSale refund rule, 101 Reshroud radius. Unblocks G-75. (06 Q9)
- RE-C5 Raise points for events 29 DestroyedFakesAll, 31 AllBridgesDestroyed, 44 AttackedByHouse. Unblocks G-74. (06 Q10)

**P — presentation**
- RE-P1 EVA trigger set and timing: construction-complete vs unit-ready, low-power edge, base-under-attack cooldown, §8 pending-slot replacement. Unblocks G-80. (05)
- RE-P2 `theme.ini` semantics: `Normal/Repeat/Side`, shuffle, skirmish default track, mission `[Basic] Theme`, gap. Unblocks G-82. (05)
- RE-P3 Shroud renderer `FUN_0047EEF0`: `shroud.shp` frame from the neighbour mask; `BlendedFog` blend table. Unblocks G-84. (05)
- RE-P4 Map `[Lighting]` → per-level palette converts. Unblocks the lighting row (05, not in the core). (05)
- RE-P5 `AnimClass` playback (`Rate/LoopCount/Start/End/Translucent/Report/YSort`), warhead `AnimList` size buckets, building `ActiveAnim` stage/`*Damaged`. Unblocks G-83, G-85. (05)
- RE-P6 Vehicle/building shadow source and blend ratio. Unblocks the shadow rows (05). (05)

**X — investigations that are not RE**
- X-1 Bisect `--demo-ai-trigger` drift across `1cf5316..f9b93ba` (G-96). (04 Q8)
- X-2 Verify in play why Soviet ships render over black water in `demo_campaign_all01t_start.png` (ownership, `visible_at` mismatch, or first-frame-before-`step_visibility`). (05) — the Phase-0 harness answers this.

## Gate-hash drift and the re-pin policy

**Observed on master `f9b93ba`:**

| Gate | Pinned | Observed | Attributed cause |
|---|---|---|---|
| `--demo-triggers` | `trigger_hash=20347f4dfa462eb4` (README:2631) | `02f4226456db131d` | P10 reported: P6's bigger roster lets `place_map_objects` resolve more of `sov07tmd`'s `[Structures]/[Units]` → more objects in the witness. Plausible, unverified in-repo. |
| `--demo-ai-trigger` | `ai_trigger_hash=8334a6ca1531cabc` (README:2618) | `1002c3f8fdb3f00a` | Unknown (X-1). |

Both drifts happened *between* phase merges without anyone re-running the full batch
on the merged tree, which is itself a symptom of RC-2: the gates guard each phase's
own world, not the merged game.

**The policy question.** Nearly every fix in the playable core legitimately changes
sim behaviour that some gate hashes: wiring the economy into `launch_skirmish` moves
`skirmish_hash`/`maps_hash`; the opcode renumber moves `ai_team_hash`/`ai_trigger_hash`;
ROF jitter and min-damage move `combat`/`tank` logs; the speed rescale moves every
movement gate; the `Ini` comment fix may move anything that reads a commented value.
Treating "gate moved" as "defect" (the P1–P10 rule) would force every fix back behind
a flag — i.e. reproduce RC-1. Proposed policy:

1. **Determinism is the invariant, not the hash value.** A gate must still print
   `deterministic=yes` (rendered and headless runs byte-identical). A fix that breaks
   determinism is a defect; a fix that moves a hash is a change to be explained.
2. **Re-pin per fix, with justification in the same commit.** Each re-pin commit
   (`gate: re-pin <gate> — <G-id> <one-line behavioural reason>`) updates the README
   gate row with the old → new hash and the RE citation that justifies the behaviour
   change, and the ledger. One re-pin commit per logical fix; never batch unrelated
   re-pins.
3. **Never re-pin an unexplained drift.** G-96 must be bisected first; G-95's
   attribution must be confirmed by diffing the two witness logs (the extra objects
   should be the only delta) before the new hash is accepted.
4. **Prefer fixes that are hash-neutral only when that is also the right design.**
   Wiring the economy in the `GameScene` ctor rather than `launch_skirmish` keeps
   `skirmish_hash` but perpetuates "the gate world is not the real world". The
   playable roadmap resolves this by giving the real path its own gate (below), after
   which `launch_skirmish` can be made complete and `--demo-skirmish` re-pinned once.
5. **Add a real-play gate.** Phase 0 adds `--play <script>` with a `play_hash` over
   the `GameScene` world driven by scripted input at a fixed 15 Hz tick (crates off or
   seeded). From then on every phase's acceptance is "the four core scripts still
   reach their expected outcome and the play_hash either matches or is re-pinned
   under rule 2". That gate hashes the thing the user runs.
6. **Full batch once per phase, at the end** (per the project's scoped-test rule),
   plus the single affected gate during development. Gate runs are slow (~2 min, 3 GB);
   never run them in parallel.
