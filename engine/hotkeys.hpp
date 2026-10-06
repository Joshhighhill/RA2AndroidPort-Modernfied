#pragma once
// G-04: the original's keyboard command table. Pure C++17 (no SDL).
//
// [RE: gamemd fixB hunt] Every hotkey is a CommandClass whose GetName (vtable
// slot 1) returns the INI key name and whose Execute is vtable slot 8, e.g.
//   StopObject  vt@7EAA7C exec FUN_005368B0 -> FUN_0072FF70 (EVENT_IDLE)
//   GuardObject vt@7EAAA4 exec FUN_00536A30 -> FUN_0072FE30 (Area_Guard)
//   ScatterObject vt@7EAACC exec FUN_00536AB0 -> FUN_007300B0 (EVENT_SCATTER)
//   DeployObject vt@7EAA2C exec FUN_005369B0 -> FUN_0072FBD0
//   TypeSelect  vt@7EA964 exec FUN_005365E0 -> FUN_00731D70/FUN_00731D90
//   CenterBase  vt@7EAB1C exec FUN_00536BD0 -> FUN_00730670
//   StructureTab/DefenseTab/InfantryTab/UnitTab exec FUN_00535AC0/B80/C40/CD0
//   ToggleRepair/ToggleSell exec FUN_00536CD0/FUN_00536D50
//   Options     vt@7EAC5C exec FUN_00537000 -> FUN_00646AE0 (options dialog)
//   SidebarUp/Down exec FUN_00536EE0/F70 -> FUN_006A6090(+-1)
//   TeamCreate_N/TeamSelect_N/TeamAddSelect_N/TeamCenter_N (names built with
//   "%d") -> FUN_00730130/FUN_00730290/FUN_00730590/FUN_00730470.
// Bindings live in KeyboardMD.ini [Hotkey] <Name>=<code>, code = Windows VK
// + 256 (Shift) + 512 (Ctrl) + 1024 (Alt). The defaults below are the
// install's shipped KeyboardMD.ini verbatim.
#include <map>
#include <string>
#include <vector>

namespace ra2engine {

enum class HotCmd {
    None,
    Options, CenterView, CenterOnRadarEvent,
    TeamSelect, TeamAddSelect, TeamCreate, TeamCenter,   // arg = 1..10
    ToggleAlliance, PlaceBeacon, AllToCheer, DeployObject, Follow, GuardObject,
    CenterBase, ToggleRepair, ToggleSell, PreviousObject, NextObject,
    CombatantSelect, StopObject, TypeSelect, PageUser, ScatterObject,
    VeterancyNav, PlanningMode, Delete, ScreenCapture,
    StructureTab, DefenseTab, InfantryTab, UnitTab, SidebarUp, SidebarDown,
    View, SetView,   // arg = 1..4
    Taunt,           // arg = 1..8
};

struct HotBinding {
    HotCmd cmd = HotCmd::None;
    int arg = 0;
};

// Encode a key press the way KeyboardMD.ini does.
inline int hotkey_code(int vk, bool shift, bool ctrl, bool alt) {
    return vk + (shift ? 256 : 0) + (ctrl ? 512 : 0) + (alt ? 1024 : 0);
}

class HotkeyTable {
public:
    // The shipped KeyboardMD.ini [Hotkey] bindings.
    static HotkeyTable defaults();
    // Override bindings from a KeyboardMD.ini text ([Hotkey] Name=code lines;
    // unknown names are ignored, other sections skipped). Returns the number
    // of bindings applied.
    int load_ini_text(const std::string& text);
    HotBinding lookup(int vk, bool shift, bool ctrl, bool alt) const;
    // The code bound to a command (or -1).
    int code_of(HotCmd c, int arg = 0) const;
    // Name -> binding, e.g. "TeamSelect_3" -> {TeamSelect, 3}. False if unknown.
    static bool parse_name(const std::string& name, HotBinding& out);
    std::size_t size() const { return by_code_.size(); }

private:
    void bind(const HotBinding& b, int code);
    std::map<int, HotBinding> by_code_;
    std::map<std::pair<int, int>, int> code_by_cmd_;
};

}  // namespace ra2engine
