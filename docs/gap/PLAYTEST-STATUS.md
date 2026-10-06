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

The **current expectation is b049b40** (master with A, B, C and E merged; D and
F pending). The per-script tables below show 63edea6 and 7778b66; the deltas
at b049b40 are listed right after this table.

| Script | 63edea6 | 7778b66 (A+B) | b049b40 (A+B+C+E) | play_hash @b049b40 |
|---|---|---|---|---|
| `sk-a` | FAIL, 28 | FAIL, 11 | FAIL, 8 | `07b084097ddfe20c` |
| `sk-s` | FAIL, 52 | FAIL, 14 | FAIL, 13 | `a77d77282d5ecc4b` |
| `sk-end` | FAIL, 2 | FAIL, 2 | FAIL, 2 | `bef7d78b1a86dcd5` |
| `sk-a-lose` | FAIL, 4 | FAIL, 4 | FAIL, 4 | `3c90c0e249d1179c` |
| `m-all01` | FAIL, 5 | FAIL, 2 | FAIL, 2 | `42e50f02da8522ab` |
| `m-sov01` | FAIL, 6 | FAIL, 6 | FAIL, 6 | `be47fff77fd9072e` |
| `shell-smoke` | FAIL, 14 (+hard stop) | FAIL, 4 (+hard stop) | FAIL, 4 (+hard stop) | `428731525d358c3e` |
| `controls` | n/a | FAIL, 2 | FAIL, 2 | `1c6c1af2303f484e` |
| `world-depth` | n/a | FAIL, 1 | **PASS** | `12d4c50213bae79a` |
| `probe-*` self-checks | PASS | PASS | PASS | — |

### Deltas at b049b40 (C combat + E render merged)

| Script / step | 7778b66 | b049b40 | G-ID | Owner |
|---|---|---|---|---|
| sk-a L125-L126 Prism Tank has damage/range | FAIL | PASS | G-41 | C |
| sk-a L139 Pillbox kills 3 Conscripts | FAIL | PASS | G-40 | C |
| sk-s L109 Sentry Gun kills 2 GIs | FAIL | PASS | G-40 | C |
| world-depth L20 Rhino kills the GI it is sent onto | FAIL | PASS | G-47 | C |
| m-all01 L44 (no input -> all units dead -> must end LOST) | FAIL | not triggered (units survive to f≈2300 now), so NEW-2 is unconfirmed on this tree | NEW-2 | F |
| m-all01 L36 TechLevel 3: no Weather Device in the ALL01 sidebar | PASS | **FAIL (regression: GAWEAT now offered)** | G-15 | F (cap) |

Every other row keeps its 7778b66 verdict. Still failing at b049b40: NEW-1
(camera), G-23 MCV start/deploy, G-22 AMCV/SMCV in the roster, G-02 skirmish
end, G-60/G-64 AI attack + Allied AI, G-03 Yuri in the lobby, G-26 Tesla Coil,
NEW-3 Soviet War Miner never unloads, G-15 campaign TechLevel, (G-01 Esc mid-placement: expectation corrected to the binary, see row below)
mid-placement, G-14 Abort/Exit confirmation.

Determinism: every script was run twice on the same tree with identical
`play_hash` (checked on 7778b66 with `sk-end` and `controls`; the hash folds the full probe
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
| shell-smoke L88-L99, controls L117-L126 | **Expectation corrected (agent B, binary evidence below):** mid-placement Esc opens the options menu and leaves the placement pending; Esc again closes the menu; a RIGHT click cancels the placement | FAIL (quit) | PASS (script corrected; code unchanged) | G-01 | B |
|  | Evidence [RE: gamemd hunt fixB, scripts `D:\ra2-re\scripts\fixB_1..8.py`]: Keyboard_Process `FUN_0055DB10` (MainLoop.CPP; callers 0x55CFF0/0x55DD90/0x647DE0/0x648180) looks the key up in the hotkey table and runs the command's Execute with **no pending-placement check**; Esc is `[Hotkey] Options=27` -> exec `FUN_00537000` -> `FUN_00646AE0` (queue the options event), and an unbound Esc falls back to the same `FUN_00646AE0`. The one-mode cancel chain `FUN_004AAC40` is the DisplayClass right-mouse-up virtual, called only from the tactical WndProc `FUN_00692AB0` on WM_RBUTTONUP without a right-drag (and from vtable slots). The old expectation came from repair-sell.md §C1 calling it "the ESC / right-click handler", corrected there. |  |  |  |  |
| shell-smoke L98-L101 | Abort Mission asks for confirmation (`confirm_open`, reserved probe key) | FAIL (straight to main menu) | FAIL | G-14 | F |
| shell-smoke L106-L109 | Exit Game asks for confirmation | FAIL (process would quit, hard stop) | FAIL | G-14 | F |
| controls (all other steps: shell Esc, select / left-move / right-deselect, building select, Ctrl+1/1 groups, S stop, Q/W/E/R tabs, K/L modes, right-click cancel, queue hold / resume / abandon) | B's selftest behaviour | n/a | PASS | G-01, G-04, G-05, G-07, G-10 | B |

### world-depth — G16 seeds

| Step | Expected | Actual @63edea6 | Actual @7778b66 | G-ID | Owner |
|---|---|---|---|---|---|
| L20 | a selected Rhino left-clicked onto an enemy GI kills it within 300 f (crush on master after C's G-47; shot otherwise) | n/a | FAIL — the GI survives 300 f, neither crushed nor shot; not yet triaged (crush is on C's unmerged branch; the Rhino not firing may be G-06/G-41) | G-47 | C |

## New findings (not in GAP-ANALYSIS.md)

| ID | Finding | Evidence | Owner |
|---|---|---|---|
| NEW-1 | A skirmish opens with the camera on the map centre, not on the player's start: no own object is on screen at frame 1 (`sk-a` L27, `sk-s` L24, `probe-game` start.png shows only shroud and ore). | `own_on_screen == 0` | F (skirmish start) |
| NEW-2 | ALL01 with no player input: every house-0 unit is dead by f≈2459 but the mission stays RUNNING (no LOST, no result screen). Needs RE of the map's `Lose 13` chain / house-defeat path before fixing. | `m-all01` L44 | F (campaign) |
| NEW-3 | Soviet War Miner never unloads: full at 40 bales, `ReturnToRef` forever, not moving; the Allied Chrono Miner path works. | `sk-s` L68, `probe-soviet-econ.play` dumps | A (G-20) |
| NEW-4 | The Allied AI slot neither builds GA\* power nor trains in 3000 frames, while the Soviet AI does train (sk-a L116 PASS). | `sk-s` L88-L94 | D (G-64) |
