# ra2assets + ra2engine

A portable C++17 library that decodes Red Alert 2 / Yuri's Revenge game data
(`ra2assets`: MIX archives, PAL palettes, SHP sprites, VXL/HVA voxel models
(vehicle meshes + animation transforms, decode only), LZO/LCW map packs, INI,
rules, `.map` cell grids, TMP theater tiles, CSF string tables, BAG/IDX audio
decoded from Westwood IMA-ADPCM to PCM), plus `ra2engine` — an SDL2
front-end that renders a real stock map with real theater terrain and lets you
select and command a squad of units (RA2-classic controls: left-click / box /
Shift to select, right-click to group-move) with A\* pathfinding,
grid-occupancy collision avoidance, and walk animation — and fight: Allied
GIs vs Soviet Conscripts with house-colored palettes, HP bars, fire/death
animations, and a damage model (Damage × Verses, ROF, range, guard behavior)
recovered from the original `gamemd.exe` via Ghidra and driven by your real
`rulesmd.ini` (see `docs/re/combat.md`). Standing/walking/firing/dying frames
and all 8 facings are resolved from your real `artmd.ini` sequence data
(`ra2assets::Art`, see `docs/re/infantry-anim.md`), and group attack orders
fan attackers out to individual firing positions instead of queuing them
single-file. The fight is audible, too: weapon-fire sound effects, unit voice
acknowledgements (select/move/attack barks), death screams, and the EVA
"Unit lost" announcement, all driven by your real `soundmd.ini`/`evamd.ini`
event definitions and the `audio.bag`/`audio.idx` sample archive decoded by
the in-house IMA-ADPCM decoder (no XCC/third-party tooling) plus a from-scratch
WAV reader for the EVA clips. Playback semantics — linear distance
attenuation over the isometric screen-pixel Chebyshev metric, per-event
simultaneous-instance limits (approximated by reject-at-trigger; only
`Control=interrupt` steals a busy voice), uniform `Control=random` variant
selection, and the 5-level Priority scale — were recovered from
`gamemd.exe` via Ghidra (see `docs/re/audio-playback.md`, which also
records the deliberate divergences) and are mixed by a small dependency-free
`ra2engine::Mixer`, played out through SDL2 on desktop/Android. The library
stays pure and dependency-free; only the engine links SDL2.

Vehicles fight too, rendered as real voxels instead of sprites: the Rhino
tank's hull, turret, and barrel sections are decoded from your real VXL/HVA
meshes (`ra2assets::Vxl`/`Hva`) and rasterized by a from-scratch software
voxel rasterizer (`ra2engine::rasterize_model`, see
`docs/re/voxel-render.md`) into 32 pre-rendered facing sprites per section,
cached once at load (`build_facing_set`) and blitted per frame — the same
"cached facings" strategy the original uses — lit by the recovered normal
tables and diffuse/specular lighting against the unit's own `voxels.vpl`
(`ra2assets::Vpl`) and remapped through the house-colored unit palette.
Locomotion follows the recovered Drive model: hull and turret both turn in
place at the type's `ROT` degrees/frame with shortest-arc, exact-landing
`FacingClass` semantics (`step_turrets`), and each vehicle drives at its own
`Speed`-derived rate. Documented divergences from vanilla (see
`docs/re/voxel-render.md` §5, §7): no slope tilt (facings are cached flat,
so a vehicle on a ramp still renders level); no crush and no debris;
projectiles are instant-hit with single-target damage — vanilla shells fly
102 leptons/frame (not modeled; travel time is skipped) but `[AP]
CellSpread=.3` only ever reaches 76 leptons, in practice just the direct
target anyway, so the single-target damage half of this is exact even
though the flight-time half is a real simplification; the engine keeps its
already-established absolute speed scale of `0.75·Speed` cells/sec rather
than vanilla's `0.15·Speed` (the tank:GI speed ratio is vanilla-faithful
either way — a whole-engine time-base recalibration to the vanilla absolute
scale is a recorded future candidate, not done piecemeal here); the
turret's ROF+5-frame idle-return hold is omitted (it re-centers
immediately); and `MoveSound` (engine/track sound effects) is deferred.

