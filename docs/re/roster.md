# RA2 Roster Broadening — RE / Data Recovery

Scope: recover, from the REAL game data, which units/buildings to add next,
their calibrated stats/prereqs/weapons, and which have art present in THIS
partial install. Calibrate-first: every stat below is grepped from the extracted
real `rulesmd.ini`, every art verdict is an `ra2extract` probe.

Tag legend: **[CONFIRMED]** = read straight from `rulesmd.ini` or an
`ra2extract` probe of this install. **[ANCHORED]** = corroborated by
documented RA2 / OpenRA structure. **[RE-FALLBACK]** = a design decision for
this port (art-guarding, batch selection), not a recovered fact.

Evidence sources:
- Rules: `D:\ra2-re\out\shroud_evidence\rulesmd.ini` (742,958 bytes)
- Extractor: `D:\ra2-android\build\ra2extract.exe`
- Game data: `D:\Red Alert 2\ra2.mix`, `ra2md.mix`, `language.mix`
- Engine roster (source of "already-wired" truth): `D:\ra2-android\engine\main.cpp`

---

## §0 How art presence was probed (method)

`ra2extract <mix> --raw <nested/asset> <out.bin>` succeeds (writes bytes) only
if the hashed filename is found. Presence = exit 0. Naming rules recovered from
the engine loaders (`engine/main.cpp`, `src/theater_name.cpp`):

- **Infantry SHP** → `ra2.mix/conquer.mix/<Image>.shp`
- **Vehicle/Aircraft** → `ra2.mix/local.mix/<Image>.vxl` (+`.hva`); md units in `ra2md.mix/localmd.mix`
- **Building SHP** → theater-letter munge (`src/theater_name.cpp`): for a name
  whose `[0]∈{g,n,c,y}` and `[1]∈{a,t}`, letter `[1]` is replaced by theater
  `t` (themed) or `g` (generic); resolved in `ra2.mix/{isotemp,temperat,generic}.mix`.
  In practice every present building resolved as the **generic** form in
  `ra2.mix/generic.mix` (e.g. `GAPOWR`→`ggpowr.shp`, `NAFLAK`→`ngflak.shp`).
- **Cameo** → `language.mix/cameo.mix/<Cameo>.shp` (guarded; absent → placeholder).
- Art `Image=` OVERRIDE matters: several ids draw under a different image
  (`APOC`→`MTNK`, `TERROR`→`TRST`, `MGTK`→`RTNK`, `JUMPJET`→`ROCK`,
  `ORCA`→`FALC`, `GHOST`→`SEAL`, `TESLA`→`NATSLA`, `ATESLA`→`GAPRIS`,
  `GASPYSAT`→`GASPST`). Probes below use the Image, not the id. [CONFIRMED]

Representative probe transcript:
```
$ ra2extract ra2.mix --raw local.mix/htnk.vxl out.bin   -> wrote 34550 bytes
$ ra2extract ra2.mix --raw conquer.mix/gi.shp   out.bin  -> wrote 186032 bytes
$ ra2extract ra2.mix --raw generic.mix/ggpowr.shp out.bin -> wrote (present)
$ ra2extract ra2.mix --raw conquer.mix/e1.shp   out.bin  -> error: not found  (E1 draws as GI)
$ ra2extract ra2md.mix --raw localmd.mix/robo.vxl out.bin -> wrote (present, md)
```

---

## §1 Full cast vs already-wired

Cast counts from `rulesmd.ini`: `[InfantryTypes]` 65, `[VehicleTypes]` ~82
(incl. civ/dummy), `[AircraftTypes]` 12, `[BuildingTypes]` 400+ (mostly civilian
`CA*`/theater props). The buildable Allied/Soviet combat cast is ~40 types; the
rest are civilians, Yuri-faction (`YA*`, Owner=YuriCountry), heroes, and props.

### Already-wired (from engine source, the source of truth)

Units — `engine/main.cpp` `load_*_unit_roster` + per-image asset loaders:
| id | role | side | note |
|----|------|------|------|
| E1 (art GI) | GI/Conscript-Allied basic inf | Allied | wired |
| E2 | Conscript | Soviet | Soviet autocreate |
| GGI | Guardian GI | Allied | Allied autocreate |
| DOG / ADOG | Attack Dog | Allied/Soviet | wired |
| MTNK (art GTNK) | Grizzly | Allied | Allied autocreate GAWEAP |
| HTNK | Rhino Tank | Soviet | Soviet autocreate NAWEAP |
| FV | IFV | Allied | Allied autocreate GAWEAP |
| ENGINEER / SPY | support | both | capture wired |
| SREF | Prism Tank | Allied | wired |
| HARV / CMIN | miners | both | economy |
| AMCV / SMCV | MCV | both | multihouse |
| APOC | Apocalypse | Soviet | **proxy only** (`APOC→NAWEAP` hardcoded token; real prereq NAWEAP,NATECH not yet honored) |
| HTK | Flak Track | Soviet | **already** in `kSoviet` roster (`main.cpp:1261`) |

