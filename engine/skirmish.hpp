#pragma once
#include <string>
#include <vector>
#include "world.hpp"     // ra2engine::World (World::Side)
#include "ai_base.hpp"   // ra2engine::AiDifficulty
namespace ra2assets { class Rules; }

namespace ra2engine {

// One lobby slot [RE: skirmish.md §3, D1]. side/color_ix/country/alliance_team/
// combatant map 1:1 into HouseRec; is_ai + diff drive enable_ai_house. NOT hashed.
struct SkirmishSlot {
    World::Side  side          = World::Side::Allied;
    int          color_ix      = 0;      // 0..7 into the 8 MP colors
    std::string  country       = "";
    int          alliance_team = -1;     // -1 sentinel -> own id (FFA)
    bool         combatant     = true;   // false for Neutral/Special
    bool         is_ai         = false;
    AiDifficulty diff          = AiDifficulty::Normal;  // meaningful only when is_ai
};

// The single input to the launcher [RE: skirmish.md §1, D1]. Defaults are the
// shipped [MultiplayerDialogSettings] values. GameSpeed is PRESENTATION-only and
// is NOT stored here (it cannot affect the deterministic dt=1/15 step / world_hash).
struct SkirmishConfig {
    std::string               map_id        = "mapsmd03";  // only one map; browser = carry-forward
    std::vector<SkirmishSlot> slots;                       // index 0 = the human player
    double                    starting_cash = 10000.0;     // slider 5000-10000
    int                       unit_count    = 10;          // 0-10 (MCV extra)
    int                       tech_level    = 10;          // 1-10
    bool                      shroud        = true;
    bool                      crates        = false;
    bool                      superweapons  = true;
    bool                      short_game    = true;
};

// "Quick start" config reproducing the current run_interactive 1v1 [D1]:
// slot0 Allied human, slot1 Soviet AI Normal, slot2 Neutral non-combatant;
// starting_cash = rules.mp_money(). (T1 item 4 may add a Special slot; the
// default here stays the minimal faithful 1v1 that build_world's 3-house world
// already reproduces.)
SkirmishConfig default_skirmish_config(const ra2assets::Rules& rules);

// PURE asset-free core [D2]: apply the config's house table + toggles + per-house
// credit seed to `world`. For each slot -> set_house(i, HouseRec{...}); then
// enable_shroud(cfg.shroud) / enable_ally_reveal(true) / enable_victory(true,
// cfg.short_game); seed grant_credits_for_test(i, cfg.starting_cash) for each
// combatant; enable_ai_house(i, AiHouseConfig{.difficulty=slot.diff}) for each AI
// slot (a MINIMAL config -> ai_enabled(i)==true; launch_skirmish OVER-WRITES it
// with the full roster-backed make_ai_config before the sim runs). Never spawns
// units/bases (that needs assets -> launch_skirmish). Host-testable on a hand-
// built World; NOT read by world_hash.
void configure_skirmish_houses(World& world, const SkirmishConfig& cfg);

}  // namespace ra2engine
