#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "ai_base.hpp"  // AI-1 base-planning: AiBaseState/BaseView/next_build/...
#include "ai_team.hpp"  // AI-2 offensive-teams: AiTeamState/TeamInstance/TeamView/...
#include "ai_trigger.hpp"  // AI-3 trigger engine: AiTriggerState/TriggerView/TriggerDecision
#include "building.hpp"
#include "occupancy.hpp"
#include "ore.hpp"  // OreField/OreKind/OreRng (Economy phase)
#include "pathfind.hpp"  // Cellxy (path storage / find_path)
#include "production.hpp"  // AI-1 base-planning: World owns per-house ProductionQueues
#include "ra2assets/land_type.hpp"  // CB-3 Naval: LandType (build_water_passability)
#include "repair_sell.hpp"  // Repair & Sell: RepairSellRules + the pure tick arithmetic
#include "superweapon.hpp"  // CB-4 Superweapons: Superweapon/SuperweaponType lifecycle
#include "visibility.hpp"  // Shroud/Fog: cell_in_sight/reveal_disc (pure reveal math)
#include "ra2assets/map.hpp"
#include "ra2assets/pal.hpp"  // ra2assets::Palette (house_palette render entry)
#include "unit_combat.hpp"  // UnitCombat (moved here to break the include cycle
                            // production.hpp -> unit_type.hpp -> world.hpp)

namespace ra2engine {

// Forward declarations only: OreValues and HarvestEvent are DEFINED in
// harvest.hpp, which itself includes world.hpp (harvest.hpp needs Unit,
// defined below) -- a real #include cycle, so world.hpp cannot include
// harvest.hpp back. These two structs appear here ONLY in function
// declaration signatures (set_economy's by-value parameter, take_harvest_
// events' by-value return), which is legal with just a forward declaration
// (no instantiation is forced by a mere declaration); World stores NO member
// of either type directly -- set_economy copies OreValues's two ints into
// private int fields, and take_harvest_events converts a private POD event
// buffer (World::PendingHarvestEvent, fully defined inline, see below) to/
// from real HarvestEvent objects, entirely inside world.cpp where
// harvest.hpp IS included. Any caller of these methods must include
// harvest.hpp itself to get the complete types (main.cpp / tests already do,
// or will).
struct OreValues;
struct HarvestEvent;

// Harvester mission states [RE: economy.md §1]. Lives here (not harvest.hpp)
// because Unit carries one; harvest.hpp includes world.hpp.
enum class HarvState : std::uint8_t {
    None,          // not a harvester / no mission
    SeekOre,       // choosing an ore cell (rescan)
    MoveToOre,     // pathing to harv_cell
    Harvesting,    // parked on ore, gulping
    ReturnToRef,   // driving (HARV) or warping (CMIN) home
    WaitDock,      // at the refinery, dock busy
    Unloading,     // draining bales into credits
};

// CB-2 Aircraft [RE: aircraft.md]: the flight-locomotor state machine. A
// Grounded unit is on the map (holds its cell); TakingOff ascends to cruise
// (ground cell released); Cruising translates straight-line at altitude
// holding no cell; Descending lowers toward the ground. Order-driven
// transitions (Grounded->TakingOff, Cruising->Descending) are set by World;
// altitude-driven ones (TakingOff->Cruising, Descending->Grounded) by
// flight::advance_flight_state. Default Grounded is INERT (spec §8).
enum class FlightState { Grounded, TakingOff, Cruising, Descending };

// CB-3 Naval [RE: naval.md §1]: MovementDomain { Land=0, Water=1 } is defined
// in unit_combat.hpp (UnitCombat carries a field of it, and that header is
// included above at line 17). world.hpp sees it transitively -- Land (the
// DEFAULT, every existing unit) resolves to the UNCHANGED passable_ grid via
// grid_for; Water resolves to water_passable_.

// A single visible/controllable actor in the world: its map cell + height
// level (raw IsoMapPack5 coordinate space -- see ra2assets::MapCell), the
// SHP base image name (a `Rules::image()` result, e.g. unit id "E1" ->
// "GI" -> GI.SHP), and the current animation frame index into that SHP.
//
// Task 12 adds selection + movement: `path` is the A* result (from the
// unit's cell to the ordered goal, inclusive of both endpoints), `path_idx`
// is the index of the NEXT cell being advanced toward (path[0] is always the
// cell the unit started the order on, so movement begins at index 1), and
// `fx`/`fy` are the CONTINUOUS iso-cell position used for smooth
// interpolated rendering between cells (integer `cell_x`/`cell_y` only
// update as each cell is reached, so game logic/hit-testing stays on a
// clean grid). `anim_time` drives the walk-frame cycle. `selected` is the
// UI selection flag.
struct Unit {
    int cell_x = 0;
    int cell_y = 0;
    int level = 0;
    int frame = 0;
    std::string image;

    // --- Task 12: selection + movement/animation state ---
    bool selected = false;
    std::vector<Cellxy> path;  // full A* path (start..goal); empty when idle
    std::size_t path_idx = 0;  // index of the next path cell to advance toward
    double fx = 0.0, fy = 0.0;  // continuous iso-cell position (interp render)
    double anim_time = 0.0;     // accumulated time for walk-frame cycling
    double wait_time = 0.0;     // seconds blocked at current cell (collision re-path timer)

    // 8-way facing block index, 0=N going COUNTERCLOCKWISE on screen to 7=NE
    // (docs/re/infantry-anim.md §4-6); spawns facing S (4).
    int facing = 4;

    bool moving() const { return !path.empty() && path_idx < path.size(); }

    // --- Combat (this phase). Stats resolved once at spawn from Rules. ---
    int house = 0;                 // 0 = player/Allied, 1 = hostile/Soviet
    int hp = 1, max_hp = 1;
    int armor_ix = 0;              // armor_index(Armor=)
    int w_damage = 0;              // weapon Damage=
    int w_rof = 0;                 // weapon ROF= (15Hz logic frames)
    double w_range = 0.0;          // weapon Range= (cells)
    std::vector<int> w_verses;     // warhead Verses percents (11 entries)
    double w_cell_spread = 0.0;    // warhead CellSpread (cells) [aircraft.md §4]
    double w_percent_at_max = 1.0; // warhead PercentAtMax (edge damage scale)
    // Projectile phase (P2) [RE: projectile.md]: the primary weapon's projectile
    // (default kind=Instant = the pre-P2 instant-hit path). Read ONLY when the
    // World's projectiles_enabled_ firewall is open; NOT read by world_hash.
    ProjectileSpec w_proj;
    int target = -1;               // unit index being attacked, -1 none
    int target_building = -1;      // building index being attacked, -1 none;
                                    // mutually exclusive with `target` --
                                    // setting one clears the other (Buildings
                                    // & Construction Task 4).
    bool attack_order = false;     // explicit order (pursues) vs auto-acquire
    double rof_cool = 0.0;         // seconds until the weapon may fire again
    double fire_flash = 0.0;       // seconds of fire-anim left (renderer)
    int guard_x = 0, guard_y = 0;  // guard anchor (cell when target auto-acquired)
    bool alive = true;             // TOMBSTONE: false after die anim; never erased
    double die_time = -1.0;        // >=0: dying (die anim playing); alive stays true until it ends

    // --- Voxel Vehicles phase: continuous-facing vehicle state ---
    // Facings live in the original's 16-bit space [0, 65536); 32768 faces
    // screen-South like the infantry spawn (docs/re/voxel-render.md §2/§5).
    bool is_vehicle = false;
    double body_facing = 32768.0;
    double turret_facing = 32768.0;
    double rot_u16_per_sec = 0.0;  // ROT * kRotU16PerRotPerSec
    double speed_cps = 0.0;        // per-unit speed; 0 = use World move_cps

    // --- Economy phase: harvester state [RE: economy.md §1-§3] ---
    bool is_harvester = false;
    bool chrono_miner = false;       // CMIN: warps home instead of driving
    int storage_bales = 0;           // Rules Storage= (HARV 40, CMIN 20)
    double bales_ore = 0.0;          // current load, ore bales
    double bales_gems = 0.0;         // current load, gem bales
    HarvState harv = HarvState::None;
    int harv_cell_x = 0, harv_cell_y = 0;   // target/last ore cell
    // True once harv_cell_x/y have been stamped with a REAL cell (SeekOre
    // finding ore, or a player's order_harvest click) -- Review Important-3:
    // a freshly spawned CMIN manually returned before ever finding ore has
    // harv_cell_x/y at their (0,0) default, which is not a place worth
    // warping back to. Deliberately never cleared back to false once set
    // (a miner that has found ore once always has a legitimate fallback
    // cell to return to).
    bool has_harv_cell = false;
    int dock_refinery = -1;          // target/docked refinery Building index:
                                     // stamped when ReturnToRef picks it, held
                                     // through WaitDock/Unloading (-1 = none,
                                     // or CMIN post-unload warp in transit)
    double harv_timer = 0.0;         // gulp/unload/rescan/warp accumulator
    bool manual_hold = false;        // player move order: pause auto-resume until arrival

    // --- CB-1 Transports/Cargo [RE: transport-cargo.md] ---------------------
    // ALL defaults INERT (spec §8): no existing demo issues a load/unload
    // order and no existing spawn path sets passengers_max > 0, so every
    // field below holds its default in every existing demo; world_hash reads
    // only cell/hp/alive and is untouched. cargo/cargo_of/enter_target store
    // unit INDICES -- stable for a World's lifetime (units are tombstoned,
    // never erased; the same invariant `target` relies on).
    int passengers_max = 0;      // capacity COUNT (Rules Passengers= bodies);
                                 // 0 = not a transport [RE-GATE APPLIED:
                                 // transport-cargo.md §1 -- NumPassengers <
                                 // Passengers, a body count, NOT a Size pool]
    int size_limit = 0;          // largest admissible passenger Size
                                 // (SizeLimit=) -- a PER-PASSENGER cap [§1]
    int size = 1;                // own bulk (Size=); feeds ONLY the pip display
                                 // (cargo_total_size) [RE-GATE APPLIED:
                                 // transport-cargo.md §1 -- absent-key defaults
                                 // CONFIRMED: Passengers/SizeLimit -> 0,
                                 // Size -> 1; Task 4 resolves real values]
    std::vector<int> cargo;      // carried unit indices in boarding order;
                                 // ejects pop the BACK (LIFO [RE-GATE APPLIED:
                                 // transport-cargo.md §6])
    int cargo_of = -1;           // >=0: index of the transport carrying this
                                 // unit -- the unit is OFF-MAP (spec §4)
    int enter_target = -1;       // pending enter order -> transport index
    bool unload_pending = false; // pending unload order on this transport
    int eject_counter = 0;       // eject_cell scatter-side alternator (+/-90 deg
                                 // by low bit), bumped once per eject
                                 // [transport-cargo.md §4]

