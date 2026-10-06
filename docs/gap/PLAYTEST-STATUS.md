# Playtest status — `--play` scripts vs the real game

Produced by the G0 harness (`ra2engine --play`, `scripts/play/README.md`). Each
row is one tagged assertion of a playable-core script, run through the REAL
shell -> lobby/campaign -> `GameScene`. Columns:

* **63edea6** — master before any gap fix (the "nothing works" baseline).
* **7778b66** — master after agent A (economy, 5948a41) and agent B (controls,
  7778b66) merged; this is the current expectation. Re-run with
  `pwsh scripts/play/run-all.ps1` (one script at a time) and update this table
  whenever a phase merges.

`PASS` / `FAIL` are the harness verdicts; `n/a` = the step did not exist in that
revision of the script. `@NEW-n` rows are findings the harness surfaced that are
not in GAP-ANALYSIS.md. Owners: A economy/production, B controls/input,
C combat/movement, D AI, E render/audio, F MCV/start/win-lose/campaign.

## Summary

| Script | 63edea6 | 7778b66 | play_hash @7778b66 |
|---|---|---|---|
| `sk-a` | FAIL, 28 failures | FAIL, 11 failures | `e83dfb8eb39f6228` |
| `sk-s` | FAIL, 52 failures | FAIL, 14 failures | `394c1d9c7629c01b` |
| `sk-end` | FAIL, 2 | FAIL, 2 | `b7a79473c7d8138c` |
| `sk-a-lose` | FAIL, 4 | FAIL, 4 | `7bbaa6d56e273753` |
| `m-all01` | FAIL, 5 | FAIL, 2 | `01fe9ec267dfa2e0` |
| `m-sov01` | FAIL, 6 | FAIL, 6 | `d3e4d4b854b60010` |
| `shell-smoke` | FAIL, 14 (+hard stop) | FAIL, 4 (+hard stop) | `57d260958ea5173e` |
| `controls` | n/a | FAIL, 2 | `a6a838a2a270dbaf` |
| `probe-shell`, `probe-game`, `probe-flow`, `probe-perf` | PASS | PASS | (self-checks) |

Determinism: every script was run twice on the same tree with identical
`play_hash` (checked on @7778b66 with `sk-end` and `controls`; the hash folds the full probe
every 15 frames). No `play_hash` is pinned yet — a hash is pinned only once its
script passes (ROADMAP-playable §G rule 6).

## Failures by script

### sk-a — Allied vs 1 Soviet AI

| Step | Expected | Actual @63edea6 | Actual @7778b66 | G-ID | Owner |
|---|---|---|---|---|---|
| L26 | `credits == 10000` at start (one wallet seeded from the lobby) | FAIL 20000 | PASS | G-21 | A |
| L27 | `own_on_screen > 0` — the view opens on the player's start | FAIL 0 (camera at the map centre) | FAIL 0 | NEW-1 | F |
| L28-L29 | start with an AMCV and no ConYard | FAIL (pre-built GACNST, no AMCV) | FAIL | G-23 | F |
| L30 | `sw(0) == 0` — no superweapon at frame 0 | FAIL (Nuke granted) | PASS | G-25 | A |
| L33-L35 | select AMCV, `D` deploys, ConYard built | FAIL (no AMCV) | FAIL | G-23 | F |
| L44 | left-click on ground moves the selection | FAIL (deselects) | PASS | G-05 | B |
| L47 | `S` stops the unit | FAIL | PASS | G-04 | B |
| L61-L67 | Esc in game opens the options menu, Esc again resumes; process never quits | FAIL (process would quit, twice) | PASS | G-01 | B |
| L78 | refinery's miner reaches `Harvesting` within 1800 f | FAIL (stays None) | PASS | G-20 | A |
| L79 | credits rise within 2700 f (harvest income) | FAIL | PASS | G-20 | A |
| L90-L95 | Engineer offered and trainable | FAIL (not in roster) | PASS | G-22 | A |
| L105 | Chrono Miner in the vehicle tab | FAIL | PASS | G-22 | A |
| L106 | AMCV in the vehicle tab (rebuildable MCV) | FAIL | FAIL | G-22 / G-23 | A / F |
| L109-L110 | train a 2nd Chrono Miner | FAIL | PASS | G-22 | A |
| L116 | AI trains a unit within 3000 f | FAIL | PASS | G-60 | D |
| L117 | AI damages the player within 6000 f more | FAIL | FAIL | G-60 | D |
| L125-L126 | Prism Tank (SREF) has damage and range | FAIL (0 / 0) | FAIL | G-41 | C |
| L139 | Pillbox kills 3 spawned Conscripts within 450 f | FAIL (1 died) | FAIL | G-40 | C |
| L145, L147 | after the enemy is gone (`world_outcome == WON` PASSES), the session ends WON | FAIL (stays RUNNING) | FAIL | G-02 | F |

