#include "ra2assets/rules.hpp"
#include <cstdlib>
#include <vector>

// Thin wrapper over Ini -- see rules.hpp for the field/lifetime contract.
// This file intentionally has no parsing logic of its own: Ini already
// handles case-insensitivity, defaults, and duplicate-key semantics.
namespace ra2assets {

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

int Rules::weapon_damage(const std::string& w) const {
    return ini_->get_int(w, "Damage", 0);
}

int Rules::weapon_rof(const std::string& w) const {
    return ini_->get_int(w, "ROF", 0);
}

int Rules::weapon_range(const std::string& w) const {
    return ini_->get_int(w, "Range", 0);
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

} // namespace ra2assets
