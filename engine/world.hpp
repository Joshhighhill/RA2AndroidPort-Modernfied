#pragma once
#include <cstdint>
#include <string>
#include <vector>

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
    bool attack_order = false;     // explicit order (pursues) vs auto-acquire
    double rof_cool = 0.0;         // seconds until the weapon may fire again
    double fire_flash = 0.0;       // seconds of fire-anim left (renderer)
    int guard_x = 0, guard_y = 0;  // guard anchor (cell when target auto-acquired)
    bool alive = true;             // TOMBSTONE: false after die anim; never erased
    double die_time = -1.0;        // >=0: dying (die anim playing); alive stays true until it ends
};

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
// processed in ascending index order (deterministic). Does NOT modify
// Unit::level -- rendering height is patched separately from the map by
// World::step. Pure: takes a passability grid + occupancy, no MapFile/SDL.
void step_units(std::vector<Unit>& units, const std::vector<std::uint8_t>& passable, int w, int h,
                Occupancy& occ, double dt, double move_cps);

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

    // Deselect every unit.
    void clear_selection();
    // Index of the unit whose CURRENT cell is (x, y), or -1 if none.
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
    // snap). No-op if nothing is selected.
    void order_move_selected(int goal_x, int goal_y);

    // Advance all units by dt seconds (collision-aware, via step_units) and
    // patch each unit's terrain level from the map for rendering height.
    void step(double dt);

private:
    const ra2assets::MapFile* map_;
    std::vector<Unit> units_;
    std::vector<std::uint8_t> passable_;
    Occupancy occ_;
    int grid_w_ = 0;
    int grid_h_ = 0;
    double move_cps_ = 3.0;
};

// Indices of `units` sorted back-to-front for painter's-order drawing: by
// screen depth (fx + fy) ascending, tie-break fx ascending then index. Pure.
std::vector<int> unit_draw_order(const std::vector<Unit>& units);

}  // namespace ra2engine