    // --- CB-2 Aircraft/Flight [RE: aircraft.md] -----------------------------
    // ALL defaults INERT (spec §8): no existing demo spawns an aircraft and no
    // existing spawn path sets is_aircraft=true, so every field holds its
    // default in every existing demo; world_hash reads only cell/hp/alive and
    // is untouched (strategy (a)). altitude is DERIVED into a screen offset at
    // draw time -- no stored screen offset.
    bool is_aircraft = false;                          // flight-locomotor unit
    double altitude = 0.0;                             // leptons; 0 = on the ground
    FlightState flight_state = FlightState::Grounded;  // Grounded/TakingOff/Cruising/Descending
    double cruise_altitude = 0.0;                      // JumpjetHeight (leptons)
    double climb_rate = 0.0;                           // JumpjetClimb (leptons/sec)
    double descend_rate = 0.0;                         // JumpjetCrash (leptons/sec)
    double flight_speed = 0.0;                         // cruise speed (cells/sec)
    double flight_turn = 0.0;                          // heading turn (u16/sec)
    bool balloon_hover = false;                        // never lands
    bool omni_fire = false;                            // fire without turning
    bool has_flight_goal = false;                      // a cruise goal is set
    int flight_goal_x = 0, flight_goal_y = 0;          // cruise goal cell (NOT A*)

    // --- CB-3 Naval/Water [RE: naval.md §1] ---------------------------------
    // ALL defaults INERT (spec §8): no existing demo spawns a naval unit and no
    // existing spawn path sets is_naval=true, so every field holds its default
    // in every existing demo; world_hash reads only cell/hp/alive and is
    // untouched (strategy (a)). A naval unit reuses path/path_idx/fx/fy/
    // cell_x/cell_y/occ_ exactly like a ground vehicle; only the GRID its A*
    // reads differs (movement_domain -> grid_for).
    bool is_naval = false;                                // naval-locomotor unit
    MovementDomain movement_domain = MovementDomain::Land;  // Water for a naval unit

    // --- Engineer/Spy Capture phase [RE: capture.md] ------------------------
    // Default INERT (spec §8): no existing demo issues an engineer capture order
    // and no spawn path sets it, so this field holds -1 in every existing demo;
    // world_hash reads only cell/hp/alive and is untouched. Mirrors enter_target
    // (a pending-order building INDEX, -1 = none). The step_capture arrival pass
    // is a strict no-op for every unit with capture_target < 0.
    int capture_target = -1;     // pending engineer capture/repair order ->
                                 // building index (-1 = none)
    // Spy infiltration order [RE: capture.md §B]: same deferred move-adjacent-
    // then-act model as capture_target, but the effect (infiltrate_building) is
    // resolved-purpose-driven -- the InfiltratePurpose is decided at the gesture
    // layer (World is asset-free) and stored here PRE-RESOLVED, so step_capture
    // acts on arrival without the roster. Default INERT: -1 target + None(0)
    // purpose -> the step_capture spy pass is a strict no-op in every existing
    // demo; world_hash reads none of these. infiltrate_purpose holds the int
    // value of World::InfiltratePurpose (defined below Unit; 0 = None).
    int infiltrate_target = -1;         // pending spy infiltrate -> building idx
    int infiltrate_purpose = 0;         // resolved InfiltratePurpose as int (0=None)
    double infiltrate_steal_pct = 0.5;  // SpyMoneyStealPercent (arrives pre-resolved)
    int infiltrate_blackout = 1000;     // SpyPowerBlackout (arrives pre-resolved)

    // --- Engineer/Spy Capture phase [RE: capture.md §B5] --------------------
    // Instance veteran rank (copied from UnitCombat at add_combat_unit). Inert
    // default (0) -> NOT read by world_hash (cell/hp/alive only); hashed only by
    // capture_hash. Recorded so the gate can fold a produced unit's rank.
    int veteran_rank = 0;

    // --- Shroud/Fog phase [RE: shroud.md §2/§3] ---
    int sight = 0;                 // reveal radius (cells); copied from UnitCombat
    std::uint8_t seen_by_mask = 0xFF;  // bit h set iff this unit's cell is visible
                                       // to house h. DEFAULT 0xFF = seen by all =
                                       // INERT (the combat.cpp:362 clause never
                                       // fires). Overwritten only by step_visibility
                                       // (behind shroud_any_). NOT read by world_hash.

