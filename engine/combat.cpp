#include "combat.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "building.hpp"  // Building, foundation_cells, kill_building (Task 4)
#include "pathfind.hpp"  // Cellxy
#include "weapon_unit.hpp"  // Gap fix C: per-target weapon slot

namespace ra2engine {

namespace {

// --- Constants (docs/re/combat.md; per-section refs noted at each use) ---
constexpr double kLogicFps = 15.0;         // ROF frames -> seconds (§5)
constexpr double kLeptonsPerCell = 256.0;  // lepton coordinate scale (§6)
// ModEnc/YRpp fallback (Low confidence -- docs/re/combat.md §7: the ini
// knobs Stray/RelaxedStray/GuardModeStray are confirmed to exist and be
// ini-configurable in this build, but the consuming formula/call site wasn't
// located). Guard-acquire radius defaults to weapon range; kGuardStray is
// the extra slack (in cells) an auto-acquired target -- or the guarding unit
// itself -- may drift before the engagement is abandoned and the unit falls
// back to its guard post.
constexpr double kGuardStray = 2.0;
constexpr double kDieAnimSecs = 1.0;    // renderer-tuned (Task 7), not RE'd.
constexpr double kFireFlashSecs = 0.4;  // renderer-tuned (Task 7), not RE'd.

const std::vector<std::string>& armor_names() {
    static const std::vector<std::string> names = {
        "none", "flak", "plate", "light", "medium", "heavy",
        "wood", "steel", "concrete", "special_1", "special_2",
    };
    return names;
}

std::string to_lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// Euclidean distance in leptons between two cell-space (fx,fy) points.
// docs/re/combat.md §6 (High): CONFIRMED Euclidean -- but the game's
// distance is full 3D (lepton Z included, `Δx²+Δy²+Δz²`); our sim carries no
// Z coordinate at all, so this is necessarily 2D. Documented divergence:
// negligible for same-plateau infantry (the overwhelming common case in this
// vertical slice), called out here rather than silently diverging from the
// calibration authority.
double dist_leptons(double x1, double y1, double x2, double y2) {
    const double dx = (x2 - x1) * kLeptonsPerCell;
    const double dy = (y2 - y1) * kLeptonsPerCell;
    return std::hypot(dx, dy);
}

// A unit is a legal combat target iff it exists, is alive, isn't mid-death-
// anim, and isn't friendly. docs/re/combat.md §1/§2 (GetFireError/
// ReceiveDamage): a dying/dead unit is never a valid target.
bool valid_target(const std::vector<Unit>& units, const World& world, const Unit& self, int idx) {
    if (idx < 0 || idx >= static_cast<int>(units.size())) return false;
    const Unit& v = units[idx];
    return v.alive && v.die_time < 0.0 && v.cargo_of < 0 && world.is_enemy(self.house, v.house) &&
           unit_weapon_for_unit(self, v) >= 0;
    // Multi-house/Alliances [RE: multihouse.md §3/§4]: hostility routes through
    // World::is_enemy. carried: untargetable (CB-1).
    // Gap fix C (G-44/G-50) [RE: docs/re/weapons.md §2, combat.md §2]: the old
    // hard `!v.is_aircraft` exclusion is replaced by GetFireError's weapon
    // gate -- an AIRBORNE aircraft is a target only for a weapon whose
    // projectile is AA=yes (Flak Track, Patriot, IFV, GGI, Apocalypse tusks),
    // a landed one for an AG weapon, and a 0 % Verses warhead cannot target
    // that armor at all. unit_weapon_for_unit picks primary, else secondary.
}

// Range (cells) of unit u's weapon slot `slot` without activating it: the
// active slot's live w_range (it may carry vet/IFV adjustments), else the
// type data of the other slot.
double slot_range(const Unit& u, int slot) {
    if (slot == u.w_active) return u.w_range;
    if (slot == 1) return secondary_view(u).range;
    if (u.vet.recv && is_elite(u.vet.veterancy) && u.vet.has_elite) return u.vet.elite.range;
    return u.vet.base.range;
}

const VeterancyRules* vet_rules_of(const World& world) {
    return world.veterancy_enabled() ? &world.veterancy_rules() : nullptr;
}

// Fires `u`'s weapon at `v` (caller has already range/cooldown-checked).
// Resets u's cooldown/fire_flash, applies damage_vs, handles idle-victim
// retaliation, and starts death if the hit is lethal. `attacker_idx`/
// `victim_idx` are u/v's own indices in `units` (needed for the retaliation
// target and occ.release_all_of).
// Projectile phase (P2) [RE: projectile.md §10]: when `launches` is non-null
// (World's projectiles_enabled_ firewall is OPEN) and the weapon's projectile
// flies, the shot emits a ProjectileLaunch instead of hitting -- World's
// step_projectiles delivers the damage at detonation through the SAME
// deliver_unit_hit / apply_spread_damage. With `launches == nullptr` (every
// byte-identity gate) this is the pre-P2 body verbatim: the extracted
// deliver_unit_hit is the old else-branch statement-for-statement.
void try_fire(std::vector<Unit>& units, Unit& u, Unit& v, int attacker_idx,
              int victim_idx, Occupancy& occ, std::vector<ProjectileLaunch>* launches) {
    // docs/re/combat.md §5: RearmTimer := GetROF() (frames), converted to
    // seconds via kLogicFps; CONFIRMED no jitter/RNG in the reset.
    u.rof_cool = u.w_rof / kLogicFps;
    u.fire_flash = kFireFlashSecs;
    // P4 [RE: abilities.md §B1/§C2/§E/§F]: a weapon whose warhead/weapon flags
    // make it an ABILITY (Parasite / Temporal / IvanBomb / Suicide) deals no
    // plain damage -- the shot is handed to World::step_abilities, which
    // resolves it this same frame (abilities are instant: checked BEFORE the
    // P2 projectile launch). fire_kind is None unless a P4 profile is bound
    // (no pre-P4 gate binds one) -> the existing path below runs.
    if (u.abil.fire_kind != AbilityKind::None) {
        u.ability_fired_unit = victim_idx;
        return;
    }
    if (launches && u.w_proj.flies()) {
        launches->push_back({attacker_idx, victim_idx, -1});
        return;
    }
    if (u.w_cell_spread >= 1.0) {
        // CB-2 [RE-GATE APPLIED: aircraft.md §4]: the BlimpHE 2-cell splash.
        // Reached ONLY by CellSpread>=1 warheads (the Kirov); every existing
        // weapon (tank AP=.3, infantry) has CellSpread<1 -> the else branch.
        apply_spread_damage(units, occ, u, v);
    } else {
        // EXISTING single-target delivery -- UNCHANGED (byte-identical path).
        deliver_unit_hit(units, occ, attacker_idx, victim_idx);
    }
}

}  // namespace

// P2: the single-target damage tail extracted VERBATIM from try_fire's old
// else-branch (same statements, same order) so the instant path and the
// projectile detonation path share one implementation.
void deliver_unit_hit(std::vector<Unit>& units, Occupancy& occ, int attacker_idx, int victim_idx) {
    deliver_hit_from(units, occ, units[attacker_idx], attacker_idx, -1, victim_idx);
}

// Gap fix C: the single-target hit generalized over the shooter -- a unit
// (attacker_idx >= 0, `u` = units[attacker_idx]) or a base defense
// (attacker_building >= 0, `u` = a transient shooter carrying the building's
// weapon). Body = the P2 deliver_unit_hit tail; the retaliation target is the
// attacking unit, or the attacking building for a defense shot.
void deliver_hit_from(std::vector<Unit>& units, Occupancy& occ, const Unit& u, int attacker_idx,
                      int attacker_building, int victim_idx) {
    Unit& v = units[victim_idx];
    // P5 [RE: veterancy.md V6]: a vet-enabled World arms v.vet.recv; the
    // receive chain (ArmorMultiplier, STRONGER, max 1) runs BEFORE the Verses.
    // v.vet.recv is false in every legacy World -> raw == u.w_damage as before.
    int raw = u.w_damage;
    if (v.vet.recv) raw = vet_receive(raw, v.vet);
    const int dmg = damage_vs(raw, u.w_verses, v.armor_ix);
    v.hp -= dmg;
    // Return fire only if IDLE (spec: "return fire when hit" applies to
    // idle/guarding units; a unit under a move order keeps walking rather
    // than stopping to retaliate). Engineer/Spy Capture [RE: capture.md
    // §Design 6]: a still-DISGUISED attacker is treated as friendly, so the
    // victim does NOT retaliate-acquire it.
    if (v.target == -1 && !v.moving() && !u.disguised && !v.abil.no_retaliate) {
        // Idle victim retaliates (docs/re/combat.md §7) and records ITS OWN
        // guard anchor at its current position (retaliation is a form of
        // auto-acquire: attack_order stays false).
        if (attacker_idx >= 0) {
            v.target = attacker_idx;
        } else if (attacker_building >= 0 && v.target_building == -1) {
            v.target_building = attacker_building;   // shoot back at the defense
        }
        v.guard_x = v.cell_x;
        v.guard_y = v.cell_y;
    }
    if (v.hp <= 0) {
        v.die_time = 0.0;
        v.vet.killed_by = attacker_idx;   // P5 [V3]: inert stamp (credit pass only)
        occ.release_all_of(victim_idx, -1, -1);
        v.target = -1;
        v.target_building = -1;
        v.path.clear();
        v.path_idx = 0;
    }
}

// P2: the attack-building damage tail extracted from step_combat's building
// branch (same statements, same order). The attacker's own target_building is
// cleared only if it still names this building -- always true on the instant
// path (it IS the branch condition), so that path is byte-identical; at a
// projectile detonation the attacker may have re-targeted meanwhile.
void deliver_building_hit(std::vector<Unit>& units, std::vector<Building>& buildings,
                          int attacker_idx, int b_idx) {
    Unit& u = units[attacker_idx];
    Building* b = &buildings[b_idx];
    // P5 [RE: veterancy.md V6]: a vet-enabled World arms b->vet_recv
    // (ArmorMultiplier, max 1, then Verses); false in every legacy World.
    int raw = u.w_damage;
    if (b->vet_recv) raw = receive_damage(raw, b->armor_mult, false, 1.0);
    const int dmg = damage_vs(raw, u.w_verses, b->armor_ix);
    b->hp -= dmg;
    if (b->hp <= 0) {
        b->killed_by = attacker_idx;   // P5 [V3]: inert stamp
        kill_building(*b);
        if (u.target_building == b_idx) {
            u.target_building = -1;
            u.attack_order = false;
        }
        // Clear every OTHER unit's target_building pointing at the now-dead
        // building immediately (their own next-call validity check would also
        // catch it via state == Dead, but the spec calls for this to happen
        // within the same call).
        for (Unit& other : units)
            if (&other != &u && other.target_building == b_idx)
                other.target_building = -1;
    }
}

// CB-2 air-to-ground splash [RE-GATE APPLIED: aircraft.md §4 -> combat.md §7,
// FUN_00489190]. Damages every ENEMY, non-carried, non-aircraft unit within
// round(cell_spread) cells of `center` (radius in leptons = round(cell_spread)
// * kLeptonsPerCell; BlimpHE CellSpread=2 -> ftol(2*256)=512 leptons). Damage
// falls off LINEARLY from 100% at the impact point to `percent_at_max` at the
// edge (the adjudicated approximation of the un-unwound §7 curve), then runs
// the standard damage_vs(Verses) truncated per victim (combat.md §3). Ground
// AA is out of scope, so victims do NOT retaliate against the aircraft.
// CB-4 [RE: superweapon.md §2/§5]: PROMOTED to ra2engine (external linkage) so
// World::fire_superweapon can REUSE it for the nuke detonation -- body BYTE-
// IDENTICAL to the CB-2 anonymous-namespace original; only its linkage moved.
void apply_spread_damage(std::vector<Unit>& units, Occupancy& occ, const Unit& attacker,
                         const Unit& center) {
    const double radius_cells = std::round(attacker.w_cell_spread);   // round(CellSpread)
    const double radius_lept = radius_cells * kLeptonsPerCell;        // splash radius (leptons)
    const double pmax = attacker.w_percent_at_max;                    // edge scale (BlimpHE .5)
    for (int j = 0; j < static_cast<int>(units.size()); ++j) {
        Unit& w = units[j];
        if (!w.alive || w.die_time >= 0.0) continue;                  // dead/dying: skip
        if (w.cargo_of >= 0) continue;                               // carried: off-map
        // Gap fix C (G-44): the blast stays in its own layer -- an AA burst at
        // an airborne target hits airborne units, a ground blast ground units
        // (the original's 3-D distance; the clone has no shared Z scale).
        if (unit_airborne(w) != unit_airborne(center)) continue;
        if (w.house == attacker.house) continue;                     // enemies only
        const double d = dist_leptons(center.fx, center.fy, w.fx, w.fy);
        if (d > radius_lept) continue;                               // outside the splash
        const double frac = radius_lept > 0.0 ? d / radius_lept : 0.0;
        const double scale = 1.0 - (1.0 - pmax) * frac;             // 1.0 -> pmax linear
        // P5 [RE: veterancy.md V6]: the receive chain runs on the full hit
        // before the falloff/Verses; only when the victim's hook is armed
        // (never in a legacy World -> the base expression is unchanged).
        const int full = w.vet.recv ? vet_receive(attacker.w_damage, w.vet) : attacker.w_damage;
        const int base = static_cast<int>(full * scale);  // trunc (combat.md §3)
        const int dmg = damage_vs(base, attacker.w_verses, w.armor_ix);
        w.hp -= dmg;
        if (w.hp <= 0) {
            w.die_time = 0.0;
            // P5 [V3]: inert killer stamp; the attacker is a real unit only when
            // it lives in `units` (the nuke's virtual detonator does not).
            if (&attacker >= units.data() && &attacker < units.data() + units.size())
                w.vet.killed_by = static_cast<int>(&attacker - units.data());
            occ.release_all_of(j, -1, -1);
            w.target = -1;
            w.path.clear();
            w.path_idx = 0;
        }
    }
}

int armor_index(const std::string& armor) {
    const std::string lower = to_lower(armor);
    const auto& names = armor_names();
    for (std::size_t i = 0; i < names.size(); ++i)
        if (names[i] == lower) return static_cast<int>(i);
    return 0;  // unknown -> "none"
}

int damage_vs(int damage, const std::vector<int>& verses, int ai) {
    if (verses.empty()) return damage;  // defensive: no Verses table -> 100%
    int idx = ai;
    if (idx < 0 || idx >= static_cast<int>(verses.size())) idx = 0;  // clamp -> "none"
    // docs/re/combat.md §3: truncate toward zero. Plain integer division
    // truncates toward zero for non-negative operands (damage and percent
    // are both always >= 0 here), which is exactly the `_ftol`-observed
    // rounding convention -- no separate std::trunc call needed.
    return damage * verses[idx] / 100;
}

std::vector<MoveRequest> step_combat(const World& world, std::vector<Unit>& units,
                                     std::vector<Building>& buildings, Occupancy& occ, double dt,
                                     std::vector<ProjectileLaunch>* launches) {
    std::vector<MoveRequest> requests;
    const int n = static_cast<int>(units.size());

    for (int i = 0; i < n; ++i) {
        Unit& u = units[i];

        // (a) Dying-progress: advance the death timer and tombstone at
        // kDieAnimSecs; skip every other combat action this call. Once
        // tombstoned (alive=false), die_time stays >= kDieAnimSecs forever,
        // so this branch keeps catching (and no-op'ing on) the unit -- it
        // never falls through to (b)/(c)/etc again.
        if (u.die_time >= 0.0) {
            if (u.alive) {
                u.die_time += dt;
                if (u.die_time >= kDieAnimSecs) u.alive = false;
            }
            continue;
        }
        // (b) Defensive: unreachable given the tombstone model above (alive
        // only ever flips false via the die_time path above), but a unit
        // spawned pre-dead should still never act.
        if (!u.alive) continue;

        // (b2) CB-1: a CARRIED unit is off-map -- it cannot fire, cool down,
        // acquire, or pursue [spec §4].
        if (u.cargo_of >= 0) continue;
        // (b3) P4 [RE: abilities.md §C1/§C2/§A]: a FROZEN unit (warping in,
        // being erased, deployed) neither fires, acquires nor pursues.
        // Inert: frozen is false unless a P4 profile is bound.
        if (u.frozen) continue;

        // (c) ROF cooldown + fire-flash decay -- every living, non-dying
        // unit, every call, regardless of target state. docs/re/combat.md
        // §5: deterministic frame countdown, no jitter.
        u.rof_cool -= dt;
        if (u.rof_cool < 0.0) u.rof_cool = 0.0;
        u.fire_flash -= dt;
        if (u.fire_flash < 0.0) u.fire_flash = 0.0;

        // --- Buildings & Construction Task 4: explicit attack-building path.
        // target_building is ONLY ever set by World::order_attack_building
        // (never by auto-acquire -- units NEVER auto-acquire buildings, and
        // buildings never retaliate, no weapons in this roster), so this
        // whole block is unreachable (u.target_building stays at its -1
        // default) for every unit in the pre-Task-4 unit-vs-unit tests and
        // for any unit not given an explicit attack-building order --
        // required for the byte-identical-when-buildings-is-empty gate.
        if (u.target_building >= 0) {
            Building* b = (u.target_building < static_cast<int>(buildings.size()))
                              ? &buildings[u.target_building]
                              : nullptr;
            const bool valid = b && b->alive && b->state != Building::State::Dead;
            if (!valid) {
                // Target validity: gone/dead/Dead-state -> clear and fall
                // back to normal (unit) targeting below, same "an explicit
                // order dies with its target" policy as (d) below.
                u.target_building = -1;
                u.attack_order = false;
            } else {
                const int b_idx = u.target_building;
                // Gap fix C: the slot that can hit this structure's armor (a
                // pure-AA weapon or a 0 % Verses warhead cannot) [weapons.md §2].
                const int ws = unit_weapon_for_building(u, b->armor_ix);
                if (ws < 0 && !u.abil.c4 && u.abil.fire_kind == AbilityKind::None) {
                    u.target_building = -1;
                    u.attack_order = false;
                    continue;
                }
                if (ws >= 0) activate_weapon_slot(u, ws, vet_rules_of(world));
                // Range to the NEAREST foundation cell (a 4x4 building is in
                // range of its EDGE, not its center).
                const auto cells = foundation_cells(b->cx, b->cy, b->fw, b->fh);
                double best_d = -1.0;
                int nx = 0, ny = 0;
                for (const Cellxy& cell : cells) {
                    const double d = dist_leptons(u.fx, u.fy, static_cast<double>(cell.x),
                                                  static_cast<double>(cell.y));
                    if (best_d < 0.0 || d < best_d) {
                        best_d = d;
                        nx = cell.x;
                        ny = cell.y;
                    }
                }
                // P4 [RE: abilities.md §D1]: a C4=yes infantry walks to its
                // Sapper (Secondary) range against a building, not its
                // primary's. abil.c4 is false unless a P4 profile is bound.
                const double range_leptons =
                    (u.abil.c4 ? u.abil.c4_range : u.w_range) * kLeptonsPerCell;
                if (best_d <= range_leptons) {
                    if (u.rof_cool <= 0.0) {
                        // Aim gate, mirroring the unit path exactly
                        // (combat.cpp ~L276-278): a vehicle holds fire until
                        // its turret has slewed onto the nearest foundation
                        // cell (step_turrets ran this frame, before
                        // step_combat, using this same facing). Infantry are
                        // exempt -- zero behavior change. rof_cool untouched
                        // while unaimed.
                        if (u.is_vehicle && !u.omni_fire &&
                            !vehicle_aimed(u, facing_u16_from_delta(
                                                  static_cast<double>(nx) - u.fx,
                                                  static_cast<double>(ny) - u.fy)))
                            continue;  // hold fire this frame; rof_cool untouched
                        u.rof_cool = u.w_rof / kLogicFps;
                        u.fire_flash = kFireFlashSecs;
                        // P4 [RE: abilities.md §C2/§D/§E/§F]: an ability weapon
                        // (or a C4 infantry) hands the building shot to
                        // World::step_abilities instead of plain damage. Inert
                        // pre-P4 (fire_kind None, c4 false).
                        if (u.abil.fire_kind != AbilityKind::None || u.abil.c4) {
                            u.ability_fired_building = b_idx;
                            continue;
                        }
                        // P2 firewall: a flying projectile defers the hit to
                        // World::step_projectiles; otherwise the pre-P2 tail.
                        if (launches && u.w_proj.flies())
                            launches->push_back({i, -1, b_idx});
                        else
                            deliver_building_hit(units, buildings, i, b_idx);
                    }
                } else if (u.attack_order && !(u.deploy_fire && u.deployed)) {
                    // Dedup mirrors the unit-pursuit branch below (f): only
                    // re-request when there's no live path, or the current
                    // path's goal has stopped being a firing position for the
                    // building (its nearest foundation cell to that goal has
                    // left weapon range). The building never moves, but the
                    // goal-to-nearest-cell distance can still change because
                    // "nearest" is relative to the goal, not the unit -- e.g.
                    // a goal on the far side of a multi-cell foundation.
                    bool need = !u.moving();
                    if (!need) {
                        const Cellxy g = u.path.back();
                        double g_best = -1.0;
                        for (const Cellxy& cell : cells) {
                            const double d = dist_leptons(
                                static_cast<double>(g.x), static_cast<double>(g.y),
                                static_cast<double>(cell.x), static_cast<double>(cell.y));
                            if (g_best < 0.0 || d < g_best) g_best = d;
                        }
                        need = g_best > range_leptons;
                    }
                    if (need) requests.push_back({i, nx, ny, /*approach=*/true});
                } else if (!u.attack_order) {
                    // Gap fix C: a retaliation/auto target (not an explicit
                    // order) out of range is dropped -- the unit goes back to
                    // normal acquisition instead of staring at it forever.
                    u.target_building = -1;
                }
                continue;  // handled -- skip the unit-vs-unit path entirely
            }
        }

        // (d) Target validation: drop a stale target (gone from the vector,
        // dead, dying, or no longer hostile). Clearing also resets
        // attack_order -- an explicit order dies with its target, so a
        // fresh acquisition below starts clean rather than inheriting a
        // stale "pursue indefinitely" flag for a target it hasn't picked
        // yet.
        if (u.target != -1 && !valid_target(units, world, u, u.target)) {
            u.target = -1;
            u.attack_order = false;
        }

        // (e) Auto-acquire: nearest hostile within weapon range, ties ->
        // lowest index. Only runs when idle (no current target survived
        // validation, and no explicit order or retaliation set one this
        // call). Records the guard anchor at the unit's OWN current cell --
        // the post it should return to if the target strays too far (§7,
        // ModEnc fallback, Low).
        // Only IDLE/guarding units auto-acquire (spec wording): a unit
        // executing a move order walks on without stopping to fight; it will
        // acquire on arrival if a hostile is still in range.
        if (u.target == -1) {
            // No auto-acquire while under a move order. A ground unit's move is
            // its A* path (moving()); an aircraft cruising or taking off toward
            // a flight goal (has_flight_goal, with an empty ground path) is the
            // flight analog -- it flies its route without stopping to fight,
            // exactly as a ground unit walks on [RE-GATE APPLIED: aircraft.md §6
            // -- a Move order overrides auto-attack; an explicit order_attack
            // sets u.target and skips this block, so air-to-ground attack is
            // unaffected]. Cannot change the 9 ground-only gates: has_flight_goal
            // is always false there.
            // P4 [RE: abilities.md §F]: CanPassiveAquire=no (the terrorist)
            // never auto-acquires. Inert: false unless a P4 profile is bound.
            if (u.moving() || u.has_flight_goal || u.abil.no_passive_acquire) { /* no auto-acquire */ }
            else {
                int best = -1;
                double best_d = 0.0;
                for (int j = 0; j < n; ++j) {
                    // Engineer/Spy Capture [RE: capture.md §Design 6]: a still-
                    // disguised unit is treated as FRIENDLY -- never auto-
                    // acquired (a disguised spy walks through enemy territory
                    // unfired-upon). Byte-identity: Unit::disguised defaults
                    // false and no gate ever calls set_disguise (capture_any_
                    // stays false), so this clause is false for every candidate
                    // in every gate -> inert (combat 78 / tank 144 unchanged).
                    // The skip lives ONLY in this AUTO-acquire scan, NOT in
                    // valid_target (which also gates the explicit-attack /
                    // stale-target-drop path (d)) -- an explicit order_attack on
                    // a known spy is never blocked. A dog's DetectDisguise clears
                    // `disguised` -> the spy is targetable again next scan (no
                    // extra code needed).
                    // Shroud/Fog [RE: shroud.md §6]: a candidate whose cell is
                    // NOT visible to the scanning house cannot be auto-acquired
                    // (mirrors the disguise precedent above). seen_by_mask
                    // defaults 0xFF and only step_visibility (behind shroud_any_)
                    // ever changes it, so with shroud off
                    // (0xFF & (1<<u.house)) != 0 -> the clause is always false ->
                    // combat 78 / tank 144 byte-identical. This lives ONLY in the
                    // AUTO-acquire scan, NOT valid_target (an explicit
                    // order_attack on a known unit is never blocked, §6).
                    if (j == i || !valid_target(units, world, u, j) || units[j].disguised
                        || !(units[j].seen_by_mask & (1u << u.house)))
                        continue;
                    const Unit& v = units[j];
                    const double d = dist_leptons(u.fx, u.fy, v.fx, v.fy);
                    // Gap fix C: in range of the weapon that WOULD engage v.
                    const double rng = slot_range(u, unit_weapon_for_unit(u, v));
                    if (d <= rng * kLeptonsPerCell && (best == -1 || d < best_d)) {
                        best = j;
                        best_d = d;
                    }
                }
                if (best != -1) {
                    u.target = best;
                    u.guard_x = u.cell_x;
                    u.guard_y = u.cell_y;
                    u.attack_order = false;
                }
            }
        }

        if (u.target == -1) continue;  // nothing to engage this call

        // (f) Engage.
        Unit& v = units[u.target];
        // Gap fix C (G-41/G-44): bring the weapon that engages THIS target
        // into w_* (primary unless only the secondary can hit it). A no-op for
        // every unit without a secondary.
        activate_weapon_slot(u, unit_weapon_for_unit(u, v), vet_rules_of(world));
        const double range_leptons = u.w_range * kLeptonsPerCell;
        const double d = dist_leptons(u.fx, u.fy, v.fx, v.fy);

        if (d <= range_leptons) {
            if (u.rof_cool <= 0.0) {
                // Aim gate (docs/re/voxel-render.md §5, FireError::FACING): a
                // vehicle holds fire until its turret has finished slewing onto
                // the target (step_turrets ran this frame, before step_combat,
                // computing `want` from these same fx/fy, so the exact
                // comparison in vehicle_aimed is well-defined). Infantry are
                // exempt -- zero behavior change.
                if (u.is_vehicle && !u.omni_fire &&
                    !vehicle_aimed(u, facing_u16_from_delta(v.fx - u.fx, v.fy - u.fy)))
                    continue;  // hold fire this frame; rof_cool untouched
                try_fire(units, u, v, i, u.target, occ, launches);
            }
        } else if (u.attack_order && !(u.deploy_fire && u.deployed)) {
            // (Gap fix C, G-45: a deployed GI holds its post -- no approach.)
            // Approach: ask the caller for a firing position near the target.
            // The original engine approaches until in weapon range and never
            // paths to the victim's own (occupied) cell (docs/re/
            // infantry-anim.md §7); World::step batch-assigns each attacker
            // a distinct free cell around the target. Re-request only when
            // the current path's goal has stopped being a firing position
            // for the target's CURRENT location (target moved out of the
            // goal's range circle), or there is no live path at all.
            bool need = !u.moving();
            if (!need) {
                const Cellxy g = u.path.back();
                need = dist_leptons(static_cast<double>(g.x), static_cast<double>(g.y),
                                    v.fx, v.fy) > range_leptons;
            }
            if (need) requests.push_back({i, v.cell_x, v.cell_y, /*approach=*/true});
        } else {
            // Auto-acquired (or retaliation-acquired): leashed to the guard
            // anchor. Drop if EITHER the unit itself or its target has
            // strayed past weapon range + kGuardStray from that anchor (§7,
            // ModEnc fallback, Low), then ask to return home if not already
            // there.
            const double stray = range_leptons + kGuardStray * kLeptonsPerCell;
            const double d_self = dist_leptons(u.fx, u.fy, static_cast<double>(u.guard_x),
                                                static_cast<double>(u.guard_y));
            const double d_target = dist_leptons(v.fx, v.fy, static_cast<double>(u.guard_x),
                                                  static_cast<double>(u.guard_y));
            if (d_self > stray || d_target > stray) {
                u.target = -1;
                if (u.cell_x != u.guard_x || u.cell_y != u.guard_y) {
                    if (u.path.empty() || u.path.back().x != u.guard_x ||
                        u.path.back().y != u.guard_y)
                        requests.push_back({i, u.guard_x, u.guard_y});
                }
            }
        }
    }

    return requests;
}

}  // namespace ra2engine
