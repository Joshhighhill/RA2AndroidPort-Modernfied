// gF: skirmish start like the original + MCV wiring for the interactive game.
// Textually included by main.cpp (after campaign_runtime.inc, before GameScene).
// [RE: docs/re/mcv-deploy.md §1-§6]
//
// The original's MP unit creation is FUN_006880F0 ("Creating %d units. Random
// seed is %08x" / "Generating units for house %d"):
//   * n = UnitCount, minus one when bases are on (the MCV is part of the count);
//   * budget = n x (integer average Cost of every [VehicleTypes] +
//     [InfantryTypes] entry with AllowedToStartInMultiplayer=yes that is not a
//     [General] BaseUnit);
//   * per house: candidate vehicles / infantry = AllowedToStartInMultiplayer,
//     TechLevel <= the game's tech level, Owner= contains the house's country,
//     not a BaseUnit;
//   * start cell: the first house takes a random start waypoint, every later
//     house the free start whose summed distance to the taken ones is largest;
//   * the BaseUnit the country may own is unlimboed ON the start cell;
//   * then, while spent < budget: a random vehicle while spent < 2/3 budget
//     (if any), else a random infantry; spent += Cost; placed around the start.
// [RE-FALLBACK: the original draws from the session RNG; the clone seeds a
//  local LCG from the map name so a launch is deterministic.]

struct GfRng {
    std::uint32_t s = 0x2545F491u;
    explicit GfRng(std::uint32_t seed) : s(seed ? seed : 0x2545F491u) {}
    // Uniform-ish integer in [lo, hi] (inclusive), the Random(lo,hi) contract.
    int range(int lo, int hi) {
        s = s * 1664525u + 1013904223u;
        if (hi <= lo) return lo;
        return lo + static_cast<int>((s >> 8) % static_cast<std::uint32_t>(hi - lo + 1));
    }
};

inline std::uint32_t gf_seed_of(const std::string& text) {
    std::uint32_t h = 2166136261u;
    for (char c : text) h = (h ^ static_cast<unsigned char>(c)) * 16777619u;
    return h;
}

// Comma list -> upper-cased trimmed tokens.
inline std::vector<std::string> gf_tokens(const std::string& v) {
    std::vector<std::string> out;
    std::string cur;
    auto flush = [&]() {
        std::size_t a = 0, b = cur.size();
        while (a < b && std::isspace(static_cast<unsigned char>(cur[a]))) ++a;
        while (b > a && std::isspace(static_cast<unsigned char>(cur[b - 1]))) --b;
        std::string t = cur.substr(a, b - a);
        for (char& ch : t) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        if (!t.empty()) out.push_back(t);
        cur.clear();
    };
    for (char c : v) {
        if (c == ';') break;
        if (c == ',') flush(); else cur.push_back(c);
    }
    flush();
    return out;
}

inline std::string gf_default_country(ra2engine::World::Side side) {
    return side == ra2engine::World::Side::Soviet ? "Russians" : "Americans";
}

inline bool gf_owner_has(const Rules& rules, const std::string& id, const std::string& country) {
    std::string c = country;
    for (char& ch : c) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    for (const std::string& t : gf_tokens(rules.section_string(id, "Owner", "")))
        if (t == c) return true;
    return false;
}

// The [General] BaseUnit entry this country may own (FUN_00505130 over the
// rules BaseUnit vector), or "" when none does.
inline std::string gf_base_unit(const Rules& rules, const std::string& country) {
    for (const std::string& id : gf_tokens(rules.general_string("BaseUnit", "AMCV,SMCV")))
        if (gf_owner_has(rules, id, country)) return id;
    return "";
}

// A vehicle's movement/combat profile exactly as load_vehicle_assets builds it.
inline ra2engine::UnitCombat gf_vehicle_combat(const Rules& rules, const std::string& id) {
    const ResolvedUnit ru = resolve_combat_unit(rules, id);
    ra2engine::UnitCombat c = ru.combat;
    c.is_vehicle = true;
    c.rot = rules.rot(id);
    c.turret = rules.has_turret(id);
    c.speed_cps = ru.speed * kCellsPerSecPerSpeed;
    return c;
}