    // --- Engineer/Spy Capture phase: disguise [RE: capture.md §C] ---
    // A disguised unit RENDERS as `disguised_as_type` (a SHP image id) in
    // `disguised_as_house`'s palette; a still-disguised enemy spy is treated as
    // FRIENDLY -- the step_combat idle auto-acquire scan (combat.cpp) skips any
    // candidate with disguised==true, so no enemy unit auto-fires on it (an
    // explicit order_attack is unaffected -- the skip lives only in the auto-
    // acquire scan, not valid_target). detect_disguise units (dogs) clear the
    // flag (step_tech_structures), restoring targetability. ALL inert defaults
    // -> NOT read by world_hash; hashed only by capture_hash.
    bool disguised = false;
    std::string disguised_as_type;   // SHP image id shown while disguised (e.g. "E1")
    int disguised_as_house = 0;      // palette house shown while disguised
    bool detect_disguise = false;    // DetectDisguise=yes (dogs) -> reveals spies
};

// One cargo event (board / capacity-refusal / eject), drained by callers per
// frame via World::take_cargo_events -- the take_harvest_events pattern:
// main.cpp turns these into demo witness lines and the guarded Enter/
// LeaveTransportSound hook [RE: transport-cargo.md; spec §6/§7]. Defined
// HERE (not cargo.hpp) so World can store a vector without an include cycle.
struct CargoEvent {
    enum class Kind { Board, Refuse, Eject };
    Kind kind = Kind::Board;
    int unit = -1;       // the passenger
    int transport = -1;  // the transport
    // Board/Refuse: x = the passenger COUNT after the event resolved
    // (capacity is a COUNT [RE-GATE APPLIED: transport-cargo.md §1]), y =
    // the transport's passengers_max (the "K/5" witness pair). Eject:
    // (x, y) = the eject cell.
    int x = 0, y = 0;
};

// Per-image (per-unit-type), facing-aware animation data -- REPLACES the
// Task 7 empirically-eyeballed table, which had two confirmed frame bugs
// (CONS fire played half of Die2's frames; die played Idle2/Prone frames).
// Calibrated against the ORIGINAL game's artmd.ini GISequence/ConSequence
// data; docs/re/infantry-anim.md is the calibration authority -- see it for
// the extracted values, the frame-selection formula's derivation, and the
// facing-block-order evidence (§1-5).
//
// A SeqEntry mirrors one artmd.ini `Name=StartFrame,FrameCount,
// FacingMultiplier` sequence entry (§1); frame selection is
// `first + facing*fmult + phase` (see seq_frame below). `fmult == 0` means
// facing-independent (deaths use one shared animation, no per-facing block).
struct SeqEntry {
    int first = 0;
    int count = 1;
    int fmult = 0;
};

// One unit-type's stand/walk/fire/die sequences.
struct UnitAnimSeq {
    SeqEntry stand, walk, fire, die;
};

// Per-image animation table: Unit::image ("GI", "CONS", ...) -> its
// UnitAnimSeq. Populated from artmd.ini by main.cpp (Task 4 wires
// ra2assets::Art in); declared here (not file-static) so both World and
// tests/test_world.cpp's pure anim-selection tests can build/pass one.
using AnimTable = std::map<std::string, UnitAnimSeq>;

// The real GI/CONS artmd.ini values (docs/re/infantry-anim.md §2). Serves as
// the fallback for any image not registered in an AnimTable -- including
// every test that builds a World without wiring artmd.ini at all -- so
// unregistered images still animate with the REAL values instead of a
// crash/UB or stale placeholder.
const UnitAnimSeq& default_anim();

// Looks up `image` in `t`; falls back to default_anim() on a miss (documented
// -- see default_anim() above). Defined in world.cpp; declared here so both
// step_units/World::step and tests/test_world.cpp can call it.
const UnitAnimSeq& anim_for_image(const AnimTable& t, const std::string& image);

// frame = e.first + facing*e.fmult + phase, with phase clamped to
// [0, e.count-1] (docs/re/infantry-anim.md §1's
// `StartFrame + facing*FacingMultiplier + phase` formula; `phase` here is
// the doc's "frame offset within the sequence", renamed to avoid confusion
// with the FrameCount field).
int seq_frame(const SeqEntry& e, int facing, int phase);

// Cell-space direction (dx, dy) -> facing block index (0=N, counterclockwise
// on screen to 7=NE; docs/re/infantry-anim.md §4/§5). Returns -1 for a zero
// vector (no direction -- caller should keep the unit's current facing).
// Takes continuous (double) input so a move segment's ray direction resolves
// the same way regardless of its magnitude.
int facing_from_delta(double dx, double dy);

// --- Voxel Vehicles phase: continuous 16-bit-space facing helpers ---
// (docs/re/voxel-render.md §5). Facings are in [0, 65536); the 8 infantry
// blocks sit at k*8192.

// ROT rate scale: Set_ROT applies ROT*256 u16 per 15 Hz logic frame, i.e.
// ROT*256*15 u16/sec (voxel-render.md §5). ROT=5 -> 19,200 u16/sec ->
// ~3.4 s per full circle.
constexpr double kRotU16PerRotPerSec = 256.0 * 15.0;

// Aim tolerance for rotation-complete fire gating (voxel-render.md §5):
// vanilla holds fire until the timed turret rotation COMPLETES (exact facing,
// via Is_Rotating). turn_toward clamp-lands exactly on its target and
// step_turrets/step_combat compute `want` from the same positions in the same
// frame, so exact double comparison (`<= 0.0` on the shortest-arc magnitude)
// is well-defined. Same rule gates the hull turn-then-move.
constexpr double kAimToleranceU16 = 0.0;

// Continuous facing (u16) from a cell-space direction, consistent with
// facing_from_delta's 8-block convention (block k spans k*8192 +/- 4096).
double facing_u16_from_delta(double dx, double dy);

// Shortest-arc step in u16 space with wraparound, clamped to max_step;
// returns the new facing (== target when the arc magnitude <= max_step),
// always normalized to [0, 65536).
double turn_toward(double facing, double target, double max_step);

// True when the shortest arc from u.turret_facing to target_facing is within
// kAimToleranceU16 (rotation-complete gate).
bool vehicle_aimed(const Unit& u, double target_facing);

// Advance every moving unit toward its `path` by `dt` seconds at `move_cps`
// cells/sec, using `occ` to enforce one-unit-per-cell: a unit reserves the next
// path cell before entering it. While that cell is held by another unit the
// unit waits (resuming the instant the cell frees) and attempts a re-path
// around occupied cells on a seniority-staggered cadence -- a unit blocked by
// a LOWER-index unit tries at kBlockedWait, one blocked by a HIGHER-index
// unit one interval later (breaks the mirror-symmetric re-path livelock); it
// abandons the order only after kGiveUpAfter seconds of continuous block
// (boxed-in / sealed corridor).
// Idle units re-assert ownership of their current cell and stand. Units are
// processed in ascending index order (deterministic). `anims` resolves each
// unit's per-image stand/walk sequence (anim_for_image) and its facing
// (Unit::facing, updated here as each walk segment is selected --
// docs/re/infantry-anim.md §5) drives frame selection (seq_frame). Does NOT
// modify Unit::level -- rendering height is patched separately from the map
// by World::step. Pure: takes a passability grid + occupancy, no MapFile/SDL.
// CB-3 Naval [RE-GATE APPLIED: naval.md §1/§3]: `water_passable` is the SECOND
// grid (immediately after `passable`) -- the blocked re-path selects it per unit
// (u.is_naval ? water_passable : passable). Every existing caller passes the
// (empty in every existing demo) water grid and no existing (Land) unit reads
// it, so ground re-pathing is byte-identical.
void step_units(std::vector<Unit>& units, const std::vector<std::uint8_t>& passable,
                const std::vector<std::uint8_t>& water_passable, int w, int h,
                Occupancy& occ, double dt, double move_cps, const AnimTable& anims);

// Slew each alive, non-dying vehicle's turret_facing toward its target (via
// turn_toward at rot_u16_per_sec*dt) when it has a live target, else toward
// its body_facing (docs/re/voxel-render.md §5). Non-vehicles are skipped.
// Called from World::step between step_units and step_combat so the fire
// gate (vehicle_aimed) sees the turret's post-slew facing this frame. Pure.
//
// Buildings & Construction Task 4 fix: a unit with a live `target_building`
// (alive, non-Dead) also slews, toward the NEAREST foundation cell of that
// building -- same nearest-cell rule combat.cpp uses for range -- mirroring
// the unit-target branch above it. With `buildings` empty (every call site
// predating this fix, and any test that doesn't attack a building) this new
// branch is unreachable, so behavior is byte-identical to the old 2-arg
// signature.
void step_turrets(std::vector<Unit>& units, const std::vector<Building>& buildings, double dt);

// Picks a passable (Clear/grass, `tile == 0` -- confirmed in Task 9 /
// task-9-report.md: "tile index 0 is Clear") cell close to the centroid of
// every Clear cell on the map. That centroid sits in the map's open,
// populated interior (biased away from edges/water/cliffs), and -- because
// `cell_to_screen` is linear in (x, y) -- projects close to the same region
// `terrain_screen_bounds`'s bounding-box center does, so a unit placed here
// lands inside whatever viewport `main.cpp` centers on the map (Task 9's
// `centered_camera`). Full tile-type passability comes later; this is a
// deliberately simple "any Clear cell near the middle" heuristic.
// Returns nullptr if the map has no cells at all.
const ra2assets::MapCell* pick_start_cell(const ra2assets::MapFile& map);

// Builds the passability grid in RAW IsoMapPack5 (x, y) space -- the SAME
// coordinate space MapCell.x/y and iso_project's cell_to_screen /
// screen_to_cell live in (see map.hpp + iso_project.hpp). The grid is sized
// to cover every cell: grid_w = max(cell.x)+1, grid_h = max(cell.y)+1. A
// cell is passable (1) iff the map materialized a record at that (x, y);
// every un-materialized coordinate stays 0 (impassable / off-map). For this
// vertical slice ALL materialized ground cells are treated walkable (water /
// cliffs / land-type refinement comes later). Returns the flat
// grid_w*grid_h vector; sets grid_w/grid_h to 0 and returns empty if there
// are no cells. Free function (not a World member) so it is unit-testable
// with a hand-built cell vector without constructing a full MapFile.
std::vector<std::uint8_t> build_passability(const std::vector<ra2assets::MapCell>& cells,
                                            int& grid_w, int& grid_h);

// Builds the WATER passability grid in the SAME raw IsoMapPack5 (x, y) space
// and the SAME grid_w*grid_h shape as build_passability -- a cell is water-
// passable (1) iff its resolved LandType is Water AND it is materialized
// (`passable[cell]`), else 0 [RE-GATE APPLIED: naval.md §1/§2 -- BINARY
// passability (the game's Ground[LandType][SpeedType]==0.0 gate), intersected
// with the Land grid so a ship never paths on a non-materialized water cell
// ("Land grid filtered by Float passability"); a speed-weighted cost is a
// carry-forward]. `land_types` is the per-cell LandType array main.cpp resolves
// from the Theater (resolve_land_types), indexed cell = y*grid_w + x, same as
// passable_. `passable` is the UNCHANGED Land grid (build_passability output).
// Cells beyond either array's size stay 0. Free function (not a World member) so
// it is unit-testable with hand-built LandType/passable vectors. Does NOT touch
// passable_ / build_passability -- the Land grid is literally unchanged (spec §8).
std::vector<std::uint8_t> build_water_passability(
    const std::vector<ra2assets::LandType>& land_types,
    const std::vector<std::uint8_t>& passable, int grid_w, int grid_h);

// --- AI-1 base-planning phase [RE: ai-base.md] -----------------------------
// Everything an AI house needs, resolved ONCE by main.cpp (assets/rules) and
// handed to World::enable_ai_house. World stays asset-free: the roster here
// is the same BuildingTypeDef list main.cpp already resolves (ba.roster),
// buildup_totals mirrors ba.art[t].buildup_total, groups mirrors ba.groups.
// --- AI-2 offensive-teams phase [RE: ai-team.md] ----------------------------
// Everything an AI house's TEAM brain needs, resolved ONCE by main.cpp
// (aimd.ini + unit roster + waypoint table) and handed inside AiHouseConfig.
// BYTE-IDENTITY FIREWALL: `enabled` defaults FALSE — every existing
// enable_ai_house caller (including --demo-ai-base) gets a team-less config,
// so step_ai_teams skips the house and all six gates stay bit-identical.
struct AiTeamConfig {
    bool enabled = false;                 // the per-house team firewall
    AimdData aimd;                        // parsed TaskForce/Script/TeamType tables
    std::vector<UnitTypeDef> unit_roster; // producible/recruitable combat types
    std::vector<int> inf_factory_types;   // BUILDING roster indices (barracks)
    std::vector<int> veh_factory_types;   // BUILDING roster indices (war factory)
    std::vector<Cellxy> waypoints;        // waypoint index -> cell (main.cpp table)
    std::string house_name;               // aimd House= token (e.g. "Russians")
    AiTeamRules rules;                    // recovered team knobs

    // --- AI-3 AITriggerTypes [RE: ai-trigger.md] -----------------------------
    // BYTE-IDENTITY FIREWALL: triggers_enabled defaults FALSE — every existing
    // caller (including --demo-ai-team) gets a trigger-less brain, the trigger
    // step never runs, and all SEVEN gates stay bit-identical. Only the
    // interactive path and --demo-ai-trigger set it true.
    bool triggers_enabled = false;  // the per-house trigger firewall
    int trigger_side = 0;           // the house's side for the SideIndex
                                    // filter (1=Allied, 2=Soviet, 3=Yuri;
                                    // triggers with side 0 match any house)
                                    // [RE-GATE APPLIED: ai-trigger.md §5 —
                                    // CONFIRMED 1-based encoding]
    int trigger_tech_level = 10;    // house tech level for the TechLevel
                                    // filter (house >= field passes)
                                    // [RE-FALLBACK: skirmish ceiling — no
                                    // recovered per-house tech input]
};

struct AiHouseConfig {
    AiDifficulty difficulty = AiDifficulty::Normal;  // default group [RE-FALLBACK: ai-base.md §4]
    AiCatalog catalog;
    AiRules ai;
    DifficultyMods mods;  // difficulty_mods(difficulty, groups)
    std::vector<BuildingTypeDef> roster;
    std::vector<double> buildup_totals;  // per roster index (0.0 = instant flip)
    std::vector<std::pair<std::string, std::vector<std::string>>> groups;
    double build_speed = 0.7;            // rules [General] BuildSpeed
    double low_power_penalty = 1.0;      // rules LowPowerPenaltyModifier
    double start_credits = 0.0;          // [RE-GATE APPLIED: ai-base.md §4 —
                                         // NO starting-credit multiplier
                                         // exists in the difficulty sections;
                                         // this is the lobby/MPlayer money,
                                         // passed through unmodified]
    AiTeamConfig team;  // AI-2 [RE: ai-team.md]; enabled=false = no team brain
};

// Holds a reference to the (caller-owned, long-lived) MapFile plus the placed
// Units, the passability grid, and a cell-occupancy grid derived from the
// map's cells. Does not copy the map's cell data -- the caller must keep the
// MapFile alive at least as long as the World. Pure engine-layer logic; no SDL.
class World {
public:
    // P7 Save/Load: the serializer (engine/save_fields.hpp) visits private state.
    friend struct SaveAccess;

    explicit World(const ra2assets::MapFile& map);

    const ra2assets::MapFile& map() const { return *map_; }

