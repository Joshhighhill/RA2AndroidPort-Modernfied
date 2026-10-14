# Wave 7 / 12: character presentation across all units

Scope: the presentation of every unit type reachable in Allied and Soviet play (campaign and skirmish) on master `b3a509f`. That is 59 buildable roster types, plus TANY and BORIS, plus the spawned and support types (HORNET, V3ROCKET, DMISL, ASW, PDPLANE, BPLN, SPYP).

The audit covers:
- infantry SHP sequences and deaths;
- voxel drawing and death/wreck effects;
- selection feedback;
- voices and EVA;
- weapon FX and beams;
- palette and house-colour remap.

Read-only audit. Method: calibrate-first against retail YR data (rulesmd/artmd/soundmd/evamd), Ghidra on gamemd.exe (YR 1.000), and targeted `--play` runs.

The work was split into five slices (A-E) run by opus sub-auditors and merged here:
- **A:** infantry sequences and deaths;
- **B:** voxel vehicles and death effects;
- **C:** selection feedback;
- **D:** voices and EVA;
- **E:** weapon FX, beams and remap.

Evidence is in `docs/gap/wave7/12-evidence/{A..E}/`. Raw RE dumps and play scripts are in `D:\ra2-re\w7-pres-{A..E}-out` and `-scripts`. The Ghidra project copies were deleted.

Tags:
- `[CONFIRMED-RE <addr>]` gamemd.exe;
- `[CONFIRMED-DATA]` YR ini/mix data;
- `[CONFIRMED-PLAY]` an `--play` run on b3a509f;
- `[SUSPECTED]`.

Already known and only cross-referenced: per-type sounds keyed by image (W7-01-12 / W7-05-03 / W7-09-01; W7-12-28 gives the concrete list), W7-01-21, W7-01-35, W7-01-43, W7-02-05/06, W7-03-05/06/07/12, W7-04-25/27/28, W7-05-07/09, W7-06-04/10/12.

## Summary

**46 gaps: 7 MAJOR, 27 MINOR, 11 COSMETIC, 1 INFO (RE detail for a known item).**

The core art pipelines are faithful:
- the infantry art sequence tables, with all frame ranges valid against the real SHPs;
- the DoControls rates and facing blocks;
- the voxel body/turret/barrel parts and HVA rotor frames;
- the health bar (DrawHealthBar 0x6F5A00);
- the chevron frames;
- the house-colour ramp and unit palettes;
- the AnimList/SplashList picks;
- the soundmd parse.

What is wrong is mostly the layer above the art: what gets triggered, keyed or overlaid.

- **Voices and EVA (MAJOR)**
  - "Unit ready" is never spoken; production plays "Canceled" instead (W7-12-01).
  - "Unit lost" fires for every V3 rocket, Dreadnought missile, Hornet and MiG (W7-12-02).
  - With real audio on, the GI, Conscript and Rhino lose deploy, fear, move-start and crush sounds. The audio-off harness hides this (W7-12-03).
- **Selection overlays (MAJOR)**
  - No control-group number on any unit (W7-12-04).
  - Passenger pips are placeholder rectangles (W7-12-05).
  - No ore, spawn, self-heal or mind-control overlays (W7-12-16/17/22/23).
  - Chevrons leak through the shroud and sit on the ground under flying units (W7-12-19/20).
- **Beams (MAJOR)**
  - The Chrono Legionnaire beam is green and straight; every rad beam should be a 15-frame sine wave, and the Chrono beam light blue (W7-12-06).
  - Prism split lasers and elite Tesla bounce bolts are invisible, though their damage applies (W7-12-07).
- **Deaths (MINOR)**
  - Non-weapon kills (radiation, superweapons) always play Die1 plus a corpse (W7-12-08).
  - Ships slide away in 1 s instead of sinking under a clipped waterline (W7-12-11).
  - Aircraft explode on the ground when hit and vanish silently on impact (W7-12-12).
  - Death-weapon blasts (Demo Truck DEMTEXP) are not drawn (W7-12-13).
  - Units that die on water show the land fire explosion (W7-12-14).
- **Firing presentation (MINOR)**
  - Only PrimaryFireFLH is read, so the Prism Tank and IFV fire from their centre, and there is no barrel alternation (W7-12-15).
  - The vehicle 8-way muzzle anim is 45° off (W7-12-29).
  - Secondary and elite impacts, projectiles and trails come from the primary (W7-12-30, extends W7-03-06).
  - Smaller beam defects: bolt life, alternate colour and sparks (W7-12-31), Prism laser colour (W7-12-32), and UseNormalLight explosions dimmed at night (W7-12-33).

Suggested fix order:
1. W7-12-01/02/03, which are cheap and heard constantly.
2. W7-12-04/05, together with the rest of DrawPips (W7-12-16/17/18/22, one function).
3. W7-12-06/07.
4. W7-12-15 + W7-12-29 + W7-12-30, which share a per-weapon FX/FLH key with W7-03-06.
5. The death set W7-12-08/11/12/13/14.
6. The rest.

## Per-unit coverage matrix

Cells hold W7-12 gap numbers (`#05` = W7-12-05), with other reports' IDs in parentheses. **OK** = checked and faithful; **n/a** = not applicable to the kind.

Gaps that apply to whole classes are not repeated per row:
- every selectable unit: W7-12-04 (group number, listed), W7-12-19 (shroud leak, enemy units) and W7-12-25/26 (voice channel and speaker);
- every infantry: W7-12-08 for non-weapon kills, and W7-12-37;
- every infantry and non-organic vehicle: W7-12-22 (self-heal pip);
- every voxel ground unit: W7-12-34 (ramp tilt) and W7-12-42 (Rocker);
- every row: W7-12-01 ("Unit ready", listed).

The per-slice detail tables, with what was expected and what was drawn per unit, are in the appendix.

| id | kind | A infantry seq/death | B voxel/death | C selection | D voices/EVA | E weapon FX/remap |
|---|---|---|---|---|---|---|
| E1 | inf | #08 #30 #37 #38 | n/a | #04 #05 | #01 #03 | OK |
| E2 | inf | #08 | n/a | #04 | #01 #03 | OK |
| SHK | inf | #08 | n/a | #04 | #01 | #07 #31 |
| ENGINEER | inf | OK | n/a | #04 | #01 #27 | OK |
| JUMPJET | inf | #40 (W7-01-21) | n/a | #04 #20 | #01 #28 (W7-05-03) | (W7-01-21) |
| GHOST | inf | #09 #35 | n/a | #04 | #01 #28 (W7-01-12) | OK |
| YURI | inf | #36 #38 | n/a | #04 #23 | #01 | #23 #30 |
| IVAN | inf | OK | n/a | #04 (W6-04-11) | #01 | (W6-04-11) |
| DESO | inf | #08 (W7-02-05) | n/a | #04 | #01 | #06 (W7-02-05) |
| DOG | inf | OK | n/a | #04 | #01 | OK |
| CLEG | inf | (W7-01-44) | n/a | #04 | #01 | #06 (W7-01-44) |
| SPY | inf | OK | n/a | #04 #46 | #01 | OK |
| CCOMAND | inf | OK | n/a | #04 | #01 | OK |
| PTROOP | inf | #38 | n/a | #04 #23 | #01 | #23 |
| CIVAN | inf | OK | n/a | #04 | #01 | OK |
| SNIPE | inf | OK | n/a | #04 | #01 | OK |
| TANY | inf | #09 #35 | n/a | #04 #19 | #01 | OK |
| FLAKT | inf | #38 | n/a | #04 | #01 | #30 |
| TERROR | inf | #08 | n/a | #04 | #01 | OK |
| SENGINEER | inf | (W7-01-12) | n/a | #04 | #01 #27 | OK |
| ADOG | inf | OK | n/a | #04 | #01 | OK |
| YENGINEER | inf | (W7-01-12) | n/a | #04 | #01 #27 | OK |
| GGI | inf | #30 | n/a | #04 #45 | #01 | #30 |
| BORIS | inf | #10 (W7-02-06) | n/a | #04 | #01 (W7-02-06) | (W6-04-24) |
| YDOG | inf | OK | n/a | #04 | #01 | OK |
| YADOG | inf | OK | n/a | #04 | #01 | OK |
| AMCV | veh | n/a | #34 | #04 | #01 | OK |
| HARV | veh | n/a | #15 | #04 #16 | #01 | #30 |
| APOC | veh | n/a | #15 #41 #43 | #04 #19 | #01 #28 | #30 (W7-03-06, W7-04-28) |
| HTNK | veh | n/a | #41 | #04 | #01 #03 | #30 (W7-03-06) |
| SAPC | veh | n/a | #14 | #04 #05 | #01 #28 | OK |
| MTNK | veh | n/a | #41 | #04 | #01 | #30 (W7-03-06) |
| CARRIER | naval | n/a | #11 | #04 #17 | #01 | OK |
| V3 | veh | n/a | (W7-04-27) | #04 #17 | #01 #02 | (W7-04-27) |
| ZEP | air(veh) | n/a | #12 #13 (W7-05-09) | #04 #20 | #01 | #30 |
| DRON | veh | n/a | OK | #04 #21 | #01 #24 (W7-04-30) | OK |
| HTK | veh | n/a | #15 #41 | #04 #05 | #01 | #30 |
| DEST | naval | n/a | #11 (W7-06-12) | #04 #17 | #01 | OK |
| SUB | naval | n/a | #14 | #04 #19 | #01 | OK |
| AEGIS | naval | n/a | #11 | #04 | #01 | (W7-06-11) |
| LCRF | naval | n/a | #14 | #04 #05 | #01 | OK |
| DRED | naval | n/a | #11 #15 (W7-06-12) | #04 #17 | #01 #02 | OK |
| SHAD | air(veh) | n/a | #12 #44 | #04 #05 #20 | #01 | #29 |
| SQD | naval | n/a | n/a | #04 #22 | #01 | #29 #30 |
| DLPH | naval | n/a | n/a | #04 | #01 | (W7-06-10) |
| SMCV | veh | n/a | #34 | #04 | #01 | OK |
| TNKD | veh | n/a | OK | #04 | #01 | #30 (W7-03-06) |
| TTNK | veh | n/a | OK | #04 | #01 | #07 #31 |
| CMIN | veh | n/a | OK | #04 #16 | #01 | OK |
| SREF | veh | n/a | #15 (W7-03-07) | #04 | #01 | #07 #32 (W7-03-07) |
| HYD | naval | n/a | #14 | #04 | #01 | #30 |
| MGTK | veh | n/a | OK | #04 #46 | #01 | OK |
| FV | veh | n/a | #15 (W7-03-05) | #04 #05 | #01 | #30 (W7-03-05, W7-03-06) |
| DTRUCK | veh | n/a | #13 #43 | #04 | #01 #28 | OK |
| BFRT | veh | n/a | (W7-03-12) | #04 #05 | #01 | OK |
| SCHP | air(veh) | n/a | #12 #15 #44 (W6-04-17) | #04 #20 | #01 | #29 #30 |
| ROBO | veh | n/a | OK | #04 | #01 (W7-03-10) | OK |
| ORCA | air | n/a | #12 | #04 #18 #20 (W7-05-07) | #01 #24 #28 | OK |
| BEAG | air | n/a | #12 | #04 #18 #20 (W7-05-07) | #01 #24 | OK |
| HORNET | spawn | n/a | #12 #15 | #18 | #02 | #30 |
| V3ROCKET | spawn | n/a | OK | n/a | #02 | OK |
| DMISL | spawn | n/a | OK | n/a | #02 | OK |
| ASW | spawn | n/a | #12 #15 | #18 | #02 | OK |
| PDPLANE | spawn | n/a | #12 | n/a | #02 | OK |
| BPLN | spawn | n/a | #12 #13 #15 | n/a | #02 | OK |
| SPYP | spawn | n/a | #12 #13 #44 | n/a | #02 | OK |

## Gaps

Ordered by severity. Each gap gives Evidence, Original, Clone, Code (file:line on b3a509f) and Fix, plus cross-references. "slice X" names the sub-audit it came from.

### W7-12-01: "Unit ready" is never spoken; "Canceled" is spoken when a production run ends (MAJOR; slice D)

- **Evidence**
  - [CONFIRMED-PLAY play1, `12-evidence/D/play1-voices-eva.txt`] 5 GIs and 3 Grizzlies were queued from the sidebar and all were built. The results were `evareq(EVA_UnitReady)=0` (both asserts FAIL), `eva(EVA_Canceled)=2` and `evareq(EVA_Training)=2`.
  - [CONFIRMED-RE presentation.md §1.3, FUN_004FAF00 @4FB45F] In the original, UnitReady fires when a produced unit exits its factory, for the local house only, gated by `RadarEvent::Create(6 UnitProduced)` (2 cells, 200 frames). EVA_Canceled (FUN_006AA390 @6AA4C4) fires only from a right-click abandon.
- **Original:** "Unit ready" for every unit that leaves a factory, except one exiting within 2 cells and 200 frames of the previous one. "Canceled" only on a cancel.
- **Clone:** `production_eva` (`engine/pres_game.inc:834-856`) waits for a render frame that observes `slot.ready`.
  - **The ready slot is never observed.** The same `World` step runs `tick_all` (sets ready), `econ_launch_ready_units` (launches and `placed()`) and `econ_refill` (`engine/world_econ.cpp:182-202, 209-258`). The ready state therefore exists for zero frames.
  - **A drained queue sounds like a cancel.** When the last unit leaves, the slot goes from type to -1 with `p.ready == false`, which takes the "Canceled" branch.
  - **Repeated same-type builds are invisible.** These keep the same `slot.type`, so no transition is seen at all.
  - **Two queues are skipped.** The loop is `c < 4`, so the Naval and Aircraft queues never say "Training" or "Unit ready".
  - `main.cpp:6517` already receives the launch list and discards it (`(void)world_.take_econ_launches(); // UnitReady is observed from the slots`).
- **Code:** `engine/pres_game.inc:834-856`; `engine/main.cpp:6517`; `engine/world_econ.cpp:209-258`.
- **Fix:**
  - Pass `take_econ_launches()` to the presentation. For each `(house == g_local_house, uidx)`, if `radar_event(6, unit fx/fy)` succeeds, say `EVA_UnitReady`.
  - Say `EVA_Canceled` only from the player's abandon command (an explicit cancel note from `econ_produce`/the sidebar), not from slot emptying.
  - Run the Training edge over all 6 queue categories.

### W7-12-02: "Unit lost" is spoken for Spawned=yes objects (V3 rockets, missiles, Hornets, MiGs) (MAJOR; slice D)

- **Evidence**
  - [CONFIRMED-RE 0x4D97D0 @4D981C] `FootClass` death speaks EVA_UnitLost only when the type's `+0xD54` byte is 0. That byte is written from the `Spawned` key by the TechnoType loader [CONFIRMED-RE FUN_00711510 @0x714210-0x714231: `PUSH "Spawned"` … `MOV [EBP+0xD54],AL`]. The other gates are IsPlayerControl and RadarEvent 7.
  - [CONFIRMED-DATA] `Spawned=yes`: V3ROCKET, DMISL, HORNET, ASW, BPLN, PDPLANE, SPYP.
  - [CONFIRMED-PLAY play2, `12-evidence/D/play2-v3-unitlost.txt`] A player V3 fired 4 rockets at a Rhino. Rocket impacts at f=79 and f=306 each logged `eva-request EVA_UnitLost` / `eva-play EVA_UnitLost`. The f=192 impact was suppressed only by the 200-frame RadarEvent window.
- **Original:** no "Unit lost" for rockets, missiles, carrier/Boris/paradrop/spy planes.
- **Clone:** `engine/pres_game.inc:1096` speaks for any dying unit with `u.house == g_local_house`. This covers every V3/Dreadnought shot and every Hornet or MiG that is shot down, and spams the line in normal play.
- **Fix:** skip when the unit's type is Spawned. Either carry `Spawned=` on the unit (it exists as `u.an.t.spawned`) or test `u.an.t.missile || u.an.t.spawned` before `radar_event(7, …)`.

### W7-12-03: With real audio, GI, Conscript and Rhino lose their extra voices and sounds (MAJOR; slice D)

- **Evidence**
  - [CONFIRMED-CODE] `init_audio` (`engine/main.cpp:740-749`, called from the GameScene ctor `main.cpp:6182` unless `--no-audio`) stores `audio.voices[image]` for E1/E2/HTNK. It fills only select, move, attack, die and report.
  - Later `Presentation::add_voices` (`engine/pres_game.inc:767-768`) does `if (image.empty() || audio_->voices.count(image)) return;`. So for GI, CONS and HTNK the full entry is never built:
    - `add_w5_voices` does not run (feedback, special, enter, capture, deploy, undeploy, sinking, crashing, turret, C4);
    - `move_sound` and `crush` are not set.
  - `--play` runs are audio-off (`init_audio` is skipped), so `audio-events.play`'s `snd(GIDeploy) >= 1` passes headless while the real game is silent.
- **Original:** GIDeploy/GIUndeploy when the GI deploys; GIFear/ConscriptFear on the yellow edge (30%); RhinoTankMoveStart; TankCrush when a Rhino squishes infantry; InfantrySquish for crushed GIs and Conscripts [CONFIRMED-DATA rulesmd].
- **Clone:** all of these are silent in a real (audio-on) game for the three most common units. Every other type is unaffected.
- **Code:** `engine/main.cpp:740-749`; `engine/pres_game.inc:767-768`.
- **Fix:**
  - Drop the E1/E2/HTNK block from `init_audio` (the presentation registers everything), or let `add_voices` merge into an existing entry.
  - Add a gate that runs `init_audio`'s path, or that asserts the voice entry is complete for GI/CONS/HTNK after `pres_.init`.

### W7-12-04: No control-group number is drawn on any unit (MAJOR; slice C)
- **Evidence:**
  - [CONFIRMED-RE 0x708F10 tail, 0x709BEx..0x709E0x] `DrawPips` reads `Group` (+0x214). For 0..9 it prints `Group+1`, with 10 printed as `0`, using the format at 0x81A3E0 (`"%d"`).
  - The text rect is `FUN_004A58F0(loc.x-4, loc.y-39 (+3 when not infantry), 0x49 font flags)`.
  - It then fills the rect black (DSurface vt+0x14), draws its border in the owner's house colour (vt+0x58, house +0x56F9 RGB), and prints the digit in the house colour (`FUN_004A65E0`, flags 0x49).
  - `DrawPips` is called from `DrawHealthBar` 0x6F5A00 at `loc = (x-10, y+10)`. So the digit sits at about (x-14, y-29) for infantry and (x-14, y-26) for other units, which is the left end of the pip row.
  - It is shown only when the health bar is drawn (selected or hovered), and only for the player's own or spied (+0x210 bit) objects.
  - [CONFIRMED-PLAY c1] I pressed ctrl+1 on a mixed selection (`c1-group-selected-zoom.png`). No digit appears.
- **Original:** every selected or hovered own unit in a team shows a small digit box in house colour next to its health bar.
- **Clone:** `Input::group_of()` exists (`engine/input.hpp:203`), but no render code reads it.
- **Code:** `engine/main.cpp:4095-4115` (the HP/pip loop), `engine/hud_pips.inc:83-98`.
- **Fix:**
  - In `draw_unit_pips`, when `input->group_of(idx) > 0` and the unit is own (or spied), draw `(g % 10)` with the 0x49 game font (hud_text.inc) at (x-14, y-29 / y-26).
  - Put it in a black box with a 1-px border in house colour, and draw the text in house colour.
  - Pass the group and the house colour through. `draw_unit_pips` currently gets only the Unit.
- **Cross-ref:** none (new).

