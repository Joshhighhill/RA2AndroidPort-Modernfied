// hud_pips.inc -- W2-UI-11: PIPBRD.SHP / PIPS.SHP health bars, the original's
// selection marker. Included by main.cpp after hud_text.inc. Interactive only:
// g_pips is set by GameScene (init_presentation), so every gate / headless
// render path keeps its old drawing.
//
// [RE: game.exe TechnoClass::DrawHealthBar 0x6C3F00 (+ 0x6C4260 unit tail),
//  art loaded by FUN_005D5E00 (PIPBRD / PIPS / PIPS2) -- D:\ra2-re\w2fix-uiout]
//  * non-building, at the object's screen point (x, y):
//      selected -> PIPBRD frame 1 (infantry) at (x+11, y-25), else frame 0 at
//      (x+1, y-26), flags 0xE00 (centred);
//      pips: infantry max 8 from (x-5, y-24), others max 17 from (x-15, y-25),
//      2 px apart, PIPS frame 15 green / 16 (ratio <= ConditionYellow) /
//      17 (ratio <= ConditionRed), flags 0x600 (centred);
//      count = ftol(ratio * max), at least 1, at most max.
//  * building: N = half the screen height of the foundation's Y edge, pips
//      4 px apart stepping (-4, +2) from the upper-right end of the line;
//      the filled ones use PIPS frame 1 green / 2 yellow / 4 red, the rest
//      frame 0 (empty).
//  * palette: cache.mix/palette.pal (frames 15/16/17 decode green / yellow / red
//      under it only).
// The building line sits on the top of the building's 3-D box [RE: DrawHealth
// Bar FUN_006c3f00 building branch projects the type's Dimension2 corner
// (-w/2, +d/2, z); BuildingType Dimension2 @45D8B0 = (FoundationW * 256,
// FoundationH * 256, art Height * CellHeight), so the lift is Height levels
// = Height x 15 px (W6-05-09 / R-145: GAPOWR Height=4 really does sit that
// high); Height defaults to 2]. [RE-FALLBACK: the unit anchor is the ground
// point the clone already uses (raised by the flight altitude).]

struct PipArt {
    ra2assets::ShpFile pipbrd, pips;
    ra2assets::Palette pal;
    bool ok = false;
    ra2assets::ShpFile bomb;   // W6-04-11: conquer.mix bomb.shp (the Ivan bomb icon)
    bool has_bomb = false;
};
const PipArt* g_pips = nullptr;
// [AudioVisual] ConditionYellow / ConditionRed (set with g_pips by GameScene).
double g_pip_yellow = 0.5, g_pip_red = 0.25;

void pips_load(const ra2assets::MixFile& ra2) {
    static PipArt art;
    if (art.ok) { g_pips = &art; return; }
    try {
        ra2assets::MixFile conquer = ra2.open_nested("conquer.mix");
        ra2assets::MixFile cache = ra2.open_nested("cache.mix");
        const ra2assets::MixEntry* b = conquer.find(std::string("pipbrd.shp"));
        const ra2assets::MixEntry* p = conquer.find(std::string("pips.shp"));
        const ra2assets::MixEntry* pe = cache.find(std::string("palette.pal"));
        if (b && p && pe) {
            art.pipbrd = ra2assets::ShpFile::load(conquer.read(*b));
            art.pips = ra2assets::ShpFile::load(conquer.read(*p));
            const auto raw = cache.read(*pe);
            art.pal = ra2assets::Palette::load(raw.data(), raw.size());
            art.ok = art.pipbrd.count() >= 2 && art.pips.count() >= 18;
        }
        if (const ra2assets::MixEntry* bm = conquer.find(std::string("bomb.shp"))) {   // W6-04-11
            art.bomb = ra2assets::ShpFile::load(conquer.read(*bm));
            art.has_bomb = art.bomb.count() > 0;
        }
    } catch (const std::exception& ex) {
        std::cerr << "[pips] unavailable: " << ex.what() << "\n";
    }
    g_pips = art.ok ? &art : nullptr;
}

// A centred SHP draw (flags 0x200): the frame CANVAS is centred on (x, y).
inline void blit_shp_centered(SDL_Renderer* ren, ra2engine::TextureCache& tc, const std::string& key,
                              const ra2assets::ShpFile& shp, int frame, const ra2assets::Palette& pal, int x, int y) {
    blit_shp(ren, tc, key, shp, frame, pal, x - shp.width() / 2, y - shp.height() / 2);
}

inline int pip_frame_unit(double ratio, double yellow, double red) {
    int f = 15;
    if (ratio <= yellow) f = 16;
    if (ratio <= red) f = 17;
    return f;
}

