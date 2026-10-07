// Agent B controls self-test scenario (see controls_selftest.inc).
using STest = ControlsSelftest;

static bool scene_is(const Scene& s, const char* n) { return std::string(s.name()) == n; }

// Step builders.
static STest::Step st_wait_scene(const char* n) {
    return {std::string("scene is ") + n, [n](STest&, Scene& s, int) { return scene_is(s, n); }};
}
static STest::Step st_click(const std::string& what, bool right = false) {
    return {"click " + what + (right ? " (right)" : ""),
            [what, right](STest& t, Scene& s, int) {
                if (!t.click(s, what, right)) {
                    t.fail("cannot locate " + what + " in scene " + s.name());
                }
                return true;
            }};
}
static STest::Step st_key(const char* label, int k, bool ctrl = false, bool shift = false,
                          bool alt = false) {
    return {std::string("key ") + label, [=](STest& t, Scene&, int) {
                t.key(k, ctrl, shift, alt);
                return true;
            }};
}
static STest::Step st_expect(const std::string& key, long long v, int timeout = 30) {
    STest::Step st{"expect " + key + " == " + std::to_string(v),
                   [key, v](STest&, Scene& s, int) { return STest::pv(s, key) == v; }};
    st.timeout = timeout;
    return st;
}
static STest::Step st_expect_ge(const std::string& key, long long v, int timeout = 30) {
    STest::Step st{"expect " + key + " >= " + std::to_string(v),
                   [key, v](STest&, Scene& s, int) { return STest::pv(s, key) >= v; }};
    st.timeout = timeout;
    return st;
}
static STest::Step st_wait(int n) {
    return {"wait " + std::to_string(n), [n](STest& t, Scene&, int f) {
                if (!t.mem.count("wait_from")) t.mem["wait_from"] = f;
                if (f - t.mem["wait_from"] < n) return false;
                t.mem.erase("wait_from");
                return true;
            }};
}
static STest::Step st_shot(const std::string& n) {
    return {"shot " + n, [n](STest& t, Scene&, int) {
                t.shot(n);
                return true;
            }};
}

static STest::Step st_cheat(std::vector<std::string> args) {
    return {"cheat " + args[0], [args](STest& t, Scene& s, int) {
                if (!s.cheat(args)) t.fail("cheat refused: " + args[0]);
                return true;
            }};
}
// Click a cell relative to the probe's own.cx/own.cy (first own unit).
static STest::Step st_click_own_rel(int dx, int dy, bool right = false) {
    return {"click own+(" + std::to_string(dx) + "," + std::to_string(dy) + ")" + (right ? " right" : ""),
            [=](STest& t, Scene& s, int) {
                const auto p = STest::probe_of(s);
                const std::string w = "cell:" + std::to_string(p.at("own.cx") + dx) + "," +
                                      std::to_string(p.at("own.cy") + dy);
                if (!t.click(s, w, right)) t.fail("cannot locate " + w);
                return true;
            }};
}

// Click candidate cells around the first own building until the pending
// placement is accepted (bld.0 grows) -- one candidate per frame.
static STest::Step st_place_near_base() {
    STest::Step st{"place the ready building next to the base", [](STest& t, Scene& s, int) {
        const auto p = STest::probe_of(s);
        if (!t.mem.count("pl_k")) { t.mem["pl_k"] = 0; t.mem["pl_b"] = p.count("bld.0") ? p.at("bld.0") : 0; }
        const long long nb = p.count("bld.0") ? p.at("bld.0") : 0;
        if (nb > t.mem["pl_b"]) { t.mem.erase("pl_k"); return true; }
        static const int kOff[][2] = {{6, 1}, {1, 6}, {-3, 1}, {1, -3}, {6, 4}, {4, 6}, {-3, 4},
                                      {7, -1}, {-1, 7}, {-4, -1}, {7, 7}, {-4, 6}, {6, -3}};
        const long long k = t.mem["pl_k"]++;
        if (k >= 13) return false;
        const std::string w = "cell:" + std::to_string(p.at("bld0.cx") + kOff[k][0]) + "," +
                              std::to_string(p.at("bld0.cy") + kOff[k][1]);
        t.click(s, w);
        return false;
    }};
    st.timeout = 40;
    return st;
}

