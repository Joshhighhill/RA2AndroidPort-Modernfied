// P10 Campaign runtime [RE: docs/re/campaign.md, docs/re/map-triggers.md].
//
// Textually included by main.cpp (inside its anonymous namespace, before
// GameScene) because it needs main.cpp's asset types (BuildingAssets,
// VehicleAssets, UnitAssets, the loaders). Everything here is reached ONLY from
// the campaign GameScene and the --demo-campaign gate: no existing mode or
// byte-identity gate constructs a CampaignRuntime.

namespace camp {

std::string up(std::string s) {
    for (char& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}
std::string trim_comment(std::string v) {
    const std::size_t sc = v.find(';');
    if (sc != std::string::npos) v.erase(sc);
    while (!v.empty() && std::isspace((unsigned char)v.back())) v.pop_back();
    std::size_t i = 0;
    while (i < v.size() && std::isspace((unsigned char)v[i])) ++i;
    return v.substr(i);
}
std::vector<std::string> list_values(const Ini& ini, const char* sec) {
    std::vector<std::string> out;
    for (const auto& kv : ini.section(sec)) {
        const std::string v = trim_comment(kv.second);
        if (!v.empty()) out.push_back(v);
    }
    return out;
}
int index_of(const std::vector<std::string>& v, const std::string& id) {
    const std::string u = up(id);
    for (std::size_t i = 0; i < v.size(); ++i) if (up(v[i]) == u) return (int)i;
    return -1;
}

// Read a file from ra2.mix/local.mix (the RA2 control INIs, RE §1).
Ini load_ra2_local_ini(const MixFile& ra2, const std::string& name) {
    try {
        MixFile local = ra2.open_nested("local.mix");
        if (const MixEntry* e = local.find(name)) {
            const auto raw = local.read(*e);
            return Ini::parse(std::string(raw.begin(), raw.end()));
        }
    } catch (const std::exception&) {}
    return Ini::parse("");
}
// RA2 rules.ini: the expand01.mix patch copy wins over ra2.mix/local.mix (RE §1/§6).
Ini load_ra2_rules_ini(const std::string& dir, const MixFile& ra2) {
    try {
        MixFile ex = MixFile::open_path(dir + "expand01.mix");
        if (const MixEntry* e = ex.find(std::string("rules.ini"))) {
            const auto raw = ex.read(*e);
            return Ini::parse(std::string(raw.begin(), raw.end()));
        }
    } catch (const std::exception&) {}
    return load_ra2_local_ini(ra2, "rules.ini");
}
// The mix holding a RA2 campaign map (RE §1): Soviet maps in MAPS02.MIX, the
// Allied/training/ending maps in Maps01.mix.
std::string mix_for_map(const std::string& map) {
    const std::string u = up(map);
    return u.rfind("SOV", 0) == 0 ? "MAPS02.MIX" : "Maps01.mix";
}
std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}
Ini load_map_ini(const std::string& dir, const std::string& map) {
    MixFile maps = MixFile::open_path(dir + mix_for_map(map));
    const MixEntry* e = maps.find(lower(map));
    if (!e) throw std::runtime_error("campaign map not found: " + map);
    const auto raw = maps.read(*e);
    return Ini::parse(std::string(raw.begin(), raw.end()));
}

// CSF strings (RE §3.2 labels): the loose patch ra2.csf first, then
// language.mix/ra2.csf. Loaded once.
const ra2assets::CsfStrings* csf(const std::string& dir) {
    static std::unique_ptr<ra2assets::CsfStrings> s;
    static bool tried = false;
    if (tried) return s.get();
    tried = true;
    try {
        const std::vector<std::uint8_t> raw = ra2assets::read_file(dir + "ra2.csf");
        if (!raw.empty()) s = std::make_unique<ra2assets::CsfStrings>(
                              ra2assets::CsfStrings::load(raw.data(), raw.size()));
    } catch (const std::exception&) {}
    if (!s) {
        try {
            MixFile lang = MixFile::open_path(dir + "language.mix");
            if (const MixEntry* e = lang.find(std::string("ra2.csf"))) {
                const auto raw = lang.read(*e);
                s = std::make_unique<ra2assets::CsfStrings>(
                    ra2assets::CsfStrings::load(raw.data(), raw.size()));
            }
        } catch (const std::exception&) {}
    }
    return s.get();
}
std::string csf_text(const std::string& dir, const std::string& label) {
    const ra2assets::CsfStrings* c = csf(dir);
    const std::u16string* t = c ? c->find(label) : nullptr;
    if (!t) return label;
    std::string out;
    for (char16_t ch : *t) out += (ch >= 32 && ch < 127) ? (char)ch : (ch == '\n' ? '\n' : ' ');
    return out;
}

// battle.ini / mission.ini / mapsel.ini from ra2.mix/local.mix (RE §1).
ra2engine::CampaignTables load_tables(const MixFile& ra2) {
    return ra2engine::CampaignTables::parse(load_ra2_local_ini(ra2, "battle.ini"),
                                            load_ra2_local_ini(ra2, "mission.ini"),
                                            load_ra2_local_ini(ra2, "mapsel.ini"));
}

// The campaign selection a GameScene / gate starts from.
struct Launch {
    std::string map;          // e.g. "ALL01t.MAP"
    std::string side;         // mapsel side section: "GDI" / "Nod"
    int difficulty = 1;       // 0 Easy, 1 Normal, 2 Hard (Scenario +0x60C)
};

// ---------------------------------------------------------------------------
// Generic type resolution for map objects + TaskForce members [RE §4].
// Buildings are appended to ba.roster (campaign-only, after every existing
// index, so nothing that indexes the roster moves); units get a UnitCombat and,
// for vehicles, a VehicleAssets (stable pointer for the render list).
struct UnitType {
    ra2engine::UnitCombat combat;
    bool vehicle = false;
    bool ok = false;
};

class Types {
public:
    Types(const std::string& dir, const MixFile& ra2, const MixFile& language, UnitAssets& ua,
          const Rules& rules, const Art& art, const Ini& rules_ini)
        : dir_(dir), ra2_(ra2), language_(language), ua_(ua), rules_(rules), art_(art),
          rules_ini_(rules_ini) {
        infantry_ = list_values(rules_ini, "InfantryTypes");
        vehicles_ = list_values(rules_ini, "VehicleTypes");
        aircraft_ = list_values(rules_ini, "AircraftTypes");
    }

    const UnitType* unit(const std::string& id_in) {
        const std::string id = up(id_in);
        auto it = units_.find(id);
        if (it != units_.end()) return it->second.ok ? &it->second : nullptr;
        UnitType t;
        try {
            if (index_of(infantry_, id) >= 0) {
                const ResolvedUnit ru = resolve_combat_unit(rules_, id);
                t.combat = ru.combat;
                t.combat.is_vehicle = false;
                t.combat.speed_cps = speed_to_cps(ru.speed);
                const std::string& img = t.combat.image;
                if (!ua_.shps.count(img)) {
                    MixFile conquer = ra2_.open_nested("conquer.mix");
                    ua_.shps.emplace(img, load_unit_shp(conquer, img));
                    ua_.anims.emplace(img, anim_from_art(art_, img));
                }
                t.ok = true;
            } else if (index_of(vehicles_, id) >= 0 && !art_.voxel(rules_.image(id))) {
                // W3-FIX-A: Voxel=no SHP vehicles (Terror Drone, Dolphin, Giant
                // Squid) were sent to the voxel loader, which threw -- they never
                // existed in a mission, as map units or TaskForce members. Same
                // registration as skirmish: a land SHP vehicle rides the
                // infantry path (W2-GA-11), an SHP ship is a vehicle (W2-AN-15).
                const ResolvedUnit ru = resolve_combat_unit(rules_, id);
                t.combat = ru.combat;
                t.combat.speed_cps = speed_to_cps(ru.speed);
                const bool ship = t.combat.is_naval;
                t.combat.is_vehicle = ship;
                t.vehicle = ship;
                const std::string img = t.combat.image;
                if (!ua_.shps.count(img)) {
                    MixFile conquer = ra2_.open_nested("conquer.mix");
                    ua_.shps.emplace(img, load_unit_shp(conquer, img));
                }
                if (!ua_.anims.count(img)) {
                    if (ship) {
                        auto sv = g_shp_vehicles.find(img);
                        const int walk = sv != g_shp_vehicles.end() ? std::max(1, sv->second.walk) : 1;
                        const int fire = sv != g_shp_vehicles.end() ? std::max(1, sv->second.fire) : 1;
                        ra2engine::UnitAnimSeq a;   // the w2_setup SHP-ship layout
                        a.stand = {0, 1, walk};
                        a.walk = {0, walk, walk};
                        a.fire = {walk * 8, fire, fire};
                        a.die = {0, 1, 0};
                        ua_.anims.emplace(img, a);
                    } else {
                        ua_.anims.emplace(img, anim_from_art(art_, img));
                    }
                }
                t.ok = true;
                std::cerr << "[campaign] SHP vehicle " << id << " image=" << img << (ship ? " ship" : " land") << "\n";
            } else if (index_of(vehicles_, id) >= 0 || index_of(aircraft_, id) >= 0) {
                // W2-CA-16: aircraft resolve through the same voxel loader
                // (resolve_an_type sets the fixed-wing / jumpjet flight model).
                auto va = std::make_unique<VehicleAssets>(load_vehicle_assets(dir_, ra2_, ua_.pal, id));
                if (va->ok) {
                    t.combat = va->combat;
                    t.vehicle = true;
                    t.ok = true;
                    vas_.push_back(std::move(va));
                }
            }
        } catch (const std::exception& ex) {
            std::cerr << "[campaign] unit type " << id << " unavailable (" << ex.what() << ")\n";
            t.ok = false;
        }
        auto res = units_.emplace(id, std::move(t));
        return res.first->second.ok ? &res.first->second : nullptr;
    }
    bool is_aircraft(const std::string& id) const { return index_of(aircraft_, id) >= 0; }

    // Roster index of building `id`, appending it (def + art) when absent.
    int building(BuildingAssets& ba, const std::string& id_in) {
        const std::string id = up(id_in);
        for (std::size_t i = 0; i < ba.roster.size(); ++i)
            if (up(ba.roster[i].id) == id) return (int)i;
        if (!rules_ini_.has(id)) return -1;
        ra2engine::BuildingTypeDef d;
        d.id = id;
        const std::string art_id = rules_.image(id);
        d.fw = d.fh = 1;
        art_.foundation(art_id, d.fw, d.fh);
        d.strength = rules_.strength(id);
        if (d.strength <= 0) d.strength = 1;
        d.sight = rules_.sight(id);
        d.armor_ix = ra2engine::armor_index(rules_.armor(id));
        d.cost = rules_.cost(id);
        d.charge = d.cost;
        d.power = rules_.power(id);
        d.adjacent = rules_.adjacent(id);
        d.tech_level = rules_.tech_level(id);
        d.conyard = rules_.is_construction_yard(id);
        d.prerequisite = rules_.prerequisite(id);
        d.image = art_id;
        d.cameo = art_.cameo(art_id);
        d.explosion = rules_.explosion_first(id);
        d.unsellable = rules_.section_flag(id, "Unsellable", false);
        d.click_repairable = rules_.section_flag(id, "ClickRepairable", true);
        // W2-SK-12/19/20: the same Capturable/Spyable/UnitRepair flags as the roster.
        d.capturable = rules_.section_flag(id, "Capturable", false);
        d.spyable = rules_.section_flag(id, "Spyable", false);
        d.unit_repair = rules_.section_flag(id, "UnitRepair", false);
        d.crewed = rules_.section_flag(id, "Crewed", false);
        // W3-FIX-A: the MAP's theater (was hard-coded TEMPERATE -- snow missions
        // drew summer art) over the global MIX search list (isogen.mix holds
        // the landmarks, <ext>.mix the ammo crates) [RE: asset-search.md §1].
        BuildingArt slot;
        slot.ok = asset_building_shp(art_id, slot.shp);
        if (slot.ok) building_art_extras(slot, art_, rules_, id, art_id);
        else std::cerr << "[campaign] building " << id << " image " << art_id << " has no art\n";
        ra2engine::fill_building_weapons(rules_, id, d);   // Gap fix C (G-40)
        ba.roster.push_back(d);
        ba.art.push_back(std::move(slot));
        ba.defense.push_back(rules_.is_buildcat_combat(id) ? 1 : 0);
        // Keep the P6 per-type vectors index-aligned (inert flags).
        ba.wall.push_back(0);
        ba.naval.push_back(0);
        // W3-FIX-A: WaterBound=yes (Naval Yards) places on the water grid.
        ba.water_bound.push_back(rules_.section_flag(id, "WaterBound", false) ? 1 : 0);
        ba.factory_kind.push_back(rules_.section_string(id, "Factory", ""));
        ba.wall_fill_range.push_back(0);
        ba.dock_offsets.emplace_back();
        return (int)ba.roster.size() - 1;
    }

