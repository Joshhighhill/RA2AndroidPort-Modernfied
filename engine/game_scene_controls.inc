// Agent B (controls/UI) GameScene members, #included INSIDE the GameScene class
// body (public section) to keep main.cpp edits to single include/call lines.

    // G-08: the cursor STATE under the pointer (what a left click would do).
    // Agent E draws the cursor art from this; over the sidebar it is None.
    ra2engine::Input::Action cursor_action() const {
        if (sidebar_up_ && lay_.contains(mx_, my_)) return ra2engine::Input::Action::None;
        return input_.action_at(mx_, my_);
    }

    // G-10: selection bracket + health bar over the selected own building.
    // [RE-FALLBACK: the original's building health pips (pips.shp frames) are
    // agent E's art; this draws the same information with plain rects.]
    // Colour thresholds = [General] ConditionYellow / ConditionRed.
    void draw_selected_building(SDL_Renderer* ren) const {
        const int sb = input_.selected_building();
        if (sb < 0 || sb >= static_cast<int>(world_.buildings().size())) return;
        const ra2engine::Building& b = world_.buildings()[sb];
        if (!b.alive || b.state == ra2engine::Building::State::Dead) return;
        // Footprint corner pixels (top, left, right, bottom of the diamond).
        auto px = [&](int cx, int cy, int& x, int& y) {
            const ra2assets::MapCell* c = world_.map().at(cx, cy);
            const int level = c ? c->level : 0;
            x = (cx - cy) * 30 + 30 + cam_.x;
            y = (cx + cy) * 15 - level * 15 + cam_.y;
        };
        int tx, ty, lx, ly, rx, ry, bx, by;
        px(b.cx, b.cy, tx, ty);
        px(b.cx, b.cy + b.fh, lx, ly);
        px(b.cx + b.fw, b.cy, rx, ry);
        px(b.cx + b.fw, b.cy + b.fh, bx, by);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 230);
        const int k = 10;   // bracket arm length
        SDL_RenderDrawLine(ren, tx - k, ty + k / 2, tx, ty);
        SDL_RenderDrawLine(ren, tx, ty, tx + k, ty + k / 2);
        SDL_RenderDrawLine(ren, bx - k, by - k / 2, bx, by);
        SDL_RenderDrawLine(ren, bx, by, bx + k, by - k / 2);
        SDL_RenderDrawLine(ren, lx, ly - k / 2, lx, ly + k / 2);
        SDL_RenderDrawLine(ren, rx, ry - k / 2, rx, ry + k / 2);
        const double frac = b.max_hp > 0 ? static_cast<double>(b.hp) / b.max_hp : 0.0;
        const double red = rules_.general_double("ConditionRed", 25.0) / 100.0;
        const int w = std::max(24, (rx - lx) / 2);
        SDL_Rect back{tx - w / 2, ty - 14, w, 5};
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 200);
        SDL_RenderFillRect(ren, &back);
        if (frac > ba_.condition_yellow) SDL_SetRenderDrawColor(ren, 40, 220, 40, 255);
        else if (frac > red) SDL_SetRenderDrawColor(ren, 230, 220, 40, 255);
        else SDL_SetRenderDrawColor(ren, 230, 40, 40, 255);
        SDL_Rect fill{back.x + 1, back.y + 1, static_cast<int>((w - 2) * frac), 3};
        SDL_RenderFillRect(ren, &fill);
    }

    // --- G-04: KeyboardMD.ini hotkeys ---------------------------------------
    // The install's KeyboardMD.ini (the original's binding file) overrides the
    // shipped defaults; a missing file leaves the defaults.
    static ra2engine::HotkeyTable load_hotkeys() {
        ra2engine::HotkeyTable t = ra2engine::HotkeyTable::defaults();
        try {
            const std::vector<uint8_t> raw = ra2assets::read_file(env_install_dir() + "KeyboardMD.ini");
            t.load_ini_text(std::string(raw.begin(), raw.end()));
        } catch (const std::exception&) { /* defaults */ }
        return t;
    }
    ra2engine::HotkeyTable hotkeys_ = load_hotkeys();
    int view_cam_[4][2] = {};          // View1..4 bookmarks (SetView = Ctrl+F1..F4)
    bool view_set_[4] = {false, false, false, false};

    void switch_tab(int tab) {
        if (tab < 0 || tab > 3 || tab == active_tab_) return;
        active_tab_ = tab;
        buildable_ = tab_buildable(active_tab_);
        lay_ = ra2engine::SidebarLayout::compute(kWinW, kWinH, buildable_);
    }

    // CenterBase [RE: FUN_00730670]: the player's ConYard (the primary one if
    // flagged), else any own building, else the first own unit.
    void center_base() {
        int pick = -1;
        for (int i = 0; i < static_cast<int>(world_.buildings().size()); ++i) {
            const ra2engine::Building& b = world_.buildings()[i];
            if (!b.alive || b.house != 0 || b.state == ra2engine::Building::State::Dead) continue;
            const bool cy = b.type >= 0 && b.type < static_cast<int>(ba_.roster.size()) &&
                            ba_.roster[b.type].conyard;
            if (cy && (pick < 0 || b.primary || !ba_.roster[world_.buildings()[pick].type].conyard)) {
                pick = i;
                if (b.primary) break;
            } else if (pick < 0) {
                pick = i;
            }
        }
        if (pick >= 0) {
            const ra2engine::Building& b = world_.buildings()[pick];
            input_.center_on_cell(b.cx + b.fw / 2.0 - 0.5, b.cy + b.fh / 2.0 - 0.5);
            return;
        }
        for (const ra2engine::Unit& u : world_.units())
            if (u.house == 0 && u.alive && u.die_time < 0.0 && u.cargo_of < 0) {
                input_.center_on_cell(u.fx, u.fy);
                return;
            }
    }

    // DeployObject (D): unload selected transports + toggle deployers (the
    // former raw-scancode path, now a real key event).
    void deploy_selected() {
        world_.order_unload_selected();
        for (int i = 0; i < static_cast<int>(world_.units().size()); ++i)
            if (world_.units()[i].selected && world_.units()[i].house == 0) world_.order_deploy(i);
    }

    // Next/PreviousObject [RE-FALLBACK: FUN_005362D0/536740 execs not
    // transcribed]: cycle the selection through own alive units and centre.
    void cycle_object(int dir) {
        const auto& us = world_.units();
        const int n = static_cast<int>(us.size());
        if (n == 0) return;
        int start = -1;
        for (int i = 0; i < n; ++i)
            if (us[i].selected) { start = i; break; }
        for (int k = 1; k <= n; ++k) {
            const int i = ((start < 0 ? (dir > 0 ? -1 : 0) : start) + dir * k + n * 4) % n;
            const ra2engine::Unit& u = us[i];
            if (u.house != 0 || !u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) continue;
            world_.clear_selection();
            world_.units()[i].selected = true;
            input_.center_on_selection();
            return;
        }
    }

    // Returns true when the key was a bound command (consumed).
    bool hotkey(const ra2engine::InputEvent& ev) {
        if (ev.kind != ra2engine::InputEvent::Key || ev.vk == 0) return false;
        const ra2engine::HotBinding b = hotkeys_.lookup(ev.vk, ev.shift, ev.ctrl, ev.alt);
        using C = ra2engine::HotCmd;
        switch (b.cmd) {
            case C::None: return false;
            case C::Options: menu_.open_ = true; return true;
            case C::StopObject: input_.stop_selected(); return true;
            case C::GuardObject: input_.guard_selected(); return true;
            case C::ScatterObject: input_.scatter_selected(); return true;
            case C::DeployObject: deploy_selected(); return true;
            case C::TypeSelect: input_.type_select(); return true;
            case C::TeamCreate: input_.group_create(b.arg); return true;
            case C::TeamSelect: input_.group_select(b.arg, ev.t_ms); return true;
            case C::TeamAddSelect: input_.group_add_select(b.arg); return true;
            case C::TeamCenter: input_.group_center(b.arg); return true;
            case C::CenterView: input_.center_on_selection(); return true;
            case C::CenterBase: center_base(); return true;
            case C::ToggleRepair: if (ba_.has_repair_sell) input_.set_repair_mode(-1); return true;
            case C::ToggleSell: if (ba_.has_repair_sell) input_.set_sell_mode(-1); return true;
            case C::StructureTab: switch_tab(0); return true;
            case C::DefenseTab: switch_tab(1); return true;
            case C::InfantryTab: switch_tab(2); return true;
            case C::UnitTab: switch_tab(3); return true;
            case C::NextObject: cycle_object(+1); return true;
            case C::PreviousObject: cycle_object(-1); return true;
            case C::SetView:
                view_cam_[b.arg - 1][0] = cam_.x;
                view_cam_[b.arg - 1][1] = cam_.y;
                view_set_[b.arg - 1] = true;
                return true;
            case C::View:
                if (view_set_[b.arg - 1]) {
                    cam_.x = view_cam_[b.arg - 1][0];
                    cam_.y = view_cam_[b.arg - 1][1];
                }
                return true;
            case C::SidebarUp: strip_scroll(-1); return true;
            case C::SidebarDown: strip_scroll(+1); return true;
            default:
                // Bound in KeyboardMD.ini but with no feature behind it in this
                // engine (beacons, planning mode, alliances, taunts, cheer,
                // follow, combatant select, veterancy nav, page user, delete,
                // screen capture, radar events): consumed, no effect.
                return true;
        }
    }

    // --- G-12: sidebar strip scrolling (r-up / r-dn gadgets + SidebarUp/Down) --
    // [RE: SidebarUp/Down exec -> FUN_006A6090(+-1); RE-FALLBACK: one row per
    // press; the strip returns to the top on a tab switch.]
    void strip_scroll(int rows) {
        const int top = std::max(0, std::min(lay_.top_row + rows, lay_.max_top_row(static_cast<int>(buildable_.size()))));
        if (top == lay_.top_row) return;
        lay_ = ra2engine::SidebarLayout::compute(kWinW, kWinH, buildable_, top);
    }
    bool strip_button_click(int x, int y) {
        if (lay_.scroll_up.contains(x, y)) { strip_scroll(-1); return true; }
        if (lay_.scroll_dn.contains(x, y)) { strip_scroll(+1); return true; }
        return false;
    }

    // --- G-13: fixed logic tick ------------------------------------------------
    // [RE-FALLBACK: 15 Hz = the rules' time base (ROF / build / mission frames
    // are 15 fps units, docs/re/*) until agent C pins the per-GameSpeed logic
    // rate (RE-T1); change kLogicDt only.] At most kMaxLogicSteps ticks run in
    // one frame [RE-FALLBACK: catch-up cap].
    static constexpr double kLogicDt = 1.0 / 15.0;
    static constexpr int kMaxLogicSteps = 4;
    double logic_acc_ = 0.0;
