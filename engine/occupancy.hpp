#pragma once
#include <cstdint>
#include <vector>

namespace ra2engine {

// A width*height grid mapping each cell to the index of the unit that owns it
// (its current cell) or is reserving it (the next cell it is stepping into),
// or kEmpty. Pure: no SDL/OS deps, so it is unit-testable and compiled into
// both ra2assets_tests and ra2engine (see CMakeLists.txt). Off-grid
// coordinates read as kEmpty and are never enterable; all mutators no-op
// off-grid.
//
// W5-B R-061 infantry sub-cells [RE: CellClass::FindInfantrySubposition
// FUN_00481090; OccupationFlags 0x1 centre, 0x2 top (unused), 0x4 right,
// 0x8 left, 0x10 down, 0x20 vehicle]: once enable_subcells() is called, a
// unit marked as infantry (mark_infantry) holds one of the three spots 2/3/4
// of a cell instead of the whole cell -- up to three infantry share a cell --
// while a vehicle still takes the whole cell. An infantry may not enter a
// cell a vehicle holds; a vehicle may not enter a cell with infantry in it.
// Without enable_subcells() every unit is whole-cell (the pre-W5 behaviour).
class Occupancy {
public:
    friend struct SaveAccess;  // P7 Save/Load (engine/save_fields.hpp)
    static constexpr int kEmpty = -1;
    static constexpr int kSpots = 3;     // spots 2, 3, 4
    static constexpr int kFirstSpot = 2;

    Occupancy() = default;
    Occupancy(int width, int height)
        : w_(width < 0 ? 0 : width), h_(height < 0 ? 0 : height),
          owner_(static_cast<std::size_t>(w_) * h_, kEmpty) {}

    int width() const { return w_; }
    int height() const { return h_; }

    // --- sub-cell mode -------------------------------------------------------
    void enable_subcells() {
        if (spots_.empty()) spots_.assign(static_cast<std::size_t>(w_) * h_ * kSpots, kEmpty);
    }
    bool subcells() const { return !spots_.empty(); }
    void mark_infantry(int unit, bool infantry) {
        if (unit < 0) return;
        if (static_cast<std::size_t>(unit) >= inf_.size()) inf_.resize(static_cast<std::size_t>(unit) + 1, 0);
        inf_[static_cast<std::size_t>(unit)] = infantry ? 1 : 0;
    }
    bool is_infantry(int unit) const {
        return subcells() && unit >= 0 && static_cast<std::size_t>(unit) < inf_.size() &&
               inf_[static_cast<std::size_t>(unit)] != 0;
    }
    // Occupant of spot s (2..4) of (x,y), kEmpty if free/off-grid.
    int spot_unit(int x, int y, int s) const {
        if (!in(x, y) || !subcells() || s < kFirstSpot || s >= kFirstSpot + kSpots) return kEmpty;
        return spots_[sidx(x, y, s)];
    }
    // The spot (2..4) `unit` holds in (x,y), or -1.
    int spot_of(int x, int y, int unit) const {
        if (!in(x, y) || !subcells()) return -1;
        for (int s = kFirstSpot; s < kFirstSpot + kSpots; ++s)
            if (spots_[sidx(x, y, s)] == unit) return s;
        return -1;
    }
    int infantry_count(int x, int y) const {
        if (!in(x, y) || !subcells()) return 0;
        int n = 0;
        for (int s = kFirstSpot; s < kFirstSpot + kSpots; ++s) n += spots_[sidx(x, y, s)] != kEmpty;
        return n;
    }
    // FindInfantrySubposition's choice: `pref` (0..4, the quadrant of the
    // approach point) if it is a free infantry spot, else the first free entry
    // of its preference row (DAT_0081BC94: 0->1,2,3,4  1->0,2,3,4  2->0,1,4,3
    // 3->0,1,4,2  4->0,2,3,1; spots 0/1 are never taken). pref 0 uses the
    // rotation row `rot` (0..3) of DAT_0081BCA8 (the original draws it from
    // the sync RNG). -1 = no free spot, or a vehicle holds the cell.
    int free_spot(int x, int y, int pref, int rot = 0) const {
        if (!in(x, y) || !subcells() || owner_[idx(x, y)] != kEmpty) return -1;
        static const signed char kRows[5][4] = {{1, 2, 3, 4}, {0, 2, 3, 4}, {0, 1, 4, 3},
                                                {0, 1, 4, 2}, {0, 2, 3, 1}};
        static const signed char kRot[4][4] = {{1, 2, 3, 4}, {2, 3, 4, 1}, {3, 4, 1, 2},
                                               {4, 1, 2, 3}};
        if (pref >= kFirstSpot && pref < kFirstSpot + kSpots && spots_[sidx(x, y, pref)] == kEmpty)
            return pref;
        const signed char* row = (pref <= 0 || pref > 4) ? kRot[rot & 3] : kRows[pref];
        for (int k = 0; k < 4; ++k) {
            const int s = row[k];
            if (s >= kFirstSpot && spots_[sidx(x, y, s)] == kEmpty) return s;
        }
        return -1;
    }
    // Put infantry `unit` in spot s of (x,y) (off any other spot of that cell).
    void set_spot(int x, int y, int unit, int s) {
        if (!in(x, y) || !subcells() || s < kFirstSpot || s >= kFirstSpot + kSpots) return;
        for (int k = kFirstSpot; k < kFirstSpot + kSpots; ++k)
            if (spots_[sidx(x, y, k)] == unit) spots_[sidx(x, y, k)] = kEmpty;
        if (owner_[idx(x, y)] == unit) owner_[idx(x, y)] = kEmpty;
        spots_[sidx(x, y, s)] = unit;
    }

