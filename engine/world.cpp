#include "world.hpp"

namespace ra2engine {

const ra2assets::MapCell* pick_start_cell(const ra2assets::MapFile& map) {
    const auto& cells = map.cells();
    if (cells.empty()) return nullptr;

    // Centroid of every Clear (tile == 0) cell.
    long long sx = 0, sy = 0;
    size_t n = 0;
    for (const auto& c : cells) {
        if (c.tile == 0) {
            sx += c.x;
            sy += c.y;
            ++n;
        }
    }
    // No Clear cell at all would be unusual for a real map; fall back to an
    // arbitrary interior-ish cell (the middle of decode order) rather than
    // leaving the unit unplaced.
    if (n == 0) return &cells[cells.size() / 2];

    const double cx = static_cast<double>(sx) / static_cast<double>(n);
    const double cy = static_cast<double>(sy) / static_cast<double>(n);

    const ra2assets::MapCell* best = nullptr;
    double best_d2 = -1.0;
    for (const auto& c : cells) {
        if (c.tile != 0) continue;
        const double dx = c.x - cx;
        const double dy = c.y - cy;
        const double d2 = dx * dx + dy * dy;
        if (best == nullptr || d2 < best_d2) {
            best = &c;
            best_d2 = d2;
        }
    }
    return best;
}

World::World(const ra2assets::MapFile& map) : map_(&map) {
    if (const ra2assets::MapCell* c = pick_start_cell(map)) {
        unit_.cell_x = c->x;
        unit_.cell_y = c->y;
        unit_.level = c->level;
    }
}

void World::set_unit_image(const std::string& image, int frame) {
    unit_.image = image;
    unit_.frame = frame;
}

}  // namespace ra2engine
