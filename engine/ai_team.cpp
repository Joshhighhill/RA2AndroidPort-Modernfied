#include "ai_team.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
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
            // dissolve_frames (AiTeamRules) mirrors this constant for a modded
            // [General] DissolveUnfilledTeamDelay= override, but step_team is a
            // pure lifecycle that only takes the scalars it needs (multiplay
            // here) — it has no AiTeamRules/config reach, and no other call
            // site consumes dissolve_frames either, so it stays intentionally
            // constant-backed rather than widening this MP-only signature.
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
                        c.x = w.x;                 // IDEAL cell (caller snaps it)
                        c.y = w.y;
                        team.move_issued = true;
                    } else if (all_within(view, team.gather_x, team.gather_y,
                                          kAiCloseEnoughCells)) {
                        // Arrival is measured against the REALIZED focus the
                        // caller stored (the snapped cell members were actually
                        // sent to), NOT the ideal `w` whose CloseEnough
                        // neighborhood may be blocked/occupied — matching the
                        // original's Focus (team+0x40) semantics and making the
                        // predicate satisfiable [FIX: Executing-move wedge; §2].
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
                    } else if (all_within(view, team.gather_x, team.gather_y,
                                          kAiCloseEnoughCells)) {
                        ++team.script_pc;          // arrival vs REALIZED focus [FIX]
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
                    } else if (all_within(view, team.gather_x, team.gather_y,
                                          kAiCloseEnoughCells)) {
                        ++team.script_pc;          // arrival vs REALIZED focus [FIX]
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

namespace {
// Optional country restriction ONLY [RE-GATE APPLIED: ai-team.md §1/§4]: stock
// data is universally House=<none>, so this returns true for "<none>"/"" (any
// house) — it is NOT the eligibility gate (fielding the TaskForce is, guaranteed
// upstream by parse_aimd's art-guard-drop). A rare House=<name> team restricts
// to that country (case-insensitive).
bool house_matches(const std::string& team_house, const std::string& house_name) {
    if (team_house.empty()) return true;
    std::string a = team_house, b = house_name;
    for (char& c : a) c = (char)std::toupper((unsigned char)c);
    for (char& c : b) c = (char)std::toupper((unsigned char)c);
    return a == "<NONE>" || a == b;
}
}  // namespace

int select_autocreate_team(const AimdData& aimd, const std::string& house_name,
                           const std::vector<TeamInstance>& live, double credits,
                           const AiTeamRules& rules, std::uint32_t& rng) {
    // [RE-GATE APPLIED: ai-team.md §4] autocreate ONLY (AITriggerTypes creation
    // is AI-3); NOT is_base_defense (defensive teams out of offensive scope);
    // the TotalAITeamCap hard per-house cap; the Max live-instance cap with
    // Max=0 = UNCAPPED; the CreditReserve spend floor; highest Priority is
    // strictly preferred; the equal-highest-priority tie is broken by a SEEDED
    // ai_rand draw among the tied eligible candidates [RE-FALLBACK: ai-team.md §4
    // — all stock offensive teams are Priority=5, so ties are the common case and
    // the enemy must vary its attack-wave composition; deterministic, NEVER
    // wall-clock]. Eligibility is NOT a per-side house-token compare —
    // house_matches only applies a rare House=<name> country restriction;
    // fielding the TaskForce is guaranteed by parse_aimd's art-guard-drop (a team
    // in `aimd` has a TaskForce this house can field).
    if (credits < rules.credit_reserve) return -1;
    if ((int)live.size() >= rules.total_team_cap) return -1;  // hard per-house cap
    // Pass 1: find the highest Priority among all eligible teams.
    int best_priority = 0;
    bool any = false;
    for (int i = 0; i < (int)aimd.teams.size(); ++i) {
        const TeamType& tt = aimd.teams[i];
        if (!tt.autocreate || tt.is_base_defense ||
            !house_matches(tt.house, house_name))
            continue;
        int count = 0;
        for (const TeamInstance& t : live)
            if (t.teamtype == i) ++count;
        if (tt.max > 0 && count >= tt.max) continue;  // Max=0 -> uncapped
        if (!any || tt.priority > best_priority) {
            best_priority = tt.priority;
            any = true;
        }
    }
    if (!any) return -1;
    // Pass 2: collect every eligible team at the highest Priority (file order).
    std::vector<int> candidates;
    for (int i = 0; i < (int)aimd.teams.size(); ++i) {
        const TeamType& tt = aimd.teams[i];
        if (!tt.autocreate || tt.is_base_defense ||
            !house_matches(tt.house, house_name))
            continue;
        int count = 0;
        for (const TeamInstance& t : live)
            if (t.teamtype == i) ++count;
        if (tt.max > 0 && count >= tt.max) continue;  // Max=0 -> uncapped
        if (tt.priority == best_priority) candidates.push_back(i);
    }
    // Seeded tie-break among the equal-highest-priority eligible candidates
    // [RE-FALLBACK: ai-team.md §4]. A single candidate still draws once so the
    // RNG stream advances identically whether or not a tie exists.
    return candidates[ai_rand(rng) % (std::uint32_t)candidates.size()];
}

int recruit_idle(TeamInstance& team, const AimdData& aimd,
                 const std::vector<IdleUnitView>& idle) {
    // [RE: ai-team.md §2] recruit-idle-first: TaskForce slots in FILE ORDER,
    // ascending idle-unit walk per slot (the caller supplies `idle` in
    // ascending unit-key order) — mirrors AI-1's deterministic
    // standing-building sync. Each idle unit claimed at most once per call.
    if (team.teamtype < 0 || team.teamtype >= (int)aimd.teams.size()) return 0;
    const TaskForce& tf = aimd.taskforces[aimd.teams[team.teamtype].taskforce];
    std::vector<char> taken(idle.size(), 0);
    int claimed = 0;
    for (const TaskForceMember& m : tf.members) {
        int have = 0;
        for (int t : team.member_types)
            if (t == m.unit) ++have;
        for (std::size_t i = 0; i < idle.size() && have < m.count; ++i) {
            if (taken[i] || idle[i].unit_type != m.unit) continue;
            taken[i] = 1;
            team.members.push_back(idle[i].unit);
            team.member_types.push_back(m.unit);
            ++have;
            ++claimed;
        }
    }
    return claimed;
}

int next_produce_type(const std::vector<TeamInstance>& teams, const AimdData& aimd,
                      double credits, double cost_mul,
                      const std::vector<UnitTypeDef>& unit_roster,
                      const AiTeamRules& rules) {
    // [RE: ai-team.md §6] produce-the-shortfall: the FIRST (oldest)
    // Recruiting team's first unfilled slot drives production — one intent
    // per house tick; the caller routes infantry->tab2, vehicle->tab3 and
    // holds when that queue is busy (the single-slot queue IS the recovered
    // one-unit-at-a-time factory gate). Affordability gates the START:
    // credits >= cost x cost_mul + CreditReserve — bank, never substitute
    // (the AI-1 rule). INFANTRY shortfall is produced FREELY above
    // InfantryReserve (50000); otherwise it falls to the normal check. VEHICLE
    // shortfall always uses the normal check [RE-GATE APPLIED: ai-team.md §6].
    for (const TeamInstance& team : teams) {
        if (team.state != TeamState::Recruiting) continue;
        const int t = team_shortfall_type(team, aimd);
        if (t < 0 || t >= (int)unit_roster.size()) continue;
        const bool infantry = !unit_roster[t].is_vehicle;
        if (infantry && credits > rules.infantry_reserve) return t;
        if (credits < unit_roster[t].cost * cost_mul + rules.credit_reserve)
            return -1;  // bank income; NEVER a cheaper substitute
        return t;
    }
    return -1;
}

bool build_slowdown_blocks(const AiTeamRules& rules, bool category_has_queued_or_onhand) {
    // BuildSlowdown is a difficulty BYTE (Easy off / Normal+Difficult on), an
    // anti-spam OVER-QUEUE damp, NOT a build-time multiplier [RE-GATE APPLIED:
    // ai-team.md §6]. When on, decline to START a category the house already has
    // queued or on-hand.
    return rules.build_slowdown && category_has_queued_or_onhand;
}

}  // namespace ra2engine
