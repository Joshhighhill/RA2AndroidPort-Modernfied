# Wave 6 / 05: presentation in real missions and skirmish (what the player sees and hears)

Baseline: master `e2b569e`, branch `w6-presentation`, my own Release build (`build-w6/`).
Read-only audit. No engine code was changed.

## Fix status (wave-6 fix agent #5, branch `w6fix-pres`)

Base `409e807`. One commit per fix. Before | after crops (JPEG, the "before" half is the audit's own shot from
`e2b569e`) are in `05-evidence/fix/`. Save format **v25** (was v24). Gate re-pin: `--demo-saveload` only (cause in
README: the new saved `timer_text_`; proven by rebuilding with v24 and without the field, which prints the old pin).

| Item | Status | Commit | Evidence / check |
|---|---|---|---|
| W6-05-01 house colours | **FIXED.** `[<House>] Color=` (else the country's `Color=`) names a rules `[Colors]` ColorScheme; the remap is `pal_for_hsv`. Player DarkBlue / DarkRed, civilians Grey. Render-only table, no save change. | `b000536` | `w6-05-01_sov05_civilians_grey.jpg`, `w6-05-01_sov01_allied_blue.jpg`; `pres-w6-campaign.play` (`house_color(0) == 18` on ALL04) |
| W6-05-02 tile black holes | **FIXED.** The theater searches the theater art mix, then `generic.mix`, after the iso mix [RE: Init_Theater FUN_00519e70]. | `a8a6a46` | `w6-05-02_all04_rubble_lots.jpg`, `w6-05-02_sov01_overpass.jpg`; probe `tile_misses` = 0 on ALL01, ALL04, SOV01 |
| W6-05-03 lamp light strips | **FIXED.** The light map is painted from every cell in terrain painter order, each cell a prism from its diamond down to its lowest front neighbour, so a cliff or ramp face takes its own tile's light. | `9a6ffb5` | `w6-05-03_all02_no_light_strips.jpg`, `w6-05-03_all05_night_light.jpg` |
| W6-05-04 TimerText | **FIXED.** Action 103 stores the label (saved, v25); the mission timer draws `"%s  %02d:%02d"` bottom-right in the player's colour [RE: FUN_006a45a0 -> FUN_006a5280, disassembled: ECX = line, EDX = colour]. | `b2fa93f` | `w6-05-04_sov04_timer_label.jpg` ("Time until Allied invasion begins:  08:51"); probes `timer_text`, `timer_line(i)` |
| W6-05-05 SW countdowns | **FIXED.** Every `ShowTimer=yes` SW of every house that has started charging, `"<UIName>  mm:ss"` in its owner's colour, stacked above the mission timer; blinks to ColorSchemes[0] at 0:00. | `b2fa93f` | `w6-05-05_skirmish_sw_countdowns.jpg`; `pres-w6-timers.play` |
| W6-05-06 sounds | **FIXED.** CreateSound [RE: HouseClass::JustBuilt FUN_004E6F10, TechnoType +0x4B8], ImpactLand/WaterSound [RE: FUN_004BCD50 @4BCF83, +0x4C4/+0x4C0], Working/NotWorkingSound [RE: BuildingClass AI @43C940, +0xC18/+0xC1C, +0x598 edge]. | `e24699f` | `audio-events.play` (PowerOn, PowerOff, OilDerrickLoop, KirovCreated, KirovCrash) |
| W6-04-27 sound batch | **FIXED except two keys.** ChronoIn/OutSound (CLEG/CCOMAND jumps), Enter/LeaveWaterSound, aircraft MoveSound loop + AuxSound1/2, per-transport Enter/LeaveTransportSound, per-type chrono-miner sounds, all positional [RE-FALLBACK: the locomotor call sites]. Robot Tank Activate/DeactivateSound stays open (the sim has no robot-control state); HasRadialIndicator is a draw (units agent). | `e24699f` | `audio-events.play` (KirovMoveLoop, ChronoLegionTeleport, IFVTransform) |
| W6-04-11 bomb sounds | **FIXED (sound half).** BombAttachSound once, BombTickingSound looping while a bomb is on [RE-FALLBACK]. The bomb icon is the units agent's. | `e24699f` | `abil-defuse.play` |
| W6-05-08 message cap | **FIXED.** TextTrigger / objective lines go through `post_message`: cap 6, `[General] MessageDelay`. | `32e3d8b` | `w6-05-08_all05_message_cap.jpg` (17 lines -> 6) |
| W6-05-07 hover health bar | **FIXED.** The unit under a real mouse pointer shows its pip row without PIPBRD [RE: FUN_006c3f00 gates only the board on IsSelected]. | `dc905bc` | `w6-05-07_hover_pips.jpg`; `ui-hud.play` (`hover_unit`) |
| W6-05-10 missing tiles | **FIXED (safety net).** A cell with no tile image draws template 0 as clear ground and is logged. After W6-05-02 no campaign map needs it: SOV01's `MWater07.tem` resolves from the fallback archives, and the engine finds no bad sub-tile on ALL01 (`tile_fallbacks` = 0). | `eb9f4c0` | probe `tile_fallbacks` |
| W6-05-09 R-145 building pip line | **RE'd, default fixed.** Dimension2 @45D8B0 = (FoundationW*256, FoundationH*256, Height*CellHeight): the line sits Height levels up, which the clone already did. GAPOWR (Height=4) really sits that high. Only the default for a missing `Height=` changed, 4 -> 2 (ctor @457C2A). | `84b2cf7` | - |
| W6-05-09 R-068 skirmish LocalSize | **NOT DONE.** A camera-only clamp from the skirmish map's `LocalSize` worked (the jagged right edge went off screen), but it breaks `--play` scripts that view or drag cells outside the local area (`drag cell 0 0 0 0` in the audit's `skfight.play`). Reverted; it needs a sweep of the skirmish scripts. | - | - |
| W6-05-09 R-107 cliff extras, Kirov on the ground | **NOT DONE** (R-107 needs the BlendedFog RE; the Kirov item overlaps the units agent). | - | - |

