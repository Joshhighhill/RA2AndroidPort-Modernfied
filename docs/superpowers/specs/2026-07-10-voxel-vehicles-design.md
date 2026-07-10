# Voxel Vehicles — Rhino tank as a full unit

**Date:** 2026-07-10 · **Status:** approved (design sections 1–4 approved in session)
**Calibration authority:** `docs/re/vxl.md` (format, complete) + `docs/re/voxel-render.md`
(NEW — Task 1 of the plan produces it; rendering + locomotion semantics recovered
from gamemd.exe, per the standing calibrate-first directive and the user's explicit
"use ghidra" instruction for this phase)

## Goal

The Rhino heavy tank ([HTNK]) as a complete unit on BOTH sides: hull + turret +
barrel rendered from the real voxel assets (decoder bit-exact since Phase 2.5),
driven with the original's turning behavior (ROT), fighting with the original
120mm math through the existing combat engine, house-remapped blue and red.

## Scope decisions (approved)

- **Full unit** (render + drive + fight), not render-only.
- **Both houses** field Rhinos (player blue alongside GIs, enemy red with
  Conscripts) — combined-arms battles, remap exercised both ways.
- **Renderer: cached facings.** A pure voxel rasterizer implements the
  Ghidra-recovered projection/lighting math, but runs once at load per
  quantized facing (per part, per house), cached as textures. Runtime cost =
  sprite blit, same as infantry; safe on Android. Divergence: no slope-tilt
  rendering (IsTilter) — documented; the slice doesn't drive on slopes.

## Ground truth already extracted (this session, from the real install)

- rulesmd.ini `[HTNK]`: Strength=400, Armor=heavy, Speed=6, ROT=5, Turret=yes,
  IsTilter=yes, Crusher=yes, Primary=120mm, Locomotor={4A582741-…} (Drive),
  VoiceSelect=GenSovVehicleSelect, VoiceMove=GenSovVehicleMove,
  VoiceAttack=GenSovVehicleAttackCommand, DieSound=GenVehicleDie,
  MoveSound=RhinoTankMoveStart, Explosion=TWLT070,S_BANG48,S_BRNL58,S_CLSN58,S_TUMU60,
  Maxdebris=3.
- rulesmd.ini `[120mm]`: Damage=90, ROF=65, Range=5.75, Projectile=Cannon,
  Speed=40, Warhead=AP, Report=RhinoTankAttack, Anim=GUNFIRE.
- rulesmd.ini `[AP]`: Verses=25%,25%,15%,75%,100%,100%,65%,45%,60%,60%,100%
  (no 0% rows — the dormant 0%-verses carry-forward stays dormant),
  CellSpread=.3, PercentAtMax=.5.
- artmd.ini `[HTNK]`: Voxel=yes, Remapable=yes, PrimaryFireFLH=150,0,100.
- Assets confirmed present in `ra2.mix → local.mix`: `htnk.vxl` (34,550 B,
  1 section 46×34×12, normals_mode=2 TS-style), `htnk.hva`, `htnktur.vxl`
  (11,694 B), `htnktur.hva`, `htnkbarl.vxl` (1,907 B), `htnkbarl.hva` — hull +
  turret + barrel, three parts, each with its own 88-byte 1-frame HVA. `voxels.vpl` (8,976 B = 16-byte header + 768-byte
  palette + exactly 32×256 lookup bytes — consistent with a 32-level lighting
  remap; layout to be confirmed by the hunt). `unittem.pal` is NOT in
  local.mix — the plan locates it (likely cache/isotemp chain).

## Task 1 — Ghidra rendering/locomotion hunt (BEFORE any implementation)

Content-anchored (known anchors: the `voxels.vpl` filename string, the
`DPOD.VXL`/`DPOD.HVA` bootstrap `FUN_005313c0` and its caller `FUN_0052b790`,
loaders `FUN_00754bd0`/`FUN_005bcff0`, and the untraced tailer-matrix helper
`FUN_005ae010` — the likely door into the renderer). Hardcoded YRpp/Ares
addresses remain untrusted. Recover into `docs/re/voxel-render.md` with a
confidence table:

