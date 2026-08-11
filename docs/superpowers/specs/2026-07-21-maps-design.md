# Map Browser / Multi-Map Skirmish — Design Spec

**Status:** DRAFT (awaiting user review)
**Roadmap slot:** `docs/superpowers/ROADMAP-completion.md` **P3 — "Map browser / multi-map"** ("One hardcoded map today. Enumerate + parse the stock map set so skirmish is replayable." Gate: `--demo-maps`).
**Depends on:** the Skirmish Shell phase (`docs/superpowers/specs/2026-07-20-skirmish-design.md` + `engine/skirmish.{hpp,cpp}` — provides `SkirmishSlot`/`SkirmishConfig`/`default_skirmish_config`/`configure_skirmish_houses`, `launch_skirmish` (`main.cpp:9431`), the `Scene`/`run_scene_driver`/`GameScene`/`ShellScene`/`SkirmishSetupScene` framework, and the `--demo-skirmish` gate template) + the existing decoders (`ra2assets::Ini`, `MixFile`, `CsfStrings`, `base64_decode`, `decompress_mappack`, `MapFile`, `TmpFile`/`ra2engine::Theater`).
**Calibration authority:** `docs/re/maps.md` (the map-set enumeration + format hunt — **ALREADY COMPLETE**). Every constant, filter, decode rule and file name in this spec cites it by §. Where maps.md is `[RE-FALLBACK]` or `PROBABLE`, this spec carries the label forward and names what Task 1 adjudicates instead.

## Goal

Today the engine loads exactly **one** map, forever: `engine/main.cpp::load_assets` (`main.cpp:143`) opens `mapsmd03.mix`, reads entry `0x133a0cb2`, and throws unless the theater is `TEMPERATE`. The completed hunt established two facts that reframe the phase:

1. That hardcoded map is **not a multiplayer map at all** — `0x133A0CB2` resolves to **`sov07tmd.map`**, YR Soviet campaign mission 7, 367,021 bytes, 103x110, TEMPERATE (maps.md §1, CONFIRMED by hashing the name against the archive index). It parses because campaign and MP maps share `[Map]`/`[IsoMapPack5]`, but it has campaign houses, embedded rules overrides, and **no `[Header]`/`[Preview]`/`[PreviewPack]`**.
2. The install carries **161 playable YR multiplayer maps**, enumerated not by scanning the archive but through a **`.pkt` manifest** — `missionsmd.pkt` in `langmd.mix` (maps.md §2, CONFIRMED). Of those, **91 carry `GameMode=standard`**, which `mpmodesmd.ini` identifies as the *Battle* mode — *"the only one that allows AI players"*, i.e. exactly the skirmish list.

This phase makes skirmish load **real multiplayer maps chosen from a list**. It delivers:

- **A `.pkt` manifest decoder** in `ra2assets` (pure): `[MultiMaps]` index + per-map `Description`/`GameMode`/`MinPlayers`/`MaxPlayers`.
- **A map catalog** in `engine/` (pure, no SDL): pkt → **mandatory existence filter** against `multimd.mix` → CSF display names from `ra2md.csf` → `GameMode=standard` filter → a stable, sorted `std::vector<MapEntry>`.
- **Generic map load by name with variable dimensions**: `load_assets` gains a defaulted `MapSelection` parameter; the default value is byte-for-byte the current `mapsmd03.mix`/`0x133a0cb2` path.
- **Non-temperate theater loading**: a pinned per-theater chain table (tile mix / control INI / iso palette / tile extension / NewTheater art letter / art mix / theater+unit palettes) driving `load_assets`, `load_building_assets` and `load_ore_art` — with `TEMPERATE` as the default so every existing candidate walk stays in the identical order.
- **Real per-house start placement from `[Waypoints]` 0–7** (`x = v % 1000`, `y = v / 1000`, maps.md §4.3 CONFIRMED) — replacing the synthetic `pick_start_cell() + i*14` fan-out and the synthetic `make_ai_waypoints` ring **on the INTERACTIVE path only**, behind an opt-in `SkirmishConfig` flag that defaults to today's behavior.
- **A selectable map list** in `SkirmishSetupScene`, replacing the fixed `MAP: MAPSMD03` label.
- **A 19th byte-identity gate `--demo-maps`.**

**Byte-identity promise.** Every one of the 18 existing gates builds its world **directly on the default map** through `load_assets(dir, ra2)` → `build_world` → `spawn_base`/`pick_start_cell`/`make_ai_waypoints`, and none of them calls the catalog, the pkt parser, the map list UI, or the new start-placement path. All new behavior is reached only through **defaulted parameters whose default values reproduce the current code exactly** and **new `SkirmishConfig` fields that default to `false`/empty**. maps.md §7's five rules are the design's spine and are restated per component below.

## Global Constraints

These bind every task in the plan.

- **All 18 existing byte-identity gates MUST stay bit-identical:** combat logs=78, tank logs=144, base `5273b5e71ca009e1`, econ `7fed96cb98b68017`, prod `f0f4e1207ecd713c`, ai-base `cd1f7c6a29933f0a`, ai-team `312e92ed862a5e74`, ai-trigger `8334a6ca1531cabc`, transport `fe500f946e913645`, aircraft `a69642564cf64773`, naval `6aee2c82bc024e86`, superweapon `189496664f5eca68`, techtree `e73adae61ff6f71f`, capture `58b87b5bc63f7046`, shroud `d302852aa9517661`, multihouse `39412ae996cce8ae`, skirmish `e9ad3f1fd88495f9`, roster `333cd43d8b5ece6a`. This phase adds the **19th: `--demo-maps`**.
- **maps.md §7 firewall rules (verbatim, non-negotiable):**
  1. **Do not change `MapFile::parse` semantics** — waypoints/preview/name are NEW fields; cell materialization (0xFFFF→Clear), record order and overlay handling are untouched.
  2. **Do not repoint the default map** — `load_assets`' `mapsmd03.mix`/`0x133a0cb2` path stays the gates' map; map choice is a **parameter that DEFAULTS to current behavior**.
  3. **`pick_start_cell`/`make_ai_waypoints` must keep their outputs on the gate paths** — real `[Waypoints]` data is routed through new `SkirmishConfig` fields / defaulted parameters, so every gate call-site is untouched.
  4. **Theater generalization must keep the temperate candidate-walk order byte-identical** — `load_building_assets`/`load_ore_art` bake `'T'` + `.tem` + `isotemp.mix`/`temperat.mix`; the generalized versions must emit the *same list in the same order* for TEMPERATE.
  5. **Do not slurp new mixes unconditionally on the gate path** — `multimd.mix`/`langmd.mix` (pkt + CSF) and `ra2md.mix`'s YR theater chain are opened only when actually needed (startup-perf discipline, `2026-07-09-startup-perf`).
