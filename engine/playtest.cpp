// G0 verification harness -- the SDL half. See playtest.hpp and
// scripts/play/README.md for the model and the script format.
//
// Input injection model: every scripted gesture goes through SDL's own event
// queue and is translated by the REAL Platform::poll (so a change to the key
// map in platform.cpp is exercised as-is), and the polled-state APIs the game
// reads directly are kept consistent:
//   * pointer position -> SDL_WarpMouseInWindow on the "dummy" video driver
//     (no OS cursor exists there, so SDL just updates its mouse state and
//     queues SDL_MOUSEMOTION -- SDL_GetMouseState then reports it);
//   * buttons / keys / text -> SDL_PushEvent;
//   * held keys -> SDL's keyboard-state array (SDL_GetKeyboardState returns a
//     pointer to it; GameScene::update polls arrows/D/N there), mods via
//     SDL_SetModState.
// Event timestamps are rewritten to virtual time (frame * 1000/15 ms) so the
// double-click window is deterministic.
#include "playtest.hpp"

#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

#include "stb_image_write.h"   // implementation lives in main.cpp

namespace ra2play {

PlayDriver* g_play = nullptr;

namespace {

struct Failure {
    int line = 0;
    long frame = 0;
    std::string what;     // the command text
    std::string detail;   // evaluated detail / reason
    std::vector<std::string> tags;
    bool hard = false;
};

std::string join_tags(const std::vector<std::string>& t) {
    std::string s;
    for (const std::string& x : t) s += (s.empty() ? "@" : " @") + x;
    return s;
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

SDL_Scancode scancode_for(const std::string& name_in) {
    std::string n = lower(name_in);
    static const std::map<std::string, std::string> alias = {
        {"esc", "Escape"},     {"escape", "Escape"}, {"enter", "Return"},   {"return", "Return"},
        {"space", "Space"},    {"tab", "Tab"},       {"backspace", "Backspace"},
        {"delete", "Delete"},  {"del", "Delete"},    {"up", "Up"},          {"down", "Down"},
        {"left", "Left"},      {"right", "Right"},   {"home", "Home"},      {"end", "End"},
        {"pageup", "PageUp"},  {"pagedown", "PageDown"}, {"shift", "Left Shift"},
        {"ctrl", "Left Ctrl"}, {"alt", "Left Alt"},  {"minus", "-"},        {"plus", "="}};
    if (auto it = alias.find(n); it != alias.end()) return SDL_GetScancodeFromName(it->second.c_str());
    std::string u = name_in;
    if (u.size() == 1) u[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(u[0])));
    else if ((u[0] == 'f' || u[0] == 'F') && u.size() <= 3) u[0] = 'F';
    return SDL_GetScancodeFromName(u.c_str());
}

class Runner final : public PlayDriver {
public:
    Runner(Script s, std::string out_dir) : s_(std::move(s)), out_(std::move(out_dir)) {
        log_.open(out_ + "/play.log", std::ios::trunc);
    }

