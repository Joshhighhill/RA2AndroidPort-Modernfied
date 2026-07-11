#include "harvest.hpp"

#include <algorithm>
#include <climits>
#include <cmath>

namespace ra2engine {

namespace {

// 8-ring / warp-fallback neighbor order: N, NE, E, SE, S, SW, W, NW
// [RE-GATE APPLIED: economy.md §1 CONFIRMED — offset table DAT_0089E5F8].
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

// Teleport `u` to (tx, ty) if passable, else the first passable cell in the
// fixed ring order around it [RE-GATE APPLIED §3 PROBABLE: the teleport
// locomotor warps to the dock coord; occupancy (a cell held by another unit)
// is handled by World in Task 6, so only terrain passability is checked
// here]. Clears path (movement stays in step_units).
void teleport_unit(Unit& u, int tx, int ty, const std::vector<std::uint8_t>& passable, int gw,
                    int gh) {
    int fx = tx, fy = ty;
    if (!is_passable(passable, gw, gh, fx, fy)) {
        for (int d = 0; d < 8; ++d) {
            int nx = tx + kRingDx[d], ny = ty + kRingDy[d];
            if (is_passable(passable, gw, gh, nx, ny)) {
                fx = nx;
                fy = ny;
                break;
            }
        }
    }
    u.cell_x = fx;
    u.cell_y = fy;
    u.fx = (double)fx;
    u.fy = (double)fy;
    u.path.clear();
    u.path_idx = 0;
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

void step_harvest(std::vector<Unit>& units, std::vector<Building>& buildings,
                  const std::vector<BuildingTypeDef>& types,
                  const std::vector<int>& refinery_types, OreField& ore,
                  const std::vector<std::uint8_t>& passable, int gw, int gh,
                  double dt, const OreValues& values, double house_credits[2],
                  std::vector<HarvestMove>& moves, std::vector<HarvestEvent>& events) {
    for (int idx = 0; idx < (int)units.size(); ++idx) {
        Unit& u = units[idx];
        if (!u.is_harvester || !u.alive || u.die_time >= 0.0) continue;

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
                // [RE-GATE APPLIED §1 CONFIRMED: 8-ring dig, DAT_0089E5F8
                // offset table order, one density step per timed gulp banked
                // into a per-type store; [RE-FALLBACK] the gulp cadence
                // itself.] Own cell first, then the fixed ring order.
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
                int ref = nearest_refinery(buildings, types, refinery_types, u.house, u.cell_x,
                                           u.cell_y);
                if (ref < 0) {
                    // No alive refinery: park the (still loaded) miner back
                    // in SeekOre, retried every rescan, per economy.md §1/§2.
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
                        teleport_unit(u, dcx, dcy, passable, gw, gh);
                        events.push_back({HarvestEvent::Kind::ChronoIn, idx});
                        u.harv = HarvState::WaitDock;
                        u.harv_timer = 0.0;
                    }
                } else {
                    bool at_dock = (u.cell_x == dcx && u.cell_y == dcy && !u.moving());
                    if (at_dock) {
                        u.harv = HarvState::WaitDock;
                        u.harv_timer = 0.0;
                    } else {
                        moves.push_back({idx, dcx, dcy});
                    }
                }
                break;
            }

            case HarvState::WaitDock: {
                int ref = nearest_refinery(buildings, types, refinery_types, u.house, u.cell_x,
                                           u.cell_y);
                if (ref < 0) {
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
                    u.dock_refinery = ref;
                    events.push_back({HarvestEvent::Kind::UnloadStart, idx});
                    u.harv = HarvState::Unloading;
                    u.harv_timer = 0.0;
                }
                break;
            }

            case HarvState::Unloading: {
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
                    if (u.chrono_miner) {
                        // [RE-FALLBACK adjudicated]: HarvState has no
                        // dedicated post-unload warp-back state (unlike the
                        // outbound ReturnToRef warp, this leg has nowhere to
                        // "wait" -- the miner heads straight back to SeekOre
                        // either way), so the warp back to the last ore cell
                        // is applied synchronously here (both ChronoOut and
                        // ChronoIn fire this same tick) rather than spread
                        // over kChronoWarpSecs.
                        events.push_back({HarvestEvent::Kind::ChronoOut, idx});
                        teleport_unit(u, u.harv_cell_x, u.harv_cell_y, passable, gw, gh);
                        events.push_back({HarvestEvent::Kind::ChronoIn, idx});
                    }
                    u.harv = HarvState::SeekOre;
                    u.harv_timer = 0.0;
                }
                break;
            }
        }
    }
}

}  // namespace ra2engine
