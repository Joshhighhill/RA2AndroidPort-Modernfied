// NET-PLAY: GameScene in lockstep mode -- #included INSIDE the GameScene class
// body (public section). net_ == nullptr is single player: nothing here runs.
//
// Tick gate. update()'s fixed-tick loop asks net_gate() before every tick:
// NetGame::step() (begin_frame(state CRC) / poll / end_frame) says Run with
// the commands of THIS frame (every house, the original's Execute_DoList
// order), which go into the command queue stamped for this tick and apply at
// its start exactly like single player's; Stall leaves the tick for later
// (the "Waiting for players" overlay after 1 s, the reconnect dialog after
// the original's dialog timeout); OutOfSync / ScenarioMismatch / Timeout end
// the match with the original's message (TXT_OUT_OF_SYNC, ...), a desync
// dump goes to the saves folder.
// Local commands never enter the queue directly: issue() sends them through
// the session (frame = now + MaxAhead) and they come back like everyone's.
// Logic is a fixed 15 Hz (the lobby speed is not used), ticks are never
// dropped (the backlog runs a few ticks per frame).
// [RE: 01-original-protocol.md §4.6 (EXIT / REMOVEPLAYER), §5 (sync),
//  05-transport-infra.md §6.3 (in game), §7.2 (Android away).]

    std::shared_ptr<NetPlay> net_;
    bool net_stalled_ = false;
    std::uint64_t net_stall_since_ = 0;        // wall ms of the current stall (0 none)
    struct NetAway { int house; std::uint64_t until_ms; };
    std::vector<NetAway> net_away_;            // players who announced AWAY
    std::vector<int> net_left_;                // houses already handled (left / dropped)
    bool net_local_left_ = false;              // our own EXIT executed
    bool net_exit_requested_ = false;
    Scene* net_held_next_ = nullptr;           // where to go once our EXIT executed
    bool net_end_dialog_ = false;              // the match ended abnormally (OOS / mismatch / ...)
    std::string net_end_title_, net_end_text_;
    SDL_Rect net_btn_ok_{0, 0, 0, 0}, net_btn_drop_{0, 0, 0, 0}, net_btn_quit_{0, 0, 0, 0},
        net_btn_chat_{0, 0, 0, 0}, net_btn_send_{0, 0, 0, 0};
    bool net_chat_open_ = false;
    std::string net_chat_text_;
    bool net_bg_away_ = false;                 // we announced AWAY (app in background)
    std::uint32_t net_last_trail_ = 0xFFFFFFFFu;
    double net_hash_ms_sum_ = 0.0, net_hash_ms_max_ = 0.0;
    std::uint32_t net_hash_n_ = 0;
    int net_stalls_ = 0;
    // Presentation events drained at tick end in lockstep mode (the World's
    // event queues are part of the save bytes the sync hash covers, so they
    // must be empty at every tick boundary on every peer).
    std::vector<ra2engine::World::FireEvent> net_fire_buf_;
    std::vector<ra2engine::World::AbilityEvent> net_ability_buf_;
    std::vector<ra2engine::World::GarrisonEvent> net_garrison_buf_;
    std::vector<ra2engine::ProjectileEvent> net_proj_buf_;

    bool net_on() const { return net_ != nullptr; }
    static constexpr std::uint32_t kNetAwaySeconds = 90;   // 05 §7.2

    void net_attach() {
        if (!g_pending_netplay) return;
        net_ = std::move(g_pending_netplay);
        g_pending_netplay.reset();
        world_.set_order_issuer(local_house_);
        std::cerr << "[net] lockstep match: local house " << local_house_ << ", " << net_->session.players.size()
                  << " houses, hash every " << net_->hash_every << " frames, max_ahead "
                  << net_->game->session().max_ahead() << " fsr " << net_->game->session().frame_send_rate() << "\n";
        post_message("Network game: waiting for every player to finish loading...", 8.0);
    }
    template <class T> static std::vector<T> net_take(std::vector<T>& v) {
        std::vector<T> out;
        out.swap(v);
        return out;
    }
    // Tick end (lockstep only): the frame-time presentation events move out of
    // the World so the next tick's state hash never depends on frame timing.
    void net_drain_tick_events() {
        auto fe = world_.take_fire_events();
        net_fire_buf_.insert(net_fire_buf_.end(), fe.begin(), fe.end());
        auto ae = world_.take_ability_events();
        net_ability_buf_.insert(net_ability_buf_.end(), ae.begin(), ae.end());
        auto ge = world_.take_garrison_events();
        net_garrison_buf_.insert(net_garrison_buf_.end(), ge.begin(), ge.end());
        auto pe = world_.take_projectile_events();
        net_proj_buf_.insert(net_proj_buf_.end(), pe.begin(), pe.end());
    }

    // The sync hash: the complete World (save_world bytes, bit-exact doubles)
    // plus the scene's sim-side FreeUnit tracker. UI state (camera, sidebar,
    // the local player's pending placement) stays out. Folded to the 32-bit
    // CRC the FRAMEINFO packet carries.
    std::uint32_t net_state_hash() {
        const auto t0 = std::chrono::steady_clock::now();
        std::uint64_t h = 1469598103934665603ull;
        auto mix = [&h](const std::vector<uint8_t>& b) {
            for (uint8_t c : b) { h ^= c; h *= 1099511628211ull; }
        };
        mix(ra2engine::save::save_world(world_));
        for (int v : bstates_) {
            h ^= static_cast<std::uint32_t>(v);
            h *= 1099511628211ull;
        }
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        net_hash_ms_sum_ += ms;
        net_hash_ms_max_ = std::max(net_hash_ms_max_, ms);
        ++net_hash_n_;
        return static_cast<std::uint32_t>(h ^ (h >> 32));
    }

    // Before every tick: may this tick run? (Run: this frame's commands are queued.)
    bool net_gate() {
        if (!net_ || net_end_dialog_ || net_local_left_) return false;
        std::vector<ra2engine::Command> cmds;
        const ra2::net::NetGame::Step st = net_->game->step([this]() { return net_state_hash(); }, cmds);
        if (st == ra2::net::NetGame::Step::Stall) {
            if (!net_stalled_) {
                net_stalled_ = true;
                net_stall_since_ = ra2mp::now_ms();
                ++net_stalls_;
            }
            return false;
        }
        if (st != ra2::net::NetGame::Step::Run) {
            net_fail(st);
            return false;
        }
        net_stalled_ = false;
        net_stall_since_ = 0;
        for (ra2engine::Command& c : cmds) cmdq_.push(std::move(c));
        // Test hook (scripts/play/net-lan-desync.ps1): RA2_NET_DESYNC_AT=<frame>
        // makes THIS machine diverge deliberately at that frame (one credit).
        static const long desync_at = [] {
            const char* e = std::getenv("RA2_NET_DESYNC_AT");
            return e && *e ? std::atol(e) : -1L;
        }();
        if (desync_at >= 0 && sim_frame_ == static_cast<std::uint32_t>(desync_at)) {
            world_.deposit_credits(local_house_, 1.0);
            std::cerr << "[net] TEST: injected a divergence at frame " << sim_frame_ << "\n";
            pres_log(0, "net test divergence");
        }
        // The hash trail (stderr / play log): one line per hash period, the
        // two-process tests compare these between the machines.
        const std::uint32_t f = sim_frame_;
        const std::uint32_t every = static_cast<std::uint32_t>(std::max(1, net_->hash_every));
        if (f % every == 0 && f != net_last_trail_) {
            net_last_trail_ = f;
            const std::uint32_t crc = net_->game->crc_of(f);
            char b[96];
            std::snprintf(b, sizeof b, "[net] trail f=%u crc=%08x", f, crc);
            std::cerr << b << "\n";   // ~1 line / s: net-match.ps1 compares the two machines' trails
        }
        return true;
    }

    // Exit / RemovePlayer reach apply_due_commands like any command (every peer,
    // same frame): the leaver's house is defeated (or handed to the AI).
    void net_on_command(const ra2engine::Command& c) {
        if (!net_) return;
        if (c.type == ra2engine::EventType::Exit) net_house_left(c.house, /*left=*/true);
        else if (c.type == ra2engine::EventType::RemovePlayer) net_house_left(c.value, /*left=*/false);
    }
    void net_house_left(int h, bool left) {
        if (h < 0 || h >= world_.house_count()) return;
        if (std::find(net_left_.begin(), net_left_.end(), h) != net_left_.end()) return;
        net_left_.push_back(h);
        const std::string who = net_->name_of(h);
        if (h == local_house_) {
            net_local_left_ = true;
            std::cerr << "[net] our EXIT executed at frame " << sim_frame_ << "\n";
        } else {
            post_message(who + (left ? " has left the game." : " lost connection and was removed."), 10.0,
                         net_color(h));
            std::cerr << "[net] house " << h << (left ? " left" : " removed") << " at frame " << sim_frame_ << "\n";
        }
        if (net_->ai_takeover && h != local_house_ && world_.house_outcome(h) == 0 && !world_.house_defeated(h)) {
            // 05 §7: the host chose "AI takes over" -- the house keeps its
            // wallet, queues and units under a Normal AI.
            ra2engine::SkirmishConfig c2 = cfg_;
            for (std::size_t i = 0; i < c2.slots.size(); ++i) c2.slots[i].is_ai = false;
            if (h < static_cast<int>(c2.slots.size())) {
                c2.slots[static_cast<std::size_t>(h)].is_ai = true;
                c2.slots[static_cast<std::size_t>(h)].diff = ra2engine::AiDifficulty::Normal;
                world_.set_human(h, false);
                arm_skirmish_ai(world_, c2, ba_, rules_, rules_ini_, ur_ai_, load_aimd_or_empty(dir_), &a_.map, ua_);
                post_message("The computer takes over " + who + ".", 10.0, net_color(h));
            }
            return;
        }
        world_.resign_house(h);
    }
    SDL_Color net_color(int house) const { return house_text_color(house); }

    void net_fail(ra2::net::NetGame::Step st) {
        if (net_end_dialog_) return;
        net_end_dialog_ = true;
        auto csf = [this](const char* key, const char* fallback) {
            const std::string t = camp::csf_text(dir_, key);
            return t.empty() || t == key ? std::string(fallback) : t;
        };
        const ra2::net::OutOfSyncInfo& o = net_->game->session().out_of_sync();
        switch (st) {
            case ra2::net::NetGame::Step::OutOfSync: {
                net_end_title_ = csf("TXT_OUT_OF_SYNC", "Out of sync!");
                const std::string dump = net_write_desync_dump();
                net_end_text_ = "The game is out of sync with " + net_->name_of(o.house) + " (frame " +
                                std::to_string(o.frame) + "). It cannot continue." +
                                (dump.empty() ? std::string() : "\nDesync report: " + dump);
                break;
            }
            case ra2::net::NetGame::Step::ScenarioMismatch:
                net_end_title_ = csf("TXT_SCENARIOS_DO_NOT_MATCH", "Scenarios do not match!");
                net_end_text_ = "The players' game data or map differ, the match cannot start.";
                net_write_desync_dump();
                break;
            case ra2::net::NetGame::Step::Timeout:
                net_end_title_ = "Connection failed";
                net_end_text_ = "Not every player made it into the game.";
                break;
            default:
                net_end_title_ = "Network error";
                net_end_text_ = "The match had to end.";
                break;
        }
        std::cerr << "[net] match ended: " << ra2::net::NetGame::step_name(st) << " at frame " << sim_frame_
                  << " (oos house " << o.house << " frame " << o.frame << " local " << o.local_crc << " remote "
                  << o.remote_crc << ")\n";
        pres_log(0, std::string("net end ") + ra2::net::NetGame::step_name(st));
    }

    // SYNC dump (the original writes SYNC<house>.TXT, FUN_0064D8E0): this
    // machine's full save at the detection frame + a text report with the
    // frames, both CRCs and the last 256 fed CRCs. The other machine writes
    // its own pair, so the two saves can be diffed (simdiff / save viewer).
    std::string net_write_desync_dump() {
        const std::string dir = save_games_dir();
        if (dir.empty()) return "";
        const ra2::net::OutOfSyncInfo& o = net_->game->session().out_of_sync();
        const std::string stem = dir + "/DESYNC_F" + std::to_string(o.frame) + "_H" + std::to_string(local_house_);
        const bool saved = save_game(stem + ".SAV", "Desync frame " + std::to_string(o.frame));
        std::ofstream t(stem + ".TXT", std::ios::binary | std::ios::trunc);
        if (t) {
            t << "RA2 clone network desync report\n";
            t << "engine " << RA2_VERSION << "\n";
            t << "local house " << local_house_ << " (" << net_->name_of(local_house_) << ")\n";
            t << "out of sync with house " << o.house << " (" << net_->name_of(o.house) << ")\n";
            t << "frame " << o.frame << "  local crc " << o.local_crc << "  remote crc " << o.remote_crc << "\n";
            t << "detected at frame " << sim_frame_ << "  hash every " << net_->hash_every << " frames\n";
            t << "save " << (saved ? stem + ".SAV" : std::string("(write failed)")) << "\n";
            t << "session seed " << net_->session.seed << "  map " << net_->session.lobby.map.file << "\n";
            // Per-member hashes of the World at this frame: diff the two
            // machines' lists to name the member that differs.
            t << "world members (name fnv32):\n";
            for (const auto& fh : ra2engine::save::world_field_hashes(world_))
                t << "  " << fh.first << " " << fh.second << "\n";
            t << "fed CRCs (frame crc), oldest first:\n";
            const std::uint32_t f1 = net_->game->frame();
            for (std::uint32_t k = 0; k < 256 && k <= f1; ++k) {
                const std::uint32_t f = f1 - std::min<std::uint32_t>(f1, 255) + k;
                if (f > f1) break;
                t << f << " " << net_->game->crc_of(f) << "\n";
            }
        }
        std::cerr << "[net] desync dump " << stem << ".SAV / .TXT\n";
        return stem + ".SAV";
    }

    // Per frame (lockstep): the global channel (chat, away), removed players,
    // away countdowns, and leaving once our EXIT executed.
    void net_frame() {
        if (!net_) return;
        if (net_->mpnet) net_->mpnet->tick(ra2mp::now_ms());   // tunnel keep-alive (no-op on LAN)
        std::vector<ra2::net::NetChat> chat;
        std::vector<ra2::net::AwayNotice> away;
        net_->game->poll_global(chat, away);
        for (const ra2::net::NetChat& c : chat) {
            const std::string who = c.house >= 0 ? net_->name_of(c.house) : c.sender;
            post_message(who + ": " + c.text, 12.0, c.house >= 0 ? net_color(c.house) : hud_ui_color());
            pres_log(0, "net chat " + who + ": " + c.text);
            std::cerr << "[net] chat " << who << ": " << c.text << "\n";
        }
        const std::uint64_t now = ra2mp::now_ms();
        for (const ra2::net::AwayNotice& a : away) {
            net_away_.erase(std::remove_if(net_away_.begin(), net_away_.end(),
                                           [&](const NetAway& w) { return w.house == a.house; }),
                            net_away_.end());
            if (!a.back) net_away_.push_back(NetAway{a.house, now + 1000ull * std::min<uint32_t>(a.seconds, 300)});
            post_message(net_->name_of(a.house) + (a.back ? " is back." : " paused the game (app in the background)."),
                         8.0, net_color(a.house));
            pres_log(0, std::string("net away ") + (a.back ? "back " : "away ") + std::to_string(a.house));
            std::cerr << "[net] " << (a.back ? "back" : "away") << " house " << a.house << " (" << a.seconds << " s)" << std::endl;
        }
        // An away player's countdown holds the stall timers (no dialog, no kick).
        net_away_.erase(std::remove_if(net_away_.begin(), net_away_.end(),
                                       [&](const NetAway& w) { return now >= w.until_ms || !net_->game->is_remote(w.house); }),
                        net_away_.end());
        if (!net_away_.empty()) net_->game->session().hold_wait();
        for (const auto& r : net_->game->take_removed())
            std::cerr << "[net] connection to house " << r.first << (r.second ? " lost" : " closed") << "\n";
        // Our EXIT executed (or nobody is left to tell): the scene moves on.
        if (net_exit_requested_ && !net_end_dialog_ &&
            (net_local_left_ || net_->game->session().players() <= 1) && !next_scene_) {
            next_scene_ = net_held_next_ ? net_held_next_ : make_main_menu_scene(no_audio_);
            net_held_next_ = nullptr;
        }
    }

    // The scene wants to leave (result screen, Abort): in a match, first EXIT
    // like the original (scheduled; the game keeps running until it executes
    // on every machine at the same frame). Returns the scene to switch to now.
    Scene* net_filter_next(Scene* n) {
        if (!net_ || !n) return n;
        if (net_end_dialog_) return n;   // the session is dead: just go
        if (net_local_left_ || net_->game->session().players() <= 1) return n;
        if (!net_exit_requested_) {
            net_exit_requested_ = true;
            net_->game->request_exit();
            std::cerr << "[net] leaving: EXIT queued at frame " << sim_frame_ << "\n";
        }
        if (net_held_next_ && net_held_next_ != n) delete net_held_next_;
        net_held_next_ = n;
        return nullptr;
    }
    // Scene teardown: a live session keeps flushing its EXIT in the driver.
    void net_teardown() {
        if (!net_) return;
        if (net_held_next_) {
            delete net_held_next_;
            net_held_next_ = nullptr;
        }
        if (!net_end_dialog_ && !net_->game->exiting()) net_->game->request_exit();
        if (net_hash_n_)
            std::cerr << "[net] sync hash: " << net_hash_n_ << " computed, avg "
                      << (net_hash_ms_sum_ / net_hash_n_) << " ms, max " << net_hash_ms_max_ << " ms; stalls "
                      << net_stalls_ << "\n";
        g_net_lingering.push_back(std::move(net_));
    }

    // --- Android lifecycle (05 §7.2): AWAY on background, BACK on return ---
    bool net_lifecycle(const ra2engine::InputEvent& ev) {
        if (!net_) return false;
        if (ev.kind == ra2engine::InputEvent::AppBackground) {
            if (!net_bg_away_) {
                net_bg_away_ = true;
                net_->game->send_away(false, kNetAwaySeconds);
                SDL_Log("[lifecycle] background in a network game: AWAY sent (%u s)", kNetAwaySeconds);
                pres_log(0, "net sent away");
            }
            return true;   // no pause menu, no autosave: the match is shared
        }
        if (ev.kind == ra2engine::InputEvent::AppForeground) {
            if (net_bg_away_) {
                net_bg_away_ = false;
                net_->game->send_away(true, 0);
                SDL_Log("[lifecycle] foreground in a network game: BACK sent");
                pres_log(0, "net sent back");
            }
            return true;
        }
        return false;
    }

    // --- input: chat box, stall / end dialogs -----------------------------
    void net_chat_send() {
        std::string t = net_chat_text_;
        while (!t.empty() && t.back() == ' ') t.pop_back();
        net_chat_text_.clear();
        net_chat_open_ = false;
        SDL_StopTextInput();
        if (t.empty()) return;
        const std::string me = net_->name_of(local_house_);
        net_->game->send_chat(me, t, net_->color_of(local_house_));
        post_message(me + ": " + t, 12.0, net_color(local_house_));
        pres_log(0, "net chat " + me + ": " + t);
    }
    static bool net_hit(const SDL_Rect& r, int x, int y) {
        return r.w > 0 && x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
    }
    bool net_handle(const ra2engine::InputEvent& ev) {
        if (!net_) return false;
        const bool down = ev.kind == ra2engine::InputEvent::PointerDown && ev.button == ra2engine::InputEvent::Left;
        if (net_end_dialog_) {
            if ((down && net_hit(net_btn_ok_, ev.x, ev.y)) ||
                (ev.kind == ra2engine::InputEvent::Key &&
                 (ev.key == ra2engine::kKeyEnter || ev.key == ra2engine::kKeyEscape))) {
                if (!next_scene_) next_scene_ = make_main_menu_scene(no_audio_);
            }
            return ev.kind != ra2engine::InputEvent::Quit;
        }
        if (net_reconnect_dialog_up()) {
            if (down && net_hit(net_btn_drop_, ev.x, ev.y)) {
                const int h = net_->game->session().slowest_peer();
                if (h >= 0) {
                    std::cerr << "[net] dropping house " << h << "\n";
                    net_->game->drop_player(h);
                }
                return true;
            }
            if (down && net_hit(net_btn_quit_, ev.x, ev.y)) {
                gf_abort_answer(0);
                return true;
            }
        }
        if (net_chat_open_) {
            if (ev.kind == ra2engine::InputEvent::Text) {
                if (net_chat_text_.size() < 100) net_chat_text_ += ev.text;
                return true;
            }
            if (ev.kind == ra2engine::InputEvent::Key) {
                if (ev.key == ra2engine::kKeyEnter) net_chat_send();
                else if (ev.key == ra2engine::kKeyEscape) {
                    net_chat_open_ = false;
                    net_chat_text_.clear();
                    SDL_StopTextInput();
                } else if (ev.key == ra2engine::kKeyBackspace && !net_chat_text_.empty()) {
                    do net_chat_text_.pop_back();
                    while (!net_chat_text_.empty() && (static_cast<unsigned char>(net_chat_text_.back()) & 0xC0) == 0x80);
                } else if (ev.key == ra2engine::kKeyDelete) {
                    net_chat_text_.clear();
                }
                return true;   // every key goes to the chat line while it is open
            }
            if (down && net_hit(net_btn_send_, ev.x, ev.y)) {
                net_chat_send();
                return true;
            }
            return false;
        }
        if (down && net_hit(net_btn_chat_, ev.x, ev.y)) {
            net_chat_open_ = true;
            SDL_StartTextInput();   // Android: the soft keyboard
            return true;
        }
        if (ev.kind == ra2engine::InputEvent::Key && ev.key == ra2engine::kKeyEnter && !menu_.open_) {
            net_chat_open_ = true;
            SDL_StartTextInput();
            return true;
        }
        return false;
    }
    bool net_reconnect_dialog_up() const {
        return net_ && net_stalled_ && sim_frame_ > 0 && net_away_.empty() && net_->game->session().reconnect_dialog();
    }

    // --- drawing --------------------------------------------------------------
    void net_box(SDL_Renderer* ren, const SDL_Rect& r) const {
        const SDL_Color col = hud_ui_color();
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 0xD0);
        SDL_RenderFillRect(ren, &r);
        SDL_SetRenderDrawColor(ren, col.r, col.g, col.b, 255);
        SDL_RenderDrawRect(ren, &r);
    }
    SDL_Rect net_button(SDL_Renderer* ren, ra2engine::TextureCache& tc, const std::string& label, int cx, int y,
                        double sc) const {
        const int w = static_cast<int>((hud_text_width(label) + 24) * sc);
        const int h = static_cast<int>(28 * sc);   // >= 28 px logical: a finger target
        SDL_Rect r{cx - w / 2, y, w, h};
        net_box(ren, r);
        hud_text(ren, tc, label, cx, y + static_cast<int>(8 * sc), hud_ui_color(), kHudCenter, false, true, sc);
        return r;
    }
    void net_render(SDL_Renderer* ren, ra2engine::TextureCache& tc) {
        if (!net_) return;
        const double sc = hud_touch_scale();
        const SDL_Color col = hud_ui_color();
        const int cx = playfield_w_ / 2;
        net_btn_ok_ = net_btn_drop_ = net_btn_quit_ = net_btn_send_ = SDL_Rect{0, 0, 0, 0};
        // The chat button (touch-friendly; Enter on a keyboard).
        if (!net_end_dialog_ && !menu_.open_) {
            const std::string lbl = "Chat";
            const int w = static_cast<int>((hud_text_width(lbl) + 24) * sc), h = static_cast<int>(26 * sc);
            net_btn_chat_ = SDL_Rect{playfield_w_ - w - 8, static_cast<int>(8 * sc) + 120, w, h};
            net_box(ren, net_btn_chat_);
            hud_text(ren, tc, lbl, net_btn_chat_.x + w / 2, net_btn_chat_.y + static_cast<int>(7 * sc), col, kHudCenter,
                     false, true, sc);
        } else {
            net_btn_chat_ = SDL_Rect{0, 0, 0, 0};
        }
        if (net_chat_open_) {
            const int h = static_cast<int>(28 * sc);
            const int y = kWinH - h - static_cast<int>(70 * sc);
            const std::string send = "Send";
            const int sw = static_cast<int>((hud_text_width(send) + 24) * sc);
            SDL_Rect box{16, y, std::max(200, playfield_w_ - 48 - sw), h};
            net_box(ren, box);
            const bool caret = (SDL_GetTicks() / 500) % 2 == 0;
            hud_text(ren, tc, gf_csf("TXT_TO_ALL", "To All:") + " " + net_chat_text_ + (caret ? "_" : ""), box.x + 6, y + static_cast<int>(8 * sc),
                     col, kHudLeft, false, true, sc);
            net_btn_send_ = SDL_Rect{box.x + box.w + 8, y, sw, h};
            net_box(ren, net_btn_send_);
            hud_text(ren, tc, send, net_btn_send_.x + sw / 2, y + static_cast<int>(8 * sc), col, kHudCenter, false, true, sc);
        }
        const std::uint64_t now = ra2mp::now_ms();
        int y = kWinH / 3;
        // Away players (05 §7.2): the countdown before the normal stall rules resume.
        for (const NetAway& a : net_away_) {
            const long left = static_cast<long>((a.until_ms > now ? a.until_ms - now : 0) / 1000);
            hud_text(ren, tc, net_->name_of(a.house) + " paused the game -- waiting " + std::to_string(left) + " s",
                     cx, y, net_color(a.house), kHudCenter, true, true, sc);
            y += static_cast<int>(22 * sc);
        }
        if (net_stalled_ && net_stall_since_ && now - net_stall_since_ > 1000 && !net_end_dialog_) {
            const std::string w = sim_frame_ == 0 ? "Waiting for the other players to finish loading..."
                                                  : "Waiting for players...";
            hud_text(ren, tc, w, cx, y, col, kHudCenter, true, true, sc);
            y += static_cast<int>(26 * sc);
        }
        if (net_reconnect_dialog_up()) {
            // TXT_RECONNECTING_TO / TXT_WAITING_FOR_CONNECTIONS (FUN_0064A8C0):
            // the slowest player, a countdown to the timeout kick, drop / quit.
            const int h = net_->game->session().slowest_peer();
            const ra2::net::WaitTimeouts to = ra2::net::wait_timeouts(net_->game->config().mode);
            const long waited = static_cast<long>((now - net_stall_since_) / 1000);
            const long total = static_cast<long>(to.total * 16 / 1000);
            SDL_Rect box{cx - static_cast<int>(230 * sc), y, static_cast<int>(460 * sc), static_cast<int>(110 * sc)};
            net_box(ren, box);
            hud_text(ren, tc, "Waiting for " + (h >= 0 ? net_->name_of(h) : std::string("players")) + "  (" +
                                  std::to_string(std::max(0L, total - waited)) + " s)",
                     cx, y + static_cast<int>(10 * sc), col, kHudCenter, false, true, sc);
            hud_text(ren, tc, "The game continues when the connection comes back.", cx, y + static_cast<int>(32 * sc),
                     col, kHudCenter, false, true, sc);
            net_btn_drop_ = net_button(ren, tc, "Drop player", cx - static_cast<int>(90 * sc), y + static_cast<int>(66 * sc), sc);
            net_btn_quit_ = net_button(ren, tc, "Quit game", cx + static_cast<int>(90 * sc), y + static_cast<int>(66 * sc), sc);
        }
        if (net_end_dialog_) {
            SDL_Rect box{cx - static_cast<int>(260 * sc), kWinH / 3, static_cast<int>(520 * sc), static_cast<int>(130 * sc)};
            net_box(ren, box);
            hud_text(ren, tc, net_end_title_, cx, box.y + static_cast<int>(10 * sc), col, kHudCenter, false, true, sc);
            hud_text(ren, tc, hud_wrap(net_end_text_, static_cast<int>(500)), cx, box.y + static_cast<int>(32 * sc), col,
                     kHudCenter, false, true, sc);
            net_btn_ok_ = net_button(ren, tc, "OK", cx, box.y + box.h - static_cast<int>(36 * sc), sc);
        }
    }

    // --play probe keys (scripts/play/README.md "Network play").
    void net_probe(ra2play::Probe& p) const {
        if (!net_) return;   // single player: no keys (its play_hash must not move)
        p.set("net", 1);
        p.set("sim_frame", static_cast<double>(sim_frame_));
        p.set("net_frame", static_cast<double>(net_->game->frame()));
        p.set("net_stalled", net_stalled_ ? 1 : 0);
        p.set("net_stalls", net_stalls_);
        p.set("net_players", net_->game->session().players());
        p.set("net_max_ahead", static_cast<double>(net_->game->session().max_ahead()));
        p.set("net_fsr", static_cast<double>(net_->game->session().frame_send_rate()));
        p.set("net_left", net_local_left_ ? 1 : 0);
        p.set("net_exiting", net_exit_requested_ ? 1 : 0);
        p.set("net_end", net_end_dialog_ ? 1 : 0);
        p.set_s("mode_rules", cfg_.mode_rules);   // W9-03-04 the game type's rules override
        p.set_s("rule(GAREFN.TechLevel)", rules_ini_.get("GAREFN", "TechLevel", ""));
        p.set_s("net_end_title", net_end_title_);
        p.set("net_oos_frame", static_cast<double>(net_->game->session().out_of_sync().frame));
        p.set("net_chat_open", net_chat_open_ ? 1 : 0);
        p.set("net_away", static_cast<double>(net_away_.size()));
        p.set("net_hash", static_cast<double>(net_->game->last_hash()));
        p.set("net_hash_avg_us", net_hash_n_ ? 1000.0 * net_hash_ms_sum_ / net_hash_n_ : 0.0);
        for (const ra2mp::MpPlayer& pl : net_->session.players)
            p.set("net_house_out(" + std::to_string(pl.house) + ")", world_.house_outcome(pl.house));
    }