    // ---------------------------------------------------------------- driver
    bool pre_frame(ra2engine::Platform& plat, const ProbeFn& probe) override {
        ren_ = static_cast<SDL_Renderer*>(plat.renderer());
        win_ = SDL_RenderGetWindow(ren_);
        ++f_;
        seq_ = 0;
        release_keys();
        if (park_pending_) {
            park_pending_ = false;
            move_to(park_x_, park_y_);
        }
        if (f_ > max_frames_) {
            fail_hard(-1, "max_frames", "run exceeded " + std::to_string(max_frames_) + " frames", {});
            return false;
        }
        Probe p;
        probe(p);
        note_scene(p);
        for (;;) {
            if (wait_left_ > 0) { --wait_left_; return true; }
            if (until_active_) {
                const Cmd& c = s_.cmds[pc_];
                std::string d;
                const Eval e = eval_expr(c.expr, p, vars_, d);
                if (e == Eval::True) {
                    say("PASS", c, d + " (after " + std::to_string(c.n - until_left_) + " frames)");
                    until_active_ = false;
                    ++pc_;
                    continue;
                }
                if (e == Eval::Missing || e == Eval::Error || until_left_ <= 0) {
                    fail(c, d + (e == Eval::False ? " (timeout " + std::to_string(c.n) + " frames)" : ""));
                    until_active_ = false;
                    ++pc_;
                    continue;
                }
                --until_left_;
                return true;
            }
            if (pc_ >= s_.cmds.size()) { finished_ = true; return false; }
            const Cmd& c = s_.cmds[pc_];
            trace(c);
            const int r = exec(c, p, probe);
            if (r == kStop) return false;
            if (r == kConsume) return true;
            // kNext: re-probe if the command may have changed state, then go on.
            if (c.op == "cheat" || c.op == "view") { p = Probe{}; probe(p); note_scene(p); }
        }
    }

    void on_event(ra2engine::InputEvent& ev) override {
        // Virtual time: 1000/15 ms per frame + 1 ms per event inside the frame.
        ev.t_ms = static_cast<uint32_t>(f_ * 1000 / 15 + (seq_++));
    }

    void on_quit() override {
        quit_seen_ = true;
        quit_frame_ = f_;
    }

    double dt() const override { return dt_; }
    bool want_render() const override {
        return !shot_.empty() || render_every_ <= 1 || (f_ % render_every_) == 0;
    }

    void post_frame(ra2engine::Platform& plat, const ProbeFn& probe) override {
        (void)plat;
        if (!shot_.empty()) {
            capture(shot_);
            shot_.clear();
        }
        if (f_ % 15 == 0) {
            Probe p;
            probe(p);
            p.set_s("scene", p.scene);
            hash_ = probe_hash(p, hash_ ^ static_cast<std::uint64_t>(f_));
            std::ostringstream o;
            o << "play_state f=" << f_ << " scene=" << p.scene;
            for (const char* k : {"credits", "units(0)", "units(1)", "buildings(0)", "buildings(1)",
                                  "selected", "tab"})
                if (auto it = p.n.find(k); it != p.n.end()) o << " " << k << "=" << it->second;
            if (auto it = p.s.find("outcome"); it != p.s.end()) o << " outcome=" << it->second;
            emit(o.str(), /*console=*/verbose_states_);
        }
    }

    // ---------------------------------------------------------------- report
    int finish(const std::string& exit_reason) {
        if (quit_seen_ && !expect_quit_) {
            std::vector<std::string> tags = last_input_tags_;
            fail_hard(last_input_line_, "process-quit",
                      "process would quit at f=" + std::to_string(quit_frame_) +
                          " (InputEvent::Quit reached run_scene_driver)",
                      tags);
        } else if (!finished_ && !quit_seen_ && !hard_stop_) {
            fail_hard(-1, "session-ended", "the scene driver returned at f=" + std::to_string(f_) +
                                               " (" + exit_reason + ") before the script finished",
                      {});
        }
        const std::size_t unreached = finished_ ? 0 : (s_.cmds.size() > pc_ ? s_.cmds.size() - pc_ : 0);
        char hb[32];
        std::snprintf(hb, sizeof hb, "%016llx", static_cast<unsigned long long>(hash_));
        std::ostringstream o;
        o << "play_result=" << (fails_.empty() ? "PASS" : "FAIL") << " script=" << s_.name
          << " frames=" << f_ << " failures=" << fails_.size() << " unreached=" << unreached;
        emit(o.str(), true);
        emit(std::string("play_hash=") + hb, true);
        // result.txt: one line per failure, machine-friendly for PLAYTEST-STATUS.md.
        std::ofstream r(out_ + "/result.txt", std::ios::trunc);
        r << o.str() << "\nplay_hash=" << hb << "\n";
        for (const Failure& x : fails_)
            r << "FAIL L" << x.line << " f=" << x.frame << " | " << x.what << " | " << x.detail
              << " | " << join_tags(x.tags) << (x.hard ? " | HARD" : "") << "\n";
        if (unreached) {
            r << "UNREACHED " << unreached << " command(s) from L"
              << (pc_ < s_.cmds.size() ? s_.cmds[pc_].line : 0) << "\n";
            for (std::size_t i = pc_; i < s_.cmds.size(); ++i)
                if (!s_.cmds[i].tags.empty())
                    r << "  unreached L" << s_.cmds[i].line << " | " << s_.cmds[i].raw << "\n";
        }
        return fails_.empty() ? 0 : 1;
    }

