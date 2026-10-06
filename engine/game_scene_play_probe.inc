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
        p.set_s("outcome", outcome);
        std::string wo = "RUNNING";
        if (world_.victory_enabled()) {
            if (world_.house_defeated(0)) wo = "LOST";
            else if (world_.winning_team() >= 0 && world_.winning_team() == world_.house_team(0)) wo = "WON";
        }
        p.set_s("world_outcome", wo);
        p.set("victory_enabled", world_.victory_enabled() ? 1 : 0);
        p.set("winning_team", world_.winning_team());
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
            if (c == "credits" && a.size() >= 2) {
                player_credits_ += std::atof(a[1].c_str());
                return "";
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
                } else {
                    return "spawn TYPE H cell X Y | spawn TYPE H building H:T[:n] +DX +DY";
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