    std::vector<const VehicleAssets*> vehicle_assets() const {
        std::vector<const VehicleAssets*> v;
        for (const auto& p : vas_) v.push_back(p.get());
        return v;
    }
    const std::vector<std::string>& infantry_list() const { return infantry_; }
    const std::vector<std::string>& vehicle_list() const { return vehicles_; }

private:
    std::string dir_;
    const MixFile& ra2_;
    const MixFile& language_;
    UnitAssets& ua_;
    const Rules& rules_;
    const Art& art_;
    const Ini& rules_ini_;
    std::vector<std::string> infantry_, vehicles_, aircraft_;
    std::map<std::string, UnitType> units_;
    std::vector<std::unique_ptr<VehicleAssets>> vas_;
};

}  // namespace camp

// ---------------------------------------------------------------------------
// The mission in progress on a real World.
class CampaignRuntime {
public:
    struct Objective { long frame; std::string text; };

    CampaignRuntime(const camp::Launch& launch, const std::string& dir, const MixFile& ra2,
                    const MixFile& language, ra2engine::World& world, Assets& a, UnitAssets& ua,
                    BuildingAssets& ba, const Rules& rules, const Art& art, const Ini& rules_ini)
        : launch_(launch), dir_(dir), w_(world), a_(a), ba_(ba), rules_ini_(rules_ini),
          map_(camp::load_map_ini(dir, launch.map)),
          ra2_rules_(camp::load_ra2_rules_ini(dir, ra2)),
          basic_(ra2engine::parse_scenario_basic(map_)),
          types_(dir, ra2, language, ua, rules, art, rules_ini),
          data_(ra2engine::parse_map_triggers(map_, camp::list_values(ra2_rules_, "Countries"),
                                              camp::list_values(ra2_rules_, "BuildingTypes"))),
          teams_data_(ra2engine::parse_map_teams(map_)),
          end_(ra2engine::borrowed_time_frames(rules.general_double("SavourDelay", 0.1))),
          host_(*this) {
        ra2_vehicles_ = camp::list_values(ra2_rules_, "VehicleTypes");
        ra2_infantry_ = camp::list_values(ra2_rules_, "InfantryTypes");
        ra2_aircraft_ = camp::list_values(ra2_rules_, "AircraftTypes");
        reveal_radius_ = rules_ini.get_int("General", "RevealTriggerRadius", 9);
        setup_houses();
        // Every type the mission can place, resolved up front (so the render
        // list is complete before the first frame).
        const std::vector<ra2engine::MapObjectRec> objs = ra2engine::parse_map_objects(map_);
        for (const ra2engine::MapObjectRec& o : objs)
            if (o.kind == 0) types_.building(ba_, o.type);
            else types_.unit(o.type);   // W2-CA-16: aircraft too
        for (const auto& tf : teams_data_.taskforces)
            for (const ra2engine::MapTaskMember& m : tf.second) types_.unit(m.type);
        resolve_base_nodes();   // [G-69] before the roster is handed to the World
        w_.refresh_building_types(ba_.roster);   // G16: btypes() must see mission types
        w_.set_repair_sell_rules(ba_.repair_sell);
        place_objects(objs);

        sys_ = std::make_unique<ra2engine::MapTriggerSystem>(data_, launch.difficulty, 0x5EEDu);
        sys_->set_campaign_mode(true);
        // Objects were placed (and attached) before scenario start [RE
        // map-triggers.md §1.8]: attach their tags now that the system exists.
        for (const Track& t : utrack_) if (t.tag >= 0) sys_->attach_tag(t.tag);
        for (const Track& t : btrack_) if (t.tag >= 0) sys_->attach_tag(t.tag);
        sys_->start(host_);
        drain_log();
        char b[200];
        std::snprintf(b, sizeof b,
                      "[camp] %s houses=%d player=%s objects=%d placed_b=%d placed_u=%d skipped=%d "
                      "triggers=%d tags=%d teams=%d",
                      launch.map.c_str(), (int)house_names_.size(), house_names_[0].c_str(),
                      (int)objs.size(), placed_b_, placed_u_, skipped_, (int)data_.triggers.size(),
                      (int)data_.tags.size(), (int)teams_data_.teams.size());
        witness_.emplace_back(b);
    }

    // --- per frame (15 Hz) --------------------------------------------------
    // One logic frame: the trigger loops, object event polling, team scripts,
    // hunters, mission-end bookkeeping. The caller steps the World afterwards.
    void frame() {
        if (finished()) return;
        sys_->tick(host_);
        drain_log();
        arm_production_houses();   // [G-69]
        poll_objects();
        step_teams();
        step_hunters();
        if (sys_->outcome() != 0) flag_end(sys_->outcome());
        ++frame_;
    }
    // Interactive: run as many 15 Hz logic frames as the wall-clock dt covers.
    void update(double dt) {
        accum_ += dt;
        int n = 0;
        while (accum_ >= 1.0 / 15.0 && n < 4) {
            accum_ -= 1.0 / 15.0;
            frame();
            ++n;
        }
        if (n == 4) accum_ = 0.0;
    }

    bool finished() const { return end_.ended(frame_); }
    int outcome() const { return end_.outcome(); }
    long frame_count() const { return frame_; }
    const camp::Launch& launch() const { return launch_; }
    const ra2engine::ScenarioBasic& basic() const { return basic_; }
    // W4-MC-04: the map's [Map] Size / LocalSize (the usable area).
    ra2engine::MapLocalSize local_size() const { return ra2engine::parse_map_local_size(map_); }
    const std::vector<Objective>& objectives() const { return objectives_; }
    const std::vector<std::string>& witness() const { return witness_; }
    std::vector<const VehicleAssets*> vehicle_assets() const { return types_.vehicle_assets(); }
    double player_credits() const { return player_credits_; }
    ra2engine::World::Side player_side() const { return player_side_; }
    const ra2engine::MapTriggerSystem& triggers() const { return *sys_; }
    int teams_created() const { return teams_created_; }
    int house_index(const std::string& name) const { return engine_house_of_name(name); }
    int raises() const { return raises_; }

    // --- P7 save/load (HOST section; the triggers go in TRIG) -----------------
    ra2engine::MapTriggerSystem& trigger_system() { return *sys_; }
    ra2engine::CampaignHostSave capture() const {
        ra2engine::CampaignHostSave h;
        for (const Track& t : utrack_) {
            h.u_tag.push_back(t.tag); h.u_cx.push_back(t.cx); h.u_cy.push_back(t.cy);
            h.u_hp.push_back(t.hp); h.u_team.push_back(t.team); h.u_type.push_back(t.type);
            h.u_flags.push_back(flags_of(t));
            h.u_mc_victim.push_back(t.mc_victim); h.u_mc_orig.push_back(t.mc_orig);
        }
        for (const Track& t : btrack_) {
            h.b_tag.push_back(t.tag); h.b_hp.push_back(t.hp); h.b_type.push_back(t.type);
            h.b_flags.push_back(flags_of(t));
        }
        for (const Team& t : teams_) {
            h.t_type.push_back(t.type); h.t_script.push_back(t.script); h.t_house.push_back(t.house);
            h.t_pc.push_back(t.st.pc); h.t_goal_x.push_back(t.goal_x); h.t_goal_y.push_back(t.goal_y);
            h.t_members.push_back(t.members);
            h.t_wait_until.push_back(t.st.wait_until);
            h.t_issued_frame.push_back(t.issued_frame);
            h.t_created.push_back(t.created);
            h.t_flags.push_back((std::uint8_t)((t.st.issued ? 1 : 0) | (t.st.done ? 2 : 0) |
                                               (t.recruiting ? 4 : 0)));
        }
        h.destroyed_units = destroyed_units_;
        h.destroyed_buildings = destroyed_buildings_;
        for (const auto& lb : last_built_) h.last_built.insert(h.last_built.end(), lb.begin(), lb.end());
        for (const Objective& o : objectives_) { h.obj_frames.push_back(o.frame); h.obj_texts.push_back(o.text); }
        h.end_outcome = end_.outcome();
        h.end_flagged_at = end_.flagged_at();
        h.frame = frame_;
        h.accum = accum_;
        h.logged = logged_;
        h.teams_created = teams_created_;
        h.raises = raises_;
        return h;
    }
    bool restore(const ra2engine::CampaignHostSave& h) {
        if (h.u_tag.size() != w_.units().size() || h.b_tag.size() != w_.buildings().size())
            return false;
        utrack_.clear();
        for (std::size_t i = 0; i < h.u_tag.size(); ++i) {
            Track t;
            t.tag = h.u_tag[i]; t.cx = h.u_cx[i]; t.cy = h.u_cy[i]; t.hp = h.u_hp[i];
            t.team = h.u_team[i]; t.type = h.u_type[i];
            set_flags(t, h.u_flags[i]);
            if (i < h.u_mc_victim.size()) t.mc_victim = h.u_mc_victim[i];
            if (i < h.u_mc_orig.size()) t.mc_orig = h.u_mc_orig[i];
            utrack_.push_back(t);
        }
        btrack_.clear();
        for (std::size_t i = 0; i < h.b_tag.size(); ++i) {
            Track t;
            t.tag = h.b_tag[i]; t.hp = h.b_hp[i]; t.type = h.b_type[i];
            set_flags(t, h.b_flags[i]);
            btrack_.push_back(t);
        }
        teams_.clear();
        for (std::size_t i = 0; i < h.t_type.size(); ++i) {
            Team t;
            t.type = h.t_type[i]; t.script = h.t_script[i]; t.house = h.t_house[i];
            t.st.pc = h.t_pc[i]; t.goal_x = h.t_goal_x[i]; t.goal_y = h.t_goal_y[i];
            t.members = h.t_members[i];
            t.st.wait_until = (long)h.t_wait_until[i];
            t.issued_frame = (long)h.t_issued_frame[i];
            t.created = (long)h.t_created[i];
            t.st.issued = (h.t_flags[i] & 1) != 0;
            t.st.done = (h.t_flags[i] & 2) != 0;
            t.recruiting = (h.t_flags[i] & 4) != 0;
            teams_.push_back(t);
        }
        destroyed_units_ = h.destroyed_units;
        destroyed_buildings_ = h.destroyed_buildings;
        for (std::size_t i = 0; i < last_built_.size() && 4 * i + 3 < h.last_built.size(); ++i)
            for (int k = 0; k < 4; ++k) last_built_[i][(std::size_t)k] = h.last_built[4 * i + (std::size_t)k];
        objectives_.clear();
        for (std::size_t i = 0; i < h.obj_frames.size() && i < h.obj_texts.size(); ++i)
            objectives_.push_back({(long)h.obj_frames[i], h.obj_texts[i]});
        end_ = ra2engine::MissionEnd(end_.borrowed_frames());
        if (h.end_outcome != 0) end_.flag(h.end_outcome, (long)h.end_flagged_at);
        frame_ = (long)h.frame;
        accum_ = h.accum;
        logged_ = (std::size_t)h.logged;
        teams_created_ = h.teams_created;
        raises_ = h.raises;
        return true;
    }

    // W3-FX play-script cheat: fire a map trigger by name now (camp_fire).
    int force_trigger(const std::string& name) {
        const int n = sys_->force_fire(name, host_);
        drain_log();
        return n;
    }
    // W2-CA-19 play-script cheat: place a map/TaskForce type like a reinforcement.
    int spawn(const std::string& type, int house, int x, int y) {
        return place_unit(type, house, x, y, 256, std::string(), "Guard", /*far=*/true);
    }
    const std::string& unit_type(int i) const {
        static const std::string none;
        return i >= 0 && i < (int)utrack_.size() ? utrack_[(std::size_t)i].type : none;
    }

    // W2-CA-12: trigger audio the scene plays through its presentation.
    struct AudioReq { int action; std::string name; int value; int waypoint; };
    std::vector<AudioReq> take_audio() { std::vector<AudioReq> o; o.swap(audio_); return o; }
    // W3-FX: trigger presentation requests (the trigger system's typed queue
    // plus the host's own DoExplosionAt anims). Presentation only.
    std::vector<ra2engine::VisualRequest> take_visuals() {
        std::vector<ra2engine::VisualRequest> o;
        o.swap(visuals_);
        return o;
    }
    // RA2 rules.ini lists the PlayAnimAt / ParticleAnim index spaces (see
    // campaign_visuals.inc) need.
    const Ini& ra2_rules() const { return ra2_rules_; }
    // W3-FX FlashTeam / MakeHouseCheer / FlashBuildingsOfType helpers.
    std::vector<int> team_unit_indices(const std::string& team_id) const {
        std::vector<int> out;
        const int ti = teams_data_.find_team(team_id);
        if (ti < 0) return out;
        for (const Team& t : teams_)
            if (t.type == ti && !t.st.done)
                for (int m : t.members) out.push_back(m);
        return out;
    }
    int engine_house_of_type(int ht) const { return engine_house(ht); }
    std::string building_type_id(int bt) const {
        return bt >= 0 && bt < (int)data_.building_types.size() ? camp::up(data_.building_types[(std::size_t)bt])
                                                                 : std::string();
    }
    // W3-FX DoExplosionAt Value -> WeaponType id. [RE-FALLBACK: the
    // WeaponTypeClass array is find-or-allocated as each TechnoType's INI is
    // read (FUN_00711510 Primary/Secondary/Elite*/Occupy/DeathWeapon); that
    // object-read order was not recovered, so the RA2 rules.ini FILE order of
    // those keys stands in.]
    std::string ra2_weapon_by_index(int ix) {
        if (ra2_weapons_.empty()) {
            static const char* kKeys[] = {"Primary", "Secondary", "ElitePrimary", "EliteSecondary",
                                          "OccupyWeapon", "EliteOccupyWeapon", "DeathWeapon"};
            for (const std::string& sec : ra2_rules_.section_names())
                for (const char* k : kKeys) {
                    const std::string w = camp::up(camp::trim_comment(ra2_rules_.get(sec, k, "")));
                    if (w.empty() || std::find(ra2_weapons_.begin(), ra2_weapons_.end(), w) != ra2_weapons_.end())
                        continue;
                    ra2_weapons_.push_back(w);
                }
        }
        return ix >= 0 && ix < (int)ra2_weapons_.size() ? ra2_weapons_[(std::size_t)ix] : std::string();
    }
    bool waypoint_xy(int wp, int& x, int& y) const { return waypoint_cell(wp, x, y); }
    std::string map_theme() const { return camp::trim_comment(map_.get("Basic", "Theme", "")); }

