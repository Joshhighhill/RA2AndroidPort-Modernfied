# Wave 3 gap analysis 03: asset loading and animation systems (cross-cutting)

Read-only audit of master `2c8616c`. Worktree `D:\ra2-wt\w3-assets` (branch `w3-assets`, build dir `build-w3`,
RelWithDebInfo). No engine source was changed. Scratch tools live in `D:\ra2-wt\w3-assets\scratch\census\`
(`census.cpp`, `where.cpp`, `q.cpp`, `hv.cpp`, `ms.cpp`, `ti.cpp`), play scripts in `scratch\play\`, renders in
`scratch\out\` (local only, not committed). Ghidra: private copy `D:\ra2-re\w3-assets` (of `p4proj`, `gamemd.exe`),
outputs `D:\ra2-re\w3-assets-out\{s1,walk,animpal,animdraw,playanim}`.

Status: WORKS / PARTIAL / WRONG / BROKEN / MISSING. Severity: BLOCKER / MAJOR / MINOR. Effort: S (< 1 d) / M (1–3 d) / L (> 3 d).

## 0. Headline

The user's two complaints have different roots.

1. **"Missing too many assets"** is mostly *not* a file-lookup problem. 93 % of placed objects resolve to an existing
   file. What is missing is (a) a short list of very visible lookup bugs (every RA2 landmark, the campaign Terror
   Drones / Dolphins / Squid, the wrong-theater civilian art in every SNOW/URBAN mission) and (b) **whole effect
   subsystems that do not exist**: particle systems (all building/vehicle damage smoke), debris, corpses, parachutes,
   ship wakes, water splashes, trails, ruins, ambient sounds.
2. **"Animations don't work"** is real and systemic: infantry use 4 of the original's 42 sequences; buildings draw
   only `ActiveAnim1..4` (no construction-yard crane, refinery dump, Tesla/Prism charge, shipyard, depot, idle anims);
   building and terrain anims tick per **render** frame (≈4× too fast at 60 Hz, frame-rate dependent); civilian
   dressing in skirmish is frozen at frame 0; voxel HVA animation, `UnloadingClass`, `TurretOffset` are ignored.

## 1. Resolution census

Tool: `scratch\census\census.cpp` (links `libra2assets.a`). It enumerates every type the **26 RA2 campaign maps**
(`mission.ini` → `Maps01.mix`/`MAPS02.MIX`) and the **161 lobby maps** (`langmd.mix/missionsmd.pkt` → `multimd.mix`,
what the clone's lobby offers) place in `[Structures] [Units] [Infantry] [Aircraft] [Terrain] [Smudge]`, the
`OverlayPack`, and campaign TaskForce members, plus the RA2 buildable roster (RA2 `rules.ini` TechLevel 1–10, all three
RA2 theaters). Each (type, theater, context) is resolved two ways:

- **Clone**: a literal transcription of the code path that kind takes in that context (`load_unit_shp`
  main.cpp:1159; `load_vehicle_assets` main.cpp:1260–1335; skirmish `resolve_shp` main.cpp:2103 / `depth_register_building`
  world_depth.inc:90; campaign `Types::building` campaign_runtime.inc:217; `PresMap::art_shp` pres_map.inc:366;
  `PresFx::type` pres_fx.inc:345 / `theater_type` :310).
- **Original-like**: art `Image=` (else section) + `.shp`/`.vxl`; `Theater=yes` → theater extension; `NewTheater=yes` and
  name pattern → letter munge with one `G` retry (building-render.md §1); **global** search over every MIX the original
  loads for that theater (RA2 set first, then the `ra2md` set; RA2 `art.ini` from expand01 for RA2 theaters).

Causes: OK; WRONG_VARIANT (clone draws a different file than the original, e.g. temperate art on a snow map);
MIX_SCOPE (file exists in an RA2 MIX the clone never searches); LOADER_KIND (sent to the wrong loader);
ONLY_IN_RA2MD (YR-only art, needed only by YR maps); YR_THEATER_FALLBACK; ABSENT_EVERYWHERE (not on the install —
the original draws nothing either).

### 1.1 Totals (type × theater × context rows; instances = placed objects)

| Context | Rows | OK | WRONG_VARIANT | MIX_SCOPE | LOADER_KIND | ONLY_IN_RA2MD | ABSENT | Placed instances (bad / total) |
|---|---|---|---|---|---|---|---|---|
| Campaign (26 RA2 maps) | 874 | 649 | 200 | 12 | 10 | 0 | 3 | **2,054 / 28,399** (1,930 wrong art, 82 units never placed, 37 invisible, 5 absent) |
| Skirmish, RA2 theaters (T/S/U) | 853 | 821 | 1 | 7 | 0 | 24 | 0 | 86 / 91,963 |
| Skirmish, YR theaters (DESERT/NEWURBAN) | 1,095 | 128 | 291 | 0 | 0 | 65 | 0 | (W2-GA-09: these maps should not be offered) |
| Buildable roster (RA2, 3 theaters) | 282 | 280 | 2 | 0 | 0 | 0 | 0 | — |
| Anims referenced by used types (Explosion/Anim/AnimList) | 55 | 55 | | | | | | |
| Building anim slots (art `*Anim*`, `BibShape`) | 576 | 468 | | 3 | | 88 | 17 | |
| Every `[Animations]` entry (via `PresFx::type`) | 425 | 134 | | 133 | | 60 | 98 | (133 = Theater=yes debris + isogen/theater building anims) |

Per kind (campaign): infantry 52/52 OK, overlays 84/84, smudges 28/28, terrain 117/117; vehicles 80/84 and team units
143/149 (the rest LOADER_KIND); buildings 145 OK / 200 WRONG_VARIANT / 12 MIX_SCOPE / 3 ABSENT.

### 1.2 Every failure that matters, with cause

**A. Landmarks in `isogen.mix` — invisible (MIX_SCOPE).** `CA*` NewTheater buildings fall back to `cg*` names that ship
only in `ra2.mix/isogen.mix` (and `ra2md/isogenmd.mix`). Every clone building walk searches only
`{iso, theater, generic}.mix` (src/theater_name.cpp:23–27). The original's bootstrap `FUN_005301a0` loads
`CONQMD, GENERMD, GENERIC, ISOGENMD, ISOGEN, CONQUER, CAMEOMD, CAMEO, MAPS*, MULTIMD, THEME(MD), MOV*` (decompiled this
wave, `w3-assets-out/s1/f_005301a0.c`) and resolves globally.

| Type | Name | Maps |
|---|---|---|
| CANEWY04 | Statue of Liberty | **all01t** (first Allied mission) |
| CAWASH01 | White House | all03u, all06u, sov08u, xmp34u4 — the all03u HINT says "don't destroy the White House" over an empty lot (`scratch/out/wh/wh_whitehouse.png`) |
| CAWASH14 / CAWASH15 / CAWSH12 | Jefferson Memorial / Lincoln Memorial / Washington Monument | all03u, all06u, sov08u, xmp34u4 |
| CAPARS01 / CAPARS11 | Eiffel Tower / Arc de Triomphe | sov05u, xmp33u4, xpowdrkeg, xrockets |
| CARUS01 | St Basil's Cathedral | all12s |
| CASTL04 | Gateway Arch | all08u |
| CACHIG04 / CACHIG06 | Chicago landmarks | all04u, xnewhghts |
| AMMOCRAT (`ammo01.shp`) | Ammo crates ×18 | all10s — only in `sno.mix/tem.mix/urb.mix`, the `<ext>.MIX` that `Init_Theater` (`FUN_00534700`) loads and the clone never opens |

The world building exists (blocks cells, can be targeted and destroyed) but `draw_building` returns on `!art.ok`
(main.cpp:2634). Their `*DM`/`*D` damaged/destroyed anims (`CAWSH12A/D`, `CARUS01D`, `CASTL04DM`, …) are in isogen too.

**B. Campaign civilian/neutral buildings drawn with TEMPERATE art in every SNOW/URBAN mission (WRONG_VARIANT, 200 types,
1,930 objects).** `camp::Types::building` hardcodes `theater_candidates(art_id, 'T', "isotemp.mix", "temperat.mix",
".shp")` (campaign_runtime.inc:217) instead of `art_chain()`. Every `CA*`/`GA*`/`NA*` type not in the 39-entry skirmish
roster gets `ct*`/`gg*` art: no snow roofs (`scratch/out/snow/snow_houses.png`, all02s), temperate street furniture on
urban maps. Worst maps: all08u 48, all12s 45, sov11s 42, all04u 40, all06u 37, all01t 36, sov03u 33 objects.

**C. Terror Drone, Dolphin, Giant Squid never exist in the campaign (LOADER_KIND, 82 objects).** `Types::unit` sends every
`[VehicleTypes]` id to `load_vehicle_assets` (campaign_runtime.inc:164–174). `DRON/DLPH/SQD` are `Voxel=no` SHP units, the
voxel load throws, `unit()` returns null and `place_unit` returns -1 (campaign_runtime.inc:862–863). The W2-GA-11 SHP-vehicle
path is skirmish-only (main.cpp:5182, 5192). Affected: all06u, all07t, all08u, all10s, all11t (5 types), all12s, sov08u,
sov10t, sov11s, sov12s — map units **and** TaskForce members (teams spawn short).

**D. Default lobby map and other YR maps (ONLY_IN_RA2MD).** `hillbtwn.map` (the lobby default, URBAN) places 55 civilian
buildings whose art exists only in `ra2md/isourbmd.mix` / `isogenmd.mix` (CALA07–15, CAURB01/02, CASIN03E/S, CASYDN02,
CAPARK04–06, CASANF15, CAMACH). PresMap drops them (`if (!o.shp) { ++missing_; continue; }`, pres_map.inc:158) — empty
plazas and parking lots, and their cells are not blocked (`scratch/out/hb_001.png`). Same on frstbite (CABUNK03/04),
2peaks/hidvally/nowimps (CAMACH), the YR "md" campaign-style maps (CASLAB), SCHP on c2s03md. The clone never opens the
`ra2md` theater overlays (`isotemmd/isosnomd/isourbmd/genermd/isogenmd`) for art.

**E. Building anims not found (PresFx).** `PresFx::type` searches only `conqmd.mix` then `conquer.mix` (pres_fx.inc:478–489);
`theater_type` adds `{iso, theater, generic}` (:323–333). Missed: every `Theater=yes` anim (debris `CRYSTAL1–4`, `BIGBLUE`,
waterfalls `WA01X–WD04X`, tunnel tops `TUNTOP01–04`), every isogen building anim, and conqmd is searched **first**, so a
YR copy overrides the RA2 one where both exist.

**F. Not clone bugs (ABSENT_EVERYWHERE).** `CASIN01E/S/W` (sov03u/sov09u stop signs), `GAGAP_A/AD`, `GAREFNL4`,
`GAHPAD_A`, `CASEAT02_A` have no file on the install; the original draws nothing either.

**G. Patch caches.** `ecache01.mix` (263 entries, 219 identified) holds cut/beta RA2 content (E5, MEDIC, HJCK, BTNK…);
only `GAHPADBB` (cut type) is ever needed. `expand01.mix` (rules/art/missions patch) is used for rules only.

## 2. Asset resolution and palettes: system table

| System | Original (cite) | Clone (file:line) | Status | Sev | Fix sketch | Effort |
|---|---|---|---|---|---|---|
| MIX set | Bootstrap `FUN_005301a0`: CONQMD, GENERMD, GENERIC, ISOGENMD, ISOGEN, CONQUER, CAMEOMD, CAMEO, MAPS(MD)*, MULTIMD, THEME(MD), MOVIES/MOVMD (also `MIXFILES\` CD path). `Init_Theater FUN_00534700`: `%s.MIX`, `ISO%s.MIX`, `<ext>.MIX`, `%sMD.MIX`; `EXPAND*/ECACHE*` patch mixes. One global filename lookup `FUN_005b3ae0` | Each loader opens its own 1–3 nested mixes: buildings `{iso,theater,generic}` (theater_name.cpp:23); infantry `conquer`→`conqmd` (main.cpp:1159); voxels `local`→`localmd` (main.cpp:1281–1310); anims `conqmd`→`conquer` (pres_fx.inc:480); dressing adds `conquer` (pres_map.inc:379). Never: isogen, `<ext>.mix`, ra2md theater overlays, ecache/expand art | WRONG | BLOCKER | One `AssetFS` per scene: ordered list of every original MIX for the theater (RA2 set first, then md set) and a single `find(name)`. Route every loader through it | M |
| Image chain | rules `Image=` → art section; art `Image=` → file name | Buildings/units use rules `Image=` only (`Rules::image`, rules.cpp:73); anims/dressing use art `Image=` (pres_fx.inc:351, pres_map.inc:372) | PARTIAL | MINOR | One `art_file(section)` helper: rules Image → art section → art Image | S |
| Theater letter / extension | `Theater=yes` → `.tem/.sno/.urb`; munge only when art `NewTheater=yes` (building-render.md §1) | Munge on name pattern alone (theater_name.cpp:29); buildings never try the theater extension (main.cpp:2104); campaign extras hardcode `T` (campaign_runtime.inc:217); YR theaters fall back to TEMPERATE (main.cpp:155) | WRONG | MAJOR | Pass the art section; honour Theater/NewTheater; use `art_chain()` everywhere | S |
| Rules/art source | RA2: `rules.ini` + `art.ini` (expand01 patch) | rulesmd + artmd for everything incl. campaign (main.cpp:5118–5121). Differences: refinery anim scheme (RA2 Pre/ProductionAnim vs YR ActiveAnim2–4 + SpecialAnim), 16 LoopEnd/LoopCount, 9 Next, Explosion lists, SNIPE sequence | WRONG | MINOR | Load RA2 art/rules for an RA2 game (or overlay RA2 art over artmd) | S |
| Loader kind dispatch | Type class decides (UnitType + `Voxel=no` → SHP) | Campaign: every vehicle → voxel loader (campaign_runtime.inc:167) | BROKEN | BLOCKER | Reuse the skirmish SHP-vehicle registration | S |
| Missing art handling | Object exists and draws nothing only when no file anywhere | Dressing dropped and cell left unblocked (pres_map.inc:57,84,108,158); world buildings invisible (main.cpp:2634) | PARTIAL | MINOR | Log once per type with the tried list; keep blocking | S |
| Palettes | Units/buildings: theater `unit*.pal` + house remap; terrain/smudge/non-wall overlay: cell LightConvert (iso pal); ore mines: unit convert (RE-Q1); anims: `anim.pal` (`DAT_0087e640`), **AltPalette** → house unit convert, building-owned anims (**ShouldUseCellDrawer**, default) → building's convert (`FUN_00451880` sets anim+0xD4; `FUN_00422cc0`) | All implemented except AltPalette (34 RA2 anims: corpses `DEATH_A–F`, `PARACH`, `DLPHDIE`, ruins `*DM`) and per-anim `UseNormalLight` | PARTIAL | MINOR | Read AltPalette; draw with the owner's remapped unit palette | S |
| Movies | `MOVIES01/02.MIX`, `MOVMD03.MIX`, also `MIXFILES\MOVIES0N.MIX` on the CD (`FUN_005301a0`) | Not played anywhere (campaign_scenes.inc has no movie path). Re-verified: all three are 4-byte stubs; no `.bik`/movie MIX anywhere on `D:\` | MISSING (data absent) | MINOR now | Play `Intro/Brief/Win/Lose` via the existing Bink decoder when a non-stub MIX is present (allow a user-supplied CD MIX) | M |

## 3. Animation systems

### 3.1 Clock

| System | Original | Clone | Status | Sev | Fix | Effort |
|---|---|---|---|---|---|---|
| Anim tick source | AnimClass AI steps once per **logic** frame; `Rate` = 900/Rate frames per anim frame (presentation.md §5) | `g_pres_tick = frame_` incremented in `Presentation::update` (pres_game.inc:103–104), called once per **render** frame outside the fixed-tick loop (main.cpp:5594; loop ends 5547). Used by building anims (pres_fx.inc:621), terrain anims (pres_map.inc:320), radar. GAPOWR_A (Rate 220 → 4 frames/step) runs at 15 anim fps instead of 3.75 at 60 Hz vsync; half that on a 30 fps phone; ignores GameSpeed. `PresFx::step` uses wall-clock `dt*15` (pres_fx.inc:410), also ignores GameSpeed | WRONG | MAJOR | Advance a presentation logic counter inside the fixed-tick loop; drive all anim state from it | S |

### 3.2 Buildings

| Slot / feature | Original | Clone | Status | Sev | Fix | Effort |
|---|---|---|---|---|---|---|
| BuildUp | `Buildup=` frames/2 at `BuildupTime*900/frames` (`FUN_00465950`) | Roster only (main.cpp:2193–2205); campaign/garrison extras have none | PARTIAL | MINOR | Load buildup in the shared building loader | S |
| ActiveAnim 1–4 + Damaged, X/Y | 21 slots × 0x44 with ZAdjust/YSort (presentation.md §6) | Drawn over the building, looping (pres_fx.inc:271, 617–624); ZAdjust/YSort ignored; LoopCount/Next ignored for slots (`loop_frame` :336) | PARTIAL | MINOR | Depth key per slot; honour LoopCount/Next | M |
| ProductionAnim / PreProductionAnim | RA2: GACNST_B / NACNST_B **crane**, refinery GAREFNOR dump + GAREFNL3, shipyard `_D` | Not read | MISSING | MAJOR | Play while the factory produces / harvester unloads | M |
| IdleAnim | NACNST_C, GAYARD_A, NAYARD_A, GADEPT_D, NADEPT_B, CAOUTP_D | Not read | MISSING | MINOR | Loop when idle | S |
| SpecialAnim 1–3 | Tesla Coil charge NATSLA_B, Prism GAPRIS_A, GASILO_A, depots/shipyards S1–S3, YR refinery dump GAREFNOR | Not read | MISSING | MAJOR | Trigger on fire/charge/repair/dock events | M |
| War factory doors | DeployingAnim/UnderDoorAnim + RoofDeployingAnim/UnderRoofDoorAnim | Ground door only (pres_fx.inc:292–293, 597) | PARTIAL | MINOR | Roof pair for aircraft-capable exits | S |
| SuperAnim 1–4 | Charge states | READY/FIRE edges, RE-FALLBACK order (pres_fx.inc:294, 604) | PARTIAL | MINOR | RE-Q13 | S |
| DestroyAnim (ruins) | rules `DestroyAnim=` GGCNSTDM, NGCNSTDM, GGPILEDM, GGCSPHDM, NGTSLADM, CACH06DM | Not read; building vanishes after `Explosion=` | MISSING | MINOR | Spawn the ruin anim | S |
| Damage fires | `DamageFireTypes` at `DamageFireOffset0..7` (spawner not located) | Looping FIRE at every offset while damaged (pres_fx.inc:625–638) | PARTIAL (RE-FALLBACK) | MINOR | — | — |
| Damage smoke | `DamageParticleSystems` (SparkSys, SmallGreySSys, BigGreySmokeSys) at `DamageSmokeOffset`; refinery smoke `RefinerySmoke*` | No particle system in the engine (grep 0 hits) | MISSING | MAJOR | Particle system (smoke/spark/gas), §3.7 | L |
| Turrets | `TurretAnim` voxel | pres_turret.inc (W2-GA-13) | WORKS | — | — | — |
| Light posts | `IN*LITE`/`INYELWLAMP` light sources | Placed as invisible buildings; light sources not applied (W2-GA-05 open) | PARTIAL | MINOR | Light-source pass | M |
| Civilian buildings in skirmish | Full BuildingClass: anims, damage frames, fires | Non-garrisonable ones are PresMap dressing: frame 0 only, no ActiveAnim, no damage (pres_map.inc:147–178, 255–313) | WRONG | MINOR | Make them neutral World buildings like campaign | S |

### 3.3 Infantry

Original (Ghidra this wave, `w3-assets-out/walk/f_00520810.c`): InfantryClass runs a DoType state machine over the
art `Sequence=` table (type+0xE3C, 0x24-byte entries `{start, count, facingMult, facing, nSounds, (frame, sound)…}`).
On Die1–5 completion it spawns a corpse from the type's `DeathAnims` or else from `[General] DeadBodies=DEATH_A..F`;
Deploy→Deployed, Undeploy→Ready, AirDeathStart→Falling→Finish, SecondaryFire/Prone transitions; per-frame sequence
sounds play at `stage % count == frame`. `WalkRate` / `IdleRate` are TechnoType ints (+0x294/+0x298, `FUN_00711510`).

| Feature | Clone | Status | Sev | Fix | Effort |
|---|---|---|---|---|---|
| Sequences | 4 of 42: Ready, Walk, FireUp, Die1 (`anim_from_art` main.cpp:1148). No Guard, Prone, Crawl, Down, Up, FireProne, Idle1/2, Die2–5, Deploy/Deployed/DeployedFire/Undeploy (**GI and Guardian GI deploy**), Paradrop, Cheer, Panic, Swim/Wet*, Fly/Hover/FireFly (Rocketeer), SecondaryFire | MISSING | MAJOR | Load the full table; a per-unit DoType state machine | M |
| Walk cadence | Fixed `kWalkFps = 9` for every image (world.cpp:146); WalkRate ignored | WRONG | MINOR | RE-Q4 | S |
| Idle fidgets | None (IdleRate, Idle1/Idle2) | MISSING | MINOR | Random idle at IdleRate | S |
| Prone under fire | None | MISSING | MINOR | Down/Crawl/FireProne | S |
| Death variety | Die1 only; warhead `InfDeath` 0–7 (RA2: 0×12, 1×10, 2×32, 3×6, 4×10, 5×9, 6×5, 7×7) ignored; no `FLAMEGUY`, `INFDIE` (explode), `ELECTRO`, `NUKEDIE`, `YURIDIE` | MISSING | MAJOR | Map InfDeath to Die2/explode/flame/electro anims | M |
| Corpses | `DeadBodies` never spawned | MISSING | MINOR | Spawn after Die* | S |
| Sequence sounds | Not parsed | MISSING | MINOR | Parse the (frame,sound) tail | S |
| Unregistered image | Falls back to GI's table (`default_anim`) — dogs, civilians, Tanya can play wrong frames when not registered | PARTIAL | MINOR | Always register from art | S |

### 3.4 Vehicles, aircraft, ships

| Feature | Original | Clone | Status | Sev | Fix | Effort |
|---|---|---|---|---|---|---|
| SHP vehicles | `FUN_0073b520` frame rule (W2 RE-Q14) | Skirmish works; **campaign broken** (§1.2 C) | BROKEN | BLOCKER | §1.2 C | S |
| HVA animation | Multi-frame HVA: SHAD, SPYP rotors/props (RA2), SCHP, YTNK (YR) | Frame 0 only (voxel_raster.cpp:83, 117, 165) | MISSING | MINOR | Cycle HVA frames per logic frame | S |
| `TurretOffset` | art key (HARV 50, HTK -80, SHAD 50, …) | Not read; turrets drawn at hull origin | WRONG | MINOR | Offset turret along hull facing | S |
| Harvester dump | `UnloadingClass=HORV` (HARV), `CMON` (CMIN) at the refinery; `HarvesterDumpRate` | Not read | MISSING | MINOR | Swap voxel while docked | S |
| Barrel recoil | No RA2 data (2 `Recoilless=yes` only) | — | N/A | — | — | — |
| Death explosion | Random `Explosion=` pick (presentation.md §5, High) | First entry only (main.cpp:1371, pres_game.inc:821) | WRONG | MINOR | Random pick | S |
| Debris | `DebrisAnims`/`MinDebris`/`MaxDebris`, `[General] MetallicDebris` (Bouncer=yes, 34 anims) | Not read | MISSING | MAJOR | Bouncer anim physics (Elasticity) | M |
| Muzzle anims | `Anim=` with 8 entries picked by facing | Always `front()` (pres_game.inc:504); only Primary weapon resolved (pres_fx.inc:96) | WRONG | MINOR | Index by facing; resolve Secondary/Elite | S |
| Ship wakes, splashes | `Wake=WAKE1`, `SplashList=H2O_EXP1..3` on water hits | Not read; land impact anim on water | MISSING | MINOR | Spawn on water | S |
| Paradrop | `Parachute=PARACH` (AltPalette) | Not read | MISSING | MINOR | Attach PARACH while falling | S |
| Kirov / aircraft | Air layer above ground objects | Mixed into ground sort (W2-GA-17 open) | PARTIAL | MINOR | Layer index | M |

### 3.5 Terrain, overlays, water

| Feature | Original | Clone | Status | Sev | Fix | Effort |
|---|---|---|---|---|---|---|
| IsAnimated terrain | Only TIBTRE01–03 in RA2 rules (AnimationRate/Probability) | Implemented (pres_map.inc:66–71, 319–336) but per render frame (§3.1) | PARTIAL | MINOR | §3.1 | S |
| Tile anims | `TileNNAnim` + offsets in theater INI (tunnel tops TUNTOP01–04) | Not read | MISSING | MINOR | Draw the anim over the tile | S |
| Waterfalls | `WA01X–WD04X` (Theater=yes, Flat, ShouldUseCellDrawer, StartSound=WaterfallLoop) | Not loadable (PresFx ignores Theater=yes) | MISSING | MINOR | RE-Q6 how they are placed | S |
| Ore twinkle | `OreTwinkle=TWNK1`, `OreTwinkleChance=30` | Not read | MISSING | MINOR | Random sparkle anim on ore | S |
| Bridge decks layer | Bridge layer | Ground pass (W2-GA-07) | PARTIAL | MINOR | — | — |

### 3.6 AnimClass (generic `[Animations]`)

`PresFx` (pres_fx.inc) implements Image, Rate (900/Rate), Start/End/LoopStart/LoopEnd/LoopCount, Next, Shadow (frame
count only), Translucent (50 %), Report, Crater/Scorch. Usage counts below are RA2 `art.ini`.

| Key (uses) | Original | Clone | Status | Sev |
|---|---|---|---|---|
| Shadow (140) | Draw frame+N/2 darkened | Halves the frame count, never draws the shadow | MISSING | MINOR |
| Translucency 25/50/75 (23), Translucent (78) | Blit levels | Translucent → 50 % only (:447) | PARTIAL | MINOR |
| Flat (47), Layer (352), ZAdjust (15), YSort | Ground-layer/depth | All FX after every object (W2-GA-17) | WRONG | MINOR |
| Bouncer (34), Elasticity (36), Trailer (9: V3TRAIL, SMOKEY2/3, DURASMOKE, NUKEPUFF), TrailerSeperation | Physics + trails. W2 said "no active Trailer=" — wrong: RA2 art has 9 (CMISL V3 rocket, FIREBALL, MISLSAM…), artmd 6 | Not read | MISSING | MAJOR |
| Spawns/SpawnCount, IsMeteor, Damage (46), MakeInfantry | Child anims, damage over area | Not read | MISSING | MINOR |
| RandomRate (41), PingPong (8), Reverse, RandomLoopDelay (3), Normalized (436) | Timing variants | Not read | MISSING | MINOR |
| StartSound (29) / StopSound | StartSound wins over Report | Report only | PARTIAL | MINOR |
| AltPalette (34), UseNormalLight (39), ShouldUseCellDrawer | §2 palettes | Not read (building anims already use building pal) | PARTIAL | MINOR |
| Theater=yes anims | Theater extension | Never resolve (§1.2 E) | BROKEN | MINOR |
| AnimList pick | `min(damage, n*25-1)/25`; `EMEffect` random; water → SplashList | Bucket ok (pres_game.inc:491); no EMEffect/Splash | PARTIAL | MINOR |

### 3.7 Particles and ambient sound

| Feature | Original | Clone | Status | Sev | Fix | Effort |
|---|---|---|---|---|---|---|
| ParticleSystems | `[ParticleSystems]`/`[Particles]`: damage smoke on buildings and vehicles, sparks, refinery smoke, gas | None (no file references Particle*) | MISSING | MAJOR | Smoke/Spark/Gas behaviours from rules | L |
| AmbientSound | 14 RA2 types: ore mines `OreMineExtract`, fountain, waves, cows/chimps, police siren, ships | Not read | MISSING | MINOR | Loop voice per object (mixer has LOOP) | S |
| Anim sounds | StartSound/Report at anim start | Report only | PARTIAL | MINOR | — | S |

## 4. Campaign presentation

| System | Original | Clone | Status | Sev | Fix | Effort |
|---|---|---|---|---|---|---|
| Loading screen | `LS800BkgdName`, `ldscrn[a|s].pal`, UIName/LSLoadBriefing | campaign_scenes.inc:32–90 | WORKS | — | — | — |
| Briefing | Briefing screen with Brief movie + CSF text | Text overlay on F1 (main.cpp:5982, 6352) | PARTIAL | MINOR | Pre-mission briefing page | S |
| Movies | Intro/Brief/Win/Lose/FinalMovie (Bink, campaign.cpp:152–158) | Parsed, never played. Data absent: 4-byte stubs; originals live on the game CDs (`MIXFILES\MOVIES0N.MIX` probe in `FUN_005301a0`) | MISSING (no data) | MINOR | Bink playback when a real MIX is present | M |
| EVA / mission speech | Trigger actions 19/20/21/99, `eva.ini Mis_*` | campaign_audio.inc:39–66 | WORKS | — | — | — |
| Text triggers | Action 11 → message line | Objectives list (campaign_runtime.inc:1548) | WORKS | — | — | — |
| Mission timer | | hud_messages.inc:58, main.cpp:6391 | WORKS | — | — | — |

## 5. Prioritized fix plan

1. **Unified asset resolver (M).** Global ordered MIX list per theater incl. `isogen`, `<ext>.mix`, ra2md overlays; honour
   Theater/NewTheater; use `art_chain()` in campaign. Unblocks: Statue of Liberty (all01t), White House + memorials
   (all03u, all06u, sov08u), Chicago (all04u), Gateway Arch (all08u), ammo crates (all10s), St Basil's (all12s), Eiffel
   Tower/Arc (sov05u), the 1,930 wrong-theater objects in 18 SNOW/URBAN missions, 55 invisible buildings on the default
   lobby map, Theater=yes debris/waterfall anims. Files: src/theater_name.cpp, campaign_runtime.inc:217, main.cpp:2103,
   pres_map.inc:366, pres_fx.inc:310/478.
2. **Campaign SHP vehicles (S).** Unblocks Terror Drones in all06u/all07t/all08u/all10s/all11t/all12s/sov08u/sov11s/sov12s,
   Dolphins and Squid in all11t/sov10t/sov12s (map units and teams).
3. **Logic-tick anim clock (S).** Fixes every building ActiveAnim, ore mine, radar anim everywhere.
4. **Building event slots (M).** ProductionAnim (crane), Pre/ProductionAnim/SpecialAnim (refinery dump), SpecialAnim
   (Tesla/Prism charge), IdleAnim, shipyard/depot sets, DestroyAnim ruins. Every base in every mode.
5. **Infantry DoType machine (M).** Full sequence table, GI/GGI deploy, prone, idle, InfDeath deaths, corpses, paradrop
   parachute. Every mission with infantry; GI deploy is a gameplay visual.
6. **Effects (L).** Particle smoke/sparks, Bouncer debris, Trailer (V3, Patriot missile), wakes, splashes, ore twinkle.
7. **Small fixes (S each).** Random Explosion pick, muzzle by facing, TurretOffset, UnloadingClass, HVA frames, AltPalette,
   anim Shadow/Translucency/Flat layer, StartSound, AmbientSound, civilian buildings as neutral World objects in skirmish,
   RA2 art.ini instead of artmd.
8. **Movies (M)** once the user supplies the CD MIX files.

## 6. Open RE questions

- **RE-Q1** Exact use of `WalkRate`/`IdleRate` (TechnoType +0x294/+0x298) in InfantryClass frame stepping; default values.
- **RE-Q2** InfDeath → sequence/anim table (InfDeath 0–7) and the `+0xEAD` flag that suppresses `DeadBodies`.
- **RE-Q3** AltPalette house: `FUN_00422cc0` reads `*DAT_00b0443c + 0x30c`; is that the owner's colour scheme or scheme 0?
- **RE-Q4** YR refinery `ActiveAnimTwo..Four` (GAREFNL2–4): shown by storage fraction or all at once? (RA2 uses Pre/ProductionAnim.)
- **RE-Q5** Prone trigger (damage threshold / Fearless) and recovery delay.
- **RE-Q6** How waterfall anims `WA01X–WD04X` are placed (tile set, map section or trigger).
- **RE-Q7** DamageFireTypes spawner (W2 RE-P6) and DamageParticleSystems spawn rate.
- **RE-Q8** Building anim draw order relative to the body (ZAdjust/YSort → clone depth key) — W2 RE-Q10.
- **RE-Q9** Whether RA2 `game.exe` resolves art identically to `gamemd.exe` (all RE here is YR 1.001).

## 7. Method notes

- Census: `scratch\census\census.exe "D:/Red Alert 2/" > census.tsv` (2,900 rows; columns kind, id, theater, context,
  maps, instances, clone result, cause, detail with the tried list and the original-like hit).
- Shots: `scratch\play\wh.play` (all03u White House lot), `scratch\play\snow.play` (all02s temperate houses on snow),
  `--scene-shot scratch\out\hb --map hillbtwn.map --peek civ --reveal` (empty lots on the default map).
- Ghidra scripts: `D:\ra2-re\w3-assets\scripts\{strx,disp,fnat}.py`, runner `run.sh`.
