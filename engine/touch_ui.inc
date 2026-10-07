// W4-AM-01/03/06/07/08/11: touch-only helpers of GameScene (members, textually
// included in the class body after touch_bar.inc). Nothing here runs for a
// mouse: every entry point checks ev.touch / touch_ui_ / !mouse_present_.
// [Engine affordance, not original UI: the original is mouse + keyboard only.]

// --- W4-AM-03: an armed cursor mode is visible and has a big cancel button ---
// 0 none, 1 sell, 2 repair, 3 placement, 4 superweapon target.
int touch_mode() const {
    if (input_.placement_active()) return 3;
    if (input_.sw_targeting()) return 4;
    if (input_.base_mode() == ra2engine::Input::BaseMode::Sell) return 1;
    if (input_.base_mode() == ra2engine::Input::BaseMode::Repair) return 2;
    return 0;
}
bool touch_banner_visible() const {
    return touch_ui_ && !mouse_present_ && touch_mode() != 0 && !menu_.open_ && !sld_.is_open() &&
           !input_locked_;
}
// The banner runs along the bottom of the tactical view, right of the command
// column (the top carries the EVA messages); the [X] cancel button is its
// right end (56x56 logical px).
SDL_Rect touch_banner_rect() const {
    const int x = kTbX + 2 * (kTbW + kTbGap) + 4;
    return SDL_Rect{x, kWinH - 60, std::max(120, playfield_w_ - x - 8), 56};
}
SDL_Rect touch_cancel_rect() const {
    const SDL_Rect b = touch_banner_rect();
    return SDL_Rect{b.x + b.w - 56, b.y, 56, 56};
}
static bool in_rect(const SDL_Rect& r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}
void touch_cancel_mode() {
    // The same path as a right click: cancel ONE armed mode (placement also
    // reports place_cancelled to the queue logic), never deselect.
    ra2engine::InputEvent e;
    e.kind = ra2engine::InputEvent::PointerDown;
    e.button = ra2engine::InputEvent::Right;
    e.touch = true;
    e.hold = true;
    e.x = mx_;
    e.y = my_;
    input_.handle(e);
    e.kind = ra2engine::InputEvent::PointerUp;
    input_.handle(e);
}
void draw_touch_mode_banner(SDL_Renderer* ren) {
    if (!touch_banner_visible()) return;
    static const char* kText[5] = {"", "SELL MODE - TAP A BUILDING", "REPAIR MODE - TAP A BUILDING",
                                   "PLACE - TAP THE GROUND, TAP AGAIN TO BUILD", "TARGET - TAP THE MAP"};
    const SDL_Rect b = touch_banner_rect();
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 20, 24, 28, 0xC8);
    SDL_RenderFillRect(ren, &b);
    SDL_SetRenderDrawColor(ren, 230, 190, 60, 0xF0);
    SDL_RenderDrawRect(ren, &b);
    draw_text(ren, kText[touch_mode()], b.x + 10, b.y + (b.h - 14) / 2, 2, 240, 220, 120);
    const SDL_Rect c = touch_cancel_rect();
    SDL_SetRenderDrawColor(ren, 150, 40, 30, 0xE0);
    SDL_RenderFillRect(ren, &c);
    SDL_SetRenderDrawColor(ren, 240, 230, 210, 0xF0);
    SDL_RenderDrawRect(ren, &c);
    draw_text(ren, "X", c.x + (c.w - 15) / 2, c.y + (c.h - 21) / 2, 3, 245, 235, 215);
}

// --- W4-AM-07: the hold ring at the finger (fills up until the hold fires) ---
void draw_touch_hold_ring(SDL_Renderer* ren) {
    if (!touch_ui_ || mouse_present_ || !g_platform) return;
    int hx = 0, hy = 0;
    float pr = 0.0f;
    if (!g_platform->touch_hold(hx, hy, pr)) return;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    const int r = 26;
    const int n = static_cast<int>(48 * pr);
    for (int k = 0; k <= n; ++k) {
        const double a = -1.5707963 + 6.2831853 * k / 48.0;
        for (int d = -1; d <= 1; ++d) {
            SDL_SetRenderDrawColor(ren, pr >= 1.0f ? 120 : 240, pr >= 1.0f ? 230 : 220, 120, 0xE0);
            SDL_RenderDrawPoint(ren, hx + static_cast<int>(std::lround((r + d) * std::cos(a))),
                                hy + static_cast<int>(std::lround((r + d) * std::sin(a))));
        }
    }
}

