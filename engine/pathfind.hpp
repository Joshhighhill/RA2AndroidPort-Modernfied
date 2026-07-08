#pragma once

#include <cstdint>
#include <vector>

namespace ra2engine {

// Grid cell coordinate (x, y) in passability-grid space.
struct Cellxy { int x, y; };

// A* pathfinding over an 8-connected width*height passability grid.
//
// PURE math/algorithm -- no SDL/OS deps, so it is unit-testable without a
// window (see tests/test_pathfind.cpp) and is compiled into both
// `ra2assets_tests` and `ra2engine` from a single source of truth, same
// pattern as engine/iso_project.cpp (see CMakeLists.txt).
//
// `passable[y*width+x] != 0` means the cell is walkable. Movement is
// 8-directional (4 cardinal + 4 diagonal); diagonal moves are blocked if
// either of the two orthogonal "corner" cells they would cut across is
// impassable (prevents cutting through a wall's corner). The heuristic is
// the standard octile distance, scaled to integers (cardinal step cost 10,
// diagonal step cost 14, i.e. 10*sqrt(2) rounded) to keep the search
// deterministic.
//
// Returns the sequence of cells from `start` to `goal` inclusive. Returns an
// empty vector if: start or goal is out of [0,width)x[0,height) range,
// either start or goal cell is impassable, or no path exists. If
// `start == goal` (and it is in range and passable), returns a single-cell
// path containing just that cell.
std::vector<Cellxy> find_path(const std::vector<uint8_t>& passable, int width, int height,
                              Cellxy start, Cellxy goal);

}  // namespace ra2engine
