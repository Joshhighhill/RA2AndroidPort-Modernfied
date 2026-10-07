// P10 Campaign shell scenes [RE: docs/re/campaign.md §3].
// Textually included by main.cpp after the shell scenes (MainMenuScene) and
// GameScene are defined. Interactive only: no gate constructs these.
//
//   Main menu > Single Player > New Campaign > Allied / Soviet
//     -> CampaignLoadScene (loading screen: LS800 art, mission name,
//        LSLoadBriefing text, difficulty)  -> GameScene (campaign mode)
//     -> CampaignResultScene (won/lost)    -> next mission / retry / menu

// The menu's long-lived state, published by run_menu (like g_user_settings)
// so a finished/aborted mission can return to the main menu.
const Rules* g_menu_rules = nullptr;
// g_menu_art lives in shell_pages.inc (W2-UI-13: also the in-game pages' art).

Scene* make_main_menu_scene(bool no_audio) {
    if (!g_menu_rules || !g_menu_art) return nullptr;
    return new MainMenuScene(*g_menu_rules, no_audio, *g_menu_art);
}

// A simple clickable button in device pixels.
struct CampButton {
    ra2engine::SbRect r;
    const char* label;
    bool enabled;
};
void draw_camp_button(SDL_Renderer* ren, const CampButton& b, bool hover) {
    SDL_Rect s{b.r.x, b.r.y, b.r.w, b.r.h};
    SDL_SetRenderDrawColor(ren, b.enabled ? (hover ? 110 : 70) : 40, b.enabled ? 30 : 30,
                           b.enabled ? 26 : 30, 255);
    SDL_RenderFillRect(ren, &s);
    SDL_SetRenderDrawColor(ren, b.enabled ? 210 : 90, b.enabled ? 70 : 90, b.enabled ? 60 : 90, 255);
    SDL_RenderDrawRect(ren, &s);
    const int lw = static_cast<int>(std::strlen(b.label)) * 12;
    draw_text(ren, b.label, b.r.x + (b.r.w - lw) / 2, b.r.y + (b.r.h - 14) / 2, 2,
              b.enabled ? 235 : 120, b.enabled ? 235 : 120, b.enabled ? 240 : 120);
}

// Word-wrap into the 5x7 glyph font.
int draw_camp_wrapped(SDL_Renderer* ren, const std::string& text, int x, int y, int cols, int scale,
                      int max_y, Uint8 r, Uint8 g, Uint8 b) {
    const std::string t = camp::up(text);
    std::size_t p = 0;
    while (p < t.size() && y < max_y) {
        std::size_t n = std::min<std::size_t>((std::size_t)cols, t.size() - p);
        const std::size_t nl = t.find('\n', p);
        if (nl != std::string::npos && nl < p + n) {
            n = nl - p;
        } else if (p + n < t.size()) {
            const std::size_t sp = t.rfind(' ', p + n);
            if (sp != std::string::npos && sp > p) n = sp - p;
        }
        draw_text(ren, t.substr(p, n), x, y, scale, r, g, b);
        y += 10 * scale;
        p += n + 1;
    }
    return y;
}

