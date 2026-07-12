#include "doctest.h"
#include <cmath>
#include "../engine/ai_team.hpp"
using namespace ra2engine;

namespace {
// Hand-built aimd tables (no Ini): TaskForce = 2 x type0 + 1 x type1;
// script exercises every implementable opcode class + an inert one.
AimdData make_aimd(bool full = false) {
    AimdData d;
    TaskForce tf;
    tf.id = "TF1";
    tf.members = {{2, 0}, {1, 1}};              // 2 x type0, 1 x type1
    d.taskforces.push_back(tf);
    ScriptType sc;
    sc.id = "SC1";
    sc.actions = {{kOpMoveToWaypoint, 0},            // pc 0: move to waypoint 0 (op 9)
                  {kOpAttackQuarry, kQuarryAnything},  // pc 1: attack anything (op 0)
                  {kOpTimedGuard, 2},                // pc 2: timed guard 2 -> 180 frames
                  {46, 0},                           // pc 3: transport load 0x2e -> Inert
                  {kOpDisband, 0}};                  // pc 4: disband (op 0x31)
    d.scripts.push_back(sc);
    TeamType tt;
    tt.id = "TT1";
    tt.house = "Russians";
    tt.taskforce = 0;
    tt.script = 0;
    tt.priority = 7;
    tt.max = 2;
    tt.full = full;
    tt.autocreate = true;
    d.teams.push_back(tt);
    return d;
}
TeamInstance make_team() {
    TeamInstance t;
    t.teamtype = 0;
    return t;
}
// A full-strength view: members parallel to the claimed 3 slots.
TeamView full_view(bool idle) {
    TeamView v;
    v.base_x = 10; v.base_y = 10;
    v.enemy_x = 40; v.enemy_y = 10;
    v.waypoints = {Cellxy{30, 12}, Cellxy{35, 15}};
    for (int i = 0; i < 3; ++i) {
        TeamMemberView m;
        m.unit = 100 + i;
        m.alive = true;
        m.cx = 11 + i; m.cy = 10;
        m.idle = idle;
        v.members.push_back(m);
    }
    return v;
}
void claim_full(TeamInstance& t) {          // 2 x type0 + 1 x type1
    t.members = {100, 101, 102};
    t.member_types = {0, 0, 1};
}
constexpr double kDt = 1.0 / 15.0;
}  // namespace

TEST_CASE("ai_team: full=yes stays Recruiting under strength; gathers on a ring around the enemy base") {
    AimdData d = make_aimd(/*full=*/true);
    TeamInstance t = make_team();
    std::uint32_t rng = 0x2545F491u;
    TeamView v = full_view(true);
    // Below strength + Full=yes: stays Recruiting, emits nothing (never launches
    // under strength) [RE-GATE APPLIED: ai-team.md §2].
    t.members = {100};
    t.member_types = {0};
    v.members.resize(1);
    TeamCommand c = step_team(t, v, d, rng, kDt);
    CHECK(t.state == TeamState::Recruiting);
    CHECK(c.kind == TeamCommand::Kind::None);
    // Full strength -> Gathering; the gather cell is on a circle of radius
    // kAiSafeDistance around the ENEMY base centre (40,10) at a seeded angle
    // [RE-GATE APPLIED: ai-team.md §2].
    claim_full(t);
    v = full_view(true);
    c = step_team(t, v, d, rng, kDt);
    CHECK(t.state == TeamState::Gathering);
    const double dx = t.gather_x - v.enemy_x, dy = t.gather_y - v.enemy_y;
    CHECK(std::sqrt(dx * dx + dy * dy) ==
          doctest::Approx((double)kAiSafeDistance).epsilon(0.10));  // on the ring
}

