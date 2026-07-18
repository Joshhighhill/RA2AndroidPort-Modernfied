#include "harvest.hpp"

#include <algorithm>
#include <climits>
#include <cmath>

namespace ra2engine {

namespace {

// 8-ring / warp-fallback neighbor order: N, NE, E, SE, S, SW, W, NW
// [RE-GATE APPLIED: economy.md §1 -- CONFIRMED that an 8-neighbour ring dig
// via an offset table (DAT_0089E5F8) exists and is walked neighbour-by-
// neighbour; the table's own dx,dy VALUES were not decompiled here, so this
// specific N,NE,E,SE,S,SW,W,NW sequence is [RE-FALLBACK adjudicated], not
// itself CONFIRMED -- review Minor-5: the banner previously overclaimed the
// exact order, not just the mechanism (the Harvesting-case comment below
// already split this correctly).]
constexpr int kRingDx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
constexpr int kRingDy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

bool is_passable(const std::vector<std::uint8_t>& passable, int gw, int gh, int x, int y) {
    if (x < 0 || y < 0 || x >= gw || y >= gh) return false;
    return passable[(size_t)y * (size_t)gw + (size_t)x] != 0;
}

// [RE-GATE APPLIED §1: FootClass::MoveToTiberium — the wide scan's exact
// radius/tie-break was not isolated (economy.md §1); [RE-FALLBACK
// adjudicated] to this deterministic nearest-cell rule: Chebyshev distance,
// ties broken by scan order (y ascending, then x ascending), which is
// equivalent to "smaller y, then smaller x" since only STRICTLY smaller
// distances replace the running best.]
bool find_nearest_ore(const OreField& ore, int cx, int cy, int radius, int& out_x, int& out_y) {
    bool found = false;
    int best_dist = INT_MAX;
    int y0 = std::max(0, cy - radius), y1 = std::min(ore.h() - 1, cy + radius);
    int x0 = std::max(0, cx - radius), x1 = std::min(ore.w() - 1, cx + radius);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            if (!ore.has_ore(x, y)) continue;
            int dist = std::max(std::abs(x - cx), std::abs(y - cy));
            if (dist > radius) continue;
            if (!found || dist < best_dist) {
                found = true;
                best_dist = dist;
                out_x = x;
                out_y = y;
            }
        }
    }
    return found;
}

// Widening-ring deflect bound for a blocked warp target (Chebyshev radius).
// 8 rings = up to a 17x17 neighborhood -- far more candidates than a dock
// pileup can plausibly exhaust; beyond it the warp aborts in place (the
// unit's own just-released cell is always re-claimable, see teleport_unit).
constexpr int kWarpDeflectRadius = 8;

// Teleport `u` (unit index `self`) to (tx, ty), reconciling occupancy
// (Task 6 review fix -- the pre-fix code only checked terrain passability
// and left `occ` untouched, so the pre-warp cell stayed claimed forever and
// the landing cell was never claimed):
//   1. release EVERY claim `self` holds (current cell + any mid-move
//      next-cell reservation -- the same release_all_of sweep World's order
//      flows use);
//   2. land on the first cell that is terrain-passable AND free in `occ`:
//      the target itself, else the radius-1 8-ring in the recovered
//      N,NE,E,SE,S,SW,W,NW order [RE-GATE §1: DAT_0089E5F8] (preserves the
//      pre-fix landing choice whenever the target/ring is open), else
//      widening Chebyshev rings walked in scan order (y asc, x asc -- the
//      same deterministic tie-break find_nearest_ore uses) out to
//      kWarpDeflectRadius;
//   3. if EVERY candidate is blocked, the warp aborts in place: the unit
//      stays on its current cell (guaranteed re-claimable -- step 1 just
//      released it and nobody else can have claimed it within this call);
//   4. claim exactly the landing cell.
// Post-invariant either way: `self` holds exactly ONE claim, on its current
// cell, and never overlaps another unit's claim. Clears path (movement
// stays in step_units).
void teleport_unit(Unit& u, int self, int tx, int ty,
                   const std::vector<std::uint8_t>& passable, int gw, int gh, Occupancy& occ) {
    occ.release_all_of(self, /*keep_x*/ -1, /*keep_y*/ -1);  // off-grid keep = release ALL
    auto ok = [&](int x, int y) {
        return is_passable(passable, gw, gh, x, y) && occ.is_free(x, y);
    };
    int fx = u.cell_x, fy = u.cell_y;  // abort-in-place fallback
    bool found = false;
    if (ok(tx, ty)) {
        fx = tx;
        fy = ty;
        found = true;
    }
    for (int d = 0; d < 8 && !found; ++d) {
        int nx = tx + kRingDx[d], ny = ty + kRingDy[d];
        if (ok(nx, ny)) {
            fx = nx;
            fy = ny;
            found = true;
        }
    }
    for (int r = 2; r <= kWarpDeflectRadius && !found; ++r) {
        for (int y = ty - r; y <= ty + r && !found; ++y)
            for (int x = tx - r; x <= tx + r && !found; ++x) {
                if (std::max(std::abs(x - tx), std::abs(y - ty)) != r) continue;  // perimeter only
                if (ok(x, y)) {
                    fx = x;
                    fy = y;
                    found = true;
                }
            }
    }
    u.cell_x = fx;
    u.cell_y = fy;
    u.fx = (double)fx;
    u.fy = (double)fy;
    u.path.clear();
    u.path_idx = 0;
    occ.set(fx, fy, self);
}

}  // namespace

