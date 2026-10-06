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

    // --- G-07: build-queue click semantics ---------------------------------------
    // [RE: gamemd fixB hunt, StripClass cameo action FUN_006AA390; event ids
    // PRODUCE=0xE, SUSPEND=0xF, ABANDON=0x10, ABANDON_ALL=0x2E, PLACE=0xB]
    //  LEFT  (flag 0x01): this type is the current product and it is ON HOLD ->
    //        PRODUCE (resume); it is DONE -> building: start placement; else a
    //        BUILDING while the building factory is busy -> refused; otherwise
    //        PRODUCE = start, or for units queue one more (FactoryClass queue,
    //        capped by [General] MaximumQueuedObjects).
    //  RIGHT (flag 0x10): the current product running -> SUSPEND (hold); held
    //        or done -> ABANDON one (Shift: ABANDON_ALL); a type that is only
    //        queued -> ABANDON one queued copy.
    // The queue model (ProductionQueues, agent A's) holds ONE slot per category;
    // the copies waiting behind it live here (backlog_) and start as the slot
    // frees, so the factory semantics stay in production.cpp untouched.
    // [RE-FALLBACK: backlog / hold are not written to save games yet.]
    std::map<int, std::vector<int>> backlog_;   // QueueCat -> waiting unit types (FIFO)
    std::set<int> held_;                        // QueueCat values on hold
    int max_queued() const {
        return static_cast<int>(rules_.general_double("MaximumQueuedObjects", 29.0));
    }
    ra2engine::QueueCat unit_cat_of_tab() const {
        return active_tab_ == 3 ? ra2engine::QueueCat::Vehicle : ra2engine::QueueCat::Infantry;
    }
    void refund_cancel(ra2engine::QueueCat c) {
        queues_.set_credits(player_credits_);
        queues_.cancel(c);
        queues_.reconcile_refund();
        player_credits_ = queues_.structure().credits;
        held_.erase(static_cast<int>(c));
    }
    int queued_count(int type, ra2engine::QueueCat c) const {
        int n = queues_.producing(c) == type ? 1 : 0;
        const auto it = backlog_.find(static_cast<int>(c));
        if (it != backlog_.end())
            for (int t : it->second) n += (t == type);
        return n;
    }
    void cameo_click(int type, bool right, bool shift) {
        if (active_tab_ >= 2) {   // ---- units ----
            const ra2engine::QueueCat cat = unit_cat_of_tab();
            const int ci = static_cast<int>(cat);
            const bool current = queues_.producing(cat) == type;
            const bool held = held_.count(ci) != 0;
            std::vector<int>& bl = backlog_[ci];
            if (right) {
                if (shift) {   // ABANDON_ALL of this type
                    bl.erase(std::remove(bl.begin(), bl.end(), type), bl.end());
                    if (current) refund_cancel(cat);
                    return;
                }
                if (current && !held && !queues_.ready(cat)) { held_.insert(ci); return; }   // SUSPEND
                const auto last = std::find(bl.rbegin(), bl.rend(), type);
                if (last != bl.rend()) { bl.erase(std::next(last).base()); return; }   // ABANDON a queued copy
                if (current) refund_cancel(cat);                                         // ABANDON current
                return;
            }
            if (current && held) { held_.erase(ci); return; }   // PRODUCE = resume
            const std::vector<int>& ft = (cat == ra2engine::QueueCat::Vehicle) ? veh_ft_ : inf_ft_;
            if (queues_.producing(cat) == -1) {
                if (ra2engine::can_start_unit(queues_.state(cat), type, ur_.defs, world_.buildings(),
                                              ba_.roster, 0, ba_.groups,
                                              !world_.factory_category_lost(ft, 0))) {
                    queues_.set_credits(player_credits_);
                    queues_.start_unit(cat, type, ur_.defs, build_speed_);
                }
                return;
            }
            // Busy: queue one more of this type (same buildability test as a start,
            // evaluated against an idle slot), capped by MaximumQueuedObjects.
            ra2engine::ProductionState probe_ps;
            if (static_cast<int>(bl.size()) + 1 < max_queued() &&
                ra2engine::can_start_unit(probe_ps, type, ur_.defs, world_.buildings(), ba_.roster, 0,
                                          ba_.groups, !world_.factory_category_lost(ft, 0)))
                bl.push_back(type);
            return;
        }
        // ---- buildings (one at a time) ----
        const ra2engine::QueueCat bcat =
            (active_tab_ == 1) ? ra2engine::QueueCat::Defense : ra2engine::QueueCat::Structure;
        const int bi = static_cast<int>(bcat);
        ra2engine::ProductionState& bq = queues_.state(bcat);
        const bool current = bq.slot.type == type;
        const bool held = held_.count(bi) != 0;
        if (right) {
            if (!current) return;
            if (!held && !bq.slot.ready) { held_.insert(bi); return; }   // SUSPEND
            refund_cancel(bcat);                                         // ABANDON
            return;
        }
        if (current && bq.slot.ready) {
            place_cat_ = bcat;
            input_.begin_placement(type, ba_.roster[type].fw, ba_.roster[type].fh);
            return;
        }
        if (current && held) { held_.erase(bi); return; }   // resume
        if (current) return;                                // running: no effect
        if (ra2engine::can_start(bq, type, ba_.roster, world_.buildings(), 0, ba_.groups,
                                 conyard_alive(world_, ba_, 0))) {
            queues_.set_credits(player_credits_);
            ra2engine::start(bq, type, ba_.roster, build_speed_);
        }
    }
    // A held queue skips its production tick: tick() early-outs on a READY
    // slot, so mark held slots ready for the duration of prod_tick only.
    std::vector<int> hold_marked_;
    void hold_begin() {
        hold_marked_.clear();
        for (int c : held_) {
            ra2engine::ProductionState& ps = queues_.state(static_cast<ra2engine::QueueCat>(c));
            if (ps.slot.type >= 0 && !ps.slot.ready) {
                ps.slot.ready = true;
                hold_marked_.push_back(c);
            }
        }
    }
    void hold_end() {
        for (int c : hold_marked_) {
            ra2engine::ProductionState& ps = queues_.state(static_cast<ra2engine::QueueCat>(c));
            if (ps.slot.type >= 0) ps.slot.ready = false;
        }
        hold_marked_.clear();
        // A slot that emptied (ConYard death cancel) drops its hold.
        for (auto it = held_.begin(); it != held_.end();)
            it = (queues_.state(static_cast<ra2engine::QueueCat>(*it)).slot.type < 0) ? held_.erase(it) : std::next(it);
    }
    void refill_unit_queues() {
        for (auto& [c, bl] : backlog_) {
            const ra2engine::QueueCat cat = static_cast<ra2engine::QueueCat>(c);
            if (bl.empty() || queues_.producing(cat) != -1) continue;
            const std::vector<int>& ft = (cat == ra2engine::QueueCat::Vehicle) ? veh_ft_ : inf_ft_;
            const int type = bl.front();
            bl.erase(bl.begin());
            if (ra2engine::can_start_unit(queues_.state(cat), type, ur_.defs, world_.buildings(),
                                          ba_.roster, 0, ba_.groups,
                                          !world_.factory_category_lost(ft, 0))) {
                queues_.set_credits(player_credits_);
                queues_.start_unit(cat, type, ur_.defs, build_speed_);
            }
        }
    }
    // Count badge (x+60, y+1, right-aligned) and "on hold" marker (x+30, y+1)
    // over the visible cameos [RE: construction.md §11a strip draw 0x6A8BD0].
    // [RE-FALLBACK: drawn with the rect font; the original uses the game font.]
    void draw_queue_badges(SDL_Renderer* ren) const {
        if (!(ba_.ok && ba_.sidebar_ok)) return;
        for (const ra2engine::CameoCell& cell : lay_.cameos) {
            ra2engine::QueueCat cat;
            if (active_tab_ >= 2) cat = unit_cat_of_tab();
            else cat = active_tab_ == 1 ? ra2engine::QueueCat::Defense : ra2engine::QueueCat::Structure;
            const int n = queued_count(cell.type, cat);
            if (n > 1) {
                const std::string s = std::to_string(n);
                draw_text(ren, s, cell.rect.x + 60 - static_cast<int>(s.size()) * 6 - 1, cell.rect.y + 2, 1,
                          255, 255, 255);
            }
            if (queues_.producing(cat) == cell.type && held_.count(static_cast<int>(cat)))
                draw_text(ren, "HOLD", cell.rect.x + 30 - 12, cell.rect.y + 2, 1, 255, 230, 120);
        }
    }

    // --- G-09: radar click [RE: RadarClass action; TS lineage RadarClass::Action:
    // a click on the ACTIVE radar with a selection is a move order to that cell,
    // otherwise it re-centres the tactical view; an inactive radar (no powered
    // radar building / shroud off) ignores clicks.] [RE-FALLBACK: the
    // cell <-> radar-pixel mapping is the clone's draw_radar_minimap stretch
    // (grid scaled to the radar rect); if agent E moves the minimap to the
    // original 140x108 iso viewport, radar_cell_at must follow it.]
    bool radar_active() const {
        return world_.shroud_enabled() && house_owns_operational_radar(world_, ba_, 0);
    }
    bool radar_cell_at(int px, int py, int& cx, int& cy) const {
        const ra2engine::SbRect& r = lay_.radar;
        if (!r.contains(px, py) || world_.grid_w() <= 0 || world_.grid_h() <= 0) return false;
        const double sx = static_cast<double>(r.w) / world_.grid_w();
        const double sy = static_cast<double>(r.h) / world_.grid_h();
        cx = std::min(world_.grid_w() - 1, static_cast<int>((px - r.x) / sx));
        cy = std::min(world_.grid_h() - 1, static_cast<int>((py - r.y) / sy));
        return true;
    }
    bool radar_click(const ra2engine::InputEvent& ev) {
        if (!lay_.radar.contains(ev.x, ev.y)) return false;
        int cx = 0, cy = 0;
        if (!radar_active() || !radar_cell_at(ev.x, ev.y, cx, cy)) return true;   // consumed, inert
        bool any = false;
        for (const ra2engine::Unit& u : world_.units())
            if (u.selected && u.house == 0 && u.alive && u.die_time < 0.0) { any = true; break; }
        if (any) world_.order_move_selected(cx, cy);
        else input_.center_on_cell(cx, cy);
        return true;
    }
