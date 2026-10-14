# W9-05: In-match multiplayer features and robustness, compared with the original

Auditor: w9mp-05. Clone: D:\ra2-android master f188125 (build\ra2engine.exe, ra2netpeer.exe, ra2lobbyd). Original: gamemd.exe YR 1.000, RE'd in Ghidra on a project copy, which has since been deleted.

Evidence is in D:\ra2-re\w9mp-05-out:
- `runs\` holds the pair runs: leave, kill, desync, relay;
- `harness\nethar.cpp` is an N-peer lockstep harness built outside the repo against engine/net;
- `harness-runs.txt` holds the harness results;
- the remaining files are the raw Ghidra logs (d1, d2, m1–m7, x1, x2, s1, c1).

## Summary

**The lockstep transport is solid.**
- 8 in-process peers with 5–30% loss, 20–300 ms jitter and latency spikes of 3–8 s stay in sync for 9,000–20,000 frames.
- A cut peer is kicked after the original's total timeout, and the other peers carry on in sync.
- A real two-process LAN match handles each of these correctly:
  - a graceful host leave (the joiner wins at the same frame);
  - a host crash (the joiner is kicked through after about 40 s and wins);
  - an injected desync (detected 15 frames later on both machines, with dump files).
- An online match survives a 10 s relay restart mid-match: the 3,000-frame trails are identical. This works because the V3 tunnel re-registers clients from their next packet.

**What a player feels is the missing in-match layer of the original's multiplayer.**
- There is no in-game diplomacy: no ally or unally, no locked alliances, no shared victory through alliances.
- Chat always goes to everyone (no allies-only or listener chat).
- A defeated player is not kept as an observer with the map revealed; they are sent to the score screen and leave the match.
- There is no "Leave / Observe" exit dialog.
- There are no beacons.
- BuildOffAlly is ignored.
- The defeat line, the score screen and the diplomacy page show country names instead of player names. The score screen is titled "Skirmish Score".
- The diplomacy page marks house 0 as "self" on every machine.

**Robustness and determinism.**
- **One real desync (blocker):** cancelling production from the Android touch cameo menu changes the World directly instead of issuing a command.
- **Version check:** the lobby's sim-version check is effectively a constant, so different clone builds or mods can join each other and then desync mid-match.
- **Network trust:** several network fields are trusted without validation (sender house, frames, TIMING, REMOVEPLAYER target).

## Gaps

### W9-05-01 Touch cameo-menu "Cancel" changes the World directly, so the match desyncs (blocker)
- **Evidence:** [CONFIRMED-RE code read] `engine/touch_ui.inc:405-411`
  ```
  case CM_CANCEL: world_.econ_abandon(local_house_, …)
  ```
- **Original vs clone:**
  - In the original, every production change is an EVENT (ABANDON 0x0F, sent through the DoList).
  - In the clone, Pause, Resume and All go through `cameo_click` → `issue_type_event`, but Cancel calls the World directly on the local peer only. The local refund and queue change make the hash mismatch within 15 frames, which is a hard stop.
- **Platforms:** Android touch UI only. The xplat and net gates are keyboard and mouse driven, so they never exercise it.
- **Fix:** issue `ET::Abandon` like `cameo_click(type, right=true)` does for the current item (game_scene_controls.inc:415-444).
- **Tests:**
  - a net loopback in which peer B runs `cameo_menu_do(CM_CANCEL)`, then assert equal `net_state_hash` for 300 frames;
  - a source-grep test: no `world_.econ_*` or other World mutators in `touch_ui.inc` or `input*.cpp`.

### W9-05-02 The version and data gate does not stop mismatched builds or mods (major)
- **Evidence:** [CONFIRMED-DATA code+git]
  - `engine/mp/mp_model.hpp:37`: `kSimVersion = 1` has not changed since it was introduced (f9083e6), across 36 later world.cpp commits and save format versions 34→45.
  - `mp_lobby.cpp:266`: the engine version string is "informational" and is not compared.
  - `mp_pages.inc:74`: only rulesmd.ini is CRC'd.
- **Original vs clone:** the original compares its version and the scenario digest (§7.1). The clone admits:
  - a phone APK built from an older commit;
  - a PC with a modified artmd.ini or aimd.ini. Infantry sequences and fire timing are in the hash; AI players and AI takeover run aimd on every peer.
- **What the user sees:** if the start state differs, "Scenarios do not match" at frame 0. Otherwise, "Out of sync" minutes into the match.
- **Root cause:** a manual version constant that is never bumped, and a data CRC that covers only one file.
- **Fix:**
  - derive `sim_ver` from the git hash or RA2_VERSION at build time, or CRC the sim sources;
  - compare the version string;
  - add CRCs of artmd.ini, aimd.ini and the effective map bytes to QueryJoin and the LAN beacon;
  - show "different version" in the browser and the join error.
- **Test:** lobby unit tests in which a different art/aimd CRC or a different build id gives `RejectReason::Version/Rules`.

### W9-05-03 No in-game diplomacy: ToggleAlliance and the Diplomacy Allies checkbox do nothing (major)
- **Evidence:** [CONFIRMED-RE 0x536C50→FUN_007309C0, FUN_00730910, EventClass::Execute FUN_004C6BD0 case 3, MakeAlly FUN_004F9990, MakeEnemy FUN_004F9DB0, Is_Allowed_To_Ally FUN_00501360]
- **Original:**
  - **EVENT_ALLY toggles the alliance.**
  - **MakeAlly is one-way.** It sets only `A.Allies |= B`.
    - It is refused if any of these holds: the two are already allied; the target is neutral; A is defeated; the alliance would leave A with no enemy.
    - Message `TXT_HAS_ALLIED` "%s has allied with %s".
    - While one-way: `EVA_RequestingAlliance` / `EVA_AllianceRequested`.
    - Once mutual: `EVA_AllianceFormed` / `EVA_EnemyAllianceFormed`.
  - **MakeEnemy breaks both directions.** Message `TXT_AT_WAR` "%s declares war on %s", plus `EVA_AllianceBroken`.
  - **Gates:**
    - the target is human;
    - the player is not defeated;
    - `[MultiplayerDialogSettings] AlliesAllowed` and `AllyChangeAllowed` are both on;
    - mode INIs: Team Game has AllyChangeAllowed=no, which is YR's "locked alliances"; FreeForAll and Coop have AlliesAllowed=no.
- **Clone:**
  - `game_scene_controls.inc:215-221`: ToggleAlliance and PlaceBeacon are "consumed with no effect".
  - The Diplomacy page (`main.cpp:7705-7724`, `ingame_pages.inc:527-546`) has read-only checkboxes, with `r.ally = house_team == house_team`.
  - `command.cpp:386-391` makes ALLY **symmetric** (`set_pair_alliance(H, other, !are_allied)`) with no gates and no messages.
  - The lobby has no game-mode or AlliesAllowed concept.
- **Fix:**
  - wire the hotkey (owner of the first selected object) and the Diplomacy checkbox to `issue(Ally)` with the gates above;
  - make `World` alliances directional (`ally_pairs_[a*n+b]` only);
  - add the messages and EVA lines;
  - add AllyChangeAllowed per mode (Team Game = locked).
- **Test:** a net play in which house 0 allies house 1. Check one-way state and the message on both peers, then the reciprocal ally gives a mutual alliance and the EVA lines. In Team Game the checkbox is disabled.

### W9-05-04 The game does not end when every remaining house is allied (major, follows from 03)
- **Evidence:** [CONFIRMED-RE tail of FUN_004FBED0, "All remaining are allied"]
- **Original:** the game ends when every pair of living, non-passive houses is *mutually* allied (dynamic `Allies` bitmasks). Lobby teams count only through the alliances they create.
- **Clone:** `world.cpp:618-626` `step_victory` uses the static `house_team()`, the "last team standing" rule.
- **Root cause:** victory is not tied to alliance state.
- **Fix:** compute victory from the mutual `ally_pairs_`.
- **Test:** an FFA with three players where two ally mid-game and kill the third. Both win, on both peers.

### W9-05-05 A defeated player is not kept as an observer; they are sent to the score screen and leave (major)
- **Evidence:** [CONFIRMED-RE FUN_004FBED0, 0xA8A4A0, FUN_005779D0 reveal, FUN_00656830(1) radar on]
- **Original:** the local defeated player:
  - stays in the match;
  - gets the whole map revealed and the radar forced on;
  - has every chat listener ticked;
  - sees "%s has been defeated!" with their own name, plus `EVA_YouHaveLost`.

  The score screen comes only when the whole game ends.
- **Clone:** `gf_scene.inc:154-172`: `house_outcome(local)=-1`, then BorrowedTime, then the result scene. `net_filter_next` then sends EXIT (`game_scene_net.inc:301-313`), so the player leaves the match.
- **Fix:** in a net game with at least 2 living opponents:
  - stay in the GameScene in observer mode (reveal the map, radar on, disable production);
  - open the score screen at the global game end.
- **Test:** a 3-player net run where house 2 is defeated at frame N. It stays in the scene, `net_left=0`, the map is fully revealed, and its trail matches the others to the end.

### W9-05-06 The exit dialog has no MP "Leave / Observe", no confirmation, and Save/Load/Delete stay visible (minor)
- **Evidence:** [CONFIRMED-RE dialog 0xB6 proc 0x4F17C0, FUN_0048C7C0 case 3] [CONFIRMED-PLAY runs\leave\host\host_menu.png, host_abort_page.png]
- **Original, in LAN/WOL:**
  - the buttons are `GUI:Leave` plus `GUI:Observe` ("Surrender and observe the battle.", shown only if the player is alive and at least 2 humans are alive), plus Resume;
  - the non-WOL path also confirms with `TXT_OK`/`TXT_CANCEL`;
  - Leave while alive sends DESTRUCT and then EXIT, so the others see "%s has been defeated!";
  - Observe sends DESTRUCT only.
- **Clone:**
  - the single-player page (`ingame_pages.inc:654-664`): "Abort" + "Resume Mission", with no confirmation;
  - Load/Save/Delete Game are shown and then refused with a banner (`main.cpp:7629`).
- **What the others see:** EXIT, then "Hosty has left the game." (`game_scene_net.inc:162`). The original shows the defeat line and the `EVA_PlayerDefeated` voice.
- **Fix:** add an MP options-page variant and the Observe action (needs 05); send DESTRUCT before EXIT.
- **Test:** a play script that opens the MP options. It shows Leave/Observe/Resume, with no Save/Load.

### W9-05-07 Chat goes to everyone only: no allies-only chat, listener column or observer rule (minor)
- **Evidence:** [CONFIRMED-RE FUN_0055E050, FUN_0048D0F0 case 0xC, MessageListClass::Init FUN_005D3480]
- **Original:**
  - **Modes:** Enter sends to the Diplomacy "Chat" listeners (all ticked by default); Backspace sends to allies; Backslash `\` sends to everyone.
  - **Prompt:** `TXT_FROM` "From %s:".
  - **Observer rule:** `TXT_OBSERVER_CANT_SEND` for observers.
  - **Display:** "Name:" in the sender's colour, shown for MessageDelay×900 ticks (0.6 min); up to 6 lines, 98 chars per line.
- **Clone:**
  - `game_scene_net.inc:354-365, 419-428`: Enter or the "Chat" button sends to all remotes (`net_game.cpp:145-153`, `chat_mask` 0), with the prompt "Say:".
  - The display time is 12 s (`post_message(...,12.0)`).
  - The Diplomacy "Chat" column checkboxes are drawn always on and do nothing.
- **Fix:**
  - add the three modes (send only to the selected recipient peers);
  - make the listener column toggle recipients;
  - use the prompt "From %s:" and a display time of MessageDelay×60 s (36 s).
- **Test:** a three-peer net play where Backspace chat reaches only the ally.

### W9-05-08 Beacons are missing (minor)
- **Evidence:** [CONFIRMED-RE PlaceBeacon 0x536DD0→FUN_00730B00, BeaconManager 0x89B328, global cmds 0x20/0x21/0x22]
- **Original (YR 1.000 has beacons):**
  - 3 per player;
  - visible to mutual allies;
  - `EVA_BeaconPlaced` / `EVA_BeaconDetected`, plus a radar event;
  - a text of up to 127 chars;
  - Delete removes one;
  - sent out of band on the global channel.
- **Clone:** the commands are recognised but do nothing (`game_scene_controls.inc:215`). `global_msg.hpp` names 0x20–0x22 but they are not implemented.
- **Fix:** a BeaconManager in the scene (not the sim), using the global packets.
- **Test:** a net play where house 0 places a beacon, the ally sees it and its own beacon count is 1; a 4th is refused.

### W9-05-09 BuildOffAlly is ignored (minor)
- **Evidence:** [CONFIRMED-RE FUN_004A8DC0 @0x4A8F0A, BuildingType +0x1550 EligibileForAllyBuilding]
- **Original:** with BuildOffAlly on, an *allied* building counts for adjacency if its type has `EligibileForAllyBuilding=yes`. Only the ConYards GACNST, NACNST, YACNST and YACOMD have it.
- **Clone:**
  - `mp_model.cpp:321-333` `base_config` never copies `build_off_ally` to the SkirmishConfig;
  - `building.cpp:61-73` `placement_check` uses own buildings only.
- **Fix:** pass the flag through, and in `placement_check` also accept allied buildings whose type is eligible.
- **Test:** a unit test in which placement next to an ally's ConYard passes when the flag is on and fails when it is off.

### W9-05-10 The defeat line, score screen and diplomacy page show country names, not player names (minor)
- **Evidence:** [CONFIRMED-PLAY runs\leave\join\join_end.png: rows "America"/"Russia", title "Skirmish Score"] [CONFIRMED-RE 0x5C95FF title select, FUN_005C92C0 UIName + colour]
- **Original:**
  - `TXT_PLAYER_DEFEATED` uses the house UIName, which is the player's nickname, in the house colour;
  - the score title is `GUI:MultiplayerScore` when not in skirmish;
  - each row is in the house's colour.
- **Clone:**
  - `gf_scene.inc:141-150`, `gf_stats` (`gf_scene.inc:200-206`) and `gf_result.inc:62` hard-code `GUI:SkirmishScore` and use country names ("[RE-FALLBACK: no MP player names]"), although `net_->name_of(h)` exists.
  - The "Built" column also counts start units (America Built=9 with nothing built). This needs checking against `FUN_005C92C0` "Built = sum of 4 counters".
- **Fix:** when `net_`, use the names and colours from `name_of`, and the MP title.
- **Test:** a net-match probe in which `score_name(h)` equals the lobby nick and the title key is MultiplayerScore.

### W9-05-11 The Diplomacy page marks house 0 as "self" on every machine (minor)
- **Evidence:** [CONFIRMED-RE code read] `main.cpp:7716` `r.self = h == 0;`
- **Impact:** on the joiner (house 1), its own row gets the checkboxes and the host's row has none.
- **Fix:** `h == local_house_`.
- **Test:** open the diplomacy page on the joiner; probe that the self row is house 1.

### W9-05-12 Remote packets are trusted (sender house, frames, TIMING, REMOVEPLAYER target) (major: cheat/grief, can desync)
- **Evidence:** [CONFIRMED-RE code read]
  - `lockstep.cpp:146-174`: `ev.house = pkt[i+2]` is never checked against the connection id, so a peer can order or sell another house's things or EXIT for it. It also corrupts that house's megamission cache on one receiver only, which is a desync.
  - `lockstep.cpp:152-156`: `their_frame=max(sf)` accepts any frame, so the gate opens early and later events are dropped as late on some peers only.
  - `lockstep.cpp:352-368`: TIMING is accepted from any house, without clamping. The u16 MaxAhead is truncated to u8 in the headers.
  - `game_scene_net.inc:151`: REMOVEPLAYER resigns any `c.value`. If that is the victim's own house, its client freezes.
  - `event_codec.cpp` ~278/305: the megamission slot is not limited to 0–7.
- **Original:** these are largely the same in the original (it trusted peers too). These are hardening gaps, not parity gaps. They matter for an internet relay that anyone can use.
- **Fix:**
  - enforce `house == conn_id`;
  - window-check frames;
  - accept TIMING only from the master and clamp it;
  - accept REMOVEPLAYER only for a disconnected human house other than the sender;
  - limit the slot to 0–7.
- **Test:** inject each malformed event in `test_net_lockstep`; assert no effect and no desync.

### W9-05-13 NaN and out-of-range float→int differ on real arm64; the gate cannot see it (major, latent)
- **Evidence:** [SUSPECTED, code read]
  - `savegame.hpp:13-14,146` hashes doubles bit-exact. An SSE default NaN is 0xFFF8…, an arm64 one is 0x7FF8…
  - Plain `static_cast<int>(double)` and `std::lround` (32-bit long on Windows, 64-bit on Android): `world.cpp:2311`, `world_an.cpp:779-780,1376`, `flight.cpp:82`. Out of range, x86 returns INT_MIN while arm64 saturates.
- **Gate coverage:** the xplat gate runs Windows vs Android **x86_64**, so it cannot see either difference.
- **Fix:**
  - canonicalise NaN in the save writer (and log it in debug builds);
  - route double→int in the sim through `om::ftol` or a clamping `om::lround`;
  - add a real arm64 device to the xplat gate.
- **Test:** saving a NaN or −NaN double gives identical bytes; `om::lround(1e30)` gives the same result on both builds.

### W9-05-14 ra2netpeer --selftest falsely reports out-of-sync after 2,550 frames (minor, test tooling)
- **Evidence:** [CONFIRMED-MEASURED]
  - `ra2netpeer --selftest --frames 20000` gives "out-of-sync frame 2570 … SELFTEST FAIL", deterministically. The first failing run is at `--frames 2551`.
  - `--protocol-zero` passes.
- **Root cause (found with the harness):** a MEGAMISSION executed from the local OutList has cache-slot byte `data[6]=0`. The copy that went through the encoder carries the slot the encoder assigned (1). `StubSim::apply` hashes the raw bytes including that slot (`stub_sim.hpp:28-36`). The engine's event→Command map ignores the slot, so real matches are not affected.
- **Fix:** zero `data[6]` for type 4 in `StubSim::apply`, or set the slot on the local echo too. Also raise `test_net_lockstep`'s 700-frame runs to 3,000 or more frames.
- **Test:** `ra2netpeer --selftest --frames 20000` passes.

### W9-05-15 Lobby game speed is ignored in a match, and net logic is fixed at 15 Hz (minor here; see the speed report)
- **Evidence:** [CONFIRMED-RE code read]
  - `main.cpp:6920` uses multiplier 1.0 when `net_`.
  - `game_scene_net.inc:15` says "the lobby speed is not used".
  - The original's TIMING fps cap follows GameSpeed (`lockstep.cpp:251`).
- This is listed for completeness; the speed audit covers it.

### W9-05-16 The house-left message and the stall overlay are hard to see (cosmetic, partly unverified)
- **Evidence:** [CONFIRMED-PLAY runs\leave\join\join_after_host_left.png, runs\kill\join\join_stall_early.png]
  - Neither shot shows the "has left" line or the "Waiting for players" text on screen. The stall overlay needs more than 1 s of wall time, and the play driver's frame pacing makes that inconclusive.
  - The original uses `TXT_LEFT_GAME`, `TXT_CONNECTION_LOST` "Connection to %s lost!", and the reconnect dialog `TXT_RECONNECTING_TO` with a kick vote (global 0x1C).
  - The clone uses its own English strings, not the CSF ones, and has no vote-kick; only the local "Drop player" button exists.
- **Fix:** use the CSF keys `TXT_LEFT_GAME`, `TXT_CONNECTION_LOST` and `TXT_RECONNECTING_TO`.
- **Test:** a kill run with real-time pacing; the shot shows the dialog.

### W9-05-17 After a kick, peers can disagree on the dropped player's last events (minor, same as the original)
- **Evidence:** [SUSPECTED, code read] `lockstep.cpp:225-232, 395-402`.
- **How it happens:** with 3 or more players, if the dropped peer's events for frame N reached peer A but not peer B, A executes them and B does not.
- The original behaves the same way. No divergence appeared in the 8-peer harness cut tests with 5% loss.
- **Fix (optional):** on REMOVEPLAYER, discard the removed house's DoList entries for frames at or after the REMOVEPLAYER frame on every peer.

## Robustness runs (measured)

| Run | Result |
|---|---|
| Harness, 2 peers, 20,000 frames clean (StubSim slot artefact masked) | in sync |
| Harness, 8 peers, 20,000 frames clean | in sync; 22,808 events executed |
| 2 peers, 5% loss, 20–80 ms LAN | in sync; +13% time |
| 2 peers, 20% loss, 50–200 ms internet | in sync; ×3.5 slower |
| 8 peers, 5% loss, 20–120 ms internet | in sync; +37% time; MaxAhead 30 / FSR 10 |
| 8 peers, 30% loss, 50–300 ms internet | in sync, but ~×58 slower (unplayable, as the original would be) |
| 4 peers, 3 s and 8 s latency spikes | in sync; 10 stalls; no kick |
| 3-peer / 8-peer cut of one peer at frame 3,000 | the cut peer is REMOVEPLAYERed after the total timeout (LAN ~40 s); the rest stay in sync |
| Real engine LAN: host aborts at frame 310 | EXIT at 320 on both; the joiner gets "Hosty has left the game." and WON → result |
| Real engine LAN: host process killed at frame 300 | the joiner stalls ~40 s, "connection lost", house 0 removed at 330, WON |
| Real engine LAN: desync injected at 600 (net-match.ps1 -DesyncAt) | both peers detect it at 630 (the frame-615 hash); DESYNC_F615_H0/H1 .SAV/.TXT written; trails identical before |
| Real engine online: relay killed at frame 630, down 10 s, restarted | the match continued; 201/201 trail entries identical; chat and away/back delivered |

**Desync behaviour vs the original:**
- The original's out-of-sync is a hard stop. It writes SYNC%d.TXT and shows `TXT_OUT_OF_SYNC` ("Reconnection Error!") with a STOP button. There is no recovery.
- The clone does the same and adds a full save and per-member hashes. This is OK and better for diagnosis.
- Like the original, the clone does not compare hashes for the first 256 frames.

**Not run:**
- 8 real engine processes: ra2netpeer is 2-peer only; the in-process harness covered the transport instead.
- A different-version or modified-data joiner: needs a second build or edited game data, both out of bounds. Reviewed in code instead (02).
- A lobby-host leave before start: covered by the lobby audits.

## Determinism review (other than the above)

**OK:**
- `-ffp-contract=off` on both builds; no fast-math or long double; SSE2 on MinGW.
- Trig, sqrt and atan2 go through om:: tables.
- Every sim `std::sort` has a strict total order (or uses `stable_sort`).
- `unordered_map` is used only for lookups; no `std::hash` or pointer ordering.
- Every RNG is session-seeded (GameRandom, ProjRng ×3, OreRng, AI `ai_rand`, per-unit `rng`); no `std::rand`, `random_device` or distributions in the sim.

**Wave-7/8 code, OK:**
- `call_for_help` (`world_w7.cpp:281-379`) scans by index, keeps the top six in a fixed array, exchange-sorts them, and draws `combat_rng_` in index order.
- The threat scan and scatter are deterministic.
- Paradrop Approach/Overfly (`world_an.cpp:967-988`) is pure state. The `Unit&` held across the spawn is protected by `reserve`.

**Lower-risk notes:**
- `combat_rng_ptr() const` and `w6_overlay_wall_hit const` change sync state behind `const`, a hazard if presentation ever calls them.
- The thread-local path memo outlives a World.
- The libm-ban test misses world_w7, world_loco, locomotion, ai_unit and command.cpp.
- The xplat gate soak defaults to 0 minutes, which skips the GameScene-only sim paths.
- `main.cpp:7238` `enable_live_infantry_anims(…,0)` treats house 0 as human even when it is an AI. This is wrong, but it is the same on every peer.

**Not covered:** the uninitialised-member and save-coverage part of the review (`save_fields.hpp`) is incomplete. The review sub-agent's own child had not reported when it handed back.

## Verified OK
- EXIT / REMOVEPLAYER handling: executed at a common frame; the leaver is resigned (or handed to the AI under the "AI takes over" option); the "has left / lost connection" messages.
- Wait and kick timeouts follow the original's table 0x836338 (LAN 2400 / Internet 7200 ticks total).
- Short Game rule: defeated when no significant buildings and no BaseUnit MCV (`world.cpp:597-615`), matching the original's FUN_004F8350. The long game counts all units, matching the original's buildings + units + infantry + aircraft.
- AllyReveal shared vision for lobby teams (`skirmish.cpp:24`). The original shares one hop through IsAlliedWith; the radar minimap has no separate sharing code in the original either.
- `TXT_PLAYER_DEFEATED` + `EVA_PlayerDefeated` are announced when another player is defeated (names aside, see W9-05-10).
- The Crates option is passed to the sim (`mp_model.cpp:327`, `main.cpp:6029`). The original's initial count is min(max(CrateMinimum, humans), CrateMaximum); not re-verified in the clone.
- In-game chat and the Android away/back notices travel on the global channel and survive a relay restart.
- The V3 tunnel needs no room state mid-match: clients re-register on their next packet (`server/internal/tunnel/tunnel.go:203-233`).
