#pragma once
// Pure harvester mission logic [RE: docs/re/economy.md]. Steps every
// harvester Unit through SeekOre -> ... -> Unloading, mutating the OreField
// and crediting `house_credits`. Movement itself stays in step_units: this
// module only emits HarvestMove requests (the step_combat MoveRequest
// pattern) that World paths afterwards. No SDL/OS.
#include <vector>
#include "occupancy.hpp"  // Occupancy (chrono-warp occupancy reconcile, Task 6 review fix)
#include "ore.hpp"
#include "world.hpp"

namespace ra2engine {

// [RE-GATE APPLIED: economy.md §1-§3] pacing constants. The hunt CONFIRMED
// the structures (8-ring dig, per-type store, chunked unload, warp-home)
// but their timer constants are [RE-FALLBACK adjudicated] to these values:
constexpr double kGulpSecs = 1.0;        // one bale per second while parked
constexpr double kUnloadSecsPerBale = 2.0 / 15.0;  // 40 bales ~ 5.3 s
constexpr int kOreScanRadius = 32;       // Chebyshev cells
constexpr double kRescanSecs = 3.0;      // idle rescan cadence
constexpr double kChronoWarpSecs = 1.0;  // CMIN in-transit time per warp
// Tiberium bale values, resolved by the caller from Rules (Riparius 25,
// Cruentus 50) [rulesmd]. PurifierBonus does NOT apply (no purifier built).
struct OreValues { int ore = 25; int gems = 50; };

struct HarvestMove { int unit; int gx, gy; };     // path request
struct HarvestEvent {                              // audio/render hooks
    enum class Kind { ChronoOut, ChronoIn, UnloadStart, UnloadEnd };
    Kind kind; int unit;
};

// The refinery dock cell [RE-GATE APPLIED: economy.md §2 — the binary
// docks at the building's DockingOffsets coord (BuildingType+0x1788), but
// GAREFN/NAREFN define NO DockingOffset in artmd (verified: only
// airfields/shipyards/depots do), so the refinery uses a binary default
// the hunt did not isolate. [RE-FALLBACK adjudicated]: one row south of
// the foundation's center column; visually calibrated against the
// refinery pad in Task 7's screenshots.]
inline int dock_cell_x(const Building& b) { return b.cx + b.fw / 2; }
inline int dock_cell_y(const Building& b) { return b.cy + b.fh; }

// Index of the nearest alive Idle refinery of `house` (types[t].id found in
// `refinery_types`), or -1. Distance = Chebyshev from (x, y) to the dock cell.
int nearest_refinery(const std::vector<Building>& buildings,
                     const std::vector<BuildingTypeDef>& types,
                     const std::vector<int>& refinery_types, int house, int x, int y);

// Advance every harvester by dt [RE: economy.md §1-§3]. Deterministic:
// ascending unit index. Appends path requests to `moves` and events to
// `events` (either may be nullptr-free — pass by reference, always).
// `refinery_types` = roster indices whose id is a refinery (GAREFN/NAREFN).
// `house_credits` accumulates unload income per house (index 0/1).
// `occ` is the World cell-ownership grid (Task 6 review fix): a CMIN
// chrono-warp releases EVERY claim the warping unit holds and claims exactly
// its landing cell, and a warp never lands on a cell owned by another unit
// (it deflects deterministically -- see teleport_unit in harvest.cpp). Only
// the teleport path touches `occ`; driving/parked harvesters' occupancy is
// managed by step_units exactly as before.
void step_harvest(std::vector<Unit>& units, std::vector<Building>& buildings,
                  const std::vector<BuildingTypeDef>& types,
                  const std::vector<int>& refinery_types, OreField& ore,
                  const std::vector<std::uint8_t>& passable, int gw, int gh, Occupancy& occ,
                  double dt, const OreValues& values, double house_credits[2],
                  std::vector<HarvestMove>& moves, std::vector<HarvestEvent>& events);

}  // namespace ra2engine
