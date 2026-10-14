# W9-MP-03: multiplayer menus (desktop/mouse) vs the original RA2/YR shell

Auditor: w9mp-03. Clone: D:\ra2-android master f188125, build\ra2engine.exe. Original: D:\Red Alert 2\gamemd.exe 1.000, ra2md.csf, rulesmd.ini.

## Summary

**The good part.** The clone's multiplayer shell works end to end:
- Network (LAN) host/join;
- Internet (relay);
- a lobby with slot kinds, country, colour, team and start;
- options that update live on the client;
- Ready that resets when the host changes an option;
- a working kick, with "host closed" and "removed" dialogs;
- lobby chat and in-game chat;
- leaving a match;
- a result screen.

All of this was confirmed by three two-process LAN runs taken under the network lock.

**The gap.** Measured against the original YR shell dialogs (RT_DIALOG resources from gamemd.exe), the CSF strings and the rules data, the menus are a reduced, partly re-labelled copy. The faults a player notices are:

1. **Overlapping text in the lobby.** The clone-only "AI takes over" and "Cross-platform (exp.)" check-boxes are drawn on top of the "Game Speed" and "Credits" labels, on every lobby screen, for host and guest alike.
2. **Guests cannot read the option values.** The Game Speed, Credits and Unit Count readouts are blank on the joiner's screen.
3. **Wrong names in diplomacy and on the score screen.** Both show country names ("Korea", "Libya"), not the players' names. The joiner's Diplomacy page also marks the wrong row as "self": its own row shows ally/chat check-boxes, and the host's row shows none.
4. **Blank map preview on the second lobby.** Every lobby after the first in a session shows an empty map preview (a texture-cache bug).
5. **No game modes.** "Customize Battle" has no Game Type list (Battle / Free For All / Unholy Alliance / Megawealth / Duel / Meat Grind / Naval War / Team Game / Cooperative) and no Random Map button.
6. **Missing lobby controls.** The lobby has:
   - no column headers;
   - no national flags;
   - no Build Off Ally ConYards check-box;
   - no STT tooltips.
   The Ready button says "Ready" where the original says "Accept".
7. **Picking a smaller map kicks players.** When the host picks a map with fewer slots, human players in the higher slots are kicked without warning. The original warns with TXT_SCENARIO_TOO_SMALL.
8. **Dead in-game menu buttons.** The in-game multiplayer options menu shows Load, Save and Delete; clicking them only says "Not available in a network game." The original multiplayer menu 0xBBA has only Game Controls, Abort Mission and Resume Mission.
9. **Missing in-game multiplayer actions.** None of these exist in a network game: forming alliances (Diplomacy is read-only), choosing chat listeners, beacons, taunts, or the ToggleAlliance hotkey.
10. **Wrong score screen and exit path.** After a network match:
    - the screen title says "Skirmish Score";
    - the player who aborts gets no score screen;
    - Continue goes to the Main Menu, not back to the Network page.
11. **The LAN page is a game browser, not the original LAN Lobby.** The original's Lobby page (0xBB) has a Players list, a Games list, a lobby chat box and New / Join / Main Menu buttons. The clone's page has no chat and no player list before a game is hosted.
12. **No observers.** YR's Observer support is absent: the Observe button, the "Open Observer" slot, the Observer country and "Surrender and observe".

There are 27 gaps in total: 0 blockers, 11 major, 10 minor and 6 cosmetic.

## Method

**Original data:**
- RT_DIALOG templates were dumped from gamemd.exe into `D:\ra2-re\w9mp-03-out\dlg_gamemd_all.txt`. The key dialogs are:
  - 0xE2 Main Menu;
  - 0xBB Lobby (the LAN page);
  - 0xBC Host Screen;
  - 0xBD Join Screen;
  - 0x6B Choose Map;
  - 0x73 Diplomacy;
  - 0xBBA multiplayer in-game options;
  - 0xB6 Abort;
  - 0x108 Multiplayer Score;
  - 0xD7 IPX/LAN options.