### sk-s — Soviet vs 1 Allied AI

| Step | Expected | Actual @63edea6 | Actual @7778b66 | G-ID | Owner |
|---|---|---|---|---|---|
| L14 | Yuri not in the lobby side cycle | FAIL | FAIL | G-03 | A |
| L23 | `credits == 10000` | FAIL 20000 | PASS | G-21 | A |
| L24 | view opens on the player's start | FAIL | FAIL | NEW-1 | F |
| L25, L34-L36 | SMCV start + deploy | FAIL | FAIL | G-23 | F |
| L26-L29 | Soviet infantry/sidebar (`cameo(NAPOWR)`, no `GAPOWR`) | FAIL (Allied sidebar) | PASS | G-03 | A |
| L30, L100 | Tesla Coil offered in the defence tab | FAIL | FAIL (tab = NAFLAK, NALASR, NAWALL) | G-26 | A |
| L31 | no SW at start | FAIL | PASS | G-25 | A |
| L41-L56 | NAPOWR -> NAHAND -> Conscript | FAIL (no cameos) | PASS | G-03 | A |
| L60-L67 | NAREFN -> War Miner harvesting | FAIL | PASS | G-03 / G-20 | A |
| L68 | credits rise within 2700 f | FAIL | **FAIL — the War Miner fills to 40 bales and stays in `ReturnToRef` forever, not moving (`probe-soviet-econ.play`)** | G-20 | A |
| L73-L84 | NAWEAP -> Rhino; Flak Track + War Miner offered | FAIL | PASS | G-03 / G-22 | A |
| L82 | SMCV offered | FAIL | FAIL | G-22 / G-23 | A / F |
| L88-L90 | the Allied AI builds GA\* (GAPOWR), no NA\* | FAIL | FAIL (no GAPOWR in 3000 f) | G-64 | D |
| L94 | the (Allied) AI trains a unit | FAIL | FAIL (the Soviet AI does, see sk-a L116) | G-60 / G-64 | D |
| L95 | AI damages the player | FAIL | FAIL | G-60 | D |
| L109 | Sentry Gun kills 2 spawned GIs within 450 f | n/a (step was Tesla-based) | FAIL (1 died) | G-40 | C |
| L115, L117 | session ends WON | FAIL | FAIL | G-02 | F |

### sk-end, sk-a-lose — the end of a skirmish

| Script / step | Expected | Actual @63edea6 | Actual @7778b66 | G-ID | Owner |
|---|---|---|---|---|---|
| sk-end L12 | `cheat kill_house 0` -> `world_outcome == LOST` | PASS | PASS | — | — |
| sk-end L13 | session ends LOST (result scene) within 900 f | FAIL (RUNNING) | FAIL | G-02 | F |
| sk-end L16 | result -> main menu | FAIL | FAIL | G-02 | F |
| sk-a-lose L19 | hardest AI kills a player unit within 9000 f, no input | FAIL | FAIL | G-60 (G-63: lobby HARD indexes the easiest table) | D |
| sk-a-lose L20 | player defeated within 18000 f more | FAIL | FAIL | G-60 | D |
| sk-a-lose L21, L23 | session ends LOST | FAIL | FAIL | G-02 | F |

### m-all01, m-sov01 — campaign mission 1