Buildings — the engine's `kRoster[20]` (`engine/main.cpp:1451`):
`GACNST GAPOWR GAPILE GAWEAP  NACNST NAPOWR NAHAND NAWEAP  GAREFN NAREFN
GAPILL GATOW NALASR NABNKR TESLA NAFLAK NAWALL NARADR GATECH CAOILD`.
So **TESLA (Tesla Coil), NAFLAK (Flak Cannon), NABNKR (Battle Bunker),
GAPILL (Allied Pillbox), NALASR (Soviet Sentry Gun) are ALREADY rostered** —
they must NOT be re-added. `GAAIRC`, `GADEPT/NADEPT`, `NANRCT`, `ATESLA`,
`NASAM`, `GTGCAN`, `GASPYSAT` are NOT yet rostered → candidates. [CONFIRMED]

> Correction vs the phase brief: the brief listed Tesla Coil/Flak Cannon/Battle
> Bunker as candidate *adds*; they are already in `kRoster`. The batch below
> drops them and instead adds the genuinely-missing defenses + faction units.

---

## §2 The ADD BATCH (16 types)

Selection rationale [RE-FALLBACK]: prioritize (a) art present in THIS install
(real render), (b) systems not yet exercised (parasite, spawner-projectiles,
naval, aircraft+dock, teleport, disguise, jumpjet, radiation, AA, base-defense
weapons, power). All stats [CONFIRMED] from `rulesmd.ini` unless tagged.

Side is from `Owner=`: Soviet = Russians,Confederation,Africans,Arabs;
Allied = British,French,Germans,Americans,Alliance. [CONFIRMED]

### Soviet units

**DRON — Terror Drone** (art image `DRON`)
| field | value | field | value |
|---|---|---|---|
| Category | AFV | Prerequisite | NAWEAP |
| TechLevel | 4 | Cost | 500 |
| Strength | 100 | Armor | special_1 |
| Speed | 10 | Sight | 4 | Turret | no |
| Primary | DroneJump | Secondary | VirtualScanner |
Weapon DroneJump: Damage 50, ROF 60, Range 1.83, Warhead **Parasite**, Projectile JUMP, Speed 30.
Flags: ImmuneToPsionics, ImmuneToRadiation, Parasiteable=no, Crewed=no.
Art: **SHP present** `ra2.mix/conquer.mix/dron.shp`. Cameo `dronicon.shp` present. [CONFIRMED]

**V3 — V3 Launcher** (art image `V3`, VXL)
| field | value | field | value |
|---|---|---|---|
| Category | AFV | Prerequisite | NAWEAP,NARADR |
| TechLevel | 3 | Cost | 800 |
| Strength | 150 | Armor | light |
| Speed | 4 | Sight | 7 | Turret | no |
| Primary | V3Launcher | Spawns | **V3ROCKET** (SpawnsNumber 1, RegenRate 400) |
Weapon V3Launcher: Damage 1, ROF 150, Range 18, MinimumRange 5, Warhead Special, Projectile InvisibleHigh — actual damage delivered by the spawned V3ROCKET missile.
Art: **VXL present** `ra2.mix/local.mix/v3.vxl`. Cameo `v3icon.shp` present. [CONFIRMED]

**ZEP — Kirov Airship** (art image `ZEP`, VXL; a VehicleType with `ConsideredAircraft=yes`)
| field | value | field | value |
|---|---|---|---|
| Category | AirPower | Prerequisite | NAWEAP,NATECH |
| TechLevel | 10 | Cost | 2000 |
| Strength | **2000** | Armor | medium |
| Speed | 5 | Sight | 8 | SpeedType | Hover |
| Primary | BlimpBomb | Locomotor | Jumpjet (`BalloonHover`, `Crashable`) |
Weapon BlimpBomb: Damage 250, ROF 50, Range 1.5, Warhead BlimpHE, Projectile BlimpBombP, Burst 1.
Flags: JumpjetHeight 750, SelfHealing, `ElitePrimary=BlimpBombE`.
Art: **VXL present** `ra2.mix/local.mix/zep.vxl`. Cameo `zepicon.shp` present. [CONFIRMED]

**SHK — Shock Trooper (Tesla Trooper)** (art image `SHK`)
| field | value | field | value |
|---|---|---|---|
| Category | Soldier | Prerequisite | NAHAND |
| TechLevel | 5 | Cost | 500 |
| Strength | 130 | Armor | Plate |
| Speed | 4 | Sight | 6 | Crushable | no |
| Primary | ElectricBolt | Secondary | AssaultBolt (`IFVMode=6`) |
Weapon ElectricBolt: Damage 50, ROF 60, Range 3, Warhead **Shock**, Projectile InvisibleLow, Speed 100.
Art: **SHP present** `ra2.mix/conquer.mix/shk.shp`. Cameo `shkicon.shp` present. [CONFIRMED]

