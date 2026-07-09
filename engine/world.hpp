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

// Holds a reference to the (caller-owned, long-lived) MapFile plus the one
// placed Unit and the passability grid derived from the map's cells. Does
// not copy the map's cell data -- the caller must keep the MapFile alive for
// at least as long as the World built from it. Pure ra2assets/engine-layer
// logic; no SDL dependency.
class World {
public:
    explicit World(const ra2assets::MapFile& map);

    const ra2assets::MapFile& map() const { return *map_; }
    const Unit& unit() const { return unit_; }
    Unit& unit() { return unit_; }

    // Passability grid accessors (iso space; see build_passability).
    int grid_w() const { return grid_w_; }
    int grid_h() const { return grid_h_; }
    const std::vector<std::uint8_t>& passable() const { return passable_; }
    // Bounds-checked walkability test at iso cell (x, y).
    bool walkable(int x, int y) const;

    // Sets the unit's SHP image name + starting frame (main.cpp calls this
    // once it has resolved Rules::image("E1") -> "GI").
    void set_unit_image(const std::string& image, int frame = 0);

    // Movement speed in iso cells/sec. Derived by the caller from
    // Rules::speed (see main.cpp); defaults to a sane walk pace so tests /
    // callers that never set it still move.
    void set_move_speed(double cells_per_sec) { move_cps_ = cells_per_sec; }
    double move_speed() const { return move_cps_; }

    // Orders the (already-selected) unit to walk to iso cell (goal_x, goal_y):
    // runs A* from the unit's current cell over the passability grid and, if
    // a real (length > 1) path exists, stores it on the unit and starts
    // movement. A single-cell / empty result (goal == current, unreachable,
    // or off-grid) clears any active path (no-op move) and snaps fx/fy onto
    // the committed cell_x/cell_y. Returns the stored path length (0 if no
    // move started).
    //
    // Safe to call MID-WALK (retargeting an already-moving unit): on success
    // fx/fy are left untouched (NOT reset to cell_x/cell_y), so a unit
    // retargeted while walking keeps its current interpolated position
    // instead of snapping backward to its last committed cell (Task 12
    // review Finding 1 -- see world.cpp for the failure this avoids).
    std::size_t order_move(int goal_x, int goal_y);

    // Advances the unit along its path by `dt` seconds (interpolating
    // fx/fy toward the next cell at move_speed cells/sec, committing
    // cell_x/cell_y + level as each cell is reached) and cycles the walk
    // animation frame; snaps to a stand frame and clears the path on arrival
    // or when idle. Safe to call every frame regardless of state.
    void step(double dt);

private:
    const ra2assets::MapFile* map_;
    Unit unit_;
    std::vector<std::uint8_t> passable_;
    int grid_w_ = 0;
    int grid_h_ = 0;
    double move_cps_ = 3.0;  // default cells/sec if caller never sets it
};

}  // namespace ra2engine
