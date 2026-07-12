#pragma once
// aimd.ini TaskForce/ScriptType/TeamType tables [RE: docs/re/ai-team.md §1].
// Pure parsing over the EXISTING ra2assets::Ini — no Ini-layer changes:
// section() already preserves numbered-key file order, which is exactly what
// the [TaskForces]/[ScriptTypes]/[TeamTypes] index sections and the per-entity
// N=... entries need. Struct layouts + key spellings are HUNT OUTPUTS
// [RE-GATE APPLIED: ai-team.md §1 — the pre-hunt shapes below are the spec's
// approved data model; the RE-GATE confirms or replaces them].
#include <functional>
#include <string>
#include <vector>
#include "ra2assets/ini.hpp"

namespace ra2engine {

// One TaskForce line: N=count,UNITID [RE: ai-team.md §1].
struct TaskForceMember {
    int count = 0;   // how many of this type the team needs
    int unit = -1;   // caller-resolved unit-roster index (see parse_aimd)
};
struct TaskForce {
    std::string id;     // section id (e.g. "01000050-G")
    std::string name;   // Name=
    int group = -1;     // Group= [RE: ai-team.md §1 — recorded; not consumed in AI-2]
    std::vector<TaskForceMember> members;  // file order
};

// One ScriptType line: N=action,argument [RE: ai-team.md §1/§3].
struct ScriptAction { int action = 0; int argument = 0; };
struct ScriptType {
    std::string id;
    std::string name;   // Name=
    std::vector<ScriptAction> actions;  // file order — script_pc indexes this
};

// One TeamType section [RE-GATE APPLIED: ai-team.md §1 — the named-key set;
// the hunt adds any flags that turn out load-bearing for the autocreate path].
struct TeamType {
    std::string id;
    std::string name;        // Name=
    std::string house;       // House= raw token (e.g. "Russians"); "<none>"/"" = any
                             // (universal in stock data — §1)
    int taskforce = -1;      // TaskForce=<id> resolved to AimdData::taskforces index
    int script = -1;         // Script=<id> resolved to AimdData::scripts index
    int priority = 0;        // Priority= (higher = considered first) [RE: ai-team.md §4]
    int max = 0;             // Max= live-instance cap; 0 = UNCAPPED [RE: ai-team.md §4]
    bool full = false;       // Full= (+0xa5): yes = wait for full strength before
                             // acting; no = act under strength [RE: ai-team.md §2]
    bool autocreate = false; // Autocreate= (+0xa9) — AI-2 creates ONLY these [RE: §4]
    bool aggressive = false; // Aggressive= (+0xad) offensive-marker, informational [RE: §1]
    bool reinforce = false;  // Reinforce= (+0xab) [RE: ai-team.md §1]
    bool suicide = false;    // Suicide= (+0xaf) [RE: ai-team.md §1]
    int tech_level = 0;      // TechLevel= (+0xcc) [RE: ai-team.md §1]
    bool is_base_defense = false;  // IsBaseDefense= (+0xf6) marks DEFENSIVE teams —
                             // skipped by the offensive selection [RE: ai-team.md §1]
    int waypoint = -1;       // Waypoint= letter -> index (A=0, B=1, ..., AA=26)
                             // [RE-GATE APPLIED: ai-team.md §1 — encoding pre-hunt]
};

struct AimdData {
    std::vector<TaskForce> taskforces;
    std::vector<ScriptType> scripts;
    std::vector<TeamType> teams;
};

// Waypoint letter encoding: "A"=0.."Z"=25, "AA"=26.. (bijective base-26).
// Empty/other -> -1 [RE-GATE APPLIED: ai-team.md §1].
int waypoint_from_letter(const std::string& s);

// Parse the three tables from an aimd-shaped Ini [RE: ai-team.md §1]:
// walk each index section ([TaskForces]/[ScriptTypes]/[TeamTypes], index=id,
// file order) via Ini::section(), then parse each entity section.
// `unit_index` maps a canonical-UPPER UNITID to the caller's unit-roster
// index, or -1 when this install cannot field that type (art missing / not
// loaded). ART-GUARD-DROP [spec §2]: a TaskForce containing ANY unresolved
// UNITID (or no members) is dropped WHOLE, and every TeamType referencing a
// dropped/missing TaskForce or a missing ScriptType is dropped too — the team
// is skipped, never a crash (AI-1's AiCatalog -1 discipline). Dangling index
// entries (id with no section) are skipped. Deterministic: output order =
// index-section file order.
AimdData parse_aimd(const ra2assets::Ini& ini,
                    const std::function<int(const std::string&)>& unit_index);

}  // namespace ra2engine