    bool verbose_states_ = true;

private:
    enum { kNext = 0, kConsume = 1, kStop = 2 };

    // ------------------------------------------------------------- commands
    int exec(const Cmd& c, Probe& p, const ProbeFn& probe) {
        const std::string& op = c.op;
        if (op == "set") {
            const std::string k = lower(c.args[0]);
            const std::string v = c.args.size() > 1 ? c.args[1] : "";
            if (k == "dt") dt_ = std::atof(v.c_str());
            else if (k == "render_every") render_every_ = std::max(1, std::atoi(v.c_str()));
            else if (k == "max_frames") max_frames_ = std::atol(v.c_str());
            else if (k == "park" && c.args.size() > 2) { park_x_ = std::atoi(v.c_str()); park_y_ = std::atoi(c.args[2].c_str()); }
            else if (k == "states") verbose_states_ = v != "off";
            else { fail(c, "unknown setting"); }
            ++pc_;
            return kNext;
        }
        if (op == "wait") { wait_left_ = c.n; ++pc_; return kNext; }
        if (op == "until") { until_active_ = true; until_left_ = c.n; return kNext; }
        if (op == "assert" || op == "require") {
            std::string d;
            const Eval e = eval_expr(c.expr, p, vars_, d);
            ++pc_;
            if (e == Eval::True) { say("PASS", c, d); return kNext; }
            if (op == "require") { fail_hard(c.line, c.raw, d, c.tags); return kStop; }
            fail(c, d);
            return kNext;
        }
        if (op == "let") {
            Value v;
            if (!resolve_operand(c.expr, p, vars_, true, v)) fail(c, "missing-probe:" + c.expr);
            else vars_[c.args[0]] = v;
            ++pc_;
            return kNext;
        }
        if (op == "dump") {
            std::string d = probe_dump(p);
            for (const auto& [k, r] : p.targets)
                d += "target " + k + " @" + std::to_string(r.x) + "," + std::to_string(r.y) + " " +
                     std::to_string(r.w) + "x" + std::to_string(r.h) + "\n";
            emit("PLAY dump L" + std::to_string(c.line) + " f=" + std::to_string(f_) + " scene=" + p.scene +
                     "\n" + d,
                 !c.args.empty() && c.args[0] == "console");
            ++pc_;
            return kNext;
        }
        if (op == "log") { emit("PLAY log L" + std::to_string(c.line) + " " + c.raw.substr(4), true); ++pc_; return kNext; }
        if (op == "expect") {
            if (lower(c.args[0]) == "quit") expect_quit_ = true;
            else fail(c, "expect: only 'expect quit' is supported");
            ++pc_;
            return kNext;
        }
        if (op == "end") {
            if (c.args.size() >= 2 && lower(c.args[0]) == "expect") {
                std::string d;
                const Eval e = eval_expr("outcome == " + c.args[1], p, vars_, d);
                if (e == Eval::True) say("PASS", c, d);
                else fail(c, d);
            }
            ++pc_;
            finished_ = true;
            return kStop;
        }
        if (op == "cheat") {
            ++pc_;
            if (!p.cheat) { fail(c, "missing-probe:cheat (scene '" + p.scene + "' has no cheats)"); return kNext; }
            const std::string err = p.cheat(c.args);
            if (!err.empty()) fail(c, "cheat: " + err);
            return kNext;
        }
        if (op == "shot") {
            shot_ = c.args[0];
            ++pc_;
            return kConsume;
        }
        if (op == "view") {
            ++pc_;
            double cx = 0, cy = 0;
            if (!locate(c, p, 0, cx, cy)) return kNext;
            if (!p.center_on) { fail(c, "missing-probe:center_on"); return kNext; }
            p.center_on(cx, cy);
            return kNext;
        }
        // ---- input gestures (each consumes the frame) ----
        last_input_line_ = c.line;
        last_input_tags_ = c.tags;
        if (op == "key" || op == "hold") {
            ++pc_;
            const SDL_Scancode sc = scancode_for(c.args[0]);
            if (sc == SDL_SCANCODE_UNKNOWN) { fail(c, "unknown key '" + c.args[0] + "'"); return kConsume; }
            const int mods = mods_of(c, 1);
            press_mods(mods);
            key_event(sc, true, mods);
            held_[sc] = (op == "hold") ? c.n : 1;
            return kConsume;
        }
        if (op == "type") {
            ++pc_;
            SDL_Event e{};
            e.type = SDL_TEXTINPUT;
            e.text.windowID = win_ ? SDL_GetWindowID(win_) : 0;
            std::snprintf(e.text.text, sizeof e.text.text, "%s", c.args[0].c_str());
            if (SDL_PushEvent(&e) != 1) fail(c, "SDL_TEXTINPUT disabled (text input not started)");
            return kConsume;
        }
        if (op == "move") {
            ++pc_;
            int x = 0, y = 0;
            if (!point_of(c, p, 0, x, y)) return kConsume;
            move_to(x, y);
            return kConsume;
        }
        if (op == "click") {
            ++pc_;
            int x = 0, y = 0;
            if (!point_of(c, p, 0, x, y)) return kConsume;
            const bool right = has_word(c, "right");
            const int mods = mods_of(c, 1);
            press_mods(mods);
            held_mods_ = mods;
            move_to(x, y);
            button(x, y, right, true);
            button(x, y, right, false);
            if (!has_word(c, "nopark")) park_pending_ = true;
            return kConsume;
        }
        if (op == "drag") {
            ++pc_;
            // drag cell x1 y1 x2 y2 | drag xy x1 y1 x2 y2
            if (c.args.size() < 5) { fail(c, "drag cell|xy X1 Y1 X2 Y2"); return kConsume; }
            int x1, y1, x2, y2;
            Cmd a = c, b = c;
            a.args = {c.args[0], c.args[1], c.args[2]};
            b.args = {c.args[0], c.args[3], c.args[4]};
            if (!point_of(a, p, 0, x1, y1) || !point_of(b, p, 0, x2, y2)) return kConsume;
            const bool right = has_word(c, "right");
            move_to(x1, y1);
            button(x1, y1, right, true);
            // Intermediate motion so the drag threshold is crossed.
            move_to((x1 + x2) / 2, (y1 + y2) / 2);
            move_to(x2, y2);
            button(x2, y2, right, false);
            park_pending_ = true;
            return kConsume;
        }
        if (op == "place") {
            ++pc_;
            if (!p.auto_place) { fail(c, "missing-probe:auto_place"); return kConsume; }
            int cx = 0, cy = 0;
            if (!p.auto_place(cx, cy)) { fail(c, "no valid placement cell (or nothing armed)"); return kConsume; }
            int x = 0, y = 0;
            if (!cell_point(p, cx, cy, x, y)) { fail(c, "placement cell off-screen"); return kConsume; }
            move_to(x, y);
            button(x, y, false, true);
            button(x, y, false, false);
            park_pending_ = true;
            return kConsume;
        }
        (void)probe;
        fail(c, "unhandled command");
        ++pc_;
        return kNext;
    }