### W7-12-05: Passenger pips are placeholder rectangles; no `Pip=` colours, wrong place and visibility (MAJOR; slice C)
- **Evidence:** [CONFIRMED-RE 0x708F10, passenger branch 0x7091D0..0x7094xx], when `TechnoType.Passengers` (+0x5E0) >= 1:
  - **Art:** pips use **PIPS2.SHP** (non-buildings).
  - **Start point:** `(loc.x-5, loc.y)`, plus 11 px x for infantry hosts, with `loc = (x-10, y+10)`. The row is therefore **under** the unit's point (y+10), not above the health bar. Steps are 4 px in x and 0 in y.
  - **Pip count:** `max = Passengers`. Every pip starts as frame 0 (empty).
  - **Fill order:** the cargo list is walked from its head. Each passenger fills slots **from the right end** (index `GetTotalSize()-1` downward):
    - an infantry passenger takes `ftol(Size-1.0)` white pips (frame 3), then one pip of its `InfantryType.Pip` (+0xDFC);
    - a Unit passenger takes `Size-1` white pips, then frame 5 (blue);
    - anything else takes frame 1.
    - [CONFIRMED-RE 0x709316..0x7093B0: `FLD [type+0x380]` (Size) `FSUB 1.0`.]
  - **Gunner:** if the type has `Gunner` (+0x805, [CONFIRMED-RE 0x713DFD]), the first pip is drawn alone and the rest start 2 pips further on.
  - **Overflow:** if `GetTotalSize()-1 >= max`, nothing is drawn.
  - **Pip enum:** green=1, yellow=2, white=3, red=4, blue=5, persongreen=7, personyellow=8 [CONFIRMED-RE table 0x81A968].
  - **YR PIPS2 frames:** 1 green, 2 yellow, 3 white, 4 red, 5 blue [CONFIRMED-DATA conqmd.mix pips2.shp].
  - **Visibility:** drawn only when the owner is the player or spied (DrawHealthBar 0x6F5A00 tail), for selected **and hovered** units.
  - [CONFIRMED-PLAY c2] A BFRT with E1 + GHOST + ENGINEER aboard shows 3 white and 2 dark rectangles above the health bar (`c2-bfrt-cargo-pips-zoom.png`). The original shows white, red and blue PIPS2 pips under the unit, filled from the right.
  - [CONFIRMED-DATA rulesmd] The transports in the roster are SAPC 12, HTK 5, LCRF 12, SHAD 5, FV 1 (Gunner) and BFRT 5. Infantry `Pip=` values are white, red or Blue (see the coverage table).
- **Original:** coloured PIPS2 pips at (x-15, y+10), right-filled, coloured by passenger type, with a Gunner gap. Shown for own units when selected or hovered.
- **Clone:** `draw_cargo_pips` fills `cargo_total_size` slots in white SDL rects from the left, in a row 47 px above the ground point. Only selected units get it, own or not, and never on hover.
- **Code:** `engine/iso_renderer.cpp:761-781`, `engine/main.cpp:4112-4114`. `Pip=` has no reader.
- **Fix:**
  - Load `pips2.shp` (conqmd.mix first) into `PipArt`.
  - Add a `draw_unit_cargo_pips` in hud_pips.inc that builds the slot array as above (needs per-passenger `Size` and the parsed `Pip=` index on Unit/UnitCombat).
  - Draw it at (x-15, y+10), or (x-4, y+10) for an infantry host, with a 4-px step.
  - Call it in the `usel || hovered` branch, gated on own or spied.
  - Drop the SDL-rect path in the interactive game.
- **Cross-ref:** cargo.hpp (Size is pip-only); W7-01-39 (garrison occupant pips, the building branch of the same function).

### W7-12-06: Rad beam: the Chrono Legionnaire beam is green, and every rad beam is a straight line instead of a sine wave (MAJOR; slice E)

- **Evidence**
  - [CONFIRMED-RE 0x6fcae0] (RadBeam creation from FUN_006fd210 @ IsRadBeam weapon+0x154):
    - Fire calls `FUN_006fcae0(target, warhead->Temporal(+0x15A) ? 1 : 0)`.
    - The colour is chosen by kind: kind 1 takes Rules+0x1866 `ChronoBeamColor`, kind 2 takes Rules+0x1869 `MagnaBeamColor`, anything else takes Rules+0x1830 `RadColor` (readers 0x66b1f4 / 0x66b21e / 0x66cafd).
  - [CONFIRMED-RE 0x658b50 / 0x658f20 / 0x658f30 / 0x659090 / 0x659500] RadBeam:
    - `Period = 15` (+0x3C) and `Amplitude = 40.0` (+0x40).
    - Segment length (+0x18) is 10 leptons for chrono and 20 otherwise.
    - The draw walks the beam in segments. Each point is displaced by `sin(i/n * len*2 * deg) * amplitude`, the amplitude scaled by `frame/Period`. The colour is jittered per segment.
    - The beam ends when frame >= Period (15 frames).
  - [CONFIRMED-DATA] `[AudioVisual] ChronoBeamColor=128,200,255`, `[Radiation] RadColor=0,255,0`. `NeutronRifle` / `NeutronRifleE` / `CRNeutronRifle` have `IsRadBeam=yes` on `Warhead=ChronoBeam (Temporal=yes)`.
  - [CONFIRMED-PLAY] `docs/gap/wave7/12-evidence/E/e_f25_zoom.png`: the Chrono Legionnaire's shot is a straight green line. `e_spawn_zoom.png` / `e_f05_zoom.png` show the same straight green Desolator beam.
- **Original:**
  - Desolator: a green wavy beam, 15 frames.
  - Chrono Legionnaire: a light-blue wavy beam, 15 frames, 10-lepton segments.
- **Clone:** every IsRadBeam is a straight line with a 5-px glow, `rad_color_` (green), 10 frames.
  - The colour falls back to (0,255,0) until a radiation field exists; `draw_radiation` lazily reads RadColor.
- **Code:** `engine/pres_fx.inc:218-221` (`add_beam` rad branch); `pres_fx.inc:248-256` (straight-line draw); `pres_fx.inc:287-291` (RadColor read lazily).
- **Fix:**
  - Pass the fired weapon's warhead Temporal flag into `add_beam`. Choose ChronoBeamColor or RadColor, both read once at init.
  - Draw the beam as a polyline of `len/seg` segments (seg 10 or 20 leptons), each point offset perpendicular, in screen z, by `sin(k/n * 2*len * pi/180) * 40 * age/15`.
  - Life 15 frames.
- **Cross-ref:** W6-04-24 (rad beam added as RE-FALLBACK), W7-02-05.

### W7-12-07: Shrapnel fragment beams are not drawn (Prism split, elite Tesla bounce) (MAJOR; slice E)

- **Evidence**
  - [CONFIRMED-RE] The callers of `TechnoClass::CreateLaser` 0x6fc6d0 and of `CreateEBolt` 0x6fc920 both include `FUN_0046a230` (BulletClass shrapnel, at 0x46a7cc / 0x46a7ec / 0x46aca1 / 0x46acd9). Each fragment draws its weapon's laser or bolt from the impact point to the fragment target.
  - [CONFIRMED-DATA]
    - `[LargeCometP]` / `[SuperCometP]` `ShrapnelWeapon=CometFragment` / `SuperCometFragment` (`IsHouseColor`, `LaserDuration=15`), `ShrapnelCount=5`.
    - `[Electricbounce]` `ShrapnelWeapon=TeslaFragment` (`IsElectricBolt`), ×2. Used by SHK, TTNK and the Tesla Coil elite weapons.
  - [CONFIRMED-CODE] `engine/combat.cpp:175-190` applies fragment damage through a copied `Unit frag` with `deliver_hit_from`. No FireEvent or `fire_flash` is raised on any observed unit, so `pres_game.inc` has nothing to draw.
- **Original:** the Prism Tank's main beam splits into up to 5 house-coloured lasers fanning out to nearby enemies. An elite Tesla Trooper / Tesla Tank bolt arcs on to 2 more targets.
- **Clone:** only the main beam is drawn, and nearby units lose HP with no visible cause.
- **Fix:**
  - Have `combat.cpp` record fragment hits: `World::FireEvent` with `unit=-1`, plus source = impact point and target.
  - Add a presentation path, `add_beam` from the impact point with the fragment weapon's `fill_beam`.
  - Renders only; no sim change needed beyond the event list.
- **Cross-ref:** W6-04-02 (fragment damage fixed), W2-SK-22 (fragment target choice).

### W7-12-08: InfDeath is lost on every non-weapon kill (radiation, superweapons, abilities) → always Die1 + corpse (MINOR; slice A)

- **Evidence:**
  - Kill paths that never set `ia.death`:
    - `World::ability_kill_unit` (`engine/world_abilities.cpp:157-167`), used by radiation (`:789`, via `ability_damage_unit`), drone kills (`:685`), Suicide (`:389`, `:454`), garrison eject, bridge collapse and culling;
    - `World::an_damage_unit` (`engine/world_sw.cpp:262-275`), used by the superweapon area damage (`:315`), Iron Curtain (`:401`) and Chronosphere (`:432`, `:455`).
  - With `death == -1`, `inf_anim.cpp:165` plays Die1, and `pres_game.inc:1024` drops a DeadBodies corpse.
  - [CONFIRMED-PLAY infB.play L44-58]:
    - three enemy E2s were spawned in a deployed Desolator's rad field;
    - all three died on the same frame (f=140) by the rad tick;
    - the probe read `iseq(Die1)=3` and `death_anims=0`;
    - corpses went 5 → 9.
- **Original:**
  - Every damage source goes through `InfantryClass::ReceiveDamage`, which switches on the warhead's InfDeath [CONFIRMED-RE 0x517DB5, infantry-anim.md §13].
  - [CONFIRMED-DATA rulesmd]: `RadSite InfDeath=7` (InfantryNuked=NUKEDIE), `NUKE 7`, `RadEruptionWarhead 7`, `PsiPulse 6` (YURIDIE), `IonCannonWH 5`, `TerrorBombWH 4`, `Crush 2`.
  - So a soldier dying in a Desolator field or under a nuke melts (NUKEDIE), and a Yuri psi-wave kill head-pops.
- **Clone:**
  - Only the combat paths (`combat.cpp:276/362/435`) copy `w_inf_death`.
  - Radiation, superweapon and ability kills show a generic Die1 fall and a skeleton corpse.
- **Code:** `engine/world_abilities.cpp:157`, `engine/world_sw.cpp:262`, `engine/inf_anim.cpp:163-168`.
- **Fix:**
  - Give `ability_kill_unit` / `ability_damage_unit` / `an_damage_unit` an `inf_death` argument and set `v.ia.death`.
  - The argument comes from: rad = the `[Radiation]` RadSiteWarhead's InfDeath; SW = the SW weapon's warhead; Suicide = the firer's own weapon warhead; eruption/psi = the deploy weapon's warhead.
  - Crush is separate: it removes the victim outright (W7-04-25).
- **Cross-ref:** W7-01-43, W7-04-25, W7-02-05.

### W7-12-09: Amphibious soldiers use the dry set on Beach cells; the original's wet test is Water OR Beach (MINOR; slice A)

- **Evidence:**
  - [CONFIRMED-RE 0x51D4C0, PlayAnim] The wet substitution runs only when:
    - `type+0x5B4 == 3` (MovementZone=AmphibiousDestroyer: GHOST, TANY);
    - the cell's LandType (+0xEC) is **6 (Beach) or 2 (Water)**;
    - the soldier is not airborne (+0x8C).
  - Its mapping:
    - Walk/Crawl→Swim;
    - **Ready/Prone**→Tread (Guard is not mapped);
    - Idle1/2→WetIdle1/2;
    - Die1/2→WetDie1/2;
    - FireUp/FireProne→WetAttack.
  - On the dry/wet edge (+0x6E8) PlayAnim plays the water enter/leave sound itself.
- **Clone:**
  - `world.cpp:5409` sets `c.in_water` from `water_passable_`, which is built from LandType::Water only (`world.cpp:242-254`, `world_depth.cpp:353`).
  - The Enter/LeaveWaterSound edge (`pres_audio_events.inc:362-369`, `w6_wet` → `is_water_cell`) uses the same water-only test.
  - So on shore/beach cells a SEAL or Tanya stands in Ready/Walk poses.
  - The amphibious test is `t.e[kSqSwim].has` (`inf_anim.cpp:52`) rather than the MovementZone. It is equivalent for stock data.
  - [CONFIRMED-PLAY infB L27-30]: `amphib_on_water(0)=2` but `iseq(Tread)=1`; the SEAL at the shore point draws Ready (evidence `c_tany_tread.png` vs `x03_after_wetdie.png`). That the shore cell's LandType is Beach is [SUSPECTED].
- **Fix:**
  - Compute `in_water` as LandType Water || Beach, plus the clone's materialized-water rule for Water.
  - Gate it on `MovementZone==AmphibiousDestroyer`.
  - Use the same predicate for the water-sound edge.
- **Cross-ref:** W7-01-43(b) (deep-water death), W6-04-27.

### W7-12-10: SecondaryFire / SecondaryProne are never played. Extends W7-02-06 (was SUSPECTED, now CONFIRMED-RE) (MINOR; slice A)

- **Evidence:**
  - [CONFIRMED-RE 0x5203E0] When a shot starts, the original picks the sequence by the weapon that `SelectWeapon` (vt+0x2E4) chose:
    - in the deploy group → DeployedFire;
    - weapon 0 → FireProne if prone, else FireUp;
    - weapon ≠ 0 → SecondaryProne if prone and that entry's count ≠ 0, else SecondaryFire if its count ≠ 0, else the FireUp/FireProne fallback.
  - The release stage is compared with art `FireUp=` (+0xE40), `FireProne=` (+0xE44), `SecondaryFire=` (+0xE48) or `SecondaryProne=` (+0xE4C). All four are read by the art loader [CONFIRMED-RE 0x524440-0x52445D].
  - [CONFIRMED-RE 0x520810 default branch] While the Secondary target persists, a finished SecondaryFire/SecondaryProne replays with force. This is the Boris "holding the flare on the building" pose.
- **Clone:** `inf_anim.cpp:190-192` always plays DeployedFire/FireProne/FireUp. Nothing reads `u.w_active` for the sequence.
- **Data:** [CONFIRMED-DATA] only BORIS defines them in the roster (`SecondaryFire=401,1,1`, `SecondaryProne=217,6,6`).
- **Fix:** pass the fired slot in `InfTickCtx` (for example `c.fire_slot = u.w_active`) and apply the rule above, including the replay while the slot-1 target lives.
- **Cross-ref:** W7-02-06 (Boris), W7-01-35 (release frames).

### W7-12-11: Sinking ships slide visibly for 1 s instead of sinking for about 80 frames below a clipped waterline with wake puffs (MINOR; slice B)
- **Evidence:**
  - [CONFIRMED-RE] **UnitClass::AI @0x735571-0x735686** runs every frame while the sinking flag (+0x3CD) is set:
    - `SetLocation(coord.z - 5)`;
    - when `GetHeight < -400` (0xFFFFFE70) the ship is removed (vtbl+0xE0, then +0xF8; no explosion);
    - every 4th frame (`Frame & 3 == 0`) it spawns `Rules+0x94` (`[General] Wake=WAKE1`, effects.md §4) at `x ± Random(-170,170)`, `y ± Random(-170,170)` and `z - height` (the water surface).
  - [CONFIRMED-RE] **Unit voxel draw 0x73A3A0 @0x73AE58 and blitter 0x706360:**
    - on the first sinking draw, `+0x3CA` stores the screen y of the waterline;
    - every later draw clips the sprite rectangle at that y (`FUN_00421B80`).
  - [CONFIRMED-RE] The voxel shadow is skipped while sinking (`if (!+0x3CD)`, 0x73A3A0 tail). FootClass::AI 0x4DA440 @0x4DA72A also stops the HVA counter.
  - [CONFIRMED-RE] ShipLocomotion `0x69FC80` keeps the hull moving while it sinks. Its speed decays by `Speed × const` per frame down to a floor.
  - [CONFIRMED-PLAY] `play2` s00→s04 (`12-evidence/B/sink0_alive.png`, `sink1_6f.png`, `sink2_24f_gone.png`): a killed Destroyer drops a few pixels and is gone after 24 frames. It is never clipped and shows no wake or bubble anims.
- **Original:** about 400 leptons over 80 frames (~5 s at 15 fps). The hull disappears into the water line by line, and wake anims ring the wreck.
- **Clone:**
  - `main.cpp:3424` uses `extra_dy = -die_time*24` px;
  - the unit is tombstoned after `kDieAnimSecs = 1.0` (`combat.cpp:27, 854-857`);
  - the shadow is still drawn (`main.cpp:3437`).
- **Code:** `engine/main.cpp:3409-3425, 3437`; `engine/combat.cpp:27`; `engine/world_an.cpp:661-664`.
- **Fix:**
  - Give a sinking unit its own timeline: z -= 5 leptons per logic frame, removed below -400. Do not use the 1 s die timer.
  - Draw the voxel clipped at the screen y of its waterline (the anchor at sink start).
  - Spawn `Wake` every 4 frames at ±170 leptons on the water surface.
  - Skip the shadow and freeze the walk counter while sinking.
  - Optionally keep the ship drifting forward with a decaying speed.
- **Cross-ref:** W7-06 (sinking rule verified; it now gets the land==Water and `+0x271` checks of 0x736DC5), W2-AN-25.

### W7-12-12: An aircraft killed in the air explodes on the ground at the moment of the hit, then vanishes on impact with no visual (MINOR; slice B)
- **Evidence:**
  - [SUSPECTED: code-read; play could not produce an airborne kill, because cheat-spawned SHAD/ZEP were grounded in runs 2-4]
    - `World::step` runs `step_air_naval` (and so `an_step_deaths`) BEFORE `step_combat` (`world.cpp:~5009` and `~5065`).
    - On the kill tick, the presentation therefore sees `die_time = 0` while `an.crashing` is still false.
    - `unit_fx_tick` (`pres_game.inc:1077-1094`) then spawns the random Explosion= with `fx_.spawn(name, u.fx, u.fy, u.level, 0.0)`. That is at ground level, not at the altitude.
    - The next tick sets `crashing` (`world_an.cpp:630-646`). The hull falls, and at impact (`crashing=false`) the die edge has already passed.
    - Nothing is spawned at impact: no Explosion=, no debris, no death weapon. Only the ImpactLand/Water sound plays (W6-05-06).
    - In live play, `draw_vehicle_unit` draws nothing for a dying vehicle (`main.cpp:3392-3410`, the `!g_fx` branch).
  - [CONFIRMED-RE, from W7-05-09] What the original does at impact:
    - fixed-wing (`FUN_004CD520`): FireDeathWeapon, which detonates a bullet and so plays the warhead's AnimList (SelectDamageAnimation);
    - BalloonHover jumpjet (Kirov, `FUN_00744F30` @0x745001): FireDeathWeapon (BlimpHE AnimList);
    - non-BalloonHover crashable jumpjet (SHAD, SCHP): `FUN_007376C0`, the normal Explosion= (+ DestroyAnim) at the impact point.
- **Original:** no explosion while the aircraft is falling; one blast where it hits.
- **Clone:** the fire anim appears on the ground under the aircraft when it is hit, and the falling wreck vanishes silently.
- **Code:** `engine/pres_game.inc:1068-1095`; `engine/world_an.cpp:627-660`; `engine/main.cpp:3392-3410`.
- **Fix:**
  - Move the death-FX trigger onto the impact edge. Emit a World "Crashed" event from `an_step_deaths` when `altitude` reaches 0.
  - At impact, play the death-weapon detonation anim for fixed-wing and BalloonHover types (W7-12-13), or the random Explosion= for the other jumpjets.
  - Suppress the explosion on the kill tick for any aircraft that will crash.
- **Cross-ref:** W7-05-09 (damage), W7-05-15 (spin and drift), W6-05-06.

### W7-12-13: Death-weapon detonations draw no anim (Demo Truck DEMTEXP, BlimpBomb) (MINOR; slice B)
- **Evidence:**
  - [CONFIRMED-RE, from W7-05-09] `FUN_0070CAC0` FireDeathWeapon creates a bullet with the weapon's warhead and detonates it. The impact anim is `SelectDamageAnimation(damage, WH)`: AnimList by 25-damage buckets (presentation.md §5 `FUN_0048A400`).
  - [CONFIRMED-DATA]
    - `[Demobomb]` Damage=300 → `DemobombWH AnimList=DEMTEXP` (the large blast);
    - `[BlimpBomb]` Damage=250 → `BlimpHE AnimList=…,TWLT070`;
    - BPLN/SPYP DeathWeapon=BlimpBomb ×0.1 = 25 → `XGRYMED1` bucket.
  - [CONFIRMED-PLAY] run4 (`12-evidence/B/dtruck_death_no_demtexp.png`; pres.log `f=212 DemoTruckDie, Explosion09, fx debris DTRUCK x1`): only a small random Explosion= appears. No DEMTEXP anim is logged.
  - [CONFIRMED-code] `World::ability_death_pass` pushes `Ev::Kind::DeathWeapon` (`world_abilities.cpp:485`). The presentation (`pres_audio_events.inc:110-200`) never looks at that kind; only the gate counter in `main.cpp:17140` reads it.