    std::vector<Unit>& units() { return units_; }
    const std::vector<Unit>& units() const { return units_; }

    // Append a unit at iso cell (cell_x, cell_y) with SHP image name + frame;
    // seeds its level from the map and claims its cell in the occupancy grid.
    // Returns a reference to the new unit (valid until units() is reallocated).
    // Precondition: (cell_x, cell_y) is walkable and unoccupied -- callers
    // check walkable() and unit_at_cell() first; seeding two units onto one
    // cell silently breaks the one-unit-per-cell invariant.
    Unit& add_unit(int cell_x, int cell_y, const std::string& image, int frame = 0);

    // Append a combat-capable unit at (cell_x, cell_y): mirrors add_unit
    // (image = c.image, frame 0, level seed, occupancy claim) and additionally
    // resolves `house` plus hp/max_hp (from c.strength), armor_ix
    // (armor_index(c.armor)), and the weapon fields (w_damage/w_rof/w_range
    // from c.damage/c.rof/c.range, w_verses = c.verses). Same precondition as
    // add_unit: (cell_x, cell_y) must be walkable and unoccupied -- callers
    // check walkable() and unit_at_cell() first; seeding two units onto one
    // cell silently breaks the one-unit-per-cell invariant.
    Unit& add_combat_unit(int cell_x, int cell_y, int house, const UnitCombat& c);

    // Registers (or replaces) the animation sequence used for Unit::image ==
    // `image`; anim_for_image falls back to default_anim() for any image
    // never registered here (main.cpp/Task 4 wires this from artmd.ini).
    void set_anim(const std::string& image, const UnitAnimSeq& seq);

    // Deselect every unit.
    void clear_selection();
    // Index of the unit whose CURRENT cell is (x, y), or -1 if none -- skips
    // dying and dead units (a corpse is unpickable; its cell was already
    // released by the time it started dying, so it never occupies anyway).
    int unit_at_cell(int x, int y) const;

    // Passability grid accessors (iso space; see build_passability).
    int grid_w() const { return grid_w_; }
    int grid_h() const { return grid_h_; }
    const std::vector<std::uint8_t>& passable() const { return passable_; }
    // CB-3 Naval [RE: naval.md §1]: the passability grid for a movement domain.
    // Land (the DEFAULT -- every existing unit) returns the SAME passable_
    // object every existing A* site already reads (byte-identical); Water
    // returns the new water_passable_ grid. Structural byte-identity: a
    // default-Land unit resolves to the identical grid object (spec §8).
    const std::vector<std::uint8_t>& grid_for(MovementDomain d) const {
        return d == MovementDomain::Water ? water_passable_ : passable_;
    }
    // CB-3 Naval [RE-GATE APPLIED: naval.md §1/§2]: build the WATER grid from a
    // per-cell LandType array (main.cpp's resolve_land_types, indexed y*grid_w+x),
    // intersected with the Land grid passable_ (a ship never paths on a non-
    // materialized water cell). Opt-in -- called ONLY by the naval demo; no
    // existing demo calls it, so water_passable_ stays empty and no naval path is
    // ever computed elsewhere (spec §8). World stays asset-free: it takes the
    // resolved array, not a map.
    void set_water_terrain(const std::vector<ra2assets::LandType>& land_types);
    bool walkable(int x, int y) const;
    const Occupancy& occupancy() const { return occ_; }

    // --- Economy phase [RE: economy.md] ---
    // Ore state built from the ctor map's overlay (empty if the map has
    // none, or the map's has_overlay() is false).
    OreField& ore() { return ore_; }
    const OreField& ore() const { return ore_; }

    // Roster copy + refinery type ids so step() can run harvest logic.
    // `types_.empty()` (never called) gates OFF ore_.step/step_harvest in
    // step() entirely -- a World that never calls this (every pre-economy-
    // phase caller, including demo-base) steps with zero ore/harvest
    // activity, byte-identical to before this phase landed.
    void set_economy(std::vector<BuildingTypeDef> types, std::vector<int> refinery_types,
                     OreValues values, double growth_interval_secs,
                     double growth_pct, double spread_pct);

    // Give the World a building-type roster for TYPE LOOKUPS ONLY, without
    // arming ore growth / harvest stepping.
    //
    // Why this is a SEPARATE vector and not just `types_`: `types_.empty()` IS
    // the economy firewall (see its declaration below) -- step() skips
    // ore_.step and step_harvest precisely when it is empty. The interactive
    // launcher (launch_skirmish) never calls set_economy, so types_ is empty
    // there, and every repair/sell type lookup silently refused: the wrench did
    // nothing in the real game while all 19 gates stayed green, because gates
    // build their worlds through wire_economy instead [RE: repair-sell.md §A4;
    // Task 4 report §6]. Writing types_ here would fix the lookup but
    // simultaneously switch the economy passes ON in the interactive/skirmish
    // world, moving skirmish_hash and maps_hash. So the roster lands in its own
    // field and `btypes()` prefers whichever is populated.
    void set_building_types(std::vector<BuildingTypeDef> types) { lookup_types_ = std::move(types); }

    // The type roster for non-economy consumers (repair/sell). set_economy's
    // roster wins when present so gate worlds behave exactly as before.
    const std::vector<BuildingTypeDef>& btypes() const {
        return types_.empty() ? lookup_types_ : types_;
    }

    // Harvest income accumulated (both houses' unload drains summed since
    // the last call to take_income for THAT house) since the last call,
    // then zeroed for `house`. House index must be 0 or 1; anything else
    // returns 0 untouched.
    double take_income(int house);

    // Spawn a harvester: add_combat_unit (same placement precondition: the
    // caller already checked walkable()/unit_at_cell()) plus the harvester
    // fields, starting the mission at HarvState::SeekOre (a freshly built
    // harvester starts hunting immediately, matching the original).
    Unit& add_harvester(int cell_x, int cell_y, int house, const UnitCombat& c,
                        bool chrono, int storage_bales);

    // Player orders (selected house-0 harvesters only). Simplified per
    // economy.md §1: no separate "pinned" flag -- just pin harv_cell_* to
    // (x, y), set the mission to MoveToOre, and emit the path immediately
    // via order_unit_move (mirrors step()'s own HarvestMove handling).
    void order_harvest(int x, int y);
    // ReturnToRef now (resets harv_timer -- every transition into
    // ReturnToRef resets it; see harvest.cpp's ChronoOut-fires-once comment).
    void order_return();

    // Drained by main.cpp each frame for audio (chrono/unload sounds); empty
    // if set_economy was never called (step() never populates it then).
    std::vector<HarvestEvent> take_harvest_events();

    // Movement speed in iso cells/sec (see main.cpp for the Rules-derived value).
    void set_move_speed(double cells_per_sec) { move_cps_ = cells_per_sec; }
    double move_speed() const { return move_cps_; }

    // Order every SELECTED unit to walk toward (goal_x, goal_y): assigns each a
    // distinct formation cell spiralling out from the goal, then paths it there.
    // Units retargeted mid-walk keep their interpolated fx/fy (no backward
    // snap). Dying/dead selected units are skipped. No-op if nothing eligible
    // is selected.
    // Economy phase addition [RE: economy.md §1 auto-resume]: a selected
    // is_harvester unit also gets manual_hold = true, so step_harvest parks
    // its mission FSM for the duration of this player-directed move and
    // resumes (at SeekOre) the instant it arrives -- a ground right-click on
    // a harvester keeps this existing move path rather than a dedicated one.
    void order_move_selected(int goal_x, int goal_y);

    // Order every SELECTED house-0, alive, non-dying unit to attack
    // `target_idx`: sets target=target_idx and attack_order=true (pursues
    // indefinitely -- step_combat's next MoveRequest paths it there via
    // step()), and releases/re-asserts its occupancy claim and clears its
    // path the same way order_move_selected does (a fresh pursuit path is
    // computed later by step_combat's request, not here). No-op (touches no
    // unit) if target_idx is out of range, dead, dying, or itself house 0 --
    // a house-0 (player) unit can't be ordered to attack a friendly.
    void order_attack(int target_idx);

    // Placed buildings (Buildings & Construction Task 4).
    std::vector<Building>& buildings() { return buildings_; }
    const std::vector<Building>& buildings() const { return buildings_; }

    // Appends a Building at foundation origin (cx, cy), sized fw x fh, owned
    // by `house`, with hp = max_hp = strength and the given armor_ix (used by
    // combat's damage_vs). State starts Buildup unless `instant` (scripted
    // spawns pass true), in which case it starts Idle. Clears passable_ for
    // every foundation cell (foundation_cells) so units can no longer walk
    // through it -- restored automatically by World::step when the building
    // dies. Returns the new building's index (stable -- buildings are never
    // erased, only tombstoned). Precondition: the caller already validated
    // the placement via placement_check (this method does not re-check it).
    int add_building(int type, int cx, int cy, int fw, int fh, int house, int strength,
                     int armor_ix, double buildup_total, bool instant, int sight = 0,
                     bool grants_map_reveal = false);

    // Index of the alive, non-Dead building whose foundation contains (x, y),
    // or -1 if none (ascending index; foundations never overlap while both
    // are alive, so at most one can ever match).
    int building_at_cell(int x, int y) const;

    // Orders every SELECTED house-0, alive, non-dying unit to attack building
    // `b_idx`: target_building = b_idx, target = -1 (mutually exclusive with
    // order_attack's unit target), attack_order = true, plus the same
    // occupancy-release/path-clear dance as order_attack (a fresh pursuit
    // path is computed later by step_combat's request, not here). No-op
    // (touches no unit) if b_idx is out of range, dead, in State::Dead, or
    // owned by house 0 (a house-0 unit can't be ordered to attack a
    // friendly building).
    void order_attack_building(int b_idx);

    // --- Production phase [RE: production.md §1.3/§1.4] ---
    // Set factory `b_idx`'s rally cell (finished units path here after exiting).
    // No-op if b_idx is out of range or not an alive building.
    void set_rally(int b_idx, int x, int y);

    // Make `b_idx` its house's primary factory of its category (factory::
    // set_primary over `factory_types`, the roster-index list for ONE category).
    // No-op if b_idx is out of range.
    void set_primary(int b_idx, const std::vector<int>& factory_types);

