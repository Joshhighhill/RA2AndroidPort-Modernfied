# Wave 2 gap analysis 05: in-world graphics, audio fidelity and render performance

Read-only audit of master `dfff02a`. I built it in my own worktree, `D:\ra2-wt\w2-gfx` (branch `w2-gfx`, default
config with no `CMAKE_BUILD_TYPE`, configured like `D:\ra2-android\build`). Then I drove the real `GameScene`
three ways:

- `--play` scripts through shell, lobby and game;
- `--scene-shot`;
- small benchmark tools. They link the *unmodified* engine objects, and no engine source was changed.

The original is RA2 Allied/Soviet. The install is the YR install at `D:\Red Alert 2`. Its RA2 data is `ra2.mix`,
`theme.mix` and `language.mix`.

This audit does not re-report wave-1 items that are now fixed. I verified each of the following in play first.

| Wave-1 item | Status now |
|---|---|
| EVA queue + 22 lines | Works. `EVA_UnitLost` plays `ceva064` with a real length (`afight` log). |
| Voices and `Report=` for every roster unit | Works. |
| AnimList muzzle and impact effects | Work: `S_CLSN16/22` and `PIFFPIFF` impacts, each with its `Report=`. |
| SHROUD.SHP soft edges | Work. |
| House colours for 8 players | Work. |
| Theater art for buildings and units | Works. |
| `[Terrain]`, overlays, smudges and neutral `[Structures]` | Drawn. |
| MOUSE.SHP cursors | Work. |
| Building ActiveAnim / Bib / damage fires / 50 % shadow half | Work. |

Some regressions and leftovers *inside* those fixes are listed below (W2-GA-01, 07, 09, 14).

