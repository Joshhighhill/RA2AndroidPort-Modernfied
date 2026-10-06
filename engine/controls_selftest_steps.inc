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
                   [key, v](STest& t, Scene& s, int) { return STest::pv(s, key) == v; }};
    st.timeout = timeout;
    return st;
}
static STest::Step st_expect_ge(const std::string& key, long long v, int timeout = 30) {
    STest::Step st{"expect " + key + " >= " + std::to_string(v),
                   [key, v](STest& t, Scene& s, int) { return STest::pv(s, key) >= v; }};
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
    S.push_back(st_click("button:Start"));
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
    S.push_back(st_expect_ge("moving.0", 1, 5));
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
    S.push_back({"T selected every on-screen GI", [](STest& t, Scene& s, int) {
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
                 }});    // (minimap / queue / touch steps follow)
}