TEST_CASE("ai_team: full=no launches immediately under strength; skirmish never dissolves, MP does") {
    AimdData d = make_aimd(/*full=*/false);
    std::uint32_t rng = 1u;
    // Full=no + at least one member: launches AT ONCE (acts under strength),
    // no wait for the dissolve timer [RE-GATE APPLIED: ai-team.md §2].
    TeamInstance t = make_team();
    TeamView v = full_view(true);
    t.members = {100};             // 1 of 3 slots
    t.member_types = {0};
    v.members.resize(1);
    (void)step_team(t, v, d, rng, kDt);
    CHECK(t.state == TeamState::Gathering);
    // Zero members: the DEFAULT skirmish path (multiplay=false) NEVER dissolves
    // on the timer — it keeps recruiting/producing [RE-GATE APPLIED: §2].
    const int frames = kAiDissolveUnfilledDelayFrames + 2;
    TeamInstance skirm = make_team();
    TeamView ev = full_view(true);
    ev.members.clear();
    for (int i = 0; i < frames; ++i)
        (void)step_team(skirm, ev, d, rng, kDt);   // multiplay defaults false
    CHECK(skirm.state == TeamState::Recruiting);
    // The MULTIPLAY path DOES dissolve an unfilled team past the timer.
    TeamInstance mp = make_team();
    for (int i = 0; i < frames && mp.state == TeamState::Recruiting; ++i)
        (void)step_team(mp, ev, d, rng, kDt, /*multiplay=*/true);
    CHECK(mp.state == TeamState::Disbanding);
}

TEST_CASE("ai_team: full=yes waits past the timeout and bounces back on a death") {
    AimdData d = make_aimd(/*full=*/true);
    TeamInstance t = make_team();
    std::uint32_t rng = 1u;
    TeamView v = full_view(true);
    t.members = {100};
    t.member_types = {0};
    v.members.resize(1);
    for (int i = 0; i < kAiDissolveUnfilledDelayFrames + 2; ++i)
        (void)step_team(t, v, d, rng, kDt);
    CHECK(t.state == TeamState::Recruiting);     // full=yes: no timeout launch
    // Fill it -> Gathering; then a death reopens a slot -> back to Recruiting
    // [RE: ai-team.md §2].
    claim_full(t);
    v = full_view(true);
    (void)step_team(t, v, d, rng, kDt);
    REQUIRE(t.state == TeamState::Gathering);
    t.members = {100, 102};                      // one type0 died (caller pruned)
    t.member_types = {0, 1};
    v.members.resize(2);
    (void)step_team(t, v, d, rng, kDt);
    CHECK(t.state == TeamState::Recruiting);
}

TEST_CASE("ai_team: gathering issues ONE move order then completes when members reach the ring (Stray)") {
    AimdData d = make_aimd();
    TeamInstance t = make_team();
    std::uint32_t rng = 1u;
    claim_full(t);
    TeamView v = full_view(true);
    (void)step_team(t, v, d, rng, kDt);          // Recruiting -> Gathering (sets gather cell)
    REQUIRE(t.state == TeamState::Gathering);
    TeamCommand c = step_team(t, v, d, rng, kDt);
    CHECK(c.kind == TeamCommand::Kind::MoveTeam);  // the ONE gather order
    CHECK(c.x == t.gather_x);
    CHECK(c.y == t.gather_y);
    // Members still far from the gather focus -> stays Gathering, no re-order.
    c = step_team(t, v, d, rng, kDt);
    CHECK(c.kind == TeamCommand::Kind::None);
    CHECK(t.state == TeamState::Gathering);
    // Move every member onto the gather cell (within Stray=2.0) -> Executing
    // [RE-GATE APPLIED: ai-team.md §2].
    for (TeamMemberView& m : v.members) { m.cx = t.gather_x; m.cy = t.gather_y; }
    (void)step_team(t, v, d, rng, kDt);
    CHECK(t.state == TeamState::Executing);
    CHECK(t.script_pc == 0);
}

TEST_CASE("ai_team: move-to-waypoint opcode targets the waypoint table then advances at CloseEnough") {
    AimdData d = make_aimd();
    TeamInstance t = make_team();
    std::uint32_t rng = 1u;
    claim_full(t);
    t.state = TeamState::Executing;              // enter the interpreter directly
    TeamView v = full_view(true);
    TeamCommand c = step_team(t, v, d, rng, kDt);
    CHECK(c.kind == TeamCommand::Kind::MoveTeam);
    CHECK(c.x == 30);                            // waypoints[0] = (30,12)
    CHECK(c.y == 12);
    CHECK(t.script_pc == 0);                     // still on the move action (far)
    // Move members within CloseEnough=2.25 of the waypoint -> advance.
    for (TeamMemberView& m : v.members) { m.cx = 30; m.cy = 12; }
    c = step_team(t, v, d, rng, kDt);            // arrived
    CHECK(t.script_pc == 1);                     // pc++ -> attack action
    // Unmapped waypoint argument -> enemy base centre [RE-FALLBACK]:
    AimdData d2 = make_aimd();
    d2.scripts[0].actions[0].argument = 99;
    TeamInstance t2 = make_team();
    claim_full(t2);
    t2.state = TeamState::Executing;
    TeamView v2 = full_view(true);
    c = step_team(t2, v2, d2, rng, kDt);
    CHECK(c.kind == TeamCommand::Kind::MoveTeam);
    CHECK(c.x == v2.enemy_x);
    CHECK(c.y == v2.enemy_y);
}

