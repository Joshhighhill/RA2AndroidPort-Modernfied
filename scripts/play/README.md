# `--play` scripts (G0 verification harness)

`ra2engine --play <script.play> [--out <dir>]` runs the **real** interactive
game (`run_menu` -> `ShellScene` -> lobby / campaign screens -> `GameScene`)
with scripted input and checks what happened. Nothing is built by hand: the
script clicks the same buttons, cameos and map cells a player would.

How it works (engine/play_driver.inc + the `ra2play` half of
engine/play_script.cpp), on top of agent B's harness groundwork in main.cpp:

* **Hidden + deterministic.** `Platform::create(..., hidden=true)` (software
  renderer, invisible window), audio off, `RA2.INI` and saved games isolated
  under `<out>/pref/` (`RA2_PREF_DIR`), and `run_scene_driver(..., src,
  fixed_dt = 1/15, hooks)` -- one fixed logic step per driver frame, no wall
  clock. Event timestamps are virtual (`frame*67+1000` ms), so double-click
  windows are deterministic too. Two runs of a script print the same
  `play_hash`. `RA2_PLAY_VISIBLE=1` shows the window for debugging.
* **Real input path.** The run is a `DriverHooks` + `EventSource`: scripted
  `InputEvent`s replace the Platform's and go through the scenes' real
  `handle()`; keys are built by `key_event_from_sdl` (the exact production
  mapping); held keys (`hold`) also set SDL's keyboard-state array for the
  arrow-key scroll `GameScene::update` polls.
* **Probe.** Each frame the harness merges two scene surfaces: G's
  `Scene::play_probe(ra2play::Probe&)` (typed keys, named click targets, hooks)
  and B's integer `Scene::probe()` (keys such as `units.0.GI`, `sel`, `menu`,
  `cursor`, `q.structure`); a G key wins on a name clash. Unknown target names
  fall back to B's `Scene::locate()` (`cell:x,y`, `building:H:ID`,
  `cameotab:ID`, `button:repair|sell|options|radar`), unknown cheats to B's
  `Scene::cheat()`.
* **Output.** Console + `<out>/play.log`: every step, `PLAY PASS/FAIL` lines,
  `PLAY scene f=N a -> b` transitions, `play_state f=N ...` every 15 frames
  (console every 150), shots `<out>/<name>.png`; at the end
  `play_result=PASS|FAIL ... failures=N unreached=N` and `play_hash=<16 hex>`
  (FNV-1a over the probe every 15 frames). `<out>/result.txt` lists each
  failure as `FAIL L<line> f=<frame> | <command> | <detail> | @tags`.
  Exit code: 0 all passed, 1 any failure, 2 script error, 3 exception.
  Default `<out>` is `play-out/<script-stem>/`.

Run scripts **one at a time** (each loads the full asset set; ~15 s start-up,
then ~400 frames/s with sparse rendering, ~20 frames/s rendering every frame).
`scripts/play/run-all.ps1` runs a list sequentially and tabulates the results.

## Script format

One command per line (agent B's simpler forms are accepted too: `click X Y`,
`click button Single Player` without quotes, `click cameo GAPOWR` -- which
switches the sidebar tab first when needed). `#` starts a comment (outside `"quotes"`). Trailing
`@<G-ID>[:<owner>]` tokens tag the line with the gap issue it checks (e.g.
`@G-20:A`); tags are copied into failures so `PLAYTEST-STATUS.md` can map them.
A `"double quoted"` run is one argument.