// ---------------------------------------------------------------------------
// The loading screen [RE §3.2]: mission.ini LS800BkgdName (ra2.mix/load.mix)
// drawn with the side's loading palette (ldscrna.pal / ldscrns.pal -- the
// "LDSCRN%c" names next to it in game.exe; PROBABLE: A = Allied, S = Soviet),
// the UIName + LSLoadBriefing CSF text, and the difficulty choice.
class CampaignLoadScene : public Scene {
public:
    CampaignLoadScene(bool no_audio, ra2engine::SkirmishConfig cfg)
        : no_audio_(no_audio), cfg_(std::move(cfg)), dir_(env_install_dir()) {
        try {
            ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir_ + "ra2.mix"));
            hud_font_load(ra2);   // W2-UI-19: the loading text is game.fnt
            const ra2engine::CampaignTables tables = camp::load_tables(ra2);
            const ra2engine::MissionInfo mi = tables.mission(cfg_.campaign_map);
            name_ = mi.ui_name.empty() ? cfg_.campaign_map : camp::csf_text(dir_, mi.ui_name);
            text_ = mi.ls_load_briefing.empty() ? std::string() : camp::csf_text(dir_, mi.ls_load_briefing);
            msg_ = mi.ls_load_message.empty() ? std::string() : camp::csf_text(dir_, mi.ls_load_message);
            MixFile load = ra2.open_nested("load.mix");
            if (const MixEntry* e = load.find(camp::lower(mi.ls800_bkgd))) {
                bg_ = ra2assets::ShpFile::load(load.read(*e));
                const std::string pal = camp::up(cfg_.campaign_side) == "NOD" ? "ldscrns.pal" : "ldscrna.pal";
                if (const MixEntry* pe = load.find(pal)) {
                    const auto raw = load.read(*pe);
                    pal_ = ra2assets::Palette::load(raw.data(), raw.size());
                    has_bg_ = bg_.count() > 0;
                }
            }
        } catch (const std::exception& ex) {
            std::cerr << "[campaign] loading screen art unavailable (" << ex.what() << ")\n";
        }
        // Dev/smoke aid: RA2_CAMP_AUTOSTART=1 skips the click.
        if (const char* e = std::getenv("RA2_CAMP_AUTOSTART")) start_ = std::atoi(e) != 0;
    }
    // W2-UI-19: the difficulty is chosen on page 0x94 now; the loading screen
    // is the LS800 art full-screen with the CSF text in the game font and a
    // progress bar, and starts the mission by itself (Enter / a click start
    // it at once). [RE-FALLBACK: text anchors (the art's dark top-left box),
    // the bar (the art's bottom strip, UI colour) and the 1.5 s fill.]
    void handle(const ra2engine::InputEvent& ev) override {
        if (ev.kind == ra2engine::InputEvent::Key && ev.key == ra2engine::kKeyEnter) start_ = true;
        if (ev.kind == ra2engine::InputEvent::PointerDown && ev.button == ra2engine::InputEvent::Left) start_ = true;
    }
    void update(double dt) override {
        clock_ += dt;
        if (clock_ >= kAutoStart) start_ = true;
    }
    void render(ra2engine::Platform& plat, ra2engine::TextureCache& tc) override {
        SDL_Renderer* ren = static_cast<SDL_Renderer*>(plat.renderer());
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        // Art letterboxed into the 1280x720 window (aspect kept).
        const int aw = has_bg_ ? bg_.width() : 800, ah = has_bg_ ? bg_.height() : 600;
        const double sc = std::min(1280.0 / aw, 720.0 / ah);
        const int dw = static_cast<int>(aw * sc), dh = static_cast<int>(ah * sc);
        const int ox = (1280 - dw) / 2, oy = (720 - dh) / 2;
        if (has_bg_)
            if (SDL_Texture* t = shp_frame_tex(tc, "camp-ls:" + cfg_.campaign_map, bg_, 0, pal_)) {
                const ra2assets::ShpFile::Frame f = bg_.frame_info(0);
                SDL_Rect d{ox + static_cast<int>(f.x * sc), oy + static_cast<int>(f.y * sc),
                           static_cast<int>(f.w * sc), static_cast<int>(f.h * sc)};
                SDL_RenderCopy(ren, t, nullptr, &d);
            }
        const bool soviet = camp::up(cfg_.campaign_side) == "NOD";
        const SDL_Color col = soviet ? SDL_Color{255, 255, 0, 255} : SDL_Color{164, 210, 255, 255};
        const int tx = ox + static_cast<int>(24 * sc), maxw = static_cast<int>(560 * sc / 1.5);
        int y = oy + static_cast<int>(20 * sc);
        hud_text(ren, tc, name_, tx, y, col, kHudLeft, false, true, 2.0);
        y += 2 * (hud_line_h() + 4);
        if (!text_.empty()) {
            const std::string t = wrap_hud(text_, maxw);
            hud_text(ren, tc, t, tx, y, col, kHudLeft, false, true, 1.5);
            y += static_cast<int>(1.5 * (hud_text_height(t) + 6));
        }
        if (!msg_.empty()) hud_text(ren, tc, wrap_hud(msg_, maxw), tx, y, col, kHudLeft, false, true, 1.5);
        // Progress bar.
        const SDL_Rect bar{ox + static_cast<int>(aw * sc * 0.25), oy + dh - static_cast<int>(18 * sc),
                           static_cast<int>(aw * sc * 0.5), static_cast<int>(8 * sc)};
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 0xAF);
        SDL_RenderFillRect(ren, &bar);
        const double frac = loading_ ? 1.0 : std::min(1.0, clock_ / kAutoStart);
        SDL_Rect fill{bar.x + 1, bar.y + 1, static_cast<int>((bar.w - 2) * frac), bar.h - 2};
        SDL_SetRenderDrawColor(ren, col.r, col.g, col.b, 255);
        SDL_RenderFillRect(ren, &fill);
        SDL_RenderDrawRect(ren, &bar);
    }
    Scene* next() override {
        if (back_) {
            back_ = false;
            if (Scene* m = make_main_menu_scene(no_audio_)) return m;
            quit_ = true;
            return nullptr;
        }
        if (!start_) return nullptr;
        // Draw one full-bar frame before the (multi-second) GameScene build.
        if (!loading_) { loading_ = true; return nullptr; }
        start_ = false;
        return new GameScene(no_audio_, cfg_);
    }
    bool wants_quit() const override { return quit_; }
    // G0 --play: the chosen mission; `start` = anywhere (a click starts).
    void play_probe(ra2play::Probe& p) override {
        p.scene = "campaign-load";
        p.set_s("map", cfg_.campaign_map);
        p.set_s("side", cfg_.campaign_side);
        p.set("difficulty", cfg_.campaign_difficulty);
        p.targets["start"] = ra2play::Rect{560, 300, 160, 120};
    }

