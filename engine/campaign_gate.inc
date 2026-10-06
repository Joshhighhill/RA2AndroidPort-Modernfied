// --demo-campaign: the P10 Campaign gate [RE: docs/re/campaign.md].
// Textually included by main.cpp after run_demo_triggers. Loads Allied 1
// (ALL01t) and Soviet 1 (SOV01t) through the REAL campaign path
// (CampaignRuntime on a World: houses, map objects + tags, the map's own
// triggers in campaign mode, map teams, object/cell event polling) and runs a
// fixed frame budget with a scripted player: every kCampScriptEvery frames each
// idle player unit is ordered toward the next player-relevant cell-tag. Rendered
// (hidden window + screenshots) then headless from a FRESH World; byte-compares
// the witness + campaign_hash. NO hardcoded hash literal.

struct DemoCampaignResult { std::vector<std::string> witness; uint64_t hash = 0; bool ok = true; };

constexpr long kCampFrames = 1800;       // 2 game-minutes per mission at 15 Hz
constexpr long kCampScriptEvery = 150;   // scripted player order cadence (10 s)

// One gate scenario. `kill_owner` non-empty = the VICTORY run: no scripted
// walker; at kCampKillFrame every live building of that house whose type id
// starts with `kill_prefix` is destroyed through the World's damage path, so
// the mission's OWN victory triggers must carry it to WON (and the mapsel
// chain must name the next mission).
constexpr long kCampKillFrame = 150;
DemoCampaignResult simulate_campaign(const std::string& map, const std::string& side, bool render,
                                     SDL_Renderer* ren, ra2engine::TextureCache* tc, int W, int H,
                                     const std::string& start_png, const std::string& end_png,
                                     const std::string& kill_owner = "",
                                     const std::string& kill_prefix = "") {
    DemoCampaignResult r;
    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    MapSelection sel;
    sel.mix = camp::mix_for_map(map);
    sel.name = camp::lower(map);
    Assets a = load_assets(dir, ra2, sel);
    UnitAssets ua = load_unit_assets(dir, ra2);
    ra2assets::MixFile language = ra2assets::MixFile::open(ra2assets::read_file(dir + "language.mix"));
    Ini rules_ini = load_ini_from_ra2md(dir, "rulesmd.ini");
    Rules rules = Rules::load(rules_ini);
    Ini art_ini = load_ini_from_ra2md(dir, "artmd.ini");
    Art art = Art::load(art_ini);
    BuildingAssets ba = load_building_assets(ra2, language, rules, art);

    ra2engine::World world = build_campaign_world(a, ua);
    camp::Launch launch;
    launch.map = map;
    launch.side = side;
    launch.difficulty = 1;   // Normal (map-triggers.md §3.7)
    CampaignRuntime rt(launch, dir, ra2, language, world, a, ua, ba, rules, art, rules_ini);
    for (const auto& [image, seq] : ua.anims) world.set_anim(image, seq);
    std::vector<const VehicleAssets*> vlist = rt.vehicle_assets();
    ra2engine::Camera cam = centered_camera(a.map, W, H);
    // Start the camera on the player's first unit (where the mission opens).
    for (const ra2engine::Unit& u : world.units())
        if (u.house == 0) {
            const double sx = (u.fx - u.fy) * 30.0, sy = (u.fx + u.fy) * 15.0;
            cam.x = W / 2.0 - sx;
            cam.y = H / 2.0 - sy;
            break;
        }

    auto render_frame = [&]() {
        if (!render) return;
        SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
        SDL_RenderClear(ren);
        render_world(ren, *tc, a, ua, world, cam, W, H, nullptr, &vlist, &ba);
    };
    // Scripted player: the cell-tag cells (the mission's own trip-wires) in file
    // order; idle player units walk to the next one.
    std::vector<std::pair<int, int>> goals;
    for (const auto& kv : rt.triggers().data().cell_tags) goals.emplace_back(kv.x, kv.y);
    std::size_t goal_i = 0;
    const double dt = 1.0 / 15.0;
    for (long f = 0; f < kCampFrames && !rt.finished(); ++f) {
        rt.frame();
        if (!kill_owner.empty() && f == kCampKillFrame) {
            const int kh = rt.house_index(kill_owner);
            int killed = 0;
            for (std::size_t i = 0; i < world.buildings().size(); ++i) {
                const ra2engine::Building& b = world.buildings()[i];
                if (b.house != kh || !b.alive || b.state == ra2engine::Building::State::Dead) continue;
                if (camp::up(ba.roster[(std::size_t)b.type].id).rfind(kill_prefix, 0) != 0) continue;
                world.campaign_damage_building((int)i, b.hp);
                ++killed;
            }
            char kb[96];
            std::snprintf(kb, sizeof kb, "[camp] f=%ld scripted kill house=%d killed=%d", f, kh, killed);
            r.witness.emplace_back(kb);
        }
        if (kill_owner.empty() && f % kCampScriptEvery == kCampScriptEvery - 1 && !goals.empty()) {
            const std::pair<int, int> g = goals[goal_i % goals.size()];
            goal_i += 7;   // stride through the trip-wires
            for (std::size_t i = 0; i < world.units().size(); ++i) {
                const ra2engine::Unit& u = world.units()[i];
                if (u.house != 0 || !u.alive || u.die_time >= 0.0 || u.moving()) continue;
                world.order_unit_move((int)i, g.first, g.second);
            }
        }
        world.step(dt);
        if (f == 0) { render_frame(); if (render) capture_png(ren, start_png, W, H); }
    }
    {
        std::vector<std::string> w = rt.witness();
        w.insert(w.end(), r.witness.begin(), r.witness.end());
        r.witness.swap(w);
    }
    if (!kill_owner.empty()) {
        const ra2engine::CampaignTables tables = camp::load_tables(ra2);
        const std::string next = tables.next_scenario(side, map);
        char nb[160];
        std::snprintf(nb, sizeof nb, "[camp] victory-run %s outcome=%d finished=%d next=%s", map.c_str(),
                      rt.outcome(), rt.finished() ? 1 : 0, next.c_str());
        r.witness.emplace_back(nb);
        if (rt.outcome() != 1 || !rt.finished()) {
            r.ok = false;
            r.witness.emplace_back("[camp] WEDGE: scripted victory did not reach WON");
        }
    }
    int pu = 0, pb = 0;
    for (const ra2engine::Unit& u : world.units()) if (u.house == 0 && u.alive && u.die_time < 0) ++pu;
    for (const ra2engine::Building& b : world.buildings())
        if (b.house == 0 && b.alive && b.state != ra2engine::Building::State::Dead) ++pb;
    {
        char b[320];
        std::snprintf(b, sizeof b,
                      "[camp] end %s frame=%ld fired=%d logic=%d house=%d raises=%d teams=%d "
                      "objectives=%d outcome=%d player_units=%d player_buildings=%d units=%d buildings=%d",
                      map.c_str(), rt.frame_count(), rt.triggers().fired_count(),
                      rt.triggers().logic_fired(), rt.triggers().house_fired(), rt.raises(),
                      rt.teams_created(), (int)rt.objectives().size(), rt.outcome(), pu, pb,
                      (int)world.units().size(), (int)world.buildings().size());
        r.witness.emplace_back(b);
    }
    uint64_t h = world_hash(world);
    h = fnv_mix(h, rt.hash());
    r.hash = h;
    // Wedge guards: the gate can never pass vacuously.
    if (rt.triggers().fired_count() < 1) { r.ok = false; r.witness.emplace_back("[camp] WEDGE: no trigger fired"); }
    if (rt.teams_created() < 1) { r.ok = false; r.witness.emplace_back("[camp] WEDGE: no team created"); }
    if (rt.raises() < 1) { r.ok = false; r.witness.emplace_back("[camp] WEDGE: no object/cell raise"); }
    if (world.buildings().empty()) { r.ok = false; r.witness.emplace_back("[camp] WEDGE: no map structure placed"); }
    render_frame();
    if (render) capture_png(ren, end_png, W, H);
    return r;
}

