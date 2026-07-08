#include "world.hpp"

#include <algorithm>
#include <cmath>

namespace ra2engine {

// GI.SHP (temperate infantry) walk animation, facing 0. RA2 infantry SHPs
// lay their sequences out as [start, count-per-facing, facing-multiplier];
// the stock E1/GI "Ready" pose is frame 0 and its "Walk" cycle is 6 frames
// starting at frame 8 (facing 0). GI.SHP has 744 frames (task-10-report.md),
// so this range is always valid. Facing is intentionally NOT modelled for
// this slice (the brief: "facing not required") -- one facing's walk cycle
// reads clearly as a walking soldier. Documented as the tuned choice.
static constexpr int kStandFrame = 0;
static constexpr int kWalkFirst = 8;
static constexpr int kWalkCount = 6;
static constexpr double kWalkFps = 9.0;  // walk-cycle frame rate

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

std::vector<std::uint8_t> build_passability(const std::vector<ra2assets::MapCell>& cells,
                                            int& grid_w, int& grid_h) {
    grid_w = 0;
    grid_h = 0;
    if (cells.empty()) return {};

    int max_x = 0, max_y = 0;
    for (const auto& c : cells) {
        // MapCell.x/y are raw iso coords (roughly 1..212, always >= 0 for a
        // real map); guard against a stray negative rather than indexing OOB.
        if (c.x > max_x) max_x = c.x;
        if (c.y > max_y) max_y = c.y;
    }
    grid_w = max_x + 1;
    grid_h = max_y + 1;

    std::vector<std::uint8_t> passable(static_cast<size_t>(grid_w) * grid_h, 0);
    for (const auto& c : cells) {
        if (c.x < 0 || c.y < 0) continue;  // defensive; not seen in real maps
        passable[static_cast<size_t>(c.y) * grid_w + c.x] = 1;
    }
    return passable;
}

World::World(const ra2assets::MapFile& map) : map_(&map) {
    if (const ra2assets::MapCell* c = pick_start_cell(map)) {
        unit_.cell_x = c->x;
        unit_.cell_y = c->y;
        unit_.level = c->level;
    }
    unit_.fx = unit_.cell_x;
    unit_.fy = unit_.cell_y;
    passable_ = build_passability(map.cells(), grid_w_, grid_h_);
}

bool World::walkable(int x, int y) const {
    if (x < 0 || y < 0 || x >= grid_w_ || y >= grid_h_) return false;
    return passable_[static_cast<size_t>(y) * grid_w_ + x] != 0;
}

void World::set_unit_image(const std::string& image, int frame) {
    unit_.image = image;
    unit_.frame = frame;
}

std::size_t World::order_move(int goal_x, int goal_y) {
    Unit& u = unit_;
    std::vector<Cellxy> path =
        find_path(passable_, grid_w_, grid_h_, {u.cell_x, u.cell_y}, {goal_x, goal_y});
    // A single-cell path (goal == current) or empty (unreachable / off-grid)
    // means "nowhere to walk": clear any active order and stay put.
    if (path.size() <= 1) {
        u.path.clear();
        u.path_idx = 0;
        u.anim_time = 0.0;
        u.frame = kStandFrame;
        return 0;
    }
    u.path = std::move(path);
    u.path_idx = 1;  // path[0] is the cell we're standing on
    u.anim_time = 0.0;
    // fx/fy already sit exactly on the current cell.
    u.fx = u.cell_x;
    u.fy = u.cell_y;
    return u.path.size();
}

void World::step(double dt) {
    Unit& u = unit_;
    if (!u.moving()) {
        u.frame = kStandFrame;
        return;
    }

    // Budget of cells we may traverse this tick (euclidean in iso-cell space;
    // a cardinal step is 1.0, a diagonal ~1.414). Walk the remaining budget
    // across as many cell segments as it covers so a large dt never overshoots
    // or skips animation.
    double budget = move_cps_ * dt;
    while (budget > 0.0 && u.path_idx < u.path.size()) {
        const Cellxy& tgt = u.path[u.path_idx];
        const double dx = tgt.x - u.fx;
        const double dy = tgt.y - u.fy;
        const double dist = std::sqrt(dx * dx + dy * dy);
        if (dist <= 1e-9 || budget >= dist) {
            // Reached this cell: snap onto it, commit the integer cell + level,
            // and move on to the next path node with the leftover budget.
            u.fx = tgt.x;
            u.fy = tgt.y;
            u.cell_x = tgt.x;
            u.cell_y = tgt.y;
            if (const ra2assets::MapCell* mc = map_->at(tgt.x, tgt.y))
                u.level = mc->level;
            budget -= (dist <= 1e-9 ? 0.0 : dist);
            ++u.path_idx;
        } else {
            const double t = budget / dist;
            u.fx += dx * t;
            u.fy += dy * t;
            budget = 0.0;
        }
    }

    if (u.path_idx >= u.path.size()) {
        // Arrived: stop, stand, drop the path.
        u.fx = u.cell_x;
        u.fy = u.cell_y;
        u.path.clear();
        u.path_idx = 0;
        u.anim_time = 0.0;
        u.frame = kStandFrame;
        return;
    }

    // Still walking: cycle the walk frames.
    u.anim_time += dt;
    const int phase = static_cast<int>(u.anim_time * kWalkFps) % kWalkCount;
    u.frame = kWalkFirst + phase;
}

}  // namespace ra2engine