private:
    static constexpr double kAutoStart = 1.5;
    static std::string wrap_hud(const std::string& text, int max_w) {
        std::string out, line, word;
        auto flush_word = [&]() {
            if (word.empty()) return;
            const std::string cand = line.empty() ? word : line + " " + word;
            if (!line.empty() && hud_text_width(cand) > max_w) { out += line + "\n"; line = word; }
            else line = cand;
            word.clear();
        };
        for (char c : text) {
            if (c == '\n') { flush_word(); out += line + "\n"; line.clear(); }
            else if (c == ' ') flush_word();
            else word += c;
        }
        flush_word();
        return out + line;
    }
    bool no_audio_;
    ra2engine::SkirmishConfig cfg_;
    std::string dir_, name_, text_, msg_;
    ra2assets::ShpFile bg_;
    ra2assets::Palette pal_;
    bool has_bg_ = false;
    bool start_ = false, back_ = false, loading_ = false, quit_ = false;
    double clock_ = 0.0;
};

// A campaign config for mission `map` of side `side` ("GDI" / "Nod").
ra2engine::SkirmishConfig campaign_config(const std::string& map, const std::string& side, int diff) {
    ra2engine::SkirmishConfig c;
    c.campaign_map = map;
    c.campaign_side = side;
    c.campaign_difficulty = diff;
    c.crates = false;
    return c;
}

// The first mission of a battle.ini campaign (RE §3.1): ALL1 / SOV1.
Scene* make_campaign_start_scene(bool no_audio, const char* battle, const char* side, int difficulty = 1) {
    std::string map = std::string(battle) == "SOV1" ? "SOV01t.MAP" : "ALL01t.MAP";
    try {
        const std::string dir = env_install_dir();
        ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
        const ra2engine::CampaignTables t = camp::load_tables(ra2);
        if (const ra2engine::BattleEntry* b = t.battle(battle)) map = b->scenario;
    } catch (const std::exception&) {}
    return new CampaignLoadScene(no_audio, campaign_config(map, side, difficulty));
}

