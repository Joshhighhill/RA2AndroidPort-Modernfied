#include "ai_base.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace ra2engine {

std::uint32_t ai_rand(std::uint32_t& s) {
    // xorshift32 [RE-FALLBACK: ai-base.md deviations — engine-local RNG for
    // any adjudicated tie-break; deterministic, never wall-clock].
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

DifficultyMods difficulty_mods(AiDifficulty d, const DifficultyGroupRaw (&groups)[3]) {
    const int i = static_cast<int>(d);
    DifficultyMods m;
    m.cost_mul = groups[i].cost;             // [RE: ai-base.md §4]
    m.build_time_mul = groups[i].build_time; // [RE: ai-base.md §4]
    m.build_delay_min = groups[i].build_delay;   // minutes [RE: ai-base.md §4]
    m.build_slowdown = groups[i].build_slowdown; // [RE: ai-base.md §4]
    m.defense_target = kAiDefenseTarget[i];  // [RE: ai-base.md §2; RE-FALLBACK index binding]
    return m;
}

namespace {
long long dist2(int ax, int ay, int bx, int by) {
    const long long dx = ax - bx, dy = ay - by;
    return dx * dx + dy * dy;
}
}  // namespace

Cellxy find_build_location(const BaseView& view, bool is_defense,
                           const std::function<bool(int, int)>& can_place) {
    // [RE-GATE APPLIED: ai-base.md §3 — the exact outward scan was NOT
    // pinned; this nearest-first Chebyshev ring from the base center,
    // row-major (dy then dx ascending) within a ring, stands as the
    // CALIBRATED RE-FALLBACK. Deterministic; foundation/overlap enforced by
    // can_place — the shared per-cell buildable/unoccupied grid check with
    // the AIBaseSpacing(1) gap. The AI BYPASSES the Adjacent= proximity
    // check (FUN_004a8dc0 early-return for non-local-human houses), so
    // can_place carries NO adjacency requirement. Defenses probe each ring's
    // cells nearest the THREAT first (perimeter-facing bias; defense/wall
    // cells derive from foundation-perimeter rings around the base) with the
    // row-major order as the stable tie-break. BaseBias is NOT used here —
    // it is a threat-TARGETING multiplier [RE: ai-base.md §2].]
    const int ox = view.center_x, oy = view.center_y;
    for (int r = 0; r <= kAiMaxScanRadius; ++r) {
        std::vector<Cellxy> ring;
        ring.reserve(r == 0 ? 1 : 8 * r);
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                ring.push_back(Cellxy{ox + dx, oy + dy});
            }
        if (is_defense) {
            std::stable_sort(ring.begin(), ring.end(), [&](const Cellxy& a, const Cellxy& b) {
                return dist2(a.x, a.y, view.threat_x, view.threat_y) <
                       dist2(b.x, b.y, view.threat_x, view.threat_y);
            });
        }
        for (const Cellxy& c : ring)
            if (can_place(c.x, c.y)) return c;
    }
    return Cellxy{-1, -1};  // boxed in / nothing within the cap -> caller holds
}

namespace {
int count_alive(const BaseView& v, int type) {
    if (type < 0) return 0;
    int n = 0;
    for (const BaseBuildingView& b : v.own)
        if (b.type == type) ++n;
    return n;
}
// Affordability [RE-GATE APPLIED: ai-base.md §2 — the START is gated on
// credits >= cost x difficulty cost_mul + CreditReserve(100, kept banked);
// unaffordable -> WAIT/bank, never a cheaper substitute].
BuildDecision pick_node(const std::vector<PlannedNode>& plan, int i,
                        const BaseView& v,
                        const std::vector<BuildingTypeDef>& roster,
                        const AiRules& ai, const DifficultyMods& m) {
    BuildDecision d;
    const int type = plan[i].type;
    if (type < 0 || type >= (int)roster.size()) return d;  // marker/gap: no-op
    if (v.credits < roster[type].cost * m.cost_mul + ai.credit_reserve) {
        d.wait = true;  // bank income [RE: ai-base.md §2 — CreditReserve]
        return d;
    }
    d.type = type;
    d.node = i;
    return d;
}
}  // namespace

