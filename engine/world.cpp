#include "world.hpp"

#include <algorithm>
#include <cmath>

#include "iso_project.hpp"   // cell_to_screen (terrain_screen_bounds)
#include "iso_renderer.hpp"  // terrain_screen_bounds declaration
#include "pathfind.hpp"

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

// Seconds between re-path attempts while blocked (next cell held by another
// unit). Short enough to feel responsive, long enough that a unit briefly
// waiting for one ahead to step off doesn't thrash A* every frame.
static constexpr double kBlockedWait = 0.30;
// Seconds of CONTINUOUS block after which a unit abandons its order (boxed-in
// or sealed corridor). Transient jams (a funnel cell mid-crossing) clear well
// inside this: the per-frame can_enter check resumes the walk the instant the
// blocker steps off, and failed re-paths retry every kBlockedWait until then.
static constexpr double kGiveUpAfter = 2.0;

// RA2 temperate tile geometry -- must match iso_renderer.cpp's / input.cpp's
// kTileW/kTileH/kLevelPx (the same duplicated-constant pattern already
// flagged as a Minor in task-12-report.md; not made any worse here). Used
// only by terrain_screen_bounds below.
static constexpr int kTileW = 60;
static constexpr int kTileH = 30;
static constexpr int kLevelPx = 15;

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

// Screen-space bounding box (camera-local projected pixels, level included)
// of every cell in `map`. Declared in iso_renderer.hpp (it's conceptually a
// renderer/camera helper -- main.cpp's centered_camera uses it), but defined
// HERE rather than in iso_renderer.cpp: iso_renderer.cpp pulls in SDL and is
// not part of the SDL-free `ra2assets_tests` binary, while world.cpp already
// is (same single-source-of-truth pattern as engine/iso_project.cpp and
// engine/pathfind.cpp -- see CMakeLists.txt). Input needs this pure function
// to clamp camera drag to the map's extent (Task 12 review Finding 2)
// without dragging SDL into the test build.
bool terrain_screen_bounds(const ra2assets::MapFile& map, int& min_x, int& min_y, int& max_x,
                           int& max_y) {
    const auto& cells = map.cells();
    if (cells.empty()) return false;
    int mnx = 1 << 30, mny = 1 << 30, mxx = -(1 << 30), mxy = -(1 << 30);
    for (const auto& c : cells) {
        const ScreenPt p =
            cell_to_screen(c.x, c.y, static_cast<int>(c.level), kTileW, kTileH, kLevelPx);
        mnx = std::min(mnx, p.x);
        mny = std::min(mny, p.y);
        mxx = std::max(mxx, p.x + kTileW);
        mxy = std::max(mxy, p.y + kTileH);
    }
    min_x = mnx;
    min_y = mny;
    max_x = mxx;
    max_y = mxy;
    return true;
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
    // means "nowhere to walk": clear any active order and stay put. Snap
    // fx/fy onto the committed cell here -- if this fires MID-WALK (fx/fy
    // fractional, between two committed cells), moving() is about to go
    // false and step() will early-return forever, so without this the
    // sprite would be stranded off-grid at whatever fractional position it
    // was interpolating through (Task 12 review Finding 1).
    if (path.size() <= 1) {
        u.path.clear();
        u.path_idx = 0;
        u.anim_time = 0.0;
        u.frame = kStandFrame;
        u.fx = u.cell_x;
        u.fy = u.cell_y;
        return 0;
    }
    u.path = std::move(path);
    u.path_idx = 1;  // path[0] is the cell we're standing on
    u.anim_time = 0.0;
    // Do NOT touch fx/fy here (Task 12 review Finding 1): if the unit is
    // idle, fx/fy already sit exactly on cell_x/cell_y, so this is moot. But
    // if it is RETARGETED MID-WALK, fx/fy hold the current in-flight
    // interpolated position; resetting them to cell_x/cell_y would snap the
    // sprite backward to its last committed cell, discarding the walk that
    // was already rendered. step() interpolates fine from any starting
    // fx/fy toward path[path_idx] regardless of whether fx/fy lies exactly
    // on that segment, so leaving it alone keeps the walk visually
    // continuous across a reorder.
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

// Copy `passable` but mark every cell OWNED/RESERVED by a unit other than
// `self` impassable, so a re-path routes around the current traffic. The
// unit's own start cell is forced walkable (find_path needs a passable start).
static std::vector<std::uint8_t> passable_minus_units(const std::vector<std::uint8_t>& passable,
                                                      int w, int h, const Occupancy& occ, int self,
                                                      int start_x, int start_y) {
    std::vector<std::uint8_t> out = passable;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int o = occ.at(x, y);
            if (o != Occupancy::kEmpty && o != self)
                out[static_cast<std::size_t>(y) * w + x] = 0;
        }
    if (start_x >= 0 && start_y >= 0 && start_x < w && start_y < h)
        out[static_cast<std::size_t>(start_y) * w + start_x] = 1;
    return out;
}