**DESO — Desolator** (art image `DESO`)
| field | value | field | value |
|---|---|---|---|
| Category | Soldier | Prerequisite | NAHAND,RADAR |
| TechLevel | 8 | Cost | 600 |
| Strength | 150 | Armor | plate |
| Speed | 4 | Sight | 6 | Deployer | yes (`DeployFire`) |
| Primary | RadBeamWeapon | Secondary | RadEruptionWeapon |
Weapon RadBeamWeapon: Damage 125, ROF 50, Range 6, Warhead RadBeamWarhead, Speed 100.
Flags: ImmuneToRadiation, SelfHealing, Fearless. `RequiredHouses=Arabs`.
Art: **SHP present** `ra2.mix/conquer.mix/deso.shp`. Cameo `desoicon.shp` present. [CONFIRMED]

**APOC — Apocalypse** — PROMOTE the proxy (art image **`MTNK`**, VXL)
| field | value | field | value |
|---|---|---|---|
| Category | AFV | Prerequisite | **NAWEAP,NATECH** |
| TechLevel | 7 | Cost | 1750 |
| Strength | 800 | Armor | heavy | Turret | yes |
| Speed | 4 | Sight | 6 | SelfHealing | yes |
| Primary | 120mmx | Secondary | MammothTusk (AA) |
Weapon 120mmx: Damage 100, ROF 80, Range 5.75, Warhead ApocAP, Projectile Cannon, Burst 2.
Weapon MammothTusk (AA): Damage 50, ROF 80, Range 8, Warhead HE, Projectile AAHeatSeeker, Burst 2.
Promotion note: replace the hardcoded `APOC→NAWEAP` proxy token with the real
`Prerequisite=NAWEAP,NATECH` (dual prereq). See `main.cpp:1159` caveat about
`ai_team_hash`. [CONFIRMED stats; ANCHORED promotion path]
Art: **VXL present** `ra2.mix/local.mix/mtnk.vxl`. Cameo `mtnkicon.shp` present.

**TERROR — Terrorist** (art image **`TRST`**)
| field | value | field | value |
|---|---|---|---|
| Category | Soldier | Prerequisite | NAHAND,RADAR |
| TechLevel | 5 | Cost | 200 |
| Strength | 75 | Armor | flak |
| Speed | 6 | Sight | 9 | Explodes | yes |
| Primary | TerrorBomb | DeathWeapon | TerrorBomb |
Weapon TerrorBomb: Damage 225, ROF 10, Range 1.5, Warhead TerrorBombWH, Projectile Invisible.
`RequiredHouses=Confederation`, CanRetaliate=no, Trainable=no (suicide unit).
Art: **SHP present** `ra2.mix/conquer.mix/trst.shp`. Cameo `trsticon.shp` present. [CONFIRMED]

**TTNK — Tesla Tank** (art image `TTNK`, VXL) *(optional 16th — see note)*
| field | value | field | value |
|---|---|---|---|
| Category | AFV | Prerequisite | NAWEAP,NARADR |
| TechLevel | 10 | Cost | 1200 |
| Strength | 300 | Armor | heavy | Turret | yes (`IsTilter`) |
| Speed | 6 | Sight | 8 | Crusher | yes |
| Primary | TankBolt | `RequiredHouses=Russians` |
Weapon TankBolt: Damage 135, ROF 75, Range 4, Warhead Electric, Projectile InvisibleLow, Speed 100.
Art: **VXL present** `ra2.mix/local.mix/ttnk.vxl`. Cameo `ttnkicon.shp` present. [CONFIRMED]

### Allied units

**GHOST — Navy SEAL** (art image **`SEAL`**)
| field | value | field | value |
|---|---|---|---|
| Category | Soldier | Prerequisite | GAPILE,RADAR (or Pentagon CAWA2*) |
| TechLevel | 9 | Cost | 1000 |
| Strength | 125 | Armor | flak |
| Sight | 8 | NavalTargeting | 4 |
| Primary | MP5 | Secondary | Sapper (C4) |
Weapon MP5: Damage 125, ROF 10, Range 6, Warhead HollowPoint, Projectile InvisibleLow.
Art: **SHP present** `ra2.mix/conquer.mix/seal.shp`. Cameo `sealicon.shp` present. [CONFIRMED]

