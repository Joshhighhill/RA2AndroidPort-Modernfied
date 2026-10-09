#pragma once
// Wave-6 fix #2 (missions): per-type / per-object targeting data and the
// usable-area (IsInPlayfield) state. All defaults are INERT: an object is in
// the playfield, a legal target and significant, and no World area is set, so
// a World that never calls World::set_playfield behaves exactly as before.
#include <string>

namespace ra2engine {

// Type data resolved once from the rules (map overrides merged in a campaign)
// by resolve_combat_unit and copied onto Unit::tg.t at add_combat_unit.
struct UnitTypeTgt {
    // W6-02-15: the rules type id (E1), not its art Image= (GI). Empty for a
    // hand-built UnitCombat (tests / old gates).
    std::string rules_id;
    bool insignificant = false;    // Insignificant=   (ObjectType +0x232)
    bool legal_target = true;      // LegalTarget=     (ObjectType +0x231)
    bool immune_psionics = false;  // ImmuneToPsionics= (W6-02-12)
    double guard_range = 0.0;     // (AirRangeBonus= lives in wave-6 #4's UnitTypeW6)      // GuardRange= cells (TechnoType +0x5B8; 0 = unset)
};

// Per-unit state.
struct UnitTgt {
    UnitTypeTgt t;
    // W6-02-08 [RE: TechnoClass +0x3D5 IsInPlayfield -- Unlimbo FUN_006F61D0
    // sets it from IsWithinUsableArea FUN_00577E90, a unit moving in sets it
    // (FUN_006F45F0, false -> true only), ResizeView FUN_00566E60 recomputes
    // every object; Evaluate_Object FUN_006F71D0 rejects a target without it].
    bool in_playfield = true;
    // W6-02-11 [RE: Mission Area_Guard (11) scans with threat 2 (@0x4D4234):
    // FUN_007072E0(1) = 2 x (GuardRange or the larger weapon range), at most
    // 16 cells]: a map object with Mission=Area Guard or a unit the player
    // ordered to Guard (GuardObject FUN_0072FE30 = Area_Guard). Any other order
    // clears it.
    bool area_guard = false;
};

// Per-building state (the campaign host sets the type flags at placement).
struct BldTgt {
    bool in_playfield = true;
    bool insignificant = false;
    bool legal_target = true;
};

// World-wide: the usable area (Map LocalSize) and the campaign switch of the
// Evaluate_Object Insignificant rule. Set only by the campaign host.
struct PlayfieldW6 {
    bool campaign = false;    // GameMode == campaign (DAT_00A8A1A0 == 0)
    bool valid = false;       // a usable area is configured
    int size_w = 0;           // [Map] Size= width
    int x = 0, y = 0, w = 0, h = 0;   // [Map] LocalSize= (or the last ResizeView)
};

}  // namespace ra2engine