    // Resolve the n-th..: target forms used by click/move/view.
    //   cell X Y | xy X Y | unit H:T[:n] | building H:T[:n] | <named target>
    bool locate(const Cmd& c, Probe& p, std::size_t i, double& cx, double& cy) {
        const std::string k = lower(c.args[i]);
        if (k == "cell") {
            if (c.args.size() < i + 3) { fail(c, "cell X Y"); return false; }
            cx = std::atof(c.args[i + 1].c_str());
            cy = std::atof(c.args[i + 2].c_str());
            return true;
        }
        if (k == "unit" || k == "building") {
            if (c.args.size() < i + 2) { fail(c, k + " H:TYPE[:n]"); return false; }
            auto& fn = (k == "unit") ? p.find_unit : p.find_building;
            if (!fn) { fail(c, "missing-probe:find_" + k); return false; }
            if (!fn(c.args[i + 1], cx, cy)) { fail(c, "no " + k + " " + c.args[i + 1]); return false; }
            return true;
        }
        fail(c, "expected cell|unit|building");
        return false;
    }

    bool cell_point(Probe& p, double cx, double cy, int& x, int& y) {
        if (!p.cell_to_screen) return false;
        if (p.cell_to_screen(cx, cy, x, y)) return true;
        if (!p.center_on) return false;
        p.center_on(cx, cy);
        return p.cell_to_screen(cx, cy, x, y);
    }

