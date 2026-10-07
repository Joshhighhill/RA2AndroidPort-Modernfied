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

// CB-4 Superweapons [RE: superweapon.md §2/§5]: the CB-2 area-damage applicator,
// PROMOTED from combat.cpp's anonymous namespace to ra2engine so World::
// fire_superweapon can REUSE it for the nuke detonation (a transient virtual
// detonator Unit passed as BOTH attacker and center). LINKAGE-ONLY change: the
// function body + its existing caller (try_fire) are byte-for-byte unchanged, so
// every existing combat path computes identical results and all 11 gate outputs
// stay bit-identical (re-verified below). This is the one deviation from the
// spec's "combat.cpp literally untouched" wording -- necessitated because the
// applicator has internal linkage and is otherwise uncallable from world.cpp;
// reimplementing the falloff loop (a "new damage system") is what §5/§8 forbid.
void apply_spread_damage(std::vector<Unit>& units, Occupancy& occ,
                         const Unit& attacker, const Unit& center);

// Projectile phase (P2) [RE: projectile.md §10]: one shot whose projectile
// flies. Emitted by step_combat ONLY when the caller passes a non-null
// `launches` sink (World's projectiles_enabled_ firewall); World turns each
// into a Projectile in launch (ascending attacker) order. Exactly one of
// target_unit / target_building is >= 0.
struct ProjectileLaunch {
    int attacker = -1;
    int target_unit = -1;
    int target_building = -1;
};

// P2: the single-target damage tail (damage_vs, idle retaliation, death) --
// the pre-P2 try_fire else-branch extracted verbatim, shared by the instant
// path and World's projectile detonation.
void deliver_unit_hit(std::vector<Unit>& units, Occupancy& occ, int attacker_idx, int victim_idx);
// Gap fix C: the same tail with an explicit shooter -- a unit (attacker_idx) or
// a base defense (attacker_building, `shooter` = its transient weapon carrier).
void deliver_hit_from(std::vector<Unit>& units, Occupancy& occ, const Unit& shooter,
                      int attacker_idx, int attacker_building, int victim_idx);

// P2: the attack-building damage tail (damage_vs vs armor, kill_building, clear
// attackers) -- extracted from step_combat's building branch.
void deliver_building_hit(std::vector<Unit>& units, std::vector<Building>& buildings,
                          int attacker_idx, int b_idx);

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
//
// Buildings & Construction Task 4: `buildings` adds an explicit-order-only
// attack-building path (Unit::target_building) alongside the pre-existing
// unit-targeting machinery above -- units never auto-acquire buildings, and
// buildings never retaliate (no weapons in this roster), so the unit-vs-unit
// code path is BIT-IDENTICAL to the pre-Task-4 3-arg step_combat whenever
// `buildings` is empty (the byte-identity demo gates depend on this). For a
// unit with target_building >= 0: the target is first validated (alive &&
// state != Dead, else cleared and normal targeting resumes); range is the
// distance to the NEAREST foundation cell (a 4x4 building is in range at its
// edge, not its center); in range, it fires on the same rof_cool cadence as
// unit combat (damage_vs against the building's armor_ix, hp -= dmg, hp <= 0
// -> kill_building, which also clears every OTHER unit's target_building
// pointing at it); out of range with attack_order set, it emits a
// MoveRequest at the nearest foundation cell (approach = true, same
// firing-position-resolution contract as the unit-pursuit case).
// Multi-house/Alliances [RE: multihouse.md §3/§4]: `world` is consulted ONLY
// for the hostility predicate (World::is_enemy) used by target validation /
// auto-acquire; nothing in `world` is mutated. With houses_configured_ FALSE
// (every existing gate + host test), is_enemy(a,b) == (a != b) -> the targeting
// scan is byte-identical to the pre-alliances inlined `v.house != self_house`.
// Projectile phase (P2): `launches` (default nullptr) is the firewall. nullptr
// -> every shot hits instantly exactly as before (byte-identical); non-null ->
// a shot whose Unit::w_proj flies() appends a ProjectileLaunch instead of
// dealing damage (ROF/fire_flash reset unchanged).
std::vector<MoveRequest> step_combat(const World& world, std::vector<Unit>& units,
                                     std::vector<Building>& buildings, Occupancy& occ, double dt,
                                     std::vector<ProjectileLaunch>* launches = nullptr,
                                     ProjRng* rof_rng = nullptr);
// W5-B R-060 [RE: Modify_Damage FUN_00489090, disassembled]: one object's
// share of an explosion. damage<0 (healing) applies only within 8 leptons.
// Otherwise the falloff is LINEAR in float: atmax = damage*PercentAtMax,
// spread = ftol(CellSpread*256.0f), and when atmax != damage and spread != 0
// dmg = ftol((damage-atmax)*(spread-distance)/spread + atmax) (no clamp of the
// distance: past the edge it keeps falling), then max(0,.) and
// ftol(dmg * Verses[armor]), clamped to [CombatDamage] MaxDamage (10000).
// No minimum of 1 here (ObjectClass::ReceiveDamage FUN_005f4ba0 floors only
// buildings at 1 after this call).
int modify_damage(int damage, const std::vector<int>& verses, int armor_ix, double cell_spread,
                  double percent_at_max, int distance);

// W5-B R-060 [RE: DamageArea FUN_00489190 + the ring tables DAT_007EC3D0 /
// DAT_00ABC408 (369 offsets built by the static initializer @0x561540)]:
// cells scanned for an explosion of CellSpread `s` = the first
// spread_ring_count(ftol(s)) entries of spread_ring_offset().
int spread_ring_count(int r);
void spread_ring_offset(int k, int& dx, int& dy);

// W5-B R-060 [RE: DamageArea FUN_00489190]: the one damage applicator every
// hit goes through, at impact point (ix, iy, iz) in clone leptons (cell
// centre = cell*256; z = level*104 + altitude). Objects in the ring cells of
// ftol(CellSpread), within ftol(CellSpread*256) leptons (3-D distance; an
// airborne aircraft only when the impact is above the ground, its distance
// halved), ALL houses -- own and allied units and buildings take splash
// unless the warhead is AffectsAllies=no (ReceiveDamage 0x700DA0: the
// shooter's house allied with the victim -> 0). The shooter itself is
// excluded. A building is hit once per foundation cell in the ring set:
// distance 0 in the impact cell, else from that cell's centre. `shooter`
// carries the weapon (w_damage/w_verses/w_cell_spread/...); `src_unit` /
// `src_building` name the firer (-1 none) for exclusion, retaliation and
// kill credit.
struct AreaResult {
    int damage = 0;              // total HP removed (units + buildings)
    int first_unit = -1;         // lowest-index unit damaged (event victim)
};
AreaResult damage_area(const World& world, std::vector<Unit>& units,
                       std::vector<Building>& buildings, Occupancy& occ, const Unit& shooter,
                       int src_unit, int src_building, int ix, int iy, int iz);

// Lepton position of a unit for damage/aim: cell-centred fx/fy plus the
// infantry sub-cell offset, z = level*104 + altitude when airborne.
int unit_lept_x(const Unit& u);
int unit_lept_y(const Unit& u);
int unit_lept_z(const Unit& u);
// Building aim point: the foundation centre (BuildingClass target coords).
void building_aim_lept(const Building& b, int level, int& x, int& y, int& z);

// Gap fix C (G-50): RearmTimer in seconds = (ROF + Random(0,2)) / 15
// [veterancy.md V7]; rng null -> no jitter.
double rearm_secs(int rof_frames, ProjRng* rng);

}  // namespace ra2engine
