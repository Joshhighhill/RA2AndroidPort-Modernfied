#include "ore.hpp"

#include <algorithm>

namespace ra2engine {

namespace {
// [RE-GATE APPLIED x2: economy.md §5, RESOLVED] Overlay bases, order-of-
// appearance positions in [OverlayTypes]: GEM01=27, TIB01=102, TIB2_01=127,
// TIB3_01=147. Each ORE tiberium's block is NumImages=12 flat density frames
// followed by NumSlopes=8 slope frames (FUN_005FD510's two-range test) --
// width 20, [base, base+20). GEM's block has only the 12 flat images (no
// slope range: "GEM has only the 12 flat images -- the slope range is
// empty/unused for gems") -- width 12, [base, base+12).
struct TibBase {
    int base;
    int width;
    OreKind kind;
};

constexpr TibBase kBases[] = {
    {27, 12, OreKind::Gems},    // GEM01..    [27,39)
    {102, 20, OreKind::Ore},    // TIB01..    [102,122)  (Riparius)
    {127, 20, OreKind::Ore},    // TIB2_01..  [127,147)
    {147, 20, OreKind::Ore},    // TIB3_01..  [147,167)
};
}  // namespace

OreKind overlay_ore_kind(int overlay_index) {
    for (const auto& b : kBases) {
        if (overlay_index >= b.base && overlay_index < b.base + b.width) {
            return b.kind;
        }
    }
    return OreKind::None;
}

int overlay_ore_variant(int overlay_index) {
    for (const auto& b : kBases) {
        if (overlay_index >= b.base && overlay_index < b.base + b.width) {
            int off = overlay_index - b.base;
            // [RE-GATE APPLIED: economy.md §5] slope frames (off >= 12) are
            // unused this phase (slope art not modeled) -- clamp to variant 0
            // rather than an out-of-range flat index.
            return off < 12 ? off : 0;
        }
    }
    return -1;
}

std::uint32_t OreRng::next() {
    if (s == 0) s = 1;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

int OreRng::below(int n) {
    return (int)(next() % (std::uint32_t)n);
}

OreField::OreField(int w, int h)
    : w_(w), h_(h), kind_((size_t)w * h, (std::uint8_t)OreKind::None),
      density_((size_t)w * h, 0), variant_((size_t)w * h, 0) {}

OreField OreField::from_map(const ra2assets::MapFile& map, int grid_w, int grid_h) {
    OreField f(grid_w, grid_h);
    for (const auto& c : map.cells()) {
        if (!f.in(c.x, c.y)) continue;
        int overlay = map.overlay_at(c.x, c.y);
        OreKind kind = overlay_ore_kind(overlay);
        if (kind == OreKind::None) continue;
        int variant = overlay_ore_variant(overlay);
        int density = map.overlay_data_at(c.x, c.y);
        density = std::max(0, std::min(density, kMaxOreDensity));
        f.set(c.x, c.y, kind, density, variant);
    }
    return f;
}

int OreField::w() const { return w_; }
int OreField::h() const { return h_; }

bool OreField::in(int x, int y) const {
    return x >= 0 && y >= 0 && x < w_ && y < h_;
}

OreKind OreField::kind_at(int x, int y) const {
    if (!in(x, y)) return OreKind::None;
    return (OreKind)kind_[(size_t)x + (size_t)y * w_];
}

int OreField::density_at(int x, int y) const {
    if (!in(x, y)) return 0;
    if (kind_at(x, y) == OreKind::None) return 0;
    return density_[(size_t)x + (size_t)y * w_];
}

int OreField::variant_at(int x, int y) const {
    if (!in(x, y)) return 0;
    return variant_[(size_t)x + (size_t)y * w_];
}

bool OreField::has_ore(int x, int y) const {
    return kind_at(x, y) != OreKind::None;
}

void OreField::set(int x, int y, OreKind k, int density, int variant) {
    if (!in(x, y)) return;
    size_t i = (size_t)x + (size_t)y * w_;
    kind_[i] = (std::uint8_t)k;
    density_[i] = (std::uint8_t)std::max(0, std::min(density, kMaxOreDensity));
    variant_[i] = (std::uint8_t)variant;
}

OreKind OreField::take_bale(int x, int y) {
    if (!in(x, y)) return OreKind::None;
    size_t i = (size_t)x + (size_t)y * w_;
    OreKind k = (OreKind)kind_[i];
    if (k == OreKind::None) return OreKind::None;
    // [RE-GATE: economy.md §1] one gulp removes one density step; the cell
    // empties (kind -> None) when density hits 0.
    if (density_[i] > 0) --density_[i];
    if (density_[i] == 0) {
        kind_[i] = (std::uint8_t)OreKind::None;
    }
    return k;
}

void OreField::step(double dt, double interval_secs, double growth_pct, double spread_pct,
                     const std::vector<std::uint8_t>& passable, OreRng& rng) {
    timer_ += dt;
    while (timer_ >= interval_secs) {
        timer_ -= interval_secs;
        // -- growth [RE-GATE: economy.md §4] --
        std::vector<int> cand;
        for (int i = 0; i < w_ * h_; ++i)
            if (kind_[i] == (std::uint8_t)OreKind::Ore && density_[i] < kMaxOreDensity)
                cand.push_back(i);
        if (!cand.empty() && growth_pct > 0.0) {
            int n = std::max(1, (int)(cand.size() * growth_pct));
            for (int k = 0; k < n && !cand.empty(); ++k) {
                int j = rng.below((int)cand.size());
                ++density_[cand[j]];
                cand.erase(cand.begin() + j);
            }
        }
        // -- spread [RE-GATE: economy.md §4] --
        std::vector<int> src;
        for (int i = 0; i < w_ * h_; ++i)
            if (kind_[i] == (std::uint8_t)OreKind::Ore && density_[i] == kMaxOreDensity)
                src.push_back(i);
        if (!src.empty() && spread_pct > 0.0) {
            int n = std::max(1, (int)(src.size() * spread_pct));
            for (int k = 0; k < n; ++k) {
                int i = src[rng.below((int)src.size())];
                int sx = i % w_, sy = i / w_;
                static const int dx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
                static const int dy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
                int start = rng.below(8);
                for (int d = 0; d < 8; ++d) {
                    int nx = sx + dx[(start + d) % 8], ny = sy + dy[(start + d) % 8];
                    if (!in(nx, ny) || kind_at(nx, ny) != OreKind::None) continue;
                    if (!passable[(size_t)nx + (size_t)ny * w_]) continue;
                    set(nx, ny, OreKind::Ore, 1, variant_at(sx, sy));
                    break;
                }
            }
        }
    }
}

double OreField::timer() const { return timer_; }

}  // namespace ra2engine
