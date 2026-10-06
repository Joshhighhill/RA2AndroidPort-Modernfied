// realplay_probe.inc -- `--probe-realplay`: gap owner A's REAL-PLAY acceptance
// harness. It constructs the actual GameScene (the exact object the menu's
// Start button creates: SkirmishSetupScene -> GameScene(cfg) -> launch_skirmish)
// headless, drives it through GameScene::update / GameScene::handle (real
// sidebar clicks at real cameo rects), and asserts the economy/production
// behaviour the player sees. Not a hashed gate: it prints CHECK lines and
// exits 1 on any failure. Included after run_demo_saveload (needs GameScene).

namespace {
struct RealPlayProbe {
    int fails = 0;
    void check(bool ok, const std::string& what) {
        std::cout << (ok ? "[probe] PASS " : "[probe] FAIL ") << what << "\n";
        if (!ok) ++fails;
    }
    static ra2engine::SkirmishConfig menu_config(const Rules& rules) {
        // What SkirmishSetupScene hands GameScene on Start (default lobby, a
        // real multiplayer map, real [Waypoints] starts).
        ra2engine::SkirmishConfig cfg = ra2engine::default_skirmish_config(rules);
        cfg.map_file = "xmp31s2.map";
        cfg.use_map_starts = true;
        return cfg;
    }
    static ra2engine::World& world(GameScene& g) { return g.world_; }
    static double credits(GameScene& g) { return g.player_credits_; }
    static const BuildingAssets& ba(GameScene& g) { return g.ba_; }
    static const UnitRoster& ur(GameScene& g) { return g.ur_; }
    static int own_index(GameScene& g, const char* id) {
        const ra2engine::World& w = g.world_;
        for (int i = 0; i < (int)w.buildings().size(); ++i) {
            const ra2engine::Building& b = w.buildings()[i];
            if (b.alive && b.house == 0 && b.state != ra2engine::Building::State::Dead &&
                g.ba_.roster[b.type].id == id)
                return i;
        }
        return -1;
    }
    // Find a legal cell for roster type `t` next to the player's base (the
    // same placement_check the confirm uses, adjacency enforced).
    static bool find_spot(GameScene& g, int t, int& ox, int& oy) {
        const ra2engine::World& w = g.world_;
        const ra2engine::BuildingTypeDef& d = g.ba_.roster[t];
        int cx = -1, cy = -1;
        for (const ra2engine::Building& b : w.buildings())
            if (b.alive && b.house == 0) { cx = b.cx; cy = b.cy; break; }
        if (cx < 0) return false;
        for (int r = 1; r <= 14; ++r)
            for (int dy = -r; dy <= r; ++dy)
                for (int dx = -r; dx <= r; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                    if (g.placement_ok(t, cx + dx, cy + dy)) {
                        ox = cx + dx; oy = cy + dy;
                        return true;
                    }
                }
        (void)d;
        return false;
    }
    // Instant own building (scripted base growth, used only to reach a state).
    static int add_instant(GameScene& g, const char* id) {
        const int t = roster_index(g.ba_, id);
        int x = 0, y = 0;
        if (t < 0 || !find_spot(g, t, x, y)) return -1;
        const ra2engine::BuildingTypeDef& d = g.ba_.roster[t];
        return g.world_.add_building(t, x, y, d.fw, d.fh, 0, d.strength, d.armor_ix, 0.0,
                                     /*instant=*/true, d.sight, d.grants_map_reveal);
    }
    // A real left click at the centre of rect r.
    static void click(GameScene& g, const ra2engine::SbRect& r) {
        ra2engine::InputEvent ev;
        ev.kind = ra2engine::InputEvent::PointerDown;
        ev.button = ra2engine::InputEvent::Left;
        ev.x = r.x + r.w / 2;
        ev.y = r.y + r.h / 2;
        g.handle(ev);
    }
    static void tab(GameScene& g, int t) { click(g, g.lay_.tabs[t]); }
    // Ids shown on the active tab, in cameo order.
    static std::vector<std::string> tab_ids(GameScene& g, int t) {
        tab(g, t);
        std::vector<std::string> v;
        for (int x : g.buildable_)
            v.push_back(t < 2 ? g.ba_.roster[x].id : g.ur_.defs[x].id);
        return v;
    }
    static bool has(const std::vector<std::string>& v, const char* id) {
        return std::find(v.begin(), v.end(), std::string(id)) != v.end();
    }
    static void run(GameScene& g, int frames) {
        for (int f = 0; f < frames; ++f) g.update(1.0 / 15.0);
    }
};

}  // namespace

