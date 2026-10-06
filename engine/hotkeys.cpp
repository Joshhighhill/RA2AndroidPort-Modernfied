#include "hotkeys.hpp"

#include <cctype>
#include <cstdlib>
#include <sstream>

namespace ra2engine {

namespace {
struct Named { const char* name; HotCmd cmd; };
const Named kSimple[] = {
    {"Options", HotCmd::Options}, {"CenterView", HotCmd::CenterView},
    {"CenterOnRadarEvent", HotCmd::CenterOnRadarEvent}, {"ToggleAlliance", HotCmd::ToggleAlliance},
    {"PlaceBeacon", HotCmd::PlaceBeacon}, {"AllToCheer", HotCmd::AllToCheer},
    {"DeployObject", HotCmd::DeployObject}, {"Follow", HotCmd::Follow},
    {"GuardObject", HotCmd::GuardObject}, {"CenterBase", HotCmd::CenterBase},
    {"ToggleRepair", HotCmd::ToggleRepair}, {"ToggleSell", HotCmd::ToggleSell},
    {"PreviousObject", HotCmd::PreviousObject}, {"NextObject", HotCmd::NextObject},
    {"CombatantSelect", HotCmd::CombatantSelect}, {"StopObject", HotCmd::StopObject},
    {"TypeSelect", HotCmd::TypeSelect}, {"PageUser", HotCmd::PageUser},
    {"ScatterObject", HotCmd::ScatterObject}, {"VeterancyNav", HotCmd::VeterancyNav},
    {"PlanningMode", HotCmd::PlanningMode}, {"Delete", HotCmd::Delete},
    {"ScreenCapture", HotCmd::ScreenCapture}, {"StructureTab", HotCmd::StructureTab},
    {"DefenseTab", HotCmd::DefenseTab}, {"InfantryTab", HotCmd::InfantryTab},
    {"UnitTab", HotCmd::UnitTab}, {"SidebarUp", HotCmd::SidebarUp},
    {"SidebarDown", HotCmd::SidebarDown},
};
struct Indexed { const char* prefix; HotCmd cmd; int lo, hi; };
const Indexed kIndexed[] = {
    {"TeamSelect_", HotCmd::TeamSelect, 1, 10}, {"TeamAddSelect_", HotCmd::TeamAddSelect, 1, 10},
    {"TeamCreate_", HotCmd::TeamCreate, 1, 10}, {"TeamCenter_", HotCmd::TeamCenter, 1, 10},
    {"SetView", HotCmd::SetView, 1, 4}, {"View", HotCmd::View, 1, 4},
    {"Taunt_", HotCmd::Taunt, 1, 8},
};

// D:\Red Alert 2\KeyboardMD.ini as shipped (the binary's defaults).
const char* kShipped =
    "[Hotkey]\n"
    "CenterView=12\nOptions=27\nCenterOnRadarEvent=32\n"
    "TeamSelect_10=48\nTeamSelect_1=49\nTeamSelect_2=50\nTeamSelect_3=51\nTeamSelect_4=52\n"
    "TeamSelect_5=53\nTeamSelect_6=54\nTeamSelect_7=55\nTeamSelect_8=56\nTeamSelect_9=57\n"
    "ToggleAlliance=65\nPlaceBeacon=66\nAllToCheer=67\nDeployObject=68\nInfantryTab=69\n"
    "Follow=70\nGuardObject=71\nCenterBase=72\nToggleRepair=75\nToggleSell=76\n"
    "PreviousObject=77\nNextObject=78\nCombatantSelect=80\nStructureTab=81\nUnitTab=82\n"
    "StopObject=83\nTypeSelect=84\nPageUser=85\nDefenseTab=87\nScatterObject=88\n"
    "VeterancyNav=89\nPlanningMode=90\nSidebarDown=98\nSidebarUp=104\nDelete=110\n"
    "View1=112\nView2=113\nView3=114\nView4=115\n"
    "Taunt_1=116\nTaunt_2=117\nTaunt_3=118\nTaunt_4=119\nTaunt_5=120\nTaunt_6=121\n"
    "Taunt_7=122\nTaunt_8=123\n"
    "TeamAddSelect_10=304\nTeamAddSelect_1=305\nTeamAddSelect_2=306\nTeamAddSelect_3=307\n"
    "TeamAddSelect_4=308\nTeamAddSelect_5=309\nTeamAddSelect_6=310\nTeamAddSelect_7=311\n"
    "TeamAddSelect_8=312\nTeamAddSelect_9=313\nScreenCapture=339\n"
    "TeamCreate_10=560\nTeamCreate_1=561\nTeamCreate_2=562\nTeamCreate_3=563\n"
    "TeamCreate_4=564\nTeamCreate_5=565\nTeamCreate_6=566\nTeamCreate_7=567\n"
    "TeamCreate_8=568\nTeamCreate_9=569\n"
    "SetView1=624\nSetView2=625\nSetView3=626\nSetView4=627\n"
    "TeamCenter_10=1072\nTeamCenter_1=1073\nTeamCenter_2=1074\nTeamCenter_3=1075\n"
    "TeamCenter_4=1076\nTeamCenter_5=1077\nTeamCenter_6=1078\nTeamCenter_7=1079\n"
    "TeamCenter_8=1080\nTeamCenter_9=1081\n";

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}
}  // namespace