Buildings round out the base game: an 8-building Allied+Soviet roster
(Construction Yard, Power Plant, Barracks-class infantry producer, and War
Factory, both houses — `GACNST/GAPOWR/GAPILE/GAWEAP` and
`NACNST/NAPOWR/NAHAND/NAWEAP`) renders from the same real SHP art pipeline as
units, resolved via the recovered
`NewTheater` letter-substitution + `'G'`-fallback rule (a `name[1]` munge
tested against `{g,n,c,y}×{a,t}`, then a hardcoded `'G'` retry on miss,
searched across every loaded mix, no per-art scoping — see
`docs/re/building-render.md` §1); multi-cell foundations occupy their full
W×H cell rectangle and block pathfinding/placement like any other obstacle.
The whole Construction Yard loop works end to end: a real RA2 sidebar
(credits readout, power bar, four tab buttons, and the cameo build-progress
clock drawn with the game's own `gclock2.shp` — one frame per production
step, 55 frames = idle + 54 steps, instead of a fill bar) drives a
54-step `FactoryClass`-faithful build economy at
`frames = trunc(Cost × BuildSpeed × 0.9)` (15 Hz, e.g. 504 frames for an
800-cost Power Plant), a placement cursor validated by the recovered
`Adjacent+1` expanded-rect proximity rule, buildup animations paced at
`BuildupTime × 900` ticks (rate = frames/image_frames, played once), and a
low-power build penalty clamped to `clamp(1 − modifier×(1 − output/drain),
.5, .8)`. Attack-capable buildings fight too, with turret-aim gating shared
with vehicles. A pre-built Soviet base exercises the Soviet roster and the
non-local-player placement-check bypass the original itself uses for
scripted placement, and `--demo-base` is a deterministic headless gate over
the whole loop. Calibration authorities:
`docs/re/building-render.md` (art/foundation/draw-order/damage-state/buildup/
palette semantics) and `docs/re/construction.md` (build-time math, power
model, placement validity, queue rules, sidebar metrics).

Documented divergences from vanilla: no idle `ActiveAnims`/production
animations; repair and sell are not implemented (their sidebar buttons are
inert); no rubble frames; no shroud; the radar is a static frame; sidebar
tabs beyond the Building tab are inert; the credits tally uses a built-in
digit strip rather than the game's own (unproven-for-this-string) font path;
the READY cameo state shows a flashing green tint (no in-game font this phase;
the divergent credits digit strip covers 0–9 only);
`GAWEAP` (Soviet War Factory) is unbuildable by the player this phase — its
`PROC` prerequisite is correctly enforced and unmet, exercised instead via
the pre-built Soviet base; units never auto-acquire buildings as combat
targets (explicit attack orders only). Plus, from the construction-phase
RE-gate: power output does **not** scale with building health (vanilla
scales output by hp ratio — `construction.md` §8 optional-fidelity item,
omitted here); `Bib=yes` bib art/row is omitted (`GAWEAP` renders fine
without its bib — `building-render.md` §2); tab buttons are drawn frame-0 +
a shade overlay rather than their full per-state frame set (the §11
`[RE-FALLBACK]`, since the frame-index-to-state mapping beyond frame 0 was
never fully pinned); and the main SHP's second damaged frame (frame 2, a
distinct debris/lighting variant) is unused, since it's only reachable
through the anim-stage machinery a static building never exercises.

