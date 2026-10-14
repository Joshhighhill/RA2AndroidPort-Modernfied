# RA2 Main Menu — TRUE design (authoritative, from game.exe + assets)

Scope: **original Red Alert 2** (`game.exe` v1.006, `ra2.mix`; ra2md/YR ignored).
This document supersedes `mainmenu.md` §1 (which wrongly pinned the palette and
claimed the menu is "not an animated movie").

Tags: **[CONFIRMED]** = read directly from game.exe code/resources or rendered from
this install's assets; **[ANCHORED]** = derived from confirmed data by a standard,
deterministic rule (e.g. Win32 `MapDialogRect`); **[RE-FALLBACK]** = inference where
the binary/data could not be fully pinned.

Mockup: `menu_TRUEDESIGN.png` (1024x768; 640x480 master `menu_TRUEDESIGN_640.png`)
in the session scratchpad, composed **only** from the recovered assets/coords below.

---

## 0. Executive summary — what the menu actually is

The RA2 front-end shell is a stack of **Win32 dialogs** (real `RT_DIALOG` resources
inside game.exe), skinned by a custom owner-draw engine and composited over a
**looping VQA movie** that plays inside the left "CRT screen" area:

- Main menu = **dialog resource 0xE2** (426x295 DLU), created via
  `CreateDialogIndirectParamA` in `FUN_005ff610` (template fetched from game.exe's
  own resources by `FUN_004954e0` / `FindResourceA`). The main-menu modal loop is
  **`FUN_00516f00`**, its DLGPROC is **`0x005171D0`**. [CONFIRMED — call at
  0x516f45: `MOV ECX,0xE2 / MOV EDX,0x5171d0 / CALL FUN_005ff610`]
- The left screen plays **`Ra2ts_s`** (640x480 mode) or **`Ra2ts_l`** (>=800x600)
  — VQA movie names sent to the movie control (ctrl id **0x71A**) with message
  0x4E4; control is centered at `((W-800)/2,(H-600)/2)` when the screen is larger
  than 800x600. [CONFIRMED — `FUN_00516f00` @0x516fd3..0x517031, strings
  `"Ra2ts_s"`@0x7db5b0, `"Ra2ts_l"`@0x7db5a8; same logic in `FUN_005115f0`]
  The movie is the animated **red radar sweep + Statue of Liberty + RA2 logo**.
  In THIS install `MOVIES01.MIX`/`MOVIES02.MIX` are 4-byte dummies ("MYTH") — the
  VQA is unavailable, so any clone must ship a static approximation. [CONFIRMED]
- Behind/beneath the movie the shell composites SHP chrome (below). When the
  movie is absent or for sub-screens, the static backdrop is
  **MNSCRNS/MNSCRNL.SHP under SHELL.PAL** — the dark-RED US-East-Coast radar map
  with a beveled CRT frame baked into the SHP. [CONFIRMED render + loader pairing]

Old mockup4's key errors: wrong palette family (fsscrn.pal — dark neutral instead
of shell.pal red), no metal sidebar chrome (SDTP/SDBTNBKGD/SDBTM/LWSCRN pieces),
buttons at guessed size/coords instead of the dialog-template rects, and no
"WARNING" mini-monitor / "Main Menu" header / version line.

---

## 1. Screen geometry (base shell = 640x480)

The shell composes at the game resolution; the two authored sizes are 640x480
("small"/`*S` assets) and 800x600 ("large"/`*L` assets). The right sidebar is a
fixed **168 px** column; the bottom bar is a fixed **32 px** strip:

- 640x480: screen area 472x448 (=640-168 x 480-32)  → `MNSCRNS.SHP` 472x448 ✓
- 800x600: screen area 632x568 (=800-168 x 600-32)  → `MNSCRNL.SHP` 632x568 ✓
- At resolutions >800x600 the 800x600 shell content is centered (movie-control
  centering code above). [CONFIRMED code / ANCHORED for non-movie chrome]

### Composition table (640x480, draw order)

