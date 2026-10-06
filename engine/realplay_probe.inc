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
    // Click the cameo of `id` on tab `t` (real sidebar click). False when the
    // strip does not list it.
    static bool click_item(GameScene& g, int t, const std::string& id) {
        tab(g, t);
        for (std::size_t k = 0; k < g.buildable_.size() && k < g.lay_.cameos.size(); ++k) {
            const int x = g.buildable_[k];
            const std::string& xid = t < 2 ? g.ba_.roster[x].id : g.ur_.defs[x].id;
            if (xid == id) {
                click(g, g.lay_.cameos[k].rect);
                return true;
            }
        }
        return false;
    }
    // Train unit `id` through the sidebar and wait (bounded) for it to roll
    // out of its factory. Returns the new unit index or -1.
    static int train(GameScene& g, int t, const std::string& id, int max_frames = 15 * 120) {
        const std::size_t n0 = g.world_.units().size();
        if (!click_item(g, t, id)) return -1;
        for (int f = 0; f < max_frames; ++f) {
            g.update(1.0 / 15.0);
            for (std::size_t i = n0; i < g.world_.units().size(); ++i)
                if (g.world_.units()[i].house == 0) return static_cast<int>(i);
        }
        return -1;
    }
    static int count_cameos_not_laid_out(GameScene& g) {
        return static_cast<int>(g.buildable_.size()) - static_cast<int>(g.lay_.cameos.size());
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

    auto join = [](const std::vector<std::string>& v) {
        std::string s;
        for (const std::string& x : v) s += (s.empty() ? "" : ",") + x;
        return s;
    };

    // ---- G-22 / G-26 / row 15/18/19: Allied (Americans) roster + strip ----
    {
        ra2engine::SkirmishConfig cfg = P::menu_config(rules);
        GameScene g(/*no_audio=*/true, cfg);
        std::vector<std::string> t0 = P::tab_ids(g, 0);
        std::cout << "[probe] allied start tab0=" << join(t0) << "\n";
        p.check(P::has(t0, "GAPOWR") && !P::has(t0, "NAPOWR") && !P::has(t0, "GAREFN"),
                "G-26 lone ConYard: strip offers GAPOWR only (no NA*, no unmet GAREFN)");
        for (const char* id : {"GAPOWR", "GAREFN", "GAPILE", "GAWEAP", "AMRADR"})
            P::add_instant(g, id);
        P::run(g, 2);
        t0 = P::tab_ids(g, 0);
        const std::vector<std::string> t1 = P::tab_ids(g, 1);
        const std::vector<std::string> t2 = P::tab_ids(g, 2);
        const std::vector<std::string> t3 = P::tab_ids(g, 3);
        std::cout << "[probe] allied base tab0=" << join(t0) << "\n[probe] allied base tab1="
                  << join(t1) << "\n[probe] allied base tab2=" << join(t2)
                  << "\n[probe] allied base tab3=" << join(t3) << "\n";
        p.check(P::has(t1, "ATESLA") && P::has(t1, "GAPILL") && !P::has(t1, "NALASR") &&
                    !P::has(t1, "GTGCAN"),
                "G-26 defenses: Prism Tower listed (was prefix-dropped), Grand Cannon not for Americans");
        p.check(!P::has(t0, "GAAIRC"), "row 18 GAAIRC ForbiddenHouses=Americans honoured");
        p.check(P::has(t2, "E1") && P::has(t2, "ENGINEER") && P::has(t2, "ADOG") &&
                    P::has(t2, "JUMPJET") && !P::has(t2, "E2") && !P::has(t2, "SNIPE"),
                "G-22 Allied infantry: GI/Engineer/Dog/Rocketeer, no Conscript, no British Sniper");
        p.check(P::has(t3, "MTNK") && P::has(t3, "CMIN") && !P::has(t3, "HTNK") &&
                    !P::has(t3, "TNKD"),
                "G-22 Allied vehicles: Grizzly + Chrono Miner, no Rhino, no German Tank Destroyer");
        const int u = P::train(g, 3, "CMIN");
        p.check(u >= 0 && P::world(g).units()[u].is_harvester,
                "G-22 a sidebar-built Chrono Miner rolled out as a working miner");
        const int e = P::train(g, 2, "ENGINEER");
        std::string eng_image;
        for (const ra2engine::UnitTypeDef& d : P::ur(g).defs)
            if (d.id == "ENGINEER") eng_image = d.combat.image;
        p.check(e >= 0 && P::world(g).units()[e].image == eng_image,
                "G-22 a sidebar-built Engineer left the Barracks");
    }

    // ---- row 19: Soviet human (stale lobby country "Americans") ----
    {
        ra2engine::SkirmishConfig cfg = P::menu_config(rules);
        cfg.slots[0].side = ra2engine::World::Side::Soviet;   // lobby side button; country stays
        cfg.slots[1].side = ra2engine::World::Side::Allied;
        cfg.slots[1].country = "Americans";
        GameScene g(/*no_audio=*/true, cfg);
        P::add_instant(g, "NAREFN");
        P::run(g, 2);
        const std::vector<std::string> t0 = P::tab_ids(g, 0);
        const std::vector<std::string> t2 = P::tab_ids(g, 2);
        const std::vector<std::string> t3 = P::tab_ids(g, 3);
        std::cout << "[probe] soviet tab0=" << join(t0) << "\n[probe] soviet tab2=" << join(t2)
                  << "\n[probe] soviet tab3=" << join(t3) << "\n";
        p.check(!P::has(t0, "GAPOWR") && P::has(t0, "NARADR"),
                "row 19 Soviet human gets NA* structures, no GA*");
        p.check(P::has(t2, "E2") && P::has(t2, "DOG") && !P::has(t2, "E1"),
                "row 19 Soviet infantry (Conscript, Attack Dog), no GI");
        p.check(P::has(t3, "HTNK") && P::has(t3, "HARV") && !P::has(t3, "MTNK"),
                "row 19 Soviet vehicles (Rhino, War Miner), no Grizzly");
        const int u = P::train(g, 2, "E2");
        p.check(u >= 0, "row 19 a Conscript trained at the Soviet human's NAHAND");
        const int h = P::train(g, 3, "HTNK");
        p.check(h >= 0, "row 19 a Rhino rolled out of the Soviet human's NAWEAP");
    }

    // ---- row 15 / row 18: lobby TechLevel + country units ----
    {
        ra2engine::SkirmishConfig cfg = P::menu_config(rules);
        cfg.tech_level = 3;
        cfg.slots[0].country = "Germans";
        GameScene g(/*no_audio=*/true, cfg);
        bool sref = false, tnkd = false, fv = false;
        for (const ra2engine::UnitTypeDef& d : P::ur(g).defs) {
            sref |= d.id == "SREF";
            tnkd |= d.id == "TNKD";
            fv |= d.id == "FV";
        }
        p.check(!sref && fv, "row 15 lobby TechLevel 3 drops the Prism Tank (TL8), keeps the IFV (TL3)");
        p.check(tnkd, "row 18 Germans get the Tank Destroyer (RequiredHouses=Germans)");
    }

    std::cout << "[probe-realplay] " << (p.fails ? "FAILED" : "ok") << " fails=" << p.fails << "\n";
    return p.fails ? 1 : 0;
}