    // Launch a finished COMBAT unit from `factory_idx`: exit_cell() walks the
    // 8-ring from the factory's anchor to the first FREE (walkable, unoccupied)
    // cell [RE-GATE APPLIED: production.md §1.3]; add_combat_unit there (house
    // = factory's house, stats from `c`), then order it to the factory's rally
    // cell (has_rally) else the DEFAULT GATHER CELL — the nearest free cell
    // from the building's own coordinate [RE-GATE APPLIED: production.md §1.4,
    // FUN_00500020: the exit-adjacent gather cell, NOT the map center] —
    // computed by re-running the ring walk now that the unit occupies the door
    // cell. order_unit_move cancels cleanly if unreachable. Returns the new
    // unit index; -1 if NO ring cell was free (caller HOLDS the queue ready)
    // or factory_idx is invalid. Asset-free: the caller resolves c + factory_def.
    int launch_unit(int factory_idx, const BuildingTypeDef& factory_def, const UnitCombat& c);

    // True iff `house` owns NO alive Idle factory whose type is in
    // factory_types (the caller then cancels+refunds that queue; a survivor ->
    // false, the queue continues). Mirrors ps_tick's ConYard-died cancel gate.
    bool factory_category_lost(const std::vector<int>& factory_types, int house) const;

    // --- CB-1 Transports/Cargo [RE: transport-cargo.md] ---
    // Enter order: every SELECTED house-0, alive, non-dying unit that passes
    // can_load NOW (capacity is re-checked at boarding -- orders may
    // oversubscribe) gets enter_target = t_idx, drops any engagement, and
    // paths toward the transport through assign_formation around its cell
    // (a raw move to the transport's own occupied cell would cancel as
    // unreachable) [spec §3]. No-op if t_idx is not an own, alive, non-dying
    // transport. Sets the cargo_any_ firewall.
    void order_enter_transport(int t_idx);
    // Unload order: sets unload_pending on every SELECTED house-0, alive,
    // non-dying transport with non-empty cargo [spec §5]. Any new move/
    // attack order on the transport clears unload_pending (an order
    // supersedes -- our order-model convention, not overturned by the hunt).
    // Sets the cargo_any_ firewall.
    void order_unload_selected();
    // Drain the board/refuse/eject events accumulated by step_cargo since
    // the last call (the take_harvest_events pattern). Empty if no cargo
    // activity ever happened.
    std::vector<CargoEvent> take_cargo_events();

    // --- AI-1 base-planning phase [RE: ai-base.md] ---
    // Flag `house` as AI-driven with the given config. The planner then runs
    // inside step() (step_ai_houses), driving the EXISTING ProductionQueues /
    // placement_check / add_building machinery — no new production or
    // placement code. BYTE-IDENTITY FIREWALL: a World that never calls this
    // steps EXACTLY as before (step_ai_houses early-returns on a single
    // bool), and none of the AI fields is read by any hash.
    void enable_ai_house(int house, AiHouseConfig cfg);
    bool ai_enabled(int house) const;
    // Test/demo accessors (read-only views of the per-house AI record;
    // nullptr / 0 / -1 when `house` is not AI-flagged).
    const AiBaseState* ai_state(int house) const;
    double ai_credits(int house) const;
    // Roster type building in the AI house's Structure (cat 0) or Defense
    // (cat 1) queue; -1 = idle.
    int ai_producing(int house, int cat01) const;

    // --- AI-2 offensive-teams phase [RE: ai-team.md] ---
    // Read-only team state (nullptr when `house` is not AI-flagged). NOT read
    // by world_hash — the new gate hashes it separately (ai_team_hash).
    const AiTeamState* ai_team_state(int house) const;
    // Unit-roster type building in the AI house's Infantry (0) or Vehicle (1)
    // queue; -1 = idle.
    int ai_unit_producing(int house, int iv01) const;

    // --- AI-3 AITriggerTypes phase [RE: ai-trigger.md] ---
    // Read-only trigger state (nullptr when `house` is not AI-flagged). NOT
    // read by world_hash — the 8th gate hashes it separately (ai_trigger_hash).
    const AiTriggerState* ai_trigger_state(int house) const;

    // --- CB-4 Superweapons [RE: superweapon.md] -----------------------------
    // A NEW per-house superweapon registry, INDEPENDENT of the AI registry so
    // it works for the PLAYER house 0 (which is never in ai_houses_). Default
    // INERT: no house has a SW until grant_superweapon, which no existing demo
    // calls -> sw_houses_ empty, sw_any_ false, step_superweapon a strict no-op
    // (spec §8). Unit and combat.cpp behavior are untouched.
    //
    // Grant `house` a superweapon: grow sw_houses_ to cover it, store the pure
    // lifecycle (type + recharge_secs already in SECONDS) and the resolved
    // detonation params ([NukePayload] Damage + [NUKE] CellSpread/PercentAtMax/
    // Verses), and open the sw_any_ firewall. verses is the 11-slot armor array
    // (int percents, matching Unit::w_verses / damage_vs).
    void grant_superweapon(int house, SuperweaponType type, double recharge_secs,
                           int dmg, double cell_spread, double pmax,
                           const std::array<int, 11>& verses);
    // Fire `house`'s superweapon at cell (cx, cy): no-op returning false unless
    // that house's SW is READY; on fire it detonates at the cell (§5) and resets
    // the timer (§4). Returns whether it fired (the wedge guard reads this).
    // [Task 4 lands the detonation body; Task 3 lands the registry it uses.]
    bool fire_superweapon(int house, int cx, int cy);
    // Read-only SW state for the sidebar (§6) and the gate (§7). false / 0 /
    // nullptr when `house` has no granted SW.
    bool sw_ready(int house) const;
    double sw_charge_fraction(int house) const;
    const Superweapon* sw_state(int house) const;

    // --- Multi-house / Alliances phase [RE: multihouse.md §1/§2, D1] ---------
    // On-screen sides (the [Sides] legacy GDI=Allied / Nod=Soviet / ThirdSide=Yuri
    // are resolved to these). Neutral = Civilian/Special (non-combatant).
    enum class Side { Allied, Soviet, Yuri, Neutral };
    // One record per house (including player 0). Defaults reproduce today's world
    // 0=player Allied / 1=enemy Soviet / 2=neutral EXACTLY. NEVER read by
    // world_hash. color_ix indexes the 8 MP colors (or a default). alliance_team
    // sentinel -1 resolves to the house's own id (self-only team = FFA).
    struct HouseRec {
        Side        side          = Side::Allied;
        int         color_ix      = 0;      // 0..7 into the 8 MP colors
        std::string country       = "";
        int         alliance_team = -1;     // sentinel; -1 -> own id on read
        bool        combatant     = true;
    };
    // Number of houses (default 3). The six per-house vectors + house_defeated_
    // are sized to this. Grows via set_house.
    int  house_count() const { return houses_.empty() ? 3 : (int)houses_.size(); }
    // Side / color for house h. With houses_configured_ FALSE -> today's exact
    // derivation (house 1 -> Soviet/red, else Allied/blue) -> byte-identical.
    Side house_side(int h) const;
    int  house_color(int h) const;
    // Render entry [RE: multihouse.md §2/D6]: with houses_configured_ FALSE ->
    // pal_for_house(unit_pal, house) EXACTLY (today's blue/red) -> pixel-identical.
    // Configured -> the 8-MP-color spread from house_color(house). NOT hashed.
    ra2assets::Palette house_palette(const ra2assets::Palette& unit_pal, int house) const;
    // Alliance team for h (sentinel -1 -> own id). Non-combatant houses keep their
    // own team but never ally/enemy anyone (see is_enemy/are_allied, Task 4).
    int  house_team(int h) const;
    bool house_combatant(int h) const;
    // Configure house h. Grows houses_ (and the per-house vectors, once T3 lands)
    // to h+1 with defaults if needed, sets houses_[h]=rec, and OPENS the firewall
    // (houses_configured_ = true). No existing gate calls this -> byte-identical.
    void set_house(int h, HouseRec rec);
    bool houses_configured() const { return houses_configured_; }

    // --- Multi-house / Alliances phase [RE: multihouse.md §3/§4, D3] ---
    // Pairwise relationship predicates. With houses_configured_ FALSE ->
    // is_enemy(a,b) == (a != b) and are_allied(a,b) == (a == b) -> EXACTLY today's
    // behavior -> byte-identical in every gate. Neutral/non-combatant is NEVER an
    // enemy of anyone (both directions) and never allied to a house it doesn't
    // share a team with. Self is always allied to self. NOT read by world_hash.
    bool are_allied(int a, int b) const;
    bool is_enemy(int a, int b) const;

    // AllyReveal [RE: multihouse.md §3, D4]: allied houses share revealed shroud.
    // Firewalled behind houses_configured_ + shroud_any_ -> OFF in every gate.
    // Default true (matches rules AllyReveal=yes) but only ACTIVE when alliances
    // are configured AND shroud is enabled. NOT hashed.
    void enable_ally_reveal(bool on) { ally_reveal_ = on; }
    bool ally_reveal_ = true;

    // --- Victory / defeat [RE: multihouse.md §5, D5] ---
    // Enable the greenfield last-team-standing victory pass. short_game=true
    // (ShortGame default) -> a combatant house is defeated when its last building
    // falls; false -> zero buildings AND zero units. Default OFF -> step_victory
    // early-returns -> gates run their fixed frame counts. NOT hashed.
    void enable_victory(bool on, bool short_game = true);
    bool victory_enabled() const { return victory_enabled_; }  // read-only; not hashed
    bool house_defeated(int h) const;   // sticky; false if h out of range / neutral
    int  winning_team() const;          // the sole surviving combatant team, or -1
    void step_victory();                // firewalled per-frame pass (public for the gate)

    // --- Engineer/Spy Capture phase [RE: capture.md] ------------------------
    // Ownership transfer (Approach A) [RE: capture.md §A3 CONFIRMED]: flip
    // buildings_[b_idx].house to new_house -> power/economy/buildability/sidebar
    // follow for free (all per-frame derived). Then the targeted fix-ups:
    //  (1) SW move: if the building grants_superweapon, transfer_superweapon
    //      old_house -> new_house;
    //  (2) AI base-node resync BOTH sides (the AI-1 mechanism): the losing AI
    //      house drops the node that pointed at this building; a capturing AI
    //      house adopts it into a matching unclaimed node;
    //  (3) primary-flag reset: the captured factory's `primary` flag is cleared
    //      (neither house treats a just-captured building as its established
    //      primary; the caller's set_primary re-elects one);
    //  (4) ProduceCashStartup: if captured FROM the neutral house, grant the
    //      new owner `startup` credits once.
    // Opens capture_any_. No-op returning false if b_idx is invalid, the
    // building is Dead, or new_house == the current owner. `startup` is the
    // captured building type's ProduceCashStartup (0 for a normal building).
    bool capture_building(int b_idx, int new_house, int startup);

