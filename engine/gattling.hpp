#pragma once
#include <algorithm>
#include <vector>
#include "weapon.hpp"

namespace ra2engine {
// Resolved original numbered ground/air weapon pairs and deterministic charge.
struct Gattling {
    std::vector<WeaponSlot> weapons, elite_weapons;
    std::vector<int> thresholds, elite_thresholds;
    int rate_up = 1, rate_down = 50;
    int charge = 0;
    bool enabled() const { return weapons.size() >= 2 && !thresholds.empty(); }
    int stage(bool elite, int value = -1) const {
        const auto& limits = elite && !elite_thresholds.empty() ? elite_thresholds : thresholds;
        int s = 0;
        while (s + 1 < static_cast<int>(limits.size()) && (value < 0 ? charge : value) >= limits[s]) ++s;
        return std::min(s, std::max(0, static_cast<int>(weapons.size()/2)-1));
    }
    void step(bool firing, bool elite) {
        if (!enabled()) return;
        const auto& limits = elite && !elite_thresholds.empty() ? elite_thresholds : thresholds;
        const int max_charge = std::max(0, limits.back());
        charge = std::clamp(charge, 0, max_charge);
        if (firing) charge = std::min(max_charge, charge + std::min(max_charge, std::max(0, rate_up)));
        else charge = rate_down <= 0 ? 0 : std::max(0, charge - rate_down);
    }
    const WeaponSlot& weapon(int slot, bool elite, int value = -1) const {
        const int i = 2*stage(elite,value) + (slot == 1 ? 1 : 0);
        if (elite && i < static_cast<int>(elite_weapons.size()) && elite_weapons[i].valid)
            return elite_weapons[i];
        return weapons[i];
    }
};
}
