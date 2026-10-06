// Controls/UI real-play self-test (--controls-selftest <outdir>), dev-only.
//
// Drives the REAL scene tree (MainMenuScene -> SkirmishSetupScene ->
// GameScene) through run_scene_driver with scripted InputEvents at a fixed
// 1/15 s tick, in a hidden window, and checks the scenes' probe() after each
// step. This is agent B's acceptance evidence for the controls fixes (G-01,
// the original left-click command scheme, hotkeys, groups, minimap, queues,
// touch); the general-purpose --play harness is agent G's. Not a gate.

#include <deque>
#include <functional>

class ControlsSelftest : public DriverHooks, public ra2engine::EventSource {
public:
    explicit ControlsSelftest(std::string outdir) : outdir_(std::move(outdir)) {}

    struct Step {
        std::string what;
        // Runs once per frame until it returns true (then the next step
        // starts on the following frame). May queue events.
        std::function<bool(ControlsSelftest&, Scene&, int)> run;
        int timeout = 300;   // frames before the step FAILs
    };
    std::vector<Step> steps;

    bool poll(ra2engine::InputEvent& ev) override {
        if (q_.empty()) return false;
        ev = q_.front();
        q_.pop_front();
        return true;
    }

    bool begin(int f, Scene& s) override {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {}   // the hidden window's own events are ignored
        frame_ = f;
        if (done_) return false;
        if (cur_ >= static_cast<int>(steps.size())) { done_ = true; return false; }
        Step& st = steps[cur_];
        if (step_start_ < 0) step_start_ = f;
        if (st.run(*this, s, f)) {
            std::printf("selftest ok   f=%-5d %s\n", f, st.what.c_str());
            ++cur_;
            step_start_ = -1;
        } else if (f - step_start_ > st.timeout) {
            fail("timeout: " + st.what);
            done_ = true;
        }
        return true;
    }
    void end(int f, Scene& s, ra2engine::Platform& plat) override {
        if (!shot_.empty()) {
            try {
                capture_png(static_cast<SDL_Renderer*>(plat.renderer()), outdir_ + "/" + shot_,
                            plat.width(), plat.height());
                std::printf("selftest shot f=%-5d %s\n", f, shot_.c_str());
            } catch (const std::exception& ex) {
                fail(std::string("shot: ") + ex.what());
            }
            shot_.clear();
        }
        hash_ = ra2engine::play::fnv1a(std::string(s.name()) + ' ' + ra2engine::play::probe_line(probe_of(s)) + '\n', hash_);
    }
    void quit(int f, Scene* s, const char* why) override {
        if (!done_) {
            fail(std::string("process would quit at f=") + std::to_string(f) + " (" + why +
                 ") in scene " + (s ? s->name() : "?"));
            done_ = true;
        }
    }