- **`world_hash` untouched.** It folds only `Unit{cell_x,cell_y,hp,alive}` + `Building{type,cx,cy,hp,state}` and is not modified.
- **Calibrate-first, no empirical guessing.** Every constant cites `maps.md` by §. Items maps.md left `PROBABLE` or absent are pinned by the **Task 1 RE-GATE** *before* code depends on them; anything that stays unpinned keeps its `[RE-FALLBACK]` label in the code comment.
- **Module split:** decoders are pure `ra2assets` (no SDL) — `.pkt` parse, `[Waypoints]` parse, the theater chain table. Catalog policy (which archive, which filter, which sort) is a pure `engine/` module registered in **BOTH** cmake files. UI/scene code stays in `engine/main.cpp`.
- **No assets committed.** Real-asset tests guard on `RA2_INSTALL_DIR` and `MESSAGE("skipped: …"); return;` without it (the `tests/test_map.cpp:122` pattern).
- Build: `cmake --build build --target ra2assets_tests ra2engine`. doctest. Run exes from **PowerShell**, not Git Bash.
- **Demo gate** = two-run compute-and-self-compare, wedge guards → exit 1, **NO pinned hash literal in code** (README pins it).
- Commit trailer for implementers: `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.

## Calibration summary (recovered ground truth — the behavior contract)

Confidence labels are maps.md's own (**CONFIRMED** = real data on this install, usually + an independent reader; **PROBABLE** = strong but single-source; **[RE-FALLBACK]** = not determined).

### C1. Where the maps live (§1, CONFIRMED)

| archive | contents | used this phase |
|---|---|---|
| `multimd.mix` (31,264,268 B, 173 entries) | **YR multiplayer maps** | **YES** — the skirmish universe |
| `langmd.mix` | `missionsmd.pkt` (21,142 B, entry `480B82E4`), `ra2md.csf` (573,137 B) | **YES** — manifest + display names |
| `mapsmd03.mix` | YR campaign (incl. `sov07tmd.map` = the hardcoded `0x133a0cb2`) | **default map only** (unchanged) |
| `MULTI.MIX` + `missions.pkt` | RA2-mode maps (148 listed, **58 absent**) | **NO** — carry-forward (§Scope) |

### C2. Enumeration = the `.pkt` manifest (§2, CONFIRMED)

`missionsmd.pkt` is an INI:

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

- `[MultiMaps]` values are **base names**; append `.map` and look up in the mix (`BldFeud` → `bldfeud.map`).
- **All 161 non-commented `missionsmd.pkt` entries resolve to real files in `multimd.mix` on this install (0 missing).** The RA2 `missions.pkt` lists 148 of which **58 are absent** → **an implementation MUST existence-filter the pkt list**. We implement the filter unconditionally even though the YR list is currently complete, because it is the recovered original behavior and it is the only thing that makes the loader safe against a partial install.
- `Description=` is a **CSF string label** (`DESC:MP01DU` → *"South Pacific (2-4)"*), resolved **case-insensitively** in `ra2md.csf`. **No stock map has `[Basic] Name=`** (0 of 267 MP maps + 14 YR campaign maps) — display names live ONLY in the CSF.
- `GameMode=` is a CSV of mode tags. `mpmodesmd.ini` maps *Battle* → the `standard` filter, and Battle is *"the only one that allows AI players"* → **the skirmish browser lists the 91 `GameMode=standard` maps.** Full census over the 161 active maps: standard 91, megawealth 30, teamgame 25, meatgrind 20, duel 11, cooperative 10, navalwar 8, nukewar 7, airwar 1 (maps carry multiple tags).
- `MinPlayers`/`MaxPlayers` is the **lobby-facing cap**; it can *disagree* with the map's waypoint count (CONFIRMED, 6 of 161 — e.g. `arena33forever.map` and `sedonapass(mw).map` define 6 start waypoints but the pkt says `MaxPlayers=4`). **Design rule:** the pkt range is what the UI advertises; the **waypoints are the placeable positions**, and the launcher's slot cap is `min(pkt MaxPlayers, start-waypoint count, 8)`.
- Loose `*.map`/`*.mpr`/`*.yrm` "unofficial" maps from the install dir: **PROBABLE (third-party docs only; this install has none)** → out of scope, carry-forward.

### C3. The playable set on this install (§3, CONFIRMED)

161 playable maps; **91 `standard`**. Theaters: TEMPERATE 76, SNOW 39, URBAN 19, DESERT 14, NEWURBAN 13, **LUNAR 0** (no LUNAR multiplayer map exists). Starts: 2p 34, 3p 7, 4p 81, 5p 1, 6p 24, 8p 14. Dimensions range from `hidvally` 52x52 to `xmp20t6` 131x146.

### C4. `[Map]` (§4.2, CONFIRMED — already parsed)

`Theater=` ∈ {`TEMPERATE`,`SNOW`,`URBAN`,`NEWURBAN`,`DESERT`,`LUNAR`}; `Size=x,y,w,h` with x,y always 0 and w,h the **diamond** dimensions; cell count = `(2·w − 1)·h` (exact on xmp01t4 105x95→19,855, xmp10s4 79x85→13,345, sov07tmd 103x110→22,550). `LocalSize=` is the playable sub-rect — **PROBABLE** (semantics from OpenRA/XCC, not exercised here) → **ignored this phase**, documented.

### C5. `[Waypoints]` — the player-start convention (§4.3, CONFIRMED)

```ini
[Waypoints]      ; xmp01t4.map (4-player)
0=98057
1=148099
2=96152
3=55110
26=93052
98=101116
99=100106
699=100100
```

1. **Decode: `x = value % 1000`, `y = value / 1000`** — a cell in the SAME coordinate space as `[IsoMapPack5]` cell records (`MapCell::x/y`), i.e. the engine's `World` grid space. Triple-sourced: OpenRA `ImportGen2MapCommand.ReadWaypoints`, XCC `map_ts_ini_reader`, and decisively this install's own `[Header]` affine cross-check (`HeaderY = round((x+y)/2)`, `HeaderX = (x−y)/2 + 255.5±0.5` hold ONLY under this decode order). Every decoded waypoint in every sampled map lands on a real IsoMapPack5 cell.
2. **Waypoints `0`–`7` are the multiplayer start positions**; the start count is how many of keys `0..7` are present. Cross-checked on **all 267 MP maps: count of keys 0–7 == `[Header] NumberStartingPoints` in every single file** (2..8, never more).
3. `[Header] NumberStartingPoints` is a cheap header-only player-count read but the authoritative start **cells** require the `[Waypoints]` decode.
4. **[RE-FALLBACK: meaning of waypoints > 7]** — 10–55 plus 98, 99, 699 (699 = map center in every sample). The maps' `[ScriptTypes]` are empty stubs so they are not script-referenced; presumably `gamemd.exe`'s own skirmish-AI pathing nodes. **Only 0–7 matter for house placement.** Carried forward as a label (see §Behavior B4).

### C6. `[IsoMapPack5]` / `[OverlayPack]` (§4.4/§4.5, CONFIRMED — already implemented)

`src/mappack.cpp` + `src/map.cpp` already do base64 → Format5 → **LZO** → 11-byte cell records with the 0xFFFF→Clear materialization, and base64 → Format5 → **LCW** → flat 512x512 overlay arrays. **Nothing new is needed and nothing here may change** (§7.1).

### C7. Theater availability (§6, CONFIRMED for data presence / PROBABLE for the `.ubn/.des/.lun` render chain)

| `Theater=` | tiles mix (base / YR-extra) | control INI | iso palette | ext | MP maps |
|---|---|---|---|---|---:|
| TEMPERATE | `ra2.mix/isotemp.mix` / `ra2md.mix/isotemmd.mix` | `local.mix/temperat.ini` | `cache.mix/isotem.pal` | `.tem` | 76 |
| SNOW | `ra2.mix/isosnow.mix` / `isosnomd.mix` | `snow.ini` | `isosno.pal` | `.sno` | 39 |
| URBAN | `ra2.mix/isourb.mix` / `isourbmd.mix` | `urban.ini` | `isourb.pal` | `.urb` | 19 |
| NEWURBAN | `ra2md.mix/isoubn.mix` + `isoubnmd.mix` | `localmd.mix/urbannmd.ini` | `cachemd.mix/isoubn.pal` | `.ubn` | 13 |
| DESERT | `ra2md.mix/isodes.mix` + `isodesmd.mix` | `desertmd.ini` | `isodes.pal` | `.des` | 14 |
| LUNAR | `ra2md.mix/isolun.mix` + `isolunmd.mix` | `lunarmd.ini` | `isolun.pal` | `.lun` | 0 (campaign only) |

maps.md §6/§8: `.tem/.sno/.urb` are exercised today; **`.ubn/.des/.lun` are PROBABLE** (corroborated by the `des.mix/lun.mix/ubn.mix` marker mixes + XCC's theater tables) until a non-temperate map is actually rendered. **Skirmish-browser MVP ships the 5 MP theaters and rejects LUNAR.**

**Gap — maps.md does NOT pin these, and code depends on them (→ Task 1 RE-GATE):**
- the **NewTheater art letter** for NEWURBAN / DESERT / LUNAR. `theater_name.hpp`'s recovered munge (from `FUN_005F8860`) pins `'T'` temperate, `'A'` snow, `'U'` urban only.
- the per-theater **building-art mix** spelling (maps.md §6 mentions "`temperat/snow/urban/urbann/desert/lunar.mix` building-art mixes" but also the marker mixes `ubn.mix/des.mix/lun.mix` — which name `open_nested` must use is not pinned).
- the per-theater **theater palette** (`temperat.pal` equivalent, used by `load_ore_art`) and **unit palette** (`unittem.pal` equivalent, used by `load_building_assets`).

### C8. Format facts we deliberately do NOT consume (§4.6/§4.7)

`[PreviewPack]` (Format5 + **LZO**, not LCW; `w·h·3` **RGB, R first**) and pre-placed `[Structures]`/`[Terrain]`/`[Units]`/`[Infantry]` are fully documented in maps.md but are **carry-forwards** — see §Scope for the justification.

## Current-engine reality (the seams this attaches to)

File:line anchors, grepped against the post-Bink tree (`54359c8`).

- **`Assets load_assets(const std::string& dir, const ra2assets::MixFile& ra2)`** — `main.cpp:143`. Hardcodes `mapsmd03.mix` (145), `find(0x133a0cb2u)` (146), `throw` unless `theater == "TEMPERATE"` (152–156), `cache.mix/isotem.pal` (159–163), `local.mix/temperat.ini` (166–170), `ra2.open_nested("isotemp.mix")` (173), `Theater th(std::move(isotemp), ini_text, "tem")` (175). Returns `Assets{map, pal, theater}` (`struct Assets` at 108). **21 call sites**, all of the form `load_assets(dir, ra2)` — every demo gate plus `GameScene` (3167).
- **`load_ore_art(const ra2assets::MixFile& ra2)`** — `main.cpp:212`. Candidate walk `theater_candidates(name, 'T', "isotemp.mix", "temperat.mix", ext)` over `{".tem", ".shp"}` (227–229); palette `cache.mix/temperat.pal` (256–257).
- **`load_building_assets(const MixFile& ra2, const MixFile& language, const Rules&, const Art&)`** — `main.cpp:1438`. Same `theater_candidates(image, 'T', "isotemp.mix", "temperat.mix", ".shp")` walk (1546–1547); `unit_pal` = `unittem.pal`.
- **`ra2assets::theater_candidates(image, letter, iso_mix, theater_mix, ext)`** — `include/ra2assets/theater_name.hpp:33`, already **fully parameterized** by letter + mixes. Nothing about it changes; only its arguments become theater-driven.
- **`ra2engine::Theater(MixFile isotile_mix, const std::string& control_ini_text, std::string ext)`** — `iso_renderer.hpp:41`, already parameterized by mix + INI + extension.
- **`pick_start_cell(const MapFile&)`** — `world.hpp:419` / `world.cpp:171`: centroid of every `tile == 0` cell, nearest Clear cell to it. Used by 20+ gate paths and by `launch_skirmish`.
- **`make_ai_waypoints(int px, int py)`** — `main.cpp:2412`, flagged `[RE-FALLBACK: ai-team.md]`: a synthetic 32-entry ring around the player at ±4 cells. Consumed via `make_ai_team_config(..., int player_x, int player_y, bool enable_triggers=false)` (`main.cpp:2424`, line 2470) — **2 gate call sites** (`main.cpp:7101`, `7350`) plus `GameScene`.
- **`launch_skirmish(cfg, a, ua, ba, rules)`** — `main.cpp:9431`. Fans bases out from `pick_start_cell(a.map)` at `bx = sc->x + (i*14) % max(1, gw-14)`, `by = sc->y` (9444–9445), ring-scans `cfg.unit_count` free cells (9451–9460). Called by `GameScene` (3185) and `simulate_skirmish` (9752) **only**.
- **`SkirmishConfig`** — `engine/skirmish.hpp:25`. `map_id = "mapsmd03"` is a **dead string**: `launch_skirmish` never reads it; only `SkirmishSetupScene::render` prints it as `MAP: MAPSMD03` (`main.cpp:4573`).
- **`SkirmishSetupScene`** — `main.cpp:4399`. Deterministic geometry helpers `global_rect(i)` `{60, 150+i*46, 520, 38}`, `slot_rect(row)` `{640, 190+row*46, 600, 38}`, `slot_cell(row,cell)`, `start_rect()` `{60,630,220,56}`, `back_rect()` `{320,630,220,56}`; `enum GOpt` 8 rows; `adjust_global(gopt, dir)`; render via `draw_text`/`draw_fnt_text`; `handle` hit-tests the same rects. `Start` → `new GameScene(no_audio_, cfg_)`.
- **`--demo-skirmish` gate template** — `skirmish_fold` (`main.cpp:~9690`), `struct DemoSkirmishResult {witness, hash, ok}` (9720), `simulate_skirmish` (9734, N=300 frames at `dt=1/15`, FNV basis `0xcbf29ce484222325ull`, wedge guards, `[sk] skirmish_hash=%016llx` witness line), `run_demo_skirmish` (9844: rendered hidden-window run + headless fresh-World run, byte-compare witness + hash). Dispatch arm `main.cpp:10369`.
- **Decoders available:** `MixFile::find(uint32_t|std::string)`, `open_nested`, `read` (`mix.hpp`); `CsfStrings::load(data,len)` + case-insensitive `find(label) -> const std::u16string*` (`csf.hpp`); `Ini::parse`, `Ini::section(name) -> vector<pair<string,string>>`, `Ini::get`, `Ini::has`; `read_file(path)` (`fileio.hpp`).
- **Greenfield** (grep found none): `PktFile`, `PktMapEntry`, `MapSelection`, `TheaterChain`, `theater_chain`, `MapCatalog`, `MapEntry`, `build_map_catalog`, `start_waypoints`, `maps_fold`, `run_demo_maps`.

## Architecture

Seven components. Each states what it does, its interface, its dependencies, and **how it stays additive / byte-identical**.

### A. `ra2assets::PktFile` — the `.pkt` manifest decoder (pure)

New TU `include/ra2assets/pkt.hpp` + `src/pkt.cpp`, registered in `RA2_LIB_SOURCES`.

```cpp
struct PktMapEntry {
    std::string index_key;    // "1"          -- the [MultiMaps] key
    std::string base_name;    // "XMP01T4"    -- verbatim from [MultiMaps]
    std::string file_name;    // "xmp01t4.map"-- lowercased base_name + ".map"
    std::string description;  // "DESC:MP01DU"-- CSF label, "" if absent
    std::vector<std::string> game_modes;  // lowercased tags from GameMode=
    int min_players = 0, max_players = 0, cd = 0;
    bool has_mode(const std::string& tag) const;   // case-insensitive
};
class PktFile {
public:
    static PktFile parse(const std::string& ini_text);
    const std::vector<PktMapEntry>& entries() const;   // [MultiMaps] order
};
```

Rules (all from §2): `[MultiMaps]` values are base names in file order; commented-out (`;169=`) lines are already dropped by `Ini`; the per-map section is looked up **case-insensitively** by base name (pkt sections are `[XMP01T4]`, values are `XMP01T4` — same case on this install, but the game resolves INI keys case-insensitively and 3 sections in the file differ in case from their index value); a `[MultiMaps]` value with **no matching section** still yields an entry with empty description/modes (the catalog's existence filter is what removes it, not the parser); `GameMode=` splits on `,` with whitespace trimmed and tags lowercased.

**Additive/inert:** a brand-new `ra2assets` TU. It touches no existing decoder and nothing on any gate path. Depends on: `ra2assets::Ini` only.

### B. `ra2assets::MapFile` — `[Waypoints]` as NEW fields (§7.1)

```cpp
struct MapWaypoint { int index; int x, y; };   // x = v % 1000, y = v / 1000  [§4.3]

