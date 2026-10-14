# W9-04: Multiplayer on the PHONE (touch menus, Android lifecycle, phone networking)

Auditor: w9mp-04. Tree: D:\ra2-android master f188125 (APK `0.9.0+gf188125`, built 2026-10-14 02:01).

## How this was tested

- **Code read:**
  - engine/mp_pages.inc, game_scene_net.inc, game_scene_lifecycle.inc, touch_ui.inc;
  - touch_gestures.cpp, platform.cpp, shell_widgets.inc;
  - engine/mp/*, engine/net/lockstep.cpp, server/internal/{tunnel,lobby};
  - android/app/src/main/AndroidManifest.xml, RA2Activity.java, and SDL 2.30.9 SDLActivity.java.
- **Real Android run** (the evidence tagged [CONFIRMED-PLAY emu]):
  - AVD `ra2x86`: Android 11 / API 30, x86_64, 1080x1920 at 420 dpi, which is phone-like;
  - the stock AOSP LatinIME keyboard;
  - the current `app-debug.apk` installed over the old one (`adb install -r`);
  - driven by `adb shell input tap/swipe/keyevent`;
  - checked with `dumpsys input_method` (keyboard shown or not, the IME's served view rect), `screencap` and logcat.
  - Screenshots are in D:\ra2-re\w9mp-04-out\ (emu*.png, r*.png, x*.png).
- **Desktop `--play` touch run:** D:\ra2-re\w9mp-04-out\touch-mp.play drives the real `TouchGestures` layer. It runs under the net lock together with the cross-platform pair test (see §"Pair test"). The pair test has a desktop joiner and the emulator host, through `adb emu redir udp:50020`.

**Scale on a phone.** The MP shell is the 640x480 template, letterboxed 4:3 into the panel height (`ShellXform::make`, main.cpp:6203). On a 6.5" 20:9 phone in landscape (about 68 mm and about 411 dp tall), 1 logical px is about 0.14 mm, or 0.86 dp. On the emulator, the 1080-px-tall panel gives 2.25 px per logical px.

| MP widget | Logical size | Phone (dp) | Android guideline |
|---|---|---|---|
| right-column button | 156x42 | ~36 dp tall | 48 dp |
| lobby combo / slot row / edit | 19 px tall | ~16 dp | 48 dp |
| checkbox, list row, combo item | 16 px | ~14 dp | 48 dp |
| list scroll strip (only way to scroll) | 12 px wide | ~10 dp | 48 dp |
| slider thumb | `kTrackbarThumbW` | drag impossible (see W9-04-05) | - |

---

## Summary

Multiplayer can be operated on a phone, but only with patience and luck. Three defects stop real players.

**1. Text entry is broken in three different ways on the soft keyboard (all confirmed on Android 11):**
- the keyboard's ⌫ cannot delete pre-filled text: the name "Player", or a saved server address;
- after the keyboard is dismissed with Back, tapping another (or the same) field focuses it but the keyboard never comes back. The in-game chat line behaves the same way;
- Enter / tapping a button does not hide the keyboard.

**2. After the app returns from the background during an MP match, the camera has scrolled off into black.** The phone acts as if a mouse were parked at (0,0), and the edge-scroll runs while the match keeps going. This was reproduced 3 out of 3 times. Workaround: CMD › MORE › NEXT.

**3. The lifecycle is only half handled:**
- Lobbies have no AWAY at all. 15 s in the background (for example sending the room code / IP to a friend in a messenger) drops the joiner, or kills the lobby for every joiner when it is the host.
- In a match, AWAY covers 90 s. But the phone's own stall timer is not reset on return, so on return it can immediately kick the very peer it was waiting for.
- The host's "address to type" can be the phone's *mobile-data* IP.

Also:
- every MP target is 14-36 dp, against a 48 dp guideline;
- lists cannot be drag-scrolled;
- sliders cannot be dragged (confirmed);
- Android Back / an edge swipe leaves a lobby (and closes it for every joiner when you host) with no confirmation;
- the host's "AI takes over" / "Cross-platform (exp.)" checkboxes are drawn on top of the "Game Speed" / "Credits" labels.

**Working:**
- the soft keyboard does appear on a tap;
- `adjustPan` keeps the focused field above it;
- lobby chat and in-game chat send with the keyboard's ⏎;
- KEEP_SCREEN_ON is set;
- AWAY / BACK is sent on background / foreground;
- the four network permissions and the MulticastLock are in place;
- the cross-platform refusal text is clear, but only the joiner ever sees it.

---

## Gaps

### W9-04-01 Soft keyboard ⌫ cannot erase pre-filled text: name, server address, any earlier entry
- **Severity:** blocker for the Internet page when the saved server address is wrong; major for names.
- **Evidence:** [CONFIRMED-PLAY emu] emu5c.png / emu6c.png. With the name "Player" pre-filled, I tapped the field and tapped ⌫ twice, then "q", "w": the result was "Playerqw". One more ⌫ gave "Playerq". Backspace only removes characters typed in the *current* IME session.
- **Original vs clone:**
  - The original is desktop-only; every Windows edit control erases normally.
  - Clone: `MpPage::handle` (mp_pages.inc:234-238) deletes on `kKeyBackspace`, but on Android 10+ the IME never sends one for text it does not know.
- **Root cause:**
  - SDL 2.30.9's `SDLInputConnection` (SDLActivity.java:2006-2018) only synthesises `'\b'` from `deleteSurroundingText` on API <= 29.
  - On API 30+ it diffs its own hidden `EditText`. That EditText starts empty on every `onCreateInputConnection` (SDLActivity.java:1938-1946), so pre-filled engine text can never be deleted.
  - `kKeyDelete` (clear field, mp_pages.inc:240) has no soft-keyboard equivalent.
  - The edit text is drawn head-first (`p.fit`, shell_widgets.inc:164), so overflowing text cannot be seen either.
- **Suggested fix (any of these):**
  - (a) Seed the IME: call `SDL_SetTextInputRect` plus a JNI `setText(edit.text)` on the DummyEdit's EditText when an MpPage edit gains focus. Alternatively, fork SDLInputConnection so that `getEditable()` mirrors the engine string.
  - (b) Give every edit a touch "clear" ✕ button inside its right end (it is the existing `kKeyDelete` path).
  - (c) Select-all on focus: the first typed character replaces the whole text.
  - Apply the same fix to the skirmish name edit, the in-game chat, and the save-name edit.
- **Proposed test:**
  - A --play `touch tap edit:nick` then `ime_commit "x"` / `ime_delete 1` (a new harness op that drives the SDLInputConnection diff logic) must leave `edit(nick)` without "Player".
  - Device check: the adb sequence above should give "qw".

### W9-04-02 After Back hides the keyboard, a tapped field (or the in-game chat) never brings it back
- **Severity:** major (Join-by-IP and the lobby chat look dead on a phone).
- **Evidence:** [CONFIRMED-PLAY emu]
  - LAN page: tap "Your name" (keyboard shown), then Back (keyboard hidden), then tap "Join by IP". The IP field is highlighted but `mInputShown=false`; a second tap also gives `false` (emu9.png).
  - Lobby: tap chat, Back, tap chat again gives `false` (emu17.png).
  - In game: tap Chat (keyboard up), Back gives `false`. The "Say:" box stays open and tapping Chat again gives `false` (emu25s.png). A second Back closes the box.
- **Original vs clone:** the original has no soft keyboard. The clone's `set_focus` (mp_pages.inc:318-330):
  - calls `SDL_StartTextInput()` only on the -1 → i transition;
  - returns early for `i == focus_`;
  - does nothing on an i → j switch.
  - `net_handle` (game_scene_net.inc:394-418) routes every touch to the open chat box and never restarts text input.
- **Root cause:** Back is consumed by the IME. SDL's `onKeyPreIme` → `onNativeKeyboardFocusLost` → `SDL_StopTextInput()` (SDLActivity.java:1929-1932). The engine's own focus / `net_chat_open_` state is never told.
- **Suggested fix:**
  - On every tap on an enabled edit, even the focused one, call `set_text_input_rect_logical` + `SDL_StartTextInput()` when `!SDL_IsTextInputActive()` or when focus changes.
  - On a focus switch, update the rect.
  - In game, a tap on the chat box or the Chat button while `net_chat_open_` restarts text input.
  - Optionally, treat `SDL_IsTextInputActive()==false` while focused as "unfocus".
- **Proposed test:** a play op `ime hide` (= SDL_StopTextInput as the IME does), then `touch tap edit:ip`, then assert a new probe `text_input_active == 1`. The same for `net_chat_open`.

### W9-04-03 Enter / buttons / leaving the page never hide the keyboard
- **Severity:** minor, but it compounds W9-04-02: users press Back to hide it, which triggers -02.
- **Evidence:** [CONFIRMED-PLAY emu] After ⏎ in the name field, the field lost focus but `mInputShown=true` (emu7.png). Tapping "Join by IP" while the keyboard was up hid it only because the page changed.
- **Root cause:**
  - `set_focus` stops text input only if `!text_was_active_` (mp_pages.inc:321,326).
  - In SDL2, `SDL_IsTextInputActive()` returns the SDL_TEXTINPUT *event state*, which is enabled by default. So the first focus records "was active", and `SDL_StopTextInput()` is never called.
  - The same pattern is at skirmish_page.inc:445-451.
- **Suggested fix:** on Android (`SDL_HasScreenKeyboardSupport()`), always `SDL_StopTextInput()` on unfocus. Keep the desktop behaviour behind `!SDL_HasScreenKeyboardSupport()`.
- **Proposed test:** a unit test of set_focus with a fake text-input flag; the device check `mInputShown=false` after ⏎.

### W9-04-04 Returning to a running MP match: the camera has scrolled into black (phantom mouse at 0,0)
- **Severity:** major.
- **Evidence:** [CONFIRMED-PLAY emu], 3 of 3 runs.
  - Steps: host a LAN game with an Easy AI, Start, wait for the match (r8s.png is normal), then HOME for 15-20 s, then return via the launcher or `am start SetupActivity`.
  - Result: the tactical view is black, a green **mouse** arrow sits at the top-left corner, and the Chat button is drawn at the 1.0 mouse scale instead of the touch 1.5x (r10s.png).
  - One-finger pans did not bring the map back. CMD › MORE › NEXT recentred and the map was intact (r13s.png).
  - Single-player resumes correctly, because the pause menu opens and the next touch clears it (emu42s.png).
- **Original vs clone:** the original is a desktop game and never backgrounds. The clone sends AWAY and keeps the match running with no menu (game_scene_net.inc:330-351, "no pause menu, no autosave").
- **Root cause:**
  - Suspected: on resume, a non-touch pointer event at (0,0) reaches `track_pointer` (game_scene_play.inc:10-18), which sets `mouse_present_ = true`.
  - The edge-scroll then runs every frame towards NW. In SP, the pause menu stops update() until a touch resets the flag.
  - Logcat shows no RenderReset, so it is not texture loss.
- **Suggested fix:**
  - Ignore pointer events with `which != SDL_TOUCH_MOUSEID` whose source is not `SOURCE_MOUSE` on Android, or ignore those that arrive within ~500 ms after AppForeground.
  - Never edge-scroll when the last pointer came from touch.
  - Also, on AppForeground in MP, recentre on the last camera position saved at AppBackground.
- **Proposed test:**
  - A --play script: host solo LAN, `lifecycle background`, `lifecycle foreground`, inject a mouse move at xy 0 0, wait 60, then assert that the `cam` value is unchanged.
  - The emulator steps above.

### W9-04-05 Sliders cannot be dragged with a finger; lists cannot be drag-scrolled
- **Severity:** major (lists: the map list holds dozens of maps and the scroll strip is about 10 dp wide). Minor for sliders, because tap-to-position works.
- **Evidence:** [CONFIRMED-PLAY emu]
  - `input swipe` along the Credits thumb left credits at 10000 (emu19c.png). A tap on the track set 6100 (emu20c.png).
  - Lists: `ShellList` has only the strip hit-test (shell_widgets.inc:177-210), and no Pan / drag / fling handler.
  - [CONFIRMED-PLAY] touch-mp.play (desktop, real TouchGestures): `touch drag` over the credits thumb left credits 10000 (L25 PASS) while `touch tap slider:credits=7500` set 7600 (L29); a `touch drag` over the map list left the frame pixel-identical (t_maps_before vs t_maps_after_drag: no diff) while one tap on `list:down` scrolled it.
- **Original vs clone:**
  - The original uses Windows trackbars and listboxes with a mouse wheel and a draggable thumb.
  - The clone's TouchGestures turns any finger motion over 12 px into `InputEvent::Pan` (touch_gestures.cpp:102-108). `MpPage::handle` ignores Pan (mp_pages.inc:261-265), so `body_drag` never sees a finger drag.
- **Suggested fix:**
  - In MpPage / ShellScene, map a Pan that starts on a ShellList to `top -= dy / item_h`, with fling.
  - Map a Pan that starts on a trackbar to `set_slider(trackbar_pos_from_x)`.
  - Widen the strip to at least 32 logical px on touch.
- **Proposed test:**
  - touch-mp.play: `touch drag` on the credits thumb must change `credits`;
  - a drag on the map list must change the first visible `map:` target.

### W9-04-06 Every MP touch target is 14-36 dp (guideline 48 dp); the touch slop is 12 *window pixels*
- **Severity:** major.
- **Evidence:**
  - [CONFIRMED-DATA] Logical rects: mp_pages.inc:584-599 (rows 19, checks 16), shell_widgets.inc:182-183 (items 16, strip 12), and `btn()` mp_pages.inc:288-290 (42). The scale comes from ShellXform (main.cpp:6203-6208). Emulator screenshots: the slot row is 40 px tall at 420 dpi, about 15 dp (emu15.png).
  - [SUSPECTED] `Config::slop_px = 12` (touch_gestures.hpp:40) is in raw window pixels. That is about 4.6 dp on a 420-dpi phone, against Android's 8 dp touch slop. A slightly rolling fingertip turns a tap into an ignored Pan on every shell page.
- **Original vs clone:** the original 640x480 layout is mouse-sized. The file header claims "touch-friendly ... 19-px row = ~28 px on a 720p panel" (mp_pages.inc:5-8), but that is pixels, not dp.
- **Root cause:** the faithful layout is reused unchanged on touch, and the slop is not scaled by DPI.
- **Suggested fix:**
  - On touch canvases, grow hit rects, not the art: pad every combo / check / list row hit-rect to at least 40 logical px (mainly vertical; the combos already stack 26 px apart, so add a "nearest row" hit rule).
  - Use bigger list `item_h` on touch.
  - Scale `slop_px` with `SDL_GetDisplayDPI` (8 dp).
- **Proposed test:** probe `min_target_dp` (smallest target height times the dp scale) at least 40. The --play `touch tap` gets a jitter variant (`touch tap ... jitter 20`) that must still click.

### W9-04-07 The lobby has no AWAY: 15 s in the background drops a joiner or kills the host's lobby; an online room dies after 60 s
- **Severity:** major (phone hosts typically switch to a messenger to send the room code or IP).
- **Evidence:**
  - [CONFIRMED-DATA] `kSilenceMs = 15000` (mp_lobby.cpp:10). The host drops silent peers (mp_lobby.cpp:363-370) and the client goes `Lost` (mp_lobby.cpp:581-585). `MpPage` has no AppBackground handling: `MpPage::handle` ignores the event, mp_pages.inc:214-278.
  - The Android SDL thread is frozen while backgrounded: logcat `nativePause`, then nothing until `nativeResume`.
  - The server's `IdleTimeout` is 60 s (server/internal/lobby/server.go:123-125) and the client ping is every 10 s (server_link.cpp:9). A host frozen for more than 60 s loses the room code.
- **Original vs clone:** the original runs on a desktop and never freezes. The clone's match side has AWAY (05 §7.2), but the lobby side was never given it.
- **Suggested fix:**
  - Send a lobby `AWAY n s` message (a new lobby body) on AppBackground. Peers then extend `kSilenceMs` for that member to n s and show "X is away (n s)".
  - The returning host re-sends options and re-registers the room (`link_->create` with the same code, if the server keeps it for a grace period).
  - Release the lock and stop the announcer only after the grace period.
- **Proposed test:** a pair run where the host sends `lifecycle background`, waits 25 s, then `lifecycle foreground`. The joiner must stay `mp_state == joined`.

### W9-04-08 Returning to a match after a long background: the phone's own stall timer is stale, so it can kick the peers it was waiting for
- **Severity:** major.
- **Evidence:** [SUSPECTED, code trace]
  - `LockstepSession::poll()` checks `now - wait_start_ >= total_after_` and calls `kick_slowest()` **before** `receive_some()` (lockstep.cpp:605-616).
  - `net_lifecycle` on AppForeground only sends BACK (game_scene_net.inc:341-349). It never calls `session().hold_wait()`.
  - If the phone froze while in `FrameWait`, which is common for the faster peer, a background longer than 40 s on LAN (2400 ticks) or 120 s on Internet makes its first poll kick the slowest peer. That peer is the host, which was alive and holding for the phone's AWAY.
  - `destroy_connection` never tells the kicked peer (lockstep.cpp:221-233). For longer absences the two sides kick each other: a split-brain where each side continues alone. With default options (`ai_takeover = false`, mp_model.hpp:67), the phone can end up "winning" alone.
- **Original vs clone:** in the original, a network split ends the same way, but the original has no background freeze; this is clone-specific.
- **Suggested fix:**
  - On AppForeground, `session().hold_wait()` and treat the first `dialog_init` ticks as a grace period.
  - Have the phone first drain the socket (`receive_some`) before evaluating timeouts.
  - If the phone was away longer than the away grace period plus the stall total, show "You were away too long and were removed" and leave, instead of kicking others.
- **Proposed test:** a pair net-match where the joiner sends `lifecycle background`, freezes its loop for 45 s (a new `set freeze 45s` play op that skips update), then `lifecycle foreground`. Assert that neither side logs `[net] dropping house` / REMOVEPLAYER and that `net_players == 2`.

### W9-04-09 Android Back / an edge-swipe Back leaves the lobby immediately, and the host closes the game for everyone
- **Severity:** major (with gesture navigation, an accidental edge swipe is very common).
- **Evidence:**
  - [CONFIRMED-PLAY emu] Back in the joiner lobby went straight to the Network page (emu14s.png). [CONFIRMED-PLAY] touch-mp.play L43-45: `key escape` in the HOST lobby went straight to mp-lan, no confirm (@back-confirm FAIL).
  - [CONFIRMED-DATA] Esc / Back → `press(back_action())` → `leave()` (mp_pages.inc:244-247, 695, 705-711). The host's `leave()` calls `host_->close()` and `link_->close_room()`.
  - RA2Activity excludes system gestures only on two strips used by the in-game touch bar (RA2Activity.java:51-90), not on shell pages.
- **Original vs clone:** the original Back button in the lobby leaves (mouse only, deliberate). The phone adds an involuntary path.
- **Suggested fix:**
  - When the host has at least one other human, or the client is joined, show `ask(C_LEAVE, "Leave the game?" / "Close the game for everyone?")` before leaving.
  - The `system_back` flag (platform.hpp:22) can restrict this to Android Back.
- **Proposed test:** --play: host lobby with a joined peer, `key escape`, then assert `confirm_open` and `scene == mp-lobby`.

### W9-04-10 The "address to type" can be the phone's mobile-data IP; LAN play on a Wi-Fi without internet may route via cellular
- **Severity:** major.
- **Evidence:**
  - [CONFIRMED-DATA] `where_ = local_ipv4s().front()` (mp_pages.inc:466-467). `local_ipv4s()` sorts the addresses **numerically** (mp_socket.cpp:198-205), so a cellular 10.x / 100.64.x address sorts before Wi-Fi 192.168.x.
  - [SUSPECTED] Android routes unbound sockets over the *default* network. When Wi-Fi has no internet (a LAN-party router, or a phone hotspot without data), cellular stays default and unicast / broadcast to 192.168.x leaves via mobile data. The engine never calls `ConnectivityManager.bindProcessToNetwork` (nothing in RA2Activity.java).
- **Original vs clone:** the original shows no IP (IPX / UDP broadcast only). This is a clone feature.
- **Suggested fix:**
  - Prefer RFC1918 Wi-Fi / Ethernet interfaces (skip `rmnet*`, `ccmni*`, `v4-rmnet*`, `tun*`) and list all candidates.
  - On Android, while LAN pages / the lobby are open, bind the process to the Wi-Fi `Network` (or use `Network.bindSocket`).
  - Show "Mobile data is your default network: LAN games need Wi-Fi" if Wi-Fi is not validated.
- **Proposed test:** a unit test of `local_ipv4s` ordering with fake interfaces (10.x rmnet0 plus 192.168.1.5 wlan0 must pick wlan0). A device check with mobile data on and Wi-Fi without internet.

### W9-04-11 Host checkbox overlap: "AI takes over" sits on "Game Speed", "Cross-platform (exp.)" sits on "Credits"
- **Severity:** minor. It is cosmetic, plus the targets are ambiguous on touch: tapping the "Game Speed" label toggles AI takeover.
- **Evidence:** [CONFIRMED-PLAY emu] emu15.png / emu16.png / emu19c.png. [CONFIRMED-DATA] `check_rect(CK_AIT)` = {170,232,125,16} and `check_rect(CK_XP)` = {170,252,…} (mp_pages.inc:591-594) overlap `slider_label` = {190, 230/256, 105,16} (mp_pages.inc:595-596).
- **Suggested fix:** move the two clone checkboxes under the four originals (30, 312/332), or to the right column under the preview.
- **Proposed test:** probe check that no `check:*` target intersects a slider label rect.

### W9-04-12 In-game chat: the soft keyboard uses a stale input rect
- **Severity:** minor. It is a suspected cover-up.
- **Evidence:**
  - [CONFIRMED-PLAY emu] With the chat open, `mServedView` was the DummyEdit at **287,912-1212,967**. That is the lobby chat edit's rect from before the match; `net_handle` calls `SDL_StartTextInput()` without `SDL_SetTextInputRect` (game_scene_net.inc:419-427).
  - It worked here only because the lobby chat edit happens to be low on the screen too.
  - [SUSPECTED] When the last edit was high (LAN name field, Internet server field), the IME will not pan and the keyboard covers the "Say:" box at `kWinH - h - 70*sc` (game_scene_net.inc:470-483).
- **Suggested fix:** call `set_text_input_rect_logical(box)` before `SDL_StartTextInput()`, or place the chat box at the top of the playfield on touch.
- **Proposed test:** device check of `mServedView` rect against the box rect.

### W9-04-13 The cross-platform guard is visible only to the refused joiner
- **Severity:** minor. Clear text, but discovery is poor.
- **Evidence:** [CONFIRMED-DATA]; the live phone-host x PC-joiner run was inconclusive (pair section).
  - The host refuses `platform != me_.platform && !cross_platform` (mp_lobby.cpp:280). The joiner gets the dialog "The host plays on another platform (Windows / Android). Mixed-platform games need the host's "Cross-platform (experimental)" option." (mp_lobby.cpp:40-42).
  - The host gets nothing: no system line, no status.
  - The LAN beacon has no platform field, so the browser cannot mark the row (mp_pages.inc:1257-1264).
  - The online room list ignores the `plat` it already receives (server_link.cpp:124 vs mp_pages.inc:1422-1427).
  - The x86_64 Android emulator (code 3) is also "another platform" for an arm64 phone (code 2), mp_model.hpp:73-81.
- **Suggested fix:**
  - Add `plat` to the LanBeacon and show "(PC)" / "(Android)" plus "needs cross-platform" in both lists.
  - The host's chat gets "X (Android) tried to join: enable Cross-platform to admit".
  - Warn at Start that mixed platforms are experimental.
- **Proposed test:** the pair test xp1.play (phone host, PC joiner). Assert that the host's `chat_last` mentions the refusal and that `lan_game_plat(0)` is set.

### W9-04-14 The text fields use a generic multi-line keyboard with autocorrect; no number/URI layout; no in-field paste
- **Severity:** minor.
- **Evidence:**
  - [CONFIRMED-DATA] DummyEdit `inputType = TYPE_CLASS_TEXT | TYPE_TEXT_FLAG_MULTI_LINE` (SDLActivity.java:1941-1942), with suggestions on (emu7.png shows the suggestion strip).
  - IP / server / room-code fields get the letter layout (emu10.png).
  - Paste works only through the keyboard's own clipboard chip (commitText). There is no long-press menu, and Ctrl+V is not handled.
- **Suggested fix:**
  - A JNI hint per field, e.g. `SDL_HINT`-like `ra2_ime_kind`: `TYPE_CLASS_TEXT|TYPE_TEXT_VARIATION_URI` for the server / IP fields, `TYPE_TEXT_FLAG_CAP_CHARACTERS|NO_SUGGESTIONS` for the room code, NO_SUGGESTIONS for the name.
  - A "Paste" button next to the server / IP / code fields (`SDL_GetClipboardText`).
- **Proposed test:** device check `dumpsys input_method` `inputType` per field.

### W9-04-15 Small UX issues
All minor or cosmetic.
- (a) A joiner that never reached the host gets the dialog "Connection to the host was lost." while the chat says "No answer from the host." (mp_lobby.cpp:560-565 sets `RejectReason::Lost`). The status line keeps "Contacting the host..." [CONFIRMED-PLAY emu, emu13.png].
- (b) The Internet page says "see docs/SERVER.md", which a phone user cannot open (mp_pages.inc:1320).
- (c) The MP nick (`[Net] Nickname`) and the Skirmish name (`[MultiPlayer] Handle`) diverge: the MP name was "Playerq" while Skirmish still showed "Player" (emu39s.png).
- (d) "Join by IP" is not remembered between visits.
- (e) Hover-only status help (`STT:*`) never appears on touch.
- (f) The map preview in the lobby / Skirmish stayed blank after the app had been backgrounded once (emu39s.png, x0s.png) [CONFIRMED-PLAY emu, cause not traced; possibly the `preview_size_` / `tc.peek` cache in mp_pages.inc:966-978 after a texture trim].
- (g) Long-press (more than 450 ms) on a button is a right click and is silently ignored (touch_gestures.cpp:212, MpPage only takes Left). A slow tap does nothing [CONFIRMED-PLAY touch-mp.play L15-17: `touch hold button Back` stayed on mp-lan].
- (h) No ally-only chat on touch. The original has separate all / allies chat keys [SUSPECTED, defer to the chat auditor].
- (i) The in-game Back opens Game Options with Load / Save / Delete Game in an MP match (emu32 / emu31_32.png) [defer to the in-game menu auditor].

### W9-04-16 Wi-Fi ↔ mobile switch, battery saver, process kill
- **Severity:** minor. These are documented limits.
- **Evidence:** [SUSPECTED, code]
  - **Online match:** the phone's new source address is refused by the tunnel until the old mapping has been idle for `Timeout` (30 s default, server/internal/tunnel/tunnel.go:76-77, 215-221). That is about a 30-s freeze, still under the 120-s Internet stall total, so it recovers.
  - **LAN match:** dead.
  - **ServerLink TCP:** dies; the status reads "Lost the connection to the server (the game lobby continues)".
  - **Battery saver / Doze:** the app is frozen in the background anyway (SDL blocks the native thread). The foreground keeps the screen on (KEEP_SCREEN_ON verified in `dumpsys window`). So battery saver adds nothing beyond W9-04-07/-08.
  - **Process killed (low memory):** an MP match deliberately writes no autosave (game_scene_net.inc:339). Others see AWAY 90 s, then the reconnect dialog, then the kick at about 130 s (LAN) or about 210 s (Internet). The house resigns unless "AI takes over" is on, which is off by default. Known limit: no rejoin.
- **Suggested fix:** show the phone user, on the next launch, "You left a network game; it continued without you". Consider defaulting "AI takes over" on when any phone is in the lobby.

---

## What other players see (lifecycle)

| Phone event | Lobby (host or joiner) | Match |
|---|---|---|
| Home / app switch / call answered / screen off | Frozen. After 15 s: a joiner is dropped ("timed out"), or a host loses every joiner ("Connection to the host was lost"). An online host loses the room after 60 s. (W9-04-07) | AWAY is sent (verified in logcat). Others see "X paused the game (app in the background)" and a 90-s countdown; the match stalls for everyone. Then "Waiting for X (n s)" with Drop / Quit after 7 s (LAN) or 12 s (Internet), and the kick at about 40 s / 120 s more. |
| Return within the window | Lobby continues if under 15 s. | BACK is sent. "X is back." The phone camera scrolls into black (W9-04-04). The phone may kick others if it was gone longer than 40 s (LAN) (W9-04-08). |
| Rotation 180° | handled in place (sensorLandscape + configChanges) | same |
| Portrait | not offered (locked landscape) | same |
| Low memory (foreground) | texture trim only | texture trim only |
| Killed | gone, no rejoin | house resigned or AI (if enabled), no rejoin |

## Pair test (phone host x PC joiner)

Attempted under the net lock (08:41-08:43): emulator (Android x86_64, platform 3) hosting a LAN lobby, desktop joiner (platform 1) to 127.0.0.1:50020 via `adb emu redir add udp:50020:50020`; scripts xp1.play / xp2.play in D:\ra2-re\w9mp-04-out.
- **Result: inconclusive.** Both runs ended `mp_state == lost` after the 15 join queries ("No answer from the host"): the emulator's UDP redirect did not carry the host's replies back to the joiner's ephemeral port, so neither the Platform refusal (xp1) nor the lobby-background drop (xp2, emulator HOME for 25 s) could be observed live.
- W9-04-07 and W9-04-13 therefore rest on code evidence. A real test needs two devices on one Wi-Fi (or two emulators with `-net` bridging): the phone host refuses the PC with the reject_text Platform dialog; with Cross-platform on, phone HOME for >15 s drops the PC joiner.

## Verified OK

- **Soft keyboard appears** on the first tap of every MP edit: name, IP, lobby chat, and the in-game Chat button. `mInputShown=true`, LatinIME [CONFIRMED-PLAY emu].
- **`adjustPan` + `set_text_input_rect_logical`** keep the bottom fields (Join by IP, lobby chat) visible above the keyboard (emu10.png, emu16.png). Taps on the panned window still hit the right widget: "Join by IP" was tapped at its panned position.
- **Lobby chat with ⏎:**
  - the message is sent ("Playerq: hi");
  - the keyboard stays open for the next line;
  - the chat log stays visible above the keyboard (emu16.png).
- **In-game chat on touch:**
  - the "Chat" button sits at the top right of the playfield, drawn at 1.5x on touch;
  - "Say:" box + Send;
  - ⏎ sends and hides the keyboard ("Playerq: gg", emu23s.png);
  - Back closes the box (second Back, see W9-04-02).
- **Combos open and pick on a tap** (kind / country / colour...). **A tap on a slider track** positions it.
- **Android Back** = Esc on every MP page (platform.cpp:163, 200). Root-page Back asks before exit (main.cpp:9414-9421).
- **Lifecycle:**
  - AWAY is sent about 0.5 s after `nativePause` and BACK on `nativeResume` (logcat: `[lifecycle] background in a network game: AWAY sent (90 s)` / `foreground ... BACK sent`);
  - SP resume still pauses in the options menu and autosaves.
- **Manifest:** INTERNET, ACCESS_NETWORK_STATE, ACCESS_WIFI_STATE, CHANGE_WIFI_MULTICAST_STATE (all install-time).
- **MulticastLock:** held only while the LAN browser / lobby is open, dropped in onPause and restored in onResume (RA2Activity.java:92-133; mp_pages.inc:104-105).
- **Screen stays on** in game and in the shell: `fl=KEEP_SCREEN_ON` in `dumpsys window`.
- **Audio focus:** handled for calls (RA2Activity R-163).
- **180° flip and size changes** are handled in place: configChanges, and `Resize` → `relayout_canvas`.
- **Platform tags:** 1 Windows, 2 Android arm64, 3 Android x86_64. Two arm64 phones are compatible by default.
