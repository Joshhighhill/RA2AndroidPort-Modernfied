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
    bool sonic = false;   // W6-04-09: the PRIMARY warhead's Sonic=
    // W6-04-15 [rules key, abilities-w5.md §6]: InfantryType
    // OpenTransportWeapon= -- the slot a passenger fires out of an OpenTopped
    // transport (E1/GGI 1, GHOST/TANY/BORIS 0); -1 = the normal choice.
    int open_transport_weapon = -1;
    // W6-04-16 [RE-FALLBACK: TechnoType AirRangeBonus added to the weapon
    // range against an airborne target]: [FV] AirRangeBonus=4 (cells).
    double air_range_bonus = 0.0;
    // W6-04-29: DeployedCrushable=no (the deployed Guardian GI).
    bool deployed_crushable = true;
    // W6-04-30: CanApproachTarget=no (the Mirage Tank holds its disguise).
    bool can_approach_target = true;
    // W6-04-14 [RE-FALLBACK, rules semantics; MovementZone=CrusherAll]:
    // OmniCrusher= (the Battle Fortress) crushes vehicles too;
    // OmniCrushResistant= (BFRT, MCVs, SMIN) cannot be omni-crushed.
    bool omni_crusher = false;
    bool omni_crush_resistant = false;
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
    // W6-04-09 [RE: FootClass::ReceiveDamage FUN_004D7250]: the ACTIVE
    // weapon's warhead Sonic= (+0x14B, the Dolphin), and the per-frame marks
    // a hit leaves on a parasite host -- a Sonic hit or a healing hit makes
    // the parasite exit (ParasiteClass::ExitUnit FUN_00629F40).
    bool sonic = false;
    bool sonic_hit = false;
    bool heal_hit = false;
};

// Per-building state.
struct BldW6 {
    int burst_index = 0;   // W6-04-01: a defence's CurrentBurstIndex
};

}  // namespace ra2engine