- ra2md.csf was extracted from langmd.mix and dumped to `D:\ra2-re\w9mp-03-out\csf_all.txt` (5208 labels).
- rulesmd.ini and mpbattlemd.ini were extracted from ra2md.mix. `mpmodesmd.ini` was taken from `D:\ra2-re\out\gapsk_shots\`.

**Clone:**
- I read the code in engine/mp_pages.inc, mp/mp_lobby.cpp, mp/mp_model.*, game_scene_net.inc, ingame_pages.inc, gf_result.inc, gf_scene.inc and skirmish_load.inc.
- I drove the menus with --play scripts in `D:\ra2-re\w9mp-03-out\scripts\`:
  - walk1 and walk2 are single-process runs;
  - pair, pair2 and pair3 are host+join runs, each run under the `w9mp-net.lock` and under 10 minutes; `pair.ps1` is the runner.

## Per-page comparison

| Page | Original (gamemd.exe dialog / CSF) | Clone | Verdict |
|---|---|---|---|
| Main Menu 0xE2 | Single Player, Internet (`GUI:WWOnline`), Network, Movies & Credits, Options, Exit Game | Same 6 buttons, same order (shot 01) | OK |
| Network → **Lobby 0xBB** | Header `GUI:Lobby`. "Your Name" edit, **Players** list, **Games** list, lobby **chat** list + edit. Buttons **New / Join / Main Menu**; Observe is hidden. | Header "Network". Your name edit, "Games on your network" list, Join-by-IP edit, help text. Buttons Host Game / Join Game / Join by IP / Back. **No player list, no lobby chat.** (shot 02) | Gap W9-03-11, 22 |
| Internet | WOL login/chat (not reproducible) | Own server: Server and name edits, Public games list, Room code edit. Connect / Host Game / Host Private / Join Game / Join Code / Back (shots 20-22) | Clone design. Usability gaps W9-03-17, 18 |
| **Host Screen 0xBC** | Column headers Players / Side / Color / Start / Team (0x796, 0x791-0x794). Per row (8 rows), from left to right:<br>- accept picture (0x6D2.., x=42);<br>- player combo;<br>- **flag picture** (0x6DA.., x=240);<br>- country combo;<br>- colour combo;<br>- start combo;<br>- team combo.<br>Check-boxes Short Game, MCV Repacks, Crates Appear, Superweapons, **Build Off Ally ConYards**. Sliders Game Speed / Credits / Unit Count. Chat list + edit. Map preview + game-type label (0x6EC) + map name (0x5A8). Buttons Start Game (row 1) / Customize Battle (row 2) / Back. | Rows: kind/name, country, colour, team, start, then the ready box at the far right. **No headers, no flags.** Check-boxes: Short Game, MCV Repacks, Crates, Superweapons, plus clone-only "AI takes over" and "Cross-platform (exp.)", **which overlap the slider labels**. No Build Off Ally. Sliders OK. Chat OK. Preview + LAN address + map name, no game-type label. Header "Network Game". (shots 03, 30) | Gaps W9-03-01, 05, 06, 07, 08, 12, 19 |
| **Join Screen 0xBD** | Same layout. Options are visible but WS_DISABLED and keep showing their values. Button **Accept** (`GUI:Accept`) on row 1. STT tooltips: `STT:GuestSlider*` ("Only the Game Host can ..."). | Options greyed; **slider readouts blank**. Button "Ready" / "Not Ready". No tooltips. Empty status line. (shots 50-53) | Gaps W9-03-02, 09, 12 |
| Combos | Country / Color / Start / Team drop-downs; taken colours are refused (`TXT_COLOR_IN_USE`) | The colour list hides taken colours, the start list hides taken starts, team is "-", A-D (shots 05-09, 51) | OK. W9-03-24 is minor |
| **Choose Map 0x6B** | `GUI:SelectEngagement`. **Game Type** list (0x6EB) + **Game Map** list (0x553). Use Map / **Random Map** (`GUI:CreateRandomMap`) / Cancel. Maps are filtered by mpmodesmd.ini mapFilter. | One list (standard maps only) with "(2-8)" player counts, Use Map / Cancel. (shots 10, 11) | Gaps W9-03-04, 13 |
| Kick | Host Options / Kick (WOL); LAN: set the slot to Closed | Click the guest's name → "Remove Joiner from the game?" OK/Cancel. The guest sees "You were removed from the game by the host." (shots 90, 95) | OK |
| Host leaves | `TXT_CONNECTION_LOST` / left-game dialogs | "The host closed the game." → OK → back to the Network page (shot 97) | OK |
| Loading | Multiplayer progress screen (per-player names) [SUSPECTED] | Local country art; the list reads "Player - Libya" / "Computer - ..."; no handles; in game, "Network game: waiting for every player to finish loading..." (shot 71) | Gap W9-03-20 |
| In-game options **0xBBA** | Game Controls / Abort Mission / Resume Mission only | The single-player menu: Game Controls, **Load Game, Save Game, Delete Game**, Abort Mission, Resume Mission (shots 38, 39) | Gap W9-03-10 |
| Abort 0xB6 | Abort + Observe (`STT:ConfirmExitButtonObserve` "Surrender and observe the battle.") | Abort / Resume Mission (shot 40) | Gap W9-03-21 |
| **Diplomacy 0x73** | Player / Allies (offer alliance; Battle mode `AlliesAllowed=yes`, `AllyChangeAllowed=yes`) / Chat (listeners, `STT:DiplomacyCBoxListeners`) / Kills | Read-only; **names are countries**; the self row is hard-coded to house 0 (shots 35, 80) | Gaps W9-03-03, 14 |
| In-game chat | Enter → "To All:" (`TXT_TO_ALL`) line; F5-F12 taunts; B beacons; A ToggleAlliance | Enter → "Say: ..." box with Send, plus an on-screen "Chat" button; works across peers (shots 36, 37, 39). Beacons, taunts and alliance do nothing. | Gaps W9-03-15, 27 |
| **Score 0x108** | Header `GUI:MultiplayerScore`. Game number + time. Player / Kills / Losses / Built / Score; player handles. Continue → back to the multiplayer lobby [SUSPECTED for the destination]. | "Skirmish Score"; country names; no game number. The player who aborted gets no score screen (goes straight to the Main Menu). The winner's Continue → Main Menu. (shots 82, 73, 83) | Gap W9-03-16 |
| IPX/LAN options 0xD7 | Network card, socket, firewall port / send delay | None (UDP ports in RA2.INI [Net]) | Not needed |

## Gaps

### W9-03-01: overlapping text, "AI takes over" and "Cross-platform" over "Game Speed" and "Credits"
- **Severity / evidence:** major (seen on every lobby, host and guest). [CONFIRMED-PLAY] shots 03, 30, 50, 53.
- **Original vs clone:** the original 0xBC has no such check-boxes. Its Game Speed / Credits / Unit Count labels are at x 301, with the sliders at 403 (800-wide template). In the clone:
  - `check_rect(CK_AIT/CK_XP)` = {170, 232/252, 125, 16} (mp_pages.inc:591-594);
  - `slider_label()` = {190, 230/256, 105, 16} (mp_pages.inc:596).
  The two boxes share the same area, so the text renders on top of each other.
- **Root cause:** the two clone-only options were added into the slider-label column.
- **Fix:** move them under Superweapons (x=30, y=312/332, the original's free rows), or onto a separate "Advanced" line above the chat box. Never place them in the 190-300 column.
- **Test:** a layout unit test that asserts no check / label / slider rects intersect, for host and guest.

### W9-03-02: guests cannot see the Speed, Credits and Unit Count values
- **Severity / evidence:** major. [CONFIRMED-PLAY] shot 53 (the readout wells are empty on the joiner).
- **Original vs clone:** the original 0xBD greys the controls but still shows their values. `STT:GuestSliderCredits` says "Only the Game Host can control the starting cash." The clone calls `p.trackbar(..., slider_text(s), host_ != nullptr && joined())` (mp_pages.inc:1037). In the disabled state `label(..., en=false)` (shell_widgets.inc:113) is drawn too dark to read.
- **Fix:** always draw the readout at full brightness; grey only the thumb.
- **Test:** add `slider_text(speed|credits|units)` probe keys on the client and assert they are non-empty, plus a shot diff.

### W9-03-03: Diplomacy rows show country names, and the joiner's "self" row is wrong
- **Severity / evidence:** major. [CONFIRMED-PLAY] shots 35 (host: "Korea" / "Germany") and 80 (joiner: "Libya" / "Cuba"; the joiner's own row "Cuba" has check-boxes with Allies ticked, while the host's row has none).
- **Original vs clone:** in the original, the 0x73 rows are player names. In the clone:
  - main.cpp:7717 sets `r.self = h == 0` (should be `local_house_`);
  - the names come from gf_stats, which uses the `Name:<country>` CSF (gf_scene.inc:197-203).
- **Fix:** take the names from the session (`MpSessionConfig.players[].name`) when `net_`, and use `r.self = h == local_house_`.
- **Test:** pair2 + probe `diplomacy_name(i)` == "Hosty"/"Joiner", and `diplomacy_self(1)` on the joiner.

### W9-03-04: Customize Battle has no Game Type list and no Random Map
- **Severity / evidence:** major. [CONFIRMED-DATA] the 0x6B template has the 0x6EB `GUI:GameType` and 0x553 `GUI:GameMap` lists plus the 0x583 `GUI:CreateRandomMap` button. mpmodesmd.ini defines Battle, TeamGame, Megawealth, Duel, MeatGrind, NavalWar, FreeForAll, Unholy and Cooperative, each with its own rules override (MPBattleMD.ini, ...) and mapFilter.
- **Clone:** one list over the "standard" catalog only (mp_pages.inc:671-682, maplist.hpp:30). Every match is effectively Battle without the MPBattleMD overrides being chosen. Shot 10.
- **Fix:** load mpmodesmd.ini. Show a Game Type list (`GUI:<mode>` labels), filter maps by mode, carry the mode in MpOptions, and apply the mode's rules-override INI at game start. Show the mode in the 0x6EC label.
- **Test:** a lobby probe `game_mode` plus map count per mode, and `rules_override` applied in-game (e.g. Megawealth with no ore).

### W9-03-05: Build Off Ally ConYards check-box missing
- **Severity / evidence:** minor. [CONFIRMED-DATA] 0xBC/0xBD control 0x69D `GUI:BuildOffAlly` = "Build Off Ally ConYards".
- **Clone:** `MpOptions::build_off_ally` exists (mp_model.hpp:62), but there is no check-box (mp_pages.inc:410, CK enum). The Skirmish page has one (skirmish_page.inc:107).
- **Fix:** add a CK_BOA check-box at the original position (x 201, y 206 DLU).
- **Test:** `check(buildoffally)` toggles and reaches `session` / options.

### W9-03-06: no column headers (Players / Side / Color / Start / Team)
- **Severity / evidence:** minor. [CONFIRMED-DATA] the 0xBC statics 0x796, 0x791, 0x792, 0x793 and 0x794 (`GUI:Players`, `GUI:Side`, `GUI:Color`, `GUI:StartPosition`="Start", `GUI:Team`).
- **Clone:** the clone's rows start at y=17 with no headers (mp_pages.inc:584, 1003-1031). Columns of "-" and "?" are unexplained (shot 03).
- **Fix:** draw the five CSF headers above the rows. Rows already start at y=17, so they need to shift by about 10 px.
- **Test:** shot comparison, and a probe `header_count`.

### W9-03-07: no national flag per row
- **Severity / evidence:** minor. [CONFIRMED-DATA] 0x6DA-0x6E1 static pictures, `STT:HostPictureFlag` "Player's national flag."
- **Clone:** the Skirmish page draws flags (skirmish_page.inc:800-805); the multiplayer lobby does not (mp_pages.inc:1003-1031).
- **Fix:** reuse the skirmish flag painter at the original x.
- **Test:** shot.

### W9-03-08: the accept indicator is at the wrong place and has no "checked-in" meaning text
- **Severity / evidence:** cosmetic. [CONFIRMED-DATA] the 0x6D2-0x6D9 acceptance pictures sit left of the player combo (x=42). `STT:HostPictureAcceptance` reads "The game can only begin when all players are checked-in."
- **Clone:** a green/red square right of the Start combo (mp_pages.inc:590, 1026-1030).
- **Fix:** move it to the left of the name, and use the original accept art if it is in the mix (otherwise a check mark).
- **Test:** shot.

### W9-03-09: the guest button reads "Ready" / "Not Ready" instead of "Accept"
- **Severity / evidence:** cosmetic. [CONFIRMED-DATA] 0xBD 0x59F `GUI:Accept` = "Accept"; `STT:GuestButtonAccept`.
- **Clone:** hard-coded strings at mp_pages.inc:654.
- **Fix:** use the CSF "Accept" label; when accepted, show it pressed or lit (the original toggles the button state).
- **Test:** `button_enabled(Accept)`.

### W9-03-10: dead Load / Save / Delete buttons in the in-game multiplayer menu
- **Severity / evidence:** major (dead buttons). [CONFIRMED-PLAY] shots 38 and 39 (banner "Not available in a network game.").
- **Original vs clone:** the original multiplayer menu is a different dialog, 0xBBA: Game Controls (row 0) / Abort Mission (row 1) / Resume Mission. The clone uses the single-player list (ingame_pages.inc:661-667) and only refuses the buttons afterwards (main.cpp:7629-7632).
- **Fix:** `InGameMenu::configure(..., net)`, and when `net`, return the 0xBBA button set.
- **Test:** pair run; assert `menu:LOAD GAME` is absent in a net game.

### W9-03-11: the LAN page is a browser, not the original Lobby (no players list, no pre-game chat)
- **Severity / evidence:** major (social / lobby feel). [CONFIRMED-DATA] 0xBB: Players list 0x53E, Games list 0x549, chat list 0x53F + edit 0x540, `STT:LobbyEditInput` "Type your message here and hit enter.", buttons `GUI:New` / `GUI:Join` / `GUI:MainMenu`.
- **Clone:** only the beacon game list (mp_pages.inc:1186-1303; shot 02).
- **Fix:** add a LAN lobby channel. Players announce their presence (name) and lobby chat goes out by broadcast on the 0x1236 lobby channel, as the original LAN lobby does (see docs/gap/net/01-original-protocol.md). Show the Players list and chat; rename the buttons to New / Join / Main Menu. Keep Join by IP as an extra.
- **Test:** a two-process LAN test where both are on the Network page: they see each other in Players and exchange a chat line.

### W9-03-12: no STT tooltips on any multiplayer page
- **Severity / evidence:** minor. [CONFIRMED-DATA] CSF `STT:Host*` (17 strings), `STT:Guest*`, `STT:Lobby*`, `TXT_ONLY_HOST_CAN_MODIFY`.
- **Clone:** the status line only carries start-check text (mp_pages.inc:946-949). A guest clicking an option gets no "Only the host can modify this option." feedback. The Skirmish page already has a `tip()` system (skirmish_page.inc:606-628).
- **Fix:** port the skirmish `tip()` map with the Host/Guest keys; show TXT_ONLY_HOST_CAN_MODIFY on a guest click.
- **Test:** probe `status` after hovering / clicking the crates check-box as a guest.

### W9-03-13: picking a smaller map silently kicks human players
- **Severity / evidence:** major. [CONFIRMED-RE-code] `LobbyHost::set_map` kicks any Human in slots >= max_players (mp/mp_lobby.cpp:150-158).
- **Original vs clone:** the original has `TXT_SCENARIO_TOO_SMALL` "This map has a %d player max. The max includes human and computer players." and does not drop the players. The clone's map list is not filtered by the current player count either (it shows "(2)" maps to a 4-player lobby).
- **Fix:** in Customize Battle, disable or grey maps whose max < `player_count`, or on Use Map show TXT_SCENARIO_TOO_SMALL and refuse. Never kick from `set_map`. `set_map` also re-opens slots the host had Closed (mp_lobby.cpp:159-160); keep Closed slots closed.
- **Test:** test_mp unit test: 3 humans + set_map(2p) → refused, nobody kicked.

### W9-03-14: Diplomacy is read-only; alliances and chat listeners cannot be set
- **Severity / evidence:** major. [CONFIRMED-DATA] mpbattlemd.ini `AlliesAllowed=yes`, rulesmd [MultiplayerDialogSettings] `AllyChangeAllowed=yes`, `STT:DiplomacyCBoxAllies` "Offer to join forces with this player.", `STT:DiplomacyCBoxListeners`, `TXT_HAS_ALLIED`.
- **Clone:** check-boxes drawn disabled (ingame_pages.inc:543-544, "read-only in skirmish") in network games too.
- **Fix:** in a net game, make Allies send the ALLY event (EventClass ALLY) through lockstep and Chat set the per-player chat mask; announce TXT_HAS_ALLIED.
- **Test:** pair: host allies the joiner; `team()` / `are_allied` agree on both machines with no desync.

### W9-03-15: beacons, taunts and the alliance hotkey do nothing in a network game
- **Severity / evidence:** major (the multiplayer-only actions are missing). [CONFIRMED-RE-code] game_scene_controls.inc:214-221: "PlaceBeacon FUN_00536DD0 and ToggleAlliance / PageUser FUN_00537B80 / Taunts act only in a network game ... consumed with no effect". This is consumed in net games too.
- **Original:** the KeyboardMD.ini defaults are PlaceBeacon=66 (B), ToggleAlliance=65 (A), Taunt_1..8=F5..F12; the Taunts\ folder holds TAU*.WAV.
- **Fix:** implement them as networked events when `net_`.
- **Test:** pair: press F5 on the host; the joiner's log shows a taunt play.

### W9-03-16: the multiplayer score screen is the skirmish one; the leaver gets no score; Continue goes to the Main Menu
- **Severity / evidence:** major. [CONFIRMED-PLAY] pair2:
  - shot 82: the header is "Skirmish Score" and the rows are "Libya" / "Cuba";
  - shot 73: the host who aborted lands directly on the Main Menu;
  - shot 83: the winner's Continue → Main Menu.
- **Original vs clone:** the original uses `GUI:MultiplayerScore` with 0x108 (game number 0x6D2 `STT:MPScoreLabelGame` and time 0x3EA), and player handles. The clone always uses `GUI:SkirmishScore` (gf_result.inc:43), `next()` → `make_main_menu_scene` (gf_result.inc:67-74), and country names (gf_scene.inc:197-203).
- **Fix:** when `net_`:
  - use the MultiplayerScore header and the session names;
  - show the score screen to the leaver as well;
  - make Continue return to the Network page (LAN) or the Internet page (online).
  The destination is [SUSPECTED]: the original returns to the lobby page.
- **Test:** pair2 assertions on `score_header`, `score_name(i)` and the scene after Continue.

### W9-03-17: player-facing text refers to a developer document
- **Severity / evidence:** cosmetic. [CONFIRMED-PLAY] shot 20: "Type the address of your server (see docs/SERVER.md) and press Connect." (mp_pages.inc:1320).
- **Fix:** "Type the address of your game server and press Connect."
- **Test:** shot.

### W9-03-18: server failures appear only as a terse lowercase status line
- **Severity / evidence:** minor. [CONFIRMED-PLAY] shot 21: "could not connect", after about 3 s, with no dialog. Shot 22: "Cannot find server no.such.host.invalid".
- **Original:** WOL used message boxes (e.g. TXT_WOL_CONNLOST). The clone uses the status line (mp_pages.inc:1396-1415).
- **Fix:** use a GfConfirm dialog with a full sentence ("Could not reach the server at X. Check the address and that the server is running.").
- **Test:** walk2 assert `confirm_open`.

### W9-03-19: lobby header text differs from the original
- **Severity / evidence:** cosmetic. [CONFIRMED-DATA] the headers are 0xBC `GUI:HostScreen` = "Host Screen", 0xBD `GUI:JoinScreen` = "Join Screen" and 0xBB `GUI:Lobby` = "Lobby".
- **Clone:** "Network Game" / "Internet Game" / "Network" (mp_pages.inc:656-659, 1207).
- **Fix:** use the CSF labels; "Internet" may stay for the relay.
- **Test:** probe `page`.

### W9-03-20: the loading screen does not show the players' names
- **Severity / evidence:** minor. [CONFIRMED-RE-code] skirmish_load.inc:68 pushes "Player - <country>" / "Computer - <country>" instead of the handles. [SUSPECTED] the original multiplayer loading screen also shows each player's progress.
- **Fix:** pass `MpSessionConfig` names into SkirmishLoadArt. The per-player progress needs a load-progress event (protocol work).
- **Test:** probe `load_players`.

### W9-03-21: no Observer mode
- **Severity / evidence:** minor (YR feature). [CONFIRMED-DATA] the 0xBB Observe button 0x6CE (present but hidden). CSF `GUI:OpenObserver`, `Name:Observer`, `STT:ConfirmExitButtonObserve` "Surrender and observe the battle.", `TXT_NO_AI_WITH_OBSERVER`.
- **Clone:** there is no Observer slot kind or country, and no Observe on the abort page (ingame_pages.inc:654-658).
- **Fix:** add an "Open Observer" slot kind and a non-combatant spectator house; add Observe to the abort page in a net game.
- **Test:** pair: the joiner as Observer sees the map, has no house, and the match ends normally.

### W9-03-22: LAN page button labels and help text are clone wording
- **Severity / evidence:** cosmetic. [CONFIRMED-DATA] 0xBB: `GUI:New` "New", `GUI:Join` "Join", `GUI:MainMenu`.
- **Clone:** "Host Game", "Join Game", "Join by IP", "Back" (mp_pages.inc:1203-1206).
- **Fix:** New / Join / Main Menu; keep Join by IP.
- **Test:** probe.

### W9-03-23: blank map preview on every lobby after the first in a session
- **Severity / evidence:** major (the key map information goes missing). [CONFIRMED-PLAY] shot 97 (the rejoined lobby shows a black preview well while the map name is shown).
- **Root cause:** `preview_tex()` returns the shared TextureCache texture via `tc.peek(key)` before the per-scene `preview_size_` is filled (mp_pages.inc:970-971). `draw_body` then reads `preview_size_[file]` = {0, 0}, so the scale is inf and the size is 0 (mp_pages.inc:987-990).
- **Fix:** store the size with the texture, or `SDL_QueryTexture` it, or make the size cache static.
- **Test:** a single-process play: host, Back, host again, then probe `preview_w > 0`.

### W9-03-24: colour / start conflicts are silently reverted
- **Severity / evidence:** minor. [CONFIRMED-RE-code] `LobbyHost::set_slot` keeps the old colour/start on a conflict (mp_lobby.cpp:135-136) and sends no message.
- **Original:** `TXT_COLOR_IN_USE` "The requested color is already taken." The combo already hides taken values, so only a race hits this.
- **Fix:** send a system line to the requester.
- **Test:** test_mp race test.

### W9-03-25: Enter does not answer an OK message box
- **Severity / evidence:** minor. [CONFIRMED-PLAY] walk2: after "Cannot find ..." the dialog stayed open on `key enter` (result.txt L37). GfConfirm handles only Esc and the mouse (gf_start.inc:325-336).
- **Original:** the Win32 dialog default button is answered by Enter.
- **Fix:** Enter returns button 0.
- **Test:** walk2 with `key enter` then `require confirm_open == 0`.

### W9-03-26: [MultiplayerDialogSettings] booleans are not read from the rules
- **Severity / evidence:** minor. [CONFIRMED-DATA] rulesmd: ShortGame=yes, Crates=yes, MCVRedeploys=yes, and the rest.
- **Clone:** reads only the money, unit, speed and tech values (mp_pages.inc:418-427). The booleans are compile-time defaults (mp_model.hpp:60-64). The values happen to match the shipped rulesmd, but a mod would not take effect.
- **Fix:** read ShortGame, Crates, MCVRedeploys and (if the key exists) SuperWeapons/BuildOffAlly with `section_flag`.
- **Test:** unit test with a modified rules.

### W9-03-27: in-game chat prompt wording
- **Severity / evidence:** cosmetic. [CONFIRMED-PLAY] shot 36 shows "Say: gg from host_".
- **Original:** `TXT_TO_ALL` "To All:" / `TXT_MESSAGE` "Message:" (game_scene_net.inc:478).
- **Fix:** use the CSF prefix.
- **Test:** shot.

### Harness note (not a game gap)
The `edit:chat` target is built from `edits_[0].enabled`, which is only updated in `draw_body`. With `set render_every N`, the target disappears after a Customize Battle round-trip until the next render (walk1 result L67). Fix: compute `enabled` in `tick()`.

### Usability notes
- **Esc leaves a hosted lobby with no confirmation.** It does so even while typing chat, which drops every joiner (mp_pages.inc:244-247). Esc → Back matches the original (Back is IDCANCEL=2 in 0xBC), but a confirm when guests are present is advisable.
- **The client's status line stays empty while it waits for the host.** A text such as "Waiting for the host to start the game" is suggested.
- **The host's "Join by IP" address is shown only as the first local IPv4.** Shot 03 shows "192.168.x:50020". There is no copy affordance, and machines with several network interfaces may show the wrong address.

## Verified OK
- **Main menu:** it has Network and Internet entries in the original order and positions (0xE2), with CSF labels.
- **Host button rows:** Start Game (row 1), Customize Battle (row 2) and Back (row 5) match the 0xBC rows. The map preview well at (486,37,144,112) matches 0x468.
- **Option set:** Short Game / MCV Repacks / Crates Appear / Superweapons and the Game Speed / Credits / Unit Count sliders use the CSF labels. The ranges come from [MultiplayerDialogSettings]: MinMoney 5000, MaxMoney 10000, MoneyIncrement 100, Money 10000, UnitCount 0..10 (default 10), GameSpeed 1. YR's 0xBC has no tech-level, bridges or multi-engineer controls, so their absence is correct; BridgeDestruction is read from the rules (world_depth.inc:66).
- **Host-only options:** only the host can change options; guests' controls are disabled and their probe targets absent (pair shots 50-53).
- **Live updates:** clients see option changes live (crates off appeared on the joiner within one frame; shot 53).
- **Accept reset:** Accept resets when the host changes options or the map ([CONFIRMED-PLAY] pair: `slot_ready(1)` 1 → 0 after `check:crates`; mp_lobby.cpp:105-108, 144-148).
- **Colour and start lists:** the colour combo hides colours other rows own (shot 51: no RED for the joiner after the host took RED). The start combo hides taken starts. Teams are "-" / A-D.
- **Map preview:** start markers come from PreviewPack (baked in). Map names come from the CSF with the "(min-max)" player counts.
- **Kick:** a confirm dialog on the host and "You were removed from the game by the host." on the guest (shots 90, 95). Rejoining after a kick works.
- **Lost host:** "The host closed the game." → OK → back to the Network page (shot 97).
- **Bad address:** Join by IP with a bad address gives a dialog with an example (shot 23). An unanswered join ends after the query retries; the dialog then reads "Connection to the host was lost." (the system line says "No answer from the host.", which is the more accurate text).
- **Version mismatch:** reject texts exist for version, rules, full, started and platform (mp_lobby.cpp:31-45); the Internet page maps server errors (mp_pages.inc:1402-1406).
- **Chat:** lobby chat (Enter sends) and in-game chat cross over (pair: "Joiner: ready again", "[net] chat Hosty: gg from host").
- **Leaving a match:** EXIT is scheduled and runs on both machines at frame 170. The remaining player sees the leave and wins (pair2 stderr).

## Screenshots (all under D:\ra2-re\w9mp-03-out\)

**walk1/ (single process, LAN host):**
- 01_main_menu
- 02_lan_browser
- 03_lobby_host_empty
- 04_lobby_kind_combo
- 05_lobby_color_combo
- 06_lobby_color_combo_row1
- 07_lobby_start_combo
- 08_lobby_team_combo
- 09_lobby_country_combo
- 10_choose_map
- 11_choose_map_scrolled
- 12_lobby_chat

**walk2/:**
- 20_internet_empty
- 21_internet_unreachable
- 22_internet_badname
- 23_joinip_bad

**pair/host/:**
- 30_host_with_client
- 31_host_client_ready
- 32_host_after_option_change
- 33_host_before_start
- 34_host_ingame
- 35_host_diplomacy
- 36_host_chat_typing
- 37_host_chat_sent
- 38_host_options_menu
- 39_host_load_in_mp
- 40_host_abort_page

**pair/join/:**
- 50_join_lobby
- 51_join_color_combo
- 52_join_ready
- 53_join_after_host_option_change
- 60_join_ingame
- 61_join_ingame_after_chat
- 62_join_after_host_left
- 63_join_final

**pair2/host/:**
- 70_host_loading_a
- 71_host_loading_b
- 72_host_after_abort_click
- 73_host_after_leave
- 74_host_after_continue

**pair2/join/:**
- 80_join_diplomacy
- 81_join_after_host_abort
- 82_join_result
- 83_join_after_continue

**pair3/host/:**
- 90_host_kick_confirm
- 91_host_after_kick
- 92_host_after_back

**pair3/join/:**
- 95_join_kicked
- 96_join_after_kick_ok
- 97_join_host_left

**Original reference data:**
- dlg_gamemd_all.txt (all 97 dialogs)
- csf_all.txt
- rulesmd.ini
- mpbattlemd.ini

## Not covered
- **Internet (relay) lobby.** The relay lobby was not run as a pair: the Internet page was shot, but `MpLobbyScene` is shared code with LAN, so the lobby gaps apply to it too.
- **Loading screen.** It loads within one driver frame headless, so no shot of it exists.