void step_units(std::vector<Unit>& units, const std::vector<std::uint8_t>& passable, int w, int h,
                Occupancy& occ, double dt, double move_cps) {
    for (std::size_t i = 0; i < units.size(); ++i) {
        Unit& u = units[i];
        const int self = static_cast<int>(i);

        if (!u.moving()) {
            occ.set(u.cell_x, u.cell_y, self);  // idle units own their cell
            u.fx = u.cell_x;
            u.fy = u.cell_y;
            u.frame = kStandFrame;
            u.wait_time = 0.0;
            continue;
        }

        double budget = move_cps * dt;
        bool blocked = false;
        while (budget > 0.0 && u.path_idx < u.path.size()) {
            const Cellxy tgt = u.path[u.path_idx];
            if (tgt.x == u.cell_x && tgt.y == u.cell_y) {  // already here (e.g. path[0])
                ++u.path_idx;
                continue;
            }
            // Reserve the next cell before stepping into it.
            if (occ.at(tgt.x, tgt.y) != self) {
                if (occ.can_enter(tgt.x, tgt.y, self)) {
                    occ.set(tgt.x, tgt.y, self);  // reserve (now owns current + next)
                } else {
                    blocked = true;
                    break;
                }
            }
            const double dx = tgt.x - u.fx;
            const double dy = tgt.y - u.fy;
            const double dist = std::sqrt(dx * dx + dy * dy);
            if (dist <= 1e-9 || budget >= dist) {
                occ.release(u.cell_x, u.cell_y, self);  // leave the old cell
                u.fx = tgt.x;
                u.fy = tgt.y;
                u.cell_x = tgt.x;
                u.cell_y = tgt.y;  // occ already owns this cell (reserved above)
                budget -= (dist <= 1e-9 ? 0.0 : dist);
                ++u.path_idx;
                u.wait_time = 0.0;
            } else {
                const double t = budget / dist;
                u.fx += dx * t;
                u.fy += dy * t;
                budget = 0.0;
            }
        }

        if (u.path_idx >= u.path.size()) {  // arrived
            u.fx = u.cell_x;
            u.fy = u.cell_y;
            u.path.clear();
            u.path_idx = 0;
            u.anim_time = 0.0;
            u.frame = kStandFrame;
            u.wait_time = 0.0;
            occ.set(u.cell_x, u.cell_y, self);
            continue;
        }

        if (blocked) {
            const double prev_wait = u.wait_time;
            u.wait_time += dt;
            u.frame = kStandFrame;  // stand while waiting (reads as "holding")
            // Seniority stagger (breaks the mirror-symmetric re-path
            // livelock): if the unit blocking our next cell has a LOWER index,
            // it holds course and WE detour early (first attempt at
            // kBlockedWait); if it has a HIGHER index, it is expected to
            // detour first, so our first attempt is delayed one extra
            // interval. Without this, two facing units cross the re-path
            // threshold on the same frame every cycle and -- with symmetric
            // occupancy and deterministic A* tie-breaks -- each computes the
            // mirror image of the other's detour, oscillating between rows
            // forever (a stable 2-cycle: observed in a 200s trace, not
            // hypothetical).
            const Cellxy blk = u.path[u.path_idx];
            const int blocker = occ.at(blk.x, blk.y);
            const double first_try =
                (blocker != Occupancy::kEmpty && blocker < self)
                    ? kBlockedWait
                    : 2.0 * kBlockedWait;
            if (u.wait_time >= kGiveUpAfter) {
                // Continuously blocked with no route found for the whole
                // timeout (boxed-in / sealed corridor) -> give up cleanly.
                u.path.clear();
                u.path_idx = 0;
                u.fx = u.cell_x;
                u.fy = u.cell_y;
                u.wait_time = 0.0;
            } else if (u.wait_time >= first_try &&
                       (prev_wait < first_try ||
                        static_cast<int>((prev_wait - first_try) / kBlockedWait) !=
                            static_cast<int>((u.wait_time - first_try) / kBlockedWait))) {
                // On a re-path attempt boundary (first_try, then every
                // kBlockedWait after) -> attempt a re-path around the current
                // traffic. On FAILURE keep the original path and keep waiting:
                // transient jams (another unit mid-crossing of a funnel cell)
                // clear on their own, and the per-frame can_enter check above
                // resumes the walk the moment the blocker steps off. Only the
                // kGiveUpAfter timeout abandons the order.
                const Cellxy goal = u.path.back();
                std::vector<std::uint8_t> p2 =
                    passable_minus_units(passable, w, h, occ, self, u.cell_x, u.cell_y);
                std::vector<Cellxy> np = find_path(p2, w, h, {u.cell_x, u.cell_y}, goal);
                if (np.size() > 1) {
                    u.path = std::move(np);
                    u.path_idx = 1;
                    u.wait_time = 0.0;
                }
            }
            continue;
        }

        // Still walking: cycle the walk frames.
        u.anim_time += dt;
        const int phase = static_cast<int>(u.anim_time * kWalkFps) % kWalkCount;
        u.frame = kWalkFirst + phase;
    }
}

}  // namespace ra2engine