TEST_CASE("ai_team: attack-quarry emits while a target exists, advances when extinct") {
    AimdData d = make_aimd();
    TeamInstance t = make_team();
    std::uint32_t rng = 1u;
    claim_full(t);
    t.state = TeamState::Executing;
    t.script_pc = 1;                             // the attack action
    TeamView v = full_view(true);
    v.quarry_exists = true;
    TeamCommand c = step_team(t, v, d, rng, kDt);
    CHECK(c.kind == TeamCommand::Kind::AttackQuarry);
    CHECK(c.quarry == kQuarryAnything);          // [RE: ai-team.md §5]
    CHECK(t.script_pc == 1);                     // held while targets remain
    c = step_team(t, v, d, rng, kDt);
    CHECK(c.kind == TeamCommand::Kind::AttackQuarry);  // re-emitted
    v.quarry_exists = false;                     // class extinct
    c = step_team(t, v, d, rng, kDt);
    CHECK(c.kind == TeamCommand::Kind::None);
    CHECK(t.script_pc == 2);                     // advanced to guard
}

TEST_CASE("ai_team: timed-guard holds for arg x 90 frames then advances") {
    AimdData d = make_aimd();
    TeamInstance t = make_team();
    std::uint32_t rng = 1u;
    claim_full(t);
    t.state = TeamState::Executing;
    t.script_pc = 2;                             // timed guard 2 -> 180 frames (12 s) [§3]
    TeamView v = full_view(true);
    int guard_ticks = 0;
    while (t.script_pc == 2) {
        const TeamCommand c = step_team(t, v, d, rng, kDt);
        if (t.script_pc == 2) {
            CHECK(c.kind == TeamCommand::Kind::GuardArea);
            CHECK(c.radius == kAiGatherRadius);
            ++guard_ticks;
        }
        REQUIRE(guard_ticks < 200);              // 2 x 90 = 180 frames + slack
    }
    CHECK(guard_ticks >= 179);                   // held ~180 frames at 15 Hz
    CHECK(t.script_pc == 3);
}

TEST_CASE("ai_team: inert opcode is a logged skip that advances the pc") {
    AimdData d = make_aimd();
    TeamInstance t = make_team();
    std::uint32_t rng = 1u;
    claim_full(t);
    t.state = TeamState::Executing;
    t.script_pc = 3;                             // action 0x2e=46 (transport load) -> Inert
    TeamView v = full_view(true);
    const TeamCommand c = step_team(t, v, d, rng, kDt);
    CHECK(c.kind == TeamCommand::Kind::Inert);
    CHECK(c.opcode == 46);                       // [RE: ai-team.md §3; spec §2]
    CHECK(t.script_pc == 4);
}

TEST_CASE("ai_team: jump consumes the tick; disband opcode ends the team") {
    AimdData d = make_aimd();
    d.scripts[0].actions[3] = {kOpJumpToLine, 1};  // pc 3: jump -> line 1
    TeamInstance t = make_team();
    std::uint32_t rng = 1u;
    claim_full(t);
    t.state = TeamState::Executing;
    t.script_pc = 3;
    TeamView v = full_view(true);
    (void)step_team(t, v, d, rng, kDt);
    CHECK(t.script_pc == 1);                     // jumped, one action this tick
    CHECK(t.state == TeamState::Executing);
    // The disband opcode (0x31=49) at pc 4 ends the team [RE: ai-team.md §3].
    t.script_pc = 4;
    (void)step_team(t, v, d, rng, kDt);
    CHECK(t.state == TeamState::Disbanding);
    // A script that runs off the end also disbands.
    TeamInstance t2 = make_team();
    claim_full(t2);
    t2.state = TeamState::Executing;
    t2.script_pc = (int)d.scripts[0].actions.size();  // past the last line
    (void)step_team(t2, v, d, rng, kDt);
    CHECK(t2.state == TeamState::Disbanding);
}

