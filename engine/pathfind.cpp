#include "pathfind.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <queue>

namespace ra2engine {

namespace {

// 8 directions in a fixed, stable order: 4 cardinal first (E, W, S, N), then
// 4 diagonal (SE, SW, NE, NW). Costs are the standard scaled-integer octile
// step costs: cardinal = 10, diagonal = 14 (~10*sqrt(2)).
constexpr int kDx[8]   = { 1, -1, 0, 0, 1, 1, -1, -1 };
constexpr int kDy[8]   = { 0, 0, 1, -1, 1, -1, 1, -1 };
constexpr int kCost[8] = { 10, 10, 10, 10, 14, 14, 14, 14 };
constexpr int kCardinal = 10;
constexpr int kDiagonal = 14;

// Node kept in the open set's binary heap.
struct OpenNode {
    int f;     // g + h
    int h;     // heuristic (used only for tie-breaking)
    int cell;  // y*width+x (used only for tie-breaking, for full determinism)
};

// Min-heap by f, tie-broken by lower h, then by lower cell index -- makes the
// expansion order fully deterministic regardless of insertion order/heap
// implementation details.
struct OpenNodeGreater {
    bool operator()(const OpenNode& a, const OpenNode& b) const {
        if (a.f != b.f) return a.f > b.f;
        if (a.h != b.h) return a.h > b.h;
        return a.cell > b.cell;
    }
};

// W2-SK-16: per-thread scratch reused across calls with a generation stamp
// (was three full-grid allocations per call). A cell whose stamp != gen reads
// as g = inf, came_from = -1, open (not closed) -- identical semantics.
struct Scratch {
    std::vector<int> g, came;
    std::vector<std::uint32_t> stamp;
    std::vector<std::uint8_t> closed;
    std::uint32_t gen = 0;
    void prepare(int n) {
        if ((int)stamp.size() != n) {
            g.assign((size_t)n, 0);
            came.assign((size_t)n, -1);
            stamp.assign((size_t)n, 0);
            closed.assign((size_t)n, 0);
            gen = 0;
        }
        if (++gen == 0) {   // wrapped: reset every stamp once
            std::fill(stamp.begin(), stamp.end(), 0u);
            gen = 1;
        }
    }
};
thread_local Scratch t_scratch;

enum class Mode { Exact, Nearest };

// The one A* body. Exact: the historical find_path (empty when unreachable, or
// when a budget > 0 runs out). Nearest: when the goal is not reached, the path
// to the closed cell nearest the goal (lowest h, then lowest g, then index).
std::vector<Cellxy> astar(const std::vector<uint8_t>& passable, int width, int height,
                          Cellxy start, Cellxy goal, int max_expand, Mode mode,
                          const std::vector<uint8_t>* leak) {
    if (leak && leak->size() != passable.size()) leak = nullptr;
    const int n = width * height;
    auto index_of = [&](int x, int y) { return y * width + x; };
    auto in_range = [&](int x, int y) { return x >= 0 && x < width && y >= 0 && y < height; };
    const int start_idx = index_of(start.x, start.y);
    const int goal_idx = index_of(goal.x, goal.y);
    const int kInf = std::numeric_limits<int>::max();
    auto octile_h = [&](int x, int y) {
        int dx = std::abs(x - goal.x);
        int dy = std::abs(y - goal.y);
        int mn = std::min(dx, dy);
        int mx = std::max(dx, dy);
        return kDiagonal * mn + kCardinal * (mx - mn);
    };
    Scratch& S = t_scratch;
    S.prepare(n);
    const std::uint32_t gen = S.gen;
    auto touch = [&](int c) {
        if (S.stamp[(size_t)c] != gen) {
            S.stamp[(size_t)c] = gen;
            S.g[(size_t)c] = kInf;
            S.came[(size_t)c] = -1;
            S.closed[(size_t)c] = 0;
        }
    };
    auto gval = [&](int c) { return S.stamp[(size_t)c] == gen ? S.g[(size_t)c] : kInf; };
    auto is_closed = [&](int c) { return S.stamp[(size_t)c] == gen && S.closed[(size_t)c] != 0; };

    std::priority_queue<OpenNode, std::vector<OpenNode>, OpenNodeGreater> open;
    touch(start_idx);
    S.g[(size_t)start_idx] = 0;
    const int h_start = octile_h(start.x, start.y);
    open.push(OpenNode{ h_start, h_start, start_idx });
    int best_cell = start_idx, best_h = h_start, best_g = 0;
    int expanded = 0;
    bool reached = false;
    while (!open.empty()) {
        OpenNode cur = open.top();
        open.pop();
        if (is_closed(cur.cell)) continue;   // stale entry
        if (cur.cell == goal_idx) { reached = true; break; }   // consistent heuristic -> optimal
        S.closed[(size_t)cur.cell] = 1;
        if (mode == Mode::Nearest) {
            const int gc = S.g[(size_t)cur.cell];
            if (cur.h < best_h ||
                (cur.h == best_h && (gc < best_g || (gc == best_g && cur.cell < best_cell)))) {
                best_h = cur.h;
                best_g = gc;
                best_cell = cur.cell;
            }
        }
        if (max_expand > 0 && ++expanded >= max_expand) break;
        const int cx = cur.cell % width;
        const int cy = cur.cell / width;
        for (int dir = 0; dir < 8; ++dir) {
            const int nx = cx + kDx[dir];
            const int ny = cy + kDy[dir];
            if (!in_range(nx, ny)) continue;
            const int ncell = index_of(nx, ny);
            if (!passable[(size_t)ncell]) continue;
            if (is_closed(ncell)) continue;
            // Block diagonal corner-cutting: both orthogonal "corner" cells
            // must be passable for a diagonal move to be allowed.
            // W5-A: a corner cell blocked only by a wall overlay (`leak`) does
            // not block the diagonal -- walls touching only at their corners
            // leave a gap [RE-FALLBACK: the original's corner rule is untraced;
            // map evidence: ALL12's Tesla Reactor fence has exactly such a gap at
            // the ParaSEALS drop point, and the reactor (objective 1) is
            // unreachable otherwise].
            if (kDx[dir] != 0 && kDy[dir] != 0) {
                const int c1 = index_of(cx + kDx[dir], cy), c2 = index_of(cx, cy + kDy[dir]);
                if (!passable[(size_t)c1] && !(leak && (*leak)[(size_t)c1])) continue;
                if (!passable[(size_t)c2] && !(leak && (*leak)[(size_t)c2])) continue;
            }
            const int tentative_g = S.g[(size_t)cur.cell] + kCost[dir];
            if (tentative_g < gval(ncell)) {
                touch(ncell);
                S.g[(size_t)ncell] = tentative_g;
                S.came[(size_t)ncell] = cur.cell;
                const int h = octile_h(nx, ny);
                open.push(OpenNode{ tentative_g + h, h, ncell });
            }
        }
    }
    int end_cell;
    if (reached) end_cell = goal_idx;
    else if (mode == Mode::Exact) return {};
    else end_cell = best_cell;
    std::vector<Cellxy> path;
    for (int cell = end_cell; cell >= 0; cell = S.came[(size_t)cell]) {
        path.push_back(Cellxy{ cell % width, cell / width });
        if (cell == start_idx) break;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

bool basic_checks(const std::vector<uint8_t>& passable, int width, int height, Cellxy start,
                  Cellxy goal) {
    if (width <= 0 || height <= 0) return false;
    if (passable.size() != static_cast<size_t>(width) * static_cast<size_t>(height)) return false;
    auto in_range = [&](int x, int y) { return x >= 0 && x < width && y >= 0 && y < height; };
    return in_range(start.x, start.y) && in_range(goal.x, goal.y);
}

}  // namespace

std::vector<Cellxy> find_path(const std::vector<uint8_t>& passable, int width, int height,
                              Cellxy start, Cellxy goal, const std::vector<uint8_t>* leak) {
    if (!basic_checks(passable, width, height, start, goal)) return {};
    if (passable[(size_t)(start.y * width + start.x)] == 0) return {};
    if (passable[(size_t)(goal.y * width + goal.x)] == 0) return {};
    if (start.x == goal.x && start.y == goal.y) return { start };
    return astar(passable, width, height, start, goal, 0, Mode::Exact, leak);
}

std::vector<Cellxy> find_path_bounded(const std::vector<uint8_t>& passable, int width, int height,
                                      Cellxy start, Cellxy goal, int max_expand,
                                      const std::vector<uint8_t>* leak) {
    if (!basic_checks(passable, width, height, start, goal)) return {};
    if (passable[(size_t)(start.y * width + start.x)] == 0) return {};
    if (passable[(size_t)(goal.y * width + goal.x)] == 0) return {};
    if (start.x == goal.x && start.y == goal.y) return { start };
    return astar(passable, width, height, start, goal, max_expand, Mode::Exact, leak);
}

std::vector<Cellxy> find_path_nearest(const std::vector<uint8_t>& passable, int width, int height,
                                      Cellxy start, Cellxy goal, int max_expand,
                                      const std::vector<uint8_t>* leak) {
    if (!basic_checks(passable, width, height, start, goal)) return {};
    if (passable[(size_t)(start.y * width + start.x)] == 0) return {};
    // Goal relaxation: an impassable goal becomes the nearest passable cell
    // (Chebyshev rings out to 16, Euclid-nearest per ring, scan order ties).
    if (passable[(size_t)(goal.y * width + goal.x)] == 0) {
        bool found = false;
        for (int r = 1; r <= 16 && !found; ++r) {
            int best = -1;
            long long bd = 0;
            for (int y = goal.y - r; y <= goal.y + r; ++y)
                for (int x = goal.x - r; x <= goal.x + r; ++x) {
                    if (std::max(std::abs(x - goal.x), std::abs(y - goal.y)) != r) continue;
                    if (x < 0 || y < 0 || x >= width || y >= height) continue;
                    if (!passable[(size_t)(y * width + x)]) continue;
                    const long long d = (long long)(x - goal.x) * (x - goal.x) +
                                        (long long)(y - goal.y) * (y - goal.y);
                    if (best < 0 || d < bd) { best = y * width + x; bd = d; }
                }
            if (best >= 0) { goal = Cellxy{ best % width, best / width }; found = true; }
        }
        if (!found) return { start };
    }
    if (start.x == goal.x && start.y == goal.y) return { start };
    return astar(passable, width, height, start, goal, max_expand, Mode::Nearest, leak);
}

}  // namespace ra2engine