// Selected unit: board + pips at the unit's screen point.
// W6-05-07 [RE: DrawHealthBar FUN_006c3f00 gates ONLY the PIPBRD board on
// IsSelected (+0x77); the pip row is drawn unconditionally, so a hovered,
// unselected object shows its pips without the board]: board = selected.
void draw_unit_pips(SDL_Renderer* ren, ra2engine::TextureCache& tc, const ra2engine::Unit& u, int x, int y,
                    double yellow, double red, bool board) {
    if (!g_pips || u.max_hp <= 0) return;
    const bool inf = !u.is_vehicle && !u.is_aircraft;
    if (board && inf) blit_shp_centered(ren, tc, "pip:brd1", g_pips->pipbrd, 1, g_pips->pal, x + 11, y - 25);
    else if (board) blit_shp_centered(ren, tc, "pip:brd0", g_pips->pipbrd, 0, g_pips->pal, x + 1, y - 26);
    const int max = inf ? 8 : 17;
    const double ratio = static_cast<double>(std::max(0, u.hp)) / u.max_hp;
    int n = static_cast<int>(ratio * max);
    if (n < 2) n = 1;   // FUN_0077F110 result < 2 -> 1
    if (n > max) n = max;
    const int f = pip_frame_unit(ratio, yellow, red);
    const int ox = inf ? -5 : -15, oy = inf ? -24 : -25;
    for (int i = 0; i < n; ++i)
        blit_shp_centered(ren, tc, "pip:" + std::to_string(f), g_pips->pips, f, g_pips->pal, x + ox + i * 2, y + oy);
}

// Selected building: the diagonal pip line along the foundation's top-left
// edge. (lx, ly) = the foundation's LEFT corner, (tx, ty) = its TOP corner.
void draw_building_pips(SDL_Renderer* ren, ra2engine::TextureCache& tc, int hp, int max_hp, int tx, int ty,
                        int fh_cells, int lift_px, double yellow, double red) {
    if (!g_pips || max_hp <= 0) return;
    const int n = std::max(1, fh_cells * 15 / 2);   // (screen dy of the Y edge) / 2
    const double ratio = static_cast<double>(std::max(0, hp)) / max_hp;
    int filled = static_cast<int>(ratio * n);
    if (filled < 2) filled = 1;
    if (filled > n) filled = n;
    int f = 1;
    if (ratio <= yellow) f = 2;
    if (ratio <= red) f = 4;
    // i = 0 at the upper-right (top corner) end, stepping (-4, +2).
    // RE: x = loc + s1.x + 3 + 4(N-i), y = loc + s1.y + 4 - 2(N-i), s1 = the left
    // corner of the box top -> i = 0 sits at the top corner (+3, +4).
    const int x0 = tx + 3, y0 = ty + 4 - lift_px;
    for (int i = 0; i < n; ++i) {
        const int fr = i < filled ? f : 0;
        blit_shp_centered(ren, tc, "pipb:" + std::to_string(fr), g_pips->pips, fr, g_pips->pal, x0 - 4 * i,
                          y0 + 2 * i);
    }
}

// W6-04-11: see the declaration in main.cpp.
void draw_bomb_icons(SDL_Renderer* ren, ra2engine::TextureCache& tc, const ra2engine::World& world,
                     const std::vector<ra2engine::Unit>& us, const ra2engine::Camera& cam) {
    if (g_pips && g_pips->has_bomb) {
        const std::uint64_t now = world.ability_frame();
        const int nf = g_pips->bomb.count();
        auto bomb_frame = [&](std::int64_t det) {
            const long left = std::max<long>(0, static_cast<long>(det - static_cast<std::int64_t>(now)));
            const int stage = std::clamp(5 - static_cast<int>(left * 6 / 451), 0, 5);
            const int f = stage * 2 + static_cast<int>((now / 8) & 1);
            return nf >= 12 ? f : f % nf;
        };
        for (const ra2engine::Unit& u : us) {
            if (u.bomb_frame < 0 || !u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) continue;
            if (world.unit_hidden_from(0, u) || !(u.seen_by_mask & 1u)) continue;
            const int up = (u.is_aircraft && u.altitude > 0.0) ? ra2engine::altitude_to_screen_dy(u.altitude) : 0;
            const double ufx = u.fx + u.sub_x / 256.0, ufy = u.fy + u.sub_y / 256.0;
            const int sx = static_cast<int>(std::lround((ufx - ufy) * 30.0)) + cam.x + 30;
            const int sy = static_cast<int>(std::lround((ufx + ufy) * 15.0 - u.level * 15)) + cam.y + 15 - up;
            blit_shp_centered(ren, tc, "w6bomb:" + std::to_string(bomb_frame(u.bomb_frame)), g_pips->bomb,
                              bomb_frame(u.bomb_frame), g_pips->pal, sx, sy - 34);
        }
        for (const ra2engine::Building& b : world.buildings()) {
            if (b.bomb_frame < 0 || !b.alive || b.state == ra2engine::Building::State::Dead) continue;
            const double cx = b.cx + (b.fw - 1) / 2.0, cy = b.cy + (b.fh - 1) / 2.0;
            const int sx = static_cast<int>(std::lround((cx - cy) * 30.0)) + cam.x + 30;
            const int sy = static_cast<int>(std::lround((cx + cy) * 15.0 - world.cell_level(b.cx, b.cy) * 15)) +
                           cam.y + 15 - 40;
            blit_shp_centered(ren, tc, "w6bomb:" + std::to_string(bomb_frame(b.bomb_frame)), g_pips->bomb,
                              bomb_frame(b.bomb_frame), g_pips->pal, sx, sy);
        }
    }
}
