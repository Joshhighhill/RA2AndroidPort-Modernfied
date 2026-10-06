# Options + Persistence (P8) — Design Spec

**Status:** APPROVED for implementation (controller-dispatched phase)
**Roadmap slot:** `docs/superpowers/ROADMAP-completion.md` **P8 — "Options + persistence"**
("Sound/keyboard/display screens, settings file." Gate: interactive).
**Calibration authority:** `docs/re/options.md` (game.exe hunt, this phase). Every
constant below cites it by §.
**Depends on:** the faithful shell (`ShellScene`/`draw_shell`, `mainmenu_true.md`),
`engine/audio_mixer.{hpp,cpp}` (Mixer), `GameScene` edge-scroll (`main.cpp`),
`ra2assets::decode_pcx`.

## Goal

Replace the greyed `PAGE_OPTIONS` placeholder (Sound/Keyboard/Display stubs that were
GUESSES — the real main-menu Options is ONE combined page, `options.md` §0) with the
faithful **dialog 0xD5** page, wire its controls to the systems that exist, and persist
the original's `RA2.INI` `[Options]/[Video]/[Audio]` keys to a settings file.

## Global constraints

- **All existing byte-identity gates stay bit-identical.** Gates never construct
  `GameScene`/`ShellScene` and never call `run_menu`; the settings file is read ONLY
  by `run_menu` (desktop default path + Android). `--menu-shot` renders with built-in
  defaults and never touches the file. Mixer bus gains default to **1000‰ (identity)**
  and the render path is unchanged when a gain is identity, so `--demo-audio` and any
  mixer test are unaffected.
- Calibrate-first: no guessed layout. Rects = `options.md` §1, skins = §4, semantics
  = §2/§3, consumers = §5.
- Pure logic in a new SDL-free TU `engine/settings.{hpp,cpp}` (registered in both cmake
  lists + the test target), TDD with doctest. SDL/UI in `engine/main.cpp`.
- No assets committed. Scoped test runs only (user rule); the controller runs the full
  suite + gates once at the end.

## Design

### D1. Settings model (`engine/settings.hpp`)
`struct Settings` holds every §3 key with the **ctor defaults** (GameSpeed 3,
Difficulty 1, ScrollMethod 0, ScrollRate 3, AutoScroll yes, DetailLevel 2,
SidebarCameoText yes, UnitActionLines yes, ShowHidden no, ToolTips yes,
ScreenWidth/Height -1, StretchMovies no, SoundVolume 0.7, VoiceVolume 1.0,
ScoreVolume 0.5, IsScoreRepeat no, IsScoreShuffle no, SoundLatency 9, InGameMusic yes).

### D2. INI document (`IniDoc`)
A small, case-preserving, order-preserving INI model (the repo's `ra2assets::Ini`
upper-cases names and is read-only, so it cannot round-trip). Lines are kept verbatim
(comments, blanks, foreign sections/keys); lookup is case-insensitive; `set()` rewrites
an existing key's value in place or appends the key to the section (creating the
section at the end if absent). `serialize(parse(text)) == text` for any LF/CRLF text
whose lines are well-formed (the round-trip test).

### D3. Value semantics (`options.md` §3)
`ww_parse_int` (`$hex`, `...h` hex, else atoi), `ww_parse_bool` (first char
1/T/Y → true, 0/F/N → false, else default), `ww_parse_double` (`%f`, `%` → ×0.01),
writers `%d` / `yes|no` / `%f`. `load_settings(const IniDoc&)` applies the §3 clamps
(Difficulty [0,4], DetailLevel [0,2], volumes `min(v,1.0)`), absent keys keep defaults.
`store_settings(const Settings&, IniDoc&)` writes exactly the §3 save list in save
order (NOT AllowHiResModes/AllowVRAMSidebar, which the original never saves).

