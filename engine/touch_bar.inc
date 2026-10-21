// Bottom-left unit command panel. Commands use existing RA2 order handlers;
// group taps select, group holds assign, battlefield double-tap selects type.
// Icons are original SDL primitives and never depend on retail icon sheets.
struct TouchBtn { const char* label; int id; };
enum : int { TB_TOGGLE = 1, TB_ADD, TB_FIRE, TB_MOVE, TB_STOP, TB_GUARD, TB_SCATTER, TB_DEPLOY, TB_BRIEF,
             TB_BOX, TB_MORE, TB_TYPE, TB_HOME, TB_EVENT, TB_NEXT, TB_PREV, TB_FOLLOW,
             TB_VIEW1 = 90, TB_GROUP1 = 100 };
// Query live selection without issuing simulation orders.
const char* touch_deploy_label() const {
    const char* result = nullptr;
    auto add = [&](const char* kind) { if (!result) result = kind; else if (std::string(result) != kind) result = "ACTION"; };
    for (int i : input_.selection().ids()) {
        if (i < 0 || i >= static_cast<int>(world_.units().size())) continue;
        const auto& u = world_.units()[i];
        if (!u.alive || u.die_time >= 0 || u.cargo_of >= 0 || !world_.controllable(u.house)) continue;
        if (!u.cargo.empty()) add("UNLOAD");
        else if (u.deploys_into >= 0 || u.deploy_fire || (u.abil.deployer && u.abil.deploy.valid))
            add(u.deployed ? "UNDEPLOY" : "DEPLOY");
    }
    const int sb = input_.selected_building();
    if (sb >= 0 && sb < static_cast<int>(world_.buildings().size())) {
        const auto& b = world_.buildings()[sb];
        if (b.house == local_house_ && !b.occupants.empty()) add("UNLOAD");
    }
    return result ? result : "DEPLOY";
}
bool touch_button_enabled(int id) const {
    if (id == TB_DEPLOY) {
        for (int i : input_.selection().ids()) {
            if (i < 0 || i >= static_cast<int>(world_.units().size())) continue;
            const auto& u = world_.units()[i];
            if (u.alive && u.die_time < 0 && u.cargo_of < 0 && world_.controllable(u.house) &&
                (!u.cargo.empty() || u.deploys_into >= 0 || u.deploy_fire || (u.abil.deployer && u.abil.deploy.valid))) return true;
        }
        const int b = input_.selected_building();
        return b >= 0 && b < static_cast<int>(world_.buildings().size()) &&
            world_.buildings()[b].house == local_house_ && !world_.buildings()[b].occupants.empty();
    }
    if (id == TB_STOP || id == TB_GUARD || id == TB_SCATTER || id == TB_FIRE || id == TB_MOVE) {
        for (int i : input_.selection().ids()) {
            if (i < 0 || i >= static_cast<int>(world_.units().size())) continue;
            const auto& u = world_.units()[i];
            if (u.alive && u.die_time < 0 && u.cargo_of < 0 && world_.controllable(u.house)) return true;
        }
        return false;
    }
    return true;
}
SDL_Rect touch_panel_rect() const {
    return SDL_Rect{8, touch_bar_top(), std::min(300, playfield_w_-16), kWinH-8-touch_bar_top()};
}
bool touch_panel_contains(int x, int y) const {
    const auto r = touch_panel_rect();
    return touch_bar_visible() && x >= r.x && x < r.x+r.w && y >= r.y && y < r.y+r.h;
}
void draw_touch_icon(SDL_Renderer* ren, int id, const SDL_Rect& r, bool enabled) {
    SDL_SetRenderDrawColor(ren, enabled ? 225 : 105, enabled ? 216 : 110, enabled ? 170 : 105, 255);
    const int x = r.x+r.w/2, y = r.y+20;
    auto line = [&](int a,int b,int c,int d){ SDL_RenderDrawLine(ren,x+a,y+b,x+c,y+d); };
    if (id == TB_STOP) { SDL_Rect q{x-9,y-9,18,18}; SDL_RenderDrawRect(ren,&q); }
    else if (id == TB_GUARD) { line(-10,-9,0,-13);line(0,-13,10,-9);line(10,-9,8,5);line(8,5,0,12);line(0,12,-8,5);line(-8,5,-10,-9); }
    else if (id == TB_DEPLOY) { line(0,-12,0,7);line(-7,0,0,7);line(0,7,7,0);line(-11,7,-11,12);line(-11,12,11,12);line(11,12,11,7); }
    else if (id == TB_FIRE || id == TB_EVENT) { SDL_Rect q{x-7,y-7,14,14}; SDL_RenderDrawRect(ren,&q);line(-13,0,13,0);line(0,-13,0,13); }
    else if (id == TB_MOVE) {line(-12,0,12,0);line(4,-8,12,0);line(12,0,4,8);}
    else if (id == TB_ADD) {line(-11,0,11,0);line(0,-11,0,11);}
    else if (id == TB_SCATTER) {for (int d : {-1,1}) {line(0,0,d*11,d*11);line(d*11,d*11,d*4,d*11);line(d*11,d*11,d*11,d*4);line(0,0,d*11,-d*11);}}
    else if (id == TB_HOME) {line(-12,0,0,-12);line(0,-12,12,0);line(-8,-4,-8,10);line(-8,10,8,10);line(8,10,8,-4);}
    else if (id == TB_BOX || id == TB_TYPE) {SDL_Rect q{x-11,y-10,22,20};SDL_RenderDrawRect(ren,&q);line(-5,-4,5,4);}
    else if (id == TB_TOGGLE || id == TB_PREV) {line(7,-10,-5,0);line(-5,0,7,10);}
    else if (id == TB_NEXT) {line(-7,-10,5,0);line(5,0,-7,10);}
    else {for (int d : {-7,0,7}) line(-11,d,11,d);}
}
void draw_touch_selection(SDL_Renderer* ren, ra2engine::TextureCache& tc) {
    const auto panel = touch_panel_rect();
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,14,22,25,235); SDL_RenderFillRect(ren,&panel);
    SDL_SetRenderDrawColor(ren,190,95,65,255); SDL_RenderDrawRect(ren,&panel);
    const int first = input_.selection().lowest();
    std::string title = "NO UNIT SELECTED";
    bool pictured = false;
    if (first >= 0 && first < static_cast<int>(world_.units().size())) {
        const auto& u = world_.units()[first]; title = u.image;
        for (size_t i=0;i<ur_.defs.size();++i) {
            if (ur_.defs[i].combat.image != u.image) continue;
            title = ur_.defs[i].id;
            if (i < ur_.has_cameo.size() && ur_.has_cameo[i]) {
                blit_shp(ren,tc,"panel:"+title,ur_.cameos[i],0,ba_.cameo_pal,panel.x+2,panel.y+2);
                pictured = true;
            }
            break;
        }
    } else if (input_.selected_building() >= 0) title = "BUILDING SELECTED";
    if (!pictured) {
        SDL_Rect p{panel.x+4,panel.y+5,54,40}; SDL_SetRenderDrawColor(ren,70,87,72,255);SDL_RenderDrawRect(ren,&p);
        draw_text(ren,"UNIT",p.x+8,p.y+16,1,170,190,150);
    }
    title = title.substr(0,28);
    draw_text(ren,title,panel.x+66,panel.y+10,1,235,225,200);
    draw_text(ren,"TAP GROUP / HOLD TO ASSIGN",panel.x+66,panel.y+30,1,165,185,165);
}
int touch_bar_top() const {
    return ra2engine::TouchBarLayout::top(kWinH, touch_btn_count());
}
const std::vector<TouchBtn>& touch_buttons() const {
    static const std::vector<TouchBtn> all = {
        {"STOP", TB_STOP}, {"GUARD", TB_GUARD}, {"DEPLOY", TB_DEPLOY}, {"SCATTER", TB_SCATTER},
        {"FIRE", TB_FIRE}, {"MOVE", TB_MOVE}, {"+SELECT", TB_ADD}, {"MORE", TB_MORE},
        {"1", TB_GROUP1}, {"2", TB_GROUP1+1}, {"3", TB_GROUP1+2}, {"4", TB_GROUP1+3}, {"5", TB_GROUP1+4}};
    // W6-01-09: page 2 -- the KeyboardMD.ini commands with no other touch path.
    static const std::vector<TouchBtn> more = {
        {"BACK", TB_TOGGLE}, {"TYPE", TB_TYPE}, {"HOME", TB_HOME}, {"EVT", TB_EVENT},
        {"NEXT", TB_NEXT}, {"PREV", TB_PREV}, {"FOLW", TB_FOLLOW}, {"6", TB_GROUP1 + 5},
        {"7", TB_GROUP1 + 6}, {"8", TB_GROUP1 + 7}, {"9", TB_GROUP1 + 8}, {"0", TB_GROUP1 + 9},
        {"V1", TB_VIEW1}, {"V2", TB_VIEW1 + 1}, {"V3", TB_VIEW1 + 2}, {"BOX", TB_BOX}, {"BRF", TB_BRIEF}};
    if (touch_page2_) return more;
    return all;
}
bool touch_bar_visible() const {
    return touch_ui_ && !menu_.open_ && !sld_.is_open() && !gf_abort_.open() && !input_locked_;
}
// Primary row sits at the bottom; extra shortcuts expand upwards.
SDL_Rect touch_btn_rect(int i) const {
    const auto r = ra2engine::TouchBarLayout::button(playfield_w_, kWinH, touch_btn_count(), i);
    return SDL_Rect{r.x, r.y, r.w, r.h};
}
int touch_btn_count() const {
    return static_cast<int>(touch_buttons().size());
}
int touch_btn_at(int x, int y) const {
    if (!touch_bar_visible()) return -1;
    const int n = touch_btn_count();
    for (int i = 0; i < n; ++i) {
        const SDL_Rect r = touch_btn_rect(i);
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h && r.y + r.h <= kWinH) return i;
    }
    return -1;
}
void touch_command(int id, bool long_press, uint32_t t_ms) {
    bool one_shot = true;
    switch (id) {
        case TB_TOGGLE: touch_page2_ = false; one_shot = false; break;
        case TB_MORE: touch_page2_ = !touch_page2_; one_shot = false; break;   // W6-01-09
        case TB_BOX:   // W6-01-11: sticky band-box drag
            touch_box_ = !touch_box_;
            ra2engine::TouchGestures::set_box_mode(touch_box_);
            one_shot = false;
            break;
        case TB_TYPE: input_.type_select(); break;          // TypeSelect FUN_00731A20
        case TB_HOME: center_base(); break;                 // CenterBase
        case TB_EVENT: hotkey_radar_event(t_ms); break;     // CenterOnRadarEvent
        case TB_NEXT: cycle_object(+1); one_shot = false; break;   // NextObject (repeatable)
        case TB_PREV: cycle_object(-1); one_shot = false; break;   // PreviousObject
        case TB_FOLLOW: hotkey_follow(); break;              // Follow
        case TB_ADD: touch_add_ = !touch_add_; one_shot = false; break;
        case TB_FIRE: touch_fire_ = !touch_fire_; touch_move_ = false; one_shot = false; break;
        case TB_MOVE: touch_move_ = !touch_move_; touch_fire_ = false; one_shot = false; break;
        case TB_STOP: input_.stop_selected(); break;
        case TB_GUARD: input_.guard_selected(); break;
        case TB_SCATTER: input_.scatter_selected(); break;
        case TB_DEPLOY: deploy_selected(); break;
        case TB_BRIEF: show_briefing_ = !show_briefing_; break;
        default:
            if (id >= TB_VIEW1 && id < TB_VIEW1 + 3) {   // W6-01-09: View / SetView 1-3
                const int v = id - TB_VIEW1;
                if (long_press) {
                    view_cam_[v][0] = cam_.x;
                    view_cam_[v][1] = cam_.y;
                    view_set_[v] = true;
                } else if (view_set_[v]) {
                    cam_.x = view_cam_[v][0];
                    cam_.y = view_cam_[v][1];
                }
            }
            if (id >= TB_GROUP1 && id < TB_GROUP1 + 10) {
                const int n = id - TB_GROUP1 + 1;
                if (long_press) input_.group_create(n);
                else input_.group_select(n, t_ms);
            }
            break;
    }
    if (one_shot) { touch_page2_ = false; } // Keep primary shortcuts visible.
}
// Consumes taps on the bar; stamps the modifier toggles on other touch
// pointer events (so Input sees Shift / Ctrl / Alt exactly as from a
// keyboard). Returns true when the event was consumed.
bool touch_bar_handle(ra2engine::InputEvent& ev) {
    if (ev.touch) touch_ui_ = true;
    // A drag that starts on the dock does not scroll the map.
    if (ev.kind == ra2engine::InputEvent::Pan && ev.touch && touch_panel_contains(ev.x0, ev.y0)) return true;
    const bool ptr = ev.kind == ra2engine::InputEvent::PointerDown || ev.kind == ra2engine::InputEvent::PointerUp;
    if (!ptr) return touch_panel_capture_ && ev.kind == ra2engine::InputEvent::PointerMove;
    if (ev.kind == ra2engine::InputEvent::PointerDown) touch_panel_capture_ = touch_panel_contains(ev.x,ev.y);
    // A map-origin selection drag must still receive its release over the panel.
    if (ev.kind == ra2engine::InputEvent::PointerUp && !touch_panel_capture_ && touch_down_btn_ < 0) return false;
    const int bi = touch_btn_at(ev.x, ev.y);
    if (bi >= 0) {
        if (ev.kind == ra2engine::InputEvent::PointerDown) {
            touch_down_btn_ = bi;
        } else if (touch_down_btn_ == bi) {
            if (touch_button_enabled(touch_buttons()[static_cast<size_t>(bi)].id))
            touch_command(touch_buttons()[static_cast<size_t>(bi)].id,
                          ev.button == ra2engine::InputEvent::Right, ev.t_ms);   // hold = right
            touch_down_btn_ = -1; touch_panel_capture_ = false;
        }
        return true;
    }
    if (ev.kind == ra2engine::InputEvent::PointerUp && touch_down_btn_ >= 0) {   // slid off a button
        touch_down_btn_ = -1; touch_panel_capture_ = false;
        return true;
    }
    const bool panel_up = ev.kind == ra2engine::InputEvent::PointerUp && touch_panel_capture_;
    if (ev.kind == ra2engine::InputEvent::PointerUp) touch_panel_capture_ = false;
    if (panel_up || touch_panel_contains(ev.x,ev.y)) return true;
    if (!ev.touch) return false;
    // W2-AR-16c: placing a building by touch -- the first tap shows the
    // footprint there, a second tap on the same cell places it.
    if (input_.placement_active() && !(sidebar_up_ && lay_.contains(ev.x, ev.y))) {
        ra2engine::InputEvent mv = ev;
        mv.kind = ra2engine::InputEvent::PointerMove;
        input_.handle(mv);
        int cx = 0, cy = 0;
        input_.placement_cell(cx, cy);
        if (ev.kind == ra2engine::InputEvent::PointerDown && ev.button == ra2engine::InputEvent::Left) {
            if (cx != touch_place_cx_ || cy != touch_place_cy_) {
                touch_place_cx_ = cx;
                touch_place_cy_ = cy;
                touch_place_swallow_up_ = true;
                return true;   // preview only
            }
        } else if (touch_place_swallow_up_) {
            touch_place_swallow_up_ = false;
            return true;
        }
    } else {
        touch_place_cx_ = touch_place_cy_ = INT_MIN;
    }
    // Modifier toggles (playfield only; the sidebar ignores modifiers).
    if (!(sidebar_up_ && lay_.contains(ev.x, ev.y))) {
        ev.shift = ev.shift || touch_add_;
        ev.ctrl = ev.ctrl || touch_fire_;
        ev.alt = ev.alt || touch_move_;
        // FIRE / MOVE are one-shot: spent by the order (a left PointerUp).
        if (ev.kind == ra2engine::InputEvent::PointerUp && ev.button == ra2engine::InputEvent::Left &&
            (touch_fire_ || touch_move_)) {
            touch_fire_ = touch_move_ = false;
        }
    }
    return false;
}
// The dock stays where the player left it; commands collapse the extra row.
void touch_bar_tick(double dt) {
    (void)dt;
    ra2engine::TouchGestures::set_box_mode(touch_box_ && touch_bar_visible());
}
void draw_touch_bar(SDL_Renderer* ren, ra2engine::TextureCache& tc) {
    if (!touch_bar_visible()) return;
    draw_touch_selection(ren, tc);
    const auto& bs = touch_buttons();
    const int n = touch_btn_count();
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < n; ++i) {
        const SDL_Rect r = touch_btn_rect(i);
        if (r.y + r.h > kWinH) break;
        const int id = bs[static_cast<size_t>(i)].id;
        const bool armed_mod = touch_add_ || touch_fire_ || touch_move_ || touch_box_;
        const bool on = (id == TB_ADD && touch_add_) || (id == TB_FIRE && touch_fire_) ||
                        (id == TB_MOVE && touch_move_) || (id == TB_TOGGLE && (touch_bar_open_ || armed_mod)) ||
                        (id == TB_BRIEF && show_briefing_) || (id == TB_BOX && touch_box_) ||
                        (id == TB_MORE && touch_page2_);
        SDL_SetRenderDrawColor(ren, on ? 150 : 20, on ? 40 : 24, on ? 30 : 28, 0xB0);
        SDL_RenderFillRect(ren, &r);
        SDL_SetRenderDrawColor(ren, 200, 70, 55, 0xE0);
        SDL_RenderDrawRect(ren, &r);
        const bool enabled = touch_button_enabled(id);
        if (!enabled) {
            SDL_SetRenderDrawColor(ren, 12, 16, 18, 180);
            SDL_RenderFillRect(ren, &r);
        }
        std::string label = id == TB_DEPLOY ? touch_deploy_label() : bs[static_cast<size_t>(i)].label;
        if (id == TB_TOGGLE && !touch_bar_open_ && armed_mod)   // closed: show what is armed
            label = touch_add_ ? "+SEL" : touch_fire_ ? "FIRE" : touch_move_ ? "MOVE" : "BOX";
        const bool has_icon = id < TB_VIEW1 && id != TB_BRIEF;
        if (has_icon) draw_touch_icon(ren, id, r, enabled);
        const int scale = label.size() <= 2 ? 2 : 1;
        const int lw = static_cast<int>(label.size()) * 6 * scale - scale;
        const int ly = has_icon ? r.y + r.h - 14 : r.y + (r.h - 7*scale)/2;
        draw_text(ren, label, r.x + (r.w-lw)/2, ly, scale,
                  enabled ? 235 : 115, enabled ? 225 : 120, enabled ? 200 : 115);
    }
}
#ifdef __ANDROID__
bool touch_ui_ = true;    // a phone: the dock is there from the first frame
#else
bool touch_ui_ = false;   // desktop: appears after the first touch event
#endif
bool touch_bar_open_ = true; // Compatibility state for diagnostics.
bool touch_add_ = false, touch_fire_ = false, touch_move_ = false;
bool touch_box_ = false;     // W6-01-11 sticky BOX (TouchGestures::set_box_mode)
bool touch_page2_ = false;   // W6-01-09 the MORE page
int touch_down_btn_ = -1;
int touch_place_cx_ = INT_MIN, touch_place_cy_ = INT_MIN;
bool touch_place_swallow_up_ = false;

bool touch_panel_capture_ = false;
