// W2-AR-08 / -07 / -16: the touch command bar (GameScene members, textually
// included in the class body). Every hotkey-only command gets an on-screen
// affordance once the player uses touch (or always on Android):
//
//   [CMD] collapse / expand
//   [+SEL] sticky Shift  (taps ADD to the selection)
//   [FIRE] one-shot Ctrl (force-fire the next order)
//   [MOVE] one-shot Alt  (force-move the next order)
//   [STOP] [GRD] [SCAT] [DPLY]  StopObject / GuardObject / ScatterObject / DeployObject
//   [1]..[5]  tap = TeamSelect_n (double tap centres), long-press = TeamCreate_n
//
// The buttons call the SAME command handlers the KeyboardMD.ini hotkeys use
// (game_scene_controls.inc hotkey()), so behaviour is identical. Buttons are
// 50x56 logical px: >= 48 dp tall on a phone, where the touch canvas is ~480
// logical px high (game_logical_size()). Desktop with a mouse never shows it.
// [Engine affordance, not original UI: the original is mouse + keyboard only.]
struct TouchBtn { const char* label; int id; };
enum : int { TB_TOGGLE = 1, TB_ADD, TB_FIRE, TB_MOVE, TB_STOP, TB_GUARD, TB_SCATTER, TB_DEPLOY, TB_GROUP1 };
static constexpr int kTbW = 50, kTbH = 56, kTbGap = 2;   // 13 x 52 fits the 685 px tactical view of an 854x480 canvas
static const std::vector<TouchBtn>& touch_buttons() {
    static const std::vector<TouchBtn> b = {
        {"CMD", TB_TOGGLE}, {"+SEL", TB_ADD}, {"FIRE", TB_FIRE}, {"MOVE", TB_MOVE},
        {"STOP", TB_STOP}, {"GRD", TB_GUARD}, {"SCAT", TB_SCATTER}, {"DPLY", TB_DEPLOY},
        {"1", TB_GROUP1}, {"2", TB_GROUP1 + 1}, {"3", TB_GROUP1 + 2}, {"4", TB_GROUP1 + 3}, {"5", TB_GROUP1 + 4}};
    return b;
}
bool touch_bar_visible() const {
    return touch_ui_ && !menu_.open_ && !sld_.is_open() && !gf_abort_.open();
}
// Button i's rectangle: a strip along the bottom of the tactical view.
SDL_Rect touch_btn_rect(int i) const {
    return SDL_Rect{4 + i * (kTbW + kTbGap), kWinH - kTbH - 4, kTbW, kTbH};
}
int touch_btn_at(int x, int y) const {
    if (!touch_bar_visible()) return -1;
    const auto& bs = touch_buttons();
    const int n = touch_bar_open_ ? static_cast<int>(bs.size()) : 1;
    for (int i = 0; i < n; ++i) {
        const SDL_Rect r = touch_btn_rect(i);
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h && r.x + r.w <= playfield_w_) return i;
    }
    return -1;
}
void touch_command(int id, bool long_press, uint32_t t_ms) {
    switch (id) {
        case TB_TOGGLE: touch_bar_open_ = !touch_bar_open_; break;
        case TB_ADD: touch_add_ = !touch_add_; break;
        case TB_FIRE: touch_fire_ = !touch_fire_; touch_move_ = false; break;
        case TB_MOVE: touch_move_ = !touch_move_; touch_fire_ = false; break;
        case TB_STOP: input_.stop_selected(); break;
        case TB_GUARD: input_.guard_selected(); break;
        case TB_SCATTER: input_.scatter_selected(); break;
        case TB_DEPLOY: deploy_selected(); break;
        default:
            if (id >= TB_GROUP1 && id < TB_GROUP1 + 5) {
                const int n = id - TB_GROUP1 + 1;
                if (long_press) input_.group_create(n);
                else input_.group_select(n, t_ms);
            }
            break;
    }
}
// Consumes taps on the bar; stamps the modifier toggles on other touch
// pointer events (so Input sees Shift / Ctrl / Alt exactly as from a
// keyboard). Returns true when the event was consumed.
bool touch_bar_handle(ra2engine::InputEvent& ev) {
    if (ev.touch) touch_ui_ = true;
    const bool ptr = ev.kind == ra2engine::InputEvent::PointerDown || ev.kind == ra2engine::InputEvent::PointerUp;
    if (!ptr) return false;
    const int bi = touch_btn_at(ev.x, ev.y);
    if (bi >= 0) {
        if (ev.kind == ra2engine::InputEvent::PointerDown) {
            touch_down_btn_ = bi;
        } else if (touch_down_btn_ == bi) {
            touch_command(touch_buttons()[static_cast<size_t>(bi)].id,
                          ev.button == ra2engine::InputEvent::Right, ev.t_ms);   // long-press = right
            touch_down_btn_ = -1;
        }
        return true;
    }
    if (ev.kind == ra2engine::InputEvent::PointerUp && touch_down_btn_ >= 0) {   // slid off a button
        touch_down_btn_ = -1;
        return true;
    }
    if (!ev.touch) return false;
    // W2-AR-16c: placing a building by touch -- the first tap shows the
    // footprint there, a second tap on the same cell places it.
    if (input_.placement_active() && !(sidebar_up_ && lay_.contains(ev.x, ev.y))) {
        ra2engine::InputEvent mv = ev;
        mv.kind = ra2engine::InputEvent::PointerMove;
        input_.handle(mv);
        int cx = 0, cy = 0;
        input_.placement_cell(cx, cy);
        if (ev.kind == ra2engine::InputEvent::PointerDown) {
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
void draw_touch_bar(SDL_Renderer* ren) {
    if (!touch_bar_visible()) return;
    const auto& bs = touch_buttons();
    const int n = touch_bar_open_ ? static_cast<int>(bs.size()) : 1;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < n; ++i) {
        const SDL_Rect r = touch_btn_rect(i);
        if (r.x + r.w > playfield_w_) break;
        const int id = bs[static_cast<size_t>(i)].id;
        const bool on = (id == TB_ADD && touch_add_) || (id == TB_FIRE && touch_fire_) ||
                        (id == TB_MOVE && touch_move_) || (id == TB_TOGGLE && touch_bar_open_);
        SDL_SetRenderDrawColor(ren, on ? 150 : 20, on ? 40 : 24, on ? 30 : 28, 0xB0);
        SDL_RenderFillRect(ren, &r);
        SDL_SetRenderDrawColor(ren, 200, 70, 55, 0xE0);
        SDL_RenderDrawRect(ren, &r);
        const std::string label = bs[static_cast<size_t>(i)].label;
        const int scale = label.size() <= 2 ? 3 : 2;
        const int lw = static_cast<int>(label.size()) * 6 * scale - scale;
        draw_text(ren, label, r.x + (r.w - lw) / 2, r.y + (r.h - 7 * scale) / 2, scale, 235, 225, 200);
    }
}
#ifdef __ANDROID__
bool touch_ui_ = true;    // a phone: the bar is there from the first frame
#else
bool touch_ui_ = false;   // desktop: appears after the first touch event
#endif
bool touch_bar_open_ = true;
bool touch_add_ = false, touch_fire_ = false, touch_move_ = false;
int touch_down_btn_ = -1;
int touch_place_cx_ = INT_MIN, touch_place_cy_ = INT_MIN;
bool touch_place_swallow_up_ = false;