    bool point_of(const Cmd& c, Probe& p, std::size_t i, int& x, int& y) {
        const std::string k = lower(c.args[i]);
        if (k == "xy") {
            if (c.args.size() < i + 3) { fail(c, "xy X Y"); return false; }
            x = std::atoi(c.args[i + 1].c_str());
            y = std::atoi(c.args[i + 2].c_str());
            return true;
        }
        if (k == "cell" || k == "unit" || k == "building") {
            double cx = 0, cy = 0;
            if (!locate(c, p, i, cx, cy)) return false;
            if (!p.cell_to_screen) { fail(c, "missing-probe:cell_to_screen (scene '" + p.scene + "')"); return false; }
            if (!cell_point(p, cx, cy, x, y)) { fail(c, "cell off-screen and cannot centre"); return false; }
            return true;
        }
        // Named target; "button X" is sugar for "button:X".
        std::string name = c.args[i];
        if (lower(name) == "button" && c.args.size() > i + 1) name = "button:" + c.args[i + 1];
        auto it = p.targets.find(name);
        if (it == p.targets.end()) {
            std::string avail;
            int n = 0;
            for (const auto& [tn, r] : p.targets) {
                (void)r;
                if (n++ < 40) avail += (avail.empty() ? "" : ", ") + tn;
            }
            fail(c, "no target '" + name + "' on scene '" + p.scene + "' (have: " + avail + ")");
            return false;
        }
        x = it->second.cx();
        y = it->second.cy();
        return true;
    }

    static bool has_word(const Cmd& c, const char* w) {
        for (const std::string& a : c.args)
            if (lower(a) == w) return true;
        return false;
    }
    static int mods_of(const Cmd& c, std::size_t from) {
        int m = 0;
        for (std::size_t i = from; i < c.args.size(); ++i) {
            const std::string a = lower(c.args[i]);
            if (a == "shift") m |= KMOD_LSHIFT;
            else if (a == "ctrl") m |= KMOD_LCTRL;
            else if (a == "alt") m |= KMOD_LALT;
        }
        return m;
    }

    // --------------------------------------------------------------- inject
    Uint8* keystate() { return const_cast<Uint8*>(SDL_GetKeyboardState(nullptr)); }