inline ra2engine::UnitCombat gf_infantry_combat(const Rules& rules, const std::string& id) {
    const ResolvedUnit ru = resolve_combat_unit(rules, id);
    ra2engine::UnitCombat c = ru.combat;
    c.is_vehicle = false;
    c.speed_cps = ru.speed * kCellsPerSecPerSpeed;
    return c;
}

struct GfStartPools {
    std::vector<std::string> vehicles, infantry;
    int budget_unit = 0;   // the integer average Cost (per unit of n)
};

// Pools + average of FUN_006880F0 for one country at one tech level.
inline GfStartPools gf_start_pools(const ra2assets::Ini& ini, const Rules& rules,
                                   const std::string& country, int tech_level) {
    GfStartPools p;
    const std::vector<std::string> base = gf_tokens(rules.general_string("BaseUnit", "AMCV,SMCV"));
    auto is_base = [&](const std::string& id) {
        return std::find(base.begin(), base.end(), id) != base.end();
    };
    long long sum = 0;
    int count = 0;
    for (const char* list : {"VehicleTypes", "InfantryTypes"}) {
        const bool veh = std::string(list) == "VehicleTypes";
        for (const auto& kv : ini.section(list)) {
            std::string id = kv.second;
            for (char& ch : id) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            if (id.empty() || !ini.has(id)) continue;
            if (!rules.section_flag(id, "AllowedToStartInMultiplayer", true)) continue;
            if (is_base(id)) continue;
            sum += rules.cost(id);
            ++count;
            if (rules.tech_level(id) > tech_level || rules.tech_level(id) < 0) continue;
            if (!gf_owner_has(rules, id, country)) continue;
            std::vector<std::string>& pool = veh ? p.vehicles : p.infantry;
            if (std::find(pool.begin(), pool.end(), id) == pool.end()) pool.push_back(id);
        }
    }
    p.budget_unit = count > 0 ? static_cast<int>(sum / count) : 0;
    return p;
}

// Start-location choice of FUN_006880F0: first house random, later houses the
// start with the largest summed (truncated Euclidean) distance to taken ones.
inline std::vector<int> gf_pick_starts(const std::vector<ra2assets::MapWaypoint>& w, int houses,
                                       GfRng& rng) {
    std::vector<int> out;
    std::vector<char> taken(w.size(), 0);
    for (int h = 0; h < houses && !w.empty(); ++h) {
        int pick = 0;
        if (out.empty()) {
            pick = rng.range(0, static_cast<int>(w.size()) - 1);
        } else {
            int best = 0;
            for (std::size_t i = 0; i < w.size(); ++i) {
                if (taken[i]) continue;
                int d = 0;
                for (std::size_t j = 0; j < w.size(); ++j) {
                    if (!taken[j]) continue;
                    const double dx = w[i].x - w[j].x, dy = w[i].y - w[j].y;
                    d += static_cast<int>(std::sqrt(dx * dx + dy * dy));
                }
                if (best < d || best == 0) { best = d; pick = static_cast<int>(i); }
            }
            if (taken[static_cast<std::size_t>(pick)]) pick = out.back();   // more houses than starts
        }
        taken[static_cast<std::size_t>(pick)] = 1;
        out.push_back(pick);
    }
    return out;
}