    // --- helpers used by the step lambdas ---
    void fail(const std::string& m) {
        ++failures_;
        std::printf("selftest FAIL f=%-5d %s\n", frame_, m.c_str());
    }
    ra2engine::InputEvent ptr(ra2engine::InputEvent::Kind k, int x, int y, bool right = false) {
        ra2engine::InputEvent e;
        e.kind = k;
        e.x = x;
        e.y = y;
        e.button = right ? ra2engine::InputEvent::Right : ra2engine::InputEvent::Left;
        e.t_ms = static_cast<uint32_t>(frame_ * 67 + 1000);
        e.shift = shift_;
        e.ctrl = ctrl_;
        e.alt = alt_;
        e.touch = touch_;
        return e;
    }
    void click_xy(int x, int y, bool right = false) {
        q_.push_back(ptr(ra2engine::InputEvent::PointerMove, x, y));
        q_.push_back(ptr(ra2engine::InputEvent::PointerDown, x, y, right));
        q_.push_back(ptr(ra2engine::InputEvent::PointerUp, x, y, right));
    }
    bool click(Scene& s, const std::string& what, bool right = false) {
        int x = 0, y = 0;
        if (!s.locate(what, x, y)) return false;
        click_xy(x, y, right);
        return true;
    }
    void drag_xy(int x0, int y0, int x1, int y1, bool right = false) {
        q_.push_back(ptr(ra2engine::InputEvent::PointerMove, x0, y0));
        q_.push_back(ptr(ra2engine::InputEvent::PointerDown, x0, y0, right));
        for (int i = 1; i <= 4; ++i)
            q_.push_back(ptr(ra2engine::InputEvent::PointerMove, x0 + (x1 - x0) * i / 4,
                             y0 + (y1 - y0) * i / 4));
        q_.push_back(ptr(ra2engine::InputEvent::PointerUp, x1, y1, right));
    }
    void key(int sdl_key, bool ctrl = false, bool shift = false, bool alt = false) {
        ra2engine::InputEvent e;
        int mod = 0;
        if (ctrl) mod |= KMOD_LCTRL;
        if (shift) mod |= KMOD_LSHIFT;
        if (alt) mod |= KMOD_LALT;
        ra2engine::key_event_from_sdl(sdl_key, mod, e);
        e.t_ms = static_cast<uint32_t>(frame_ * 67 + 1000);
        q_.push_back(e);
    }
    void push(const ra2engine::InputEvent& e) { q_.push_back(e); }
    static long long pv(const Scene& s, const std::string& k) {
        std::map<std::string, long long> p;
        s.probe(p);
        const auto it = p.find(k);
        return it == p.end() ? 0 : it->second;
    }
    static std::map<std::string, long long> probe_of(const Scene& s) {
        std::map<std::string, long long> p;
        s.probe(p);
        return p;
    }
    void shot(const std::string& n) { shot_ = n; }
    int frame() const { return frame_; }
    int failures() const { return failures_; }
    bool finished_all() const { return cur_ >= static_cast<int>(steps.size()); }
    bool shift_ = false, ctrl_ = false, alt_ = false, touch_ = false;
    std::map<std::string, long long> mem;   // scratch values between steps

private:
    std::string outdir_;
    std::deque<ra2engine::InputEvent> q_;
    int cur_ = 0, step_start_ = -1, frame_ = 0, failures_ = 0;
    bool done_ = false;
    uint64_t hash_ = 1469598103934665603ULL;
public:
    uint64_t probe_hash() const { return hash_; }
    std::string shot_;
};

// The scenario list (agent B's controls). Each later fix appends steps.
#include "controls_selftest_steps.inc"

int run_controls_selftest(const std::string& outdir) {
    const std::string dir = env_install_dir();
    Ini rules_ini = load_ini_from_ra2md(dir, "rulesmd.ini");
    Rules rules = Rules::load(rules_ini);
    ra2engine::Platform plat = ra2engine::Platform::create("RA2 selftest", 1280, 720, true);
    ra2engine::TextureCache tc(plat.renderer());
    MenuArtStore store;
    load_menu_art(dir, /*no_audio=*/true, store);
    ra2engine::Settings user;   // OptionsClass defaults (never the user's RA2.INI)
    g_user_settings = &user;
    g_menu_rules = &rules;
    g_menu_art = &store.art;
    std::error_code ec;
    std::filesystem::create_directories(outdir, ec);
    ControlsSelftest t(outdir);
    add_controls_selftest_steps(t);
    run_scene_driver(plat, tc, new MainMenuScene(rules, /*no_audio=*/true, store.art), &t,
                     1.0 / 15.0, &t);
    g_menu_rules = nullptr;
    g_menu_art = nullptr;
    g_user_settings = nullptr;
    const bool pass = t.failures() == 0 && t.finished_all();
    std::printf("selftest %s (%d failure(s), frame %d) probe_hash=%016llx\n", pass ? "PASS" : "FAIL",
                t.failures(), t.frame(), static_cast<unsigned long long>(t.probe_hash()));
    return pass ? 0 : 1;
}
