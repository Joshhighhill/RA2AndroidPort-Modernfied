// ============================================================================
// --ai-soak : the REAL-PLAY AI acceptance harness [G-60..G-67].
//
// Constructs the actual GameScene (SkirmishSetupScene's Start -> GameScene(cfg)
// -> launch_skirmish -> arm_skirmish_ai) headless and drives it through
// GameScene::update at the fixed dt = 1/15 with a PASSIVE human (house 0 never
// gets an order). Reports, once per game minute, what the AI did: structures,
// credits, units fielded, teams created/executing, triggers fired, superweapon
// shots, damage dealt to the human's base. Optional AI-base damage probe (G-65)
// and an end-of-run PNG centred on the human base. NOT a byte-identity gate.
//
//   ra2engine --ai-soak [--minutes N] [--diff easy|normal|brutal]
//                       [--ai-side soviet|allied] [--map <file.map>]
//                       [--damage-ai-at MIN] [--ai-vs-ai] [--png <path>]
// ============================================================================

namespace {
struct AiSoakProbe {
    static ra2engine::World& world(GameScene& g) { return g.world_; }
    static const BuildingAssets& ba(GameScene& g) { return g.ba_; }

    static int run(int argc, char** argv) {
        int minutes = 15, damage_ai_at = -1;
        ra2engine::AiDifficulty diff = ra2engine::AiDifficulty::Normal;
        bool ai_allied = false, ai_vs_ai = false;
        std::string map_file = "xmp31s2.map", png;   // the --probe-realplay menu map
        for (int i = 2; i < argc; ++i) {
            const std::string a = argv[i];
            auto next = [&]() -> std::string { return i + 1 < argc ? std::string(argv[++i]) : ""; };
            if (a == "--minutes") minutes = std::max(1, std::atoi(next().c_str()));
            else if (a == "--diff") {
                const std::string d = next();
                diff = d == "easy" ? ra2engine::AiDifficulty::Easy
                     : (d == "brutal" || d == "hard") ? ra2engine::AiDifficulty::Difficult
                     : ra2engine::AiDifficulty::Normal;
            } else if (a == "--ai-side") ai_allied = next() == "allied";
            else if (a == "--map") map_file = next();
            else if (a == "--damage-ai-at") damage_ai_at = std::atoi(next().c_str());
            else if (a == "--png") png = next();
            else if (a == "--ai-vs-ai") ai_vs_ai = true;   // house 0 is an AI too
        }
        const std::string dir = env_install_dir();
        Ini rules_ini = load_ini_from_ra2md(dir, "rulesmd.ini");
        Rules rules = Rules::load(rules_ini);
        ra2engine::SkirmishConfig cfg = ra2engine::default_skirmish_config(rules);
        if (!map_file.empty() && map_file != "default") { cfg.map_file = map_file; cfg.use_map_starts = true; }
        cfg.slots[1].diff = diff;
        if (ai_allied) {   // human Soviet vs Allied AI (the SK-S scenario)
            cfg.slots[0].side = ra2engine::World::Side::Soviet;  cfg.slots[0].country = "Russians";
            cfg.slots[1].side = ra2engine::World::Side::Allied;  cfg.slots[1].country = "Americans";
        }
        if (ai_vs_ai) cfg.slots[0].is_ai = true;
        GameScene g(/*no_audio=*/true, cfg);
        ra2engine::World& w = world(g);
        const BuildingAssets& b_a = ba(g);
        const int ai = 1, human = 0;
        auto base_hp = [&](int h) {
            long long hp = 0, mx = 0;
            for (const ra2engine::Building& b : w.buildings())
                if (b.house == h && b.alive && b.state != ra2engine::Building::State::Dead) {
                    hp += std::max(0, b.hp); mx += b.max_hp;
                }
            return std::make_pair(hp, mx);
        };
        const auto hp0 = base_hp(human);
        long long ai_units_built = 0, first_dmg = -1, repair_frames = 0, sell_frames = 0;
        int max_alive = 0, sw_shots = 0;
        bool sw_was_ready = false;
        std::size_t prev_units = w.units().size();
        std::cout << "[ai-soak] map=" << (cfg.map_file.empty() ? "<default>" : cfg.map_file)
                  << " ai_side=" << (ai_allied ? "Allied" : "Soviet") << " diff=" << (int)diff
                  << " minutes=" << minutes << "\n";
        const long long frames = (long long)minutes * 900;
        for (long long f = 1; f <= frames; ++f) {
            if (damage_ai_at >= 0 && f == (long long)damage_ai_at * 900) {
                long long t = 0;
                for (ra2engine::Building& b : w.buildings())
                    if (b.alive && b.house == ai && b.state == ra2engine::Building::State::Idle) {
                        b.hp = std::max(1, b.hp / 2);
                        t += b.hp;
                    }
                std::cout << "[ai-soak] probe: AI buildings halved at min " << damage_ai_at
                          << " ai_base_hp=" << t << "\n";
            }
            const bool ready_before = w.sw_ready(ai);
            g.update(1.0 / 15.0);
            // A ready SW that is no longer ready after the AI tick was fired.
            if (ready_before && !w.sw_ready(ai) && w.sw_state(ai)) ++sw_shots;
            sw_was_ready = sw_was_ready || ready_before;
            for (std::size_t u = prev_units; u < w.units().size(); ++u)
                if (w.units()[u].house == ai) ++ai_units_built;
            prev_units = w.units().size();
            if (first_dmg < 0 && base_hp(human).first < hp0.first) first_dmg = f;
            int alive = 0;
            for (const ra2engine::Unit& u : w.units())
                if (u.alive && u.house == ai) ++alive;
            max_alive = std::max(max_alive, alive);
            for (const ra2engine::Building& b : w.buildings())
                if (b.house == ai && b.alive) { repair_frames += b.being_repaired; sell_frames += b.selling; }
            if (f % 900 == 0) {
                int nb = 0;
                std::map<std::string, int> types;
                for (const ra2engine::Building& b : w.buildings())
                    if (b.alive && b.house == ai && b.state != ra2engine::Building::State::Dead) {
                        ++nb;
                        if (b.type >= 0 && b.type < (int)b_a.roster.size()) ++types[b_a.roster[b.type].id];
                    }
                const ra2engine::AiTeamState* ts = w.ai_team_state(ai);
                const ra2engine::AiTriggerState* tr = w.ai_trigger_state(ai);
                int executing = 0;
                if (ts) for (const ra2engine::TeamInstance& t : ts->teams)
                    if (t.state == ra2engine::TeamState::Executing) ++executing;
                const auto hp = base_hp(human);
                int miners = 0;
                for (const ra2engine::Unit& u : w.units())
                    if (u.alive && u.house == ai && u.is_harvester) ++miners;
                std::cout << "min " << f / 900 << ": ai_buildings=" << nb << " ai_miners=" << miners
                          << " ai_credits=" << (long long)w.ai_credits(ai) << " ai_units_alive=" << alive
                          << " ai_units_built=" << ai_units_built
                          << " teams_created=" << (ts ? ts->teams_created : 0)
                          << " teams_live=" << (ts ? (int)ts->teams.size() : 0)
                          << " executing=" << executing
                          << " triggers_fired=" << (tr ? tr->triggers_fired : 0)
                          << " sw_shots=" << sw_shots
                          << " human_base_hp=" << hp.first << "/" << hp.second << "  [";
                for (const auto& kv : types) std::cout << kv.first << "x" << kv.second << " ";
                std::cout << "]\n";
            }
        }
        const auto hp1 = base_hp(human);
        if (const ra2engine::AiTeamState* ts = w.ai_team_state(ai)) {   // where are the teams?
            int hx = -1, hy = -1;
            for (const ra2engine::Building& b : w.buildings())
                if (hx < 0 && b.house == human && b.alive) { hx = b.cx; hy = b.cy; }
            for (const ra2engine::TeamInstance& t : ts->teams) {
                long sx = 0, sy = 0, n = 0;
                for (int m : t.members)
                    if (m >= 0 && m < (int)w.units().size() && w.units()[m].alive) {
                        sx += w.units()[m].cell_x; sy += w.units()[m].cell_y; ++n;
                    }
                std::cout << "[ai-soak] team tt=" << t.teamtype << " state=" << (int)t.state
                          << " pc=" << t.script_pc << " members=" << n;
                if (const ra2engine::AiHouseConfig* hc = w.ai_config(ai)) {
                    const ra2engine::AimdData& d = hc->team.aimd;
                    if (t.teamtype >= 0 && t.teamtype < (int)d.teams.size()) {
                        const ra2engine::ScriptType& sc = d.scripts[d.teams[t.teamtype].script];
                        std::cout << " '" << d.teams[t.teamtype].name << "'";
                        if (t.script_pc >= 0 && t.script_pc < (int)sc.actions.size())
                            std::cout << " line=" << sc.actions[t.script_pc].action << ","
                                      << sc.actions[t.script_pc].argument;
                    }
                }
                { int mv = 0; for (int m : t.members) if (m >= 0 && m < (int)w.units().size() && w.units()[m].alive && w.units()[m].moving()) ++mv;
                  std::cout << " gather=(" << t.gather_x << "," << t.gather_y << ") issued=" << t.move_issued << " moving=" << mv << " stall=" << t.stall_reissues; }
                if (n) std::cout << " at=(" << sx / n << "," << sy / n << ") human_base=(" << hx << "," << hy << ")";
                std::cout << "\n";
            }
        }
        const auto ai_hp = base_hp(ai);
        std::cout << "[ai-soak] ai_base_hp_end=" << ai_hp.first << "/" << ai_hp.second
                  << " repair_frames=" << repair_frames << " sell_frames=" << sell_frames << "\n";
        std::cout << "[ai-soak] RESULT ai_units_built=" << ai_units_built
                  << " max_ai_units_alive=" << max_alive << " sw_ever_ready=" << sw_was_ready
                  << " sw_shots=" << sw_shots
                  << " human_base_hp " << hp0.first << " -> " << hp1.first
                  << " first_human_damage_min=" << (first_dmg < 0 ? -1.0 : first_dmg / 900.0)
                  << " human_defeated=" << w.house_defeated(human)
                  << " winning_team=" << w.winning_team() << "\n";
        if (!png.empty()) {
            const int W = 1280, H = 720;
            if (SDL_Init(SDL_INIT_VIDEO) == 0) {
                SDL_Window* win = SDL_CreateWindow("ai-soak", SDL_WINDOWPOS_CENTERED,
                                                   SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_HIDDEN);
                SDL_Renderer* ren = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED) : nullptr;
                if (ren) {
                    {
                        ra2engine::TextureCache tc(ren);
                        ra2engine::Camera cam = centered_camera(g.a_.map, W, H);
                        int fx = -1, fy = -1;   // the human base, else the AI base
                        for (int h : {human, ai})
                            for (const ra2engine::Building& b : w.buildings())
                                if (fx < 0 && b.house == h && b.alive) { fx = b.cx; fy = b.cy; }
                        if (fx >= 0) {
                            const ra2engine::ScreenPt p = ra2engine::cell_to_screen(fx, fy, 0, 60, 30, 15);
                            cam.x = W / 2 - p.x;
                            cam.y = H / 2 - p.y;
                        }
                        SDL_SetRenderDrawColor(ren, 10, 20, 60, 255);
                        SDL_RenderClear(ren);
                        render_world(ren, tc, g.a_, g.ua_, w, cam, W, H, nullptr, &g.vlist_, &g.ba_,
                                     &g.ore_art_);
                        capture_png(ren, png, W, H);
                        std::cout << "png=" << png << "\n";
                    }
                    SDL_DestroyRenderer(ren);
                }
                if (win) SDL_DestroyWindow(win);
                SDL_Quit();
            }
        }
        return 0;
    }
};
}  // namespace

int run_ai_soak(int argc, char** argv) { return AiSoakProbe::run(argc, argv); }
