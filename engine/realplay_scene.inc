// realplay_scene.inc -- GameScene member functions owned by gap owner A
// (economy / production / base). #included INSIDE `class GameScene` (private
// section) so the scene body in main.cpp stays a thin caller and parallel
// fixers do not collide on it.
public:
    friend struct RealPlayProbe;   // --probe-realplay drives the real scene

    // Is a building of roster type 	ype placeable with its origin at
    // (cx, cy) for the human? The one rule set the placement cursor and the
    // confirm share [RE: construction.md §9]. Gap owner A: a WaterBound type
    // (Naval Yard) needs every foundation cell on WATER (roster2.md TL;DR 6),
    // and BaseNormal=no neighbours do not extend the base (G-28).
    ra2engine::PlacementCheck placement_eval(int type, int cx, int cy) const {
        if (type < 0 || type >= static_cast<int>(ba_.roster.size())) return {};
        const ra2engine::BuildingTypeDef& d = ba_.roster[type];
        const bool water = type < static_cast<int>(ba_.water_bound.size()) && ba_.water_bound[type];
        const std::vector<std::uint8_t>& grid =
            water ? world_.grid_for(ra2engine::MovementDomain::Water) : world_.passable();
        if (water && grid.empty()) return {};
        return ra2engine::placement_check(cx, cy, d.fw, d.fh, 0, d.adjacent, grid, world_.grid_w(),
                                          world_.grid_h(), world_.occupancy(), world_.buildings(),
                                          /*bypass_adjacency=*/false,
                                          rp_on_ ? &rp_base_normal_ : nullptr);
    }
    bool placement_ok(int type, int cx, int cy) const { return placement_eval(type, cx, cy).ok; }

    // Place a finished-in-the-queue building at (cx, cy): validity, then
    // add_building in Buildup (instant=false). Returns false (nothing placed)
    // when the cell is illegal. A wall closes the line to the next own wall
    // within GuardRange for free (G-30, roster2.md TL;DR 4).
    bool place_building(int type, int cx, int cy) {
        if (!placement_ok(type, cx, cy)) return false;
        auto add = [&](int x, int y) {
            const ra2engine::BuildingTypeDef& d = ba_.roster[type];
            world_.add_building(type, x, y, d.fw, d.fh, 0, d.strength, d.armor_ix,
                                ba_.art[type].buildup_total, /*instant=*/false, d.sight,
                                d.grants_map_reveal);
            if (type < static_cast<int>(ba_.water_bound.size()) && ba_.water_bound[type])
                world_.block_water_footprint(x, y, d.fw, d.fh);   // ships path around the yard
        };
        add(cx, cy);
        if (rp_on_ && type < static_cast<int>(ba_.wall.size()) && ba_.wall[type]) {
            auto own_wall = [&](int x, int y) {
                const int bi = world_.building_at_cell(x, y);
                return bi >= 0 && world_.buildings()[bi].type == type &&
                       world_.buildings()[bi].house == 0;
            };
            auto can_place = [&](int x, int y) {
                const ra2engine::PlacementCheck c = ra2engine::placement_check(
                    x, y, 1, 1, 0, 0, world_.passable(), world_.grid_w(), world_.grid_h(),
                    world_.occupancy(), world_.buildings(), /*bypass_adjacency=*/true);
                return c.ok;
            };
            for (const ra2engine::Cellxy& c :
                 ra2engine::wall_line_fill(cx, cy, ba_.wall_fill_range[type], own_wall, can_place))
                add(c.x, c.y);
        }
        return true;
    }

    // After a Structure/Defense start: a wall builds WallBuildSpeedCoefficient
    // (rulesmd 3.0) times slower [RE: roster2.md TL;DR 5] (G-30).
    void rp_after_building_start(ra2engine::ProductionState& ps, int type) {
        if (!rp_on_ || type < 0 || type >= static_cast<int>(ba_.wall.size()) || !ba_.wall[type])
            return;
        const int wf = ra2engine::wall_build_frames(
            ba_.roster[type].cost, build_speed_,
            rules_.general_double("WallBuildSpeedCoefficient", ra2engine::kWallBuildSpeedCoefficient));
        ps.slot.total_frames = std::max(1, wf);
        ps.slot.step_ticks = std::clamp(ps.slot.total_frames / ra2engine::kProductionSteps, 1, 255);
    }