    void press_mods(int mods) {
        held_mods_ = mods;
        SDL_SetModState(static_cast<SDL_Keymod>(mods));
        Uint8* ks = keystate();
        ks[SDL_SCANCODE_LSHIFT] = (mods & KMOD_LSHIFT) ? 1 : 0;
        ks[SDL_SCANCODE_LCTRL] = (mods & KMOD_LCTRL) ? 1 : 0;
        ks[SDL_SCANCODE_LALT] = (mods & KMOD_LALT) ? 1 : 0;
    }

    void key_event(SDL_Scancode sc, bool down, int mods) {
        keystate()[sc] = down ? 1 : 0;
        SDL_Event e{};
        e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
        e.key.windowID = win_ ? SDL_GetWindowID(win_) : 0;
        e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
        e.key.keysym.scancode = sc;
        e.key.keysym.sym = SDL_GetKeyFromScancode(sc);
        e.key.keysym.mod = static_cast<Uint16>(mods);
        SDL_PushEvent(&e);
    }

    void release_keys() {
        for (auto it = held_.begin(); it != held_.end();) {
            if (--it->second <= 0) {
                key_event(static_cast<SDL_Scancode>(it->first), false, held_mods_);
                it = held_.erase(it);
            } else {
                ++it;
            }
        }
        if (held_.empty() && held_mods_) press_mods(0);
    }

    void move_to(int x, int y) {
        if (win_) SDL_WarpMouseInWindow(win_, x, y);
        // SDL only queues a motion event when the position changes; the game
        // also tracks the pointer from button events, so nothing is lost.
    }

    void button(int x, int y, bool right, bool down) {
        SDL_Event e{};
        e.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
        e.button.windowID = win_ ? SDL_GetWindowID(win_) : 0;
        e.button.which = 0;
        e.button.button = right ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT;
        e.button.state = down ? SDL_PRESSED : SDL_RELEASED;
        e.button.clicks = 1;
        e.button.x = x;
        e.button.y = y;
        SDL_PushEvent(&e);
    }

    void capture(const std::string& name) {
        if (!ren_) return;
        int w = 0, h = 0;
        SDL_GetRendererOutputSize(ren_, &w, &h);
        std::vector<unsigned char> px(static_cast<std::size_t>(w) * h * 4);
        if (SDL_RenderReadPixels(ren_, nullptr, SDL_PIXELFORMAT_RGBA32, px.data(), w * 4) != 0) {
            emit("PLAY shot " + name + " FAILED: " + SDL_GetError(), true);
            return;
        }
        std::string fn = name;
        if (fn.size() < 4 || fn.substr(fn.size() - 4) != ".png") fn += ".png";
        const std::string path = out_ + "/" + fn;
        if (!stbi_write_png(path.c_str(), w, h, 4, px.data(), w * 4))
            emit("PLAY shot " + path + " FAILED (write)", true);
        else
            emit("PLAY shot f=" + std::to_string(f_) + " " + path, true);
    }

    // ------------------------------------------------------------- logging
    void note_scene(Probe& p) {
        if (p.scene != scene_) {
            emit("PLAY scene f=" + std::to_string(f_) + " " + scene_ + " -> " + p.scene, true);
            scene_ = p.scene;
            sf_ = 0;
        } else {
            ++sf_;
        }
        p.set("f", static_cast<double>(f_));
        p.set("sf", static_cast<double>(sf_));
        p.set_s("scene", p.scene);
        p.set("quit_seen", quit_seen_ ? 1 : 0);
    }
    void trace(const Cmd& c) {
        if (c.op == "until" && until_active_) return;
        emit("PLAY step L" + std::to_string(c.line) + " f=" + std::to_string(f_) + " " + c.raw, trace_console_);
    }
    void say(const char* what, const Cmd& c, const std::string& d) {
        emit(std::string("PLAY ") + what + " L" + std::to_string(c.line) + " f=" + std::to_string(f_) + " " +
                 c.raw + "  [" + d + "]",
             true);
    }
    void fail(const Cmd& c, const std::string& d) {
        fails_.push_back({c.line, f_, c.raw, d, c.tags, false});
        emit("PLAY FAIL L" + std::to_string(c.line) + " f=" + std::to_string(f_) + " " + c.raw + "  [" + d +
                 "] " + join_tags(c.tags),
             true);
    }
    void fail_hard(int line, const std::string& what, const std::string& d, const std::vector<std::string>& tags) {
        fails_.push_back({line, f_, what, d, tags, true});
        hard_stop_ = true;
        emit("PLAY FAIL(hard) L" + std::to_string(line) + " f=" + std::to_string(f_) + " " + what + "  [" + d +
                 "] " + join_tags(tags),
             true);
    }
    void emit(const std::string& s, bool console) {
        if (console) std::cout << s << "\n" << std::flush;
        if (log_) log_ << s << "\n";
    }

