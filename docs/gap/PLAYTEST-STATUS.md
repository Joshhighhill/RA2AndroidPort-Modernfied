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

**Current expectation: all twelve scripts PASS on master 30dfe00 + final-fix**
(see "Final verification" below). Historical: the expectation was b049b40
(master with A, B, C and E merged; D and F pending). The per-script tables below show 63edea6 and 7778b66; the deltas
at b049b40 are listed right after this table.

| Script | 63edea6 | 7778b66 (A+B) | b049b40 (A+B+C+E) | play_hash @b049b40 |
|---|---|---|---|---|
| `sk-a` | FAIL, 28 | FAIL, 11 | FAIL, 8 (gD-ai: FAIL, 7 -- no D row left) | `07b084097ddfe20c` |
| `sk-s` | FAIL, 52 | FAIL, 14 | FAIL, 13 (gD-ai: FAIL, 10 -- no D row left) | `a77d77282d5ecc4b` |
| `sk-end` | FAIL, 2 | FAIL, 2 | FAIL, 2 | `bef7d78b1a86dcd5` |
| `sk-a-lose` | FAIL, 4 | FAIL, 4 | FAIL, 4 (gD-ai: FAIL, 2 -- only G-02:F) | `3c90c0e249d1179c` |
| `m-all01` | FAIL, 5 | FAIL, 2 | FAIL, 2 | `42e50f02da8522ab` |
| `m-sov01` | FAIL, 6 | FAIL, 6 | FAIL, 6 | `be47fff77fd9072e` |
| `shell-smoke` | FAIL, 14 (+hard stop) | FAIL, 4 (+hard stop) | FAIL, 4 (+hard stop) | `428731525d358c3e` |
| `controls` | n/a | FAIL, 2 | FAIL, 2 | `1c6c1af2303f484e` |
| `world-depth` | n/a | FAIL, 1 | **PASS** (crush only) | `12d4c50213bae79a` (expanded script on gG2-depth: PASS `3af75244aa4d547e`) |
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

### gF-mcv-endgame over master 354bdd0 (A, A2, B, C, D, E + F)

Every playable-core script, run one at a time on the gF branch tip after
merging 354bdd0. **All eight PASS.**

