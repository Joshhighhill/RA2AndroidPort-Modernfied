#include "ai_team.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace ra2engine {

OpClass classify_op(int action) {
    // [RE-GATE APPLIED: ai-team.md §3 — the FULL recovered dispatch table. The
    // classifier FUN_006e30d0 enumerates ~90 valid actions (up to 0x90) via a
    // jump table; this switch is the single table location. The IMPLEMENTABLE
    // core below is exactly the recovered set; EVERY other valid action
    // (transport 0x2e/0x2f/0x50/0x6b, aircraft 0x1e/0x55-0x57, superweapon/
    // support 0x3d/0x3e, droppod, cosmetics) and every unrecognised number is
    // Inert: logged skip, pc++ — parsed + RE-documented, never executed
    // (spec §2 scope bound).]
    switch (action) {
        case kOpAttackQuarry:     return OpClass::AttackQuarry;
        case kOpAttackMove:       return OpClass::AttackQuarry;  // 0x3f = quarry + regroup
        case kOpAttackHouse:      return OpClass::AttackHouse;
        case kOpAttackBuilding:   return OpClass::AttackBuilding;
        case kOpAttackUnits:      return OpClass::AttackUnits;
        case kOpMoveToWaypoint:   return OpClass::MoveWaypoint;
        case kOpMoveToCell:       return OpClass::MoveCell;
        case kOpGuardArea:        return OpClass::GuardArea;
        case kOpTimedGuard:       return OpClass::TimedGuard;
        case kOpTimedGuardAlt:    return OpClass::TimedGuard;    // 0x3a same unit as 5
        case kOpRegroup:          return OpClass::Regroup;
        case kOpScatter:          return OpClass::Scatter;
        case kOpJumpToLine:       return OpClass::Jump;
        case kOpDisband:          return OpClass::Disband;
        default:                  return OpClass::Inert;
    }
}

Cellxy gather_cell(int ex, int ey, std::uint32_t& rng) {
    // [RE-GATE APPLIED: ai-team.md §2] — a point on a circle of radius
    // kAiSafeDistance (AISafeDistance = 20 cells) around the ENEMY base centre
    // (ex,ey), at a SEEDED-xorshift32 angle from `rng` (ai_base's ai_rand
    // substituting the lockstep RNG [DEVIATION]). The caller snaps this ideal
    // ring cell to the nearest free cell via the existing move/path machinery.
    const double theta =
        (double)ai_rand(rng) / 4294967296.0 * 6.283185307179586;  // [0, 2*pi)
    const double rad = (double)kAiSafeDistance;
    return Cellxy{ex + (int)std::lround(rad * std::cos(theta)),
                  ey + (int)std::lround(rad * std::sin(theta))};
}

namespace {
// Alive-member helpers over the PARALLEL view.
int alive_count(const TeamView& v) {
    int n = 0;
    for (const TeamMemberView& m : v.members)
        if (m.alive) ++n;
    return n;
}
// All live members within `cells` of (cx,cy) — the recovered Stray/CloseEnough
// radius predicate [RE-GATE APPLIED: ai-team.md §2]. Squared-distance in cells.
bool all_within(const TeamView& v, int cx, int cy, double cells) {
    const double r2 = cells * cells;
    for (const TeamMemberView& m : v.members) {
        if (!m.alive) continue;
        const double dx = (double)(m.cx - cx), dy = (double)(m.cy - cy);
        if (dx * dx + dy * dy > r2) return false;
    }
    return true;
}
Cellxy centroid(const TeamView& v) {
    long long sx = 0, sy = 0, n = 0;
    for (const TeamMemberView& m : v.members)
        if (m.alive) { sx += m.cx; sy += m.cy; ++n; }
    if (n == 0) return Cellxy{v.base_x, v.base_y};
    return Cellxy{(int)(sx / n), (int)(sy / n)};
}
}  // namespace

