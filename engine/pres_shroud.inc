// pres_shroud.inc -- G-84 shroud / fog post-pass. Included by main.cpp BEFORE
// render_world (which calls draw_shroud_pass after every object is drawn).
//
// The original draws shroud as a separate layer over the tactical view using
// SHROUD.SHP (conquer.mix, 96 frames of 60x30; the first 48 are the edge set)
// [RE: shroud.md §5, renderer region FUN_0047EEF0]. Each pixel is a BRIGHTNESS
// level (2 = black ... 127 = untouched; 254 = outside the diamond), i.e. the
// blitter scales the destination by v/127. Measured on this install: frame 15
// is uniformly 2 (a fully shrouded cell), frames 1..14 / 16..47 are soft edges.
//
// Frame selection [RE-FALLBACK: RE-P4 open -- the binary's neighbour-mask ->
// frame table is not yet read]: the TS/RA2 extended-edge scheme as implemented
// by OpenRA's ShroudRenderer for this very SHP layout (mods/ts world.yaml Index
// table, UseExtendedIndex): sides shroud their two corners, a corner-only
// neighbour selects the dedicated corner frames (edges ^ side-implied corners).
// Fog (explored, not visible) uses the same frames at half strength
// (BlendedFog=yes [RE-FALLBACK: shroud.md adjudicated the dim; 50 %]).

const ra2assets::ShpFile* g_shroud_shp = nullptr;   // set by GameScene (pres_game.inc)

namespace pres_shroud {

enum : uint8_t {
    TL = 0x01, TR = 0x02, BR = 0x04, BL = 0x08, AllCorners = 0x0F,
    TopS = 0x10, RightS = 0x20, BottomS = 0x40, LeftS = 0x80, AllSides = 0xF0,
    Top = TopS | TL | TR, Right = RightS | TR | BR, Bottom = BottomS | BR | BL,
    Left = LeftS | TL | BL,
};
// OpenRA mods/ts ShroudRenderer Index: frame i shows edge mask kIndex[i].
constexpr uint8_t kIndex[48] = {255, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240,
                                20,  40, 56, 65, 97, 130, 148, 194, 24, 33, 66, 132, 28, 41, 67, 134,
                                1,   2,  4,  8,  3,  6,   12,  9,   7,  14, 13, 11,  5,  10, 15, 255};

inline const std::array<int, 256>& frame_for_mask() {
    static const std::array<int, 256> t = [] {
        std::array<int, 256> a{};
        a.fill(-1);
        for (int i = 0; i < 48; ++i) a[kIndex[i]] = i;
        a[AllSides] = 15;   // fully covered
        return a;
    }();
    return t;
}

// Edge mask for cell (x,y) given a per-cell "clear" predicate (explored for
// shroud, visible for fog). Neighbours: Top=(0,-1) Right=(1,0) Bottom=(0,1)
// Left=(-1,0) and the four diagonals, the projection cell_to_screen uses.
template <class Clear>
uint8_t edges(int x, int y, Clear clear) {
    if (!clear(x, y)) return AllSides;
    uint8_t e = 0;
    if (!clear(x, y - 1)) e |= Top;
    if (!clear(x + 1, y)) e |= Right;
    if (!clear(x, y + 1)) e |= Bottom;
    if (!clear(x - 1, y)) e |= Left;
    const uint8_t ucorner = e & AllCorners;
    if (!clear(x - 1, y - 1)) e |= TL;
    if (!clear(x + 1, y - 1)) e |= TR;
    if (!clear(x + 1, y + 1)) e |= BR;
    if (!clear(x - 1, y + 1)) e |= BL;
    return static_cast<uint8_t>(e ^ ucorner);
}

// One shroud frame as an RGBA darkening texture (alpha = 1 - v/127).
inline SDL_Texture* frame_tex(SDL_Renderer*, ra2engine::TextureCache& tc, int frame, bool fog,
                              int& fx, int& fy, int& fw, int& fh) {
    const ra2assets::ShpFile& shp = *g_shroud_shp;
    const ra2assets::ShpFile::Frame f = shp.frame_info(frame);
    fx = f.x;
    fy = f.y;
    fw = f.w;
    fh = f.h;
    if (f.w <= 0 || f.h <= 0) return nullptr;
    char key[40];
    std::snprintf(key, sizeof key, "shroud:%c:%d", fog ? 'f' : 's', frame);
    SDL_Texture* tex = static_cast<SDL_Texture*>(tc.peek(key));
    if (tex) return tex;
    const std::vector<uint8_t> idx = shp.decode_indexed(frame);
    std::vector<uint8_t> rgba(static_cast<std::size_t>(f.w) * f.h * 4, 0);
    for (std::size_t i = 0; i < idx.size(); ++i) {
        const int v = idx[i];
        if (v == 0 || v > 127) continue;   // 254 = outside the diamond
        int a = (127 - v) * 255 / 127;
        if (fog) a /= 2;
        rgba[i * 4 + 3] = static_cast<uint8_t>(a);
    }
    return static_cast<SDL_Texture*>(tc.get_rgba(key, rgba.data(), f.w, f.h));
}

// Fallback when SHROUD.SHP is unavailable: a solid 60x30 diamond.
inline SDL_Texture* diamond_tex(ra2engine::TextureCache& tc, bool fog) {
    const char* key = fog ? "shroud:dfog" : "shroud:dsolid";
    SDL_Texture* tex = static_cast<SDL_Texture*>(tc.peek(key));
    if (tex) return tex;
    std::vector<uint8_t> rgba(60 * 30 * 4, 0);
    for (int y = 0; y < 30; ++y) {
        const int half = (y < 15 ? y + 1 : 30 - y) * 2;
        for (int x = 30 - half; x < 30 + half; ++x) rgba[(y * 60 + x) * 4 + 3] = fog ? 128 : 255;
    }
    return static_cast<SDL_Texture*>(tc.get_rgba(key, rgba.data(), 60, 30));
}

}  // namespace pres_shroud