bool HotkeyTable::parse_name(const std::string& name, HotBinding& out) {
    for (const Named& n : kSimple)
        if (name == n.name) { out = {n.cmd, 0}; return true; }
    for (const Indexed& ix : kIndexed) {
        const std::string p = ix.prefix;
        if (name.size() <= p.size() || name.compare(0, p.size(), p) != 0) continue;
        const std::string num = name.substr(p.size());
        char* e = nullptr;
        const long v = std::strtol(num.c_str(), &e, 10);
        if (!e || *e != '\0' || v < ix.lo || v > ix.hi) continue;
        out = {ix.cmd, static_cast<int>(v)};
        return true;
    }
    return false;
}

void HotkeyTable::bind(const HotBinding& b, int code) {
    const auto key = std::make_pair(static_cast<int>(b.cmd), b.arg);
    const auto old = code_by_cmd_.find(key);
    if (old != code_by_cmd_.end()) by_code_.erase(old->second);   // rebinding moves it
    code_by_cmd_[key] = code;
    if (code > 0) by_code_[code] = b;
}

int HotkeyTable::load_ini_text(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    bool in_section = false;
    int applied = 0;
    while (std::getline(in, line)) {
        const std::size_t sc = line.find(';');
        if (sc != std::string::npos) line.erase(sc);
        line = trim(line);
        if (line.empty()) continue;
        if (line.front() == '[') {
            in_section = (line == "[Hotkey]");
            continue;
        }
        if (!in_section) continue;
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        HotBinding b;
        if (!parse_name(trim(line.substr(0, eq)), b)) continue;
        char* e = nullptr;
        const std::string v = trim(line.substr(eq + 1));
        const long code = std::strtol(v.c_str(), &e, 10);
        if (!e || *e != '\0') continue;
        bind(b, static_cast<int>(code));
        ++applied;
    }
    return applied;
}

HotkeyTable HotkeyTable::defaults() {
    HotkeyTable t;
    t.load_ini_text(kShipped);
    return t;
}

HotBinding HotkeyTable::lookup(int vk, bool shift, bool ctrl, bool alt) const {
    const auto it = by_code_.find(hotkey_code(vk, shift, ctrl, alt));
    return it == by_code_.end() ? HotBinding{} : it->second;
}

int HotkeyTable::code_of(HotCmd c, int arg) const {
    const auto it = code_by_cmd_.find(std::make_pair(static_cast<int>(c), arg));
    return it == code_by_cmd_.end() ? -1 : it->second;
}

}  // namespace ra2engine