// on MapFile:
const std::vector<MapWaypoint>& waypoints() const;        // ALL keys, ascending index
std::vector<MapWaypoint>        start_waypoints() const;  // indices 0..7 only, ascending
int                             start_count() const;      // == start_waypoints().size()
```

`MapFile::parse` gains one block after the overlay collection: walk `ini.section("Waypoints")`, `std::stoi` each key as the index and each value as the packed cell (non-numeric key or value → skipped, never throws), decode `x = v % 1000`, `y = v / 1000`, sort ascending by index. **Nothing else in `parse` changes** — theater, size, cells, overlay and their order are byte-for-byte the same code.

**Additive/inert (§7.1 compliance):** the new members are appended to the class; `cells_`/`overlay_`/`width_`/`height_`/`theater_` and every existing accessor are untouched. The gates' map (`sov07tmd.map`) *does* have a `[Waypoints]` section (mission waypoints 0..5+), so the new fields are populated on the gate path — but **nothing on the gate path reads them**, and `world_hash` folds only `Unit`/`Building` state, so no gate byte can move.

### C. `ra2assets::TheaterChain` — the pinned per-theater table (pure)

Appended to `include/ra2assets/theater_name.hpp` + `src/theater_name.cpp` (the existing home of `theater_candidates`).

```cpp
struct TheaterChain {
    const char* name;         // "TEMPERATE"
    char        letter;       // NewTheater art letter [T1 RE-GATE for N/D/L]
    const char* iso_mix;      // "isotemp.mix"   -- tile templates
    const char* art_mix;      // "temperat.mix"  -- building/overlay art
    const char* control_ini;  // "temperat.ini"
    const char* iso_pal;      // "isotem.pal"
    const char* theater_pal;  // "temperat.pal"  -- ore/gem overlay palette
    const char* unit_pal;     // "unittem.pal"
    const char* ext;          // "tem"           -- WITHOUT the dot
    bool        yr_only;      // true -> ra2md.mix / localmd.mix / cachemd.mix chain
    bool        playable;     // false for LUNAR (no MP map exists, §3)
};
const TheaterChain* theater_chain(const std::string& theater_name);  // nullptr if unknown
```

Six rows, values verbatim from §6 (the `TEMPERATE` row is exactly what `load_assets` hardcodes today). `LUNAR` is present with `playable = false`; the loader rejects it with a clear message (§3: no LUNAR multiplayer map exists).

**Additive/inert (§7.4 compliance):** `theater_candidates` itself is **not modified** — it already takes the letter and both mix names as parameters. The `TEMPERATE` row reproduces the exact current arguments (`'T'`, `"isotemp.mix"`, `"temperat.mix"`, `".tem"`/`".shp"`), so the candidate list and its **order** are bit-identical for every existing asset. A pure host test pins the TEMPERATE row's fields against the literals currently in `main.cpp`.

### D. `load_assets(dir, ra2, sel)` — map selection + theater generalization

```cpp
// main.cpp, anonymous namespace, beside load_assets
struct MapSelection {
    std::string mix_file  = "mapsmd03.mix";   // relative to the install dir
    std::uint32_t entry_id = 0x133a0cb2u;     // used when entry_name is empty
    std::string entry_name = "";              // e.g. "xmp31s2.map"
};
Assets load_assets(const std::string& dir, const ra2assets::MixFile& ra2,
                   const MapSelection& sel = MapSelection{});