TEST_CASE("ai_team: a wiped team disbands from any post-recruiting state") {
    AimdData d = make_aimd();
    TeamInstance t = make_team();
    std::uint32_t rng = 1u;
    claim_full(t);
    t.state = TeamState::Executing;
    t.script_pc = 1;
    TeamView v = full_view(true);
    for (TeamMemberView& m : v.members) m.alive = false;
    (void)step_team(t, v, d, rng, kDt);
    CHECK(t.state == TeamState::Disbanding);
}

TEST_CASE("ai_team: step_team is deterministic") {
    for (int run = 0; run < 2; ++run) {
        AimdData d = make_aimd();
        TeamInstance t = make_team();
        std::uint32_t rng = 0x2545F491u;
        claim_full(t);
        TeamView idle = full_view(true);
        idle.quarry_exists = true;
        static std::vector<int> first_pcs;
        std::vector<int> pcs;
        for (int i = 0; i < 40; ++i) {
            (void)step_team(t, idle, d, rng, kDt);
            pcs.push_back(t.script_pc * 10 + (int)t.state);
        }
        if (run == 0) first_pcs = pcs;
        else CHECK(first_pcs == pcs);
    }
}

// --- Install-gated real-rulesmd pins (skip clean without an install, exactly
//     like tests/test_ai_base.cpp's install-gated case) ---
#include "ra2assets/rules.hpp"
#include "ra2assets/ini.hpp"
#include "ra2assets/mix.hpp"
#include <cstdlib>
#include <fstream>
#include <string>

static std::vector<uint8_t> slurp_ai_team(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}

TEST_CASE("ai_team: real rulesmd [AI] team knobs match the recovered constants") {
    using namespace ra2assets;
    const char* dir = std::getenv("RA2_INSTALL_DIR");
    std::string base = dir ? dir : "D:\\Red Alert 2";
    auto top_bytes = slurp_ai_team(base + "\\ra2md.mix");
    if (top_bytes.empty()) { MESSAGE("skipped: ra2md.mix not found"); return; }
    MixFile top = MixFile::open(std::move(top_bytes));

    std::vector<uint8_t> rules_bytes;
    if (const MixEntry* e = top.find(mix_id("rulesmd.ini"))) {
        rules_bytes = top.read(*e);
    } else {
        static const char* candidates[] = {
            "localmd.mix",
            "expandmd01.mix", "expandmd02.mix", "expandmd03.mix",
            "expandmd04.mix", "expandmd05.mix", "expandmd99.mix",
            "cachemd.mix", "conqmd.mix",
        };
        for (const char* name : candidates) {
            try {
                MixFile nested = top.open_nested(name);
                if (const MixEntry* e2 = nested.find(mix_id("rulesmd.ini"))) {
                    rules_bytes = nested.read(*e2);
                    break;
                }
            } catch (const std::exception&) {
            }
        }
    }
    if (rules_bytes.empty()) {
        MESSAGE("skipped: rulesmd.ini not found in ra2md.mix top level or in "
                "localmd.mix/expandmd*.mix/cachemd.mix/conqmd.mix");
        return;
    }

    Ini ini = Ini::parse(std::string(rules_bytes.begin(), rules_bytes.end()));
    Rules rules = Rules::load(ini);

    // [RE-GATE APPLIED: ai-team.md §2/§4 — these pin the committed constants
    // against the REAL file so a rules/constant drift is caught.]
    // [DEVIATION — empirical calibration]: in the real rulesmd these team-clock
    // keys live in the [General] INI section, NOT [AI] (verified against the
    // shipped rulesmd.ini: TeamDelays/TotalAITeamCap/DissolveUnfilledTeamDelay
    // are all under [General]; the "[AI]" tag in the header comments denotes the
    // AI subsystem / Rules+offset, not the INI section). Querying [General] is
    // what makes the pin actually validate the constants against reality.
    CHECK(rules.section_double("General", "DissolveUnfilledTeamDelay", -1.0) ==
          doctest::Approx((double)ra2engine::kAiDissolveUnfilledDelayFrames));
    // TeamDelays= is a CSV; section_double parses the leading number — the
    // FILE-ORDER head. The constant array stores the recovered file order
    // {2000,2500,3500} at indices 0/1/2 as loaded [RE-GATE APPLIED: ai-team.md
    // §4], so the head equals index 0:
    CHECK(rules.section_double("General", "TeamDelays", -1.0) ==
          doctest::Approx((double)ra2engine::kAiTeamDelayFrames[0]));
    CHECK(rules.section_double("General", "AISafeDistance", -1.0) ==
          doctest::Approx((double)ra2engine::kAiSafeDistance));
}
