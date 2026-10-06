# RA2 Options screen + settings persistence — RE (P8 calibration authority)

Scope: **original Red Alert 2** `game.exe` (v1.006; the same binary `mainmenu_true.md`
traced). Every claim below was read out of `game.exe` (Ghidra project `D:\ra2-re`,
headless, scripts `D:\ra2-re\scripts\options_hunt1..13.py` + the pefile dialog dumper
`options_dump_dialogs.py`; dumps `D:\ra2-re\out\opt*_*.c`, logs `opt_hunt*.log`,
full dialog dump `opt_all_dialogs.txt`) or out of this install's data files.

Tags: **[CONFIRMED]** = read directly from code/resources/data; **[ANCHORED]** = derived
from confirmed data by a deterministic rule (e.g. `MapDialogRect`); **[RE-FALLBACK]** =
inference where the binary was not fully pinned (always says what was assumed).

---

## §0 Executive summary

- The main-menu **Options** button (`0x55C`, code 5) runs `FUN_00542460`, which shows
  **ONE page: dialog `0xD5`** ("Options", DLGPROC `0x00542590`). It is a single
  combined screen with four sections (Display / Game / UI / Audio) — there are **no**
  separate Sound / Display sub-pages reachable from the main menu. The two side
  buttons open the **Keyboard** page (`0xA3`, via `FUN_005da360`) and the **IPX
  network** page (`0xD7`, via `FUN_0066f3a0`). [CONFIRMED]
- Dialogs `0xD6` ("Sound Options") and `0xD8` ("Display Options" resolution list) exist
  as resources but are **not** created by the main-menu path (`0xD6` is the in-game
  sound page; `0xD8` is the front-end mode picker). The in-game options page `0xBBB`
  carries the **GameSpeed** slider; the main-menu page `0xD5` has **no game-speed
  control**. [CONFIRMED: `FUN_00542460` creates only `0xD5` (MOV ECX,0xD5 @0x542498);
  the only other `0xD5` immediate is in `FUN_005606d0`]
- Settings live in **`RA2.INI`**, sections **`[Options]`, `[Video]`, `[Audio]`**
  (+ `[Network]`), loaded by `FUN_005d8b00` and saved by `FUN_005d91b0` into the
  global `OptionsClass` at `0x00A40B18`, whose constructor `0x005D8840` holds the
  **defaults**. The options page saves the file on exit (`FUN_00542460` tail calls
  `FUN_005d91b0`). [CONFIRMED]

---

## §1 Page `0xD5` — exact layout (640x480 shell, MapDialogRect 6x13 base units)

Dialog 426x295 DLU (same shell skeleton as every page — `mainmenu_true.md` §1/§7).
Pixel rects = `x*6/4, y*13/8` [ANCHORED]. CSF strings read from `ra2.csf` [CONFIRMED].