| Script | Result | play_hash |
|---|---|---|
| `sk-a` | PASS | `3b6e89700a245a8e` |
| `sk-s` | PASS | `16646d40ae6a7c0e` |
| `sk-end` | PASS | `58405a4c35ce5b92` |
| `sk-a-lose` | PASS (the AI kills the passive player's start force/MCV; LOST -> result) | `b6ad9d31bacd502a` |
| `m-all01` | PASS | `a329b0e3e0fe4399` |
| `m-sov01` | PASS | `660ff6e0587afc40` |
| `m-lose` (new, NEW-2) | PASS | `3935514c32dbd9be` |
| `shell-smoke` | PASS | `da65d96ad04737f4` |

F rows closed: NEW-1 (camera on the player's MCV), NEW-2 (ALL01 ends LOST via
its own trigger chain), G-02 (skirmish WON/LOST -> BorrowedTime -> result ->
menu), G-14 (Exit / Abort confirms: `confirm_open`, `confirm:yes|no`), G-15
(TechLevel cap in campaign and skirmish), G-23 (MCV start + deploy).

Script edits made for the MCV start (the skirmish no longer ships a base):
`shell-smoke` unfolds the AMCV before its placement steps; `sk-s` checks the
structure cameos after the SMCV unfolds (no ConYard = no structure strip);
`sk-a` L47 waits for the stopped GI to finish the cell it was entering (Stop
semantics in unit_orders.cpp) instead of asserting after exactly 2 frames;
`sk-end` waits for `scene == result` after the LOST flag (BorrowedTime).

### Final verification: master 30dfe00 + branch final-fix (current expectation)

Every script run one at a time on final-fix (master 30dfe00, the rebuilt
Skirmish page 0x102 merged, `--play` settings isolated via `RA2_PREF_DIR`).
**All twelve PASS.**

| Script | Result | play_hash |
|---|---|---|
| `sk-a` | PASS | `656b22a85318f512` |
| `sk-s` | PASS | `fe1cf263ad56f1c5` |
| `sk-end` | PASS | `1709fc69a3add31b` |
| `sk-a-lose` | PASS | `168493b5406e7591` |
| `sk-lobby` (new, Skirmish page end to end) | PASS | `4daf570e159f13e0` |
| `m-all01` | PASS | `85c095753cd8d71e` |
| `m-sov01` | PASS | `93563d7d332437e7` |
| `m-lose` | PASS | `7fdb2d4108e1e6a0` |
| `shell-smoke` | PASS | `648511870a18d992` |
| `controls` | PASS | `53397d2c6b362375` |
| `world-depth` | PASS (script fixed on final-fix, below) | `c8815382309441ed` |
| `probe-soviet-econ` | PASS (script fixed on final-fix, below) | `5e33a902e876a58a` |

Script fixes on final-fix (assumptions broken by later merges, no engine
regression):

* `probe-soviet-econ` failed 1 step on master 25bd5b0: L23
  `assert cameo(NAPOWR) == 1` at frame 6, BEFORE the SMCV deploys. The probe
  was written when the skirmish shipped a pre-built ConYard; since gF (98d5521,
  G-23) the player starts with an SMCV and no ConYard, so no structure strip
  exists yet. Same fix `sk-s` got in d575300: the three cameo asserts now run
  after `built(0,NACNST) == 1`. Lobby steps migrated to the 0x102 page (copied
  from `sk-s`). Income still verified: credits 8200 -> 9200 at f=3000 (War
  Miner unload, NEW-3 stays fixed).
* `world-depth` Part B used the old lobby's `cycle map:+`; the rebuilt page
  picks the map on Customize Battle (0x6B): `click button "Customize Battle"`,
  3x `list:down`, `click map:xmp31s2.map`, `Use Map`.

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
| L106 | AMCV in the vehicle tab (rebuildable MCV) | FAIL | FAIL -> **PASS @gA2** (script now builds GADEPT first: `[AMCV] Prerequisite=GAWEAP,GADEPT`) | G-22 / G-23 | A / F |
| L109-L110 | train a 2nd Chrono Miner | FAIL | PASS | G-22 | A |
| L116 | AI trains a unit within 3000 f | FAIL | PASS | G-60 | D |
| L117 | AI damages the player within 6000 f more | FAIL | FAIL — **PASS on gD-ai** (G-60/G-61: teams + real script table + move-progress backstop) | G-60 | D |
| L125-L126 | Prism Tank (SREF) has damage and range | FAIL (0 / 0) | FAIL | G-41 | C |
| L139 | Pillbox kills 3 spawned Conscripts within 450 f | FAIL (1 died) | FAIL | G-40 | C |
| L145, L147 | after the enemy is gone (`world_outcome == WON` PASSES), the session ends WON | FAIL (stays RUNNING) | FAIL | G-02 | F |

### sk-s — Soviet vs 1 Allied AI

| Step | Expected | Actual @63edea6 | Actual @7778b66 | G-ID | Owner |
|---|---|---|---|---|---|
| L14 | Yuri not in the lobby side cycle | FAIL | FAIL -> **PASS @gA2** (side toggles Allied/Soviet) | G-03 | A |
| L23 | `credits == 10000` | FAIL 20000 | PASS | G-21 | A |
| L24 | view opens on the player's start | FAIL | FAIL | NEW-1 | F |
| L25, L34-L36 | SMCV start + deploy | FAIL | FAIL | G-23 | F |
| L26-L29 | Soviet infantry/sidebar (`cameo(NAPOWR)`, no `GAPOWR`) | FAIL (Allied sidebar) | PASS | G-03 | A |
| L30, L100 | Tesla Coil offered in the defence tab | FAIL | FAIL -> **PASS @gA2**: `[TESLA] Prerequisite=POWER,RADAR,NACNST` -- the script now builds NARADR first and expects no Tesla Coil at the start (the original does not offer it without a radar) | G-26 | A |
| L31 | no SW at start | FAIL | PASS | G-25 | A |
| L41-L56 | NAPOWR -> NAHAND -> Conscript | FAIL (no cameos) | PASS | G-03 | A |
| L60-L67 | NAREFN -> War Miner harvesting | FAIL | PASS | G-03 / G-20 | A |
| L68 | credits rise within 2700 f | FAIL | FAIL — the War Miner fills to 40 bales and stays in `ReturnToRef` forever, not moving (`probe-soviet-econ.play`) -> **PASS @gA2** (NEW-3 fixed: blocked dock cell -> QueueingCell approach) | G-20 | A |
| L73-L84 | NAWEAP -> Rhino; Flak Track + War Miner offered | FAIL | PASS | G-03 / G-22 | A |
| L82 | SMCV offered | FAIL | FAIL -> **PASS @gA2** (after NADEPT: `[SMCV] Prerequisite=NAWEAP,NADEPT`) | G-22 / G-23 | A / F |
| L88-L90 | the Allied AI builds GA\* (GAPOWR), no NA\* | FAIL | FAIL (no GAPOWR in 3000 f) — **PASS on gD-ai** (side-correct catalog + side-consistent country, NEW-4) | G-64 | D |
| L94 | the (Allied) AI trains a unit | FAIL | FAIL (the Soviet AI does, see sk-a L116) — **PASS on gD-ai** | G-60 / G-64 | D |
| L95 | AI damages the player | FAIL | FAIL — **PASS on gD-ai** | G-60 | D |
| L109 | Sentry Gun kills 2 spawned GIs within 450 f | n/a (step was Tesla-based) | FAIL (1 died) | G-40 | C |
| L115, L117 | session ends WON | FAIL | FAIL | G-02 | F |

### sk-end, sk-a-lose — the end of a skirmish

| Script / step | Expected | Actual @63edea6 | Actual @7778b66 | G-ID | Owner |
|---|---|---|---|---|---|
| sk-end L12 | `cheat kill_house 0` -> `world_outcome == LOST` | PASS | PASS | — | — |
| sk-end L13 | session ends LOST (result scene) within 900 f | FAIL (RUNNING) | FAIL | G-02 | F |
| sk-end L16 | result -> main menu | FAIL | FAIL | G-02 | F |
| sk-a-lose L19 | hardest AI kills a player unit within 9000 f, no input | FAIL | FAIL — **PASS on gD-ai** (f=5150; G-63 binds lobby HARD to [Easy] = house+0x184 0) | G-60 (G-63) | D |
| sk-a-lose L20 | player defeated within 18000 f more | FAIL | FAIL — **PASS on gD-ai** (defeated at f=5150) | G-60 | D |
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

### world-depth — G16 (gG2-depth: G-48 garrisons, G-49 bridges)

Expanded script (Part A default map, Part B May Day; both parts now deploy
the starting MCV first, as F's skirmish start requires). On gG2-depth (master
98d5521 + G16, save v7): **PASS**, 0 failures, `play_hash=0b994340ca1d27be`
(two runs equal); `sk-a` and `controls` also PASS on that tree. On master without G16 every @G-48/@G-49 step fails (no garrison
order, no bridge state, bridge cells are water, no ground force-fire).

| Step | Expected | Actual @gG2-depth | G-ID | Owner |
|---|---|---|---|---|
| L30 | a Rhino ordered onto an enemy GI kills it (crush) | PASS | G-47 | C |
| L39-L43 | 3 GIs clicked onto the nearest neutral civilian building enter it; it turns house 0, 3 occupants | PASS | G-48 | G |
| L52-L53 | 2 Conscripts spawned 4 cells away: shots fired out of the garrison, both die | PASS | G-48 | G |
| L57-L63 | select the garrison, `D`: occupants walk out, the building returns to the neutral house | PASS | G-48 | G |
| L86-L95 | May Day ships 48 destroyed low-bridge cells; an engineer enters the bridge hut, the span is repaired (48 -> 12), the engineer is consumed | PASS | G-49 | G |
| L101-L106 | a Grizzly drives from the near bank over the bridge (`on_bridge(0) >= 1`) to the far bank | PASS | G-49 | G |
| L118-L122 | back on the near bank, Ctrl+click force-fire on a middle section (`point bridge_span`): it collapses (+3 destroyed cells) | PASS | G-49 | G |
| L118-L124 | a second engineer at the hut repairs it again (15 -> 12) | PASS | G-49 | G |

## New findings (not in GAP-ANALYSIS.md)

| ID | Finding | Evidence | Owner |
|---|---|---|---|
| NEW-1 | A skirmish opens with the camera on the map centre, not on the player's start: no own object is on screen at frame 1 (`sk-a` L27, `sk-s` L24, `probe-game` start.png shows only shroud and ore). | `own_on_screen == 0` | F (skirmish start) |
| NEW-2 | ALL01 with no player input: every house-0 unit is dead by f≈2459 but the mission stays RUNNING (no LOST, no result screen). Needs RE of the map's `Lose 13` chain / house-defeat path before fixing. | `m-all01` L44 | F (campaign) |
| NEW-3 | **FIXED @gA2** -- Soviet War Miner never unloads: full at 40 bales, `ReturnToRef` forever, not moving; the Allied Chrono Miner path works. | `sk-s` L68, `probe-soviet-econ.play` dumps | A (G-20) |
| NEW-4 | The Allied AI slot neither builds GA\* power nor trains in 3000 frames, while the Soviet AI does train (sk-a L116 PASS). **Fixed on gD-ai:** the lobby cycled the slot to Allied but left country=Russians, so the Owner-filtered catalog picked NACNST as the plan head while the house stood on a GACNST; the AI country is now checked against rulesmd `[Sides]`. | `sk-s` L88-L94 | D (G-64) |

## Wave-2 UI fixes (branch `w2fix-ui`, W2-UI-01..30)

These results come from running every script on its own on the `w2fix-ui` tip
(`run-all.ps1`, output in `build/play-final/`). The UI fixes changed some
script targets:

- `shell-smoke` now checks `banner == "Mission Saved"`. This is the
  `TXT_GAME_WAS_SAVED` message-list line.
- `m-all01`, `m-sov01` and `m-lose` go through the new Campaign page 0x94:
  `require scene == "shell:Campaign"`, then `click well:allied` or `well:soviet`,
  then `click button Load`.
- `button:Back` still resolves on pages whose bottom button is now labelled
  "Main Menu".

| Script | Result | play_hash |
|---|---|---|
| `controls` | PASS | `1546660df2b00b6f` |
| `m-all01` | PASS | `f1bfd06283ccfeb7` |
| `m-lose` | PASS | `e8c89e9e605251d6` |
| `m-sov01` | PASS | `c3538a86f04ff4cb` |
| `shell-smoke` | PASS | `787f0c96de7028bb` |
| `sk-a` | PASS | `d00d60f6ac308736` |
| `sk-a-lose` | PASS | `0c9c954868feb215` |
| `sk-end` | PASS | `c7e7cd2aa9217021` |
| `sk-lobby` | PASS | `4daf570e159f13e0` |
| `sk-s` | PASS | `5924a716722272a7` |
| `world-depth` | PASS | `5895f5a6f9b77bb6` |
| `probe-game` / `probe-garrison` / `probe-perf` / `probe-shell` / `probe-soviet-econ` | PASS | — |
| `probe-bridge` | FAIL, 2. It uses the `map:+` lobby target, which the Skirmish page removed. The same failure happens on master `dfff02a`. | — |
| `probe-flow` | FAIL, 7. It expects a ConYard at the start, but the skirmish now starts with an MCV. The same failure happens on master `dfff02a`. | — |
| **new** `ui-hud` | PASS | `74645b1b4832590f` |
| **new** `ui-pages` | PASS | `201f6d614d0b9b6e` |
| **new** `ui-campaign` | PASS | `841913f27b3ae367` |
| **new** `ui-result` | PASS | `7dac2ea048d278bb` |
| **new** `ui-shell` | PASS | `e948779eb0ab3bb3` |

## Wave-2 graphics/audio/perf fixes (branch `w2fix-gfx`, W2-GA)

These results come from running every script on its own on the `w2fix-gfx` tip,
after merging master b532586 (`run-all.ps1`, output in `build/play-final/`).
The play hashes move for two reasons. First, FogOfWar is now off by default
(W2-GA-03): AI and units auto-acquire targets in any explored cell, as the
original does (RE-Q3). Second, the omitted cells on hillbtwn are now ground
(W2-GA-04), which changes passability on the default map.

Script fix: in `sk-a`, step L135 used to require two **live** Chrono Miners.
With fog off, the AI kills the refinery's miner before the player's second one
is trained (`units(0,CMIN)=0` when the player clicks the cameo,
`units_lost(0)=3`). The step now checks that one more miner is trained.

| Script | Result | play_hash |
|---|---|---|
| `controls` | PASS | `1a363502bda5c1bb` |
| `m-all01` | PASS | `f1bfd06283ccfeb7` |
| `m-lose` | PASS | `e8c89e9e605251d6` |
| `m-sov01` | PASS | `c3538a86f04ff4cb` |
| `shell-smoke` | PASS | `787f0c96de7028bb` |
| `sk-a` | PASS (after the script fix) | `31a3a539fb57a9ca` |
| `sk-a-lose` | PASS | `ebc8ebf2737ab4cf` |
| `sk-end` | PASS | `c7e7cd2aa9217021` |
| `sk-lobby` | PASS | `4daf570e159f13e0` |
| `sk-s` | PASS | `85dc337d5efa7e77` |
| `world-depth` | PASS | `3fa081b33435d91d` |
| `probe-game` / `probe-garrison` / `probe-perf` / `probe-shell` / `probe-soviet-econ` | PASS | — |
| `ui-hud` / `ui-pages` / `ui-campaign` / `ui-result` / `ui-shell` | PASS | unchanged from w2fix-ui |
| **new** `gfx-vis` (base, GGI, Terror Drone, war-factory door, Patriot turret, shadows, lighting) | PASS | `7e202ab530ebb43f` |
| **new** `gfx-fx` (Tesla bolt, Prism beam, craters) | PASS | `41b1d11e0e6bc843` |

### Wave-2 W2-AN (aircraft, naval, superweapons) -- branch `w2fix-air` over master 844033b

Every script run one at a time on the merged tree; all PASS. The three new
scripts were run twice with identical `play_hash`.

| Script | Result | play_hash |
|---|---|---|
| `naval` (new) | PASS | `de56fa937aeaf935` |
| `air` (new) | PASS | `cc0f701752a6f8e8` |
| `sw` (new) | PASS | `6ffc7e946bd18889` |
| `sk-a` | PASS | `7525b71fc44fcc7f` |
| `sk-s` | PASS | `a099e461333761ff` |
| `sk-end` | PASS | `95a67a058e3882b9` |
| `sk-a-lose` | PASS | `47af64760366a980` |
| `sk-lobby` | PASS | `4daf570e159f13e0` |
| `m-all01` | PASS | `844c6dd06a8177ae` |
| `m-sov01` | PASS | `1ed80b2eabf7f2cb` |
| `m-lose` | PASS | `ccc8340943cd2baa` |
| `shell-smoke` | PASS | `520e202cf28c6070` |
| `controls` | PASS | `299536cc427c8525` |
| `world-depth` | PASS | `c30243df544b3c2a` |
| `ui-hud` / `ui-pages` / `ui-campaign` / `ui-result` / `ui-shell` | PASS | `6ab1e68d829d928d` / `d9b913b15d4fd971` / `93692c6ee26595eb` / `cf809b78de945e17` / `e948779eb0ab3bb3` |

W2-AN rows closed by play evidence: W2-AN-01/02/07/08 (`air`), W2-AN-04/05/06/17/23
(`sw`), W2-AN-09/10/11/12/13/15/25 (`naval`); W2-AN-22 by `--probe-realplay`
(0 fails); W2-AN-16/21 by `--ai-soak --map xbayopigs.map --cash 50000`.

### Wave-2 W2-AR (Android, robustness, release) -- branch `w2fix-android` over master 0fc3444

Each script was run on its own on the merged tree (`build/play-final/`).
**Every `play_hash` matches the W2-AN table above.** Under `--play` the canvas
is still 1280x720 and the deferred scene build happens in the same frame, so
the game state is byte-identical.

| Script | Result | play_hash |
|---|---|---|
| `controls` / `m-all01` / `m-lose` / `m-sov01` / `shell-smoke` | PASS | `299536cc427c8525` / `844c6dd06a8177ae` / `ccc8340943cd2baa` / `1ed80b2eabf7f2cb` / `520e202cf28c6070` |
| `sk-a` / `sk-a-lose` / `sk-end` / `sk-lobby` / `sk-s` / `world-depth` | PASS | `7525b71fc44fcc7f` / `47af64760366a980` / `95a67a058e3882b9` / `4daf570e159f13e0` / `a099e461333761ff` / `c30243df544b3c2a` |
| `ui-campaign` / `ui-hud` / `ui-pages` / `ui-result` / `ui-shell` | PASS | `93692c6ee26595eb` / `6ab1e68d829d928d` / `d9b913b15d4fd971` / `cf809b78de945e17` / `e948779eb0ab3bb3` |
| `air` / `naval` / `sw` | PASS | `cc0f701752a6f8e8` / `de56fa937aeaf935` / `6ffc7e946bd18889` |
| `gfx-fx` / `gfx-vis` | PASS | `21e5218607633dda` / `c7a9ed8d8b98498b` |
| `probe-game` / `-garrison` / `-perf` / `-shell` / `-soviet-econ` | PASS | — |
| `probe-bridge` / `probe-flow` | FAIL 2 / FAIL 7 | Pre-existing, same as on master. |
| **new** `robust-missing` (W2-AR-05; run with `RA2_INSTALL_DIR` set to an install without `Maps01.mix`) | PASS | `14650fb0739d0383` |

Gates: `--demo-saveload` `8291b85b9957b34a`, `--demo-skirmish`
`fde3f587e747f765` and `--demo-campaign` `760c46ae2eed28b4` all match the
master pins (`--demo-combat` deterministic=yes). The full suite passes:
788/788.
### Wave-2 W2-CA (campaign) -- branch `w2fix-campaign` over master 256908d

Every script run one at a time (Release build of the merged tree). All campaign
and regression scripts PASS; `probe-bridge` and `probe-flow` fail identically on
the master-equivalent w2fix-air build (pre-existing: lobby `map:+` target and
`building 0:GACNST`), so they are not regressions. Hash moves on the four
campaign-entry scripts are expected: the campaign GameScene now has the
rules-driven roster/map rules overlay (W2-CA-03/04), map AI teams (07), mission
audio (12), aircraft (16) and the other W2-CA mission logic.

| Script | Result | play_hash |
|---|---|---|
| **new** `m-all02` (05 side fallback + 01 EnteredBy: chapel capture -> WON) | PASS | `5217e30168a1ec3f` |
| **new** `m-all04` (08 sea landing: AMCV lands + deploys) | PASS | `e245c8b8db089648` |
| **new** `m-all08` (09 team release: survives f=900, -> WON) | PASS | `b1d09e489d92deeb` |
| **new** `m-sov03` (01/04/13: lab capture -> beacon -> defend -> WON) | PASS | `0f4ead58401a8206` |
| **new** `m-sov05` (02 PlayerControl troopers -> tower -> WON) | PASS | `5337d0b1c362454b` |
| **new** `m-sov06` (06 events 55/56 -> WON) | PASS | `b67641e8d840439a` |
| **new** `m-sov09` (15 mind control of the President -> WON) | PASS | `48d961d52f7c6fac` |
| `m-all01` | PASS | `c662c9a2d7a7dc48` (was `844c6dd06a8177ae`) |
| `m-sov01` | PASS | `7f683e9ff7a6c716` (was `1ed80b2eabf7f2cb`) |
| `m-lose` | PASS | `63207b943877f614` (was `ccc8340943cd2baa`) |
| `ui-campaign` | PASS | `9c3bfe42d1e3fd7f` (was `93692c6ee26595eb`) |
| `air` / `naval` / `sw` | PASS | `cc0f701752a6f8e8` / `de56fa937aeaf935` / `6ffc7e946bd18889` (unchanged) |
| `sk-a` / `sk-s` / `sk-end` / `sk-a-lose` / `sk-lobby` | PASS | unchanged (`7525b71fc44fcc7f` / `a099e461333761ff` / `95a67a058e3882b9` / `47af64760366a980` / `4daf570e159f13e0`) |
| `shell-smoke` / `controls` / `world-depth` | PASS | unchanged |
| `ui-hud` / `ui-pages` / `ui-result` / `ui-shell` / `gfx-vis` / `gfx-fx` | PASS | — |
| `probe-game` / `probe-garrison` / `probe-perf` / `probe-shell` / `probe-soviet-econ` | PASS | — |
| `probe-bridge` / `probe-flow` | FAIL (pre-existing, same on master) | — |

Full suite (Debug, after merging master 256908d): 792 test cases / 350313 assertions, 0 failed. All 33 scripts re-run on the final merged tree: same results and hashes (gfx-vis `c7a9ed8d8b98498b` moves with the w2fix-android merge). Gates:
`--demo-campaign` re-pinned `5d033b6aa019beaa`, `--demo-saveload`
`c19113168b76bae7` (save v12), `--demo-triggers` unchanged `b8cd267e98f5e002`
(causes in README).