    // --- whole-cell view (every pre-W5 caller) -------------------------------
    // The vehicle holding the cell, else the first infantry (spot order), else
    // kEmpty.
    int at(int x, int y) const {
        if (!in(x, y)) return kEmpty;
        const int o = owner_[idx(x, y)];
        if (o != kEmpty || !subcells()) return o;
        for (int s = kFirstSpot; s < kFirstSpot + kSpots; ++s)
            if (spots_[sidx(x, y, s)] != kEmpty) return spots_[sidx(x, y, s)];
        return kEmpty;
    }
    bool is_free(int x, int y) const { return in(x, y) && at(x, y) == kEmpty; }

    // unit may enter (x,y) iff it is in-grid AND (free OR already owned by
    // unit). Sub-cell mode: an infantry needs no vehicle there and a free spot
    // (or one it already holds); a vehicle needs the cell clear of infantry.
    bool can_enter(int x, int y, int unit) const {
        if (!in(x, y)) return false;
        const int o = owner_[idx(x, y)];
        if (!subcells()) return o == kEmpty || o == unit;
        if (is_infantry(unit)) {
            if (o != kEmpty && o != unit) return false;
            return spot_of(x, y, unit) >= 0 || free_spot(x, y, 0) >= 0;
        }
        if (o != kEmpty && o != unit) return false;
        for (int s = kFirstSpot; s < kFirstSpot + kSpots; ++s) {
            const int v = spots_[sidx(x, y, s)];
            if (v != kEmpty && v != unit) return false;
        }
        return true;
    }

