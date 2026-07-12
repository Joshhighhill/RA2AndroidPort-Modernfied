#include "ai_base.hpp"
#include <algorithm>
#include <cmath>

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

}  // namespace ra2engine
