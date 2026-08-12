# The multiplayer MAP SET — enumeration + file format (skirmish map browser)

Calibrate-first hunt for the next phase: replace the engine's single hardcoded
map (`mapsmd03.mix` entry `0x133a0cb2`, temperate, 103x110) with a real map
list — enumerate the stock multiplayer maps on this install, parse any of them
generically, and let the skirmish setup screen choose. **Asset/INI-driven hunt:
every claim below is from real files on `D:\Red Alert 2`, decoded with the
repo's own tooling (`build/ra2extract.exe`, `libra2assets.a`), cross-checked
against `D:\ra2-re\reference\{xcc,OpenRA}`. No Ghidra was used.**

Confidence labels per `workflow.md`: **CONFIRMED** (real data on this install,
usually + an independent reader), **PROBABLE** (strong but single-source or
third-party-reader-only), **[RE-FALLBACK]** (not determined; what was ruled out).

Evidence commands are inline. Hash→name resolution used `ra2extract --resolve`
with a names file parsed out of the XCC filename database
(`D:\ra2-re\reference\OpenRA\global mix database.dat`, format
`[u32 count]("name\0desc\0")*count` repeated per game section; 24,492 names).

---

## §1 Where the maps live (CONFIRMED — full enumeration of this install)

`D:\Red Alert 2\*.mix`, all opened and resolved:

| archive | size | entries | resolved | contents |
|---|---:|---:|---:|---|
| `MULTI.MIX` | 18,666,588 | 97 | 97/97 | **RA2 multiplayer maps** (`mp*.map`, `tn*` tournament, 25 `c#m#x` World-Domination-Tour cluster maps) |
| `multimd.mix` | 31,264,268 | 173 | 170/173 | **YR multiplayer maps** (`xmp*`, named maps, `c1a01md`-style co-op, `xtn*` tournament) |
| `Maps01.mix` | 3,823,604 | 18 | 18/18 | RA2 **Allied campaign** (`all01t.map`…, `trn01t/trn02t` training, `e31/e32` Ending) |
| `MAPS02.MIX` | 3,170,324 | 18 | 17/18 | RA2 **Soviet campaign** (`sov01t.map`…) |
| `mapsmd03.mix` | 4,419,780 | 14 | 14/14 | **YR campaign** (`all01umd`…`all07smd`, `sov01umd`…`sov07tmd`) |
| `expand01.mix` | 3,323,076 | 38 | 24/38 | RA2 patch: **`missions.pkt`**, `mpmodes.ini`, `rules.ini`, `trn01t/trn02t`, mode INIs |
| `ecache01.mix` | 5,885,605 | 263 | 51/263 | patch art/audio cache (no maps) |
| `WDT.MIX` | 6,181,704 | 126 | 125/126 | World Domination Tour **UI art** (`euterr*.shp`, `*_worlddom_b.pcx`) — no maps |
| `ra2.mix` / `ra2md.mix` / `language.mix` / `langmd.mix` / `theme*/movies*` | — | — | — | no maps; hold the pkt/CSF/theater data (§2, §6) |

Reproduce: `./build/ra2extract.exe "D:\Red Alert 2\multimd.mix" --resolve "" <namesfile>`
(empty nested path = top level).

**The engine's hardcoded map is a CAMPAIGN mission, not a multiplayer map**
(CONFIRMED): `mapsmd03.mix` entry `0x133A0CB2` resolves to **`sov07tmd.map`**
— YR Soviet mission 7, 367,021 bytes. It happens to parse like any map (same
`[Map]`/`[IsoMapPack5]`), which is why the engine works, but it has campaign
houses (`[BadGuy1 House]`…), embedded rules overrides, and **no
`[PreviewPack]`/`[Header]`**. The skirmish browser must draw from
`multimd.mix` (+ `MULTI.MIX` if RA2-mode maps are wanted), not `mapsmd03.mix`.

Unresolved entries, honestly: 3 in `multimd.mix` (`9498E004`, `94C4BDFA`,
`61B60AB4` — extracted by hash, they ARE map INIs, but no name in the XCC DB
and no `missionsmd.pkt` name hashes to them; unreachable leftovers) and 1 each
in `MAPS02.MIX`/`WDT.MIX`. **[RE-FALLBACK: identity of those 4 entries]** —
ruled out: every pkt-referenced name (`.map/.yrm/.mpr`), `deez1-8`,
`escalation`, `moonbase`, `xskyscrpr`, `dinowars/dinozoo`, `demomap`,
`testersmap`.

## §2 How the game enumerates maps: the `.pkt` manifest (CONFIRMED)

The stock map list is NOT "every entry in the mix" — it is a **packet file**,
an INI with a `[MultiMaps]` index and one section per map:

- **YR (`gamemd.exe`, our target): `missionsmd.pkt`** in `langmd.mix`
  (21,142 B, entry `480B82E4`).
- RA2: `missions.pkt` in `ra2.mix/local.mix` (15,979 B), superseded by the
  patch copy in `expand01.mix` (16,011 B).

Real excerpt (`missionsmd.pkt`):

```ini
[MultiMaps]
; 2 Player Maps
1=XMP29U2
2=XMP21S2
3=BldFeud
...
;South Pacific
[XMP01T4]
Description=DESC:MP01DU
CD=2
MinPlayers=2
MaxPlayers=4
GameMode=standard
```

- `[MultiMaps]` values are map **base names**: append `.map` and look them up
  in the mix chain (`BldFeud` → `bldfeud.map` in `multimd.mix`). All **161**
  non-commented `missionsmd.pkt` entries resolve to real files in
  `multimd.mix` on this install (0 missing). The RA2 `missions.pkt` lists 148,
  of which **58 are absent** from `MULTI.MIX` (50 WDT `C#M#x` variants + 8
  `deez*`) — an implementation MUST existence-filter the pkt list.
- `Description=DESC:...`/`NAME:...` is a **CSF string label**, resolved
  case-insensitively in `ra2md.csf` (`langmd.mix`, 573,137 B). E.g.
  `DESC:MP01DU` → **"South Pacific (2-4)"**, `DESC:MP02T2` → "Isle of War (2)".
  Stock display names live ONLY there — **no stock map has `[Basic] Name=`**
  (checked: 0 of 267 MP maps + 14 YR campaign maps have it).
- `GameMode=` is a CSV of mode tags. `mpmodesmd.ini`
  (`ra2md.mix/localmd.mix`) maps lobby modes → map filter:
  `[Battle] 1=GUI:Battle, STT:ModeBattle, MPBattleMD.ini, standard, true` —
  Battle ("standard" filter) is *"the only one that allows AI players"*, i.e.
  **the skirmish browser lists the 91 `GameMode=standard` maps**. Full tag
  census over the 161 active maps: standard 91, megawealth 30, teamgame 25,
  meatgrind 20, duel 11, cooperative 10, navalwar 8, nukewar 7, airwar 1
  (maps carry multiple tags).
- `MinPlayers`/`MaxPlayers` is the advertised lobby range. It normally equals
  the map's start-waypoint count but **can disagree** (CONFIRMED, 6 of 161):
  e.g. `arena33forever.map` and `sedonapass(mw).map` define **6** start
  waypoints but the pkt says `MaxPlayers=4`; `xdustbowlmw`/`saharamimw` define
  2 but say 4. The pkt is the lobby-facing cap; the waypoints are the placeable
  positions.
- PROBABLE (third-party docs only, nothing on this install to test): the
  original browser additionally lists loose `*.map`/`*.mpr` (RA2) / `*.yrm`
  (YR) files from the install dir as "unofficial" maps. This install has none.

## §3 The playable map set on THIS install

**YR skirmish universe: 161 playable maps** (all `missionsmd.pkt`-active, all
present in `multimd.mix`), of which **91 are `standard`** (the skirmish/Battle
list). Theaters: TEMPERATE 76, SNOW 39, URBAN 19, DESERT 14, NEWURBAN 13 —
**no LUNAR multiplayer map exists**. Starts: 2-player 34, 3p 7, 4p 81, 5p 1,
6p 24, 8p 14. RA2-mode additionally has 90 pkt-listed maps present in
`MULTI.MIX` (55 TEMPERATE / 36 SNOW / 6 URBAN among the 97 files).

