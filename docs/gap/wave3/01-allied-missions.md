# Wave 3 gap analysis 01: Allied campaign missions 1-12, visuals

This is a read-only audit of master `2c8616c`, made on 2026-10-07. The worktree is `D:\ra2-wt\w3-allied` (branch
`w3-allied`, Release build `build-rel`). Nothing is committed. To get evidence, the worktree adds scratch-only
diagnostics (it does not change behaviour):

- `[w3-*]` log lines in `pres_map.inc` and `campaign_runtime.inc`: missing art, skipped objects, overlay and terrain
  counts.
- Four scene cheats in `game_scene_play.inc`:
  - `w3_reveal`: reveals the whole map for every house.
  - `w3_dump`: lists every live building and unit with its art status.
  - `w3_tiles`: prints a tile-index histogram.
  - `w3_view`: centres the view.

The original is RA2 `game.exe` with its data on `D:\Red Alert 2`. "Original" art resolution follows
`building-render.md` §1:
- RA2 `art.ini`, with the expand01 patch winning.
- `Theater=yes` selects the theater extension.
- `NewTheater=yes` munges the theater letter, with one `G` retry.
- Files are found by a global lookup over the MIXes the original loads for that theater: `isoXXX`, `XXX`, `<ext>.mix`,
  `generic`, `isogen`, `conquer`, `local`, `cache` and `expand01`.

The sibling report `03-asset-anim-systems.md` covers the engine-wide systems. This report is the per-mission view. It
cross-references 03 and corrects it in two places (see §3, RC-1a and RC-2).