    // Spy infiltration [RE: capture.md §B CONFIRMED]. Per building purpose, keyed
    // off `purpose` (resolved by the caller from the building type -- see
    // InfiltratePurpose): Refinery -> steal cash_steal_amount(victim credits,
    // steal_pct) into the spy house; PowerPlant -> arm blackout_frames_[victim]
    // = blackout (1000); Superweapon -> reset_superweapon(victim); Factory ->
    // set produced_veteran_[spy house] (Task 6); Radar/Lab -> inert (no shroud,
    // [RE-FALLBACK] documented). No-op returning false if the spy already owns
    // the building (self-spy) or b_idx is invalid/Dead. Opens capture_any_. The
    // caller consumes the spy on true. steal_pct = SpyMoneyStealPercent (0.5),
    // blackout = SpyPowerBlackout (1000).
    enum class InfiltratePurpose { None, Refinery, PowerPlant, Superweapon, Factory, RadarLab };
    bool infiltrate_building(int b_idx, int spy_house, InfiltratePurpose purpose,
                             double steal_pct, int blackout);
    // Engineer repair [RE-GATE APPLIED: capture.md §A5]: heal building b_idx to
    // full HP (a damaged OWN building) and open capture_any_. No-op returning
    // false if b_idx is invalid, Dead, not owned by `by_house`, or already at
    // full HP. The caller consumes the engineer on true.
    bool repair_building(int b_idx, int by_house);

    // --- Projectile phase (P2) [RE: projectile.md] -------------------------
    // ALL inert by default. projectiles_enabled_ is opened ONLY by
    // enable_projectiles; step_projectiles' first line is the early return, and
    // World::step passes a launch sink to step_combat only when it is open. None
    // of this state is read by world_hash.
    bool projectiles_enabled_ = false;          // the firewall
    ProjectileRules proj_rules_;                // Gravity / BallisticScatter
    ProjRng proj_rng_;                          // dedicated scatter RNG (spec D3)
    std::vector<Projectile> projectiles_;       // in flight, launch order
    std::vector<ProjectileEvent> proj_events_;  // pending take_projectile_events
    double proj_accum_ = 0.0;                   // dt accumulator -> 15 Hz frames
    int proj_frame_ = 0;                        // logic-frame counter
    // Firewalled World::step pass (pure insertion before step_combat): advance
    // every projectile one logic frame per accumulated 1/15 s.
    void step_projectiles(double dt);
    void step_projectiles_frame();
    // Turn one step_combat launch into a Projectile (muzzle via FLH, target
    // coords, compute_launch incl. scatter draws). Refused shots log an event.
    void launch_projectile(int attacker, int target_unit, int target_building);
    // Detonate `p` at its position: Explode snap, then the EXISTING damage layer
    // (apply_spread_damage / deliver_unit_hit / deliver_building_hit).
    void detonate_projectile(Projectile& p, int victim);

    // --- Repair & Sell phase [RE: repair-sell.md] -------------------------
    // The four [General] scalars + the three survivor divisors. The struct's
    // defaults ARE this install's real values, so a World that never calls
    // this already behaves like the original; main.cpp overrides from rulesmd.
    void set_repair_sell_rules(const RepairSellRules& r) { repair_rules_ = r; }
    const RepairSellRules& repair_sell_rules() const { return repair_rules_; }

    // BuildingClass::Repair(control) [RE: repair-sell.md §A4, vtbl+0x19C].
    // control: -1 toggle / 0 off / 1 on -- the classic Westwood convention.
    // Returns true iff the flag CHANGED. Refuses (false, no change) unless:
    //   * b_idx names an alive, non-Dead building,
    //   * b.house == by_house (the network REPAIR/SELL event is authoritative
    //     about ownership, §C1 -- you cannot repair someone else's building),
    //   * types_ covers b.type (set_economy must have run; repair needs Cost
    //     and Strength to price a step), and
    //   * turning ON additionally passes can_be_click_repaired (§A4).
    // Turning ON opens repair_sell_any_. Turning OFF is always allowed on a
    // live building (it is off-ramp 1/2/5 -- see step_repair_sell for 3/4).
    bool set_repair(int b_idx, int by_house, int control);
    bool toggle_repair(int b_idx, int by_house) { return set_repair(b_idx, by_house, -1); }
    bool building_repairing(int b_idx) const {
        return b_idx >= 0 && b_idx < static_cast<int>(buildings_.size()) &&
               buildings_[b_idx].being_repaired;
    }
    // The GLOBAL repair cadence counter [RE: §A2]. It advances EVERY frame,
    // above the firewall, so `frame % interval == 0` is an ABSOLUTE phase --
    // all repairing buildings step together and a newly-armed one waits
    // 0..interval-1 frames, exactly like the original's `Frame % interval`.
    std::uint64_t repair_frame() const { return repair_frame_; }
    // The firewalled per-frame Repair & Sell pass. Public so a gate/test can
    // drive it directly; normally called from World::step.
    void step_repair_sell(double dt);

    // BuildingClass::Sell_Back(-1) [RE: repair-sell.md §B2/§B5, vtbl+0x1A0].
    // Starts mission 0x13 (Selling). Returns true iff the mission STARTED (or,
    // for a type with no buildup art, completed instantly -- §B5 item 6).
    // Refuses per can_be_sold PLUS ownership (b.house == by_house -- the SELL
    // event is authoritative, §C1) PLUS btypes() coverage. Calls
    // set_repair(b_idx, by_house, 0) FIRST -- selling always cancels repair
    // (§B2 entry, 0x00449B81). Opens repair_sell_any_.
    //
    // c4_applied is passed through to can_be_sold; the clone has no C4 mechanic
    // until roadmap P4, so every caller passes false today.
    bool begin_sell(int b_idx, int by_house, bool c4_applied = false);
    bool building_selling(int b_idx) const {
        return b_idx >= 0 && b_idx < static_cast<int>(buildings_.size()) &&
               buildings_[b_idx].selling;
    }
    // The crew UnitCombat spawned as a survivor for a given side [RE: §B3].
    // World is ASSET-FREE, so the CALLER resolves the infantry (E1 Allied /
    // E2 Soviet, [General] AlliedCrew/SovietCrew -- §D1) and registers it here.
    // side_ix is World::Side's underlying value. UNREGISTERED (the default for
    // every existing World) -> that side spawns ZERO survivors.
    // [RE-GATE APPLIED: repair-sell.md §D item 1] the 25%-chance special crew
    // type for building purpose-class 7 is CUT: Rules+0xF70 was never
    // string-anchored, and it is an RNG draw that would break gate determinism.
    void set_survivor_crew(int side_ix, const UnitCombat& c);
    // Add `amount` to house's world-side credit pool (the pool repair charges
    // and sell refunds land in). The interactive GameScene keeps the player's
    // wallet outside the World and drains this pool via take_income every
    // frame, so it deposits the wallet here before a step that may charge a
    // repair. No-op for an out-of-range house. No gate calls it.
    void deposit_credits(int house, double amount) {
        if (house >= 0 && house < house_count()) house_credits_[house] += amount;
    }
    // True iff any alive building of `house` has its wrench on.
    bool house_repairing(int house) const {
        for (const Building& b : buildings_)
            if (b.alive && b.house == house && b.being_repaired) return true;
        return false;
    }

    // Consume a unit (the capture/repair gesture spends its engineer, and any
    // future "removed from map" consumer): tombstone it directly (no die anim --
    // it walked INTO the building) and release its occupancy cell, reusing the
    // combat.cpp death path (alive=false + occ_.release_all_of). No-op if idx is
    // out of range or the unit is already tombstoned. Interactive-only -- no
    // demo calls it, so every gate stays byte-identical.
    void consume_unit(int idx);
    // Move a house's whole superweapon grant old_house -> new_house (the SW
    // record + params); no-op if old_house has none. Reuses the CB-4 registry.
    void transfer_superweapon(int old_house, int new_house);
    // Reset a house's superweapon recharge to 0/not-ready [RE: capture.md §B4
    // CONFIRMED - spy SW-building infiltrate resets the timer]; reuses
    // Superweapon::on_fired(). No-op if the house has no granted SW.
    void reset_superweapon(int house);
    // Engineer capture/repair ORDER [RE: capture.md §A3/§A5]: record building
    // b_idx as unit eng_idx's capture_target and path it to a free cell adjacent
    // to the building's footprint (the transport/attack-building adjacency-
    // approach analogue -- a raw move onto the occupied foundation would cancel
    // as unreachable). It does NOT capture or consume here: step_capture performs
    // the capture_building/repair_building + consume_unit when the engineer
    // reaches adjacency (§A5: move-adjacent-then-act, like enter-transport).
    // Supersedes any engagement/enter order on the engineer. Opens capture_any_
    // (the step_capture firewall). No-op if eng_idx is not an alive, non-dying
    // unit or b_idx is not an alive, non-Dead building. Interactive-only -- no
    // demo issues it, so every gate stays byte-identical.
    void order_engineer_capture(int eng_idx, int b_idx);

    // Spy infiltrate ORDER [RE: capture.md §B]: the deferred sibling of
    // order_engineer_capture. Records building b_idx as unit spy_idx's
    // infiltrate_target and paths it to a free cell adjacent to the footprint;
    // the PRE-RESOLVED purpose + steal_pct/blackout are stored on the unit so
    // step_capture calls infiltrate_building (and consume_unit) on Chebyshev
    // arrival WITHOUT the roster (World is asset-free -- the caller resolved the
    // purpose from the building type at the gesture layer). It does NOT
    // infiltrate or consume here -- the effect fires only on arrival, so a spy
    // no longer infiltrates across the map on a single click. Supersedes any
    // engagement/enter order. Opens capture_any_. No-op if spy_idx is not an
    // alive, non-dying unit or b_idx is not an alive, non-Dead building.
    // Interactive-only -- no demo issues it, so every gate stays byte-identical.
    void order_spy_infiltrate(int spy_idx, int b_idx, InfiltratePurpose purpose,
                              double steal_pct, int blackout);