Every row below was parsed from the real extracted file with `MapFile::parse`
(the repo's own code): dims = `[Map] Size` WxH; starts = count of `[Waypoints]`
keys 0–7; name = pkt `Description` label resolved via `ra2md.csf`.
File-size bytes are the mix-index entry sizes.

| file | display name | bytes | theater | WxH | starts | pkt min-max | modes |
|---|---|---:|---|---|---:|---|---|
| `xmp29u2.map` | The Alamo (2) | 83573 | URBAN | 60x60 | 2 | 2-2 | standard, meatgrind |
| `xmp21s2.map` | Alaskan Oil Spill (2) | 180174 | SNOW | 94x97 | 2 | 2-2 | nukewar, navalwar |
| `bldfeud.map` | Blood Feud (2) | 72457 | TEMPERATE | 60x52 | 2 | 2-2 | standard |
| `xdeadman.map` | Deadman's Ridge (2) | 106491 | TEMPERATE | 70x65 | 2 | 2-2 | standard |
| `dunepatr.map` | Dune Patrol (2) | 68466 | DESERT | 60x52 | 2 | 2-2 | standard |
| `xdustbowl.map` | Dustbowl (2) | 124135 | TEMPERATE | 70x76 | 2 | 2-2 | standard |
| `xmp14t2.map` | El Dorado (2) | 113288 | TEMPERATE | 55x90 | 2 | 2-2 | standard |
| `xhailmary.map` | Hail Mary (2) | 145662 | URBAN | 77x87 | 2 | 2-2 | standard |
| `xxmas.map` | Happy Trails (2) | 112557 | SNOW | 75x81 | 2 | 2-2 | standard |
| `hidvally.map` | Hidden Valley (2) | 64637 | TEMPERATE | 52x52 | 2 | 2-2 | standard |
| `hillbtwn.map` | A Hill Between (2) | 104272 | URBAN | 60x68 | 2 | 2-2 | standard |
| `xmp02t2.map` | Isle of War (2) | 200280 | TEMPERATE | 100x100 | 2 | 2-2 | standard, meatgrind |
| `fight.map` | Let There Be Fight (2) | 91254 | NEWURBAN | 81x52 | 2 | 2-2 | standard |
| `xmp08t2.map` | Little Big Lake (2) | 72781 | TEMPERATE | 40x80 | 2 | 2-2 | meatgrind, standard |
| `pcofdune.map` | Little Piece of Dune (2) | 94375 | DESERT | 70x60 | 2 | 2-2 | standard |
| `xbarrel.map` | Loaded Barrel (2) | 120331 | URBAN | 70x73 | 2 | 2-2 | standard |
| `xmp31s2.map` | May Day (2) | 124908 | SNOW | 70x83 | 2 | 2-2 | standard, navalwar |
| `xgrinder.map` | Meat Grinder (2) | 193917 | TEMPERATE | 94x97 | 2 | 2-2 | standard |
| `xnewhghts.map` | New Heights (2) | 64870 | TEMPERATE | 50x57 | 2 | 2-2 | standard |
| `xmp11t2.map` | Pinch Point (2) | 181990 | TEMPERATE | 120x70 | 2 | 2-2 | meatgrind, standard |
| `saharami.map` | Sahara Mirage (2) | 140184 | DESERT | 70x90 | 2 | 2-2 | standard |
| `xeb3.map` | Sovereign Land (2) | 189637 | SNOW | 100x80 | 2 | 2-2 | standard |
| `2peaks.map` | Twin Peaks (2) | 169875 | TEMPERATE | 80x102 | 2 | 2-2 | standard |
| `xmp06t2.map` | Urban Rush (2) | 62526 | TEMPERATE | 52x53 | 2 | 2-2 | standard, meatgrind |
| `xbreak.map` | Breakaway (2-3) | 176882 | SNOW | 90x98 | 3 | 2-3 | standard |
| `xmp09t3.map` | Canyon Fodder (2-3) | 144155 | TEMPERATE | 75x93 | 3 | 2-3 | meatgrind, standard |
| `xmp18s3.map` | Cold War (2-3) | 161180 | SNOW | 101x75 | 3 | 2-3 | standard |
| `xkiller.map` | Killer Instinct (2-3) | 184596 | TEMPERATE | 90x100 | 3 | 2-3 | standard |
| `xyuriplot.map` | Yuri's Plot (2-3) | 244027 | TEMPERATE | 100x120 | 3 | 2-3 | standard |
| `xmp03t4.map` | Anytown, Amerika (2-4) | 156269 | TEMPERATE | 80x85 | 4 | 2-4 | standard, teamgame |
| `xarena.map` | Arena (2-4) | 137830 | URBAN | 80x80 | 4 | 2-4 | standard, teamgame |
| `xdisaster.map` | Brink of Disaster (2-4) | 205192 | SNOW | 87x99 | 4 | 2-4 | standard |
| `xcarville.map` | Carville's Convoy (2-4) | 143415 | SNOW | 80x86 | 4 | 2-4 | standard |
| `xeb1.map` | City Under Siege (2-4) | 84187 | URBAN | 60x62 | 4 | 2-4 | standard, teamgame |
| `xeb4.map` | Country Swing (2-4) | 154752 | TEMPERATE | 100x80 | 4 | 2-4 | standard, teamgame |
| `xmp34u4.map` | DC Uprising (2-4) | 214201 | URBAN | 95x111 | 4 | 2-4 | standard |
| `xmp10s4.map` | Depth Charge (2-4) | 139871 | SNOW | 79x85 | 4 | 2-4 | navalwar, nukewar |
| `doubletrouble.map` | Double Trouble (2-4) | 117129 | TEMPERATE | 90x60 | 4 | 2-4 | teamgame |
| `downtown.map` | Downtown, Cityville (2-4) | 177924 | NEWURBAN | 80x95 | 4 | 2-4 | standard |
| `dryheat.map` | Dry Heat (2-4) | 81954 | DESERT | 60x62 | 4 | 2-4 | standard, teamgame |
| `facedown.map` | Face Down (2-4) | 167612 | SNOW | 90x95 | 4 | 2-4 | standard |
| `fourcorners.map` | Four Corners (2-4) | 189762 | TEMPERATE | 90x90 | 4 | 2-4 | standard |
| `frstbite.map` | Frostbite (2-4) | 203204 | SNOW | 100x100 | 4 | 2-4 | standard |
| `xmp19t4.map` | Golden State Fwy (2-4) | 247447 | TEMPERATE | 93x115 | 4 | 2-4 | standard, meatgrind |
| `groundze.map` | Ground Zero (2-4) | 79818 | NEWURBAN | 50x70 | 4 | 2-4 | standard |
| `xmp23t4.map` | Hammer and Sickle (2-4) | 221496 | TEMPERATE | 97x103 | 4 | 2-4 | standard, nukewar, navalwar |
| `xhills.map` | Head for the Hills (2-4) | 139078 | TEMPERATE | 80x80 | 4 | 2-4 | standard |
| `xmp05t4.map` | Heartland (2-4) | 162992 | TEMPERATE | 80x83 | 4 | 2-4 | meatgrind, standard |
| `xinvasion.map` | Invasion Confirmed (2-4) | 143130 | URBAN | 80x80 | 4 | 2-4 | standard |
| `isleofoades.map` | Isle Of Oades (2-4) | 168422 | TEMPERATE | 98x85 | 4 | 2-4 | standard |
| `xmp12s4.map` | Lake Blitzen (2-4) | 183194 | SNOW | 81x113 | 4 | 2-4 | standard, meatgrind |
| `xlostlake.map` | Lost Lake (2-4) | 207004 | TEMPERATE | 120x83 | 4 | 2-4 | standard, teamgame |
| `manhatta.map` | Manhattan Mayhem (2-4) | 211336 | NEWURBAN | 87x103 | 4 | 2-4 | standard |
| `xmp13s4.map` | Montana DMZ (2-4) | 154727 | SNOW | 85x85 | 4 | 2-4 | standard, meatgrind |
| `xeb5.map` | Mount Olympus (2-4) | 97202 | TEMPERATE | 60x71 | 4 | 2-4 | standard |
| `xnorest.map` | No Rest for the Wicked (2-4) | 195128 | TEMPERATE | 100x100 | 4 | 2-4 | standard |
| `nowimps.map` | No Wimps (2-4) | 161096 | TEMPERATE | 80x96 | 4 | 2-4 | meatgrind, standard |
| `xoceansid.map` | Oceanside (2-4) | 174198 | URBAN | 85x100 | 4 | 2-4 | standard |
| `offensedefense.map` | Offense Defense (2-4) | 84554 | TEMPERATE | 80x52 | 4 | 2-4 | teamgame |
| `ottersrevenge.map` | Otters Revenge (2-4) | 137414 | TEMPERATE | 80x82 | 4 | 2-4 | standard |
| `xpacific.map` | Pacific Heights (2-4) | 188812 | TEMPERATE | 90x95 | 4 | 2-4 | standard |
| `xmp33u4.map` | Paris Revisited (2-4) | 134469 | URBAN | 86x75 | 4 | 2-4 | standard |
| `xrockets.map` | Rockets Red Glare (2-4) | 216584 | URBAN | 100x100 | 4 | 2-4 | standard |
| `xround.map` | Roundhouse Kick (2-4) | 109176 | SNOW | 68x78 | 4 | 2-4 | standard |
| `rushhr.map` | Rush Hour (2-4) | 230986 | NEWURBAN | 100x88 | 4 | 2-4 | standard |
| `xshrapnel.map` | Shrapnel Mountain (2-4) | 148622 | SNOW | 82x82 | 4 | 2-4 | standard |
| `xeb2.map` | Sinkhole (2-4) | 218229 | TEMPERATE | 100x100 | 4 | 2-4 | standard |
| `xmp15s4.map` | Snow Valley (2-4) | 203755 | SNOW | 98x99 | 4 | 2-4 | standard, meatgrind |
| `xmp16s4.map` | Snowball's Chance (2-4) | 159076 | SNOW | 110x70 | 4 | 2-4 | standard, meatgrind |
| `xmp01t4.map` | South Pacific (2-4) | 208817 | TEMPERATE | 105x95 | 4 | 2-4 | standard |
| `xtanyas.map` | Tanya's Training Grounds (2-4) | 144888 | TEMPERATE | 80x80 | 4 | 2-4 | standard, teamgame |
| `xtower.map` | Tower of Power (2-4) | 204490 | SNOW | 100x100 | 4 | 2-4 | standard |
| `xtsunami.map` | Tsunami (2-4) | 194728 | TEMPERATE | 94x99 | 4 | 2-4 | standard |
| `xvalley.map` | Valley of Gems (2-4) | 231775 | TEMPERATE | 105x105 | 4 | 2-4 | standard |
| `arena33forever.map` | Arena 33 Forever (2-6) | 89591 | TEMPERATE | 60x66 | 6 | 2-4 | standard |
| `xbayopigs.map` | Bay of Pigs (2-6) | 362767 | URBAN | 136x140 | 6 | 2-6 | standard, teamgame |
| `xmp26s6.map` | Bering Strait (2-6) | 242313 | SNOW | 124x97 | 6 | 2-6 | standard, nukewar, navalwar |
| `bridgegap.map` | Bridging the Gap (2-6) | 115968 | TEMPERATE | 65x78 | 6 | 2-6 | standard |
| `xmp25t6.map` | DEFCON 6 (2-6) | 254683 | TEMPERATE | 106x114 | 6 | 2-6 | standard, meatgrind |
| `eastvsbest.map` | East vs Best (2-6) | 159111 | TEMPERATE | 100x75 | 6 | 2-6 | teamgame |
| `xkaliforn.map` | Kalifornia (2-6) | 149975 | URBAN | 90x72 | 6 | 2-6 | standard |
| `xmp17t6.map` | Malibu Cliffs (2-6) | 316392 | TEMPERATE | 101x128 | 6 | 2-6 | standard, meatgrind |
| `monumentvalley.map` | Monument Valley (2-6) | 238057 | DESERT | 120x100 | 6 | 2-6 | standard |
| `sedonapass.map` | Sedona Pass (2-6) | 249970 | DESERT | 120x104 | 6 | 2-4 | standard |
| `xmp30s6.map` | Siberian Wastes (2-6) | 258859 | SNOW | 107x113 | 6 | 2-6 | nukewar, navalwar, standard |
| `xgoldst.map` | Streets of Gold (2-6) | 189457 | URBAN | 103x82 | 6 | 2-6 | standard |
| `tourofegypt.map` | Tour Of Egypt (2-6) | 85613 | DESERT | 65x67 | 6 | 2-6 | teamgame |
| `xmp20t6.map` | Wild Animal Park (2-6) | 414307 | TEMPERATE | 131x146 | 6 | 2-6 | standard, nukewar, navalwar |
| `xdeath.map` | Death trap (2-8) | 353006 | TEMPERATE | 124x132 | 8 | 2-8 | standard |
| `xmp32s8.map` | Heck Freezes Over (2-8) | 193649 | SNOW | 90x110 | 8 | 2-8 | standard, meatgrind |
| `xmp27t8.map` | A Path Beyond II (2-8) | 370329 | TEMPERATE | 120x143 | 8 | 2-8 | standard, meatgrind |
| `xpowdrkeg.map` | Powder Keg (2-8) | 318545 | URBAN | 120x125 | 8 | 2-8 | standard |
| `xroulette.map` | Russian Roulette (2-8) | 297336 | SNOW | 157x83 | 8 | 2-8 | standard |
| `trailerpark.map` | Trailer Park (2-8) | 123912 | TEMPERATE | 98x59 | 8 | 2-8 | teamgame |
| `c1a01md.map` | Campaign 1 Map 1 | 163770 | TEMPERATE | 87x71 | 4 | 2-4 | cooperative |
| `c1a02md.map` | Campaign 1 Map 2 | 337896 | NEWURBAN | 105x110 | 4 | 2-4 | cooperative |
| `c1a03md.map` | Campaign 1 Map 3 | 237231 | SNOW | 110x80 | 4 | 2-4 | cooperative |
| `c2s01md.map` | Campaign 2 Map 1 | 248872 | NEWURBAN | 79x105 | 4 | 2-4 | cooperative |
| `c2s02md.map` | Campaign 2 Map 2 | 236298 | DESERT | 90x103 | 4 | 2-4 | cooperative |
| `c2s03md.map` | Campaign 2 Map 3 | 291033 | SNOW | 100x105 | 4 | 2-4 | cooperative |
| `c3y01md.map` | Campaign 3 Map 1 | 233850 | NEWURBAN | 84x110 | 4 | 2-4 | cooperative |
| `c3y02md.map` | Campaign 3 Map 2 | 223889 | TEMPERATE | 70x115 | 4 | 2-4 | cooperative |
| `c3y03md.map` | Campaign 3 Map 3 | 276487 | NEWURBAN | 90x102 | 4 | 2-4 | cooperative |
| `c4w01md.map` | Campaign 4 Map 1 | 238834 | NEWURBAN | 87x103 | 4 | 2-4 | cooperative |
| `xmp09du.map` | Canyon Fodder (2-3) | 144463 | TEMPERATE | 75x93 | 3 | 2-3 | duel |
| `xmp24du.map` | Bonanza (2-4) | 112016 | TEMPERATE | 55x86 | 4 | 2-4 | duel |
| `xmp18du.map` | Cold War (2-4) | 162459 | SNOW | 101x75 | 4 | 2-4 | duel |
| `xmp05du.map` | Heartland (2-4) | 162568 | TEMPERATE | 80x83 | 4 | 2-4 | duel |
| `xmp13du.map` | Montana DMZ (2-4) | 154532 | SNOW | 85x85 | 4 | 2-4 | duel |
| `xmp15du.map` | Snow Valley (2-4) | 203368 | SNOW | 98x99 | 4 | 2-4 | duel |
| `xmp01du.map` | South Pacific (2-4) | 208937 | TEMPERATE | 105x95 | 4 | 2-4 | duel |
| `xmp25du.map` | DEFCON 6 (2-6) | 254578 | TEMPERATE | 106x114 | 6 | 2-6 | duel |
| `xmp17du.map` | Malibu Cliffs (2-6) | 316248 | TEMPERATE | 101x128 | 6 | 2-6 | duel |
| `xmp27du.map` | A Path Beyond II (2-8) | 370236 | TEMPERATE | 120x143 | 8 | 2-8 | duel |
| `xmp32du.map` | Heck Freezes Over (2-8) | 191285 | SNOW | 90x110 | 8 | 2-8 | duel |
| `xdustbowlmw.map` | Dustbowl (2) | 122943 | TEMPERATE | 70x76 | 2 | 2-4 | megawealth |
| `xmp14mw.map` | El Dorado (2) | 111998 | TEMPERATE | 55x90 | 2 | 2-2 | megawealth |
| `xmp08mw.map` | Little Big Lake (2) | 72194 | TEMPERATE | 40x80 | 2 | 2-2 | megawealth |
| `saharamimw.map` | Sahara Mirage (2) | 139296 | DESERT | 70x90 | 2 | 2-4 | megawealth |
| `xmp29mw.map` | The Alamo (2) | 81059 | URBAN | 60x60 | 2 | 2-2 | megawealth |
| `xmp06mw.map` | Urban Rush (2) | 62175 | TEMPERATE | 52x53 | 2 | 2-2 | megawealth |
| `xeb1mw.map` | City Under Siege (2-4) | 83859 | URBAN | 60x62 | 4 | 2-4 | megawealth |
| `deathvalleygirlmw.map` | Death Valley Girl (2-4) | 107836 | DESERT | 100x52 | 4 | 2-4 | megawealth |
| `dryheatmw.map` | Dry Heat (2-4) | 80991 | DESERT | 60x62 | 4 | 2-4 | megawealth |
| `fourcornersmw.map` | Four Corners (2-4) | 189429 | TEMPERATE | 90x90 | 4 | 2-4 | megawealth |
| `groundzemw.map` | Ground Zero (2-4) | 80153 | NEWURBAN | 50x70 | 4 | 2-4 | megawealth |
| `xmp23mw.map` | Hammer and Sickle (2-4) | 220235 | TEMPERATE | 97x103 | 4 | 2-4 | megawealth |
| `xmp05mw.map` | Heartland (2-4) | 162347 | TEMPERATE | 80x83 | 4 | 2-4 | megawealth |
| `xmp13mw.map` | Montana DMZ (2-4) | 154977 | SNOW | 85x85 | 4 | 2-4 | megawealth |
| `xpacificmw.map` | Pacific Heights (2-4) | 188274 | TEMPERATE | 90x95 | 4 | 2-4 | megawealth |
| `xmp15mw.map` | Snow Valley (2-4) | 201269 | SNOW | 98x99 | 4 | 2-4 | megawealth |
| `xmp16mw.map` | Snowball's Chance (2-4) | 158790 | SNOW | 110x70 | 4 | 2-4 | megawealth |
| `xtowermw.map` | Tower of Power (2-4) | 203845 | SNOW | 100x100 | 4 | 2-4 | megawealth |
| `xvalleymw.map` | Valley of Gems (2-4) | 231537 | TEMPERATE | 105x105 | 4 | 2-4 | megawealth |
| `xmp25mw.map` | DEFCON 6 (2-6) | 255869 | TEMPERATE | 106x114 | 6 | 2-6 | megawealth |
| `xmp17mw.map` | Malibu Cliffs (2-6) | 314348 | TEMPERATE | 101x128 | 6 | 2-6 | megawealth |
| `monumentvalleymw.map` | Monument Valley (2-6) | 239080 | DESERT | 120x100 | 6 | 2-4 | megawealth |
| `sedonapassmw.map` | Sedona Pass (2-6) | 251447 | DESERT | 120x104 | 6 | 2-4 | megawealth |
| `xmp30mw.map` | Siberian Wastes (2-6) | 258694 | SNOW | 107x113 | 6 | 2-6 | megawealth |
| `xmp27mw.map` | A Path Beyond II (2-8) | 366746 | TEMPERATE | 120x143 | 8 | 2-8 | megawealth |
| `xmp22mw.map` | Arctic Circle (2-8) | 269558 | SNOW | 155x83 | 8 | 2-8 | megawealth |
| `xmp32mw.map` | Heck Freezes Over (2-8) | 191424 | SNOW | 90x110 | 8 | 2-8 | megawealth |
| `topothehill.map` | Top o'the Hill (2-3) | 80892 | TEMPERATE | 41x92 | 3 | 2-3 | teamgame |
| `austintx.map` | Austin, TX (2-4) | 110951 | NEWURBAN | 94x53 | 4 | 2-4 | meatgrind, teamgame |
| `deathvalleygirl.map` | Death Valley Girl (2-4) | 108273 | DESERT | 100x52 | 4 | 2-4 | teamgame |
| `xseaofiso.map` | Sea of Isolation (2-4) | 147097 | TEMPERATE | 90x80 | 4 | 2-4 | teamgame |
| `xamazon01.map` | Stormy Weather (2-4) | 148540 | TEMPERATE | 70x90 | 4 | 2-4 | teamgame |
| `turfwar.map` | Turfwar (2-4) | 89017 | NEWURBAN | 120x33 | 4 | 2-4 | teamgame |
| `mountmoras.map` | Mount Rush More (2-5) | 103241 | TEMPERATE | 61x87 | 5 | 2-5 | meatgrind, teamgame |
| `xpotomac.map` | Army of the Potomac (2-6) | 181967 | URBAN | 90x90 | 6 | 2-6 | teamgame |
| `xbermuda.map` | Bermuda Triangle (2-6) | 153130 | URBAN | 88x82 | 6 | 2-6 | teamgame |
| `triplecrossed.map` | Triple Crossed (3-6) | 159558 | SNOW | 115x71 | 6 | 2-6 | meatgrind, teamgame |
| `xmp22s8.map` | Arctic Circle (2-8) | 271376 | SNOW | 155x83 | 8 | 2-8 | teamgame, airwar, nukewar, navalwar |
| `nearoref.map` | Near Ore Far (2-8) | 97641 | TEMPERATE | 80x58 | 8 | 2-8 | standard |
| `xterritor.map` | Territorial Imperative (2-8) | 215944 | TEMPERATE | 120x80 | 8 | 2-8 | teamgame |
| `xtn02s4.map` | Official Tournament Map (4) | 144369 | SNOW | 85x92 | 4 | 2-4 | standard |
| `xtn01t2.map` | Official Tournament Map A (2) | 127655 | TEMPERATE | 87x67 | 2 | 2-2 | standard |
| `xtn04t2.map` | Official Tournament Map B (2) | 148203 | TEMPERATE | 85x92 | 2 | 2-2 | standard |
| `xtn02mw.map` | Official Tournament Map (4) | 146118 | SNOW | 85x92 | 4 | 2-4 | megawealth |
| `xtn01mw.map` | Official Tournament Map A (2) | 126451 | TEMPERATE | 87x67 | 2 | 2-2 | megawealth |
| `xtn04mw.map` | Official Tournament Map B (2) | 147849 | TEMPERATE | 85x92 | 2 | 2-2 | megawealth |

(12 more entries exist in `multimd.mix` that the stock game never lists: 9
named files that are pkt-inactive or commented out — `xmp04t8`, `xmp07t4`,
`xmp20mw`, `xmp24t2`, `xmp28u4`, `xmp30s8`, `xtn03t8`, `xtn03mw` (";169="/
";173="), `returntoforestfires` — plus the 3 unnamed hash entries of §1.)

## §4 The map file format (INI + packed sections)

A stock map is a plain-text INI (same syntax `ra2assets::Ini` already parses).
All excerpts below are verbatim from files on this install. Multiplayer maps
carry exactly these sections (xmp01t4.map order): `[SpecialFlags] [Ranking]
[Header] [Basic] [Lighting] [Preview] [PreviewPack] [Map] [Waypoints]
[IsoMapPack5] [Terrain] [Structures] [OverlayPack] [OverlayDataPack]
[ScriptTypes]/[TaskForces] stubs [AITriggerTypesEnable] [Triggers] [Events]
[Actions] [Tags] [Digest]`. Campaign maps add `[Houses]`, `[Units]`,
`[Infantry]`, `[Aircraft]`, `[Smudge]`, `[CellTags]`, `[VariableNames]`,
embedded rules-override sections, and full trigger/team data (sov07tmd.map has
~380 sections); they LACK `[Header]`/`[Preview]`/`[PreviewPack]`.

### 4.1 `[Basic]` (CONFIRMED)

```ini
[Basic]                      ; xmp01t4.map:41
Briefing=Brief:ALL01md       ; boilerplate, same in every MP map
NewINIFormat=4
Official=yes
MultiplayerOnly=1
...                          ; 22 keys total
```

**There is no `Name=` and no player-count key in any stock map's `[Basic]`**
(0/281 files). Display name and player range come from the pkt + CSF (§2);
start count comes from `[Waypoints]`/`[Header]` (§4.3). XCC's reader looks for
`Basic→Name/Player/MultiplayerOnly`; only `MultiplayerOnly` exists in stock
files.

### 4.2 `[Map]` (CONFIRMED — engine already parses Theater/Size)

```ini
[Map]                        ; xmp01t4.map:522
Theater=TEMPERATE
Size=0,0,105,95
LocalSize=5,4,95,85
```

- `Theater=` one of `TEMPERATE SNOW URBAN` (RA2) + `NEWURBAN DESERT LUNAR`
  (YR) — all six observed in real files on this install (LUNAR only in
  campaign `sov06lmd.map`; never in MP).
- `Size=x,y,w,h` — x,y always 0. `w,h` are the DIAMOND dimensions; the actual
  cell records span `x,y ∈ [1..w+h-ish]` (xmp01t4: x 1..199, y 1..199,
  x+y 106..295). **Cell count = `(2·w − 1) · h`** — verified exactly on
  xmp01t4 (105x95 → 19,855), xmp10s4 (79x85 → 13,345), sov07tmd (103x110 →
  22,550), and matching OpenRA's `ImportGen2MapCommand.ReadTiles`
  (`var cells = (fullSize.X * 2 - 1) * fullSize.Y`).
- `LocalSize=x,y,w,h` — the playable/visible sub-rect (map border trim), in
  the same units as `Size`. PROBABLE (semantics from OpenRA/XCC; not
  independently exercised here).

### 4.3 `[Waypoints]` + `[Header]` — the player-start convention

```ini
[Waypoints]                  ; xmp01t4.map:527 (4-player map)
0=98057
1=148099
2=96152
3=55110
26=93052
...
98=101116
99=100106
699=100100
```

**CONFIRMED — the key facts for placing houses:**

1. **Decode: `value = 1000·Y + X`**, i.e. `x = value % 1000`,
   `y = value / 1000`, giving a cell in the SAME coordinate space as the
   `[IsoMapPack5]` cell records (`MapCell::x/y`). Evidence:
   (a) OpenRA `ImportGen2MapCommand.ReadWaypoints`
   (`ry = pos / 1000; rx = pos - ry * 1000`); (b) XCC
   `map_ts_ini_reader`; (c) **decisively on this install's own data**: the MP
   `[Header]` section stores the same starts as x,y pairs in preview space —
   for all four xmp01t4 starts, `HeaderY = round((x+y)/2)` and
   `HeaderX = (x−y)/2 + 255.5±0.5` hold ONLY under this decode order
   (98057→(57,98): 78=155/2↑, 235=−20.5+255.5; the swapped decode gives no
   constant offset). Every decoded waypoint in every sampled map lands on a
   real IsoMapPack5 cell.
2. **Waypoints `0`–`7` are the multiplayer start positions**; the start count
   is simply how many of the keys `0..7` are present. Cross-checked
   **all 267 MP maps: the count of keys 0–7 equals `[Header]
   NumberStartingPoints` in every single file** (2..8; never more than 8).
   XCC computes `max_players()` the same way; OpenRA imports index ≤7 as
   `mpspawn`. Campaign maps also start waypoints at 0 (sov07tmd has 0..5+)
   but those are mission waypoints, not skirmish starts.
3. `[Header]` (MP maps only) is lobby/preview metadata:

```ini
[Header]                     ; xmp01t4.map:25
StartX=209
StartY=57
Width=94
Height=86
NumberStartingPoints=4
NumCoopHumanStartSpots=4
Waypoint1=235,78             ; = start waypoint 0, in preview space (see above)
...
Waypoint8=0,0                ; unused slots zero-filled
```

   A loader can use `NumberStartingPoints` as a cheap header-only player-count
   read (it never disagrees with `[Waypoints]` on this install), but the
   authoritative start CELLS require the `[Waypoints]` decode.
4. **[RE-FALLBACK: meaning of waypoints > 7]** — MP maps also carry waypoints
   in the 10–55 range plus `98`, `99`, and `699` (699 = map center in every
   sample; 98/99 near-center). The maps' embedded `[ScriptTypes]` are empty
   stubs, so these are NOT referenced by in-map scripts; presumably skirmish-AI
   pathing/base nodes used by `gamemd.exe` itself. Not determined here —
   ruled out: in-map script references. Only 0–7 matter for house placement.

### 4.4 `[IsoMapPack5]` (CONFIRMED — engine already decodes)

```ini
[IsoMapPack5]                ; xmp01t4.map:564
1=uw8AIA5pAAEAOgEAAAAEAGgAAgBDAYkBaUkBOkgBAQIEAGrIAQUDBABnAAMAO1QCjAWo...
```

Concatenate values in key order → base64 → "Format5" chunk stream
(`[u16 cmp][u16 uncmp][body]`*) with **LZO1X** bodies → 11-byte cell records
`{i16 x, i16 y, i32 tile, u8 subtile, u8 level, u8 icegrowth}`. This is
exactly what `src/mappack.cpp` + `src/map.cpp::parse_cell_records` implement
(0xFFFF tile sentinel → materialized as Clear; see map.cpp comments). Nothing
new needed for the map browser.

### 4.5 `[OverlayPack]` / `[OverlayDataPack]` (CONFIRMED — engine already decodes)

Same collect→base64→Format5 shape, **LCW ("format 80") bodies**, each
decoding to a flat 512x512 byte array (overlay type index / data byte per
cell, 0xFF = none). Implemented in `map.cpp::collect_overlay` and verified
against real data in economy.md §5. Nothing new needed.

### 4.6 `[Preview]` + `[PreviewPack]` — the lobby thumbnail (CONFIRMED, decodable)

```ini
[Preview]                    ; xmp01t4.map:86
Size=0,0,188,86
[PreviewPack]                ; xmp01t4.map:89
1=KwoAIAVITGBITGBITDUXAEhgSEgANowASAMnCAA8jAAqdACoBipEACosADB0AEQCJ4AA...
```

- Same base64 + Format5 wrapper, but the chunk bodies are **LZO — like
  IsoMapPack5, NOT LCW like OverlayPack**. (XCC `map_ts_encoder.cpp:431`
  decodes PreviewPack with `decode5(..., 5)`; ModEnc-style folklore says
  "format 80" — that is WRONG for these files.) An LCW attempt fails on all
  267 files; **LZO succeeds on all 267** and every one decodes to **exactly
  `w·h·3` bytes** for `[Preview] Size=0,0,w,h` (xmp01t4: 188x86 → 48,504).
- Pixel format: **24-bit RGB, R first** (CONFIRMED visually: rendered
  xmp01t4's preview both ways with stb; R-first gives tan islands /
  green terrain / dark-blue ocean with red start markers; B-first gives a
  cyan-land absurdity).
- Sizes on this install range 70..308 wide x 23..149 high (per-map, from the
  267 `[Preview] Size=` lines) — lobby-thumbnail scale, perfectly usable for
  the browser. **All 267 MP maps have one; the 14 campaign maps have none**
  (no `[Preview]`/`[PreviewPack]` sections at all), so a campaign/
  hardcoded-map fallback (blank or minimap render) is needed only if we ever
  browse those.
- Engine cost: ZERO new codecs — `base64_decode` +
  `decompress_mappack(..., MapPackCodec::LZO)` already exist; the browser just
  needs the `[Preview] Size` parse + RGB upload.

### 4.7 Pre-placed objects (format documented; engine does NOT parse these yet)

Key = arbitrary id, value = CSV. Real lines:

```ini
[Structures]                 ; xmp01t4.map (MP maps: neutral tech/city buildings)
1032800=Neutral,CAMIAM04,256,57,113,0,None,0,0,1,0,0,None,None,None,0,0
[Units]                      ; sov07tmd.map (campaign)
0=BadGuy3 House,CMIN,256,49,134,64,Harvest,None,0,-1,0,-1,1,1
[Infantry]                   ; sov07tmd.map
10=Einstein House,TERROR,256,190,110,4,Guard,128,093B806C,0,-1,0,1,1
[Terrain]                    ; xmp01t4.map (key = packed cell, 1000*y+x)
102112=TREE21
[Smudge]                     ; sov07tmd.map: TYPE,x,y,0
0=BURNT11,144,84,0
[CellTags]                   ; campaign only: packedcell=trigger-tag id
30105=0CBDCF4C
```

CONFIRMED on this install: field order head `OWNER,TYPE,HEALTH(0-256),X,Y`
for all three object sections; `[Terrain]` key is the same `1000·y+x` packed
cell as waypoints (102112 → (112,102), a real cell). PROBABLE (from OpenRA's
`ReadActors`, not exercised here): field 5 = facing for Units/Structures but
= sub-cell for Infantry (facing at field 7); the long tail (tag, veterancy,
group, on-bridge, upgrades, spotlight, ai-sellable/rebuildable…) differs per
section. All 267 MP maps carry `[Structures]`+`[Terrain]` (neutral eye-candy/
tech buildings, trees; 12,083 structure entries total, max 358 in one map),
and **85 of 267 also carry pre-placed `[Units]`/`[Infantry]`** — Neutral
civilian eye-candy (`0=Neutral,CAR,256,66,104,0,Sleep,None,0,-1,0,-1,1,1`,
CONA, SUVB, civilians), plus co-op maps' scripted forces. A generic loader
cannot treat these sections as campaign-only.

### 4.8 The rest

`[Lighting]`, `[SpecialFlags]`, `[Ranking]` — small scalar sections, ignorable
for a browser. `[Digest]` — one base64 line (a signature over the file; both
XCC and OpenRA ignore it; safe to skip). Campaign maps embed rules overrides
(`[General]`, unit sections, `[Countries]`, `[Houses]`/`[... House]`) that a
generic MP loader can ignore entirely.

## §5 What the engine ALREADY does vs what is MISSING

Already implemented (and oracle/gate-verified — do not re-do):

| piece | where | status |
|---|---|---|
| INI parse | `src/ini.cpp` | maps are plain INI — works on every stock map (this hunt parsed all 281 with it) |
| `[Map] Theater/Size` | `src/map.cpp` `MapFile::parse` | parsed (w/h from fields 2,3) |
| `[IsoMapPack5]` base64+Format5+LZO+11-byte records | `src/mappack.cpp`, `src/map.cpp` | CONFIRMED against XCC; 0xFFFF sentinel handled |
| `[OverlayPack]/[OverlayDataPack]` (LCW, 512x512) | `src/map.cpp` | done (economy.md §5) |
| variable grid dims | `engine/world.hpp` (`grid_w = max(cell.x)+1`) | already derived from cells, not hardcoded |
| theater tile pipeline (temperate) | `src/theater.cpp`, `engine/iso_renderer.hpp` | .tem/temperat.ini/isotem.pal chain wired |

Missing / hardcoded today:

1. **Map enumeration**: `engine/main.cpp::load_assets` opens `mapsmd03.mix`
   and reads hash `0x133a0cb2` (a CAMPAIGN mission, §1). No pkt reading, no
   `multimd.mix` access, no CSF name lookup, no GameMode filter.
2. **`[Waypoints]` is unparsed** — `MapFile` has no waypoint API.
   Consequences: `launch_skirmish` (main.cpp:9431) fans bases out from
   `pick_start_cell(map)` at `x + i*14` steps, and `make_ai_waypoints`
   (main.cpp:2412) synthesizes a ring around the player for AI scripts, both
   flagged `[RE-FALLBACK]` in ai-team.md. The real fix is §4.3: waypoints 0–7,
   `x=v%1000, y=v/1000`.
3. **Theater is temperate-only**: `load_assets` throws on any
   `Theater != TEMPERATE`; palette (`isotem.pal`), tile config
   (`temperat.ini`), tile mix (`isotemp.mix`) and the `.tem` extension are all
   hardcoded (also in `load_building_assets`' candidate walk and the ore-art
   loader). Data for the other five theaters IS on this install (§6).
4. **`[Preview]/[PreviewPack]`**: unparsed (trivial with existing codecs, §4.6).
5. **Pre-placed `[Structures]`/`[Terrain]`**: unparsed — stock MP maps ship
   neutral tech buildings, city blocks and trees that currently never spawn.
6. **`[Basic] MultiplayerOnly` / pkt metadata**: nothing reads them (fine for
   a browser fed by the pkt list itself).

## §6 Theater availability on this install (CONFIRMED by resolve)

Everything a generic theater loader needs exists locally, following the exact
naming pattern the temperate chain already uses (`<iso*>.mix` tiles,
`<name>.ini` tileset config, `iso<short>.pal` palette, per-theater tile
extension):

| Theater= | tiles mix (base / YR-extra) | config INI | iso palette | ext | MP maps |
|---|---|---|---|---|---:|
| TEMPERATE | `ra2.mix/isotemp.mix` / `ra2md.mix/isotemmd.mix` | `local.mix/temperat.ini` (+`temperatmd.ini`) | `cache.mix/isotem.pal` | `.tem` | 76 |
| SNOW | `ra2.mix/isosnow.mix` / `isosnomd.mix` | `snow.ini` (+`snowmd.ini`) | `isosno.pal` | `.sno` | 39 |
| URBAN | `ra2.mix/isourb.mix` / `isourbmd.mix` | `urban.ini` (+`urbanmd.ini`) | `isourb.pal` | `.urb` | 19 |
| NEWURBAN | `ra2md.mix/isoubn.mix` + `isoubnmd.mix` | `localmd.mix/urbannmd.ini` | `cachemd.mix/isoubn.pal` | `.ubn` | 13 |
| DESERT | `ra2md.mix/isodes.mix` + `isodesmd.mix` | `desertmd.ini` | `isodes.pal` | `.des` | 14 |
| LUNAR | `ra2md.mix/isolun.mix` + `isolunmd.mix` | `lunarmd.ini` | `isolun.pal` | `.lun` | 0 (campaign only) |

(Also present per theater: `temperat/snow/urban/urbann/desert/lunar.mix`
building-art mixes, `unit*.pal`, `lib*.pal`, `city*.pal`. YR theaters need
`ra2md.mix` + `cachemd.mix`/`localmd.mix`, which the engine already opens for
`rulesmd.ini`.) Tile extensions `.tem/.sno/.urb` are exercised today; the
`.ubn/.des/.lun` pattern is corroborated by the `des.mix/lun.mix/ubn.mix`
marker mixes and XCC's theater tables — PROBABLE until a non-temperate map is
actually rendered.

Skirmish-browser MVP can ship with the 5 MP theaters and reject LUNAR.

## §7 Gate-breakage risks (the byte-identity demo gates)

The demo gates (`--demo-*`, test_world) all build worlds **directly on the
hardcoded sov07tmd map** through `build_world`/`pick_start_cell`/
`make_ai_waypoints` and hash the result. Rules for the next phase:

1. **Do not change `MapFile::parse` semantics** — add waypoints/preview/name
   as NEW fields; any change to cell materialization (sentinel handling,
   record order) or overlay handling shifts every world hash.
2. **Do not repoint the default map.** `load_assets`' `mapsmd03/0x133a0cb2`
   path must stay the gates' map even after the browser exists; map choice
   must be a parameter that DEFAULTS to current behavior.
3. **`pick_start_cell`/`make_ai_waypoints` must keep their outputs on the gate
   paths.** Replacing them with real `[Waypoints]` data changes spawn cells →
   AI pathing → every hashed frame. Route real waypoints through
   `SkirmishConfig`/new parameters so gate call-sites are untouched.
4. Theater generalization must keep the temperate candidate-walk order
   byte-identical for existing assets (building/ore loaders bake `'T'`-letter
   and `.tem` candidates; reordering candidates can silently change which SHP
   variant wins).
5. New mixes (`multimd.mix`, `langmd.mix` for pkt/CSF) are additive opens —
   safe; but do NOT slurp them inside `load_assets` unconditionally on the
   gate path (startup-perf discipline, see 2026-07-09-startup-perf plan).

## §8 Confidence summary

| claim | evidence | confidence |
|---|---|---|
| MP maps live in `MULTI.MIX` (RA2) / `multimd.mix` (YR); hardcoded `0x133a0cb2` = campaign `sov07tmd.map` | resolve of every archive against XCC name DB | **CONFIRMED** |
| Enumeration = pkt manifest (`missionsmd.pkt` in `langmd.mix`; 161 active, all present; RA2 pkt lists 58 absent) | extracted + cross-checked file sets | **CONFIRMED** |
| Display names = pkt `Description` label → CSF (`ra2md.csf`), case-insensitive; stock maps have no `[Basic] Name` | all 161 resolved; 0/281 maps have Name= | **CONFIRMED** |
| Waypoint decode `x=v%1000, y=v/1000` in IsoMapPack5 cell space | OpenRA + XCC + this install's `[Header]` affine cross-check | **CONFIRMED** |
| Starts = waypoints 0–7; count == `[Header] NumberStartingPoints` | equality on 267/267 MP maps | **CONFIRMED** |
| `Size=w,h` → cell count `(2w−1)·h` | exact on 3 maps + OpenRA formula | **CONFIRMED** |
| PreviewPack = Format5 + **LZO** (not LCW) → `w·h·3` **RGB** | 267/267 decode exact; LCW fails 267/267; visual render | **CONFIRMED** (RGB order: visual, single map — spot-check more when wiring UI) |
| pkt MaxPlayers can under-state map waypoints (6 maps) | table cross-check | **CONFIRMED** |
| Theater data/palettes/INIs for all 6 theaters present locally | cache/local(+md) resolves | **CONFIRMED** (`.ubn/.des/.lun` render chain untested → PROBABLE) |
| Object-section CSV tails (facing/subcell/etc. beyond OWNER,TYPE,HP,X,Y) | OpenRA reader only | **PROBABLE** |
| Loose `.mpr/.yrm` dir enumeration; purpose of waypoints >7 (98/99/699); identity of 4 unnamed mix entries | — | **[RE-FALLBACK]** (recorded above) |

### Reproduce (PowerShell, tools already in the repo)

```powershell
# names file from the XCC DB: [u32 n]("name\0desc\0")*n repeated — see §0 note
./build/ra2extract.exe "D:\Red Alert 2\multimd.mix" --resolve "" $names   # entry -> filename
./build/ra2extract.exe "D:\Red Alert 2\langmd.mix" --raw missionsmd.pkt out.pkt
./build/ra2extract.exe "D:\Red Alert 2\multimd.mix" --raw xmp01t4.map out.map
# parse with the repo's own MapFile (scratch tool, links build/libra2assets.a):
#   g++ -I include mapdump.cpp -L build -lra2assets  → theater/size/cells/waypoints/preview
```

## §9 Adjudication (RE-GATE)

Pins the eight items §1–§8 left open, so the map-browser implementation depends
on measured values rather than recollection. Evidence is reproducible against
this install (`D:\Red Alert 2`) with `build/ra2extract.exe` plus a scratch probe
linked against the repo's own `libra2assets.a` and the prebuilt `ra2engine`
objects (`engine/iso_renderer.cpp` supplies `ra2engine::Theater`).

### §9.1 NewTheater art letters — OVERTURNS the pre-gate assumption

Probing every `g?cnst / n?cnst / g?refn / g?powr / g?weap` variant across all
theater art mixes returns exactly two hits per building:

```
gacnst.shp -> ra2.mix/snow.mix      ggcnst.shp -> ra2.mix/generic.mix
nacnst.shp -> ra2.mix/snow.mix      ngcnst.shp -> ra2.mix/generic.mix
garefn.shp -> ra2.mix/snow.mix      ggrefn.shp -> ra2.mix/generic.mix
gapowr.shp -> ra2.mix/snow.mix      ggpowr.shp -> ra2.mix/generic.mix
gaweap.shp -> ra2.mix/snow.mix      ggweap.shp -> ra2.mix/generic.mix
```

There is no `gtcnst` / `gucnst` / `gncnst` / `gdcnst` / `glcnst` anywhere.

**CONFIRMED:** on this install only SNOW has dedicated building art (letter
`A`, i.e. *arctic*). TEMPERATE, URBAN, NEWURBAN, DESERT and LUNAR all resolve
through the generic `G` art in `ra2.mix/generic.mix` — which is exactly what the
engine's existing `G`-fallback retry already does (`building-render.md` §1).

Consequence: the per-theater letter is near-irrelevant for art resolution. The
letters `N` / `D` / `L` stay in the table for completeness but are unexercised,
so they carry no risk and are recorded as `[RE-FALLBACK: unexercised]`.
`T` / `A` / `U` remain as already recovered.

### §9.2 Building-art mix names + parent archive — CONFIRMED

`temperat.mix` (237 entries), `snow.mix` (377), `urban.mix` (237) are nested in
**`ra2.mix`**; `urbann.mix` (241), `desert.mix` (241), `lunar.mix` (242) are
nested in **`ra2md.mix`**. The marker mixes `ubn.mix` / `des.mix` / `lun.mix`
(and `tem/sno/urb.mix`) hold 3 entries each and are NOT the art mixes — they
must not be used as such. No `*md` art variant exists except `snowmd.mix` (80).

### §9.3 Theater / unit / iso palettes — CONFIRMED

All 18 exist, in exactly two containers. `ra2.mix/cache.mix`:
`isotem/isosno/isourb.pal`, `temperat/snow/urban.pal`,
`unittem/unitsno/uniturb.pal`. `ra2md.mix/cachemd.mix`:
`isoubn/isodes/isolun.pal`, `urbann/desert/lunar.pal`,
`unitubn/unitdes/unitlun.pal`. Every pre-gate fallback name was correct.

### §9.4 Render chains — CONFIRMED for all five MP theaters (item had no fallback)

Constructing `ra2engine::Theater(iso_mix, control_ini, ext)` per theater and
resolving every distinct tile of a real map of that theater:

| Theater | control INI | tile mix that resolves | sample map | distinct tiles resolved |
|---|---|---|---|---:|
| TEMPERATE | `localmd.mix/temperatmd.ini` | `ra2.mix/isotemp.mix` | `xmp01t4.map` 105x95 | **270 / 270** |
| SNOW | `localmd.mix/snowmd.ini` | `ra2.mix/isosnow.mix` | `xmp31s2.map` 70x83 | **164 / 164** |
| URBAN | `localmd.mix/urbanmd.ini` | `ra2.mix/isourb.mix` | `xmp29u2.map` 60x60 | **116 / 116** |
| NEWURBAN | `localmd.mix/urbannmd.ini` | `ra2md.mix/isoubn.mix` | `fight.map` 81x52 | **154 / 155** |
| DESERT | `localmd.mix/desertmd.ini` | `ra2md.mix/isodes.mix` | `dunepatr.map` 60x52 | **103 / 103** |
| LUNAR | `localmd.mix/lunarmd.ini` | `ra2md.mix/isolun.mix` | *(no MP map exists)* | untested |

**All five multiplayer theaters are PLAYABLE.** Two findings that would have
broken a naive implementation:

1. **The `*md` tile mixes are OVERLAYS, not the tile set.** `isotemmd.mix`,
   `isosnomd.mix`, `isourbmd.mix`, `isoubnmd.mix` and `isodesmd.mix` each
   resolve **0** of the sample tiles, with `first-miss = Clear01.<ext>` at index
   0 — they do not even carry the clear tile. The **base** mix is authoritative;
   a loader that opens only the `md` variant renders an empty map. Search base
   first, then the `md` overlay.
2. **The control INI must be the `md` variant for every theater**, including
   TEMPERATE (`temperatmd.ini`, in `ra2md.mix/localmd.mix`) — the same file the
   engine already reaches through its `load_ini_from_ra2md` chain.

NEWURBAN's single miss is `stdmrb01.ubn` (template index 950) on `fight.map`:
one absent terrain template out of 155. Recorded as a known cosmetic gap —
NEWURBAN stays playable, and the renderer must tolerate an unresolved template
(draw nothing) rather than assume every index resolves. LUNAR has 0 MP maps
(§6), so it is excluded from the browser and its chain stays unverified.

### §9.5 Lobby-slot to start-waypoint binding — ADJUDICATED (not recovered)

Not addressed by the hunt and not recoverable from map data. We adopt
**deterministic ascending binding**: combatant slot *k* takes
`start_waypoints()[k]`. The original's randomized assignment is recorded as
`[RE-FALLBACK: randomized lobby-to-start assignment]` — deliberately not
reproduced, consistent with every prior phase's substitution of a deterministic
rule for the lockstep RNG.

### §9.6 Census re-verification — CONFIRMED, all four numbers re-measured

- **161** active `[MultiMaps]` entries in `missionsmd.pkt`.
- **161 / 161** exist in `multimd.mix` (which holds 173 entries — 12 unlisted).
  The existence filter therefore drops nothing on this install, but stays
  mandatory: RA2's `missions.pkt` lists 58 maps that do not exist (§2).
- **91** carry `GameMode=standard`.
- Gate map `xmp31s2.map` = SNOW, 70x83, 2 start waypoints, `GameMode=standard`,
  124,908 bytes — all four confirmed.

Start decoding re-confirmed on five maps across four theaters; the count of
`[Waypoints]` keys 0–7 equals `[Header] NumberStartingPoints` on every one:

```
xmp31s2  (SNOW)      starts=2  0=(54,53)  1=(94,111)
xmp01t4  (TEMPERATE) starts=4  0=(57,98)  1=(99,148) 2=(152,96) 3=(110,55)
fight    (NEWURBAN)  starts=2  0=(49,113) 1=(85,26)
dunepatr (DESERT)    starts=2  0=(34,79)  1=(79,34)
xmp29u2  (URBAN)     starts=2  0=(52,97)  1=(65,20)
```

### §9.7 CSF access path — CONFIRMED

`ra2md.csf` is a direct entry of top-level `langmd.mix` (id `BD835079`,
573,137 bytes), beside `missionsmd.pkt` (id `480B82E4`, 21,142 bytes). The
repo's `CsfStrings::load` parses it to **5,208 labels**, and `find` is
case-insensitive as required:

```
find("DESC:MP01DU") = find("desc:mp01du") = find("Desc:Mp01Du") = "South Pacific (2-4)"
find("DESC:MP31U2") = "May Day (2)"
find("DESC:MP02T2") = "Isle of War (2)"
find("NAME:MP01DU") = (null)
```

**`DESC:` is the correct and only prefix** — the `NAME:` form does not exist.

### §9.8 LocalSize — ADJUDICATED: IGNORED this phase

The full `[Map] Size` rect renders as today. `LocalSize` (the playable sub-rect
the original clips the camera to) is left for a later camera/bounds pass.

### §9 TheaterChain table (copy verbatim)

| name | letter | iso mix (base, authoritative) | iso mix (md overlay) | art mix | control INI | iso pal | theater pal | unit pal | ext | yr_only | playable |
|---|:--:|---|---|---|---|---|---|---|:--:|:--:|:--:|
| TEMPERATE | T | `ra2.mix/isotemp.mix` | `ra2md.mix/isotemmd.mix` | `ra2.mix/temperat.mix` | `ra2md.mix/localmd.mix/temperatmd.ini` | `ra2.mix/cache.mix/isotem.pal` | `ra2.mix/cache.mix/temperat.pal` | `ra2.mix/cache.mix/unittem.pal` | `.tem` | no | **yes** |
| SNOW | A | `ra2.mix/isosnow.mix` | `ra2md.mix/isosnomd.mix` | `ra2.mix/snow.mix` | `ra2md.mix/localmd.mix/snowmd.ini` | `ra2.mix/cache.mix/isosno.pal` | `ra2.mix/cache.mix/snow.pal` | `ra2.mix/cache.mix/unitsno.pal` | `.sno` | no | **yes** |
| URBAN | U | `ra2.mix/isourb.mix` | `ra2md.mix/isourbmd.mix` | `ra2.mix/urban.mix` | `ra2md.mix/localmd.mix/urbanmd.ini` | `ra2.mix/cache.mix/isourb.pal` | `ra2.mix/cache.mix/urban.pal` | `ra2.mix/cache.mix/uniturb.pal` | `.urb` | no | **yes** |
| NEWURBAN | N* | `ra2md.mix/isoubn.mix` | `ra2md.mix/isoubnmd.mix` | `ra2md.mix/urbann.mix` | `ra2md.mix/localmd.mix/urbannmd.ini` | `ra2md.mix/cachemd.mix/isoubn.pal` | `ra2md.mix/cachemd.mix/urbann.pal` | `ra2md.mix/cachemd.mix/unitubn.pal` | `.ubn` | yes | **yes** |
| DESERT | D* | `ra2md.mix/isodes.mix` | `ra2md.mix/isodesmd.mix` | `ra2md.mix/desert.mix` | `ra2md.mix/localmd.mix/desertmd.ini` | `ra2md.mix/cachemd.mix/isodes.pal` | `ra2md.mix/cachemd.mix/desert.pal` | `ra2md.mix/cachemd.mix/unitdes.pal` | `.des` | yes | **yes** |
| LUNAR | L* | `ra2md.mix/isolun.mix` | `ra2md.mix/isolunmd.mix` | `ra2md.mix/lunar.mix` | `ra2md.mix/localmd.mix/lunarmd.ini` | `ra2md.mix/cachemd.mix/isolun.pal` | `ra2md.mix/cachemd.mix/lunar.pal` | `ra2md.mix/cachemd.mix/unitlun.pal` | `.lun` | yes | **no** (0 MP maps) |

`*` = `[RE-FALLBACK: unexercised]` per §9.1 — no building art uses these letters
on this install; resolution falls through to `generic.mix`.

**TEMPERATE row check:** it must stay behaviourally identical to the literals
`main.cpp` hardcodes today, so the default path — and therefore all 18
byte-identity gates — is unchanged.