**CLEG — Chrono Legionnaire** (art image `CLEG`)
| field | value | field | value |
|---|---|---|---|
| Category | Soldier | Prerequisite | GAPILE,TECH |
| TechLevel | 10 | Cost | 1500 |
| Strength | 125 | Armor | none | `Teleporter=yes` |
| Speed | 5 | Sight | 8 | MoveToShroud | no |
| Primary | NeutronRifle | Locomotor | Teleport |
Weapon NeutronRifle: Damage 8, ROF 120, Range 5, Warhead **ChronoBeam** (erase), Projectile InvisibleMedium, Speed 100.
Art: **SHP present** `ra2.mix/conquer.mix/cleg.shp`. Cameo `clegicon.shp` present. [CONFIRMED]

**JUMPJET — Rocketeer** (art image **`ROCK`**)
| field | value | field | value |
|---|---|---|---|
| Category | Soldier | Prerequisite | GAPILE,RADAR |
| TechLevel | 3 | Cost | 600 |
| Strength | 125 | Armor | none | `JumpJet=yes` |
| Speed | 9 | Sight | 8 | SpeedType | Hover |
| Primary | 20mm | `ConsideredAircraft=yes`, HoverAttack |
Weapon 20mm: Damage 25, ROF 30, Range 5, Warhead SSA, Projectile Invisible3, Speed 100.
Flags: JumpjetHeight 500, BalloonHover, Crashable.
Art: **SHP present** `ra2.mix/conquer.mix/rock.shp`. Cameo `rockicon.shp` **ABSENT** → placeholder cameo. [CONFIRMED]

**MGTK — Mirage Tank** (art image **`RTNK`**, VXL)
| field | value | field | value |
|---|---|---|---|
| Category | AFV | Prerequisite | GAWEAP,GATECH |
| TechLevel | 9 | Cost | 1000 |
| Strength | 200 | Armor | light | Turret | no (`IsTilter`) |
| Speed | 7 | Sight | 9 | `CanDisguise`/`DisguiseWhenStill` |
| Primary | MirageGun | Crusher | yes |
Weapon MirageGun: Damage 100, ROF 70, Range 7, Warhead MirageWH, Projectile InvisibleLow, Speed 100.
Art: **VXL present** `ra2.mix/local.mix/rtnk.vxl`. Cameo `rtnkicon.shp` present. [CONFIRMED]

**ORCA — Intruder** (AircraftType; art image **`FALC`**, VXL)
| field | value | field | value |
|---|---|---|---|
| Category | AirPower | Prerequisite | RADAR |
| TechLevel | 3 | Cost | 1200 |
| Strength | 150 | Armor | light | Ammo | 1 |
| Speed | 14 | Sight | 8 | `Dock=GAAIRC,AMRADR` |
| Primary | Maverick | Landable, AirportBound |
Weapon Maverick: Damage 150, ROF 10, Range 6, Warhead ORCAAP, Projectile AirToGroundMissile, Speed 70.
Exercises aircraft + dock/rearm (requires GAAIRC in roster). `Owner` excludes Alliance.
Art: **VXL present** `ra2.mix/local.mix/falc.vxl`. Cameo `falcicon.shp` present. [CONFIRMED]

**DEST — Destroyer** (naval; art image `DEST`, VXL)
| field | value | field | value |
|---|---|---|---|
| Category | Support | Prerequisite | GAYARD (Naval Yard) |
| TechLevel | 4 | Cost | 1000 |
| Strength | 600 | Armor | heavy | `Naval=yes`, SpeedType Float |
| Speed | 6 | Sight | 7 | Spawns | **ASW** helicopter |
| Primary | 155mm | Secondary | ASWLauncher |
Weapon 155mm: Damage 60, ROF 110, Range 8, Warhead ARTYHE, Projectile Ballistic, Speed 10.
Naval unit — requires naval yard `GAYARD` (not yet rostered) → prereq art-guard.
Art: **VXL present** `ra2.mix/local.mix/dest.vxl`. Cameo `desticon.shp` present. [CONFIRMED]

**AEGIS — Aegis Cruiser** (naval AA; art image `AEGIS`, VXL)
| field | value | field | value |
|---|---|---|---|
| Category | AFV | Prerequisite | GAYARD,RADAR |
| TechLevel | 7 | Cost | 1200 |
| Strength | 800 | Armor | light | `Naval=yes`, SpeedType Float |
| Speed | 4 | Sight | 8 | Crusher | no |
| Primary | Medusa (AA) | RadialFireSegments 10 |
Weapon Medusa: Damage 100, ROF 15, Range 12, Warhead SAMWH, Projectile MedusaProjectile, Speed 120.
Art: **VXL present** `ra2.mix/local.mix/aegis.vxl`. Cameo `aegisicon.shp` **ABSENT** → placeholder. [CONFIRMED]

### Buildings (new defenses + Allied radar)