    // Register a captured/owned income building (an oil derrick) [RE: capture.md
    // §D1]. Opens capture_any_. amount/delay are the resolved [CAOILD]
    // ProduceCashAmount/ProduceCashDelay. Idempotent per building index; ignores
    // a non-positive amount/delay or a negative b_idx.
    void register_tech_income(int b_idx, int amount, int delay);

    // Set/clear a unit's disguise [RE: capture.md §C1/§C2]. set_disguise opens
    // capture_any_ (the step_tech_structures detection firewall). Both no-op on
    // an out-of-range index. clear_disguise leaves the type/house fields alone
    // (only the flag matters for render/detection).
    void set_disguise(int unit_idx, const std::string& as_type, int as_house);
    void clear_disguise(int unit_idx);
    // Read-only Capture state for the gate (§gate) and the power path.
    double house_credits(int house) const {
        return (house >= 0 && house < house_count()) ? house_credits_[house] : 0.0;
    }
    int blackout_frames(int house) const {
        return (house >= 0 && house < house_count()) ? blackout_frames_[house] : 0;
    }
    bool house_blacked_out(int house) const { return blackout_frames(house) > 0; }
    // Produced-unit veterancy flag [RE: capture.md §B5]. Set by a factory
    // infiltration; consumed by add_combat_unit/launch_unit in Task 6 (units
    // this house produces thereafter spawn at veteran_rank 1). Index 2 inert.
    bool produced_veteran(int house) const {
        return house >= 0 && house < house_count() && produced_veteran_[house];
    }

    // --- Shroud/Fog phase [RE: shroud.md] ---------------------------------
    // Enable the per-house visibility layer: opens shroud_any_ and allocates
    // explored_[h]/visible_[h] (grid_w_*grid_h_, zero-filled) for h in 0..2.
    // Default OFF -> every existing gate leaves the grids EMPTY and step_visibility
    // is a strict no-op. Idempotent (re-enabling with grids already sized is a
    // no-op). enable(false) is not needed by any gate (left as a hard reset).
    void enable_shroud(bool on);
    bool shroud_enabled() const { return shroud_any_; }

    // --- Projectile phase (P2) [RE: projectile.md] ---------------------------
    // Open the projectile firewall: from now on a shot whose Unit::w_proj
    // flies() launches a real projectile (ballistic arc / falling bomb) whose
    // damage lands at detonation, instead of the instant hit. Called ONLY by the
    // interactive game (GameScene) and --demo-projectile; no byte-identity gate
    // calls it, so every existing gate keeps the instant path bit-for-bit.
    void enable_projectiles(const ProjectileRules& rules);
    bool projectiles_enabled() const { return projectiles_enabled_; }
    const std::vector<Projectile>& projectiles() const { return projectiles_; }
    int projectile_frame() const { return proj_frame_; }
    // Drain the projectile event log (launch/refused/detonate/removed).
    std::vector<ProjectileEvent> take_projectile_events();
    // Is cell (x,y) currently VISIBLE / ever EXPLORED to house h? EMPTY grid
    // (shroud off, or h out of range) -> true (all-visible / all-explored),
    // so every consult is inert with shroud off. Out-of-bounds cell -> false.
    bool visible_at(int house, int x, int y) const;
    bool explored_at(int house, int x, int y) const;
    // Stamp a near-circular sight disc for house h around (cx,cy) into
    // visible_[h] + explored_[h] (delegates to reveal_disc). No-op if the grid
    // is empty or h is out of range. The mutating entry for buildings/units.
    void reveal_source(int house, int cx, int cy, int sight);
    // The firewalled per-frame visibility pass (see world.cpp). Public so the
    // gate can drive it directly if needed; normally called from World::step.
    void step_visibility();
    // Re-shroud house h [RE: shroud.md §7B]: clear BOTH explored_[h] and
    // visible_[h] (fully re-shrouded -- the victim must re-scout). No-op if the
    // grid is empty (shroud off) or h out of range. shroud_any_ is NOT needed
    // (only meaningful once enabled). The classic spy radar-sabotage.
    void reshroud_house(int h);
    // Reveal the whole map for house h [RE: shroud.md §7A]: set explored_[h]
    // fully (and visible_[h] fully). Driven by owning a grants_map_reveal
    // building; also the "reveal map / no-shroud" start option. No-op if empty.
    void reveal_all_house(int h);

    // Test seam: directly seed a house's credits without running the full
    // harvest/unload pipeline (the cash-steal victim in the infiltrate tests).
    // Index 2 (neutral) inert. Interactive/demo code never calls it.
    void grant_credits_for_test(int house, double amount) {
        if (house >= 0 && house < house_count()) house_credits_[house] = amount;
    }

    // Advance all units by dt seconds: step_units (collision-aware movement)
    // -> step_combat (targeting/firing/deaths, returns pursuit/return-to-
    // anchor MoveRequests) -> path each request via order_unit_move (skipping
    // any request for a unit that died/started dying this same step) -> patch
    // each still-alive unit's terrain level from the map for rendering height.
    void step(double dt);

    // Per-unit body shared by order_move_selected (per selected unit, given
    // its formation-assigned cell) and step's MoveRequest execution (per
    // pursuing/returning unit, given step_combat's requested cell): paths
    // unit `idx` directly to (gx, gy) -- no formation, since a pursuit/
    // return-to-anchor request already names one specific goal cell --
    // releasing/re-asserting its occupancy claim first. Unreachable/self-goal
    // cancels cleanly (fx/fy snap to the current cell, mirroring the
    // single-unit Finding-1 cancel branch); otherwise fx/fy are left alone
    // (no backward snap) and the unit starts walking. No-op if idx is out of
    // range.
    // CB-2 [RE: aircraft.md §1/§6]: for an is_aircraft unit this instead sets a
    // FLIGHT GOAL and initiates takeoff/re-launch (no A*, no occupancy). Public
    // as the direct single-unit order seam the flight World tests drive (a
    // compile-time access change only -- no gate reads it).
    void order_unit_move(int idx, int gx, int gy);

private:
    // --- AI-1 base-planning phase [RE: ai-base.md] ---
    struct HouseAiRec {
        bool is_ai = false;
        AiHouseConfig cfg;
        AiBaseState state;
        ProductionQueues queues;  // ONLY the Structure/Defense slots are used
        double credits = 0.0;
        AiTeamState team_state;  // AI-2 [RE: ai-team.md]
        AiTriggerState trigger_state;  // AI-3 [RE: ai-trigger.md]
    };
    std::vector<HouseAiRec> ai_houses_;  // index = house id (grown on demand)
    bool ai_any_ = false;                // the step() firewall bool
    void step_ai_houses(double dt);
    BaseView make_ai_base_view(int house, const HouseAiRec& rec) const;
    bool ai_place_ready(int house, HouseAiRec& rec, QueueCat cat);

    // --- AI-2 offensive-teams phase [RE: ai-team.md] ---
    void step_ai_teams(double dt);
    int ai_unit_type_of(const HouseAiRec& rec, const Unit& u) const;
    std::vector<IdleUnitView> ai_idle_units(int house, const HouseAiRec& rec) const;
    void ai_fill_team_view(int house, const HouseAiRec& rec, const TeamInstance& team,
                           TeamView& v) const;
    bool ai_quarry_exists(int house, int quarry) const;
    // Pickers score by distance^2 x value x seeded-RNG jitter (ai_rand over the
    // passed rng) and keep the nearest-reachable [RE-GATE APPLIED: ai-team.md §5].
    int ai_pick_quarry_unit(int house, int quarry, int nx, int ny, std::uint32_t& rng) const;
    int ai_pick_quarry_building(int house, int quarry, int nx, int ny, std::uint32_t& rng) const;
    // Shared mutation tail extracted from order_attack/order_attack_building so
    // both the player selection-loops and the AI per-unit entry points commit a
    // target identically [RE-GATE APPLIED: ai-team.md §5 — de-duplication].
    void commit_attack_target(int unit_idx, int target_unit, int target_building);
    void ai_order_attack(int unit_idx, int target_idx);
    void ai_order_attack_building(int unit_idx, int b_idx);
    void ai_execute_team_command(int house, HouseAiRec& rec, TeamInstance& team,
                                 const TeamCommand& c);
    bool ai_launch_unit_ready(int house, HouseAiRec& rec, QueueCat cat);

    // --- AI-3 AITriggerTypes phase [RE: ai-trigger.md] ---
    void ai_fill_trigger_view(int house, const HouseAiRec& rec, TriggerView& v) const;

    // --- CB-4 Superweapons [RE: superweapon.md] ---
    // Per-house SW record: the pure lifecycle + the resolved detonation params.
    // Mirrors HouseAiRec's shape but INDEPENDENT of ai_houses_ (spec §2). All
    // defaults INERT (has_sw=false). eff_verses is the [NUKE] 11-slot armor
    // array as int percents (assigned straight into the transient detonator's
    // Unit::w_verses in fire_superweapon -- damage_vs takes std::vector<int>).
    struct HouseSuperweapon {
        bool has_sw = false;
        Superweapon sw;
        int eff_damage = 0;                 // [NukePayload] Damage=600
        double eff_cell_spread = 0.0;       // [NUKE] CellSpread=10
        double eff_percent_at_max = 1.0;    // [NUKE] PercentAtMax=.02
        std::array<int, 11> eff_verses{};   // [NUKE] Verses (armor percents)
    };
    std::vector<HouseSuperweapon> sw_houses_;  // index = house id (grown on demand)
    bool sw_any_ = false;                      // the step() firewall bool
    void step_superweapon(double dt);          // firewalled World::step pass (§3)

