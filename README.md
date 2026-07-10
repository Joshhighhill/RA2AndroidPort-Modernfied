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
simultaneous-instance limits enforced by voice-stealing, `Control=random`
variant selection over Attack/body/Decay sample partitions, and the 5-level
Priority scale — were recovered from `gamemd.exe` via Ghidra (see
`docs/re/audio-playback.md`) and are mixed by a small dependency-free
`ra2engine::Mixer`, played out through SDL2 on desktop/Android. The library
stays pure and dependency-free; only the engine links SDL2.

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

`ra2engine.exe` also accepts `--no-audio` (skips audio init entirely — same
silent fallback as a failed device open, just without touching the mix chain
at all) alongside the interactive run.

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
