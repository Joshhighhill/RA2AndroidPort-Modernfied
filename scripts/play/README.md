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
| `wait <n>` | Let n frames pass. | n |
| `click <target> [right] [shift] [ctrl] [alt] [nopark]` | Move + press + release at the target (original scheme: left = select AND order, right = cancel/deselect). The pointer then parks at (560,360) next frame so it does not sit in an edge-scroll band. A `cameo:ID` on another tab clicks that tab first. | 1 |
| `drag cell\|xy x1 y1 x2 y2 [right]` | Press, move, release (box select). | 1 |
| `move <target>` | Pointer motion only. | 1 |
| `key <name> [shift] [ctrl] [alt]` | Key down this frame, up next frame. Names: SDL key names plus `esc enter space tab backspace delete up down left right f1..f12 home end pageup pagedown`. | 1 |
| `hold <name> <n>` | Key held for n frames (the script continues meanwhile). | 1 |
| `type "<text>"` | One `SDL_TEXTINPUT` event. | 1 |
| `view <cell\|unit\|building ...>` | Pan the camera so the cell is centred (as scrolling would). | 0 |
| `place auto` | With a building armed for placement, click the nearest valid origin to the player's first building (same `placement_check` as the game). | 1 |
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

**Targets.** `cell X Y` (map cell centre; the view is centred first when it is
off-screen), `xy X Y` (window pixels), `unit H:TYPE[:n] [+dx +dy]` (the n-th
live unit of house H and rules id TYPE, `*` = any; optional signed cell offset),
`building H:TYPE[:n] [+dx +dy]`, or a **named target** from the probe:

| Scene (`scene`) | Named targets |
|---|---|
| `shell:<page>` (`shell:Main Menu`, `shell:Single Player`, `shell:New Campaign`, `shell:Options`, `shell:Movies & Credits`) | `button:<label>` (sugar: `click button "<label>"`), `dialog:*` when the Load dialog is open |
| `lobby` | `start`, `back`, `map:-`, `map:+`, `map:-10`, `map:+10`, `opt:<ROW>:-` / `opt:<ROW>:+` (`AI_PLAYERS DIFFICULTY STARTING_CASH UNIT_COUNT TECH_LEVEL CRATES SUPERWEAPONS SHORT_GAME`), `slot<N>:side` / `slot<N>:color` / `slot<N>:team` |
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
`lobby`: `side(N)` (`ALLIED`/`SOVIET`/`YURI`), `color(N)`, `team(N)`,
`opt(<ROW>)` (displayed value), `ai_players`, `cash`, `crates`, `map`,
`map_name`, `map_ix`, `map_count`.
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

**Cheats** (`game` only; dev hooks for reaching a state quickly — acceptance
should prefer real input): `kill_house H`, `damage_building H:TYPE[:n] DMG`,
`credits N`, `spawn TYPE H cell X Y`, `spawn TYPE H building H:TYPE[:n] +DX +DY`
(TYPE from the player or AI roster).

Keys reserved for later phases (scripts already assert them and report
`missing-probe` until a phase adds them): `confirm_open` + targets
`confirm:yes`/`confirm:no` (Abort/Exit confirmation, G-14).

## The scripts

| Script | What it encodes |
|---|---|
| `sk-a.play` | Playable core SK-A: Allied vs 1 Soviet AI — start state, MCV deploy, select/move/stop, power/refinery/barracks/factory, income, GI/Engineer/Grizzly/Chrono Miner, AI trains + attacks, weapons, Pillbox fires, win path |
| `sk-a-lose.play` | SK-A with no input on the hardest lobby difficulty: the AI must win and the session must end LOST |
| `sk-s.play` | SK-S: Soviet vs 1 Allied AI — no Yuri, Soviet sidebar/roster, SMCV, Conscript/Rhino/Flak Track/War Miner, Tesla Coil, Allied AI builds GA\*, win path |
| `sk-end.play` | Fast lose path (`cheat kill_house 0`) -> LOST result -> main menu |
| `m-all01.play` | Allied mission 1 from the shell: loading screen, objectives arm, briefing, TechLevel 3 cap, Tanya, Esc |
| `m-sov01.play` | Soviet mission 1: objectives, SMCV deploy, Soviet sidebar, TechLevel cap |
| `shell-smoke.play` | Menus, Options round trip, lobby round trip, Esc in menu/lobby/game/placement, F10 menu, save -> load -> resume, Abort/Exit confirmation |
| `probe-*.play` | Harness self-checks (no gameplay claims; all pass on master) |

The expected results on each tree are kept in `docs/gap/PLAYTEST-STATUS.md`.
