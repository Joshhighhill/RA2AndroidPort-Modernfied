#include "factory.hpp"
#include <algorithm>

namespace ra2engine {

namespace {
// 8-neighbour (dx,dy) ring [RE-GATE APPLIED: production.md §1.3 — the
// DAT_0089E5F8 table shared with the harvester dig ring (economy.md §1)],
// rotated so the deterministic door facing (SOUTH) is probed first
// [RE-FALLBACK: the original seeds the start index from the lockstep RNG].
constexpr int kExitRing[8][2] = {
    {0, 1}, {-1, 1}, {1, 1}, {-1, 0}, {1, 0}, {-1, -1}, {1, -1}, {0, -1}};
}  // namespace

Cellxy exit_cell(const Building& factory, const BuildingTypeDef& def,
                 const std::function<bool(int, int)>& free) {
    // [RE-GATE APPLIED: production.md §1.3 — KickOutUnit FUN_00443BC0: walk
    // the 8-ring outward from the building's anchor cell to the first free
    // cell; foundation cells are skipped because they are never free. The
    // radius expands so wide foundations still find a door (the original
    // loops "while occupant == this_building"). ExitCoord's sub-cell lean is
    // dropped at cell resolution (Task 9 divergence).]
    const int ax = factory.cx + def.fw / 2;   // anchor: foundation center cell
    const int ay = factory.cy + def.fh / 2;
    const int max_r = std::max(def.fw, def.fh) + 1;
    for (int r = 1; r <= max_r; ++r)
        for (const auto& d : kExitRing) {
            const int x = ax + d[0] * r, y = ay + d[1] * r;
            if (free(x, y)) return Cellxy{x, y};
        }
    return Cellxy{-1, -1};                     // fully boxed in -> caller HOLDs
}

namespace {
bool is_cat(const std::vector<int>& factory_types, int type) {
    return std::find(factory_types.begin(), factory_types.end(), type) != factory_types.end();
}
}  // namespace

int primary_factory(const std::vector<Building>& buildings,
                    const std::vector<int>& factory_types, int house) {
    int fallback = -1;
    for (int i = 0; i < (int)buildings.size(); ++i) {
        const Building& b = buildings[i];
        if (!b.alive || b.state != Building::State::Idle || b.house != house) continue;
        if (!is_cat(factory_types, b.type)) continue;
        if (b.primary) return i;                 // flagged primary wins outright
        if (fallback < 0) fallback = i;          // else the first-built eligible one
    }
    return fallback;
}

void set_primary(std::vector<Building>& buildings, const std::vector<int>& factory_types,
                 int b_idx) {
    if (b_idx < 0 || b_idx >= (int)buildings.size()) return;
    if (!is_cat(factory_types, buildings[b_idx].type)) return;
    const int house = buildings[b_idx].house;
    for (int i = 0; i < (int)buildings.size(); ++i) {
        Building& b = buildings[i];
        if (b.house != house || !is_cat(factory_types, b.type)) continue;
        b.primary = (i == b_idx);
    }
}

}  // namespace ra2engine
