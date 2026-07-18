#pragma once
#include <string>
#include <vector>

namespace ra2engine {

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
};

}  // namespace ra2engine
