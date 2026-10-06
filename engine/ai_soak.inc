// ============================================================================
// --ai-soak : the REAL-PLAY AI acceptance harness [G-60..G-67].
//
// Builds the World exactly the way GameScene does for a skirmish (launch_skirmish
// -> the economy firewall -> projectiles -> abilities -> arm_skirmish_ai ->
// P5), then runs N game minutes headless at the fixed dt = 1/15 with a PASSIVE
// human (house 0 never gets an order) and reports, once per game minute, what
// the AI did: structures standing, credits, units fielded, teams created,
// triggers fired, damage dealt to the human's base. Optional PNG of the human
// base at the end. NOT a byte-identity gate (no pinned hash); it is the
// acceptance evidence that the AI builds, trains, forms teams and attacks.
//
//   ra2engine --ai-soak [--minutes N] [--diff easy|normal|brutal]
//                       [--ai-side soviet|allied] [--map <file.mmx>] [--png <path>]
// ============================================================================

int run_ai_soak(int argc, char** argv) {
    int minutes = 15;
    ra2engine::AiDifficulty diff = ra2engine::AiDifficulty::Normal;
    bool ai_allied = false;
    std::string map_file, png;
    int damage_ai_at = -1;   // G-65 probe: halve every AI building's HP at this minute
    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? std::string(argv[++i]) : ""; };
        if (a == "--minutes") minutes = std::max(1, std::atoi(next().c_str()));
        else if (a == "--diff") {
            const std::string d = next();
            diff = d == "easy" ? ra2engine::AiDifficulty::Easy
                 : d == "brutal" || d == "hard" ? ra2engine::AiDifficulty::Difficult
                 : ra2engine::AiDifficulty::Normal;
        } else if (a == "--ai-side") ai_allied = next() == "allied";
        else if (a == "--map") map_file = next();
        else if (a == "--png") png = next();
        else if (a == "--damage-ai-at") damage_ai_at = std::atoi(next().c_str());
    }

    const std::string dir = env_install_dir();
    ra2assets::MixFile ra2 = ra2assets::MixFile::open(ra2assets::read_file(dir + "ra2.mix"));
    ra2engine::SkirmishConfig cfg;
    {
        Ini ri = load_ini_from_ra2md(dir, "rulesmd.ini");
        Rules r0 = Rules::load(ri);
        cfg = ra2engine::default_skirmish_config(r0);
    }
    if (!map_file.empty()) { cfg.map_file = map_file; cfg.use_map_starts = true; }
    cfg.slots[1].diff = diff;
    if (ai_allied) {   // human Soviet vs Allied AI (the SK-S scenario)
        cfg.slots[0].side = ra2engine::World::Side::Soviet;  cfg.slots[0].country = "Russians";
        cfg.slots[1].side = ra2engine::World::Side::Allied;  cfg.slots[1].country = "Americans";
    }
    Assets a = load_assets(dir, ra2, cfg.map_file.empty() ? MapSelection{}
                                                          : MapSelection{"multimd.mix", 0u, cfg.map_file});
    UnitAssets ua = load_unit_assets(dir, ra2);
    ra2assets::MixFile language =
        ra2assets::MixFile::open(ra2assets::read_file(dir + "language.mix"));
    Ini rules_ini = load_ini_from_ra2md(dir, "rulesmd.ini");
    Rules rules = Rules::load(rules_ini);
    Ini art_ini = load_ini_from_ra2md(dir, "artmd.ini");
    Art art = Art::load(art_ini);
    BuildingAssets ba = load_building_assets(ra2, language, rules, art);
    MinerAssets miners = load_miner_assets(dir, ra2, ua.pal, rules);
    UnitRoster ur_ai = load_ai_skirmish_roster(dir, ra2, ua, rules, art);
    // Infantry SHPs/anims for the AI roster (GameScene ctor does the same).
    try {
        MixFile conquer = ra2.open_nested("conquer.mix");
        for (const ra2engine::UnitTypeDef& d : ur_ai.defs) {
            if (d.is_vehicle || ua.shps.count(d.combat.image)) continue;
            try {
                ua.shps.emplace(d.combat.image, load_unit_shp(conquer, d.combat.image));
                ua.anims.emplace(d.combat.image, anim_from_art(art, d.combat.image));
            } catch (const std::exception&) {}
        }
    } catch (const std::exception&) {}
    if (!ba.ok) { std::cerr << "[ai-soak] building assets unavailable\n"; return 1; }

    // ---- the GameScene world setup, in order ----
    ra2engine::World world = launch_skirmish(cfg, a, ua, ba, rules);
    // The economy firewall (G-20, owner A): idempotent setters, so calling it
    // here is harmless once launch_skirmish arms it itself.
    wire_economy(world, ba, rules);
    world.enable_projectiles(ra2engine::resolve_projectile_rules(rules));
    wire_abilities(world, rules, rules_ini);
    arm_skirmish_ai(world, cfg, ba, rules, rules_ini, ur_ai, load_aimd_or_empty(dir), &a.map, ua);
    enable_p5_features(world, rules, ba, cfg, {&ur_ai.defs});

    const int ai = 1, human = 0;
    auto human_base_hp = [&]() {
        long long hp = 0, mx = 0;
        for (const ra2engine::Building& b : world.buildings())
            if (b.house == human && b.alive) { hp += std::max(0, b.hp); mx += b.max_hp; }
        return std::make_pair(hp, mx);
    };
    const auto hp0 = human_base_hp();
    std::vector<int> bstates;
    long long ai_units_built = 0;
    int max_ai_units = 0;
    std::size_t prev_units = world.units().size();
    long long first_human_damage_frame = -1;
    const double dt = 1.0 / 15.0;
    const long long frames = (long long)minutes * 60 * 15;
    std::cout << "[ai-soak] map=" << (cfg.map_file.empty() ? "<default>" : cfg.map_file)
              << " ai_side=" << (ai_allied ? "Allied" : "Soviet") << " diff=" << (int)diff
              << " minutes=" << minutes << "\n";
    long long damaged_hp_after = -1, repairs_seen = 0, sells_seen = 0;
    for (long long f = 1; f <= frames; ++f) {
        if (damage_ai_at >= 0 && f == (long long)damage_ai_at * 900) {
            for (ra2engine::Building& b : world.buildings())
                if (b.alive && b.house == ai && b.state == ra2engine::Building::State::Idle)
                    b.hp = std::max(1, b.hp / 2);
            long long t = 0;
            for (const ra2engine::Building& b : world.buildings())
                if (b.alive && b.house == ai) t += b.hp;
            (void)damaged_hp_after; damaged_hp_after = t;
            std::cout << "[ai-soak] probe: AI buildings halved at min " << damage_ai_at
                      << " ai_base_hp=" << t << "\n";
        }
        world.step(dt);
        for (const ra2engine::Building& b : world.buildings())
            if (b.house == ai && b.alive) { repairs_seen += b.being_repaired ? 1 : 0;
                                            sells_seen += b.selling ? 1 : 0; }
        spawn_free_units_on_open(world, ba, rules, miners, bstates);
        // count AI units created this frame
        for (std::size_t u = prev_units; u < world.units().size(); ++u)
            if (world.units()[u].house == ai) ++ai_units_built;
        prev_units = world.units().size();
        if (first_human_damage_frame < 0 && human_base_hp().first < hp0.first)
            first_human_damage_frame = f;
        int alive = 0;
        for (const ra2engine::Unit& u : world.units())
            if (u.alive && u.house == ai) ++alive;
        max_ai_units = std::max(max_ai_units, alive);
        if (f % (60 * 15) == 0) {
            int nb = 0;
            std::map<std::string, int> types;
            for (const ra2engine::Building& b : world.buildings())
                if (b.alive && b.house == ai && b.state != ra2engine::Building::State::Dead) {
                    ++nb;
                    if (b.type >= 0 && b.type < (int)ba.roster.size()) ++types[ba.roster[b.type].id];
                }
            const ra2engine::AiTeamState* ts = world.ai_team_state(ai);
            const ra2engine::AiTriggerState* tr = world.ai_trigger_state(ai);
            int executing = 0;
            if (ts) for (const ra2engine::TeamInstance& t : ts->teams)
                if (t.state == ra2engine::TeamState::Executing) ++executing;
            const auto hp = human_base_hp();
            std::cout << "min " << f / 900 << ": ai_buildings=" << nb << " ai_credits="
                      << (long long)world.ai_credits(ai) << " ai_units_alive=" << alive
                      << " ai_units_built=" << ai_units_built
                      << " teams_created=" << (ts ? ts->teams_created : 0)
                      << " teams_live=" << (ts ? (int)ts->teams.size() : 0)
                      << " executing=" << executing
                      << " triggers_fired=" << (tr ? tr->triggers_fired : 0)
                      << " human_base_hp=" << hp.first << "/" << hp.second << "  [";
            for (const auto& kv : types) std::cout << kv.first << "x" << kv.second << " ";
            std::cout << "]\n";
        }
    }
    const auto hp1 = human_base_hp();
    {
        long long t = 0, mx = 0;
        for (const ra2engine::Building& b : world.buildings())
            if (b.alive && b.house == ai) { t += b.hp; mx += b.max_hp; }
        std::cout << "[ai-soak] ai_base_hp_end=" << t << "/" << mx
                  << " repair_frames=" << repairs_seen << " sell_frames=" << sells_seen << "\n";
    }
    std::cout << "[ai-soak] RESULT ai_units_built=" << ai_units_built
              << " max_ai_units_alive=" << max_ai_units
              << " human_base_hp " << hp0.first << " -> " << hp1.first
              << " first_human_damage_min="
              << (first_human_damage_frame < 0 ? -1.0 : first_human_damage_frame / 900.0)
              << " human_defeated=" << world.house_defeated(human)
              << " winning_team=" << world.winning_team() << "\n";

    if (!png.empty()) {
        const int W = 1280, H = 720;
        if (SDL_Init(SDL_INIT_VIDEO) == 0) {
            SDL_Window* win = SDL_CreateWindow("ai-soak", SDL_WINDOWPOS_CENTERED,
                                               SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
            SDL_Renderer* ren = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED) : nullptr;
            if (ren) {
                ra2engine::TextureCache tc(ren);
                // Centre on the human base (where the AI's attack lands).
                ra2engine::Camera cam = centered_camera(a.map, W, H);
                for (const ra2engine::Building& b : world.buildings())
                    if (b.house == human) {
                        const ra2engine::ScreenPt p =
                            ra2engine::cell_to_screen(b.cx, b.cy, 0, 60, 30, 15);
                        cam.x = W / 2 - p.x;
                        cam.y = H / 2 - p.y;
                        break;
                    }
                std::vector<const VehicleAssets*> vlist{&miners.harv, &miners.cmin};
                for (const VehicleAssets& v : ur_ai.vehicles) vlist.push_back(&v);
                SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
                SDL_RenderClear(ren);
                render_world(ren, tc, a, ua, world, cam, W, H, nullptr, &vlist, &ba, nullptr);
                capture_png(ren, png, W, H);
                std::cout << "png=" << png << "\n";
                SDL_DestroyRenderer(ren);
            }
            if (win) SDL_DestroyWindow(win);
            SDL_Quit();
        }
    }
    return 0;
}