int run_probe_realplay() {
    const std::string dir = env_install_dir();
    Ini rules_ini = load_ini_from_ra2md(dir, "rulesmd.ini");
    Rules rules = Rules::load(rules_ini);
    RealPlayProbe p;
    using P = RealPlayProbe;

    // ---- G-20 / G-21: wallet + armed economy (Allied human vs Soviet AI) ----
    {
        ra2engine::SkirmishConfig cfg = P::menu_config(rules);
        cfg.starting_cash = 7500.0;   // a non-default slider value
        GameScene g(/*no_audio=*/true, cfg);
        ra2engine::World& w = P::world(g);
        p.check(P::credits(g) == 7500.0,
                "G-21 human wallet == lobby Money (7500), got " + std::to_string(P::credits(g)));
        p.check(w.house_credits(0) == 0.0, "G-21 no second house-0 seed left in the World pool");
        p.check(w.ai_credits(1) == 7500.0 && w.house_credits(1) == 0.0,
                "G-21 AI wallet == lobby Money once (ai=" + std::to_string(w.ai_credits(1)) +
                    " pool=" + std::to_string(w.house_credits(1)) + ")");
        // A refinery for the human, built through Buildup like a placed one, so
        // the real FreeUnit grand-opening spawns the miner.
        P::add_instant(g, "GAPOWR");
        const int t = roster_index(P::ba(g), "GAREFN");
        int rx = 0, ry = 0;
        const bool spot = t >= 0 && P::find_spot(g, t, rx, ry);
        p.check(spot, "G-20 found a legal GAREFN spot by the ConYard");
        if (spot) g.place_building(t, rx, ry);
        const double c0 = P::credits(g);
        int ore0 = 0;
        for (int y = 0; y < w.ore().h(); ++y)
            for (int x = 0; x < w.ore().w(); ++x) ore0 += w.ore().density_at(x, y);
        bool moved = false, unloaded = false;
        int miner = -1;
        for (int f = 0; f < 15 * 150; ++f) {
            g.update(1.0 / 15.0);
            for (int i = 0; i < (int)w.units().size(); ++i)
                if (w.units()[i].is_harvester && w.units()[i].house == 0) miner = i;
            if (miner >= 0) {
                const ra2engine::Unit& u = w.units()[miner];
                if (u.harv == ra2engine::HarvState::Harvesting ||
                    u.harv == ra2engine::HarvState::ReturnToRef)
                    moved = true;
                if (u.harv == ra2engine::HarvState::Unloading) unloaded = true;
            }
        }
        int ore1 = 0;
        for (int y = 0; y < w.ore().h(); ++y)
            for (int x = 0; x < w.ore().w(); ++x) ore1 += w.ore().density_at(x, y);
        p.check(miner >= 0, "G-20 refinery grand opening spawned the human's Chrono Miner");
        p.check(moved, "G-20 human miner left the dock and harvested");
        p.check(unloaded, "G-20 human miner docked and unloaded");
        p.check(P::credits(g) > c0, "G-20 harvest income reached the sidebar wallet (" +
                                        std::to_string(c0) + " -> " +
                                        std::to_string(P::credits(g)) + ")");
        p.check(ore1 != ore0, "G-20 ore field changed (harvest/growth): " + std::to_string(ore0) +
                                  " -> " + std::to_string(ore1));
        int ai_miners = 0;
        for (const ra2engine::Unit& u : w.units())
            if (u.alive && u.is_harvester && u.house == 1 && u.harv != ra2engine::HarvState::None &&
                u.harv != ra2engine::HarvState::SeekOre)
                ++ai_miners;
        p.check(ai_miners > 0, "G-20 AI miners are working (" + std::to_string(ai_miners) + ")");
    }

    std::cout << "[probe-realplay] " << (p.fails ? "FAILED" : "ok") << " fails=" << p.fails << "\n";
    return p.fails ? 1 : 0;
}