```

- `struct Assets` gains **one** new member: `const ra2assets::TheaterChain* chain = nullptr;` (so the art loaders can follow the same theater).
- The body: open `dir + sel.mix_file`; `sel.entry_name.empty() ? maps.find(sel.entry_id) : maps.find(sel.entry_name)`; parse; `chain = theater_chain(map.theater())`; throw a clear error if `!chain || !chain->playable`.
- Palette / control INI / tile mix now come from `chain`: for `yr_only == false` from `ra2.open_nested("cache.mix"|"local.mix")` and `ra2.open_nested(chain->iso_mix)` — the **identical calls** the temperate path makes today; for `yr_only == true` (NEWURBAN/DESERT/LUNAR) `load_assets` opens `dir + "ra2md.mix"` and descends `cachemd.mix`/`localmd.mix` — **only on that branch** (§7.5).
- `Theater th(std::move(iso), ini_text, chain->ext)`.

**Additive/byte-identical (§7.2/§7.5 compliance):** the default `MapSelection{}` is `mapsmd03.mix` / `0x133a0cb2` / by-id — literally today's three constants. All 21 existing `load_assets(dir, ra2)` call sites compile and behave unchanged. For the default map `theater()=="TEMPERATE"` → `chain->yr_only == false` → the `ra2md.mix` open never executes on a gate path, and the `cache.mix`→`isotem.pal`, `local.mix`→`temperat.ini`, `isotemp.mix`, `"tem"` sequence is the same sequence of calls in the same order.

### E. `load_building_assets` / `load_ore_art` — theater-driven candidate walks

Both gain a **defaulted trailing parameter**:

```cpp
OreArt load_ore_art(const ra2assets::MixFile& ra2,
                    const ra2assets::TheaterChain* chain = nullptr);   // nullptr => TEMPERATE