TeamCommand step_team(TeamInstance& team, const TeamView& view,
                      const AimdData& aimd, std::uint32_t& rng, double dt,
                      bool multiplay) {
    TeamCommand c;
    if (team.teamtype < 0 || team.teamtype >= (int)aimd.teams.size()) {
        team.state = TeamState::Disbanding;
        return c;
    }
    const TeamType& tt = aimd.teams[team.teamtype];
    const ScriptType& sc = aimd.scripts[tt.script];
    const int alive = alive_count(view);

    // Team wiped after launch -> Disbanding, any state [RE: ai-team.md §2].
    if (team.state != TeamState::Recruiting && alive == 0) {
        team.state = TeamState::Disbanding;
        return c;
    }

    switch (team.state) {
        case TeamState::Recruiting: {
            team.recruit_timer += dt;
            const bool complete = team_shortfall_type(team, aimd) < 0;
            // Full=yes: WAIT for full TaskForce strength before leaving
            // Recruiting. Full=no (all stock offensive teams): ACT UNDER
            // STRENGTH — launch as soon as it has any member, never waiting for
            // the dissolve timer [RE-GATE APPLIED: ai-team.md §2].
            if (complete || (!tt.full && alive > 0)) {
                team.state = TeamState::Gathering;
                const Cellxy g = gather_cell(view.enemy_x, view.enemy_y, rng);
                team.gather_x = g.x;
                team.gather_y = g.y;
                team.move_issued = false;
                return c;
            }
            // DissolveUnfilledTeamDelay is MULTIPLAY ONLY: only an MP unfilled
            // team dissolves on the timer. The skirmish/campaign default
            // (multiplay=false) NEVER dissolves — it keeps recruiting/producing
            // toward strength [RE-GATE APPLIED: ai-team.md §2].
            if (multiplay &&
                team.recruit_timer * 15.0 >= (double)kAiDissolveUnfilledDelayFrames)
                team.state = TeamState::Disbanding;
            return c;  // Recruiting emits no field command (caller recruits/produces)
        }

        case TeamState::Gathering: {
            // A death that reopens a slot drops a full=yes team back to
            // Recruiting [RE: ai-team.md §2].
            if (tt.full && team_shortfall_type(team, aimd) >= 0) {
                team.state = TeamState::Recruiting;
                team.recruit_timer = 0.0;
                team.move_issued = false;
                return c;
            }
            if (!team.move_issued) {
                c.kind = TeamCommand::Kind::MoveTeam;
                c.x = team.gather_x;
                c.y = team.gather_y;
                team.move_issued = true;
                return c;
            }
            // GATHERED = all live members within Rules.Stray (2.0 cells) of the
            // gather focus [RE-GATE APPLIED: ai-team.md §2].
            if (all_within(view, team.gather_x, team.gather_y, kAiStrayCells)) {
                team.state = TeamState::Executing;
                team.script_pc = 0;
                team.script_timer = 0.0;
                team.move_issued = false;
            }
            return c;
        }

        case TeamState::Executing: {
            if (team.script_pc < 0 || team.script_pc >= (int)sc.actions.size()) {
                team.state = TeamState::Disbanding;  // script ran off the end
                return c;
            }
            const ScriptAction a = sc.actions[team.script_pc];
            const OpClass oc = classify_op(a.action);

            // The four attack classes share ONE emit-while-target-exists /
            // advance-when-extinct body, differing only in the quarry class they
            // resolve. The caller (World) computes view.quarry_exists for THIS
            // action's effective quarry [RE-GATE APPLIED: ai-team.md §3/§5].
            if (oc == OpClass::AttackQuarry || oc == OpClass::AttackHouse ||
                oc == OpClass::AttackBuilding || oc == OpClass::AttackUnits) {
                if (!view.quarry_exists) {
                    ++team.script_pc;  // target class extinct -> advance
                    team.move_issued = false;
                    return c;
                }
                c.kind = TeamCommand::Kind::AttackQuarry;
                c.quarry = (oc == OpClass::AttackQuarry) ? a.argument
                         : (oc == OpClass::AttackBuilding) ? kQuarryBuildings
                         : kQuarryAnything;  // AttackHouse / AttackUnits (§5)
                return c;                    // re-emitted until the class is gone
            }

            switch (oc) {
                case OpClass::MoveWaypoint: {
                    const Cellxy w =
                        (a.argument >= 0 && a.argument < (int)view.waypoints.size())
                            ? view.waypoints[a.argument]
                            : Cellxy{view.enemy_x, view.enemy_y};  // [RE-FALLBACK:
                              // unmapped waypoint -> enemy base centre]
                    if (!team.move_issued) {
                        c.kind = TeamCommand::Kind::MoveTeam;
                        c.x = w.x;
                        c.y = w.y;
                        team.move_issued = true;
                    } else if (all_within(view, w.x, w.y, kAiCloseEnoughCells)) {
                        ++team.script_pc;          // arrived (CloseEnough) -> next
                        team.move_issued = false;
                    }
                    return c;
                }
                case OpClass::MoveCell: {
                    // [RE-FALLBACK: ai-team.md §3 — argument is a PACKED cell; the
                    // pure module has no map width to unpack it, so it resolves to
                    // the enemy base centre (march-at-enemy) until packed-cell
                    // decoding lands (AI-3).]
                    const Cellxy w{view.enemy_x, view.enemy_y};
                    if (!team.move_issued) {
                        c.kind = TeamCommand::Kind::MoveTeam;
                        c.x = w.x;
                        c.y = w.y;
                        team.move_issued = true;
                    } else if (all_within(view, w.x, w.y, kAiCloseEnoughCells)) {
                        ++team.script_pc;
                        team.move_issued = false;
                    }
                    return c;
                }
                case OpClass::Regroup: {
                    const Cellxy m = centroid(view);
                    if (!team.move_issued) {
                        c.kind = TeamCommand::Kind::MoveTeam;
                        c.x = m.x;
                        c.y = m.y;
                        team.move_issued = true;
                    } else if (all_within(view, m.x, m.y, kAiCloseEnoughCells)) {
                        ++team.script_pc;
                        team.move_issued = false;
                    }
                    return c;
                }
                case OpClass::GuardArea: {
                    // 0x0c guard area at current location: set the guard anchors
                    // then advance (the EXISTING auto-acquire / return-to-anchor
                    // machinery does the rest). One-shot so an untimed guard can
                    // never wedge the script [RE-GATE APPLIED: ai-team.md §3].
                    const Cellxy m = centroid(view);
                    c.kind = TeamCommand::Kind::GuardArea;
                    c.x = m.x;
                    c.y = m.y;
                    c.radius = kAiGatherRadius;
                    ++team.script_pc;
                    team.move_issued = false;
                    return c;
                }
                case OpClass::TimedGuard: {
                    // 0x05 / 0x3a guard for <arg> tenths-of-min; arg x 90 frames
                    // at 15 Hz [RE-GATE APPLIED: ai-team.md §3].
                    team.script_timer += dt * 15.0;  // accumulate FRAMES
                    if (team.script_timer >=
                        (double)(a.argument * kAiGuardTicksToFrames)) {
                        ++team.script_pc;
                        team.script_timer = 0.0;
                        team.move_issued = false;
                        return c;
                    }
                    const Cellxy m = centroid(view);
                    c.kind = TeamCommand::Kind::GuardArea;
                    c.x = m.x;
                    c.y = m.y;
                    c.radius = kAiGatherRadius;
                    return c;
                }
                case OpClass::Jump: {
                    // 0x06 Jump consumes the tick — a jump cycle can never spin
                    // within one tick; argument is the 0-based script line index
                    // [RE-GATE APPLIED: ai-team.md §3 — CONFIRMED].
                    team.script_pc =
                        std::max(0, std::min(a.argument, (int)sc.actions.size()));
                    team.script_timer = 0.0;
                    team.move_issued = false;
                    return c;
                }
                case OpClass::Scatter: {
                    const Cellxy m = centroid(view);
                    c.kind = TeamCommand::Kind::MoveTeam;
                    c.x = m.x;
                    c.y = m.y;
                    c.scatter = true;  // caller spreads members [RE: ai-team.md §3]
                    ++team.script_pc;  // one-shot
                    team.move_issued = false;
                    return c;
                }
                case OpClass::Disband: {
                    // 0x31 (49): set success flag then disband [RE: ai-team.md §3].
                    team.state = TeamState::Disbanding;
                    return c;
                }
                case OpClass::Inert:
                default: {
                    // Valid-but-unimplementable / unrecognised opcode: logged
                    // skip, pc++ [RE-GATE APPLIED: ai-team.md §3; spec §2].
                    c.kind = TeamCommand::Kind::Inert;
                    c.opcode = a.action;
                    ++team.script_pc;
                    team.move_issued = false;
                    return c;
                }
            }
        }

        case TeamState::Disbanding:
        default:
            return c;  // caller releases survivors + erases the instance
    }
}

int team_shortfall_type(const TeamInstance& team, const AimdData& aimd) {
    // First unfilled TaskForce slot in FILE ORDER [RE: ai-team.md §2] —
    // count claimed members of each required type against tf.members.
    if (team.teamtype < 0 || team.teamtype >= (int)aimd.teams.size()) return -1;
    const TaskForce& tf = aimd.taskforces[aimd.teams[team.teamtype].taskforce];
    for (const TaskForceMember& m : tf.members) {
        int have = 0;
        for (int t : team.member_types)
            if (t == m.unit) ++have;
        if (have < m.count) return m.unit;
    }
    return -1;
}

}  // namespace ra2engine