| Script / step | Expected | Actual @63edea6 | Actual @7778b66 | G-ID | Owner |
|---|---|---|---|---|---|
| m-all01 L11-L33 | shell -> New Campaign -> loading screen -> mission; objectives arm (f≈653), triggers fire, F1 briefing | PASS | PASS | — | — |
| m-all01 L36 | TechLevel 3: no Weather Device | PASS | PASS | G-15 | F |
| m-all01 L37 | TechLevel 3: no Battle Lab (GATECH) | FAIL | FAIL | G-15 | F |
| m-all01 L40 | Tanya present | PASS | PASS | — | — |
| m-all01 L44 | with no input every player unit is dead by f≈2459; the mission must then end LOST | FAIL (RUNNING with 0 units) | FAIL | NEW-2 | F |
| m-all01 L49-L52 | Esc mid-mission opens the options menu | FAIL (process would quit) | PASS | G-01 | B |
| m-sov01 L11-L30 | mission starts, objectives arm, SMCV present | PASS | PASS | — | — |
| m-sov01 L33-L34 | SMCV deploys (`D`) into NACNST | FAIL | FAIL | G-23 | F |
| m-sov01 L36-L37 | Soviet sidebar | PASS | PASS | — | — |
| m-sov01 L38 | TechLevel cap: no Nuclear Silo | FAIL | FAIL | G-15 | F |
| m-sov01 L41-L44 | build NAPOWR from the deployed ConYard | FAIL (no ConYard) | FAIL | G-23 | F |

### shell-smoke, controls

| Script / step | Expected | Actual @63edea6 | Actual @7778b66 | G-ID | Owner |
|---|---|---|---|---|---|
| shell-smoke L9-L15 | main menu, Options page round trip | PASS | PASS | — | — |
| shell-smoke L17-L19 | Esc on the main menu does not quit | FAIL | PASS | G-01 | B |
| shell-smoke L29-L32 | Esc in the lobby = Back | FAIL (quit) | PASS | G-01 | B |
| shell-smoke L45-L52 | Esc closes / opens the in-game options menu | FAIL | PASS | G-01 | B |
| shell-smoke L57-L81 | F10 menu, Save (banner "GAME SAVED"), Load row 0, resumed game has the same units/buildings | PASS | PASS | — | — |
| shell-smoke L89-L91, controls L110-L113 | Esc mid-placement cancels the placement (FUN_004AAC40 chain: placement -> repair -> sell -> SW -> dialog 0xB5) and does not open the menu | FAIL (quit) | FAIL (menu opens, placement stays armed) | G-01 | B |
| shell-smoke L98-L101 | Abort Mission asks for confirmation (`confirm_open`, reserved probe key) | FAIL (straight to main menu) | FAIL | G-14 | F |
| shell-smoke L106-L109 | Exit Game asks for confirmation | FAIL (process would quit, hard stop) | FAIL | G-14 | F |
| controls (all other steps: shell Esc, select / left-move / right-deselect, building select, Ctrl+1/1 groups, S stop, Q/W/E/R tabs, K/L modes, right-click cancel, queue hold / resume / abandon) | B's selftest behaviour | n/a | PASS | G-01, G-04, G-05, G-07, G-10 | B |

## New findings (not in GAP-ANALYSIS.md)

| ID | Finding | Evidence | Owner |
|---|---|---|---|
| NEW-1 | A skirmish opens with the camera on the map centre, not on the player's start: no own object is on screen at frame 1 (`sk-a` L27, `sk-s` L24, `probe-game` start.png shows only shroud and ore). | `own_on_screen == 0` | F (skirmish start) |
| NEW-2 | ALL01 with no player input: every house-0 unit is dead by f≈2459 but the mission stays RUNNING (no LOST, no result screen). Needs RE of the map's `Lose 13` chain / house-defeat path before fixing. | `m-all01` L44 | F (campaign) |
| NEW-3 | Soviet War Miner never unloads: full at 40 bales, `ReturnToRef` forever, not moving; the Allied Chrono Miner path works. | `sk-s` L68, `probe-soviet-econ.play` dumps | A (G-20) |
| NEW-4 | The Allied AI slot neither builds GA\* power nor trains in 3000 frames, while the Soviet AI does train (sk-a L116 PASS). | `sk-s` L88-L94 | D (G-64) |
