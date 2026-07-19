#pragma once
#include <string>
#include <vector>

namespace ra2engine {

// CB-3 Naval [RE: naval.md §1]: the movement domain a unit's A* reads. Land
// (the DEFAULT -- every existing unit) resolves to the UNCHANGED passable_
// grid; Water resolves to the new water_passable_ grid. Land = 0 so a
// value-initialized/legacy Unit is Land (inert -- spec §8). Amphibious is
// reserved for a later phase (land<->water transition is out of scope).
//
// Defined HERE (not world.hpp) because UnitCombat below carries a
// MovementDomain field, and unit_combat.hpp is included by BOTH world.hpp and
// unit_type.hpp BEFORE world.hpp's own body -- so the enum must be visible at
// this point. world.hpp gets it transitively via its unit_combat.hpp include.
enum class MovementDomain { Land = 0, Water = 1 };

// Per-unit combat stats resolved once by the caller (main.cpp, from Rules)
// and handed to World::add_combat_unit to fill in a spawned Unit's hp/armor/
// weapon fields. Mirrors the Rules-derived values Task 2 exposes (strength,
// armor, the primary weapon's damage/rof/range, and its warhead's verses)
// plus the SHP image name add_unit already takes directly.
struct UnitCombat {
    int strength = 1;
    std::string armor;
    std::string image;
    int damage = 0;
    int rof = 0;
    double range = 0.0;
    std::vector<int> verses;
    // --- CB-2 air-to-ground splash [RE-GATE APPLIED: aircraft.md §4 ->
    // combat.md §7] --- the primary weapon's warhead CellSpread / PercentAtMax
    // (BlimpBomb -> BlimpHE CellSpread=2 PercentAtMax=.5). Default 0 = no
    // spread (single-target). Fed to combat.cpp's apply_spread_damage, GATED on
    // cell_spread >= 1 so every existing sub-1-cell warhead keeps its EXACT
    // single-target path (the 9 gates stay byte-identical).
    double cell_spread = 0.0;      // warhead CellSpread (cells; radius = round(.))
    double percent_at_max = 1.0;   // warhead PercentAtMax (edge damage scale)
    // --- Voxel Vehicles phase (docs/re/voxel-render.md §5) ---
    int rot = 0;               // Rules ROT= (turn rate); 0 = no turning
    bool turret = false;       // has an independently-slewing turret
    double speed_cps = 0.0;    // per-unit speed in cells/sec; 0 = World move_cps
    bool is_vehicle = false;   // gates all vehicle-specific locomotion

    // --- CB-1 Transports/Cargo [RE: transport-cargo.md §1] ---
    // Resolved once from Rules by main.cpp like every existing stat; ALL
    // defaults inert (0 passengers = not a transport). Copied onto the Unit
    // at add_combat_unit.
    int passengers = 0;        // Rules Passengers= (capacity COUNT [RE-GATE
                               // APPLIED: transport-cargo.md §1])
    int size_limit = 0;        // Rules SizeLimit= (largest admissible Size --
                               // a per-passenger cap)
    int size = 1;              // Rules Size= (own bulk; pip weight only)
                               // [RE-GATE APPLIED: transport-cargo.md §1 --
                               // absent-key default 1 CONFIRMED]

    // --- CB-2 Aircraft/Flight [RE: aircraft.md] ---
    // Resolved once from Rules by main.cpp like every existing stat; ALL
    // defaults inert (is_aircraft=false = a ground unit). Copied onto the Unit
    // at add_combat_unit. Rates are per-SECOND (multiplied by dt in
    // step_flight -- the rot_u16_per_sec precedent), converted from the
    // recovered per-frame Jumpjet values at resolution time [RE-GATE APPLIED:
    // aircraft.md §1/§2/§5].
    bool is_aircraft = false;      // flight-locomotor type (Jumpjet CLSID)
    double cruise_altitude = 0.0;  // JumpjetHeight (leptons)
    double climb_rate = 0.0;       // JumpjetClimb (leptons/sec)
    double descend_rate = 0.0;     // JumpjetCrash (leptons/sec)
    double flight_speed = 0.0;     // cruise speed (cells/sec) from JumpjetSpeed
                                   // (fallback base Speed) [D2, aircraft.md §1]
    double flight_turn = 0.0;      // heading turn rate (u16/sec) from ROT x2 --
                                   // NOT JumpJetTurnRate [D1, aircraft.md §1]
    bool balloon_hover = false;    // BalloonHover: takes off once, never lands
    bool omni_fire = false;        // weapon OmniFire: fire without turning

    // --- CB-3 Naval/Water [RE: naval.md §1] ---
    // Resolved once from Rules by main.cpp like every existing stat; ALL
    // defaults inert (is_naval=false = a ground unit). Copied onto the Unit at
    // add_combat_unit. A ship reuses speed_cps/rot/damage/rof/range/verses (it
    // is a voxel vehicle, is_vehicle=true); naval adds only the domain flags.
    bool is_naval = false;                                // naval-locomotor type (CLSID)
    MovementDomain movement_domain = MovementDomain::Land;  // Water for a naval unit

    // --- Engineer/Spy Capture phase [RE-GATE APPLIED: capture.md §B5] ---
    // Veteran rank: 0 = none (byte-identical), 1 = veteran. rank>0 scales
    // firepower + HP at spawn (add_combat_unit) via veteran_firepower/veteran_hp;
    // combat.cpp is UNTOUCHED (a veteran just spawns with higher w_damage + hp).
    int veteran_rank = 0;

    // --- Shroud/Fog phase [RE: shroud.md §2] ---
    // Reveal radius in cells, from rules.sight(id) (hard-clamped to 10). Copied
    // onto Unit at add_combat_unit. Inert default 0 (a source with sight 0
    // reveals only its own cell / nothing, and is meaningless while the
    // visibility grids are empty). NOT read by world_hash.
    int sight = 0;
};

}  // namespace ra2engine