// The start-object scatter of FUN_00688910(obj, cell, d0): the exact cell when
// free; otherwise, for d = d0..31, a random first direction (0=N,1=NE,..7=NW
// in cell space) and the 8 directions at distance d -- first exactly, then a
// second pass with a random +-0/1 jitter per axis -- clamped to the map.
// Called with d0 = 1 for the MCV fallback and d0 = 4 for every starting unit,
// which is what keeps the MCV's foundation clear at frame 0.
inline bool gf_free_cell_near(const ra2engine::World& world, int x, int y, int d0, GfRng& rng,
                              int& ox, int& oy) {
    auto free_at = [&](int cx, int cy) {
        return world.walkable(cx, cy) && world.unit_at_cell(cx, cy) < 0 &&
               world.occupancy().is_free(cx, cy);
    };
    if (free_at(x, y)) { ox = x; oy = y; return true; }
    static const int kDx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int kDy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    const int gw = world.grid_w(), gh = world.grid_h();
    for (int d = d0; d < 32; ++d) {
        int dir = rng.range(0, 7);
        for (int pass = 0; pass < 2; ++pass)
            for (int k = 0; k < 8; ++k, dir = (dir + 1) & 7) {
                int cx = std::max(0, std::min(gw - 1, x + kDx[dir] * d));
                int cy = std::max(0, std::min(gh - 1, y + kDy[dir] * d));
                if (pass > 0) {
                    const int jx = rng.range(0, 1);
                    cx = rng.range(0, 99) < 50 ? std::min(gw - 1, cx + jx) : std::max(0, cx - jx);
                    const int jy = rng.range(0, 1);
                    cy = rng.range(0, 99) < 50 ? std::min(gh - 1, cy + jy) : std::max(0, cy - jy);
                }
                if (cx == x && cy == y) continue;
                if (free_at(cx, cy)) { ox = cx; oy = cy; return true; }
            }
    }
    return false;
}

// Spawn one house's MCV + starting force at (sx, sy). Returns the MCV's unit
// index (-1 when the country owns no BaseUnit). Records every spawned type id
// in `spawned_ids` so the scene can load render assets for them.
inline int gf_spawn_start_force(ra2engine::World& world, const ra2assets::Ini& ini,
                                const Rules& rules, int house, const std::string& country,
                                int sx, int sy, int unit_count, int tech_level, GfRng& rng,
                                std::vector<std::string>* spawned_ids) {
    int mcv = -1;
    const std::string base_id = gf_base_unit(rules, country);
    int n = unit_count;
    if (!base_id.empty()) {
        --n;   // the MCV is one of UnitCount (bases on)
        int x = sx, y = sy;
        if (gf_free_cell_near(world, sx, sy, 1, rng, x, y)) {
            world.add_combat_unit(x, y, house, gf_vehicle_combat(rules, base_id));
            mcv = static_cast<int>(world.units().size()) - 1;
            if (spawned_ids) spawned_ids->push_back(base_id);
        }
    }
    const GfStartPools p = gf_start_pools(ini, rules, country, tech_level);
    const int budget = p.budget_unit * std::max(0, n);
    int spent = 0, guard = 0;
    while (spent < budget && guard++ < 64) {
        std::string id;
        if (spent < (budget * 2) / 3 && !p.vehicles.empty())
            id = p.vehicles[(std::size_t)rng.range(0, (int)p.vehicles.size() - 1)];
        else if (!p.infantry.empty())
            id = p.infantry[(std::size_t)rng.range(0, (int)p.infantry.size() - 1)];
        else if (!p.vehicles.empty())
            id = p.vehicles[(std::size_t)rng.range(0, (int)p.vehicles.size() - 1)];
        else
            break;
        const bool veh = std::find(p.vehicles.begin(), p.vehicles.end(), id) != p.vehicles.end();
        int x = sx, y = sy;
        if (!gf_free_cell_near(world, sx, sy, 4, rng, x, y)) break;
        world.add_combat_unit(x, y, house,
                              veh ? gf_vehicle_combat(rules, id) : gf_infantry_combat(rules, id));
        if (spawned_ids) spawned_ids->push_back(id);
        spent += std::max(1, rules.cost(id));
    }
    return mcv;
}