### D4. Control math (pure, `options.md` §2/§4)
`volume_to_slider(v) = (int)(v*10.0 + 0.5)` clamped 0..10, `slider_to_volume(p) = p*0.1f`;
`scroll_rate_to_slider(r) = 6 - r`, `slider_to_scroll_rate(p) = 6 - p`;
`detail_to_slider(d) = d != 0`, `slider_to_detail(p) = p ? 2 : 0`;
`trackbar_pos_from_x(x, client_w, readout_w, range)` = the §4 click/drag mapping;
`trackbar_thumb_x(pos, client_w, readout_w, range)`.
`volume_gain_permille(v)` = `clamp(lround(v*1000), 0, 1000)` (linear law, §5).

### D5. Edge scroll (`options.md` §5)
`edge_scroll_distance(rate, accel, multiplier)` with the recovered table
`{448,384,320,256,192,128,64,32,16}`, `idx = max(8-accel, rate+1)` (rate clamped to
[0,6], idx clamped to [1,8]), `(int)(table[idx]*multiplier)`. `GameScene` replaces the
guessed `kPan = 40` by this distance (pixels per 16 ms accel tick, applied per update),
`accel` advancing one step per elapsed 16 ms while an edge/arrow scroll is active and
resetting to 0 when it stops (RE-FALLBACK per §5). Multiplier = rules
`[General] ScrollMultiplier` (default .07). GameScene is interactive-only (no gate).

### D6. Mixer buses
`MixerSound::bus` (`Bus::Sound` default, `Bus::Voice`, `Bus::Score`),
`Mixer::set_bus_gain(Bus, permille)`; the EVA slot mixes on `Bus::Voice`. Render
multiplies a contribution by `gain/1000` only when `gain != 1000`. The menu theme
(`menu:grinder`) registers on `Bus::Score`.

### D7. Persistence
File = `<SDL_GetPrefPath("ra2engine","RA2")>RA2.INI` (Windows `%APPDATA%\ra2engine\RA2\`,
Android the app's internal files dir — writable on both). `run_menu` loads it (absent
→ defaults) into a process-wide `Settings`, applies volumes to the menu mixer, and
publishes a pointer GameScene reads at construction (null in every gate). Leaving the
Options page via "Main Menu" commits (§2 `FUN_00542280`) and saves (read-modify-write
of the existing file so foreign keys survive).

### D8. Options page (`PAGE_OPTIONS` = dialog 0xD5)
- Sidebar: row 0 "Keyboard" (greyed — §6 deferral), row 1 "Network" (greyed — no
  netcode), row 5 "Main Menu" (live). Header "Options".
- Left screen: static MNSCRNS backdrop (no CRT movie on 0xD5, §1).
- Controls at the §1 logical rects, drawn with the §4 skins: section headers + labels
  (12metfnt, yellow tint), trackbars (red groove frame, `trakgrip.pcx` thumb,
  `trofl/trofm/trofr.pcx` readout with the number for the 3 volume sliders),
  checkboxes (`cue_i.pcx` / `cce_i.pcx` + label at +26 px), resolution combobox
  (red frame + `"<w> x <h> x 16"` of the actual window, inert).
- Interaction: click/drag on a trackbar uses D4's mapping; checkbox click toggles;
  volume drags apply live to the menu mixer (Score bus audible immediately).
- All PCX/fonts null-guarded (partial install → controls drawn as plain red frames).

## Out of scope / deferrals
Keyboard customization page 0xA3 (no remappable command table yet); IPX/Network page
0xD7; resolution switching (fixed-window/Android port); in-game options 0xBBB and its
GameSpeed slider (no in-game options menu, no game-speed system); consumers for
DetailLevel/Difficulty/TargetLines/ShowHidden/ToolTips (persisted only); slider tick
and volume preview sounds; tooltip status line; page power-on transitions.

## Testing
`tests/test_settings.cpp` (new): IniDoc round-trip byte-identity (CRLF + LF, comments,
foreign sections), defaults == ctor, store→load round-trip, WW parse semantics, clamps,
save order + foreign-key preservation, slider/scroll math. `tests/test_audio_mixer.cpp`:
bus gain identity + scaling. Manual: `--menu-shot` options PNG eyeball.
