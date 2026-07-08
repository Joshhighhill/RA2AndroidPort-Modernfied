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

}  // namespace

std::vector<Cellxy> find_path(const std::vector<uint8_t>& passable, int width, int height,
                              Cellxy start, Cellxy goal) {
    if (width <= 0 || height <= 0) return {};

    auto in_range = [&](int x, int y) {
        return x >= 0 && x < width && y >= 0 && y < height;
    };
    if (!in_range(start.x, start.y) || !in_range(goal.x, goal.y)) return {};

    auto index_of = [&](int x, int y) { return y * width + x; };

    if (passable[index_of(start.x, start.y)] == 0) return {};
    if (passable[index_of(goal.x, goal.y)] == 0) return {};

    if (start.x == goal.x && start.y == goal.y) {
        return { start };
    }

    const int n = width * height;
    const int start_idx = index_of(start.x, start.y);
    const int goal_idx = index_of(goal.x, goal.y);
    const int kInf = std::numeric_limits<int>::max();

    // Octile heuristic to `goal`, scaled to match the integer step costs
    // above: D*(dx+dy) + (D2-2*D)*min(dx,dy) with D=10, D2=14.
    auto octile_h = [&](int x, int y) {
        int dx = std::abs(x - goal.x);
        int dy = std::abs(y - goal.y);
        int mn = std::min(dx, dy);
        int mx = std::max(dx, dy);
        return kDiagonal * mn + kCardinal * (mx - mn);
    };

    std::vector<int> g(static_cast<size_t>(n), kInf);
    std::vector<int> came_from(static_cast<size_t>(n), -1);
    std::vector<uint8_t> closed(static_cast<size_t>(n), 0);

    std::priority_queue<OpenNode, std::vector<OpenNode>, OpenNodeGreater> open;

    g[start_idx] = 0;
    {
        int h0 = octile_h(start.x, start.y);
        open.push(OpenNode{ h0, h0, start_idx });
    }

    while (!open.empty()) {
        OpenNode cur = open.top();
        open.pop();

        // Stale entry (a cheaper one for this cell was already processed).
        if (closed[cur.cell]) continue;

        if (cur.cell == goal_idx) break;  // consistent heuristic -> optimal
        closed[cur.cell] = 1;

        const int cx = cur.cell % width;
        const int cy = cur.cell / width;

        for (int dir = 0; dir < 8; ++dir) {
            const int nx = cx + kDx[dir];
            const int ny = cy + kDy[dir];
            if (!in_range(nx, ny)) continue;

            const int ncell = index_of(nx, ny);
            if (!passable[ncell]) continue;
            if (closed[ncell]) continue;

            // Block diagonal corner-cutting: both orthogonal "corner" cells
            // must be passable for a diagonal move to be allowed.
            if (kDx[dir] != 0 && kDy[dir] != 0) {
                if (!passable[index_of(cx + kDx[dir], cy)]) continue;
                if (!passable[index_of(cx, cy + kDy[dir])]) continue;
            }

            const int tentative_g = g[cur.cell] + kCost[dir];
            if (tentative_g < g[ncell]) {
                g[ncell] = tentative_g;
                came_from[ncell] = cur.cell;
                const int h = octile_h(nx, ny);
                open.push(OpenNode{ tentative_g + h, h, ncell });
            }
        }
    }

    if (g[goal_idx] == kInf) return {};  // unreachable

    std::vector<Cellxy> path;
    for (int cell = goal_idx; ; cell = came_from[cell]) {
        path.push_back(Cellxy{ cell % width, cell / width });
        if (cell == start_idx) break;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

}  // namespace ra2engine
