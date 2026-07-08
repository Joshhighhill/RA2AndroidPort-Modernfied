#pragma once
#include <string>

#include "ra2assets/map.hpp"

namespace ra2engine {

// A single visible/controllable actor in the world: its map cell + height
// level (raw IsoMapPack5 coordinate space -- see ra2assets::MapCell), the
// SHP base image name (a `Rules::image()` result, e.g. unit id "E1" ->
// "GI" -> GI.SHP), and the current animation frame index into that SHP.
// Task 11/12 add movement/pathfinding/passability and multiple units; this
// vertical slice holds exactly one, placed once at construction.
struct Unit {
    int cell_x = 0;
    int cell_y = 0;
    int level = 0;
    int frame = 0;
    std::string image;
};

// Picks a passable (Clear/grass, `tile == 0` -- confirmed in Task 9 /
// task-9-report.md: "tile index 0 is Clear") cell close to the centroid of
// every Clear cell on the map. That centroid sits in the map's open,
// populated interior (biased away from edges/water/cliffs), and -- because
// `cell_to_screen` is linear in (x, y) -- projects close to the same region
// `terrain_screen_bounds`'s bounding-box center does, so a unit placed here
// lands inside whatever viewport `main.cpp` centers on the map (Task 9's
// `centered_camera`). Full tile-type passability comes in Task 11; this is
// a deliberately simple "any Clear cell near the middle" heuristic.
// Returns nullptr if the map has no cells at all.
const ra2assets::MapCell* pick_start_cell(const ra2assets::MapFile& map);

// Holds a reference to the (caller-owned, long-lived) MapFile plus the one
// placed Unit. Does not copy the map's cell data -- the caller must keep the
// MapFile alive for at least as long as the World built from it. Pure
// ra2assets/engine-layer logic; no SDL dependency.
class World {
public:
    explicit World(const ra2assets::MapFile& map);

    const ra2assets::MapFile& map() const { return *map_; }
    const Unit& unit() const { return unit_; }
    Unit& unit() { return unit_; }

    // Sets the unit's SHP image name + starting frame (main.cpp calls this
    // once it has resolved Rules::image("E1") -> "GI").
    void set_unit_image(const std::string& image, int frame = 0);

private:
    const ra2assets::MapFile* map_;
    Unit unit_;
};

}  // namespace ra2engine
