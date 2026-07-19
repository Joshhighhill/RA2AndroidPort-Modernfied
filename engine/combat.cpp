#include "combat.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "building.hpp"  // Building, foundation_cells, kill_building (Task 4)
#include "pathfind.hpp"  // Cellxy

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
bool valid_target(const std::vector<Unit>& units, int self_house, int idx) {
    if (idx < 0 || idx >= static_cast<int>(units.size())) return false;
    const Unit& v = units[idx];
    return v.alive && v.die_time < 0.0 && v.cargo_of < 0 && !v.is_aircraft &&
           v.house != self_house;
    // carried: untargetable (CB-1). !v.is_aircraft: an airborne/ground aircraft
    // is not a valid target -- air-to-air + ground AA are out [RE-GATE APPLIED:
    // aircraft.md §4]. The aircraft's OWN targets are ground units (is_aircraft
    // false), so its targeting is unaffected. Provably true (negation false) in
    // every existing demo (is_aircraft=false).
}

// Fires `u`'s weapon at `v` (caller has already range/cooldown-checked).
// Resets u's cooldown/fire_flash, applies damage_vs, handles idle-victim
// retaliation, and starts death if the hit is lethal. `attacker_idx`/
// `victim_idx` are u/v's own indices in `units` (needed for the retaliation
// target and occ.release_all_of).
void try_fire(std::vector<Unit>& units, Unit& u, Unit& v, int attacker_idx,
              int victim_idx, Occupancy& occ) {
    // docs/re/combat.md §5: RearmTimer := GetROF() (frames), converted to
    // seconds via kLogicFps; CONFIRMED no jitter/RNG in the reset.
    u.rof_cool = u.w_rof / kLogicFps;
    u.fire_flash = kFireFlashSecs;
    if (u.w_cell_spread >= 1.0) {
        // CB-2 [RE-GATE APPLIED: aircraft.md §4]: the BlimpHE 2-cell splash.
        // Reached ONLY by CellSpread>=1 warheads (the Kirov); every existing
        // weapon (tank AP=.3, infantry) has CellSpread<1 -> the else branch.
        apply_spread_damage(units, occ, u, v);
    } else {
        // EXISTING single-target delivery -- UNCHANGED (byte-identical path).
        const int dmg = damage_vs(u.w_damage, u.w_verses, v.armor_ix);
        v.hp -= dmg;
        // Return fire only if IDLE (spec: "return fire when hit" applies to
        // idle/guarding units; a unit under a move order keeps walking rather
        // than stopping to retaliate). Engineer/Spy Capture [RE: capture.md
        // §Design 6]: a still-DISGUISED attacker is treated as friendly, so the
        // victim does NOT retaliate-acquire it (the return-fire analog of the
        // auto-acquire skip below) -- keeping a disguised unit un-targeted even
        // if it fires. Inert for byte-identity: u.disguised defaults false and
        // no gate ever calls set_disguise, so `!u.disguised` is always true ->
        // the guard is unchanged in every gate.
        if (v.target == -1 && !v.moving() && !u.disguised) {
            // Idle victim retaliates (docs/re/combat.md §7: retaliation target
            // choice confirmed). Also records ITS OWN guard anchor at its
            // current position -- retaliation is a form of auto-acquire
            // (attack_order stays false), so it needs the same "post to return
            // to" bookkeeping the acquisition path below sets; without this the
            // anchor would default to (0,0) and misfire the stray-leash check
            // the next time this victim is processed.
            v.target = attacker_idx;
            v.guard_x = v.cell_x;
            v.guard_y = v.cell_y;
        }
        if (v.hp <= 0) {
            v.die_time = 0.0;
            occ.release_all_of(victim_idx, -1, -1);
            v.target = -1;
            v.path.clear();
            v.path_idx = 0;
        }
    }
}

}  // namespace

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
        if (w.is_aircraft) continue;                                 // no air-to-air
        if (w.house == attacker.house) continue;                     // enemies only
        const double d = dist_leptons(center.fx, center.fy, w.fx, w.fy);
        if (d > radius_lept) continue;                               // outside the splash
        const double frac = radius_lept > 0.0 ? d / radius_lept : 0.0;
        const double scale = 1.0 - (1.0 - pmax) * frac;             // 1.0 -> pmax linear
        const int base = static_cast<int>(attacker.w_damage * scale);  // trunc (combat.md §3)
        const int dmg = damage_vs(base, attacker.w_verses, w.armor_ix);
        w.hp -= dmg;
        if (w.hp <= 0) {
            w.die_time = 0.0;
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

std::vector<MoveRequest> step_combat(std::vector<Unit>& units, std::vector<Building>& buildings,
                                     Occupancy& occ, double dt) {
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
                const double range_leptons = u.w_range * kLeptonsPerCell;
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
                        const int dmg = damage_vs(u.w_damage, u.w_verses, b->armor_ix);
                        b->hp -= dmg;
                        if (b->hp <= 0) {
                            kill_building(*b);
                            u.target_building = -1;
                            u.attack_order = false;
                            // Clear every OTHER unit's target_building
                            // pointing at the now-dead building immediately
                            // (their own next-call validity check would also
                            // catch it via state == Dead, but the spec calls
                            // for this to happen within the same call).
                            for (Unit& other : units)
                                if (&other != &u && other.target_building == b_idx)
                                    other.target_building = -1;
                        }
                    }
                } else if (u.attack_order) {
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
        if (u.target != -1 && !valid_target(units, u.house, u.target)) {
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
            if (u.moving() || u.has_flight_goal) { /* no auto-acquire under a move order */ }
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
                    if (j == i || !valid_target(units, u.house, j) || units[j].disguised
                        || !(units[j].seen_by_mask & (1u << u.house)))
                        continue;
                    const Unit& v = units[j];
                    const double d = dist_leptons(u.fx, u.fy, v.fx, v.fy);
                    if (d <= u.w_range * kLeptonsPerCell && (best == -1 || d < best_d)) {
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
                try_fire(units, u, v, i, u.target, occ);
            }
        } else if (u.attack_order) {
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