    std::uint64_t hash() const {
        std::uint64_t h = sys_->state_hash();
        auto mix = [&](std::uint64_t v) {
            for (int i = 0; i < 8; ++i) { h ^= (v >> (i * 8)) & 0xff; h *= 0x100000001b3ull; }
        };
        mix((std::uint64_t)frame_);
        mix((std::uint64_t)teams_created_);
        mix((std::uint64_t)raises_);
        mix((std::uint64_t)end_.outcome());
        for (const Team& t : teams_) {
            mix((std::uint64_t)t.type);
            mix((std::uint64_t)t.st.pc);
            mix((std::uint64_t)t.members.size());
        }
        for (const std::string& s : witness_)
            for (char c : s) { h ^= (unsigned char)c; h *= 0x100000001b3ull; }
        return h;
    }

private:
    // Tracking record per World unit/building (parallel to world arrays).
    struct Track {
        int tag = -1;               // tag type (-1 none)
        std::string type;           // TechnoType id (upper)
        int cx = -1, cy = -1;       // last cell (units)
        int hp = 0;                 // last hp
        bool dead = false;          // death already raised
        bool discovered = false;    // event 4 raised
        bool selected = false;      // last selected state
        int team = -1;              // map team index
        bool hunt = false;          // Hunt mission
        bool initial = true;        // placed by the map (not "built")
        std::uint8_t dmg_stage = 0; // W2-CA-14: 1 damaged, 2 <= half, 4 <= quarter raised
        int mc_victim = -1;         // W2-CA-15: unit this controller holds
        int mc_orig = -1;           // W2-CA-15: house before it was mind-controlled
    };
    struct Team {
        int type = -1;              // teams_data_.teams index
        int script = -1;            // teams_data_.scripts index
        int house = -1;             // engine house
        std::vector<int> members;   // world unit indices
        ra2engine::MapTeamState st;
        int goal_x = -1, goal_y = -1;
        long issued_frame = 0;
        bool recruiting = false;
        long created = 0;
    };

    static std::uint8_t flags_of(const Track& t) {
        return (std::uint8_t)((t.dead ? 1 : 0) | (t.discovered ? 2 : 0) | (t.selected ? 4 : 0) |
                              (t.hunt ? 8 : 0) | (t.initial ? 16 : 0) | ((t.dmg_stage & 7) << 5));
    }
    static void set_flags(Track& t, std::uint8_t f) {
        t.dead = f & 1; t.discovered = f & 2; t.selected = f & 4; t.hunt = f & 8; t.initial = f & 16;
        t.dmg_stage = (std::uint8_t)((f >> 5) & 7);
    }

    // --- houses [RE §3.3, map-triggers §6] ------------------------------------
    void setup_houses() {
        house_names_ = ra2engine::campaign_house_order(map_, basic_.player);
        if (house_names_.empty()) house_names_.push_back(basic_.player);
        for (const auto& kv : map_.section("Houses")) {
            const std::string hn = camp::trim_comment(kv.second);
            houses_order_ht_.push_back(data_.find_house_type(map_.get(hn, "Country", "")));
        }
        auto allies_of = [&](const std::string& h) {
            std::vector<std::string> v;
            std::string cur;
            for (char c : map_.get(h, "Allies", "") + ",") {
                if (c == ',') { v.push_back(camp::up(camp::trim_comment(cur))); cur.clear(); }
                else cur += c;
            }
            return v;
        };
        auto lists = [&](const std::string& a, const std::string& b) {
            const std::vector<std::string> v = allies_of(a);
            return std::find(v.begin(), v.end(), camp::up(b)) != v.end();
        };
        const std::string& player = house_names_[0];
        for (std::size_t i = 0; i < house_names_.size(); ++i) {
            const std::string& hn = house_names_[i];
            const std::string country = camp::trim_comment(map_.get(hn, "Country", ""));
            house_type_of_.push_back(data_.find_house_type(country));
            // W2-CA-05: [<Country>] Side= from the map, else from RA2 rules.ini
            // [Countries] sections (the standard countries live there; the map
            // only adds countries) [campaign.md §6].
            std::string side = camp::up(camp::trim_comment(map_.get(country, "Side", "")));
            if (side.empty()) side = camp::up(camp::trim_comment(ra2_rules_.get(country, "Side", "")));
            ra2engine::World::HouseRec rec;
            rec.side = side == "GDI" ? ra2engine::World::Side::Allied
                     : side == "NOD" ? ra2engine::World::Side::Soviet
                                     : ra2engine::World::Side::Neutral;
            rec.country = country;
            rec.color_ix = (int)(i % 8);
            // [DEVIATION: engine alliances are symmetric teams] team 0 = the
            // player + houses MUTUALLY allied with it; a civilian-side house the
            // player lists as an ally is a neutral non-combatant; every other
            // house is the enemy team 1.
            const bool mutual = i == 0 || (lists(player, hn) && lists(hn, player));
            // W2-CA-02 [RE: FUN_00500960 reads PlayerControl -> house+0x1ED;
            // FUN_0050B510 ControlledByHuman = +0x1EC IsHuman || +0x1ED outside a
            // network session]: the player commands this house's units. It
            // fights on the player's side [RE-FALLBACK: the maps list it in
            // the player's Allies=; the clone forces the mutual team-0 bond].
            {
                const std::string pc = camp::up(camp::trim_comment(map_.get(hn, "PlayerControl", "no")));
                rec.player_control = i > 0 && !pc.empty() && (pc[0] == 'Y' || pc[0] == 'T' || pc[0] == '1');
            }
            if (rec.player_control) {
                rec.combatant = true;
                rec.alliance_team = 0;
            } else if (rec.side == ra2engine::World::Side::Neutral && (i == 0 || lists(player, hn))) {
                rec.combatant = false;
                rec.alliance_team = (int)i + 100;
            } else {
                rec.combatant = true;
                rec.alliance_team = mutual ? 0 : 1;
            }
            if (i == 0) player_side_ = rec.side;
            w_.set_house((int)i, rec);
            // [X House] Credits= is in hundreds (RA1 HouseClass::Read_INI
            // lineage, PROBABLE).
            const double credits = 100.0 * map_.get_int(hn, "Credits", 0);
            if (i == 0) player_credits_ = credits;
            else w_.grant_credits_for_test((int)i, credits);
        }
        w_.enable_shroud(true);
        // W2-GA-03 hand-off: the scenario's own FogOfWar flag ([Basic], every
        // RA2 campaign map says no).
        {
            std::string fog = camp::up(camp::trim_comment(map_.get("Basic", "FogOfWar", "")));
            if (fog.empty()) fog = camp::up(camp::trim_comment(map_.get("Map", "FogOfWar", "no")));
            w_.set_fog_of_war(!fog.empty() && (fog[0] == 'Y' || fog[0] == 'T' || fog[0] == '1'));
        }
        destroyed_units_.assign(house_names_.size(), 0);
        destroyed_buildings_.assign(house_names_.size(), 0);
        last_built_.assign(house_names_.size(), {-1, -1, -1, -1});
    }
    // --- [G-69] campaign enemy base build-out ---------------------------------
    // [RE: campaign.md §9; map [<House>] NodeCount= + NNN=TYPE,X,Y base nodes
    // (TS/RA2 BaseClass::Read_INI lineage, the format on every RA2 map that
    // has nodes); trigger action 3 "Production begins" sets the house's
    // production flag (map_trigger.cpp house_flags bit0)]. Once a computer
    // house's production flag is set it runs the AI-1 base brain over its
    // OWN node list: builds missing nodes and rebuilds destroyed ones in
    // place, repairs (G-65) and fires superweapons it owns (G-66).
    // RE-FALLBACK (RE-A5 open): the DifficultyClass of a campaign computer
    // house is taken as 2 - campaign difficulty (the skirmish binding);
    // AI-2/AI-3 (action 74, aimd teams for map houses) are not armed yet.
    void resolve_base_nodes() {
        base_nodes_.assign(house_names_.size(), {});
        for (std::size_t i = 1; i < house_names_.size(); ++i) {
            const std::string& hn = house_names_[i];
            const int n = map_.get_int(hn, "NodeCount", 0);
            for (int k = 0; k < n; ++k) {
                char key[8];
                std::snprintf(key, sizeof key, "%03d", k);
                const std::string v = map_.get(hn, key, "");
                std::vector<std::string> f;
                std::string cur;
                for (char c : v + ",") {
                    if (c == ',') { f.push_back(camp::trim_comment(cur)); cur.clear(); }
                    else cur += c;
                }
                if (f.size() < 3) continue;
                const int t = types_.building(ba_, f[0]);
                if (t < 0) continue;
                ra2engine::PlannedNode node;
                node.type = t;
                node.cx = std::atoi(f[1].c_str());
                node.cy = std::atoi(f[2].c_str());
                base_nodes_[i].push_back(node);
            }
        }
    }
    // W2-CA-07 [map-triggers.md §5 action 74 AITriggersBegin; ai-trigger.md]:
    // once a computer house's AI-trigger flag is set (action 74, house_flags
    // bit 2) its team brain runs the MAP's own [AITriggerTypes]/[TeamTypes]/
    // [TaskForces]/[ScriptTypes] (the aimd.ini format) with the map country
    // as the House=/OwnerHouse key. [RE-FALLBACK (RE-A5 open): the map list
    // replaces aimd.ini for map houses; IsForSkirmish is not a filter in a
    // campaign (every map trigger has it 0).] A house with no base nodes
    // gets an empty catalog: it trains from the factories it owns but plans
    // no base of its own.
    void arm_production_houses() {
        for (std::size_t i = 1; i < house_names_.size(); ++i) {
            if (!w_.house_combatant((int)i) || w_.controllable((int)i)) continue;
            const ra2engine::World::Side side = w_.house_side((int)i);
            if (side == ra2engine::World::Side::Neutral) continue;
            const unsigned fl = sys_->house_flags(house_type((int)i));
            const bool want_base = (fl & 1u) && i < base_nodes_.size() && !base_nodes_[i].empty();
            const bool want_teams = (fl & 4u) && ai_roster_ && map_.has("AITriggerTypes");
            if (armed_.size() < house_names_.size()) armed_.resize(house_names_.size(), 0);
            // A loaded game's AI houses come back armed with the saved config.
            if (!armed_[i] && w_.ai_enabled((int)i)) armed_[i] = (std::uint8_t)((want_base ? 1 : 0) | (want_teams ? 2 : 0) | 4);
            const bool has_base = armed_[i] & 1, has_teams = armed_[i] & 2;
            if ((has_base || !want_base) && (has_teams || !want_teams)) continue;
            if (!want_base && !want_teams) continue;
            const Rules rules = Rules::load(rules_ini_);
            const std::string country = camp::trim_comment(map_.get(house_names_[i], "Country", ""));
            ra2engine::AiHouseConfig cfg = make_ai_config(
                ba_, rules, static_cast<ra2engine::AiDifficulty>(launch_.difficulty),
                /*start_credits=*/0.0, side, country);   // the house pool flows in via take_income
            const bool base = want_base || has_base;
            if (!base) cfg.catalog = ra2engine::AiCatalog{};   // trains, never plans a base
            if (want_teams || has_teams) arm_map_teams(cfg, (int)i, side, country, rules);
            armed_[i] = (std::uint8_t)((base ? 1 : 0) | ((want_teams || has_teams) ? 2 : 0) | 4);
            w_.enable_ai_house((int)i, std::move(cfg));
            if (base) w_.set_ai_base_nodes((int)i, base_nodes_[i]);
            char b[200];
            std::snprintf(b, sizeof b, "[camp] f%ld %s%s: %s nodes=%d", frame_,
                          base ? "production begins" : "", (want_teams || has_teams) ? " +map AI triggers" : "",
                          house_names_[i].c_str(), (int)base_nodes_[i].size());
            witness_.emplace_back(b);
        }
    }
    void arm_map_teams(ra2engine::AiHouseConfig& cfg, int h, ra2engine::World::Side side,
                       const std::string& country, const Rules& rules) {
        // This house's share of the AI roster by its stock (parent) country.
        UnitRoster mine;
        const std::string parent = camp::trim_comment(map_.get(country, "ParentCountry", ""));
        const std::string own = parent.empty() ? country : parent;
        for (const ra2engine::UnitTypeDef& d : ai_roster_->defs)
            if (aiside::owned_by(rules, d.id, side, own)) mine.defs.push_back(d);
        int ax = w_.grid_w() / 2, ay = w_.grid_h() / 2;
        for (const ra2engine::Unit& u : w_.units())
            if (live(u) && u.house == 0) { ax = u.cell_x; ay = u.cell_y; break; }
        cfg.team = make_ai_team_config(ba_, mine, rules, map_, cfg.difficulty, ax, ay,
                                       /*enable_triggers=*/true, &w_.map());
        for (ra2engine::AITriggerType& t : cfg.team.aimd.triggers) t.is_for_skirmish = true;
        auto id_of = [&](int t) -> const char* {
            return (t >= 0 && t < (int)ba_.roster.size()) ? ba_.roster[(std::size_t)t].id.c_str() : "";
        };
        const ra2engine::AiCatalog full = make_ai_config(ba_, rules, cfg.difficulty, 0.0, side, own).catalog;
        cfg.team.inf_factory_types = factory_types_for(ba_, id_of(full.barracks));
        cfg.team.veh_factory_types = factory_types_for(ba_, id_of(full.war_factory));
        cfg.team.trigger_side = side == ra2engine::World::Side::Soviet ? 2 : 1;
        cfg.team.trigger_tech_level = map_.get_int(house_names_[(std::size_t)h], "TechLevel", 10);
        cfg.team.house_name = country;
        fill_ai_building_maps(cfg.team, ba_, rules, rules_ini_);
    }
public:
    // W2-CA-07: the GameScene's AI roster (unit defs the map teams can field).
    void set_ai_roster(const UnitRoster* r) { ai_roster_ = r; }
private:
    int engine_house_of_name(const std::string& name) const {
        const std::string u = camp::up(name);
        for (std::size_t i = 0; i < house_names_.size(); ++i)
            if (camp::up(house_names_[i]) == u) return (int)i;
        // TeamType House= names a country.
        for (std::size_t i = 0; i < house_names_.size(); ++i)
            if (camp::up(map_.get(house_names_[i], "Country", "")) == u) return (int)i;
        return -1;
    }
    int engine_house(int ht) const {
        for (std::size_t i = 0; i < house_type_of_.size(); ++i)
            if (house_type_of_[i] == ht && ht >= 0) return (int)i;
        return -1;
    }
    int house_type(int eh) const {
        return eh >= 0 && eh < (int)house_type_of_.size() ? house_type_of_[(std::size_t)eh] : -1;
    }

