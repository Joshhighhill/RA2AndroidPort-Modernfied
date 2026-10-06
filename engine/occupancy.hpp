#pragma once
#include <vector>

namespace ra2engine {

// A width*height grid mapping each cell to the index of the unit that owns it
// (its current cell) or is reserving it (the next cell it is stepping into),
// or kEmpty. Pure: no SDL/OS deps, so it is unit-testable and compiled into
// both ra2assets_tests and ra2engine (see CMakeLists.txt). Off-grid
// coordinates read as kEmpty and are never enterable; all mutators no-op
// off-grid.
class Occupancy {
public:
    friend struct SaveAccess;  // P7 Save/Load (engine/save_fields.hpp)
    static constexpr int kEmpty = -1;

    Occupancy() = default;
    Occupancy(int width, int height)
        : w_(width < 0 ? 0 : width), h_(height < 0 ? 0 : height),
          owner_(static_cast<std::size_t>(w_) * h_, kEmpty) {}

    int width() const { return w_; }
    int height() const { return h_; }

    int at(int x, int y) const {
        if (!in(x, y)) return kEmpty;
        return owner_[idx(x, y)];
    }
    bool is_free(int x, int y) const { return in(x, y) && owner_[idx(x, y)] == kEmpty; }

    // unit may enter (x,y) iff it is in-grid AND (free OR already owned by unit).
    bool can_enter(int x, int y, int unit) const {
        if (!in(x, y)) return false;
        const int o = owner_[idx(x, y)];
        return o == kEmpty || o == unit;
    }

    void set(int x, int y, int unit) {
        if (in(x, y)) owner_[idx(x, y)] = unit;
    }
    void clear(int x, int y) {
        if (in(x, y)) owner_[idx(x, y)] = kEmpty;
    }
    // Clears (x,y) only if it is currently owned by `unit` (so a unit releasing
    // its old cell cannot stomp a claim another unit already made there).
    void release(int x, int y, int unit) {
        if (in(x, y) && owner_[idx(x, y)] == unit) owner_[idx(x, y)] = kEmpty;
    }
    // Clear EVERY cell owned by `unit` except (keep_x, keep_y). Used when a
    // unit is re-ordered mid-walk: it may hold a reservation on a next cell it
    // will now never enter, which would otherwise stay owned forever and block
    // other units. O(w*h) scan -- called only on user orders, not per frame.
    void release_all_of(int unit, int keep_x, int keep_y) {
        for (int y = 0; y < h_; ++y)
            for (int x = 0; x < w_; ++x)
                if (owner_[idx(x, y)] == unit && !(x == keep_x && y == keep_y))
                    owner_[idx(x, y)] = kEmpty;
    }

private:
    bool in(int x, int y) const { return x >= 0 && y >= 0 && x < w_ && y < h_; }
    std::size_t idx(int x, int y) const {
        return static_cast<std::size_t>(y) * w_ + x;
    }
    int w_ = 0, h_ = 0;
    std::vector<int> owner_;
};

}  // namespace ra2engine
