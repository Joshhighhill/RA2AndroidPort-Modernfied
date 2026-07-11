#include "production.hpp"
#include <algorithm>
#include <cmath>

namespace ra2engine {

int build_frames(int cost, double build_speed) {
    // [RE: construction.md §7, FUN_00711280] Binary operation order:
    // ftol(Cost * BuildSpeed * 0.9). Do NOT algebraically rearrange —
    // /1000*900 rounds differently under IEEE 754 (503 vs 504 for GAPOWR:
    // 800*0.7 = exactly 560.0, *0.9 = exactly 504.0).
    return (int)(cost * build_speed * 0.9);
}

PowerTotals power_totals(const std::vector<Building>& buildings,
                         const std::vector<BuildingTypeDef>& types, int house) {
    PowerTotals p;
    for (const Building& b : buildings) {
        if (!b.alive || b.house != house || b.state != Building::State::Idle) continue;
        if (b.type < 0 || b.type >= (int)types.size()) continue;
        int pw = types[b.type].power;
        if (pw >= 0) p.output += pw;
        else p.drain -= pw;
    }
    return p;
}

double build_speed_multiplier(const PowerTotals& p, double low_power_penalty_modifier) {
    if (p.drain <= p.output) return 1.0;
    // [RE-GATE: construction.md §8] f = output/drain; 1 - mod*(1-f), then
    // floor .5 and (low power only) cap .8.
    double f = (double)p.output / p.drain;
    double s = 1.0 - low_power_penalty_modifier * (1.0 - f);
    s = std::max(s, kMinLowPowerSpeed);
    s = std::min(s, kMaxLowPowerSpeed);
    return s;
}

bool prerequisites_met(const std::vector<std::string>& tokens,
                       const std::vector<Building>& buildings,
                       const std::vector<BuildingTypeDef>& types, int house,
                       const std::vector<std::pair<std::string, std::vector<std::string>>>& groups) {
    auto have = [&](const std::string& id) {
        for (const Building& b : buildings) {
            if (!b.alive || b.house != house || b.state != Building::State::Idle) continue;
            if (b.type >= 0 && b.type < (int)types.size() && types[b.type].id == id) return true;
        }
        return false;
    };
    for (const std::string& tok : tokens) {
        bool ok = have(tok);
        if (!ok) {
            for (const auto& g : groups) {
                if (g.first != tok) continue;
                for (const std::string& id : g.second)
                    if (have(id)) { ok = true; break; }
                break;
            }
        }
        if (!ok) return false;
    }
    return true;
}

bool can_start(const ProductionState& ps, int type,
               const std::vector<BuildingTypeDef>& types,
               const std::vector<Building>& buildings, int house,
               const std::vector<std::pair<std::string, std::vector<std::string>>>& groups,
               bool conyard_alive) {
    if (type < 0 || type >= (int)types.size()) return false;
    if (!conyard_alive || ps.slot.type != -1) return false;
    if (types[type].tech_level <= 0) return false;       // -1 = never buildable
    return prerequisites_met(types[type].prerequisite, buildings, types, house, groups);
}

void start(ProductionState& ps, int type, const std::vector<BuildingTypeDef>& types,
           double build_speed) {
    ps.slot = QueueSlot{};
    ps.slot.type = type;
    const BuildingTypeDef& t = types[type];
    ps.slot.total_frames = std::max(1, build_frames(t.cost, build_speed));
    // [RE-GATE: construction.md §10 FUN_004C9DC0] frames per production step.
    ps.slot.step_ticks = std::clamp(ps.slot.total_frames / kProductionSteps, 1, 255);
    // [RE-GATE: economy.md §6a] charge basis: the caller's FreeUnit-subtracted
    // amount when set (> 0), else raw cost — resolved once here so tick()
    // never has to special-case an unset BuildingTypeDef::charge (see
    // QueueSlot::charge_basis).
    ps.slot.charge_basis = t.charge > 0 ? t.charge : t.cost;
}

void tick(ProductionState& ps, double dt, double speed_mult,
          const std::vector<BuildingTypeDef>& types) {
    if (ps.slot.type == -1 || ps.slot.ready) return;
    (void)types;  // charge basis was already resolved into ps.slot.charge_basis by start()
    ps.slot.frames_accum += dt * 15.0 * speed_mult;
    while (ps.slot.frames_accum >= ps.slot.step_ticks &&
           ps.slot.stage < kProductionSteps) {
        // [RE-GATE: construction.md §10; charge basis economy.md §6a]
        // entering the next stage charges the remaining balance (against
        // charge_basis, NOT raw cost) split over the remaining steps.
        double charge = ((double)ps.slot.charge_basis - ps.slot.spent) /
                        (kProductionSteps - ps.slot.stage);
        if (ps.credits < charge) {
            // Stall: no progress, no charge; retried next tick. Reset the
            // accumulator so a funding gap never produces a catch-up burst.
            ps.slot.frames_accum = 0.0;
            return;
        }
        ps.credits -= charge;
        ps.slot.spent += charge;
        ps.slot.stage += 1;
        ps.slot.frames_accum -= ps.slot.step_ticks;
    }
    if (ps.slot.stage >= kProductionSteps) {
        // settle rounding: total spend is exactly charge_basis
        ps.credits -= (double)ps.slot.charge_basis - ps.slot.spent;
        ps.slot.spent = (double)ps.slot.charge_basis;
        ps.slot.ready = true;
    }
}

void cancel(ProductionState& ps) {
    ps.credits += ps.slot.spent;                          // [RE: construction.md §10]
    ps.slot = QueueSlot{};
}

void placed(ProductionState& ps) { ps.slot = QueueSlot{}; }

}  // namespace ra2engine