int nearest_refinery(const std::vector<Building>& buildings,
                     const std::vector<BuildingTypeDef>& /*types*/,
                     const std::vector<int>& refinery_types, int house, int x, int y) {
    int best = -1;
    int best_dist = INT_MAX;
    for (size_t i = 0; i < buildings.size(); ++i) {
        const Building& b = buildings[i];
        if (!b.alive || b.state != Building::State::Idle) continue;
        if (b.house != house) continue;
        bool is_ref = false;
        for (int rt : refinery_types) {
            if (b.type == rt) { is_ref = true; break; }
        }
        if (!is_ref) continue;
        int dx = std::abs(dock_cell_x(b) - x);
        int dy = std::abs(dock_cell_y(b) - y);
        int dist = std::max(dx, dy);
        if (dist < best_dist) {
            best_dist = dist;
            best = (int)i;
        }
    }
    return best;
}

std::vector<std::size_t> free_unit_grand_openings(
    const std::vector<Building>& buildings, const std::vector<bool>& free_unit_of_type,
    std::vector<int>& prev_states) {
    prev_states.resize(buildings.size(), -1);
    std::vector<std::size_t> opened;
    for (std::size_t i = 0; i < buildings.size(); ++i) {
        const int cur = static_cast<int>(buildings[i].state);
        const int prev = prev_states[i];
        prev_states[i] = cur;
        if (prev != static_cast<int>(Building::State::Buildup) ||
            cur != static_cast<int>(Building::State::Idle))
            continue;
        const int t = buildings[i].type;
        if (t < 0 || t >= (int)free_unit_of_type.size() || !free_unit_of_type[t]) continue;
        opened.push_back(i);
    }
    return opened;
}

