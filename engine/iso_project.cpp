#include "iso_project.hpp"

#include <cmath>

namespace ra2engine {

ScreenPt cell_to_screen(int cell_x, int cell_y, int level, int tile_w, int tile_h, int level_px) {
    const int hw = tile_w / 2;
    const int hh = tile_h / 2;
    ScreenPt p;
    p.x = (cell_x - cell_y) * hw;
    p.y = (cell_x + cell_y) * hh - level * level_px;
    return p;
}

void screen_to_cell(int px, int py, int tile_w, int tile_h, int& cell_x, int& cell_y) {
    const double hw = tile_w / 2.0;
    const double hh = tile_h / 2.0;
    const double fx = (px / hw + py / hh) / 2.0;
    const double fy = (py / hh - px / hw) / 2.0;
    cell_x = static_cast<int>(std::lround(fx));
    cell_y = static_cast<int>(std::lround(fy));
}

}  // namespace ra2engine
