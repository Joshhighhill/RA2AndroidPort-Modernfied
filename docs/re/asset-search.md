# Asset search order, art `Palette=`, `InvisibleInGame`, building light sources (W3-FIX-A)

Recovered from RA2 `game.exe` (1.006) with Ghidra, private project `D:\ra2-re\w3fix-a`
(copy of `p4proj`), outputs `D:\ra2-re\w3fix-a-out\{mixorder,mixcallers,mixlist,mixiter,
bldini,objflags,ttype,lightsrc,lsclass,lsupd,lsarr,palconv,palload}`. Scripts: `strx.py`,
`fnat.py`, `callers.py`, `disp.py` in `D:\ra2-re\w3fix-a\scripts`.

## 1. One global MIX list, first loaded wins

* `MixFileClass` ctor `FUN_005952f0` links the new archive **after the current last
  node** of the global list at `0x00A70CC0` (`FirstNode` 0xA70CC4, `LastNode` 0xA70CD0;
  node `+4` Next, `+8` Prev; the ctor reads `LastNode.Prev` = 0xA70CD8 and inserts after
  it). Static init `FUN_00595190` / teardown `FUN_005951e0` confirm the layout.
* `MixFileClass::Offset` `FUN_00595aa0` hashes the name (`FUN_00493fc0`, the TS CRC) and
  walks from `FirstNode.Next` (0xA70CC8) forward; a binary search per archive; the
  **first** archive containing the id wins. `FUN_00595770` (find archive by name) walks the
  same way.
* So **load order = priority**. A nested archive (CACHE.MIX inside RA2.MIX) is opened
  through the same file lookup and joins the same list.

### Load order (game.exe)

| Step | Function (caller `FUN_005116a0`) | Archives, in order |
|---|---|---|
| Bootstrap | `FUN_00515c90` @0x5117A0 | `EXPAND%02d.MIX` for 99 down to 0 (each that exists), `RA2.MIX`, `CACHE.MIX`, `LOCAL.MIX` |
| Patch caches | `FUN_00515af0` @0x511F38 | `FindFirstFile("ECACHE*.MIX")` all, then `ELOCAL*.MIX` all |
| Secondary | `FUN_00515e70` @0x512234 | `CONQUER.MIX`, `CAMEO.MIX`, `MAPS*.MIX`, `MULTI.MIX`, `THEME.MIX`, `MOVIES*.MIX` |
| Theater | `FUN_00519e70` (Init_Theater; callers 0x57B805, 0x65F30D, 0x656C5D) | `GENERIC.MIX` (only when the new theater is **not SNOW** and the previous one was SNOW or none), `<root>.MIX` (`TEMPERAT`/`SNOW`/`URBAN`), `<ext>.MIX` (`TEM`/`SNO`/`URB`), `ISO<..>.MIX` (`ISOTEMP`/`ISOSNOW`/`ISOURB`), `ISOGEN.MIX`; then `%s.PAL` and `UNIT%s.PAL` |
| Scenario | `FUN_0051a2b0` | `NEUTRAL.MIX` |

YR `gamemd.exe` (`FUN_005301a0`, W3 audit 03) loads the md copy **before** each RA2
archive (CONQMD, GENERMD, GENERIC, ISOGENMD, ISOGEN, CONQUER, CAMEOMD, CAMEO, ...) and a
`%sMD.MIX` per theater name.

### What the clone does (`src/asset_fs.cpp`, `engine/asset_search.inc`)

`ra2assets::AssetFS` opens, in this order (missing archives are skipped):

1. `expand99..00.mix` (top level), `ra2.mix`, `ra2.mix/cache.mix`, `ra2.mix/local.mix`,
   `ecache*.mix`, `elocal*.mix`;
2. `ra2.mix/conquer.mix`;
3. `generic.mix` (not for SNOW), `<root>.mix`, `<ext>.mix`, `iso<..>.mix`, `isogen.mix`,
   `neutral.mix`;
4. **[RE-FALLBACK]** SNOW only: `generic.mix` at the end of the RA2 set. The original opens
   it only if a non-snow map was played earlier in the session; the clone keeps it last so a
   `g`-only name never disappears;