BuildingAssets load_building_assets(const ra2assets::MixFile& ra2, const ra2assets::MixFile& language,
                                    const Rules& rules, const Art& art,
                                    const ra2assets::TheaterChain* chain = nullptr);
```

Inside, `const TheaterChain& th = chain ? *chain : *theater_chain("TEMPERATE");` and every hardcoded `'T'` / `"isotemp.mix"` / `"temperat.mix"` / `".tem"` / `"temperat.pal"` / `"unittem.pal"` becomes `th.letter` / `th.iso_mix` / `th.art_mix` / `("." + th.ext)` / `th.theater_pal` / `th.unit_pal`. For YR theaters the nested-mix opens are attempted on `ra2md.mix`'s chain first with the `ra2.mix` chain as fallback — each individually guarded exactly as today (a miss leaves `present=false`, never a crash).

**Additive/byte-identical (§7.4 compliance):** with the parameter omitted (every existing call site) the substituted values are *character-identical* to the current literals, so `theater_candidates` receives identical arguments and returns an identically-ordered list. This is the highest-risk task in the plan and is where the plan re-verifies all 18 gates.

### F. `ra2engine::MapCatalog` — the pure engine-side catalog

New pure module `engine/maplist.hpp` + `engine/maplist.cpp`, registered in **BOTH** `cmake/ra2_sources.cmake` (→ `ra2engine`) and `CMakeLists.txt` (→ `ra2assets_tests`), following the `engine/skirmish.cpp` precedent.

```cpp
namespace ra2engine {

struct MapEntry {
    std::string file_name;     // "xmp01t4.map"  -- the multimd.mix entry
    std::string base_name;     // "XMP01T4"
    std::string display_name;  // "South Pacific (2-4)"  (CSF; falls back to base_name)
    std::string csf_label;     // "DESC:MP01DU"
    std::vector<std::string> game_modes;
    int min_players = 0, max_players = 0;
};

// Build the skirmish map list [RE: maps.md §2/§3]. Steps, in this exact order:
//   1. PktFile::parse(pkt_text)                          -- the manifest
//   2. EXISTENCE FILTER: drop entries whose file_name is not in `maps`  [§2 MUST]
//   3. MODE FILTER: keep entries with has_mode(mode_tag) (""=keep all)  [§2 standard]
//   4. CSF display name from `csf` (case-insensitive); fall back to base_name [§2]
//   5. stable sort by display_name (case-insensitive), then file_name (tie-break)
std::vector<MapEntry> build_map_catalog(const std::string& pkt_text,
                                        const ra2assets::MixFile& maps,
                                        const ra2assets::CsfStrings* csf,
                                        const std::string& mode_tag = "standard");

// Convenience for the engine: open <dir>/multimd.mix + <dir>/langmd.mix, read
// missionsmd.pkt + ra2md.csf, and call build_map_catalog. Throws on a missing
// archive (the caller guards and degrades to the default map).
struct MapCatalog { std::vector<MapEntry> entries; ra2assets::MixFile maps; };
MapCatalog load_yr_map_catalog(const std::string& dir, const std::string& mode_tag = "standard");

}  // namespace ra2engine
```

CSF names are UTF-16; `display_name` is the label's value narrowed to Latin-1/ASCII (every stock map name is ASCII; non-ASCII code units are replaced with `'?'`). Empty CSF result or missing `csf` → `display_name = base_name`.

**Additive/inert (§7.5 compliance):** a new pure module; `load_yr_map_catalog` is called **only** by `SkirmishSetupScene` and the `--demo-maps` gate — never from `load_assets`, never on a gate path, so `multimd.mix`/`langmd.mix` are never slurped at gate startup.

### G. Real start placement — `SkirmishConfig` opt-in (§7.3)

Two new `SkirmishConfig` fields, both defaulting to today's behavior:

```cpp
struct SkirmishConfig {
    std::string map_id   = "mapsmd03";   // UNCHANGED (legacy label)
    std::string map_file = "";           // NEW: "" => the default hardcoded map;
                                         //      else a multimd.mix entry, e.g. "xmp31s2.map"
    bool use_map_starts  = false;        // NEW: false => the existing pick_start_cell fan-out
    ...                                  // every existing field UNCHANGED
};
```

- **`launch_skirmish`** (`main.cpp:9431`) gains one branch at the top of the per-slot loop:

```cpp
const std::vector<ra2assets::MapWaypoint> starts =
    cfg.use_map_starts ? a.map.start_waypoints() : std::vector<ra2assets::MapWaypoint>{};