**NANRCT — Soviet Nuclear Reactor** (art image `NANRCT`)
Prerequisite NATECH,NACNST; TechLevel 9; Cost 1000; Strength 1000; Armor concrete;
Sight 5; **Power +2000**; Adjacent 2; PoweredSpecial. Exercises the power system (huge output).
Art: **SHP present** `ra2.mix/generic.mix/ngnrct.shp`. Cameo absent → placeholder. [CONFIRMED]

**ATESLA — Allied Prism Tower** (art image **`GAPRIS`**)
Prerequisite POWER,RADAR,GACNST; TechLevel 6; Cost 1500; Strength 600; Armor steel;
Sight 8; Power −75; Adjacent 2; Primary PrismShot; Powered.
Weapon PrismShot: Damage 120, ROF 45, Range 8, Warhead PrismWarhead, Projectile InvisibleHigh, Speed 100.
Art: **SHP present** `ra2.mix/generic.mix/ggpris.shp`. Cameo absent → placeholder. [CONFIRMED]

**NASAM — Allied Patriot Missile** (art image `NASAM`)
Prerequisite BARRACKS,GACNST; TechLevel 4; Cost 1000; Strength 900; Armor steel;
Sight 10; Power −50; Adjacent 4; Primary RedEye2 (AA); Turret (voxel `SAM`).
Weapon RedEye2: Damage 75, ROF 55, Range 12, Warhead SAMWH, Projectile AAHeatSeeker, Speed 100.
Art: **SHP present** `ra2.mix/generic.mix/ngsam.shp`. Cameo absent → placeholder. [CONFIRMED]

**GTGCAN — Allied Grand Cannon** (art image `GTGCAN`)
Prerequisite RADAR,GACNST; TechLevel 7; Cost 2000; Strength 900; Armor steel;
Sight 10; Power −100; Adjacent 2; Primary GrandCannonWeapon; Turret (voxel, recoil).
Weapon GrandCannonWeapon: Damage 150, ROF 120, Range 15, MinimumRange 3, Warhead GrandCannonWH, Projectile GrandCannonBall.
Art: **SHP present** `ra2.mix/generic.mix/gggcan.shp`. Cameo absent → placeholder. [CONFIRMED]

**GAAIRC — Allied Airforce Command HQ** (art image `GAAIRC`) — enables radar + aircraft + ORCA dock
Prerequisite GAREFN,GACNST; TechLevel 3; Cost 1000; Strength 600; Armor steel;
Sight 5; Power −50; Factory=AircraftType. Also satisfies the `RADAR` prereq category
(`PrerequisiteRadar=GAAIRC,NARADR,...`) → gates ATESLA/GTGCAN/GHOST/JUMPJET/ORCA on the Allied side.
Art: **SHP present** `ra2.mix/generic.mix/ggairc.shp`. Cameo `gaaircicon.shp` — probe as guarded. [CONFIRMED]

### Batch summary (16)
Soviet: DRON, V3, ZEP, SHK, DESO, APOC(promote), TERROR, TTNK.
Allied: GHOST, CLEG, JUMPJET, MGTK, ORCA, (DEST/AEGIS naval — need GAYARD).
Buildings: NANRCT, ATESLA, NASAM, GTGCAN, GAAIRC.

That is 8 + 5(+2 naval) + 5 = up to 20. To stay in one phase, the **core 16**
is: DRON, V3, ZEP, SHK, DESO, APOC, TERROR, GHOST, CLEG, JUMPJET, MGTK, ORCA,
NANRCT, ATESLA, NASAM, GAAIRC — all art-present. **TTNK, GTGCAN** are drop-in
extras (also art-present). **DEST + AEGIS** are stretch (need the not-yet-rostered
`GAYARD` naval yard as prereq — defer to a naval phase). [RE-FALLBACK selection]

---

## §3 Allied base set for spawn_base symmetry

`spawn_base` (`engine/main.cpp:1785`) builds a full base only for Soviet
(house_side==Soviet): NACNST + NAPOWR + NAHAND + NAWEAP (+NAREFN if refinery).
Allied houses currently get ConYard (GACNST) only. The symmetric Allied set,
mapped 1:1 from the Soviet placements, with real stats [CONFIRMED]:

| Soviet (wired) | Allied (add) | Name | Prerequisite | Tech | Cost | Str | Power | Factory |
|---|---|---|---|---|---|---|---|---|
| NACNST | **GACNST** | Construction Yard | (none) | -1 | 3000 | 1000 | 0 | BuildingType |
| NAPOWR | **GAPOWR** | Power Plant | GACNST | 1 | 800 | 750 | +200 | — |
| NAHAND | **GAPILE** | Barracks | POWER,GACNST | 2 | 500 | 500 | −10 | InfantryType |
| NAWEAP | **GAWEAP** | War Factory | PROC,GAPILE,GACNST | 2 | 2000 | 1000 | −25 | UnitType |
| NAREFN | **GAREFN** | Ore Refinery | POWER,GACNST | 1 | 2000 | 1000 | −50 | (FreeUnit **CMIN**) |
| — | **GAAIRC** | Airforce Cmd HQ (radar) | GAREFN,GACNST | 3 | 1000 | 600 | −50 | AircraftType |