**Tooling** (everything is under `D:\ra2-wt\w3-allied\scratch\`):

| Tool | What it does |
|---|---|
| `census\census3.py` → `census.txt` | Every object section, TaskForce member, presentation trigger action, `[Basic]` movie/theme and `mission.ini` loading-screen entry of the 12 maps (maps copied from the wave-2 extraction) |
| `census\resolve.py` → `resolve.txt` | For every placed structure, terrain and smudge type in each map's theater, the file the original resolves versus the file the clone's code path resolves. Clone paths are transcribed from `camp::Types::building` (campaign_runtime.inc:185-240), `PresMap::art_shp` (pres_map.inc:366) and `PresFx::theater_type` (pres_fx.inc:310). MIX entry lists come from `build-rel\ra2extract --list` (`census\lists\`) |
| `census\pairs.py` → `pairs_out.txt` | Renders every original-versus-clone file pair (frame 0, theater unit palette) with `ra2extract --frames` and diffs the pixels |
| `census\gen_table.py` → `type_table2.md` | The per-type table in §2 |
| `census\tileanim.py` | Maps the `[w3-tile]` histogram onto each theater INI's `Tile##Anim` sets |
| `play\gen.py` → `play\w3-<map>.play` | Runs the real campaign `GameScene` (`set campaign <MAP> GDI`). It shoots the loading screen, the start view and the F1 briefing, then reveals the map and dumps state. At the start area, every landmark and the six densest structure clusters it takes 2 shots 6 frames apart. `play\diff.py` diffs each pair, so animating pixels show up magenta in `*_diff.png` |
| `play\w3t-<map>.play`, `play\w3-lamp.play` | Tile dump and own-unit shots; a close-up of one lamp post |

Each mission run takes 10-30 s in the Release build. All 12 runs passed (`play_result=PASS`). Screenshots, renders of
the original art and 2x2 contact sheets (`shots\m_allNN.png`) stay local and are not committed. The EVA clips logged
as "unavailable" in these runs are a harness artifact: `--play` runs with audio off, so `want_clips` is false at
pres_audio.inc:162. In-mission speech itself works (W2-CA-12).

## 0. Headline

The user's two complaints have five concrete causes in the Allied campaign. All of them show up in screenshots.

1. **Every famous landmark is invisible.** The affected objects:
   - the Statue of Liberty (ALL01);
   - the White House, Jefferson Memorial, Lincoln Memorial and Washington Monument (ALL03 and ALL06);
   - the Chicago Associates Center and Chicago Water Tower (ALL04);
   - the Gateway Arch (ALL08);
   - St Basil's Cathedral (ALL12);
   - 18 ammo crates (ALL10).

   The clone shows empty plinths, empty lots and empty plazas, and ALL06 adds black tile holes. ALL03's HINT says
   "don't destroy the White House" over an empty lawn. These types are real World buildings: they block cells, can be
   targeted and trigger the mission. Only the art lookup fails. The files are in `isogen.mix` and `sno.mix`, which the
   campaign resolver never opens. The Statue also needs the art key `Palette=lib` (`liburb.pal`); without it the Statue
   renders as magenta noise (`scratch\ref\statue.png` against `landmarks.png`).
2. **Every snow mission draws summer art.** ALL02, ALL05, ALL10 and ALL12 have 379 placed civilian objects of 44 types
   that use the temperate files:
   - houses, barns, silos, chapels and cottages without snow roofs;
   - the Kremlin walls and palace;
   - flags standing on green grass plaques;
   - barrels and tents.

   This includes ALL02's objective chapel (CACOLO01) and ALL12's Kremlin palace (CARUS03). The urban missions are hit
   by the same bug, but `pairs.py` proves that all 91 urban/temperate file pairs are pixel-identical, so urban maps
   look right by accident.
3. **Map objects are dropped.**
   - Naval yards on water are never placed: ALL04 ×2, ALL07 (the player's own shipyard), ALL11 ×3.
   - Terror Drones, Dolphins and Giant Squid never exist. Placed objects are affected in ALL11 and ALL12, and TaskForce
     members in ALL06, ALL07, ALL08, ALL10, ALL11 and ALL12.
4. **104 "invisible" light posts are drawn as gold lamp sculptures** (`GALITE`), and none of them emits light. The night
   missions (ALL02, ALL03, ALL05 and ALL09, ambient 0.5-0.68) are a flat dark wash instead of lit streets.
5. **"Animations don't work"** is systemic. The animations that do work (`*_diff.png`):
   - building `ActiveAnim` 1-4: flags, power plants and radar;
   - damage fires;
   - the ore-mine animation;
   - walking and firing.

   The animations that never happen:
   - **infantry:** idle fidgets, prone/crawl, civilian panic, cheer, swim, and every death except Die1. In every
     standing shot, infantry change 0 px between the two frames;
   - **buildings:** IdleAnim, SpecialAnim and ProductionAnim (the construction-yard crane, the refinery dump, the Tesla
     and Prism charge, shipyard and depot animations), gates and roof doors;
   - **landmark destruction:** DestroyAnim, rubble and debris;
   - **map art:** the ALL07 waterfalls;
   - **mission-script effects:** `PlayAnimAt` arrows and chrono flashes, Apply100Damage and DoExplosionAt explosions,
     particle effects, camera pans, radar pings, flashes and cheers. These are recorded and never shown.

What already works and does not need fixing:
- loading screens, with the correct background, text, palette and map;
- the F1 briefing text;
- TextTrigger hints;
- the map `Theme=`;
- every `[Terrain]` object (50 types, all theater-correct, 0 missing);
- every `[Smudge]` and every overlay (`missing-art=0` in all 12 missions);
- all RA2-roster `GA*`/`NA*` buildings, which take the theater-aware path;
- all voxel vehicles, including civilian cars;
- all infantry SHPs, including the animals.

Movies (`Intro=`/`Brief=`/`Win=`) cannot play because `MOVIES01.MIX` is a 4-byte stub on this install. That is not
counted as a gap.

## 1. Per-mission table

Object counts: S = structures, V = units, I = infantry, A = aircraft, T = terrain, Sm = smudges, O = non-ore overlay
cells drawn. TF = TaskForce members that reinforcements and AI create. Screenshot paths are relative to
`D:\ra2-wt\w3-allied\scratch\shots\`.

| Mission | Objects placed | Missing / wrong art | Static but should animate | Other presentation gaps | Screenshots |
|---|---|---|---|---|---|
| **ALL01** `all01t` URBAN, "Lone Guardian", New York | S190 V19 I40 A0 T344 Sm218 O145; TF: DRED SAPC E1/E2 TANY CMIN GHOST COP ORCA ZEP V3 CIVBF* CARGOPLANE ENGINEER. All 190/59 placed | **Statue of Liberty CANEWY04 INVISIBLE**: an empty star plinth (RC-1b, plus `Palette=lib`). **18 invisible lamps drawn as gold GALITE posts** (NEGLAMP 9, INBLULMP 5, INYELWLAMP 3, INGALITE 1) (RC-2). Urban `CA*` drawn from `ct*` files: pixel-identical, no visible effect | GACNST ProductionAnim (crane); NACNST Idle and Production anims; GAREFN SpecialAnim; NAWEAP roof door; GAGATE_A gate; all infantry idle (RC-3, RC-7). The flags (CAUSFGL/CARUFGL), power plants and damage fires do animate | 41 PlayAnimAt ×2 (bridge-hut ARRW* arrows); **63 Apply100Damage ×10 with no explosion anim** (the Soviet bombardment of Manhattan); 55 RadarEvent ×40, 104 FlashTeam ×8, 48 MoveCamera ×2, 113 Cheer, 46/47 LockInput, 114/115 tab and cameo flash, all recorded only (RC-8). `Intro=A01_f00e` is a stub | `all01t\v_lm_CANEWY04_62_70_a.png` (empty plinth), `all01t\v_start_45_69_a.png`, `all01t\v_lm_CAUSFGL_80_47_diff.png` (what animates), `lamp\crop.png` (lamp drawn), `all01t\00_loading.png`, `all01t\02_briefing.png` |
| **ALL02** `all02s` SNOW, Colorado | S60 V25 I36 T399 Sm1 O155; TF: HTNK HTK E2 DOG ENGINEER ORCA SENGINEER JUMPJET E1 HARV TRUCKB SHK. All placed | **28 objects in snow-less temperate art**: CAHSE01-04 ×20 houses, **CACOLO01 (the objective chapel)**, CAGARD01, CAMISC, CAARMY01 (RC-1a). 6 lamps drawn (RC-2) | GACNST, NACNST, GAREFN and GAWEAP event slots; gate (RC-3); infantry (RC-7) | Night lighting (0.68, blue) with no light pools (RC-2). 55 ×4, 113 ×3 recorded. `Brief=A02_f00e` is a stub | `m_all02.png`, `all02s\v_start_107_108_a.png` (snow-less houses), `scratch\ref\snow_vs_temperate.png` |
| **ALL03** `all03u` URBAN night, "Hail to the Chief", Washington DC | S111 V21 I52 T501 Sm27 O353; TF: CTECH E2 CIV* PRES SHAD ENGINEER SHK HTK HTNK DOG SSRV FLAKT MTNK FV E1. All placed | **White House CAWASH01, Jefferson CAWASH14, Lincoln CAWASH15 and Washington Monument CAWSH12 INVISIBLE** (RC-1b). The White House is the objective the HINT warns about. 15 lamps drawn (RC-2) | NACNST, NAREFN and NAWEAP slots; infantry; landmark DestroyAnims `CAWS01DM` and others (RC-4) | **Darkest mission (ambient 0.5)**: a uniform dark wash, because none of the 15 lamps emits light (RC-2). 41 PlayAnimAt ×2 ("Arrows - Garrison"), 88 ParticleAnim ×1, 104 ×4 recorded (RC-8) | `m_all03a.png`, `all03u\v_lm_CAWASH01_78_14_a.png` (empty lot), `all03u\v_lm_CAWSH12_86_69_a.png` (empty red square), `all03u\v_lm_CAWASH14_77_125_a.png` |
| **ALL04** `all04u` URBAN, Chicago | S127 (125 placed) V44 I29 T460 Sm75 O186; TF: AMCV LCRF DEST MTNK E1/E2 SHK HTK HTNK ENGINEER V3 SUB JOSH SENGINEER JUMPJET COP CIVBFM | **NAYARD ×2 not placed** (water foundation rejected, RC-5). **CACHIG04 Associates Center and CACHIG06 Water Tower INVISIBLE**: an empty brick plaza (RC-1b). 8 lamps drawn. CIVBFM (map `Image=BeInvisibleHack`, invisible in the original as well) is dropped, so its tag is lost | NACNST, NAREFN and NAWEAP slots; infantry | 103 TimerText, 112 CenterCamera, 104 ×7, 55 ×5, 44 recorded. `Win=A04_f03e` is a stub | `m_all04.png`, `all04u\v_lm_CACHIG06_81_65_a.png`, `all04u\v_lm_CACHIG04_50_95_a.png` |
| **ALL05** `all05s` SNOW night, "Dark Night", Polish/German border | S171 V5 I91 T1709 Sm2 O844; TF: E2 DOG SPY SHAD TANY SHK E1 ENGINEER PROPA COW | **102 objects in temperate art**: barrels ×43, CAFARM01/02 ×19 (red barns and silos with no snow), CAARMY tents, **CARUFGL ×14 and CAGEFGL ×2 flags on green grass plaques**, CAMISC05/06, CAMSC11, CAEUR2 (RC-1a). 17 lamps drawn (RC-2) | **TESLA ×15 SpecialAnim** (coil charge `NATSLA_B`); NAWEAP ×7 roof door; NAREFN; 13 COW and the Tanya and Spy infantry idle (RC-7) | Night (0.68) with no light pools. 41 PlayAnimAt ×3 ("Anim: Power Plants" arrows), 88 ParticleAnim ×2, 55 ×34 recorded | `m_all05.png` (green flag bases, barns), `all05s\v_cl11_89_98_a.png` |
| **ALL06** `all06u` URBAN, Washington DC (Pentagon) | S165 V26 I37 T384 Sm187 O237; TF: HTNK E2 LCRF AMCV ENGINEER E1 SENGINEER DOG SHK FLAKT **DRON ×30** HTK V3 CIV1-3 | **White House, Jefferson, Lincoln and Washington Monument INVISIBLE again.** The White House lot shows 2 **black tile holes** where the building would cover them (RC-1b). The Pentagon CAWA2A-D draws. 20 lamps drawn. **30 Terror Drones in teams never spawn** (RC-6) | ATESLA ×8 and TESLA ×5 SpecialAnim (charge); NACNST and NAREFN slots; GAGATE_A gate; infantry | 101 Reshroud ×3 runs; 112, 113 and 48 ×2 recorded | `m_all06.png`, `all06u\v_lm_CAWASH01_98_17_a.png` (black holes), `all06u\v_lm_CAWSH12_117_62_a.png`, `all06u\v_start_107_165_a.png` (Pentagon OK) |
| **ALL07** `all07t` TEMPERATE, Hawaii | S128 (127 placed) V35 I47 T655 O192; TF: DRED HYD SUB SHAD E1/E2 SAPC HTNK SMCV SHK DOG FLAKT HTK **DRON ×6** V3 SENGINEER CMIN MTNK AEGIS CARRIER CIV* | **The player's GAYARD (186,131) is not placed** (RC-5). **6 TF Terror Drones missing** (RC-6). Temperate art is correct here | **20 waterfall tile cells** (`Waterfalls` WA01X-WA04X) do not animate (RC-9). GAYARD and GAREFN slots; no ship wakes (RC-10); beach civilians frozen (RC-7) | 18 RevealZone ×3 is unimplemented (wave 2). 101 ×10 runs; 113 ×4 and 48 ×3 recorded | `m_all07.png`, `all07t\v_lm_CAWT01_202_131_a.png` |
| **ALL08** `all08u` URBAN, St Louis | S138 V4 I66 T228 Sm61 O397; TF: E1 ENGINEER TANY LCRF SREF MTNK E2 DOG CTECH ADOG AMCV FV **DRON ×19** DESO FLAKT HTK HTNK SHK V3 SENGINEER CIV1-3 | **Gateway Arch CASTL04 INVISIBLE**: an empty park with paths (RC-1b). Busch Stadium CASTL05* draws. 9 lamps drawn. 19 TF Terror Drones missing | **GADEPT** Idle and Special anims; TESLA ×7 charge; GAGATE_A ×4 gates; NACNST, NAREFN and NAWEAP slots | 113 ×3, 48 ×4, 103 TimerText and 51 recorded or unimplemented | `m_all08.png`, `all08u\v_lm_CASTL04_105_79_a.png` (empty park) |
| **ALL09** `all09t` TEMPERATE dusk, Yucatan | S96 V17 I112 T248 O288; TF: DOG GHOST E2 SHK HTK ENGINEER HTNK SHAD | Nothing missing. The Mayan temples (MAYAN, CAMEX02-05) draw in the correct theater. 3 lamps drawn | 5 SEALs (player `Hack` team, they start on the beach): SEAL/Tanya Swim/Wet sequences are never used, so in water they show land frames (RC-7); TESLA ×4 and NADEPT slots; 2 alligators idle | Dusk (0.68) with no lights. 114 and 113 recorded | `m_all09.png`, `t_all09t\own_a.png` (SEALs) |
| **ALL10** `all10s` SNOW, "Mirage", Black Forest | S140 V10 I43 T424 O364; TF: MGTK E2 HTNK V3 ENGINEER CTECH E1 HTK SMCV DOG SHK **DRON ×21** DESO FLAKT APOC HARV POLARB CIVBF EUROC SENGINEER | **AMMOCRAT ×18 INVISIBLE** (`ammo01.shp` only in `sno.mix`, RC-1b). **85 objects in temperate art**: Euro cottages CAEUR1/2/04, CABARN02, CAFARM, barrels ×49, **CAGEFGL ×4 flags with grass bases**, CAPARS07, CAOUTP (RC-1a). Einstein's lab CALAB is fine (its snow and temperate art are identical). 21 TF drones missing | CAOUTP Special and Idle anims; ATESLA ×3; GACNST, GAWEAP and GAREFN slots; 2 gates | **42 DoExplosionAt ×3 is unimplemented** (scripted explosions). 112 CenterCamera ×6, 104 ×2, 101 ×15 | `m_all10.png` (snow-less cottages, green flag base) |
| **ALL11** `all11t` TEMPERATE, island assault | S145 (142 placed) V35 I49 T279 O324; TF: HARV E2 DOG SHK FLAKT HTNK **DRON ×23** DESO HTK V3 **SQD ×13** SUB HYD DRED SAPC SENGINEER APOC CRUISE **DLPH ×4** | **GAYARD ×1 and NAYARD ×2 not placed** (RC-5). **4 placed Dolphins and 4 Giant Squid dropped**, plus every TF drone, squid and dolphin (RC-6) | TESLA ×10; GAREFN ×3, NADEPT, GACNST and NACNST slots; no ship wakes | 35 ×3, 117 ×2 and 40 unimplemented; 112 ×2 recorded | `m_all11.png`, `all11t\v_start_105_105_a.png` (no shipyard) |
| **ALL12** `all12s` SNOW, "Chrono Storm", Moscow | S268 V41 I38 T410 O1072; TF: GHOST AMCV SREF MTNK FV E2 **DRON ×23** FLAKT DOG HTNK HTK V3 ENGINEER APOC TTNK TANY E1 ZEP CLEG SHK SENGINEER | **St Basil's CARUS01 INVISIBLE**: an empty square inside the Kremlin walls (RC-1b). **164 objects in temperate art**: Kremlin walls CARUS02A/B ×38, **Kremlin palace CARUS03 (the "Kremlin" objective; the snow variant is 47 px taller)**, Moscow blocks CARUS04-06, CARUFGL ×19 flags, CAWASH07/08, CAIND01, CAWT01, barrels and tires (RC-1a). **2 placed Terror Drones dropped**, 23 in TF (RC-6) | TESLA ×34 charge; CAOUTP; NAREFN, NAWEAP and NACNST slots; 1 gate | **41 PlayAnimAt ×8 is the chrono-arrival flash** (`CHRONOTG`, see RC-8) for "Chrono 1b-4b" and "Kremlin Secured 3b", never shown. 10 PlayMovie ×2 and 100 ×3 are stubs. `Win=A13_f01e` and `PostScore` are stubs | `m_all12.png`, `all12s\v_lm_CARUS01_96_95_a.png` (empty square), `all12s\v_start_62_61_a.png` |

Every mission's loading screen (`allNN\00_loading.png`) and F1 briefing (`02_briefing.png`) are correct.

## 2. Per-object-type table, all Allied maps

"Used in" lists the mission number and placed count (`01x9` means ALL01, 9 objects); `TF:` lists the missions whose
TaskForces contain the type. Status meanings:

- **OK**: draws with the right art.
- **WRONG-ART**: draws with the wrong file.
- **INVISIBLE**: a World object with no art, or not placed at all.
- **NOT PLACED**: the object never exists.
- **NO-ANIM**: the type has art-defined animation slots the clone never plays.

Structures in the 39-entry building roster resolve through `art_chain()` and are theater-correct. Every other
structure goes through `camp::Types::building`. Terrain, smudge and overlay types are grouped at the end of the table;
every one of them is OK.

| type id | kind | name | used in (map x count; TF = TaskForce) | status | root cause | fix sketch |
|---|---|---|---|---|---|---|
| ADOG | infantry | Allied Attack Dog | 01x4 TF:08 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| AEGIS | vehicle | Aegis Cruiser | 07x2 TF:07 | OK draw; no Wake/splash on water (03 §3.4) | RC-10 | wake anim while moving |
| ALL | infantry | Animal Alligator | 09x2,11x18 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| AMCV | vehicle | Allied Construction Vehicle | 10x1 TF:04,06,08,12 | OK | - | - |
| AMMOCRAT | structure | Ammo Crates | 10x18 | INVISIBLE (10) | RC-1b | search isogen.mix + tem/sno/urb.mix |
| AMRADR | structure | Allied American Airforce Command Headquarters | 07x1 | OK | - | - |
| APOC | vehicle | Apocalypse | 11x4,12x13 TF:10,11,12 | OK | - | - |
| ATESLA | structure | Allied Prism Cannon | 06x8,10x3,11x1 | OK; NO-ANIM: SpecialAnim=GAPRIS_A | RC-3 | draw Idle/Special/Production/Roof slots |
| BUS | vehicle | School Bus | 07x1,11x1 | OK | - | - |
| CAAIRP | structure | Tech Airport | 04x1,07x1,09x1,12x1 | WRONG-ART on snow (12) | RC-1a | theater-aware art chain |
| CAARMY01 | structure | Army Tent | 02x1,03x6,05x8,07x2,10x1 | WRONG-ART on snow (02,05,10) | RC-1a | theater-aware art chain |
| CAARMY02 | structure | Army Tent | 03x1,10x1 | WRONG-ART on snow (10) | RC-1a | theater-aware art chain |
| CAARMY03 | structure | Army Tent | 03x4,05x1,10x1 | WRONG-ART on snow (05,10) | RC-1a | theater-aware art chain |
| CAARMY04 | structure | Army Tent | 03x3,10x1 | WRONG-ART on snow (10) | RC-1a | theater-aware art chain |
| CABARN02 | structure | Farm Barn 02 | 10x1 | WRONG-ART on snow (10) | RC-1a | theater-aware art chain |
| CABHUT | structure | Bridge repair hut | 01x2,03x2,04x3,11x6 | OK | - | - |
| CABUNK01 | structure | Concrete Bunker 01 | 06x1,12x2 | WRONG-ART on snow (12) | RC-1a | theater-aware art chain |
| CABUNK02 | structure | Concrete Bunker 02 | 12x2 | WRONG-ART on snow (12) | RC-1a | theater-aware art chain |
| CACHIG01 | structure | Chigo Brick Building | 04x2,08x1,11x1 | OK | - | - |
| CACHIG02 | structure | Chigo Brick Building var2 | 04x3,08x1 | OK | - | - |
| CACHIG03 | structure | Chigo Office Building | 04x2,06x1,07x1,08x1,11x2 | OK | - | - |
| CACHIG04 | structure | Chicago Associates Center | 04x1 | INVISIBLE (04); no DestroyAnim CACHIG04D | RC-1b,RC-4 | search isogen.mix + tem/sno/urb.mix |
| CACHIG05 | structure | Chicago Sears Tower | 04x1 | OK | - | - |
| CACHIG06 | structure | Chicago Water Tower | 04x1 | INVISIBLE (04); no DestroyAnim CACH06DM | RC-1b,RC-4 | search isogen.mix + tem/sno/urb.mix |
| CACOLO01 | structure | Air Force Academy Chapel Colorado | 02x1 | WRONG-ART on snow (02) | RC-1a | theater-aware art chain |
| CAEUR04 | structure | Euro Building 04 | 10x2,12x3 | WRONG-ART on snow (10,12) | RC-1a | theater-aware art chain |
| CAEUR1 | structure | Euro Cottage A | 10x5 | WRONG-ART on snow (10) | RC-1a | theater-aware art chain |
| CAEUR2 | structure | Euro Cottage B | 05x2,07x8,10x5 | WRONG-ART on snow (05,10) | RC-1a | theater-aware art chain |
| CAEURO05 | structure | Statue | 07x1,12x1 | WRONG-ART on snow (12) | RC-1a | theater-aware art chain |
| CAFARM01 | structure | Farm | 05x7,10x1 | WRONG-ART on snow (05,10) | RC-1a | theater-aware art chain |
| CAFARM02 | structure | Farm Silo | 05x12,10x2,12x1 | WRONG-ART on snow (05,10,12) | RC-1a | theater-aware art chain |
| CAFARM06 | structure | Lighthouse | 07x1,11x1,12x1 | WRONG-ART on snow (12) | RC-1a | theater-aware art chain |
| CAFRMA | structure | Farmhouse | 07x2 | OK | - | - |
| CAFRMB | structure | Farm Outhouse | 05x1,06x4,10x2,11x1 | OK | - | - |
| CAGARD01 | structure | Guard Shack | 02x2,06x1,08x3,12x1 | WRONG-ART on snow (02,12) | RC-1a | theater-aware art chain |
| CAGAS01 | structure | Gas Station | 01x1,04x2,08x3,11x1 | OK | - | - |
| CAGEFGL | structure | Flag Germany Left | 05x2,10x4 | WRONG-ART on snow (05,10) | RC-1a | theater-aware art chain |
| CAHOSP | structure | Old Civilian Hospital | 03x1,04x1,06x1,07x1,08x1 | OK | - | - |
| CAHSE01 | structure | American House 01 | 02x4,06x2 | WRONG-ART on snow (02) | RC-1a | theater-aware art chain |
| CAHSE02 | structure | American House 02 | 02x6,06x3,07x1 | WRONG-ART on snow (02) | RC-1a | theater-aware art chain |
| CAHSE03 | structure | American House 03 | 02x4,06x1,07x1 | WRONG-ART on snow (02) | RC-1a | theater-aware art chain |
| CAHSE04 | structure | American House 04 | 02x6,06x2,07x1 | WRONG-ART on snow (02) | RC-1a | theater-aware art chain |
| CAIND01 | structure | Industrial building | 08x1,12x2 | WRONG-ART on snow (12) | RC-1a | theater-aware art chain |
| CALAB | structure | Einstein's Lab | 10x1 | OK | - | - |
| CAMEX02 | structure | Mayan Castillo | 09x1 | OK | - | - |
| CAMEX03 | structure | Mayan Minor Temple | 09x2 | OK | - | - |
| CAMEX04 | structure | Mayan Large Temple | 09x1 | OK | - | - |
| CAMEX05 | structure | Mayan Platfrom | 09x3 | OK | - | - |
| CAMIAM01 | structure | Miami Hotel 01 | 07x2,08x1 | OK | - | - |
| CAMIAM02 | structure | Miami Hotel 02 | 04x1,11x1 | OK | - | - |
| CAMIAM03 | structure | Miami Hotel 03 | 03x1,04x1,06x1,11x1 | OK | - | - |
| CAMIAM04 | structure | Lifeguard Hut | 07x6,11x4 | OK | - | - |
| CAMIAM05 | structure | Miami Hotel 05 | 07x1,11x1 | OK | - | - |
| CAMIAM06 | structure | Miami Hotel 06 | 11x1 | OK | - | - |
| CAMIAM07 | structure | Miami Hotel 07 | 07x2,11x2 | OK | - | - |
| CAMIAM08 | structure | Arizona Memorial | 07x1 | OK | - | - |
| CAMISC01 | structure | Barrels | 01x12,02x2,03x2,05x22,07x9,08x8,09x13,10x27,11x3,12x9 | WRONG-ART on snow (02,05,10,12) | RC-1a | theater-aware art chain |
| CAMISC02 | structure | Barrel | 01x10,02x1,03x2,05x21,07x9,08x7,09x18,10x22,11x4,12x9 | WRONG-ART on snow (02,05,10,12) | RC-1a | theater-aware art chain |
| CAMISC03 | structure | Dumpster | 01x3,02x1,03x1,04x1,06x1,07x1,10x3,11x4,12x4 | WRONG-ART on snow (02,10,12) | RC-1a | theater-aware art chain |
| CAMISC04 | structure | Mail Box | 01x2,03x6,04x2,06x3,07x2,08x3,11x3 | OK | - | - |
| CAMISC05 | structure | Construction Pipes | 01x9,05x2,07x1,10x3,12x13 | WRONG-ART on snow (05,10,12) | RC-1a | theater-aware art chain |
| CAMISC06 | structure | V3 Rack | 01x9,05x10,08x3,09x3,11x1 | WRONG-ART on snow (05) | RC-1a | theater-aware art chain |
| CAMSC01 | structure | Hot Dog Stand | 04x1,06x2 | OK | - | - |
| CAMSC02 | structure | Beach Umbrellas A | 06x1,07x1,11x3 | OK | - | - |
| CAMSC03 | structure | Beach Umbrellas B | 07x3,11x2 | OK | - | - |
| CAMSC04 | structure | Beach Towels A | 07x5,11x3 | OK | - | - |
| CAMSC05 | structure | Beach Towels B | 07x4,11x6 | OK | - | - |
| CAMSC06 | structure | Camp Fire | 02x3,05x1,07x1 | OK | - | - |
| CAMSC07 | structure | Thatched Hut Chief | 07x4,09x11 | OK | - | - |
| CAMSC08 | structure | Thatched Hut A | 07x1,09x6 | OK | - | - |
| CAMSC09 | structure | Thatched Hut B | 07x3,09x5 | OK | - | - |
| CAMSC10 | structure | McBurger Kong | 01x1,04x1,07x1,08x1 | OK | - | - |
| CAMSC11 | structure | Tires | 05x1,10x2,12x10 | WRONG-ART on snow (05,10,12) | RC-1a | theater-aware art chain |
| CANEWY01 | structure | Building | 06x1 | OK | - | - |
| CANEWY04 | structure | Statue of Liberty | 01x1 | INVISIBLE (01); no DestroyAnim CANY04DM | RC-1b,RC-4 | search isogen.mix + tem/sno/urb.mix |
| CANEWY06 | structure | Wall Street Office | 01x1,04x1 | OK | - | - |
| CANEWY07 | structure | Wall Street Office Var 2 | 01x1 | OK | - | - |
| CANEWY08 | structure | Wall Street Office Var 3 | 01x1,04x1,07x1,08x1 | OK | - | - |
| CANEWY10 | structure | NY Building 10 | 01x2,04x2,08x1,11x1 | OK | - | - |
| CANEWY11 | structure | NY Building 11 | 01x1,04x3,08x1 | OK | - | - |
| CANEWY12 | structure | NY Building 12 | 01x6,04x4,11x1 | OK | - | - |
| CANEWY13 | structure | NY Building 13 | 01x3,04x1,08x1 | OK | - | - |
| CANEWY14 | structure | NY Building 14 | 01x5 | OK | - | - |
| CANEWY15 | structure | NY Building 15 | 01x3,04x1,08x2,11x1 | OK | - | - |
| CANEWY16 | structure | NY Building 16 | 01x4,04x2,07x1,08x2,11x1 | OK | - | - |
| CANEWY17 | structure | NY Building 17 | 01x6,04x2 | OK | - | - |
| CANEWY18 | structure | NY Building 18 | 01x6,04x7 | OK | - | - |
| CANEWY20 | structure | Warehouse | 06x3,08x1,12x5 | WRONG-ART on snow (12) | RC-1a | theater-aware art chain |
| CANEWY21 | structure | Warehouse B | 01x1,06x4,08x2,12x4 | WRONG-ART on snow (12) | RC-1a | theater-aware art chain |
| CANWY09 | structure | NY Building 09 | 11x1 | OK | - | - |
| CANWY22 | structure | NY Building 22 | 01x2,04x3,07x1,08x2,11x1 | OK | - | - |
| CANWY23 | structure | NY Building 23 | 01x3,04x2,08x1,11x1 | OK | - | - |
| CANWY24 | structure | NY Building 24 | 01x3,06x1,08x1 | OK | - | - |
| CANWY25 | structure | NY Building 25 | 01x3,03x2,04x1,07x1 | OK | - | - |
| CANWY26 | structure | NY Building 26 | 01x3,03x2,04x1,07x1,08x1,11x1 | OK | - | - |
| CAOILD | structure | Tech Oil Derrick | 11x2,12x8 | OK | - | - |
| CAOUTP | structure | Tech Outpost | 10x1,12x1 | WRONG-ART on snow (10,12); NO-ANIM: SpecialAnim=CAOUTP_A/SpecialAnimTwo=CAOUTP_B/SpecialAnimThree=CAOUTP_C/IdleAnim=CAOUTP_D | RC-1a,RC-3 | theater-aware art chain; draw Idle/Special/Production/Roof slots |
| CAPARK01 | structure | Park Bench | 01x11,03x4,04x4,06x5,07x4 | OK | - | - |
| CAPARK02 | structure | Play Ground Swing Set | 04x1,06x2,12x4 | OK | - | - |
| CAPARK03 | structure | Play Ground Merry Go Round | 06x1 | OK | - | - |
| CAPARS02 | structure | Large Paris Building Var 1 | 04x1 | OK | - | - |
| CAPARS04 | structure | Paris Building 4 | 01x2 | OK | - | - |
| CAPARS05 | structure | Paris Building 5 | 04x1,08x1 | OK | - | - |
| CAPARS07 | structure | Euro Phone Booth | 10x3,12x3 | WRONG-ART on snow (10,12) | RC-1a | theater-aware art chain |
| CAPARS10 | structure | Paris Bistro Var 1 | 06x1,08x1 | OK | - | - |
| CAPARS13 | structure | Paris Bistro Var 2 | 07x1,11x1 | OK | - | - |
| CAPARS14 | structure | Paris Bistro Var 3 | 11x1 | OK | - | - |
| CAR | vehicle | Automobile | 02x5,04x2,07x3,11x2 | OK | - | - |
| CARGOPLANE | aircraft | Transport Plane | TF:01 | placed via W2-AN flight model (not shot-verified) | - | - |
| CARRIER | vehicle | Aircraft Carrier | 07x2 TF:07 | OK draw; no Wake/splash on water (03 §3.4) | RC-10 | wake anim while moving |
| CARUFGL | structure | Flag Russian Left | 01x4,05x14,06x3,12x19 | WRONG-ART on snow (05,12) | RC-1a | theater-aware art chain |
| CARUS01 | structure | Russian Basil's Cathedral | 12x1 | INVISIBLE (12); no DestroyAnim CARUS01D | RC-1b,RC-4 | search isogen.mix + tem/sno/urb.mix |
| CARUS02A | structure | Russain Kremlin Wall LTower | 12x15 | WRONG-ART on snow (12) | RC-1a | theater-aware art chain |
| CARUS02B | structure | Russain Kremlin Wall STower | 12x23 | WRONG-ART on snow (12) | RC-1a | theater-aware art chain |
| CARUS03 | structure | Russain Kremlin Palace | 12x1 | WRONG-ART on snow (12) | RC-1a | theater-aware art chain |
| CARUS04 | structure | Moscow City A | 12x4 | WRONG-ART on snow (12) | RC-1a | theater-aware art chain |
| CARUS05 | structure | Moscow City B | 08x1,12x6 | WRONG-ART on snow (12) | RC-1a | theater-aware art chain |
| CARUS06 | structure | Moscow City C | 12x5 | WRONG-ART on snow (12) | RC-1a | theater-aware art chain |
| CARUS08 | structure | Russain Gum Corner | 04x6 | OK | - | - |
| CASTL01 | structure | St Louis Building A | 08x3 | OK | - | - |
| CASTL02 | structure | St Louis Building B | 08x3 | OK | - | - |
| CASTL03 | structure | St Louis Building C | 08x1 | OK | - | - |
| CASTL04 | structure | St Louis Arch | 08x1 | INVISIBLE (08); no DestroyAnim CASTL04DM | RC-1b,RC-4 | search isogen.mix + tem/sno/urb.mix |
| CASTL05A | structure | Busch Stadium A | 08x1 | OK | - | - |
| CASTL05D | structure | Busch Stadium D | 08x1 | OK | - | - |
| CASTL05F | structure | Busch Stadium F | 08x1 | OK | - | - |
| CASTL05G | structure | Busch Stadium G | 08x1 | OK | - | - |
| CASTL05H | structure | Busch Stadium H | 08x1 | OK | - | - |
| CASTRT05 | structure | Bus Stop | 03x2,07x2 | OK | - | - |
| CATEXS03 | structure | San Antonio Office building | 04x1,08x1 | OK | - | - |
| CATEXS04 | structure | San Antonio Office building Var 2 | 04x1,08x1 | OK | - | - |
| CATEXS05 | structure | San Antonio Office building Var 3 | 04x1,08x1 | OK | - | - |
| CATEXS06 | structure | Texas Office Building | 01x1,06x2,08x2 | OK | - | - |
| CATEXS07 | structure | Texas Office Building var2 | 01x1,08x1 | OK | - | - |
| CATEXS08 | structure | Texas Office Building var3 | 01x1,03x3,04x4,06x1,07x1 | OK | - | - |
| CATS01 | structure | Farm Twin Silos | 01x2,12x2 | WRONG-ART on snow (12) | RC-1a | theater-aware art chain |
| CAUSFGL | structure | Flag US Left | 01x9,03x1,04x1,06x1,07x2 | OK on urban/temperate (cloth anim CAUSFGL_A waves); body = ct* (pixel-identical to cu*); see RE-W3-1 | - | - |
| CAWA2A | structure | Wash Pent A | 06x1 | OK | - | - |
| CAWA2B | structure | Wash Pent B | 06x1 | OK | - | - |
| CAWA2C | structure | Wash Pent C | 06x1 | OK | - | - |
| CAWA2D | structure | Wash Pent D | 06x1 | OK | - | - |
| CAWASH01 | structure | White House | 03x1,06x1 | INVISIBLE (03,06); no DestroyAnim CAWS01DM | RC-1b,RC-4 | search isogen.mix + tem/sno/urb.mix |
| CAWASH03 | structure | Wash Building 3 | 03x3,06x1,08x1 | OK | - | - |
| CAWASH04 | structure | Wash Building 4 | 03x1,06x1,08x1 | OK | - | - |
| CAWASH05 | structure | Wash Building 5 | 03x3,06x1,07x2,08x1 | OK | - | - |
| CAWASH07 | structure | Wash Building 7 | 01x4,03x1,04x1,06x3,08x1,12x6 | WRONG-ART on snow (12) | RC-1a | theater-aware art chain |
| CAWASH08 | structure | Wash Building 8 | 06x1,08x1,12x8 | WRONG-ART on snow (12) | RC-1a | theater-aware art chain |
| CAWASH09 | structure | Wash Building 9 | 06x1,07x1 | OK | - | - |
| CAWASH10 | structure | Wash Building 10 | 03x4,06x2,08x1 | OK | - | - |
| CAWASH11 | structure | Wash Building 11 | 03x2,06x1,08x1 | OK | - | - |
| CAWASH13 | structure | Wash Building 13 | 07x1 | OK | - | - |
| CAWASH14 | structure | Jefferson Memorial | 03x1,06x1 | INVISIBLE (03,06); no DestroyAnim CAWA14DM | RC-1b,RC-4 | search isogen.mix + tem/sno/urb.mix |
| CAWASH15 | structure | Wash Lincoln Memorial | 03x1,06x1 | INVISIBLE (03,06); no DestroyAnim CAWA15DM | RC-1b,RC-4 | search isogen.mix + tem/sno/urb.mix |
| CAWASH16 | structure | Smithsonian Castle | 03x1,06x1 | OK | - | - |
| CAWASH17 | structure | Smithsonian Natural History Museum | 03x1 | OK | - | - |
| CAWASH18 | structure | White House Fountain | 01x1,03x2,04x2,07x1,11x1 | OK | - | - |
| CAWSH12 | structure | Washington Monument | 03x1,06x1 | INVISIBLE (03,06); no DestroyAnim CAWSH12D | RC-1b,RC-4 | search isogen.mix + tem/sno/urb.mix |
| CAWT01 | structure | Water Tower | 07x1,12x3 | WRONG-ART on snow (12) | RC-1a | theater-aware art chain |
| CIV1 | infantry | Civilian | 03x1,06x5,07x4,08x9,09x5 TF:06,07,08 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| CIV2 | infantry | Civilian | 03x1,06x5,07x5,08x9,09x5 TF:06,07,08 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| CIV3 | infantry | Civilian | 03x1,06x4,07x3,08x7,09x5 TF:06,07,08 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| CIVA | infantry | Civilian Texan A | TF:03 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| CIVBF | infantry | Civilian Beach Female | 07x8,11x1 TF:01,07,10 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| CIVBFM | infantry | Civilian Beach Fat Male | 07x3,11x1 TF:01,04,07 | OK draw elsewhere; ALL04 map overrides Image=BeInvisibleHack -> original draws nothing, clone drops the unit (tag lost, not visual); RC-7 | RC-7 | full infantry sequence table |
| CIVBTM | infantry | Civilian Beach Thin Male | 07x5 TF:01,07 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| CIVC | infantry | Civilian Texan C | TF:03 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| CIVSFM | infantry | Civilian Snow Fat Male | 07x2 TF:03,07 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| CIVSTM | infantry | Civilian Snow Thin Male | TF:03 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| CLEG | infantry | Chrono Legionnaire | TF:12 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| CMIN | vehicle | Chrono Miner | 02x1,07x2,11x3 TF:01,07 | OK draw; no UnloadingClass dump (03 §3.4) | RC-10 | swap to HORV/CMON while docked |
| CONA | vehicle | Construction Excavator | 01x1,10x1,12x3 | OK | - | - |
| COP | vehicle | Police Car | 01x1,07x1,11x1 TF:01,04 | OK | - | - |
| COW | infantry | Animal Cow | 05x13 TF:05 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| CRUISE | vehicle | Cruise Ship | 11x1 TF:11 | OK draw; no Wake/splash on water (03 §3.4) | RC-10 | wake anim while moving |
| CTECH | infantry | Technician | 08x17,10x6 TF:03,08,10 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| DESO | infantry | Desolater | 08x6 TF:08,10,11 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| DEST | vehicle | Destroyer | 04x6,07x3,11x2 TF:04 | OK draw; no Wake/splash on water (03 §3.4) | RC-10 | wake anim while moving |
| DLPH | vehicle | Dolphin | 11x4 TF:11 | NOT PLACED (SHP vehicle) | RC-6 | SHP-vehicle path in camp::Types::unit |
| DOG | infantry | Attack Dog | 02x2,03x3,05x5,06x2,08x6,09x14,12x8 TF:02,03,05,06,07,08,09,10,11,12 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| DRED | vehicle | Dreadnought | 01x4 TF:01,07,11 | OK draw; no Wake/splash on water (03 §3.4) | RC-10 | wake anim while moving |
| DRON | vehicle | Terror Drone | 12x2 TF:06,07,08,10,11,12 | NOT PLACED (SHP vehicle) | RC-6 | SHP-vehicle path in camp::Types::unit |
| E1 | infantry | GI | 01x13,02x2,03x31,05x10,07x14,08x2,09x10,10x24,11x12,12x6 TF:01,02,03,04,05,06,07,08,10,12 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| E2 | infantry | Conscript | 01x22,02x26,03x9,04x17,05x57,06x12,07x3,08x9,09x52,10x3,12x16 TF:01,02,03,04,05,06,07,08,09,10,11,12 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| ENGINEER | infantry | Engineer | 02x3,03x2,05x2,09x6,10x8,12x2 TF:01,02,03,04,05,06,08,09,10,12 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| EUROC | vehicle | European Car A | 02x1,04x1,07x1,10x1,11x1 TF:10 | OK | - | - |
| FLAKT | infantry | Flak Trooper | 06x5,11x3 TF:03,06,07,08,10,11,12 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| FV | vehicle | IFV | 03x3,07x2 TF:03,08,12 | OK | - | - |
| GAAIRC | structure | Allied Airforce Command Headquarters | 02x1,10x1,11x1 | OK | - | - |
| GACNST | structure | Allied Construction Yard | 01x1,02x1,03x1,07x1,10x1,11x1 | OK; NO-ANIM: ProductionAnim=GACNST_B | RC-3 | draw Idle/Special/Production/Roof slots |
| GACSPH | structure | Allied Chrono Sphere | 10x1 | OK | - | - |
| GADEPT | structure | Allied Service Depot | 08x1 | OK; NO-ANIM: SpecialAnim=GADEPT_A/SpecialAnimTwo=GADEPT_B/SpecialAnimThree=GADEPT_C/IdleAnim=GADEPT_D | RC-3 | draw Idle/Special/Production/Roof slots |
| GAGATE_A | structure | Guard Border Crossing | 01x1,02x1,06x1,08x4,10x2,12x1 | OK; NO-ANIM (gate never opens) | RC-3 | gate open/close frames |
| GAPILE | structure | Allied Barracks | 02x1,07x2,10x4,11x1 | OK | - | - |
| GAPILL | structure | Allied Pill Box | 01x6,06x6,07x7,10x12,11x4 | OK | - | - |
| GAPOWR | structure | Allied Power Plant | 01x7,02x1,06x6,07x6,09x2,10x6,11x4 | OK | - | - |
| GAREFN | structure | Allied Ore Refinery | 01x1,02x1,07x1,08x1,10x1,11x3 | OK; NO-ANIM: SpecialAnim=GAREFNOR | RC-3 | draw Idle/Special/Production/Roof slots |
| GAWEAP | structure | Allied War Factory | 02x1,07x1,08x1,10x1 | OK; NO-ANIM: RoofDeployingAnim=GAWEAP_3/UnderRoofDoorAnim=GAWEAP_4 | RC-3 | draw Idle/Special/Production/Roof slots |
| GAYARD | structure | Allied Shipyard | 07x1,11x1 | INVISIBLE (not placed); NO-ANIM: IdleAnim=GAYARD_A/ProductionAnim=GAYARD_D/SpecialAnim=GAYARD_S1/SpecialAnimTwo=GAYARD_S2/SpecialAnimThree=GAYARD_S3 | RC-3,RC-5 | water-aware map placement; draw Idle/Special/Production/Roof slots |
| GHOST | infantry | SEAL | TF:01,09,12 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants; no Swim/Wet seq in water | RC-7 | full infantry sequence table |
| GTGCAN | structure | Allied Grand Cannon | 04x2 | OK | - | - |
| HARV | vehicle | War Miner | 02x1,03x3,04x1,05x2,06x8,08x2,09x3,12x8 TF:02,10,11 | OK draw; no UnloadingClass dump (03 §3.4) | RC-10 | swap to HORV/CMON while docked |
| HORV | vehicle | War Miner | 05x2 | OK | - | - |
| HTK | vehicle | Flak Track | 02x2,03x5,04x8,06x7,09x6 TF:02,03,04,06,07,08,09,10,11,12 | OK | - | - |
| HTNK | vehicle | Rhino Heavy Tank | 02x3,03x10,04x4,06x9,08x1,09x6,12x5 TF:02,03,04,06,07,08,09,10,11,12 | OK | - | - |
| HYD | vehicle | Sea Scorpion | TF:07,11 | OK draw; no Wake/splash on water (03 §3.4) | RC-10 | wake anim while moving |
| INBLULMP | structure | Invisible Blue Light Post | 01x5,03x2,05x4 | WRONG-ART (drawn, should be invisible) | RC-2 | skip draw for InvisibleInGame; add LightSource |
| INGALITE | structure | Invisible Light Post | 01x1,02x5,06x8 | WRONG-ART (drawn, should be invisible) | RC-2 | skip draw for InvisibleInGame; add LightSource |
| INPURPLAMP | structure | Invisible Purple Light Post | 05x3,06x1,08x2 | WRONG-ART (drawn, should be invisible) | RC-2 | skip draw for InvisibleInGame; add LightSource |
| INREDLMP | structure | Invisible Red Light Post | 03x1,08x1 | WRONG-ART (drawn, should be invisible) | RC-2 | skip draw for InvisibleInGame; add LightSource |
| INYELWLAMP | structure | Invisible Yellow Light Post | 01x3,02x1,03x10,04x2,06x8,08x5,09x3 | WRONG-ART (drawn, should be invisible) | RC-2 | skip draw for InvisibleInGame; add LightSource |
| JOSH | infantry | Animal Monkey | TF:04 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| JUMPJET | infantry | Rocketeer | 02x2,10x2 TF:02,04 | OK draw; Rocketeer Fly/Hover seqs not used + RC-7 | RC-7 | full infantry sequence table |
| LCRF | vehicle | Landing Craft | TF:04,06,08 | OK draw; no Wake/splash on water (03 §3.4) | RC-10 | wake anim while moving |
| LIMO | vehicle | Limo | 01x1 | OK | - | - |
| MAYAN | structure | Mayan Prism Pyramid | 09x2 | OK | - | - |
| MGTK | vehicle | Mirage Tank | TF:10 | OK | - | - |
| MTNK | vehicle | Grizzly Battle Tank | 01x3,05x1,07x5,09x2,10x4 TF:03,04,07,08,12 | OK | - | - |
| NACNST | structure | Soviet Construction Yard | 01x1,02x1,03x1,04x3,06x2,08x1,09x1,11x1,12x2 | OK; NO-ANIM: IdleAnim=NACNST_C/ProductionAnim=NACNST_B | RC-3 | draw Idle/Special/Production/Roof slots |
| NADEPT | structure | Soviet Service Depot | 09x1,11x1 | OK; NO-ANIM: IdleAnim=NADEPT_B/SpecialAnim=NADEPT_C1/SpecialAnimTwo=NADEPT_C2/SpecialAnimThree=NADEPT_C3 | RC-3 | draw Idle/Special/Production/Roof slots |
| NAFLAK | structure | Soviet Flak Cannon | 02x4,03x3,04x4,06x5,08x6,09x4,11x14,12x16 | OK | - | - |
| NAHAND | structure | Soviet Barracks | 01x2,02x1,03x2,04x2,05x7,06x2,08x2,09x1,11x1,12x4 | OK | - | - |
| NAIRON | structure | Soviet Iron Curtain Device | 12x1 | OK | - | - |
| NALASR | structure | Soviet Sentry Gun | 01x2,02x6,03x7,04x11,05x2,06x15,07x1,08x7,09x3,11x8,12x9 | OK | - | - |
| NAMISL | structure | Soviet Nuclear Missile Silo | 05x2,11x3,12x1 | OK | - | - |
| NANRCT | structure | Soviet Nuclear Reactor | 05x3,11x2,12x6 | OK | - | - |
| NAPOWR | structure | Soviet Tesla Reactor | 01x3,02x4,03x10,04x10,05x9,06x16,08x14,09x6,11x7,12x5 | OK | - | - |
| NAPSYA | structure | Psychic Amplifier | 04x1 | OK | - | - |
| NAPSYB | structure | Psychic Beacon | 03x1,08x1 | OK | - | - |
| NARADR | structure | Soviet Radar Tower | 01x1,03x1,04x1,06x2,08x1,09x1,11x1,12x2 | OK | - | - |
| NAREFN | structure | Soviet Ore Refinery | 03x3,04x3,05x3,06x4,08x2,09x2,11x2,12x4 | OK; NO-ANIM: SpecialAnim=NAREFNOR | RC-3 | draw Idle/Special/Production/Roof slots |
| NASAM | structure | Allied Patriot Missile | 01x5,06x2,07x9,10x2,11x3 | OK | - | - |
| NATECH | structure | Soviet Battle Lab | 05x2,08x1,09x1,11x1,12x2 | OK | - | - |
| NAWEAP | structure | Soviet War Factory | 01x2,03x2,04x1,05x7,06x2,08x1,09x1,11x1,12x4 | OK; NO-ANIM: RoofDeployingAnim=NAWEAP_4/UnderRoofDoorAnim=NAWEAP_3 | RC-3 | draw Idle/Special/Production/Roof slots |
| NAYARD | structure | Soviet Shipyard | 04x2,11x2 | INVISIBLE (not placed); NO-ANIM: IdleAnim=NAYARD_A/ProductionAnim=NAYARD_D/SpecialAnim=NAYARD_S1/SpecialAnimTwo=NAYARD_S2/SpecialAnimThree=NAYARD_S3 | RC-3,RC-5 | water-aware map placement; draw Idle/Special/Production/Roof slots |
| NEGLAMP | structure | Negative Light Post | 01x9,04x6,05x10,06x3,08x1,11x8 | WRONG-ART (drawn, should be invisible) | RC-2 | skip draw for InvisibleInGame; add LightSource |
| ORCA | aircraft | Intruder | TF:01,02 | placed via W2-AN flight model (not shot-verified) | - | - |
| PICK | vehicle | Pickup Truck | 02x3,07x3,12x1 | OK | - | - |
| POLARB | infantry | Animal Polar Bear | TF:10 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| PRES | infantry | President | TF:03 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| PROPA | vehicle | Propaganda Truck | TF:05 | OK | - | - |
| PTRUCK | vehicle | Pickup Truck | 01x1,02x3,04x2,07x1,11x1,12x4 | OK | - | - |
| SAPC | vehicle | Armored Transport | TF:01,07,11 | OK | - | - |
| SENGINEER | infantry | Soviet Engineer | TF:02,04,06,07,08,10,11,12 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| SHAD | vehicle | BlackHawk Transport | 07x4 TF:03,05,07,09 | OK draw; rotor HVA frames static (03 §3.4) | RC-10 | cycle HVA frames |
| SHK | infantry | Shock Trooper | 03x4,04x12,05x3,06x4,08x1,09x13,11x14,12x6 TF:02,03,04,05,06,07,08,09,10,11,12 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| SMCV | vehicle | Soviet Construction Vehicle | TF:07,10 | OK | - | - |
| SNONITLAMP | structure | InvisibleSnowNightLamp | 03x2 | WRONG-ART (drawn, should be invisible) | RC-2 | skip draw for InvisibleInGame; add LightSource |
| SPY | infantry | Spy | TF:05 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| SQD | vehicle | Giant Squid | 11x4 TF:11 | NOT PLACED (SHP vehicle) | RC-6 | SHP-vehicle path in camp::Types::unit |
| SREF | vehicle | Prism Tank | TF:08,12 | OK | - | - |
| SSRV | infantry | Secret Service | TF:03 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants | RC-7 | full infantry sequence table |
| STANG | vehicle | Sports Car | 01x2,02x3,04x2,07x3,11x2 | OK | - | - |
| SUB | vehicle | Typhoon Attack Sub | 04x9,11x4 TF:04,07,11 | OK draw; no Wake/splash on water (03 §3.4) | RC-10 | wake anim while moving |
| SUVB | vehicle | SUV Black | 01x2,02x2,04x2,07x1,11x2,12x1 | OK | - | - |
| SUVW | vehicle | SUV White | 01x1,02x1,04x2,07x1,11x1 | OK | - | - |
| TANY | infantry | Tanya | 01x1,02x1,05x1 TF:01,05,08,12 | OK draw; NO-ANIM idle/prone/panic/cheer/death variants; no Swim/Wet seq in water | RC-7 | full infantry sequence table |
| TAXI | vehicle | Taxi | 11x2 | OK | - | - |
| TESLA | structure | Soviet Tesla Coil | 05x15,06x5,08x7,09x4,11x10,12x34 | OK; NO-ANIM: SpecialAnim=NATSLA_B | RC-3 | draw Idle/Special/Production/Roof slots |
| TNKD | vehicle | Tank Destroyer | 10x3 | OK | - | - |
| TRUCKB | vehicle | Truck (loaded) | TF:02 | OK | - | - |
| TTNK | vehicle | Tesla Tank | 12x4 TF:12 | OK | - | - |
| V3 | vehicle | V3 Launcher | 01x3,04x5,06x2,08x1 TF:01,04,06,07,08,10,11,12 | OK | - | - |
| ZEP | vehicle | Kirov Airship | TF:01,12 | OK | - | - |
| *[Terrain] (50 types)* | terrain | trees TREE01-19, TRFF*, POLE01/02, LT_GEN*/LT_SGN* signs, rocks, TIBTRE01-03 ore mines | every map | **OK**: every type resolves to its `.tem`/`.sno`/`.urb` file (`missing-art=0`); shadows drawn; TIBTRE01-03 animate | - | Trees cannot be damaged, burn or fall (static PresMap dressing). This is low priority (RC-11). |
| *[Smudge] (24 types)* | smudge | CRATER01-09, BURNT01-05 and others | 01, 02, 03, 04, 05, 06, 08 | **OK** | - | - |
| *Overlays (37 types)* | overlay | GASAND/GAWALL/NAWALL walls, CAKRMW Kremlin wall, CAFNC* fences, SROCK/TROCK rocks, BRIDGE1/2, LOBRDG/LOBRDB low bridges | every map | **OK** (`missing-art=0`); wall connection frames, live bridge state | - | - |

## 3. Root causes, grouped for the cross-cutting fixer

| RC | Cause | Code | Visible effect in the Allied missions | Fix sketch | Effort |
|---|---|---|---|---|---|
| **RC-1a** | The campaign structure resolver is **hard-wired to TEMPERATE**: `camp::Types::building` calls `theater_candidates(art_id, 'T', "isotemp.mix", "temperat.mix", ".shp")` | campaign_runtime.inc:217 | 379 objects of 44 types in ALL02/05/10/12 draw temperate art (snow-less roofs, green bases): see `scratch\ref\snow_vs_temperate.png` and the m_all05/m_all10/m_all12 contact sheets. **Correction to 03 §1.2 B**: on URBAN maps the same bug is pixel-invisible. All 91 urban/temperate pairs (`cuXXX`/`ctXXX`) diff to 0 px (`census\pairs_out.txt`), so URBAN is not a visual gap | Use `art_chain()` (as `depth_register_building` already does) and one shared resolver with `PresMap::art_shp` | S |
| **RC-1b** | **Missing MIX scope**: all three building resolvers (Types::building, depth_register_building, PresMap) search only `{iso, theater, generic}.mix`. The original also loads `isogen.mix` and `<ext>.mix` (`sno/tem/urb.mix`), both confirmed in 03 §1.2 A | src/theater_name.cpp:23-27, campaign_runtime.inc:217 | 9 landmark types (Statue of Liberty, White House, Jefferson, Lincoln, Washington Monument, Chicago ×2, Gateway Arch, St Basil's) and AMMOCRAT ×18 are invisible. **Adding isogen is not enough on its own**: CANEWY04 has art `Palette=lib`, which RA2 resolves to `lib<ext>.pal` (`liburb.pal`, `cache.mix`). Under the unit palette the Statue renders as magenta noise (`scratch\ref\statue.png` against `landmarks.png`). The engine has no `Palette=` support (grep: 0 hits) | Add `isogen.mix` and `<ext>.mix` to the search. Read art `Palette=` and load `<name><ext>.pal`. Draw the DestroyAnim ruins from isogen too (RC-4) | S |
| RC-1c | The name munge ignores `NewTheater` | theater_name.cpp:28 | Flags and the guard shack (`NewTheater=no`) draw the `ct*` body. On snow that is a grass plaque under a snow-base flag (ALL05/10/12). For urban/temperate see RE-W3-1 | Pass the art section; munge only when `NewTheater=yes` | S |
| **RC-2** | **`InvisibleInGame` is ignored, and light sources are not modelled** | grep `InvisibleInGame` / `LightIntensity`: 0 hits; pres_light.inc:14 | 104 lamp objects (NEGLAMP, INYELWLAMP, INGALITE, INBLULMP, INPURPLAMP, INREDLMP, SNONITLAMP) draw as gold `gglite` sculptures (`lamp\crop.png`). The night maps ALL02/03/05/09 lose their street-light pools. **Correction to 03 §3.2 "Light posts"**: the lamps are *not* invisible in the clone; they are drawn. NEGLAMP is a negative (darkening) light | Skip the body draw when `InvisibleInGame=yes` (keep the World building so triggers and targeting still work). Add a light-source pass that adds `LightIntensity` and `Light*Tint` over `LightVisibility` leptons to the cell lighting (`FUN_00484090` neighbourhood, W2-GA-05 open). Also TIBTRE glow | S + M |
| **RC-3** | **Only ActiveAnim 1-4, the ground door, SuperAnim, Bib and damage fires are drawn** | pres_fx.inc:271-296, 617 | Not drawn: ProductionAnim (GACNST/NACNST crane, shipyard `_D`), IdleAnim (NACNST_C, GAYARD_A/NAYARD_A, GADEPT_D, NADEPT_B, CAOUTP_D), SpecialAnim 1-3 (TESLA `NATSLA_B` in 9 missions with 75 coils, ATESLA `GAPRIS_A`, GAREFN/NAREFN ore dump, shipyards, depots, CAOUTP), RoofDeployingAnim/UnderRoofDoorAnim (GAWEAP/NAWEAP), and the `Gate=yes` open/close (GAGATE_A ×10 across 6 missions; its SHP has 42 frames and no gate logic exists). Same as 03 §3.2 | Read all 21 slots (presentation.md §6). Drive them from World events (factory producing, harvester docking, weapon charge, gate approach) | M |
| **RC-4** | **No destruction visuals** | grep `DestroyAnim`, `LeaveRubble`, `Debris`, `DamageParticleSystems`: 0 hits | Landmarks have `DestroyAnim` (CANY04DM, CAWS01DM, CAWA14DM, CAWA15DM, CAWSH12D, CACHIG04D, CACH06DM, CASTL04DM, CARUS01D). 99 placed structure types have `LeaveRubble=yes`. When destroyed they explode and vanish instead of leaving ruins. No damage smoke or sparks (`DamageParticleSystems` on 23 placed types) | Spawn the DestroyAnim and rubble frame; add a debris anim (Bouncer); particle smoke (03 §3.7) | M-L |
| **RC-5** | **Map structures pass the land `placement_check`**: water cells fail `passable`, so Unlimbo fails and the object is deleted | campaign_runtime.inc:924-927, building.cpp:34-35 | NAYARD ALL04 ×2 and ALL11 ×2, GAYARD ALL07 ×1 (the player's own yard) and ALL11 ×1 are never created | For map objects use the water grid for `WaterBound`/naval-yard types (the skirmish yard placement already knows water). Or bypass legality for scenario objects except occupancy | S |
| **RC-6** | **Campaign `Types::unit` sends every VehicleType to the voxel loader**; `Voxel=no` SHP units throw | campaign_runtime.inc:164-174, main.cpp:1321 | DRON (2 placed in ALL12, ~122 TaskForce slots in ALL06/07/08/10/11/12), SQD (4 placed and 13 TF in ALL11), DLPH (4 placed and 4 TF in ALL11) never exist. Teams spawn short. Same as 03 §1.2 C | Reuse the skirmish W2-GA-11/W2-AN-15 SHP-vehicle registration | S |
| **RC-7** | **Infantry play 4 of the sequences**: Ready/Walk/FireUp/Die1 | main.cpp:1148, world.cpp:4487-4667 | In every Allied mission infantry stand frozen: 0 px change in 6 frames (`t_all01t`, `t_all08u`, `t_all09t` own shots). No Idle1/2, Guard, Prone/Down/Crawl/Up under fire, civilian **Panic** (CIV1-3, CIVA/C, CIVB*, CIVS* in 01/03/06/07/08/09 would flee in Walk frames), **Cheer** (trigger 113 in all 12 missions), Paradrop (CARGOPLANE drop in ALL01), **Swim/Wet*** (TANY, GHOST/SEAL), Rocketeer Fly, Die2-5 per `InfDeath`, or animal-specific idles (COW ×13, ALL ×20, POLARB, JOSH). Same as 03 §3.3 | Full `Sequence=` table plus a DoType state machine | M |
| **RC-8** | **Trigger presentation actions are recorded only** (`presentation_action()` → "(recorded)") | map_trigger.cpp:118-127, 960-963 | **41 PlayAnimAt** ×15: ALL01 ×2, ALL03 ×2, ALL05 ×3, ALL12 ×8. **Index-space trap**: the value indexes the RA2 `rules.ini [Animations]` list *after duplicate names collapse*. That reading gives idx 479 = `ARRWNE` (the "Bridge Hut Anim" arrow) and idx 253 = `CHRONOTG` (ALL12 "Chrono 1b-4b"). Raw rulesmd indexing gives the nonsense `ARRWDEST`/`XGRYMED2` (RE-W3-2). Also: **63 Apply100Damage** ×10 (ALL01) applies damage with no explosion anim (`FUN_006dfa00` fires 5 `C4Warhead` explosions). **42 DoExplosionAt** ×3 (ALL10) and **88 ParticleAnim** ×3 (ALL03/05) are unimplemented. 48/112 camera moves (ALL01/04/06/07/08/10/11), 55 radar ping (all 12), 104/131 flash team/buildings, 113 Cheer, 103 TimerText (ALL04/08), 114/115 tab/cameo flash and 46/47 LockInput are all recorded only | Execute 41 through `PresFx::spawn` at the waypoint with the RA2 index table. 63/42: spawn the warhead's explosion anim. 48/112: pan the camera. 55: radar ping. 104/131: owner-colour flash. 113: Cheer sequence (RC-7) | M |
| **RC-9** | **Theater tile animations are not drawn** (`Tile##Anim` sections of the theater INI) | grep `Tile01Anim`: 0 hits | ALL07 has 20 `Waterfalls` cells (WA01X-WA04X) frozen. No Allied map uses the tunnel tops. Same as 03 §3.5 | Draw the anim at the tile's X/Y offset per `AttachesTo` | S |
| RC-10 | Vehicle/ship sub-anims (cross-cutting, 03 §3.4) | - | No wakes behind destroyers, subs, Aegis, carriers and dreadnoughts (ALL01/04/07/11); BlackHawk SHAD rotor HVA frozen (ALL07); no harvester dump swap | Per 03 | S-M |
| RC-11 | Map `[Terrain]` objects are inert dressing | pres_map.inc:49-74 | Trees cannot be shot, burn or fall | TerrainClass health and burn anims | M |

Not counted as gaps:
- movies, because the MOVIES MIX files are stubs on this install;
- EVA "unavailable" lines, a harness artifact (§ preamble);
- TextTrigger floods. ALL05 shows 15 "Tesla Coil detected" lines at once only because `w3_reveal` fired every
  DiscoveredByPlayer trigger together. Separately, the message list has no line cap; that is for the UI owner.

Also observed but out of scope here: building sprites that cross into unexplored black cells are drawn over the black
(`all01t\01_initial.png`, the building at left). The shroud pass does not paint fully-unexplored cells over objects.

## 4. Prioritized fix list (Allied visual impact per effort)

1. **RC-1b + `Palette=` (S).** Add isogen and `<ext>.mix` to the building search; support art `Palette=lib`.
   - Restores: the Statue of Liberty (ALL01), the White House and three memorials (ALL03, ALL06), Chicago ×2 (ALL04),
     the Gateway Arch (ALL08), St Basil's (ALL12), 18 ammo crates (ALL10).
   - Acceptance: rerun `scratch\play\w3-all01t.play`; `v_lm_CANEWY04_*` shows the statue in green, not magenta, and no
     `[w3-noart]` lines remain.
2. **RC-1a (S).** Make `camp::Types::building` theater-aware, sharing one resolver with PresMap and
   depth_register_building; also covers RC-1c. Fixes 379 snow objects, including ALL02's objective chapel, ALL12's
   Kremlin palace and walls, and every flag base. Acceptance: `census\resolve.py` shows no CLONE-DIFFERENT-FILE on
   SNOW maps.
3. **RC-6 (S).** Campaign SHP vehicles: Terror Drones in 6 missions, Dolphins and Squid in ALL11.
4. **RC-5 (S).** Water-bound map structures: the player's shipyard in ALL07, and the enemy and player yards in
   ALL04/ALL11.
5. **RC-2 part 1 (S).** Honour `InvisibleInGame`: 104 lamp sculptures disappear. **Part 2 (M):** light sources, which
   ALL03 (ambient 0.5) needs to look like the original's lit night streets.
6. **RC-8 (M).** Trigger visuals:
   - 41 PlayAnimAt with the RA2 de-duplicated index (ALL12 chrono flashes, objective arrows in ALL01/03/05);
   - explosion anims for 63/42;
   - 88 ParticleAnim (needs the particle system);
   - camera 48/112, radar ping 55 and flashes 104/131.
7. **RC-7 (M).** Infantry sequence state machine: idle, prone, panic, cheer, swim and death variants. This is the
   biggest "nothing animates" fix by screen area: every mission has 36-112 infantry.
8. **RC-3 (M).** Building Idle, Special and Production slots, and gates. Tesla coils alone appear 75 times in ALL05/06/
   08/09/11/12.
9. **RC-4 (M-L).** DestroyAnim, rubble, debris and smoke. Landmark destruction matters to ALL01 ("protect the Statue")
   and ALL03 ("don't destroy the White House") once RC-1b makes them visible.
10. **RC-9 (S).** ALL07 waterfalls. **RC-10/RC-11** are cross-cutting and low priority.

The 03 report's §3.1 also applies to every animation that does work here: anims are clocked per render frame, about
4× too fast at 60 Hz. It should go in with item 1-2 so the flag and power-plant timing is right once the art is right.

## 5. Open RE questions

- **RE-W3-1** RA2 `art.ini` sets `NewTheater=no` on CAUSFGL/CARUFGL/CAGEFGL/CAGARD01, yet the
  bodies exist only as theater variants (`ctusfgl`/`cuusfgl`/`causfgl`). `causfgl.shp` is the snow file and lives
  only in `isosnow.mix`. Under building-render.md §1 (munge only with NewTheater), the original in URBAN/TEMPERATE would
  find no body and draw only the waving cloth anim (`*_A`, which holds only the cloth;
  `scratch\ref\flags.png`). Confirm in `game.exe` whether the munge really is gated for these, or whether RA2 differs
  from the YR RE (03 RE-Q9).
- **RE-W3-2** Action 41's index space. The test above de-duplicates the RA2 `[Animations]` list. That fits 2 of 3
  indices (479 = ARRWNE, 253 = CHRONOTG), but 471 ("Arrows - Garrison" in ALL03/ALL05) maps to `CAWA19_AG`, which
  suggests the list differs by about 2 entries. Recover AnimTypeClass find-or-allocate order (anims allocated by
  earlier INI references?) from the TAction 41 case (`FUN_006dd...`, Execute jump table 0x6DF35C).
- **RE-W3-3** Light source pass: how `LightVisibility`, `LightIntensity` and `Light*Tint` of buildings and terrain add
  to the cell LightConvert, and whether `Powered=true` lamps go dark without power (W2-GA-05).
- **RE-W3-4** `Apply100Damage` (`FUN_006dfa00`): the anim used for its five C4Warhead explosions (the warhead's
  `AnimList`?) and their spread around the waypoint.