Evidence lives in `docs/gap/wave2/05-evidence/`. It holds screenshots, crops, reference art rendered from the
original files with `ra2extract`, the benchmark sources, the perf CSVs and the Ghidra decompile. A private Ghidra copy
is at `D:\ra2-re\w2-gfx`, with output in `D:\ra2-re\w2-gfxout\`.

Status: WORKS / PARTIAL / WRONG / BROKEN / MISSING. Severity: BLOCKER / MAJOR / MINOR. Effort: S (< 1 d) / M (1–3 d) / L (> 3 d).

## 1. Gap table

### 1a. Terrain, shroud, map dressing, theaters

#### W2-GA-01: Ore and gems are invisible on every SNOW and URBAN map, including the default lobby map

| | |
|---|---|
| **Status / severity / effort** | BROKEN · BLOCKER · S |
| **Depends on** | — |

- **Original:** Ore is drawn from `tib01..20` / `gem01..12` in the theater's own extension. That is `.sno` / `.urb`
  via the `TheaterChain` (`economy.md §5`, `maps.md §6`).
- **Clone:** `load_ore_art` tries only `{".tem", ".shp"}` (`engine/main.cpp:313`).
  - The log on `hillbtwn.map` (URBAN, the lobby default) and on `xmp31s2.map` (SNOW) both say
    `[ore] art files loaded=0 … tib01=absent`.
  - On the default map the 102-overlay ore field around (54..60, 23..31) is in the sim but not drawn. See
    `05-evidence/urban_hillbtwn_base_hole_no_ore.png`.
  - TEMPERATE `xmp14t2` loads 32 files.
- **Fix sketch:** Use `"." + art_chain().ext` instead of the literal `.tem`.

#### W2-GA-02: Terrain leaks through the shroud on cliffs, ramps and raised ground

| | |
|---|---|
| **Status / severity / effort** | WRONG · MAJOR · S/M |
| **Depends on** | — |

- **Original:** An unmapped cell is black. The shroud tile is drawn at the cell's point minus (30,15) for every cell
  (`presentation.md §4`, `FUN_00480100`). No terrain of a shrouded cell is visible.
- **Clone:** `render_world` calls `draw_terrain(..., nullptr)` (`main.cpp:2989`). That draws *every* cell, including
  cliff and slope extra images. `draw_shroud_pass` (`pres_shroud.inc:94`) then lays 62×32 diamonds that cannot cover
  extra images rising above or below their cell.
  - Cliff walls, ramps and road strips show all over the black area.
  - Evidence: `temperate_shroud_cliff_leak.png` (xmp14t2), `urban_shroud_leak_oremine.png`,
    `xmp27t8_8p_perf_shroud_leak.png`.
  - This reveals the map layout before scouting.
- **Fix sketch:** Pass `vis` to `draw_terrain` again for the explored test only, so unexplored cells and their extras
  are skipped. Keep fog out of it (see W2-GA-03). Then draw the SHROUD.SHP edge post-pass as today.

#### W2-GA-03: Fog of war is applied in a game without fog of war

| | |
|---|---|
| **Status / severity / effort** | WRONG · MAJOR · S |
| **Depends on** | Combat/AI owners (targeting gate) |

Explored cells that are not currently visible are dimmed, and enemy units in them are hidden.

- **Original:** `FOG.SHP` is drawn only when `Scen->Flags & 0x1000` (FogOfWar) (`presentation.md §4`, `FUN_00480100`).
  `rulesmd [General] FogOfWar=no` is the default, and again in the MP map defaults (`shroud.md §4`). So in RA2
  skirmish, explored terrain stays full-bright and enemy units moving through explored ground are visible.
- **Clone:**
  - The fog pass always runs (`pres_shroud.inc:99-107`).
  - `draw_one_unit` hides every non-own unit whose cell is not `visible_at` (`main.cpp:3028`).
  - Impact anims are also gated on `visible_at` (`pres_game.inc` `update`).
  - Evidence: `urban_fog_dim_hpbars.png` shows the large dimmed areas and dark blotches inside the sight area.
  - `shroud.md §1/§5` modelled fog on OpenRA, not on the RA2 default.
- **Fix sketch:** Gate the fog pass and the unit, impact and projectile hiding on a `fog_of_war` scenario flag that
  defaults off. With fog off, use `explored_at` for objects. Flag targeting (`shroud.md §6`) to the combat/AI owners.

#### W2-GA-04: Black holes where a map's `IsoMapPack5` omits cells

| | |
|---|---|
| **Status / severity / effort** | WRONG · MAJOR · S |
| **Depends on** | — |

- **Original:** The map's cell array is initialised to clear ground (tile 0) for the whole map. `IsoMapPack5` records
  only overwrite it, so the original shows ordinary ground there. The exact init routine is RE-Q5.
- **Clone:** `MapFile::parse` creates only the records present (`src/map.cpp:40-90`), and `draw_terrain` draws only
  `map.cells()`.
  - `hillbtwn.map` lacks cells x=61..62 / y=28..31 (`--depth-dump hillbtwn.map 54 20 66 32` prints blanks there).
  - They render as a 2×4-cell black parallelogram next to the player's start, and its mirror sits near the AI base.
  - Evidence: `crop_missing_cells_hole.png`, `fight_080`.
  - Passability for those cells is also undefined.
- **Fix sketch:** Materialise every in-bounds cell of the `Size`/`LocalSize` diamond as tile 0, level 0 before
  applying the pack. This is the same rule as the 0xFFFF sentinel.

#### W2-GA-05: Map lighting is not applied

| | |
|---|---|
| **Status / severity / effort** | MISSING · MAJOR · M/L |
| **Depends on** | RE-Q7 |

Not implemented:

- `[Lighting] Ambient/Red/Green/Blue/Ground/Level`;
- per-height brightening;
- light sources (`LightVisibility/LightIntensity/*Tint` on lamps, ore mines and buildings);
- ion storm and Dominator lighting.

- **Original:** Tiles and terrain objects draw through the cell's LightConvert (`cell+0x34`) with a cell intensity
  (`cell+0x10c`) (`TerrainClass::Draw_It FUN_0071b450`, decompiled this wave at
  `05-evidence/tools/ghidra_TerrainClass_DrawIt_0071b450.c`). 139 of the 161 YR MP maps have non-neutral lighting.
  Examples: night maps at `Ambient=0.6` (xmp06t2, xnorest), `hillbtwn` at R/G/B .8/.8/.75, and per-level
  `Level=.035..0.039`.
- **Clone:** There is no reader (grep `Ambient` → only trigger enums `map_trigger.hpp:37`). Everything draws at
  full palette brightness: night maps look like noon, hills have no height shading, and the default map is about 20 %
  too bright.
- **Fix sketch:** Build per-(level, light) palette variants from `[Lighting]` (tiles, overlays, objects). Add a
  light-source pass later. Cache textures per light key.

#### W2-GA-06: Ore mines (`TIBTRE01..`, `SpawnsTiberium=yes`) use the wrong palette and are not animated

| | |
|---|---|
| **Status / severity / effort** | WRONG · MAJOR · S |
| **Depends on** | RE-Q1 (name of the convert) |

- **Original:** For `SpawnsTiberium`, `FUN_0071b450` swaps the convert to the global `[0x0087E63C]` instead of the
  cell's `+0x34`. It uses intensity `+0x10a` and draws 16 px higher. `IsAnimated` (`+0x2b3`) picks animation frames
  at `AnimationRate`.
  - The same SHP rendered with `uniturb.pal` / `unittem.pal` is clean.
  - With the iso or theater palette it is noise. See `art_oremine_palettes.png`.
- **Clone:** Every `[Terrain]` object uses palette 0, the iso palette (`pres_map.inc:43`). The ore mine shows as
  multicoloured noise, with no y-offset and no animation (`crop_oremine_wrong_palette.png`, `fight_045`).
- **Fix sketch:** For `SpawnsTiberium` types, use the theater unit palette, apply y −16 and step frames by
  `AnimationRate/AnimationProbability`.

#### W2-GA-07: High-bridge decks never draw

| | |
|---|---|
| **Status / severity / effort** | BROKEN · MAJOR · S (draw) / L (layer) |
| **Depends on** | G-49 high bridges |

- **Original:** The `BRIDGE1/BRIDGE2` (`BRIDGEB1/B2`) overlays carry `Image=BRIDGE` in **rulesmd** and draw from
  `temperat.mix/bridge.tem`. Units on the deck are drawn on the bridge layer (`world-depth.md B3`).
- **Clone:** `PresMap::art_shp` reads `Image=` only from **artmd** (`pres_map.inc:233`), so the bridge overlays
  resolve nothing.
  - `xmp14t2` logs `missing-art=22`, which is exactly 17 BRIDGE1 + 5 BRIDGE2 (overlay census tool `ovl.cpp`).
  - High bridges appear as a gap.
- **Fix sketch:** Fall back to the rules `Image=` for overlays. Then draw the deck with a bridge depth key.

#### W2-GA-08: Terrain objects, overlays and civilian buildings have no shadows

| | |
|---|---|
| **Status / severity / effort** | MISSING · MINOR · S |
| **Depends on** | — |

- **Original:** `TerrainClass::Draw_It` draws a second time with frame `+Frames/2` and flag `|1` (Darken) when
  `DAT_00821cf1` is set (`FUN_0071b450`). Tree SHPs carry the shadow half: for example `tree01.urb` frame 1 is a
  74×36 shadow (`art_tree_shadow_place_shp_oremine.png`).
- **Clone:** `PresMap::draw` uses only the first half (`pres_map.inc:177`). No tree, rock or civilian-building
  shadows show (`snow_no_tree_shadows.png`).
- **Fix sketch:** Reuse the building 50 % shadow path (`main.cpp:2569-2587`) for dressing objects and overlays.

#### W2-GA-09: The lobby offers YR-only theaters

| | |
|---|---|
| **Status / severity / effort** | PARTIAL · MINOR · S |
| **Depends on** | Skirmish owner |

- **Original:** RA2 has TEMPERATE, SNOW and URBAN only.
- **Clone:** The catalog is YR's `missionsmd.pkt`, which carries 14 DESERT and 13 NEWURBAN maps. Their structures
  fall back to temperate art (README "Theater art").
- **Fix sketch:** Filter the catalog to RA2 theaters, or flag these maps (skirmish owner).

### 1b. Units, buildings, effects

#### W2-GA-10: The Guardian GI is invisible

| | |
|---|---|
| **Status / severity / effort** | BROKEN · MAJOR · S |
| **Depends on** | — |

- **Original:** `ggi.shp` ships only in `ra2md.mix/conqmd.mix` on this install (`[GGI] Sequence=GuardianGISequence`).
- **Clone:** `load_unit_shp` searches `conquer.mix` only (`main.cpp:1094`, used at `main.cpp:4807`). The log says
  `[unit-roster] infantry SHP GGI unavailable`. The Allied player can build the GGI (tech 2, GAPILE) but it draws
  nothing (`main.cpp:3040` skip).
- **Fix sketch:** Search `conqmd.mix` then `conquer.mix`, the same order `PresFx::load_shp` already uses.

#### W2-GA-11: SHP vehicles (`Voxel=no`) are not supported

| | |
|---|---|
| **Status / severity / effort** | BROKEN · MAJOR · M |
| **Depends on** | RE-Q14 |

These are the Terror Drone `DRON` (176 frames), the Dolphin `DLPH` (232) and the Giant Squid `SQD` (296).

- **Original:** They are SHP units with `WalkFrames/FiringFrames` and 8 facings (artmd `[DRON] Voxel=no`, `[DLPH]`,
  `[SQD]`).
- **Clone:**
  - `load_vehicle_assets` fails (`[vehicle] assets unavailable (hull vxl/hva missing (dron))`).
  - A roster entry with `vehicle=true` is then dropped (`main.cpp:1568-1571`).
  - The demo roster routes DRON through the *infantry* path with infantry sequences (`main.cpp:1653`).
- **Fix sketch:** Add an SHP-vehicle draw path: frame = facing·WalkFrames + step, with `StartWalkFrame`/`Firing`. Keep
  `is_vehicle` sim semantics.

#### W2-GA-12: Ground vehicles cast no shadow

| | |
|---|---|
| **Status / severity / effort** | MISSING · MINOR · M |
| **Depends on** | RE-Q9 |

- **Original:** The VoxLib shadow renderer `FUN_00755680` plots each projected column in colour 1, which is shadow, at
  ground level (`voxel-render.md` fn table).
- **Clone:** `draw_vehicle_unit` draws hull, turret and barrel only (`main.cpp:2656-2700`). Only airborne units get a
  flat blob (`main.cpp:2684`, `iso_renderer.cpp:364`). See `crop_vehicles_no_shadow.png`.
- **Fix sketch:** Rasterise a shadow sprite per facing in `build_facing_set` (z-flattened projection) and blit it at
  50 % before the hull.

#### W2-GA-13: Defence turrets are not drawn

| | |
|---|---|
| **Status / severity / effort** | MISSING · MAJOR · M |
| **Depends on** | G-40 (fire facing) |

- **Original:** rulesmd sets `Turret=yes TurretAnim=SAM/FLAKTUR/GTGCANTUR/LASER`, `TurretAnimIsVoxel=true`,
  `TurretAnimX/Y/ZAdjust` (NASAM, NAFLAK, GTGCAN, NALASR), and `TurretRotateSound`.
- **Clone:** There is no `TurretAnim` anywhere in `engine/` (grep). Patriot and Flak Cannon bases render
  without turrets, and nothing rotates.
- **Fix sketch:** Load the turret VXL as a `VehicleAssets`-style facing set and draw it at the building anchor plus
  `TurretAnimX/Y`, facing the defence's target.

#### W2-GA-14: Building anims beyond ActiveAnim are missing

| | |
|---|---|
| **Status / severity / effort** | PARTIAL · MINOR · M |
| **Depends on** | RE-Q13 |

- **Original:** BuildingType has 21 anim slots (`presentation.md §6`). They include:
  - `SuperAnim*` (charge states of NAMISL, GACSPH, NAIRON);
  - `SpecialAnim` (refinery ore dump GAREFNOR);
  - the war-factory door set `DeployingAnim/UnderDoorAnim/RoofDeployingAnim/UnderRoofDoorAnim` (GAWEAP_1..4);
  - `IdleAnim`.

  Each slot has its own `ZAdjust` / `YSort`.
- **Clone:** `kSlots` covers only `ActiveAnim..Four` (`pres_fx.inc:137`). All slots draw on top of the building, and
  `*ZAdjust/*YSort` are ignored. The war-factory door never opens and superweapons show no charge state.
- **Fix sketch:** Extend the slot table, and drive Super/Special/Deploying from SW charge, unload and unit-exit
  events.

#### W2-GA-15: Beams, trails, debris, craters and SW effects are missing

| | |
|---|---|
| **Status / severity / effort** | MISSING · MAJOR · L |
| **Depends on** | RE-P5 (residual) |

Missing: Tesla bolts, Prism beams, radiation glow, smoke trails, debris, crater and scorch smudges, chrono warp and
the Iron Curtain tint.

- **Original:** The data uses weapon `IsElectricBolt` (Tesla coil and trooper), `IsLaser`/Prism, `[Radiation]` and
  `RadColor`, projectile `Trailer=`, `DebrisTypes`, anim `Crater=yes` (S_CLSN16/22), chrono in/out anims, and the
  Iron Curtain palette.
- **Clone:** None of these keys are read (grep). Tesla and Prism shots show only an impact. Radiation fields are
  invisible although the sim has `RadLevel` (`main.cpp:975-1037`).
- **Fix sketch:** Add a line or arc beam renderer and a `Trailer` spawner in `PresFx`. Make `Crater=` spawn a smudge.
  Tint radiation cells. Add chrono anims from `ChronoIn/OutAnim`.

#### W2-GA-16: Infantry death variety, prone and idle are missing

| | |
|---|---|
| **Status / severity / effort** | MISSING · MINOR · M |
| **Depends on** | RE-W9 |

- **Original:** Warhead `InfDeath` selects Die1, Die2, explode, flame, electro, melt and so on (anims such as
  `ELECTRO` and `FLAMEGUY`). Sequences also cover Prone/Crawl under fire, Idle1/2 and Cheer
  (`infantry-anim.md §6`).
- **Clone:** Only `Die1` (`world.cpp:30-36` notes). There is no prone, idle, cheer or `InfDeath` anim.
- **Fix sketch:** Map `InfDeath` to a sequence or anim, and add a prone flag when fired upon.

#### W2-GA-17: Draw order has no layer system

| | |
|---|---|
| **Status / severity / effort** | WRONG · MINOR · M |
| **Depends on** | RE-Q2, RE-Q10 |

- **Original:** Objects live in layers Ground/Air/Top (YRpp `Layer`). Anims carry `Layer=` (GUNFIRE `Layer=ground`)
  and `YSort`/`ZAdjust`. Flying aircraft and the Kirov draw after ground objects.
- **Clone:**
  - Aircraft are merged with ground units by `fx+fy` (`main.cpp:3071`), so they can be hidden by a nearer building.
  - The whole FX layer draws after every object (`main.cpp:3094`), so an explosion behind a building draws over it.
  - Map walls, fences and sandbags draw in the ground pass under all units (`pres_map.inc:155`). A unit behind a
    fence is drawn over it.
- **Fix sketch:** Add a layer index to the merged draw list. Give FX a depth key with `YSort`. Put walls and fences in
  the object merge.

#### W2-GA-18: The placement grid is SDL outlines instead of `PLACE.SHP`

| | |
|---|---|
| **Status / severity / effort** | WRONG · MINOR · S |
| **Depends on** | — |

- **Original:** Vanilla RA2 shows only the foundation grid, drawn with `conquer.mix/place.shp` (38 frames, filled
  60×29 diamond) (`art_tree_shadow_place_shp_oremine.png`). The building-image ghost is an Ares addition, so the
  wave-1 "add a ghost image" suggestion should be dropped.
- **Clone:** `draw_placement_cursor` draws one-pixel green or red diamond outlines (`main.cpp:4025-4060`,
  `crop_placement_outline.png`).
- **Fix sketch:** Blit `place.shp` frames per cell (ok/blocked frame) translucently.

#### W2-GA-19: Building health bars

| | |
|---|---|
| **Status / severity / effort** | WRONG · MINOR · S |
| **Depends on** | HUD owner |

- **Original:** The bar shows only for a selected building. It uses pips and the `ConditionYellow/Red` thresholds.
- **Clone:** A bar is drawn whenever a building is damaged, using the hardcoded thresholds `2/3` and `1/3`
  (`main.cpp:2606-2620`). G-88 fixed this for units only. Unit selection is still a green diamond with a solid bar.
- **Fix sketch:** Draw only when selected and use the rules thresholds. Pip art is shared with the HUD owner.

#### W2-GA-20: The house remap ramp is a heuristic

| | |
|---|---|
| **Status / severity / effort** | UNVERIFIED · MINOR · M |
| **Depends on** | RE-Q6 |

- **Original:** The ramp comes from `[Colors]` HSV, but the original's ColorScheme ramp generator was not RE'd. RA2's
  own `rules.ini` has `Gold=41,240,230`, not YR's `43,239,255`.
- **Clone:** `pal_for_color` builds a generic HSV→RGB ramp scaled by source brightness from 8 hardcoded YR triples
  (`world.cpp:323-370`).
- **Fix sketch:** RE the ramp. Read `[Colors]` from the rules in use instead of hardcoding.

### 1c. Audio

#### W2-GA-21: There is no in-game music on this install, and the playlist is YR's

| | |
|---|---|
| **Status / severity / effort** | BROKEN · MAJOR · S/M |
| **Depends on** | — |

- **Original:** RA2 plays `theme.ini` / `theme.mix`: Grinder, Power, Fortification, InDeep, Tension, EagleHunter,
  IndustroFunk, 200Meters, BlowItUp, Destroy, Burn, Motorized and HM2.
  - `Next_Song` honours `Side=`, `IsScoreShuffle` and `IsScoreRepeat` (`presentation.md §2`, High).
  - With shuffle off, the first available track is played.
- **Clone:** The clone loads `thememd.ini` and `thememd.mix` (`pres_audio.inc:113,150`).
  - On this install every `thememd.mix` entry is zero-filled: `brainfre.wav` has 5,330,092 bytes and all of them are
    0. The same holds for drok, deceiver and tactics.
  - The log says `[pres-audio] theme BrainFre failed: wav: not a RIFF WAVE`, so the game is silent.
  - `theme.mix` is intact (`grinder.wav` starts with `RIFF`) and is used only by the menu.
  - Playlist order ignores `Side=`, shuffle and repeat (RE-FALLBACK comment `pres_audio.inc` `music_start`).
- **Fix sketch:** For the RA2 target, use `theme.ini` and `theme.mix`, falling back to md only if the entry is
  non-zero. Implement `IsAvailable`/`Next_Song` as recovered. Read `IsScoreShuffle/Repeat` from RA2.INI.

#### W2-GA-22: The music track is decoded in full on the main thread and its PCM is never freed

| | |
|---|---|
| **Status / severity / effort** | WRONG · MAJOR · M |
| **Depends on** | W2-GA-21 |

- **Original:** The theme is streamed (`ThemeClass +0x2c` stream).
- **Clone:** `play_track` runs `load_wav_ima` on the whole file, then downmixes and resamples on the update thread
  (`pres_audio.inc:339`), and adds the result to the `SoundBank` permanently.
  - Measured on the engine library (`wavtime.cpp`): grinder 443 ms, hm2 689 ms, fortific 728 ms of decode.
  - Each track keeps 6.5–10.7 MB.
  - The audio queue holds only 200 ms (`audio_sdl.hpp:15`), so every track change is a visible hitch plus an audio
    underrun. Memory grows by about 100 MB over the playlist.
- **Fix sketch:** Stream the IMA ADPCM in chunks, either on the audio pump or on a worker thread. Free the previous
  track.

#### W2-GA-23: The mixer is mono with no pan, attenuation is latched and clipping is hard

| | |
|---|---|
| **Status / severity / effort** | WRONG · MINOR · M |
| **Depends on** | RE-Q11 |

- **Original:**
  - `FUN_0074f8e0` returns volume **and pan**.
  - Attenuation is re-evaluated every frame on live voices, and a sound that leaves range is stopped
    (`audio-playback.md §3`).
  - Music is stereo.
- **Clone:**
  - The device is opened as 22050 Hz mono (`audio_sdl.cpp:14-16`).
  - All clips, music included, are downmixed (`pres_audio.inc:49`).
  - Gain is fixed at trigger (`audio_mixer.cpp` `play`).
  - Voices are summed and clamped with no headroom (`audio_mixer.cpp` `render`).
  - `render()` erases finished voices once per *sample* (`audio_mixer.cpp:182`).
- **Fix sketch:** Use a stereo S16 sink with a pan law from screen x. Update gain and pan per frame from the camera.
  Use a soft limiter or headroom. Retire voices once per buffer.

#### W2-GA-24: The sound-event `Control`/`Type` semantics are incomplete

| | |
|---|---|
| **Status / severity / effort** | PARTIAL · MINOR · M |
| **Depends on** | — |

- **Original:** These are recovered in `audio-playback.md §2-§4`:
  - VShift and FShift (pitch and volume jitter);
  - `Delay` / `PREDELAY`;
  - `LOOP`, `ALL`, `ATTACK/DECAY` (intro/body/outro);
  - `AMBIENT`;
  - `Type=SHROUD/LOCAL`.

  In soundmd, 218 events use FShift, 153 VShift, 64 Delay, and about 50 use loop, all or attack/decay.
- **Clone:** Only `random`, `interrupt`, `global`, `Limit` and `Range` are wired (`pres_audio.inc` `ensure_sound`).
  `Report=A,B` always plays the first entry.
- **Fix sketch:** Add the recovered formulas to `MixerSound` and `Mixer::play`, plus a loop handle API for ambient
  and move sounds.

#### W2-GA-25: Many unit and world sounds are never played

| | |
|---|---|
| **Status / severity / effort** | MISSING · MAJOR · M |
| **Depends on** | W2-GA-24 (loops) |

- **Original:** rulesmd defines `VoiceFeedback` (133 units), `MoveSound` (50), `CrushSound` (107),
  `VoiceSpecialAttack` (59), `DeploySound`/`UndeploySound`, `VoiceEnter/Deploy/Capture/Harvest`, `ImpactWaterSound`,
  `SinkingSound`, `CrashingSound`, `TurretRotateSound`, and `[AudioVisual]` `CheerSound`, `BuildingRepairedSound`,
  `BuildingGarrisonedSound`, `Crate*Sound` and `CreditTicks`.
- **Clone:** The grep count is 0 for every key except `LeaveTransport/EnterTransportSound`, `PlaceBuilding` and
  `SellSound`.
  - Vehicles have no engine sound.
  - Crushes are silent.
  - Tanks hitting water use the land explosion.
- **Fix sketch:** Resolve these keys in `add_voices` and play them on the matching World events. Move sounds need a
  loop voice.

#### W2-GA-26: Many EVA lines are never triggered

| | |
|---|---|
| **Status / severity / effort** | PARTIAL · MAJOR · M |
| **Depends on** | SW/AI owners |

- **Original:** `evamd.ini [DialogList]` has 472 entries. The skirmish-relevant ones that never fire are:
  - `NuclearSiloDetected`, `IronCurtainDetected/Activated`, `ChronosphereDetected/Activated`,
    `WeatherDeviceReady`, `LightningStormCreated/Ready` (SW warnings for *enemy* superweapons);
  - `OreMinerUnderAttack`, `OurAllyIsUnderAttack`;
  - `PrimaryBuildingSelected`, `NewRallyPointEstablished`;
  - `BuildingOffLine/OnLine`;
  - `UnitRepaired`, `TechBuildingCaptured/Lost`, `BuildingInfiltrated*`;
  - `BridgeRepaired`, `UnableToComply`;
  - the crate upgrade lines `Armor/Firepower/SpeedUpgraded`.
- **Clone:** The engine references 22 `EVA_*` names (grep). `superweapon_eva` falls through to
  `EVA_ChronosphereReady` for every type that is neither nuke nor Iron Curtain (`pres_game.inc:656`), so Weather
  Control would say "Chronosphere ready". "Launched" fires for the nuke only.
- **Fix sketch:** Add the trigger points (house SW-built edge for *detected*, enemy SW fire, ore-miner damage with
  RadarEvent suppression, rally and primary clicks, power edges). Map SW types to their own lines.

### 1d. Render performance

All perf numbers are measured with the accelerated renderer (`direct3d`, visible window) while five other agents shared
the CPU. Treat them as ±20 %.

#### W2-GA-P1: `draw_terrain` sorts the whole map and builds string keys every frame

| | |
|---|---|
| **Status / severity / effort** | WRONG · MAJOR · S/M |
| **Depends on** | — |

- **Original:** The original redraws only dirty cells, from cached per-cell draw info.
- **Clone:** Each frame `draw_terrain` does all of the following:
  - it fills and `std::sort`s an index array of *all* cells (`iso_renderer.cpp:121`);
  - it calls `Theater::tmp_for_tile` → `entry_name` → `std::string` and a hash-map lookup per cell;
  - it builds two `snprintf` string keys per drawn tile and looks them up by string (`iso_renderer.cpp:171, 200`).

  Measured with `bench.cpp` on the unmodified engine objects:

  | Map | Build | `draw_terrain` average | Max |
  |---|---|---|---|
  | XMP27T8 (34,177 cells, 566 tiles visible) | default `-O0` | 60–84 ms | 208 ms |
  | XMP27T8 | `-O2` | 24 ms | — |
  | XMP14T2 (9,810 cells), camera off-map, 0 tiles drawn | `-O0` | 11.7 ms | — |

  On top of this, `draw_shroud_pass` runs two passes, and `draw_ore` and the overlay pass loop over every cell
  every frame.
- **Fix sketch:**
  - Build the painter-ordered cell list once per map (terrain is static) as `{screen rect, SDL_Texture*, extra*}`.
  - Iterate only the visible diagonal band.
  - Key the cache by integer `(tile, subtile)`.
  - Do the same for shroud, ore and overlays: compute the visible cell range once per frame.

  The target is under 2 ms.

#### W2-GA-P2: The default desktop build is unoptimised

| | |
|---|---|
| **Status / severity / effort** | WRONG · MAJOR · S |
| **Depends on** | Android owner |

- **Original:** Not applicable.
- **Clone:**
  - `README.md` "Building" runs `cmake -S . -B build -G Ninja` with no `CMAKE_BUILD_TYPE`, so the build uses no `-O`.
  - `D:\ra2-android\build` has an empty `CMAKE_BUILD_TYPE`.
  - Android `.cxx` holds only `Debug` outputs (`android/app/.cxx/Debug/...`).
  - Terrain draw is 2.5–3.5× slower at `-O0` (P1). IMA decode and voxel rasterisation are also `-O0`.
- **Fix sketch:** Default `CMAKE_BUILD_TYPE` to `RelWithDebInfo` in CMakeLists and document it. Make the APK a release
  ndkBuild.

#### W2-GA-P3: Frame time in an 8-player game is 92–122 ms (8–11 fps)

| | |
|---|---|
| **Status / severity / effort** | WRONG · MAJOR · M |
| **Depends on** | AI owner (update half) |

The game had 74–97 units (house 0..7: 9, 15, 16, 16, 10, 6, 10, 15 at frame 2423) on XMP27T8 with seven Brutal AIs.

- **Original:** The original runs 15 Hz logic and renders at the frame rate.
- **Clone:** `perf-r1.play` (render every frame) against `perf-r0.play` (same deterministic game, no render):

  | Frames | r1, update + render (ms/frame) | r0, update only (ms/frame) |
  |---|---|---|
  | 150–600 | 106 | 43 |
  | 600–1200 | 92 | 39 |
  | 1200–1800 | 102 | 56 |
  | 1800–2400 | 122 | 92 |

  - Render is therefore about 30–60 ms, mostly terrain (P1).
  - **Logic alone exceeds the 66.7 ms 15 Hz budget once the AIs grow.** The 200-unit target was not reached in the
    measured window; extrapolation suggests it is worse.
  - CSVs: `05-evidence/tools/perf_r{0,1}.mem.csv`.
- **Fix sketch:** Do P1 and P2 first. Profile `World::step` and the AI per house (hand off to the AI/sim owner).
  Decouple render from logic: fixed 15 Hz accumulator plus interpolation.

#### W2-GA-P4: Several work items run synchronously and cause hitches

| | |
|---|---|
| **Status / severity / effort** | WRONG · MAJOR · M |
| **Depends on** | W2-GA-22 |

- **Original:** The original caches voxels per facing on first draw, but in 8-bit software, which is cheap.
- **Clone:**
  - **Voxel facing sets are rasterised lazily per (type × house colour) inside the draw call** (`vxl_set`,
    `main.cpp:2643-2653`). Measured with `vxbench.cpp` (`-O0` engine objects): GTNK 51 ms, HTNK 54 ms, MTNK 69 ms per
    type-colour. An 8-player game produces dozens of these the first time each colour's tank appears on screen.
  - **Theme track decode:** 0.44–0.73 s (W2-GA-22).
  - **First-use tile textures:** the first frame takes 77–85 ms (cache fill), and scrolling into new terrain repeats
    this.
  - **The audio queue is 200 ms and is pumped only from `update`**, so any frame over 200 ms underruns.
- **Fix sketch:**
  - Pre-rasterise every roster vehicle for every in-game colour on the loading screen, or on a worker thread.
  - Pre-warm the theater tile textures at load.
  - Move audio to an SDL callback thread.

#### W2-GA-P5: Memory use is high and caches never evict

| | |
|---|---|
| **Status / severity / effort** | WRONG · MAJOR (Android) / MINOR (desktop) · M |
| **Depends on** | Android owner |

- **Original:** The original loads MIX files lazily with file-offset reads.
- **Clone:**
  - Working set is 573–592 MB in game, and private bytes peak at 724 MB during load, oscillating between 500 and
    720 MB as whole MIX files are read and freed (`perf_r1.mem.csv`).
  - Every loader calls `MixFile::open(read_file(...))`, which reads whole archives (ra2.mix 281 MB, ra2md.mix 205 MB,
    langmd 85 MB, language 53 MB, thememd 47 MB).
  - `open_nested` copies the nested bytes (`src/mix.cpp:144-147`).
  - `TextureCache` has no eviction (`texture_cache.hpp`). Keys are per frame × palette × colour, so they grow with
    every new unit type, colour and anim.
  - Steady state over 2400 frames was flat (+6 MB).
- **Fix sketch:** Use offset-based MIX reads (`fseek` or mmap), and share one opened `ra2.mix`/`ra2md.mix`. Add an
  LRU or budget to `TextureCache`.

#### W2-GA-P6: Frame pacing

| | |
|---|---|
| **Status / severity / effort** | WRONG · MINOR · S |
| **Depends on** | P3 |

- **Original:** The original uses a fixed-rate loop.
- **Clone:**
  - The renderer is created without `SDL_RENDERER_PRESENTVSYNC` (`platform.cpp:43`), so it tears.
  - `run_scene_driver` adds a fixed `SDL_Delay(16)` *after* the work (`main.cpp:4317`), so 16 ms of the frame budget
    is lost on every frame.
  - The logic tick is variable-dt (`main.cpp:4304`).
- **Fix sketch:** Use vsync, or a sleep-until-deadline, instead of a fixed 16 ms.

## 2. Perf measurements (raw)

| Measurement | Value | How |
|---|---|---|
| `draw_terrain`, XMP27T8, `-O0` engine objects | 60.4–84.4 ms avg, 86–208 ms max (120 frames) | `05-evidence/tools/bench.cpp` (links `build/CMakeFiles/ra2engine.dir/engine/*.obj`) |
| `draw_terrain`, XMP27T8, iso_renderer and texture_cache at `-O2` | 23.8 ms avg, 55.7 ms max | same, `iso_renderer.cpp`/`texture_cache.cpp` recompiled `-O2` |
| Full-map index sort alone, 34k cells, `-O2` | 4.9–5.7 ms | bench |
| Three full-map cell loops (shroud ×2 + ore), projection and cull only, `-O2` | 1.3–1.7 ms | bench |
| First frame (tile texture cache fill) | 77–85 ms | bench |
| `draw_terrain`, XMP14T2, camera off-map, `-O0` | 11.7 ms (0 tiles drawn) | bench |
| Whole frame, 8-player XMP27T8, 74–97 units, render every frame | 92–122 ms | `perf-r1.play`, `RA2_PLAY_VISIBLE=1` (direct3d) |
| Same game, logic only | 39–92 ms | `perf-r0.play` (`render_every 100000`) |
| Voxel facing set (32 facings, hull + turret + barrel) per type × colour | 51–69 ms | `vxbench.cpp` |
| Theme IMA decode (147–243 s stereo track) | 443–728 ms, 6.5–10.7 MB mono PCM kept | `wavtime.cpp` |
| Working set in game / private peak during load | 573–592 MB / 724 MB | `perf_run.ps1` 1 Hz sampling |
| Asset start → game frame 0 (8-player map) | ~35–45 s | perf CSV (`last_frame` first non-empty) |

## 3. Prioritised fix plan and file hotspots

1. **Correctness of what the player sees.** These are all S or S/M and touch no gate hashes.
   - W2-GA-01 ore extension: `main.cpp:313`.
   - W2-GA-04 missing cells: `src/map.cpp:40-100`. Fill the diamond before the pack.
   - W2-GA-02 shroud leak: `main.cpp:2989` and `iso_renderer.cpp` `draw_terrain` explored test.
   - W2-GA-03 fog-off: `pres_shroud.inc:99`, `main.cpp:3028`, `pres_game.inc` impact gate.
   - W2-GA-10 GGI art: `main.cpp:1094`, `4807`.
   - W2-GA-06 ore-mine palette: `pres_map.inc:30-45`.
   - W2-GA-07 bridge `Image=` from rules: `pres_map.inc:233`.
2. **Audio that is silent today.**
   - W2-GA-21 RA2 `theme.ini`/`theme.mix` playlist with `Next_Song`: `pres_audio.inc:113-160, 225-245`.
   - W2-GA-22 streaming decode: `pres_audio.inc:330-360`, `audio_mixer.*`.
   - W2-GA-25 / W2-GA-26 missing sound keys and EVA triggers: `pres_game.inc` (`add_voices`, `unit_audio`,
     `superweapon_eva`, `building_events`).
3. **Performance.**
   - P2 default build type: `CMakeLists.txt`, README.
   - P1 cached terrain draw list: `iso_renderer.cpp` `draw_terrain` and `Theater::tmp_for_tile`, plus
     `pres_shroud.inc`, `draw_ore` (`main.cpp:2739`), `PresMap::draw_ground`.
   - P4 pre-rasterise voxel colour sets at load: `main.cpp:2637-2655`, `gf_scene.inc`.
   - P5 offset MIX reads and texture LRU: `src/mix.cpp`, `src/fileio.cpp`, `engine/texture_cache.*`.
   - P6 pacing: `platform.cpp:43`, `main.cpp:4317`.
   - P3: hand the logic half to the AI/sim owner.
4. **Look and feel.**
   - W2-GA-05 lighting: map reader, palette variants keyed by light into `TextureCache`.
   - W2-GA-12 vehicle shadows: `facing_cache.cpp`, `voxel_raster.cpp`.
   - W2-GA-08 dressing shadows: `pres_map.inc:170-200`.
   - W2-GA-13 defence turrets: new building turret path in `draw_building`, `main.cpp:2488`.
   - W2-GA-14 anim slots: `pres_fx.inc:130-170`.
   - W2-GA-15 beams, trails and craters: `PresFx`.
   - W2-GA-11 SHP vehicles: `main.cpp:1180`, `1568`, new draw path.
5. **Polish.**
   - W2-GA-23 / W2-GA-24 stereo, pan and Control semantics.
   - W2-GA-16 infantry deaths.
   - W2-GA-17 layers.
   - W2-GA-18 `place.shp`.
   - W2-GA-19 health bars.
   - W2-GA-20 remap ramp.
   - W2-GA-09 lobby theaters.

## 4. Open RE questions

- **RE-Q1:** Which ConvertClass is the global `[0x0087E63C]` that `TerrainClass::Draw_It` (`FUN_0071b450`) uses for
  `SpawnsTiberium` types? The visual test says it is the theater unit palette.
  - What are cell `+0x10a` and `+0x10c`? They look like terrain and object intensity.
  - Which option sets `DAT_00821cf1`, the terrain-shadow gate?
- **RE-Q2:** Draw order of overlays (walls, fences, sandbags) against objects. Does `CellClass` draw the overlay in
  the cell pass, under every object, or is it Y-sorted with objects?
- **RE-Q3:** With `FogOfWar` off, confirm that `TechnoClass` visibility for drawing and targeting tests only the
  *mapped* bit (`cell+0x12C & 0x08`). The fog bit (`cell+0x140`) should matter only when `Scen->Flags & 0x1000`.
- **RE-Q4:** Does `CellClass::Draw_It` skip unmapped cells, and how are cliff extra images clipped against neighbour
  shroud? This is needed for W2-GA-02.
- **RE-Q5:** The `MapClass` init that fills cells absent from `IsoMapPack5`: confirm tile 0 at level 0 for the whole
  `Size` diamond.
- **RE-Q6:** The ColorScheme ramp generator from `[Colors]` HSV, that is the remap of indices 16–31 for units and
  buildings.
- **RE-Q7:** `[Lighting]` to per-cell LightConvert: how Ambient, Ground, Level and RGB combine, and how light sources
  (`LightVisibility/Intensity/RedTint...`) accumulate.
- **RE-Q8:** The trigger for `VoiceFeedback`: on damage, on a failed order, or something else.
- **RE-Q9:** Voxel shadow (`FUN_00755680`): projection offset, blend (Darken 50 % as for SHPs?) and the aircraft
  shadow at altitude.
- **RE-Q10:** Layer assignment: aircraft in flight and Kirov in the Air layer, anim `Layer=` default, and how
  `YSort`/`ZAdjust` map onto the clone's depth key.
- **RE-Q11:** The pan law in `FUN_0074f8e0`, and whether music and EVA are stereo or centred.
- **RE-Q12:** The `ThemeClass` scan (`FUN_0071fa90`): does `Exists` check `theme.mix` as well as `thememd.mix` when
  the md entry is unusable? This decides whether W2-GA-21 is "use RA2 data" or "fallback".
- **RE-Q13:** BuildingClass slot semantics for `SuperAnim` (charge stages), `SpecialAnim` (refinery dump) and the
  war-factory door anims. Which events start them, and in what order?
- **RE-Q14:** SHP vehicle (`Voxel=no`) frame layout: `Facings`, `WalkFrames`, `FiringFrames`, `StartWalkFrame`,
  `StartFiringFrame`, and turret frames for SHP turrets.

## 5. Method notes

- **Screens:**
  - `vis-base.play`: lobby default `hillbtwn.map` (URBAN), MCV deploy, the four core buildings, army, 8,400 frames.
  - `--scene-shot --script fight` on `hillbtwn.map`.
  - `--peek terrain` on `xmp31s2.map` (SNOW) and `xmp14t2.map` (TEMPERATE).
  - `perf-r1.play` on `xmp27t8.map`.

  `--scene-shot --script tanks` is broken since the MCV start, because `spawn_tanks` needs an own building. It renders
  pure shroud (`shots/tem_tanks_*.png`), which is a harness issue, not a game one.
- **Audio** was run with `SDL_AUDIODRIVER=dummy --audio` so clips really load. In `--play` and no-audio runs the
  `EVA clip … unavailable` lines are an artefact of `want_clips=false`, not a gap.
- **Asset checks** used `ra2extract` on the original mixes:
  - `place.shp`, `tree01.urb` (frame 1 = shadow), `tibtre01.urb` under five palettes, `gi.shp` (744 frames; the
    second half is empty);
  - `ggi.shp` (conqmd only), `dron.shp`, `dlph.shp`, `sqd.shp`;
  - the `thememd.mix` zero-fill check.
- **Ghidra:** private project `D:\ra2-re\w2-gfx`. Scripts are `fx.py`, `scan.py` and `fnat.py` under
  `D:\ra2-re\w2-gfx\scripts`.
  - `SpawnsTiberium` is TerrainType `+0x2b1`, read by `FUN_0071d140`.
  - `TerrainClass::Draw_It` is `FUN_0071b450`.

## 6. Fix status (branch `w2fix-gfx`)

Fixed on `w2fix-gfx` (merged with master 4a93ed6, w2fix-ui). Screens come from `scripts/play/gfx-vis.play`,
`scripts/play/gfx-fx.play` and `--scene-shot` (new `--view X,Y` / `--reveal` options). They are local only; renders
of original art are not committed. Audio evidence is the `--scene-shot ... --audio` presentation log with
`SDL_AUDIODRIVER=dummy`.

### RE answers (gamemd.exe, private Ghidra copy `D:\ra2-re\w2-gfx`, outputs `D:\ra2-re\w2-gfxout\q1..q17`)

| Question | Answer | Confidence |
|---|---|---|
| RE-Q1 | `[0x0087E63C]` is assigned once at start-up (`FUN_0052b790` @0x52bf2b) from the convert built from the unit palette, so `SpawnsTiberium` terrain draws through the **unit palette**. `DAT_00821cf1` is initialised to 1 and never written, so terrain and tree shadows are always drawn. | Medium-High |
| RE-Q3 | `MapClass::IsLocationFogged` (`FUN_00586010`) is a stub that returns 0. The tactical draw loop (`FUN_006d8320`) culls only on that fog test, and only under `Scen & 0x1000`. The mouse pick (`FUN_00691d00`) rejects only shrouded cells (`!(cell+0x12C & 8)`) plus the stub. The auto-acquire evaluator (`FUN_006f71d0`) has no shroud or fog test at all; it checks only cloak and sensors. FogOfWar comes from `[MultiplayerDialogSettings] FogOfWar` (or map `[SpecialFlags]`) and is cleared in campaign. `[General] FogOfWar` is never read. | High |
| RE-Q5 | Before IsoMapPack5 is applied, `FUN_004acd80` pre-fills every cell with the first Clear tile at `[Map] Level=` (default 0). `[Map] Fill=Water` uses random water tiles instead. | High |
| RE-Q7 | `FUN_00689890` reads the values. `FUN_00484090` computes I = Ambient·1000 + Level·1000·height − Ground·1000 (max 2000). `FUN_00555570` normalises the tint to a max channel of 1000 and moves the brightness into I. The `LightConvertClass` rows scale each palette channel by I·tint. Light sources add `Intensity·(r−d)/r`. | High (formula), Medium (row pick) |
| RE-Q11 | `FUN_0074f8e0`: pan = clamp(screenX − halfW, ±W)·8192/W + 8192, which is 4096 or 12288 at the screen edges. It is not affected by the vertical position. EVA and music never call it, so they are centred. | High |
| RE-Q12 | The YR binary reads THEMEMD.INI and opens THEMEMD.MIX, then THEME.MIX if that open fails. `Next_Song` is `FUN_0071fd20` and `Is_Allowed` is `FUN_007203e0` (rules quoted in `pres_audio.inc`). Classic RA2 is the theme.ini / theme.mix pair. | High |
| RE-Q14 | `FUN_00746450` reads the art and `FUN_00745f00` holds the defaults. `FUN_0073b520` picks the frame as `facing group = ((((raw>>12)+1)>>1)+1)&7`, with `Walk·g + StartWalk + WalkCounter%Walk` when moving, the standing frame or Walk·g when idle, and `Fire·g + StartFire + counter/2` when firing. | High |
| RE-Q2, Q4, Q6, Q8, Q9, Q10, Q13 | Not resolved. Each fix that depends on one is marked RE-FALLBACK below. | — |

### Rows

| Row | Status | What changed | Evidence |
|---|---|---|---|
| W2-GA-01 | **FIXED** | `load_ore_art` searches `.<theater ext>` then `.shp`. The ore texture key is tagged by theater, so a second game in another theater never reuses the wrong palette. | `[ore] art files loaded=32 pal=urban.pal` / `snow.pal`; `gfx-vis/v01_start.png` (hillbtwn ore field) |
| W2-GA-02 | **FIXED** | `draw_terrain(…, &world)` skips every never-explored cell, base and cliff/slope extra alike (RE-Q4 open: no neighbour clipping). The SHROUD.SHP post-pass is unchanged. | `--scene-shot --map xmp14t2 --peek terrain`: the cliff walls and ramps in the black are gone |
| W2-GA-03 | **FIXED** | `World::set_fog_of_war` defaults to off (RE-Q3). The one query `World::object_visible_at` returns explored with fog off and visible with fog on. It feeds `seen_by_mask` (unit and defence auto-acquire), the building-defence engage test (`combat.cpp`), unit and projectile drawing, the cursor pick, impact and fire FX, and the radar. The fog pass and terrain dimming run only with the flag set. `--demo-shroud` opts into fog. Note for the combat/AI owners: the binary's auto-acquire has **no** shroud test at all; ours still needs an explored cell. | `gfx-fx`: an enemy in explored but out-of-sight ground is drawn and engaged |
| W2-GA-04 | **FIXED** | `MapFile::parse` adds every missing cell of the IsoMapPack5 enumeration diamond as tile 0 at `[Map] Level` (RE-Q5). The enumeration matches every full map exactly. Test `map: fill_missing_diamond…`. | hillbtwn: 11 cells filled, the black parallelogram by the start is gone |
| W2-GA-05 | **FIXED (light sources open)** | `pres_light.inc` applies the RE-Q7 formula. Terrain is baked per height level, and the level-0 ambient·tint is applied as a MOD multiply over the world sprites. Not modelled: light sources, ion storm and Dominator lighting, and per-height light on objects. | xmp06t2 (Ambient .6, night), xmp14t2 (warm .73), hillbtwn (.76 at level 0) shots |
| W2-GA-06 | **FIXED** | `SpawnsTiberium` terrain uses the unit palette (RE-Q1), is lifted 16 px, and animates `IsAnimated` frames at `AnimationRate`/`AnimationProbability` on a render-only RNG. | hillbtwn ore mine is clean (crop) |
| W2-GA-07 | **FIXED (bridge layer open)** | Overlay art takes `Image=` from rules when artmd has none, so `BRIDGE1/2` resolve to `BRIDGE`. Non-ore, non-wall overlays (rocks, bridges, low bridges) are drawn with the **iso** palette (visual check). Decks draw in the ground pass; there is no bridge layer for units yet. | xmp14t2 `missing-art=0` (was 22); `--view 89,100` deck shot |
| W2-GA-08 | **FIXED** | `[Terrain]` objects and civilian dressing draw their shadow half at 50 % (`DAT_00821cf1` is always on). | snow xmp31s2 tree shadows |
| W2-GA-10 | **FIXED** | `load_unit_shp` falls back to `ra2md.mix/conqmd.mix`. | `[shp] image=GGI file=conqmd.mix/ggi.shp frames=744`; `gfx-vis/v02_units.png` |
| W2-GA-11 | **FIXED** | `Voxel=no` vehicles (every `[VehicleTypes]` entry) use the RE-Q14 frame rule. The sim is unchanged; DRON stays on the infantry path and takes its facing from `u.facing`. An unregistered unit type now loads its SHP the first time it is seen instead of being invisible. | `gfx-vis/v02_units.png` (Terror Drone) |
| W2-GA-12 | **FIXED (RE-FALLBACK RE-Q9)** | `rasterize_shadow` builds the hull silhouette flattened to z=0 at 50 % black, per facing, under each ground vehicle. There is no light offset. | `--script tanks` crop |
| W2-GA-13 | **FIXED (turret ROT open)** | `pres_turret.inc` draws the `TurretAnim` voxel (plus `barl`) per house colour at `TurretAnimX/Y`, facing `def_target`. Patriot, Flak, Sentry and Grand Cannon are covered. | `gfx-vis/v03b_patriot.png` |
| W2-GA-14 | **PARTIAL** | The war-factory `DeployingAnim` plays over and `UnderDoorAnim` under when a vehicle exits. `SuperAnim..Four` run on READY/FIRE edges (RE-Q13 open, ModEnc order). Still missing: `SpecialAnim` (refinery dump), the roof door set, `IdleAnim`, and `ZAdjust`/`YSort` per slot. | `gfx-vis/v03a_door.png` |
| W2-GA-15 | **PARTIAL** | `IsElectricBolt` gives three jagged arcs and `IsLaser` gives a house-coloured beam (`IsHouseColor`), for units and defences. `Crater=` / `Scorch=` anims leave `CRATERnn` / `BURNTnn` smudges. Radiation shows as a `RadColor` glow. Still missing: debris, chrono warp, the Iron Curtain tint, and `Trailer=` (no active `Trailer=` in rulesmd/artmd on this install). | `gfx-fx/f01_beams.png`, `gfx-fx/f03_after.png` |
| W2-GA-16/17/18/20, W2-GA-09 | **OPEN** | Polish rows. Not started. | — |
| W2-GA-19 | **FIXED** | The live game no longer draws the always-on foundation HP bar. Only the selected building shows health, using the w2fix-ui pips and ConditionYellow/Red. | `gfx-vis` shots |
| W2-GA-21 | **FIXED** | theme.ini comes from ra2.mix/local.mix and the tracks from theme.mix, with the thememd pair as fallback. Each entry is checked as a real RIFF/WAVE (`IsAvailable`). `Next_Song` and `Is_Allowed` follow RE-Q12, with `IsScoreShuffle`/`IsScoreRepeat` from RA2.INI. The w2fix-ui jukebox (Sound page) plays through the same stream. | `[pres] f=1 music Grinder sound=Grinder src=theme.mix setup_ms=0.22` |
| W2-GA-22 | **FIXED** | `WavImaStream` (block decoder; test: its concatenated blocks equal `load_wav_ima`) feeds `MusicStream`, a worker thread with a 2 s ring that resamples to 22050 stereo. The game thread only starts the stream: 0.1–0.3 ms, was 443–728 ms. No track PCM is kept. | presentation log `setup_ms`; tests `mixer: MusicStream …` |
| W2-GA-23 | **FIXED** | Stereo SDL callback sink (2048 frames). RE-Q11 pan law. Live positional voices are re-attenuated every frame and stopped out of range. Soft limiter. Voices retire once per buffer. | tests `mixer: render_stereo pans…`, `…soft-limits…`, `update_voice…` |
| W2-GA-24 | **FIXED (Delay/AMBIENT/SHROUD open)** | VShift, FShift (playback rate), ALL, ATTACK, DECAY and LOOP in `Mixer`, using the audio-local RNG. `Report=A,B` picks one per shot. | test `mixer: FShift…LOOP` |
| W2-GA-25 | **PARTIAL** | Added: `MoveSound` (move-start one-shot), `CrushSound` (crushed infantry squish plus crusher), `ImpactWaterSound` on water cells. Still missing: VoiceFeedback, Deploy/Undeploy, VoiceEnter/Capture/Harvest, Sinking/Crashing, TurretRotate, Cheer, BuildingRepaired, Garrisoned, Crate*, CreditTicks. | `[pres] sound GrizzlyTankMoveStart (move start)` |
| W2-GA-26 | **PARTIAL** | Each SW type now has its own Ready line; Weather says `EVA_LightningStormReady`, not Chronosphere. Added the enemy *Detected* lines (`EVA_WeatherDeviceReady` is the weather-detected warning), the launch/activation lines heard by every player, `EVA_OreMinerUnderAttack` (RadarEvent 4) and `EVA_OurAllyIsUnderAttack` (RadarEvent 16 plus the siren). Still missing (controls/UI hooks): PrimaryBuildingSelected, NewRallyPoint, BuildingOff/OnLine, UnitRepaired, TechBuildingCaptured/Lost, BridgeRepaired, UnableToComply, crate upgrades. | code; `superweapon_eva` |
| W2-GA-P1 | **FIXED** | `MapIndex` (painter order and depth rows) and `TerrainDrawList` (screen rects plus pinned per-(tile, subtile, level) textures) are built once per map. Shroud, ore and radiation walk only the visible rows. `MapFile::at` is O(1). Dressing objects are pre-sorted. `PresMap::draw` keeps texture pointers instead of building per-frame string keys. | `bench.cpp`, XMP27T8: 39.2 ms → **0.43 ms** (`-O0`) / **0.10 ms** (`-O2`) |
| W2-GA-P2 | **FIXED (Android build types: Android owner)** | CMake defaults to `RelWithDebInfo`. `Release` and `Debug` are documented in the README. | gate hashes identical at `-O0` and `-O2` |
| W2-GA-P3 | **PARTIAL** | The render half is fixed (see the table below). The logic half (World::step / AI) belongs to the skirmish-match owner. A 15 Hz accumulator with interpolation is not done. | perf table |
| W2-GA-P4 | **FIXED** | Voxel colour sets are prewarmed on a worker thread at game start (`vxl_set` is locked). Tile textures are prewarmed when the draw list is built. Music is streamed. Audio runs on the SDL callback thread. | `vxbench` at `-O2`: 9–20 ms per set (was 51–69), off the draw path |
| W2-GA-P5 | **FIXED** | `MixFile::open_path` uses file-offset reads, nested **views** and one shared handle per path; there are no whole-archive reads. `TextureCache` has an LRU budget (384 MB), pinning and a two-frame safety. | private peak at game start ~200 MB (was 724 MB); see the table below |
| W2-GA-P6 | **FIXED** | Vsync renderer, plus a deadline sleep instead of a fixed 16 ms after the work. | — |

Harness: `--scene-shot --script tanks|fight` now anchors on the MCV start (the first unit when there is no building),
the camera follows the player's engaged units, and the `--view X,Y` and `--reveal` options were added.

### Perf after the fixes (same method as section 2, one run at a time)

| Measurement | Before (audit, `-O0`) | After (`w2fix-gfx`, `RelWithDebInfo`) |
|---|---|---|
| `draw_terrain`, XMP27T8, 566 tiles | 60–84 ms (39 ms on this box, `-O0`) | 0.43 ms (`-O0` objects), **0.10 ms** avg, 0.23 ms max (`-O2`) |
| First terrain frame (tile cache fill) | 77–85 ms | 79 ms, once at list build (prewarms **every** tile the map uses) |
| Whole frame, 8-player XMP27T8, render every frame (`perf-r1.play`, visible) | 92–122 ms | **15.9–17.8 ms** (vsync-capped at 60 Hz). House 0 had 9 units and house 1 had 16 at f=2400 |
| Same game, logic only (`perf-r0.play`) | 39–92 ms | 4.6–9.4 ms |
| Voxel set per type × colour | 51–69 ms on the draw path | 9–20 ms on a worker thread |
| Theme start on the game thread | 443–728 ms, 6.5–10.7 MB PCM kept | 0.1–0.3 ms, nothing kept |
| Working set / private peak, 8-player game | 573–592 / 724 MB | 339 / 332 MB |
| Asset start to game frame 0 (8 players) | ~35–45 s | ~8 s |
