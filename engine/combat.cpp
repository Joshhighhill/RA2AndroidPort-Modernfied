#include "combat.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

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
    return v.alive && v.die_time < 0.0 && v.house != self_house;
}

// Fires `u`'s weapon at `v` (caller has already range/cooldown-checked).
// Resets u's cooldown/fire_flash, applies damage_vs, handles idle-victim
// retaliation, and starts death if the hit is lethal. `attacker_idx`/
// `victim_idx` are u/v's own indices in `units` (needed for the retaliation
// target and occ.release_all_of).
void try_fire(Unit& u, Unit& v, int attacker_idx, int victim_idx, Occupancy& occ) {
    // docs/re/combat.md §5: RearmTimer := GetROF() (frames), converted to
    // seconds via kLogicFps; CONFIRMED no jitter/RNG in the reset.
    u.rof_cool = u.w_rof / kLogicFps;
    u.fire_flash = kFireFlashSecs;
    const int dmg = damage_vs(u.w_damage, u.w_verses, v.armor_ix);
    v.hp -= dmg;
    // Return fire only if IDLE (spec: "return fire when hit" applies to
    // idle/guarding units; a unit under a move order keeps walking rather
    // than stopping to retaliate).
    if (v.target == -1 && !v.moving()) {
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

}  // namespace

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

std::vector<MoveRequest> step_combat(std::vector<Unit>& units, Occupancy& occ, double dt) {
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

        // (c) ROF cooldown + fire-flash decay -- every living, non-dying
        // unit, every call, regardless of target state. docs/re/combat.md
        // §5: deterministic frame countdown, no jitter.
        u.rof_cool -= dt;
        if (u.rof_cool < 0.0) u.rof_cool = 0.0;
        u.fire_flash -= dt;
        if (u.fire_flash < 0.0) u.fire_flash = 0.0;

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
            if (u.moving()) { /* no auto-acquire while under a move order */ }
            else {
                int best = -1;
                double best_d = 0.0;
                for (int j = 0; j < n; ++j) {
                    if (j == i || !valid_target(units, u.house, j)) continue;
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
            if (u.rof_cool <= 0.0) try_fire(u, v, i, u.target, occ);
        } else if (u.attack_order) {
            // Explicit order: pursue indefinitely. Dedup: only ask again if
            // the target's cell differs from where this unit is already
            // headed.
            const Cellxy goal{v.cell_x, v.cell_y};
            if (u.path.empty() || u.path.back().x != goal.x || u.path.back().y != goal.y)
                requests.push_back({i, goal.x, goal.y});
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
