#pragma once
// Wave-6 #4 (units and weapons) per-type / per-object data. Pure (no World,
// no Rules): main.cpp resolves the type half from rulesmd into UnitCombat::w6,
// World::add_combat_unit copies it onto Unit::w6.t, and the runtime half lives
// next to it. Every default is inert (the pre-wave-6 behaviour).

namespace ra2engine {

// Type data resolved once per unit type (docs/gap/wave6/04-units.md).
struct UnitTypeW6 {
    // W6-04-01 [RE: WeaponTypeClass::LoadFromINI Burst -> +0x9C]: the PRIMARY
    // weapon's Burst= (the elite and secondary slots carry their own).
    int burst = 1;
    // W6-04-06 [RE: WeaponType OmniFire +0x12B, aircraft.md §4]: the PRIMARY
    // weapon's OmniFire= (fires without turning to face).
    bool omni_fire = false;
};

// Per-unit state.
struct UnitW6 {
    UnitTypeW6 t;
    // W6-04-01 [RE: TechnoClass::Fire FUN_006FD210 @0x6FD8xx, GetROF
    // FUN_006FC460]: the ACTIVE weapon's Burst= (swapped with w_rof) and the
    // techno's CurrentBurstIndex (+0x3B8).
    int burst = 1;
    int burst_index = 0;
    // W6-04-06: the ACTIVE weapon's OmniFire= (swapped with w_rof; the
    // type-level Unit::omni_fire of fliers stays as it was).
    bool omni_fire = false;
};

// Per-building state.
struct BldW6 {
    int burst_index = 0;   // W6-04-01: a defence's CurrentBurstIndex
};

}  // namespace ra2engine
