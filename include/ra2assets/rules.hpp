#pragma once
#include "ra2assets/ini.hpp"
#include <string>

namespace ra2assets {

// Thin, minimal lookup for the handful of unit fields the engine vertical
// slice needs from rulesmd.ini. Deliberately does NOT model rules.ini
// generally (armor types, weapons, warheads, ...) -- only image/speed exist
// today; more fields land when the sim actually needs them (YAGNI).
//
// Rules wraps an already-parsed Ini by reference (not by copy) to stay
// "thin" -- the caller must keep the Ini alive for at least as long as the
// Rules object built from it.
class Rules {
public:
    static Rules load(const Ini& ini);

    // Art image (SHP base name) for a unit id, e.g. "E1" -> "GI" (then
    // GI.SHP). Reads [unit_id] Image=; falls back to unit_id itself if the
    // section/key is absent (this also covers unknown unit ids).
    std::string image(const std::string& unit_id) const;

    // [unit_id] Speed=, or 0 if the section/key is absent (unknown unit ids
    // included).
    int speed(const std::string& unit_id) const;

private:
    explicit Rules(const Ini& ini) : ini_(&ini) {}
    const Ini* ini_;
};

} // namespace ra2assets
