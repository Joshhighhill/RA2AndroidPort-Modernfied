#pragma once
// Pure ore-field state: per-cell tiberium kind/density/variant from the map's
// overlay, harvester consumption, and the growth/spread timer.
// Calibration: docs/re/economy.md (§ refs inline). No SDL/OS.
#include <cstdint>
#include <vector>
#include "ra2assets/map.hpp"

namespace ra2engine {

enum class OreKind : std::uint8_t { None = 0, Ore = 1, Gems = 2 };

// [RE-GATE APPLIED x2: economy.md §5, RESOLVED] the binary classifies
// range-based (FUN_005FD510): overlay index i belongs to tiberium t iff
// i is in [base_t, base_t + NumImages=12) flat images or the following
// NumSlopes=8 slope frames. The runtime overlay index is the 0-based
// ORDER-OF-APPEARANCE position in [OverlayTypes] (NOT key-1 — the INI
// keys have gaps): positions GEM01=27, TIB01=102, TIB2_01=127,
// TIB3_01=147 — exact match to the recovered binary bases, and Task 2's
// real-map dump observed exactly indices {27,102,127} with data bytes
// 0..11 (variant-indexed model CONFIRMED). The 20-entry TIB blocks
// decompose as 12 flat images + 8 slope variants (TIB13..TIB20).
// FINAL table: gems [27,39); ore [102,122) + [127,147) + [147,167)
// (each = 12 flat + 8 slope; slope cells classify as ore too).
OreKind overlay_ore_kind(int overlay_index);
// 0-based art variant within its set (TIB07 -> 6); -1 for non-ore indices.
int overlay_ore_variant(int overlay_index);

// [RE-GATE APPLIED: economy.md §4 CONFIRMED] density byte is 0-based,
// cap 11 (each tiberium overlay has NumImages = 12 density frames).
constexpr int kMaxOreDensity = 11;

// Deterministic engine-local RNG (xorshift32) for growth/spread selection.
// DOCUMENTED DEVIATION: the original uses its global Randomizer; matching its
// stream is unreachable, so selection RULES are calibrated, the stream is ours.
struct OreRng {
    std::uint32_t s = 0x0EC0417Au;   // fixed seed: demo determinism
    std::uint32_t next();            // xorshift32; never returns with s==0
    int below(int n);                // uniform [0, n), n >= 1
};

class OreField {
public:
    OreField() = default;
    OreField(int w, int h);                       // empty field (tests)
    // Build from the map: for every terrain cell, classify overlay_at via
    // overlay_ore_kind; density = clamp(overlay_data_at, 0, kMaxOreDensity).
    static OreField from_map(const ra2assets::MapFile& map, int grid_w, int grid_h);

    int w() const;
    int h() const;
    bool in(int x, int y) const;
    OreKind kind_at(int x, int y) const;          // None when out of range
    int density_at(int x, int y) const;           // 0 when out of range/None
    int variant_at(int x, int y) const;           // art variant, 0 default
    bool has_ore(int x, int y) const;             // kind != None (density >= 0 means >=1 bale: see take_bale)
    void set(int x, int y, OreKind k, int density, int variant);

    // One gulp [RE-GATE: economy.md §1]: pre-hunt, a gulp removes one
    // density step; the cell empties (kind -> None) when density hits 0.
    // Returns the kind consumed, or None if the cell had nothing.
    OreKind take_bale(int x, int y);

    // Growth/spread timer [RE-GATE APPLIED: economy.md §4 — per-type
    // fields, GrowthRate cadence, cap 11, and spread-from-fully-grown are
    // recovered; the exact per-firing candidate-drain slice is
    // [RE-FALLBACK], adjudicated to the shape below; RNG = engine-local
    // seeded xorshift32 (documented deviation, README)]. Call once per sim
    // tick; fires every `interval_secs`. Per firing:
    //   growth: candidates = ore cells with density < kMaxOreDensity;
    //     n = max(1, (int)(candidates * growth_pct)) if candidates else 0;
    //     pick n via rng without replacement; density += 1.
    //   spread: sources = ore cells with density == kMaxOreDensity;
    //     n = max(1, (int)(sources * spread_pct)) if sources else 0; each
    //     picks one rng 8-neighbor that is in-grid, passable, empty of ore
    //     -> new Ore cell, density 1 (variant copied from the source).
    //   Gems (GrowthPercentage=0 in rulesmd) never grow or spread.
    // `passable` is the World passability grid (same w/h).
    void step(double dt, double interval_secs, double growth_pct, double spread_pct,
              const std::vector<std::uint8_t>& passable, OreRng& rng);

    double timer() const;                          // accumulated seconds (tests)
private:
    int w_ = 0, h_ = 0;
    std::vector<std::uint8_t> kind_, density_, variant_;
    double timer_ = 0.0;
};

}  // namespace ra2engine
