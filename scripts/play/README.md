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
| `set canvas <W>x<H>` | W6-01-15: the game canvas a phone would get (e.g. `974x548`, the 6" 1080p phone of `docs/gap/wave6/01-touch.md`) instead of the harness's 1280x720. Put it **before** the game starts (it is read when the game scene is built); every later game scene of the run uses it. | 0 |
| `set movie_dir <dir>` | The movie folder for this run (`RA2_MOVIE_DIR`), relative to the script's folder; ignored when `RA2_MOVIE_DIR` is already set. `movies-w6` points it at `tests/data/movies` (a test Bink) because the install's MOVIES0x.MIX are stubs. | 0 |
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
| `waituntil <n> <expr>` | P3: `until` without the timeout failure (logs `PLAY timeout`). | <= n |
| `repeat N` ... `endrepeat` | P3: the body (nestable) is expanded N times before parsing -- exactly as if written out. | body x N |

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

**Edge-band trap (W6-01-15).** A mouse `click` leaves the pointer where it
clicked for one frame before it parks; a click inside the 24 px window-edge
band (for example `click xy 900 700 right` near the bottom of a 720-px canvas,
or with `nopark`) leaves the mouse there and the camera edge-scrolls every
frame after. Click / right-click away from the edges (e.g. `click xy 900 650
right nopark`), or use `touch` gestures, which never edge-scroll.

**Touch (wave 6).** `touch tap|dtap|hold <target>` / `touch drag|boxdrag|pan2`
drive the real `TouchGestures`. A `hold` lasts 700 ms (the hold fires at
450 ms). Keys: `touch_ui`, `touch_bar_open`, `touch_banner`, `touch_mode`,
`touch_aim` (the building a sell / repair tap has marked, -1 none),
`touch_aim_sw` (a superweapon target is aimed), `cameo_menu` (the cameo popup
is up), `touch_box` (sticky BOX), `touch_page2` (the command column's MORE
page), `selected_building_type` (rules id). Targets: `touch:<label>` (the
column's buttons on the current page), `touch:cancel`, `cmenu:PAUSE|RESUME|CANCEL|ALL`
(the cameo popup), `art:<ID>` (a point on the UPPER art of house 0's first
building of that type -- a tower's head, not its foundation).

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
| `sel_occupier`, `sel_occupy_target`, `sel_attack_cell`, `sel_target`, `sel_rof_cool` (rearm, whole frames since Net L0), `sel_turret`, `sel_body`, `sel_damage`, `sel_wall` | first selected unit (debug) |

**P3 keys / targets** (real-order micro): per controllable unit `nenemy(h,ID,n)` / `ndog(h,ID,n)` (cells to the
nearest hostile ground unit / dog), `utgtb(h,ID,n)` (building target index), `ucell(h,ID,n)` (`x*1000+y`);
`bc4n(h,ID)` (buildings with C4 ticking); per house `blackout(h)` (spy power-plant blackout left); campaign
`view_resized` (ResizeView count). Target `unit H:TYPE:near` / `unit e:TYPE:near`: the unit of house H (e = any
house hostile to the first selected unit) nearest that unit, within 9 cells; `building H:TYPE:near` (TYPE or `*`):
that house's building nearest the first selected unit (TYPE `*` skips Insignificant decorations), clicked at a
footprint point no unit covers. P3 real-order wins: `m-all05-real`, `m-all06-real`, `m-all07-real` (gen/mk_all05.py, mk_all06.py).

**R-210 keys** (engine/r210_play.inc): `cliff_steps` / `cliff_steps(h)` (unit steps whose standing height -- Level, + 4 on a high-bridge deck -- changed by more than one level), `deck_steps(h)`, `udeck(h,ID,n)` / `uheight(h,ID,n)`, `sel_n` / `sel_deck` / `sel_min_cy` / `sel_max_cy` (the selection); `RA2_R210_TRACE=1` logs the selected units' steps. Debug cheat `map_terrain PATH` (one line per cell: x y level ramp land walkable water amphib highbridge lowbridge). A `cell X Y` target on an intact high-bridge deck clicks the deck.

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
P2: `power_out(h)` / `power_drain(h)` / `low_power(h)` for every AI house with power, and
`cheat map_ascii PATH` (debug only: walkable `.`, blocked `#`, building house digit, unit `*`).

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
| `sk-map-units.play` | W7-09-05 / W7-09-17: the default skirmish map spawns its own Neutral cars/trucks on the civilian house (`units(2) >= 60`, CAR/PTRUCK by rules id, no DTRUCK), the start forces stand and the AI still builds out |
| `m-sov11-fate.play` | W7-10-01: units the enemy Yuris take in SOV11 get the DecideUnitFate result (`camp_log fate`: join the capturer's team or Hunt) instead of standing still |
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
| `move-bridge-cliff.play` | R-210: ALL01 -- Tanya on Liberty Island's cliff edge is ordered into the river below and walks down the island stairs instead of dropping (`cliff_steps(0) == 0`); after the Fort Bradley rendezvous an engineer repairs the broken east bridge at the hut and the box-selected GIs pass their own gate, walk on the deck (`sel_deck`, `deck_steps(0)`) and reach the far bank |
| `trn02-bridge-repair.play` | fv7d: TRN02 -- the MCV deploy fires `RedTeamRepairBridge`; the RedTeam engineer's script line 46 (attack CABHUT) sends it into the hut [RE: InfantryClass::Mission_Attack 0x51F0F0, Infiltrate set for Engineer= by the loader 0x5243B9] and the destroyed middle row of the only bridge is repaired (`bridge_destroyed == 0`). The Shore sandbar's Beach sub-tiles (TMP byte 10) stay impassable to Foot as in the original |
| `pres-w7.play` | Wave 7 presentation (docs/gap/wave7/12-presentation.md): voices keyed by rules id (`voice(ID,field)`), EVA UnitReady / Canceled, group number, Desolator eruption, Robot Tank on/offline, drone move loop, Chrono wave beam, Prism split beams (`pres(fragment_beam)`), Rocketeer Hover/Fly, no UnitLost for V3 rockets (`pres(spawned_no_unit_lost)`), Kirov crash impact (`pres(crash_impact)`), Demo Truck DEMTEXP (`pres(death_weapon)`) |
| `pres-w7-mc.play` | W7-10-05 / W7-12-23: SOV09 Yuri capture (mc-range orders) -> `mc_pairs`, `pres(mind_control)`, MINDANIM ring + link line shot |
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
`m-sov09` (one per fixed W2-CA blocker class). Real-order (cheat-free) proofs include
`m-sov01-real`, `m-all03-real`, `m-sov07-real` (WON, about 3 min) and `m-sov09-real` (opening + the
first power-plant blackout); their generators live in `scripts/play/gen/` (edit those, not the `.play`).
After wave 7 (w7r2) `m-sov01-real` (Normal, WON f9334, play_hash `a7c65a53fd1057a8`), `m-sov07-real`
(Easy, WON f41455, `36097e32b872b66b`) and `m-sov09-real` (Easy, WON f18124, `2c627b73ef432b25`) were
re-planned for the original speeds, area-guard scans and call for help: SOV01's army shoots the six
pillboxes out before walking past them; SOV09 lures the SEALs and then the Grizzlies out of the
compound (Yuri 0 takes one) before the IFV shells the plants. SOV07 needed only the engine fix for
two harvesters meeting head-on (UnitClass::Scatter 0x742880).
fv7b (the w7r1/w7r2/w7r3 engine changes merged together, bisected with per-change toggles; no
engine fault found, choreography only): `m-trn02-real` stays on each building until it falls and
masses 12 GIs per sweep (WON f8938 on 9bf5ccd); `m-all03-real`'s regrouping Grizzlies go at a raider
within 8 cells (Normal, WON f25095, `6a72c4b32241c0f8` on 844a88a); `m-all10-real`'s Rocketeers hunt
with the tanks -- the invasion V3s now shell the base from their Approach_Target ring (Normal, WON
f17211, `26ab47b1ab2f50ac` on 844a88a); `m-sov09-real`'s breaching IFV drives back to the gap before
each lure hit (Easy, WON f19877 on 9bf5ccd; NOT WON after fv7c's unified call for help, which no
longer draws the SEALs/Grizzlies out). `m-trn02-real` is NOT WON after fv7c: the per-tile LandType
makes the Shore sandbar between the bases Beach (TMP byte 10, impassable to Foot) and the only bridge
has a destroyed middle section (overlay 101), so no ground route joins the two bases. fv7d: that is the
original's map too -- the route is the bridge, which the RedTeam engineer repairs at the CABHUT once the
player deploys (`RedTeamRepairBridge`); the clone gave the computer engineer an attack order instead of
the original's Capture/Enter (InfantryClass::Mission_Attack 0x51F0F0). `m-trn02-real` WON again (f9460).
fv7d: `m-sov09-real` WON again (Easy, f27447, `819650fe268955ad`). fv7c's call for help is faithful (every
building hit with an attacker calls FUN_00707500; recruits stop once their ThreatPosed passes the IFV's 10 x
ComputerBaseDefenseResponse 3), so one lure draws about two units: the script now lures three times for the
SEALs and three for the Grizzlies, parks both IFVs south of the wall after the blackout, and Yuri 2 takes the
President from (62,67) without first taking his guard (the three GDI SEALs on his stop answer that).
`move-bridge-cliff` section 1 now aims at (55,58): (57,58) is Water08 sub 3, TMP byte 15 = Rock in the original.
fv7e (all three WON / PASS again on the same Normal difficulty): `m-sov02-real` (PASS f1038, `cb52931ddc171560`)
was an engine fault, not the Beach rule or the unload height gate. Since fv7c an amphibious SAPC blocked at sea
can be pushed aside (ScatterContent -> UnitClass::Scatter 0x742880, cells tested by the unit's own movement
domain); SAPC u105 was pushed (93,49) -> (92,50), stopped there, and the campaign team took the idle member as
"arrived", so its Unload line held at sea forever. The original's coordinate move (TeamClass FUN_006EB030, the
callee of every move-family script action) re-sends a member farther than Stray from the team destination
whose NavCom is empty, and the line completes only when all are in; the campaign runtime now does the same.
`m-sov01-real` WON again (Normal, f10229, `c4a58326930c54d2`) with that fix and no script change (one
'Initial Paratroopers' member stranded 3 cells off its drop point is re-sent; the fight unfolds differently --
a fragile win, not a re-tune). `m-sov04-real` WON again (Normal, f13439, `6a302d236c36f477`): with the faithful
Beach block the landing craft unload on a different schedule, the feint infantry were ashore before the MCV, and
the target loop sent the Rhinos after them south-west; the generator now holds the Rhinos at (108,48) until
`free(1,AMCV) >= 1` (choreography).

## Multiplayer (agent S, engine/mp_pages.inc)

Pages: `shell:Main Menu` -> `button:Network` (scene `mp-lan`) /
`button:Internet` (scene `mp-online`) -> `mp-lobby` -> `game` (NET-PLAY:
the lockstep match; the old `mp-start` placeholder page is gone). Edits on every page: `edit:<id>`; while an
edit has focus `type "..."` appends, `key delete` clears it, `key enter`
commits (chat: sends). Probe `edit(<id>)`, `focus`, `status`.

| Scene | Targets | Probe keys |
|---|---|---|
| `mp-lan` | `button:Host Game`, `button:Join Game`, `button:Join by IP`, `button:Back`, `edit:nick`, `edit:ip`, `game:<i>` | `lan_games`, `lan_listening`, `lan_sel`, `lan_game_host(i)`, `lan_game_room(i)`, `nick` |
| `mp-online` | `button:Connect`, `button:Host Game`, `button:Host Private`, `button:Join Game`, `button:Join Code`, `button:Back`, `edit:server`, `edit:nick`, `edit:code`, `room:<i>` | `online_state` (`idle/connecting/hello/ready/failed`), `rooms`, `room_code(i)`, `server_name`, `motd`, `online_error` |
| `mp-lobby` | `button:Start Game` / `button:Customize Battle` (host), `button:Ready` / `button:Not Ready` (client), `button:Back`, `edit:chat`, `combo:kind<N>` (host, rows 1-7), `combo:country<N>` / `combo:color<N>` / `combo:team<N>` / `combo:start<N>` (rows you may edit), with a list open `pick:<label>` / `item:<i>` / `list:up` / `list:down`, `check:short|mcv|crates|superweapons`, `slider:speed|credits|units=<v>` (host), `map:<file>` + `button:Use Map` / `button:Cancel` while choosing | `mp_role` (`host/client`), `mp_net` (`lan/online`), `mp_state` (`joining/joined/rejected/lost/started`), `reject_reason`, `local_slot`, `members`, `rev`, `players`, `humans`, `max_players`, `map`, `map_digest`, `room_code`, `where`, `lan_port`, `credits`, `unit_count`, `game_speed`, `check(<id>)`, `can_start`, `choosing`, `combo_open`, per slot N `slot_kind(N)` (`Open/Closed/Human/AI`), `slot_name(N)`, `slot_ready(N)`, `slot_has_map(N)`, `country(N)`, `color(N)`, `team(N)`, `start(N)`, `ai(N)`; `chat_count`, `chat_last` (`"Name: text"`), `ready` (client) |
| `game` (a network match) | the usual game targets | NET-PLAY keys (only in a match): `net` (1), `sim_frame`, `net_frame`, `net_stalled`, `net_stalls`, `net_players`, `net_max_ahead`, `net_fsr`, `net_left` (our EXIT executed), `net_exiting`, `net_end` (the match ended: out of sync / mismatch / timeout), `net_end_title`, `net_oos_frame`, `net_chat_open`, `net_away`, `net_hash`, `net_hash_avg_us`, `net_house_out(h)` |

Network is real time (wall clock), not the harness's virtual clock: wait
with large `until` budgets (`until 2000000 humans == 2`). Two-process tests:
`scripts/play/mp-lan-loopback.ps1` (host + joiner over UDP, LAN discovery on
50010) and `scripts/play/mp-online-loopback.ps1` (also starts `ra2lobbyd
--allow-loopback`; the join goes through the room list and the CnCNet V3
tunnel). Both check that the two games agree on the seed and options string
and that the match starts. `scripts/play/net-match.ps1` [`-Online`] [`-DesyncAt N`]
plays a whole 3000-frame match between two processes (net-*-host/join.play)
and compares the two machines' sync-hash trails (`[net] trail f=N crc=X`
on stderr); in a match Enter opens the chat line (`type`, `key enter`).
