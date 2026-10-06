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
    // (scheme / hotkey / group / minimap / queue / touch steps follow)
}