| id | class | px rect (x,y,w,h) | text key → CSF | notes |
|---|---|---|---|---|
| 0x694 | STATIC (header) | (477,1,162,16) | `GUI:OptionsMenu` → "Options" | animated gold header (shell) |
| 0x5CE | BUTTON (sidebar) | template (475,198,162,37) → grid row 0 | `GUI:Keyboard` → "Keyboard" | opens dlg 0xA3 |
| 0x5CD | BUTTON (sidebar) | template (475,242,162,37) → grid row 1 | `GUI:Network` → "Network" | opens dlg 0xD7 (IPX) |
| 0x686 | BUTTON (sidebar) | template (477,417,162,37) → grid row 5 | `GUI:MainMenu` → "Main Menu" | exit code 0x5CB → back to main |
| 0x6EE | STATIC | (15,16,210,16) | `GUI:DisplayOptions` → "Display Options" | section header |
| 0x6EF | STATIC | (15,121,447,16) | `GUI:GameOptions` → "Game Options" | section header |
| 0x6F0 | STATIC | (15,227,447,16) | `GUI:UIOptions` → "UI Options" | section header |
| 0x6F1 | STATIC | (15,349,447,16) | `GUI:AudioOptions` → "Audio Options" | section header |
| — | STATIC | (37,48,90,16) | `GUI:VisualDetails` → "Visual Details" | left label |
| 0x673 | STATIC right | (127,48,90,16) | `GUI:HigherDetail` → "Higher" | right-aligned end label |
| 0x52B | trackbar | (37,73,180,21) | — | **Visual Details**, range 0..1, no readout |
| 0x6F2 | STATIC | (255,48,180,16) | `GUI:SetResolution` → "Set Game Resolution" | |
| 0x6ED | COMBOBOX | (255,73,180,120 dropped) | items `"%d x %d x 16"` | resolution picker |
| — | STATIC | (37,154,90,16) | `GUI:Difficulty` → "Difficulty" | |
| 0x670 | STATIC right | (127,154,90,16) | `GUI:Harder` → "Harder" | |
| 0x50F | trackbar | (37,178,180,21) | — | **Difficulty**, range 0..2, no readout |
| 0x602 | checkbox | (37,260,195,16) | `GUI:Tooltips` → "Tooltips" | |
| 0x601 | checkbox | (37,284,195,16) | `GUI:TargetLines` → "Target Lines" | |
| 0x604 | checkbox | (37,308,195,16) | `GUI:ShowHidden` → "See Hidden Objects" | |
| 0x603 | checkbox | (256,256,195,16) | `GUI:AssumeNoObserve` | style 0x48008003 = **NOT visible** (no CSF string either) |
| — | STATIC | (255,260,90,16) | `GUI:ScrollRate` → "Scroll Rate" | |
| 0x672 | STATIC right | (345,260,90,16) | `GUI:Faster` → "Faster" | |
| 0x52A | trackbar | (255,284,180,21) | — | **Scroll Rate**, range 0..6, no readout |
| — | STATIC center | (37,381,127,16) | `GUI:MusicVolume` → "Music Volume" | |
| 0x52F | trackbar | (37,406,127,21) | — | **Score** volume, 0..10, numeric readout |
| — | STATIC center | (183,381,127,16) | `GUI:SoundVolume` → "Sound Volume" | |
| 0x532 | trackbar | (183,406,127,21) | — | **Sound** volume, 0..10, readout |
| — | STATIC center | (330,381,127,16) | `GUI:VoiceVolume` → "Voice Volume" | |
| 0x536 | trackbar | (330,406,127,21) | — | **Voice** volume, 0..10, readout |
| 0x695 | STATIC | (15,458,454,19) | `GUI:Blank` | status/tooltip line |
| 0x71C | STATIC | (508,47,91,53) | — | SDWRNANM monitor (every shell page) |

