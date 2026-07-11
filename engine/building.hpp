#pragma once
// Pure building core: type defs resolved from Rules/Art by the caller,
// foundation cell math, placement validity, damage/frame selection, and
// per-frame stepping (buildup progression, death anim timing). No SDL/OS.
// Calibration: docs/re/building-render.md (§ refs inline).
#include <string>
#include <vector>
#include "occupancy.hpp"
#include "pathfind.hpp"  // Cellxy

namespace ra2engine {

// One building type's stats, resolved ONCE by the caller (main.cpp from
// Rules/Art, or a test by hand). Index into the roster vector is the
// Building::type key used everywhere.
struct BuildingTypeDef {
    std::string id;                  // "GAPOWR"
    int fw = 1, fh = 1;              // Art Foundation=WxH
    int strength = 1;
    int armor_ix = 0;                // armor_index(Rules armor)
    int cost = 0;
    int power = 0;                   // + output, - drain
    int adjacent = 2;
    int tech_level = 0;
    bool conyard = false;
    std::vector<std::string> prerequisite;  // uppercased tokens (may be groups)
    std::string image;               // SHP image name (== id for all 8)
    std::string buildup;             // Art Buildup= ("" = place instantly Idle)
    std::string cameo;               // Art Cameo= ("" = never in sidebar)
    std::string explosion;           // Rules explosion_first(id)
};

// A placed building instance.
struct Building {
    int type = -1;                   // index into the roster
    int cx = 0, cy = 0;              // foundation ORIGIN cell [RE: building-render.md §2]
    int fw = 1, fh = 1;              // copied from the type at placement
    int house = 0;
    int hp = 1, max_hp = 1;
    enum class State { Buildup, Idle, Dead };
    State state = State::Buildup;
    double buildup_time = 0.0;       // seconds elapsed in Buildup
    double buildup_total = 0.0;      // seconds Buildup lasts (0 = instant)
    double die_time = -1.0;          // >=0: death anim playing
    bool alive = true;               // tombstone after death anim (never erased)
};

// Foundation origin convention [RE: building-render.md §2, pre-hunt]: the
// origin cell (cx, cy) is the foundation's minimum-(x, y) corner in raw iso
// cell space; the building occupies (cx+i, cy+j) for i in [0,fw), j in [0,fh).
std::vector<Cellxy> foundation_cells(int cx, int cy, int fw, int fh);

// Per-cell placement feedback for the cursor grid: one flag per foundation
// cell in foundation_cells order; true = that cell individually OK
// (in-bounds + passable + unoccupied + no building). The ADJACENCY test is
// all-or-nothing and reported separately.
struct PlacementCheck {
    bool ok = false;                  // all cells OK AND adjacency satisfied
    bool adjacency = false;           // within Adjacent= range of an own building
    std::vector<bool> cell_ok;        // per foundation cell
};

// Validity [RE: construction.md §9, pre-hunt]: every foundation cell must be
// in-grid, passable (grid value 1), free in `occ` (no unit), and not overlap
// any ALIVE building's foundation. Adjacency: the candidate foundation rect
// expanded by `adjacent` cells (Chebyshev) must intersect the foundation
// rect of at least one alive, same-house, non-Dead building. `bypass_adjacency`
// (scripted spawns: the very first ConYard has no neighbors) skips ONLY the
// adjacency test, never the cell tests.
PlacementCheck placement_check(int cx, int cy, int fw, int fh, int house, int adjacent,
                               const std::vector<std::uint8_t>& passable, int grid_w, int grid_h,
                               const Occupancy& occ,
                               const std::vector<Building>& buildings,
                               bool bypass_adjacency);

// Frame selection for the MAIN building SHP [RE: building-render.md §4,
// pre-hunt]: image frames are the first half (second half = shadows, like
// unit SHPs); frame 0 = healthy, frame 1 = damaged, shown when
// hp/max_hp <= condition_yellow (0..1 fraction). Returns 0 when the SHP has
// fewer than 2 image frames. Only meaningful in State::Idle.
int building_frame(const Building& b, int shp_frame_count, double condition_yellow);

// Frame selection for the BUILDUP SHP during State::Buildup
// [RE-GATE APPLIED: building-render.md §5]: image frames = first half; the
// anim plays ONCE, one frame per fixed PERIOD, period = buildup_total /
// image_frames seconds (the original's per-frame delay is
// trunc(BuildupTime*900/image_frames) ticks; buildup_total is period *
// image_frames, so dividing recovers the period). frame =
// trunc(buildup_time / period), clamped to [0, image_frames-1].
int buildup_frame(const Building& b, int buildup_shp_frame_count);

// Death anim duration (the explosion SHP is drawn by main.cpp; this is the
// sim-side tombstone timer, matching the unit kDieAnimSecs pattern).
constexpr double kBuildingDieSecs = 1.0;

// Advance every building by dt: Buildup accumulates buildup_time and flips
// to Idle at buildup_total; Dead accumulates die_time and tombstones
// (alive=false) at kBuildingDieSecs. Idle does nothing. Pure; deterministic;
// ascending index. Returns indices that flipped Buildup->Idle this call (the
// caller re-computes power on those) -- Dead->tombstone needs no report (the
// caller released blocking when the building DIED, not when it tombstoned).
std::vector<int> step_buildings(std::vector<Building>& buildings, double dt);

// Kill helper shared by combat and tests: marks Dead + die_time=0 (idempotent
// -- already-Dead/dead buildings are untouched). hp clamped to 0.
void kill_building(Building& b);

}  // namespace ra2engine
