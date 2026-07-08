#include "ra2assets/rules.hpp"

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

} // namespace ra2assets