- **Original:** the death weapon's warhead anim plays at the unit, on top of the Explosion= anim.
- **Clone:** no detonation anim, so the damage happens "invisibly".
- **Code:** `engine/world_abilities.cpp:472-490`; `engine/pres_audio_events.inc:139+`.
- **Fix:** Handle `Kind::DeathWeapon` in `Presentation::on_world_events`. Spawn the warhead AnimList entry picked by `min(dmg, n*25-1)/25` at the unit (the same picker as rocket_fx/impact), with its Report sound. The event needs the weapon id or warhead id plus x/y/level.
- **Cross-ref:** W7-04 DTRUCK row (damage OK), W7-05-09, W6-04-12.

### W7-12-14: A unit that dies on water without sinking shows the land fire explosion instead of Wake plus a big splash (MINOR; slice B)
- **Evidence:**
  - [CONFIRMED-RE] **UnitClass::ReceiveDamage death branch `0x736DC5`** (decompiled lines at 0x736E3F-0x736FCC). When the unit does not sink, and `GetHeight < 11` and flag `+0x8F` is set, and the cell land type (+0xEC) == 2 (Water):
    - it spawns `Rules+0x94` (`Wake`) at the unit;
    - it spawns the LAST SplashList entry (`Rules+0xBC4[count-1]`, `SplashList=H2O_EXP3,H2O_EXP2,H2O_EXP1` → H2O_EXP1) at z+5;
    - it skips `FUN_007376C0` (no Explosion=, no DestroyAnim).
    - The meaning of `+0x8F` was not traced; it is on for normal units.
  - [CONFIRMED-PLAY] play2 s06 (`12-evidence/B/hyd_water_death_fire.png`): a killed Sea Scorpion (Weight 2, no sink) shows the orange land explosion on open water. pres.log `f=81 Explosion12 (explosion)`.
- **Applies to:**
  - HYD; SUB (Underwater never sinks); SAPC/LCRF killed afloat;
  - Explodes on water generally, e.g. DTRUCK on a bridge/ford (land type decides).
- **Clone:** `pres_game.inc:1077-1095` always plays Explosion=. Debris is already suppressed on water (OK).
- **Fix:** In `unit_fx_tick`, when the unit is on a water cell below 11 leptons and not sinking, spawn `[General] Wake` plus the last SplashList anim (z+5) instead of the Explosion= pick.
- **Cross-ref:** W7-06 (naval).

### W7-12-15: FireFLH: only PrimaryFireFLH is read. No Secondary/Elite/Weapon%dFLH, no burst L-flip, no TurretOffset (MINOR; slice B)
- **Evidence:**
  - [CONFIRMED-RE] **TechnoTypeClass::LoadFromINI `0x711510`, art block @0x714EB7-0x715338:**
    - unless the type has WeaponCount (`FUN_00716C20`), it reads `PrimaryFireFLH` (+0x89C), `SecondaryFireFLH` (+0x8B8), `ElitePrimaryFireFLH` (+0xA98, **default = PrimaryFireFLH**) and `EliteSecondaryFireFLH` (+0xAB4, default = SecondaryFireFLH);
    - otherwise it reads `Weapon%dFLH` and `EliteWeapon%dFLH` (default = Weapon%dFLH), plus `%sBarrelLength`, `%sBarrelThickness` and `%sTurretLocked`, for 1..WeaponCount;
    - `AlternateFLH0..4` default to PrimaryFireFLH.
  - [CONFIRMED-RE] **GetFLH `0x6F3030`** (voxel-render.md §6) negates L on every other burst shot (flag at dword index `0xEE` of the object). **`0x6F32C0`** shows the turret origin is `T(TurretOffset,0,0) ∘ Rz(body − turret)`, so FLH is measured from the offset turret pivot.
  - [CONFIRMED-code]
    - `src/art.cpp:73-92` reads only `PrimaryFireFLH`;
    - `weapon_resolve.cpp:120-124` and `main.cpp:1728-1735` copy it to the secondary and elite slots;
    - `world.cpp:5921-5938` uses `u.turret_facing` raw (not quantized), with no L flip and no TurretOffset;
    - `pres_game.inc:680-692` (muzzle anim) and `main.cpp:3490-3515` (fallback flash) do the same.
- **Affected types** [CONFIRMED-DATA]:
  - **SREF** (`Weapon1FLH=48,0,184`) and **FV** (`Weapon1..17FLH`, e.g. 64,48,180) have no PrimaryFireFLH, so in the clone they fire and flash from the unit centre at ground level;
  - **APOC** 120mmx Burst 2 with L=25: the original alternates the left and right barrels; the clone fires both from +25;
  - APOC's MammothTusk should start at the Secondary FLH 0,0,0, not the cannon tip;
  - **DRED** (L=43, Burst 2), **BPLN** (L=100, Maverick3 Burst 2), **HORNET/ASW** (L=32);
  - **SCHP** deployed 160mm uses `SecondaryFireFLH=200,0,250` (clone: 0,0,50);
  - **HTK** TurretOffset -80 is missing from the muzzle (80 leptons ≈ 0.3 cell).
- **Fix:**
  - Add `Art::fire_flh(image, key)` for the four named keys and `Weapon%dFLH` / `EliteWeapon%dFLH`, with the defaults above. Store the FLH per weapon slot (and per IFV weapon index).
  - In `launch_projectile` and in the muzzle FX:
    - rotate by the quantized turret yaw;
    - add `TurretOffset` along the body yaw first;
    - negate L when `burst_index & 1`.
- **Cross-ref:** W7-03-12 (AlternateFLH), W7-03-05/07 (IFV/Prism turrets), W7-03-06 (Report/Anim from the image's primary).

### W7-12-16: No Tiberium (ore/gem) pips on the miners (MINOR; slice C)
- **Evidence:** [CONFIRMED-RE 0x708F10, `PipScale` (+0x3D4) == 2]:
  - It reads the four tiberium amounts `FUN_006C8D00(0..3)`. Gems (type 1) and ore (types 0, 2, 3) are scaled to `max` pips (TechnoType vt+0x74) with ftol.
  - Each slot is drawn as frame 5 (blue, gems) first, then frame 2 (yellow, ore), then frame 0 (empty).
  - It uses PIPS2 at (x-15, y+10), steps 4 px.
  - [CONFIRMED-DATA] HARV and CMIN have `PipScale=Tiberium` (Storage 40 / 20).
  - [CONFIRMED-code] `PipScale` has no reader in the clone (also noted in W7-05-07).
- **Original:** a selected or hovered own miner shows its load, with gems blue and ore yellow.
- **Clone:** nothing.
- **Code:** `engine/main.cpp:4095-4115`.
- **Fix:**
  - Parse `PipScale`.
  - For Tiberium, compute the gem and ore pip counts from the harvester's bale load (the clone's harvest state) and draw PIPS2 frames 5/2/0 in the W7-12-05 row position.
  - The pip count is `TechnoType::GetPipMax` (vt+0x74). I did not decompile it [SUSPECTED: derived from Storage], so confirm it before you hard-code a value.
- **Cross-ref:** W7-05-07.

### W7-12-17: No spawn pips on CARRIER / DRED / V3 / DEST (MINOR; slice C)
- **Evidence:**
  - [CONFIRMED-RE 0x708F10 head] If `SpawnsNumber` (+0xD5C, [CONFIRMED-RE 0x714295]) > 0, it draws SpawnsNumber pips: frame 1 (green) for `i < SpawnManager ready count`, else frame 0.
  - The ready count is `FUN_006B73D0`, which counts spawns in state 0 or 6 (docked or idle).
  - It uses PIPS2 at the same row start (x-15, y+10), 4-px step. This is independent of `PipScale`.
  - [CONFIRMED-DATA] CARRIER Spawns=HORNET ×3, DRED DMISL ×2, V3 V3ROCKET ×1, DEST ASW ×1.
- **Clone:** nothing.
- **Fix:** draw the row from the spawner's docked/ready count. The air_naval spawn state already tracks `planes_docked` and the rockets.
- **Cross-ref:** W7-06 naval / W7-04 V3/DRED reports (gameplay only).

### W7-12-18: Ammo pip layout (extends W7-05-07) (MINOR; slice C)
- **Evidence:** [CONFIRMED-RE 0x708F10, `PipScale` == 1]:
  - **PipWrap (+0x3E4) == 0:** it draws only the *current* ammo count, as PIPS2 frame 13 (a tall green pip), at (row.x + 4i, row.y - 3). There are no empty pips.
  - **PipWrap > 0:** it draws PipWrap pips. Each is frame `15 + level`, where level is the highest wrap layer whose index is below the current ammo (+0x2FC). Otherwise it is frame 14 (empty tall).
  - PipWrap is not set on any roster type [CONFIRMED-DATA].
  - The row is the same (x-15, y+10) anchor as W7-12-05.
- **Fix:** as W7-05-07, with these frames and anchor.
- **Cross-ref:** W7-05-07 (only extended here).