private:
    // --- gap owner A state (skirmish real play; empty/false in a campaign) ---
    bool                 rp_on_ = false;     // the real-play roster/sidebar path is active
    std::string          rp_country_;        // the human's country (Owner/Required/Forbidden)
    std::vector<char>    rp_bld_allowed_;    // per roster building: country + lobby tech ok
    std::vector<int>     nav_ft_;            // Naval=yes UnitType factories (GAYARD/NAYARD)
    std::vector<int>     air_ft_;            // AircraftType factories (GAAIRC/AMRADR)
    std::vector<int>     bld_ft_;            // BuildingType factories (the ConYards)
    std::vector<RpSwGrant> rp_sw_table_;     // per roster building: SW granted (G-25)
    SuperweaponParams    rp_nuke_;
    std::vector<char>    rp_base_normal_;
    std::vector<int>     rp_build_limit_;    // per unit def: rules BuildLimit= (0 = none)    // per roster building: rules BaseNormal (default yes)

    // The roster the ctor init list loads for the human: the full side/country/
    // tech-level roster in skirmish (G-22), the historical tables in a campaign.
    static UnitRoster rp_load_roster(const std::string& dir, const ra2assets::MixFile& ra2,
                                     const UnitAssets& ua, const Rules& rules, const Art& art,
                                     const Ini& rules_ini, const ra2engine::SkirmishConfig& cfg) {
        // W2-CA-03: a campaign player builds from the same rules-driven roster
        // as skirmish -- its house's country (ParentCountry) and map
        // TechLevel, the map overrides already in `rules`, RA2 types only.
        if (!cfg.campaign_map.empty()) {
            const camp::PlayerRules pr =
                camp::campaign_player_rules(dir, ra2, cfg.campaign_map, cfg.campaign_side);
            return load_player_unit_roster(dir, ra2, ua, rules, art, rules_ini, pr.country,
                                           pr.tech_level, /*with_aircraft=*/false, &pr.allowed);
        }
        const ra2engine::SkirmishSlot s0 =
            cfg.slots.empty() ? ra2engine::SkirmishSlot{} : cfg.slots.front();
        return load_player_unit_roster(dir, ra2, ua, rules, art, rules_ini,
                                       rp_player_country(rules, s0), cfg.tech_level,
                                       /*with_aircraft=*/false);
    }

    // Ctor tail (skirmish only): factories by rules Factory=, the building
    // country/tech mask, and the first sidebar list.
    void rp_setup() {
        rp_on_ = true;
        const ra2engine::SkirmishSlot s0 =
            cfg_.slots.empty() ? ra2engine::SkirmishSlot{} : cfg_.slots.front();
        int tech = cfg_.tech_level;
        bool sw = cfg_.superweapons;
        camp::PlayerRules pr;   // W2-CA-03: campaign = the mission's player house
        if (!cfg_.campaign_map.empty()) {
            pr = camp::campaign_player_rules(dir_, ra2_, cfg_.campaign_map, cfg_.campaign_side);
            rp_country_ = pr.country;
            tech = pr.tech_level;
            sw = true;
        } else {
            rp_country_ = rp_player_country(rules_, s0);
        }
        // [RE: roster2.md TL;DR 7; production.md §1.1] every factory of the
        // category counts (a captured enemy factory still produces).
        inf_ft_ = rp_factory_types(ba_, rules_, "InfantryType");
        veh_ft_ = rp_factory_types(ba_, rules_, "UnitType", /*naval=*/false);
        nav_ft_ = rp_factory_types(ba_, rules_, "UnitType", /*naval=*/true);
        air_ft_ = rp_factory_types(ba_, rules_, "AircraftType");
        bld_ft_ = rp_factory_types(ba_, rules_, "BuildingType");
        std::vector<int> unit_ft = inf_ft_;
        for (const std::vector<int>* v : {&veh_ft_, &nav_ft_, &air_ft_})
            unit_ft.insert(unit_ft.end(), v->begin(), v->end());
        std::vector<int> any_veh = veh_ft_;
        any_veh.insert(any_veh.end(), nav_ft_.begin(), nav_ft_.end());
        any_veh.insert(any_veh.end(), air_ft_.begin(), air_ft_.end());
        input_.set_unit_factory_types(unit_ft);
        input_.set_infantry_factory_types(inf_ft_);
        input_.set_vehicle_factory_types(any_veh);
        if (sw) {
            rp_sw_table_ = rp_superweapon_table(ba_, rules_);
            rp_nuke_ = resolve_superweapon(rules_);
        }
        rp_base_normal_.assign(ba_.roster.size(), 1);
        for (std::size_t i = 0; i < ba_.roster.size(); ++i) {
            const std::string v = rules_.section_string(ba_.roster[i].id, "BaseNormal", "");
            if (!v.empty()) rp_base_normal_[i] = rp_flag(rules_, ba_.roster[i].id, "BaseNormal");
        }
        rp_build_limit_.assign(ur_.defs.size(), 0);
        for (std::size_t i = 0; i < ur_.defs.size(); ++i)
            rp_build_limit_[i] = static_cast<int>(rules_.section_double(ur_.defs[i].id, "BuildLimit", 0.0));
        rp_bld_allowed_.assign(ba_.roster.size(), 0);
        for (std::size_t i = 0; i < ba_.roster.size(); ++i) {
            const ra2engine::BuildingTypeDef& d = ba_.roster[i];
            rp_bld_allowed_[i] = d.tech_level > 0 && d.tech_level <= tech &&
                                 rp_country_allows(rules_, d.id, rp_country_) &&
                                 camp::allowed_type(pr, d.id);
            // [SK-27] Lobby Superweapons off: "Super Weapons can be built" is
            // false, so no SuperWeapon= structure is offered. [RE-FALLBACK: the
            // [AI] BuildTech tech centres stay buildable -- their YR
            // SuperWeapon= is the side special, not a lobby superweapon.]
            if (!sw && rp_bld_allowed_[i] && rp_is_lobby_sw_building(rules_, d.id))
                rp_bld_allowed_[i] = 0;
        }
        buildable_ = tab_buildable(active_tab_);
        lay_ = ra2engine::SidebarLayout::compute(kWinW, kWinH, buildable_);
    }

    ra2engine::QueueCat rp_cat_of_unit(int uslot) const {
        if (uslot < 0 || uslot >= static_cast<int>(ur_.defs.size()))
            return ra2engine::QueueCat::Infantry;
        return rp_unit_cat(ur_.defs[uslot]);
    }
    const std::vector<int>& rp_ft_of(ra2engine::QueueCat c) const {
        switch (c) {
            case ra2engine::QueueCat::Vehicle:  return veh_ft_;
            case ra2engine::QueueCat::Naval:    return nav_ft_;
            case ra2engine::QueueCat::Aircraft: return air_ft_;
            default:                            return inf_ft_;
        }
    }

    // [RE-FALLBACK: the RA2 sidebar strip lists a cameo once the item becomes
    // buildable (the "New construction options" moment); the StripClass add
    // path itself was not hunted]. Side membership falls out of the
    // prerequisites (GA* need GACNST, NA* need NACNST -- rulesmd Owner= lists
    // every country for buildings), country rules from Owner/Required/
    // ForbiddenHouses, and the lobby TechLevel. An item already in a queue
    // stays listed so it can be cancelled/placed.
    std::vector<int> rp_tab_buildable(int tab) const {
        std::vector<int> v;
        const std::vector<ra2engine::Building>& bs = world_.buildings();
        if (tab < 2) {
            const ra2engine::QueueCat bc =
                tab == 1 ? ra2engine::QueueCat::Defense : ra2engine::QueueCat::Structure;
            for (int i = 0; i < static_cast<int>(ba_.roster.size()); ++i) {
                const ra2engine::BuildingTypeDef& d = ba_.roster[i];
                if (!ba_.art[i].ok || d.cameo.empty()) continue;
                if (i >= static_cast<int>(rp_bld_allowed_.size()) || !rp_bld_allowed_[i]) continue;
                const bool def = i < static_cast<int>(ba_.defense.size()) && ba_.defense[i] != 0;
                if (def != (tab == 1)) continue;
                const bool queued = queues_.producing(bc) == i;
                if (!queued &&
                    !ra2engine::prerequisites_met(d.prerequisite, bs, ba_.roster, 0, ba_.groups))
                    continue;
                v.push_back(i);
            }
            std::stable_sort(v.begin(), v.end(), [&](int a, int b) {
                return ba_.roster[a].tech_level < ba_.roster[b].tech_level;
            });
            return v;
        }
        for (int i = 0; i < static_cast<int>(ur_.defs.size()); ++i) {
            const ra2engine::UnitTypeDef& d = ur_.defs[i];
            const ra2engine::QueueCat c = rp_unit_cat(d);
            if ((c == ra2engine::QueueCat::Infantry) != (tab == 2)) continue;
            const bool queued = queues_.producing(c) == i;
            if (!queued &&
                !ra2engine::prerequisites_met(d.prerequisite, bs, ba_.roster, 0, ba_.groups))
                continue;
            // BuildLimit= (Tanya, Boris: 1): no new one while the limit is on
            // the field [RE-FALLBACK: counted by the unit's art image].
            if (!queued && i < static_cast<int>(rp_build_limit_.size()) && rp_build_limit_[i] > 0) {
                int alive = 0;
                for (const ra2engine::Unit& u : world_.units())
                    alive += u.alive && u.house == 0 && u.image == d.combat.image;
                if (alive >= rp_build_limit_[i]) continue;
            }
            v.push_back(i);
        }
        return v;
    }

    // Once per frame: the strip follows the buildable set (a finished Barracks
    // makes its units appear, a lost Tech Center removes its items).
    void rp_after_world_step() {
        if (rp_on_ && !rp_sw_table_.empty()) rp_sync_superweapons(world_, rp_sw_table_, rp_nuke_);
    }

    void rp_refresh_sidebar() {
        if (!rp_on_) return;
        std::vector<int> now = rp_tab_buildable(active_tab_);
        if (now != buildable_) {
            buildable_ = std::move(now);
            lay_ = ra2engine::SidebarLayout::compute(kWinW, kWinH, buildable_, lay_.top_row);   // keep the G-12 strip scroll
        }
    }

    // The unit-tab queue whose progress the strip draws: tab 2 = Infantry; tab
    // 3 = the first busy of Vehicle / Naval / Aircraft (one strip, three
    // concurrent factory queues -- only one clock is drawn at a time).
    ra2engine::QueueCat rp_vis_unit_cat() const {
        if (active_tab_ == 2) return ra2engine::QueueCat::Infantry;
        for (ra2engine::QueueCat c : {ra2engine::QueueCat::Vehicle, ra2engine::QueueCat::Naval,
                                      ra2engine::QueueCat::Aircraft})
            if (queues_.producing(c) >= 0) return c;
        return ra2engine::QueueCat::Vehicle;
    }

    // Exit handoff: each READY unit queue launches from its category's primary
    // factory [RE: production.md §1.3]; a HARV/CMIN becomes a working miner.
    void rp_launch_ready_units() {
        for (ra2engine::QueueCat cat :
             {ra2engine::QueueCat::Infantry, ra2engine::QueueCat::Vehicle,
              ra2engine::QueueCat::Naval, ra2engine::QueueCat::Aircraft}) {
            if (!queues_.ready(cat)) continue;
            const std::vector<int>& ft = rp_ft_of(cat);
            const int fi = ra2engine::primary_factory(world_.buildings(), ft, 0);
            if (fi < 0) continue;
            const int uslot = queues_.producing(cat);
            if (uslot < 0 || uslot >= static_cast<int>(ur_.defs.size())) {
                queues_.placed(cat);
                continue;
            }
            const ra2engine::UnitTypeDef& ud = ur_.defs[uslot];
            const ra2engine::BuildingTypeDef& fdef = ba_.roster[world_.buildings()[fi].type];
            const int uidx = world_.launch_unit(fi, fdef, ud.combat);
            if (uidx >= 0) {
                if (ud.harvester)   // [RE: economy.md §1] a built miner starts hunting ore
                    world_.make_harvester(uidx, ud.chrono_miner, ud.storage > 0 ? ud.storage : 20);
                world_.set_anim(ud.combat.image, ua_.anims.count(ud.combat.image)
                                                     ? ua_.anims.at(ud.combat.image)
                                                     : ra2engine::default_anim());
                queues_.placed(cat);
            }
        }
    }
