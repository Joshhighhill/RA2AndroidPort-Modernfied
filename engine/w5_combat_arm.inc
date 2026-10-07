// W5-B (wave-5 agent B, combat and movement core): arms the real game's World
// with the RE'd movement/combat rules. Called from the GameScene ctor / load
// path and the save-load gate World (never a unit-test World, which keeps the
// pre-W5 one-unit-per-cell model unless a test opts in).
//   R-061 infantry sub-cells (three infantry per cell, FUN_00481090).
void w5_arm_combat(ra2engine::World& world, const Rules& rules) {
    (void)rules;
    world.enable_subcells();
}
