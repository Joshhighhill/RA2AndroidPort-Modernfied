# Wave-5 presentation (agent D): recovered behaviour

Calibration RE against `gamemd.exe` (YR 1.001) for the W5-PRES branch. Private Ghidra
copies `D:\ra2-re\w5-pres*` (raw decompiles in `D:\ra2-re\w5-pres-re*-out\`).
Confidence: **High** = read in the decompile, **Medium** = read, one detail inferred,
**Low** = inferred. `[RE-FALLBACK]` marks clone stand-ins.

## 1. Layers and the high-bridge deck (R-100 / R-101)

- Render loop `0x6D8320` draws layers 0..4 in order (Underground, Surface, Ground, Air,
  Top); only Ground is sorted (insert `0x551720` + one bubble pass `0x5516C0` per frame),
  key `GetYSort` `0x5F63C0` = coords X+Y (buildings +0x20 / −0x10 by two type flags,
  `0x449360`). Air/Top keep insertion order. **High**.
- InWhichLayer: Fly loco `0x4CFC10` height > 0 → Top (aircraft are always Top in flight);
  Jumpjet `0x54B600` → Air while climbing, Top at cruise height; Drive/Hover/Ship → Ground.
- High-bridge decks are drawn in the terrain pass (`DrawOverlay 0x47F5B0`) at cell level
  + 4 with Z-write; objects blit with Z-read, so the Z-buffer hides what is under a deck
  and lets an OnBridge unit (object+0x8C, height + BridgeHeight) draw over it.
  BridgeHeight = 4 × LevelHeight = 416 leptons, a constant. **Medium-High**.
- Clone (no Z-buffer) `[RE-FALLBACK]`: decks join the ground sort at x+y+0.99; a ground
  unit on a deck cell over water draws 4 levels up and sorts at fx+fy+1.001; airborne
  units are drawn after the whole ground layer. Building-anim ZAdjust/YSort are still
  not separate sort items (they draw with their building).

## 2. Placement grid (R-102) — CellClass::DrawPlacement `0x47EBA0` (Tactical `0x6D45A0`)

- `PLACE.SHP` (shape global `0x89F36C`), centred at (cell.x + 30, cell.y − 15·height − 1),
  flags `0x20600` (centre + Z): **opaque**, no translucency. **High**.
- A foundation cell that passes the proximity (adjacency) check AND `IsClearToBuild`
  (`0x47C530`) draws **frame 0**; a blocked cell draws **frame 2 + the cell's ramp type**
  (`cell+0x11C`). Failing adjacency makes every cell blocked. **High**.
- Frame 1: the foundation of a placement already sent (network echo). **Low**.
- `0x6D4CA0/0x6D4F40/0x6D51C0` draw frame-0 range cells for some building / SW types
  (gap/sensor/SW range). **Medium**; not drawn by the clone.
- The building-image ghost is drawn only when global `0xA8DCD3` (editor/debug) is set:
  **vanilla has no ghost**. **Medium-High**.

## 3. Primary factory and rally point (R-144)

- Primary: TechnoClass::DrawExtraInfo `0x709EE0`. A building with IsPrimary (+0x3D3)
  shows CSF `TXT_PRIMARY` (short `TXT_PRI` for one type class) in a box: black fill,
  house-colour outline, house-colour text, style `0x149`, padding (4,2). Power plants
  show their power text in the same style. Shown for the selected object [Medium-Low].
- Rally point: `0x443860` only queues event 0x1E and speaks
  `EVA_NewRallyPointEstablished`; no rally-point draw was found (see §8).

## 4. House remap ramp (R-103) — ColorScheme `0x68C110`, ramp `0x68BDB0`

`[Colors]` (`0x66CDE0`) reads H,S,V bytes. Indices 16..31 of the unit palette are
**replaced** (the source ramp is ignored):

```
for i in 0..15:
  a = i*0.08144869842640204 + 0.3490658503988659     // 20deg + i*70/15
  if (i == 0) a = 0.19634954084936207                 // pi/16
  b = i*0.046542113386515455 + 0.8726646259971648     // 50deg + i*40/15
  S' = trunc(SIN(b) * S);  V' = trunc(COS(a) * V)
  pal[16+i] = HSV2RGB(H, S', V')
```

SIN/COS are a float table (`0x84E004`, `sinf(k*2pi/8192)`): `n = trunc(x*2607.594482421875f)`,
`idx = ((n>>1) + (n&1)) & 0x1FFF`, COS = idx + 0x800. HSV2RGB `0x517260` is integer:
`f=(H*6)%255, sec=H*6/255, p=(255-S)*V/255, q=(255-f*S/255)*V/255,
t=(255-(255-f)*S/255)*V/255`; sectors 0 (V,t,p) 1 (q,V,p) 2 (p,V,t) 3 (p,q,V)
4 (t,p,V) 5 (V,p,q), 6 as 0. **High**.

## 5. Lighting (R-104) — `0x689890`, cell `0x484590`/`0x484090`, convert `0x555D20`

- Cell intensity (per mille): `I = Ambient*10 + LightSources + Level*height - Ground`,
  clamped **0..2000** (light up to **2.0x** is allowed). A bridge deck cell uses height+4.
- Objects take the cell's I (which already has the height term) plus:
  buildings `ExtraLight`; units `+Level*4` on a bridge, `-500` under a bridge;
  infantry/aircraft `+(Z/(2*LevelHeight))*Level` in the air / on a bridge. **High**.
- LightConvert: 53 shades, shade k = k·2/52 (0..2.0x), channel `min(255, pal*mul>>16)`;
  intensity→shade `min(52, 127*L*52/32258)`, `L = min(254, I*0x105>>11)`. **High**.
- Clone: the screen-space light pass multiplies (MOD) up to 1.0 and brightens the excess
  (1.0..2.0) with a MUL pass; objects add their own height/bridge term with an additive
  re-draw (`ObjTint`, iso_renderer.hpp).

## 6. Turrets (R-121) — Set_ROT in `0x442bc0`, Mission_Attack `0x44acf0`, AI `0x735a60`

- Rate `min(ROT,127)<<8` u16 per frame (the sim's `kRotU16PerRotPerSec` already matches).
- A building turret tracks its target (also while reloading) and **never returns** to a
  default facing; only an unanswerable hit gives a random twitch (`0x442a10`, Medium).
- Vehicles return to the hull facing `GuardAreaTargetingDelay + 5` = 41 frames after the
  last shot (not ROF+5). TurretRotateSound loops while rotating, stopped when it ends
  (TechnoClass::AI `0x6F94D9`). **High**.
- Clone: `pres_turret.inc` draws the sim's slewed `Building::def_turret_facing`.

## 7. AnimClass keys (R-123) — reader `0x427c80`, AI `0x423ae0`, ctor `0x421ec0`

- `RandomRate=a,b`: 900/a, 900/b, then `if (max < min) min = max` (a clamp, no swap); the
  delay is `Random(min,max)` at the ctor and at each `Next=`. Every artmd entry has a<b, so
  it is a fixed 900/b. **High**.
- `Normalized=yes`: delay through `0x5FAAD0`; at GameSpeed 4 (15 Hz) d<5 unchanged,
  d>=5 → d*8/5. **High**.
- `Reverse`: starts at LoopEnd−1 stepping −1; a loop restarts at LoopEnd. **High**.
- `PingPong`: flips the step at End/0 (LoopEnd/Start with loops) before the end test, so
  it never expires. **Medium-High**.
- `Spawns`/`SpawnCount`: only for Bouncer/IsMeteor anims at a land impact:
  `Random(0,SC)+Random(0,SC)` Spawns anims. `IsMeteor` runs the bounce physics.
- `Damage`: accumulated per frame step, DamageArea with Rules FlameDamage2 (C4 for
  INVISO) — the clone keeps anim damage cosmetic (sim-side) [RE-FALLBACK, R-124].
- `MakeInfantry`: at expire, AnimToInfantry[idx] unlimboed for the owner. RA2/YR data has
  one user (BRUTE, YR-only), so nothing to draw for Allied/Soviet content.

## 8. Ore art on slopes (R-123 ore) — CellClass draw overlay `0x47f5b0`

```
t = Tiberiums[overlay]; s = cell ramp type; frame = OverlayData (0..11)
s == 0: ovl = t.Image + (X*Y) % 12
else    ovl = t.Image + 12 + (s-1)*2 + (X*Y) % 2     // ramps 1..4, 8 slope images
```
The variant is a hash of the cell coordinates, not the stored overlay; the frame is the
density. Image=1 TIB01, 2 GEM01 (no slope art), 3 TIB2_01, 4 TIB3_01. **High**.

## 9. Jump weapons (R-054)

DroneJump / BadTeeth are `LimboLaunch=yes` homing bullets (ROT 8, Speed 30) drawn with
DRONP / DOGP (32-facing, frame `(28 - idx32) & 31`, BulletClass Shape_Number
`0x467ff0`); the firer is limboed (not drawn) during the jump. SquidGrab is `Inviso`: only
the `SQDG_<dir>` weapon anim shows. **High** (flight details Medium).

## 10. Iron Curtain, flash and chrono warp (R-120)

- IronCurtainColor=0;4 reads ColorAdd index 0 (None): **no colour tint**. The visible
  effect is a brightness envelope (stage machine `0x70D9D0`, scale `0x70D7B0`):
  1.0→2.0x over 6 frames, 2.0x for 4, decay to 0.2x over ~20, 24-frame 0.2..0.5x pulses
  while ≥ 54 frames remain, 0.2x until < 31 remain, then 2.0x and back to 1.0x. **High**.
- Flash (`FlashData 0x4CC690`, `0x70C610`): while (counter & 2) brightness becomes 2000
  (500 if already > 1500); elite promotion sets EliteFlashTimer (150); veteran has no
  flash; trigger FlashTeam uses the same counter; buildings ±500 (`0x456F80`). **High**.
- Teleport (`0x718600`): WARPOUT at the old and the new location (WarpIn unused), the unit
  drawn 50 % translucent while IsWarpingIn / BeingWarpedOut (`0x705290`/`0x705AD0`),
  CHRONOSK every 24 frames at +(120,120) leptons. **High**.

## 11. Parachute (R-122) — SpawnParachuted `0x5F5150`, fall `0x5F3680`

PARACH is created at the paratrooper + 75 leptons Z and attached as owner-relative
(`0x424AD0`); FallRate steps by 1 to ParachuteMaxFallRate (−3); on touchdown the anim's
loop flag is cleared so its tail (LoopEnd..End) plays out. **High** (tail Medium).

## 12. Sidebar (R-142 / R-039)

- Credits: CreditClass::AI `0x4A2510` moves `clamp(|d|>>3, 1, 143)` per frame; colour by
  side (`FUN_0072E520`): Allied 164,210,255, Soviet/Yuri 255,255,0; CreditTicks[up?0:1] at
  0.5 volume per changed frame (audio owner). **High**.
- Tabs: StripClass::AI `0x6A81C0` flashes tabs 0/1 while a building is ready to place (or
  a SW tab per FlashSidebarTabFrames), toggling every 10 frames; flashing frame 3, 4 when
  selected (`0x69D540`). **High**.
- Cameo flash: only trigger action FlashCameo (`0x6E36C0` → `0x6ABC80`): DARKEN at 50 %
  while Frame < end and Frame % 16 > 8. There is no automatic new-cameo flash. **High**.
- Rally point (`0x6D9F40`): selected own factory → dashed line (5 on / 3 off, offset
  (INT_MAX−Frame)%15) to the rally cell: black at y+2, house colour at y+1 and y. **High**.

## 13. Jumpjets, aircraft and shadows

- Jumpjet loco (ILocomotion vtable `0x7EBD68`, movement `0x54CDA0`; the vtable
  `0x7E9CFC` aircraft.md calls Jumpjet is **Hover**): target height += sin(phase)×Deviation,
  phase += 2π/(15/Wobbles) per frame unless JumpjetNoWobbles (set on every RA2 jumpjet but
  SHAD). **High**.
- Fly Draw_Matrix `0x4CF530`: ±RollAngle (default 30°) while turning and PitchAngle when
  speed > PitchSpeed — only ASW/HORNET/BPLN/SPYP/PDPLANE qualify. Not drawn by the clone.
  Fly Draw_Point bob: `sin((Frame%20)·π/10)·1.5` px while hovering high. Not drawn.
- Airborne shadow: the unit's own silhouette straight below at constant size
  (`0x55A550`, `0x4144B0`, `0x755680`). Clone: the voxel shadow sprite at the ground.
