#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "building.hpp"
#include "occupancy.hpp"
#include "pathfind.hpp"  // Cellxy (path storage / find_path)
#include "ra2assets/map.hpp"

namespace ra2engine {

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
void step_units(std::vector<Unit>& units, const std::vector<std::uint8_t>& passable, int w, int h,
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

// Per-unit combat stats resolved once by the caller (main.cpp, from Rules)
// and handed to World::add_combat_unit to fill in a spawned Unit's hp/armor/
// weapon fields. Mirrors the Rules-derived values Task 2 exposes (strength,
// armor, the primary weapon's damage/rof/range, and its warhead's verses)
// plus the SHP image name add_unit already takes directly.
struct UnitCombat {
    int strength = 1;
    std::string armor;
    std::string image;
    int damage = 0;
    int rof = 0;
    double range = 0.0;
    std::vector<int> verses;
    // --- Voxel Vehicles phase (docs/re/voxel-render.md §5) ---
    int rot = 0;               // Rules ROT= (turn rate); 0 = no turning
    bool turret = false;       // has an independently-slewing turret
    double speed_cps = 0.0;    // per-unit speed in cells/sec; 0 = World move_cps
    bool is_vehicle = false;   // gates all vehicle-specific locomotion
};

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

// Holds a reference to the (caller-owned, long-lived) MapFile plus the placed
// Units, the passability grid, and a cell-occupancy grid derived from the
// map's cells. Does not copy the map's cell data -- the caller must keep the
// MapFile alive at least as long as the World. Pure engine-layer logic; no SDL.
class World {
public:
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
    bool walkable(int x, int y) const;
    const Occupancy& occupancy() const { return occ_; }

    // Movement speed in iso cells/sec (see main.cpp for the Rules-derived value).
    void set_move_speed(double cells_per_sec) { move_cps_ = cells_per_sec; }
    double move_speed() const { return move_cps_; }

    // Order every SELECTED unit to walk toward (goal_x, goal_y): assigns each a
    // distinct formation cell spiralling out from the goal, then paths it there.
    // Units retargeted mid-walk keep their interpolated fx/fy (no backward
    // snap). Dying/dead selected units are skipped. No-op if nothing eligible
    // is selected.
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
                     int armor_ix, double buildup_total, bool instant);

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

    // Advance all units by dt seconds: step_units (collision-aware movement)
    // -> step_combat (targeting/firing/deaths, returns pursuit/return-to-
    // anchor MoveRequests) -> path each request via order_unit_move (skipping
    // any request for a unit that died/started dying this same step) -> patch
    // each still-alive unit's terrain level from the map for rendering height.
    void step(double dt);

private:
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
    void order_unit_move(int idx, int gx, int gy);

    const ra2assets::MapFile* map_;
    std::vector<Unit> units_;
    std::vector<Building> buildings_;
    std::vector<std::uint8_t> passable_;
    Occupancy occ_;
    int grid_w_ = 0;
    int grid_h_ = 0;
    double move_cps_ = 3.0;
    AnimTable anims_;  // Unit::image -> UnitAnimSeq (set_anim); unregistered -> default_anim()
};

// Indices of `units` sorted back-to-front for painter's-order drawing: by
// screen depth (fx + fy) ascending, tie-break fx ascending then index.
// Includes dying units (die_time >= 0 -- the die anim must still render);
// excludes tombstoned (!alive) units. Pure.
std::vector<int> unit_draw_order(const std::vector<Unit>& units);

}  // namespace ra2engine