## TL;DR

The renderer is in good shape overall. Statue, gems, lamp pools, damage fires, chevrons, the radar boot and
shroud, PLACE.SHP and the bridge and air layers all look right in the 24 missions. But a player sees five things
straight away that are wrong:

1. **Wrong colours for every campaign house** (W6-05-01).
   - The player's own army is **gold** in every mission. It should be blue for Allied and red for Soviet.
   - Civilian buildings and vehicles have **purple** trim instead of grey.
   - Enemy and side houses get colours by list position, not from the map's `Color=`.
2. **Black holes in the city maps** (W6-05-02). War-rubble tiles and overpass tiles are not found because the clone
   looks only in the `iso*.mix` archive. 4x4 black diamonds show in ALL03, ALL04, ALL06, ALL08, SOV01 (at the
   Pentagon wall) and SOV08.
3. **Dark diamond strips on snow and night maps that have lamps** (W6-05-03). The light-source map is drawn as flat
   per-cell diamonds at each cell's height. At every height step, unlit gaps show the darker ambient colour.
   Affects ALL02, ALL05, SOV07 and SOV11.
4. **Countdowns have no meaning** (W6-05-04, W6-05-05).
   - The mission timer always says "Time Remaining:" and sits top-centre. The original shows the map's `TimerText`
     at the bottom-right, e.g. "Time until Allied invasion begins:" or "Enemy Forces Arriving In:".
   - The superweapon countdowns (`ShowTimer=yes`) the original stacks at the bottom-right are not drawn at all.
5. **Signature unit lines are silent** (W6-05-06). "Kirov reporting", Tanya's and the Demo Truck's creation lines
   (`CreateSound`), aircraft crash impacts (`ImpactLandSound`) and the defence power-on/off hum (`WorkingSound` /
   `NotWorkingSound`) are never read.

The rest are MINOR: no health bar on mouse-over, an unbounded objective list, and a few missing tiles that need a
check against the original.

## Method

