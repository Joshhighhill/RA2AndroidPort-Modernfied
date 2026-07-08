# RA2 Engine Vertical Slice — Design (Phase 2)

**Status:** Approved (brainstorming) — 2026-07-08
**Builds on:** `ra2assets` asset-decoding library (Phase 1, complete) at `D:\ra2-android`.
**Supersedes nothing.** This is the next milestone after the asset pipeline.

---

## Goal

Produce the first thing that *looks and behaves like Red Alert 2* on an Android phone: a real stock map, rendered with real theater terrain tiles, with one real unit that the player can tap to select and tap to move — pathfinding around the terrain, walk-animated. The same code runs on desktop (for fast iteration) and ships as an installable `.apk`.

This is a **vertical slice**: it cuts thinly through every major engine subsystem (asset load → data model → rendering → input → simulation → platform/packaging) rather than building one subsystem deeply. The point is to prove the whole stack end-to-end and de-risk the hard integration seams early, while producing a visible, motivating result.

**One-sentence success test:** on both desktop and an Android device, a real stock temperate map renders with recognizable RA2 terrain, and a real infantry unit walks to where the player taps.

---

## Non-Goals (deliberately deferred to later milestones)

Buildings and their functionality; multiple or enemy units; combat/weapons; fog of war; resources/economy/harvesting; AI; audio; menus/HUD/sidebar; networking/multiplayer; save/load; player-color (house) remapping; cliffs, bridges, and overlay-object rendering beyond flat terrain; unit facings beyond what the walk animation frames provide; matching-grade simulation fidelity. If a feature is not required to "see a real map and command one unit on the phone," it is out of scope for this slice.

---

## Global Constraints

- **Two layers, one repo.** Everything extends `D:\ra2-android`. The existing `ra2assets` library **stays pure C++17 with no OS/SDL/third-party dependencies** (its current discipline is preserved). The new engine layer is the only code that links SDL2.
- **Platform layer = SDL2** (window, GL/renderer context, input, main loop, Android backend). All *game* code — asset decode, rules, map/tile decode, rendering logic, projection, pathfinding, simulation, the game loop — is ours. SDL2 only abstracts the OS.
- **Rendering = SDL2's hardware-accelerated 2D** (`SDL_Renderer` + `SDL_Texture`). No OpenGL ES shaders this milestone. Sprites/tiles are palettized to RGBA on the CPU (reusing `ra2assets` palettize) and uploaded as textures. A GLES path is a future performance milestone.
- **Desktop-first, then Android.** Develop and iterate on desktop; bring up the Android target and produce the APK once the slice works on desktop. Same native code both places.
- **No game assets, game binary, or GPL code committed.** Assets resolve at runtime through `RA2_INSTALL_DIR` (default `D:\Red Alert 2`), exactly as `ra2assets` already does. New parsers are original implementations verified against references + byte oracles, never GPL copies.
- **Target theater/asset set for the slice:** temperate theater (`temperat.mix`), one small stock skirmish map, one infantry unit (e.g. GI) whose SHP already decodes (proven in Phase 1). Rules come from `rulesmd.ini` (extracted from the MIX).
- **Same test discipline as Phase 1:** pure parsers get doctest unit tests and byte-for-byte oracle checks against XCC where applicable; asset-dependent tests skip gracefully without `RA2_INSTALL_DIR`.

---

## Architecture

### Layer 1 — `ra2assets` additions (pure library, no SDL/OS deps)

These extend the existing library and follow its established interface/testing style. Each is an independently testable unit.

1. **Map-pack decompressor** (`include/ra2assets/mappack.hpp`, `src/mappack.cpp`)
   - Decodes the chunked LCW/Format80-family compression RA2 uses for a map's `IsoMapPack5` (and, later, `OverlayPack`/`OverlayDataPack`): base64-concatenate the numbered INI keys, then walk the chunk headers (`compressed size`, `uncompressed size`) decompressing each chunk.
   - The exact chunk-header layout and the Format80/LCW decompressor variant are confirmed against the XCC/OpenRA references during planning and validated **byte-for-byte against XCC's `decode80`** (XCC `misc/shp_decode.cpp`, already cloned) plus the real map data — the same oracle method that validated the SHP decoder.
   - **Interface:** `std::vector<uint8_t> decompress_mappack(const uint8_t* data, size_t len);` (and a base64 helper).