    // --- objects [RE §4] ------------------------------------------------------
    static bool live(const ra2engine::Unit& u) { return u.alive && u.die_time < 0.0; }
    static bool live(const ra2engine::Building& b) {
        return b.alive && b.state != ra2engine::Building::State::Dead;
    }
    bool free_cell_near(int x, int y, int& ox, int& oy, int maxr = 6) const {
        for (int r = 0; r <= maxr; ++r)
            for (int dy = -r; dy <= r; ++dy)
                for (int dx = -r; dx <= r; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                    const int cx = x + dx, cy = y + dy;
                    if (w_.walkable(cx, cy) && w_.unit_at_cell(cx, cy) < 0 &&
                        w_.building_at_cell(cx, cy) < 0) {
                        ox = cx;
                        oy = cy;
                        return true;
                    }
                }
        return false;
    }
    bool free_water_near(int x, int y, int& ox, int& oy, int maxr) const {
        const std::vector<std::uint8_t>& wg = w_.grid_for(ra2engine::MovementDomain::Water);
        if (wg.empty()) return false;
        for (int r = 0; r <= maxr; ++r)
            for (int dy = -r; dy <= r; ++dy)
                for (int dx = -r; dx <= r; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                    const int cx = x + dx, cy = y + dy;
                    if (cx < 0 || cy < 0 || cx >= w_.grid_w() || cy >= w_.grid_h()) continue;
                    if (!wg[(std::size_t)cy * w_.grid_w() + cx]) continue;
                    if (w_.unit_at_cell(cx, cy) >= 0 || w_.building_at_cell(cx, cy) >= 0) continue;
                    if (!w_.occupancy().is_free(cx, cy)) continue;
                    ox = cx;
                    oy = cy;
                    return true;
                }
        return false;
    }
    bool waypoint_cell(int wp, int& x, int& y) const {
        for (const ra2assets::MapWaypoint& m : w_.map().waypoints())
            if (m.index == wp) { x = m.x; y = m.y; return true; }
        return false;
    }
    void sync_tracks() {
        while (utrack_.size() < w_.units().size()) {
            Track t;
            t.initial = false;
            const ra2engine::Unit& u = w_.units()[utrack_.size()];
            t.type = camp::up(u.image);
            t.cx = u.cell_x;
            t.cy = u.cell_y;
            t.hp = u.hp;
            // A unit the World produced (factory) counts as last-built.
            if (!placing_ && u.house >= 0 && u.house < (int)last_built_.size()) {
                const int vi = camp::index_of(ra2_vehicles_, t.type);
                const int ii = camp::index_of(ra2_infantry_, t.type);
                if (vi >= 0) last_built_[(std::size_t)u.house][1] = vi;
                // W2-CA-14: event 22 BuildAircraftType.
                const int ai = camp::index_of(ra2_aircraft_, t.type);
                if (ai >= 0) last_built_[(std::size_t)u.house][3] = ai;
                if (ii >= 0) last_built_[(std::size_t)u.house][2] = ii;
            }
            utrack_.push_back(t);
        }
        while (btrack_.size() < w_.buildings().size()) {
            Track t;
            t.initial = false;
            const ra2engine::Building& b = w_.buildings()[btrack_.size()];
            if (b.type >= 0 && b.type < (int)ba_.roster.size())
                t.type = camp::up(ba_.roster[(std::size_t)b.type].id);
            t.hp = b.hp;
            if (!placing_ && b.house >= 0 && b.house < (int)last_built_.size())
                last_built_[(std::size_t)b.house][0] = data_.find_building_type(t.type);
            btrack_.push_back(t);
        }
    }
    // W2-CA-08 [RE-C1 open -> RE-FALLBACK]: a reinforcement whose waypoint
    // has no legal cell within 12 (at sea, off the land grid, beyond the
    // map edge) lands on the nearest free legal cell anywhere: ships on the
    // nearest free water, ground units on the nearest free land cell that is
    // CONNECTED to the team's first move-waypoint (the beach the landing
    // craft would have run to), else the nearest free land cell. The
    // original delivers by transport/edge entry (TeamClass FUN_006eff20).
    bool far_cell(int x, int y, bool water, int goal_x, int goal_y, int& ox, int& oy) {
        const int W = w_.grid_w(), H = w_.grid_h();
        if (W <= 0 || H <= 0) return false;
        const std::vector<std::uint8_t>& wg = w_.grid_for(ra2engine::MovementDomain::Water);
        std::vector<std::uint8_t> reach;
        if (!water && goal_x >= 0 && w_.walkable(goal_x, goal_y)) {
            reach.assign((std::size_t)W * H, 0);
            std::vector<int> q{goal_y * W + goal_x};
            reach[(std::size_t)q[0]] = 1;
            for (std::size_t k = 0; k < q.size(); ++k) {
                const int cx = q[k] % W, cy = q[k] / W;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int nx = cx + dx, ny = cy + dy;
                        if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                        const std::size_t ni = (std::size_t)ny * W + nx;
                        if (reach[ni] || !w_.walkable(nx, ny)) continue;
                        reach[ni] = 1;
                        q.push_back((int)ni);
                    }
            }
        }
        x = std::clamp(x, 0, W - 1);
        y = std::clamp(y, 0, H - 1);
        const int maxr = std::max(W, H);
        for (int pass = 0; pass < 2; ++pass) {
            if (pass == 1 && reach.empty()) break;
            const bool need_reach = pass == 0 && !reach.empty();
            for (int r = 0; r <= maxr; ++r)
                for (int dy = -r; dy <= r; ++dy)
                    for (int dx = -r; dx <= r; ++dx) {
                        if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                        const int cx = x + dx, cy = y + dy;
                        if (cx < 0 || cy < 0 || cx >= W || cy >= H) continue;
                        const std::size_t ci = (std::size_t)cy * W + cx;
                        if (water) {
                            if (wg.empty() || !wg[ci] || !w_.occupancy().is_free(cx, cy)) continue;
                        } else {
                            if (!w_.walkable(cx, cy)) continue;
                            if (need_reach && !reach[ci]) continue;
                        }
                        if (w_.unit_at_cell(cx, cy) >= 0 || w_.building_at_cell(cx, cy) >= 0) continue;
                        ox = cx;
                        oy = cy;
                        return true;
                    }
            if (water) break;
        }
        return false;
    }
    int place_unit(const std::string& type, int eh, int x, int y, int health, const std::string& tag,
                   const std::string& mission, bool far = false, int goal_x = -1, int goal_y = -1) {
        const camp::UnitType* ut = types_.unit(type);
        if (!ut) return -1;
        int cx = 0, cy = 0;
        // gF G-73: a ship is placed on the nearest free WATER cell (naval.md
        // §1 water grid); a ground unit whose waypoint lies at sea goes to the
        // nearest free shore cell within 12 [RE-FALLBACK §9.3: the original
        // delivers by transport / edge entry; no TeamClass delivery yet].
        if (ut->combat.is_aircraft) {
            // W2-CA-16: an aircraft enters at its waypoint (the map edge for
            // most teams) on a free cell nearby; it takes off on its first order.
            x = std::clamp(x, 0, std::max(0, w_.grid_w() - 1));
            y = std::clamp(y, 0, std::max(0, w_.grid_h() - 1));
            cx = x;
            cy = y;
            bool found = false;
            for (int r = 0; r <= 12 && !found; ++r)
                for (int dy = -r; dy <= r && !found; ++dy)
                    for (int dx = -r; dx <= r && !found; ++dx) {
                        if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                        const int ax = x + dx, ay = y + dy;
                        if (ax < 0 || ay < 0 || ax >= w_.grid_w() || ay >= w_.grid_h()) continue;
                        if (w_.unit_at_cell(ax, ay) >= 0 || w_.building_at_cell(ax, ay) >= 0) continue;
                        if (!w_.occupancy().is_free(ax, ay)) continue;
                        cx = ax; cy = ay; found = true;
                    }
            if (!found) return -1;
        } else if (ut->combat.is_naval) {
            if (!free_water_near(x, y, cx, cy, 12) &&
                !(far && far_cell(x, y, /*water=*/true, -1, -1, cx, cy)))
                return -1;
        } else if (!free_cell_near(x, y, cx, cy) && !free_cell_near(x, y, cx, cy, 12) &&
                   !(far && far_cell(x, y, /*water=*/false, goal_x, goal_y, cx, cy))) {
            return -1;
        }
        w_.add_combat_unit(cx, cy, eh, ut->combat);
        const int idx = (int)w_.units().size() - 1;
        ra2engine::Unit& u = w_.units()[(std::size_t)idx];
        if (health > 0) u.hp = ra2engine::campaign_strength(u.max_hp, health, false);
        if (ua_anims_has(u.image)) {}
        // W3-FIX-A (R7): a BalloonHover jumpjet (the Kirov) never lands, so a
        // map-placed or reinforcing one starts at its hover height instead of
        // parked on the ground. order_unit_move releases its ground cell and
        // opens the flight step (the factory-exit lift-off path, W2-AN-07).
        if (u.is_aircraft && u.balloon_hover) {
            w_.order_unit_move(idx, cx, cy);
            ra2engine::Unit& a = w_.units()[(std::size_t)idx];
            a.altitude = a.cruise_altitude > 0.0 ? a.cruise_altitude : a.altitude;
            if (a.altitude > 0.0) a.flight_state = ra2engine::FlightState::Cruising;
        }
        placing_ = true;
        sync_tracks();
        placing_ = false;
        Track& t = utrack_[(std::size_t)idx];
        t.type = camp::up(type);
        t.initial = true;
        t.hp = u.hp;
        t.tag = tag.empty() ? -1 : data_.find_tag(tag);
        // An object created after scenario start attaches its tag now (the
        // ctor attaches the map's own objects once the system exists).
        if (t.tag >= 0 && sys_) sys_->attach_tag(t.tag);
        t.hunt = ra2engine::mission_from_name(mission) == ra2engine::kMissionHunt;
        return idx;
    }
    static bool ua_anims_has(const std::string&) { return true; }
    void place_objects(const std::vector<ra2engine::MapObjectRec>& objs) {
        for (const ra2engine::MapObjectRec& o : objs) {
            const int eh = engine_house_of_name(o.owner);
            if (eh < 0) { ++skipped_; continue; }
            if (o.kind == 0) {
                const int bt = types_.building(ba_, o.type);
                if (bt < 0) { ++skipped_; continue; }
                const ra2engine::BuildingTypeDef& d = ba_.roster[(std::size_t)bt];
                // W3-FIX-A: a WaterBound type (Naval Yard) is legal on WATER, as
                // the player's placement already checks (realplay_scene.inc);
                // the land grid refused every map-placed yard (ALL04/07/11).
                const bool water = (std::size_t)bt < ba_.water_bound.size() && ba_.water_bound[(std::size_t)bt] &&
                                   !w_.grid_for(ra2engine::MovementDomain::Water).empty();
                const ra2engine::PlacementCheck chk = ra2engine::placement_check(
                    o.x, o.y, d.fw, d.fh, eh, d.adjacent,
                    water ? w_.grid_for(ra2engine::MovementDomain::Water) : w_.passable(), w_.grid_w(),
                    w_.grid_h(), w_.occupancy(), w_.buildings(), /*bypass=*/true);
                if (!chk.ok) {   // Unlimbo failed -> deleted [RE §4]
                    std::cerr << "[campaign] structure " << o.type << " at " << o.x << "," << o.y
                              << " refused (" << (water ? "water" : "land") << ")\n";
                    ++skipped_;
                    continue;
                }
                const int idx = w_.add_building(bt, o.x, o.y, d.fw, d.fh, eh, d.strength, d.armor_ix,
                                                0.0, /*instant=*/true, d.sight, d.grants_map_reveal);
                if (idx < 0) { ++skipped_; continue; }
                if (water) w_.block_water_footprint(o.x, o.y, d.fw, d.fh);   // ships path around it
                ra2engine::Building& b = w_.buildings()[(std::size_t)idx];
                b.hp = ra2engine::campaign_strength(d.strength, o.health, true);
                placing_ = true;
                sync_tracks();
                placing_ = false;
                Track& t = btrack_[(std::size_t)idx];
                t.tag = o.tag.empty() ? -1 : data_.find_tag(o.tag);
                t.hp = b.hp;
                ++placed_b_;
            } else if (o.kind == 3) {   // W2-CA-16: [Aircraft] objects
                if (place_unit(o.type, eh, o.x, o.y, o.health, o.tag, o.mission) >= 0) ++placed_u_;
                else ++skipped_;
            } else {
                if (!w_.walkable(o.x, o.y) && !types_.unit(o.type)) { ++skipped_; continue; }
                if (place_unit(o.type, eh, o.x, o.y, o.health, o.tag, o.mission) >= 0) ++placed_u_;
                else ++skipped_;
            }
        }
    }

    // --- event polling [RE map-triggers §3.2] --------------------------------
    ra2engine::RaiseContext ctx_for(int house) const {
        ra2engine::RaiseContext c;
        c.has_object = true;
        c.object_house_type = house_type(house);
        return c;
    }
    void raise_obj(Track& t, int kind, int house) {
        if (t.tag < 0) return;
        const ra2engine::MapTriggerSystem::ObjectRaise r =
            sys_->raise_object(t.tag, kind, ctx_for(house), host_);
        ++raises_;
        if (r.detached || r.tag_gone) t.tag = -1;
    }
    // gF G-74 [RE: docs/re/mcv-deploy.md §7]: event 44 AttackedByHouse is
    // raised by ReceiveDamage right after 6 with the ATTACKER's house
    // (FUN_005F4BA0 @0x5F5015, BuildingClass FUN_004690A0 @0x4692A8). World
    // keeps no per-hit source, so the attacker is the first live enemy unit
    // whose current target is the victim [RE-FALLBACK: inferred source].
    int gf_attacker_house(int unit_victim, int building_victim, int victim_house) const {
        const std::vector<ra2engine::Unit>& us = w_.units();
        for (const ra2engine::Unit& a : us) {
            if (!live(a) || a.house == victim_house) continue;
            if ((unit_victim >= 0 && a.target == unit_victim) ||
                (building_victim >= 0 && a.target_building == building_victim))
                return a.house;
        }
        return -1;
    }
    void raise_attacked(Track& t, int house, int src_house) {
        raise_obj(t, ra2engine::kEvAttackedByAnybody, house);
        if (t.tag < 0 || src_house < 0) return;
        ra2engine::RaiseContext c = ctx_for(house);
        c.source_house_type = house_type(src_house);
        const ra2engine::MapTriggerSystem::ObjectRaise r =
            sys_->raise_object(t.tag, ra2engine::kEvAttackedByHouse, c, host_);
        ++raises_;
        if (r.detached || r.tag_gone) t.tag = -1;
    }
    // gF G-74: 25/26 CrossesHorizontal/VerticalLine -- FootClass per-cell
    // (FUN_004D84F0 @0x4D8990..0x4D8A8A): entering a cell whose row (column)
    // is flagged scans that whole row (column) and raises 25 (26) on every
    // cell tag in it.
    void gf_raise_lines(int x, int y, const ra2engine::RaiseContext& c) {
        for (const ra2engine::MapCellTag& ct : data_.cell_tags) {
            if (ct.y == y && sys_->raise_cell(ct.x, ct.y, ra2engine::kEvCrossesHorizontalLine, c, host_)) ++raises_;
            if (ct.x == x && sys_->raise_cell(ct.x, ct.y, ra2engine::kEvCrossesVerticalLine, c, host_)) ++raises_;
        }
    }
    // W2-CA-01 [RE: FUN_00519449 @0x519545/0x5195AB/0x51982E/0x519973/
    // 0x519D2C/0x51A052/0x51A304]: an infantry arriving INTO a building
    // raises 1 EnteredBy on the building's tag with the infantry as the
    // object; the consumed infantry then raises 48 on its own tag
    // (@0x5196B4/0x519E0D) -- not 7/29, and it is not a loss.
    // [RE-FALLBACK: no SpiedBy (2) RaiseEvent site was found in the infantry
    // handler; 2 is raised on infiltration beside 1 as the event's name says.]
    void poll_entered() {
        w_.set_track_entered(true);
        for (const ra2engine::World::EnteredEvent& e : w_.take_entered_events()) {
            if (e.building >= 0 && e.building < (int)btrack_.size()) {
                Track& bt = btrack_[(std::size_t)e.building];
                raise_obj(bt, ra2engine::kEvEnteredBy, e.house);
                if (e.kind == ra2engine::World::EnteredEvent::Infiltrate)
                    raise_obj(bt, ra2engine::kEvSpiedBy, e.house);
                char b[120];
                std::snprintf(b, sizeof b, "[camp] f%ld entered kind=%d unit=%d building=%d %s house=%d",
                              frame_, (int)e.kind, e.unit, e.building, bt.type.c_str(), e.house);
                witness_.emplace_back(b);
            }
            if (e.consumed && e.unit >= 0 && e.unit < (int)utrack_.size()) {
                Track& ut = utrack_[(std::size_t)e.unit];
                if (ut.dead) continue;
                ut.dead = true;
                raise_obj(ut, ra2engine::kEvDestroyedByAnything, e.house);
                if (ut.tag >= 0) { sys_->detach_tag(ut.tag); ut.tag = -1; }
            }
        }
    }
    // W2-CA-14: 38/41 first damage, 39/42 at or below half, 40/43 at or below
    // a quarter of Strength, each once per object [map-triggers.md §3 event
    // table; RE-W2-4 open: the combat-only (38-40) vs any-source (41-43) split
    // is not modelled -- the clone has no non-combat damage source on these
    // objects, so both of a pair are raised together (RE-FALLBACK)].
    void raise_damage_stages(Track& t, int hp, int max_hp, int house) {
        if (max_hp <= 0 || t.tag < 0) return;
        const struct { std::uint8_t bit; bool hit; int ev_combat, ev_any; } st[] = {
            {1, hp < max_hp, 38, 41}, {2, 2 * hp <= max_hp, 39, 42}, {4, 4 * hp <= max_hp, 40, 43}};
        for (const auto& s : st) {
            if (!s.hit || (t.dmg_stage & s.bit)) continue;
            t.dmg_stage |= s.bit;
            raise_obj(t, s.ev_combat, house);
            if (t.tag >= 0) raise_obj(t, s.ev_any, house);
            if (t.tag < 0) return;
        }
    }
    // W2-CA-15 [RE-W2-5 open -> RE-FALLBACK; rules: a weapon whose Warhead has
    // MindControl=yes (Yuri, Psychic Tower/Beacon lineage)]: a controller
    // whose current target is an enemy unit in weapon range takes it over --
    // the victim changes to the controller's house and drops its orders; one
    // victim per controller (a new capture frees the previous one back to
    // its house); a dead controller frees its victim. The capture raises 6
    // and 44 (attacker = controller's house) on the victim, as a hit does.
    bool mind_controller(const std::string& type) {
        auto it = mc_type_.find(type);
        if (it != mc_type_.end()) return it->second;
        const Rules rules = Rules::load(rules_ini_);
        const std::string wh = rules.weapon_warhead(rules.section_string(type, "Primary", ""));
        const bool mc = !wh.empty() && rules.section_flag(wh, "MindControl", false);
        mc_type_[type] = mc;
        return mc;
    }
    void mc_release(Track& ctl) {
        const int v = ctl.mc_victim;
        ctl.mc_victim = -1;
        if (v < 0 || v >= (int)utrack_.size()) return;
        Track& vt = utrack_[(std::size_t)v];
        ra2engine::Unit& vu = w_.units()[(std::size_t)v];
        if (vt.mc_orig >= 0 && live(vu)) { vu.house = vt.mc_orig; vu.target = -1; vu.target_building = -1; }
        vt.mc_orig = -1;
    }
    void poll_mind_control() {
        for (std::size_t i = 0; i < utrack_.size(); ++i) {
            Track& t = utrack_[i];
            ra2engine::Unit& u = w_.units()[i];
            if (t.mc_victim >= 0 && (!live(u))) { mc_release(t); continue; }
            if (t.dead || !live(u) || u.target < 0 || u.target >= (int)w_.units().size()) continue;
            if (!mind_controller(t.type)) continue;
            ra2engine::Unit& v = w_.units()[(std::size_t)u.target];
            if (!live(v) || !w_.is_enemy(u.house, v.house) || v.is_aircraft) continue;
            const double dx = v.fx - u.fx, dy = v.fy - u.fy;
            if (std::sqrt(dx * dx + dy * dy) > std::max(1.0, u.w_range) + 0.5) continue;
            const int vi = u.target;
            if (t.mc_victim >= 0 && t.mc_victim != vi) mc_release(t);
            Track& vt = utrack_[(std::size_t)vi];
            const int from = v.house;
            if (vt.mc_orig < 0) vt.mc_orig = from;
            raise_attacked(vt, from, u.house);
            v.house = u.house;
            v.target = -1;
            v.target_building = -1;
            u.target = -1;
            u.attack_order = false;
            t.mc_victim = vi;
            char b[120];
            std::snprintf(b, sizeof b, "[camp] f%ld mind control unit=%d %s house %d -> %d", frame_, vi,
                          vt.type.c_str(), from, u.house);
            witness_.emplace_back(b);
        }
    }
    void poll_objects() {
        sync_tracks();
        poll_entered();
        poll_mind_control();
        for (std::size_t i = 0; i < utrack_.size(); ++i) {
            Track& t = utrack_[i];
            if (t.dead) continue;
            const ra2engine::Unit& u = w_.units()[i];
            if (!live(u) && u.deployed_away) {
                // gF: an MCV that unfolded into its ConYard left the map by
                // Limbo+delete, not by destruction -- no Destroyed event, not
                // counted as a loss [RE: docs/re/mcv-deploy.md §2 step 6].
                t.dead = true;
                if (t.tag >= 0) { sys_->detach_tag(t.tag); t.tag = -1; }
                continue;
            }
            if (!live(u)) {
                t.dead = true;
                if (u.house >= 0 && u.house < (int)destroyed_units_.size())
                    ++destroyed_units_[(std::size_t)u.house];
                // gF G-74 [FUN_00743550 / FUN_007021D0]: the original order is
                // 7 DestroyedByAnybody (killer present), 48, then 29.
                raise_obj(t, ra2engine::kEvDestroyedByAnybody, u.house);
                raise_obj(t, ra2engine::kEvDestroyedByAnything, u.house);
                raise_obj(t, ra2engine::kEvDestroyedFakesAll, u.house);
                if (t.tag >= 0) { sys_->detach_tag(t.tag); t.tag = -1; }
                continue;
            }
            if (u.cell_x != t.cx || u.cell_y != t.cy) {
                t.cx = u.cell_x;
                t.cy = u.cell_y;
                const ra2engine::RaiseContext c = ctx_for(u.house);
                if (sys_->raise_cell(t.cx, t.cy, ra2engine::kEvEnteredBy, c, host_)) ++raises_;
                sys_->raise_cell(t.cx, t.cy, ra2engine::kEvEnteredOrOverflownBy, c, host_);
                gf_raise_lines(t.cx, t.cy, c);
            }
            if (u.hp < t.hp) raise_attacked(t, u.house, gf_attacker_house((int)i, -1, u.house));
            if (u.hp < t.hp) raise_damage_stages(t, u.hp, u.max_hp, u.house);
            t.hp = u.hp;
            if (!t.discovered && u.house != 0 && w_.explored_at(0, u.cell_x, u.cell_y)) {
                t.discovered = true;
                raise_obj(t, ra2engine::kEvDiscoveredByPlayer, u.house);
            }
            if (u.selected && !t.selected) raise_obj(t, ra2engine::kEvSelectedByPlayer, u.house);
            t.selected = u.selected;
        }
        for (std::size_t i = 0; i < btrack_.size(); ++i) {
            Track& t = btrack_[i];
            if (t.dead) continue;
            const ra2engine::Building& b = w_.buildings()[i];
            if (!live(b)) {
                t.dead = true;
                if (b.house >= 0 && b.house < (int)destroyed_buildings_.size())
                    ++destroyed_buildings_[(std::size_t)b.house];
                // gF G-74 [FUN_00743550 / FUN_007021D0]: the original order is
                // 7 DestroyedByAnybody (killer present), 48, then 29.
                raise_obj(t, ra2engine::kEvDestroyedByAnybody, b.house);
                raise_obj(t, ra2engine::kEvDestroyedByAnything, b.house);
                raise_obj(t, ra2engine::kEvDestroyedFakesAll, b.house);
                if (t.tag >= 0) { sys_->detach_tag(t.tag); t.tag = -1; }
                continue;
            }
            if (b.hp < t.hp) raise_attacked(t, b.house, gf_attacker_house(-1, (int)i, b.house));
            if (b.hp < t.hp) raise_damage_stages(t, b.hp, b.max_hp, b.house);
            t.hp = b.hp;
            if (!t.discovered && b.house != 0 && w_.explored_at(0, b.cx, b.cy)) {
                t.discovered = true;
                raise_obj(t, ra2engine::kEvDiscoveredByPlayer, b.house);
            }
        }
    }

    // --- teams [RE §7, §9 item 3] ---------------------------------------------
    std::vector<int> taskforce_counts(const Team& tm, std::vector<std::string>& types) const {
        std::vector<int> need;
        types.clear();
        const ra2engine::MapTeamType& tt = teams_data_.teams[(std::size_t)tm.type];
        if (const auto* tf = teams_data_.taskforce(tt.taskforce))
            for (const ra2engine::MapTaskMember& m : *tf) {
                types.push_back(camp::up(m.type));
                need.push_back(m.count);
            }
        return need;
    }
    bool recruit(Team& tm) {
        std::vector<std::string> types;
        std::vector<int> need = taskforce_counts(tm, types);
        for (int m : tm.members)
            for (std::size_t k = 0; k < types.size(); ++k)
                if (utrack_[(std::size_t)m].type == types[k] && need[k] > 0) { --need[k]; break; }
        bool any = false;
        for (std::size_t i = 0; i < utrack_.size(); ++i) {
            Track& t = utrack_[i];
            if (t.dead) continue;
            // W2-CA-09 [ai-team.md §2]: a unit is free when it has no team, its
            // team is finished, or its TeamType says AreTeamMembersRecruitable.
            if (t.team >= 0 && !member_recruitable(t.team, &tm)) continue;
            const ra2engine::Unit& u = w_.units()[i];
            if (!live(u) || u.house != tm.house) continue;
            for (std::size_t k = 0; k < types.size(); ++k)
                if (need[k] > 0 && t.type == types[k]) {
                    --need[k];
                    if (t.team >= 0) {   // leave the old team
                        std::vector<int>& om = teams_[(std::size_t)t.team].members;
                        om.erase(std::remove(om.begin(), om.end(), (int)i), om.end());
                        t.team = -1;
                    }
                    tm.members.push_back((int)i);
                    any = true;
                    break;
                }
        }
        return any;
    }
    bool member_recruitable(int team_ix, const Team* self) const {
        if (team_ix < 0 || team_ix >= (int)teams_.size()) return true;
        const Team& o = teams_[(std::size_t)team_ix];
        if (&o == self) return false;
        if (o.st.done) return true;
        return o.type >= 0 && teams_data_.teams[(std::size_t)o.type].recruitable;
    }
    // W2-CA-09: a finished team lets its members go.
    void release_team(Team& tm, int team_idx) {
        for (int m : tm.members)
            if (utrack_[(std::size_t)m].team == team_idx) utrack_[(std::size_t)m].team = -1;
        tm.members.clear();
    }
    void adopt(Team& tm, int team_idx) {
        for (int m : tm.members) utrack_[(std::size_t)m].team = team_idx;
    }
    // CreateTeam (4) / Reinforcement (7/80/107).
    bool create_team(const std::string& id, bool reinforce, int waypoint) {
        const int ti = teams_data_.find_team(id);
        if (ti < 0) { note_ = "[team not found]"; return false; }
        const ra2engine::MapTeamType& tt = teams_data_.teams[(std::size_t)ti];
        Team tm;
        tm.type = ti;
        tm.script = teams_data_.find_script(tt.script);
        tm.house = engine_house_of_name(tt.house);
        tm.created = frame_;
        if (tm.house < 0) { note_ = "[no house]"; return false; }
        char b[160];
        if (reinforce) {
            // [RE-FALLBACK §9.3] TaskForce members appear at the team waypoint.
            const int wp = waypoint >= 0 ? waypoint : tt.waypoint;
            int x = 0, y = 0;
            if (!waypoint_cell(wp, x, y)) { note_ = "[no waypoint]"; return false; }
            int placed = 0, skipped = 0;
            // W2-CA-08: the team's first move-waypoint is the landing goal.
            int gx = -1, gy = -1;
            if (tm.script >= 0)
                for (const ra2engine::MapScriptAction& sa : teams_data_.scripts[(std::size_t)tm.script].actions)
                    if (sa.action == ra2engine::kSaMoveWaypoint) {
                        if (!waypoint_cell(sa.arg, gx, gy)) gx = gy = -1;
                        break;
                    }
            if (const auto* tf = teams_data_.taskforce(tt.taskforce))
                for (const ra2engine::MapTaskMember& m : *tf)
                    for (int k = 0; k < m.count; ++k) {
                        // TeamType Tag= rides on every member [map-triggers.md §1].
                        const std::string team_tag =
                            tt.tag.empty() || camp::up(tt.tag) == "<NONE>" ? std::string() : tt.tag;
                        const int idx = place_unit(m.type, tm.house, x, y, 256, team_tag, "Guard",
                                                   /*far=*/true, gx, gy);
                        // W2-CA-08: a far landing becomes the team's anchor, so
                        // the rest spread around it instead of lining the shore.
                        if (idx >= 0) {
                            const ra2engine::Unit& pu = w_.units()[(std::size_t)idx];
                            const int ddx = pu.cell_x - x, ddy = pu.cell_y - y;
                            if (!pu.is_naval && std::max(std::abs(ddx), std::abs(ddy)) > 12) {
                                x = pu.cell_x;
                                y = pu.cell_y;
                            }
                        }
                        if (idx >= 0) { tm.members.push_back(idx); ++placed; }
                        else ++skipped;
                    }
            std::snprintf(b, sizeof b, "[house=%d wp=%d(%d,%d) placed=%d skipped=%d]", tm.house, wp,
                          x, y, placed, skipped);
            if (placed == 0) { note_ = b; return false; }
        } else {
            recruit(tm);
            tm.recruiting = tm.members.empty() || tt.full;
            std::snprintf(b, sizeof b, "[house=%d recruited=%d]", tm.house, (int)tm.members.size());
        }
        note_ = b;
        teams_.push_back(tm);
        adopt(teams_.back(), (int)teams_.size() - 1);
        ++teams_created_;
        return true;
    }
    int nearest_enemy(int house, int x, int y, int quarry, bool& is_building) const {
        int best = -1;
        long bd = 0;
        is_building = false;
        const bool want_b = quarry == 2 || quarry == 6 || quarry == 7 || quarry == 9;
        const bool want_inf = quarry == 4, want_veh = quarry == 5;
        if (!want_b)
            for (std::size_t i = 0; i < w_.units().size(); ++i) {
                const ra2engine::Unit& u = w_.units()[i];
                if (!live(u) || !w_.is_enemy(house, u.house)) continue;
                if (want_inf && u.is_vehicle) continue;
                if (want_veh && !u.is_vehicle) continue;
                const long d = (long)(u.cell_x - x) * (u.cell_x - x) + (long)(u.cell_y - y) * (u.cell_y - y);
                if (best < 0 || d < bd) { best = (int)i; bd = d; }
            }
        if (!want_inf && !want_veh)
            for (std::size_t i = 0; i < w_.buildings().size(); ++i) {
                const ra2engine::Building& b = w_.buildings()[i];
                if (!live(b) || !w_.is_enemy(house, b.house)) continue;
                const long d = (long)(b.cx - x) * (b.cx - x) + (long)(b.cy - y) * (b.cy - y);
                if (best < 0 || d < bd || (!is_building && quarry != 1 && want_b)) {
                    best = (int)i;
                    bd = d;
                    is_building = true;
                }
            }
        return best;
    }
    void order_attack(int m, int target, bool building) {
        // W2-CA-16: a fixed-wing team member flies a strike sortie.
        if (w_.order_fixed_wing(m, building ? -1 : target, building ? target : -1, -1, -1)) return;
        if (building) w_.order_unit_attack_building(m, target);
        else w_.order_unit_attack(m, target);
    }
    void step_teams() {
        for (std::size_t ti = 0; ti < teams_.size(); ++ti) {
            Team& tm = teams_[ti];
            if (tm.st.done) { if (!tm.members.empty()) release_team(tm, (int)ti); continue; }
            // Prune dead members.
            std::vector<int> live_m;
            for (int m : tm.members)
                if (live(w_.units()[(std::size_t)m]) && w_.units()[(std::size_t)m].house == tm.house)
                    live_m.push_back(m);
            tm.members = live_m;
            if (tm.recruiting) {
                if (recruit(tm)) adopt(tm, (int)ti);
                const bool full = teams_data_.teams[(std::size_t)tm.type].full;
                std::vector<std::string> types;
                std::vector<int> need = taskforce_counts(tm, types);
                int total = 0;
                for (int n : need) total += n;
                if ((!full && !tm.members.empty()) || (int)tm.members.size() >= total)
                    tm.recruiting = false;
                else if (frame_ - tm.created > 900) tm.st.done = true;
                if (tm.recruiting) continue;
            }
            if (tm.script < 0) { tm.st.done = true; continue; }
            const ra2engine::MapScript& sc = teams_data_.scripts[(std::size_t)tm.script];
            ra2engine::MapTeamView v;
            v.live_members = (int)tm.members.size();
            bool all_idle = true, all_close = true;
            for (int m : tm.members) {
                const ra2engine::Unit& u = w_.units()[(std::size_t)m];
                if (u.moving() || u.target >= 0 || u.target_building >= 0) all_idle = false;
                if (tm.goal_x >= 0) {
                    const double dx = u.cell_x - tm.goal_x, dy = u.cell_y - tm.goal_y;
                    if (dx * dx + dy * dy > 2.25 * 2.25) all_close = false;
                }
            }
            v.arrived = all_close || (all_idle && frame_ - tm.issued_frame > 2);
            v.members_idle = false;
            v.target_exists = true;
            if (!tm.members.empty() && tm.st.pc >= 0 && tm.st.pc < (int)sc.actions.size()) {
                const ra2engine::MapScriptAction& a = sc.actions[(std::size_t)tm.st.pc];
                if (a.action == ra2engine::kSaAttack || a.action == ra2engine::kSaAttackEnemyBuilding) {
                    bool isb = false;
                    const ra2engine::Unit& lead = w_.units()[(std::size_t)tm.members[0]];
                    v.target_exists = nearest_enemy(tm.house, lead.cell_x, lead.cell_y,
                                                    a.action == ra2engine::kSaAttack ? a.arg : 2,
                                                    isb) >= 0;
                } else if (a.action == ra2engine::kSaAttackWaypoint) {
                    int x = 0, y = 0;
                    v.target_exists = waypoint_cell(a.arg, x, y) &&
                                      (w_.building_at_cell(x, y) >= 0 || w_.unit_at_cell(x, y) >= 0);
                    v.members_idle = all_idle && frame_ - tm.issued_frame > 2;
                }
            }
            const ra2engine::MapTeamCommand c = ra2engine::step_map_team(tm.st, sc, v, frame_);
            execute_team(tm, c, all_idle);
        }
    }
    void execute_team(Team& tm, const ra2engine::MapTeamCommand& c, bool all_idle) {
        using K = ra2engine::MapTeamCommand::Kind;
        switch (c.kind) {
        case K::MoveWaypoint:
        case K::MoveCell: {
            if (!c.fresh) return;
            int x = -1, y = -1;
            if (c.kind == K::MoveCell) { x = c.arg % 128; y = c.arg / 128; }
            else if (c.action == ra2engine::kSaMoveEnemyBuilding ||
                     c.action == ra2engine::kSaGatherEnemy) {
                bool isb = false;
                const ra2engine::Unit& lead = w_.units()[(std::size_t)tm.members[0]];
                const int t = nearest_enemy(tm.house, lead.cell_x, lead.cell_y, 2, isb);
                if (t >= 0 && isb) { x = w_.buildings()[(std::size_t)t].cx; y = w_.buildings()[(std::size_t)t].cy; }
            } else if (c.action == ra2engine::kSaGatherBase) {
                const ra2engine::Unit& lead = w_.units()[(std::size_t)tm.members[0]];
                x = lead.cell_x;
                y = lead.cell_y;
            } else if (!waypoint_cell(c.arg, x, y)) {
                x = -1;
            }
            tm.goal_x = x;
            tm.goal_y = y;
            tm.issued_frame = frame_;
            if (x < 0) return;
            for (int m : tm.members)
                if (!w_.order_fixed_wing(m, -1, -1, x, y)) w_.order_unit_move(m, x, y);
            return;
        }
        case K::Attack:
        case K::AttackBuilding:
        case K::AttackWaypoint: {
            if (!c.fresh && !all_idle) return;
            tm.issued_frame = frame_;
            tm.goal_x = -1;
            for (int m : tm.members) {
                const ra2engine::Unit& u = w_.units()[(std::size_t)m];
                if (!c.fresh && (u.target >= 0 || u.target_building >= 0 || u.moving())) continue;
                bool isb = false;
                int t = -1;
                if (c.kind == K::AttackWaypoint) {
                    // [DEVIATION §9.7, narrowed by W2-CA-11] An attack-waypoint
                    // bombardment of an object whose tag guards the player's
                    // LOSS (Allied 1's dreadnought/V3 shelling of the Statue)
                    // relies on Patriot interception, which the engine does
                    // not model; that one is held. Every other attack-waypoint
                    // (artillery, building assaults) executes literally.
                    int wx = 0, wy = 0;
                    if (!waypoint_cell(c.arg, wx, wy)) continue;
                    const int tb = w_.building_at_cell(wx, wy);
                    const int tu = tb < 0 ? w_.unit_at_cell(wx, wy) : -1;
                    const int tag = tb >= 0 && tb < (int)btrack_.size() ? btrack_[(std::size_t)tb].tag
                                  : tu >= 0 && tu < (int)utrack_.size() ? utrack_[(std::size_t)tu].tag : -1;
                    if (tag >= 0 && tag_guards_loss(tag)) continue;
                    if (tb >= 0) { t = tb; isb = true; }
                    else if (tu >= 0) { t = tu; isb = false; }
                } else {
                    t = nearest_enemy(tm.house, u.cell_x, u.cell_y,
                                      c.kind == K::Attack ? c.arg : 2, isb);
                }
                if (t >= 0) order_attack(m, t, isb);
            }
            return;
        }
        case K::Mission:
            if (c.arg == ra2engine::kMissionHunt)
                for (int m : tm.members) utrack_[(std::size_t)m].hunt = true;
            return;
        case K::Win: flag_end(1); return;
        case K::Lose: flag_end(2); return;
        case K::SetGlobal: sys_->set_global(c.arg, true); return;
        case K::ClearGlobal: sys_->set_global(c.arg, false); return;
        case K::SetLocal: sys_->set_local(c.arg, true); return;
        case K::ClearLocal: sys_->set_local(c.arg, false); return;
        case K::ChangeHouse: {
            const int eh = engine_house(c.arg);
            if (eh < 0) return;
            for (int m : tm.members) w_.units()[(std::size_t)m].house = eh;
            tm.st.done = true;
            return;
        }
        // W2-CA-17: Deploy (9) unfolds every member MCV (enemy MCVs too);
        // Unload (8) empties every member transport; Scatter (21) steps each
        // member to a nearby free cell [campaign.md §7].
        case K::Deploy:
            for (int m : tm.members)
                if (w_.is_mcv(m)) w_.order_deploy_mcv(m);
            return;
        case K::Unload:
            for (int m : tm.members) w_.order_unload(m);
            return;
        case K::Scatter:
            for (int m : tm.members) {
                const ra2engine::Unit& u = w_.units()[(std::size_t)m];
                int x = 0, y = 0;
                if (u.moving() || !free_cell_near(u.cell_x + 1, u.cell_y + 1, x, y, 2)) continue;
                w_.order_unit_move(m, x, y);
            }
            return;
        case K::DeleteMembers:
            for (int m : tm.members) w_.campaign_erase_unit(m);
            tm.members.clear();
            return;
        default:
            return;
        }
    }
    // W2-CA-11: does any trigger on this tag (its chain) lose the mission,
    // directly or by enabling a trigger that does?
    bool tag_guards_loss(int tag) const {
        if (tag < 0 || tag >= (int)data_.tags.size()) return false;
        auto loses = [&](int ti) {
            for (const ra2engine::MapAction& a : data_.triggers[(std::size_t)ti].actions)
                if (a.kind == 2) return true;
            return false;
        };
        int guard = 0;
        for (int ti = data_.tags[(std::size_t)tag].first_trigger;
             ti >= 0 && ti < (int)data_.triggers.size() && guard < 32;
             ti = data_.triggers[(std::size_t)ti].linked, ++guard) {
            if (loses(ti)) return true;
            for (const ra2engine::MapAction& a : data_.triggers[(std::size_t)ti].actions)
                if (a.kind == 53 && a.trigger >= 0 && a.trigger < (int)data_.triggers.size() && loses(a.trigger))
                    return true;
        }
        return false;
    }
    void step_hunters() {
        if (frame_ % 15 != 0) return;
        for (std::size_t i = 0; i < utrack_.size(); ++i) {
            const Track& t = utrack_[i];
            if (!t.hunt || t.dead) continue;
            const ra2engine::Unit& u = w_.units()[i];
            if (!live(u) || u.moving() || u.target >= 0 || u.target_building >= 0) continue;
            bool isb = false;
            const int tg = nearest_enemy(u.house, u.cell_x, u.cell_y, 1, isb);
            if (tg >= 0) order_attack((int)i, tg, isb);
        }
    }

    // --- outcome [RE §5] ------------------------------------------------------
    void flag_end(int o) {
        if (end_.outcome() != 0) return;
        end_.flag(o, frame_);
        char b[120];
        std::snprintf(b, sizeof b, "[camp] f=%ld outcome=%s borrowed=%d", frame_,
                      o == 1 ? "WON" : "LOST", end_.borrowed_frames());
        witness_.emplace_back(b);
    }

    // Trigger log -> witness; TextTrigger labels -> objectives; W2-CA-12
    // speech/sound/theme actions -> audio requests for the scene.
    void drain_log() {
        const std::vector<std::string>& lg = sys_->log();
        for (; logged_ < lg.size(); ++logged_) {
            const std::string& l = lg[logged_];
            witness_.push_back(l);
            {
                int k = 0, v = 0, wp = -1;
                char name[128] = {0};
                if (std::sscanf(l.c_str(), "  a%d %*s v=%d wp=%d %127s", &k, &v, &wp, name) == 4 &&
                    (k == 19 || k == 20 || k == 21 || k == 99) &&
                    l.find("(recorded)") != std::string::npos)
                    audio_.push_back({k, name[0] == '-' && name[1] == 0 ? std::string() : std::string(name), v, wp});
            }
            {   // W3-B: action 113 MakeHouseCheer -> that house's infantry cheer.
                int k = 0, v = 0;
                if (std::sscanf(l.c_str(), "  a%d %*s v=%d", &k, &v) == 2 && k == 113 &&
                    l.find("(recorded)") != std::string::npos) {
                    const int eh = engine_house(v);
                    if (eh >= 0) w_.make_house_cheer(eh);
                }
            }
            if (l.rfind("  a11 ", 0) == 0) {
                // "  a11 TextTrigger v=.. wp=.. LABEL (recorded)"
                const std::size_t e = l.rfind(" (recorded)");
                const std::size_t s = l.rfind(' ', e == std::string::npos ? std::string::npos : e - 1);
                if (e != std::string::npos && s != std::string::npos && s + 1 < e)
                    objectives_.push_back({frame_, camp::csf_text(dir_, l.substr(s + 1, e - s - 1))});
            }
        }
    }

    // --- the TriggerHost over this runtime ------------------------------------
    struct Host : ra2engine::TriggerHost {
        explicit Host(CampaignRuntime& r) : rt(r) {}
        CampaignRuntime& rt;
        bool house_exists(int ht) const override { return ht >= 0 && rt.engine_house(ht) >= 0; }
        std::vector<int> house_instances() const override { return rt.houses_order_ht_; }
        int player_house_type() const override { return rt.house_type(0); }
        int house_buildings(int ht) const override {
            const int eh = rt.engine_house(ht);
            int n = 0;
            for (const ra2engine::Building& b : rt.w_.buildings()) if (live(b) && b.house == eh) ++n;
            return n;
        }
        int house_vehicles(int ht) const override {
            const int eh = rt.engine_house(ht);
            int n = 0;
            for (const ra2engine::Unit& u : rt.w_.units())
                if (live(u) && u.house == eh && (u.is_vehicle || u.is_naval) && !u.is_aircraft) ++n;
            return n;
        }
        int house_naval_units(int ht) const override {
            const int eh = rt.engine_house(ht);
            int n = 0;
            for (const ra2engine::Unit& u : rt.w_.units())
                if (live(u) && u.house == eh && u.is_naval && !u.is_aircraft) ++n;
            return n;
        }
        int house_infantry(int ht) const override {
            const int eh = rt.engine_house(ht);
            int n = 0;
            for (const ra2engine::Unit& u : rt.w_.units())
                if (live(u) && u.house == eh && !u.is_vehicle && !u.is_naval && !u.is_aircraft) ++n;
            return n;
        }
        int house_building_type_count(int ht, int bt) const override {
            if (bt < 0 || bt >= (int)rt.data_.building_types.size()) return 0;
            const std::string id = camp::up(rt.data_.building_types[(std::size_t)bt]);
            const int eh = rt.engine_house(ht);
            int n = 0;
            for (const ra2engine::Building& b : rt.w_.buildings())
                if (live(b) && b.house == eh && b.type >= 0 &&
                    camp::up(rt.ba_.roster[(std::size_t)b.type].id) == id)
                    ++n;
            return n;
        }
        long house_credits(int ht) const override {
            const int eh = rt.engine_house(ht);
            return eh == 0 ? (long)rt.player_credits_ : (long)std::llround(rt.w_.house_credits(eh));
        }
        double house_power_ratio(int ht) const override {
            const int eh = rt.engine_house(ht);
            long out = 0, drain = 0;
            for (const ra2engine::Building& b : rt.w_.buildings()) {
                if (!live(b) || b.house != eh || b.type < 0) continue;
                const int p = rt.ba_.roster[(std::size_t)b.type].power;
                if (p > 0) out += p; else drain -= p;
            }
            return drain == 0 ? 1.0 : (double)out / (double)drain;
        }
        bool house_has_factory(int ht) const override {
            const int eh = rt.engine_house(ht);
            for (const ra2engine::Building& b : rt.w_.buildings())
                if (live(b) && b.house == eh && b.type >= 0 &&
                    !rt.rules_ini_.get(rt.ba_.roster[(std::size_t)b.type].id, "Factory", "").empty())
                    return true;
            return false;
        }
        int house_destroyed_buildings(int ht) const override {
            const int eh = rt.engine_house(ht);
            return eh >= 0 ? rt.destroyed_buildings_[(std::size_t)eh] : 0;
        }
        int house_destroyed_units(int ht) const override {
            const int eh = rt.engine_house(ht);
            return eh >= 0 ? rt.destroyed_units_[(std::size_t)eh] : 0;
        }
        int house_last_built(int ht, int cat) const override {
            const int eh = rt.engine_house(ht);
            return (eh >= 0 && cat >= 0 && cat < 4) ? rt.last_built_[(std::size_t)eh][(std::size_t)cat] : -1;
        }
        int techno_count(const std::string& id) const override {
            int n = 0;
            if (id.empty()) {
                for (const ra2engine::Building& b : rt.w_.buildings()) if (live(b)) ++n;
                for (const ra2engine::Unit& u : rt.w_.units()) if (live(u)) ++n;
                return n;
            }
            if (!rt.rules_ini_.has(id)) return -1;
            const std::string u = camp::up(id);
            for (std::size_t i = 0; i < rt.w_.buildings().size() && i < rt.btrack_.size(); ++i)
                if (live(rt.w_.buildings()[i]) && rt.btrack_[i].type == u) ++n;
            for (std::size_t i = 0; i < rt.w_.units().size() && i < rt.utrack_.size(); ++i)
                if (live(rt.w_.units()[i]) && rt.utrack_[i].type == u) ++n;
            return n;
        }
        std::string take_note() override {
            std::string n;
            n.swap(rt.note_);
            return n;
        }
        bool apply(const ra2engine::TriggerEffect& e) override { return rt.apply(e); }
        void present(const ra2engine::VisualRequest& v) override { rt.visuals_.push_back(v); }   // W3-FX
    };

    bool apply(const ra2engine::TriggerEffect& e) {
        switch (e.action) {
        case 4:
            return create_team(e.team, /*reinforce=*/false, -1);
        case 7: case 80: case 107:
            return create_team(e.team, /*reinforce=*/true, e.action == 7 ? -1 : e.waypoint);
        case 5: {   // DestroyTeam: the team objects go; members stay
            const int ti = teams_data_.find_team(e.team);
            int n = 0;
            for (std::size_t i = 0; i < teams_.size(); ++i)
                if (teams_[i].type == ti && !teams_[i].st.done) {
                    for (int m : teams_[i].members) utrack_[(std::size_t)m].team = -1;
                    teams_[i].st.done = true;
                    ++n;
                }
            return n > 0;
        }
        case 6: {   // AllToHunt
            const int eh = engine_house(e.house_type);
            for (std::size_t i = 0; i < utrack_.size(); ++i)
                if (!utrack_[i].dead && w_.units()[i].house == eh) utrack_[i].hunt = true;
            return eh >= 0;
        }
        case 16:
            w_.reveal_all_house(0);
            return true;
        case 17: {
            int x = 0, y = 0;
            if (!waypoint_cell(e.waypoint, x, y)) return false;
            w_.reveal_source(0, x, y, reveal_radius_);
            return true;
        }
        case 125: {
            const int bt = types_.building(ba_, e.text);
            const int eh = engine_house(e.house_type);
            int x = 0, y = 0;
            if (bt < 0 || eh < 0 || !waypoint_cell(e.waypoint, x, y)) return false;
            const ra2engine::BuildingTypeDef& d = ba_.roster[(std::size_t)bt];
            const ra2engine::PlacementCheck chk = ra2engine::placement_check(
                x, y, d.fw, d.fh, eh, d.adjacent, w_.passable(), w_.grid_w(), w_.grid_h(),
                w_.occupancy(), w_.buildings(), /*bypass=*/true);
            if (!chk.ok) return false;
            w_.refresh_building_types(ba_.roster);   // G16
            w_.add_building(bt, x, y, d.fw, d.fh, eh, d.strength, d.armor_ix, 0.0, true, d.sight,
                            d.grants_map_reveal);
            return true;
        }
        case 14: {   // ChangeHouse: every object whose tag holds the trigger (FUN_006e0010)
            const int eh = engine_house(e.house_type);
            if (eh < 0) return false;
            int n = 0;
            for (std::size_t i = 0; i < utrack_.size(); ++i)
                if (!utrack_[i].dead && utrack_[i].tag >= 0 &&
                    sys_->tag_has_trigger(utrack_[i].tag, e.trigger_type)) {
                    w_.units()[i].house = eh;
                    ++n;
                }
            for (std::size_t i = 0; i < btrack_.size(); ++i)
                if (!btrack_[i].dead && btrack_[i].tag >= 0 &&
                    sys_->tag_has_trigger(btrack_[i].tag, e.trigger_type)) {
                    w_.capture_building((int)i, eh, 0);
                    w_.buildings()[i].house = eh;
                    ++n;
                }
            char b[48];
            std::snprintf(b, sizeof b, "[changed=%d to=%d]", n, eh);
            note_ = b;
            return n > 0;
        }
        case 36: {   // AllChangeHouse (FUN_006e00d0)
            const int from = engine_house(e.trigger_house_type), to = engine_house(e.house_type);
            if (from < 0 || to < 0) return false;
            for (std::size_t i = 0; i < w_.units().size(); ++i)
                if (live(w_.units()[i]) && w_.units()[i].house == from) w_.units()[i].house = to;
            for (std::size_t i = 0; i < w_.buildings().size(); ++i)
                if (live(w_.buildings()[i]) && w_.buildings()[i].house == from) {
                    w_.capture_building((int)i, to, 0);
                    w_.buildings()[i].house = to;
                }
            return true;
        }
        case 37: case 38: {   // MakeAlly / MakeEnemy (Execute 0x25/0x26)
            // W2-CA-10 [RE FUN_004f9990/FUN_004f9db0]: pairwise and mutual for
            // the two houses only -- house 0 is never moved onto the enemy team.
            const int a = engine_house(e.trigger_house_type), b = engine_house(e.house_type);
            if (a < 0 || b < 0 || a == b) return false;
            for (int h : {a, b})
                if (e.action == 37 && !w_.house_combatant(h)) {
                    // A civilian-side house that is allied becomes a combatant
                    // on its own (it has no team of its own to bring along).
                    ra2engine::World::HouseRec r;
                    r.side = w_.house_side(h);
                    r.color_ix = w_.house_color(h);
                    r.country = map_.get(house_names_[(std::size_t)h], "Country", "");
                    r.combatant = true;
                    r.alliance_team = 200 + h;
                    r.player_control = w_.controllable(h) && h != 0;
                    w_.set_house(h, r);
                }
            w_.set_pair_alliance(a, b, e.action == 37);
            return true;
        }
        case 32: {   // DestroyAttachedObject (FUN_006e15c0)
            int n = 0;
            for (std::size_t i = 0; i < utrack_.size(); ++i)
                if (!utrack_[i].dead && utrack_[i].tag >= 0 &&
                    sys_->tag_has_trigger(utrack_[i].tag, e.trigger_type) && live(w_.units()[i])) {
                    w_.campaign_kill_unit((int)i);
                    ++n;
                }
            for (std::size_t i = 0; i < btrack_.size(); ++i)
                if (!btrack_[i].dead && btrack_[i].tag >= 0 &&
                    sys_->tag_has_trigger(btrack_[i].tag, e.trigger_type) && live(w_.buildings()[i])) {
                    w_.campaign_damage_building((int)i, w_.buildings()[i].hp);
                    ++n;
                }
            return n > 0;
        }
        case 60:      // W2-CA-13 SellBuilding: every tagged building of the trigger
        case 111: {   // EvictOccupiers: its garrison steps out [map-triggers.md §5]
            int n = 0;
            for (std::size_t i = 0; i < btrack_.size(); ++i)
                if (!btrack_[i].dead && btrack_[i].tag >= 0 &&
                    sys_->tag_has_trigger(btrack_[i].tag, e.trigger_type) && live(w_.buildings()[i])) {
                    if (e.action == 60) {
                        if (w_.begin_sell((int)i, w_.buildings()[i].house)) ++n;
                    } else if (!w_.buildings()[i].occupants.empty()) {
                        w_.unload_occupants((int)i);
                        ++n;
                    }
                }
            return n > 0;
        }
        case 108: {   // CreateCrate: Value = [Powerups] index; 20 = untyped (random)
            int x = 0, y = 0;
            if (!waypoint_cell(e.waypoint, x, y)) return false;
            return w_.place_crate(x, y, e.value == 20 ? -1 : e.value);
        }
        case 101: {   // ReshroudMapAtWaypoint, radius [General] RevealTriggerRadius
            int x = 0, y = 0;
            if (!waypoint_cell(e.waypoint, x, y)) return false;
            w_.reshroud_area(0, x, y, reveal_radius_);
            return true;
        }
        case 9: {   // FireSale [RE-FALLBACK: the original flips the house's AI state
                    // to 4 (+0x250) and its AI sells; the clone sells every building
                    // of the house at once through the real Sell mission]
            const int eh = engine_house(e.house_type);
            if (eh < 0) return false;
            int n = 0;
            for (int i = 0; i < (int)w_.buildings().size(); ++i)
                if (live(w_.buildings()[(std::size_t)i]) && w_.buildings()[(std::size_t)i].house == eh &&
                    w_.begin_sell(i, eh))
                    ++n;
            return n > 0;
        }
        case 90: case 102: {   // W3-FX LightningStrikeAt / LightningStorm (World)
            int x = 0, y = 0;
            if (!waypoint_cell(e.waypoint, x, y)) return false;
            const int eh = std::max(0, engine_house(e.house_type));
            return e.action == 90 ? w_.trigger_lightning_strike(eh, x, y)
                                  : w_.trigger_lightning_storm(eh, x, y);
        }
        case 42: {   // W3-FX DoExplosionAt [RE: FUN_006e1900]
            int x = 0, y = 0;
            if (!waypoint_cell(e.waypoint, x, y)) return false;
            const std::string wpn = ra2_weapon_by_index(e.value);
            if (wpn.empty()) return false;
            const int dmg = std::atoi(camp::trim_comment(ra2_rules_.get(wpn, "Damage", "0")).c_str());
            // [DEVIATION, as 63: the waypoint cell's objects take the weapon's
            // Damage; the warhead's CellSpread falloff is not applied.]
            const int u = w_.unit_at_cell(x, y);
            if (u >= 0 && dmg > 0) w_.campaign_damage_unit(u, dmg);
            const int b = w_.building_at_cell(x, y);
            if (b >= 0 && dmg > 0) w_.campaign_damage_building(b, dmg);
            ra2engine::VisualRequest v;   // the warhead's impact anim (presentation)
            v.action = 42;
            v.value = dmg;
            v.waypoint = e.waypoint;
            v.text = camp::trim_comment(ra2_rules_.get(wpn, "Warhead", ""));
            visuals_.push_back(v);
            return true;
        }
        case 63: {   // Apply100Damage [DEVIATION §9.6: the waypoint cell, no splash]
            int x = 0, y = 0;
            if (!waypoint_cell(e.waypoint, x, y)) return false;
            const int u = w_.unit_at_cell(x, y);
            if (u >= 0) w_.campaign_damage_unit(u, 100);
            const int b = w_.building_at_cell(x, y);
            if (b >= 0) w_.campaign_damage_building(b, 100);
            return u >= 0 || b >= 0;
        }
        default:
            return false;
        }
    }

    camp::Launch launch_;
    std::string dir_;
    ra2engine::World& w_;
    Assets& a_;
    BuildingAssets& ba_;
    const Ini& rules_ini_;
    Ini map_;
    Ini ra2_rules_;
    ra2engine::ScenarioBasic basic_;
    camp::Types types_;
    ra2engine::MapTriggerData data_;
    ra2engine::MapTeamData teams_data_;
    ra2engine::MissionEnd end_;
    Host host_;
    std::unique_ptr<ra2engine::MapTriggerSystem> sys_;
    std::vector<std::string> house_names_;
    std::vector<int> house_type_of_;
    std::vector<int> houses_order_ht_;
    std::vector<std::vector<ra2engine::PlannedNode>> base_nodes_;   // [G-69] per engine house
    std::vector<std::string> ra2_vehicles_, ra2_infantry_, ra2_aircraft_;
    std::vector<int> destroyed_units_, destroyed_buildings_;
    std::vector<std::array<int, 4>> last_built_;
    std::vector<Track> utrack_, btrack_;
    std::vector<Team> teams_;
    std::vector<Objective> objectives_;
    std::vector<std::string> witness_;
    const UnitRoster* ai_roster_ = nullptr;   // W2-CA-07 (GameScene's ur_ai_)
    std::map<std::string, bool> mc_type_;      // W2-CA-15 cache: type -> MindControl weapon
    std::vector<std::uint8_t> armed_;         // W2-CA-07 per house: 1 base, 2 map teams (derived)
    std::vector<AudioReq> audio_;   // W2-CA-12 (presentation only, not hashed/saved)
    std::vector<ra2engine::VisualRequest> visuals_;   // W3-FX (presentation only, not hashed/saved)
    std::vector<std::string> ra2_weapons_;            // W3-FX DoExplosionAt index space (lazy)
    std::string note_;
    std::size_t logged_ = 0;
    ra2engine::World::Side player_side_ = ra2engine::World::Side::Allied;
    double player_credits_ = 0.0;
    double accum_ = 0.0;
    long frame_ = 0;
    int reveal_radius_ = 9;
    int placed_b_ = 0, placed_u_ = 0, skipped_ = 0;
    int teams_created_ = 0, raises_ = 0;
    bool placing_ = false;
};

// The World a campaign GameScene starts from: the plain map world (no
// pre-spawned squads) with the water grid; the CampaignRuntime then configures
// houses and places the map's own objects.
ra2engine::World build_campaign_world(Assets& a, UnitAssets& ua) {
    load_progress("CAMPAIGN WORLD");   // W2-AR-02 loading-screen checkpoint
    ra2engine::World world = build_world(a, ua, 0);
    const int gw = world.grid_w(), gh = world.grid_h();
    // Gap fix C (G-43): water grid + ground blocked on water/cliffs.
    (void)gw; (void)gh;
    depth_set_terrain(world, a);   // G16 (agent G): + low bridges [RE: world-depth.md §B]
    return world;
}

// Scene factories the campaign GameScene transitions through; defined in
// campaign_scenes.inc (after the shell scenes exist).
Scene* make_campaign_result_scene(bool no_audio, const ra2engine::SkirmishConfig& cfg, int outcome,
                                  long frames);
Scene* make_main_menu_scene(bool no_audio);   // nullptr when no shell is running
