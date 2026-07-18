#pragma once
// Pure offensive-team brain [RE: docs/re/ai-team.md]. No SDL, no World —
// consumes a read-only TeamView the caller (World) fills, plus per-house
// AiTeamState. doctest-testable. EVERY numeric constant below is a HUNT
// OUTPUT recorded in docs/re/ai-team.md; none is a guess — tests assert
// against the named constants so a RE-GATE amendment updates constant, doc,
// and test together. Reuses ai_base.hpp's ai_rand xorshift32 + the AI-1
// carry-forward constants (kAiSafeDistance/kAiCreditReserve/kAiInfantryReserve).
#include <cstdint>
#include <string>
#include <vector>
#include "ai_base.hpp"     // ai_rand, kAiSafeDistance, kAiCreditReserve, kAiInfantryReserve, AiDifficulty
#include "aimd_parse.hpp"  // AimdData / TeamType / TaskForce / ScriptType
#include "pathfind.hpp"    // Cellxy
#include "unit_type.hpp"   // UnitTypeDef (production intent)

namespace ra2engine {

// --- Recovered team-clock / lifecycle constants [RE-GATE APPLIED: ai-team.md
// §2/§4 — the hunt PINNED the real values and clarified the two-clock model:
// TeamDelays (frames, per-difficulty) gates the team-management CHECK pass;
// AutocreateTime (minutes) gates the actual autocreate SPAWN] ---
// [AI] TeamDelays= (Rules+0x1158) frames between team-management CHECK passes,
// indexed by AiDifficulty (Easy/Normal/Difficult). Recovered file order is
// 2000,2500,3500; the index binding carries the SAME hardest<->easiest caveat
// as ai-base.md's count arrays [RE-FALLBACK: index 0/1/2 as loaded, default the
// middle 2500] [RE-GATE APPLIED: ai-team.md §4].
constexpr int kAiTeamDelayFrames[3] = {2000, 2500, 3500};
// [AI] AutocreateTime= (Rules+0x1510, double MINUTES) — average minutes between
// creating an autocreate team; gates the actual SPAWN (the global scheduler
// latch) [RE-GATE APPLIED: ai-team.md §4]. 1 min = 900 frames at 15 Hz.
constexpr double kAiAutocreateTimeMin = 1.0;
// [General] TotalAITeamCap= (Rules+0x13c8) hard cap on simultaneous AI teams
// per house [RE-GATE APPLIED: ai-team.md §4].
constexpr int kAiTotalTeamCap = 30;
// [AI] DissolveUnfilledTeamDelay= (Rules+0x1190) frames an unfilled team waits
// before disbanding — rulesmd tags it MULTIPLAY ONLY, so the default skirmish/
// campaign path does NOT auto-dissolve on this timer (an unfilled Full=no team
// keeps recruiting/producing) [RE-GATE APPLIED: ai-team.md §2].
constexpr int kAiDissolveUnfilledDelayFrames = 5000;
// GuardArea radius handed to the caller (guard anchor spread), cells
// [RE-FALLBACK: engine bound, not a binary constant].
constexpr int kAiGatherRadius = 3;
// "Gathered" / arrival predicate radii [RE-GATE APPLIED: ai-team.md §2 — the
// recovered rulesmd values]: a team is GATHERED when all live members are
// within Rules.Stray (+0x171c = 2.0 cells) of the gather focus; a move ARRIVES
// when all live members are within Rules.CloseEnough (+0x1718 = 2.25 cells).
constexpr double kAiStrayCells = 2.0;
constexpr double kAiCloseEnoughCells = 2.25;
// Timed-guard argument unit: actions 5/0x3a take a duration in TENTHS OF A
// MINUTE; converted to frames at 15 Hz (1/10 min = 6 s = 90 frames)
// [RE-GATE APPLIED: ai-team.md §3 — RE-FALLBACK x90 frames].
constexpr int kAiGuardTicksToFrames = 90;

// --- ScriptType opcode numbers [RE-GATE APPLIED: ai-team.md §3 — the classifier
// FUN_006e30d0 enumerates ~90 valid actions (up to 0x90) via a jump table;
// classify_op below is the single table location and marks the IMPLEMENTABLE
// core, everything else -> Inert]. The recovered IMPLEMENTABLE set for AI-2: ---
constexpr int kOpAttackQuarry = 0;      // 0x00 attack quarry kind <argument> (§5)
constexpr int kOpTimedGuard = 5;        // 0x05 timed guard <arg> tenths-of-min
constexpr int kOpJumpToLine = 6;        // 0x06 script_pc = <arg> (0-based line, CONFIRMED)
constexpr int kOpAttackHouse = 8;       // 0x08 all members attack house <arg>
constexpr int kOpMoveToWaypoint = 9;    // 0x09 move team to waypoint <arg>
constexpr int kOpMoveToCell = 11;       // 0x0b move team to packed cell <arg>
constexpr int kOpGuardArea = 12;        // 0x0c guard area at current location
constexpr int kOpRegroup = 14;          // 0x0e regroup at the team centroid
constexpr int kOpDisband = 49;          // 0x31 set success flag then disband
constexpr int kOpAttackBuilding = 53;   // 0x35 attack nearest enemy building
constexpr int kOpAttackUnits = 54;      // 0x36 attack enemy units near
constexpr int kOpScatter = 57;          // 0x39 scatter members (one-shot)
constexpr int kOpTimedGuardAlt = 58;    // 0x3a timed guard <arg> (same unit as 5)
constexpr int kOpAttackMove = 63;       // 0x3f attack quarry <arg> with regroup

// --- Quarry kinds (attack argument) [RE-GATE APPLIED: ai-team.md §5 — the
// STANDARD RA2 enumeration adopted as the calibrated RE-FALLBACK; target chosen
// by class-filter + distance^2 x value x seeded-RNG, nearest-reachable] ---
constexpr int kQuarryAnything = 0;
constexpr int kQuarryBuildings = 1;
constexpr int kQuarryHarvesters = 2;
constexpr int kQuarryInfantry = 3;
constexpr int kQuarryVehicles = 4;
constexpr int kQuarryFactories = 5;    // resolved as buildings pre-map [RE-FALLBACK]
constexpr int kQuarryDefenses = 6;     // resolved as buildings pre-map [RE-FALLBACK]
constexpr int kQuarryBaseThreats = 7;  // nearest hostile to own base [RE-FALLBACK]

// Opcode classes the interpreter dispatches on. classify_op maps EVERY action
// number to one of these; the ~90 valid-but-unimplementable actions (transport
// 0x2e/0x2f/0x50/0x6b, aircraft 0x1e/0x55-0x57, superweapon/support 0x3d/0x3e,
// droppod, cosmetics) and every action outside the recovered implementable core
// are Inert (logged skip, pc++) — parsed + RE-documented, never executed
// [RE-GATE APPLIED: ai-team.md §3; spec §2 scope bound].
enum class OpClass {
    AttackQuarry, AttackHouse, AttackBuilding, AttackUnits,
    MoveWaypoint, MoveCell, GuardArea, TimedGuard,
    Regroup, Scatter, Jump, Disband, Inert
};
OpClass classify_op(int action);  // [RE-GATE APPLIED: ai-team.md §3 — FULL table]

// Lifecycle [RE: ai-team.md §2]: Recruiting -> Gathering -> Executing ->
// Disbanding.
enum class TeamState { Recruiting = 0, Gathering = 1, Executing = 2, Disbanding = 3 };

// Read-only per-team snapshot, filled by the caller (World) each tick.
struct TeamMemberView {
    int unit = -1;       // caller's unit key (World unit index)
    bool alive = false;  // false = died this tick (caller prunes next tick)
    int cx = 0, cy = 0;  // current cell
    bool idle = false;   // not moving, no live attack pursuit
};
struct TeamView {
    std::vector<TeamMemberView> members;  // PARALLEL to TeamInstance::members
    std::vector<Cellxy> waypoints;        // waypoint index -> cell (caller table)
    int base_x = 0, base_y = 0;           // own base center (gather anchor)
    int enemy_x = 0, enemy_y = 0;         // enemy base centroid
    bool quarry_exists = false;           // a target matching the CURRENT attack
                                          // action's effective quarry class exists
                                          // (caller-computed; consulted on any of
                                          // AttackQuarry/House/Building/Units)
};

// One command per team per tick; the caller fans it out per member
// [RE: ai-team.md §3 — spec §3's opcode-class -> command table].
struct TeamCommand {
    enum class Kind { None, MoveTeam, AttackQuarry, GuardArea, Wait, Inert };
    Kind kind = Kind::None;
    int x = 0, y = 0;      // MoveTeam goal / GuardArea center
    int radius = 0;        // GuardArea radius (kAiGatherRadius)
    int quarry = 0;        // AttackQuarry kind [RE: ai-team.md §5]
    int opcode = -1;       // Inert: the skipped action number (witness logging)
    bool scatter = false;  // MoveTeam: spread members instead of one goal
};

// One live team [RE: ai-team.md §2]. members/member_types are maintained by
// the caller (claim on recruit/launch, erase on death) and stay PARALLEL.
struct TeamInstance {
    int teamtype = -1;               // AimdData::teams index
    TeamState state = TeamState::Recruiting;
    std::vector<int> members;        // caller unit keys (World indices)
    std::vector<int> member_types;   // unit-roster index per member (claim-stamped)
    int script_pc = 0;               // ScriptType action cursor [RE: ai-team.md §3]
    double script_timer = 0.0;       // dt-accumulated timer for timed actions
    double recruit_timer = 0.0;      // full=no timeout clock [RE: ai-team.md §2]
    int gather_x = 0, gather_y = 0;  // REALIZED move focus: the caller snaps the
                                     // ideal gather/move cell to the nearest free
                                     // cell and writes it back here, so the pure
                                     // arrival predicate is satisfiable [RE: §2].
    bool move_issued = false;        // the current move/gather order went out once
    int stall_reissues = 0;          // World-only backstop counter: consecutive
                                     // stalled re-issues of the CURRENT Executing
                                     // move before the pc is force-advanced (the
                                     // wedge-freedom guarantee) [RE: ai-team.md §2].
    bool completed_success = false;  // AI-3 [RE: ai-trigger.md §6]: set TRUE only
                                     // when the team runs its script's Disband
                                     // (kOpDisband) success action; stays FALSE
                                     // when it goes Disbanding via wipe (zero
                                     // members). Task 4 reads it at disband to
                                     // pick RegisterSuccess vs RegisterFailure
                                     // (adjust_weight). ADDITIVE — no AI-2
                                     // lifecycle transition changes.
};

// Per-house team state [RE: ai-team.md §2/§4]. `rng` is the engine-local
// seeded xorshift32 (ai_base's ai_rand) reserved for any hunt-recovered
// tie-break [RE-FALLBACK: ai-team.md deviations — deterministic, NEVER
// wall-clock]. teams_created feeds the gate hash + witnesses.
struct AiTeamState {
    std::vector<TeamInstance> teams;
    double check_clock = 0.0;  // secs since the last TeamDelays management pass [§4]
    double spawn_clock = 0.0;  // secs accrued toward the AutocreateTime spawn [§4]
    std::uint32_t rng = 0x2545F491u;  // deterministic fixed seed
    int teams_created = 0;     // lifetime counter (witness + hash)
};

// The team knobs, defaulting to the recovered constants (main.cpp re-reads
// them from the real rulesmd; tests use the defaults) [RE: ai-team.md §2/§4/§6].
struct AiTeamRules {
    int team_delay_frames = kAiTeamDelayFrames[1];          // Normal check pass [RE: §4]
    double autocreate_time_min = kAiAutocreateTimeMin;      // spawn cadence, minutes [RE: §4]
    int total_team_cap = kAiTotalTeamCap;                   // hard per-house cap [RE: §4]
    int dissolve_frames = kAiDissolveUnfilledDelayFrames;   // MP-only unfilled dissolve [RE: §2]
    bool multiplay = false;                                 // false = skirmish: NEVER dissolve on the timer [RE: §2]
    double credit_reserve = kAiCreditReserve;               // general spend floor (100) [RE: ai-base.md §2/§6]
    int infantry_reserve = kAiInfantryReserve;              // build infantry FREELY above this (50000) [RE-GATE: §6]
    bool build_slowdown = false;                            // difficulty BYTE: over-queue damp, NOT a time mul [RE-GATE: §6]
};

// Gather cell [RE-GATE APPLIED: ai-team.md §2 — the recovered rule]: a point on
// a circle of radius kAiSafeDistance (Rules.AISafeDistance = 20 cells) around the
// ENEMY/target base centre (ex,ey), at a SEEDED-xorshift32 angle drawn from
// `rng` (ai_base's ai_rand, substituting the lockstep RNG). The caller snaps the
// returned ideal ring cell to the nearest free cell via the existing move/path
// machinery (the original's FUN_0056d750 snap; [DEVIATION: engine snap]).
Cellxy gather_cell(int ex, int ey, std::uint32_t& rng);

// Advance `team` ONE tick [RE: ai-team.md §2/§3]: lifecycle transitions +
// the ScriptType interpreter (one action per tick; Jump consumes a tick, so
// jump cycles can never spin within a tick). Emits the single TeamCommand the
// caller executes this tick. `rng` (ai_base's ai_rand) draws the gather-ring
// angle and any hunt-recovered tie-break [DEVIATION: seeded xorshift32 for the
// lockstep RNG]. `multiplay` gates the DissolveUnfilledTeamDelay timer: false
// (skirmish/campaign default) NEVER dissolves an unfilled team on the timer; a
// Full=no team keeps recruiting/producing and launches as soon as it has any
// member [RE-GATE APPLIED: ai-team.md §2]. The caller prunes dead members BEFORE
// calling (members/member_types shrink); strength is read via
// team_shortfall_type + view.members.
TeamCommand step_team(TeamInstance& team, const TeamView& view,
                      const AimdData& aimd, std::uint32_t& rng, double dt,
                      bool multiplay = false);

// --- Task 4 (recruitment + production intent) — declared here, defined there ---

// First unfilled TaskForce slot's unit-roster type, walking tf.members in
// file order against the claimed member_types; -1 = team complete
// [RE: ai-team.md §2 recruit-vs-produce].
int team_shortfall_type(const TeamInstance& team, const AimdData& aimd);

// An idle own combat unit available for recruitment (caller-filled,
// ascending unit-key order).
struct IdleUnitView { int unit = -1; int unit_type = -1; };

// Recruit-idle-first [RE: ai-team.md §2]: walk the TaskForce members in file
// order; for each unfilled count, claim ascending idle units of that type
// (each idle unit at most once per call). Appends to team.members/
// member_types. Returns the number claimed. Deterministic.
int recruit_idle(TeamInstance& team, const AimdData& aimd,
                 const std::vector<IdleUnitView>& idle);

// Autocreate selection [RE-GATE APPLIED: ai-team.md §4]: among TeamTypes that
// are autocreate=true AND NOT is_base_defense (defensive teams are out of the
// AI-2 offensive scope, §1). Eligibility is NOT a per-side house-token compare:
// stock data is universally House=<none>, so the real gate is whether the house
// can FIELD the TaskForce — which parse_aimd already guarantees (a team survives
// the art-guard-drop only if every UNITID in its TaskForce resolved in THIS
// house's roster). `house_name` applies ONLY the optional country restriction
// of a rare House=<name> team (case-insensitive; "<none>"/"" matches any).
// Caps: Max (+0xb8) live instances — Max=0 = UNCAPPED (not zero!), else a team
// is eligible while cntInstances < Max; plus the hard per-house
// rules.total_team_cap (TotalAITeamCap=30). Also gated on
// credits >= rules.credit_reserve (the general spend floor). Higher Priority is
// strictly preferred; the equal-highest-priority tie is broken by a seeded
// ai_rand draw among the equal-highest-priority eligible teams
// [RE-FALLBACK: ai-team.md §4 — stock offensive teams are all Priority=5, so
// ties are the common case; deterministic seeded xorshift32, NEVER wall-clock].
// `rng` is the caller's seeded ai_rand stream (AiTeamState::rng). Returns the
// AimdData::teams index or -1. ONE team per call — the caller enforces one per
// autocreate-spawn tick. AITriggerTypes-driven creation is AI-3: this function
// is the whole AI-2 creation surface.
int select_autocreate_team(const AimdData& aimd, const std::string& house_name,
                           const std::vector<TeamInstance>& live, double credits,
                           const AiTeamRules& rules, std::uint32_t& rng);

// Production intent [RE: ai-team.md §6]: the unit-roster type the house should
// start producing NEXT — the shortfall type of the FIRST (oldest) Recruiting
// team. Returns -1 when nothing should start. InfantryReserve GATE [RE-GATE
// APPLIED: ai-team.md §6 — the recovered rule]: an INFANTRY shortfall is
// produced FREELY while `credits > rules.infantry_reserve` (50000); otherwise it
// falls to the normal affordability check `credits >= cost x cost_mul +
// credit_reserve` (bank, never substitute — the AI-1 rule). Vehicle shortfall
// always uses the normal affordability check.
int next_produce_type(const std::vector<TeamInstance>& teams, const AimdData& aimd,
                      double credits, double cost_mul,
                      const std::vector<UnitTypeDef>& unit_roster,
                      const AiTeamRules& rules);

// BuildSlowdown over-queue DAMP [RE-GATE APPLIED: ai-team.md §6]: BuildSlowdown
// is a difficulty BYTE (Easy off / Normal+Difficult on), an anti-spam damp —
// "don't queue more of a category the AI already has queued/on-hand" — NOT a
// build-time multiplier. Returns true when the damp should BLOCK starting
// another of a category the house already has queued/on-hand (i.e. when
// rules.build_slowdown && category_has_queued_or_onhand). Easy (build_slowdown
// off) never damps.
bool build_slowdown_blocks(const AiTeamRules& rules, bool category_has_queued_or_onhand);

}  // namespace ra2engine