There is **no CRT movie control (0x71A)** on `0xD5`, so the left screen area shows the
static shell backdrop (MNSCRNS) under the controls (`mainmenu_true.md` §0: "for
sub-screens, the static backdrop is MNSCRNS") [ANCHORED].

Tooltips (`STT:` keys, status line 0x695) [CONFIRMED CSF]: `STT:MainOptSliderVisual`
"Customize in-game graphical effects.", `STT:MainOptComboModes`, `STT:MainOptSliderDifficulty`,
`STT:MainOptCBoxTooltips`, `STT:MainOptCBoxTargetLines`, `STT:MainOptCBoxHidden`,
`STT:MainOptSliderScroll` "Controls the scroll speed of the mouse.",
`STT:MainOptSliderMusic`, `STT:MainOptSliderSound` "Controls the sound effects volume
level.", `STT:MainOptSliderVoice`, `STT:MainOptButtonKeyboard`, `STT:MainOptButtonNetwork`,
`STT:MainOptButtonBack` "Return to the Main Menu.".

---

## §2 DLGPROC `0x00542590` — control ↔ setting mapping [CONFIRMED]

WM_INITDIALOG-equivalent (shell msg `0x497`):

| control | TBM_SETRANGE (0x406) | initial TBM_SETPOS (0x405) |
|---|---|---|
| 0x52B Visual | `MAKELONG(0,1)` | `DetailLevel != 0` |
| 0x50F Difficulty | `MAKELONG(0,2)` | `Difficulty` |
| 0x52A Scroll | `MAKELONG(0,6)` | `6 - ScrollRate` |
| 0x52F/0x532/0x536 volumes | `MAKELONG(0,10)` | `ftol(vol * 10.0 + 0.5)` (consts `0x79D130`=10.0, `0x79A728`=0.5) |

- Checkboxes: BM_SETCHECK from `UnitActionLines` (0x601), `ShowHidden` (0x604), `ToolTips` (0x602).
- Sliders 0x52B/0x50F/0x52A get skin msg `0x4AC(0,0)` = **no numeric readout**; the
  volume sliders get `0x4AE(0,0)` = **mute the slider tick sound** and keep the default
  **readout ON** (see §4).
- Volume sliders are `EnableWindow(audio_present)` (`FUN_00406f30`: sound device open).
- Live: on `WM_HSCROLL` with `TB_THUMBTRACK` (5) after init, the volume slider calls the
  matching setter with `pos * 0.1` and `preview=1` (`FUN_005d8980/89f0/8a70`); the sound
  and voice setters then play a preview sample (`FUN_00713db0`, Rules+0x630 sound).
  Dragging Visual/Difficulty/Scroll only refreshes their end-labels.
- Commit on exit (`FUN_00542280`, runs for every exit path of the page):
  - `DetailLevel = (pos != 0) ? 2 : 0` (so a saved `DetailLevel=1` round-trips to 2)
  - `Difficulty = pos`; `UnitActionLines/ShowHidden/ToolTips = BM_GETCHECK == 1`
  - `ScrollRate = 6 - pos`
  - `ScoreVolume = pos*0.1`, `SoundVolume = pos*0.1`, `VoiceVolume = pos*0.1`
- Buttons: `0x686` → exit code `0x5CB` (back to main menu); `0x5CD`/`0x5CE` → exit with
  their own id; `FUN_00542460` loops: `0x5CD` → IPX page `0xD7` then re-shows `0xD5`;
  `0x5CE` → keyboard page `0xA3` then re-shows `0xD5`; anything else → **save RA2.INI**
  (`FUN_005d91b0`) and return. [CONFIRMED `opt_entry_542460.c`]
- Resolution combobox (`0x6ED`): lists modes from `FUN_004962a0`; without
  `AllowHiResModes` only 640x480 / 800x600 / 1024x768 are listed; item text
  `"%d x %d x 16"`; a change pops a confirm dialog and switches the video mode. [CONFIRMED]

---

## §3 OptionsClass — keys, defaults, load clamps, file format [CONFIRMED]

Object `0x00A40B18`; ctor `0x005D8840` (raw disasm in `opt_hunt3.log`); load
`FUN_005d8b00`; save `FUN_005d91b0`. Defaults are what an absent key/file yields.

| Section | Key | +off | type | ctor default | load clamp |
|---|---|---|---|---|---|
| Options | `GameSpeed` | +0x00 | int | **3** | — |
| Options | `Difficulty` | +0x04 | int | **1** | `[0,4]` (`CMP EAX,4; JL; MOV EAX,4` then `x<=0 → 0`) |
| Options | `ScrollMethod` | +0x08 | int | **0** | — |
| Options | `ScrollRate` | +0x0C | int | **3** | — |
| Options | `AutoScroll` | +0x10 | bool | **yes** | — |
| Options | `DetailLevel` | +0x14 | int | **2** | `[0,2]` |
| Options | `SidebarCameoText` | +0x19 | bool | **yes** | — |
| Options | `UnitActionLines` | +0x1A | bool | **yes** | — |
| Options | `ShowHidden` | +0x1B | bool | **no** | — |
| Options | `ToolTips` | +0x1C | bool | **yes** | — |
| Video | `ScreenWidth` | +0x20 | int | **-1** | — |
| Video | `ScreenHeight` | +0x24 | int | **-1** | — |
| Video | `StretchMovies` | +0x28 | bool | **no** | AND'd with a capability flag |
| Video | `AllowHiResModes` | +0x29 | bool | **no** | (load only, never saved) |
| Video | `AllowVRAMSidebar` | +0x2A | bool | **no** | (load only, never saved) |
| Audio | `SoundVolume` | +0x2C | float | **0.7** (`0x3F333333`) | `min(v, 1.0)` (no lower clamp) |
| Audio | `VoiceVolume` | +0x30 | float | **1.0** | `min(v, 1.0)` |
| Audio | `ScoreVolume` | +0x34 | float | **0.5** | `min(v, 1.0)` |
| Audio | `IsScoreRepeat` | +0x38 | bool | **no** | — |
| Audio | `InGameMusic` | +0x39 | bool | **yes** | — |
| Audio | `IsScoreShuffle` | +0x3A | bool | **no** | — |
| Audio | `SoundLatency` | +0x3C | short | **9** | — |

(`[Video] AllowModeToggle` is read into a separate global; `[Network]
Socket/NetCard/DestNet/NetID` are network-only and out of scope.)

**Save order** (`FUN_005d91b0`, written in this order): Options: GameSpeed, Difficulty,
ScrollMethod, ScrollRate, AutoScroll, DetailLevel, SidebarCameoText, UnitActionLines,
ShowHidden, ToolTips; Video: ScreenWidth, ScreenHeight, StretchMovies; Audio:
SoundVolume, VoiceVolume, ScoreVolume, IsScoreRepeat, IsScoreShuffle, SoundLatency,
InGameMusic; then Network. The save **rewrites only these keys** inside the existing
RA2.INI (the same file also holds `[MultiPlayer]`, `[Skirmish]`, `[LAN]`, ... — the
install's RA2.INI shows them), so every other section/key is preserved.

**INIClass value semantics** (Westwood INIClass primitives, [CONFIRMED]):
- write int `FUN_0050d6e0`: `"%d"`; write bool `FUN_0050f380`: `"yes"` / `"no"`;
  write double `FUN_0050e540`: `"%f"` (→ `0.700000`, matching the install's RA2.INI).
- read int `FUN_0050d7f0`: value starting with `$` → hex (`$%x`); value whose last
  char lowercases to `h` → hex (`%xh`); else `atoi`.
- read bool `FUN_0050f410`: `toupper(first char)` ∈ {`1`,`T`,`Y`} → true,
  ∈ {`0`,`F`,`N`} → false, anything else → the default.
- read double `FUN_0050e3a0`: `sscanf("%f")`; if the string contains `%`, result ×0.01
  (`0x79C518`).
- Section/key names are looked up through a name CRC (`FUN_00493fc0`); the repo's
  existing reader treats RA2 INIs as case-insensitive (`include/ra2assets/ini.hpp`)
  — kept. [ANCHORED]

---

## §4 Control skins (owner-drawn by the shell engine) [CONFIRMED]

Class dispatch: `FUN_005ed180` subclasses each control by class name.

- **Trackbar** → proc `0x005FB270`:
  - Optional numeric readout box at the right, `0x32` = **50 px** wide, drawn from
    `local.mix` `trofl.pcx` (8x24) + `trofm.pcx` (114x24, stretched to fill) +
    `trofr.pcx` (10x24); the value text (the integer position) is drawn into the
    rect `(right-49, top, right, bottom)` with shell-text flags 5 (`FUN_005fe020`,
    12-px shell font, yellow; disabled = dark red) [CONFIRMED rect/colour;
    flag-5 = centered per `mainmenu_true.md` §7.2 ANCHORED].
  - Track usable width = `client_w - readout_w - 13`; thumb = `trakgrip.pcx` (12x22)
    at `x = 1 + usable*pos/range`, 12 px wide.
  - Groove frame: two `FUN_005fdb20` rect outlines in the control line colour.
  - Mouse: `WM_LBUTTONDOWN` on the bottom-18px band outside the thumb jumps to
    `pos = clamp(((x-6) - 1) * (range+1) / usable, 0, range)`; on the thumb it starts a
    drag (same mapping on `WM_MOUSEMOVE`). Every change notifies the parent with
    `WM_HSCROLL(TB_THUMBTRACK=5)`; unless muted by `0x4AE`, a tick sound plays.
- **Checkbox** (`BS_AUTOCHECKBOX`) → proc `0x005F3CF0`: an 18x18 PCX at the control
  origin — `cue_i.pcx` unchecked, `cce_i.pcx` checked (`"c%ce_i.pcx"` with `u`/`c`);
  `cce_ir/cce_il` pressed variants; label text box starts at `x + 0x1A` (26 px),
  yellow, shell-text flags 4 [CONFIRMED offset/colour; RE-FALLBACK: drawn
  left-aligned and vertically centred in the 16-px row].
- **Combobox** → proc `0x005F4BA0` (lines + text, no PCX).
- **Colours** (set in `FUN_005ed180`, Win32 COLORREF 0x00BBGGRR): control line
  `0x0000FF` = **red (255,0,0)**; control text `0x00FFFF` = **yellow (255,255,0)**;
  disabled text/line `0x00009F` = **dark red (159,0,0)**.
- All four PCX sets are present in this install (`ra2.mix/local.mix`): `trakgrip.pcx`
  1198 B, `trofl/trofm/trofr.pcx`, `cue_i/cce_i/cce_ir/cce_il.pcx` (all 18x18, 8bpp,
  own palette). [CONFIRMED via ra2extract]
- Section-header statics (0x6EE..0x6F1) and the plain labels are drawn by the shell
  static skin in the shell text engine; exact face/colour not separately traced —
  [RE-FALLBACK: draw them with the same 12metfnt face as the shell header, labels in
  the control yellow].

---

## §5 What each setting drives in the game [CONFIRMED unless tagged]

### Volumes → three mixer buses
Each setter stores the float, then sets a bus volume `ftol(vol * 16384.0)`
(`0x7A77AC`) via `FUN_00407080` (`bus->vol = v << 16`):
- `SoundVolume` → bus `DAT_00831458` = the **sound-effect voice pool** (`FUN_00405b60`
  stamps it on every allocated SFX voice — unit responses/weapons/UI all go here).
- `VoiceVolume` → bus `DAT_00831450` + speech volume `DAT_007f9bd4 = ftol(v*255)`
  (`FUN_00715ef0`), consumed by the **speech (EVA/VOX) streamer** `0x7156xx` and movie
  audio streams (`FUN_00411d80`). [CONFIRMED consumers; "EVA = voice bus" ANCHORED]
- `ScoreVolume` → bus `DAT_00831454` + theme player `0xA35D78`+0xC `= ftol(v*255)`
  (`FUN_006e8590`) = the **music** (themes, incl. the shell's `GRINDER` intro theme).
- Gain law: linear in the stored float (fixed point ×16384) [CONFIRMED].

### ScrollRate → edge-scroll speed (`FUN_00669ec0` @0x66A0B2..0x66A1A0)
- Distance table `int[9]` at `0x7F2BE8` = **{448, 384, 320, 256, 192, 128, 64, 32, 16}**.
- Per scroll step: `idx = 8 - accel`; if `idx < ScrollRate + 1` then `idx = ScrollRate + 1`
  (and `accel` is pinned to `8 - idx`); `distance = ftol(table[idx] * ScrollMultiplier)`
  where `ScrollMultiplier` = Rules+0x4D8 = `[General] ScrollMultiplier` (loader
  @0x645397) = **.07** in both rules.ini and rulesmd.ini.
- `accel` increments by 1 each time the scroll timer expires (`FUN_0069a430` =
  `timeGetTime() >> 4`, i.e. 16 ms ticks; period 1 tick) while still scrolling — so a
  scroll **starts slow (16·.07 = 1)** and accelerates to the rate's cap:
  rate 0 → 384·.07 = **26**, rate 3 (default) → 192·.07 = **13**, rate 6 → 32·.07 = **2**.
- A modifier branch (`FUN_00532890(2)`) bumps idx by one within [4,8] (slower) —
  [RE-FALLBACK: not wired; it is a held-key variant].
- The distance unit is passed to the tactical view's `Scroll_Map` (vtbl+0xA4);
  [RE-FALLBACK: treated as screen pixels per 16 ms tick; the accel reset point when
  scrolling stops was not traced — reset to 0 when the pointer leaves the edge].
- `ScrollMethod` 0/1/2 switches the right-drag pan path (`FUN_0066a7a0`,
  `12.0/(ScrollRate+1)` scaling) — not on page 0xD5, persisted only.

### Others (persisted; no consumer in the clone yet)
`DetailLevel`, `Difficulty` (campaign difficulty), `UnitActionLines` (target lines),
`ShowHidden`, `ToolTips`, `SidebarCameoText`, `AutoScroll`, `ScrollMethod`,
`GameSpeed` (in-game page 0xBBB only), `[Video]` keys, `IsScoreRepeat/Shuffle`,
`InGameMusic`, `SoundLatency`.

---

## §6 Keyboard page `0xA3` (reachable, out of P8 scope)

DLGPROC `0x005D97A0` (created by `FUN_005da360`, MOV ECX,0xA3). Layout: header
`GUI:KeyboardOptions`, "Customize Keyboard", Category combobox 0x4C7 (12,81,207,..),
Commands listbox 0x4C8 (231,79,219,178), Description group 0x6E9, "Current Hotkey",
hotkey edit 0x4CD (13,344,186,22), Assign 0x4CF, Reset All 0x4D0, "Currently assigned
to", Back 0x686 (row 5). Bindings persist in `Keyboard.ini [Hotkey] <Command>=<VK
code>` (install copy: `CenterView=12`, `Options=27`, `DeployObject=68`, `TeamSelect_1=49`,
...). The clone has no remappable command table (hotkeys are fixed in `GameScene`), so
this page is a deferral.

## §7 Clone decisions (for the P8 spec)

1. Persist exactly the §3 keys with the §3 value semantics, in an `RA2.INI` in the
   platform's writable app-data dir (never the install dir), preserving foreign
   sections/keys verbatim.
2. Gates never read the file; engine-side gain defaults are identity (full volume),
   applied ONLY on the interactive path after the file is loaded.
3. Render page 0xD5 with the §1 rects, §4 PCX skins/colours; wire volumes to three
   mixer buses (§5), ScrollRate to the recovered edge-scroll table; persist the rest.
4. Keyboard (0xA3) and Network (0xD7) side buttons drawn greyed (deferred), the
   resolution combobox drawn showing the current mode but inert (fixed-window port).
