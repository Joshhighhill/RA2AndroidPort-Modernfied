#pragma once
// Pure construction economy: the single BuildingType queue, credits, power.
// Calibration: docs/re/construction.md (§ refs inline). No SDL/OS.
#include <string>
#include <vector>
#include "building.hpp"

namespace ra2engine {

// [RE: construction.md §7] gamemd computes ftol(Cost x BuildSpeed x 0.9) in
// EXACTLY that operation order (FUN_00711280). Do NOT algebraically
// rearrange to /1000*900: IEEE rounding differs (503/1889 instead of the
// recovered 504/1890 pins for GAPOWR/GACNST).
int build_frames(int cost, double build_speed);   // = (int)(cost * build_speed * 0.9)

// Power totals over ALIVE, non-Dead buildings of `house`. Buildings still in
// Buildup do NOT contribute output or drain [RE: construction.md §8,
// pre-hunt: power applies when operational].
struct PowerTotals { int output = 0; int drain = 0; };   // drain stored positive
PowerTotals power_totals(const std::vector<Building>& buildings,
                         const std::vector<BuildingTypeDef>& types, int house);

// [RE-GATE APPLIED: construction.md §8] low power = drain > output. Recovered
// rule (FUN_004FCC50 + the production-speed chain): with f = output/drain,
//   speed = 1 - low_power_penalty_modifier * (1 - f)        (0 output -> 0)
//   speed = max(speed, kMinLowPowerSpeed)                    (= .5, rulesmd)
//   speed = min(speed, kMaxLowPowerSpeed)   [low power only] (= .8, rulesmd)
// Full power -> exactly 1.0. With modifier=1 and any shortage, the result is
// therefore always in [.5, .8].
constexpr double kMinLowPowerSpeed = 0.5;   // rulesmd MinLowPowerProductionSpeed
constexpr double kMaxLowPowerSpeed = 0.8;   // rulesmd MaxLowPowerProductionSpeed
double build_speed_multiplier(const PowerTotals& p, double low_power_penalty_modifier);

// One-slot BuildingType queue [RE-GATE APPLIED: construction.md §10 —
// FactoryClass's 54-step incremental economy]. Production advances in
// exactly kProductionSteps stages; each stage lasts `step_ticks` 15 Hz
// frames (= clamp(trunc(total_frames/54), 1, 255)); entering stage s+1
// charges `(cost - spent) / (54 - s)` credits — equal cost/54 chunks under
// uninterrupted funding. When a stage's charge is unaffordable the build
// STALLS (no progress, no charge, retried every tick).
constexpr int kProductionSteps = 54;
struct QueueSlot {
    int type = -1;              // roster index; -1 = idle
    int stage = 0;              // 0..kProductionSteps
    int step_ticks = 1;         // 15 Hz frames per stage
    double frames_accum = 0.0;  // fractional frames toward the next stage
    int total_frames = 1;       // Time_To_Build (the 504-frame pin) — display/witness
    double spent = 0.0;         // credits deducted so far
    bool ready = false;         // finished, waiting for placement
    // Effective per-build charge total [RE-GATE APPLIED: economy.md §6a].
    // Resolved ONCE by start(): types[type].charge when the caller set one
    // (> 0 — e.g. GAREFN/NAREFN's FreeUnit-subtracted 600), else
    // types[type].cost (every pre-economy-phase caller/test that never
    // touches BuildingTypeDef::charge, which defaults to 0). tick()'s
    // per-stage charge math and its final settle-to-exact-total line both
    // read THIS field, never types[...].cost/.charge directly — `types` is a
    // const ref shared across every build of that type, so resolving the
    // "charge unset -> use cost" fallback into the per-slot state here
    // (instead of mutating the roster) keeps concurrent/repeat builds of the
    // same type independent. build_frames (wall-clock build TIME) is
    // unaffected — it stays on raw cost, computed once in start() below.
    int charge_basis = 0;
};

struct ProductionState {
    double credits = 0.0;
    QueueSlot slot;
};

// Prerequisite check [RE: construction.md §10]: every token must be satisfied.
// A plain token = an ALIVE, IDLE (fully built) building of that id owned by
// `house` exists. Group tokens (POWER/BARRACKS/PROC/FACTORY/RADAR/TECH) =
// any id in `groups[token]` satisfied the same way. Unknown tokens fail.
bool prerequisites_met(const std::vector<std::string>& tokens,
                       const std::vector<Building>& buildings,
                       const std::vector<BuildingTypeDef>& types, int house,
                       const std::vector<std::pair<std::string, std::vector<std::string>>>& groups);

// Can `type` start building? conyard_alive = an own alive Idle ConYard exists
// (the BuildingType factory); slot must be idle; prereqs met; TechLevel > 0.
// Affordability is NOT gated here — RA2 starts a build with any credits and
// stalls at 0 [RE: construction.md §10].
bool can_start(const ProductionState& ps, int type,
               const std::vector<BuildingTypeDef>& types,
               const std::vector<Building>& buildings, int house,
               const std::vector<std::pair<std::string, std::vector<std::string>>>& groups,
               bool conyard_alive);

// Start building `type` (caller checked can_start): total_frames =
// build_frames(cost, build_speed); step_ticks = clamp(total_frames /
// kProductionSteps, 1, 255) (integer division = the binary's trunc)
// [RE-GATE: construction.md §10 FUN_004C9DC0]. GAPOWR: 504 -> step_ticks 9
// (wall duration 54*9 = 486 frames = 32.4 s). Also resolves
// QueueSlot::charge_basis = types[type].charge > 0 ? types[type].charge :
// types[type].cost [RE-GATE: economy.md §6a] — the "charge unset" fallback;
// see QueueSlot's comment.
void start(ProductionState& ps, int type, const std::vector<BuildingTypeDef>& types,
           double build_speed);

// Advance by dt seconds [RE: construction.md §10; charge basis vs. cost —
// economy.md §6a]:
//   frames_accum += dt * 15 * speed_mult;
//   while (frames_accum >= step_ticks && stage < kProductionSteps):
//     charge = (charge_basis - spent) / (kProductionSteps - stage);
//     if (credits < charge) { frames_accum = 0.0; break; }  // stall
//     credits -= charge; spent += charge; ++stage; frames_accum -= step_ticks;
//   stage == kProductionSteps -> spent settles to exactly charge_basis, ready = true.
// charge_basis (see QueueSlot) is cost for every type with no FreeUnit, and
// the caller's FreeUnit-subtracted amount otherwise — see start() below.
// The stall reset (frames_accum = 0.0) models the original's free-running
// stage timer, which restarts after a broke expiry (rolled-back stage,
// retried on the NEXT timer expiry) — a funding gap adds 0..step_ticks
// latency and never produces a catch-up burst. No-op when idle/ready.
void tick(ProductionState& ps, double dt, double speed_mult,
          const std::vector<BuildingTypeDef>& types);

// Cancel: refund everything spent so far, clear the slot
// [RE-GATE: construction.md §10 — Abandon refunds Cost_Of minus balance,
// i.e. ALL spent].
void cancel(ProductionState& ps);

// Placement succeeded: clear the slot (credits already fully spent).
void placed(ProductionState& ps);

}  // namespace ra2engine
