// Wave-5 unit and building abilities (agent C) -- the World-side pass.
// [RE: docs/re/abilities-w5.md]. A World member TU (the world_abilities.cpp
// precedent) so the wave's churn stays out of world.cpp. Everything here is
// behind w5_ state that only a real-play roster or a stolen-tech event fills.
#include <algorithm>
#include <cmath>

#include "world.hpp"

namespace ra2engine {

void World::grant_stolen_tech(int house, std::uint8_t bits) {
    if (house < 0 || bits == 0) return;
    if ((int)w5_.stolen_tech.size() <= house) w5_.stolen_tech.resize((std::size_t)house + 1, 0);
    w5_.stolen_tech[(std::size_t)house] |= bits;
}

// [RE: abilities-w5.md §1, HouseClass::HasAllStolenTech (YRpp naming; the
// flags are the house+0x2BC..0x2BE bytes FUN_004571E0 writes)]: every
// RequiresStolen*Tech bit of the type must be held.
void World::apply_stolen_tech_gate(std::vector<UnitTypeDef>& defs, int house) const {
    const std::uint8_t have = stolen_tech(house);
    for (UnitTypeDef& d : defs) {
        if (d.w5.requires_stolen == 0) continue;
        d.tech_level = (have & d.w5.requires_stolen) == d.w5.requires_stolen ? d.w5.base_tech_level : 0;
    }
}

void World::step_w5(double dt) {
    (void)dt;
    // AI houses: their producible rosters follow their stolen tech (a spy the
    // AI sent into an enemy Battle Lab unlocks the stolen-tech infantry for
    // its team builder).
    for (int h = 0; h < (int)ai_houses_.size(); ++h) {
        HouseAiRec& rec = ai_houses_[(std::size_t)h];
        if (!rec.is_ai || !rec.cfg.team.enabled) continue;
        apply_stolen_tech_gate(rec.cfg.team.unit_roster, h);
    }
}

}  // namespace ra2engine