- **24 campaign maps.** For each map, `set campaign <MAP>` -> loading screen -> game. Shots were taken at:
  - the loading screen;
  - frames 0, 90, 1000 and 2200 with no input;
  - then `cheat reveal` plus views of houses 0..3 (buildings) and of the player's first unit.
  - `cheat art_dump` logged every building and unit image.
  - Scripts: `scratchpad/plays/<map>.play`. Shots: `scratchpad/shots/<map>/*.png` (not committed). The crops that
    matter are under `docs/gap/wave6/05-evidence/`.
- **Skirmish fight** (`skfight.play`).
  - AMCV deploy and a cheat-built base.
  - Mixed armies on both sides: Grizzly, GI, IFV vs Rhino, Conscript, Tesla Tank, Kirov.
  - A vet/elite cheat, a box select, building damage and a sell.
- **Tile census.** A small tool (`scratchpad/tools/flat.cpp`) linked against `libra2assets.a` resolves every map
  cell's TMP the same way `Theater::tmp_for_tile` does, and lists `MISSING` and `BADSUB` files.
  - `mixfind.cpp` searched every archive for those files.
  - `iniext.cpp` diffed `snow.ini` against `snowmd.ini`: the YR sets are appended only, so indices are safe.
- **Ghidra.** Headless, on a private copy `D:\ra2-re\w6-presentation` of `p4proj`, program `game.exe` (RA2).
  - Scripts `strhunt.py`, `fndump.py` and `dis.py` are in `D:\ra2-re\w6-presentation-out\`.
- **Not duplicated:** the post-merge regression list (ctl-all01, ctl-sov01, m-all03/06-real, m-sov07-real,
  m-sov09, the demo-abilities hash). Rows already in `REMAINING.md` are cited by ID, not re-reported.

## Gaps, ordered by how noticeable they are

### W6-05-01 Campaign house colours ignore the map's `Color=`: the player is gold, civilians are purple

| | |
|---|---|
| Severity / effort | **MAJOR** (every mission, first second) / **S-M** |
| Fix location | `engine/campaign_runtime.inc:686`; `engine/world.cpp:327-341,399-402,448`; `engine/world.hpp:1282` |

**What the player sees:**
- The Allied player's GIs, tanks, ConYard and landing craft carry **gold** stripes.
  - Evidence: `05-evidence/all03_player_units_gold.png` (ALL03 at night) and `all04_player_units_gold.png` (the
    ALL04 landing flotilla).
- The Soviet player's Kirovs in SOV12 are gold too.
- Every Paris, New York and Washington civilian building and car has **purple** remap trim and doors. Sample pixel
  (122,58,185) in `sov05_civilians_purple_not_grey.png`. The original's civilian house is `Grey`.
- Which enemy is "red" depends on where the house falls in the list.

**Evidence in the code:**
- `rec.color_ix = (int)(i % 8);` (campaign_runtime.inc:686): the house's index in the map's house list. `Color=` is
  never read.
- `pal_for_color` (world.cpp:327) only knows the 8 MP colours, so Grey, LightGold, LightBlue, Red, NeonBlue and the
  other 9 non-MP `[Colors]` schemes cannot be shown at all.
- The map data, for example:
  - ALL04: `[Player House] Color=DarkBlue`, `[Russian1 House] Color=DarkRed`, `[Civilian1 House] Color=Grey`.
  - SOV05: `[Civie1 House] Color=Grey` owns 38 CAMISC02 and 14 CAPARS05.
- What players actually get, as house index -> colour: 0 -> Gold (the player), 4 -> Purple.

**What the original does:**
- Each house section's `Color=` names a `[Colors]` entry. `rulesmd.ini [Colors]` lists LightGold, Gold, LightGrey,
  Grey, Red, DarkRed, Orange, Magenta, Purple, LightBlue, DarkBlue, NeonBlue, DarkSky, Green, DarkGreen, NeonGreen,
  Yellow, Purple2, Purple3, AlliedLoad and SovietLoad.
- Units and buildings are remapped through that ColorScheme. The ramp generator is already RE'd and implemented
  (`pal_for_hsv`, R-103, 0x68C110).
- [RE-FALLBACK: the HouseClass INI reader of `Color=` was not traced in this audit. The key semantics follow from the
  map data, and the clone's own skirmish path already uses the same scheme list.]

**Suggested fix:**
1. Resolve `Color=` to an HSV triple from `[Colors]` and store it on `World::HouseRec`, either as a scheme index into
   all `[Colors]` entries or as the raw HSV.
2. Make `house_palette` call `pal_for_hsv` with it.
3. Keep `color_ix` for the skirmish lobby.
4. Radar dots and other house-coloured UI already go through `house_palette`, so they follow automatically.
5. Bump the save format only if `HouseRec` grows.

### W6-05-02 Terrain tiles that live outside `iso*.mix` draw as black holes (rubble lots, overpasses)

| | |
|---|---|
| Severity / effort | **MAJOR** / **S** |
| Fix location | `engine/iso_renderer.cpp:93-114` (`Theater::tmp_for_tile`); theater set-up `engine/main.cpp:244-290` |

**What the player sees:** large black diamonds in the middle of revealed city blocks.
- In SOV01, on the very first Soviet mission, the black hole sits where the road overpass goes through the Pentagon
  wall (`sov01_overpass_black_hole.png`).
- In ALL04 there are two 4x4 black squares in the Soviet-held Chicago block (`all04_warub_black_holes.png`).
- ALL06 shows several, including one in front of the White House (`all06_warub_black_holes.png`).

**Evidence.** Tile census of the 24 maps. Every file is present in the theater art archive, not in the iso archive.
`mixfind` finds `warub*.urb` in `ra2.mix/urban.mix` and `Ovrpsb*.tem` in `ra2.mix/temperat.mix`.

| Map | Missing tile files (cells) |
|---|---|
| ALL03 | warub02 (24), warub09 (16) |
| ALL04 | warub03 (32) |
| ALL06 | warub01 (2), warub02 (36), warub05 (16), warub09 (16), warub10 (20), warub13 (9) |
| ALL08 | warub10 (4) |
| SOV01 | Ovrpsb11 (4), Ovrpsb16 (4); also MWater07.tem (54), which no archive contains (see W6-05-10) |
| SOV08 | warub02 (12), warub03 (16), warub10 (2), warub11 (9) |

`tmp_for_tile` looks a name up only in `mix_`, the iso archive (iso_renderer.cpp:101), and caches a miss as
`nullptr`. The terrain pass then draws nothing.

**What the original does:**
- Theater files are opened by name through the global MIX search list. `Init_Theater` registers `ISOURB.MIX` and
  `URBAN.MIX` (`src/asset_fs.cpp:21` cites `FUN_00519e70`), and a TMP name resolves from whichever registered
  archive holds it.
- [RE: asset_fs.cpp header -- Init_Theater FUN_00519e70 opens both the iso and the theater mix; the file search is
  over all open MIXes.]

**Suggested fix:**
1. Give `Theater` a fallback list in the same order as `asset_fs`: the theater art mix (`urban.mix`, `temperat.mix`,
   `snow.mix`), then `generic.mix`, then the md overlays.
2. Look up `entry_name` there before caching a miss.
3. The radar's `tile_colour` uses the same accessor, so the radar holes heal with it.

### W6-05-03 Dark diamond strips at every height step on lamp-lit snow and night maps

| | |
|---|---|
| Severity / effort | **MAJOR** (ALL02, ALL05, SOV07, SOV11: the whole map) / **M** |
| Fix location | `engine/pres_light.inc:192-262` (`PresLight::pass` with lit cells) |

**What the player sees:** staircase strips of flat, darker lavender diamonds along every cliff, ramp and plateau
edge on the snow missions that have street lamps (`all02_lightmap_gaps_snow.png`).

**Evidence:**
- Sampled pixels (ALL02, f0): normal snow (178,178,227) next to a strip (130,130,208). The ratio is
  (0.73, 0.73, 0.92), exactly the un-lit base `ambient x tint` against the lamp-lit value.
- The maps that show it are the four with light sources and a non-zero `[Lighting] Level`:

  | Map | Lamp sources | Lit cells | Level |
  |---|---|---|---|
  | ALL02 | 6 | 4497 | 0.028 |
  | ALL05 | 17 | 7367 | 0.028 |
  | SOV07 | 8 | 1800 | 0.027 |
  | SOV11 | 6 | 4343 | 0.028 |

- Snow maps with no Level term (ALL10, ALL12, SOV12) do not show it.
- The TMP art is not at fault: the slope and fixup tiles average (225,236,240), the same as `Clear01`.
- `pass()` clears the light map to the base colour, then draws every lit cell as a **flat 60x30 diamond at the
  cell's own height** (lines 237-249). Where a higher cell's diamond is lifted 15 px per level, the vertical faces
  and ramp faces below it are covered by no lit diamond, so the darker base shows through. Every height step
  becomes a dark strip.

**What the original does:**
- Light is per cell and baked into the palette the tile is drawn with: LightConvertClass per cell intensity and
  tint [RE: `FUN_00484090` cell intensity, `FUN_00555a30/00555d20` LightConvert, cited in the `pres_light.inc`
  header].
- A tile's whole image, including its cliff and slope extra, takes that cell's light, so there are no seams.

**Suggested fix:**
1. Apply the light-source term in the terrain pass: a per-cell colour mod (`SDL_SetTextureColorMod` on the tile blit,
   or the per-level palettes extended to per-cell tint buckets).
2. Apply objects' light at their own draw.
3. Drop the screen-space diamond map, or keep it only for objects.
4. Check against `anim-terrain.play` and the night missions.

### W6-05-04 Mission timer: the `TimerText` label is ignored and the timer is in the wrong place

| | |
|---|---|
| Severity / effort | **MINOR** (7 missions; the countdown loses its meaning) / **S** |
| Fix location | `engine/map_trigger.cpp:113,129` (action 103 is only labelled); `engine/hud_messages.inc:58-64`; `engine/main.cpp:7357-7363` |

**What the player sees:** "Time Remaining: 08:51" in the top centre of SOV04 (`sov04_timer_label_ignored.png`) and
SOV07 (`sov07_timer_label_ignored.png`). The original says what is coming. The labels, from `ra2.csf`:

| Mission | Label |
|---|---|
| ALL04 `MISSION:All04c` | "Estimated time until Psychic Amplifier is online:" |
| ALL08 `MISSION:All08T` | "Time until Beacon affects Tanya:" |
| SOV03 `MISSION:Sov03d` | "Beacon Deployment in:" |
| SOV04 `MISSION:sov04a` | "Time until Allied invasion begins:" |
| SOV06 `MISSION:Sov06b` | "Estimated time until Korean Fleet arrives:" |
| SOV07 `MISSION:Sov07l` (x6) | "Enemy Forces Arriving In:" |
| SOV10 `Name:Storm` | "Weather Storm" |

**Evidence:**
- Action 103 appears only in the name table and the `presentation_action` list. No handler stores its text.
  `draw_mission_timer` hard-codes `GUI:TimeRemaining` at `(playfield_w_/2, 8)`, centred.
- [RE: `game.exe` `FUN_006a45a0` (TacticalClass draw, Tactical.CPP)]:
  - When the scenario timer runs (Scenario+0x7C8 != -1), it calls `FUN_006a5280(frames/15, *(Scenario+0x7D4), ...)`.
  - `FUN_006a5280` formats `"%s  %02d:%02d"`, or `"%s  %d:%02d:%02d"` above one hour, from the label pointer at
    Scenario+0x7D4, which is the text TimerText sets.
  - It draws at `x = TacticalWidth - 3`, `y = TacticalHeight - (fontH + 2) * (line + 1)`: right-aligned at the
    **bottom-right** of the tactical view, stacking upward.

**Suggested fix:**
1. Handle action 103: store the CSF text of the action's string parameter in the trigger system, and save it.
2. Draw label + time bottom-right, right-aligned, with the stacking rule above.
3. The `GUI:TimeRemaining` default stays for timers with no TimerText.

### W6-05-05 Superweapon countdowns (`ShowTimer=yes`) are not shown on the battlefield

| | |
|---|---|
| Severity / effort | **MAJOR** in skirmish (players rely on seeing the enemy Nuke and Weather timers); MINOR in campaign / **S-M** |
| Fix location | new HUD draw next to `draw_mission_timer` (`engine/hud_messages.inc`), fed by the SW state in `engine/world_sw.cpp` |

**What the player sees:** nothing. Only the own sidebar cameo clock exists.

**Evidence:** `git grep ShowTimer engine` finds 0 readers. The SW design spec misread the key as "drives the cameo
clock" (`docs/superpowers/specs/2026-07-19-superweapon-design.md:417`).

**What the original does:**
- [RE: `FUN_006a45a0`, after the mission timer]: it loops over the global SuperClass list (`DAT_00a35dbc`, count
  `DAT_00a35dc8`).
- For each SW it computes the remaining time (`+0x34` TimeLeft minus elapsed since `+0x2C`), then calls
  `FUN_006a5280(frames/15, Type+0x60 (UI name), &blinkTimer +0x40, &blink +0x3C)`.
- At 0:00 the line blinks every 1000 ms (it toggles between the house colour and `*DAT_00ab6efc`). It is drawn in
  the **owner house's colour** and stacked above the mission timer, at the bottom-right.
- An entry is skipped while it sits at its full recharge and `+0x58` is set, unless `DAT_00a3d298`.
- `rules.ini` `ShowTimer=yes` on NukeSpecial, LightningStormSpecial, IronCurtainSpecial and ChronoSphereSpecial
  gates membership. [RE-FALLBACK: the list-insert site that tests type+0xF4 ShowTimer was not traced. The skip
  flag +0x58 is assumed to be "not yet charged-once".]

**Suggested fix:**
1. For every house's granted `ShowTimer` SW, one line "`<UIName>  mm:ss`" in that house's colour, right-aligned
   at the bottom-right.
2. Blink at 0:00.
3. Share the line stack with W6-05-04.

### W6-05-06 Unit and building sounds from rules keys the engine never reads

| | |
|---|---|
| Severity / effort | **MINOR** (iconic lines) / **S** |
| Fix location | `engine/pres_audio_events.inc` (the RE'd sound table), creation hook in `engine/production.cpp` / the factory exit path, crash landing in `engine/world_an.cpp`, power state in `engine/building_combat.cpp` |

**What the player hears:**
- No **"Kirov reporting"** when a Kirov leaves the factory.
- No Tanya, Chrono Commando or Demo Truck creation line.
- No crash impact when a Harrier, Black Eagle, Kirov, Rocketeer or the paradrop and cargo planes hit the ground:
  `CrashingSound` plays, but the impact does not.
- No power-on or power-off sound on the Tesla Coil, Prism Tower, Patriot, Grand Cannon and Flak Cannon when power
  drops or returns, and no Oil Derrick loop.

**Evidence:**
- A reader census of every `*Sound=` / `Voice*=` key in `rules.ini` against `engine/` and `src/`. These keys have
  **0 readers**: `CreateSound` (TANY, CCOMAND, DTRUCK, ZEP), `ImpactLandSound` (JUMPJET, ORCA, HIND, SHAD, ZEP,
  HORNET, ASW, PDPLANE, CARGOPLANE, BEAG), `WorkingSound` / `NotWorkingSound` (TESLA, ATESLA, NASAM, GTGCAN, NAFLAK,
  CAOILD), and the per-weapon/unit `DamageSound` (27 entries, all `Dummy`, so harmless).
- [RE: the strings are read by `FUN_006daf60` (TechnoTypeClass INI, `CreateSound` @0x6DBBC2, `ImpactLandSound`
  @0x6DBC34) and `FUN_00643e10` (`ImpactLandSound` @0x644968) in `game.exe`. `WorkingSound` / `NotWorkingSound`
  are read at 0x45A212 in the BuildingType loader. Consumers not traced: [RE-FALLBACK: CreateSound on Unlimbo from
  a factory, ImpactLandSound on the crash detonation, Working / NotWorking on the power-state change].]
- The SW `RechargeVoice` keys are covered by the EVA set.

**Suggested fix:** add the four keys to the sound-event table at the hook points above.

### W6-05-07 No health bar on mouse-over

| | |
|---|---|
| Severity / effort | **MINOR** / **S** |
| Fix location | `engine/main.cpp:3890` (pips only `if (u.selected)`); `engine/hud_pips.inc:71-86` |

**What the player sees:** hovering an unselected unit, own or enemy, shows no health. The original shows the pip row
without the selection board.

**Evidence:** `game.exe` `FUN_006c3f00` (DrawHealthBar) gates **only the PIPBRD board** on IsSelected (`+0x77`). The
pip loop runs unconditionally, so the function is also called for non-selected objects.
- [RE-FALLBACK: the caller condition (cursor-over object) is reached through the vtable slots at
  0x79B50C/0x79CF9C/... and was not traced. ModEnc/YR behaviour: the hovered object's bar is drawn.]
- The tail of the same function also calls vtable+0x3E4 (`DrawPips`: passengers, ammo and ore) at (x-10, y+10)
  when the player owns or spots the object. Check that the clone's cargo pips match that anchor.

### W6-05-08 Objective and Text-trigger lines: no cap, fixed 20 s lifetime

| | |
|---|---|
| Severity / effort | **MINOR** / **S** |
| Fix location | `engine/main.cpp:7345-7356` (`draw_campaign_overlay`) |

**What the player sees:** in ALL05, after a reveal, 17 lines of "Tesla Coil detected - CAUTION: EXTREME DANGER" and
"Allied patrol discovered" stack down the left of the screen (`all05_message_list_unbounded.png`). A spy or radar
reveal does the same in play.

**Evidence:**
- Objectives are drawn from `campaign_->objectives()` for 300 frames each, with no line cap.
- The HUD list (`hud_messages.inc:21`) caps at 6 and uses `[General] MessageDelay`, but TextTrigger lines bypass it.
- The original's message list is one list with a fixed number of slots and MessageDelay timeouts. [RE-FALLBACK:
  slot count, as already used by `kMaxMsgs`.]

**Suggested fix:** route TextTrigger lines through `post_message`, with the same cap and timeout.

### W6-05-09 (existing rows that are clearly visible in play; raise their priority)

- **R-145, building pip line.** The line floats a full building-height above small buildings. A selected
  power plant's bar sits over the ConYard roof (`skirmish_building_pipline_lift.png`). The lift is
  `art Height x 15` [RE-FALLBACK].
- **R-107, explored cliff extras not clipped by shroud.** Lone cliff slabs float in the black in ALL10 (f90, f1000).
- **R-068, skirmish LocalSize.** The jagged map edge is visible on the right of the skirmish battlefield (skfight
  k05-k10).
- **Kirov on the ground** (wave3 R7, never given an R-row). A cheat-spawned ZEP sat on the parking lot for the whole
  fight and was destroyed there. Real-production Kirovs in SOV12 do fly, with shadows (`h0u`). Check whether
  map-placed ZEPs in SOV12 start airborne. This is not re-tested here because it overlaps the units agent.

### W6-05-10 Tiles that no archive contains (check what the original shows)

| | |
|---|---|
| Severity / effort | **MINOR** / **S** |
| Fix location | `engine/iso_renderer.cpp` miss path |

- SOV01 has 54 cells of `MWater07.tem`, and no archive in the install contains it (`mixfind`).
- ALL01 has 2 cells whose subtile index exceeds `glat15.tem`'s image count (`BADSUB`).
- The unused `sov09t.map` has hundreds of `MDrod*` and `Mclif*` misses, so it is not playable as is.
- The clone draws nothing for these cells. The original most likely falls back to tile 0 (clear) through the
  IsoTileTypeClass default: the 0xFFFF sentinel rule in `src/map.cpp:43-54` is the same idea.
- [RE-FALLBACK: missing-image behaviour not traced.]

**Suggested fix:** draw tile 0 / subtile 0 for a miss, and log it once.

## Checked and found faithful (no gap)

- **Score per mission.** `[Basic] Theme=` starts the named theme. `No theme` (ALL01, ALL05, ALL12, SOV01, SOV08,
  SOV11) is silence until a PlayMusicTheme action. [RE: `FUN_0065bc70` -- `if (Scenario+0x11F8 == -1)
  Theme.Stop(1) else Theme.Queue_Song(...)`; `Stop` `FUN_006e81a0` sets pending = -1, and `Theme::AI`
  `FUN_006e7cd0` plays nothing on -1.] The clone matches (`campaign_audio.inc:13-21`).
- **Theater tile indices.** `snowmd.ini` and `urbanmd.ini` only **append** sets (SNOW: 798 -> 964 tiles, URBAN:
  1077 -> 1081), so RA2 maps keep their indices under the YR control INIs.
- **Visuals that look right:**
  - ALL01: Statue of Liberty position, Dreadnought missiles and trails, Patriot interception.
  - Gem and ore art, lamp light pools at night (ALL03), the radar boot, shroud on the radar, radar viewport.
  - Damage fires on buildings, the selection board and pips for units, elite chevrons.
  - The loading screens: side art, objectives, map and the three unit stills.
  - Power bar, sidebar tabs and cameos, and both sides' sidebar art. Each run is a fresh process, so R-009 was not
    re-tested.
- **Radar events in SOV12.** The many large rotating squares are the trigger-driven radar pings (`g_radar_pings`),
  not stale viewport rectangles (`sov12_radar_event_squares.png`).

## Prioritized fix list (one fix agent, about 2-3 days)

| # | Item | Sev | Eff | Files |
|---|---|---|---|---|
| 1 | W6-05-01 campaign house colours from `Color=` through `[Colors]` HSV (player blue/red, civilians grey) | MAJOR | S-M | `campaign_runtime.inc:686`, `world.cpp:327-402`, `world.hpp:1282` |
| 2 | W6-05-02 TMP lookup falls back to the theater, generic and md mixes (no more black holes) | MAJOR | S | `iso_renderer.cpp:93-114`, `main.cpp:244-290` |
| 3 | W6-05-03 lamp light baked per tile, not as flat screen diamonds | MAJOR | M | `pres_light.inc:192-262`, terrain draw in `iso_renderer.cpp` |
| 4 | W6-05-05 + W6-05-04 bottom-right timer stack: SW `ShowTimer` lines in house colour plus the mission timer with its `TimerText` label (action 103) | MAJOR / MINOR | S-M | `hud_messages.inc`, `map_trigger.cpp` (action 103), `world_sw.cpp` (read access) |
| 5 | W6-05-06 CreateSound, ImpactLandSound, WorkingSound and NotWorkingSound | MINOR | S | `pres_audio_events.inc` plus hook sites |
| 6 | W6-05-08 TextTrigger lines through `post_message` (cap 6, MessageDelay) | MINOR | S | `main.cpp:7345-7356` |
| 7 | W6-05-07 hover health pips (no board) | MINOR | S | `main.cpp:3890`, `hud_pips.inc` |
| 8 | W6-05-10 missing-tile fallback to tile 0 | MINOR | S | `iso_renderer.cpp` |

**Acceptance for the fix agent:**
- Re-run the `scratchpad/plays/*.play` shot scripts for ALL03, ALL04, ALL06, SOV01, ALL02, SOV04, SOV05 and
  SOV07, one at a time.
- Compare against the crops in `05-evidence/`.
- Add one `--play` script that asserts:
  - `house_color` of the campaign player, read through a new probe key;
  - the timer label text, read through a new probe key;
  - a non-null TMP for every map cell (`cheat tile_misses` == 0).