| Command | Meaning | Frames |
|---|---|---|
| `set render_every <n>` / `set max_frames <n>` / `set park <x> <y>` / `set states on\|off` / `set on_quit stop\|continue` | Run options. `render_every` renders (and presents) every n-th frame; shots always render. `on_quit continue` records `InputEvent::Quit` reaching the driver as a failure and keeps going (default `stop`: the run ends there, like the real program). | 0 |
| `set hz <n>` / `set dt <s>` / `set align <n>` | W3-B: the driver's fixed step for the WHOLE run (`RA2_PLAY_HZ=<n>` overrides it, to re-run a script at another render rate); `align N` makes every input / cheat wait for an anim tick that is a multiple of N (and, for frame-consuming input, later than the last input), so 60 Hz and 30 Hz runs feed identical input at identical logic ticks. `{tick}` in a `shot` name becomes the scene's `anim_tick`; `<out>/shots.txt` records each shot's `view`-target screen point (`scripts/play/anim_check.py`). | 0 |
| `wait <n>` | Let n frames pass. | n |
| `click <target> [right] [shift] [ctrl] [alt] [nopark] [soft]` | Move + press + release at the target (original scheme: left = select AND order, right = cancel/deselect). The pointer then parks at (560,360) next frame so it does not sit in an edge-scroll band. A `cameo:ID` on another tab clicks that tab first. A `building` target whose centre cannot be brought on screen (camera clamped at a map edge) falls back to its other foundation cells. `soft`: a target that cannot be found / shown is skipped (`PLAY skip`), not a failure. | 1 |
| `drag cell\|xy x1 y1 x2 y2 [right]` | Press, move, release (box select). | 1 |
| `move <target>` | Pointer motion only. | 1 |
| `key <name> [shift] [ctrl] [alt]` | Key down this frame, up next frame. Names: SDL key names plus `esc enter space tab backspace delete up down left right f1..f12 home end pageup pagedown`. | 1 |
| `hold <name> <n>` | Key held for n frames (the script continues meanwhile). | 1 |
| `type "<text>"` | One `SDL_TEXTINPUT` event. | 1 |
| `view <cell\|unit\|building ...>` | Pan the camera so the cell is centred (as scrolling would). | 0 |
| `place auto` | With a building armed for placement, click the nearest valid origin to the player's first building (same `placement_check` as the game). | 1 |
| `place near <x> <y>` | As `place auto`, but the search is anchored on cell (x,y) (the yard the player is looking at) instead of the player's first building -- e.g. SOV04, whose first building is a map lamp post far from the ConYard. | 1 |
| `shot <name>` | PNG of this frame. | 1 |
| `assert <expr>` | Soft check: a failure is logged and the run continues. | 0 |
| `require <expr>` | Hard check: a failure ends the run. | 0 |
| `until <n> <expr>` | Wait up to n frames for expr; failure at timeout. | <= n |
| `cycle <target> <expr>` | Click target (once per frame, max 8) until expr holds. | <= 8 |
| `if <expr> then <command>` | Run one non-blocking command when expr is true (missing keys = false). | per command |
| `let <name> = <operand>` | Remember a value as `$name`. | 0 |
| `cheat <args>` | Scene dev hook (below). | 0 |
| `dump [console]` | Write the whole probe + targets to the log. | 0 |
| `log <text>` | Note in the log. | 0 |
| `expect quit` | The next Quit is the expected end of the run. | 0 |
| `end [expect WON\|LOST\|RUNNING]` | Stop; optionally check `outcome`. | - |

