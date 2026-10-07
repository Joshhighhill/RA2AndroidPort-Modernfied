// w2air_play.inc -- wave-2 air/naval/superweapon (W2-AN-*) --play support,
// #included INSIDE the GameScene class body right after
// game_scene_play_probe.inc. Probe keys, named map points and dev cheats for
// scripts/play/{naval,air,sw}.play. Interactive-only: no gate reaches it.

    // The first selected live house-0 unit, or -1.
    int w2_first_selected() const {
        const auto& us = world_.units();
        for (int i = 0; i < static_cast<int>(us.size()); ++i)
            if (us[i].selected && us[i].house == 0 && us[i].alive && us[i].die_time < 0.0) return i;
        return -1;
    }
    bool w2_water_cell(int x, int y) const {
        const auto& g = world_.grid_for(ra2engine::MovementDomain::Water);
        if (x < 0 || y < 0 || x >= world_.grid_w() || y >= world_.grid_h() || g.empty()) return false;
        return g[static_cast<std::size_t>(y) * world_.grid_w() + x] != 0;
    }

    // Named points (`point NAME` / `click point NAME`):
    //   water_near_sel  a free water cell 4..8 cells from the first selected unit
    //                   that the selection can enter
    //   shore_near_sel  the nearest LAND cell (walkable, free) that touches water,
    //                   from the first selected unit
    //   beach_near_sel  the nearest WATER cell touching land from the selected unit
    //   pad_free        (reserved)
    bool w2_find_point(const std::string& name, double& cx, double& cy) const {
        const int si = w2_first_selected();
        if (si < 0) return false;
        const ra2engine::Unit& s = world_.units()[si];
        const int gw = world_.grid_w(), gh = world_.grid_h();
        auto free_at = [&](int x, int y) { return world_.occupancy().is_free(x, y) && world_.building_at_cell(x, y) < 0; };
        if (name == "water_near_sel") {
            for (int r = 6; r <= 12; ++r)
                for (int dy = -r; dy <= r; ++dy)
                    for (int dx = -r; dx <= r; ++dx) {
                        if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                        const int x = s.cell_x + dx, y = s.cell_y + dy;
                        if (!w2_water_cell(x, y) || !free_at(x, y)) continue;
                        if (!world_.selection_can_enter(0, x, y)) continue;
                        // keep away from the shore so a group fans out on water
                        bool open = true;
                        for (int k = -1; k <= 1 && open; ++k)
                            for (int l = -1; l <= 1 && open; ++l) open = w2_water_cell(x + k, y + l);
                        if (!open) continue;
                        cx = x; cy = y;
                        return true;
                    }
            return false;
        }
        if (name == "shore_near_sel" || name == "beach_near_sel") {
            const bool want_land = name == "shore_near_sel";
            int best = -1, bx = 0, by = 0;
            for (int y = 0; y < gh; ++y)
                for (int x = 0; x < gw; ++x) {
                    const bool water = w2_water_cell(x, y);
                    if (want_land ? (water || !world_.walkable(x, y)) : !water) continue;
                    if (!free_at(x, y)) continue;
                    bool touches = false;
                    for (int k = -1; k <= 1 && !touches; ++k)
                        for (int l = -1; l <= 1 && !touches; ++l)
                            touches = want_land ? w2_water_cell(x + k, y + l)
                                                : (world_.walkable(x + k, y + l) && !w2_water_cell(x + k, y + l));
                    if (!touches) continue;
                    const int d = (x - s.cell_x) * (x - s.cell_x) + (y - s.cell_y) * (y - s.cell_y);
                    if (best < 0 || d < best) { best = d; bx = x; by = y; }
                }
            if (best < 0) return false;
            cx = bx; cy = by;
            return true;
        }
        return false;
    }

    void w2_play_probe(ra2play::Probe& p) const {
        p.set_s("cursor_name", ra2engine::Input::action_name(cursor_action()));
        const int si = w2_first_selected();
        if (si >= 0) {
            const ra2engine::Unit& u = world_.units()[si];
            p.set("sel_on_water", w2_water_cell(u.cell_x, u.cell_y) ? 1 : 0);
            p.set("sel_hp", u.hp);
            p.set("sel_max_hp", u.max_hp);
            p.set("sel_cargo", static_cast<double>(u.cargo.size()));
            p.set("sel_altitude", u.altitude);
        }
    }

    // Cheats: `cheat w2 ...` (dev hooks, acceptance prefers real input).
    bool w2_cheat(const std::vector<std::string>& a, std::string& out) {
        (void)a;
        out.clear();
        return false;
    }
