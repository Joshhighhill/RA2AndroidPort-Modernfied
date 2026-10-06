#pragma once
// Gap fix C -- a Unit's active weapon (slot 0 primary / slot 1 secondary) and
// the per-target pick. Used by combat.cpp (choose + activate before a shot),
// world_veterancy.cpp (re-derive w_* after a rank/crate change) and the
// projectile detonation (re-activate the slot a shell was fired from).
#include "world.hpp"

namespace ra2engine {

// True iff `v` is in the air (an aircraft above ground). A landed aircraft is
// a ground target.
inline bool unit_airborne(const Unit& v) { return v.is_aircraft && v.altitude > 0.0; }

// The primary slot as a WeaponSlot view (for choose_weapon): valid, the
// unit's ACTIVE-independent primary verses + AA/AG.
WeaponSlot primary_view(const Unit& u);
// The secondary currently in effect (elite variant when elite and defined).
const WeaponSlot& secondary_view(const Unit& u);

// Weapon slot `u` would use against unit `v` / a building (ground, armor
// `armor_ix`); -1 = none can engage. A deployed GI-type forces its
// DeployFireWeapon slot.
int unit_weapon_for_unit(const Unit& u, const Unit& v);
int unit_weapon_for_building(const Unit& u, int armor_ix);

// Load slot `slot` into u.w_* (damage/rof/range/verses/spread/proj/AA/AG),
// applying the veterancy multipliers when `vr` is non-null and u.vet.recv
// (the vet World), else the legacy veteran_rank firepower bake. No-op when
// slot == u.w_active. Slot 1 without a valid secondary is ignored.
void activate_weapon_slot(Unit& u, int slot, const VeterancyRules* vr);

}  // namespace ra2engine