// Draws fog then shroud over the whole view for house 0. No-op with shroud off.
void draw_shroud_pass(SDL_Renderer* ren, ra2engine::TextureCache& tc, const ra2assets::MapFile& map,
                      const ra2engine::World& world, const ra2engine::Camera& cam, int W, int H) {
    if (!world.shroud_enabled()) return;
    auto explored = [&](int x, int y) { return world.explored_at(0, x, y); };
    auto visible = [&](int x, int y) { return world.visible_at(0, x, y); };
    const auto& table = pres_shroud::frame_for_mask();
    for (int pass = 0; pass < 2; ++pass) {   // 0 fog, 1 shroud
        const bool fog = pass == 0;
        for (const ra2assets::MapCell& c : map.cells()) {
            const ra2engine::ScreenPt p =
                ra2engine::cell_to_screen(c.x, c.y, static_cast<int>(c.level), 60, 30, 15);
            const int sx = p.x + cam.x, sy = p.y + cam.y;
            if (sx + 60 <= 0 || sy + 30 <= 0 || sx >= W || sy >= H) continue;
            uint8_t e;
            if (fog) {
                if (!explored(c.x, c.y)) continue;   // the shroud pass covers it
                e = pres_shroud::edges(c.x, c.y, visible);
            } else {
                e = pres_shroud::edges(c.x, c.y, explored);
            }
            if (e == 0) continue;
            SDL_Texture* tex = nullptr;
            SDL_Rect dst{sx, sy, 60, 30};
            if (g_shroud_shp && g_shroud_shp->count() >= 48) {
                int fr = table[e];
                if (fr < 0) fr = 15;
                int fx, fy, fw, fh;
                tex = pres_shroud::frame_tex(ren, tc, fr, fog, fx, fy, fw, fh);
                dst = SDL_Rect{sx + fx, sy + fy, fw, fh};
            } else if (e == pres_shroud::AllSides) {
                tex = pres_shroud::diamond_tex(tc, fog);
            }
            if (tex) SDL_RenderCopy(ren, tex, nullptr, &dst);
        }
    }
}

// G-84: an enemy/neutral building is drawn once any foundation cell has been
// explored by house 0 (it then stays visible under fog, as in the original).
inline bool pres_building_seen(const ra2engine::World& world, const ra2engine::Building& b) {
    if (b.house == 0 || !world.shroud_enabled()) return true;
    for (int j = 0; j < b.fh; ++j)
        for (int i = 0; i < b.fw; ++i)
            if (world.explored_at(0, b.cx + i, b.cy + j)) return true;
    return false;
}
