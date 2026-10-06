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
        MixFile ex = MixFile::open(ra2assets::read_file(dir + "expand01.mix"));
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
    MixFile maps = MixFile::open(ra2assets::read_file(dir + mix_for_map(map)));
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
            MixFile lang = MixFile::open(ra2assets::read_file(dir + "language.mix"));
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

// Map a [<Side>] mapsel section to the World side.
ra2engine::World::Side side_of(const std::string& s) {
    const std::string u = up(s);
    if (u == "GDI") return ra2engine::World::Side::Allied;
    if (u == "NOD") return ra2engine::World::Side::Soviet;
    return ra2engine::World::Side::Neutral;
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
                t.combat.speed_cps = ru.speed * kCellsPerSecPerSpeed;
                const std::string& img = t.combat.image;
                if (!ua_.shps.count(img)) {
                    MixFile conquer = ra2_.open_nested("conquer.mix");
                    ua_.shps.emplace(img, load_unit_shp(conquer, img));
                    ua_.anims.emplace(img, anim_from_art(art_, img));
                }
                t.ok = true;
            } else if (index_of(vehicles_, id) >= 0) {
                auto va = std::make_unique<VehicleAssets>(load_vehicle_assets(dir_, ra2_, ua_.pal, id));
                if (va->ok) {
                    t.combat = va->combat;
                    t.vehicle = true;
                    t.ok = true;
                    vas_.push_back(std::move(va));
                }
            }
            // Aircraft: no campaign placement path yet (none in the two first
            // missions) -- logged as skipped by the caller.
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
        d.crewed = rules_.section_flag(id, "Crewed", false);
        BuildingArt slot;
        for (const auto& c : ra2assets::theater_candidates(art_id, 'T', "isotemp.mix",
                                                           "temperat.mix", ".shp")) {
            try {
                MixFile m = ra2_.open_nested(c.mix);
                if (const MixEntry* e = m.find(c.file)) {
                    slot.shp = ShpFile::load(m.read(*e));
                    slot.ok = true;
                    break;
                }
            } catch (const std::exception&) {}
        }
        ba.roster.push_back(d);
        ba.art.push_back(std::move(slot));
        ba.defense.push_back(rules_.is_buildcat_combat(id) ? 1 : 0);
        // Keep the P6 per-type vectors index-aligned (inert flags).
        ba.wall.push_back(0);
        ba.naval.push_back(0);
        ba.water_bound.push_back(0);
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
        reveal_radius_ = rules_ini.get_int("General", "RevealTriggerRadius", 9);
        setup_houses();
        // Every type the mission can place, resolved up front (so the render
        // list is complete before the first frame).
        const std::vector<ra2engine::MapObjectRec> objs = ra2engine::parse_map_objects(map_);
        for (const ra2engine::MapObjectRec& o : objs)
            if (o.kind == 0) types_.building(ba_, o.type);
            else if (o.kind != 3) types_.unit(o.type);
        for (const auto& tf : teams_data_.taskforces)
            for (const ra2engine::MapTaskMember& m : tf.second) types_.unit(m.type);
        w_.set_building_types(ba_.roster);
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
    const std::vector<Objective>& objectives() const { return objectives_; }
    const std::vector<std::string>& witness() const { return witness_; }
    std::vector<const VehicleAssets*> vehicle_assets() const { return types_.vehicle_assets(); }
    double player_credits() const { return player_credits_; }
    ra2engine::World::Side player_side() const { return player_side_; }
    const ra2engine::MapTriggerSystem& triggers() const { return *sys_; }
    int teams_created() const { return teams_created_; }
    int house_index(const std::string& name) const { return engine_house_of_name(name); }
    int raises() const { return raises_; }

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
            const std::string side = camp::up(camp::trim_comment(map_.get(country, "Side", "")));
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
            if (rec.side == ra2engine::World::Side::Neutral && (i == 0 || lists(player, hn))) {
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
        destroyed_units_.assign(house_names_.size(), 0);
        destroyed_buildings_.assign(house_names_.size(), 0);
        last_built_.assign(house_names_.size(), {-1, -1, -1, -1});
    }
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
    int place_unit(const std::string& type, int eh, int x, int y, int health, const std::string& tag,
                   const std::string& mission) {
        const camp::UnitType* ut = types_.unit(type);
        if (!ut) return -1;
        int cx = 0, cy = 0;
        if (!free_cell_near(x, y, cx, cy)) return -1;
        w_.add_combat_unit(cx, cy, eh, ut->combat);
        const int idx = (int)w_.units().size() - 1;
        ra2engine::Unit& u = w_.units()[(std::size_t)idx];
        if (health > 0) u.hp = ra2engine::campaign_strength(u.max_hp, health, false);
        if (ua_anims_has(u.image)) {}
        placing_ = true;
        sync_tracks();
        placing_ = false;
        Track& t = utrack_[(std::size_t)idx];
        t.type = camp::up(type);
        t.initial = true;
        t.hp = u.hp;
        t.tag = tag.empty() ? -1 : data_.find_tag(tag);
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
                const ra2engine::PlacementCheck chk = ra2engine::placement_check(
                    o.x, o.y, d.fw, d.fh, eh, d.adjacent, w_.passable(), w_.grid_w(), w_.grid_h(),
                    w_.occupancy(), w_.buildings(), /*bypass=*/true);
                if (!chk.ok) { ++skipped_; continue; }   // Unlimbo failed -> deleted [RE §4]
                const int idx = w_.add_building(bt, o.x, o.y, d.fw, d.fh, eh, d.strength, d.armor_ix,
                                                0.0, /*instant=*/true, d.sight, d.grants_map_reveal);
                if (idx < 0) { ++skipped_; continue; }
                ra2engine::Building& b = w_.buildings()[(std::size_t)idx];
                b.hp = ra2engine::campaign_strength(d.strength, o.health, true);
                placing_ = true;
                sync_tracks();
                placing_ = false;
                Track& t = btrack_[(std::size_t)idx];
                t.tag = o.tag.empty() ? -1 : data_.find_tag(o.tag);
                t.hp = b.hp;
                ++placed_b_;
            } else if (o.kind == 3) {
                ++skipped_;   // aircraft: no campaign placement path (cut, logged)
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
    void poll_objects() {
        sync_tracks();
        for (std::size_t i = 0; i < utrack_.size(); ++i) {
            Track& t = utrack_[i];
            if (t.dead) continue;
            const ra2engine::Unit& u = w_.units()[i];
            if (!live(u)) {
                t.dead = true;
                if (u.house >= 0 && u.house < (int)destroyed_units_.size())
                    ++destroyed_units_[(std::size_t)u.house];
                raise_obj(t, ra2engine::kEvDestroyedByAnything, u.house);
                raise_obj(t, ra2engine::kEvDestroyedByAnybody, u.house);
                if (t.tag >= 0) { sys_->detach_tag(t.tag); t.tag = -1; }
                continue;
            }
            if (u.cell_x != t.cx || u.cell_y != t.cy) {
                t.cx = u.cell_x;
                t.cy = u.cell_y;
                const ra2engine::RaiseContext c = ctx_for(u.house);
                if (sys_->raise_cell(t.cx, t.cy, ra2engine::kEvEnteredBy, c, host_)) ++raises_;
                sys_->raise_cell(t.cx, t.cy, ra2engine::kEvEnteredOrOverflownBy, c, host_);
            }
            if (u.hp < t.hp) raise_obj(t, ra2engine::kEvAttackedByAnybody, u.house);
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
                raise_obj(t, ra2engine::kEvDestroyedByAnything, b.house);
                raise_obj(t, ra2engine::kEvDestroyedByAnybody, b.house);
                if (t.tag >= 0) { sys_->detach_tag(t.tag); t.tag = -1; }
                continue;
            }
            if (b.hp < t.hp) raise_obj(t, ra2engine::kEvAttackedByAnybody, b.house);
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
            if (t.dead || t.team >= 0) continue;
            const ra2engine::Unit& u = w_.units()[i];
            if (!live(u) || u.house != tm.house || u.moving()) continue;
            for (std::size_t k = 0; k < types.size(); ++k)
                if (need[k] > 0 && t.type == types[k]) {
                    --need[k];
                    tm.members.push_back((int)i);
                    any = true;
                    break;
                }
        }
        return any;
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
            if (const auto* tf = teams_data_.taskforce(tt.taskforce))
                for (const ra2engine::MapTaskMember& m : *tf)
                    for (int k = 0; k < m.count; ++k) {
                        const int idx = place_unit(m.type, tm.house, x, y, 256, std::string(), "Guard");
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
        if (building) w_.order_unit_attack_building(m, target);
        else w_.order_unit_attack(m, target);
    }
    void step_teams() {
        for (std::size_t ti = 0; ti < teams_.size(); ++ti) {
            Team& tm = teams_[ti];
            if (tm.st.done) continue;
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
            for (int m : tm.members) w_.order_unit_move(m, x, y);
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
                    // [DEVIATION §9.7] Attack-waypoint bombardments (Allied 1's
                    // dreadnought/V3 shelling of the Statue) rely on Patriot
                    // missile interception, which the engine does not model;
                    // executed literally they destroy the objective in seconds.
                    // The team holds position instead (logged in the witness).
                    continue;
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
        case K::DeleteMembers:
            for (int m : tm.members) w_.campaign_erase_unit(m);
            tm.members.clear();
            return;
        default:
            return;
        }
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

    // Trigger log -> witness; TextTrigger labels -> objectives.
    void drain_log() {
        const std::vector<std::string>& lg = sys_->log();
        for (; logged_ < lg.size(); ++logged_) {
            const std::string& l = lg[logged_];
            witness_.push_back(l);
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
            w_.set_building_types(ba_.roster);
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
        case 37: case 38: {   // MakeAlly / MakeEnemy, both directions (Execute 0x25/0x26)
            const int a = engine_house(e.trigger_house_type), b = engine_house(e.house_type);
            if (a < 0 || b < 0 || a == b) return false;
            ra2engine::World::HouseRec rb;
            rb.side = w_.house_side(b);
            rb.color_ix = w_.house_color(b);
            rb.country = map_.get(house_names_[(std::size_t)b], "Country", "");
            rb.combatant = true;
            // [DEVIATION: symmetric team model] ally = join a's team; enemy =
            // leave it for the other combat team.
            rb.alliance_team = e.action == 37 ? w_.house_team(a) : (w_.house_team(a) == 0 ? 1 : 0);
            w_.set_house(b, rb);
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
    std::vector<std::string> ra2_vehicles_, ra2_infantry_;
    std::vector<int> destroyed_units_, destroyed_buildings_;
    std::vector<std::array<int, 4>> last_built_;
    std::vector<Track> utrack_, btrack_;
    std::vector<Team> teams_;
    std::vector<Objective> objectives_;
    std::vector<std::string> witness_;
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
    ra2engine::World world = build_world(a, ua, 0);
    const int gw = world.grid_w(), gh = world.grid_h();
    world.set_water_terrain(resolve_land_types(a.map, a.theater, gw, gh));
    return world;
}
