#include "aimd_parse.hpp"
#include <cctype>
#include <cstdlib>
#include <unordered_map>

namespace ra2engine {
namespace {

std::string upper(std::string s) {
    for (char& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}
// RA2 boolean values: yes/no (also true/1 tolerated) [RE: ai-team.md §1].
bool truthy(const std::string& v) {
    if (v.empty()) return false;
    const char c = (char)std::toupper((unsigned char)v[0]);
    return c == 'Y' || c == 'T' || c == '1';
}
bool all_digits(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s)
        if (!std::isdigit((unsigned char)c)) return false;
    return true;
}
// "count,UNITID" / "action,argument": split on the FIRST comma.
bool split2(const std::string& v, std::string& a, std::string& b) {
    const std::size_t comma = v.find(',');
    if (comma == std::string::npos) return false;
    a = v.substr(0, comma);
    b = v.substr(comma + 1);
    return true;
}

}  // namespace

int waypoint_from_letter(const std::string& s) {
    // Bijective base-26: A=0..Z=25, AA=26 [RE-GATE APPLIED: ai-team.md §1].
    if (s.empty()) return -1;
    long v = 0;
    for (char c : s) {
        const char u = (char)std::toupper((unsigned char)c);
        if (u < 'A' || u > 'Z') return -1;
        v = v * 26 + (u - 'A' + 1);
    }
    return (int)(v - 1);
}

AimdData parse_aimd(const ra2assets::Ini& ini,
                    const std::function<int(const std::string&)>& unit_index) {
    AimdData d;
    std::unordered_map<std::string, int> tf_ix, sc_ix;  // entity id -> table index

    // [TaskForces] index section: N=id, file order [RE: ai-team.md §1].
    for (const auto& kv : ini.section("TaskForces")) {
        const std::string id = upper(kv.second);
        if (id.empty() || !ini.has(id) || tf_ix.count(id)) continue;  // dangling/dup
        TaskForce tf;
        tf.id = id;
        bool ok = true;
        for (const auto& e : ini.section(id)) {
            if (e.first == "NAME") tf.name = e.second;
            else if (e.first == "GROUP") tf.group = std::atoi(e.second.c_str());
            else if (all_digits(e.first)) {   // N=count,UNITID
                std::string cnt, uid;
                if (!split2(e.second, cnt, uid)) { ok = false; break; }
                TaskForceMember m;
                m.count = std::atoi(cnt.c_str());
                m.unit = unit_index(upper(uid));
                if (m.count <= 0 || m.unit < 0) { ok = false; break; }  // art-guard-drop
                tf.members.push_back(m);
            }
        }
        if (!ok || tf.members.empty()) continue;  // whole TaskForce dropped
        tf_ix[id] = (int)d.taskforces.size();
        d.taskforces.push_back(std::move(tf));
    }

    // [ScriptTypes] index section: N=id [RE: ai-team.md §1/§3]. Every action is
    // KEPT even if this engine interprets it inert — the inert split is the
    // INTERPRETER's job (ai_team), not the parser's.
    for (const auto& kv : ini.section("ScriptTypes")) {
        const std::string id = upper(kv.second);
        if (id.empty() || !ini.has(id) || sc_ix.count(id)) continue;
        ScriptType sc;
        sc.id = id;
        for (const auto& e : ini.section(id)) {
            if (e.first == "NAME") sc.name = e.second;
            else if (all_digits(e.first)) {   // N=action,argument
                std::string act, arg;
                if (!split2(e.second, act, arg)) continue;  // malformed line skipped
                ScriptAction a;
                a.action = std::atoi(act.c_str());
                a.argument = std::atoi(arg.c_str());
                sc.actions.push_back(a);
            }
        }
        if (sc.actions.empty()) continue;
        sc_ix[id] = (int)d.scripts.size();
        d.scripts.push_back(std::move(sc));
    }

    // [TeamTypes] index section: N=id [RE-GATE APPLIED: ai-team.md §1 — the
    // named-key set]. A team whose TaskForce/Script was dropped is dropped too.
    for (const auto& kv : ini.section("TeamTypes")) {
        const std::string id = upper(kv.second);
        if (id.empty() || !ini.has(id)) continue;
        TeamType t;
        t.id = id;
        t.name = ini.get(id, "Name");
        t.house = ini.get(id, "House");
        const auto tf = tf_ix.find(upper(ini.get(id, "TaskForce")));
        const auto sc = sc_ix.find(upper(ini.get(id, "Script")));
        if (tf == tf_ix.end() || sc == sc_ix.end()) continue;  // art-guard-drop
        t.taskforce = tf->second;
        t.script = sc->second;
        t.priority = ini.get_int(id, "Priority", 0);
        t.max = ini.get_int(id, "Max", 0);
        t.full = truthy(ini.get(id, "Full"));
        t.autocreate = truthy(ini.get(id, "Autocreate"));
        t.aggressive = truthy(ini.get(id, "Aggressive"));   // [RE: ai-team.md §1]
        t.reinforce = truthy(ini.get(id, "Reinforce"));
        t.suicide = truthy(ini.get(id, "Suicide"));
        t.tech_level = ini.get_int(id, "TechLevel", 0);
        t.is_base_defense = truthy(ini.get(id, "IsBaseDefense"));  // DEFENSIVE marker
        t.waypoint = waypoint_from_letter(ini.get(id, "Waypoint"));
        d.teams.push_back(std::move(t));
    }
    return d;
}

}  // namespace ra2engine