### W7-12-19: Chevrons and hover health pips leak through the shroud and for hidden units (MINOR; slice C)
- **Evidence:**
  - [CONFIRMED-PLAY c1] I spawned an enemy elite TANY and a veteran APOC in unexplored shroud. Their chevrons (3 and 1), and the hovered TANY's 8-pip health row, are drawn on the black shroud (`c1-enemy-shroud.png`, `-zoom.png`).
  - [CONFIRMED-RE 0x6F46F0 @0x6F48C0..0x6F48E8] The original draws chevrons (`vt+0x454` = 0x709E10) only when the object is not disguised to the player (`vt+0xC8`, Unit 0x745580 / Infantry 0x522520) **and** `VisualCharacter(vt+0x68) != Hidden (5)`.
  - So enemy cloaked or submerged units show no chevron. Own Ripple-cloaked units still do.
  - In the original, objects under the shroud are covered with their extras. [SUSPECTED: TacticalClass draws the shroud after the object extras; the clone's own pass order is the opposite.]
- **Clone:**
  - `draw_p5_chevrons` loops over every unit after `draw_shroud_pass`. It checks only alive/cargo/disguised. There is no `unit_hidden_from`, `seen_by_mask` or cloak test.
  - The hover pick (`g_hover_unit = input_.pick_unit_at`) also returns shrouded units, and the bar loop checks only `unit_hidden_from(0, u)`.
- **Code:** `engine/main.cpp:3668-3684`, `engine/main.cpp:4097-4109`, `engine/main.cpp:6816`.
- **Fix:**
  - Skip units with `world.unit_hidden_from(g_local_house, u)` or `!(u.seen_by_mask & local_bit)` (or not explored/visible), in both the chevron loop and the hover pick and bar loop.
  - Better: draw chevrons inside the bar loop, before the shroud pass, as the original does in DrawExtras.
- **Cross-ref:** W6-05-07 (the hover fix: this extends it with the shroud case).

### W7-12-20: Chevrons of airborne units are drawn at the ground point (MINOR; slice C)
- **Evidence:**
  - [CONFIRMED-PLAY c2] A veteran Kirov in flight has its health bar on the body, but its chevron sits about 110 px below, on its shadow (`c2-kirov-air-chevron-on-ground-zoom.png` vs `c2-kirov-air-body-zoom.png`).
  - [CONFIRMED-RE 0x709E10] The original offsets from the same object draw point as the health bar: +(5,2) for infantry, +(10,6) for others.
- **Clone:** `draw_p5_chevrons` computes `sy` with no `altitude_to_screen_dy` lift and no `pres_unit_lift_levels` (bridge deck). The health-bar loop applies both.
- **Code:** `engine/main.cpp:3677-3683` vs `4103-4106`.
- **Fix:** use the same `up` and `lv` terms as the bar loop. This affects ZEP, SHAD, SCHP, ORCA, BEAG, JUMPJET in flight, paratroopers, and units on high-bridge decks.
- **Cross-ref:** none.

### W7-12-21: The Terror Drone gets the infantry health bar and chevron offset (MINOR; slice C)
- **Evidence:**
  - [CONFIRMED-PLAY c1] The DRON's bar is the short 8-pip infantry one (`c1-group-selected-zoom.png`, centre).
  - [CONFIRMED-RE 0x6F5A00] The original branches on `WhatAmI == Infantry (0xF)`. DRON is a UnitType (rules [VehicleTypes]), so it gets PIPBRD frame 0 at (x+1, y-26) and 17 pips from x-15.
  - The chevron offset is +(10,6) for anything that is not infantry [CONFIRMED-RE 0x709E10].
- **Clone:** `main.cpp:2056-2067` (W6-04-10) registers DRON with `is_vehicle=false`. `draw_unit_pips` (`inf = !is_vehicle && !is_aircraft`) and `draw_p5_chevrons` (`u.is_vehicle ? 10 : 5`) then treat it as infantry.
- **Fix:** add a `Unit::rtti_infantry` (or `is_shp_vehicle`) flag, set from `rules.is_vehicle(id)`, and use it for the two presentation tests. Keep `is_vehicle=false` for the SHP render path if it is needed there. The same applies to any cheat- or campaign-spawned SHP vehicle.
- **Cross-ref:** W6-04-10.

### W7-12-22: No self-heal indicator pip (Tech Hospital / Machine Shop) (MINOR; slice C)
- **Evidence:** [CONFIRMED-RE 0x708F10, after the pip rows]
  - **Infantry**, or a Unit with type +0xD97 (Organic, [SUSPECTED name]): if `FUN_0050D7E0` (house infantry self-heal, [SUSPECTED]: the HouseClass +0x164 count the clone already uses), it draws PIPS.SHP frame 13 (the red cross) at row + (19, -35).
  - **Other Units:** with `FUN_0050D7F0` (machine shop), it draws frame 20 (the wrench) at row + (38, -32).
  - **Flash:** flags 0x601 instead of 0x600 while `Frame % SelfHealInfantryFrames (50) / SelfHealUnitFrames (75) < 6` and `Health < Strength`.
  - [CONFIRMED-DATA] YR pips.shp frame 13 is the 8×8 cross and frame 20 is the 15×15 icon.
- **Clone:** the heal itself is implemented (`engine/world_w5.cpp:83-100`), but there is no indicator.
- **Fix:** in `draw_unit_pips`, when the owner's `inf_self_heal` / `unit_self_heal` count is > 0, draw the frame at the offset and flash it in the 6-frame window. This needs the YR `pips.shp`: `PipArt` loads the RA2 conquer.mix copy, which has only 19 frames.
- **Cross-ref:** abilities-w5.md §5.

### W7-12-23: Mind-control visuals are missing: no MINDANIM/MINDANIMR over the victim, no link line (MINOR; slice E)

- **Evidence**
  - [CONFIRMED-RE 0x471e73] `CaptureManagerClass` capture (FUN_00471c80) creates `new AnimClass(Rules+0x320 ControlledAnimationType, coord, 0, 1, 0x600)`, attaches it to the victim (0x424ad0) and stores it at techno+0x2C8. A ZAdjust of -1024 is set in one branch.
  - Reader 0x66c451 (Rules+0x320 `ControlledAnimationType`, +0x324 `PermaControlledAnimationType`).
  - [CONFIRMED-DATA] `ControlledAnimationType=MINDANIM`, `PermaControlledAnimationType=MINDANIMR` (artmd: LoopCount=-1, Rate=300).
  - [CONFIRMED-CODE] `git grep MINDANIM/ControlledAnimation engine/` → none.
- **Original:** a looping purple swirl over every unit Yuri / Psi-Corps holds. MINDANIMR is used for Mastermind / permanent control.
- **Clone:** a controlled unit only changes colour.
- **Fix:** while a unit is controlled, draw a looping MINDANIM (MINDANIMR when permanent) at the unit (attached; the anim's own Rate/Loop keys).

- **Merged:** sub-audits C (C-11) and E (E-07) both found this. C adds the link line: `MindControlAttackLineFrames=20` is read to Rules+0x324 [CONFIRMED-RE 0x66C48F]. Its draw was not traced, so the line is [SUSPECTED].

### W7-12-24: soundmd `Attack=`/`Decay=` counts are ignored (Harrier/Black Eagle acks are mostly radio clicks) (MINOR; slice D)

- **Evidence**
  - [CONFIRMED-RE FUN_0074F260] The sound-entry parser reads `ReadInteger(section, "Attack", 0)` and `"Decay"` into `+0x138` / `+0x13C` (FUN_00406520/30). Then FUN_00406570 only forces a count to 1 when the ATTACK/DECAY Control bit is set and the count is 0.
  - FUN_004048B0 (audio-playback.md §2) then picks a random intro in `[0, A-1]`, the body in `[A, count-D-1]`, and a random outro in `[count-D, count-1]`.
  - [CONFIRMED-DATA soundmd] The affected sections:
    - `IntruderSelect/Move/AttackCommand`: Attack=7, Decay=7, 18 samples (7 `gradio1*`, 4 speech, 7 `gradio3*`);
    - `BlackEagleSelect/Move/AttackCommand`: Attack=3, Decay=3;
    - `IntruderMoveLoop`, `BlackEagleMoveLoop` and `TerrorDroneMoveLoop`: Attack=3.
- **Original:** every Harrier ack is radio-in, then one of the 4 spoken lines, then radio-out.
- **Clone:** `ra2assets::Sounds::entry` (`src/sounds.cpp:786-817`) does not parse Attack/Decay. The mixer (`engine/audio_mixer.cpp:146-158`) uses a fixed 1 + 1. That makes the intro always `gradio1a` and the outro always `gradio3g`, with the body uniform over the other 16 samples. Only 4 of those 16 are speech, so **75% of ORCA acks have no words**. For BEAG it is 4 of 10, so 60% have no words.
  - The ALL+LOOP move loops (`loop_from == -2`, `audio_mixer.cpp:319-325`) also replay intro clips 2..A and the decay on every pass.
  - Whether the original plays the decay on loop stop is [SUSPECTED] (not traced).
- **Code:** `src/sounds.cpp:786-817`; `include/ra2assets/sounds.hpp` (SoundEntry); `engine/audio_mixer.cpp:144-158, 177, 319-325`; `engine/pres_audio.inc:269-272`.
- **Fix:**
  - Add `attack_n` and `decay_n` to SoundEntry and MixerSound, with defaults from Control as in FUN_00406570.
  - In `start_`: intro = `variants[ranged_(0, A-1)]`; body = `ranged_(A, count-D-1)` (or all of `[A, count-D)` for ALL); outro = `variants[ranged_(count-D, count-1)]`.
  - For LOOP+ALL, loop only the body.

### W7-12-25: Order/select voices have no per-unit channel and are played positionally (MINOR; slice D)

- **Evidence**
  - [CONFIRMED-RE 0x708210] `QueueVoice(voice)` stores `techno+0x4F0 = voice`. It does so only when `AllowVoice` (DAT_00821CF2) is set and the owner IsPlayerControl. Every Voice* virtual goes through it:
    - VoiceSelect 0x708330;
    - VoiceMove 0x708440;
    - VoiceAttack 0x708520;
    - VoiceEnter 0x7084A0;
    - VoiceCapture 0x708240;
    - VoiceDeploy 0x708280.
  - [CONFIRMED-RE 0x6F9380 @6F9400] TechnoClass::AI then works as follows:
    - Its voice controller (`+0x4DC`) idle: the queued voice is played with **FUN_0074F740(1.0)**, and `+0x4F4` remembers it.
    - Controller busy with the **same** voice: the request is dropped.
    - Controller busy with a different voice: the request stays queued until the controller is free. Only the last request survives.
  - [CONFIRMED-RE 0x74F740 vs 0x74F800] FUN_0074F740 never calls the attenuation function FUN_0074F8E0. It is the non-positional start; FUN_0074F800 is the positional one.
  - A per-frame `FUN_0074FB60` at the top of 0x6F9380 re-attenuates a controller at the object's coords when type flag `+0xCD5` is set. The key for that flag was not identified [SUSPECTED that it applies to the voice controller].
- **Original:**
  - Clicking a unit 5 times quickly gives at most one line, plus one follow-up if a different line was asked for.
  - The ack is centred and at full volume, even for a unit off screen (team recall, radar-ordered).
- **Clone:**
  - `unit_audio` / `order_voice` (`engine/pres_game.inc:1121-1127`, `engine/pres_audio_events.inc:230-253`) call `snd_.play_at` each time. Overlapping copies stack up to the event's Limit (default 5).
  - Each copy is panned. Off-screen units are attenuated past the screen edge and silent beyond Range=10 cells.
  - `docs/re/audio-playback.md` ("Decisions": "GISelect/GIMove … distance-attenuated") should be corrected.
- **Fix:**
  - Give each unit a `voice_handle` and a `queued_voice`.
  - In `unit_audio`, start `queued_voice` with `snd_.play(key, 0.0, …)` (non-positional) only when the unit's handle is not alive. Drop it if it equals the playing one; otherwise keep it queued.

### W7-12-26: The wrong unit speaks for a group; type-select speaks (MINOR; slice D)

- **Evidence**
  - [CONFIRMED-RE 0x4AE660] The order dispatch loops `CurrentObjects` (DAT_00A8DC24, count DAT_00A8DC30) and clears AllowVoice after the first object, so only `CurrentObjects[0]` answers.
  - [CONFIRMED-RE 0x6D9B30] Box select iterates the tactical display list, and the first object whose `Select()` succeeds speaks.
  - [CONFIRMED-RE 0x730290 / 0x730470 / 0x730590] Team select/recall: the first member in the Technos array speaks.
  - [CONFIRMED-RE 0x7318A0] Select-same-type saves AllowVoice and forces it to 0, so it is **silent**. It is reached from box-select (FUN_00731DD0) and the clone's own RE cite FUN_00731A20.
  - [CONFIRMED-RE 0x6FB4C0] `TechnoClass::Select` calls VoiceSelect only when AllowVoice is set and the owner is the player (or the house flag `+0x1A6`).
- **Clone:**
  - The selection is an index bitset (`engine/selection.hpp:49`, `lowest()`). The actor is always the **lowest unit index** (`engine/input.cpp:421-478, 536-818`; `engine/input_original.cpp:318, 344, 541, 569`). So shift-adding a newer tank to an older GI makes the GI answer.
  - `select-same-type` (`input_original.cpp:533-543`) sets `actions_.selected` and plays a select voice that the original does not.
- **Fix:**
  - Keep an insertion-ordered selection list, and make the actor the first selected unit.
  - Do not set `actions_.selected` in the select-same-type path.

### W7-12-27: Units that share an image get the first registered type's voices (engineers) (MINOR; slice D)

- **Evidence**
  - [CONFIRMED-CODE] `AudioSystem::voices` is keyed by image (`engine/main.cpp:641`), and `add_voices` keeps the first registration (`engine/pres_game.inc:768`). The defs are registered player roster first (`pres_game.inc:33-35`).
  - [CONFIRMED-DATA] Roster types that share an image: ENGINEER, SENGINEER and YENGINEER (image ENGINEER); DOG and YDOG (DOG); ADOG and YADOG (ADOG).
    - The dogs' data is identical, so they are harmless.
    - The engineers differ: EngAll*, EngSov* and EngYuri* select, move, fear and capture lines, and EngAllDie vs EngSovDie.
  - [CONFIRMED-PLAY play1] The only engineer selection logged `EngAllSelect` (Allied player). Clicking `unit 0:SENGINEER` failed (`no unit 0:SENGINEER`); see Method. The engineer kind therefore could not be told apart in play, so the collision is from code and data.
- **Original:** each type speaks its own lines.
- **Clone:** for an Allied player, Soviet engineers die with the Allied engineer's DieSound. A captured or mind-controlled SENGINEER, or a campaign-placed one, speaks Allied lines. The reverse holds for a Soviet player.
- **Fix:** key `voices` by rules id (the same fix as W7-01-12 / W7-09-01: carry the rules id on `Unit`).

### W7-12-28: The concrete image ≠ id list for the image-keyed per-type sound lookups (MINOR; slice D)

The voice/report/Die/MoveSound/CrushSound **data** is read by rules id (`add_voices(rules, d.id, image)`) and only stored under the image. So APOC→MTNK does **not** cross-key the Grizzly's voices [CONFIRMED-PLAY play1: ApocalypseSelect, ApocalypseMove, ApocalypseMoveStart; MTNK says GenAllVehicleSelect]. The keys that are lost come from `w5_bld_key(u.image, key)` / `w6_type_or_av(u.image, …)` (`engine/pres_audio_events.inc:153-154, 354-356, 366, 482, 500, 525-534, 571`), which read **the rules section named by the image** [CONFIRMED-DATA, `rulesmd` sections checked].

| id → image | image section in rulesmd? | keys lost (id section) | effect |
|---|---|---|---|
| JUMPJET → ROCK | no | MoveSound=RocketeerMoveLoop, ImpactLandSound=RocketeerCrash | no flight loop; the crash impact falls back to [AudioVisual] ImpactLandSound=(empty), so it is silent (W7-05-03) |
| GHOST → SEAL | no | Enter/LeaveWaterSound | silent (W7-01-12) |
| SAPC → TRS | no | EnterTransportSound=EnterTransport, LeaveTransportSound=ExitTransport | [AudioVisual] has no default, so boarding is silent |
| ORCA → FALC | no | MoveSound=IntruderMoveLoop, AuxSound1/2 (take-off/landing), ImpactLandSound=GenAircraftCrash | silent flight/take-off/landing/impact (W7-05-03) |
| DTRUCK → TRUCKA | **yes** (civilian truck) | CreateSound=DemoTruckCreated | reads [TRUCKA] (no CreateSound), so it falls to [AudioVisual] CreateUnitSound=(empty) and is silent |
| APOC → MTNK | **yes** (Grizzly) | none of the currently image-keyed keys are set in [APOC] | latent: any key moved onto `w5_bld_key` (e.g. MoveSound, CrushSound) would give APOC the Grizzly's |
| E1→GI, E2→CONS, TERROR→TRST, AMCV→MCV, MTNK→GTNK, MGTK→RTNK | no | none of the image-keyed keys | OK today |
| SENGINEER/YENGINEER→ENGINEER, YDOG→DOG, YADOG→ADOG | yes (own types) | (none of these keys set) | voice collision only (W7-12-27) |

`infantry_death_fx` also reads `DeadBodies` by image (`engine/pres_game.inc:1025`). That is harmless for the roster (none of the roster's image-named sections define DeadBodies). **Fix:** as W7-01-12, use the rules id.

### W7-12-29: The 8-facing muzzle anim of voxel and aircraft shooters is one facing off (MINOR; slice E)

- **Evidence**
  - [CONFIRMED-RE FUN_006fd210, muzzle block after the bullet launch]:

    ```
    if (weapon->Anim.Count(+0x104) == 8) {
        DirStruct d = this->vt+0x308();   // UnitClass 0x73fd90: turret facing if Turret, else body
        anim = Anim[(((d.raw >> 12) + 1 >> 1 & 7) + 1) & 7];
    } else anim = Anim[0];
    new AnimClass(anim, FLH, 0, 1, 0x600); then SetOwnerObject(this) for non-buildings (0x424ad0)
    ```

  - DirStruct 0 is map-north, which is screen up-right (voxel-render.md §2). The anim lists are named by screen compass (MGUN-N, MGUN-NE ...), so the `+1` maps map-north to "NE". The infantry path (vt+0x308 → 0x7081f0 body facing) gives the same mapping, and the clone's infantry formula `(8 - k) % 8` matches it.
  - [CONFIRMED-CODE] `engine/pres_game.inc:680-681`: `muzzle[(((turret_facing>>12)+1>>1)&7)]`, with no `+1`.
  - [CONFIRMED-DATA] 8-entry Anim lists on units: SHAD/SCHP `BlackHawkCannon(E)` MGUN-*, SQD `SquidGrab(E)` SQDG_*, FV `CRM60`. All 83 rulesmd weapon Anim lists have 1 or 8 entries; none has 16.
- **Original:** the flash points along the gun.
- **Clone:** the flash points 45° counter-clockwise of the gun.
  - Also, the muzzle anim is not attached to the firer: a strafing Night Hawk leaves its flash behind. Original SetOwnerObject [CONFIRMED-RE 0x424ad0 call]. COSMETIC.
- **Code:** `engine/pres_game.inc:680-681`, `:705` (spawn at a fixed cell point).
- **Fix:**
  - Use `name = muzzle[((((facing>>12)+1)>>1) + 1) & 7]` with the turret facing for Turret=yes types and the body facing otherwise.
  - Optionally move the spawned anim with the firer while it lives.

### W7-12-30: The FX weapon is resolved once per image from the primary, so secondary and elite impacts, projectile art and trails are wrong (MINOR; slice E)

- **Evidence**
  - [CONFIRMED-CODE] `engine/pres_fx.inc:151-174` `add_unit`: `muzzle`, `anim_list`, `proj_image`, `proj_rotates` and `conventional` all come from `Primary` (else `Weapon1`) per image. Only the beam flags of `Secondary` are kept (`weapons2_`, `:176-181`).
  - The users of that data:
    - `impact()` (`pres_game.inc:589-614`);
    - `pres_draw_projectile` (`pres_fx.inc:814`);
    - `effects_.unit_weapon(u.image, 0)` (`pres_game.inc:713`).
  - [CONFIRMED-DATA] Affected weapons:
    - Elite AnimLists never shown: VTEXPLOD (HTNK/MTNK/TNKD/APOC), KTSTLEXP (ZEP), ARTYHE (HORNET), S_CLSN (HARV elite 120MM).
    - Secondary AnimLists replaced by the primary's: SMKPUFF (HTK/FLAKT/HYD AA), GUARDWH (GGI deployed), SCHOPWH (SCHP deployed), MammothTusk HE (APOC), SquidPunch HE (SQD).
    - Projectile art lost: GGI deployed `AAHeatSeeker2 Image=DRAGON UseLineTrail` falls back to the placeholder square (M60 has no image); APOC AA DRAGON; SCHP deployed 120MM.
  - [CONFIRMED-RE FUN_006fd210] Fire builds the bullet, muzzle anim, Report and beams from the *fired* `WeaponTypeClass` (a local holding the slot's weapon). There is no per-type cache.
- **Fix:** build `FxWeapon` per weapon id and look it up with the shot's slot weapon. The same key as W7-03-06's proposed `w_active_id`: elite slots plus Weapon1..N.
- **Cross-ref:** W7-03-06, W7-03-05 (IFV modes).

- **Infantry instance (merged from sub-audit A, A-08):** a deployed GI plays the M60 `GIAttack` report and the MGUN flash instead of `[Para] Report=GIAttackDeployed`. A deployed GGI plays `GIAttack` with an MGUN flash instead of `[MissileLauncher] Report=GuardianGIDeployedAttack` with no flash [CONFIRMED-DATA; CONFIRMED-RE 0x6FD210]. Code: `pres_game.inc:774`, `pres_fx.inc:151-176`.

### W7-12-31: Electric bolt: 8-frame life (original 17), IsAlternateColor unread, no bolt-end sparks (MINOR; slice E)

- **Evidence**
  - [CONFIRMED-RE 0x4c1d30 / 0x4c2750] EBolt +0x28 = 0x10000. `EBolt::UpdateAll` shifts it right by 1 every frame and removes the bolt at 0, so the bolt lives 17 frames.
    - The bolt is re-drawn (`FUN_004c1e40`) with fresh midpoint-displacement jitter each frame: up to 8 subdivisions while the segment is over 64 leptons.
    - It is drawn as 3 lines: two in `PALETTE.PAL[colour]` and one in `PALETTE.PAL[15]`.
    - `colour = IsAlternateColor ? 5 : 10` (`(-(alt!=0) & ~4) + 10`).
  - [CONFIRMED-DATA] The convert `DAT_0087e644` is built from PALETTE.PAL (0x52bcf1, string 0x8250a4). Index 10 = (80,80,252), index 5 = (252,252,84), index 15 = (252,252,252). `IsAlternateColor` = weapon+0x153 (loader 0x77148c); used by `AssaultBolt` (SHK coil charge) and `OPCoilBolt`.
  - [CONFIRMED-RE 0x4c2980] `EBolt::Fire` spawns `ParticleSystem(Rules+0x1020 = [General] DefaultSparkSystem=SparkSys)` at the bolt end. The key name was read at 0x66c58a.
  - [CONFIRMED-RE 0x6fca30 / 0x4c2af0] For a UnitClass firer the bolt start follows the firer's FLH each frame.
  - [CONFIRMED-CODE] `pres_fx.inc:216`: life `fw.bolt ? 8`. The colours white plus (110,110,255) are close to the original. `fill_beam` (`:183-195`) has no IsAlternateColor.
- **Fix:**
  - Set the bolt life to 17 frames.
  - Add `alt` to FxWeapon: outer colour (252,252,84) when IsAlternateColor.
  - Spawn SparkSys (pres_effects already runs Spark systems) at the target on each bolt.

### W7-12-32: House-colour (Prism) laser: wrong colour, white core, wrong glow (MINOR; slice E)

- **Evidence**
  - [CONFIRMED-RE 0x6fc6d0] With IsHouseColor (weapon+0x14D):
    - inner = `house+0x56FC` (LaserColor);
    - outer = inner >> 1 per channel;
    - laser +0x20 = 1 (house-colour draw path `FUN_00550680`), and Fire sets thickness +0x1C = 2 (0x6fea16 path).
  - [CONFIRMED-RE 0x50b820] `LaserColor = c * 240 / |c|` per channel, clamped to 255. A channel below 96 becomes 0. `c` = house colour RGB (+0x56F9, written in FUN_00500960 from the house's 16-bit colour).
    - Example: DarkRed (0,230,255) gives about (237,0,0).
  - [CONFIRMED-RE 0x550680] The draw starts with the house colour and draws `thickness` parallel line pairs, halving the brightness each step. It stops when every channel is below 0x40.
  - Without IsHouseColor, inner/outer = `LaserInnerColor` (+0x120) / `LaserOuterColor` (+0x123) and `LaserOuterSpread` (+0x126) randomises the outer.
  - [CONFIRMED-CODE] `pres_fx.inc:227-232`: outer = `house` (= `house_palette[24]`, ramp shade 8, about 0.54·V, `pres_game.inc:746-749`) and inner = white. A non-house laser is always (216,0,184) and ignores LaserInner/OuterColor; CowShot's own 216,0,184 / 80,0,88 is not read.
  - [CONFIRMED-PLAY] `e_f02_zoom.png`: the Prism shot is a thin pale-yellow/white line.
- **Fix:**
  - Compute LaserColor per house from its ColorScheme RGB with the normalise-to-240 rule.
  - Draw the core in LaserColor and 2 halving outer pairs.
  - Read LaserInnerColor / LaserOuterColor for non-house lasers.
  - IsBigLaser (CowShot) is a different draw path; not traced.

### W7-12-33: `UseNormalLight=yes` anims and beams are dimmed by the ambient light pass (MINOR; slice E)

- **Evidence**
  - [CONFIRMED-RE 0x422cc0 AnimClass draw] intensity defaults to 1000. If the type's `UseNormalLight` (+0x35D; reader 0x428686) is **no**, intensity = the cell's light (cell+0x10A, or +0x10C with the cell convert). If **yes**, it stays 1000, i.e. full bright.
  - [CONFIRMED-DATA] 49 artmd anims carry `UseNormalLight=yes`: every big explosion (TWLT*, EXPLOLRG/MED, S_CLSN*, VTEXPLOD, APOCEXP, KTSTLEXP ...).
  - [CONFIRMED-CODE] `engine/main.cpp:4086-4089`: `g_fx->draw` (anims and beams) runs **before** `g_light->pass`, so every FX anim and beam is darkened or tinted by ambient light. UseNormalLight is never read (`git grep`).
- **Original:** on dark or night maps, explosions stay bright while muzzle puffs and smoke are lit by the cell. Lasers and bolts are drawn as direct lines; it is [SUSPECTED] that they are unlit (no light term seen in 0x4c1e40 / 0x54fef0).
- **Fix:**
  - Draw UseNormalLight anims and the beams after the light pass, or exempt them from it.
  - Keep the other anims lit.

### W7-12-34: Voxel units never tilt on ramps (MINOR; slice B)
- **Evidence:**
  - [CONFIRMED-RE] **DriveLocomotionClass::Draw_Matrix `0x4AFE70`** (voxel-render.md §2):
    - the draw matrix is `ramp_matrix(loco+0x18) ∘ facing`;
    - entering or leaving a ramp lerps the tilt over a timer (`+0x20/+0x28/+0x2C`, `FUN_00754860(ramp, t)`);
    - any tilt sets the cache key to -1 (uncached).
  - [CONFIRMED-code] `draw_vehicle_unit` always rasterizes with yaw only. `rasterize_model` takes only a pitch for rockets (`main.cpp:3448-3466`).
- **Fix:**
  - Pass the cell's ramp index (`pres_cell_ramp`) to a tilted `rasterize_model`, using the 3×4 ramp matrices of `0x753AD0` / `DAT_00B440F0`.
  - Cache the result per (part, facing, ramp).
  - Lerp at ramp edges, or snap as a first step.
- **Cross-ref:** none in W7 (new).

### W7-12-35: A soldier that dies in water (WetDie1/2) leaves a land corpse floating on the water (COSMETIC; slice A)

- **Evidence:**
  - [CONFIRMED-RE 0x520810]: when a sequence finishes, cases 0x14/0x15 (WetDie1/2) and 0x24 (AirDeathFinish) call UnInit (vt+0xF8) with no DeadBodies spawn. Only Die1-5 leave corpses.
  - Clone: `pres_game.inc:1024-1035` spawns a `DeadBodies` corpse for every non-vanish death, whatever the sequence.
  - [CONFIRMED-PLAY infB L33-40]:
    - `cheat infkill 0:TANY 1` on open water gave `iseq(WetDie1)=1`, `iseq(Die1)=0`, then `corpses=1` (no other death before f=112);
    - a skeleton corpse lies on the sea (`12-evidence/A/c_tany_corpse_water.png`).
- **Fix:** skip the corpse when the body's last sequence was WetDie1/2 (or AirDeath*). The cheapest test is `u.ia.seq`.
- **Cross-ref:** W7-01-43(b).

### W7-12-36: Zero-count / missing sequences: the original refuses them, the clone substitutes (COSMETIC; slice A)

- **Evidence:**
  - [CONFIRMED-RE 0x51D4C0 head]: `if (Sequence[q].CountFrames == 0) return 0` — the request is ignored and the current sequence keeps playing.
  - Clone:
    - `main.cpp:1488` turns `DeployedIdle=0,0,0` (GI/GGI) into count 1;
    - `inf_anim.cpp:65-77` maps missing entries to kin (DeployedFire→FireUp, WetAttack→FireUp, Crawl→Walk…).
  - Reachable case: a deployed YURI (no DeployedFire) that shoots would play FireUp and then fall back to Ready while still deployed. In the original it keeps `Deployed=299,2,0`. Other stock cases are unreachable today.
- **Fix:**
  - Keep `count=0` as count 0.
  - In `play()`, return without change when the resolved entry has count 0 or is absent. The wet/panic substitutions are the only legal swaps.
  - Keep the fallback only for the presentation's default table (gates).

### W7-12-37: A shot force-interrupts non-interruptible sequences (Deploy, Undeploy, Down, Up, Cheer) (COSMETIC; slice A)

- **Evidence:**
  - [CONFIRMED-RE 0x5203E0]: the fire sequence is requested with `vt+0x558(q, 0)`, i.e. not forced. PlayAnim then refuses it while the running sequence's Interrupt byte is 0 (DoControls 0x7E9F7C: Down, Up, Die*, WetDie*, Deploy, Undeploy, Cheer, AirDeath*).
  - Clone: `inf_anim.cpp:190-192` plays the fire sequence with `force=true`. A GI that deploys next to an enemy cuts its 15-frame Deploy (300..314) to DeployedFire on the first shot. A soldier dropping prone snaps to FireProne mid-Down.
- **Fix:** use `force=false` for the fire request. Keep the rest unchanged; the deploy edge and deaths stay forced.

### W7-12-38: The infantry muzzle Anim ignores the weapon FLH (fixed 15 px lift, no forward/lateral offset) (COSMETIC; slice A)

- **Evidence:**
  - [CONFIRMED-RE 0x6FD210, TechnoClass::Fire]: the weapon `Anim=` list is indexed by facing when it has 8 entries, as `((round8(facing)+1)&7)`. It is created at the muzzle coordinate from GetFLH (vt+0x300 → `InfantryClass` 0x521A60 → vt+0xB0 = 0x522F80 → `TechnoClass::GetFLH` 0x6F3030), and attached to the firer (`FUN_00424AD0`).
  - GetFLH uses the weapon's FLH (Primary/SecondaryFireFLH, or the elite variant via GetWeapon), rotated by the body facing, with L negated on odd burst shots.
  - Vanilla has no prone or deployed FLH: [CONFIRMED-DATA] artmd has 0 `PronePrimaryFireFLH`/`DeployedPrimaryFireFLH` keys (Ares-only), so a prone soldier flashes at standing height in the original too.
  - In open-topped transports 0x522F80 uses the transporter's FLH.
- **Clone:**
  - `pres_game.inc:693-696` has the facing pick right: `(8-k)%8` matches the original's world+1 rule for screen blocks.
  - But it uses `lift = 15.0` and no F/L offset. The GI's MGUN flash (FLH 80,0,105) sits on the body centre instead of ~9 px ahead at the gun.
  - FLAKT GUNFIRE (90,0,175) is drawn ~10 px too low.
  - YURICNTL (15,0,140) is drawn ~5 px too low.
- **Fix:**
  - For infantry, project `PrimaryFireFLH` (or `SecondaryFireFLH` for slot 1/deployed fire) with the vehicle formula at `pres_game.inc:683-690`, using the infantry 8-way facing yaw.
  - Mirror L per burst shot.
  - Optionally attach the anim to the unit.
- **Cross-ref:** W7-02-10 (garrison OccupantAnim). For infantry the Fire path's OccupantAnim branch is never taken: vt+0x400 = 0x41BFE0 returns 0.

### W7-12-39: The sequence facing label is applied as a screen direction; the original applies it as a world DirStruct (45° off) (COSMETIC, SUSPECTED; slice A)

- **Evidence:**
  - [CONFIRMED-RE 0x523A30] The art parser reads `Start,Count,Mult,Label` (`"%d,%d,%d,%s"`) and maps Label by string compare to 0..7. String 0x8248BC "NW" = 7, clockwise from 0x8248D8 = 0.
  - [CONFIRMED-RE 0x520810 default branch] On finish, a labelled sequence sets the facing to `DirStruct(label << 13)`.
  - [CONFIRMED-RE 0x6FD210] The muzzle index rule `(world_dir8 + 1) & 7` → MGUN-N shows that world dir 7 (NW) is screen-up, so a world label X is screen X+45° clockwise.
  - So after `Idle1=…,S` the original soldier ends facing world S = screen SW (block k=3). The clone (`inf_anim.cpp:103`, `(8-label)&7`) turns him to screen S (block 4).
  - Not confirmed by play. The label→string order between 0x8248D8 and 0x8248BC was not dumped.
- **Fix:** if confirmed, `facing = (8 - ((label + 1) & 7)) & 7`, i.e. convert world dir8 to the screen block.

### W7-12-40: Rocketeer flight-sequence rules (RE detail). Extends W7-01-21 (INFO; slice A)

- **Evidence:**
  - PlayAnim turns Ready into Hover when the unit is airborne (vt+0x54) and Hover is defined [CONFIRMED-RE 0x51D4C0].
  - Moving with the jumpjet locomotor (CLSID 0x7E8AC0): Hover if the speed fraction (+0x578) ≤ 0.8, else Fly [CONFIRMED-RE 0x520C70].
  - On stopping, Walk/Fly/Hover → Ready (→ Hover in the air), Crawl → Prone, Swim → Tread [CONFIRMED-RE 0x520C70].
  - Firing with the jumpjet locomotor → FireFly (0x1A) [CONFIRMED-RE 0x5203E0].
  - AirDeathStart → AirDeathFalling on finish; AirDeathFinish → UnInit [CONFIRMED-RE 0x520810].
  - While Paradrop is running and the soldier is still falling (+0x8D), every other request is refused; on landing → Ready [CONFIRMED-RE 0x51D4C0, 0x520810].
- **Clone:** see W7-01-21 (the Rocketeer is outside `inf_anim`, `world.cpp:5394`).

### W7-12-41: The barrel is always drawn on top of the turret (COSMETIC; slice B)
- **Evidence:**
  - [CONFIRMED-RE] **Unit voxel draw `0x73A3A0` @0x73ACBB-0x73ADD4:**
    - `q = ((turret_facing >> 13) + 1 >> 1) & 3`;
    - when q is 0 (N) or 3 (W), the barrel (+0xC0, cache +0x280) is drawn BEFORE the turret (+0xB8, cache +0x258);
    - otherwise the turret is drawn first and then the barrel. Multi-turret types do the same with `+0xC8+n*8` / `+0x158+n*8`.
  - [CONFIRMED-code] `main.cpp:3471-3487` always draws the turret, then the barrel.
  - [CONFIRMED-PLAY, weak] `12-evidence/B/rhino_north_barrel_over_turret.png`: a north-facing Rhino barrel is blitted over the turret top.
- **Affects:** HTNK, MTNK, APOC, HTK (types with a barl.vxl).
- **Fix:** In `draw_vehicle_unit`, compute q from `u.turret_facing` and swap the two `draw_voxel_sprite` calls when q is 0 or 3.

### W7-12-42: No rocking from `Rocker=yes` warheads (COSMETIC; slice B)
- **Evidence:**
  - [CONFIRMED-RE] **WarheadType `Rocker` → +0x14E** (parser @0x75C3D1).
  - [CONFIRMED-RE] **DamageArea `0x489190` @0x489A94:** when Rocker is set and `amount = min(damage × k, 4.0)` exceeds a threshold, every object in the 7×7 cells around the impact gets `vtbl+0x3D8 Rock(direction, amount)`. Direction is away from the impact for the centre cell and a fixed vector elsewhere; the outer cells only rock when CellSpread > threshold.
  - The rock angles live at unit `+0x328/+0x32C` and are consumed by Draw_Matrix `0x4AFE70` (they make the draw uncacheable).
  - [CONFIRMED-DATA] Rocker=yes is set on 18 warheads, among them V3WH, BlimpHE, MaverickHE, ARTYHE, SCHOPWH, SonicWarhead, GrandCannonWH, Parasite and ParasitePlus.
- **Clone:** `grep -i rocker engine/` finds 0 hits.
- **Fix:** Add a per-unit rock state (pitch/roll angles that decay to 0, plus Rock(dir, amount)) driven by an impact event for Rocker warheads, and apply it in the tilt matrix of W7-12-34.
- **Cross-ref:** W7-04-25 (tilt after crushing) and W7-04-26 (drone rocking) use the same `+0x3D8` Rock. Implement once.

### W7-12-43: `Explodes=yes` units should always use the LAST Explosion= entry (COSMETIC; slice B)
- **Evidence:**
  - [CONFIRMED-RE] **`FUN_007376C0`:** after the random pick, `if ((Explodes(+0xD15) || HasAbility(EXPLODES=10) (FUN_0070C550)) && (Ammo(+0x684) == -1 || this->Ammo > 0))`, then anim = Explosion[count-1]. `FUN_0070C550` checks the veteran (+0x29C+n) and elite (+0x2AE+n) ability arrays.
  - [CONFIRMED-DATA] APOC and DTRUCK have `Explodes=yes`, so their death anim is always S_TUMU60.
  - [CONFIRMED-RE] The same function then picks a random **DestroyAnim** (+0x748 list); no roster vehicle sets one.
- **Clone:** `pres_game.inc:1080-1083` always picks at random.
- **Fix:** If `Explodes=yes` (or an EXPLODES vet ability), use `ex->back()`.

### W7-12-44: The shadow of a multi-section voxel should use only the `ShadowIndex` section (COSMETIC; slice B)
- **Evidence:**
  - [CONFIRMED-RE]
    - the unit and aircraft draws (`0x73A3A0` tail, `0x4144B0`) call `FUN_004DAFE0(body vxl, ShadowIndex (+0x7FC, art key @0x714CD1), …)`;
    - the shadow is drawn only when `loco->Is_To_Have_Shadow()` and `!NoShadow (+0xD98)` (`0x706060`);
    - `FUN_00706710` builds the shadow from the HVA frame-0 matrix of section `ShadowIndex` alone, and halves its scale when type `+0xD96` is set;
    - the blit uses flags 0x2001 (shadow/darken).
  - The turret is never part of the shadow. `UseTurretShadow` is a UnitType key (+0xE15) with no voxel-draw consumer found.
  - [CONFIRMED-DATA] `ShadowIndex=2` on SHAD and SCHP (section DUMMY01), `ShadowIndex=3` on SPYP (DUMMY01). These are rotor-less silhouettes.
- **Clone:** `rasterize_shadow` (`voxel_raster.cpp:167-213`) flattens every section of HVA frame 0, so rotor blades appear in the shadow.
- **Fix:** Read the art `ShadowIndex` (default 0) and flatten only that section.

### W7-12-45: `PixelSelectionBracketDelta` is ignored (COSMETIC; slice C)
- **Evidence:**
  - [CONFIRMED-RE 0x6F5A00] Every y term of the board and the pips adds `TechnoType+0x3E0`. That offset is `PixelSelectionBracketDelta` [CONFIRMED-RE 0x713513].
  - The `DrawPips` anchor (x-10, y+10) does **not** add it.
  - [CONFIRMED-DATA] In the roster only GGI has it (-6, which raises the bar 6 px). BRUTE, YURIPR, ARND and RMNV have it too, but they are not in the roster.
- **Clone:** `draw_unit_pips` uses fixed -25 / -24 / -26.
- **Fix:** parse the key into UnitCombat and add it to `oy` and to the board y.
- **Cross-ref:** none.

### W7-12-46: Own disguised units lose their chevron (COSMETIC; slice C)
- **Evidence:** [CONFIRMED-RE 0x745580 / 0x522520] The chevron skip is `IsDisguisedAs(PlayerPtr)`. That returns false when the viewer owns the unit (`FUN_004F9870`), so the player's own disguised SPY or Mirage keeps its chevron.
- **Clone:** `if (u.disguised) continue;` hides it for everyone.
- **Code:** `engine/main.cpp:3674`.
- **Fix:** skip only when `u.disguised && u.house` is not allied to the local house.
- **Cross-ref:** none.

## Verified OK

### Infantry sequences and deaths (slice A)
- The art Sequence parse: 42 keys in YRpp order, the 3 ints plus the label tolerated (`main.cpp:1453-1505`), and the frame formula `first + k*mult + stage` with mult 0 = facing-independent. [CONFIRMED-DATA] All roster ranges fit their SHP body (frames/2); the table is in `12-evidence/A/seqcheck.txt`.
- **CLEG and TRST data quirks.**
  - CLEG `Cheer=166,8,0` reaches frame 173, the first shadow frame (body 0..172).
  - TRST `FireUp/FireProne=164,6,6` reaches 211 > 187.
  - The data is identical in the original. The clone's draw (`iso_renderer.cpp:491`, frame < full count) shows the same frames, so it is faithful.
  - TRST FireUp lasts at most 1 stage before the TerrorBomb suicide.
- The DoControls table (Interrupt/IsMobile/RandomStart/Rate) equals 0x7E9F7C (`inf_anim.cpp:20-32`). The rate normaliser for Idle1/2, WetIdle1/2, Hover and Cheer is FUN_005FAAD0 [CONFIRMED-RE 0x51D4C0], already documented. RandomStart: every PlayAnim call seen passes random_start = 0 [CONFIRMED-RE 0x5203E0/0x520810/0x520C70], so the clone ignoring it is faithful.
- The facing block order is counter-clockwise from N, and the 8-way infantry MGUN pick `(8-k)%8` is equivalent to the original's `(world_dir8+1)&7` [CONFIRMED-RE 0x6FD210].
- Panic substitution: Walk with fear > 199 [CONFIRMED-RE 0x51D4C0]. Down needs Crawls=yes (+0xEBD), and Up/Deploy clear prone [CONFIRMED-RE 0x51D4C0]. Deploy→Deployed, Undeploy→Ready, deploy group loops Deployed [CONFIRMED-RE 0x520810].
- **InfDeath→anim for weapon kills** [CONFIRMED-RE 0x517DB5 via §13; CONFIRMED-DATA].
  - Mapping in `pres_game.inc:1007-1013`:
    - 1 Die1; 2 / 9 Die2;
    - 3 S_BANG34; 4 FLAMEGUY; 5 ELECTRO (= `[Animations]` 2nd entry);
    - 6 YURIDIE; 7 NUKEDIE; 8 VIRUSD; 10 BRUTDIE;
    - 0 / absent → removed with no anim.
  - NotHuman → Die1 and no `[General] DeadBodies` [CONFIRMED-RE 0x520810].
  - Warhead census [CONFIRMED-DATA]:

    | InfDeath | warheads | examples |
    |---|---|---|
    | none | 18 | |
    | 0 | 4 | |
    | 1 | 17 | |
    | 2 | 18 | incl. Crush |
    | 3 | 15 | |
    | 4 | 11 | |
    | 5 | 5 | |
    | 6 | 3 | |
    | 7 | 6 | incl. RadSite |
    | 8 | 2 | |
    | 9 | 2 | |
    | 10 | 4 | |

- Cheer is wired through `make_house_cheer` (trigger 113, victory) and gated on art Cheer [CONFIRMED-PLAY anim-infantry.play precedent]. Paradrop frames under the chute come from art `Paradrop=` (`world_an.cpp:824-830`). WetDie selection in water works [CONFIRMED-PLAY infB: iseq(WetDie1)=1].
- Muzzle anims exist only for M60/M60E/Para (MGUN-N..NW, 8-way), FlakGuyGun/FlakGuyAAGun (GUNFIRE), MindControl (YURICNTL), PsiWave/TerrorBomb (RING1) [CONFIRMED-DATA]. All other roster infantry weapons have no `Anim=` (the flash is in the SHP frames), so no muzzle anim is correct.

### Voxel vehicles and death effects (slice B)
- **Voxel parts.** The hull, `tur` and `barl` parts are loaded and drawn with the hull at the body facing and turret+barrel at the turret facing. Both are quantized to 32 with `((f>>10)+1>>1)&31`. [CONFIRMED-RE 0x73A3A0; voxel-render.md §2]
- **TurretOffset.** It is applied along the BODY facing before the turret rotation. The sign convention matches the FLH axes. [CONFIRMED-RE 0x73A3A0 @0x73A98D `T(+0x720·scale)`, 0x6F32C0; code main.cpp:3471-3481]
- **HVA frame source** = WalkedFramesSoFar (+0x538) % body HVA frames, the same for aircraft (0x4144B0).
  - FootClass::AI 0x4DA440 increments it every `WalkRate` frames while moving and every `IdleRate` frames when idle (default 0 = never). It increments every frame for an airborne `DeployToLand` type (SCHP). It is frozen while sinking.
  - The clone (`pres_bldanim.inc:408-443`) treats "airborne aircraft" as moving, which gives the same rotor spin for SHAD/SCHP/SPYP.
  - Only SCHP, SHAD and SPYP have HVA frames > 1 in the data (2 each). ZEP and every ground vehicle have 1. [CONFIRMED-RE; CONFIRMED-DATA, HVA headers parsed]
- **UnloadingClass.** A harvester that is dumping draws its UnloadingClass voxel (HARV→HORV, CMIN→CMON). [CONFIRMED-RE 0x73A3A0 @0x73A3EA `+0x6E0 && type+0x6B8`; code main.cpp:3941-3946]
  - The miners have no separate harvesting animation: HORV/CMON/HARV/CMIN HVAs are 1 frame. The refinery's own dump anim is handled by the building-anim layer. [CONFIRMED-DATA]
- **House remap for all voxels.** `Remapable` (+0xCCF) is read only by the parser, the ctor and the type CRC `FUN_00716540`; there is no draw consumer. [CONFIRMED-RE] The clone's always-remap is faithful.
- **Turret recoil.** `TurretRecoil` → +0xCA2, plus TurretTravel/Compress/Hold/RecoverFrames and the Barrel* equivalents, are parsed at 0x714610-0x71477A [CONFIRMED-RE]. No roster image sets them [CONFIRMED-DATA], so no recoil is correct (agrees with W7-03).
- **Damage smoke.** On reaching yellow (NowYellow/NowRed), one random `BehavesLike=Smoke` entry of DamageParticleSystems is created at Location + DamageSmokeOffset and follows the unit. It is removed above ConditionYellow, and the SparkSys entries are inert. [CONFIRMED-RE FUN_00700DA0 via effects.md §1; code pres_effects.inc:338-356, 1045-1058]
- **Debris.** Min/MaxDebris, DebrisTypes/DebrisMaximums voxel debris (TIRE), the MetallicDebris fallback, and no debris low over water. [CONFIRMED-RE via effects.md §2; code pres_effects.inc:236-263]
  - Ships have no MaxDebris, so they spawn none [CONFIRMED-DATA].
- **Death Explosion=.** Uniform random pick, at the unit coord, flags 0x600. [CONFIRMED-RE 0x7376C0]
- **No screen shake.** The "Strength > ShakeScreen" branch calls `FUN_0048DDE0`, which is an empty `return;` in YR 1.000, so there is no shake on unit death. [CONFIRMED-RE] The clone has none: faithful.
- **NoShadow.** No ground shadow for V3ROCKET and DMISL. [CONFIRMED-RE 0x706060 +0xD98; code main.cpp:3425]
- **Sink selection.** `Naval && !Underwater && !Organic && Weight ≥ ShipSinkingWeight && land==Water && !+0x271` sinks; the other branch explodes. [CONFIRMED-RE 0x736DC5]
- **Rocket hull pitch.** V3ROCKET/DMISL draw at their pitch (W4).

### Selection feedback (slice C)
- `DrawHealthBar` (gamemd 0x6F5A00, vtable +0x44C for Unit/Infantry/Aircraft/Building) [CONFIRMED-RE]:
  - infantry board frame 1 at (x+11, y-25) and 8 pips from (x-5, y-24);
  - others board frame 0 at (x+1, y-26) and 17 pips from (x-15, y-25), 2 px apart;
  - count `ftol(ratio*max)`, values below 2 become 1, capped at max;
  - the board is drawn only when selected (+0x83).
  - The clone's hud_pips.inc matches every term except PSBD (W7-12-45).
- Health colours: YR pips.shp frames 16/17/18 at `<= ConditionYellow (Rules+0x1700)` / `<= ConditionRed (+0x1708)` [CONFIRMED-RE 0x6F5A00].
  - The clone uses the RA2 conquer.mix pips.shp frames 15/16/17. These are pixel-identical (indices 124/127, 156/159, 229/239) and land on the same screen pixel after canvas centring (10×15 at (4,5) vs 16×16 at (7,6)) [CONFIRMED-DATA, decoded both files].
  - The thresholds come from rulesmd (`main.cpp:6765-6766`) [CONFIRMED-code].
- PIPBRD: YR has no conqmd copy, so the RA2 conquer.mix one is the file the game loads [CONFIRMED-DATA].
- Hover: `DrawExtras` (0x6F46F0) calls DrawHealthBar for selected objects, and for hovered (+0x431) non-selected ones with the disguise test [CONFIRMED-RE]. The clone's W6-05-07 behaviour matches, apart from the shroud case in W7-12-19.
- Select box: a non-building gets no corner brackets or box. The 3-D line box (`FUN_006F5450`, `DSurface::DrawLine`) runs only in the `WhatAmI == Building` branch [CONFIRMED-RE 0x6F46F0]. The clone draws only the board in the game [CONFIRMED-PLAY c1].
- Chevron frames: YR pips.shp 14 (veteran) / 15 (elite), with 19 for a negative rank (not reachable). Offsets +(5,2) infantry and +(10,6) others, flags 0xE00 [CONFIRMED-RE 0x709E10]. The clone loads conqmd pips.shp and draws 14/15. This is correct apart from W7-12-19/07/08/12 [CONFIRMED-PLAY c1: E1 elite, MTNK vet].
- Chevrons are drawn every frame, not only when selected [CONFIRMED-RE: the call at 0x6F48E2 is outside the IsSelected test]. The clone matches.
- The vtable +0x448 slot (0x6F5620) is an empty stub, so it draws nothing [CONFIRMED-RE].
- TALKBUBL.SHP (0xAC03F4) is drawn by DrawExtras only for a global talk-bubble object set by `FUN_0070E550`. Its timer is `Rules+0x1800`, and its only callers are 0x6DCE20 and 0x6E35B0 [CONFIRMED-RE]. Not a per-unit selection feature. I did not audit it, and it is out of scope.
- Elite promotion flash: already W5-PRES R-120 (`pres-state.play`), so not repeated. There is no promotion anim key in rulesmd [CONFIRMED-DATA].
- Iron Curtain / chrono unit tints exist (`pres_unit_tint`, `main.cpp:3917`). They are covered by W5-PRES R-120, so not re-audited.

### Voices and EVA (slice D)
- **soundmd parse.** Sounds (`$`/`#` stripped), Volume, Limit, MinVolume, Range, Priority, VShift, FShift, Delay, Control `random/all/loop/predelay/interrupt/ambient/attack/decay` (flags only), Type `global/local/shroud`, and [Defaults] seeding (MinVolume 50, Range 10, Volume 80, Limit 5) all match the RE'd struct [CONFIRMED-DATA soundmd [Defaults]; code `src/sounds.cpp:759-817`, `engine/pres_audio.inc:237-283`]. The exception is the Attack/Decay counts (W7-12-24).
- **Control=random is a uniform pick with replacement** (no anti-repeat), as FUN_004048B0 does (audio-playback.md §2). The clone uses `next_rand_() % n` (`audio_mixer.cpp:154`) [CONFIRMED-RE (doc) + code].
- **Every roster type's voice data is resolved from its own rules section.** APOC, MTNK, V3, JUMPJET, DRON, ENGINEER and MCV select/move/attack keys fired as expected [CONFIRMED-PLAY play1/play2 `snd(...)` counters].
- **ClickedMission → voice mapping** [CONFIRMED-RE 0x6FF0A0 + 0x7084A0 + 0x708240]:

  | Mission | Voice | Clone |
  |---|---|---|
  | Attack (1) | VoiceAttack (per-weapon override, else random VoiceAttack) | `order_voice` matches |
  | Move (2) | VoiceMove | matches |
  | Enter (7) | VoiceEnter, else VoiceMove | matches |
  | Capture (8) | VoiceCapture, else VoiceEnter, else VoiceMove | matches |
  | Harvest (10) | VoiceHarvest | matches |
  | Unload (16) | VoiceDeploy | matches |
  | any other mission | random VoiceSpecialAttack | matches |

  `order_voice` is at `engine/pres_audio_events.inc:230-253`. The only difference is Boris's VoiceSecondaryWeaponAttack (W7-02-06).
- **VoiceSelect variants** [CONFIRMED-RE 0x708330]: VoiceSelectEnslaved (techno `+0x2DC` set) and VoiceSelectDeactivated (`+0x1C8` flag), else VoiceSelect.
  - Enslaved: no Allied/Soviet roster type has the key (only SLAV) [CONFIRMED-DATA], so n/a.
  - Deactivated: ROBO only, already W7-03-10.
- **There are no elite voice sets for the roster.** No `VoiceSelectElite`-style keys exist. VoicePrimary/SecondaryWeaponAttack (TechnoType `+0x580..+0x58C`, picker 0x708520) is set only on BORIS in the roster [CONFIRMED-DATA].
- **EVA_UnitLost for normal units:** local house + RadarEvent 7 (8 cells, 200 frames), Priority IMPORTANT [CONFIRMED-RE 0x4D97D0; CONFIRMED-PLAY play1: one GI killed gave exactly one `EVA_UnitLost` plus `GIDie`].
- **EVA_UnitPromoted** plus [AudioVisual] `UpgradeVeteranSound=UpgradeVeteran` / `UpgradeEliteSound=UpgradeElite` at the unit, local house only (`engine/pres_game.inc:170-176`), as FUN_006F9380 @6F95F6/6F9664 (presentation.md §1.3) [CONFIRMED-DATA + code].
- **There is no generic "unit under attack" EVA.** Only EVA_OreMinerUnderAttack (harvester, RadarEvent 4) exists, and the clone matches (`pres_game.inc:1115-1119`) [presentation.md §1.3 RE].
- **VoiceFeedback:** the 30% roll on the NowYellow edge for the player's units, with VoiceFeedback lists. SUB, SQD and DLPH have `Volume=0` fear entries, so they are effectively silent, as in the data [CONFIRMED-RE W5 0x701B20; code `pres_audio_events.inc:313-320`].
- **VoiceCrashing / CrashingSound / ImpactLandSound** for ZEP, SHAD, SCHP and BEAG are keyed by id-equal images, so they work [code `pres_audio_events.inc:331-357`]. BPLN `VoiceCrashing=MigVoiceDie` has no soundmd section, so it is silent in the original too [CONFIRMED-DATA].
- **The [AudioVisual] Create{Unit,Infantry,Aircraft}Sound are empty in rulesmd.** Only typed CreateSounds (TANY, BORIS, ZEP, CCOMAND, DTRUCK) speak; W6-05-06 is wired for them except DTRUCK (W7-12-28) [CONFIRMED-DATA].

### Weapon FX, beams and palette (slice E)
- House remap ramp: `pal_for_hsv` (`engine/world.cpp:397-409`) replaces indices 16-31 with the ColorScheme 0x68C110 / ramp 0x68BDB0 formula (sin/cos table, integer HSV2RGB). [CONFIRMED-RE w5-presentation.md §4]
- The 8 MP HSV triples (Gold 43,239,255; DarkRed 0,230,255; Orange 25,230,255; Magenta 221,102,255; Purple 201,201,189; DarkBlue 153,214,212; DarkSky 131,200,230; DarkGreen 81,200,210) match rulesmd `[Colors]`. [CONFIRMED-DATA]
- SHP infantry and voxels share the same theater unit palette plus remap. Voxels index the unit palette, not the vpl palette. [CONFIRMED-RE voxel-render.md §4; CONFIRMED-CODE `art_chain().unit_pal`, main.cpp:2512]
- `Remapable=` (art) is read into TechnoType+0xCCF (0x714c7c), but its only other reader is the type CRC (0x716a43). The original remaps every techno, including V3ROCKET/DMISL `Remapable=no`, so the clone's not reading it is faithful. [CONFIRMED-RE]
- AltPalette anims (+0x361) take ColorScheme[0] or the owner's convert. [CONFIRMED-RE 0x422cc0; clone `pres_fx.inc:676-683`]
- `Translucent=yes` life fade (frame > 0.2/0.4/0.6·End → 25/50/75 %) and `Translucency=25/50/75` → blitter 2/4/6. [CONFIRMED-RE 0x422cc0, constants 0x7e2548/50/58 = 0.2/0.4/0.6; clone `pres_fx.inc:642-652`]
- Warhead AnimList pick (damage/25 buckets) and Conventional water SplashList (damage/35). [CONFIRMED-RE FUN_0048a400 per presentation.md §5; clone `pres_game.inc:595-610`]
- Infantry 8-way muzzle index `(8-k)%8` equals the original `(dir8+1)&7`. [CONFIRMED-RE FUN_006fd210 + infantry-anim.md §4/§5]
- EBolt colours (white plus blue) are close to PALETTE.PAL[15] / [10]. Bolts are re-jittered every frame, as in the original. [CONFIRMED-RE 0x4c1e40]
- Projectile trails: `Trailer=` / SpawnDelay, UseLineTrail colour and decrement (DRAGON 216,216,255 / 16; MEDUSA 208,208,208 / 12), voxel bullets (ZBOMB, SUBT), V3TAKOFF / V3TRAIL. [CONFIRMED-CODE pres_effects.inc:1276-1284 against effects.md §3]
- Muzzle anim types are 1 or 8 entries in all 83 rulesmd weapon Anim lists; no 16-facing lists exist. [CONFIRMED-DATA]
- Prism Tank fires Weapon1 Comet (WeaponCount path) and its house-colour flag is read. Colour rule: see W7-12-32. [CONFIRMED-CODE pres_fx.inc:153-154, 190]
- The beams are visible in play: Tesla bolt, Prism laser, Desolator / Chrono beam. Impacts logged: TSTIMPCT, S_CLSN16/22, XGRYMED2, PIFFPIFF. [CONFIRMED-PLAY `D:\ra2-re\w7-pres-E-out\play1\pres.log`, evidence PNGs]

## Appendix A: per-slice coverage detail

### Slice A detail
Legend for the "seqs engine plays" column:
- **std** = Ready, Walk, FireUp, Idle1/2, Die1/2, Panic, Cheer.
- **+P** = Down/Prone/Crawl/FireProne/Up (Crawls=yes).
- **+D** = Deploy/Deployed/DeployedFire/Undeploy.
- **+W** = Tread/Swim/WetAttack/WetIdle1/2/WetDie1/2.
- **+Pd** = Paradrop (team drops only).

Other notes:
- Die3-5 are `0,1,1` placeholders in every art section. Neither the original nor the clone plays them.
- InfDeath→anim is the mapping for WEAPON kills, which is OK everywhere. W7-12-08 applies to every row for non-weapon kills.

| id | image | seqs in art | seqs engine plays | frame-range errors | deaths (InfDeath->anim) | muzzle/FLH | notes |
|---|---|---|---|---|---|---|---|
| E1 | GI | 24 (std+P+D, DeployedIdle 0,0,0, Paradrop) | std+P+D+Pd | none (max 371/372) | OK; partial(W7-12-08) | MGUN-* OK; FLH partial(W7-12-38); deployed Report partial(W7-12-30) | Deploy cut by a shot (W7-12-37) |
| E2 | CONS | 19 | std+P+Pd | none | OK; partial(W7-12-08) | no Anim (M1Carbine) n/a | |
| SHK | SHK | 19 (ConSequence) | std+P+Pd | none | OK; partial(W7-12-08) | bolt beam; Anim n/a | |
| ENGINEER | ENGINEER | 19 | std+P+Pd | none | OK | n/a | |
| SENGINEER | ENGINEER | 19 | std+P | none | OK | n/a | image shared: W7-01-12 |
| YENGINEER | ENGINEER | 19 | std+P | none | OK | n/a | image shared: W7-01-12 |
| JUMPJET | ROCK | 26 (Fly/Hover/FireFly/Tumble/AirDeath*) | std only (ground poses) | none | air death missing (W7-01-21) | no Anim | GAP W7-01-21 + W7-12-40 |
| GHOST | SEAL | 26 (+W) | std+P+W | none | WetDie corpse GAP W7-12-35 | no Anim (MP5) | beach GAP W7-12-09 |
| TANY | TANY | 26 (+W, WetDie 20 frames) | std+P+W | none (max 610/619) | WetDie corpse GAP W7-12-35 [PLAY] | no Anim | beach GAP W7-12-09 |
| YURI | YURI | 22 (+Deploy/Deployed/Undeploy, no DeployedFire) | std+P+D | none | OK | YURICNTL at fixed lift (W7-12-38) | missing DeployedFire→FireUp fallback (W7-12-36) |
| PTROOP | PTROOP | 22 (Deploy* unused: not a Deployer) | std+P | none | OK | YURICNTL (W7-12-38) | Paradrop=0,1,0 data |
| IVAN | IVAN | 21 (Deploy/Deployed/Undeploy unused) | std+P | none | OK | no Anim | |
| CIVAN | CIVAN | 21 | std+P | none | OK | no Anim | |
| DESO | DESO | 22 (DeployedFire 292,7,0) | std+P+D | none | OK; rad kills W7-12-08 [PLAY] | beam; no Anim | DeployedFire never fires: W7-02-05 |
| DOG / YDOG | DOG | 18 | std (Crawls=no) | none | NotHuman→Die1 OK, no corpse OK | n/a | |
| ADOG / YADOG | ADOG | 18 | std | none | NotHuman→Die1 OK | n/a | |
| CLEG | CLEG | 19 | std+P | Cheer 166,8 last=173 = 1st shadow frame (data, same in both) | OK (erase: W7-01-44) | beam; no Anim | |
| SPY | SPY | 18 | std+P | none | OK | n/a | |
| CCOMAND | CCOMAND | 19 | std+P | none | OK | no Anim | Paradrop=0,1,0 data |
| SNIPE | SNIPE | 19 (ConSequence) | std+P+Pd | none | OK | no Anim | |
| FLAKT | FLAKT | 19 | std (Crawls=no) | none | OK | GUNFIRE at 15 px, FLH H=175 → ~25 px (W7-12-38) | |
| TERROR | TRST | 20 | std | FireUp/FireProne 164,6,6 reach 211 > 187 (shadow half; data, same in both) | Suicide→Die1 (W7-12-08) | RING1 | |
| GGI | GGI | 24 (+D, DeployedIdle 0,0,0) | std+P+D+Pd | none | OK | deployed missile shows MGUN (W7-12-30) | |
| BORIS | BORIS | 21 (SecondaryFire 401,1,1 / SecondaryProne) | std (Crawls=no); Secondary* never | none (max 408/409) | OK | no Anim | GAP W7-12-10 (extends W7-02-06) |
| INIT (YuriParaDropInf) | INIT | 19 | std+P+Pd | none | OK | no Anim | Yuri side only; for reference |

### Slice B detail
Legend: B = body .vxl, T = tur, Br = barl, (n) = HVA frames when > 1. "OK" = faithful as far as checked.

| id | vxl/hva parts (body/tur/barl/extra) | turret & barrel draw | HVA anim | shadow | damaged smoke | recoil | FireFLH per facing | death explosion/debris/sink/crash | notes |
|---|---|---|---|---|---|---|---|---|---|
| AMCV (MCV) | B | n/a | n/a | OK | OK | n/a | n/a (no weapon) | OK random + MetallicDebris (Max 6) | ramp tilt W7-12-34 |
| HARV | B+T, TurretOffset 50 | OK (offset along body) | n/a; dump body HORV OK | OK | OK | n/a | partial(W7-12-15: no TurretOffset in FLH) | OK random + TIRE ×≤4 | UnloadingClass HORV (1 frame) drawn while Unloading |
| APOC (MTNK) | B+T+Br | partial(W7-12-41) | n/a | OK | OK | n/a (no TurretRecoil) | GAP W7-12-15 (Burst 2, L=25 flip; MammothTusk FLH should be 0,0,0) | GAP W7-12-43 (Explodes → last entry) | |
| HTNK | B+T+Br | partial(W7-12-41) | n/a | OK | OK (offset 100,100,275) | n/a | OK (150,0,100, L=0) | OK random + MetallicDebris | |
| SAPC (TRS) | B | n/a | n/a | OK | OK | n/a | OK (80,0,120) | GAP W7-12-14 on water | amphibious |
| MTNK (GTNK) | B+T+Br | partial(W7-12-41) | n/a | OK | OK | n/a | OK (L=0; elite Burst 2 → no visible flip) | OK | |
| CARRIER | B | n/a | n/a | OK | OK | n/a | n/a (spawner) | GAP W7-12-11 (sinks) | |
| V3 | B (+V3WO) | n/a | n/a | OK | OK | n/a | OK (-160,0,75) | OK | NoSpawnAlt known W7-04-27 |
| ZEP | B (1 frame in data) | n/a | n/a (no propeller frames in zep.hva) | OK (airborne silhouette, W5) | OK | n/a | OK (-50,0,-140) | GAP W7-12-12/W7-12-13 (BlimpBomb anim on crash) | W7-05-09/15 |
| DRON | SHP | n/a | n/a | n/a | OK | n/a | n/a | OK | SHP vehicle |
| HTK | B+T+Br, TurretOffset -80 | partial(W7-12-41) | n/a | OK | OK | n/a | GAP W7-12-15 (no TurretOffset; Secondary = same value) | OK + TIRE ×≤6 | |
| DEST | B (+DESTWO) | n/a | n/a | OK | OK | n/a | OK (280,0,120) | GAP W7-12-11 (sinks) | NoSpawnAlt known W7-06-12 |
| SUB | B | n/a | n/a | n/a (submerged) | n/a (no DamageParticleSystems) | n/a | OK (150,0,0) | GAP W7-12-14 (Underwater → explodes on water) | |
| AEGIS | B | n/a | n/a | OK | OK | n/a | OK (no FLH key → 0,0,0) | GAP W7-12-11 (sinks) | |
| LCRF | B | n/a | n/a | OK | OK | n/a | n/a | GAP W7-12-14 on water | |
| DRED | B (+DREDWO) | n/a | n/a | OK | OK | n/a | GAP W7-12-15 (Burst 2, L=43 flip) | GAP W7-12-11 (sinks) | W7-06-12 |
| SHAD | B 3 sect (2) | n/a | OK (rotor) | GAP W7-12-44 (ShadowIndex 2) | OK | n/a | OK (175,0,10) | GAP W7-12-12 | |
| SQD | SHP | n/a | n/a | n/a (NoShadow) | n/a | n/a | n/a | n/a | SHP |
| DLPH | SHP | n/a | n/a | n/a | n/a | n/a | n/a | n/a (SHP path) | SHP |
| SMCV | B | n/a | n/a | OK | OK | n/a | n/a | OK | ramp tilt W7-12-34 |
| TNKD | B (Turret=no) | n/a | n/a | OK | OK | n/a | OK (200,0,55) | OK | |
| TTNK | B+T | OK (no barrel) | n/a | OK | OK | n/a | OK (Elite = same) | OK | |
| CMIN | B | n/a | n/a; dump body CMON OK | OK | OK | n/a | n/a | OK + TIRE | |
| SREF | B+T (charge turret) | partial (W7-03-07) | n/a | OK | OK | n/a | GAP W7-12-15 (Weapon1FLH 48,0,184 unread → 0,0,0) | OK | |
| HYD | B | n/a | n/a | OK | OK | n/a | OK (65,0,180) | GAP W7-12-14 [CONFIRMED-PLAY] | Weight 2 → no sink (OK) |
| MGTK (RTNK) | B (rtnktur/rtnkbarl exist, unused) | n/a (Turret=no) | n/a | OK | OK | n/a | OK (130,0,80) | OK | see open question 2 |
| FV | B+T (fvtur only) | partial (W7-03-05) | n/a | OK | OK | n/a | GAP W7-12-15 (Weapon1/17FLH 64,48,180, Burst 2/4 flip) | OK + TIRE | |
| DTRUCK (TRUCKA) | B | n/a | n/a | OK | OK | n/a | OK | GAP W7-12-13 (no DEMTEXP) + W7-12-43 (last entry) | [CONFIRMED-PLAY] |
| BFRT | B | n/a | n/a | OK | OK | n/a | partial (AlternateFLH known W7-03-12) | OK | |
| SCHP | B 3 sect (2), no tur voxel | n/a (W6-04-17) | OK (rotor) | GAP W7-12-44 (ShadowIndex 2) | OK | n/a | GAP W7-12-15 (SecondaryFireFLH 200,0,250 for deployed 160mm) | GAP W7-12-12 (non-balloon → Explosion=) | |
| ROBO | B+T | OK | n/a | OK | OK | n/a | OK (150,0,100) | OK | Hover loco |
| ORCA (FALC) | B | n/a | n/a | OK (W5) | OK | n/a | OK (no FLH key) | GAP W7-12-12 | roll/pitch: w5-presentation §13 |
| BEAG | B | n/a | n/a | OK | OK | n/a | OK | GAP W7-12-12 | as ORCA |
| HORNET | B | n/a | n/a | OK | OK | n/a | partial(W7-12-15 L=32 burst flip) | GAP W7-12-12 (crash on carrier loss) | |
| V3ROCKET | B | n/a | n/a | OK (NoShadow) | n/a | n/a | n/a | OK (rocket_fx, W4) | pitch drawn (W4) |
| DMISL | B | n/a | n/a | OK (NoShadow) | n/a | n/a | n/a | OK | as V3ROCKET |
| ASW | B | n/a | n/a | OK | OK | n/a | partial(W7-12-15 L=32) | GAP W7-12-12 | |
| PDPLANE | B | n/a | n/a | OK | OK | n/a | n/a | GAP W7-12-12 | |
| BPLN | B | n/a | n/a | OK | OK | n/a | GAP W7-12-15 (25,100,0, Maverick3 Burst 2 → L flip) | GAP W7-12-12/W7-12-13 (BlimpBomb ×0.1) | |
| SPYP | B 4 sect (2) | n/a | OK | GAP W7-12-44 (ShadowIndex 3) | OK | n/a | n/a | GAP W7-12-12/W7-12-13 | |

The ramp tilt (W7-12-34) and Rocker rocking (W7-12-42) apply to every voxel ground unit. They are left out of the per-row cells.

### Slice C detail
Legend:
- **health bar**: inf = PIPBRD frame 1 at (x+11, y-25) + 8 PIPS; veh = PIPBRD frame 0 at (x+1, y-26) + 17 PIPS.
- **pips**: what the original draws -> what the clone draws.
- **group no.**: W7-12-04 applies to every selectable unit.
- **select box**: the original has no bracket for non-buildings. "OK" = the clone also draws only the board.
- **W7-12-19** (chevron and hover leak through the shroud, and for hidden subs/cloaked units) applies to every enemy row and is not repeated per row.
- **W7-12-22** (self-heal pip) applies to every infantry row (Tech Hospital) and every non-organic vehicle row (Machine Shop).

| id | kind | health bar (pos/len) | pips (type expected -> drawn) | veterancy | group no. | select box | notes |
|---|---|---|---|---|---|---|---|
| E1 | I | OK inf/8 | none -> none OK | OK (5,2) | GAP W7-12-04 | OK | Pip=white (as a passenger, W7-12-05) |
| E2 | I | OK inf/8 | none -> none | OK | GAP W7-12-04 | OK | Pip=white |
| SHK | I | OK inf/8 | none -> none | OK | GAP W7-12-04 | OK | Pip=white |
| ENGINEER | I | OK inf/8 | none -> none | OK | GAP W7-12-04 | OK | Pip=Blue |
| JUMPJET | I | OK inf/8 (lifted) | none -> none | GAP W7-12-20 in flight | GAP W7-12-04 | OK | Pip=white |
| GHOST | I | OK inf/8 | none -> none | OK | GAP W7-12-04 | OK | Pip=red |
| YURI | I | OK inf/8 | none (no PipScale) -> none | OK | GAP W7-12-04 | OK | Pip=red; MC feedback W7-12-23 |
| IVAN | I | OK inf/8 | none -> none | OK | GAP W7-12-04 | OK | Pip=red; bomb icon W6-04-11 |
| DESO | I | OK inf/8 | none -> none | OK | GAP W7-12-04 | OK | Pip=red |
| DOG | I | OK inf/8 | none -> none | OK | GAP W7-12-04 | OK | Pip=white |
| CLEG | I | OK inf/8 | none -> none | OK | GAP W7-12-04 | OK | Pip=red |
| SPY | I | OK inf/8 | none -> none | partial(W7-12-46) | GAP W7-12-04 | OK | Pip=blue; own disguised spy loses its chevron |
| CCOMAND | I | OK inf/8 | none -> none | OK | GAP W7-12-04 | OK | Pip=red |
| PTROOP | I | OK inf/8 | none -> none | OK | GAP W7-12-04 | OK | Pip=red; W7-12-23 |
| CIVAN | I | OK inf/8 | none -> none | OK | GAP W7-12-04 | OK | Pip=red |
| SNIPE | I | OK inf/8 | none -> none | OK | GAP W7-12-04 | OK | Pip=red |
| TANY | I | OK inf/8 | none -> none | OK | GAP W7-12-04 | OK | Pip=red; W7-12-19 seen in play |
| FLAKT | I | OK inf/8 | none -> none | OK | GAP W7-12-04 | OK | Pip=white |
| TERROR | I | OK inf/8 | none -> none | OK | GAP W7-12-04 | OK | Pip=red |
| SENGINEER | I | OK inf/8 | none -> none | OK | GAP W7-12-04 | OK | Pip=Blue |
| ADOG | I | OK inf/8 | none -> none | OK | GAP W7-12-04 | OK | Pip=white |
| YENGINEER | I | OK inf/8 | none -> none | OK | GAP W7-12-04 | OK | Pip=Blue |
| GGI | I | partial(W7-12-45) PSBD -6 ignored | none -> none | OK | GAP W7-12-04 | OK | Pip=white |
| BORIS | I | OK inf/8 | none -> none | OK | GAP W7-12-04 | OK | Pip=red |
| YDOG | I | OK inf/8 | none -> none | OK | GAP W7-12-04 | OK | Pip=white |
| YADOG | I | OK inf/8 | none -> none | OK | GAP W7-12-04 | OK | Pip=white |
| AMCV | V | OK veh/17 | none -> none | OK (10,6) | GAP W7-12-04 | OK | |
| HARV | V | OK veh/17 | Tiberium (gem/ore/empty) -> none GAP W7-12-16 | OK | GAP W7-12-04 | OK | Storage=40 |
| APOC | V | OK veh/17 | none -> none | OK | GAP W7-12-04 | OK | W7-12-19 seen in play |
| HTNK | V | OK veh/17 | none -> none | OK | GAP W7-12-04 | OK | |
| SAPC | V | OK veh/17 | Passengers 12 -> rects GAP W7-12-05 | OK | GAP W7-12-04 | OK | not on the Allied cheat roster (image TRS not drawn); code path as BFRT |
| MTNK | V | OK veh/17 | none -> none | OK | GAP W7-12-04 | OK | |
| CARRIER | V | OK veh/17 | Spawns 3 -> none GAP W7-12-17 | OK | GAP W7-12-04 | OK | |
| V3 | V | OK veh/17 | Spawns 1 -> none GAP W7-12-17 | OK | GAP W7-12-04 | OK | |
| ZEP | V | OK veh/17 (on body in flight) | none -> none | GAP W7-12-20 (on the ground) | GAP W7-12-04 | OK | CONFIRMED-PLAY c2 |
| DRON | V | GAP W7-12-21 (inf/8) | none -> none | GAP W7-12-21 (5,2) | GAP W7-12-04 | OK | rides the infantry path |
| HTK | V | OK veh/17 | Passengers 5 -> rects GAP W7-12-05 | OK | GAP W7-12-04 | OK | |
| DEST | V | OK veh/17 | Spawns 1 (ASW) -> none GAP W7-12-17 | OK | GAP W7-12-04 | OK | |
| SUB | V | OK veh/17 | none -> none | OK; W7-12-19 when submerged | GAP W7-12-04 | OK | |
| AEGIS | V | OK veh/17 | none -> none | OK | GAP W7-12-04 | OK | |
| LCRF | V | OK veh/17 | Passengers 12 -> rects GAP W7-12-05 | OK | GAP W7-12-04 | OK | |
| DRED | V | OK veh/17 | Spawns 2 -> none GAP W7-12-17 | OK | GAP W7-12-04 | OK | |
| SHAD | V | OK veh/17 (lifted) | Passengers 5 -> rects GAP W7-12-05 | GAP W7-12-20 in flight | GAP W7-12-04 | OK | |
| SQD | V | OK veh/17 | none -> none | OK | GAP W7-12-04 | OK | Organic: heal pip comes from the Hospital (W7-12-22) |
| DLPH | V | OK veh/17 | none -> none | OK | GAP W7-12-04 | OK | Organic |
| SMCV | V | OK veh/17 | none -> none | OK | GAP W7-12-04 | OK | |
| TNKD | V | OK veh/17 | none -> none | OK | GAP W7-12-04 | OK | |
| TTNK | V | OK veh/17 | none -> none | OK | GAP W7-12-04 | OK | |
| CMIN | V | OK veh/17 | Tiberium -> none GAP W7-12-16 | OK | GAP W7-12-04 | OK | Storage=20 |
| SREF | V | OK veh/17 | none -> none | OK | GAP W7-12-04 | OK | |
| HYD | V | OK veh/17 | none -> none | OK | GAP W7-12-04 | OK | |
| MGTK | V | OK veh/17 | none -> none | OK | GAP W7-12-04 | OK | disguise check as W7-12-46 |
| FV | V | OK veh/17 | Passengers 1 (Gunner) -> 1 white rect GAP W7-12-05 | OK | GAP W7-12-04 | OK | CONFIRMED-PLAY c2 |
| DTRUCK | V | OK veh/17 | none -> none | OK | GAP W7-12-04 | OK | |
| BFRT | V | OK veh/17 | Passengers 5 -> rects GAP W7-12-05 | OK | GAP W7-12-04 | OK | CONFIRMED-PLAY c2 |
| SCHP | V | OK veh/17 (lifted) | none -> none | GAP W7-12-20 in flight | GAP W7-12-04 | OK | |
| ROBO | V | OK veh/17 | none -> none | OK | GAP W7-12-04 | OK | |
| ORCA | A | OK veh/17 (lifted) | Ammo 1 -> none (W7-05-07, ext W7-12-18) | GAP W7-12-20 in flight | GAP W7-12-04 | OK | |
| BEAG | A | OK veh/17 (lifted) | Ammo 1 -> none (W7-05-07, ext W7-12-18) | GAP W7-12-20 in flight | GAP W7-12-04 | OK | |
| HORNET | spawn | OK veh/17 | Ammo 1 -> none (W7-12-18) | n/a | n/a (not selectable) | n/a | |
| ASW | spawn | OK veh/17 | Ammo 1 -> none (W7-12-18) | n/a | n/a | n/a | |
| V3ROCKET / DMISL | spawn | n/a | none | n/a | n/a | n/a | no PipScale |
| PDPLANE / BPLN / SPYP | support | n/a (not selectable) | none | n/a | n/a | n/a | |

### Slice D detail
Voice keys: S=VoiceSelect, M=VoiceMove, A=VoiceAttack, F=VoiceFeedback, D=DieSound, Sp=VoiceSpecialAttack, En=VoiceEnter, Cap=VoiceCapture, Hv=VoiceHarvest, VC=VoiceCrashing, 2A=VoiceSecondaryWeaponAttack, SD=VoiceSelectDeactivated, Dep/Und=Deploy/UndeploySound, MS=MoveSound, Cr=CrushSound, CS=CrashingSound, IL=ImpactLandSound, Cre=CreateSound, Chr=ChronoIn/OutSound, W=Enter/LeaveWaterSound, T=Enter/LeaveTransportSound.

The EVA column is the same for every row:
- **Ready:** GAP W7-12-01 (never spoken).
- **Lost:** OK for normal units, except where the row says otherwise.

Full resolution of every soundmd entry (Sounds, Control, Priority, Limit, Range, Type, Volume, MinVolume, Delay): `docs/gap/wave7/12-evidence/D/rules-voice-keys-resolved.txt`.

| id | image | Voice keys present | resolved in clone | alternation (Control) | EVA ready/lost | notes |
|---|---|---|---|---|---|---|
| E1 | GI | S M A F D Sp Dep Und Cr | partial(W7-12-03) | random; Dep random interrupt | GAP W7-12-01 / OK | F, Dep, Und and Cr are lost with real audio (W7-12-03) |
| E2 | CONS | S M A F D Sp Cr | partial(W7-12-03) | random | GAP W7-12-01 / OK | F and Cr are lost with real audio |
| SHK | SHK | S M A F D Sp Cr | Y | random interrupt | GAP W7-12-01 / OK | |
| ENGINEER | ENGINEER | S M A(=Move) F D Sp En Cap Cr | Y (Allied player) | random | GAP W7-12-01 / OK | shared image (W7-12-27) |
| JUMPJET | ROCK | S M A F Sp; CS IL MS(loop) | partial(W7-12-28) | random interrupt; MS loop random all decay attack | GAP W7-12-01 / OK | no DieSound, dies via CrashingSound. MS/IL lost (W7-05-03) |
| GHOST | SEAL | S M A D Sp(global) W Cr | partial(W7-12-28) | random interrupt | GAP W7-12-01 / OK | W lost (W7-01-12). F is empty in rules |
| YURI | YURI | S M A F D Sp Cr | Y | random | GAP W7-12-01 / OK | |
| IVAN | IVAN | S M A F D Sp Cr | Y | random | GAP W7-12-01 / OK | |
| DESO | DESO | S M A D Sp Cr | Y | random | GAP W7-12-01 / OK | F empty in rules |
| DOG | DOG | S M A F D Sp Cr | Y | single clip; F random interrupt | GAP W7-12-01 / OK | image shared with YDOG; data identical |
| CLEG | CLEG | S M A F D Sp Chr Cr | Y | random; Chr interrupt Limit=1 | GAP W7-12-01 / OK | |
| SPY | SPY | S M A F D Sp(global) Cr | Y | random interrupt | GAP W7-12-01 / OK | |
| CCOMAND | CCOMAND | S M A D Sp Cre Chr Cr | Y | random interrupt | GAP W7-12-01 / OK | |
| PTROOP | PTROOP | S M A D Sp Cr | Y | random interrupt | GAP W7-12-01 / OK | |
| CIVAN | CIVAN | S M A F D Sp Chr Cr | Y | random | GAP W7-12-01 / OK | |
| SNIPE | SNIPE | S M A F D Sp Cr | Y | random interrupt | GAP W7-12-01 / OK | |
| TANY | TANY | S M A F D Sp Cre(critical, global) W Cr | Y | random | GAP W7-12-01 / OK | |
| FLAKT | FLAKT | S M A F D Sp Cr | Y | random | GAP W7-12-01 / OK | |
| TERROR | TRST | S M A F D Sp Cr | Y | random interrupt | GAP W7-12-01 / OK | image differs; no image-keyed key is affected |
| SENGINEER | ENGINEER | S M A F D Sp En Cap Cr | wrong when the player is not Soviet (W7-12-27) | random | GAP W7-12-01 / OK | EngAll* used instead of EngSov* |
| ADOG | ADOG | S M A F D Sp Cr | Y | single clip | GAP W7-12-01 / OK | |
| YENGINEER | ENGINEER | S M A(=Capture) F D Sp En Cap Cr | wrong (W7-12-27) | random | GAP W7-12-01 / OK | |
| GGI | GGI | S M A F D Sp Dep Und Cr | Y | random; Dep interrupt Limit=3 | GAP W7-12-01 / OK | |
| BORIS | BORIS | S M A F D 2A Cre W Cr | partial | random; 2A single, critical, global | GAP W7-12-01 / OK | 2A not read (W7-02-06) |
| YDOG | DOG | as DOG | Y | | GAP W7-12-01 / OK | |
| YADOG | ADOG | as ADOG | Y | | GAP W7-12-01 / OK | |
| AMCV | MCV | S M A(=Move) D Dep MS | Y | random | GAP W7-12-01 / OK | Dep is PlaceBuilding. MS is read by id, so OK |
| HARV | HARV | S M A D En Hv Cr | Y | random | GAP W7-12-01 / OK | |
| APOC | MTNK | S M A D MS Cr | Y [CONFIRMED-PLAY] | random; MS random predelay | GAP W7-12-01 / OK | `w5_bld_key` reads [MTNK] (Grizzly), latent (W7-12-28) |
| HTNK | HTNK | S M A D MS Cr | partial(W7-12-03) | random | GAP W7-12-01 / OK | MS and Cr lost with real audio |
| SAPC | TRS | S M A(=Move) D MS Cr T | partial(W7-12-28) | random | GAP W7-12-01 / OK | T lost; there is no [AudioVisual] default, so it is silent |
| MTNK | GTNK | S M A D MS Cr | Y [CONFIRMED-PLAY] | random | GAP W7-12-01 / OK | |
| CARRIER | CARRIER | S M A; Sinking MS | Y | random | GAP W7-12-01 / OK | no DieSound; it sinks |
| V3 | V3 | S M A D MS Cr | Y [CONFIRMED-PLAY] | random | GAP W7-12-01 / OK | its rockets trigger "Unit lost" (W7-12-02) |
| ZEP | ZEP | S M A VC Cre CS IL MS(loop) | Y | random | GAP W7-12-01 / OK | |
| DRON | DRON | S M A D MS(loop, Attack=3) | partial(W7-12-24) | random; MS Random loop all attack decay | GAP W7-12-01 / OK | the loop never stops (W7-04-30) |
| HTK | HTK | S M A D MS T Cr | Y | random | GAP W7-12-01 / OK | |
| DEST | DEST | S M A; Sinking MS | Y | random | GAP W7-12-01 / OK | |
| SUB | SUB | S M A F(Volume=0) D MS | Y | random | GAP W7-12-01 / OK | |
| AEGIS | AEGIS | S M A; Sinking MS | Y | random | GAP W7-12-01 / OK | |
| LCRF | LCRF | S M A(=Move) D MS T | Y | random | GAP W7-12-01 / OK | |
| DRED | DRED | S M A; Sinking MS | Y | random | GAP W7-12-01 / OK | its missiles trigger "Unit lost" (W7-12-02) |
| SHAD | SHAD | S M A VC CS IL MS(loop) T | Y | random | GAP W7-12-01 / OK | |
| SQD | SQD | S M(=Attack) A F(Volume=0) D MS | Y | single / random | GAP W7-12-01 / OK | |
| DLPH | DLPH | S M A F(Volume=0) D | Y | random | GAP W7-12-01 / OK | |
| SMCV | SMCV | S M A(=Move) D MS Cr Dep | Y | random | GAP W7-12-01 / OK | |
| TNKD | TNKD | S M A D MS Cr | Y | random | GAP W7-12-01 / OK | |
| TTNK | TTNK | S M A D MS Cr | Y | random | GAP W7-12-01 / OK | |
| CMIN | CMIN | S M A(=Move) D En Hv Chr Cr | Y | random; Chr interrupt Limit=1 | GAP W7-12-01 / OK | |
| SREF | SREF | S M A D MS Cr | Y | random | GAP W7-12-01 / OK | |
| HYD | HYD | S M A D MS | Y | random | GAP W7-12-01 / OK | |
| MGTK | RTNK | S M A D MS Cr | Y | random | GAP W7-12-01 / OK | MS is read by id |
| FV | FV | S M A D MS T Cr | Y | random | GAP W7-12-01 / OK | T is IFVTransform |
| DTRUCK | TRUCKA | S M A D MS Cre | partial(W7-12-28) | random | GAP W7-12-01 / OK | Cre DemoTruckCreated lost: it reads the civilian [TRUCKA] section |
| BFRT | BFRT | S M A D MS T | Y | random | GAP W7-12-01 / OK | |
| SCHP | SCHP | S M A VC CS IL MS(loop) T | Y | random | GAP W7-12-01 / OK | |
| ROBO | ROBO | S SD M A D MS Activate/Deactivate Cr | partial | random | GAP W7-12-01 / OK | SD, Activate and Deactivate not read (W7-03-10) |
| ORCA | FALC | S M A (Attack=7 Decay=7) VC CS IL MS(loop, Attack=3) Aux1/2 | wrong (W7-12-24); MS/IL/Aux lost (W7-12-28) | random attack decay | GAP W7-12-01 / OK | about 75% of acks have no speech |
| BEAG | BEAG | S M A (Attack=3 Decay=3) VC CS IL MS(loop, Attack=3) | wrong (W7-12-24) | random attack decay | GAP W7-12-01 / OK | about 40% of acks have no speech |
| HORNET (spawn) | HORNET | CS IL | Y | random | n/a / GAP W7-12-02 | Spawned=yes |
| V3ROCKET (spawn) | V3ROCKET | none | n/a | | n/a / GAP W7-12-02 [CONFIRMED-PLAY] | Spawned, MissileSpawn, DontScore |
| DMISL (spawn) | DMISL | none | n/a | | n/a / GAP W7-12-02 | |
| ASW (spawn) | ASW | CS IL | Y | | n/a / GAP W7-12-02 | |
| PDPLANE (spawn) | PDPLANE | MS(loop) CS IL | Y | random loop all | n/a / GAP W7-12-02 | |
| BPLN (spawn) | BPLN | MS(loop) VC CS IL | Y | | n/a / GAP W7-12-02 | VC=MigVoiceDie is missing from soundmd, so it is silent in the original too |
| SPYP (spawn) | SPYP | MS(loop) CS IL | Y | random loop all attack | n/a / GAP W7-12-02 | |

### Slice E detail
Column key: "muzzle" = weapon Anim= (expected → drawn). "beam" = bolt / laser / rad / line. "impact" = warhead AnimList. Palette for every type is the theater unit pal plus the 16-31 house ramp, OK unless noted. The anim palette is anim.pal (OK).

| id | weapons | muzzle anim (expected -> drawn) | beam/bolt/laser | projectile image/trail | impact anim | palette | house remap | notes |
|---|---|---|---|---|---|---|---|---|
| E1 | M60 / Para (dep) / M60E / ParaE | MGUN-8 -> OK (inf index RE-matches) | n/a | none (Inviso) OK | PIFFPIFF OK | OK | OK | |
| E2 | M1Carbine / E | none -> none OK | n/a | Inviso | PIFFPIFF OK | OK | OK | |
| SHK | ElectricBolt / AssaultBolt / ElectricBoltE | none | bolt drawn; GAP W7-12-31 (life, alt colour, sparks); elite bounce GAP W7-12-07 | Inviso | TSTIMPCT OK | OK | OK | |
| ENGINEER | DefuseKit / VirtualScanner | n/a | n/a | n/a | n/a | OK | OK | |
| JUMPJET | 20mm / E | none | n/a | Inviso | PIFFPIFF OK | OK | OK | flight seqs W7-01-21 |
| GHOST | MP5 / Sapper / MP5E | none | n/a | Inviso | PIFF OK | OK | OK | |
| YURI | MindControl / PsiWave | YURICNTL (1) OK; PsiWave RING1 partial(W7-12-30) | n/a | Inviso | YURICNTL OK | OK | OK | MINDANIM on victim GAP W7-12-23 |
| IVAN | IvanBomber / E | n/a | n/a | n/a | bomb icon (W6-04-11) | OK | OK | |
| DESO | RadBeamWeapon / RadEruption / E | none | rad beam straight, not sine: GAP W7-12-06 | Inviso | none (data) | OK | OK | eruption W7-02-05 |
| DOG | BadTeeth | n/a | n/a | DOGP Rotates (jump) OK | none | OK | OK | |
| CLEG | NeutronRifle / E | none | **green, should be ChronoBeamColor 128,200,255**; GAP W7-12-06 | Inviso | none | OK | OK | erase anim W7-01-44 |
| SPY | MakeupKit | n/a | n/a | n/a | n/a | OK | OK | |
| CCOMAND | ChronoMP5 / FakeC4 / E | none | n/a | Inviso | PIFF OK | OK | OK | |
| PTROOP | MindControl / E | YURICNTL OK | n/a | Inviso | YURICNTL OK | OK | OK | GAP W7-12-23 |
| CIVAN | IvanBomber | n/a | n/a | n/a | bomb icon | OK | OK | |
| SNIPE | AWP / E | none | n/a | Inviso | PIFF OK | OK | OK | |
| TANY | DoublePistols / Sapper / E | none | n/a | Inviso | PIFF OK | OK | OK | C4 placement: no anim in data |
| FLAKT | FlakGuyGun / FlakGuyAAGun / E / E | GUNFIRE OK | n/a | 120MM arcing OK; AA shot partial(W7-12-30) | ground HTRKPUFF OK; AA shows HTRKPUFF not SMKPUFF GAP W7-12-30 | OK | OK | |
| TERROR | TerrorBomb (+Death) | RING1 (1) OK | n/a | Inviso | MININUKE OK | OK | OK | |
| SENGINEER / YENGINEER | as ENGINEER | n/a | n/a | n/a | n/a | OK | OK | |
| ADOG / YDOG / YADOG | Good/BadTeeth | n/a | n/a | ADOGP/DOGP OK | none | OK | OK | |
| GGI | M60 / MissileLauncher / E / E | MGUN-8 OK | n/a | deployed DRAGON + line trail -> placeholder square GAP W7-12-30 | GUARDWH list -> shows PIFFPIFF GAP W7-12-30 | OK | OK | |
| BORIS | AKM / Flare(IsLine, IsHouseColor) / E | none | flare line drawn red (RE-FALLBACK W6-04-24) | Inviso | PIFFPIFF OK | OK | OK | |
| AMCV / SMCV | none | n/a | n/a | n/a | n/a | OK | OK | Explosion= OK |
| HARV | 20mmRapid / 20mmRapidE(120MM) | GUNFIRE OK; elite partial(W7-12-30) | n/a | elite 120MM missing GAP W7-12-30 | PIFFPIFF; elite S_CLSN missing | OK | OK | |
| APOC | 120mmx / MammothTusk / 120mmxE | APMUZZLE OK; elite VTMUZZLE W7-03-06 | TargetLaser W7-04-28 | AA DRAGON+trail missing GAP W7-12-30 | APOCEXP OK; elite VTEXPLOD missing GAP W7-12-30 | OK | OK | |
| HTNK | 120mm / 120mmE | GUNFIRE OK; elite W7-03-06 | n/a | 120MM OK | S_CLSN16/22 OK; elite VTEXPLOD GAP W7-12-30 | OK | OK | |
| SAPC | none | n/a | n/a | n/a | n/a | OK | OK | |
| MTNK | 105mm / 105mmE | GUNFIRE OK; elite W7-03-06 | n/a | 120MM OK | OK; elite GAP W7-12-30 | OK | OK | |
| CARRIER | HornetLauncher | n/a | n/a | n/a | n/a | OK | OK | |
| V3 | V3Launcher | n/a | n/a | V3ROCKET trail OK (effects.md §3) | rocket block AnimList OK | OK | OK | NoSpawnAlt W7-04-27 |
| ZEP | BlimpBomb / BlimpBombE | none | n/a | ZBOMB voxel OK | XGRY list OK; elite KTSTLEXP GAP W7-12-30 | OK | OK (art no Remapable key; remapped, as original) | |
| DRON | DroneJump | n/a | n/a | DRONP OK | none | OK | OK | |
| HTK | FlakTrackGun / FlakTrackAAGun / E / E | GUNFIRE OK | n/a | 120MM OK; AA partial(W7-12-30) | AA SMKPUFF GAP W7-12-30 | OK | OK | |
| DEST | 155mm / ASWLauncher / E | GUNFIRE OK | n/a | 120MM OK | ARTYHE list OK; splash OK | OK | OK | |
| SUB | SubTorpedo / E | none | n/a | SUBT voxel + BBBLELRG Trailer OK | APSplash OK | OK | OK | |
| AEGIS | Medusa / E | none | n/a | MEDUSA + line trail OK | SAMWH OK | OK | OK | DistributedFire W7-06-11 |
| LCRF | none | n/a | n/a | n/a | n/a | OK | OK | |
| DRED | DredLauncher | n/a | n/a | DMISL trail OK | rocket AnimList OK | OK | OK | |
| SHAD | BlackHawkCannon / E | MGUN-8 -> **index off 45°** GAP W7-12-29 | n/a | Inviso | PIFFPIFF OK | OK | OK | anim not attached to the moving firer (W7-12-29 note) |
| SQD | SquidGrab / SquidPunch / E / E | SQDG_*-8 -> off 45° GAP W7-12-29 | n/a | Inviso | punch HE list missing GAP W7-12-30 | OK | OK | |
| DLPH | SonicZap / E | none | sonic wave not drawn (W7-06-10) | n/a | none | OK | OK | |
| TNKD | SABOT / SABOTE | GUNFIRE OK; elite W7-03-06 | n/a | 120MM OK | S_CLSN30 OK; elite GAP W7-12-30 | OK | OK | |
| TTNK | TankBolt / TankBoltE | none | bolt OK-ish GAP W7-12-31; elite bounce GAP W7-12-07 | Inviso | TSTIMPCT OK | OK | OK | |
| CMIN | none | n/a | n/a | n/a | n/a | OK | OK | |
| SREF | Weapon1 Comet / EliteWeapon1 SuperComet | none | laser drawn, colours GAP W7-12-32; fragments GAP W7-12-07 | Inviso | CometWH list OK | OK | OK | charge turret W7-03-07 |
| HYD | FlakTrackGun / FlakWeapon / E / E | GUNFIRE OK | n/a | 120MM OK | AA SMKPUFF GAP W7-12-30 | OK | OK | |
| MGTK | MirageGun / (EliteSecondary MirageGunE) | VTMUZZLE OK | n/a | Inviso | IRONFX OK | OK | OK | |
| FV | Weapon1..17 | per mode W7-03-06 | CowShot IsBigLaser / CRElectricBolt / CRRadBeam: not drawn per mode (W7-03-05) | DRAGON+trail OK (mode 1) | mode 1 only GAP W7-12-30 | OK | OK | |
| DTRUCK | Demobomb | n/a | n/a | n/a | DEMTEXP OK | OK | OK | |
| BFRT | 20mmRapid | GUNFIRE OK | n/a | Inviso | PIFFPIFF OK | OK | OK | |
| SCHP | BlackHawkCannon / 160mm / E / E | MGUN-8 off 45° GAP W7-12-29; deployed GUNFIRE GAP W7-12-30 | n/a | deployed 120MM missing GAP W7-12-30 | deployed SCHOPWH GAP W7-12-30 | OK | OK | |
| ROBO | Robogun | GUNFIRE OK | n/a | 120MM OK | OK | OK | OK | |
| ORCA / BEAG | Maverick(2) / E | none | n/a | DRAGON+trail OK | ORCAAP OK | OK | OK | |
| HORNET | HornetBomb / HornetCollision / E | none | n/a | DRAGON OK | OK; elite ARTYHE GAP W7-12-30 | OK | OK | |
| V3ROCKET / DMISL | (rocket) | n/a | n/a | V3TAKOFF/V3TRAIL OK | OK | OK | OK: Remapable=no is ignored by the original too | |
| ASW | ASWBomb / ASWCollision | none | n/a | DRAGON OK | APSplash OK | OK | OK | |
| BPLN | Maverick3 / E | none | n/a | DRAGON OK | MIGWH OK | OK | OK | |
| PDPLANE / SPYP | ParaDrop / SpyCamera | n/a | n/a | n/a | n/a | OK | OK | |

## Appendix B: method and RE addresses

### Infantry sequences and deaths (slice A)
- **Data:**
  - `artseq.py` + `check.py` (copied to `12-evidence/A/`) resolve rules id → Image → artmd Sequence for 27 ids.
  - SHP headers came from `ra2extract --raw` of `ra2.mix/conquer.mix` and `ra2md.mix/conqmd.mix` (BORIS, GGI and INIT are YR-only).
- **Ghidra (copy deleted):**
  - FUN_0051D4C0 PlayAnim
  - FUN_005203E0 fire-sequence choice
  - FUN_00520810 sequence finished
  - FUN_00520C70 movement sequence
  - FUN_006FD210 TechnoClass::Fire muzzle Anim
  - 0x521A60 / 0x522F80 / 0x6F3030 GetFLH chain
  - 0x41BFE0 (vt+0x400 = 0)
  - 0x523A30 art Sequence parser (label strings 0x8248BC..D8)
  - 0x524440 SecondaryFire/SecondaryProne art keys
  - InfantryClass vtable base 0x7EA058
- **Play:** `D:\ra2-re\w7-pres-A-scripts\infA.play` / `infB.play` on xbayopigs: SEAL/Tanya on water and shore, Tanya WetDie1 corpse, E2s in a Desolator rad field. The probe dump is `12-evidence/A/infB-probe.txt`.

### Voxel vehicles and death effects (slice B)
- **Ghidra copy** `D:\ra2-re\w7-pres-B` (deleted).
  - Scripts in `D:\ra2-re\w7-pres-B-scripts`: dec/decx/refs/dis, plus new `dispw.py` (writes to `[reg+disp]`) and `dispr.py` (any operand `[reg+disp]`).
  - Raw dumps in `D:\ra2-re\w7-pres-B-out` (b1-b14, refs1-3, dis1-3, hva headers in `hva\`).
- **Functions used:**
  - Unit voxel draw `0x73A3A0` (HVA frame, NoSpawnAlt body, turret/barrel order, sink clip +0x3CA, shadow call);
  - aircraft draw `0x4144B0`; shadow `0x4DAFE0` → `0x706060` → `0x706710`; blit `0x706360`; Drive Draw_Matrix `0x4AFE70`;
  - FootClass::AI `0x4DA440` (walk counter); UnitClass::AI `0x735571` (sinking); ReceiveDamage death `0x736DC5`; explode `0x7376C0`; HasAbility `0x70C550`; empty shake `0x48DDE0`;
  - TechnoType parser `0x711510` (TurretRecoil @0x714610, TurretOffset @0x714C1F, ShadowIndex @0x714CD1, FLH block @0x714EB7, AlternateFLH @0x71534F, Explosion @0x712D47, DestroyAnim @0x712E44);
  - UnitType art `0x746450` (UseTurretShadow +0xE15); GetFLH `0x6F32C0`;
  - Warhead parser `0x75C3D1` (Rocker +0x14E); DamageArea `0x489190` @0x489A94; ShipLoco `0x69FC80`; Remapable CRC `0x716540`.
- **Play runs** (`D:\ra2-re\w7-pres-B-out\play1..4`, scripts `b-vis*.play`), all on skirmish:
  - 1: Rhino/Apoc turret facings;
  - 2: xbayopigs: DEST sinking, HYD water death;
  - 3: SHAD kill (grounded, so the crash was not reproduced);
  - 4: DTRUCK death and ZEP kill (grounded).
  - The evidence PNGs are 3× nearest-neighbour crops around the view point.
- **Open questions:**
  1. Flag `+0x8F` in the water-death branch (W7-12-14).
  2. Whether the original loader fills the barrel slot `+0xC0` for a `Turret=no` type with a barl.vxl. The non-turret branch of 0x73A3A0 @0x73A6E8 draws `+0xC0` at the weapon FLH, so the Mirage Tank (MGTK, `rtnkbarl.vxl` 2.3 KB present) may show a gun the clone omits. The loader (`0x5F7900` family) was not traced.
  3. `HarvesterDumpRate` is a [General] float (Rules+0x1528, parser @0x670711) absent from the data. Its consumer (unload timing) was not traced; it is economy, not presentation.

### Selection feedback (slice C)
- **Ghidra:** a copy of w5-pres-re1 (gamemd.exe YR 1.000), now deleted.
- **Scripts:** `D:\ra2-re\w7-pres-C-scripts`: decs.py (finds the function start and decompiles), vts.py (vtable slots), dis2.py, ptrtab.py, shpdump.py, zoom.py.
- **Output:** `D:\ra2-re\w7-pres-C-out` (r1-r13).
- **Vtables:**

  | class | vtable |
  |---|---|
  | Unit | 0x7F4C70 |
  | Infantry | 0x7EA058 |
  | Aircraft | 0x7E12A4 |
  | Building | 0x7E2EBC |

- **Vtable slots:**

  | slot | function | address |
  |---|---|---|
  | +0x110 | DrawExtras | 0x6F46F0 |
  | +0x448 | stub | 0x6F5620 |
  | +0x44C | DrawHealthBar | 0x6F5A00 |
  | +0x450 | DrawPips | 0x708F10 |
  | +0x454 | DrawVeterancy | 0x709E10 |
  | +0x458 | DrawExtraInfo (buildings) | 0x709EE0 |
  | +0xC8 | IsDisguisedAs | 0x745580 / 0x522520 |

- **Art loader:** 0x5F6EA0 loads PIPBRD (0xAC03E8), PIPS (0xAC03EC), PIPS2 (0xAC03F0) and TALKBUBL (0xAC03F4).
- **INI keys:**
  - read in 0x711510: SpawnsNumber +0xD5C, PipScale +0x3D4, PixelSelectionBracketDelta +0x3E0, PipWrap +0x3E4, Gunner +0x805;
  - Pip= is read in 0x523DD0 to InfantryType +0xDFC.
- **Enum tables:** Pip enum table 0x81A968; PipScale table 0x81A9C0 (Ammo 1, Tiberium 2, Passengers 3, Power 4, MindControl 5).
- **Art:** I extracted and decoded YR conqmd.mix and RA2 conquer.mix pips.shp / pips2.shp / pipbrd.shp.
- **Plays:** two `--play` runs, c1 and c2 (scripts copied to the evidence dir). Decisive PNGs are in `docs/gap/wave7/12-evidence/C/`. c1 failed only its 3 SAPC clicks, because SAPC is not on the Allied roster and its image does not render. c2 passed.

### Voices and EVA (slice D)
- **Data.** `rulesmd.ini`/`soundmd.ini`/`evamd.ini` were resolved per roster type with `D:\ra2-re\w7-pres-D-scripts\voices.py`, and image-named sections were checked with `imgsec.py`. The output is copied to `12-evidence/D/rules-voice-keys-resolved.txt`.
- **Ghidra** (copy `w7-pres-D`, now deleted). Key decompiles are in `12-evidence/D/re-voice-path.txt`.
  - Voice path:
    - FUN_006FF0A0 ClickedMission (gate DAT_00821CF2 = AllowVoice);
    - 0x708210 QueueVoice (`+0x4F0`);
    - 0x708330 VoiceSelect;
    - 0x708440 VoiceMove;
    - 0x708520 VoiceAttack;
    - 0x7084A0 VoiceEnter;
    - 0x708240 VoiceCapture;
    - 0x708280 VoiceDeploy;
    - 0x6F9380 @6F9400 queued-voice player.
  - Sound players: 0x74F740 (non-positional) and 0x74F800 (positional).
  - AllowVoice writers:
    - 0x6FB4C0 TechnoClass::Select;
    - 0x4AE660 order dispatch;
    - 0x6D9B30 box select;
    - 0x730290/0x730470/0x730590 team select;
    - 0x7318A0 type select.
  - Sound parse: 0x74F260 (`Attack`/`Decay` ReadInteger) and 0x406570.
  - EVA_UnitLost: 0x4D97D0. The type offset `+0xD54` = `Spawned` was found via the loader string xref @0x71421D. DontScore = `+0xC9F`, Insignificant = ObjectType `+0x232`.
  - The vtable slot math (Unit vtable 0x7F4C70, Infantry 0x7EA058) was derived from the VoiceSelect slot.
- **Play runs** (audio off; evidence is the `snd()/eva()/evareq()` counters and `pres.log`):
  - `voices-d.play`: production EVA, per-type select/move keys, UnitLost.
  - `voices-d2.play`: V3 rocket UnitLost.
  - Outputs: `12-evidence/D/play1-voices-eva.txt` and `play2-v3-unitlost.txt`. The `ceva0xx unavailable` lines are a harness artifact (clips are not decoded when audio is off).
  - `cheat spawn SENGINEER/YENGINEER` units could not be clicked as `unit 0:SENGINEER`; not pursued.

### Weapon FX, beams and palette (slice E)
- **Ghidra:** a private copy `D:\ra2-re\w7-pres-E` of w5-pres-re1, now deleted. Scripts are in `D:\ra2-re\w7-pres-E-scripts` (dec / decx / strx / dispscan / dis2 / peek, runm.ps1); dumps are in `D:\ra2-re\w7-pres-E-out`.
- **Key addresses:**
  - FUN_006fd210 TechnoClass::Fire: muzzle anim, particle systems, TargetLaser, beam dispatch.
  - 0x6fc6d0 CreateLaser.
  - 0x6fc920 / 0x6fca30 CreateEBolt.
  - 0x6fcae0 RadBeam; 0x6fccc0 RadEruption.
  - 0x54faf0 LaserDrawClass ctor; 0x54fef0 / 0x550680 laser draw.
  - 0x4c1d30 / 0x4c1e40 / 0x4c2750 / 0x4c2980 EBolt.
  - 0x658b50–0x659500 RadBeam.
  - 0x77148c WeaponType loader tail. Offsets: IsLaser 0x149, IsHouseColor 0x14D, LaserInner 0x120, Outer 0x123, Spread 0x126, Duration 0x14E, IsBigLaser 0x14C, IsElectricBolt 0x151, IsAlternateColor 0x153, IsRadBeam 0x154, IsRadEruption 0x155, IsMagBeam 0x15C, Anim list +0xF8 / count +0x104.
  - 0x50b820 house LaserColor (constants 240 / 96 / 255).
  - 0x471e73 ControlledAnimationType.
  - 0x422cc0 AnimClass draw (UseNormalLight +0x35D, AltPalette +0x361).
  - 0x52bcf1 PALETTE.PAL convert.
  - Vtables: Unit 0x7F4C70+0x308 → 0x73fd90 (turret facing); Infantry 0x7EA058+0x308 → 0x7081f0.
- **Data:** the per-unit weapon / art table was generated from the extracted rulesmd / artmd (`D:\ra2-re\w7-pres-E-out\table.txt`). PALETTE.PAL was extracted with ra2extract.
- **Play:** one `--play` run, `D:\ra2-re\w7-pres-E-scripts\beams.play`.
  - Skirmish against an Easy AI, house colours 0 (Gold) vs 2 (Orange).
  - Units: SREF / MTNK / CLEG / E1 / SNIPE against TTNK / HTNK / SHK / DESO / FLAKT.
  - Result PASS, 117 frames.
  - Evidence (zoomed crops): `D:\ra2-android\docs\gap\wave7\12-evidence\E\`.