    // --- Engineer/Spy Capture phase [RE: capture.md] ---
    bool capture_any_ = false;   // the step() firewall bool (opened by any
                                 // capture/infiltrate/neutral-spawn/disguise)
    // Resync AI base-node ownership after a capture [RE: capture.md §A3 -
    // reuses the AI-1 standing-building<->node sync]. For the LOSING house (if
    // AI): drop the node whose `building == b_idx` (n.building=-1, n.dead=true)
    // -- a captured building is still ALIVE so node-death detection never fires.
    // For the GAINING house (if AI): claim b_idx into the FIRST unclaimed plan
    // node of the same type (the standing-building sync rule). The player
    // (house 0) is never AI -> no node work when the player captures.
    void ai_resync_captured_building(int b_idx, int old_house, int new_house);
    // Engineer capture-arrival pass [RE: capture.md §A5], called from step()
    // beside step_cargo (a pure insertion). Early-returns unless capture_any_
    // (the firewall: false until the first capture/repair/order). For each unit
    // with a live capture_target that has reached Chebyshev adjacency to its
    // target building's footprint, performs capture_building (enemy/neutral) or
    // repair_building (own damaged), consumes the engineer on a successful act,
    // and clears the intent; a vanished/dead target clears the intent cleanly.
    void step_capture(double dt);

    // Tech-structure periodic income [RE: capture.md §D1 CONFIRMED - CAOILD
    // ProduceCashAmount=20 every ProduceCashDelay=100 frames]. One record per
    // captured income building; the income accrues to whoever CURRENTLY owns
    // the building (buildings_[b].house), so capturing an oil derrick transfers
    // its future income for free (per-frame derived, Approach A).
    struct TechIncome { int building = -1; int amount = 0; int delay = 0; int frames = 0; };
    std::vector<TechIncome> tech_income_;
    // Power blackout per house [RE: capture.md §B2 CONFIRMED - SpyPowerBlackout
    // =1000 frames]. >0 -> that house's power OUTPUT is forced to 0 (see
    // apply_blackout). Index 2 inert. Decays one per logic frame in
    // step_tech_structures.
    // T3 [RE: multihouse.md §D2]: [3] -> std::vector sized house_count() (default
    // 3 -> indices 0/1/2 identical). NOT hashed. Sized in ensure_houses_default /
    // grow_house_arrays.
    std::vector<int> blackout_frames_;
    // Produced-unit veterancy flag [RE: capture.md §B5]. Set by a factory
    // infiltration; consumed by add_combat_unit/launch_unit in Task 6 (units
    // this house produces thereafter spawn at veteran_rank 1). Index 2 inert.
    // Defaults false (INERT): no existing demo infiltrates a factory, so every
    // gate is byte-identical.
    // T3 [RE: multihouse.md §D2]: bool[3] -> std::vector<uint8_t> sized
    // house_count(). NOT hashed. Sized in ensure_houses_default/grow_house_arrays.
    std::vector<std::uint8_t> produced_veteran_;
    // The single firewalled Capture per-frame pass: tech-structure income +
    // blackout decay (+ disguise detection, added in Task 7). First line is
    // `if (!capture_any_) return;` -> a strict no-op in every existing demo.
    void step_tech_structures(double dt);

    // --- Shroud/Fog phase [RE: shroud.md §1/D1] ---
    // Two independent per-house grids, sized grid_w_*grid_h_, index y*grid_w_+x
    // (same as passable_). EMPTY (size 0) until enable_shroud allocates them ->
    // accessors return all-visible -> inert with shroud off (the water_passable_
    // precedent). explored_ is sticky (fog no-regrow §4): set on reveal, cleared
    // ONLY by reshroud_house (Task 7). visible_ is cleared+recomputed each tick.
    // House index 0=player, 1=enemy, 2=neutral (kNeutralHouse, left inert). NONE
    // read by world_hash.
    // T3 [RE: multihouse.md §D2]: outer [3] -> std::vector sized house_count()
    // (each inner grid still grid_w_*grid_h_). NOT hashed. Sized in
    // ensure_houses_default/grow_house_arrays (inner grids stay EMPTY until
    // enable_shroud).
    std::vector<std::vector<std::uint8_t>> explored_;
    std::vector<std::vector<std::uint8_t>> visible_;
    bool shroud_any_ = false;   // the step_visibility firewall (opened by
                                // enable_shroud); false in every existing gate.
    // SpySat/GPS latch per house [RE: shroud.md §7A]. Recomputed each tick from
    // the house's alive grants_map_reveal buildings; when set, step_visibility
    // keeps the whole map revealed and reshroud_house(h) is SUPPRESSED for h
    // (the §7B "unless SpySat-protected" guard). Index 2 inert. Not hashed.
    // T3 [RE: multihouse.md §D2]: bool[3] latch -> std::vector<uint8_t> sized
    // house_count(). NOT hashed. Sized in ensure_houses_default/grow_house_arrays.
    std::vector<std::uint8_t> map_reveal_;

    // --- Multi-house / Alliances phase [RE: multihouse.md §1/D1] ---
    // The single source of truth for every house. Populated with 3 defaults at
    // construction (0=Allied/team0/comb, 1=Soviet/team1/comb, 2=Neutral/team2/
    // non-comb) so house_count()==3 and today's world is reproduced. NOT hashed.
    std::vector<HouseRec> houses_;
    bool houses_configured_ = false;   // the side/color/enmity/AllyReveal firewall;
                                       // false in every existing gate.

    // Victory / defeat state [RE: multihouse.md §5]. house_defeated_ sized
    // house_count(); sticky. victory_enabled_ (the step_victory firewall) is
    // false in every existing gate -> step_victory early-returns. NONE hashed.
    bool victory_enabled_ = false;
    bool short_game_      = true;
    std::vector<std::uint8_t> house_defeated_;
    int  winning_team_    = -1;
    // Populate houses_ with the 3 defaults if empty (idempotent). Called from the
    // constructor; harmless if invoked again.
    void ensure_houses_default();
    // T3: resize the six per-house vectors + house_defeated_ to n. Stub in Task 2
    // (declared here so set_house compiles).
    void grow_house_arrays(int n);

    // --- CB-1 Transports/Cargo [RE: transport-cargo.md] ---
    // Boarding / carry / eject / transport-death, called between step_units
    // and step_turrets (a pure insertion -- spec §8). Early-returns unless
    // cargo_any_ (the firewall: false until the first enter/unload order).
    void step_cargo(double dt);
    bool cargo_any_ = false;
    std::vector<CargoEvent> cargo_events_;

    // CB-2 Aircraft [RE: aircraft.md §3/§6]: the airborne movement pass --
    // takeoff/cruise/descend + altitude integration. A NEW pass inserted
    // between step_cargo and step_turrets in step(); firewalled behind
    // aircraft_any_ (false until the first aircraft is ordered) so every
    // existing demo pays only the early return [spec §8].
    void step_flight(double dt);
    bool aircraft_any_ = false;

    // Fully-defined-inline POD mirror of HarvestEvent (kind stored as int,
    // via static_cast to/from HarvestEvent::Kind) -- see the forward-
    // declaration comment near the top of this header for why World cannot
    // store a real HarvestEvent (or a container of one) as a member.
    struct PendingHarvestEvent { int kind; int unit; };

    const ra2assets::MapFile* map_;
    std::vector<Unit> units_;
    std::vector<Building> buildings_;
    std::vector<std::uint8_t> passable_;
    // CB-3 Naval [RE: naval.md §1]: the WATER passability grid, empty by default
    // (built only by set_water_terrain -- the naval demo). Same grid_w_*grid_h_
    // shape as passable_. naval_any_ is a diagnostics/guard flag opened by the
    // first naval order (naval movement reuses step_units -- no separate step to
    // firewall, unlike flight's step_flight).
    std::vector<std::uint8_t> water_passable_;
    bool naval_any_ = false;
    Occupancy occ_;
    int grid_w_ = 0;
    int grid_h_ = 0;
    double move_cps_ = 3.0;
    AnimTable anims_;  // Unit::image -> UnitAnimSeq (set_anim); unregistered -> default_anim()

    // --- Economy phase [RE: economy.md] ---
    OreField ore_;
    OreRng ore_rng_;
    // Roster copy + refinery type ids (set_economy). EMPTY types_ (the
    // default -- set_economy never called) is the byte-identity gate:
    // step() skips ore_.step and step_harvest entirely in that case.
    std::vector<BuildingTypeDef> types_;
    std::vector<int> refinery_types_;
    // Type roster for lookups that must NOT arm the economy passes above
    // (set_building_types / btypes()). Read only through btypes().
    std::vector<BuildingTypeDef> lookup_types_;
    // OreValues's two fields, copied by set_economy (see the forward-
    // declaration comment above for why OreValues itself isn't stored).
    int ore_value_ore_ = 25;
    int ore_value_gems_ = 50;
    double growth_interval_ = 0.0;
    double growth_pct_ = 0.0;
    double spread_pct_ = 0.0;
    // T3 [RE: multihouse.md §D2]: double[3] -> std::vector sized house_count().
    // per-house income; index 2 (kNeutralHouse) inert -- neutral holds no credits
    // [RE: capture.md §D1]. NOT hashed. Sized in ensure_houses_default/
    // grow_house_arrays.
    std::vector<double> house_credits_;
    std::vector<PendingHarvestEvent> harvest_events_;  // pending take_harvest_events() drain

    // --- Repair & Sell phase [RE: repair-sell.md] -------------------------
    // ALL inert by default. repair_sell_any_ is opened ONLY by set_repair
    // (turning ON) and begin_sell, neither of which ANY existing demo calls --
    // so step_repair_sell costs every existing gate one increment and one
    // branch, and none of this state is read by world_hash.
    RepairSellRules repair_rules_;              // recovered defaults [§A1]
    bool            repair_sell_any_ = false;   // the firewall
    std::uint64_t   repair_frame_ = 0;          // the global cadence phase [§A2]
    std::vector<UnitCombat>    survivor_crew_;      // by side_ix; empty = none registered
    std::vector<std::uint8_t>  survivor_crew_set_;  // parallel "was registered" flags
    // Finish a sell mission: free the footprint, spawn survivors, pay the FLAT
    // refund, then kill + mark sold. See world.cpp for the recovered ordering.
    void complete_sell(int b_idx);
};

// Indices of `units` sorted back-to-front for painter's-order drawing: by
// screen depth (fx + fy) ascending, tie-break fx ascending then index.
// Includes dying units (die_time >= 0 -- the die anim must still render);
// excludes tombstoned (!alive) units. Pure.
std::vector<int> unit_draw_order(const std::vector<Unit>& units);

}  // namespace ra2engine