// MCV wiring for any interactive World: buildup timings (deploy plays the
// unfold), the ConYard -> MCV undeploy registry, and the session flags.
inline void gf_arm_mcv(ra2engine::World& world, const BuildingAssets& ba, const Rules& rules,
                       bool skirmish_session) {
    std::vector<double> bt(ba.art.size(), 0.0);
    for (std::size_t i = 0; i < ba.art.size(); ++i) bt[i] = ba.art[i].buildup_total;
    world.set_buildup_totals(std::move(bt));
    for (std::size_t i = 0; i < ba.roster.size(); ++i) {
        const std::string und = rules.section_string(ba.roster[i].id, "UndeploysInto", "");
        if (!und.empty() && rules.is_vehicle(und))
            world.set_undeploy_unit(static_cast<int>(i), gf_vehicle_combat(rules, und));
    }
    world.set_skirmish_session(skirmish_session);
    // [MultiplayerDialogSettings] MCVRedeploys=yes (repair-sell.md §B6; the
    // lobby option defaults from it).
    bool redeploys = true;
    const std::string v = rules.section_string("MultiplayerDialogSettings", "MCVRedeploys", "yes");
    if (!v.empty()) redeploys = (v[0] == 'y' || v[0] == 'Y' || v[0] == 't' || v[0] == 'T' || v[0] == '1');
    world.set_mcv_redeploys(redeploys);
}

// Type ids launch_skirmish spawned for the real start (GameScene loads their
// render assets right after construction, then clears it).
inline std::vector<std::string> g_gf_spawned;

// Skirmish result screen (defined in gf_result.inc, after GameScene).
struct GfSkirmishStats {
    int units_killed = 0, units_lost = 0, buildings_killed = 0, buildings_lost = 0;
};
Scene* gf_make_skirmish_result_scene(bool no_audio, const ra2engine::SkirmishConfig& cfg, int outcome,
                                     long frames, const GfSkirmishStats& stats);

// gF G-14: the shell's yes/no message boxes [RE: mainmenu_true.md §3 -- Exit
// Game goes through FUN_005B46B0 with dialogs 0x120 (two buttons) / 0x121
// (three buttons); RT_DIALOG templates dumped from gamemd.exe: GUI:Blank text
// at (40,40,220,50) DLU, OK/[extra]/Cancel stacked at the bottom right]. Texts
// are the CSF labels: GUI:ExitAreYouSure + OK/Cancel for Exit Game;
// GUI:AskAbortMission with Abort / Restart (campaign, STT:ConfirmExitButton-
// Restart) / Resume (STT:ConfirmExitButtonResume) for Abort Mission.
// [ANCHORED: the label->button pairing is from the STT tooltips + templates.]
class GfConfirm {
public:
    bool open() const { return open_; }
    void ask(const std::string& text, std::vector<std::string> buttons) {
        open_ = true;
        text_ = text;
        buttons_ = std::move(buttons);
    }
    void close() { open_ = false; }
    // -1 = nothing / still open, else the index of the clicked button. A Quit
    // (Esc) answers the LAST button (Cancel / Resume).
    int handle(const ra2engine::InputEvent& ev) {
        if (!open_) return -1;
        if (ev.kind == ra2engine::InputEvent::Quit) { open_ = false; return (int)buttons_.size() - 1; }
        if (ev.kind != ra2engine::InputEvent::PointerDown || ev.button != ra2engine::InputEvent::Left)
            return -1;
        for (int i = 0; i < (int)buttons_.size(); ++i)
            if (btn(i).contains(ev.x, ev.y)) { open_ = false; return i; }
        return -1;
    }
    void render(SDL_Renderer* ren) const {
        if (!open_) return;
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 150);
        SDL_Rect full{0, 0, 1280, 720};
        SDL_RenderFillRect(ren, &full);
        const SDL_Rect p = panel();
        SDL_SetRenderDrawColor(ren, 18, 20, 26, 255);
        SDL_RenderFillRect(ren, &p);
        SDL_SetRenderDrawColor(ren, 200, 60, 50, 255);
        SDL_RenderDrawRect(ren, &p);
        // Word-wrap the message into the text box (40..260 DLU wide).
        std::string line, word;
        int y = p.y + 50;
        const int maxc = (p.w - 80) / 12;
        auto flush = [&]() {
            if (line.empty()) return;
            draw_text(ren, line, p.x + 40, y, 2, 235, 220, 150);
            y += 24;
            line.clear();
        };
        for (std::size_t i = 0; i <= text_.size(); ++i) {
            const char ch = i < text_.size() ? text_[i] : ' ';
            if (ch == ' ' || ch == '\n') {
                if ((int)(line.size() + word.size() + 1) > maxc) flush();
                if (!word.empty()) line += (line.empty() ? "" : " ") + word;
                word.clear();
                if (ch == '\n') flush();
            } else {
                word += ch;
            }
        }
        flush();
        for (int i = 0; i < (int)buttons_.size(); ++i) {
            const ra2engine::SbRect r = btn(i);
            SDL_Rect s{r.x, r.y, r.w, r.h};
            SDL_SetRenderDrawColor(ren, 70, 26, 24, 255);
            SDL_RenderFillRect(ren, &s);
            SDL_SetRenderDrawColor(ren, 210, 70, 60, 255);
            SDL_RenderDrawRect(ren, &s);
            const int lw = (int)buttons_[(std::size_t)i].size() * 12;
            draw_text(ren, buttons_[(std::size_t)i], r.x + (r.w - lw) / 2, r.y + 8, 2, 235, 235, 240);
        }
    }
    ra2engine::SbRect btn(int i) const {
        // Template DLU (207, 175 - 20*(n-1-i), 83, 15) on a 300x200 DLU box,
        // scaled x2 (600x400 px) and centred on the 1280x720 window.
        const SDL_Rect p = panel();
        const int n = (int)buttons_.size();
        return {p.x + 207 * 2, p.y + (175 - 20 * (n - 1 - i)) * 2, 83 * 2, 15 * 2};
    }