All six are already in `kRoster` EXCEPT GAAIRC (add it). `spawn_base` should
mirror the Soviet arm: `place(GACNST)`; then `GAPOWR (cx+6,cy)`,
`GAPILE (cx+6,cy+4)`, `GAWEAP (cx-1,cy+6)`, and with_refinery → `GAREFN (cx+9,cy+4)`
+ free CMIN at its dock (Allied FreeUnit is **CMIN** = Chrono Miner, vs Soviet). [CONFIRMED ids/stats]

Art: **all present** (see §4) — GACNST `ggcnst`, GAPOWR `ggpowr`, GAPILE `ggpile`,
GAWEAP `ggweap`, GAREFN `ggrefn`, GAAIRC `ggairc`, all in `ra2.mix/generic.mix`.

---

## §4 Art-presence table (ra2extract verdicts, this install)

Probe = `ra2extract <mix> --raw <path> out.bin` (exit 0 = present).

| id | art image | main art path | main? | cameo `<x>icon.shp` |
|----|-----------|---------------|:---:|:---:|
| DRON | DRON | ra2.mix/conquer.mix/dron.shp | **Y** | Y |
| V3 | V3 | ra2.mix/local.mix/v3.vxl | **Y** | Y |
| ZEP | ZEP | ra2.mix/local.mix/zep.vxl | **Y** | Y |
| HTK (wired) | HTK | ra2.mix/local.mix/htk.vxl | **Y** | Y |
| APOC | MTNK | ra2.mix/local.mix/mtnk.vxl | **Y** | Y |
| TTNK | TTNK | ra2.mix/local.mix/ttnk.vxl | **Y** | Y |
| MGTK | RTNK | ra2.mix/local.mix/rtnk.vxl | **Y** | Y |
| AEGIS | AEGIS | ra2.mix/local.mix/aegis.vxl | **Y** | **N** |
| DEST | DEST | ra2.mix/local.mix/dest.vxl | **Y** | Y |
| ROBO | ROBO | ra2md.mix/localmd.mix/robo.vxl | **Y** | **N** |
| ORCA | FALC | ra2.mix/local.mix/falc.vxl | **Y** | Y |
| SHK | SHK | ra2.mix/conquer.mix/shk.shp | **Y** | Y |
| DESO | DESO | ra2.mix/conquer.mix/deso.shp | **Y** | Y |
| TERROR | TRST | ra2.mix/conquer.mix/trst.shp | **Y** | Y |
| GHOST | SEAL | ra2.mix/conquer.mix/seal.shp | **Y** | Y |
| CLEG | CLEG | ra2.mix/conquer.mix/cleg.shp | **Y** | Y |
| JUMPJET | ROCK | ra2.mix/conquer.mix/rock.shp | **Y** | **N** |
| TESLA (wired) | NATSLA | ra2.mix/generic.mix/ngtsla.shp | **Y** | Y (`teslaicon`) |
| NAFLAK (wired) | NAFLAK | ra2.mix/generic.mix/ngflak.shp | **Y** | **N** |
| NANRCT | NANRCT | ra2.mix/generic.mix/ngnrct.shp | **Y** | **N** |
| ATESLA | GAPRIS | ra2.mix/generic.mix/ggpris.shp | **Y** | **N** |
| NASAM | NASAM | ra2.mix/generic.mix/ngsam.shp | **Y** | **N** |
| GTGCAN | GTGCAN | ra2.mix/generic.mix/gggcan.shp | **Y** | **N** |
| GASPYSAT | GASPST | ra2.mix/generic.mix/ggspst.shp | **Y** | **N** |
| GACNST | GACNST | ra2.mix/generic.mix/ggcnst.shp | **Y** | (wired) |
| GAPOWR | GAPOWR | ra2.mix/generic.mix/ggpowr.shp | **Y** | (wired) |
| GAPILE | GAPILE | ra2.mix/generic.mix/ggpile.shp | **Y** | (wired) |
| GAWEAP | GAWEAP | ra2.mix/generic.mix/ggweap.shp | **Y** | (wired) |
| GAREFN | GAREFN | ra2.mix/generic.mix/ggrefn.shp | **Y** | (wired) |
| GAAIRC | GAAIRC | ra2.mix/generic.mix/ggairc.shp | **Y** | probe-guard |
| **NABNKR** (wired) | NABNKR | — | **N** (absent everywhere) | N |
| **NATBNK** | NATBNK | — | **N** (Yuri faction) | N |
| **GAROBO** | GAROBO | — | **N** (absent everywhere) | N |