// ---------------------------------------------------------------------------
// Mission result [RE §3.4 Do_Win/Do_Lose; §9.1 score-screen fallback]:
// won -> Continue to the next MapSel stage (or "campaign complete" after
// EndOfGame); lost -> Retry; both -> Main Menu.
class CampaignResultScene : public Scene {
public:
    CampaignResultScene(bool no_audio, ra2engine::SkirmishConfig cfg, int outcome, long frames)
        : no_audio_(no_audio), cfg_(std::move(cfg)), outcome_(outcome), frames_(frames) {
        try {
            const std::string dir = env_install_dir();
            ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
            const ra2engine::CampaignTables t = camp::load_tables(ra2);
            const ra2engine::ScenarioBasic b =
                ra2engine::parse_scenario_basic(camp::load_map_ini(dir, cfg_.campaign_map));
            end_of_game_ = b.end_of_game;
            if (!end_of_game_ && !b.one_time_only) next_ = t.next_scenario(cfg_.campaign_side, cfg_.campaign_map);
            const ra2engine::MissionInfo mi = t.mission(cfg_.campaign_map);
            name_ = mi.ui_name.empty() ? cfg_.campaign_map : camp::csf_text(dir, mi.ui_name);
        } catch (const std::exception&) {}
    }
    void handle(const ra2engine::InputEvent& ev) override {
        if (ev.kind == ra2engine::InputEvent::PointerMove || ev.kind == ra2engine::InputEvent::PointerDown) {
            mx_ = ev.x;
            my_ = ev.y;
        }
        if (ev.kind != ra2engine::InputEvent::PointerDown || ev.button != ra2engine::InputEvent::Left) return;
        if (cont_btn().enabled && cont_btn().r.contains(ev.x, ev.y)) choice_ = 1;
        if (retry_btn().r.contains(ev.x, ev.y)) choice_ = 2;
        if (menu_btn().r.contains(ev.x, ev.y)) choice_ = 3;
    }
    void render(ra2engine::Platform& plat, ra2engine::TextureCache& tc) override {
        (void)tc;
        SDL_Renderer* ren = static_cast<SDL_Renderer*>(plat.renderer());
        SDL_SetRenderDrawColor(ren, 8, 10, 16, 255);
        SDL_RenderClear(ren);
        const char* head = outcome_ == 1 ? (next_.empty() && end_of_game_ ? "CAMPAIGN COMPLETE"
                                                                            : "MISSION ACCOMPLISHED")
                                         : "MISSION FAILED";
        const int w = static_cast<int>(std::strlen(head)) * 24;
        draw_text(ren, head, (1280 - w) / 2, 200, 4, 255, 220, 90);
        draw_text(ren, camp::up(name_), (1280 - (int)name_.size() * 12) / 2, 260, 2, 220, 220, 220);
        char b[64];
        std::snprintf(b, sizeof b, "TIME %02ld:%02ld", frames_ / 15 / 60, (frames_ / 15) % 60);
        draw_text(ren, b, (1280 - (int)std::strlen(b) * 12) / 2, 300, 2, 200, 200, 200);
        draw_camp_button(ren, cont_btn(), cont_btn().r.contains(mx_, my_));
        draw_camp_button(ren, retry_btn(), retry_btn().r.contains(mx_, my_));
        draw_camp_button(ren, menu_btn(), menu_btn().r.contains(mx_, my_));
    }
    Scene* next() override {
        const int c = choice_;
        choice_ = 0;
        if (c == 1)
            return new CampaignLoadScene(no_audio_, campaign_config(next_, cfg_.campaign_side,
                                                                    cfg_.campaign_difficulty));
        if (c == 2) return new CampaignLoadScene(no_audio_, cfg_);
        if (c == 3) {
            if (Scene* m = make_main_menu_scene(no_audio_)) return m;
            quit_ = true;
        }
        return nullptr;
    }
    bool wants_quit() const override { return quit_; }
    // G0 --play: the result screen IS the session's verdict (outcome WON/LOST).
    void play_probe(ra2play::Probe& p) override {
        p.scene = "result";
        p.set_s("outcome", outcome_ == 1 ? "WON" : "LOST");
        p.set_s("map", cfg_.campaign_map);
        p.set_s("next_map", next_);
        p.set("result_frames", static_cast<double>(frames_));
        const CampButton c = cont_btn(), r = retry_btn(), m = menu_btn();
        if (c.enabled) p.targets["continue"] = ra2play::Rect{c.r.x, c.r.y, c.r.w, c.r.h};
        p.targets["retry"] = ra2play::Rect{r.r.x, r.r.y, r.r.w, r.r.h};
        p.targets["menu"] = ra2play::Rect{m.r.x, m.r.y, m.r.w, m.r.h};
    }

private:
    CampButton cont_btn() const {
        return {{460, 420, 360, 46}, "CONTINUE", outcome_ == 1 && !next_.empty()};
    }
    static CampButton retry_btn() { return {{460, 480, 360, 46}, "RETRY MISSION", true}; }
    static CampButton menu_btn() { return {{460, 540, 360, 46}, "MAIN MENU", true}; }
    bool no_audio_;
    ra2engine::SkirmishConfig cfg_;
    int outcome_;
    long frames_;
    std::string next_, name_;
    bool end_of_game_ = false;
    int choice_ = 0;
    bool quit_ = false;
    int mx_ = 0, my_ = 0;
};

Scene* make_campaign_result_scene(bool no_audio, const ra2engine::SkirmishConfig& cfg, int outcome,
                                  long frames) {
    return new CampaignResultScene(no_audio, cfg, outcome, frames);
}
