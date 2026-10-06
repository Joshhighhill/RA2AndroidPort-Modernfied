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