**Targets.** `point NAME [+dx +dy]` (G16 named map points relative to the
player's first building: `garrison` (nearest empty neutral garrison),
`garrisoned` (own occupied one), `hut` (bridge repair hut), `bridge` (nearest
intact low-bridge centre cell), `bridge_near` / `bridge_far` (its two banks), `bridge_span` (a middle section 4 cells in from the near bank)),
`cell X Y` (map cell centre; the view is centred first when it is
off-screen), `xy X Y` (window pixels), `unit H:TYPE[:n] [+dx +dy]` (the n-th
live unit of house H and rules id TYPE, `*` = any; optional signed cell offset), `unit H:TYPE:near` / `building H:TYPE:near` (P4: the one nearest the centre of the player's current selection),
`building H:TYPE[:n] [+dx +dy]`, or a **named target** from the probe:

| Scene (`scene`) | Named targets |
|---|---|
| `shell:<page>` (`shell:Main Menu`, `shell:Single Player`, `shell:New Campaign`, `shell:Options`, `shell:Movies & Credits`) | `button:<label>` (sugar: `click button "<label>"`), `dialog:*` when the Load dialog is open |
| `lobby` (Skirmish page 0x102 / Customize Battle 0x6B) | `button:Start Game`, `button:Customize Battle`, `button:Back` (aliases `start`, `back`), `edit:name`, `combo:ai<N>` (rows 1-7), `combo:country<N>` / `combo:color<N>` (active rows), with a list open `pick:<item label>` (`pick:Brutal Enemy`, `pick:Russia`, `pick:RED`), `item:<i>`, `list:up` / `list:down`; `check:short` / `check:mcv` / `check:crates` / `check:superweapons`; `slider:speed=<0..6>` / `slider:credits=<5000..10000>` / `slider:units=<0..10>`; on Customize Battle `button:Use Map`, `button:Cancel`, `map:<file>` (visible rows), `list:up` / `list:down`; `confirm:OK` / `confirm:yes` when a validation message is up |
| `campaign-load` | `diff:EASY` `diff:NORMAL` `diff:HARD`, `start`, `back` |
| `result` | `continue` (when enabled), `retry`, `menu` |
| `game` | `tab:0..3`, `cameo:<ID>` (active tab), `repair`, `sell`, `options`, `sw` (when the house has a SW), `menu:LOAD GAME` / `menu:SAVE GAME` / `menu:DELETE GAME` / `menu:ABORT MISSION` / `menu:RESUME MISSION` (menu open), `dialog:action` `dialog:back` `dialog:row<N>` `dialog:yes` `dialog:no` (save/load dialog) |

**Expressions.** `clause { and|or clause }`, evaluated left to right; a clause
is `operand OP operand` (`== != < <= > >=`) or a bare operand (true when
non-zero / non-empty). Operands: numbers, `"strings"`, `$var`, `$var+N` /
`$var-N`, probe keys, and bare words (a bare word that is not a probe key is a
string literal on the right-hand side). A key whose family is a counting family
(`units`, `buildings`, `built`, `harv`, `harvesters`, `harv_bales`, `moving`,
`selected_type`, `can_build`, `cameo`, `cameo_tab`) reads 0 when absent; any
other absent key fails the check with `missing-probe:<key>` — so a script
written for a later phase degrades gracefully on an older tree.

## Probe keys

Always: `scene`, `f` (driver frame), `sf` (frames in this scene), `quits`
(Quit events seen), `transitions` (scene swaps), `quit_seen`.

`shell:*`: `page`, `button_enabled(<label>)`, `quit_requested`, `dialog_*`.
`lobby` (per row N = 0..7): `ai(N)` (`Human`/`None`/`Easy`/`Medium`/`Brutal`),
`country(N)` (rules id or `Random`), `side(N)` (`ALLIED`/`SOVIET`/`RANDOM`),
`color(N)` (lobby colour 0..7, -2 = Random), `row_active(N)`; `page`, `name`,
`editing`, `ai_players`, `visible_rows`, `max_players`, `cash`, `unit_count`,
`game_speed`, `check(<id>)`, `crates`, `combo_open` / `combo_row` /
`combo_count`, `map`, `map_name`, `map_ix`, `map_count`, `map_sel` (Customize
Battle), `status` (tooltip line), `confirm_open` / `confirm_text`.
`game` adds, per house h: `side(h)`, `country(h)`, `house_color(h)`, `ai(h)`
(`Human`/`Easy`/`Medium`/`Brutal`), `combatant(h)`; and `game_speed`,
`mcv_redeploys`, `sw_allowed`, `short_game`, `crates_on`, `unit_count_cfg`,
`start_cash` (the lobby options as the game consumes them).
`campaign-load`: `map`, `side` (`GDI`/`Nod`), `difficulty`.
`result`: `outcome` (`WON`/`LOST`), `map`, `next_map`, `result_frames`.

`game`:

| Key | Meaning |
|---|---|
| `mode` | `skirmish` / `campaign` |
| `credits`, `credits_shown` | the player's wallet as the sidebar uses it, and the rolling tally |
| `house_credits(h)`, `houses`, `team(h)`, `defeated(h)` | World per-house state |
| `units(h)`, `units(h,ID)`, `moving(h)` | live (not dying) units; ID = rules id (`E1`, `MTNK`, `CMIN`, ...) |
| `buildings(h)`, `buildings(h,ID)`, `built(h,ID)` | live buildings / fully built (Idle) ones |
| `units_lost(h)`, `buildings_lost(h)`, `hp_lost(h)` | cumulative losses; `hp_lost` = damage on live buildings |
| `harvesters(h)`, `harv(h,STATE)`, `harv_bales(h)` | STATE = `None SeekOre MoveToOre Harvesting ReturnToRef WaitDock Unloading` |
| `weapon_damage(ID)`, `weapon_range(ID)`, `weapon_rof(ID)` | the first live unit of that type's resolved primary weapon |
| `selected`, `selected_type(ID)`, `sel_cx`, `sel_cy`, `sel_moving`, `selected_building` | selection |
| `own_on_screen` | house-0 objects inside the playfield view |
| `outcome` | `RUNNING`/`WON`/`LOST` as the **session** reports it (campaign outcome; a skirmish has no end path on master) |
| `world_outcome`, `victory_enabled`, `winning_team` | the World's own victory state |
| `power_out`, `power_drain` | house 0 |
| `tab`, `tab_ids`, `sidebar`, `cameo(ID)`, `cameo_tab(ID)`, `can_build(ID)` | sidebar contents (all four tabs) and whether a click would start the entry now |
| `queue(cat)`, `queue_stage(cat)`, `queue_ready(cat)` | cat = `structure defense infantry vehicle` |
| `placing`, `placing_type`, `base_mode` (`none/repair/sell`), `sw_targeting` | armed modes |
| `sw(h)`, `sw_ready(h)` | superweapon granted / ready |
| `menu_open`, `dialog_open`, `dialog_mode` (1 load, 2 save, 3 delete), `dialog_rows`, `dialog_sel`, `dialog_confirm`, `dialog_message`, `banner` | in-game menu / save-load dialog |
| `map`, `cam_x`, `cam_y`, `playfield_w` | view |
| campaign only: `campaign_frame`, `objectives`, `triggers_fired`, `campaign_outcome`, `briefing_open` | mission runtime |
| `sel_near(h,ID)` | P4: cells from the centre of the player's selection to the nearest live house-h unit of type ID (present only while something is selected) |
| `garrisonable`, `owned_garrisons(h)`, `garrisoned(h)`, `occupants(h)`, `in_garrison(h)`, `garrison_shots` | G16 garrisons |
| `bridge_cells`, `bridge_destroyed`, `bridge_version`, `depth_armed`, `on_bridge(h)`, `pt_<name>_x/y`, `sel_dist_<name>` | G16 low bridges; named points below |
| `sel_occupier`, `sel_occupy_target`, `sel_attack_cell`, `sel_target`, `sel_rof_cool`, `sel_turret`, `sel_body`, `sel_damage`, `sel_wall` | first selected unit (debug) |

**W2-AN keys** (engine/w2air_play.inc): `cursor_name` (Input action name),
`sel_on_water`, `sel_hp`, `sel_max_hp`, `sel_cargo`, `sel_altitude`, `sel_flight_state`,
`sel_flight_goal`, `sel_unload_pending`; per house h `submerged(h)`, `sub_detected_by0(h)`,
`iron_units(h)`, `iron_buildings(h)`, `chrono_units(h)`, `airborne(h)`, `spawned_out(h)`,
`planes(h)`, `planes_docked(h)`, `plane_ammo(h)`, `amphib_on_water(h)`, `cargo_total(h)`,
`sunk(h)`, `crashing(h)`, `unit_hp(h,ID)`, `pos_x(h,ID)`/`pos_y(h,ID)` (first unit);
superweapons `sw_count(h)`, `sw_has(h,TYPE)`, `sw_ready_t(h,TYPE)`, `sw_charge(h,TYPE)`
(TYPE = Nuke IronCurtain LightningStorm ChronoSphere ParaDrop AmerParaDrop), `sw_armed`,
`sw_armed_phase`, `sw_strikes`, `sw_ev_launched|detonated|activated|bolts|dropped`,
`rad_total`. Targets: `sw:<TYPE>` (one sidebar slot per granted SW); named points
`water_near_sel`, `shore_near_sel`, `shore_far_sel`, `beach_near_sel`, `yard_dock`.
Cheats: `w2 build ID [H]` (instant structure on the nearest legal spot), `w2 damage
H:TYPE[:n] DMG`, `w2 kill H:TYPE[:n]`.

**Cheats** (`game` only; dev hooks for reaching a state quickly — acceptance
should prefer real input): `kill_house H`, `damage_building H:TYPE[:n] DMG`,
`credits N`, `spawn TYPE H cell X Y`, `spawn TYPE H building H:TYPE[:n] +DX +DY`
(TYPE from the player or AI roster, else resolved from rules),
`spawn TYPE H point NAME +DX +DY`, `bridge_hit X Y DMG` (one bridge roll).
`set cycle_max N` raises `cycle`'s click limit (default 8).

Keys reserved for later phases (scripts already assert them and report
`missing-probe` until a phase adds them): `confirm_open` + targets
`confirm:yes`/`confirm:no` (Abort/Exit confirmation, G-14).

## The scripts

| Script | What it encodes |
|---|---|
| `sk-a.play` | Playable core SK-A: Allied vs 1 Soviet AI — start state, MCV deploy, select/move/stop, power/refinery/barracks/factory, income, GI/Engineer/Grizzly/Chrono Miner, AI trains + attacks, weapons, Pillbox fires, win path |
| `sk-a-lose.play` | SK-A with no input against a Brutal Enemy row: the AI must win and the session must end LOST |
| `sk-s.play` | SK-S: Soviet vs 1 Allied AI — no Yuri, Soviet sidebar/roster, SMCV, Conscript/Rhino/Flak Track/War Miner, Tesla Coil, Allied AI builds GA\*, win path |
| `sk-lobby.play` | The Skirmish page end to end: Customize Battle map pick, name edit, country/colour/AI dropdowns, exclusive colours, sliders, checkboxes, Back saves + restore, the two-players message, Start -> every choice asserted in the game probe |
| `sk-end.play` | Fast lose path (`cheat kill_house 0`) -> LOST result -> main menu |
| `m-all01.play` | Allied mission 1 from the shell: loading screen, objectives arm, briefing, TechLevel 3 cap, Tanya, Esc |
| `m-sov01.play` | Soviet mission 1: objectives, SMCV deploy, Soviet sidebar, TechLevel cap |
| `shell-smoke.play` | Menus, Options round trip, lobby round trip, Esc in menu/lobby/game/placement, F10 menu, save -> load -> resume, Abort/Exit confirmation |
| `world-depth.play` | G16: crush; garrison a civilian building, fire out, evict; May Day: engineer repairs the blown bridge at the hut, a tank drives across, Ctrl force-fire collapses a section, a hut repairs it |
| `naval.play` | W2-AN naval (Bay of Pigs): Naval Yard on water, a Destroyer sails where it is clicked, a submerged Typhoon sub found by sonar and hunted by the ASW Osprey, a landing craft that beaches / loads GIs / crosses / unloads on land (not at sea), yard repair, Carrier Hornets + Dreadnought missiles, a sinking Destroyer, the SHP Dolphin |
| `air.play` | W2-AN aircraft: Airforce Command (AMRADR) through the sidebar, a Harrier built onto its pad, a strike (take off, attack pass, Ammo 0, return, land, rearm at ReloadRate), a Rocketeer that lifts off on exit and obeys Stop, a Kirov killed in the air that crashes, a Night Hawk that lands to load and lands to unload |
| `sw.play` | W2-AN superweapons v2: five SW buildings (set-up cheat), five sidebar slots with their own cameos, each SW charges in real time and is armed from its slot: American paratroopers, Iron Curtain (3x3, expires), Chronosphere (source + destination picks, GI chrono-killed), nuke (launch, ~400 frames of flight, blast + fallout), Lightning Storm (bolts until the storm ends) |
| `anim-buildings.play` | W3-B building slots: construction-yard crane, power plant loop, service depot cycle, Prism SpecialAnim per shot, war factory door, refinery level + ore dump (`live_*`). Shots `<prefix>_<tick>`; check with `anim_check.py` |
| `anim-infantry.play` | W3-B infantry machine: idle fidgets, prone under fire, InfDeath deaths + corpses, civilian Panic, Guardian GI deploy, Cheer |
| `anim-vehicles.play` | W3-B vehicles: Night Hawk rotor HVA frames, random Explosion=, harvester UnloadingClass (`live_*`) |
| `anim-terrain.play` | W3-B ALL07 waterfalls (theater tile anims) |
| `all01-opening.play` | W4: ALL01 from the menu, 4 min no input -- the Dreadnoughts shell the Statue, the Liberty Island Patriots shoot DMISLs down, the Statue stands, RUNNING |
| `all01-v3-statue.play` | W4: the scripted V3 still one-shots the Statue (V3WH 50000%); DestroyAnim/rubble in liburb.pal |
| `missiles.play` | W4: skirmish weapon flights to READ -- Rhino shell, IFV homing missile + trail, Kirov voxel bombs, V3 tilt/climb/arc, Grand Cannon, tusks |
| `missiles-intercept.play` | W4: ALL01, an enemy Patriot site shoots a player V3 rocket down (homing RedEye2, DontScore) |
| `probe-*.play` | Harness self-checks / diagnostics (no gameplay claims) |

The expected results on each tree are kept in `docs/gap/PLAYTEST-STATUS.md`.

## Campaign missions directly (W2-CA-19)

`set campaign <MAP> <GDI|Nod> [difficulty]` anywhere in a script makes the run
start at that mission's loading screen (a `CampaignLoadScene`) instead of the
main menu, so missions 2-12 are testable without winning every earlier one
(`click start`, then `until 300 scene == game`). Campaign-only scene cheats
(`cheat camp_*`, engine/campaign_play.inc) skip combat or issue the player's
real World orders: `camp_killb H TYPE`, `camp_killu H TYPE|*`, `camp_killdef`
(armed enemy buildings), `camp_spawn TYPE H X Y [n]` (placed like a
reinforcement), `camp_move UTYPE|* X Y`, `camp_capture UTYPE BH BTYPE`,
`camp_occupy ...`, `camp_attackb BH BTYPE [UTYPE]`, `camp_attacku UTYPE TH TTYPE`,
`camp_deploy` (first controllable MCV: unfold, or drive to a legal spot).
Scripts: `m-all02`, `m-all04`, `m-all08`, `m-sov03`, `m-sov05`, `m-sov06`,
`m-sov09` (one per fixed W2-CA blocker class).