    // Claim (x,y) for `unit`: an infantry keeps the spot it already holds there,
    // else takes the first free one (spot order 2,3,4; a full cell's spot 2 is
    // overwritten, the pre-W5 "set overwrites" contract); a vehicle owns the
    // whole cell.
    void set(int x, int y, int unit) {
        if (!in(x, y)) return;
        if (!is_infantry(unit)) {
            owner_[idx(x, y)] = unit;
            return;
        }
        if (owner_[idx(x, y)] == unit) owner_[idx(x, y)] = kEmpty;
        if (spot_of(x, y, unit) >= 0) return;
        int s = -1;
        for (int k = kFirstSpot; k < kFirstSpot + kSpots && s < 0; ++k)
            if (spots_[sidx(x, y, k)] == kEmpty) s = k;
        spots_[sidx(x, y, s < 0 ? kFirstSpot : s)] = unit;
    }
    void clear(int x, int y) {
        if (!in(x, y)) return;
        owner_[idx(x, y)] = kEmpty;
        if (subcells())
            for (int s = kFirstSpot; s < kFirstSpot + kSpots; ++s) spots_[sidx(x, y, s)] = kEmpty;
    }
    // Clears (x,y) only if it is currently owned by `unit` (so a unit releasing
    // its old cell cannot stomp a claim another unit already made there).
    void release(int x, int y, int unit) {
        if (!in(x, y)) return;
        if (owner_[idx(x, y)] == unit) owner_[idx(x, y)] = kEmpty;
        if (subcells())
            for (int s = kFirstSpot; s < kFirstSpot + kSpots; ++s)
                if (spots_[sidx(x, y, s)] == unit) spots_[sidx(x, y, s)] = kEmpty;
    }
    // Clear EVERY cell owned by `unit` except (keep_x, keep_y). Used when a
    // unit is re-ordered mid-walk: it may hold a reservation on a next cell it
    // will now never enter, which would otherwise stay owned forever and block
    // other units. O(w*h) scan -- called only on user orders, not per frame.
    void release_all_of(int unit, int keep_x, int keep_y) {
        for (int y = 0; y < h_; ++y)
            for (int x = 0; x < w_; ++x) {
                if (x == keep_x && y == keep_y) continue;
                if (owner_[idx(x, y)] == unit) owner_[idx(x, y)] = kEmpty;
                if (subcells())
                    for (int s = kFirstSpot; s < kFirstSpot + kSpots; ++s)
                        if (spots_[sidx(x, y, s)] == unit) spots_[sidx(x, y, s)] = kEmpty;
            }
    }

private:
    bool in(int x, int y) const { return x >= 0 && y >= 0 && x < w_ && y < h_; }
    std::size_t idx(int x, int y) const {
        return static_cast<std::size_t>(y) * w_ + x;
    }
    std::size_t sidx(int x, int y, int s) const {
        return idx(x, y) * kSpots + static_cast<std::size_t>(s - kFirstSpot);
    }
    int w_ = 0, h_ = 0;
    std::vector<int> owner_;
    std::vector<int> spots_;            // W5-B: 3 infantry spots per cell (sub-cell mode)
    std::vector<std::uint8_t> inf_;     // W5-B: per unit index, 1 = infantry
};

// W5-B R-061 [RE: spot coordinates DAT_0089D960, static initializer
// 0x48E390]: spot k's offset from the cell centre in leptons -- 0 centre
// (128,128), 1 top (64,64), 2 right (192,64), 3 left (64,192), 4 down
// (192,192), minus the centre.
inline void spot_offset(int spot, int& dx, int& dy) {
    static const signed char kOff[5][2] = {{0, 0}, {-64, -64}, {64, -64}, {-64, 64}, {64, 64}};
    if (spot < 0 || spot > 4) spot = 0;
    dx = kOff[spot][0];
    dy = kOff[spot][1];
}

// FindInfantrySubposition's quadrant of a point (in-cell offset from the cell
// centre, leptons): within 60 leptons of the centre -> 0; else x>0 -> +1,
// y>0 -> +2, nonzero +1 (so right 2, left 3, down 4; the top quadrant -> 0).
inline int spot_quadrant(int ox, int oy) {
    if (ox * ox + oy * oy < 60 * 60) return 0;
    const int q = (ox > 0 ? 1 : 0) | (oy > 0 ? 2 : 0);
    return q ? q + 1 : 0;
}

}  // namespace ra2engine
