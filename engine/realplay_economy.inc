// realplay_economy.inc -- gap owner A (economy / production / base) for the
// REAL interactive game (GameScene + launch_skirmish). Included into main.cpp
// once, just before `class GameScene`, so everything here sees the asset
// loaders above it and GameScene/launch_skirmish can call it. Kept out of
// main.cpp's body so six parallel fixers do not collide.
//
// Gap rows: docs/gap/02-economy-production.md, GAP-ANALYSIS.md G-20..G-31.

// G-20 [RE: economy.md §1-§4, naval.md §1]: everything launch_skirmish must
// open so the real game's World has a working economy: the full set_economy
// (types_ non-empty -> ore growth/spread + harvester FSM step), the rules
// [General] repair/sell scalars, and the water passability grid (Naval Yard
// placement + ship movement, G-24).
void real_play_arm_economy(ra2engine::World& world, const BuildingAssets& ba, const Rules& rules,
                           Assets& a) {
    wire_economy(world, ba, rules);
    world.set_water_terrain(resolve_land_types(a.map, a.theater, world.grid_w(), world.grid_h()));
}
