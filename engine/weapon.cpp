#include "weapon.hpp"

namespace ra2engine {

bool verses_allows(const std::vector<int>& verses, int armor_ix) {
    if (verses.empty()) return true;
    int idx = armor_ix;
    if (idx < 0 || idx >= static_cast<int>(verses.size())) idx = 0;
    return verses[idx] != 0;
}

bool slot_can_target(const WeaponSlot& w, bool air, int armor_ix) {
    if (!w.valid) return false;
    if (air ? !w.aa : !w.ag) return false;
    return verses_allows(w.verses, armor_ix);
}

int choose_weapon(const WeaponSlot& primary, const WeaponSlot& secondary, bool air,
                  int armor_ix) {
    if (slot_can_target(primary, air, armor_ix)) return 0;
    if (slot_can_target(secondary, air, armor_ix)) return 1;
    return -1;
}

}  // namespace ra2engine
