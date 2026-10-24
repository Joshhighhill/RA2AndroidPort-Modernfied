#pragma once
#include <cmath>
#include <cstdint>
#include <cstddef>
namespace ra2engine {
inline int hva_animation_frame(std::uint64_t stage, std::size_t frames) {
    return frames ? static_cast<int>(stage % frames) : 0;
}
// Presentation only: never change combat facing or consume simulation RNG.
inline double spinning_turret_facing(double facing, std::uint64_t tick, double rate, bool spins) {
    if (!spins || rate <= 0 || !std::isfinite(rate)) return facing;
    const double result=std::fmod(static_cast<double>(tick)*rate/15.0,65536.0);
    return result < 0 ? result+65536.0 : result;
}
}