Deep-probe transcript for the absent ones (checked conquer/local/generic/expand/
ecache + md nested + munged forms):
```
DRON   => ra2.mix:conquer.mix/dron.shp     (SHP, not VXL)
NATBNK => ABSENT-everywhere
NABNKR => ABSENT-everywhere
GAROBO => ABSENT-everywhere
```
Building cameos (`naflakicon/nanrcticon/gaprisicon/nasamicon/gtgcanicon/gaspsticon`,
plus `aegisicon/roboicon/rockicon`) are **absent** as both `.shp` and `.pcx` in
`cameo.mix`, `ecache01.mix`, `expand01.mix` — this install carries no sidebar
cameo for them. Engine cameo lookup is already guarded → placeholder cameo. [CONFIRMED]

---

## §5 Confidence summary

**Every stat, prereq, weapon value, side, and art verdict above is
[CONFIRMED]** — read directly from the extracted real `rulesmd.ini` or from an
`ra2extract` probe of this exact install. [ANCHORED]/[RE-FALLBACK] apply only to
*decisions*: which types form the batch, the APOC promotion path, art-guarding of
absent art, and the spawn_base symmetry mapping.

### Art-present (REAL render) vs placeholder split

**Real render — main art present (17 batch + 6 base = ship un-guarded art):**
DRON, V3, ZEP, SHK, DESO, APOC(MTNK), TERROR(TRST), TTNK, GHOST(SEAL), CLEG,
JUMPJET(ROCK), MGTK(RTNK), ORCA(FALC), DEST, AEGIS, NANRCT, ATESLA(GAPRIS),
NASAM, GTGCAN, GAAIRC + base set GACNST/GAPOWR/GAPILE/GAWEAP/GAREFN.
(Some have NO cameo — sidebar cameo falls back to placeholder, unit still real.)

**Placeholder needed (main art ABSENT → ship art.ok=false, still buildable/functional):**
- **NABNKR** (Battle Bunker) — already rostered but art absent in this install.
- **GAROBO** (Robot Control Center) — absent → blocks ROBO's prereq; defer ROBO.
- **NATBNK** (Tank Bunker) — Yuri faction (Prereq YACNST), out of Allied/Soviet scope.

### Prereq-blockers to note for the plan
- **APOC** promotion needs the real dual prereq NAWEAP,NATECH (NATECH not yet a
  rostered/wired factory gate) — verify NATECH is buildable or the tech tree stalls.
- **ORCA** needs **GAAIRC** rostered (its dock + the RADAR category). Add GAAIRC first.
- **DEST/AEGIS** need **GAYARD** (naval yard) — not rostered; defer naval pair.
- **ROBO** needs **GAROBO** — art absent; defer.
- **GASPYSAT** is art-present but rostering it triggers the shroud-phase T7
  reveal-ordering carry (`SpySat=` whole-map reveal, `main.cpp:1518`) — include
  only if the plan accepts that carry; otherwise defer.

### Prerequisite categories (from `[General]`, needed to resolve prereqs) [CONFIRMED]
```
PrerequisitePower   = GAPOWR,NAPOWR,NANRCT,YAPOWR
PrerequisiteFactory = GAWEAP,NAWEAP,YAWEAP
PrerequisiteBarracks= NAHAND,GAPILE,YABRCK
PrerequisiteRadar   = GAAIRC,NARADR,AMRADR,NAPSIS
PrerequisiteTech    = GATECH,NATECH,YATECH
PrerequisiteProc    = GAREFN,NAREFN,YAREFN
```

---

## §Adjudication (RE-GATE) — 2026-07-20

Task 1 RE-GATE for the Broaden-Roster phase. Re-confirmed all 16 batch types exist in
the real `rulesmd.ini` with stats (spot-verified Name/Prerequisite/TechLevel/Cost/
Strength). Closes the four items.