| # | Element | Asset | Palette | Screen rect (x,y,w,h) | Tag |
|---|---|---|---|---|---|
| 1 | CRT screen backdrop (red radar map of US East Coast, bevel frame baked in) | `neutral.mix/mnscrns.shp` (472x448, 1 fr) | `cache.mix/shell.pal` | (0,0,472,448) | [CONFIRMED] |
| 2 | Animated menu movie (radar sweep + Statue of Liberty + RA2 logo), plays over #1 in ctrl 0x71A | `movies01.mix/ra2ts_s.vqa` (`ra2ts_l.vqa` at 800x600) | (VQA self-palettized) | movie ctrl, template (0,0,456,432); movie plays at native VQA size | [CONFIRMED names; file absent in this install] |
| 3 | Bottom bar under screen | `neutral.mix/lwscrns.shp` (472x32; `lwscrnl.shp` 632x32 at 800x600) | `shell.pal` | (0,448,472,32) | [CONFIRMED asset+pal; position ANCHORED by dims] |
| 4 | Sidebar top: metal panel + **mini-monitor showing red "WARNING" screen** | `neutral.mix/sdtp.shp` (168x199, **2 frames**) | `shell.pal` | (472,0,168,199) | [CONFIRMED asset+pal; position ANCHORED] |
| 5 | Sidebar bottom: V-console + cables | `neutral.mix/sdbtm.shp` (168x65) | `shell.pal` | (472,415,168,65) bottom-anchored | [ANCHORED] |
| 6 | Per-button metal slice (behind each red button) | `neutral.mix/sdbtnbkgd.shp` (168x42) | `shell.pal` | x=472, one per button row | [ANCHORED] |
| 7 | Menu buttons (red beveled, 3 states: 0=up,1=hover,2=down) | `local.mix/mnbttn.shp` (126x25, 3 fr) | `local.mix/mainbttn.pal` | 6 rects below, control size 162x37 (SHP stretched) | [CONFIRMED asset+pal; rects ANCHORED; stretch RE-FALLBACK] |
| 8 | Button labels + "Main Menu" header + version (gold metallic) | `12metfnt.fnt` (Westwood FNT) | n/a (палette-indexed glyphs 1..4) | centered in owner rects | [CONFIRMED font present; that the shell uses 12metfnt for these = RE-FALLBACK, visually matched] |
| 9 | Mini-monitor content region (static ctrl 0x71C) | (drawn by engine over SDTP's screen hole) | — | (508,47,91,53) | [ANCHORED from template] |
| 10 | Status text line (ctrl 0x695, `GUI:Blank` on main menu) | text | — | (15,458,454,19) | [ANCHORED] |
| 11 | Version line (ctrl 0x71D; set at runtime via msg 0x497 to "%s %s" incl. version, cf. `GUI:Version`="V6.8 / ") | text | — | (477,461,162,16) | [CONFIRMED behavior; exact string RE-FALLBACK] |

Palette pairings come from the shell asset loaders — the load groups are explicit
in the code: `FUN_006f1b60`/`FUN_006f31b0` load PUDLGBG*/MNBTTN with
**DIALOG.PAL/MAINBTTN.PAL**, then SDBTNANM/SDMPBTN/SDWRNTMP/MNSCRNS/MNSCRNL/SDTP/
SDBTNBKGD/SDBTM/LWSCRNS/LWSCRNL with **SHELL.PAL/SHELL2.PAL/SDBTNANM.PAL**;
`FUN_006f2c40` loads the FS* family with **FSSCRN.PAL**. [CONFIRMED — instruction
listing in `D:\ra2-re\out\menu_hunt4.log`]. The asset-name registry lives at
0x7f86xx–0x7f8814 in .data (dumped in `menu_hunt3.log`).

### Button rects — dialog 0xE2 [CONFIRMED template / ANCHORED px]

Dialog resource 0xE2 (game.exe `.rsrc`), font "MS Sans Serif" 8pt → dialog base
units 6x13, so px = MapDialogRect(dlu): x*6/4, y*13/8. Dialog = 426x295 DLU =
639x479 px (i.e. authored against 640x480).

| Control | id | DLU rect | Pixel rect (x,y,w,h) | CSF label |
|---|---|---|---|---|
| Movie/screen static | 0x71A | (0,0,304,266) | (0,0,456,432) — repositioned/covered at runtime | — |
| "Main Menu" header  | 0x694 | (318,1,108,10) | (477,2,162,16) | `GUI:MainMenu` "Main Menu" |
| Mini-monitor static | 0x71C | (339,29,61,33) | (508,47,92,54) | — |
| **Single Player** | 0x683 | (317,122,108,23) | **(476,198,162,37)** | "Single Player" |
| **Internet** (WWOnline) | 0x684 | (317,149,108,23) | **(476,242,162,37)** | "Internet" |
| **Network** | 0x578 | (317,176,108,23) | **(476,286,162,37)** | "Network" |
| **Movies & Credits** | 0x686 | (317,203,108,23) | **(476,330,162,37)** | "Movies & Credits" |
| **Options** | 0x55C | (318,230,108,23) | **(477,374,162,37)** | "Options" |
| **Exit Game** | 0x3EE | (318,257,108,23) | **(477,418,162,37)** | "Exit Game" |
| Status text | 0x695 | (10,282,303,12) | (15,458,454,19) | `GUI:Blank` |
| Version text | 0x71D | (318,284,108,10) | (477,461,162,16) | set at runtime |

Button pitch = 27 DLU = 43.875 px (~44 px). Buttons are `BS_OWNERDRAW`
(style 0x5000000B); the skin engine (WM_DRAWITEM → `FUN_005fe380`, classifier
`FUN_005e6860`) draws MNBTTN frames into the control rect. MNBTTN.SHP is 126x25
vs control 162x37 ⇒ the engine stretch-blits the frame [RE-FALLBACK — matches
retail screenshots where buttons nearly fill the 168px sidebar].

Cross-checks that anchor the DLU mapping: first button top (198) = SDTP height
(199); status/version rows land inside the 32px bottom bar (448..480); 0x71C
(508..600 x 47..101) lands exactly in SDTP's monitor hole. [ANCHORED]

### 800x600 ("large") layout

Backdrop `MNSCRNL.SHP` (632x568) + `LWSCRNL.SHP` (632x32) + movie `Ra2ts_l`;
sidebar stays 168 px wide at x=632. Button x ≈ 636/637. The vertical
redistribution of the 6 buttons over the taller column was **not** recovered from
the binary (runtime repositioning happens in the WM_INITDIALOG skinners
`FUN_005ecf40`/`FUN_005ed180`/code at `0x5ECB00`, not fully traced) — [RE-FALLBACK]:
keep the same top anchor (button 1 below SDTP at y≈198) and pitch, bottom-anchor
SDBTM at y=535.

---

## 2. Animated vs static

| Element | Animated? | Notes |
|---|---|---|
| Left screen (radar sweep, Statue of Liberty, logo) | **YES — VQA loop** (`ra2ts_s/_l.vqa`) | Not extractable here (movies mixes are dummies). Static approximation: MNSCRNS+SHELL.PAL red radar map, with the RA2 logo emblem (croppable from `local.mix/logo.pcx`, which is the full 640x480 title/loading screen incl. identical chrome) composited on top. |
| Mini "WARNING" monitor (top of sidebar) | 2-frame SHP (`sdtp.shp` frames 0/1) + engine may draw into ctrl 0x71C | Frame 0 shows red WARNING screen with rising graph. |
| Buttons | 3-state SHP (up/hover/down), plus `sdbtnanm.shp` (156x42, **17 frames**, orange fire/sweep, own `sdbtnanm.pal`) | SDBTNANM is loaded with the shell group; exact trigger (hover flame anim) not traced [RE-FALLBACK]. |
| Backdrop map, sidebar metal, bottom bar | Static single-frame SHPs | — |

---

## 3. Page tree (dialog ids, procs, and flow) [CONFIRMED]

Shell state machine = `FUN_005135f0`; main-menu loop `FUN_00516f00` returns a
code set by DLGPROC 0x5171D0 on WM_COMMAND:

| Button (ctrl) | Code | Action in FUN_005135f0 |
|---|---|---|
| Single Player 0x683 | 1 | `FUN_005eab60(1)` → dialog **0x100** (Single Player menu) |
| Internet 0x684 | 2 | WOL path (`DAT_00a3d298=4`, `FUN_00523950`) |
| Network 0x578 | 3 | LAN path (`DAT_00a3d298=3`) |
| Movies & Credits 0x686 | 4 | `FUN_005eab60(...)` → dialog **0x101** |
| Options 0x55C | 5 | `FUN_00542460` → dialogs **0xD5/0xD7** (options) |
| Exit Game 0x3EE | 6 | confirm via `FUN_005b46b0` (dialogs 0x120/0x121), then quit |

All shell pages share the same skeleton (426x295 DLU; movie ctrl 0x71A; header
0x694; status 0x695; right-column buttons 108x23 DLU at x=317/318, y=122+27k;
Back/MainMenu always at y=257):

- **0x100 Single Player** (proc — via FUN_005eab60): New Campaign 0x688 (y122),
  Load Saved Game 0x689 (y149), Skirmish 0x579 (y176), Back→Main 0x686 (y257).
- **0x94 Campaign select** (created in `FUN_005135f0`@0x513bea, proc 0x5147A0):
  Load 0x40E (y122), campaign listbox 0x455, difficulty slider 0x50F, three
  SS_BLACKFRAME art wells 0x6EA/0x6EB/0x6EC (Allied/Soviet/tutorial emblems),
  Back 0x686 (y253).
- **0xB7 Load Mission** (created in `FUN_0053b770`@0x53b8ce, proc 0x53B3D0):
  Load 0x40F, savegame listbox 0x525 (24,50,266x187 DLU), Back 0x686.
- **0x101 Movies & Credits**: Sneak Peeks 0x68D (y122), Play Movies 0x68E
  (y149), View Credits 0x68F (y176), Back 0x686 (y257).
- **0x102 Skirmish setup** (created in `FUN_00683bf0`@0x683c53, proc 0x683D10):
  51 controls — player rows (name edit 0x6A0, country/color combos 0x50B..0x528),
  map preview 0x468 (324,23,96x69 DLU), Start Game 0x617 (y149), Choose Map
  0x5AA (y176), Back 0x5C0 (y257), game-option checkboxes/sliders.
- Tooltips for all of these: dispatcher `FUN_005e22b0` maps (dialogId, ctrlId) →
  `STT:*` CSF keys (dump: `D:\ra2-re\out\m2_MainButton_5E22B0.c`).

Intro/attract: `Title.PCX` is the default `Background=` of the `[Intro]` INI
section read by `FUN_004de380` (movie/slideshow list, `Item%d`, `Theme=`);
`-ATTRACT` cmdline handled near `FUN_00515110`. `title.pcx`/`logo.pcx` are the
full-screen title/loading art with the SAME chrome as the shell (their right
column/bevels match the SDTP/SDBTM silhouette) — logo.pcx = chrome + logo-over-
tanks art, used outside the animated menu. [CONFIRMED assets; usage ANCHORED]

Front-end music: `theme.ini [Themes] 1=INTRO`, `[INTRO] Sound=Grinder Repeat=yes`
→ `theme.mix/grinder.wav` loops on the shell. [CONFIRMED data]

---

## 4. Font note (for faithful labels)

`12metfnt.fnt` (present in this install's mixes; copy in scratchpad/out):
classic Westwood FNT header — u16 fsize; u8 fmt(=2); offsets: glyph-offset block
@0x14 (u16/glyph, **relative to data block @0x45B**), width block @0x1CA
(u8/glyph), height block @0x2A5 (u8 yoff + u8 rows per glyph), font height 0x12
(=16), 219 glyphs indexed by raw ASCII. Glyph data = **8bpp row-major, w*rows
bytes, values 0..4** (0 transparent; 1 shadow, 2 body, 3/4 highlights — engine
remaps; gold ramp matches retail). [CONFIRMED by decoding + rendering]

---

## 5. Ghidra artifacts (project D:\ra2-re, game.exe)

Scripts added: `scripts/menu_hunt3.py` … `menu_hunt9.py`. Key dumps in `out/`:

| Address | Function | Dump |
|---|---|---|
| 0x00516F00 | Main-menu modal loop (creates dlg 0xE2, movie ctrl setup, PENGO cheat parser) | m5_mainloop_516F00.c |
| 0x005171D0 | Main-menu DLGPROC (WM_COMMAND→codes; version text msg 0x497) | m7_mainmenu_dlgproc_5171D0.c |
| 0x005FF610 | Shell dialog creator (template → CreateDialogIndirectParamA) | m4_dlgcreate_5FF610.c |
| 0x004954E0 | Template fetch (FindResourceA/LoadResource from game.exe) | m5_tmplfetch_4954E0.c |
| 0x005FFB00 | Shared shell dialog engine (WM_DRAWITEM→0x5FE380, WM_INITDIALOG skinners) | m8_dlgengine_5FFB00.c |
| 0x005FE380 | Owner-draw dispatch | m9_ownerdraw_5FE380.c |
| 0x005E6860 | Per-dialog "skinned control" classifier (0xE2: 0x683/0x684/0x686/0x578/0x55C; every page: 0x694, 0x71C) | m3_ctrlids_5E6860.c |
| 0x005135F0 | Shell state machine (page flow) | m8_shellsm_5135F0.c |
| 0x006F1B60 / 0x006F31B0 / 0x006F2C40 / 0x006F4B00 | Shell asset loaders (SHP↔PAL groups) | m3_tbluser_*.c + menu_hunt4.log listings |
| 0x005E22B0 | Tooltip (dialogId, ctrlId)→STT dispatcher | m2_MainButton_5E22B0.c |
| 0x005115F0 | Movie-control repositioning + Ra2ts_s/_l selection | m6_moviemsg_5115F0.c |

Dialog templates: dumped/parsed by `scratchpad/dump_dialogs.py` (pefile) — 97
RT_DIALOG resources; shell pages of interest: 0xE2, 0x100, 0x101, 0x102, 0x94,
0xB7, 0xD5/0xD7 (options), 0x120/0x121 (quit confirm).

---

## 6. Clone guidance (static-faithful main menu)

1. 640x480 logical canvas (scale to device): draw MNSCRNS+shell.pal at (0,0);
   LWSCRNS at (0,448); SDTP at (472,0); SDBTM at (472,415); SDBTNBKGD at
   (472, btnY-2) per button; MNBTTN frame (state) stretched to 162x37 at the six
   rects above; 12metfnt gold labels centered; "Main Menu" at (477,2,162,16);
   version bottom-right.
2. The animated screen: until a VQA decoder exists, composite the RA2 logo
   emblem (cropped/sky-keyed from logo.pcx) over the red radar map — this is the
   look of the first frames of ra2ts. Optionally animate a radar sweep
   procedurally [RE-FALLBACK by design].
3. Hover/press = MNBTTN frames 1/2; optional SDBTNANM 17-frame flame overlay.
   **SUPERSEDED by §7.2 — main-menu buttons are SDBTNANM frames 2/3/4, not MNBTTN.**
4. Sub-pages reuse the identical skeleton — only the button column and the
   center-well controls change (see §3 tables).

---

## §7 Video hunt + exact button/animation design (second recovery pass)

This section supersedes §1's button row ("SHP stretched" [RE-FALLBACK]) and
§2's button/monitor rows. Everything below was read from game.exe disassembly
(project D:\ra2-re, scripts `menu_hunt10..13.py`, dumps `out/m10_*`,`m11_*`,
`m12_*`, `menu_hunt13.log`) and verified against this install's assets via
`ra2extract` (which gained `--scanmagic` / `--scanraw` modes for the hunt).

### 7.1 TASK A — the menu VIDEO is definitively absent [CONFIRMED]

Exhaustive magic-byte scan (`FORM`@0 + `WVQA`@8; plus `WVQA/VQFL/VQFR`
anywhere) over every entry of every mix, recursing content-detected nested
mixes (depth 3):

| Container | entries scanned | real VQA |
|---|---|---|
| ra2.mix (117 nested: local/conquer/neutral/cache/isotemp/…) | 5688 | 0 |
| ra2md.mix (96 nested) | 5868 | 0 |
| language.mix / langmd.mix | 670 / 820 | 0 |
| theme.mix / thememd.mix / WDT.MIX / ecache01.mix / expand01.mix | 16/10/126/266/38 | 0 |
| MULTI.MIX / multimd.mix / Maps01 / MAPS02 / mapsmd03 / *.mmx | 97/173/18/18/14/2×7 | 0 |
| coredat1/2.dat, ecameo.dat, aftrmath.dat (MIX-format .dat) | 105/61/42/17 | 0 |
| myth.pak, music.pak (Myth-rip containers, raw byte scan) | whole files | 0 |

All 60 “WVQA/VQFL/VQFR” chunk hits were verified to be 4-char coincidences
inside base64 `[IsoMapPack5]` text of .map entries (e.g. EB3.mmx offset 69583:
`...gB3QpSSAFgD0EEU8gB..477=QQRUyAFVAlVJATtIAkkBVsgB...`). MOVIES01/02.MIX and
MOVMD03.MIX are 4-byte `MYTH` stubs. **`ra2ts_s.vqa`/`ra2ts_l.vqa` do not exist
anywhere in this install** — a faithful clone cannot recover the CRT movie from
this data; ship the §6.2 static composite or source the VQA externally.
What the user remembers as “video” is partly real SHP animation (below).

### 7.2 TASK B — TRUE main-menu button design [CONFIRMED]

The template rect (162x37) and MNBTTN are **not** what the shell draws in the
sidebar. The skinned-button painter (function region 0x5F0400..0x5F093D,
`menu_hunt13.log`; forced decompile `m11_btnanim_user_5F0663.c`) switches on
button class (obj+0xB0):

| class | SHP | palette (call @addr) | normal | hover | pressed | used for |
|---|---|---|---|---|---|---|
| 1 | `neutral.mix/SDBTNANM.SHP` 156x42, 17 fr | `SDBTNANM.PAL` (FUN_006f34c0 @0x5f065a) | **frame 2** (dark-red face in metal slice) | **frame 3** (orange glow) | **frame 4** (yellow-hot) | **main-menu/shell sidebar buttons** |
| 2 | DAT_00ac0ff8 (in-game sidebar button) | sidebar pal (FUN_006f45f0) | 0 | 2 | 1 | in-game options |
| 3 | `local.mix/MNBTTN.SHP` 126x25, 3 fr | `MAINBTTN.PAL` (FUN_006f2250 @0x5f06d5) | **frame 0** (red) | **frame 2** (yellow) | **frame 1** (orange) | **popup-dialog buttons only** |

- Blit is **1:1 at a point** (FUN_0049eea0(shp, frame, &pos, &clip, 0x400,…) —
  position, not a destination rect). **No stretching, ever.**
- The WM_INITDIALOG skinner **resizes each shell button control to exactly
  156x42** = SDBTNANM frame size: `MoveWindow(hwnd, parentRight-0x9C(156),
  gridY, width, height)`, where width is the SDBTNANM header's 16-bit field at
  `+2` (=156) and height its 16-bit field at `+4` (=42)
  (`m10_slotuser_5E8C50.c`; bottom-anchored variant `m10_slotuser_5E8FA0.c`
  for the last/Back/Exit button).
- Sidebar layout is derived from SHP headers in `FUN_006f3e70`
  (`m11_layout_6F3E70.c`): button strip starts at y = SDTP.cy = **199**, row
  pitch = SDBTNBKGD.cy = **42**, row count = (usableH-199)/42 = **6 at
  640x480** (9 at 800x600); buttons flush to the RIGHT screen edge
  (x = right-156, i.e. 484 at 640x480; rows y=199,241,283,325,367,409).
  Template y's are snapped to this grid at runtime — §1's pixel rects
  (pitch 44, x=476) are the *template*, not what is rendered.
- Chrome behind the buttons (`FUN_006f3650`, `m10_slotuser_6F3650.c`): SDTP
  frame 0 at (right-168, 0); SDBTNBKGD frame 0 tiled once per row (x=right-168,
  y=199+42k); on rows without a live button control, **SDBTNANM frame 10** is
  drawn as the “empty socket”; SDBTM frame 0 below the last row (y=451 at
  640x480, clipped to the remaining 29 px); LWSCRNS frame 0 at (0, H-32).
- Label text: drawn over the SHP by `FUN_005fe020(&rect, text, gold565, 5,
  0xC, …)` (shell text engine, centered flag 4, 12-px metal font ⇒ 12METFNT
  family per §4); text box y+1 in normal state, **offset (+2,+5) when
  pressed**; gold color is computed from shell-palette entries
  (DAT_00ac109C/9D path) [CONFIRMED code; exact glyph source 12METFNT stays
  ANCHORED/visual].
- Page enter/leave transition (`FUN_005e5150`): staggered top-to-bottom
  power-on — each row plays a 6-step SDBTNANM sequence (enter uses the
  frame-5..10 band, leave the 11..16 band, 1-frame stagger per row), monitor
  plays SDWRNTMP (168x177, 6 fr), one tick = `Sleep(0x1E)` = **30 ms/frame**.
- Palette pinning (renders in scratchpad): SDBTNANM is correct ONLY under
  `neutral.mix/sdbtnanm.pal` (frame2 dark-red / frame3 orange / frame4 yellow
  = retail look); garbage under shell/shell2. MNBTTN correct only under
  MAINBTTN.PAL (§1 stands).
- **Clone deltas to fix**: we stretched `mnbttn.shp` to 162x37 at the template
  rects — wrong asset (use SDBTNANM f2/f3/f4 @ sdbtnanm.pal), wrong geometry
  (156x42, x=right-156, y=199+42k), wrong state mapping, missing empty-socket
  frame 10, missing 30ms staggered transition, missing pressed text offset.

### 7.3 TASK C — top-right “WARNING” monitor animation [CONFIRMED]

- The animated asset is **`neutral.mix/SDWRNANM.SHP` — 92x53, 91 frames**
  (mix id 674B55D4, 446272 B), drawn in static control **0x71C**
  (template px rect 508,47,92,54 — exact fit). Content: red WARNING screen,
  rising graph, missile-launch flash around frames ~40-50, loops.
- Resolver: `FUN_005e1a80` (xref of string `SDWRNANM.SHP` @0x7e8d50, ptr slot
  0x7e8d3c) returns SDWRNANM for ctrl 0x71C on every shell page id (0xE2,
  0x94, 0x100, 0x101, 0x102, 0xB7, 0xD5/0xD7, …) — dump
  `m10_sdwrnanm_ptruser_5E1A80.c`.
- Palette: **SHELL2.PAL** — the 0x71C init path swaps the default SHELL for
  `FUN_006f34a0()` = SHELL2 (`m9_sub2_5E8130.c` @ the FUN_005e8590(dialog) &&
  ctrl==0x71C branch).
- Cadence: the same init does `SetTimer(hwnd, 0, 1, NULL)` (elapse from
  obj[0x28]=1 ms → Windows clamps to ~10-16 ms); each WM_TIMER tick blits the
  current frame then advances `frame = (frame+1) % count` (fragment
  `m11_blitcaller_5F315A.c`: wrap at obj+0x94 = frame count read from the SHP
  header, notify msg 0x4D8). Net: **91 frames at ~64 Hz ≈ 1.4 s loop**
  (~0.9 s if the 10 ms clamp is honored exactly).
- Base chrome: SDTP has 2 frames — frame 0 = monitor WITH the warning screen
  baked in (this is what `FUN_006f3650` draws), frame 1 = monitor with an
  EMPTY black screen (used with the SDWRNTMP power-on during transitions).
  The 2 frames are NOT alternated as an animation.
- Related: `SDWRNTMP.SHP` (168x177, 6 fr, id 4C49C720) = transition power-on
  of the whole monitor bezel region; drawn by `FUN_005e5150` during page
  changes at 30 ms/frame.
- Header “Main Menu” (ctrl 0x694) is also an animated type-1 control:
  30 ms period, step 1, 8 phases (`FUN_005defb0/5df8e0/5e0010` = 0x1E/1/8)
  — a ~240 ms gold-text shimmer [CONFIRMED params; visual = RE-FALLBACK].

### 7.4 Palette map (final)

| Asset | Palette | Evidence |
|---|---|---|
| MNSCRNS/MNSCRNL, SDTP, SDBTM, LWSCRNS/LWSCRNL, SDMPBTN | SHELL.PAL | FUN_006f3a20/3ac0/3a60 read pal obj 0xac1188 + renders |
| SDBTNBKGD, SDWRNANM | **SHELL2.PAL** | 5e8130 (0x71C→6f34a0); sdbtnbkgd metal matches SDTP grey only under shell2 (shell gives red speckle noise) |
| SDBTNANM (+empty-socket f10) | SDBTNANM.PAL | painter @0x5f065a calls 6f34c0; renders |
| MNBTTN, PUDLGBG* | MAINBTTN.PAL / DIALOG.PAL | painter @0x5f06d5 calls 6f2250; loader 6f1b60 |
| SDWRNTMP | SHELL.PAL (renders correct) | [ANCHORED] |

Key new game.exe addresses: 0x5F0400..0x5F093D skinned-button painter
(class/frame/text); 0x5E8130 per-control skin init (+SetTimer); 0x5E8590
shell-page predicate; 0x5E1A80 (dlg,ctrl)→SHP resolver; 0x5E8C50/0x5E8FA0
button movers (156x42); 0x6F3E70 sidebar layout builder; 0x6F3650 chrome
painter; 0x5E5150 page-transition animator; 0x5FE020 skinned text draw.
Scratchpad renders: `btn_TRUE_normal/hover/pressed/emptyrow.png` (sdbtnanm
f2/3/4/10 @ sdbtnanm.pal), `sdbtnanm_anmpal_*.png`, `btn_correct_0/1/2.png`
(mnbttn @ mainbttn.pal, popup buttons), `sdtp_f0/f1.png`,
`sdwrnanm_shell2_{0,10,22,45,68,80,90}.png`, `sdwrntmp_0..5.png`,
`sdbtnbkgd_shell2.png`, `sdbtm_shell.png`, `lwscrns_shell.png`.

---

## §8 The main-menu CRT: definitive VQA-vs-frame-list verdict (third pass)

Question re-examined: is the 0xE2 main-menu CRT background a multi-frame SHP
frame-list rather than a VQA movie? **NO. It is a VQA movie, traced end-to-end
through the binary; there is no SHP fallback path.** The frame-lists that DO
exist at CRT size are campaign-select / skirmish / transition art (all now
name-identified, below). Scripts `menu_hunt14..19.py`, dumps `out/m14_*..m19_*`.

### 8.1 Full code trace of ctrl 0x71A (the CRT control) [CONFIRMED]

- Template: ctrl 0x71A in dialog 0xE2 is a plain **STATIC** (class atom 0x0082,
  style 0x50000000, DLU rect 0,0,304x266) — parsed from RT_DIALOG (dlgtemplateex).
  It is subclassed by the shell **screen-control proc** in 0x5F2C00..0x5F3C4E
  (same proc family that runs the SDWRNANM monitor anim, §7.3).
- Message dispatch in that proc = two jump tables (raw listing
  `out/m19_listing_5F3300.txt`):
  - msgs 0x498..0x4D3 @0x5F357E via tables 0x5F3C54/0x5F3C68;
  - msgs 0x4D5..0x4F0 @0x5F390D via tables 0x5F3CA4/0x5F3CD4.
  - **0x4E3 -> 0x5F3C0B**: `obj+0x5C = wParam` (loop flag; menu sends 1).
  - **0x4E4 -> 0x5F3AE9**: "play movie named lParam":
    1. destroy old handle at `obj+0x58`, `KillTimer(hwnd, 0x65)`;
    2. rect from FUN_00735690;
    3. **`FUN_005A1D30(name, surface=DAT_00839958)` — MovieHandle factory**:
       - extension `.bik` (strrchr vs ".bik"@0x7E329C) → **BinkMovieHandle**
         (vft 0x7A6684) via BinkOpen FUN_0042FEA0; logs
         "Play_Movie() as Bink!" @0x7E3284;
       - else → FUN_005A1010 → **VQAClass open FUN_0071C5F0** ("VQA playback
         5.0 (Jun 6 2001)" module; centers movie on the surface) →
         **VQMovieHandle** (vft 0x7A6624);
       - open failure → returns **NULL**.
    4. success: `handle->vtbl+0x18`, `MoveWindow(ctrl, movieW=hdl+8,
       movieH=hdl+0xC)`, `SetTimer(hwnd, 0x65, 0x22, 0)` = **34 ms tick**
       (each WM_TIMER advances a frame; VQAClass::Advance_Frame =
       FUN_0071CFC0);
    5. **failure (VQA absent): @0x5F3B83 kills timer 0x65, frees `obj+0x60`,
       and the control simply stays inert — the MNSCRNS backdrop shows.
       There is NO SHP/frame-list fallback anywhere in this path.**
- Sender (already in §0): main-menu loop FUN_00516F00 @0x516FD3..0x517031 sends
  0x4E3 wParam=1 then 0x4E4 lParam="Ra2ts_s" (640x480) / "Ra2ts_l" (else);
  FUN_005115F0 is the reposition/replay helper. `Ra2ts` exists ONLY as those
  two strings; hashes RA2TS_S/L.SHP (81B77031/7337C07F) exist in **no** mix;
  no VQA exists in this install (§7.1 scan re-verified incl. new full scan
  `scan_ra2_full.txt`).
- The RTTI class family MSAnim/MSShapeAnim/MSVQAnim/MSBinkAnim/MSPCXAnim/
  MSFadeAnim/MSOverlayAnim (MSEngine, code 0x5AC000..0x5B0000) is the
  **[Intro]/attract slideshow subsystem** (built by FUN_004DE380), NOT the
  menu CRT. MovieHandle/VQMovieHandle/BinkMovieHandle (0x5A1xxx) is the
  shared movie wrapper used by the CRT control.

### 8.2 Every CRT-sized frame-list in neutral.mix, now NAMED [CONFIRMED]

All ids matched by recomputing Westwood mix CRC of names found in the shell
asset registry (0x7F86xx..0x7F8814). Loaders and palettes read from code;
renders in scratchpad `anim_<NAME>_<HASH>.png`:

| id | name | dims x frames | palette (loader) | purpose |
|---|---|---|---|---|
| 97442168 / 65C49126 | MNSCRNS / MNSCRNL | 472x448 / 632x568, 1fr | SHELL.PAL | main-menu static radar map (§1) |
| 62778D29 / 56BB8CB3 | ASCRBKSM / ASCRBKMD | 472x448 / 632x568, 1fr | ASCORE.PAL | campaign-select Allied static bg (eagle emblem) |
| 8254FE4A / B698FFD0 | SSCRBKSM / SSCRBKMD | 472x448 / 632x568, 1fr | SSCORE.PAL | campaign-select Soviet static bg |
| 9283A59D / 826D13C6 | ASCRTSM / ASCRTMD | both 472x448, 6fr | ASCORE.PAL | campaign-select **T**ransition: monitors fold away -> eagle emblem |
| 72A0D6FE / 624E60A5 | SSCRTSM / SSCRTMD | both 472x448, 6fr | SSCORE.PAL | Soviet transition (hammer&sickle) |
| A6D89AD2 / B6362C89 | ASCRASM / ASCRAMD | 424x220, 44fr | ASCORE.PAL | Allied emblem glint loop **inside** the campaign screen |
| 46FBE9B1 / 56155FEA | SSCRASM / SSCRAMD | 424x230, 44fr | SSCORE.PAL | Soviet emblem glint loop |
| B36583F2 / 51D583A1 | MPASCRNS / MPASCRNL | 472x448 / 632x568, 1fr | MPASCRN.PAL | skirmish/MP screen, GI before burning Kremlin (render is clean under this pal) |
| 903ED227 / 728ED274 | MPSSCRNS / MPSSCRNL | 472x448 / 632x568, 1fr | MPSSCRN.PAL | skirmish/MP Soviet-side scene |
| 47E6853F / FFD62DD0 | FSBKGDSM / FSBKGDLG | 472x448 / 632x568, 1fr | FSSCRN.PAL | "FS" faction/score bg: Allied+tutorial+Soviet emblems stacked |
| 837288F0 / 939C3EAB / B37F47B1 | (no name string in game.exe) | 472x448 / 632x568 / **856x736**, 8fr | (n/a) | CRT power-on **flash** trio (dark bevel -> red -> orange -> white-yellow flash -> dark); 856x736 = 1024x768 shell size |

Loaders: FUN_006F2840 loads the ASCR*/SSCR* trio per faction+resolution with
the pal getter at 0x6F2960 returning **ASCORE.PAL / SSCORE.PAL**;
FUN_006F2A30 pairs MPASCRN*<->MPASCRN.PAL, MPSSCRN*<->MPSSCRN.PAL;
FUN_006F2C40 pairs FSBKGD*<->FSSCRN.PAL (immediates verified in .text).

**None of these frame-lists — nor any SHP in any container of this install
(full recursive scan of every mix incl. ra2md) — contains the Statue of
Liberty / radar-sweep menu animation.** The statue footage exists only in the
absent `ra2ts_s/_l.vqa`. What the user remembers as a menu "frame-list" is
almost certainly (a) the campaign-select ASCRT*/ASCRA* SHP animations and/or
(b) the SDWRNANM monitor loop — both ARE frame-lists, but they are not the
main-menu CRT background.

### 8.3 Clone directive

1. Main menu CRT: VQA is unrecoverable here; ship §6's static composite
   (MNSCRNS + logo emblem) or a procedural radar sweep; if a `ra2ts_s.vqa`
   is ever provided, play it looped in (0,0) of the screen area at native
   size with a 34 ms frame pump — that is bit-faithful to the control.
2. Campaign select (dlg 0x94): blit ASCRBK* static, play ASCRT* 6-frame
   transition on entry, then loop ASCRA* 44-frame glint inset — all under
   ASCORE.PAL (Soviet: SSCR*/SSCORE.PAL). Skirmish: MPASCRN*+MPASCRN.PAL.
3. Optional page-flash: the unnamed 8-frame trio (837288F0/939C3EAB/B37F47B1)
   is a ready-made CRT power-on flash at all three shell sizes.

## §9 DEFINITIVE: the CRT scene is RA2TS_L.BIK — a Bink movie inside language.mix (fourth pass)

§8's verdict ("VQA absent, scene unrecoverable, use static composite") was
WRONG. The logo+radar-sweep+Statue-of-Liberty+jets scene the running game
shows is a **Bink video**, and it is present in this install.

### 9.1 The mechanism (game.exe proof)

- Menu loop `FUN_00516F00` (0x516F00): gets the CRT control
  `GetDlgItem(hDlg, 0x71A)`, centers it (`SetWindowPos` at
  (w-800)/2,(h-600)/2), then
  `SendMessageA(ctrl, 0x4E3, 1, 0)` (loop flag on) and
  `SendMessageA(ctrl, 0x4E4, 0, name)` where
  **name = "Ra2ts_s" if screen width == 640 (0x280) else "Ra2ts_l"**
  (strings at 0x7DB5B0 / 0x7DB5A8).
- CRT control proc (0x5F2C00..0x5F3C4E), msg 0x4E4 handler at 0x5F3A6C:
  calls movie factory `FUN_005A1D30`.
- `FUN_005A1D30` -> `FUN_005A1BB0`: strips the extension, then probes file
  availability via CCFileClass (`FUN_00468F50`, vtbl+0x14 Is_Available)
  **first with ".BIK" (DAT_007D9D74), then with ".VQA" (DAT_007D9D7C)**.
  BIK wins if present.
- Back in `FUN_005A1D30`: `strrchr(name,'.')` compared against ".bik"
  (DAT_007E329C, case-insensitive `FUN_00781F20`); on match it logs
  **"Play_Movie() as Bink!"** (0x7E3284) and constructs a Bink movie object
  (`FUN_0042FEA0`, vtable PTR_FUN_007A6684) driven by BINKW32.DLL imports
  (_BinkOpen@8, _BinkNextFrame@4, _BinkGoto@12, _BinkPause@8, ...).
  Only the non-.bik path falls through to the VQA open `FUN_005A1010`.
- After a successful open (0x5F3A6C): `MoveWindow` resizes the control to
  the movie's WxH and `SetTimer(hwnd, 0x65, 0x22, 0)` — a **34 ms pump**
  that advances/blits Bink frames (Bink self-paces to the file's 15 fps).
  On end-of-movie with the 0x4E3 loop flag set, the handler at 0x5F3300
  logs "Looping movie" (0x7EAEB0) and restarts (movie vtbl+0x1C).
  This is why the sweep animates when the window has focus and freezes
  when unfocused (timer/menu loop paused), with zero large mix reads at
  steady state (Bink streams ~24 KB/frame).

### 9.2 The asset

`language.mix` (53,116,040 bytes, game root) top-level entries include:

| hash | name | size | format |
|---|---|---|---|
| 33665128 | **RA2TS_L.BIK** | 7,184,828 | BIKi, **632x570**, 431 frames, 15 fps, 28.73 s loop |
| C1E6E166 | **RA2TS_S.BIK** | 7,184,988 | BIKi, **472x450**, 431 frames, 15 fps, 28.73 s loop |

(`langmd.mix` carries a YR variant of RA2TS_L.BIK, 7,185,136 bytes.)
Content: rounded-CRT bezel + red radar map (coastline), rotating bright
radar sweep wedge, COMMAND & CONQUER / RED ALERT 2 chrome logo, Statue of
Liberty and jet/cloud footage ghosted through the sweep. Frame extractions
(ffmpeg, binkvideo decoder) match the live-menu screenshots pixel-for-pixel:
scratchpad `crt_scene_bik_1..5.png`.

Why every earlier pass missed it: the VQAs really are absent —
MOVIES01.MIX / MOVIES02.MIX are 4-byte "MYTH" crack stubs — but the loader
tries **.BIK before .VQA**, so the menu never needed the VQA. The one failed
274,332-byte VQA-ish read Frida saw was a doomed read against the 4-byte
stub (got=0) and is irrelevant to the CRT. MNSCRNL/MNSCRNS (the plain
coastline-map SHPs, 632x568 / 472x448) are only the static no-movie
fallback drawn by the control when the factory returns NULL — the running
game with language.mix present never shows them bare.

### 9.3 Clone recipe (bit-faithful)

1. Shell 800x600 -> play `language.mix/RA2TS_L.BIK`; shell 640x480 ->
   `RA2TS_S.BIK`. Decode with any Bink decoder (ffmpeg `binkvideo` works).
2. Blit the movie 1:1 (632x570 / 472x450) at the CRT control position
   (control is the left screen area of the centered 800x600 dialog).
3. Pump frames on a ~34 ms timer but present at the file's 15 fps; when
   the last (431st) frame finishes, seek to frame 1 and continue (seamless
   loop). Pause the pump when the app loses focus.
4. Only if the .bik is missing: fall back to static MNSCRNL/MNSCRNS.SHP
   (radar-map-only art) as §6 described. No palette cycling and no
   procedural sweep exist anywhere in this path — the sweep is baked into
   the video.
