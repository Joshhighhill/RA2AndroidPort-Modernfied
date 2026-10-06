# P7 Save / Load — design

RE: `docs/re/saveload.md`. Roadmap: P7 (prerequisite for P10 campaign).

## 1. Goal

`load(save(state))` continues **bit-identically**: a game saved at frame N and
loaded into a fresh process produces exactly the state the uninterrupted game
reaches at every later frame. Covered state:

* **World** — every member except the `MapFile` pointer: units (incl.
  cargo/flight/naval/capture/disguise/veteran fields), buildings (repair/sell
  state), houses + alliances + victory, credits/blackout/veteran flags,
  ore field + ore RNG, shroud grids + SpySat latches, occupancy +
  passability, AI (base/team/trigger state *and* the resolved AI config),
  superweapon registry + timers, projectiles in flight + `ProjRng`, all
  firewall bools, pending event buffers, rosters handed to the World
  (`types_`, `lookup_types_`, survivor crews, anim table).
* **Map-trigger runtime** (`MapTriggerSystem`) — tags/triggers/timers/
  latches/locals/globals/house lists/mission timer/outcome/RNG/log/counters.
  The parsed `MapTriggerData` is NOT saved (it is re-derived from the map,
  exactly like the original re-reads the scenario INI); a header check
  (trigger/tag counts) rejects a mismatched map.
* **GameScene / UI** — the skirmish config that rebuilds the scene's assets,
  camera, active sidebar tab, the player's `ProductionQueues`, wallet
  (`player_credits_`), rolling credits display, the pending placement queue
  category, and the FreeUnit tracker. Transient input (drag box, armed
  repair/sell/placement cursor, hover) is reset to idle on load — the original
  also returns from the dialog with no armed mode.

## 2. Format decision

The original's payload is an OLE compound file of raw `IPersistStream` class
memory images (saveload.md §4) — not reproducible and not portable. **We keep
the original's naming, header-first listing and UI flow, and use our own
binary format:**

```
"RA2SAVE\0"                 8 bytes magic
u32 format_version          kSaveFormatVersion (bump on ANY schema change)
u32 header_len, header      description (<=80 chars, UTF-8), scenario kind,
                            map_file, map_id, map signature (w,h,cells),
                            game frame, unix time
{ u32 tag, u64 len, bytes } sections, any order: 'CONF' skirmish config,
                            'WRLD' world, 'TRIG' map triggers, 'SCNE' scene,
                            'HOST' gate host extras. Unknown tags skipped.
```

