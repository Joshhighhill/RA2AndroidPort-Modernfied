#include "skirmish.hpp"
#include "ra2assets/rules.hpp"

namespace ra2engine {

SkirmishConfig default_skirmish_config(const ra2assets::Rules& rules) {
    SkirmishConfig cfg;
    cfg.starting_cash = (double)rules.mp_money();   // shipped 10000
    cfg.slots = {
        SkirmishSlot{World::Side::Allied,  0, "Americans", 0, /*comb*/true,  /*ai*/false, AiDifficulty::Normal},
        SkirmishSlot{World::Side::Soviet,  1, "Russians",  1, /*comb*/true,  /*ai*/true,  AiDifficulty::Normal},
        SkirmishSlot{World::Side::Neutral, 2, "Civilian",  2, /*comb*/false, /*ai*/false, AiDifficulty::Normal},
    };
    return cfg;
}

void configure_skirmish_houses(World& world, const SkirmishConfig& cfg) {
    for (int i = 0; i < (int)cfg.slots.size(); ++i) {
        const SkirmishSlot& s = cfg.slots[i];
        world.set_house(i, World::HouseRec{s.side, s.color_ix, s.country, s.alliance_team, s.combatant});
    }
    world.enable_shroud(cfg.shroud);
    world.enable_ally_reveal(true);                 // rules AllyReveal=yes
    world.enable_victory(true, cfg.short_game);
    for (int i = 0; i < (int)cfg.slots.size(); ++i) {
        if (!cfg.slots[i].combatant) continue;
        world.grant_credits_for_test(i, cfg.starting_cash);   // deterministic per-house credit seed
        if (cfg.slots[i].is_ai) {
            AiHouseConfig ai;                       // minimal: difficulty only (roster filled by launch_skirmish)
            ai.difficulty = cfg.slots[i].diff;
            world.enable_ai_house(i, ai);
        }
    }
}

}  // namespace ra2engine
