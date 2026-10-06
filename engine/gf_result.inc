// gF: the skirmish result screen [RE: docs/re/mcv-deploy.md §5]. The original
// shows TXT_VICTORIOUS / TXT_LOST during BorrowedTime and then its MP score
// screen before returning to the shell; the score screen is not recovered, so
// this page carries the outcome, the game time and the kill/loss tallies
// [RE-FALLBACK: score screen layout], then MAIN MENU or REMATCH (same lobby
// config). Included after campaign_scenes.inc (needs CampButton + GameScene).
class GfSkirmishResultScene : public Scene {
public:
    GfSkirmishResultScene(bool no_audio, ra2engine::SkirmishConfig cfg, int outcome, long frames,
                          GfSkirmishStats st)
        : no_audio_(no_audio), cfg_(std::move(cfg)), outcome_(outcome), frames_(frames), st_(st) {
        const std::string dir = env_install_dir();
        head_ = camp::up(camp::csf_text(dir, outcome_ > 0 ? "TXT_VICTORIOUS" : "TXT_LOST"));
        if (head_ == "TXT_VICTORIOUS") head_ = "YOU ARE VICTORIOUS";
        if (head_ == "TXT_LOST") head_ = "YOU HAVE LOST";
    }
    void handle(const ra2engine::InputEvent& ev) override {
        if (ev.kind == ra2engine::InputEvent::PointerMove || ev.kind == ra2engine::InputEvent::PointerDown) {
            mx_ = ev.x;
            my_ = ev.y;
        }
        if (ev.kind != ra2engine::InputEvent::PointerDown || ev.button != ra2engine::InputEvent::Left) return;
        if (again_btn().r.contains(ev.x, ev.y)) choice_ = 1;
        if (menu_btn().r.contains(ev.x, ev.y)) choice_ = 2;
    }
    void render(ra2engine::Platform& plat, ra2engine::TextureCache& tc) override {
        (void)tc;
        SDL_Renderer* ren = static_cast<SDL_Renderer*>(plat.renderer());
        SDL_SetRenderDrawColor(ren, 8, 10, 16, 255);
        SDL_RenderClear(ren);
        const int w = static_cast<int>(head_.size()) * 24;
        draw_text(ren, head_, (1280 - w) / 2, 180, 4, 255, 220, 90);
        char b[96];
        std::snprintf(b, sizeof b, "TIME %02ld:%02ld", frames_ / 15 / 60, (frames_ / 15) % 60);
        draw_text(ren, b, (1280 - (int)std::strlen(b) * 12) / 2, 250, 2, 200, 200, 200);
        std::snprintf(b, sizeof b, "UNITS DESTROYED %d   UNITS LOST %d", st_.units_killed, st_.units_lost);
        draw_text(ren, b, (1280 - (int)std::strlen(b) * 12) / 2, 290, 2, 200, 200, 200);
        std::snprintf(b, sizeof b, "BUILDINGS DESTROYED %d   BUILDINGS LOST %d", st_.buildings_killed,
                      st_.buildings_lost);
        draw_text(ren, b, (1280 - (int)std::strlen(b) * 12) / 2, 320, 2, 200, 200, 200);
        draw_camp_button(ren, again_btn(), again_btn().r.contains(mx_, my_));
        draw_camp_button(ren, menu_btn(), menu_btn().r.contains(mx_, my_));
    }
    Scene* next() override {
        const int c = choice_;
        choice_ = 0;
        if (c == 1) return new GameScene(no_audio_, cfg_);
        if (c == 2) {
            if (Scene* m = make_main_menu_scene(no_audio_)) return m;
            quit_ = true;
        }
        return nullptr;
    }
    bool wants_quit() const override { return quit_; }
    // G0 --play: the skirmish result page is the session's verdict.
    void play_probe(ra2play::Probe& p) override {
        p.scene = "result";
        p.set_s("outcome", outcome_ > 0 ? "WON" : "LOST");
        p.set("result_frames", static_cast<double>(frames_));
        const CampButton a = again_btn(), m = menu_btn();
        p.targets["rematch"] = ra2play::Rect{a.r.x, a.r.y, a.r.w, a.r.h};
        p.targets["menu"] = ra2play::Rect{m.r.x, m.r.y, m.r.w, m.r.h};
    }
    // Test seam: the choice a click would make (1 rematch, 2 menu).
    void choose(int c) { choice_ = c; }

private:
    static CampButton again_btn() { return {{460, 440, 360, 46}, "REMATCH", true}; }
    static CampButton menu_btn() { return {{460, 500, 360, 46}, "MAIN MENU", true}; }
    bool no_audio_;
    ra2engine::SkirmishConfig cfg_;
    int outcome_;
    long frames_;
    GfSkirmishStats st_;
    std::string head_;
    int choice_ = 0;
    bool quit_ = false;
    int mx_ = 0, my_ = 0;
};

Scene* gf_make_skirmish_result_scene(bool no_audio, const ra2engine::SkirmishConfig& cfg, int outcome,
                                     long frames, const GfSkirmishStats& stats) {
    return new GfSkirmishResultScene(no_audio, cfg, outcome, frames, stats);
}

Scene* gf_make_restart_scene(bool no_audio, const ra2engine::SkirmishConfig& cfg) {
    if (!cfg.campaign_map.empty()) return new CampaignLoadScene(no_audio, cfg);
    return new GameScene(no_audio, cfg);
}
