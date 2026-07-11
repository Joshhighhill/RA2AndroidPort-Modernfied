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

    // [unit_id] VoiceSelect=, or "" if absent -- soundmd.ini section name for
    // the "unit selected" voice line (e.g. E1 -> "GISelect").
    std::string voice_select(const std::string& unit_id) const;

    // [unit_id] VoiceMove=, or "" if absent -- soundmd.ini section name for
    // the "move order acknowledged" voice line.
    std::string voice_move(const std::string& unit_id) const;

    // [unit_id] VoiceAttack=, or "" if absent -- soundmd.ini section name for
    // the "attack order acknowledged" voice line.
    std::string voice_attack(const std::string& unit_id) const;

    // [unit_id] DieSound=, or "" if absent -- soundmd.ini section name for the
    // unit's death voice line.
    std::string die_sound(const std::string& unit_id) const;

    // [weapon] Damage=, or 0 if absent.
    int weapon_damage(const std::string& weapon) const;

    // [weapon] ROF=, or 0 if absent.
    int weapon_rof(const std::string& weapon) const;

    // [weapon] Range=, or 0 if absent. A double (not int) because real
    // weapons use fractional cell ranges, e.g. 120mm's Range=5.75 -- an int
    // truncated that to 5.
    double weapon_range(const std::string& weapon) const;

    // [weapon] Warhead=, or "" if absent.
    std::string weapon_warhead(const std::string& weapon) const;

    // [weapon] Report=, or "" if absent.
    std::string weapon_report(const std::string& weapon) const;

    // [warhead] Verses= parsed into percent integers; empty vector if absent.
    // Tolerates spaces and missing '%' signs in the comma-separated list.
    std::vector<int> verses(const std::string& warhead) const;

    // [unit_id] ROT= (rotation speed, in game "steps per tick" units), or 0
    // if absent.
    int rot(const std::string& unit_id) const;

    // [unit_id] Turret=, true iff the value is "yes"/"true" (case-
    // insensitive); false if absent.
    bool has_turret(const std::string& unit_id) const;

    // First comma-separated entry of [unit_id] Explosion= (the VoxelAnim/
    // animation name used for the unit's death explosion), or "" if absent.
    std::string explosion_first(const std::string& unit_id) const;

    // [id] Cost=, or 0 if absent.
    int cost(const std::string& id) const;

    // [id] Power=, or 0 if absent (negative = drain, positive = generation).
    int power(const std::string& id) const;

    // [id] Adjacent=, or 0 if absent.
    int adjacent(const std::string& id) const;

    // [id] TechLevel=, or 0 if absent.
    int tech_level(const std::string& id) const;

    // [id] ConstructionYard=, true iff the value is "yes"/"true" (case-
    // insensitive); false if absent.
    bool is_construction_yard(const std::string& id) const;

    // [id] Prerequisite= split on commas, whitespace-trimmed and uppercased;
    // empty if absent.
    std::vector<std::string> prerequisite(const std::string& id) const;

    // [General] Prerequisite<group>= (e.g. group="Power" reads
    // PrerequisitePower=) split on commas, whitespace-trimmed and
    // uppercased; empty if absent.
    std::vector<std::string> prerequisite_group(const std::string& group) const;

    // [General] key= parsed as a double, tolerating a trailing '%' (percent
    // values are returned as given, e.g. "50%" -> 50.0). Falls back to
    // [AudioVisual] key= if the key isn't in [General] -- ConditionYellow/
    // ConditionRed live there in the real rulesmd.ini (loaded by
    // FUN_00668C20's [AudioVisual] reader, per docs/re/building-render.md
    // §4), while BuildSpeed/RefundPercent/LowPowerPenaltyModifier live in
    // [General]; this getter serves all five known keys regardless of which
    // section actually holds them. Returns `fallback` if absent from both.
    double general_double(const std::string& key, double fallback) const;

    // [MultiplayerDialogSettings] Money=, fallback 10000 if absent.
    int mp_money() const;

private:
    explicit Rules(const Ini& ini) : ini_(&ini) {}
    const Ini* ini_;
};

} // namespace ra2assets