1. **Projection:** voxel space → screen (camera transform and scale — how one
   voxel maps to pixels at our 30×15 half-cell iso metric) and how section
   `scale`, tailer `transform[3][4]`, and HVA matrices compose.
2. **Facing quantization:** how many cached orientations for a vehicle body
   and for a turret (believed 32 vs infantry's 8) and the facing → rotation
   angle mapping.
3. **Lighting:** normal-table contents for `normals_mode=2` (the Rhino's) and
   how a voxel's normal index selects a lighting level; **voxels.vpl
   semantics** (header fields, which of the 32 tables applies at which light
   level, which base palette the result indexes); where `unittem.pal` (or the
   actual unit palette) loads from.
4. **House remap** for voxels: same 16–31 index scheme as SHP, or the vpl
   remap_start/remap_end range.
5. **Locomotion (Drive):** `Speed=6` → real cells/sec conversion; `ROT=5`
   semantics (facing-change rate in the original's 16-bit facing space,
   per-frame step); whether the hull turns in place before driving; turret
   tracking rate toward target and return-to-body when idle.
6. **Fire point:** `PrimaryFireFLH=150,0,100` (forward/lateral/height,
   leptons) → muzzle flash / Report origin.
7. **Combat deltas:** projectile Speed=40 travel time and AP CellSpread=.3 /
   PercentAtMax=.5 splash semantics — recovered and documented; whether we
   IMPLEMENT them this phase is decided at the plan amendment gate (a 0.3-cell
   spread may be target-cell-only in practice).

Anything unrecoverable gets an `[RE-FALLBACK]` tag (OpenRA/ModEnc, read
off-repo, prose only) and the plan-amendment gate runs before dependent tasks
dispatch — same mechanism as the audio phase.

## Architecture (approved: pure rasterizer + load-time facing cache)

1. **`ra2assets::Vpl`** (`include/ra2assets/vpl.hpp`, `src/vpl.cpp`) —
   voxels.vpl loader: header, embedded 768-byte palette, the N (from header,
   expected 32) 256-entry lookup tables. Pure; throws std::runtime_error with
   context on malformed input; layout per the hunt. Shared via
   `cmake/ra2_sources.cmake`.
2. **`engine/voxel_raster.{hpp,cpp}`** — pure rasterizer (no SDL): (VxlSection
   + HVA/tailer transforms + rotation angle + Vpl + base palette + house
   remap) → small RGBA sprite + anchor point (ground-center registration).
   Implements the recovered projection and normal→vpl lighting; back-to-front
   voxel traversal for self-occlusion. All recovered math lives here;
   headless-testable.
3. **`engine/facing_cache.{hpp,cpp}`** — builds at load the N-facing sprite
   sets per part (hull/turret/barrel) × house remap from the rasterizer.
   Pure struct building; main.cpp uploads RGBA → SDL textures (texture_cache
   pattern).
4. **Vehicle state in `world.hpp`/`world.cpp`** — `Unit` gains `is_vehicle`,
   continuous `body_facing`/`turret_facing` (the original's facing units per
   the hunt), and turn rate. `step_units`: vehicles turn at ROT toward the
   path segment before/while advancing (exact gating per the hunt) — no
   sideways sliding. Combat step slews the turret toward the target at the
   recovered rate; returns to body facing when disengaged. Infantry path
   untouched (8-way snap as today).
5. **`main.cpp` wiring** — load 3 voxel parts + vpl + unit palette through
   the established mix chains; build facing caches for houses 0/1; spawn
   Rhinos both sides via the existing `UnitCombat` flow; draw hull → turret →
   barrel at the unit's position inside the existing painter's order
   (`unit_draw_order` unchanged); fire flash/Report anchored per FLH.

Combat reuses the existing engine verbatim (damage=trunc(Damage×Verses%),
armor heavy=index into the 11-row verses, ROF in 15 Hz frames).

## Gameplay integration (approved)

- **Selection/orders:** tanks are `Unit`s — box/click/Shift select, formation
  move, right-click attack all work unchanged. Hit-testing by cell; HP bar +
  selection box sized from cached sprite bounds.
- **Audio:** zero new audio code — register the [HTNK] voice/report keys
  per-image through the existing AudioSystem (GenSovVehicleSelect/Move/
  AttackCommand, GenVehicleDie, RhinoTankAttack).
- **Death:** vehicles have no die animation — play the first Explosion= anim
  (TWLT070, a one-shot facing-independent SHP) at the tank's position, then
  tombstone. Debris (Maxdebris) out of scope.
- **Crusher:** OUT — tanks respect one-unit-per-cell like everyone else this
  phase. Documented divergence.
- **Demo:** new `--demo-tank` (blue GIs+Rhinos vs red Conscripts+Rhinos,
  scripted, deterministic, headless-capable) is the regression gate.
  `--demo-combat` byte-identity must remain untouched (no tanks spawn there).

## Documented divergences (RE doc + README, like the audio list)

No slope tilt (IsTilter) — cached flat facings; no crush; no debris; projectile
travel/splash per the hunt's findings (if hitscan is kept, state the ~0.5 s
shell-flight divergence at Range 5.75); MoveSound=RhinoTankMoveStart deferred
to the ambient/loop audio phase.

## Error handling

Every new asset load (`htnk*.vxl/.hva`, `voxels.vpl`, unit palette) is
individually guarded: log once, degrade to infantry-only (tanks simply don't
spawn), never crash. Malformed-file throws are caught at the load boundary in
main.cpp like the audio chain.

## Testing (approved)

- **Vpl loader:** synthetic buffers (header parse, table count, truncation
  throws) + install-guarded pins on the real voxels.vpl (size arithmetic
  16+768+32×256; first/last table bytes pinned once verified vs the hunt).
- **Rasterizer (core gate, pure):** hand-built tiny models (single voxel,
  2×1×1) with identity transforms → pinned RGBA at facing 0 (exact pixel
  positions prove the projection); rotation tests (off-axis voxel at facing N
  vs N+quarter-turn lands where hand-computed); lighting tests (two voxels
  differing only in normal index select the hand-computed vpl row); house
  remap test (remap indices shift, others don't). Install-guarded: real htnk
  hull renders non-empty, in-bounds, byte-deterministic across two runs, with
  a pinned content hash (re-pinned only with visual re-verification against
  the original game).
- **Facing cache:** N facings × 3 parts × 2 houses all non-empty; facing i's
  sprite equals a direct rasterizer call at facing i's angle (no drift).
- **Locomotion (pure):** ROT shortest-arc turn steps hand-computed; clamp at
  target facing; hull-turn-before-move gating per the recovered rule; turret
  slew + return-to-body; Speed conversion pinned to the recovered constant
  (tank vs infantry speed ratio asserted).
- **Combat:** 120mm vs GI (none armor) trunc(90×25%)=22 per hit; vs Rhino
  (heavy) trunc(90×100%)=90 → 5 shots kill 400 HP, ROF=65 → 4.33 s cadence;
  hand-computed kill time asserted in a scripted micro. Existing combat suite
  untouched and green.
- **Integration gates:** `--demo-combat` byte-identity unchanged; `--demo-tank`
  deterministic (rendered vs headless identical) and decisive; suite green
  with AND without `RA2_INSTALL_DIR` (clean skips); arm64 NDK compiles all new
  sources into `libmain.so`.

## Constraints (standing, binding)

GPL-clean (XCC/OpenRA/YRpp/ModEnc read off-repo, prose only); no game assets
or Ghidra project committed (RE docs = prose + short excerpts); tests skip
cleanly without `RA2_INSTALL_DIR` (default `D:\Red Alert 2`); pure modules
shared via `cmake/ra2_sources.cmake`; test exes from PowerShell; work on
`master`, frequent commits; commit trailer
`Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`.