### Item 1 — final batch membership → ADJUDICATED (16 core, all art-present)
Confirmed present + resolvable (stats verbatim from rules): Soviet units APOC
(Apocalypse, NAWEAP,NATECH, TL7, C1750, Str800), DRON (Terror Drone, NAWEAP, TL4,
C500, Str100), V3 (V3 Launcher, NAWEAP,NARADR, TL3, C800, Str150), SHK (Shock/Tesla
Trooper, NAHAND, TL5, C500, Str130), DESO (Desolator, NAHAND,RADAR, TL8, C600,
Str150), ZEP (Kirov, NAWEAP,NATECH, TL10, C2000, Str2000), TERROR (Terrorist,
NAHAND,RADAR, TL5, C200, Str75); Allied units GHOST (SEAL, GAPILE,RADAR, TL9, C1000,
Str125), CLEG (Chrono Legionnaire, GAPILE,TECH, TL10, C1500, Str125), JUMPJET
(Rocketeer, GAPILE,RADAR, TL3, C600, Str125), MGTK (Mirage Tank, GAWEAP,GATECH, TL9,
C1000, Str200), ORCA (Intruder bomber, RADAR, TL3, C1200, Str150); buildings NANRCT
(Nuclear Reactor, NATECH,NACNST, TL9, C1000, Str1000), ATESLA (Prism Tower/"Prism
Cannon", POWER,RADAR,GACNST, TL6, C1500, Str600), NASAM (Patriot, BARRACKS,GACNST,
TL4, C1000, Str900), GAAIRC (Airforce Command HQ/radar, GAREFN,GACNST, TL3, C1000,
Str600). Drop-in extras TTNK/GTGCAN NOT included (stay at 16). Already-rostered
(dropped): TESLA/NAFLAK/NABNKR/GAPILL/NALASR + HTK.

### Item 2 — content-only-deferred abilities → ADJUDICATED (full list)
Wire STATS/WEAPON/ARMOR/PREREQ/ART/BUILDABILITY + basic combat via the generic
weapon path. DEFER these unique ACTIVE abilities (each a future mini-phase):
DRON burrow-into-vehicle; V3 rocket-projectile flight; ZEP bomb-drop; SHK
power-Tesla-coil + EMP; DESO radiation/deploy; TERROR suicide; APOC dual-weapon;
GHOST C4/water-crossing; CLEG teleport-erase + freeze; MGTK tree-disguise; ORCA
bomber run; ATESLA prism-beam-chain; NASAM anti-air targeting. **JUMPJET/ORCA
flight:** ALLOW the existing aircraft/jumpjet flight path THIS PHASE only if their
Locomotor CLSID resolves via resolve_combat_unit (main.cpp:658-661) with NO new code
— it's free reuse of the aircraft gate's mechanism; anything beyond basic flight is
deferred. If the CLSID does not resolve, they spawn as ground content and flight is
deferred. (Implementer confirms at resolve time.)

### Item 3 — Allied base set + placements → ADJUDICATED
spawn_base allied_full set = GACNST + GAPOWR + GAPILE + GAWEAP + GAREFN + GAAIRC
(all in kRoster after GAAIRC is added T2). Placements mirror the Soviet arm 1:1
(NAPOWR@+6,0 -> GAPOWR; NAHAND@+6,+4 -> GAPILE; NAWEAP@-1,+6 -> GAWEAP; NAREFN@+? ->
GAREFN; + GAAIRC in a free adjacent cell). Allied refinery FreeUnit = CMIN (Chrono
Miner) vs Soviet HARV. Behind the new default-false allied_full flag → existing
callers byte-identical.

### Item 4 — APOC-promote without touching kSoviet → ADJUDICATED
APOC is added to the NEW kRosterUnits content table (prereq NAWEAP,NATECH per rules).
The kSoviet AI proxy entry (main.cpp:1261, full_parse=false, ai_team_hash caveat) is
left BYTE-FOR-BYTE unchanged. NATECH is NOT rostered this phase, so APOC's LIVE
buildability (build-menu prereq chain) is a documented CARRY-FORWARD; the --demo-roster
gate RESOLVES + SPAWNS APOC directly (bypassing the buildability predicate, exactly
how --demo-techtree places GATECH), so the gate validates APOC content without needing
NATECH built.

**Net:** all four items adjudicated; none needs human escalation. Plan tasks T2-T6
consistent with these (append-only, kUnits/kSoviet untouched, allied_full flag,
abilities content-only, APOC gate-spawned).

### §Adjudication addendum — ATESLA/GAPRIS art reconciliation (2026-07-20)
§4's raw `ra2extract` probe listed `ATESLA→GAPRIS` art as PRESENT, but the ENGINE's
building loader (`resolve_shp` theater-munge path) does NOT resolve `GAPRIS` on this
install → at runtime `--demo-roster` reports `ATESLA … no-art` and `place_explicit`
art-guards it out (no instance placed). The LOADER's resolution is authoritative for
the shipped gate: ATESLA is treated ART-ABSENT (like GATOW/NABNKR/TESLA). It still
RESOLVES as a building (cost 1500/tech 6/str 600) — only its render/placement is
guarded. The `roster_hash=333cd43d8b5ece6a` pin is INSTALL-SPECIFIC accordingly:
were GAPRIS to resolve in the loader, an ATESLA instance would place and the hash
would change. (The §4 raw-probe "Y" reflects a bare MIX-membership check, not the
loader's theater-suffix resolution — the two legitimately differ.)
