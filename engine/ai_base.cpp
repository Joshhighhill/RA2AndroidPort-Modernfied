#include "ai_base.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace ra2engine {

std::uint32_t ai_rand(std::uint32_t& s) {
    // xorshift32 [RE-FALLBACK: ai-base.md deviations — engine-local RNG for
    // any adjudicated tie-break; deterministic, never wall-clock].
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

DifficultyMods difficulty_mods(AiDifficulty d, const DifficultyGroupRaw (&groups)[3]) {
    const int i = static_cast<int>(d);
    DifficultyMods m;
    m.cost_mul = groups[i].cost;             // [RE: ai-base.md §4]
    m.build_time_mul = groups[i].build_time; // [RE: ai-base.md §4]
    m.build_delay_min = groups[i].build_delay;   // minutes [RE: ai-base.md §4]
    m.build_slowdown = groups[i].build_slowdown; // [RE: ai-base.md §4]
    m.defense_target = kAiDefenseTarget[i];  // [RE: ai-base.md §2; RE-FALLBACK index binding]
    return m;
}

namespace {
long long dist2(int ax, int ay, int bx, int by) {
    const long long dx = ax - bx, dy = ay - by;
    return dx * dx + dy * dy;
}
}  // namespace

Cellxy find_build_location(const BaseView& view, bool is_defense,
                           const std::function<bool(int, int)>& can_place) {
    // [RE-GATE APPLIED: ai-base.md §3 — the exact outward scan was NOT
    // pinned; this nearest-first Chebyshev ring from the base center,
    // row-major (dy then dx ascending) within a ring, stands as the
    // CALIBRATED RE-FALLBACK. Deterministic; foundation/overlap enforced by
    // can_place — the shared per-cell buildable/unoccupied grid check with
    // the AIBaseSpacing(1) gap. The AI BYPASSES the Adjacent= proximity
    // check (FUN_004a8dc0 early-return for non-local-human houses), so
    // can_place carries NO adjacency requirement. Defenses probe each ring's
    // cells nearest the THREAT first (perimeter-facing bias; defense/wall
    // cells derive from foundation-perimeter rings around the base) with the
    // row-major order as the stable tie-break. BaseBias is NOT used here —
    // it is a threat-TARGETING multiplier [RE: ai-base.md §2].]
    const int ox = view.center_x, oy = view.center_y;
    for (int r = 0; r <= kAiMaxScanRadius; ++r) {
        std::vector<Cellxy> ring;
        ring.reserve(r == 0 ? 1 : 8 * r);
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                ring.push_back(Cellxy{ox + dx, oy + dy});
            }
        if (is_defense) {
            std::stable_sort(ring.begin(), ring.end(), [&](const Cellxy& a, const Cellxy& b) {
                return dist2(a.x, a.y, view.threat_x, view.threat_y) <
                       dist2(b.x, b.y, view.threat_x, view.threat_y);
            });
        }
        for (const Cellxy& c : ring)
            if (can_place(c.x, c.y)) return c;
    }
    return Cellxy{-1, -1};  // boxed in / nothing within the cap -> caller holds
}

}  // namespace ra2engine