The Economy phase closes the loop from raw terrain to spendable credits. Ore
and gems are decoded straight off the real map's `[OverlayPack]`/
`[OverlayDataPack]` (no synthetic placement) and classified into an
`ra2engine::OreField` by the recovered range-based overlay-index rule — an
overlay index belongs to a tiberium type when it falls in that type's
`[base, base+NumImages=12)` flat-image range or the following `NumSlopes=8`
slope range (`docs/re/economy.md` §5); the runtime index is the 0-based
order-of-appearance position in `[OverlayTypes]` (bases 27/102/127/147 for
GEM01/TIB01/TIB2_01/TIB3_01 on the real stock map), density is the map's
per-cell byte, 0..11. `HARV`/`CMIN` miners run the recovered harvester FSM
(`ra2engine::step_harvest`, `engine/harvest.cpp`): an 8-neighbour dig banks
bale value into cargo until full — the engine models a **2-kind** store
(`Unit::bales_ore`/`bales_gems`), collapsing the original binary's
**4-slot** per-tiberium-type store (one slot each for GEM/TIB/TIB2/TIB3,
`docs/re/economy.md` §1) into a single ore bucket — then the
miner drives (`HARV`) or Chrono-teleports (`CMIN`, a `TeleportLocomotionClass`
analogue bracketed by `ChronoInSound`/`ChronoOutSound`) to the refinery's
single dock, where a chunked per-tick payout transfers `Σ bales × Value` into
house credits. Refineries also spawn their `FreeUnit` (HARV/CMIN) at
grand-opening with its cost pre-billed into the refinery's own price
(`charge = Cost − FreeUnit.Cost`, e.g. GAREFN 2000 − 1400 = 600, while its
1260-frame build time still derives from the raw, un-subtracted 2000 —
`economy.md` §6a), and once built the refinery satisfies the `PROC`
prerequisite group, unlocking `GAWEAP`/`NAWEAP` (the War Factory, correctly
gated off in the Buildings phase). Ore itself is alive: a
`GrowthRate`-cadenced timer raises density toward the cap (11) on a slice of
eligible cells and spreads new tiberium onto adjacent empty, passable cells
from fully-grown sources. Calibration authority: `docs/re/economy.md`
(harvester FSM, dock/unload math, chrono locomotion, growth/spread, overlay
tables, FreeUnit cost split); companion of `docs/re/construction.md`.

