#include "rocket.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "ra2assets/rules.hpp"

namespace ra2engine {

namespace {

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

std::string trimmed(std::string v) {
    const std::size_t c = v.find_first_of(";,");
    if (c != std::string::npos) v = v.substr(0, c);
    const auto b = v.find_first_not_of(" \t");
    if (b == std::string::npos) return std::string();
    const auto e = v.find_last_not_of(" \t");
    return v.substr(b, e - b + 1);
}

bool yes(const std::string& raw, bool fallback) {
    const std::string v = trimmed(raw);
    if (v.empty()) return fallback;
    switch (v[0]) {
        case 'y': case 'Y': case 't': case 'T': case '1': return true;
        case 'n': case 'N': case 'f': case 'F': case '0': return false;
        default: return fallback;
    }
}

// A [CombatDamage] warhead key ([General] tolerated: older overlays put it there).
std::string warhead_key(const ra2assets::Rules& rules, const char* key, const char* fallback) {
    std::string v = trimmed(rules.section_string("CombatDamage", key, ""));
    if (v.empty()) v = trimmed(rules.section_string("General", key, ""));
    return v.empty() ? std::string(fallback) : v;
}

RocketWarhead resolve_warhead(const ra2assets::Rules& rules, const std::string& id) {
    RocketWarhead w;
    w.id = id;
    w.spread = rules.section_double(id, "CellSpread", 0.0);
    w.pmax = rules.section_double(id, "PercentAtMax", 1.0);
    w.verses.fill(100);
    const std::vector<int> v = rules.verses(id);
    for (std::size_t i = 0; i < v.size() && i < w.verses.size(); ++i) w.verses[i] = v[i];
    return w;
}

RocketBlock resolve_block(const ra2assets::Rules& rules, const std::string& pre, const RocketBlock& d,
                          const char* wh_key, const char* wh_def, const char* ewh_key,
                          const char* ewh_def) {
    RocketBlock b = d;
    auto num = [&](const char* k, double def) { return rules.general_double(pre + k, def); };
    b.pause_frames = static_cast<int>(num("PauseFrames", d.pause_frames));
    b.tilt_frames = static_cast<int>(num("TiltFrames", d.tilt_frames));
    b.pitch_initial = num("PitchInitial", d.pitch_initial);
    b.pitch_final = num("PitchFinal", d.pitch_final);
    b.turn_rate = num("TurnRate", d.turn_rate);
    b.raise_rate = static_cast<int>(num("RaiseRate", d.raise_rate));
    b.acceleration = num("Acceleration", d.acceleration);
    b.altitude = static_cast<int>(num("Altitude", d.altitude));
    b.damage = static_cast<int>(num("Damage", d.damage));
    b.elite_damage = static_cast<int>(num("EliteDamage", d.elite_damage));
    b.body_length = static_cast<int>(num("BodyLength", d.body_length));
    b.lazy_curve = yes(rules.section_string("General", pre + "LazyCurve", ""), d.lazy_curve);
    const std::string wh = warhead_key(rules, wh_key, wh_def);
    const std::string ewh = warhead_key(rules, ewh_key, ewh_def);
    b.warhead = resolve_warhead(rules, wh);
    // [RE-FALLBACK: an elite warhead id with no [section] in the rules (an RA2
    // rules.ini without the *Elite keys) keeps the normal warhead instead of
    // the engine's all-default WarheadType.]
    b.elite_warhead = rules.section_string(ewh, "Verses", "").empty() ? b.warhead : resolve_warhead(rules, ewh);
    return b;
}

}  // namespace

RocketParams resolve_rocket_params(const ra2assets::Rules& rules) {
    RocketParams p;
    // Stock rulesmd [General] values (the fallbacks for an absent key).
    RocketBlock v3;
    v3.pause_frames = 0; v3.tilt_frames = 60; v3.pitch_initial = 0.21; v3.pitch_final = 0.5;
    v3.turn_rate = 0.05; v3.raise_rate = 1; v3.acceleration = 0.4; v3.altitude = 768;
    v3.damage = 200; v3.elite_damage = 400; v3.body_length = 256; v3.lazy_curve = true;
    RocketBlock dm;
    dm.pause_frames = 20; dm.tilt_frames = 60; dm.pitch_initial = 0.0; dm.pitch_final = 0.5;
    dm.turn_rate = 0.08; dm.raise_rate = 1; dm.acceleration = 0.8; dm.altitude = 768;
    dm.damage = 300; dm.elite_damage = 600; dm.body_length = 128; dm.lazy_curve = false;
    RocketBlock cm;
    cm.pause_frames = 20; cm.tilt_frames = 100; cm.pitch_initial = 1.0; cm.pitch_final = 1.0;
    cm.turn_rate = 0.10; cm.raise_rate = 1; cm.acceleration = 1.0; cm.altitude = 768;
    cm.damage = 200; cm.elite_damage = 250; cm.body_length = 128; cm.lazy_curve = false;
    p.block[kRocketV3] = resolve_block(rules, "V3Rocket", v3, "V3Warhead", "V3WH", "V3EliteWarhead", "V3EWH");
    p.block[kRocketDMisl] =
        resolve_block(rules, "DMisl", dm, "DMislWarhead", "DMISLWH", "DMislEliteWarhead", "DMISLEWH");
    p.block[kRocketCMisl] =
        resolve_block(rules, "CMisl", cm, "CMislWarhead", "CMISLWH", "CMislEliteWarhead", "CMISLEWH");
    auto type = [&](const char* k, const char* def) {
        const std::string v = trimmed(rules.section_string("General", k, ""));
        return v.empty() ? std::string(def) : v;
    };
    p.type_id[kRocketV3] = type("V3RocketType", "V3ROCKET");
    p.type_id[kRocketDMisl] = type("DMislType", "DMISL");
    p.type_id[kRocketCMisl] = type("CMislType", "CMISL");
    return p;
}

int rocket_kind_for(const RocketParams& p, const std::string& unit_id) {
    const std::string u = upper(unit_id);
    if (u == upper(p.type_id[kRocketV3])) return kRocketV3;
    if (u == upper(p.type_id[kRocketCMisl])) return kRocketCMisl;
    return kRocketDMisl;
}

}  // namespace ra2engine
