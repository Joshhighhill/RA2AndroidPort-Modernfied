# Wave-4 gap analysis 02: player control in campaign missions (desktop mouse/keyboard)

Read-only audit of master `4647b17` (2026-10-07). Worktree `D:\ra2-wt\w4-ctl` (branch `w4-ctl`,
nothing committed), Release build `D:\ra2-wt\w4-ctl\build-rel`, private Ghidra copy `D:\ra2-re\w4-ctl`
(scripts `D:\ra2-re\scripts\w4ctl\lock1-3.py`, output `D:\ra2-re\w4-ctl-out\`). All evidence paths
below are relative to `D:\ra2-wt\w4-ctl\scratch\`.

User report: *"I can't control the mission"* (ALL01 named). The `--play` mission scripts pass.

## TL;DR: root cause

1. **ROOT CAUSE (BLOCKER): Tanya and the Navy SEAL cannot swim.** `engine/main.cpp:915-919` gives a unit
   the Amphibious movement domain only when `MovementZone` is exactly `"amphibious"`. RA2 `rules.ini`
   gives `[TANY]`, `[GHOST]` (SEAL), `[PCOMMANDO]`, `[JFISH]` and `[CPHAL]` `MovementZone=AmphibiousDestroyer`
   (`[APC]` has `AmphibiousCrusher`), all with `SpeedType=Amphibious`. They fall through to Land.
   In ALL01 the player starts with **one** controllable unit, Tanya. The 11 GIs on Liberty Island
   belong to `Other3 House` (`PlayerControl=no`) and become the player's only when the player's
   unit *enters* the water cells (61..64, 84) between the pier and the island. That fires
   "Initial Paratroopers 1" (event 1 EnteredBy house 13), then "Enable GI's", which runs team
   `0A7475CC` script `20,13` (ChangeHouse to Player) plus "ButtNugget" AllChangeHouse. Tanya
   cannot leave the pier: water clicks show the `nomove` cursor and she stops at the shore
   (`out-all01b`: ordered to (62,85), she stops at (63,97)). So the GIs the player can see never
   answer clicks, the one unit the player owns cannot go anywhere useful, and the mission never
   moves past its opening.
   **Proof:** a scratch build that maps every `Amphibious*` zone to the Amphibious domain (a one-line
   experiment, reverted) lets Tanya swim to (62,77). Then 21 Soviet paratroopers drop and
   `units(0,E1)` goes 0 → **10**: the GIs become the player's (`out-amphib3/play.log` L16,
   `out-amphib3/a3_180.png`).
2. **Contributing (MAJOR): the opening is not presented as in the original, so the human gets no cue
   about which units are theirs.**
   - **Wrong opening view.** The camera opens on "the player's first unit" (`main.cpp:6565-6571`).
     In ALL01 that is Tanya in the map's off-board staging strip at (62,108), drawn behind a
     building in black shroud. On screen there is nothing to select (`out-all01/t0.png`). The
     original opens on `[Waypoints] 98` (ALL01: (58,72), the GIs on Liberty Island). This is
     PROBABLE, see RE-W4-MC-1.
   - **`[Map] LocalSize` is ignored.** The original hides that staging strip. Nothing in the
     engine reads `LocalSize` (`grep -rn LocalSize engine src` finds 0 hits).
   - **LockInput/UnlockInput (actions 46/47) are only recorded** (`map_trigger.cpp:118-127`;
     `campaign_visuals.inc:61-148` has no case for them). In the original, input is locked
     from 0 to 55 s and the cursor is hidden. The player's first command comes right after
     "Battlefield control online", the `TanyaAttackCommand` voice and the Tanya flash. In the
     clone the player can click during the intro, but scripted teams then override them: an
     order to Tanya at t≈33 s is replaced by the Tanya1 team's move to wp36 at t≈50 s
     (`out-all01`). The camera pan (action 48) also re-centres the view every logic tick for
     22–44 s, which fights the player's scrolling.
3. Nothing else blocks control. Selection, the original left-click scheme, PlayerControl houses
   (`World::controllable`, `world.hpp:1116`), hotkeys and the touch-bar modes all work in every
   mission's start (table below). The input-event path is the same one the harness drives.

## 1. Exact reproduction

**A. The human path, scripted through the real shell.** These use no cheats and no `set campaign`.
Script `ctl-all01.play`, run as `build-rel\ra2engine.exe --play scratch\ctl-all01.play --out scratch\out-all01`:

1. Main Menu → `Single Player` → `New Campaign` → the Allied well → `Load`. The loading screen
   auto-starts. The game scene is up at f=29.
2. **T0:** `selected=0`, `units(0)=1` (Tanya at (62,108)), `units(7)=11` (the GIs, `Other3 House`).
   The view is centred on Tanya, but she is hidden behind a building in shroud (`t0.png`). Clicking
   her screen position selects her (`sel=(62,108)`). A move order works on land (she moves to (64,108)).
3. **T30:** the "Start Scroll" pan (action 48, wp S (57,92), speed 1) drags the view to the harbour.
   Tanya can still be ordered, until the Tanya1 team takes her to wp36 (60,99) at t≈50 s.
4. **T60–T120** (`ctl-all01b.play`, `out-all01b`): she is ordered to (62,85) in the water toward
   Liberty Island. The cursor is `nomove`. She stops at (63,97) on the pier.
   `units(0)=1` for the whole run. The 11 GIs stay `Other3`. Clicking them does nothing:
   `classify()` sees a non-controllable ally on ground with nothing selected → `Action::None`
   (`input_original.cpp:115-127`). **This is the user's report.**
5. SOV01 through the real shell (`ctl-sov01.play`): the player has 1 unit at T0 and 22 by T30.
   Select and move work at T0, T30, T60 and T120. In this mission the only difference from the
   original is that input is never locked.
6. ALL02 and SOV02 cannot be reached through the real UI without winning mission 1. Winning ALL01
   is impossible by real input (point 4). They were started with `set campaign` (the same
   `CampaignLoadScene` that the result screen's Continue builds). Both are controllable from T0/T2
   (table).

**B. Real OS input (attempted; blocked by the environment).** `rc.py` drives the interactive exe with
posted `WM_MOUSEMOVE`/`WM_LBUTTONDOWN`/`UP` and captures it with `PrintWindow`. On this shared desktop,
stray input reached the window while it was visible: the main menu's Exit-confirm popped up by itself
and the game closed with exit code 0 within 3–7 s. That screenshot was overwritten by a later
session; the effect reproduced on 6 launches.
With the window disabled or moved off-screen, SDL did not turn the posted button messages into clicks
(`rcshots/d1.png`: still on the main menu). So `Platform::poll` (`platform.cpp:239-270`) was reviewed
instead of executed. It is a 1:1 mapping of SDL button/motion events to the same `InputEvent`s that
`--play` injects into `Scene::handle`. Logical scaling uses `SDL_RenderSetLogicalSize`
(`platform.cpp:73,97`). Edge scroll and arrow keys are per frame (`main.cpp:5596-5630`). No
interactive-only branch was found that would block selection. A raw-event record/replay mode is
proposed below (HB-6).

## 2. Per-mission table

Data: `spot-*.play` (`set campaign`, select + order at T2/T30/T90), `pc-*.play` (targeted orders to
PlayerControl houses), census of the 24 maps (`ctl_census.py` → `census.txt`; `spot_summary.txt`).
"Own" counts house 0. "PC" counts `PlayerControl=yes` houses, which `World::controllable` accepts.
Lock and pan times are in game seconds. Event 13 counts from the moment the trigger is enabled,
event 47 counts from scenario start, and event 8 fires immediately (`docs/re/map-triggers.md` §4).

| Mission | Player house | Starting units owned/controllable (T0 → T2 → T30) | Control in the clone (locked? when/why) | Expected (original) |
|---|---|---|---|---|
| **ALL01** | `Player House` (type 13) | Tanya only, staged off-board (62,108) and hidden; 11 GIs = `Other3` (not PC) | **Never locked. Effectively uncontrollable:** Tanya cannot swim → the GIs are never handed over; the opening view shows no unit | Lock 0→55 s (Lock Input → Unlock Input 3), view at wp98 (58,72) on the GIs, pan to S at 30 s, Tanya walks in at 50 s and flashes; she **swims** to the island → paratroopers + GIs become the player's |
| ALL02 | `Americans` | 5 own (3 ENGINEER, 2 E1) + Tanya in PC `Germans` → 7+7 by T30 | controllable from T0 (Tanya moved, `pc-ALL02`) | no LockInput in the map: control from T0 ✓ |
| ALL03 | `Player House` | 36 own | controllable from T2 | Lock on `!!Lock/Reveal` (event 47 t=0) → `UnlockInput` 18 s; not locked in the clone |
| ALL04 | `Player House` | 6 own; PC `PlayerAlt`/`PlayerAlt2` +25 by T2 | controllable from T2 | Lock on `StartNavyEnters1` (47, t=0) → unlock 20 s |
| ALL05 | `Player House` (owns nothing) | 0 → 0 → 1 (Tanya House, PC) | controllable once Tanya lands (moved, `pc-ALL05`) | Lock (47 t=0) → `Mission Start 4` unlock +5 s; pans CU/CV |
| ALL06 | `Player House` | 0 units, 23 buildings → 27 by T2 | controllable from T2 | Lock `Player Start Forces Go` (event 8) → `Bring Camera Back` unlock |
| ALL07 | `Player House` | 20 own | controllable | `_Intro Step 1` lock → `_Intro Step 5` unlock at 23 s + 3 pans |
| ALL08 | `Player House` | 0 → 8 paradropped → moved to PC TanyaLand/GILand/EngineerLand by T30 | controllable (houses 5 and 6 moved, `pc-ALL08*`) | Lock at 0 → `Camera Back to Paratroops` unlock 13 s |
| ALL09 | `Player House` | 0 → 5 SEALs (GHOST) | the SEALs move on land; **cannot swim** (same root cause) | Lock `SEAL Insertion GO` → unlock "Seals have Landed" (EnteredBy) |
| ALL10 | `Player House` | 21 own, 17 buildings | controllable; view pulled away by pans | Lock `_0 Lock Input` → `_5 Player Start` unlock 25 s |
| ALL11 | `Player House` | 21 own | controllable | Lock `Battlefield Control Hold` → unlock at scenario t=62 s |
| ALL12 | `Player House` | 0 → 3 SEALs (Kremlin courtyard); PC `Other2` later | controllable on land (`pc-ALL12`); **SEALs cannot swim** | no start lock |
| SOV01 | `Player House` | 1 → 1 → 22 | controllable through the real shell at T0/T30/T60/T120 | Lock at 0 → Start Scroll 22 s (speed 0) → Unlock +4 s |
| SOV02 | `Confederation` | 0 → 33 (beach landing) | controllable from T2 | Lock (47 t=0) → unlock on local 2 |
| SOV03 | `Player House` | 1 + 2 buildings | controllable | no start lock |
| SOV04 | `Player House` | 2 + 3 buildings | controllable | no start lock |
| SOV05 | `Player House` | 16 own + 4 Tesla Troopers in PC `Other3` | controllable (house 7 moved, `pc-SOV05`) | no start lock |
| SOV06 | `Player House` | 0 → 17 → 22 | controllable from T2 | Lock `PlayerForcesLand1` → unlock 35 s |
| SOV07 | `Player House` | 14 + 7 buildings | controllable | none at start |
| SOV08 | `Player House` | 16 own | controllable (`pc-SOV08`) | none |
| SOV09 | `Confederation` (owns nothing) | 0 → 0 → 3 (Yuri, PC `Africans`) | controllable by T30 (`pc-SOV09`); **LOST at ≈f1200 with no input** (W4-MC-08) | Lock (47 t=0) → unlock at scenario t=65 s (by `Africans`); pan AL at 35 s |
| SOV10 | `Player House` | 0 units, 5 buildings → 10 | controllable from T2 | Lock 0 → unlock 10 s (or 3 s) + map pans |
| SOV11 | `Player House` | 34 own | controllable | Lock 0 → Start Scroll 14 s → unlock +3 s |
| SOV12 | `Player House` | 0 units, 4 buildings (build the navy) | nothing to select by design; the sidebar is the control | none |

Only ALL01 locks the human out completely. ALL09 and ALL12 lose the SEALs' swimming. Their
later objectives may need it; that was not verified past T90. In ALL02 and
SOV09 the player's own house starts with no units. Control there goes through PlayerControl
houses, and that works (wave-2 W2-CA-02 fix).

## 3. Harness blind spots: why the scripts passed

| # | Blind spot | Where |
|---|---|---|
| HB-1 | **No mission script issues a single select + move order through real clicks.** `m-all01` checks the scene, objectives, briefing, TechLevel cameos, `units(0,TANY)>=1`, outcome and Esc. It never clicks a unit. `m-sov01` is the only one with one unit/cell click | `scripts/play/m-all01.play`; `grep -c 'click unit\|click cell\|drag'` = 0 in m-all01/02/04/08, m-sov03/05/06/09 |
| HB-2 | **Cheats do the playing.** `m-sov05` moves the troopers with `cheat camp_move`, `m-sov09` uses 8 cheats (`camp_killu`, `camp_attacku YURI 3 PRES`), m-all02/04/08 and m-sov03/06 use `kill_house`/`camp_*`. Wave 2's "Winnable: YES" for ALL01 was a cheat-kill of BadGuy1 (`wave2/01-campaign.md` l.35) | `engine/campaign_play.inc:37-100` |
| HB-3 | **`units(0) > 0` and `own_on_screen > 0` count units the player cannot use.** Tanya hidden in the off-board strip counts as "own, on screen" (`game_scene_play_probe.inc:185-194`). No probe asks "can a house-0/PC unit reach an objective" or "is anything selectable visible" | probe |
| HB-4 | **The harness helps the pointer.** `click unit/cell` centres the view on an off-screen target (`play_driver.inc:535-541`) and parks the pointer at (560,360). A human must find the unit; in ALL01 nothing visible at T0 is theirs | play_driver |
| HB-5 | **`set campaign` skips progression.** ALL02+ are never reached by winning; the shell-chain test of wave 2 used cheats to win ALL01 | `play_driver.inc:833-840` |
| HB-6 | **`Platform::poll` (SDL → InputEvent) and the real-time loop are never exercised.** `--play` replaces the EventSource and runs at a fixed dt with a hidden software renderer. No record/replay of raw SDL events exists | `main.cpp:4783-4856` |
| HB-7 | **Trigger presentation is unasserted.** Nothing checks `input_locked`, the opening view or the camera pans, so the 46/47/48 gap was invisible | — |

## 4. Gap table

| ID | Gap | Original [RE] | Clone (file:line) | Status | Severity | Fix | Size |
|---|---|---|---|---|---|---|---|
| **W4-MC-01** | **Amphibious infantry is Land-bound**: Tanya, SEAL (GHOST), and also PCOMMANDO/JFISH/CPHAL/APC | `rules.ini`: `MovementZone=AmphibiousDestroyer` / `AmphibiousCrusher`, `SpeedType=Amphibious`; ALL01's GI hand-over cells (61..64,84) lie in open water between the pier and Liberty Island | `main.cpp:915-919` exact match `== "amphibious"`. Experiment (prefix match) → Tanya swims, GIs → player (`out-amphib3`) | WRONG | **BLOCKER** (ALL01 uncontrollable; ALL09/ALL12 SEALs) | Map every `Amphibious*` MovementZone (or `SpeedType=Amphibious`) to `MovementDomain::Amphibious`. Check that the W2-AN-10 transport/hover paths keyed on Amphibious (`world.cpp:1131-1133,1440`, `world_an.cpp:113,629`) do not treat infantry as landing craft. Use the infantry Swim sequences in water (wave3 RC-7). Tests: unit test resolving TANY/GHOST → Amphibious; play `m-ctl-all01` (HB fix) walks Tanya onto the island by clicks and asserts `units(0,E1) >= 8` | S (+S swim anim) |
| **W4-MC-02** | LockInput / UnlockInput not executed | TAction 0x2E → `FUN_00683c80`: `DAT_00a8dd04 = 1`, `Scenario+0x35a2 = 1`, mouse cursor hidden (`DAT_008865c0` vtbl +0xc), clear the pending cursor/drag state. 0x2F → `FUN_00683cd0` undoes it. While locked: the tactical WndProc `FUN_00692ab0` drops every mouse message except 0x215; edge scroll `FUN_00692940` off; keyboard `FUN_0054ee90` passes only Esc (0x1B); `FUN_0054f420`/`FUN_0054ef80` mouse paths off; the sidebar `FUN_006ab0b0` returns 0; the main loop `FUN_0055cff0` auto-unlocks when the `DAT_00b066f4` frame deadline passes (`D:\ra2-re\w4-ctl-out\_lock1.txt`, `_lock3.txt`, `lk_*.c`, `rd_*.c`) | recorded only: `map_trigger.cpp:118-127`; no case in `campaign_visuals.inc:61-148`. The player acts during intros and team scripts then override them (Tanya re-ordered at t≈50 s) | MISSING | MAJOR (all 16 missions with a start lock; feel and script conflicts) | `VisualRequest` 46/47 → `GameScene::input_locked_`. In `handle()`, drop pointer events, hotkeys, the sidebar and edge/arrow scroll while locked; Esc still opens the menu; hide the cursor. Also probe key `input_locked`. Save/load the flag | S |
| **W4-MC-03** | Opening view | view starts at `[Waypoints] 98` (WAYPT_HOME) [PROBABLE: RA1 GPL source `WAYPT_HOME = 98` + `Start_Scenario`; not traced in gamemd, RE-W4-MC-1] | `main.cpp:6565-6571`: the first house-0 unit, without terrain height; ALL01 opens on the hidden Tanya (`out-all01/t0.png`) | WRONG | MAJOR (ALL01; any map whose first own unit is staged) | Centre on wp98 when it exists, else the current rule; account for the cell level | S |
| **W4-MC-04** | `[Map] LocalSize` ignored | the visible/scrollable rectangle; objects outside it are staged off-board (ALL01 Tanya at (62,108), bottom edge of `LocalSize=2,6,46,52` by the usual iso formula, PROBABLE) | no reader (`grep LocalSize` = 0) | MISSING | MINOR→MAJOR (staged units visible and clickable; camera can leave the map) | Clamp the camera and the shroud/visibility to LocalSize; treat cells outside it as not clickable | M |
| W4-MC-05 | Camera pan (48) fights the player's scroll | pan runs with input locked (pairs with 02) | `campaign_visuals.inc:157-163` re-centres every logic tick for 1/rate ticks (speed 0: 667 ticks ≈ 44 s at 1x; SOV01 pans from 22 s) | PARTIAL | MINOR (resolved by 02 in most maps) | Keep the pan; with 02 the user input is locked anyway. Where a map unlocks mid-pan (SOV01: unlock at 26 s), stop the pan on user scroll [RE-FALLBACK] | S |
| W4-MC-06 | A player order does not detach the unit from its script team | in the original (RA1 `EventClass::Execute` MEGAMISSION removes a player-ordered unit from its team; TS/RA2 not traced: RE-W4-MC-2) | `campaign_runtime.inc:1427-1500` `step_teams` keeps members until the script ends | UNVERIFIED | MINOR (mostly hidden by 02) | After RE-W4-MC-2: on a player order to a house-0/PC unit with `utrack.team >= 0`, release it from the team | S |
| W4-MC-07 | No control-level verification | — | HB-1..HB-7 | MISSING | MAJOR (process) | Add `m-ctl-<mission>` scripts: the real shell, no cheats, click-select + click-move at T0/T60, assert `selected`/`sel_moving`; probe keys `ctl_units` (house 0 + PC, alive, not staged), `ctl_on_screen`, `input_locked`; a "reach the objective by clicks" script for ALL01 | M |
| W4-MC-08 | SOV09 is LOST at ≈f1200 (~80 s at 1.33x) with no input | — (the original locks input until 65 s; the first lose triggers are `Pres is Dead` (event 41) and `Yuri's are Dead` (event 9)) | `out-spot-SOV09` (`outcome=LOST` by f=1200) | UNTRIAGED | MAJOR (if a human cannot prevent it) | Trace which trigger fires (trigger log) and whether the Yuri team or the President takes damage before control is possible | S |
| W4-MC-09 | No raw-input replay (HB-6) | — | — | MISSING | MINOR (test infra) | `RA2_INPUT_RECORD/REPLAY`: log `SDL_Event`s with timestamps and replay them through `Platform::poll` under the real-time driver; makes "a human can do X" testable without desktop automation | M |

## 5. Prioritized fix list

1. **W4-MC-01 amphibious zones** (S): `main.cpp:918`, prefix match on `amphibious` (or read
   `SpeedType=Amphibious`). Unit test: TANY/GHOST resolve to Amphibious, E1 to Land. Play:
   `m-ctl-all01` must, through the real shell and real clicks only, select Tanya, click a cell on
   Liberty Island, and see `units(0,E1) >= 8` within 600 frames (the scratch experiment already
   does this: `ctl-all01-amphib3.play`).
2. **W4-MC-02 LockInput/UnlockInput** (S): forward 46/47 as `VisualRequest`s. Add a
   `GameScene::input_locked_` gate at the top of `handle()` and in the scroll code of `update()`
   (after `lifecycle_event`/`ui_handle`, so Esc still pauses). Hide the cursor while locked.
   Assert `input_locked == 1` at ALL01 f=30 and `0` after f=830.
3. **W4-MC-03 opening view on wp98** (S): `main.cpp:6565`. Assert that the ALL01 view centre
   is within 3 cells of (58,72) at f=1.
4. **W4-MC-07 control-level play scripts and probe keys** (M): a gate for every mission start, so
   this class of bug cannot pass again.
5. **W4-MC-08 SOV09 no-input loss triage** (S).
6. **W4-MC-04 LocalSize** (M), then **W4-MC-06** after RE-W4-MC-2, then **W4-MC-05** and **W4-MC-09**.

## 6. RE notes and open items

- **LockInput/UnlockInput, CONFIRMED** (`D:\ra2-re\w4-ctl-out\lk_00683C80.c`, `lk_00683CD0.c`,
  `_lock3.txt`). Callers of the lock: TAction `0x6de347`, the game-end paths `FUN_004fc800`/`FUN_004fc9f0`,
  `FUN_0069b140`, `FUN_0053b190`, `FUN_0055cc60`. Unlock: TAction `0x6de35b`, the main-loop deadline
  `0x55dabc`, `FUN_0053b290`.
- **RE-W4-MC-1** (open): trace the initial tactical position in gamemd. Look for the `ScenarioClass`
  waypoint read with index 98 near the TacticalClass set-position call (`FUN_006d55e0`'s callees
  `0x6D1650`/`0x6D7BB0`/`0x6D80A0` are the centring helpers; a `PUSH 0x62` + callee intersection scan
  found nothing, so the home view is probably set through a stored cell, not an immediate).
- **RE-W4-MC-2** (open): does a player order remove a unit from its TeamClass in YR (`EventClass`
  MEGAMISSION handler)?
- The parallel audit `03-all01-opening.md` covers the rest of the ALL01 script (V3/Statue, A1-07 =
  W4-MC-02 here). It agrees on the 0–55 s lock timeline and does not cover the swim hand-over.

## 7. Evidence index (`D:\ra2-wt\w4-ctl\scratch\`)

| File | What |
|---|---|
| `ctl-all01.play`, `out-all01/` (`t0.png`, `t0_sel.png`, `t30.png`, `t90.png`, `play.log`) | the human path through the real shell; select and move at T0/T30/T90 |
| `ctl-all01b.play`, `out-all01b/` (`t80.png`) | Tanya ordered toward the island: `nomove`, stops at (63,97); GIs stay `Other3` |
| `ctl-all01-amphib3.play`, `out-amphib3/` (`a3_180.png`) | scratch exe `build-rel\w4ctl_amphib.exe` (prefix match, source reverted): Tanya swims, GIs → house 0 |
| `ctl-sov01.play`, `out-sov01/` | SOV01 through the real shell |
| `spot-*.play`, `out-spot-*/`, `spot_summary.txt`, `spot_sum.py` | 24-mission start sweep |
| `pc-*.play`, `out-pc-*/` | PlayerControl-house orders (ALL02/04/05/08/12, SOV05/08/09) |
| `ctl_census.py`, `census.txt` | per-map player house, PC houses, Lock/Unlock/pan/ChangeHouse triggers |
| `rc.py`, `rcshots/` | real OS-input driver (posted messages + PrintWindow) and why it was blocked |
| `D:\ra2-re\w4-ctl-out\` | Ghidra output (lock/unlock and their readers) |

## W4-FIX-CTL status (branch `w4fix-ctl`)

| ID | Status | Where |
|---|---|---|
| W4-MC-01 amphibious zones | **Fixed.** `is_amphibious_zone(MovementZone, SpeedType)`: `AmphibiousDestroyer`, `AmphibiousCrusher`, `Amphibious` and `SpeedType=Amphibious` → land+water (gamemd name table @0x81AA98, `docs/re/movement-zones.md`). Only a naval factory launches an amphibious unit onto water (Tanya/SEAL leave the barracks on land). Infantry in water use the Swim set. `ctl-all01`: Tanya swims, `units(0,E1)` 0 → 11. | `unit_combat.hpp`, `main.cpp` unit resolve, `world.cpp` `launch_unit` |
| W4-MC-02 Lock/UnlockInput | **Fixed.** Actions 46/47 → `GameScene::input_locked_`: pointer / key / touch past the options menu dropped, edge/arrow scroll off, cursor hidden, half gesture cancelled; Esc/Back, F1 and the options button still work. Not saved (a plain global in the original); released after 1800 frames or at mission end (no soft-lock). Probe `input_locked`. | `campaign_visuals.inc`, `map_trigger.cpp`, `main.cpp` `handle`/`update`/render |
| W4-MC-03 opening view | **Fixed, CONFIRMED** (RE-W4-MC-1 answered): `[Basic] HomeCell` (FUN_00689890) → `Waypoint[HomeCell]` centred by FUN_004acd80/FUN_006d55e0; map centre when missing. ALL01 opens on wp98 (58,72). | `campaign.cpp`, `main.cpp` `campaign_view_setup` |
| W4-MC-04 LocalSize | **Fixed (campaign).** IsWithinUsableArea FUN_00577e90 and the tactical clamp FUN_006d7bb0 decompiled (`docs/re/mission-view.md`); camera clamped, off-board placement refused, probe counts exclude off-board/shrouded units. Skirmish keeps the old free camera (open). | `campaign.cpp`, `input_original.cpp` `set_view_limits`, `realplay_scene.inc` |
| W4-MC-05 pan vs scroll | **Fixed** [RE-FALLBACK]: a trigger pan ends once anything else moved the camera after the unlock (SOV01). | `campaign_visuals.inc` |
| W4-MC-06 player order vs team | **Fixed in `w4fix-seq`** [RE-FALLBACK, RE-W4-MC-2 not traced]: Input marks the ordered selection (`World::mark_player_order`) and the campaign releases those units from their script teams. | `input.cpp`, `input_original.cpp`, `campaign_runtime.inc` |
| W4-MC-07 control-level scripts | **Done.** `ctl-all01` (real clicks, `onscreen`, arrow-key scrolling), `ctl-touch-all01` (real gestures), `ctl-all02`, `ctl-sov01`, `ctl-sides`; `m-all01`/`m-sov01` follow the original opening. New `--play` flag `onscreen`, `touch` commands, keys `ctl_units`, `ctl_on_screen`, `input_locked`, `view_cx/cy`. | `scripts/play/ctl-*.play`, `play_driver.inc` |
| W4-MC-08 SOV09 early loss | **Fixed in `w4fix-seq`**: the Russian drop groups were on the Americans' alliance team and hunted the Yuris; houses now use their own Allies= lists (see 05 W4-FIX-SEQ status). | `campaign_runtime.inc` |
| W4-MC-09 raw input replay | Open. | — |

Open observation: ALL01's GI hand-over fires only through the 4-cell strip (61..64,84). On the emulator Tanya
was ordered to the island's west dock, swam around the strip and the GIs stayed `Other3`; ordering her to the
island centre (as `ctl-all01` does) crosses it. Whether the original routes a west-dock order through the strip
was not traced (mission-sequence / pathfinding question).