Documented divergences from vanilla (Economy phase): ore growth/spread
candidate selection uses an engine-local **seeded xorshift32** RNG in place
of the original's lockstep-synchronised `Randomizer` — same rates
(`GrowthRate`, `Growth`/`Spread`, `*Percentage`, cap 11), not bit-identical
(the original's netgame-synchronised sequence is unreachable single-machine);
the `UnloadingClass` art swap (e.g. `HARV`→`HORV`, `CMIN`→`CMON`) while docked
is not implemented — miners keep their normal body during unload; the
refinery dock cell is adjudicated as the foundation's south-center cell
(`dock_cell_x`/`dock_cell_y`, `engine/harvest.hpp`) since GAREFN/NAREFN define
no `DockingOffset` in artmd — **[RE-FALLBACK]**; the per-gulp/unload/
chrono-warp/ore-scan pacing constants (`kGulpSecs`, `kUnloadSecsPerBale`,
`kChronoWarpSecs`, `kOreScanRadius`) are adjudicated, not recovered —
**[RE-FALLBACK]**; classification (`ore.cpp`'s `kBases`) models all four
overlay-index bases (27 GEM01, 102 TIB01, 127 TIB2_01, 147 TIB3_01 — all
`OreKind::Ore` except GEM), and the real stock map DOES reference index 127
(`economy.md` §5's own Task 2 dump observed it directly) — but only the
generic `TIB01..TIB20`/`GEM01..GEM12` art files are loaded (`load_ore_art`,
`engine/main.cpp`), so TIB2/TIB3 cells render through the shared
`TIB01..TIB20` set (whichever slot their in-block offset selects) instead of
dedicated `TIB2_*`/`TIB3_*` art, which is never loaded; `--demo-base`'s
roster witness now reports `roster=10` (GAREFN/NAREFN
appended to the buildable roster) — an intended content change, the gated
`hash=5273b5e71ca009e1` is unchanged; the ConYard `BaseUnit` cost-subtraction
rule recovered in `economy.md` §6a (Cost −= avg MCV cost when the type is
`BaseUnit[0].DeploysInto`) is not implemented, since Construction Yards are
never player-purchasable in this engine; and ore renders flat-cells-only —
the 8 `NumSlopes` slope frames per tiberium type are unused (no sloped-tile
ore rendering).

The Unit Production phase brings the sidebar's other three tabs to life
alongside Buildings (tab0). The player's Construction Yard now drives **four
concurrent production queues** (`ra2engine::ProductionQueues`), one per
sidebar tab: Structures (tab0), Defenses (tab1), Infantry (tab2), Vehicles
(tab3, sharing the slot with Ships/Aircraft, out of scope here). Structures
and Defenses are **separate** `FactoryClass` queues keyed on `BuildCat==Combat`
(not `IsBaseDefense` — GAWALL/GTGCAN are `BuildCat=Combat` but
`IsBaseDefense=None`, so the classifier can't be the flag), so a power plant
and a pillbox build **at the same time** from the same ConYard
(`docs/re/production.md` §1.1). The Allied defense roster (tab1) is GAPILL
(pillbox); the Allied unit roster (tab3/tab2) is GI (`E1`), Guardian GI
(`GGI`), Grizzly tank (`MTNK`), and IFV (`FV`) — whichever the loaded
`rulesmd.ini`/art resolve. Each unit queue reuses the identical 54-step
`FactoryClass` charge economy as buildings (`build_frames = trunc(Cost ×
BuildSpeed × 0.9)`), with **no FreeUnit subtraction for units** (charge ==
Cost, unlike refineries — `production.md` §1.5), and is gated by the matching
producer building (Barracks for infantry, War Factory for vehicles) via the
same prerequisite AND-mechanism buildings use.

A finished unit is emitted by the queue's **primary factory** — the first-built
building of that category by default, switchable with a second left-click on
the already-selected factory (right-click is reserved for setting rally, see
Deviations below). A second factory of the same category speeds up its class's
build time via the recovered **MultipleFactory = 0.8** compounding (one extra
same-class factory → ×0.8 build time; buildings share ONE ConYard count slot
across Structures+Defenses for this multiplier, `production.md` §1.1/§1.2).
On completion the unit is placed at its **exit cell** — an 8-ring first-free
walk outward from the factory's foundation anchor (`ra2engine::exit_cell`,
`production.md` §1.3) — then given a Move order to its **rally point**: the
cell set by the player, or by default the exit-adjacent gather cell nearest
the building's own coordinate (`FUN_00500020`, `production.md` §1.4).
Calibration authority: `docs/re/production.md` (queue model, primary +
MultipleFactory, exit cell, rally); companion of `docs/re/construction.md`
and `docs/re/economy.md`.

Deviations we take (adjudicated divergences from the recovered `production.md`
ground truth — not bugs, do not "fix" in review):
- **Queue depth is single-slot.** RA2 queues up to `MaximumQueuedObjects = 29`
  items per factory (`production.md` §1.5); this engine keeps one item
  building + one ready per queue — a deliberate simplification.
- **Exit facing is deterministic**, not the lockstep game RNG the original
  uses (`production.md` §1.3): south-first, then a fixed 8-ring walk order to
  the true nearest free perimeter cell. Same reachable set as the original,
  not bit-identical to its netgame RNG sequence. The `ExitCoord` sub-cell lean
  (+0xEC8) is also dropped at our cell-resolution grid.
- **Rally default = the exit-adjacent gather cell** (nearest free cell from
  the building's own coordinate, `FUN_00500020`), matching the recovered
  default — not the map center.
- **Primary-building auto-first-on-placement** (a newly placed factory becomes
  primary only if the house has none yet for its category) is the standard
  RA2 rule; the auto-first-primary-on-placement path itself was recovered
  **PROBABLE**, not pinned 1:1 in the disassembly.
- **Primary-set input trigger differs**: the original toggles primary with a
  right-click on the selected factory; this engine's right-click sets the
  rally point instead, so primary-set is a second **left-click** on the
  already-selected factory ([RE-FALLBACK adjudicated], Task 7).
- **GATOW (Patriot Missile System) is absent** from this asset install, so its
  SHP is dropped by the art guard; tab1 (Defenses) shows only GAPILL. This is
  an environmental gap in the install, not an engine limitation.
- **No leave-factory door animation.** The unit drives out under a Move order
  to its exit/rally cell; no per-building door-open/close animation frames are
  replicated.

## Building

Prerequisites: CMake 3.20+, MinGW-w64 gcc, Ninja, Python 3.13. The engine also
pulls SDL2 via CMake `FetchContent` (pinned `release-2.30.9`), built from source
on first configure.

Configure and build with CMake + Ninja:
```bash
cmake -S . -B build -G Ninja
cmake --build build
```

Run tests (PowerShell — the compiled test exe must be launched from PowerShell,
not Git Bash, on this Windows setup; set `RA2_INSTALL_DIR` to exercise the
real-asset tests, otherwise they skip cleanly):
```powershell
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
& .\build\ra2assets_tests.exe
```

## Engine (desktop)

`ra2engine` reads your own legally-owned game files at run time via
`RA2_INSTALL_DIR` (default `D:\Red Alert 2`); no assets are bundled or
committed. It loads a real temperate stock map (`mapsmd03.mix` entry
`0x133a0cb2`, 103×110), the temperate theater tiles/palette
(`ra2.mix → isotemp.mix`, `isotem.pal`), and the GI infantry
(`conquer.mix/gi.shp` + `cache.mix/unittem.pal`, resolved via `rulesmd.ini`
unit `E1`).

Build and run (PowerShell):
```powershell
cmake --build build --target ra2engine
$env:RA2_INSTALL_DIR = "D:\Red Alert 2"
& .\build\ra2engine.exe
```

Startup: MIX files are block-read (`ra2assets::read_file`) and `ra2.mix` is
opened once per run; cold start dropped from ~79 s to ~0.87 s on the host
(measured via `Measure-Command { ra2engine --screenshot }`). The same sources
build into the Android APK via `cmake/ra2_sources.cmake` — no jni changes.

Interactive controls: left-click a unit to select it (green marker); left-drag
a box to select many; Shift+left-click to add/remove one; double-click a unit
to select all of its type on screen; right-click ground to move the selection
(units fan out to distinct cells and route around each other); **right-click a
hostile Conscript to attack it** — units move into weapon range, hold, and
fire; idle units auto-acquire hostiles in range and return fire; pan with the
arrow keys or by pushing the pointer to a screen edge; Esc/close quits.

Headless/verification CLI modes (render one or more frames and exit):
- `ra2engine.exe --screenshot <out.png>` — render one frame of the map+unit to a PNG.
- `ra2engine.exe --demo-move <cellX> <cellY>` — select the unit, path to the cell, run the sim to arrival, write `task12_start.png`/`task12_end.png` to the scratchpad.
- `ra2engine.exe --demo-retarget <x1> <y1> <x2> <y2>` — order a move, retarget mid-walk, confirm arrival at the second target.
- `ra2engine.exe --demo-group <n> <cellX> <cellY>` — spawn `n` GI units, select
  all, group-move to the cell, run to arrival; writes `demo_group_start.png`/
  `demo_group_end.png` to the scratchpad and exits non-zero if any two units
  ever shared a cell.
- `ra2engine.exe --demo-combat [seed]` — 5 GIs vs 5 Conscripts fight to
  elimination with a deterministic player-micro policy; the sim runs twice
  (rendered + headless) and the per-hit damage logs must be byte-identical
  (combat is fully deterministic — the original's combat paths use no RNG,
  see `docs/re/combat.md`; the seed is accepted for interface stability and
  unused). Writes start/end screenshots to the scratchpad; exits non-zero on
  any overlap, dangling target, log divergence, or a bloodless battle.
- `ra2engine.exe --demo-audio` — headless-safe proof that the audio pipeline
  actually produces sound: loads `rulesmd.ini`/`soundmd.ini`/`evamd.ini` and
  the `audio.bag` archive, plays `GIAttack` and renders straight into a plain
  buffer bypassing SDL entirely (fails if every sample comes back zero); if a
  real audio device opens, it also plays an audible ~4 s scripted sequence
  (`GIAttack` → `GISelect` → `GIDie` → EVA "Unit lost") through it. Device
  absence is not a failure (CI/headless boxes have none — only check 1 must
  pass); prints a `[demo-audio] clips=<n> device=yes|no mix=nonzero` summary
  and exits 0.
- `ra2engine.exe --demo-tank` — the voxel-vehicle counterpart of
  `--demo-combat`: 5 GIs + 3 Rhino tanks per side fight to elimination; the
  sim runs twice (rendered + headless) and the per-hit damage logs must be
  byte-identical. Writes start/end screenshots to the scratchpad; exits
  non-zero on any overlap, dangling target, or log divergence.
- `ra2engine.exe --demo-base` — the Buildings & Construction integration
  gate: builds a Construction Yard → Power Plant → Barracks-class producer,
  confirms `GAWEAP` stays prerequisite-blocked (`PROC` unmet) against a
  pre-built Soviet base, and hashes the whole world state (FNV-1a 64-bit).
  Rendered and headless runs must produce identical witness lines and hash.
- `ra2engine.exe --demo-econ` — the Economy integration gate: places a
  refinery (charging the FreeUnit-subtracted cost, building at the raw-Cost
  frame count), watches its `CMIN` FreeUnit complete a chunked unload, watches
  an AI refinery's `HARV` unload for income, confirms `GAWEAP` unblocks once
  `PROC` is met, and crosses one ore-growth timer firing — then hashes the
  world state (ore field + credits included). Rendered and headless runs must
  be byte-identical.
- `ra2engine.exe --demo-prod` — the Unit Production integration gate: queues a
  Grizzly (`MTNK`) in the Vehicle queue and a GI (`E1`) in the Infantry queue
  concurrently from the same base, confirms the tank exits at its computed
  8-ring exit cell and drives to its rally, places a second War Factory,
  proves `set_primary` routes the next tank to the new primary and that its
  build time is exactly the recovered MultipleFactory ×0.8 of the
  single-factory build — then hashes the world + queue state (`prod_hash`).
  Rendered and headless runs must be byte-identical.

`ra2engine.exe` also accepts `--no-audio` (skips audio init entirely — same
silent fallback as a failed device open, just without touching the mix chain
at all) alongside the interactive run.

### Determinism gates (permanent)

These five headless demos are byte-identity regression gates: any change to
their printed output signals a defect in whatever change caused it, not an
intentional update (content changes that legitimately alter output, e.g. a
new roster entry, must be called out explicitly — see the Economy divergences
above for `--demo-base`'s `roster=10`, and note `--demo-base`'s roster is now
`roster=12` since the Unit Production phase's roster additions, hash
unaffected).

| Demo | Gate | Key witnesses |
|------|------|----------------|
| `--demo-combat` | `deterministic=yes logs=78` | `shots=73 deaths=5` |
| `--demo-tank` | `deterministic=yes logs=144` | `shots=135 deaths=9 hit90_rhino=yes hit22_infantry=yes` |
| `--demo-base` | `hash=5273b5e71ca009e1` | `roster=12`, GAPOWR `frames=504 step_ticks=9`, credits `9200→8700`, `GAWEAP blocked (PROC unmet)` |
| `--demo-econ` | `econ_hash=7fed96cb98b68017` | GAREFN `frames=1260 step_ticks=23 charge=600`; CMIN unload `+500` credits; enemy `income=1000`; `GAWEAP can_start=yes (PROC met)` |
| `--demo-prod` | `prod_hash=f0f4e1207ecd713c` | `deterministic=yes`; MTNK frames/charge, exit cell, concurrent GI, 2nd-factory primary routing |

## Android (arm64-v8a) cross-compile

This proves the library is portable: the same sources in `src/` build for
Android via the NDK's own CMake toolchain file, with no source changes.
There is no app and no on-device run here — artifact inspection only
(confirming `libra2assets.so` is a real AArch64 ELF shared object).

Prerequisites: Android NDK r27c for Windows
(https://dl.google.com/android/repository/android-ndk-r27c-windows.zip),
unzipped so that `$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake`
exists, e.g.:

```powershell
Invoke-WebRequest -Uri "https://dl.google.com/android/repository/android-ndk-r27c-windows.zip" -OutFile "D:\android-ndk-r27c-windows.zip"
Expand-Archive -Path "D:\android-ndk-r27c-windows.zip" -DestinationPath "D:\"
$env:ANDROID_NDK_HOME = "D:\android-ndk-r27c"
```

Configure and build the library targets (the test/CLI executables reference
`RA2_INSTALL_DIR` game-install paths meant for the host and aren't part of
this cross-build):

```powershell
cmake -S . -B build-android -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE="$env:ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" `
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 `
  -DRA2_BUILD_SHARED=ON
cmake --build build-android --target ra2assets ra2assets_so
```

`RA2_BUILD_SHARED=ON` additionally builds `ra2assets_so` (installed as
`libra2assets.so`), a shared target compiled from the same sources as the
static `ra2assets` library, so the resulting `.so` actually contains the
library's real code (verifiable with `llvm-nm`/`llvm-readelf`), proving the
sources are portable to Android/arm64 as an ELF shared object too, not just
as a static archive. Verify the result is AArch64:

```powershell
python -c "
import struct
with open('build-android/libra2assets.so','rb') as fh:
    m = fh.read(20)
assert m[:4] == b'\x7fELF'
machine = struct.unpack('<H', m[18:20])[0]
print('machine=0x%x' % machine, 'AArch64' if machine == 0xB7 else 'NOT-ARM64')
"
```

Expected output: `machine=0xb7 AArch64`.

## Android app (APK — arm64-v8a)

`android/` is a self-contained SDL2 Android project that packages the whole
engine (all of `engine/*.cpp` + the `ra2assets` library from `src/`) into an
installable **arm64-v8a** APK. `SDLActivity` loads `libSDL2.so` + `libmain.so`
and calls `SDL_main` (our `engine/main.cpp`), which on Android launches
straight into the interactive scene — tap to select the unit, tap to move
(A\* + walk animation), drag to pan. No menus.

The native `.so` list and the source lists come from the same
`cmake/ra2_sources.cmake` the desktop build uses, so the two cannot drift.
Game assets are **never** bundled (copyright); they are read at run time from
the app's external files dir (see "Assets on device" below).

### Prerequisites

- **Android SDK** (command-line tools) with `platforms;android-34`,
  `build-tools;34.0.0`, `platform-tools`, `cmake;3.22.1`. Install to e.g.
  `D:\android-sdk` and set `ANDROID_HOME`/`ANDROID_SDK_ROOT` to it.
- **NDK r27c** (e.g. `D:\android-ndk-r27c`).
- **JDK 17+** for Gradle (this project uses **JDK 21**). Gradle 8.9 + Android
  Gradle Plugin 8.5.2 are pinned; the bundled `gradlew` wrapper auto-downloads
  Gradle, so no system Gradle is needed.
- The desktop CMake configure must have run once so SDL2 (release-2.30.9) is
  present at `build/_deps/sdl2-src` (the jni build reuses it; override with
  `-DSDL2_SRC_DIR=` if it lives elsewhere).

Create `android/local.properties` (gitignored — machine-specific) pointing at
your SDK + NDK:

```properties
sdk.dir=D:/android-sdk
ndk.dir=D:/android-ndk-r27c
```

### Build the APK

```powershell
$env:JAVA_HOME = "C:\Program Files\Java\jdk-21"
$env:ANDROID_HOME = "D:\android-sdk"
Set-Location D:\ra2-android\android
.\gradlew.bat assembleDebug
```

Output: `android/app/build/outputs/apk/debug/app-debug.apk` (arm64-v8a only).
Verify it contains AArch64 native libs (no device needed):

```powershell
python - <<'PY'
import zipfile, struct
apk = r"android/app/build/outputs/apk/debug/app-debug.apk"
with zipfile.ZipFile(apk) as z:
    for name in ("lib/arm64-v8a/libmain.so", "lib/arm64-v8a/libSDL2.so"):
        m = z.read(name)[:20]
        machine = struct.unpack('<H', m[18:20])[0]
        print(name, "ELF" if m[:4] == b'\x7fELF' else "NOT-ELF",
              "machine=0x%x" % machine, "AArch64" if machine == 0xB7 else "NOT-ARM64")
PY
```

Expected: both `machine=0xb7 AArch64`.

### Running it (device or emulator)

The APK ships **both** `arm64-v8a` (real phones) and `x86_64` (emulator on an
x86 host / x86 Chromebooks). It has been run on an Android 11 (API 30) x86_64
emulator: it loads the real temperate map and GI unit and renders them
full-screen. Game files are **never** bundled (copyright) — they are read at
run time from the app's own external files dir.

1. **Install** the APK (on a real phone connect it with USB debugging on):
   ```powershell
   adb install -r android\app\build\outputs\apk\debug\app-debug.apk
   ```
2. **Place your own game files** where the app reads them. The engine reads the
   mix chain from `SDL_AndroidGetExternalStoragePath()/ra2/`, i.e.
   `/storage/emulated/0/Android/data/org.ra2.engine/files/ra2/`. Push at least
   `mapsmd03.mix`, `ra2.mix`, and `ra2md.mix` (the loader opens the
   theater/config/palette/unit mixes nested inside `ra2.mix`/`ra2md.mix`, so
   push the full set you own).

   **The files must be readable by the app** — this is the #1 cause of "it
   opens then instantly closes". `Android/data/<pkg>/` is scoped storage, so:
   - **Real phone:** copy the files in with a file manager / MTP, or
     `adb push` **as the normal (non-root) shell user** — both route through
     MediaProvider and give the files the correct SELinux label
     (`media_rw_data_file`) that the app is allowed to read.
   - **Do NOT `adb root` then push** into that folder: root-created files get
     the `storage_file` label and the app's sandbox is SELinux-denied `read`
     (you'll see `avc: denied { read } ... tcontext=...storage_file` in
     logcat). If you already did this, either re-push as non-root, or on an
     emulator only, work around it with `adb root; adb shell setenforce 0`.
   ```powershell
   # non-root push (correct label); create the dir first if needed
   adb shell mkdir -p /sdcard/Android/data/org.ra2.engine/files/ra2
   adb push "D:\Red Alert 2\mapsmd03.mix" /sdcard/Android/data/org.ra2.engine/files/ra2/
   adb push "D:\Red Alert 2\ra2.mix"      /sdcard/Android/data/org.ra2.engine/files/ra2/
   adb push "D:\Red Alert 2\ra2md.mix"    /sdcard/Android/data/org.ra2.engine/files/ra2/
   ```
3. **Launch** the app (`RA2 Engine`) from the launcher, or:
   ```powershell
   adb shell am start -n org.ra2.engine/.RA2Activity
   ```
   It opens into the real temperate map; tap-select and tap-move work with
   touch. Capture a device screenshot with `adb exec-out screencap -p > shot.png`.

   **Diagnosing failures:** the engine logs to Android logcat under the
   `SDL/APP` tag. On startup it prints `[assets] android asset dir = <path>`,
   and any fatal error is logged as `fatal: <reason>` (e.g.
   `cannot open file: .../ra2.mix`) before it exits — so
   `adb logcat -s SDL/APP:V` tells you exactly what went wrong instead of the
   app vanishing silently.

   **Note on x86_64 emulator:** it uses software GL (SwiftShader) and slow
   emulated storage, so first load takes ~1 minute; a real arm64 phone is far
   faster. The emulator confirms asset-loading + logic + layout, but final GPU
   behavior is only authoritative on a real device.

## Preflight Check

Verify the game install and binary before RE/oracle work:
```bash
RA2_INSTALL_DIR="D:\Red Alert 2" python scripts/preflight.py
```

The script checks that `gamemd.exe` exists and matches the expected SHA1 hash for YR 1.001.
