// G0 --play (agent G): the extended state probe + script hooks of GameScene,
// #included INSIDE the class body (public section) next to agent B's
// game_scene_play.inc (name/locate/probe/cheat). Keys documented in
// scripts/play/README.md. Interactive-only: nothing here runs in a gate.

    // --- G0 --play state probe (playtest.hpp) --------------------------------
    // Append-only and read-only (the cheats below are the one exception, and
    // only a script can call them). Keys are documented in
    // scripts/play/README.md; a field that a later phase renames or removes
    // makes scripts report missing-probe:<key> instead of crashing.
    void play_probe(ra2play::Probe& p) override {
        p.scene = "game";
        p.zero_families = {"units", "buildings", "built", "selected_type", "harv", "harvesters",
                           "harv_bales", "can_build", "cameo", "cameo_tab", "moving"};
        p.set_s("mode", campaign_ ? "campaign" : "skirmish");
        p.set("credits", player_credits_);
        p.set("credits_shown", credits_shown_);
        p.set("playfield_w", playfield_w_);
        p.set("cam_x", cam_.x);
        p.set("cam_y", cam_.y);
        const int houses = world_.house_count();
        p.set("houses", houses);
        for (int h = 0; h < houses; ++h) {
            const std::string hs = std::to_string(h);
            p.set("house_credits(" + hs + ")", world_.house_credits(h));
            p.set("defeated(" + hs + ")", world_.house_defeated(h) ? 1 : 0);
            p.set("team(" + hs + ")", world_.house_team(h));
            // [SK] what the Skirmish page handed the game, per house.
            const ra2engine::World::Side hsd = world_.house_side(h);
            p.set_s("side(" + hs + ")", hsd == ra2engine::World::Side::Soviet ? "SOVIET"
                                        : hsd == ra2engine::World::Side::Allied ? "ALLIED"
                                        : hsd == ra2engine::World::Side::Yuri ? "YURI" : "NEUTRAL");
            p.set("house_color(" + hs + ")", world_.house_color(h));
            if (h < static_cast<int>(cfg_.slots.size())) {
                p.set_s("country(" + hs + ")", cfg_.slots[(size_t)h].country);
                p.set("combatant(" + hs + ")", cfg_.slots[(size_t)h].combatant ? 1 : 0);
            }
            if (const ra2engine::AiHouseConfig* ac = world_.ai_config(h))
                p.set_s("ai(" + hs + ")", ac->difficulty == ra2engine::AiDifficulty::Easy ? "Easy"
                                          : ac->difficulty == ra2engine::AiDifficulty::Normal ? "Medium"
                                          : "Brutal");
            else
                p.set_s("ai(" + hs + ")", "Human");
            p.set("sw(" + hs + ")", world_.sw_state(h) ? 1 : 0);
            p.set("sw_ready(" + hs + ")", world_.sw_ready(h) ? 1 : 0);
            p.set("units(" + hs + ")", 0);
            p.set("buildings(" + hs + ")", 0);
        }
        // Units (alive, not dying), keyed by rules id where the roster knows it.
        int selected = 0;
        for (const ra2engine::Unit& u : world_.units()) {
            if (!u.alive || u.die_time >= 0.0) continue;
            const std::string hs = std::to_string(u.house);
            const std::string id = play_unit_id(u);
            p.add("units(" + hs + ")", 1);
            p.add("units(" + hs + "," + id + ")", 1);
            if (u.moving()) p.add("moving(" + hs + ")", 1);
            if (!p.n.count("uhp(" + hs + "," + id + ")")) {   // W2-SK: first live unit's HP / rank
                p.set("uhp(" + hs + "," + id + ")", u.hp);
                p.set("urank(" + hs + "," + id + ")", u.veteran_rank);
                p.set("udisguised(" + hs + "," + id + ")", u.disguised ? 1 : 0);
            }
            if (!p.n.count("weapon_damage(" + id + ")")) {   // first live unit of the type
                p.set("weapon_damage(" + id + ")", u.w_damage);
                p.set("weapon_range(" + id + ")", u.w_range);
                p.set("weapon_rof(" + id + ")", u.w_rof);
            }
            if (u.selected) { ++selected; p.add("selected_type(" + id + ")", 1); }
            if (u.is_harvester) {
                static const char* kHs[] = {"None", "SeekOre", "MoveToOre", "Harvesting",
                                            "ReturnToRef", "WaitDock", "Unloading"};
                const int st = static_cast<int>(u.harv);
                p.add("harvesters(" + hs + ")", 1);
                p.add("harv(" + hs + "," + (st >= 0 && st < 7 ? kHs[st] : "?") + ")", 1);
                p.add("harv_bales(" + hs + ")", u.bales_ore + u.bales_gems);
            }
        }
        p.set("selected", selected);
        {   // the lowest-index selected unit's cell (for "did it move" checks)
            const ra2engine::Unit* s0 = nullptr;
            for (const ra2engine::Unit& u : world_.units())
                if (u.selected && u.alive && u.die_time < 0.0) { s0 = &u; break; }
            p.set("sel_cx", s0 ? std::floor(s0->fx + 0.5) : -1);
            p.set("sel_cy", s0 ? std::floor(s0->fy + 0.5) : -1);
            p.set("sel_moving", s0 && s0->moving() ? 1 : 0);
            p.set("sel_occupier", s0 && s0->occupier ? 1 : 0);          // G16
            p.set("sel_occupy_target", s0 ? s0->occupy_target : -1);
            p.set("sel_attack_cell", s0 ? s0->attack_cell_x : -1);
            p.set("sel_target", s0 ? s0->target : -1);
            p.set("sel_rof_cool", s0 ? s0->rof_cool : -1);
            p.set("sel_turret", s0 ? s0->turret_facing : -1);
            p.set("sel_body", s0 ? s0->body_facing : -1);
            p.set("sel_damage", s0 ? s0->w_damage : -1);
            p.set("sel_wall", s0 ? (s0->w_active == 1 ? s0->w2.wall : s0->p_wall) : -1);
        }
        for (int h = 0; h < houses; ++h) {   // losses since the game began
            int lost = 0, blost = 0;
            long hp_lost = 0;
            for (const ra2engine::Unit& u : world_.units())
                if (u.house == h && (!u.alive || u.die_time >= 0.0)) ++lost;
            for (const ra2engine::Building& b : world_.buildings()) {
                if (b.house != h) continue;
                if (!b.alive || b.state == ra2engine::Building::State::Dead) ++blost;
                else hp_lost += b.max_hp - b.hp;
            }
            p.set("units_lost(" + std::to_string(h) + ")", lost);
            p.set("buildings_lost(" + std::to_string(h) + ")", blost);
            p.set("hp_lost(" + std::to_string(h) + ")", static_cast<double>(hp_lost));
        }
        for (const ra2engine::Building& b : world_.buildings()) {
            if (!b.alive || b.state == ra2engine::Building::State::Dead) continue;
            const std::string hs = std::to_string(b.house);
            const std::string id = (b.type >= 0 && b.type < static_cast<int>(ba_.roster.size()))
                                       ? ba_.roster[b.type].id : std::to_string(b.type);
            p.add("buildings(" + hs + ")", 1);
            p.add("buildings(" + hs + "," + id + ")", 1);
            if (b.state == ra2engine::Building::State::Idle) p.add("built(" + hs + "," + id + ")", 1);
        }
        p.set("selected_building", input_.selected_building());
        {   // own objects inside the playfield view (camera opens on the player?)
            int on = 0;
            auto vis = [&](double fx, double fy) {
                const int sx = static_cast<int>(std::lround((fx - fy) * 30.0)) + 30 + cam_.x;
                const int sy = static_cast<int>(std::lround((fx + fy) * 15.0)) + 15 + cam_.y;
                return sx >= 0 && sx < playfield_w_ && sy >= 0 && sy < kWinH;
            };
            for (const ra2engine::Unit& u : world_.units())
                if (u.house == 0 && u.alive && u.die_time < 0.0 && vis(u.fx, u.fy)) ++on;
            for (const ra2engine::Building& b : world_.buildings())
                if (b.house == 0 && b.alive && vis(b.cx + b.fw / 2.0, b.cy + b.fh / 2.0)) ++on;
            p.set("own_on_screen", on);
        }
        // Session verdict as the player sees it. A skirmish has no end path on
        // this tree (GameScene never reads the World's victory state, G-02), so
        // `outcome` stays RUNNING there; `world_outcome` is the World's view.
        std::string outcome = "RUNNING";
        if (campaign_ && campaign_->outcome() != 0) outcome = campaign_->outcome() == 1 ? "WON" : "LOST";
        else if (!campaign_ && gf_outcome_ != 0) outcome = gf_outcome_ > 0 ? "WON" : "LOST";   // gF G-02
        p.set_s("outcome", outcome);
        std::string wo = "RUNNING";
        if (world_.victory_enabled()) {
            if (world_.house_defeated(0)) wo = "LOST";
            else if (world_.winning_team() >= 0 && world_.winning_team() == world_.house_team(0)) wo = "WON";
        }
        p.set_s("world_outcome", wo);
        p.set("victory_enabled", world_.victory_enabled() ? 1 : 0);
        p.set("winning_team", world_.winning_team());
        // [SK] lobby options as the game consumes them.
        p.set("game_speed", cfg_.game_speed >= 0 ? cfg_.game_speed
                                                 : g_user_settings ? g_user_settings->game_speed : -1);
        p.set("mcv_redeploys", world_.mcv_redeploys() ? 1 : 0);
        p.set("sw_allowed", cfg_.superweapons ? 1 : 0);
        p.set("short_game", cfg_.short_game ? 1 : 0);
        p.set("crates_on", cfg_.crates ? 1 : 0);
        p.set("unit_count_cfg", cfg_.unit_count);
        p.set("start_cash", cfg_.starting_cash);
        // Power (house 0).
        const ra2engine::PowerTotals pt = ra2engine::power_totals(world_.buildings(), ba_.roster, 0);
        p.set("power_out", pt.output);
        p.set("power_drain", pt.drain);
        // Sidebar: active tab, its cameos (click targets), every tab's roster
        // and whether a click would start each entry now.
        p.set("tab", active_tab_);
        p.set("sidebar", sidebar_up_ ? 1 : 0);
        auto rect = [](const ra2engine::SbRect& r) { return ra2play::Rect{r.x, r.y, r.w, r.h}; };
        for (int t = 0; t < 4; ++t) p.targets["tab:" + std::to_string(t)] = rect(lay_.tabs[t]);
        if (ba_.has_repair_sell) {
            p.targets["repair"] = rect(lay_.repair_btn);
            p.targets["sell"] = rect(lay_.sell_btn);
        }
        if (world_.sw_state(0))
            p.targets["sw"] = ra2play::Rect{lay_.bar.x + ra2engine::kCameoX, lay_.radar.y + 2,
                                            ra2engine::kCameoW, ra2engine::kCameoH};
        if (sidebar_up_ && ba_.has_optbtns) {
            const int w = ba_.sb_optbtn.width() > 0 ? ba_.sb_optbtn.width() : 70;
            const int h = ba_.sb_optbtn.height() > 0 ? ba_.sb_optbtn.height() : 16;
            p.targets["options"] = ra2play::Rect{lay_.bar.x + 11, 20, w, h};
        }
        for (std::size_t i = 0; i < lay_.cameos.size() && i < buildable_.size(); ++i)
            p.targets["cameo:" + play_entry_id(active_tab_, buildable_[i])] = rect(lay_.cameos[i].rect);
        std::string ids;
        for (int t = 0; t < 4; ++t) {
            for (int e : tab_buildable(t)) {
                const std::string id = play_entry_id(t, e);
                p.set("cameo(" + id + ")", 1);
                p.set("cameo_tab(" + id + ")", t);
                p.set("can_build(" + id + ")", play_can_start(t, e) ? 1 : 0);
                if (t == active_tab_) ids += (ids.empty() ? "" : ",") + id;
            }
        }
        p.set_s("tab_ids", ids);
        // Queues.
        static const std::pair<ra2engine::QueueCat, const char*> kCats[] = {
            {ra2engine::QueueCat::Structure, "structure"}, {ra2engine::QueueCat::Defense, "defense"},
            {ra2engine::QueueCat::Infantry, "infantry"}, {ra2engine::QueueCat::Vehicle, "vehicle"},
            {ra2engine::QueueCat::Naval, "naval"}, {ra2engine::QueueCat::Aircraft, "aircraft"}};
        for (const auto& [cat, nm] : kCats) {
            const ra2engine::QueueSlot& sl = queues_.state(cat).slot;
            const int t = (cat == ra2engine::QueueCat::Structure) ? 0 : (cat == ra2engine::QueueCat::Defense) ? 1
                        : (cat == ra2engine::QueueCat::Infantry) ? 2 : 3;   // 2/3 = unit roster
            p.set_s(std::string("queue(") + nm + ")", sl.type >= 0 ? play_entry_id(t, sl.type) : "");
            p.set(std::string("queue_stage(") + nm + ")", sl.stage);
            p.set(std::string("queue_ready(") + nm + ")", sl.ready ? 1 : 0);
        }
        // Armed modes / overlays.
        p.set("placing", input_.placement_active() ? 1 : 0);
        p.set_s("placing_type", input_.placement_active() && input_.placement_type() >= 0 &&
                                        input_.placement_type() < static_cast<int>(ba_.roster.size())
                                    ? ba_.roster[input_.placement_type()].id : "");
        const ra2engine::Input::BaseMode bm = input_.base_mode();
        p.set_s("base_mode", bm == ra2engine::Input::BaseMode::Repair ? "repair"
                             : bm == ra2engine::Input::BaseMode::Sell ? "sell" : "none");
        p.set("sw_targeting", input_.sw_targeting() ? 1 : 0);
        p.set("dialog_open", 0);
        menu_.play_probe(p);
        gf_abort_.play_probe(p);   // gF G-14: Abort Mission confirm
        sld_.play_probe(p);
        p.set_s("banner", (!banner_.empty() && SDL_GetTicks() < banner_until_) ? banner_ : "");
        if (campaign_) {
            p.set_s("map", cfg_.campaign_map);
            p.set("campaign_frame", static_cast<double>(campaign_->frame_count()));
            p.set("objectives", static_cast<double>(campaign_->objectives().size()));
            p.set("triggers_fired", campaign_->triggers().fired_count());
            p.set("campaign_outcome", campaign_->outcome());
            p.set("briefing_open", show_briefing_ ? 1 : 0);
        } else {
            p.set_s("map", cfg_.map_file);
        }
        // ---- hooks ----
        p.cell_to_screen = [this](double cx, double cy, int& sx, int& sy) {
            const ra2assets::MapCell* mc =
                world_.map().at(static_cast<int>(std::lround(cx)), static_cast<int>(std::lround(cy)));
            const int level = mc ? mc->level : 0;
            sx = static_cast<int>(std::lround((cx - cy) * 30.0)) + 30 + cam_.x;
            sy = static_cast<int>(std::lround((cx + cy) * 15.0 - level * 15)) + 15 + cam_.y;
            return sx >= 40 && sx < playfield_w_ - 40 && sy >= 40 && sy < kWinH - 40;
        };
        p.center_on = [this](double cx, double cy) {
            const ra2assets::MapCell* mc =
                world_.map().at(static_cast<int>(std::lround(cx)), static_cast<int>(std::lround(cy)));
            const int level = mc ? mc->level : 0;
            const int tx = static_cast<int>(std::lround(playfield_w_ / 2.0 - (cx - cy) * 30.0 - 30));
            const int ty = static_cast<int>(std::lround(kWinH / 2.0 - (cx + cy) * 15.0 + level * 15 - 15));
            input_.pan(tx - cam_.x, ty - cam_.y);
        };
        p.find_unit = [this](const std::string& spec, double& cx, double& cy) {
            int h = 0, n = 0;
            std::string type;
            if (!play_parse_spec(spec, h, type, n)) return false;
            for (const ra2engine::Unit& u : world_.units()) {
                if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0 || u.house != h) continue;
                if (type != "*" && play_unit_id(u) != type && u.image != type) continue;
                if (n-- > 0) continue;
                cx = u.fx;
                cy = u.fy;
                return true;
            }
            return false;
        };
        p.find_building = [this](const std::string& spec, double& cx, double& cy) {
            const int bi = play_find_building(spec);
            if (bi < 0) return false;
            const ra2engine::Building& b = world_.buildings()[bi];
            cx = b.cx + b.fw / 2;
            cy = b.cy + b.fh / 2;
            return true;
        };
        // ---- G16 world depth (agent G): garrisons + bridges ----
        p.zero_families.insert("occupants");
        p.zero_families.insert("garrisoned");
        p.zero_families.insert("in_garrison");
        p.zero_families.insert("on_bridge");
        p.zero_families.insert("owned_garrisons");
        int garrisonable = 0;
        for (int bi = 0; bi < static_cast<int>(world_.buildings().size()); ++bi) {
            const ra2engine::Building& b = world_.buildings()[bi];
            if (!b.alive || b.state == ra2engine::Building::State::Dead) continue;
            if (b.type < 0 || b.type >= static_cast<int>(ba_.roster.size()) || !ba_.roster[b.type].can_be_occupied)
                continue;
            ++garrisonable;
            const std::string hs = std::to_string(b.house);
            p.add("owned_garrisons(" + hs + ")", 1);
            if (!b.occupants.empty()) {
                p.add("garrisoned(" + hs + ")", 1);
                p.add("occupants(" + hs + ")", static_cast<double>(b.occupants.size()));
            }
        }
        p.set("garrisonable", garrisonable);
        p.set("garrison_shots", static_cast<double>(world_.garrison_shots()));
        for (const ra2engine::Unit& u : world_.units()) {
            if (!u.alive || u.die_time >= 0.0) continue;
            if (u.garrison_of >= 0) p.add("in_garrison(" + std::to_string(u.house) + ")", 1);
            const int bo = world_.bridge_overlay_at(u.cell_x, u.cell_y);
            if (u.cargo_of < 0 && ra2engine::is_low_bridge_overlay(bo) && !ra2engine::low_bridge_destroyed(bo))
                p.add("on_bridge(" + std::to_string(u.house) + ")", 1);
        }
        {
            int cells = 0;
            for (int y = 0; y < world_.grid_h(); ++y)
                for (int x = 0; x < world_.grid_w(); ++x)
                    if (world_.bridge_overlay_at(x, y) >= 0) ++cells;
            p.set("bridge_cells", cells);
            p.set("bridge_destroyed", world_.bridge_cells_destroyed());
            if (world_.bridges_enabled())
                for (const char* nm : {"bridge", "bridge_near", "bridge_far", "hut"}) {
                    double px = -1, py = -1;
                    if (play_find_point(nm, px, py)) {
                        p.set(std::string("pt_") + nm + "_x", px);
                        p.set(std::string("pt_") + nm + "_y", py);
                        // Chebyshev distance from the first selected unit to the point.
                        for (const ra2engine::Unit& u : world_.units())
                            if (u.selected && u.alive && u.die_time < 0.0) {
                                p.set(std::string("sel_dist_") + nm,
                                      std::max(std::abs(u.cell_x - px), std::abs(u.cell_y - py)));
                                break;
                            }
                    }
                }
            p.set("bridge_version", world_.bridge_version());
            p.set("depth_armed", world_.depth_rules().armed ? 1 : 0);
        }
        p.find_point = [this](const std::string& name, double& cx, double& cy) {
            return play_find_point(name, cx, cy);
        };
        p.auto_place = [this](int& cx, int& cy) {
            if (!input_.placement_active()) return false;
            const int type = input_.placement_type();
            if (type < 0 || type >= static_cast<int>(ba_.roster.size())) return false;
            const ra2engine::BuildingTypeDef& d = ba_.roster[type];
            int ax = -1, ay = -1;   // anchor: the player's first live building
            for (const ra2engine::Building& b : world_.buildings())
                if (b.alive && b.house == 0 && b.state != ra2engine::Building::State::Dead) {
                    ax = b.cx + b.fw / 2;
                    ay = b.cy + b.fh / 2;
                    break;
                }
            if (ax < 0) return false;
            for (int r = 1; r <= 24; ++r)
                for (int oy = ay - r; oy <= ay + r; ++oy)
                    for (int ox = ax - r; ox <= ax + r; ++ox) {
                        if (std::max(std::abs(ox - ax), std::abs(oy - ay)) != r) continue;
                        if (!placement_ok(type, ox, oy)) continue;   // the confirm's own test
                        cx = ox + d.fw / 2;
                        cy = oy + d.fh / 2;
                        return true;
                    }
            return false;
        };
        p.cheat = [this](const std::vector<std::string>& a) -> std::string {
            const std::string c = a.empty() ? "" : a[0];
            if (c == "kill_house" && a.size() >= 2) {
                const int h = std::atoi(a[1].c_str());
                for (int i = 0; i < static_cast<int>(world_.units().size()); ++i) {
                    const ra2engine::Unit& u = world_.units()[i];
                    if (u.alive && u.die_time < 0.0 && u.house == h) world_.campaign_kill_unit(i);
                }
                for (int i = 0; i < static_cast<int>(world_.buildings().size()); ++i) {
                    const ra2engine::Building& b = world_.buildings()[i];
                    if (b.alive && b.house == h && b.state != ra2engine::Building::State::Dead)
                        world_.campaign_damage_building(i, b.hp + 1);
                }
                return "";
            }
            if (c == "damage_building" && a.size() >= 3) {
                const int bi = play_find_building(a[1]);
                if (bi < 0) return "no building " + a[1];
                world_.campaign_damage_building(bi, std::atoi(a[2].c_str()));
                return "";
            }
            if (c == "bridge_hit" && a.size() >= 4) {   // G16 debug: one explosion roll
                world_.bridge_explosion(std::atoi(a[1].c_str()), std::atoi(a[2].c_str()),
                                        std::atoi(a[3].c_str()), true);
                return "";
            }
            if (c == "credits" && a.size() >= 2) {
                player_credits_ += std::atof(a[1].c_str());
                return "";
            }
            // W2-SK probes: damage_unit H:TYPE[:n] DMG ; place TYPE H building H:T +DX +DY
            if (c == "damage_unit" && a.size() >= 3) {
                int h = 0, n = 0;
                std::string type;
                if (!play_parse_spec(a[1], h, type, n)) return "bad spec " + a[1];
                for (int i = 0; i < static_cast<int>(world_.units().size()); ++i) {
                    ra2engine::Unit& u = world_.units()[i];
                    if (!u.alive || u.die_time >= 0.0 || u.house != h) continue;
                    if (type != "*" && play_unit_id(u) != type) continue;
                    if (n-- > 0) continue;
                    u.hp = std::max(1, u.hp - std::atoi(a[2].c_str()));
                    return "";
                }
                return "no unit " + a[1];
            }
            if (c == "place" && a.size() >= 7 && a[3] == "building") {
                const int t = roster_index(ba_, a[1]);
                if (t < 0) return "no building type " + a[1];
                const int bi = play_find_building(a[4]);
                if (bi < 0) return "no building " + a[4];
                const ra2engine::Building& ab = world_.buildings()[bi];
                const ra2engine::BuildingTypeDef& d = ba_.roster[static_cast<std::size_t>(t)];
                const int x = ab.cx + std::atoi(a[5].c_str()), y = ab.cy + std::atoi(a[6].c_str());
                const int idx = world_.add_building(t, x, y, d.fw, d.fh, std::atoi(a[2].c_str()), d.strength,
                                                    d.armor_ix, 0.0, /*instant=*/true, d.sight,
                                                    d.grants_map_reveal);
                return idx >= 0 ? "" : "place failed";
            }
            // spawn TYPE H (cell X Y | building H:T[:n] +DX +DY)
            if (c == "spawn" && a.size() >= 5) {
                const ra2engine::UnitTypeDef* def = nullptr;
                for (const UnitRoster* r : {&ur_, &ur_ai_})
                    for (const ra2engine::UnitTypeDef& d : r->defs)
                        if (!def && d.id == a[1]) def = &d;
                // Not in either roster: resolve straight from rules (as the crews are).
                ra2engine::UnitTypeDef tmp;
                if (!def) {
                    tmp.id = a[1];
                    tmp.combat = resolve_combat_unit(rules_, a[1]).combat;
                    if (tmp.combat.image.empty()) return "unknown unit type " + a[1];
                    def = &tmp;
                }
                const int h = std::atoi(a[2].c_str());
                int x = 0, y = 0;
                if (a[3] == "cell") {
                    x = std::atoi(a[4].c_str());
                    y = a.size() > 5 ? std::atoi(a[5].c_str()) : 0;
                } else if (a[3] == "building" && a.size() >= 7) {
                    const int bi = play_find_building(a[4]);
                    if (bi < 0) return "no building " + a[4];
                    const ra2engine::Building& b = world_.buildings()[bi];
                    x = b.cx + std::atoi(a[5].c_str());
                    y = b.cy + std::atoi(a[6].c_str());
                } else if (a[3] == "point" && a.size() >= 7) {   // G16: named map point
                    double px = 0, py = 0;
                    if (!play_find_point(a[4], px, py)) return "no point " + a[4];
                    x = static_cast<int>(px) + std::atoi(a[5].c_str());
                    y = static_cast<int>(py) + std::atoi(a[6].c_str());
                } else {
                    return "spawn TYPE H cell X Y | spawn TYPE H building H:T[:n] +DX +DY | spawn TYPE H point NAME +DX +DY";
                }
                world_.add_combat_unit(x, y, h, def->combat);
                world_.set_anim(def->combat.image, ua_.anims.count(def->combat.image)
                                                       ? ua_.anims.at(def->combat.image)
                                                       : ra2engine::default_anim());
                return "";
            }
            return "unknown cheat (have: kill_house H, damage_building H:TYPE[:n] DMG, credits N, "
                   "spawn TYPE H cell X Y | spawn TYPE H building H:T[:n] +DX +DY)";
        };
    }
