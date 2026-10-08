// G0 --play harness surface of GameScene, #included INSIDE the class body
// (public section) so it can read the scene's private state without touching
// the rest of main.cpp. Interactive-only: nothing here runs in a gate.

    const char* name() const override { return "game"; }

    // Pointer position + "a real mouse is present" from the event stream
    // (G-71: SDL_GetMouseState is the synthetic touch position on Android and
    // undefined for a hidden --play window).
    void track_pointer(const ra2engine::InputEvent& ev) {
        if (ev.kind != ra2engine::InputEvent::PointerMove &&
            ev.kind != ra2engine::InputEvent::PointerDown &&
            ev.kind != ra2engine::InputEvent::PointerUp)
            return;
        mx_ = ev.x;
        my_ = ev.y;
        mouse_present_ = !ev.touch;
    }

    // Window pixel at the centre of map cell (cx, cy), level-aware (the same
    // projection Input::unit_ground_px / resolve_ground_cell use).
    void cell_to_window(int cx, int cy, int& x, int& y) const {
        const ra2assets::MapCell* c = world_.map().at(cx, cy);
        const int level = c ? c->level : 0;
        x = (cx - cy) * 30 + 30 + cam_.x;
        y = (cx + cy) * 15 + 15 - level * 15 + cam_.y;
    }

    bool locate(const std::string& what, int& x, int& y) const override {
        const std::size_t colon = what.find(':');
        const std::string kind = what.substr(0, colon);
        const std::string arg = colon == std::string::npos ? "" : what.substr(colon + 1);
        if (kind == "cell") {
            const std::size_t comma = arg.find(',');
            if (comma == std::string::npos) return false;
            cell_to_window(std::atoi(arg.substr(0, comma).c_str()),
                           std::atoi(arg.substr(comma + 1).c_str()), x, y);
            return true;
        }
        if (kind == "unit") {
            int h = 0, n = 0;
            std::string img;
            if (!ra2engine::play::parse_unit_selector(arg, h, img, n)) return false;
            int k = 0;
            for (const ra2engine::Unit& u : world_.units()) {
                if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0 || u.house != h ||
                    u.image != img)
                    continue;
                if (k++ != n) continue;
                x = static_cast<int>(std::lround((u.fx - u.fy) * 30.0)) + 30 + cam_.x;
                y = static_cast<int>(std::lround((u.fx + u.fy) * 15.0 - u.level * 15)) + 15 +
                    cam_.y - 10;   // a little above the feet: inside the hit box
                return true;
            }
            return false;
        }
        if (kind == "radar") {   // radar:cx,cy -> a radar pixel that maps to that cell
            const std::size_t comma = arg.find(',');
            if (comma == std::string::npos) return false;
            const int tx = std::atoi(arg.substr(0, comma).c_str()), ty = std::atoi(arg.substr(comma + 1).c_str());
            int best = -1;
            for (int py = lay_.radar.y; py < lay_.radar.y + lay_.radar.h; ++py)
                for (int px = lay_.radar.x; px < lay_.radar.x + lay_.radar.w; ++px) {
                    int cx = 0, cy = 0;
                    if (!radar_cell_at(px, py, cx, cy)) continue;
                    const int d = std::abs(cx - tx) + std::abs(cy - ty);
                    if (best < 0 || d < best) { best = d; x = px; y = py; }
                }
            return best >= 0 && best <= 2;
        }
        if (kind == "building") {   // building:H:ID[:n] -> footprint centre
            int h = 0, n = 0;
            std::string id;
            if (!ra2engine::play::parse_unit_selector(arg, h, id, n)) return false;
            int k = 0;
            for (const ra2engine::Building& b : world_.buildings()) {
                if (!b.alive || b.state == ra2engine::Building::State::Dead || b.house != h) continue;
                if (b.type < 0 || b.type >= static_cast<int>(ba_.roster.size()) || ba_.roster[b.type].id != id) continue;
                if (k++ != n) continue;
                cell_to_window(b.cx + b.fw / 2, b.cy + b.fh / 2, x, y);
                return true;
            }
            return false;
        }
        if (kind == "cameo" || kind == "cameotab") {
            // The cameo's tab: 0/1 buildings (by Defense), 2/3 units.
            int want_tab = -1, want_type = -1;
            for (int t = 0; t < 4 && want_tab < 0; ++t)
                for (int idx : tab_buildable(t)) {
                    const std::string& id = (t >= 2) ? ur_.defs[idx].id : ba_.roster[idx].id;
                    if (id == arg) { want_tab = t; want_type = idx; break; }
                }
            if (want_tab < 0) return false;
            if (kind == "cameotab" || want_tab != active_tab_) {
                if (kind == "cameo") return false;
                x = lay_.tabs[want_tab].x + lay_.tabs[want_tab].w / 2;
                y = lay_.tabs[want_tab].y + lay_.tabs[want_tab].h / 2;
                return true;
            }
            for (const ra2engine::CameoCell& c : lay_.cameos)
                if (c.type == want_type) {
                    x = c.rect.x + c.rect.w / 2;
                    y = c.rect.y + c.rect.h / 2;
                    return true;
                }
            return false;
        }
        if (kind == "button") {
            std::string a = arg;
            for (char& ch : a) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            const ra2engine::SbRect* r = nullptr;
            if (a == "repair") r = &lay_.repair_btn;
            else if (a == "sell") r = &lay_.sell_btn;
            if (r) {
                x = r->x + r->w / 2;
                y = r->y + r->h / 2;
                return true;
            }
            if (a == "diplomacy") {   // W2-UI-29
                x = lay_.diplo_btn.x + 8;
                y = lay_.diplo_btn.y + 4;
                return true;
            }
            if (a == "options") {
                x = lay_.opt_btn.x + 8;
                y = 20 + 4;
                return true;
            }
            if (a == "radar") {
                x = lay_.radar.x + lay_.radar.w / 2;
                y = lay_.radar.y + lay_.radar.h / 2;
                return true;
            }
        }
        return false;
    }

    void probe(std::map<std::string, long long>& out) const override {
        out["scene.game"] = 1;
        out["credits"] = std::llround(player_credits_);
        out["tab"] = active_tab_;
        out["mode"] = static_cast<int>(input_.base_mode());
        out["placing"] = input_.placement_active() ? 1 : 0;
        out["sw_armed"] = input_.sw_targeting() ? 1 : 0;
        out["menu"] = menu_.open_ ? 1 : 0;
        out["dialog"] = sld_.is_open() ? 1 : 0;
        out["selb"] = input_.selected_building();
        out["cursor"] = static_cast<int>(cursor_action());
        out["radar"] = radar_active() ? 1 : 0;
        out["cam_x"] = cam_.x;
        out["cam_y"] = cam_.y;
        int sel = 0;
        for (const ra2engine::Unit& u : world_.units()) {
            if (!u.alive || u.die_time >= 0.0) continue;
            if (u.selected) ++sel;
            if (u.selected && u.moving()) ++out["sel.moving"];
            ++out["units." + std::to_string(u.house)];
            ++out["units." + std::to_string(u.house) + "." + u.image];
            if (u.moving()) ++out["moving." + std::to_string(u.house)];
        }
        out["sel"] = sel;
        for (const ra2engine::Unit& u : world_.units())   // first selected unit's cell
            if (u.selected && u.alive && u.die_time < 0.0) {
                out["sel.cx"] = u.cell_x;
                out["sel.cy"] = u.cell_y;
                break;
            }
        for (const ra2engine::Unit& u : world_.units())   // first own unit's cell
            if (u.house == 0 && u.alive && u.die_time < 0.0 && u.cargo_of < 0) {
                out["own.cx"] = u.cell_x;
                out["own.cy"] = u.cell_y;
                break;
            }
        for (const ra2engine::Building& b : world_.buildings()) {
            if (!b.alive || b.state == ra2engine::Building::State::Dead) continue;
            ++out["bld." + std::to_string(b.house)];
            if (b.type >= 0 && b.type < static_cast<int>(ba_.roster.size()))
                ++out["bld." + std::to_string(b.house) + "." + ba_.roster[b.type].id];
        }
        for (int h = 0; h < world_.house_count(); ++h)
            if (world_.house_defeated(h)) out["defeated." + std::to_string(h)] = 1;
        out["win_team"] = world_.winning_team();
        out["q.structure"] = queues_.state(ra2engine::QueueCat::Structure).slot.type;
        out["q.structure.ready"] = queues_.state(ra2engine::QueueCat::Structure).slot.ready ? 1 : 0;
        out["q.infantry"] = queues_.producing(ra2engine::QueueCat::Infantry);
        out["q.vehicle"] = queues_.producing(ra2engine::QueueCat::Vehicle);
        out["q.structure.stage"] = queues_.state(ra2engine::QueueCat::Structure).slot.stage;
        out["q.infantry.stage"] = queues_.state(ra2engine::QueueCat::Infantry).slot.stage;
        int held_mask = 0;
        for (int c : held_) held_mask |= 1 << c;
        out["held"] = held_mask;
        for (const auto& [c, bl] : backlog_) out["backlog." + std::to_string(c)] = static_cast<long long>(bl.size());
        for (const ra2engine::Building& b : world_.buildings())   // first own building
            if (b.alive && b.house == 0) { out["bld0.cx"] = b.cx; out["bld0.cy"] = b.cy; break; }
        if (campaign_) out["campaign.outcome"] = campaign_->outcome();
        // W3-FIX-A: live buildings drawn without art / hidden by InvisibleInGame.
        for (const ra2engine::Building& b : world_.buildings()) {
            if (!b.alive || b.state == ra2engine::Building::State::Dead || b.type < 0 ||
                b.type >= static_cast<int>(ba_.art.size()))
                continue;
            const BuildingArt& a = ba_.art[static_cast<std::size_t>(b.type)];
            if (a.invisible) ++out["art.invisible"];
            else if (!a.ok) ++out["art.missing"];
        }
        out["art.dressing_missing"] = map_dressing_.missing();
        for (const ra2engine::Unit& u : world_.units()) {   // W3-FIX-A: per image, any house
            if (!u.alive || u.die_time >= 0.0) continue;
            ++out["img." + u.image];
            if (u.is_aircraft && u.altitude > 0.0) ++out["img." + u.image + ".air"];
        }
    }

    bool cheat(const std::vector<std::string>& args) override {
        if (campaign_cheat(args)) return true;   // W2-CA-19 (campaign_play.inc)
#include "play_art_cheats.inc"   // W3-FIX-A art acceptance verbs (--play only)
#include "pres_play_cheats.inc"  // W5-PRES presentation acceptance verbs (--play only)
        if (args.size() == 1 && args[0] == "center_own") {   // camera on the first own unit
            for (const ra2engine::Unit& u : world_.units())
                if (u.house == 0 && u.alive && u.die_time < 0.0 && u.cargo_of < 0) {
                    input_.center_on_cell(u.fx, u.fy);
                    return true;
                }
            return false;
        }
        if (args.size() == 4 && args[0] == "building") {   // building ID x y: instant own building
            for (int t = 0; t < static_cast<int>(ba_.roster.size()); ++t)
                if (ba_.roster[t].id == args[1]) {
                    const ra2engine::BuildingTypeDef& d = ba_.roster[t];
                    world_.add_building(t, std::atoi(args[2].c_str()), std::atoi(args[3].c_str()), d.fw, d.fh, 0,
                                        d.strength, d.armor_ix, ba_.art[t].buildup_total, /*instant=*/true,
                                        d.sight, d.grants_map_reveal);
                    return true;
                }
            return false;
        }
        if (args.size() == 3 && args[0] == "pan") {   // move the camera by (dx, dy)
            input_.pan(std::atoi(args[1].c_str()), std::atoi(args[2].c_str()));
            return true;
        }
        if (args.size() == 2 && args[0] == "credits") {
            player_credits_ = std::atof(args[1].c_str());
            return true;
        }
        return false;
    }
