// G-04: the KeyboardMD.ini hotkey table (engine/hotkeys.{hpp,cpp}).
#include "doctest.h"
#include "hotkeys.hpp"

using namespace ra2engine;

TEST_CASE("hotkeys: shipped defaults decode VK + modifier codes") {
    const HotkeyTable t = HotkeyTable::defaults();
    CHECK(t.size() == 85);   // every [Hotkey] line of the shipped KeyboardMD.ini
    CHECK(t.lookup('S', false, false, false).cmd == HotCmd::StopObject);
    CHECK(t.lookup('G', false, false, false).cmd == HotCmd::GuardObject);
    CHECK(t.lookup('X', false, false, false).cmd == HotCmd::ScatterObject);
    CHECK(t.lookup('D', false, false, false).cmd == HotCmd::DeployObject);
    CHECK(t.lookup('T', false, false, false).cmd == HotCmd::TypeSelect);
    CHECK(t.lookup('H', false, false, false).cmd == HotCmd::CenterBase);
    CHECK(t.lookup('K', false, false, false).cmd == HotCmd::ToggleRepair);
    CHECK(t.lookup('L', false, false, false).cmd == HotCmd::ToggleSell);
    CHECK(t.lookup('Q', false, false, false).cmd == HotCmd::StructureTab);
    CHECK(t.lookup('W', false, false, false).cmd == HotCmd::DefenseTab);
    CHECK(t.lookup('E', false, false, false).cmd == HotCmd::InfantryTab);
    CHECK(t.lookup('R', false, false, false).cmd == HotCmd::UnitTab);
    CHECK(t.lookup(27, false, false, false).cmd == HotCmd::Options);
    const HotBinding sel3 = t.lookup('3', false, false, false);
    CHECK(sel3.cmd == HotCmd::TeamSelect);
    CHECK(sel3.arg == 3);
    const HotBinding c3 = t.lookup('3', false, true, false);    // Ctrl+3 = 512+51
    CHECK(c3.cmd == HotCmd::TeamCreate);
    CHECK(c3.arg == 3);
    CHECK(t.lookup('3', true, false, false).cmd == HotCmd::TeamAddSelect);
    CHECK(t.lookup('3', false, false, true).cmd == HotCmd::TeamCenter);
    const HotBinding g10 = t.lookup('0', false, false, false);
    CHECK(g10.cmd == HotCmd::TeamSelect);
    CHECK(g10.arg == 10);
    CHECK(t.lookup(112, false, false, false).cmd == HotCmd::View);
    CHECK(t.lookup(112, false, true, false).cmd == HotCmd::SetView);
    CHECK(t.lookup('S', true, false, false).cmd == HotCmd::ScreenCapture);  // 339 = Shift+S
    CHECK(t.lookup('J', false, false, false).cmd == HotCmd::None);
}

TEST_CASE("hotkeys: a user KeyboardMD.ini rebinds and moves a command") {
    HotkeyTable t = HotkeyTable::defaults();
    const int n = t.load_ini_text("[Other]\nStopObject=1\n[Hotkey]\n; comment\nStopObject=74\n"
                                  "Bogus=5\nGuardObject = 83 ; trailing\n");
    CHECK(n == 2);
    CHECK(t.lookup('J', false, false, false).cmd == HotCmd::StopObject);
    CHECK(t.lookup('S', false, false, false).cmd == HotCmd::GuardObject);
    CHECK(t.lookup('G', false, false, false).cmd == HotCmd::None);   // old guard key freed
    CHECK(t.code_of(HotCmd::StopObject) == 74);
}

TEST_CASE("hotkeys: parse_name ranges") {
    HotBinding b;
    CHECK(HotkeyTable::parse_name("TeamCenter_10", b));
    CHECK(b.arg == 10);
    CHECK_FALSE(HotkeyTable::parse_name("TeamCenter_11", b));
    CHECK(HotkeyTable::parse_name("SetView4", b));
    CHECK(b.cmd == HotCmd::SetView);
    CHECK_FALSE(HotkeyTable::parse_name("View5", b));
}