std::vector<PlannedNode> build_base_plan(const BaseView& view, const AiCatalog& cat,
                                         const std::vector<BuildingTypeDef>& roster,
                                         const AiRules& ai, const DifficultyMods& mods) {
    // [RE-GATE APPLIED: ai-base.md §2 — FUN_005052d0's static order. A
    // catalog gap (-1: art missing / not in this install) is SKIPPED —
    // art-guard-drop, never a crash. `view` is reserved for the prereq-
    // sorted-remainder step of later sub-phases (AI-1's remainder is the
    // single radar entry).]
    (void)view;
    std::vector<PlannedNode> plan;
    auto push = [&](int t) {
        if (t < 0 || t >= (int)roster.size()) return;
        PlannedNode n;
        n.type = t;
        plan.push_back(n);
    };
    push(cat.conyard);      // 1) ConYard      (BuildConst)   [RE: §2]
    push(cat.power);        // 2) Power        (BuildPower)   [RE: §2]
    push(cat.barracks);     // 3) Barracks     (BuildBarracks — hoisted) [RE: §2]
    push(cat.war_factory);  // 4) War Factory  (BuildWeapons — hoisted)  [RE: §2]
    push(cat.radar);        // 5) prereq-sorted remainder (AI-1: BuildRadar) [RE: §2]
    for (int i = 0; i < ai.refinery_limit; ++i)  // 6) Refineries x RefineryLimit(4) [RE: §2]
        push(cat.refinery);
    const int defs = std::min(mods.defense_target, ai.defense_limit);
    if (!cat.defenses.empty())
        for (int i = 0; i < defs; ++i)  // 7) defenses x per-difficulty count
                                        //    [RE: §2; RE-FALLBACK: index binding],
                                        //    cycling the side list deterministically
                                        //    (stands in for the AIForcePrediction
                                        //    weighting [RE-FALLBACK: ai-base.md §2])
            push(cat.defenses[i % (int)cat.defenses.size()]);
    if (cat.wall >= 0 && !cat.defenses.empty())
        for (int i = 0; i < defs; ++i)  // 8) walls (ConcreteWalls) at the tail —
                                        //    cells resolve at placement from the
                                        //    defense foundation perimeter
                                        //    (FUN_0050c160) [RE: §2/§3; RE-FALLBACK:
                                        //    one perimeter node per defense in AI-1]
            push(cat.wall);
    // NOTE: the static plan is NOT trimmed by BaseSizeAdd — the cap gates
    // DYNAMIC expansion (next_build) at rival + BaseSizeAdd(3) [RE: §2].
    return plan;
}

BuildDecision next_build(AiBaseState& state, const BaseView& view,
                         const AiCatalog& cat,
                         const std::vector<BuildingTypeDef>& roster,
                         const AiRules& ai, const DifficultyMods& mods) {
    // Lazy plan build — the original builds the node vector iff empty
    // (FUN_0050a3e0 -> FUN_005052d0) [RE: ai-base.md §2/§3].
    if (state.plan.empty())
        state.plan = build_base_plan(view, cat, roster, ai, mods);

    // PLAN WALK [RE-GATE APPLIED: ai-base.md §1/§2/§5 — FUN_004fe200]: the
    // first UNSATISFIED node in plan order, ONE structure at a time (the
    // single Structure-queue slot / house+0x564c gate lives with the
    // caller). Satisfaction is existence-based: each node is covered by one
    // standing own building of its type; a DEAD node is unsatisfied again
    // at its ORIGINAL position -> rebuild-before-tail-expansion [RE: §5].
    std::vector<int> claimed(roster.size(), 0);
    for (int i = 0; i < (int)state.plan.size(); ++i) {
        const PlannedNode& n = state.plan[i];
        if (n.type < 0 || n.type >= (int)roster.size()) continue;  // gap/marker
        if (!n.dead && claimed[n.type] < count_alive(view, n.type)) {
            ++claimed[n.type];  // satisfied by a standing building
            continue;
        }
        // POWER PREEMPTION [RE-GATE APPLIED: ai-base.md §1/§2 —
        // FUN_004fe200 step 4]: before a drainer that would push
        // drain > output - PowerSurplus(50), INSERT a Power node ahead of
        // it. PowerSurplus (the reserve), this gate, and
        // PowerEmergency=0.75 (a SELL trigger — recorded in kAiPowerEmergency,
        // NOT consumed by AI-1) are THREE DISTINCT rules [RE: §2].
        const BuildingTypeDef& t = roster[n.type];
        if (t.power < 0 && cat.power >= 0 && n.type != cat.power &&
            view.power_drain + (-t.power) >
                view.power_output - (int)ai.power_surplus) {
            PlannedNode p;
            p.type = cat.power;
            state.plan.insert(state.plan.begin() + i, p);
            return pick_node(state.plan, i, view, roster, ai, mods);
        }
        return pick_node(state.plan, i, view, roster, ai, mods);
    }

    // ECONOMY EXPANSION [RE-GATE APPLIED: ai-base.md §2 — FUN_004fdb30]: a
    // SEPARATE trigger once the plan is satisfied — APPEND an extra
    // refinery (then war-factory) node when under its Ratio/Limit and
    // affordable. Opportunistic: unaffordable -> no wait, just bank.
    // BaseSizeAdd caps the whole base at rival + BaseSizeAdd(3) [RE: §2].
    if ((int)view.own.size() >= view.rival_base_size + ai.base_size_add)
        return BuildDecision{};  // base big enough: build nothing
    const int expand[2] = {cat.refinery, cat.war_factory};
    const int limit[2] = {ai.refinery_limit, ai.war_limit};
    for (int e = 0; e < 2; ++e) {
        const int t = expand[e];
        if (t < 0 || count_alive(view, t) >= limit[e]) continue;
        if (view.credits < roster[t].cost * mods.cost_mul + ai.credit_reserve)
            continue;  // opportunistic — no wait
        PlannedNode n;
        n.type = t;
        state.plan.push_back(n);
        BuildDecision d;
        d.type = t;
        d.node = (int)state.plan.size() - 1;
        return d;
    }
    return BuildDecision{};  // plan satisfied, limits met: build nothing
}

}  // namespace ra2engine