void step_harvest(std::vector<Unit>& units, std::vector<Building>& buildings,
                  const std::vector<BuildingTypeDef>& types,
                  const std::vector<int>& refinery_types, OreField& ore,
                  const std::vector<std::uint8_t>& passable, int gw, int gh, Occupancy& occ,
                  double dt, const OreValues& values, double house_credits[2],
                  std::vector<HarvestMove>& moves, std::vector<HarvestEvent>& events) {
    for (int idx = 0; idx < (int)units.size(); ++idx) {
        Unit& u = units[idx];
        if (!u.is_harvester || !u.alive || u.die_time >= 0.0 || u.cargo_of >= 0 ||
            u.is_aircraft)
            continue;  // carried harvester: mission FSM parks (CB-1); CB-2: an
                       // aircraft is never a harvester (data-unreachable, guarded)

        // Auto-resume after a manual move order [RE-GATE APPLIED §1 PROBABLE:
        // "was harvesting" flag restores the mission once the ordered move
        // completes]. While the move is in flight, mission logic is entirely
        // skipped; on arrival the mission restarts at SeekOre (a fresh rescan
        // picks up wherever the miner ends up, load intact).
        if (u.manual_hold) {
            if (!u.moving()) {
                u.manual_hold = false;
                u.harv = HarvState::SeekOre;
                u.harv_timer = 0.0;
            }
            continue;
        }

        switch (u.harv) {
            case HarvState::None:
                break;

            case HarvState::SeekOre: {
                // Scan immediately when due (harv_timer <= 0 -- true the
                // instant the mission enters this state, since every
                // transition into SeekOre resets harv_timer to 0), otherwise
                // wait out the rescan cadence [RE-FALLBACK: economy.md §1].
                u.harv_timer -= dt;
                if (u.harv_timer > 0.0) break;
                u.harv_timer = kRescanSecs;
                int ox = 0, oy = 0;
                if (find_nearest_ore(ore, u.cell_x, u.cell_y, kOreScanRadius, ox, oy)) {
                    u.harv_cell_x = ox;
                    u.harv_cell_y = oy;
                    u.has_harv_cell = true;  // real stamp -- see world.hpp
                    moves.push_back({idx, ox, oy});
                    u.harv = HarvState::MoveToOre;
                }
                break;
            }

            case HarvState::MoveToOre: {
                bool at_cell = (u.cell_x == u.harv_cell_x && u.cell_y == u.harv_cell_y);
                if (at_cell && !u.moving()) {
                    u.harv = HarvState::Harvesting;
                    u.harv_timer = 0.0;
                    break;
                }
                if (!ore.has_ore(u.harv_cell_x, u.harv_cell_y)) {
                    // Target emptied en route (another harvester got there
                    // first) -- rescan rather than arrive at a dead cell.
                    u.harv = HarvState::SeekOre;
                    u.harv_timer = 0.0;
                    break;
                }
                if (!at_cell && !u.moving()) {
                    // Stopped without arriving: the path was abandoned
                    // (unreachable / collision give-up in step_units).
                    u.harv = HarvState::SeekOre;
                    u.harv_timer = 0.0;
                }
                break;
            }

            case HarvState::Harvesting: {
                // [RE-GATE APPLIED §1 CONFIRMED: 8-ring dig in DAT_0089E5F8
                // offset-table order, one density step per timed gulp banked
                // into a per-type store. [RE-FALLBACK] the gulp cadence
                // itself; and [RE-FALLBACK adjudicated] trying the miner's
                // OWN cell before the ring -- the doc confirms only the
                // 8-neighbour ring walk, not an own-cell-first step.]
                u.harv_timer += dt;
                while (u.harv_timer >= kGulpSecs && u.harv == HarvState::Harvesting) {
                    u.harv_timer -= kGulpSecs;
                    int tx = u.cell_x, ty = u.cell_y;
                    OreKind k = OreKind::None;
                    if (ore.has_ore(tx, ty)) {
                        k = ore.take_bale(tx, ty);
                    } else {
                        for (int d = 0; d < 8 && k == OreKind::None; ++d) {
                            int nx = tx + kRingDx[d], ny = ty + kRingDy[d];
                            if (ore.has_ore(nx, ny)) k = ore.take_bale(nx, ny);
                        }
                    }
                    if (k == OreKind::None) {
                        // Own cell AND ring exhausted -> relocate.
                        u.harv = HarvState::SeekOre;
                        u.harv_timer = 0.0;
                        break;
                    }
                    if (k == OreKind::Ore) {
                        u.bales_ore += 1.0;
                    } else {
                        u.bales_gems += 1.0;
                    }
                    if (u.bales_ore + u.bales_gems >= (double)u.storage_bales) {
                        u.harv = HarvState::ReturnToRef;
                        u.harv_timer = 0.0;
                        break;
                    }
                }
                break;
            }

            case HarvState::ReturnToRef: {
                // Stamp the target refinery ONCE, the moment this state picks
                // it (review fix: re-deriving `nearest` every tick made the
                // WaitDock busy check evaluate against a moving target in a
                // multi-refinery house). The stamp persists through WaitDock/
                // Unloading; repick ONLY when the stamped building dies
                // [economy.md §2 -- the radio handshake binds a miner to the
                // refinery that answered it].
                int ref = u.dock_refinery;
                if (ref < 0 || ref >= (int)buildings.size() ||
                    !buildings[(size_t)ref].alive ||
                    buildings[(size_t)ref].state != Building::State::Idle) {
                    ref = nearest_refinery(buildings, types, refinery_types, u.house, u.cell_x,
                                           u.cell_y);
                    u.dock_refinery = ref;
                }
                if (ref < 0) {
                    // No alive refinery: park the (still loaded) miner back
                    // in SeekOre, retried every rescan, per economy.md §1/§2.
                    u.dock_refinery = -1;
                    u.harv = HarvState::SeekOre;
                    u.harv_timer = 0.0;
                    break;
                }
                int dcx = dock_cell_x(buildings[(size_t)ref]);
                int dcy = dock_cell_y(buildings[(size_t)ref]);
                if (u.chrono_miner) {
                    // [RE-GATE APPLIED §3 PROBABLE: teleport locomotor warps
                    // on going full; [RE-FALLBACK] exact timing ->
                    // kChronoWarpSecs.] harv_timer == 0.0 exactly on the
                    // first tick of this warp (every transition INTO
                    // ReturnToRef resets it), so ChronoOut fires once.
                    if (u.harv_timer == 0.0) {
                        events.push_back({HarvestEvent::Kind::ChronoOut, idx});
                    }
                    u.harv_timer += dt;
                    if (u.harv_timer >= kChronoWarpSecs) {
                        teleport_unit(u, idx, dcx, dcy, passable, gw, gh, occ);
                        events.push_back({HarvestEvent::Kind::ChronoIn, idx});
                        u.harv = HarvState::WaitDock;
                        u.harv_timer = 0.0;
                    }
                } else {
                    bool at_dock = (u.cell_x == dcx && u.cell_y == dcy && !u.moving());
                    if (at_dock) {
                        u.harv = HarvState::WaitDock;
                        u.harv_timer = 0.0;
                    } else if (!u.moving() || u.path.back().x != dcx ||
                               u.path.back().y != dcy) {
                        // Review Important-1: emit the HarvestMove ONLY when
                        // the unit isn't already pathing to this exact dock
                        // cell -- mirroring SeekOre's emit-once-on-transition
                        // pattern (MoveToOre never re-emits either). The old
                        // unconditional push_back() here forced World to run
                        // a full A* repath every tick for every driving
                        // harvester (wasteful) AND reset wait_time=0 every
                        // tick via order_unit_move, silently disabling
                        // step_units' kBlockedWait/kGiveUpAfter give-up
                        // machinery for returning harvesters. Now step_units
                        // owns the drive (and its own re-path/give-up
                        // handling) once the move is issued; this branch only
                        // re-fires when the unit stops without having
                        // arrived (blocked give-up cleared its path, or the
                        // stamped dock changed), exactly like MoveToOre's
                        // "stopped without arriving -> retry" case.
                        moves.push_back({idx, dcx, dcy});
                    }
                }
                break;
            }

            case HarvState::WaitDock: {
                // Queue at the refinery STAMPED by ReturnToRef (review fix:
                // never re-derive `nearest` here -- another refinery going
                // Idle mid-wait must not poach a queued miner). Repick (via
                // ReturnToRef) only if the stamped building died.
                int ref = u.dock_refinery;
                if (ref < 0 || ref >= (int)buildings.size() ||
                    !buildings[(size_t)ref].alive ||
                    buildings[(size_t)ref].state == Building::State::Dead) {
                    u.dock_refinery = -1;
                    u.harv = HarvState::ReturnToRef;
                    u.harv_timer = 0.0;
                    break;
                }
                // [RE-GATE APPLIED §2: the radio handshake refuses "dock
                // busy"; [RE-FALLBACK adjudicated] to deterministic
                // lowest-index FIFO -- an emergent property of processing
                // units ascending: a lower-index unit that claims the dock
                // THIS tick is already Unloading by the time a higher-index
                // unit checks, below.]
                bool busy = false;
                for (size_t j = 0; j < units.size(); ++j) {
                    if ((int)j == idx) continue;
                    if (units[j].dock_refinery == ref && units[j].harv == HarvState::Unloading) {
                        busy = true;
                        break;
                    }
                }
                if (!busy) {
                    events.push_back({HarvestEvent::Kind::UnloadStart, idx});
                    u.harv = HarvState::Unloading;
                    u.harv_timer = 0.0;
                }
                break;
            }

            case HarvState::Unloading: {
                // CMIN post-unload warp-back transit: `dock_refinery == -1`
                // with an empty load marks the return warp in flight (set
                // below when the last bale drains, ONLY when has_harv_cell --
                // review Important-3 -- so `harv` never re-enters this state
                // with dock_refinery == -1 unless a real warp is in flight).
                // Must be checked BEFORE the refinery-death repick, which
                // keys off the same -1.
                if (u.chrono_miner && u.dock_refinery < 0) {
                    u.harv_timer += dt;
                    if (u.harv_timer >= kChronoWarpSecs) {
                        teleport_unit(u, idx, u.harv_cell_x, u.harv_cell_y, passable, gw, gh, occ);
                        events.push_back({HarvestEvent::Kind::ChronoIn, idx});
                        u.harv = HarvState::SeekOre;
                        u.harv_timer = 0.0;
                    }
                    break;
                }
                if (u.dock_refinery < 0 || u.dock_refinery >= (int)buildings.size() ||
                    !buildings[(size_t)u.dock_refinery].alive ||
                    buildings[(size_t)u.dock_refinery].state == Building::State::Dead) {
                    // Refinery died mid-unload -> repick.
                    u.dock_refinery = -1;
                    u.harv = HarvState::ReturnToRef;
                    u.harv_timer = 0.0;
                    break;
                }
                // [RE-GATE APPLIED §2 CONFIRMED: per-tick chunked transfer,
                // first-non-empty type slot first (ore before gems here,
                // matching the rig's fixed 2-slot ore/gems store); income =
                // bales * Value; no PurifierBonus this phase.]
                u.harv_timer += dt;
                while (u.harv_timer >= kUnloadSecsPerBale &&
                       (u.bales_ore > 0.0 || u.bales_gems > 0.0)) {
                    u.harv_timer -= kUnloadSecsPerBale;
                    int house = (u.house == 0 || u.house == 1) ? u.house : 0;
                    if (u.bales_ore > 0.0) {
                        u.bales_ore -= 1.0;
                        house_credits[house] += (double)values.ore;
                    } else {
                        u.bales_gems -= 1.0;
                        house_credits[house] += (double)values.gems;
                    }
                }
                if (u.bales_ore <= 0.0 && u.bales_gems <= 0.0) {
                    events.push_back({HarvestEvent::Kind::UnloadEnd, idx});
                    u.dock_refinery = -1;
                    if (u.chrono_miner && u.has_harv_cell) {
                        // [RE-GATE APPLIED §3: "warp back after unload, same
                        // mechanics" -- the return warp is TIMED like the
                        // outbound one; [RE-FALLBACK adjudicated]: HarvState
                        // has no dedicated warp-back state, so the transit
                        // stays parked in Unloading (dock_refinery == -1
                        // marks it -- see the branch at the top of this
                        // case): ChronoOut fires now, the teleport + ChronoIn
                        // land once harv_timer >= kChronoWarpSecs.]
                        //
                        // Review Important-3: gated on has_harv_cell (a REAL
                        // cell was stamped at least once) rather than firing
                        // unconditionally. A CMIN manually returned (order_
                        // return) with an empty hold and no ore ever found
                        // has harv_cell_x/y stuck at their (0,0) default --
                        // without this guard it would still play both chrono
                        // sounds and warp toward (0,0)/whatever is nearest to
                        // it, a no-op-unload-then-teleport-to-garbage
                        // player-facing bug. Skip straight to SeekOre in
                        // place instead (the `else` branch below), same as a
                        // non-chrono HARV.
                        events.push_back({HarvestEvent::Kind::ChronoOut, idx});
                        u.harv_timer = 0.0;
                    } else {
                        u.harv = HarvState::SeekOre;
                        u.harv_timer = 0.0;
                    }
                }
                break;
            }
        }
    }
}

}  // namespace ra2engine
