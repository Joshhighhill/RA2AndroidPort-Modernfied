# RA2 Main Menu / Front-End — RE + Asset-Presence Probe

Scope: **original Red Alert 2 (NOT Yuri's Revenge)**. Base game data = `ra2.mix`,
`language.mix`, `theme.mix` in `D:\Red Alert 2`. (`ra2md.mix` / `thememd.mix` /
`langmd.mix` are the YR counterparts and are deliberately ignored here.)

Tags: **[CONFIRMED]** = probed present in *this* install via ra2extract;
**[ANCHORED]** = from XCC name DB / documented RA2 / SHP headers; **[RE-FALLBACK]** = inference.

Tooling: `D:\ra2-android\build\ra2extract.exe`. Usage:
`ra2extract <mix> --list [nested]`, `--raw <path> <out>`, or `<mix> <shp> <pal> <out.png> [frame]`.
Nested paths descend local mixes (ra2extract computes the Westwood filename hash for the name given).

---

## §1 Real RA2 main-menu composition

The RA2 front-end is drawn by the shell in the exe: a **full-screen background SHP**,
a vertical strip of **glowing tech-framed buttons**, the **Westwood/RA2 logo**, and the
button list. It is a static (single-frame) dark "tactical map of the US East Coast"
background, not an animated movie. [ANCHORED from SHP headers + documented RA2]

| Element | Asset (name) | Container | Format | Dims / frames | Tag |
|---|---|---|---|---|---|
| Main-menu BACKGROUND (hi-res, ~800×600) | `mnscrnl.shp` | `ra2.mix/neutral.mix` | SHP | **632×568, 1 frame** | [CONFIRMED] |
| Main-menu BACKGROUND (lo-res, 640×480) | `mnscrns.shp` | `ra2.mix/neutral.mix` | SHP | **472×448, 1 frame** | [CONFIRMED] |
| Menu BUTTON strip | `mnbttn.shp` | `ra2.mix/local.mix` | SHP | **126×25, 3 frames** (up/down/hover) | [CONFIRMED] |
| RA2 LOGO | `logo.pcx` | `ra2.mix/local.mix` | PCX | 163858 B | [CONFIRMED] |
| TITLE image | `title.pcx` | `ra2.mix/local.mix` | PCX | 76413 B | [CONFIRMED] |
| Dialog/panel chrome (Allied) | `bkgdlg.shp`, `optbtn.shp`, `tabs.shp`, `button00.shp`, `diplobtn.shp` | `ra2.mix/sidec01.mix` | SHP | bkgdlg = 856×736 | [CONFIRMED] |
| Dialog/panel chrome (Soviet) | same names | `ra2.mix/sidec02.mix` | SHP | — | [CONFIRMED] |
| Loadscreen — Allied | `aloadlg.pcx` | `ra2.mix/neutral.mix` | PCX | 659243 B | [CONFIRMED] |
| Loadscreen — Soviet | `rloadlg.pcx` | `ra2.mix/neutral.mix` | PCX | 980802 B | [CONFIRMED] |
| Local loadscreen | `load.pcx` | `ra2.mix/local.mix` | PCX | 76884 B | [CONFIRMED] |
| MP score screen | `mpscore.pcx` | `ra2.mix/local.mix` | PCX | 243796 B | [CONFIRMED] |

**Button list (real RA2 base set)** — CONFIRMED from `ra2.csf` (a loose file in this
install; our loader reads CSF). The `GUI:` label keys present:

- **Single Player** (`GUI:SinglePlayer`) → New Campaign (`GUI:NewCampaign` → `GUI:SelectCampaign`:
  `GUI:AlliedCampaign` / `GUI:SovietCampaign` / `GUI:TutorialCampaign`), **Skirmish** (`GUI:Skirmish`),
  **Load Saved Game** (`GUI:LoadSavedGame`)
- **Network** / LAN (`GUI:Network`, IPX — `GUI:IPXOptions`, `GUI:NetworkCard`)
- **Internet** / Westwood Online (`GUI:Internet`, `GUI:UDPInternet`)
- **Options** (`GUI:Options`)
- **Sneak Peeks** (`GUI:SneakPeeks`) → Play Movies (`GUI:PlayMovies`), View Credits (`GUI:ViewCredits`)
- **Exit Game** (`GUI:ExitGame`)

[CONFIRMED — strings present in `D:\Red Alert 2\ra2.csf`]

Container map (ra2extract `--list`): `ra2.mix` → `local.mix`(591), `conquer.mix`(289),
`generic.mix`(139), `load.mix`(80), `sidec01.mix`(51, Allied UI), `sidec02.mix`(51, Soviet UI),
`neutral.mix`(69, faction-neutral front-end screens), `cache.mix`(42, fonts+cameo pal),
`isotemp.mix`(1035), `isogen.mix`(24). `language.mix` → `cameo.mix`(203), `audio.mix`(461).
`OpenRA` ships **no** RA2 mod (only ra/cnc/d2k/ts), so no OpenRA chrome yaml exists to cite.

---

## §2 ASSET-PRESENCE TABLE (the key deliverable)

All commands run against `D:\Red Alert 2`. Method: `ra2extract <mix> --raw <nested/asset> out.bin`
→ file written = **PRESENT** (bytes shown), error = **ABSENT**. Every row below was executed.

| Asset | Command (mix + `--raw` path) | Result |
|---|---|---|
| `mnscrnl.shp` (menu bg, hi) | `ra2.mix --raw neutral.mix/mnscrnl.shp` | **PRESENT 359008** [CONFIRMED] |
| `mnscrns.shp` (menu bg, lo) | `ra2.mix --raw neutral.mix/mnscrns.shp` | **PRESENT 211488** [CONFIRMED] |
| `mnbttn.shp` (button) | `ra2.mix --raw local.mix/mnbttn.shp` | **PRESENT 9536** [CONFIRMED] |
| `logo.pcx` | `ra2.mix --raw local.mix/logo.pcx` | **PRESENT 163858** [CONFIRMED] |
| `title.pcx` | `ra2.mix --raw local.mix/title.pcx` | **PRESENT 76413** [CONFIRMED] |
| `aloadlg.pcx` (Allied load) | `ra2.mix --raw neutral.mix/aloadlg.pcx` | **PRESENT 659243** [CONFIRMED] |
| `rloadlg.pcx` (Soviet load) | `ra2.mix --raw neutral.mix/rloadlg.pcx` | **PRESENT 980802** [CONFIRMED] |
| `load.pcx` | `ra2.mix --raw local.mix/load.pcx` | **PRESENT 76884** [CONFIRMED] |
| `mpscore.pcx` | `ra2.mix --raw local.mix/mpscore.pcx` | **PRESENT 243796** [CONFIRMED] |
| `bkgdlg.shp` (dialog, Allied) | `ra2.mix --raw sidec01.mix/bkgdlg.shp` | **PRESENT 630048** [CONFIRMED] |
| `bkgdlg.shp` (dialog, Soviet) | `ra2.mix --raw sidec02.mix/bkgdlg.shp` | **PRESENT 630048** [CONFIRMED] |
| `optbtn.shp` / `tabs.shp` / `button00.shp` / `diplobtn.shp` | `ra2.mix --raw sidec01.mix/<name>` (and `sidec02.mix`) | **PRESENT** (2648/2720/3384/2648 B) [CONFIRMED] |
| `loadout.shp` | `ra2.mix --raw cache.mix/loadout.shp` | **PRESENT 50072** [CONFIRMED] |
| **Palette** `fsscrn.pal` (fullscreen) | `ra2.mix --raw neutral.mix/fsscrn.pal` | **PRESENT 768** [CONFIRMED] |
| **Palette** `uibkgd.pal` | `ra2.mix --raw sidec01.mix/uibkgd.pal` (also sidec02) | **PRESENT 768** [CONFIRMED] |
| **Palette** `sidebar.pal` | `ra2.mix --raw sidec01.mix/sidebar.pal` (also sidec02) | **PRESENT 768** [CONFIRMED] |
| **Palette** `ldscrn/ldscrna/ldscrns.pal` | `ra2.mix --raw load.mix/<name>` | **PRESENT 768 each** [CONFIRMED] |
| **Palette** `palette.pal`, `cameo.pal` | `ra2.mix --raw cache.mix/<name>` | **PRESENT 768** [CONFIRMED] |
| `mnscrnl.shp` in `local.mix` (wrong container test) | `ra2.mix --raw local.mix/mnscrnl.shp` | ABSENT (correct container is `neutral.mix`) |

**Nothing menu-related is missing from this install** — every front-end art id resolved.
The only "absents" encountered were container mismatches while locating each asset.

Themed variants: front-end **backgrounds/loadscreens are in `neutral.mix`** (faction-neutral);
Allied vs Soviet theming is in the **in-game/dialog chrome**: `sidec01.mix` = Allied,
`sidec02.mix` = Soviet (each has its own `optbtn/diplobtn/uibkgd/sidebar`), plus the
Allied `aloadlg.pcx` vs Soviet `rloadlg.pcx` loadscreens. [CONFIRMED]

---

## §3 Shell FONT

- RA2 menu text uses the **Westwood `.fnt` bitmap font** format. [ANCHORED]
- Present in this install [CONFIRMED via `--raw`]:
  - `ra2.mix/local.mix`: `game.fnt` (1584195 B, in-game HUD font), `efnt.fnt` (4304),
    `huge.fnt` (35373), `map.fnt` (9723), `metal12.fnt` (7698)
  - `ra2.mix/cache.mix`: `12metfnt.fnt` (10637), `6point.fnt` (15277), `8point.fnt` (14796),
    `grad6fnt.fnt` (27250), `editfnt.fnt` (9798), `kia6pt.fnt` (7430)
  - The metallic shell/menu font is `12metfnt.fnt` / `metal12.fnt` (the bevelled tech look).
- **DECODER GAP:** ra2assets has **no `.fnt` decoder**. `include/ra2assets/` has
  art/audio_bag/blowfish/csf/fileio/hva/ini/land_type/lcw/lzo1x/map/mappack/mix/pal/rules/shp/sounds/theater/vpl/vxl/wav
  — no `fnt.hpp`; no font code in `src/`. Rendering authentic menu text needs a **new FNT decoder**
  (Westwood variable-width bitmap font: per-glyph width table + palettized rows). Until then, text
  must fall back to the engine's rect-font. [CONFIRMED gap]

---

## §4 Menu MUSIC

- `theme.ini` (extracted from `ra2.mix/local.mix`) defines the themes. The **front-end / shell
  theme is `[INTRO]` → `Sound=Grinder`, `Repeat=yes`, `Normal=no`** — i.e. the menu loops the
  "Grinder" track. [CONFIRMED from theme.ini]
- In *this* install the score tracks live in `theme.mix` as **`.wav`** (not `.aud`). All 16
  present [CONFIRMED]: `grinder.wav` (3264572 B — the menu track), plus `fortific/indeep/
  tension/eaglehun/industro/blowitup/destroy/burn/jank/motorize/hm2/200meter/ra2-opt/ra2-sco/power .wav`.
  - Command: `ra2extract "theme.mix" --raw grinder.wav out.bin` → PRESENT.
- **Format:** RIFF, `fmt`=**0x0011 (IMA ADPCM)**, **2 channels (stereo)**, 22050 Hz, 4-bit. [CONFIRMED via header]
- **Loader status / GAP:** `src/wav.cpp` decodes IMA-ADPCM (`load_wav_ima`, shares `ima_adpcm.hpp`
  with the BAG path) **but hard-rejects stereo**: line 42 `throw "wav: only mono IMA supported"`.
  The menu track is stereo → **not playable as-is**; needs a small **stereo-IMA extension** to
  `wav.cpp` (interleaved L/R nibble decode). The `--demo-audio` path exists but feeds the mono decoder. [CONFIRMED gap]

---

## §5 Layout / coordinates [ANCHORED]

No OpenRA RA2 chrome exists (see §1), so layout is anchored from the SHP dimensions + documented RA2 shell:

- RA2 shell runs at **640×480 (lo-res → `mnscrns.shp` 472×448)** or **800×600 (hi-res → `mnscrnl.shp` 632×568)**.
  The `mnscr*` SHP is the full backdrop, drawn centered; its own frame is slightly smaller than the
  screen (the shell fills the border with the palette's background color).
- The **button column** is a vertical stack of `mnbttn.shp` instances (126×25 each, 3 state-frames)
  down the **lower/right** region of the backdrop, one per top-level item (§1 list), with the RA2
  `logo.pcx` above them. [ANCHORED — matches documented RA2 front-end]
- In-game dialogs (Options, Load, Skirmish setup) use `bkgdlg.shp` (856×736) framed panels with
  `optbtn`/`tabs`/`button00` controls from `sidec0{1,2}.mix`. [ANCHORED]
- Exact per-button pixel offsets are hard-coded in the RA2 exe (not data-driven) — would need a
  Ghidra pass on the shell dialog init to recover to the pixel; a faithful *looking* layout can be
  approximated from the SHP sizes above. [RE-FALLBACK]

---

## §6 VERDICT — faithful menu with the assets ACTUALLY present

**All source art exists in this install — nothing is missing.** The limits are **decoder gaps in
our pipeline**, not absent assets.

Our loader supports: **SHP ✅, PAL ✅, CSF ✅, WAV(IMA mono) ✅**. It does **not** support:
**PCX ❌, FNT ❌, stereo-IMA WAV ❌** (no PCX in include/ or src/; no FNT; wav.cpp mono-only).

What renders **today**, unchanged:
- ✅ **Background** `mnscrnl.shp` / `mnscrns.shp` (SHP) — decodes (632×568 PNG produced).
- ✅ **Buttons** `mnbttn.shp` (SHP, 3 states) — decodes.
- ✅ **Dialog chrome** `bkgdlg/optbtn/tabs/button00` (SHP) — decodes.
- ✅ **Button labels** from `ra2.csf`.
- ✅ **Palette:** RESOLVED — see §7. Pinned **`cache.mix/palette.pal`** (renders the real East-Coast
  situation map). `fsscrn.pal` is the *wrong* pal (its base indices are the dark red/blue speckle).

What is **present but blocked** until a decoder is added (all straightforward, ~small tasks):
- 🔒 **`logo.pcx` / `title.pcx` / loadscreens** — need a **PCX decoder** (8-bit RLE PCX, palette in trailer).
- 🔒 **Authentic menu font** (`12metfnt.fnt`) — need a **Westwood FNT decoder** (fallback: rect-font).
- 🔒 **Menu music** `grinder.wav` — need **stereo IMA-ADPCM** support in `wav.cpp` (currently mono-only).

**Recommended fidelity target for THIS install:**
1. **Now / no new code:** SHP background (`mnscrnl.shp` + a calibrated UI palette) + `mnbttn.shp`
   button strip + CSF labels rendered in the engine's rect-font. This already reads as the RA2 menu shape.
2. **+1 small decoder each (recommended):** add **PCX** (unlocks the real RA2 logo/title/loadscreens),
   add **stereo IMA to wav.cpp** (unlocks the looping menu theme `grinder.wav`).
3. **Full fidelity:** add the **FNT** decoder for authentic bevelled menu text, and a Ghidra pass on the
   shell for pixel-exact button coordinates.

Bottom line: **a faithful background + buttons + music + font menu is achievable with the art in this
install** — it is gated only by three self-contained decoder additions (PCX, FNT, stereo-IMA-WAV) plus
one palette-calibration step, none of which require any missing game data.

---

## §7 Adjudication (RE-GATE / Task-1 Calibration)

Method: rendered `neutral.mix/mnscrnl.shp` frame 0 through **every `.pal` reachable in `ra2.mix`**
(17 palettes: neutral `fsscrn`/`radar`; cache `palette`/`shell`/`temperat`/`cameo`/`unittem`/`unitsno`/
`snow`/`anim`; load `ldscrn`/`ldscrna`/`ldscrns`; sidec01+02 `uibkgd`/`sidebar`; local `dialog`) and
measured each. PNGs kept in the session scratchpad as `menupal_<name>.png` for eyeballing.

### Item 1 — MENU PALETTE  →  **PINNED: `cache.mix/palette.pal`**  ✅ (was `[RE-FALLBACK]`)

The menu image is **95 %** composed of just six high indices — `254`(107768 px), `252`(59331),
`249`(54552), `237`(52937), `245`(46663), `232`(17746). Whichever palette maps *those* to natural
map colors wins; the low indices (45/61/42/56…) are only fine coastline detail. Result of the sweep:

| palette | idx 254 (land base) | idx 237 | idx 232 | verdict |
|---|---|---|---|---|
| **`cache.mix/palette.pal`** | **(132,136,116) tan/olive** | (132,0,0) red hi | (216,0,0) red hi | **✅ real East-Coast relief map** |
| `neutral.mix/fsscrn.pal` | (8,8,8) near-black | (12,20,108) **blue** | (88,8,24) red | ❌ the "dark red/blue speckle" (matches controller report) |
| `cache.mix/shell.pal` | (0,0,0) | (36,4,4) | (56,4,4) | ❌ featureless near-black, faint-red monochrome |
| `neutral.mix/radar.pal` | (8,0,0) | (28,12,0) | (24,24,16) | ❌ near-black |
| `sidec01/02 uibkgd.pal` | (0,0,0) | (8,8,4) | (0,8,32) | ❌ near-black (brightness ~5/255) |
| `cache.mix/anim.pal` | (0,0,16) | (52,16,16) | (40,32,16) | ❌ whole map tinted blue |
| `cache.mix/cameo.pal` | (16,10,10) | — | — | ❌ near-black |
| `cache.mix/unittem/unitsno.pal` | bright, 98 % saturated | — | — | ❌ full RGB noise |

(All `.pal` values are **6-bit VGA, 0–63**, and MUST be scaled ×4 to 8-bit before use — `ra2extract`
does this; **T5 must too** or every render comes out ~4× too dark.)

**Only `cache.mix/palette.pal` renders a coherent, recognisable image** — the US East Coast situation
map (Chesapeake Bay, Delaware Bay, Long Island, coastline all legible), olive/grey land, smooth grey
ocean to the right, red terrain highlighting. Full-res proof:
`scratchpad/menupal_palette.png` (ra2extract's own decode = exactly what T5 will produce) and
`scratchpad/menupal_palette_FULLRES.png`. This is a decisive win, not a least-bad pick.

**Caveat (minor, Ghidra carry-forward):** indices `237`/`232` render as saturated red in `palette.pal`
(it is the cache/theater default palette, whose 232–239 band is the "highlight/remap" slots), so the
land carries heavy red shading. This reads as the intended military-map styling and is faithful, but
the pixel-exact shell palette may be embedded in the exe — a Ghidra pass on the shell load could refine
the 232–239 band later. **Pin `cache.mix/palette.pal` now.** `neutral.mix/fsscrn.pal` (the co-located
"fullscreen" pal) is **rejected** — its base indices are near-black with blue/red tints, i.e. the exact
dark red/blue speckle the controller saw.

### Format finding — **normal uncompressed TS/RA2 SHP, NOT a special raw blit**

`mnscrnl.shp` header: `zero=0, width=632, height=568, nframes=1`; frame-0 header `x=0,y=0,w=632,h=568,
flags=0, dataoffset=32`. **`flags=0` = uncompressed**; the 8-byte file header + 24-byte frame header =
32 bytes, then `632*568 = 358976` bytes of raw 8-bit indices → `358976 + 32 = 359008` (the observed
size, exactly). So the "359008 = 632×568 + 32" hint is a plain uncompressed single-frame SHP, and our
`ShpFile` decoder handles it correctly (geometry/coastline is pixel-correct — proven across all 17
renders). **T5 should use the normal SHP path; no raw-blit special-case and no row-stride fix needed.**
The *only* calibration issue was the palette, now resolved.

### Item 2 — BUTTON / LOGO COORDS  →  adjudicated approximation (Ghidra exact-rects carry-forward)

Backdrop `mnscrnl` (632×568) scaled **uniformly** into 1280×720: fit = min(1280/632, 720/568) ≈ **1.267**,
giving ~801×720, centered, with side letterbox bars filled with the palette background color (the olive
land-base tone, idx 254). Button column = right-side vertical stack of `mnbttn.shp` (126×25, 3 state
frames up/down/hover), scaled ~1.6× → ~200×40 each, at **x ≈ 980**, first **y ≈ 250**, **gap ≈ 8 px**,
one per live/greyed top-level item; `logo.pcx` centered above at **y ≈ 60**; `title.pcx` top-left.
Exact per-button pixel rects remain a **Ghidra carry-forward** (hard-coded in the exe, not data-driven).

### Item 3 — MENU-ITEM LIST + LIVE/GREYED  →  confirmed

`ra2.csf` (loose file) contains all `GUI:` keys (verified this pass — hit counts): `SinglePlayer`,
`NewCampaign`, `LoadSavedGame`, `Skirmish`, `Network`, `Internet`, `Options`, `SneakPeeks`, `ExitGame`.
Phase-3 (D5) split: **LIVE = Skirmish + Exit/Quit**; **GREYED = Campaign / Load / Network / Internet /
Options / Sneak Peeks**.

### Item 4 — FRONT-END MUSIC  →  confirmed

`theme.ini` `[INTRO]`: `Name=THEME:Intro`, `Sound=Grinder`, `Normal=no`, `Repeat=yes` → loop-on-menu /
stop-on-game. `theme.mix/grinder.wav`: RIFF/WAVE, `fmt` tag **0x0011 (IMA ADPCM)**, **channels=2
(stereo)**, **22050 Hz**, **4-bit**, blockalign=1024, 3264572 B. Confirms the **T4 stereo-IMA branch** is
required (`wav.cpp` currently hard-rejects stereo).