private:
    static SDL_Rect panel() { return {(1280 - 600) / 2, (720 - 400) / 2, 600, 400}; }
    bool open_ = false;
    std::string text_;
    std::vector<std::string> buttons_;
};

inline std::string gf_csf(const std::string& label, const char* fallback) {
    std::string t = camp::csf_text(env_install_dir(), label);
    return t == label ? std::string(fallback) : t;
}

// Restart = the same mission/config from its loading screen (defined in gf_result.inc).
Scene* gf_make_restart_scene(bool no_audio, const ra2engine::SkirmishConfig& cfg);

// gF G-17 [RE: multihouse.md §1, CONFIRMED [Countries] + per-country Side=]:
// the lobby picks a COUNTRY; the side follows from its rules Side= (GDI ->
// Allied, Nod -> Soviet). The playable list is the [Countries] order minus
// YuriCountry (Yuri's side is a non-goal) and the passive/civilian houses.
inline const std::vector<std::string>& gf_lobby_countries() {
    static const std::vector<std::string> k = {"Americans", "Alliance", "French", "Germans",
                                               "British", "Africans", "Arabs", "Confederation",
                                               "Russians"};
    return k;
}
inline ra2engine::World::Side gf_side_of_country(const Rules& rules, const std::string& c) {
    std::string side = rules.section_string(c, "Side", "");
    for (char& ch : side) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    if (side == "NOD") return ra2engine::World::Side::Soviet;
    if (side == "GDI") return ra2engine::World::Side::Allied;
    const auto& k = gf_lobby_countries();   // rules without Side=: the documented split
    const auto it = std::find(k.begin(), k.end(), c);
    return (it != k.end() && it - k.begin() >= 5) ? ra2engine::World::Side::Soviet
                                                 : ra2engine::World::Side::Allied;
}
inline void gf_next_country(const Rules& rules, ra2engine::SkirmishSlot& s) {
    const auto& k = gf_lobby_countries();
    const std::string cur = s.country.empty() ? gf_default_country(s.side) : s.country;
    auto it = std::find(k.begin(), k.end(), cur);
    const std::size_t i = (it == k.end()) ? 0 : (std::size_t)((it - k.begin() + 1) % (long)k.size());
    s.country = k[i];
    s.side = gf_side_of_country(rules, s.country);
}
// Display label: the country's CSF UIName ("Name:Americans").
inline std::string gf_country_label(const ra2engine::SkirmishSlot& s) {
    const std::string c = s.country.empty() ? gf_default_country(s.side) : s.country;
    std::string t = camp::csf_text(env_install_dir(), "Name:" + c);
    if (t == "Name:" + c) t = c;
    for (char& ch : t) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return t.size() > 11 ? t.substr(0, 11) : t;
}