void add_controls_selftest_steps(STest& t) {
    auto& S = t.steps;
    // --- G-01: Esc in the shell / lobby is Back, never program exit ---
    S.push_back(st_wait_scene("shell"));
    S.push_back(st_key("Escape on main page", SDLK_ESCAPE));
    S.push_back(st_wait(2));
    S.push_back(st_wait_scene("shell"));
    S.push_back(st_click("button:Single Player"));
    S.push_back(st_expect("page", PAGE_SINGLE));
    S.push_back(st_key("Escape on Single Player page", SDLK_ESCAPE));
    S.push_back(st_expect("page", PAGE_MAIN));
    S.push_back(st_click("button:Single Player"));
    S.push_back(st_click("button:Skirmish"));
    S.push_back(st_wait_scene("lobby"));
    S.push_back(st_key("Escape in lobby", SDLK_ESCAPE));
    S.push_back(st_wait_scene("shell"));
    S.push_back(st_click("button:Single Player"));
    S.push_back(st_click("button:Skirmish"));
    S.push_back(st_wait_scene("lobby"));
    S.push_back(st_click("start"));   // R-183: the 0x102 page's Start Game (alias)
    S.push_back(st_wait_scene("game"));
    S.push_back(st_wait(5));
    S.push_back(st_shot("game_start.png"));
    // --- G-01: Esc in game opens the options dialog; Esc again resumes ---
    S.push_back(st_key("Escape in game", SDLK_ESCAPE));
    S.push_back(st_expect("menu", 1));
    S.push_back(st_shot("game_esc_menu.png"));
    S.push_back(st_key("Escape on options dialog", SDLK_ESCAPE));
    S.push_back(st_expect("menu", 0));
    S.push_back(st_wait_scene("game"));
    // --- G-05: the original LEFT-click command scheme in the real GameScene ---
    S.push_back(st_expect_ge("units.0", 1));
    S.push_back(st_cheat({"center_own"}));
    S.push_back(st_wait(1));
    S.push_back(st_click("unit:0:GI:0"));
    S.push_back(st_expect_ge("sel", 1));
    S.push_back(st_click_own_rel(5, 0));          // LEFT click on ground = move
    S.push_back(st_expect_ge("sel.moving", 1, 5));
    S.push_back(st_shot("game_left_move.png"));
    S.push_back(st_click_own_rel(5, 3, true));    // right click = deselect
    S.push_back(st_expect("sel", 0));
    // --- G-10: an own building is selectable with an HP bar; right deselects ---
    S.push_back(st_click("building:0:GACNST:0"));
    S.push_back(st_expect_ge("selb", 0));
    S.push_back(st_shot("game_building_selected.png"));
    S.push_back(st_click_own_rel(2, 2, true));
    S.push_back(st_expect("selb", -1));    // --- G-04: KeyboardMD.ini hotkeys through the real event path ---
    S.push_back({"remember cam", [](STest& t, Scene& s, int) {
                     t.mem["cam_x"] = STest::pv(s, "cam_x");
                     return true;
                 }});
    S.push_back(st_click_own_rel(12, 12, true));          // (deselect)
    S.push_back(st_click("unit:0:GI:0"));
    S.push_back(st_key("Ctrl+1 (TeamCreate_1)", SDLK_1, /*ctrl=*/true));
    S.push_back(st_click_own_rel(12, 12, true));
    S.push_back(st_expect("sel", 0));
    S.push_back(st_key("1 (TeamSelect_1)", SDLK_1));
    S.push_back(st_expect("sel", 1));
    S.push_back(st_key("T (TypeSelect)", SDLK_t));
    S.push_back({"T selected every on-screen GI", [](STest&, Scene& s, int) {
                     return STest::pv(s, "sel") == STest::pv(s, "units.0.GI") ||
                            STest::pv(s, "sel") > 1;
                 }});
    S.push_back(st_click_own_rel(6, 0));                   // move...
    S.push_back(st_expect_ge("moving.0", 1, 5));
    S.push_back(st_key("S (StopObject)", SDLK_s));
    S.push_back(st_expect("moving.0", 0, 30));
    S.push_back(st_key("W (DefenseTab)", SDLK_w));
    S.push_back(st_expect("tab", 1));
    S.push_back(st_key("E (InfantryTab)", SDLK_e));
    S.push_back(st_expect("tab", 2));
    S.push_back(st_key("R (UnitTab)", SDLK_r));
    S.push_back(st_expect("tab", 3));
    S.push_back(st_key("Q (StructureTab)", SDLK_q));
    S.push_back(st_expect("tab", 0));
    S.push_back(st_key("K (ToggleRepair)", SDLK_k));
    S.push_back(st_expect("mode", 1));
    S.push_back(st_key("L (ToggleSell)", SDLK_l));
    S.push_back(st_expect("mode", 2));
    S.push_back(st_click_own_rel(12, 12, true));          // right-click cancels the mode
    S.push_back(st_expect("mode", 0));
    S.push_back(st_key("Ctrl+F1 (SetView1)", SDLK_F1, true));
    S.push_back(st_cheat({"pan", "300", "200"}));
    S.push_back(st_key("F1 (View1)", SDLK_F1));
    S.push_back({"View1 restored the camera", [](STest& t, Scene& s, int) {
                     return STest::pv(s, "cam_x") == t.mem["cam_x"];
                 }});
    S.push_back(st_cheat({"pan", "-900", "-400"}));
    S.push_back(st_key("H (CenterBase)", SDLK_h));
    S.push_back(st_shot("game_center_base.png"));
    S.push_back({"H centred the camera near the start", [](STest& t, Scene& s, int) {
                     return std::llabs(STest::pv(s, "cam_x") - t.mem["cam_x"]) < 400;
                 }});    // --- G-07: build-queue click semantics (StripClass FUN_006AA390) ---
    S.push_back(st_key("Q", SDLK_q));
    S.push_back(st_cheat({"center_own"}));
    S.push_back(st_click("cameo:GAPOWR"));                      // LEFT = start
    S.push_back(st_expect_ge("q.structure", 0));
    S.push_back(st_wait(20));
    S.push_back(st_click("cameo:GAPOWR"));                      // LEFT on running = no effect
    S.push_back(st_expect_ge("q.structure", 0));
    S.push_back(st_click("cameo:GAPOWR", true));                // RIGHT = hold
    S.push_back(st_expect("held", 1));
    S.push_back({"remember stage", [](STest& t, Scene& s, int) {
                     t.mem["stage"] = STest::pv(s, "q.structure.stage");
                     return true;
                 }});
    S.push_back(st_wait(30));
    S.push_back({"held queue did not advance", [](STest& t, Scene& s, int) {
                     return STest::pv(s, "q.structure.stage") == t.mem["stage"];
                 }});
    S.push_back(st_shot("queue_hold.png"));
    S.push_back(st_click("cameo:GAPOWR"));                      // LEFT on held = resume
    S.push_back(st_expect("held", 0));
    S.push_back(st_wait(30));
    S.push_back({"resumed queue advanced", [](STest& t, Scene& s, int) {
                     return STest::pv(s, "q.structure.stage") > t.mem["stage"];
                 }});
    S.push_back({"remember credits", [](STest& t, Scene& s, int) {
                     t.mem["credits"] = STest::pv(s, "credits");
                     return true;
                 }});
    S.push_back(st_click("cameo:GAPOWR", true));                // hold
    S.push_back(st_click("cameo:GAPOWR", true));                // abandon (refund)
    S.push_back(st_expect("q.structure", -1));
    S.push_back({"abandon refunded the spend", [](STest& t, Scene& s, int) {
                     return STest::pv(s, "credits") > t.mem["credits"];
                 }});
    // Build power + barracks for real, then queue infantry.
    S.push_back(st_click("cameo:GAPOWR"));
    S.push_back(st_expect("q.structure.ready", 1, 900));
    S.push_back(st_click("cameo:GAPOWR"));                      // LEFT on READY = place
    S.push_back(st_expect("placing", 1));
    S.push_back(st_place_near_base());
    S.push_back({"start GAPILE once its prerequisite power plant has built up", [](STest& t, Scene& s, int f) {
                     if (STest::pv(s, "q.structure") >= 0) return true;
                     if (f % 10 == 0) t.click(s, "cameo:GAPILE");
                     return false;
                 }, 400});
    S.push_back(st_expect("q.structure.ready", 1, 900));
    S.push_back(st_click("cameo:GAPILE"));
    S.push_back(st_place_near_base());
    S.push_back(st_expect_ge("bld.0.GAPILE", 1));
    S.push_back(st_wait(120));                                   // barracks build-up
    S.push_back(st_key("E", SDLK_e));
    S.push_back({"remember GI count", [](STest& t, Scene& s, int) {
                     t.mem["gi"] = STest::pv(s, "units.0.GI");
                     return true;
                 }});
    S.push_back(st_click("cameo:E1"));                           // start
    S.push_back(st_click("cameo:E1"));                           // +1
    S.push_back(st_click("cameo:E1"));                           // +1
    S.push_back(st_click("cameo:E1"));                           // +1
    S.push_back(st_expect("backlog.2", 3));
    S.push_back(st_shot("queue_count.png"));
    S.push_back(st_click("cameo:E1", true));                     // hold current
    S.push_back(st_expect("held", 4));
    S.push_back(st_click("cameo:E1", true));                     // abandon one queued copy
    S.push_back(st_expect("backlog.2", 2));
    S.push_back(st_click("cameo:E1"));                           // resume
    S.push_back(st_expect("held", 0));
    S.push_back({"three GIs come out of the queue", [](STest& t, Scene& s, int) {
                     return STest::pv(s, "units.0.GI") >= t.mem["gi"] + 3;
                 }, 2400});
    S.push_back(st_expect("backlog.2", 0));
    S.push_back(st_shot("queue_done.png"));    // --- G-09: radar click: re-centre, and with a selection a move order ---
    S.push_back({"add a powered Air Force Command (radar) next to the base", [](STest& t, Scene& s, int) {
                     const auto p = STest::probe_of(s);
                     const std::string x = std::to_string(p.at("bld0.cx") - 6), y = std::to_string(p.at("bld0.cy") - 6);
                     if (!s.cheat({"building", "GAAIRC", x, y})) t.fail("cheat building");
                     return true;
                 }});
    S.push_back(st_expect("radar", 1, 90));   // online after the power-up animation
    S.push_back(st_click_own_rel(14, 14, true));               // deselect
    S.push_back({"remember cam", [](STest& t, Scene& s, int) {
                     t.mem["cam_x"] = STest::pv(s, "cam_x");
                     t.mem["cam_y"] = STest::pv(s, "cam_y");
                     return true;
                 }});
    S.push_back({"click the radar near its top-left", [](STest& t, Scene&, int) {
                     t.click_xy(1280 - 168 + 120, 48 + 80);
                     return true;
                 }});
    S.push_back({"radar click re-centred the view", [](STest& t, Scene& s, int) {
                     return STest::pv(s, "cam_x") != t.mem["cam_x"] || STest::pv(s, "cam_y") != t.mem["cam_y"];
                 }});
    S.push_back(st_shot("radar_center.png"));
    S.push_back(st_key("H", SDLK_h));
    S.push_back(st_click("unit:0:GI:0"));
    S.push_back(st_expect_ge("sel", 1));
    S.push_back({"remember cam", [](STest& t, Scene& s, int) {
                     t.mem["cam_x"] = STest::pv(s, "cam_x");
                     return true;
                 }});
    S.push_back({"click the radar on a land cell near the GI, with a selection", [](STest& t, Scene& s, int) {
                     const auto p = STest::probe_of(s);
                     const std::string w = "radar:" + std::to_string(p.at("own.cx") - 4) + "," +
                                           std::to_string(p.at("own.cy") + 2);
                     if (!t.click(s, w)) t.fail("cannot locate " + w);
                     return true;
                 }});
    S.push_back(st_expect_ge("sel.moving", 1, 5));
    S.push_back({"a radar order does not move the camera", [](STest& t, Scene& s, int) {
                     return STest::pv(s, "cam_x") == t.mem["cam_x"];
                 }});    // --- G-70/G-71: touch through the real gesture layer ---
    S.push_back(st_key("H", SDLK_h));
    S.push_back(st_click_own_rel(14, 14, true));               // deselect (mouse)
    S.push_back({"tap a GI", [](STest& t, Scene& s, int) {
                     int x = 0, y = 0;
                     if (!s.locate("unit:0:GI:0", x, y)) t.fail("no GI");
                     t.touch_tap(x, y);
                     return true;
                 }});
    S.push_back(st_expect("sel", 1));                          // ONE tap = select (not a double)
    S.push_back({"tap the ground = move order", [](STest& t, Scene& s, int) {
                     const auto p = STest::probe_of(s);
                     int x = 0, y = 0;
                     s.locate("cell:" + std::to_string(p.at("own.cx") - 4) + "," + std::to_string(p.at("own.cy") + 2), x, y);
                     t.touch_tap(x, y);
                     return true;
                 }});
    S.push_back(st_expect_ge("sel.moving", 1, 5));
    S.push_back({"remember cam", [](STest& t, Scene& s, int) {
                     t.mem["cam_x"] = STest::pv(s, "cam_x");
                     t.mem["cam_y"] = STest::pv(s, "cam_y");
                     return true;
                 }});
    S.push_back({"two-finger pan", [](STest& t, Scene&, int) {
                     t.touch_pan(400, 300, 120, 60);
                     return true;
                 }});
    S.push_back({"the map followed the fingers", [](STest& t, Scene& s, int) {
                     return STest::pv(s, "cam_x") - t.mem["cam_x"] == 120 &&
                            STest::pv(s, "cam_y") - t.mem["cam_y"] == 60;
                 }});
    S.push_back({"hold = cancel a mode, never deselect (W4-AM-07)", [](STest& t, Scene&, int) {
                     t.touch_long(500, 400);
                     return true;
                 }});
    S.push_back(st_expect("sel", 1));
    S.push_back({"two-finger tap = right click (deselect)", [](STest& t, Scene&, int) {
                     t.touch_two_tap(500, 400);
                     return true;
                 }});
    S.push_back(st_expect("sel", 0));
    S.push_back({"a tap at the screen edge", [](STest& t, Scene& s, int) {
                     t.mem["cam_x"] = STest::pv(s, "cam_x");
                     t.touch_tap(3, 360);
                     return true;
                 }});
    S.push_back(st_wait(20));
    S.push_back({"no sticky edge scroll after a touch", [](STest& t, Scene& s, int) {
                     return STest::pv(s, "cam_x") == t.mem["cam_x"];
                 }});
    S.push_back(st_shot("touch_done.png"));
}