int run_demo_campaign() {
    const int W = 1280, H = 720;
    const std::string scratch =
        "C:\\Users\\Omar\\AppData\\Local\\Temp\\claude\\d--Red-Alert-2\\"
        "7fdac188-b617-4ae7-b05d-0ba2d5efcda9\\scratchpad\\";
    bool all_ok = true;
    uint64_t total = 0xcbf29ce484222325ull;
    std::size_t lines = 0;
    struct Scn { const char* map; const char* side; const char* kill_owner; const char* kill_prefix; };
    // Allied 1: victory = DestroyedBuildingsAll(BadGuy1). Soviet 1: victory =
    // the four Pentagon sections (CAWA2A..D, Civie1) gone -> locals 1..4.
    const Scn missions[] = {{"ALL01t.MAP", "GDI", "", ""},
                            {"SOV01t.MAP", "Nod", "", ""},
                            {"ALL01t.MAP", "GDI", "BadGuy1 House", ""},
                            {"SOV01t.MAP", "Nod", "Civie1 House", "CAWA2"}};
    int scn_i = 0;
    // Dev aid: RA2_CAMP_ONLY=<n> runs just scenario n (the gate run leaves it unset).
    const char* only = std::getenv("RA2_CAMP_ONLY");
    for (const Scn& sc : missions) {
        if (only && std::atoi(only) != scn_i) { ++scn_i; continue; }
        const std::string map = sc.map, side = sc.side;
        const std::string tagname = camp::lower(map).substr(0, 6) + "_" + std::to_string(scn_i++);
        DemoCampaignResult r1;
        {
            if (SDL_Init(SDL_INIT_VIDEO) != 0) throw std::runtime_error(SDL_GetError());
            SDL_Window* win = SDL_CreateWindow("RA2 Engine (demo-campaign)", SDL_WINDOWPOS_CENTERED,
                                               SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
            if (!win) throw std::runtime_error(SDL_GetError());
            SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
            if (!ren) throw std::runtime_error(SDL_GetError());
            {
                ra2engine::TextureCache tc(ren);
                r1 = simulate_campaign(map, side, true, ren, &tc, W, H,
                                       scratch + "demo_campaign_" + tagname + "_start.png",
                                       scratch + "demo_campaign_" + tagname + "_end.png",
                                       sc.kill_owner, sc.kill_prefix);
            }
            SDL_DestroyRenderer(ren);
            SDL_DestroyWindow(win);
            SDL_Quit();
        }
        const DemoCampaignResult r2 = simulate_campaign(map, side, false, nullptr, nullptr, W, H, "", "",
                                                        sc.kill_owner, sc.kill_prefix);
        const bool identical = (r1.witness == r2.witness) && (r1.hash == r2.hash);
        std::cerr << "[demo-campaign] " << map << " run1 lines=" << r1.witness.size()
                  << " run2 lines=" << r2.witness.size() << " ok=" << (r1.ok ? "Y" : "n") << "/"
                  << (r2.ok ? "Y" : "n") << " identical=" << (identical ? "Y" : "n") << "\n";
        for (const std::string& s : r2.witness) std::cout << s << "\n";
        all_ok = all_ok && identical && r1.ok && r2.ok;
        total = fnv_mix(total, r2.hash);
        lines += r2.witness.size();
    }
    char th[64];
    std::snprintf(th, sizeof th, "%016llx", (unsigned long long)total);
    std::cout << "[demo-campaign] deterministic=" << (all_ok ? "yes" : "no") << " lines=" << lines
              << " campaign_hash=" << th << "\n";
    return all_ok ? 0 : 1;
}
