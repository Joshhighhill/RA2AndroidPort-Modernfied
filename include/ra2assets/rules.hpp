#pragma once
#include "ra2assets/ini.hpp"
#include <string>
#include <vector>

namespace ra2assets {

// Thin, minimal lookup for the unit/weapon/warhead fields the engine needs
// from rulesmd.ini: art (image), movement (speed), and combat (strength/armor/
// primary + weapon damage/rof/range/warhead/report + warhead verses). Still
// deliberately NOT a general rules.ini model -- fields land only when the sim
// needs them (YAGNI).
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

    // [unit_id] Strength=, or 0 if the section/key is absent.
    int strength(const std::string& unit_id) const;

    // [unit_id] Armor=, or "none" if absent.
    std::string armor(const std::string& unit_id) const;

    // [unit_id] Primary=, or "" if absent.
    std::string primary(const std::string& unit_id) const;

    // [weapon] Damage=, or 0 if absent.
    int weapon_damage(const std::string& weapon) const;

    // [weapon] ROF=, or 0 if absent.
    int weapon_rof(const std::string& weapon) const;

    // [weapon] Range=, or 0 if absent.
    int weapon_range(const std::string& weapon) const;

    // [weapon] Warhead=, or "" if absent.
    std::string weapon_warhead(const std::string& weapon) const;

    // [weapon] Report=, or "" if absent.
    std::string weapon_report(const std::string& weapon) const;

    // [warhead] Verses= parsed into percent integers; empty vector if absent.
    // Tolerates spaces and missing '%' signs in the comma-separated list.
    std::vector<int> verses(const std::string& warhead) const;

private:
    explicit Rules(const Ini& ini) : ini_(&ini) {}
    const Ini* ini_;
};

} // namespace ra2assets
