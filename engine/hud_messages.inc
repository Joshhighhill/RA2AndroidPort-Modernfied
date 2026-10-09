// hud_messages.inc -- W2-UI-12/21: the in-game message list, mission timer,
// end banner and F1 briefing, all in the ONE HUD font (hud_text.inc).
// #included INSIDE the GameScene class body (public section).
//
// [RE: wave2/04 W2-UI-21 -- TXT lines, objectives, TXT_LOW_POWER "Low Power -
// Production Slowed", TXT_PLAYER_DEFEATED, TXT_GAME_WAS_SAVED go to the
// top-left message list with a per-line timeout ([General] MessageDelay,
// minutes).]
// [RE-FALLBACK: list origin (8, 8) of the tactical view, at most 6 lines, UI
// colour; the timer sits top-centre as "Time Remaining: mm:ss"
// (GUI:TimeRemaining); the end banner is game.fnt at 2x centred on the
// tactical view -- the original's banner face/scale was not traced.]

    struct HudMsg {
        std::string text;
        double expire = 0.0;   // msg_clock_ seconds
    };
    std::vector<HudMsg> msgs_;
    double msg_clock_ = 0.0;
    bool low_power_was_ = false;
    static constexpr std::size_t kMaxMsgs = 6;

    double message_seconds() const {
        const double m = rules_.general_double("MessageDelay", 0.6);   // minutes
        return std::max(4.0, m * 60.0);
    }
    void post_message(const std::string& s, double seconds = -1.0) {
        if (s.empty()) return;
        msgs_.push_back({s, msg_clock_ + (seconds > 0 ? seconds : message_seconds())});
        while (msgs_.size() > kMaxMsgs) msgs_.erase(msgs_.begin());
    }
    // The newest live message ("" = none) -- the `banner` probe key.
    std::string newest_message() const {
        for (auto it = msgs_.rbegin(); it != msgs_.rend(); ++it)
            if (it->expire > msg_clock_) return it->text;
        return "";
    }
    void messages_tick(double dt) {
        msg_clock_ += dt;
        msgs_.erase(std::remove_if(msgs_.begin(), msgs_.end(),
                                   [&](const HudMsg& m) { return m.expire <= msg_clock_; }),
                    msgs_.end());
        // TXT_LOW_POWER on the drop into low power (edge).
        const ra2engine::PowerTotals pt = ra2engine::power_totals(world_.buildings(), ba_.roster, 0);
        const bool low = pt.drain > pt.output;
        if (low && !low_power_was_) post_message(camp::csf_text(dir_, "TXT_LOW_POWER"));
        low_power_was_ = low;
    }
    void draw_messages(SDL_Renderer* ren, ra2engine::TextureCache& tc) const {
        int y = 8;
        const SDL_Color col = hud_ui_color();
        const double sc = hud_touch_scale();   // W6-01-12: 1.5x on a touch canvas
        for (const HudMsg& m : msgs_) {
            if (m.expire <= msg_clock_) continue;
            hud_text(ren, tc, m.text, 8, y, col, kHudLeft, false, true, sc);
            y += static_cast<int>(hud_text_height(m.text) * sc) + 4;
        }
    }
    void draw_mission_timer(SDL_Renderer* ren, ra2engine::TextureCache& tc, long frames_left) const {
        char b[64];
        std::snprintf(b, sizeof b, " %02ld:%02ld", frames_left / 15 / 60, (frames_left / 15) % 60);
        const std::string label = camp::csf_text(dir_, "GUI:TimeRemaining");
        hud_text(ren, tc, (label == "GUI:TimeRemaining" ? std::string("Time Remaining:") : label) + b,
                 playfield_w_ / 2, 8, hud_ui_color(), kHudCenter, true, true);
    }
    void draw_end_banner(SDL_Renderer* ren, ra2engine::TextureCache& tc, const std::string& s) const {
        if (s.empty()) return;
        hud_text(ren, tc, s, playfield_w_ / 2, kWinH / 2 - 14, hud_ui_color(), kHudCenter, true, true, 2.0);
    }
    // Word-wrap to `max_w` px in the HUD font (explicit '\n' kept).
    static std::string hud_wrap(const std::string& text, int max_w) {
        std::string out, line, word;
        auto flush_word = [&]() {
            if (word.empty()) return;
            const std::string cand = line.empty() ? word : line + " " + word;
            if (!line.empty() && hud_text_width(cand) > max_w) {
                out += line + "\n";
                line = word;
            } else {
                line = cand;
            }
            word.clear();
        };
        for (char c : text) {
            if (c == '\n') { flush_word(); out += line + "\n"; line.clear(); }
            else if (c == ' ') flush_word();
            else word += c;
        }
        flush_word();
        out += line;
        return out;
    }