5. **[RE-FALLBACK] YR tail**: `ra2md.mix`, `expandmd0N.mix`, `cachemd`, `localmd`, `conqmd`,
   `genermd`, `isogenmd`, `cameomd`, `<root>md.mix`, `iso<..>md.mix`. RA2 `game.exe` never
   opens these. The clone needs them because it uses YR `artmd.ini` and offers YR lobby maps
   (`hillbtwn.map` places 55 civilian buildings whose art is only in `isourbmd`/`isogenmd`).
   Putting them **after** the RA2 set keeps RA2 art authoritative wherever both exist (the
   old `PresFx` searched `conqmd` first, so YR copies overrode RA2 anims).

The YR-only theaters (DESERT/NEWURBAN/LUNAR) keep the TEMPERATE list (W2-GA-09).

Name candidates are unchanged (building-render.md §1): `{g,n,c,y}{a,t}` names try
`[themed, 'g' generic]`; `Theater=yes` art tries `.<ext>` first. Every loader (building
roster, campaign and skirmish extras, garrison/depth extras, map dressing, building and
`[Animations]` anims, infantry and SHP vehicle bodies, voxels) resolves through one
`AssetFS` per theater.

## 2. Art `Palette=`

TechnoTypeClass art read `FUN_006daf60` @0x6DD96E reads `Palette=` (0x20 chars) into
`+0xB6C`; when non-empty, `+0xB8C = FUN_00603380(name)`: a named-palette registry that on a
miss calls `FUN_00604540(name)`, which builds the file name **name + theater extension**
(`PTR_0079abac + theater*0x1A`: `TEM`/`SNO`/`URB`) + `.PAL` and loads it. `Palette=lib` on
an urban map is `liburb.pal` (`ra2.mix/cache.mix`). The object draws through that converter
instead of the house unit converter, so it is **not house-remapped**.

## 3. `InvisibleInGame`

BuildingTypeClass `Read_INI` `FUN_00459a00` @0x45A678: `InvisibleInGame=yes` (`+0x1385`)
forces TechnoType `Invisible=yes` (`+0xAA6`) and `RadarVisible=no` (`+0xAA7`)
(TechnoType reader `FUN_006daf60`: `Invisible` @+0xAA6, `RadarVisible` @+0xAA7). The
object exists (cells, tag, light source) but is never drawn, has no radar blip and cannot
be picked with the mouse.

## 4. Building light sources

* Keys (`FUN_00459a00`): `LightVisibility` int leptons `+0xBC8`; `LightIntensity`,
  `LightRedTint`, `LightGreenTint`, `LightBlueTint` read as doubles and stored ×1000
  (`+0xBCC`, `+0xBD0`, `+0xBD4`, `+0xBD8`).
* `BuildingClass::Unlimbo` `FUN_004426d0` @0x442DD9: when `LightIntensity != 0`, creates
  `LightSourceClass(center coord, vis, intensity, r, g, b)` (`FUN_00537240`; fields `+0x24`
  intensity, `+0x28/+0x2C/+0x30` tint, `+0x38/+0x3C/+0x40` x/y/z, `+0x44` visibility,
  `+0x34` required detail level 2, `+0x48` active) and activates it (`FUN_00537540` →
  `FUN_005375d0`, which marks every cell centre within `vis` leptons for re-lighting).
* Cell lighting `FUN_00479440` (RA2's version of gamemd `FUN_00484090`), per cell, cell
  centre `(x*256+128, y*256+128)`:

  ```
  I     = Ambient*1000                      (scenario [Lighting], stored x100)
  R,G,B = Red/Green/Blue*1000
  for each ACTIVE light source with detail <= current detail:
      d = (int) sqrt(dx*dx + dy*dy)         (2-D, leptons; z ignored)
      if d <= vis:
          f  = (vis*1000 - d*1000) / vis    (unsigned per-mille, linear falloff)
          I += intensity * f / 1000
          R += r * f / 1000;  G += g * f / 1000;  B += b * f / 1000
  I += Level*height - Ground                (normal / ion-storm / dominator sets)
  I  = min(I, 2000); tint normalised (FUN_005383c0); negatives clamp to 0
  ```

  A negative `LightIntensity` (`NEGLAMP`) darkens.

The clone (`engine/pres_light.inc`) evaluates exactly this per cell for every live
building with `LightIntensity != 0` and multiplies the playfield by the per-cell result
(vertex-interpolated across cell corners). [RE-FALLBACK] Objects take the light of the
ground under each screen pixel, not their own cell, and values above 1.0 clamp (the
original's LightConvert can brighten up to 2.0).