// --- W4-AM-01: the gesture hint, shown on touch for 12 s after control starts --
void touch_hint_tick(double dt) {
    if (!touch_ui_ || input_locked_ || touch_hint_left_ <= 0.0) return;
    touch_hint_left_ -= dt;
}
void draw_touch_hint(SDL_Renderer* ren) {
    if (!touch_ui_ || input_locked_ || touch_hint_left_ <= 0.0 || menu_.open_ || sld_.is_open()) return;
    static const char* kLines[3] = {"DRAG: SCROLL    TAP: SELECT / ORDER",
                                    "DOUBLE TAP: ALL OF TYPE    HOLD + DRAG: BOX",
                                    "HOLD: CANCEL MODE    TWO-FINGER TAP: DESELECT"};
    const int w = std::min(playfield_w_ - 16, 46 * 12 + 16);
    const int y = touch_banner_visible() ? kWinH - 60 - 72 : kWinH - 72;
    const SDL_Rect r{(playfield_w_ - w) / 2, y, w, 66};
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 10, 12, 16, 0xB0);
    SDL_RenderFillRect(ren, &r);
    for (int i = 0; i < 3; ++i) {
        const int lw = static_cast<int>(std::string(kLines[i]).size()) * 12 - 2;
        const int scale = lw <= r.w - 8 ? 2 : 1;
        const int tw = static_cast<int>(std::string(kLines[i]).size()) * 6 * scale - scale;
        draw_text(ren, kLines[i], r.x + (r.w - tw) / 2, r.y + 5 + i * 20, scale, 235, 225, 200);
    }
}
double touch_hint_left_ = 12.0;

// --- W4-AM-08: forgiving sidebar targets for a finger ---------------------------
// A tap on the sidebar that hits NO element snaps to the nearest small button
// (tab, strip arrow, repair, sell) within 20 logical px -- the hit area grows
// to the full gap around the art without ever stealing a cameo tap.
bool touch_snap_sidebar(ra2engine::InputEvent& ev) const {
    if (!ev.touch || !sidebar_up_ || !lay_.contains(ev.x, ev.y)) return false;
    if (lay_.cameo_at(ev.x, ev.y) >= 0 || lay_.tab_at(ev.x, ev.y) >= 0 ||
        lay_.scroll_up.contains(ev.x, ev.y) || lay_.scroll_dn.contains(ev.x, ev.y) ||
        lay_.repair_btn.contains(ev.x, ev.y) || lay_.sell_btn.contains(ev.x, ev.y) ||
        lay_.radar.contains(ev.x, ev.y) || lay_.opt_btn.contains(ev.x, ev.y))
        return false;
    const ra2engine::SbRect* best = nullptr;
    int best_d = 21;
    auto consider = [&](const ra2engine::SbRect& r) {
        if (r.w <= 0 || r.h <= 0) return;
        const int dx = ev.x < r.x ? r.x - ev.x : ev.x >= r.x + r.w ? ev.x - (r.x + r.w - 1) : 0;
        const int dy = ev.y < r.y ? r.y - ev.y : ev.y >= r.y + r.h ? ev.y - (r.y + r.h - 1) : 0;
        const int d = std::max(dx, dy);
        if (d < best_d) { best_d = d; best = &r; }
    };
    for (const ra2engine::SbRect& t : lay_.tabs) consider(t);
    consider(lay_.scroll_up);
    consider(lay_.scroll_dn);
    if (ba_.has_repair_sell) {
        consider(lay_.repair_btn);
        consider(lay_.sell_btn);
    }
    if (!best) return false;
    ev.x = best->x + best->w / 2;
    ev.y = best->y + best->h / 2;
    return true;
}

// --- the touch router (called from handle() after the touch column) -----------
// Returns true when the event was consumed.
bool touch_ui_handle(ra2engine::InputEvent& ev) {
    if (!ev.touch) return false;
    // The mode banner's [X].
    if (touch_banner_visible() && (ev.kind == ra2engine::InputEvent::PointerDown ||
                                   ev.kind == ra2engine::InputEvent::PointerUp) &&
        in_rect(touch_cancel_rect(), ev.x, ev.y)) {
        if (ev.kind == ra2engine::InputEvent::PointerUp) touch_cancel_mode();
        return true;
    }
    // W4-AM-11: an open briefing closes on the next tap.
    if (show_briefing_ && ev.kind == ra2engine::InputEvent::PointerDown &&
        !(sidebar_up_ && lay_.contains(ev.x, ev.y))) {
        show_briefing_ = false;
        touch_swallow_up_ = true;
        return true;
    }
    if (ev.kind == ra2engine::InputEvent::PointerUp && touch_swallow_up_) {
        touch_swallow_up_ = false;
        return true;
    }
    if (ev.kind == ra2engine::InputEvent::Pan) {
        if (!(sidebar_up_ && lay_.contains(ev.x0, ev.y0))) return false;   // the map scrolls
        // W4-AM-11: a drag that starts on the radar scrolls the view with it.
        if (lay_.radar.contains(ev.x0, ev.y0)) {
            int cx = 0, cy = 0;
            if (radar_active() && radar_cell_at(ev.x, ev.y, cx, cy)) input_.center_on_cell(cx, cy);
            return true;
        }
        // A vertical swipe on the cameo strip scrolls it one row per 48 px.
        touch_strip_acc_ += ev.dy;
        while (touch_strip_acc_ <= -48) { strip_scroll(+1); touch_strip_acc_ += 48; }
        while (touch_strip_acc_ >= 48) { strip_scroll(-1); touch_strip_acc_ -= 48; }
        return true;
    }
    if (ev.kind == ra2engine::InputEvent::PointerDown || ev.kind == ra2engine::InputEvent::PointerUp)
        touch_strip_acc_ = 0;
    (void)touch_snap_sidebar(ev);   // W4-AM-08 (rewrites ev.x/y onto a button)
    return false;
}
int touch_strip_acc_ = 0;
bool touch_swallow_up_ = false;