...
int bx, by;
if (!starts.empty()) {                        // [RE: maps.md §4.3] real MP starts
    const ra2assets::MapWaypoint& w = starts[combatant_ix % starts.size()];
    bx = w.x; by = w.y;
} else {                                      // UNCHANGED legacy fan-out
    bx = sc ? sc->x + (i * 14) % std::max(1, gw - 14) : 6 + i * 6;
    by = sc ? sc->y : 6 + i * 4;
}
```

  where `combatant_ix` counts combatant slots in order (so the Neutral/Special non-combatants do not consume a start). Slot → waypoint binding is **`combatant_ix` → start-waypoint index in ascending order** (see §Behavior B3 for why, and the `[RE-FALLBACK]` it carries).
- **`make_ai_team_config`** (`main.cpp:2424`) gains a defaulted trailing parameter `const ra2assets::MapFile* map = nullptr`. When non-null AND the map has waypoints, `tc.waypoints` is built by `make_ai_waypoints_from_map(*map, px, py)`: a 32-entry table where index *i* is the map's waypoint *i* if present, else `make_ai_waypoints(px, py)[i]` (the existing synthetic ring). When null → **exactly `make_ai_waypoints(px, py)`**, the current code.
- **`GameScene`** passes `MapSelection{"multimd.mix", 0, cfg.map_file}` to `load_assets` when `cfg.map_file` is non-empty, and `&a_.map` to `make_ai_team_config`.

**Additive/byte-identical (§7.3 compliance):** `pick_start_cell` and `make_ai_waypoints` are **not modified**. Both gate call sites of `make_ai_team_config` (`main.cpp:7101`, `7350`) omit the new parameter → nullptr → identical `tc.waypoints`. `--demo-skirmish`'s fixed config (`main.cpp:9742`) leaves `use_map_starts = false` and `map_file = ""` → `starts` is empty → the legacy fan-out runs → `skirmish_hash=e9ad3f1fd88495f9` is preserved.

### H. `--demo-maps` — the 19th gate

Mirrors `run_demo_skirmish` exactly. It exercises the *whole* new path: catalog → map-by-name load → non-temperate theater → real waypoint starts → skirmish launch → deterministic stepping.

- **Fixed map: `xmp31s2.map`** — "May Day (2)", **SNOW**, 70x83, **2 start waypoints**, `GameMode=standard, navalwar`, 124,908 B (maps.md §3 table row). Chosen because (a) SNOW exercises theater generalization on the **`ra2.mix` base chain with the CONFIRMED `'A'` NewTheater letter** (`theater_name.hpp`), so the gate does not hinge on a Task-1-pinned YR letter; (b) 70x83 is small and fast; (c) 2 starts matches a 2-slot config exactly; (d) it is a `standard` map, so it must appear in the catalog the gate also folds.
- `maps_fold(h, world)` — reuse `skirmish_fold`'s shape (house count; per-house alive buildings/units, `ai_enabled`, credits; the full `is_enemy` matrix; `winning_team`).
- Fold checkpoints: **(A)** catalog signature — entry count + FNV over each entry's `file_name`, `min_players`, `max_players`, `game_modes.size()` in list order; **(B)** map signature — theater string, `width()`, `height()`, `cells().size()`, and each start waypoint's `index/x/y`; **(C)** the launched world (`maps_fold`); **(D)** after **N = 300** steps at fixed `dt = 1/15`, `maps_fold` again.
- **Wedge guards → `r.ok = false` → exit 1:** catalog non-empty; every catalog entry exists in `multimd.mix` (the existence filter actually ran); `xmp31s2.map` is present in the `standard` catalog; the loaded map reports `theater()=="SNOW"`, `width()==70`, `height()==83`, `start_count()==2`; **each combatant house's spawned base cell equals its map start waypoint** (the load-bearing assertion — proves §4.3's decode reached the world); the sim advanced (state signature changed).
- Two-run rendered(hidden window + start/end PNG)/headless(fresh `World`) self-compare of the witness log + hash; **no pinned literal** in code.

**Additive/inert:** a new gate function + a new `--demo-maps` dispatch arm inserted beside `--demo-roster` (`main.cpp:10371`). No existing gate's code path is touched.

### I. Map list UI in `SkirmishSetupScene`

The `MAP: MAPSMD03` label (`main.cpp:4573`) becomes a **scrollable single-select list** on the right of the setup screen (the slot rows move down), plus the existing Start/Back. Per row: `display_name`, theater, `min-max` players and `WxH`. Selection writes `cfg_.map_file`, `cfg_.use_map_starts = true`, and **clamps the AI-slot count** so that `combatant slots ≤ min(pkt max_players, start_count, 8)` (§2's "pkt is the lobby cap, waypoints are the placeable positions" rule). The catalog is loaded once in the scene's constructor via `load_yr_map_catalog(env_install_dir())`, **try/catch-guarded**: on any failure the scene falls back to a single synthetic entry representing the default map (`map_file = ""`, `use_map_starts = false`) and the screen behaves exactly as it does today.

Dimensions/theater/start-count per row require parsing each map — 91 maps × ~150 KB is too slow for a scene constructor, so rows show pkt data (`min/max players`) immediately and the **selected** map is parsed lazily (one `MapFile::parse` on click) to fill theater/size/start-count and to clamp the slot count.

**Additive/inert:** UI only, reachable exclusively through `run_scene_driver` from the two non-gate `main()` seams. Not hashed. Depends on: `MapCatalog` (F), `draw_text`/`draw_fnt_text`/`SbRect` (existing).

## Data model (exact additions)

| Field / type | On | Default | In `world_hash`? | Why additive/inert |
|---|---|---|---|---|
| `struct PktMapEntry`, `class PktFile` | (new TU `ra2assets/pkt.hpp`) | — | No | New decoder; no existing TU references it. |
| `struct MapWaypoint`, `waypoints()`, `start_waypoints()`, `start_count()` | `ra2assets::MapFile` | empty | No | NEW fields (§7.1); populated on the gate path but read by nobody there. |
| `struct TheaterChain`, `theater_chain(name)` | `ra2assets/theater_name.hpp` | — | No | Pure const table; `theater_candidates` unmodified; TEMPERATE row == today's literals (§7.4). |
| `struct MapSelection`, `load_assets(..., sel = {})` | `main.cpp` | `mapsmd03.mix` / `0x133a0cb2` / by-id | No | Default == today's three constants (§7.2). |
| `const TheaterChain* chain` | `struct Assets` (`main.cpp:108`) | `nullptr` | No | Render-side plumbing only; `Assets` is not hashed. |
| `const TheaterChain* chain = nullptr` param | `load_ore_art`, `load_building_assets` | `nullptr` → TEMPERATE | No | Omitted at every existing call site → identical candidate order (§7.4). |
| `std::string map_file = ""` | `ra2engine::SkirmishConfig` | `""` → default map | No | Config data; `--demo-skirmish` leaves it empty. |
| `bool use_map_starts = false` | `ra2engine::SkirmishConfig` | `false` → legacy fan-out | No | Config data; `--demo-skirmish` leaves it false → `e9ad3f1fd88495f9` preserved (§7.3). |
| `const ra2assets::MapFile* map = nullptr` param | `make_ai_team_config` (`main.cpp:2424`) | `nullptr` → `make_ai_waypoints(px,py)` | No | Both gate call sites omit it (§7.3). |
| `struct MapEntry`, `MapCatalog`, `build_map_catalog`, `load_yr_map_catalog` | (new module `engine/maplist.hpp`) | — | No | Pure module; only the setup scene + the new gate call it (§7.5). |

**None of it enters `world_hash`.** `world_hash` folds only `Unit{cell_x,cell_y,hp,alive}` + `Building{type,cx,cy,hp,state}` and is not modified. The new `--demo-maps` gate uses its **own** `maps_fold`, exactly as every previous phase did.

## Behavior details (each tied to a recovered §)

- **B1 — Enumeration (§2).** The list is the pkt, not the archive. `build_map_catalog` applies, in order: pkt parse → **existence filter against `multimd.mix`** → `GameMode` filter (`standard`) → CSF display-name resolution → stable sort. On this install that is 173 archive entries → 161 pkt-active → 161 present → **91 standard**. The existence filter is mandatory even though the YR list is currently complete (§2: the RA2 pkt lists 58 absent maps; a partial install would otherwise offer unloadable maps). [CONFIRMED]
- **B2 — Display names (§2).** `Description=DESC:MP01DU` → case-insensitive `ra2md.csf` lookup → *"South Pacific (2-4)"*. Stock maps have **no `[Basic] Name=`** (0/281), so there is no in-map fallback: a missing CSF label falls back to the pkt base name. [CONFIRMED]
- **B3 — Start placement (§4.3).** `x = v % 1000`, `y = v / 1000`; keys `0..7`; count == `[Header] NumberStartingPoints` on 267/267 MP maps. **Slot binding:** combatant slots are bound to start waypoints **in ascending waypoint index**, i.e. the *k*-th combatant slot gets `start_waypoints()[k]` (wrapping if a config somehow has more combatants than starts — the UI prevents this by clamping). The original game *randomizes* the lobby-position → start-location assignment; a deterministic ascending binding is required for a byte-identity gate, so the randomized assignment is carried as **[RE-FALLBACK: lobby→start assignment order]** and adjudicated in Task 1. [Decode CONFIRMED / binding RE-FALLBACK]
- **B4 — AI script waypoints (§4.3 item 4).** On the interactive path `tc.waypoints[i]` becomes the map's waypoint *i* when the map defines it, else the synthetic ring entry. This directly retires the `[RE-FALLBACK]` in `main.cpp:2407` for indices the map actually defines. The **purpose** of waypoints > 7 (10–55, 98, 99, 699 = map center) remains **[RE-FALLBACK]** — maps.md ruled out in-map script references and did not determine them; we simply use them as positional nodes, which is strictly better than a ring around the player and cannot be wrong about anything maps.md pinned. Gate paths keep the pure ring (nullptr default). [RE-FALLBACK carried]
- **B5 — Player cap (§2/§3).** Effective combatant cap = `min(pkt MaxPlayers, map start_count, 8)`. The pkt cap can be *lower* than the waypoint count (6 of 161 maps) and is the lobby-facing number; the waypoint count is the hard placement limit. 8 is the engine cap inherited from the skirmish phase (`[Maximums] Players=8`). [CONFIRMED]
- **B6 — Theaters (§6).** The 5 MP theaters load; **LUNAR is rejected** with a clear error (`playable == false`) because no LUNAR multiplayer map exists. TEMPERATE/SNOW/URBAN resolve through the `ra2.mix` chain; NEWURBAN/DESERT additionally open `ra2md.mix`/`localmd.mix`/`cachemd.mix` — **only on those branches** (§7.5). The `.ubn`/`.des` render chain is **PROBABLE** in maps.md; Task 1 proves it by resolving real templates before any code depends on it. [CONFIRMED data / PROBABLE chain → T1]
- **B7 — Variable dimensions (§4.2, §5).** `World::grid_w/grid_h` are already derived as `max(cell.x)+1` from the decoded cells, so a 52x52 or a 131x146 map needs no engine change. Overlay arrays are 512x512 and iso coordinates span roughly `1..w+h` — max 277 on this install's largest map, comfortably in range. `LocalSize` is **ignored** (PROBABLE semantics, §4.2). [CONFIRMED]
- **B8 — Default map untouched (§7.2).** `--demo-*`, `--screenshot`, `run_menu` and every host test keep loading `mapsmd03.mix`/`0x133a0cb2`. Only a `SkirmishSetupScene` selection or the `--demo-maps` gate ever passes a different `MapSelection`. [design]

## Scope / non-goals (carry-forwards — documented, NOT implemented)

Deliberate cuts, each with its justification:

| Carry-forward | maps.md ref | Why deferred |
|---|---|---|
| **`[Preview]`/`[PreviewPack]` lobby thumbnails** | §4.6 CONFIRMED (Format5 + **LZO**, `w·h·3` RGB R-first; 267/267 decode exact) | **CUT deliberately.** The decode is cheap (zero new codecs), but the *phase's* risk is concentrated in generic map load + five theater chains + waypoint starts, all of which must stay gate-safe. A thumbnail adds a second UI subsystem (per-map RGB texture upload/caching/invalidation) to a screen this phase is already rewriting, for zero simulation value; and the RGB byte order is the one item maps.md §8 flags as *"visual, single map — spot-check more when wiring UI"*, i.e. it wants its own verification pass. A list showing name / theater / players / dimensions is a complete, usable browser. Ships next phase as a self-contained `ra2assets` decoder + one blit. |
| **Pre-placed `[Units]`/`[Infantry]` spawning** | §4.7 (head `OWNER,TYPE,HEALTH,X,Y` CONFIRMED; **field tail PROBABLE**, from OpenRA's reader only) | **CUT deliberately.** 85 of 267 MP maps carry them, almost entirely Neutral civilian eye-candy (CAR/CONA/SUVB) plus co-op scripted forces. Spawning them needs an owner-string → house-index mapping that does not exist (stock MP maps name `Neutral`, campaign maps name `BadGuy3 House`), plus the facing-vs-subcell field disambiguation that maps.md marks **PROBABLE**. Implementing on a PROBABLE field layout is exactly the empirical guessing the calibrate-first directive forbids. Needs its own RE pass. |
| **Pre-placed `[Structures]`/`[Terrain]`** (neutral tech buildings, city blocks, trees) | §4.7 (12,083 structure entries across 267 maps; `[Terrain]` key = `1000·y+x`, CONFIRMED) | Same reason as above plus a gameplay dependency: neutral tech buildings are only meaningful once they can be captured/garrisoned, and city blocks need collision + building art per theater. Deferring keeps the new `--demo-maps` gate's world simple and its hash stable. |
| **RA2-mode map set** (`MULTI.MIX` + `missions.pkt`, 90 present of 148 listed) | §1/§2 CONFIRMED | The engine targets `gamemd.exe`/YR (`rulesmd.ini`, `multimd.mix`). Adding a second archive+manifest pair doubles the catalog surface for maps that are largely duplicated in the YR set. The catalog API is already parameterized by pkt text + archive, so this is a small follow-up. |
| **Non-`standard` game modes** (megawealth 30, teamgame 25, meatgrind 20, duel 11, cooperative 10, navalwar 8, nukewar 7, airwar 1) | §2 CONFIRMED | Those maps *load* fine — they are simply filtered out of the list, because `mpmodesmd.ini` says Battle/`standard` is the only mode that allows AI players, and skirmish is AI-only. Their mode **rules** (starting cash multipliers, naval-only restrictions, …) are a separate calibration. `build_map_catalog`'s `mode_tag` parameter makes exposing them a one-line change once the rules exist. |
| **LUNAR theater rendering** | §3/§6 CONFIRMED (0 MP maps) | No multiplayer map uses it; the chain row exists with `playable=false` and is rejected. Needed only when campaign missions land (roadmap P10). |
| **Loose `*.map`/`*.mpr`/`*.yrm` from the install directory** | §2 **PROBABLE** (third-party docs only; this install has none) | Nothing on this install to calibrate or test against. |
| **`LocalSize` (playable sub-rect / map border trim)** | §4.2 **PROBABLE** | Semantics from OpenRA/XCC only, not independently exercised. We render the full `Size` rect as today. Affects camera bounds polish, not correctness. |
| **`[Header]` metadata as a fast player-count read** | §4.3 CONFIRMED | Redundant: `[Waypoints]` is already parsed and never disagrees on this install (267/267). Would only be an optimization for listing 91 maps without full parse — solved instead by lazy per-selection parsing. |
| **`[Digest]`, `[Lighting]`, `[SpecialFlags]`, `[Ranking]`** | §4.8 | Both XCC and OpenRA ignore `[Digest]`; the rest are small scalar sections with no consumer yet. |
| **Randomized lobby→start-location assignment** | §4.3 (not recovered) | The original shuffles which lobby slot gets which start waypoint. A byte-identity gate needs determinism; we bind ascending. Re-openable once a seeded match RNG exists. |
| **Identity of the 4 unnamed mix entries / purpose of waypoints > 7** | §1, §4.3 item 4 — **[RE-FALLBACK]** | maps.md recorded what was ruled out; unreachable leftovers and non-script-referenced nodes respectively. Neither blocks anything here. |

## Testing strategy

- **Pure host doctests (no install required):**
  - `tests/test_pkt.cpp` — synthetic pkt INI covering: `[MultiMaps]` ordering, a base name with no matching section, `GameMode` CSV split + lowercasing + `has_mode` case-insensitivity, `MinPlayers`/`MaxPlayers`/`CD` parse, `file_name` = lowercased base + `.map`, absent `Description`.
  - `tests/test_map.cpp` — `[Waypoints]` decode on a hand-built INI: `0=98057` → `(57, 98)`; `1=148099` → `(99, 148)`; keys > 7 present in `waypoints()` but absent from `start_waypoints()`; `start_count()` counts only 0..7; non-numeric key/value skipped without throwing; **a map with no `[Waypoints]` still parses and reports `start_count()==0`** (the campaign/regression guard).
  - `tests/test_theater_name.cpp` — the `TheaterChain` table: `theater_chain("TEMPERATE")` returns exactly `{'T', "isotemp.mix", "temperat.mix", "temperat.ini", "isotem.pal", "temperat.pal", "unittem.pal", "tem", yr_only=false, playable=true}` (the byte-identity pin against `main.cpp`'s current literals); all six rows present; `theater_chain("LUNAR")->playable == false`; unknown name → `nullptr`; lookup is case-insensitive.
  - `tests/test_maplist.cpp` — `build_map_catalog` against a synthetic pkt + a stub archive: the existence filter drops a listed-but-absent map; the mode filter keeps only `standard`; CSF resolution supplies `display_name` and falls back to `base_name` when the label is missing; the sort is stable and deterministic across two calls.
- **Real-asset tests (guarded on `RA2_INSTALL_DIR`, `MESSAGE("skipped: …"); return;` without it):**
  - real `langmd.mix/missionsmd.pkt` parses; **161** entries; `XMP01T4`'s row == `Description=DESC:MP01DU`, `MinPlayers=2`, `MaxPlayers=4`, `GameMode` contains `standard` (§2 excerpt).
  - real catalog: every entry exists in `multimd.mix` (0 dropped by the existence filter on this install); the `standard` catalog has **91** entries; `DESC:MP01DU` resolves to `"South Pacific (2-4)"`.
  - real `multimd.mix/xmp01t4.map` parses to `TEMPERATE`, `105x95`, `19,855` cells (§4.2's `(2w−1)h`), `start_count()==4`, and waypoint `0` decodes to `(57, 98)`.
  - real `multimd.mix/xmp31s2.map` (the gate map) → `SNOW`, `70x83`, `start_count()==2`.
- **New 19th gate `--demo-maps`** (component H): catalog + SNOW map load + real waypoint starts + `launch_skirmish` + 300 fixed `dt=1/15` frames, two-run rendered/headless self-compare, wedge guards, no pinned literal (README pins). Its hash is **INSTALL-SPECIFIC** and the README row must say so (the `--demo-roster` precedent).
- **The 18 existing gates stay byte-identical.** Full re-verify at the theater-generalization commit (the highest-risk touch — `load_assets` + both art loaders), again at the start-placement commit (where `--demo-skirmish` in particular must still read `e9ad3f1fd88495f9`), and at finalize — **ONE GATE PER FOREGROUND CALL** (each is ~2 min / ~3 GB; batching exceeds the tool timeout and reads as a false hang).
- **UI is not hash-gateable.** `SkirmishSetupScene`'s list is validated by (a) a clean build, (b) the gate exercising the catalog + load path deterministically, (c) a finalize screenshot of the setup screen showing real map names, and (d) an interactive launch of a non-temperate map for a visual check that the theater art is right.
- **arm64:** `android; ./gradlew.bat assembleDebug` must stay green and link the new symbols (`PktFile`, `build_map_catalog`, `theater_chain`).

## [RE-FALLBACK] / PROBABLE items for the Task 1 RE-GATE to pin

Task 1 is a **confirmatory RE-GATE adjudication** using `ra2extract` + the repo's own `libra2assets.a` against the real install — **not a fresh hunt**. It writes a `§9 Adjudication (RE-GATE)` section into `docs/re/maps.md` and must close these **before any code depends on them**:

1. **YR NewTheater art letters** for `NEWURBAN` / `DESERT` / `LUNAR`. maps.md §6 pins the tile mix / control INI / iso palette / extension but **not** the letter that `theater_candidates`' recovered munge substitutes at `name[1]`. Only `'T'`/`'A'`/`'U'` are recovered (`theater_name.hpp`). Probe: resolve `g?cnst.shp` / `n?cnst.shp` variants inside the YR art mixes. FALLBACK if unresolvable: `'N'`, `'D'`, `'L'` tagged `[RE-FALLBACK]` in the table (the generic-`'g'` retry already covers a wrong letter with a visible-but-non-crashing degrade).
2. **Per-theater building-art mix names.** §6 says "`temperat/snow/urban/urbann/desert/lunar.mix` building-art mixes" but also lists `des.mix`/`lun.mix`/`ubn.mix` as marker mixes. Pin which name `MixFile::open_nested` must actually use per theater (and in which parent — `ra2.mix` vs `ra2md.mix`).
3. **Per-theater theater palette + unit palette** (`temperat.pal` / `unittem.pal` equivalents) consumed by `load_ore_art` and `load_building_assets`. Not enumerated in §6 beyond "Also present per theater: … `unit*.pal`, `lib*.pal`, `city*.pal`".
4. **The `.sno`/`.urb`/`.ubn`/`.des` render chain** — §6/§8 mark it **PROBABLE**. Prove it: construct `ra2engine::Theater` for SNOW, URBAN, NEWURBAN and DESERT from the pinned chain and confirm `template_count() > 0` and that a sampled real map's tiles resolve to non-null `TmpFile`s. This is what flips §8's row from PROBABLE to CONFIRMED.
5. **Lobby-slot → start-waypoint assignment order** — not recovered anywhere in maps.md. Adjudicate the deterministic ascending binding (combatant *k* → `start_waypoints()[k]`) and record the original's randomized assignment as `[RE-FALLBACK]`.
6. **Catalog census re-verification** so the gate's wedge bounds are real, not remembered: 161 pkt-active entries, 0 missing from `multimd.mix`, **91** `standard`; and `xmp31s2.map` = SNOW / 70x83 / 2 starts / `standard` (the gate map's facts).
7. **CSF resolution path** — confirm `ra2md.csf` is reachable as a `langmd.mix` entry via the engine's existing `CsfStrings` loader and that `DESC:`/`NAME:` labels resolve case-insensitively (§2 CONFIRMED for the game; this pins *our* access path).
8. **`LocalSize` disposition** — §4.2 PROBABLE. Adjudicate explicitly: **ignored this phase**, full `Size` rect rendered as today.
