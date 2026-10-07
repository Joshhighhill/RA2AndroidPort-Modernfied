#pragma once
// Wave-5 unit and building abilities (agent C, branch w5-abil): the plain data
// the new mechanics need, kept in ONE header so the per-type / per-object /
// per-World additions are three struct members (BuildingTypeDef::w5,
// UnitTypeDef::w5, Unit::w5, Building::w5, World::w5_) instead of dozens of
// loose fields across shared files. Every default is INERT: a World whose
// rosters never fill these (every gate) behaves exactly as before.
//
// RE notes: docs/re/abilities-w5.md.
#include <cstdint>
#include <string>
#include <vector>

namespace ra2engine {

// Spy purposes (World::InfiltratePurpose as int) beyond the legacy six.
// [RE: abilities-w5.md §1, BuildingClass::Infiltrate FUN_004571E0]
enum : int {
    kSpyStolenAllied = 6,   // BuildTech type with AIBasePlanningSide=0 -> house+0x2BE
    kSpyStolenSoviet = 7,   // AIBasePlanningSide=1 -> house+0x2BD
    kSpyStolenThird = 8,    // any other side -> house+0x2BC
};
// Stolen-tech bits (HouseClass Side0/1/2TechInfiltrated).
enum : std::uint8_t { kStolenAllied = 1, kStolenSoviet = 2, kStolenThird = 4 };

// Per building TYPE (filled by fill_building_abilities from rulesmd).
struct BldTypeW5 {
    int spy_purpose = -1;        // resolved InfiltratePurpose (int); -1 = legacy heuristic
    bool radar = false;          // Radar=yes (+0x16A4)
    bool prism = false;          // this type == [General] PrismType (ATESLA)
    bool overpowerable = false;  // Overpowerable=yes (TESLA)
    int gap_radius = 0;          // GapGenerator=yes -> GapRadiusInCells (0 = not a gap)
    int inf_self_heal = 0;       // InfantryGainSelfHeal= (CATHOSP 1)
    int unit_self_heal = 0;      // UnitsGainSelfHeal= (CAMACH 1)
    bool cloning = false;        // Cloning=yes (NACLON)
    int psychic_radius = 0;      // PsychicDetectionRadius= (NAPSIS)
    int sensors_sight = 0;       // SensorArray=yes -> SensorsSight=
};

// Per UNIT TYPE in a producible roster.
struct UnitTypeW5 {
    std::uint8_t requires_stolen = 0;   // kStolen* bits the house must hold
    int base_tech_level = 0;            // TechLevel= before the stolen-tech gate
};

// Per building OBJECT.
struct BldW5 {
    int dummy = 0;
};

// Per unit OBJECT.
struct UnitW5 {
    int dummy = 0;
};

// World-wide state.
struct WorldW5 {
    std::vector<std::uint8_t> stolen_tech;   // per house: kStolen* bits
};

}  // namespace ra2engine
