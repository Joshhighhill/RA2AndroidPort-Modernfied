#include "ra2assets/rules.hpp"
#include <cctype>
#include <cstdlib>
#include <vector>

// Thin wrapper over Ini -- see rules.hpp for the field/lifetime contract.
// This file intentionally has no parsing logic of its own beyond a couple of
// small local helpers (bool-ish "yes"/"true" values, comma-split) that Ini
// doesn't provide: Ini already handles case-insensitivity, defaults, and
// duplicate-key semantics for everything else.
namespace ra2assets {

namespace {

// Case-insensitive "yes"/"true" check, mirroring the game's own tolerant
// boolean parsing (Turret=yes, Turret=Yes, Turret=TRUE all count).
bool is_truthy(const std::string& v) {
    std::string lower = v;
    for (char& c : lower) c = (char)std::tolower((unsigned char)c);
    return lower == "yes" || lower == "true";
}

// Comma-split `v`, trimming whitespace and uppercasing each token; empty
// tokens (from "" or trailing commas) are skipped. Used by prerequisite()/
// prerequisite_group() to normalize section-id lists.
std::vector<std::string> split_trim_upper(const std::string& v) {
    std::vector<std::string> out;
    size_t pos = 0;
    while (pos <= v.size()) {
        size_t comma = v.find(',', pos);
        std::string tok = v.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
        size_t b = tok.find_first_not_of(" \t");
        size_t e = tok.find_last_not_of(" \t");
        if (b != std::string::npos) {
            std::string trimmed = tok.substr(b, e - b + 1);
            for (char& c : trimmed) c = (char)std::toupper((unsigned char)c);
            out.push_back(trimmed);
        }
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    return out;
}

} // namespace

Rules Rules::load(const Ini& ini) {
    return Rules(ini);
}

std::string Rules::image(const std::string& unit_id) const {
    return ini_->get(unit_id, "Image", unit_id);
}

int Rules::speed(const std::string& unit_id) const {
    return ini_->get_int(unit_id, "Speed", 0);
}

int Rules::strength(const std::string& unit_id) const {
    return ini_->get_int(unit_id, "Strength", 0);
}

std::string Rules::armor(const std::string& unit_id) const {
    std::string a = ini_->get(unit_id, "Armor", "");
    return a.empty() ? "none" : a;
}

std::string Rules::primary(const std::string& unit_id) const {
    return ini_->get(unit_id, "Primary", "");
}

std::string Rules::voice_select(const std::string& unit_id) const {
    return ini_->get(unit_id, "VoiceSelect", "");
}

std::string Rules::voice_move(const std::string& unit_id) const {
    return ini_->get(unit_id, "VoiceMove", "");
}

std::string Rules::voice_attack(const std::string& unit_id) const {
    return ini_->get(unit_id, "VoiceAttack", "");
}

std::string Rules::die_sound(const std::string& unit_id) const {
    return ini_->get(unit_id, "DieSound", "");
}

int Rules::weapon_damage(const std::string& w) const {
    return ini_->get_int(w, "Damage", 0);
}

int Rules::weapon_rof(const std::string& w) const {
    return ini_->get_int(w, "ROF", 0);
}

double Rules::weapon_range(const std::string& w) const {
    // Range= can be fractional (e.g. 120mm's Range=5.75) -- Ini::get_int
    // would truncate it, so parse the raw string with strtod instead.
    std::string v = ini_->get(w, "Range", "");
    if (v.empty()) return 0.0;
    return std::strtod(v.c_str(), nullptr);
}

std::string Rules::weapon_warhead(const std::string& w) const {
    return ini_->get(w, "Warhead", "");
}

std::string Rules::weapon_report(const std::string& w) const {
    return ini_->get(w, "Report", "");
}

std::vector<int> Rules::verses(const std::string& warhead) const {
    // Verses=100%,80%,... -> percent ints, one per armor class, in ini order.
    // Tolerates spaces and a missing '%' (the game's parser reads the number).
    std::vector<int> out;
    std::string s = ini_->get(warhead, "Verses", "");
    if (s.empty()) return out;
    size_t pos = 0;
    while (pos <= s.size()) {
        size_t comma = s.find(',', pos);
        std::string tok = s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
        size_t b = tok.find_first_not_of(" \t");
        size_t e = tok.find_last_not_of(" \t%");
        out.push_back(b == std::string::npos ? 0 : std::atoi(tok.substr(b, e - b + 1).c_str()));
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    return out;
}

int Rules::rot(const std::string& unit_id) const {
    return ini_->get_int(unit_id, "ROT", 0);
}

bool Rules::has_turret(const std::string& unit_id) const {
    return is_truthy(ini_->get(unit_id, "Turret", ""));
}

std::string Rules::explosion_first(const std::string& unit_id) const {
    // Explosion=TWLT070,S_BANG48,... -- only the first entry (the actual
    // death VoxelAnim/animation) is needed today.
    std::string v = ini_->get(unit_id, "Explosion", "");
    if (v.empty()) return "";
    size_t comma = v.find(',');
    std::string first = (comma == std::string::npos) ? v : v.substr(0, comma);
    size_t b = first.find_first_not_of(" \t");
    size_t e = first.find_last_not_of(" \t");
    return b == std::string::npos ? "" : first.substr(b, e - b + 1);
}

int Rules::cost(const std::string& id) const {
    return ini_->get_int(id, "Cost", 0);
}

int Rules::power(const std::string& id) const {
    return ini_->get_int(id, "Power", 0);
}

int Rules::adjacent(const std::string& id) const {
    return ini_->get_int(id, "Adjacent", 0);
}

int Rules::tech_level(const std::string& id) const {
    return ini_->get_int(id, "TechLevel", 0);
}

bool Rules::is_construction_yard(const std::string& id) const {
    return is_truthy(ini_->get(id, "ConstructionYard", ""));
}

std::vector<std::string> Rules::prerequisite(const std::string& id) const {
    return split_trim_upper(ini_->get(id, "Prerequisite", ""));
}

std::vector<std::string> Rules::prerequisite_group(const std::string& group) const {
    return split_trim_upper(ini_->get("General", "Prerequisite" + group, ""));
}

double Rules::general_double(const std::string& key, double fallback) const {
    // [General] first (BuildSpeed/RefundPercent/LowPowerPenaltyModifier's
    // real home); [AudioVisual] second (ConditionYellow/ConditionRed's real
    // home -- see the header comment).
    std::string v = ini_->get("General", key, "");
    if (v.empty()) v = ini_->get("AudioVisual", key, "");
    if (v.empty()) return fallback;
    return std::strtod(v.c_str(), nullptr);
}

int Rules::mp_money() const {
    return ini_->get_int("MultiplayerDialogSettings", "Money", 10000);
}

} // namespace ra2assets