All scalars little-endian fixed width (`bool`->u8, `int`->i32, `long`->i64 —
`long` differs between Win64 and arm64 — `size_t`->u64, `double` bit-exact
u64, enums -> i32). A save from a different `format_version` is listed but
refused (the original's `IsWrongVersion`). Files: `SAVE%04X.SAV` with a random
16-bit number retried until free, in `<SDL pref dir>/Saved Games/` (the
install dir is read-only on Android).

## 3. Serializer structure (design for growth)

`engine/savegame.hpp/.cpp`:

* `save::Writer` / `save::Reader` archives; one generic `io(ar, x)` handles
  arithmetic, enums, `std::string`, `std::vector`, `std::array`, C arrays,
  `std::map`, `std::pair`, and any struct with a `fields(ar, T&)` overload.
* **One visitor per struct, one line per field**, shared by save and load:
  `template <class Ar> void fields(Ar& ar, Unit& u) { io(ar, u.cell_x); ... }`.
* Classes with private state get ONE line: `friend struct SaveAccess;`
  (World, OreField, Occupancy, ProductionQueues, MapTriggerSystem).
* Saving is `const` w.r.t. simulation (the writer only reads) — existing
  gates are untouched.

### Field-coverage tripwires (tests in `tests/test_saveload.cpp`)

1. **Aggregate arity**: for every serialized aggregate struct
   (Unit, Building, BuildingTypeDef, UnitCombat, ProjectileSpec, Projectile,
   QueueSlot, AiBaseState, TeamInstance, ...), a test compares the number of
   members the compiler sees (brace-init arity) with the number the visitor
   visits. **Adding a field to any of them without a visitor line fails the
   test.**
2. **World sizeof** (desktop MinGW x64 ABI only): `sizeof(World)` and the
   other private-state classes are pinned; a new World member changes it
   (a lone `bool` may hide in padding — tripwire 3 covers behavior).
3. **Round-trip determinism**: a richly populated World (cargo, flight,
   naval, projectiles in flight, SW, shroud, AI, repair/sell, capture,
   disguise, ore) is saved, loaded into a fresh World, both are stepped
   K frames, and their saves must be byte-identical; save(load(bytes)) ==
   bytes.

## 4. UI (original flow, our widgets)

* In-game options overlay (dialog 0xB5): **Load / Save / Delete / Abort /
  Resume** (Controls is P8's). Opened by the sidebar Options button and
  `Esc` when no mode is armed [FALLBACK: hotkey not traced].
* Load/Save/Delete dialog (saveload.md §2): newest-first list, Save mode adds
  a `<New Saved Game>` row and an 80-char description edit defaulting to the
  scenario name; empty description refused; overwriting asks to confirm;
  wrong-version rows refused on Load; Delete confirms.
* Main menu "Load Saved Game" (now enabled) opens the same dialog in Load
  mode; loading builds a `GameScene` from the save.

## 5. Gate `--demo-saveload`

Two scenarios, each: run to frame N, save to bytes (and to a file), continue
to frame M -> full-state hash **A**; separately construct a fresh scenario,
load the save, run to M -> hash **B**; require A==B. The hash is FNV-1a over
the serialized World (+ triggers + scene) bytes, i.e. *complete* state, mixed
with `world_hash`.

* **Skirmish**: `launch_skirmish` 2 houses (house 1 AI), economy wired,
  projectiles enabled, player production queue running (GameScene's
  `prod_tick` path), N=450, M=900.
* **Triggers**: the P9 sov07tmd world + `MapTriggerSystem`, N=900, M=1800.

Prints `saveload_hash=<A of skirmish ^ A of triggers>`; exit 1 on any
mismatch or a vacuous run (no projectile/production/trigger activity).

## 6. Checklist — state other branches must add

**Status at P7 finalize (after merging master fc36b96):** P4 abilities
(Unit ability fields, Building C4/bomb/erase fields, `RadField` + sites,
ability profiles/rules/frame/events), P5 veterancy + crates (`UnitVet`,
Building vet/kill fields, `VeterancyRules`, crate list + `GameRandom` + crate
pool/events, vet frame/events, new `UnitCombat` stats) and P6 roster2 (no new
persistent World state; `block_water_footprint` only edits the serialized
passability grids) are **all serialized** and covered by `--demo-saveload`.

How to add state (anyone, from now on): add **one `io(ar, x.field)` line** in
the matching visitor in `engine/save_fields.hpp` (or `SaveAccess::world` for a
World member), then bump `save::kFormatVersion`; a new struct type needs a
`fields` overload + a `CHECK_ARITY` line. `tests/test_saveload.cpp` names the
struct / member that is out of sync.

Remaining:

* **P10 campaign**: campaign/mission identity in the save **header**
  (`ScenarioKind::Campaign`, mission id, difficulty); GameScene must own the
  `MapTriggerSystem` + its host and write the `TRIG` (and `HOST` for host-side
  state such as `unit_type_of`) sections — the load path re-parses
  `MapTriggerData` from the mission map exactly as the gate does; mission
  timers/briefing/outcome state in the scene record; default description =
  the mission's CSF `UIName` (`%sSav`, saveload.md §3).
* **Anyone adding a GameScene member** that affects simulation: add it to
  `SceneSave` (`fields(SceneSave)`) and to `GameScene::capture_save/apply_save`.

## 7. Cuts / deferrals

* Map-trigger runtime is saved/loaded and gated, but the interactive
  GameScene does not run map triggers yet (P10 wires it); the in-game save
  therefore has no `TRIG` section today.
* Audio playback state (playing voices, music position) is not saved —
  sounds restart cleanly, as in the original.
* No bar-graph / "Saving game..." progress banner; save is instantaneous.
* Free-disk-space check (`SpaceRequirement`) omitted.
* Abort Mission exits the game (GameScene cannot reach the menu art to build a
  ShellScene); visuals are rect-font widgets, not the original dialog art.
* The in-game menu opens from the sidebar Options button or F10 (Escape stays
  the app's Quit, handled by the scene driver) [FALLBACK: hotkey not traced].
