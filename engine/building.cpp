#include "building.hpp"
#include <algorithm>

namespace ra2engine {

std::vector<Cellxy> foundation_cells(int cx, int cy, int fw, int fh) {
    std::vector<Cellxy> out;
    out.reserve((size_t)fw * fh);
    for (int j = 0; j < fh; ++j)
        for (int i = 0; i < fw; ++i)
            out.push_back({cx + i, cy + j});
    return out;
}

// Chebyshev gap between two axis-aligned cell rects (0 = touching/overlap).
static int rect_gap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    int dx = std::max({bx - (ax + aw - 1) - 1, ax - (bx + bw - 1) - 1, 0});
    int dy = std::max({by - (ay + ah - 1) - 1, ay - (by + bh - 1) - 1, 0});
    return std::max(dx, dy);
}

PlacementCheck placement_check(int cx, int cy, int fw, int fh, int house, int adjacent,
                               const std::vector<std::uint8_t>& passable, int grid_w, int grid_h,
                               const Occupancy& occ,
                               const std::vector<Building>& buildings,
                               bool bypass_adjacency) {
    PlacementCheck r;
    auto cells = foundation_cells(cx, cy, fw, fh);
    r.cell_ok.assign(cells.size(), false);
    bool all = true;
    for (size_t k = 0; k < cells.size(); ++k) {
        int x = cells[k].x, y = cells[k].y;
        bool ok = x >= 0 && y >= 0 && x < grid_w && y < grid_h &&
                  passable[(size_t)y * grid_w + x] != 0 && occ.is_free(x, y);
        if (ok) {   // no overlap with any alive building's foundation
            for (const Building& b : buildings) {
                if (!b.alive || b.state == Building::State::Dead) continue;
                if (x >= b.cx && x < b.cx + b.fw && y >= b.cy && y < b.cy + b.fh) {
                    ok = false;
                    break;
                }
            }
        }
        r.cell_ok[k] = ok;
        all = all && ok;
    }
    // [RE: construction.md §9] expanded-rect adjacency vs own alive buildings.
    r.adjacency = bypass_adjacency;
    if (!bypass_adjacency) {
        for (const Building& b : buildings) {
            if (!b.alive || b.state == Building::State::Dead || b.house != house) continue;
            if (rect_gap(cx, cy, fw, fh, b.cx, b.cy, b.fw, b.fh) <= adjacent) {
                r.adjacency = true;
                break;
            }
        }
    }
    r.ok = all && r.adjacency;
    return r;
}

int building_frame(const Building& b, int shp_frame_count, double condition_yellow) {
    int image_frames = shp_frame_count / 2;   // [RE: building-render.md §4] shadows = 2nd half
    if (image_frames < 2) return 0;
    double frac = b.max_hp > 0 ? (double)b.hp / b.max_hp : 0.0;
    return frac <= condition_yellow ? 1 : 0;
}

int buildup_frame(const Building& b, int buildup_shp_frame_count) {
    int image_frames = buildup_shp_frame_count / 2;   // [RE-GATE: building-render.md §5]
    if (image_frames < 1) return 0;
    if (b.buildup_total <= 0.0) return image_frames - 1;
    double period = b.buildup_total / image_frames;   // one frame per period
    int f = (int)(b.buildup_time / period);
    return std::clamp(f, 0, image_frames - 1);
}

std::vector<int> step_buildings(std::vector<Building>& buildings, double dt) {
    std::vector<int> completed;
    for (size_t i = 0; i < buildings.size(); ++i) {
        Building& b = buildings[i];
        if (!b.alive) continue;
        if (b.state == Building::State::Buildup) {
            b.buildup_time += dt;
            if (b.buildup_time >= b.buildup_total) {
                b.state = Building::State::Idle;
                completed.push_back((int)i);
            }
        } else if (b.state == Building::State::Dead) {
            b.die_time += dt;
            if (b.die_time >= kBuildingDieSecs) b.alive = false;
        }
    }
    return completed;
}

void kill_building(Building& b) {
    if (!b.alive || b.state == Building::State::Dead) return;
    b.hp = 0;
    b.state = Building::State::Dead;
    b.die_time = 0.0;
}

}  // namespace ra2engine