2. **`MapFile`** (`include/ra2assets/map.hpp`, `src/map.cpp`)
   - Parses a real `.map`/`.mpr` (INI text) plus the decompressed `IsoMapPack5` into:
     - map dimensions and a **cell grid**: per cell → tile id, sub-tile index, height/level, and a passability flag derived from the tile's land type.
     - object/unit start entries relevant to the slice (at minimum, the cell + type of the unit we will control, or a chosen start cell).
   - Includes a minimal INI reader (sections/keys) — either shared with `Rules` (below) or a small internal one; planning decides whether to factor a shared `ini` unit.
   - **Interface:** `static MapFile open(std::vector<uint8_t> bytes);` exposing `width()`, `height()`, `const Cell& at(int x,int y)`, and the start entries.

3. **`Theater` / TMP decoder** (`include/ra2assets/theater.hpp`, `src/theater.cpp`)
   - Decodes the RA2 `.tmp` isometric-template tile format (from `temperat.mix`) into indexed tile images (each template has one or more sub-tiles with a known width/height and an index buffer), plus loads the theater palette (e.g. `isotem.pal`).
   - Validated against XCC's TMP handling / a byte oracle and by visual parity on the real map.
   - **Interface:** a `Theater` that, given a tile id + sub-tile, returns an indexed tile image and the theater `Palette`.

4. **`Rules`** (`include/ra2assets/rules.hpp`, `src/rules.cpp`)
   - Minimal INI-backed lookup over `rulesmd.ini` for the few fields the slice needs: a unit's art/image name (→ SHP) and movement speed. Not a full rules model — only what the slice consumes.
   - **Interface:** `Rules::load(bytes)` → `unit_image(name)`, `unit_speed(name)`, etc.

### Layer 2 — `engine/` (links `ra2assets` + SDL2)

New top-level directory; its own CMake target(s). Only this layer includes SDL headers.

5. **Platform** (`engine/platform.*`) — SDL2 init, window/renderer creation, event pump, frame timing, main loop. One code path for desktop and Android (SDL2 abstracts the difference).

6. **`TextureCache`** (`engine/texture_cache.*`) — turns an indexed image + `Palette` (via `ra2assets` palettize) into an `SDL_Texture`, cached by asset key. Index 0 stays transparent (as in Phase 1).

7. **`IsoRenderer`** (`engine/iso_renderer.*`) — isometric projection (cell (x,y,height) ↔ screen (px,py)), draws visible terrain cells in correct back-to-front iso order, then unit sprites at their interpolated positions; supports a scrolling camera. Projection math is pure and unit-tested (round-trip cell→screen→cell).

8. **`World` + simulation** (`engine/world.*`, `engine/pathfind.*`) — the cell grid + passability, one controllable unit (position, current path, animation frame), selection state, **A\*** pathfinding over the passability grid, and per-tick movement that advances the unit along its path and cycles the walk animation. Deterministic, framerate-independent stepping.

9. **`Input`** (`engine/input.*`) — maps SDL mouse (desktop) and touch (Android) events to game actions: tap-select the unit, tap-move to a destination, drag to scroll the camera.

### Layer 3 — `android/` (packaging)

10. **Android app shell** — a minimal SDL2 Android project (Gradle + a thin `SDLActivity` subclass + the native library built for `arm64-v8a` via the NDK, as proven in Phase 1). Launches straight into the scene. **This target produces the `.apk`.** No menus.

---

## Data Flow