    Script s_;
    std::string out_;
    std::ofstream log_;
    SDL_Renderer* ren_ = nullptr;
    SDL_Window* win_ = nullptr;
    long f_ = 0;
    long sf_ = 0;
    std::string scene_ = "(none)";
    int seq_ = 0;
    std::size_t pc_ = 0;
    int wait_left_ = 0;
    bool until_active_ = false;
    int until_left_ = 0;
    std::map<int, int> held_;   // scancode -> frames left
    int held_mods_ = 0;
    bool park_pending_ = false;
    int park_x_ = 560, park_y_ = 360;
    std::string shot_;
    double dt_ = 1.0 / 15.0;
    int render_every_ = 1;
    long max_frames_ = 40000;
    std::map<std::string, Value> vars_;
    std::vector<Failure> fails_;
    std::uint64_t hash_ = 1469598103934665603ull;
    bool finished_ = false, hard_stop_ = false;
    bool quit_seen_ = false, expect_quit_ = false;
    long quit_frame_ = 0;
    int last_input_line_ = -1;
    std::vector<std::string> last_input_tags_;
    bool trace_console_ = true;
};

}  // namespace

int run_play(const std::string& script_path, const std::string& out_dir_in,
             const std::function<int()>& run_menu) {
    std::ifstream in(script_path, std::ios::binary);
    if (!in) {
        std::cerr << "play: cannot read " << script_path << "\n";
        return 2;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    Script s;
    std::string err;
    if (!parse_script(ss.str(), s, err)) {
        std::cerr << "play: " << script_path << ": " << err << "\n";
        return 2;
    }
    s.name = std::filesystem::path(script_path).stem().string();
    const std::string out_dir = out_dir_in.empty() ? ("play-out/" + s.name) : out_dir_in;
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);
    // Isolated RA2.INI + saved games (never the user's real pref dir).
    const std::string pref = out_dir + "/pref";
    std::filesystem::remove_all(pref, ec);
    std::filesystem::create_directories(pref, ec);
    const std::string pref_abs = std::filesystem::absolute(pref).string() + "/";
    SDL_setenv("RA2_PREF_DIR", pref_abs.c_str(), 1);
    // Hidden, CI-safe: SDL's dummy video driver (software renderer, no OS
    // window, no OS cursor). RA2_PLAY_VISIBLE=1 keeps the real driver for
    // debugging (the pointer warp then moves the real cursor).
    const char* vis = std::getenv("RA2_PLAY_VISIBLE");
    if (!(vis && std::atoi(vis) != 0)) SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);

    Runner r(std::move(s), out_dir);
    g_play = &r;
    std::string reason = "returned";
    try {
        const int rc = run_menu();
        reason = "run_menu returned " + std::to_string(rc);
    } catch (const std::exception& ex) {
        g_play = nullptr;
        std::cout << "PLAY FAIL(hard) exception: " << ex.what() << "\n";
        r.finish(std::string("exception: ") + ex.what());
        return 3;
    }
    g_play = nullptr;
    return r.finish(reason);
}

}  // namespace ra2play
