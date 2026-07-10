#pragma once
// Pure combat core: armor lookups, warhead damage vs armor, and per-frame
// engagement (targeting, rate-of-fire cadence, damage, deaths, pursuit
// requests). Formulas confirmed in docs/re/combat.md (Combat Phase Task 1);
// see that file's numbered sections for the RE evidence behind each rule
// referenced in engine/combat.cpp's comments.
//
// Pure: no SDL/OS deps (same pattern as pathfind.hpp/occupancy.hpp), so it is
// unit-testable and compiled into both ra2assets_tests and ra2engine from a
// single source of truth (see CMakeLists.txt / cmake/ra2_sources.cmake).
#include <string>
#include <vector>

#include "world.hpp"  // Unit; pulls in Occupancy (occupancy.hpp) transitively

namespace ra2engine {

// Maps an [ArmorType] ini class name to its Verses[] index. docs/re/combat.md
// §1/§4 (High): TechnoTypeClass::Armor is used directly, UNSCALED, as the
// index into WarheadTypeClass::Verses[0..10] -- none..special_2 = 0..10, no
// remapping table, no off-by-one. Case-insensitive (ini convention);
// unrecognized names default to "none" (0) rather than failing, since a
// mistyped/absent Armor= key should behave like the softest class rather
// than crash.
int armor_index(const std::string& armor);

// One hit's damage from `damage` vs a target of armor index `ai`, given the
// firing weapon's warhead `verses` percents (Verses[0..10], ini order).
// Result is `damage * verses[ai] / 100`, truncated toward zero:
// docs/re/combat.md §3 (Medium-High confidence -- the exact multiply
// instruction wasn't isolated, but the pervasive truncate-toward-zero
// double->int convention, the disassembled `_ftol`-shaped helper, was).
// Plain integer division truncates toward zero for non-negative operands,
// which is exactly this case (damage and percent are never negative), so no
// separate std::trunc call is needed.
// `verses` empty -> 100% (defensive: a warhead with no Verses table hits
// everything at full damage rather than silently dealing 0).
// `ai` outside `verses`'s bounds clamps to armor index 0 ("none") rather
// than indexing out of range.
int damage_vs(int damage, const std::vector<int>& verses, int ai);

// A request from step_combat for World to path a unit toward a cell (pursuit
// of an explicitly-ordered target, or return to a guard anchor). step_combat
// never touches Unit::path/find_path itself -- it only asks; World (later
// tasks) is what actually paths, keeping this module pure and pathing-
// agnostic (same "produce requests, let the caller act" shape as the rest of
// the pure engine layer).
struct MoveRequest {
    int unit;
    int goal_x, goal_y;
    // true: `goal_x/goal_y` is the TARGET's own cell and the caller (World)
    // must resolve a distinct firing position near it (docs/re/
    // infantry-anim.md §7 -- the original never paths an attacker to the
    // victim's own occupied cell; it approaches until in weapon range).
    // false (default): move exactly to (goal_x, goal_y) -- guard-anchor
    // returns are always false.
    bool approach = false;
};

// Advances combat by dt seconds for every unit, ascending index (fully
// deterministic -- docs/re/combat.md §5: CONFIRMED no ROF jitter/RNG
// anywhere in vanilla combat, so this whole module is intentionally free of
// any randomness). Per living, non-dying unit: decays rof_cool/fire_flash,
// validates/acquires a target, fires on ROF cadence when in range (applying
// damage_vs and starting the victim's death/retaliation), and emits a
// MoveRequest when an out-of-range target needs pursuing (explicit
// attack_order) or the unit needs to fall back to its guard anchor
// (auto-acquired target strayed too far). Dying units (die_time >= 0) only
// advance their die timer and are skipped for everything else; at
// kDieAnimSecs they tombstone (alive=false) but are never erased from
// `units`. Returns the accumulated MoveRequests for the caller (World) to
// path.
std::vector<MoveRequest> step_combat(std::vector<Unit>& units, Occupancy& occ, double dt);

}  // namespace ra2engine
