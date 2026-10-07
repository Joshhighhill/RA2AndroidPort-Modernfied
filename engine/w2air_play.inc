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
        for (const char* f : {"submerged", "sub_detected_by0", "iron_units", "chrono_units", "airborne",
                              "spawned_out", "planes", "planes_docked", "plane_ammo", "amphib_on_water",
                              "unit_hp", "cargo_total", "iron_buildings", "sw_has", "sw_ready_t",
                              "sw_charge"})
            p.zero_families.insert(f);
        p.set("sw_armed", input_.sw_targeting() ? input_.sw_armed_type() : 0);
        p.set("sw_armed_phase", input_.sw_armed_phase());
        // Superweapons v2: per house and type (TYPE = Nuke IronCurtain
        // LightningStorm ChronoSphere ParaDrop AmerParaDrop).
        for (int h = 0; h < world_.house_count(); ++h) {
            const std::string hs = std::to_string(h);
            int nsw = 0;
            for (ra2engine::SuperweaponType t : world_.sw_types(h)) {
                ++nsw;
                const std::string k = hs + "," + ra2engine::superweapon_type_name(t);
                p.set("sw_has(" + k + ")", 1);
                p.set("sw_ready_t(" + k + ")", world_.sw_ready(h, t) ? 1 : 0);
                p.set("sw_charge(" + k + ")", world_.sw_charge_fraction(h, t));
            }
            p.set("sw_count(" + hs + ")", nsw);
        }
        p.set("sw_strikes", static_cast<double>(world_.sw_strikes().size()));
        int launched = 0, detonated = 0, activated = 0, bolts = 0, dropped = 0;
        for (const ra2engine::SwEvent& e : world_.sw_events()) {
            launched += e.kind == ra2engine::SwEvent::Launched;
            detonated += e.kind == ra2engine::SwEvent::Detonated;
            activated += e.kind == ra2engine::SwEvent::Activated;
            bolts += e.kind == ra2engine::SwEvent::Bolt;
            dropped += e.kind == ra2engine::SwEvent::Dropped;
        }
        p.set("sw_ev_launched", launched);
        p.set("sw_ev_detonated", detonated);
        p.set("sw_ev_activated", activated);
        p.set("sw_ev_bolts", bolts);
        p.set("sw_ev_dropped", dropped);
        p.set("sw_ev_seq", world_.sw_events().empty() ? 0 : world_.sw_events().back().seq);
        // Naval / aircraft per house.
        for (const ra2engine::Unit& u : world_.units()) {
            if (!u.alive || u.die_time >= 0.0) continue;
            const std::string hs = std::to_string(u.house);
            if (u.an.submerged) p.add("submerged(" + hs + ")", 1);
            if (u.an.submerged && (u.an.detected_mask & 1u)) p.add("sub_detected_by0(" + hs + ")", 1);
            if (u.an.iron_left > 0.0) p.add("iron_units(" + hs + ")", 1);
            if (u.an.chrono_left > 0.0) p.add("chrono_units(" + hs + ")", 1);
            if (u.is_aircraft && u.altitude > 0.0) p.add("airborne(" + hs + ")", 1);
            if (u.an.t.spawned && u.cargo_of < 0) p.add("spawned_out(" + hs + ")", 1);
            if (u.an.t.fixed_wing && !u.an.t.spawned) {
                p.add("planes(" + hs + ")", 1);
                if (u.an.sortie == 0 && u.altitude <= 0.0) p.add("planes_docked(" + hs + ")", 1);
                p.add("plane_ammo(" + hs + ")", u.an.ammo);
            }
            if (u.an.in_water && !u.is_naval) p.add("amphib_on_water(" + hs + ")", 1);
            p.add("unit_hp(" + hs + "," + play_unit_id(u) + ")", u.hp);
            {
                const std::string kx = "pos_x(" + hs + "," + play_unit_id(u) + ")";
                if (!p.n.count(kx)) {
                    p.set(kx, u.cell_x);
                    p.set("pos_y(" + hs + "," + play_unit_id(u) + ")", u.cell_y);
                }
            }
            if (u.cargo_of < 0) p.add("cargo_total(" + hs + ")", static_cast<double>(u.cargo.size()));
        }
        for (const ra2engine::Building& b : world_.buildings())
            if (b.alive && b.iron_left > 0.0) p.add("iron_buildings(" + std::to_string(b.house) + ")", 1);
        p.set("rad_total", world_.rad_field().total());
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

    // ---------------------------------------------------------------------
    // Scene set-up (rp_setup): the World's SW / naval / aircraft parameters,
    // the per-building SW / UnitRepair / Helipad flags, the GAAIRC dock pads,
    // and the spawned / paradrop unit types (+ their render assets).
    // ---------------------------------------------------------------------
    std::deque<VehicleAssets> w2_va_;   // spawned / paradrop hulls (stable addresses)
    int w2_chrono_x_ = -1, w2_chrono_y_ = -1;   // Chronosphere source pick
    int w2_armed_ = 0;                          // SuperweaponType armed via the sidebar

    void w2_setup(bool skirmish) {
        world_.set_sw_params(resolve_sw_params(rules_));
        const std::size_t n = ba_.roster.size();
        std::vector<int> swb(n, 0), rep(n, 0), air(n, 0);
        std::vector<std::pair<int, int>> pads;
        for (std::size_t i = 0; i < n; ++i) {
            const std::string& id = ba_.roster[i].id;
            if (skirmish && i < rp_sw_table_.size()) swb[i] = static_cast<int>(rp_sw_table_[i].type);
            rep[i] = rp_flag(rules_, id, "UnitRepair") ? 1 : 0;
            air[i] = rp_flag(rules_, id, "Helipad") ? 1 : 0;
            if (air[i] && pads.empty() && i < ba_.dock_offsets.size())
                for (const ra2engine::LeptonXY& o : ba_.dock_offsets[i]) pads.push_back({o.x, o.y});
        }
        world_.set_sw_building_types(swb);
        world_.set_an_building_flags(rep, air);
        world_.set_dock_offsets(pads);
        // Spawned aircraft / missiles (Spawns=, V3/DMisl types) + the paradrop
        // plane and its infantry.
        const ra2engine::SwParams sp = world_.sw_params();
        std::vector<std::string> ids = {"PDPLANE", sp.amer_para_inf, sp.ally_para_inf, sp.sov_para_inf};
        for (const UnitRoster* r : {&ur_, &ur_ai_})
            for (const ra2engine::UnitTypeDef& d : r->defs)
                if (!d.combat.an.spawns.empty()) ids.push_back(d.combat.an.spawns);
        std::vector<std::string> vehicle_ids, infantry_ids;
        for (const std::string& id : ids) {
            if (id.empty() || world_.has_an_type(id)) continue;
            ResolvedUnit ru = resolve_combat_unit(rules_, id);
            if (ru.combat.image.empty()) continue;
            world_.register_an_type(id, ru.combat);
            (ru.combat.is_vehicle ? vehicle_ids : infantry_ids).push_back(id);
        }
        for (const std::string& id : vehicle_ids) {
            if (gf_has_vehicle_art(rules_.image(id))) continue;
            VehicleAssets va = load_vehicle_assets(dir_, ra2_, ua_.pal, id);
            if (!va.ok) {
                std::cerr << "[w2] no voxel art for spawned " << id << "\n";
                continue;
            }
            w2_va_.push_back(std::move(va));
            vlist_.push_back(&w2_va_.back());
        }
        if (!infantry_ids.empty()) gf_load_assets(infantry_ids);
        std::cerr << "[w2] setup pads=" << pads.size() << " an-types=" << vehicle_ids.size()
                  << "+" << infantry_ids.size() << "\n";
    }

    // Sidebar SW slot clicked (READY): arm that SW's targeting.
    void w2_arm_sw(ra2engine::SuperweaponType t) {
        w2_armed_ = static_cast<int>(t);
        w2_chrono_x_ = w2_chrono_y_ = -1;
        input_.arm_superweapon(w2_armed_, 0);
    }
    // A committed SW target cell. The Chronosphere takes two picks [rules
    // [ChronoSphereSpecial] PreClick=yes, [ChronoWarpSpecial] PostClick=yes]:
    // the source area, then the destination.
    void w2_sw_target(int cx, int cy) {
        using T = ra2engine::SuperweaponType;
        T t = static_cast<T>(w2_armed_);
        if (t == T::None) {
            const ra2engine::Superweapon* s = world_.sw_state(0);
            if (!s) return;
            t = s->type;
        }
        if (t == T::ChronoSphere && w2_chrono_x_ < 0) {
            w2_chrono_x_ = cx;
            w2_chrono_y_ = cy;
            input_.arm_superweapon(w2_armed_, 1);   // now pick the destination
            return;
        }
        if (t == T::ChronoSphere)
            world_.fire_superweapon_at(0, t, w2_chrono_x_, w2_chrono_y_, cx, cy);
        else
            world_.fire_superweapon_at(0, t, cx, cy);
        w2_armed_ = 0;
        w2_chrono_x_ = w2_chrono_y_ = -1;
    }

    // Cheats: `cheat w2 ...` (dev hooks; acceptance scripts prefer real input).
    //   w2 damage H:TYPE[:n] DMG  damage a unit (yard-repair set-up)
    bool w2_cheat(const std::vector<std::string>& a, std::string& out) {
        out.clear();
        if (a.size() < 2 || a[0] != "w2") return false;
        if (a[1] == "damage" && a.size() >= 4) {
            std::vector<std::string> parts;
            std::string cur;
            for (char ch : a[2]) {
                if (ch == ':') { parts.push_back(cur); cur.clear(); }
                else cur += ch;
            }
            parts.push_back(cur);
            if (parts.size() < 2) { out = "damage H:TYPE[:n] DMG"; return true; }
            const int h = std::atoi(parts[0].c_str());
            const int nth = parts.size() > 2 ? std::atoi(parts[2].c_str()) : 0;
            int seen = 0;
            for (ra2engine::Unit& u : world_.units()) {
                if (!u.alive || u.die_time >= 0.0 || u.house != h) continue;
                bool match = false;
                for (const UnitRoster* r : {&ur_, &ur_ai_})
                    for (const ra2engine::UnitTypeDef& d : r->defs)
                        if (d.id == parts[1] && d.combat.image == u.image) match = true;
                if (!match || seen++ != nth) continue;
                u.hp = std::max(1, u.hp - std::atoi(a[3].c_str()));
                return true;
            }
            out = "no unit " + a[2];
            return true;
        }
        out = "unknown w2 cheat";
        return true;
    }