1. **Load** (startup, via `ra2assets`): open `ra2.mix`/`ra2md.mix` from `RA2_INSTALL_DIR`; descend nested mixes; extract `rulesmd.ini`, the chosen `.map`, the temperate theater tiles + palette, and the unit's `.shp` + palette.
2. **Parse:** `Rules` → unit stats; map-pack decompress + `MapFile` → cell grid + unit start; `Theater` → indexed tiles; `ShpFile` (Phase 1) → unit frames.
3. **Upload (lazy):** `TextureCache` palettizes each tile/sprite to RGBA on first draw and caches the `SDL_Texture`.
4. **Frame loop:** pump input → step simulation (advance active path, animate) → render visible terrain (iso order) then the unit → present.
5. **Command:** tap destination → A\* over passability → unit walks the path, frame-animated.

After startup, no further game-file access; everything is in memory / cached textures.

---

## Error Handling

- Missing/short assets, unknown tile ids, unsupported compression or TMP variants → clear thrown errors with context (consistent with `ra2assets`), surfaced by the engine as a readable message rather than a crash.
- Asset-dependent tests skip gracefully when `RA2_INSTALL_DIR` is absent (asset-free CI still builds/links).
- Bounds validation on all new parsers (truncation, oversized counts, out-of-range cell/tile indices), matching the hardening standard Phase 1 settled on.

---

## Testing Strategy

- **Pure parsers** (`mappack`, `MapFile`, `Theater`/TMP, `Rules`): doctest unit tests on synthetic inputs + **byte-for-byte oracle** vs XCC (`decode80` for the map-pack; TMP tiles vs XCC's tile output) and against the real map/theater from the install. Same non-vacuous-gate discipline as Phase 1 (assert something was actually compared).
- **Engine logic that can be headless:** A\* pathfinding on known grids (path exists / blocked / shortest); iso projection round-trips; movement stepping determinism. Unit-tested with doctest.
- **Rendering / integration:** verified by running it — desktop screenshots of the real stock map (terrain + unit), then the APK launching on the device with tap-to-move working. This is the milestone's acceptance gate.

---

## Success Criteria

1. On **desktop**, a real stock temperate map renders with recognizable RA2 terrain tiles in correct isometric order, camera scrolls, and a real infantry unit stands at a sensible start cell.
2. Tapping the unit selects it; tapping a destination path-finds around impassable terrain and the unit walks there, walk-animated.
3. The same native code builds for `arm64-v8a` and installs as an **`.apk`** that launches into the scene and supports tap-to-select / tap-to-move on a physical device.
4. New pure parsers pass their unit + oracle tests; `ra2assets` remains dependency-free; no assets/GPL committed.

---

## Risks & Open Questions (to resolve during planning)

- **Map-pack compression variant:** confirm the exact `IsoMapPack5` chunk-header + Format80/LCW variant against XCC/OpenRA before implementing (oracle: XCC `decode80`). Highest-uncertainty parser.
- **TMP tile format details:** sub-tile layout, extra/height data, and how sub-tile index maps into the cell grid; confirm against XCC TMP handling.
- **Which stock map:** pick one small temperate skirmish map that decodes cleanly and has a clear passable area; identified during planning by listing candidates from the map mixes.
- **Unit placement:** if the chosen map has no obvious single-unit start, place the GI at a chosen passable cell rather than depending on a full house/spawn model.
- **SDL2 acquisition:** desktop via CMake (FetchContent or pinned prebuilt) and Android via SDL2's Android project template; pin versions, keep SDL out of the pure library target.
- **Passability source:** derive from tile land type (Rules `[General]`/land types) vs. a simpler per-tile heuristic; planning picks the smallest correct approach.

---

## References (read-only, already local under `D:\ra2-re\reference`)

- XCC Utilities (`xcc`): `misc/shp_decode.cpp` (`decode80` — map-pack oracle), TMP handling, theater/palette code.
- OpenRA: map pack (`Format5`/`Format80`), TS/RA2 tileset and map readers — for algorithm cross-checking (do not copy GPL source; reimplement).
- Phase 1 RE docs: `docs/re/` (MIX/PAL/SHP methodology, confidence ratings).
