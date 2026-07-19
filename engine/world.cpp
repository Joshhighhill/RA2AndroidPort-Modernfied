#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

#include "cargo.hpp"          // can_load/cargo_total_size/next_eject/eject_cell (CB-1)
#include "combat.hpp"         // armor_index, step_combat, MoveRequest
#include "flight.hpp"         // CB-2: step_altitude/cruise_step/heading_toward/advance_flight_state
#include "factory.hpp"        // exit_cell/primary_factory/set_primary (Production phase)
#include "group_move.hpp"     // assign_formation
#include "harvest.hpp"        // step_harvest/HarvestMove/HarvestEvent/OreValues (Economy phase)
#include "iso_project.hpp"   // cell_to_screen (terrain_screen_bounds)
#include "iso_renderer.hpp"  // terrain_screen_bounds / pal_for_house declarations
#include "pathfind.hpp"
#include "ra2assets/pal.hpp"  // Palette/Rgba (pal_for_house)
#include "capture.hpp"        // grants_startup_from_neutral, kNeutralHouse (Capture phase)

namespace ra2engine {

// Per-image animation data (Task 2): calibrated against the ORIGINAL game's
// artmd.ini GISequence/ConSequence data. docs/re/infantry-anim.md is the
// calibration authority -- see it for the extracted values (§2), the
// StartFrame/FrameCount/FacingMultiplier frame-selection formula (§1), and
// the facing-block-order evidence (§4/§5). This REPLACES the Task 7
// empirically-eyeballed table, which had two confirmed frame bugs: CONS fire
// was hardcoded to 158..163 (the tail of Die2), and both units' die used
// 80..87 (Idle2/Prone frames), not the real Die1=134..148 (docs §3).
const UnitAnimSeq& default_anim() {
    static const UnitAnimSeq a{
        /*stand=*/{0, 1, 1}, /*walk=*/{8, 6, 6},
        /*fire=*/{164, 6, 6}, /*die=*/{134, 15, 0},
    };
    return a;
}

const UnitAnimSeq& anim_for_image(const AnimTable& t, const std::string& image) {
    const auto it = t.find(image);
    return it != t.end() ? it->second : default_anim();
}

int seq_frame(const SeqEntry& e, int facing, int phase) {
    const int p = phase < 0 ? 0 : (phase >= e.count ? e.count - 1 : phase);
    return e.first + facing * e.fmult + p;
}

// Cell-space direction -> SHP facing block index (counterclockwise from
// North on screen -- docs/re/infantry-anim.md §4/§5). Screen projection is
// (sx, sy) ~ (dx - dy, dx + dy) with y down.
int facing_from_delta(double dx, double dy) {
    if (dx == 0.0 && dy == 0.0) return -1;
    const double ang = std::atan2(dx + dy, dx - dy) * 180.0 / 3.14159265358979323846;
    const double ccw = std::fmod(-90.0 - ang + 720.0, 360.0);
    return static_cast<int>(std::lround(ccw / 45.0)) % 8;
}

// Continuous 16-bit facing from a cell-space direction, in the ORIGINAL's
// RENDER facing space (docs/re/voxel-render.md §2) -- the space
// facing_to_index/index_to_yaw (voxel_raster.cpp) are calibrated to, so a
// vehicle's rendered nose LEADS its travel/aim direction. In that space
// facing 0x4000 = screen East (the down-right 2:1 diagonal = the unrotated
// +X-nose model), and facing increases CLOCKWISE on screen. atan2(dx, -dy)
// maps: cell (+1,0) -> pi/2 -> 16384 = 0x4000 (East, index 8); (0,+1) -> pi
// -> 32768 = South (matching the Unit spawn body_facing=32768 constant);
// (-1,0) -> 49152 = 0xC000; (0,-1) -> 0 = North (index 0). NOTE this differs
// (by a rotation AND a reflection) from infantry's facing_from_delta 8-block
// space -- vehicles must use THIS function, infantry the other; the two are
// never mixed. turn_toward / vehicle_aimed / the hull turn gate / turret slew
// are isometry-invariant (they only compare facings from this same function),
// so locomotion semantics are unchanged.
double facing_u16_from_delta(double dx, double dy) {
    constexpr double kPi = 3.14159265358979323846;
    // atan2 in [-pi, pi] -> [-32768, 32768]; the +65536 then fmod normalizes
    // into [0, 65536) without a separate negative-wrap fixup.
    return std::fmod(std::atan2(dx, -dy) * (65536.0 / (2.0 * kPi)) + 65536.0, 65536.0);
}

double turn_toward(double facing, double target, double max_step) {
    double d = std::fmod(target - facing, 65536.0);
    if (d > 32768.0) d -= 65536.0;
    if (d < -32768.0) d += 65536.0;
    double out;
    if (std::abs(d) <= max_step) out = target;
    else out = facing + (d > 0 ? max_step : -max_step);
    out = std::fmod(out, 65536.0);
    if (out < 0) out += 65536.0;
    return out;
}

bool vehicle_aimed(const Unit& u, double target_facing) {
    double d = std::fmod(target_facing - u.turret_facing, 65536.0);
    if (d > 32768.0) d -= 65536.0;
    if (d < -32768.0) d += 65536.0;
    return std::abs(d) <= kAimToleranceU16;
}

void step_turrets(std::vector<Unit>& units, const std::vector<Building>& buildings, double dt) {
    for (auto& u : units) {
        if (!u.is_vehicle || !u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) continue;
        double want = u.body_facing;
        if (u.target >= 0 && u.target < static_cast<int>(units.size())) {
            const Unit& t = units[u.target];
            if (t.alive && t.die_time < 0.0)
                want = facing_u16_from_delta(t.fx - u.fx, t.fy - u.fy);
        } else if (u.target_building >= 0 &&
                   u.target_building < static_cast<int>(buildings.size())) {
            // Buildings & Construction Task 4 fix: mirror the unit-target
            // branch above for a live attack-building order -- slew toward
            // the NEAREST foundation cell (same rule combat.cpp uses for
            // range), not the building's origin/center. With `buildings`
            // empty (every pre-Task-4 call site/test) this branch is
            // unreachable, preserving byte-identity.
            const Building& b = buildings[static_cast<std::size_t>(u.target_building)];
            if (b.alive && b.state != Building::State::Dead) {
                const auto cells = foundation_cells(b.cx, b.cy, b.fw, b.fh);
                double best_d = -1.0;
                int nx = 0, ny = 0;
                for (const Cellxy& cell : cells) {
                    const double dx = (static_cast<double>(cell.x) - u.fx);
                    const double dy = (static_cast<double>(cell.y) - u.fy);
                    const double d = dx * dx + dy * dy;
                    if (best_d < 0.0 || d < best_d) {
                        best_d = d;
                        nx = cell.x;
                        ny = cell.y;
                    }
                }
                want = facing_u16_from_delta(static_cast<double>(nx) - u.fx,
                                             static_cast<double>(ny) - u.fy);
            }
        }
        u.turret_facing = turn_toward(u.turret_facing, want, u.rot_u16_per_sec * dt);
    }
}

static constexpr double kWalkFps = 9.0;  // walk-cycle frame rate (shared cadence, all images)
static constexpr double kFireAnimFps = 15.0;  // Fire sequence pacing: the 6-frame recoil plays through ONCE per shot, spanning the kFireFlashSecs (0.4s) flash window exactly at the game's ~15Hz animation rate (6 frames / 0.4s = 15fps). Distinct from kWalkFps: walk cycles loop, fire plays once.
// Die-progress frame rate: 15 frames (default_anim's die.count) at 15fps is
// EXACTLY combat.cpp's kDieAnimSecs (1.0s), so every die frame is reachable
// before the unit tombstones (docs/re/infantry-anim.md §2/§3). Distinct from
// kWalkFps: this only paces the die-progress formula, not walk cycling.
static constexpr double kDieAnimFps = 15.0;

// Duplicated from engine/combat.cpp's (anonymous-namespace) kFireFlashSecs --
// NOT exposed via combat.hpp, so World::step's anim-override pass below
// (Task 7) can't reference it directly. Same tolerated duplicated-constant
// pattern this file already carries for kTileW/kTileH/kLevelPx (flagged
// Minor in task-12-report.md, not made any worse here). MUST stay in sync
// with combat.cpp's value if that one ever changes.
static constexpr double kFireFlashSecs = 0.4;

// Seconds between re-path attempts while blocked (next cell held by another
// unit). Short enough to feel responsive, long enough that a unit briefly
// waiting for one ahead to step off doesn't thrash A* every frame.
static constexpr double kBlockedWait = 0.30;
// Seconds of CONTINUOUS block after which a unit abandons its order (boxed-in
// or sealed corridor). Transient jams (a funnel cell mid-crossing) clear well
// inside this: the per-frame can_enter check resumes the walk the instant the
// blocker steps off, and failed re-paths retry every kBlockedWait until then.
static constexpr double kGiveUpAfter = 2.0;

// RA2 temperate tile geometry -- must match iso_renderer.cpp's / input.cpp's
// kTileW/kTileH/kLevelPx (the same duplicated-constant pattern already
// flagged as a Minor in task-12-report.md; not made any worse here). Used
// only by terrain_screen_bounds below.
static constexpr int kTileW = 60;
static constexpr int kTileH = 30;
static constexpr int kLevelPx = 15;

const ra2assets::MapCell* pick_start_cell(const ra2assets::MapFile& map) {
    const auto& cells = map.cells();
    if (cells.empty()) return nullptr;

    // Centroid of every Clear (tile == 0) cell.
    long long sx = 0, sy = 0;
    size_t n = 0;
    for (const auto& c : cells) {
        if (c.tile == 0) {
            sx += c.x;
            sy += c.y;
            ++n;
        }
    }
    // No Clear cell at all would be unusual for a real map; fall back to an
    // arbitrary interior-ish cell (the middle of decode order) rather than
    // leaving the unit unplaced.
    if (n == 0) return &cells[cells.size() / 2];

    const double cx = static_cast<double>(sx) / static_cast<double>(n);
    const double cy = static_cast<double>(sy) / static_cast<double>(n);

    const ra2assets::MapCell* best = nullptr;
    double best_d2 = -1.0;
    for (const auto& c : cells) {
        if (c.tile != 0) continue;
        const double dx = c.x - cx;
        const double dy = c.y - cy;
        const double d2 = dx * dx + dy * dy;
        if (best == nullptr || d2 < best_d2) {
            best = &c;
            best_d2 = d2;
        }
    }
    return best;
}

std::vector<std::uint8_t> build_passability(const std::vector<ra2assets::MapCell>& cells,
                                            int& grid_w, int& grid_h) {
    grid_w = 0;
    grid_h = 0;
    if (cells.empty()) return {};

    int max_x = 0, max_y = 0;
    for (const auto& c : cells) {
        // MapCell.x/y are raw iso coords (roughly 1..212, always >= 0 for a
        // real map); guard against a stray negative rather than indexing OOB.
        if (c.x > max_x) max_x = c.x;
        if (c.y > max_y) max_y = c.y;
    }
    grid_w = max_x + 1;
    grid_h = max_y + 1;

    std::vector<std::uint8_t> passable(static_cast<size_t>(grid_w) * grid_h, 0);
    for (const auto& c : cells) {
        if (c.x < 0 || c.y < 0) continue;  // defensive; not seen in real maps
        passable[static_cast<size_t>(c.y) * grid_w + c.x] = 1;
    }
    return passable;
}

std::vector<std::uint8_t> build_water_passability(
        const std::vector<ra2assets::LandType>& land_types,
        const std::vector<std::uint8_t>& passable, int grid_w, int grid_h) {
    std::vector<std::uint8_t> water(static_cast<size_t>(grid_w) * grid_h, 0);
    const size_t n = std::min(land_types.size(), water.size());
    for (size_t i = 0; i < n; ++i)
        // [RE-GATE APPLIED: naval.md §1/§2] Water-land-type AND materialized: a
        // ship never paths on a non-materialized water cell (intersect the Land
        // grid). BINARY (matches the game's Ground[land][Float]==0.0 gate).
        if (land_types[i] == ra2assets::LandType::Water &&
            i < passable.size() && passable[i])
            water[i] = 1;   // Water & passable -> passable to a Float ship
    return water;
}

// Screen-space bounding box (camera-local projected pixels, level included)
// of every cell in `map`. Declared in iso_renderer.hpp (it's conceptually a
// renderer/camera helper -- main.cpp's centered_camera uses it), but defined
// HERE rather than in iso_renderer.cpp: iso_renderer.cpp pulls in SDL and is
// not part of the SDL-free `ra2assets_tests` binary, while world.cpp already
// is (same single-source-of-truth pattern as engine/iso_project.cpp and
// engine/pathfind.cpp -- see CMakeLists.txt). Input needs this pure function
// to clamp camera drag to the map's extent (Task 12 review Finding 2)
// without dragging SDL into the test build.
bool terrain_screen_bounds(const ra2assets::MapFile& map, int& min_x, int& min_y, int& max_x,
                           int& max_y) {
    const auto& cells = map.cells();
    if (cells.empty()) return false;
    int mnx = 1 << 30, mny = 1 << 30, mxx = -(1 << 30), mxy = -(1 << 30);
    for (const auto& c : cells) {
        const ScreenPt p =
            cell_to_screen(c.x, c.y, static_cast<int>(c.level), kTileW, kTileH, kLevelPx);
        mnx = std::min(mnx, p.x);
        mny = std::min(mny, p.y);
        mxx = std::max(mxx, p.x + kTileW);
        mxy = std::max(mxy, p.y + kTileH);
    }
    min_x = mnx;
    min_y = mny;
    max_x = mxx;
    max_y = mxy;
    return true;
}

// House palette remap for unit sprites. Declared in iso_renderer.hpp (it's
// conceptually a renderer/palette helper -- draw_unit's call site lives
// there) but defined HERE for the SAME reason as terrain_screen_bounds just
// above: iso_renderer.cpp pulls in SDL and is not part of the SDL-free
// `ra2assets_tests` binary, while world.cpp already is, and this function
// needs to be pure-unit-testable (tests/test_world.cpp).
//
// unittem.pal reserves palette indices 16..31 as the "house remap ramp" --
// the strip of colors the game substitutes per house so the same GI/CONS
// sprite reads as a different color for each side. The source ramp (as
// authored in unittem.pal) is red, which is house 1 (Soviet) here, so house 1
// returns it unchanged; house 0 (Allied) rescales each ramp entry's ORIGINAL
// brightness (average of its own r/g/b, integer division) onto a blue ramp
// instead, preserving alpha. Integer math throughout -> deterministic.
ra2assets::Palette pal_for_house(const ra2assets::Palette& unit_pal, int house) {
    ra2assets::Palette out = unit_pal;
    if (house == 1) return out;  // Soviet: native red ramp, unchanged
    auto clamp_u8 = [](int v) -> std::uint8_t {
        if (v < 0) return 0;
        if (v > 255) return 255;
        return static_cast<std::uint8_t>(v);
    };
    for (int i = 16; i <= 31; ++i) {
        const ra2assets::Rgba src = unit_pal[i];
        const int b = (static_cast<int>(src.r) + src.g + src.b) / 3;
        out[i] = ra2assets::Rgba{clamp_u8(b * 2 / 5), clamp_u8(b * 3 / 5), clamp_u8(b), src.a};
    }
    return out;
}

World::World(const ra2assets::MapFile& map) : map_(&map) {
    passable_ = build_passability(map.cells(), grid_w_, grid_h_);
    occ_ = Occupancy(grid_w_, grid_h_);
    // Economy phase: built unconditionally (empty if the map has no overlay
    // section) so ore() is always valid; set_economy gates whether it ever
    // steps (byte-identity guard -- see types_'s comment in world.hpp).
    ore_ = OreField::from_map(map, grid_w_, grid_h_);
}

bool World::walkable(int x, int y) const {
    if (x < 0 || y < 0 || x >= grid_w_ || y >= grid_h_) return false;
    return passable_[static_cast<size_t>(y) * grid_w_ + x] != 0;
}

Unit& World::add_unit(int cell_x, int cell_y, const std::string& image, int frame) {
    Unit u;
    u.cell_x = cell_x;
    u.cell_y = cell_y;
    u.fx = cell_x;
    u.fy = cell_y;
    u.image = image;
    u.frame = frame;
    if (const ra2assets::MapCell* mc = map_->at(cell_x, cell_y)) u.level = mc->level;
    units_.push_back(std::move(u));
    const int idx = static_cast<int>(units_.size()) - 1;
    occ_.set(cell_x, cell_y, idx);
    return units_.back();
}

Unit& World::add_combat_unit(int cell_x, int cell_y, int house, const UnitCombat& c) {
    Unit& u = add_unit(cell_x, cell_y, c.image, 0);
    u.house = house;
    u.hp = u.max_hp = c.strength;
    u.armor_ix = armor_index(c.armor);
    u.w_damage = c.damage;
    // Engineer/Spy Capture [RE-GATE APPLIED: capture.md §B5]: a veteran unit
    // spawns with scaled firepower + HP. rank 0 (every existing spawn) leaves
    // w_damage/hp/max_hp EXACTLY as resolved above -> byte-identical.
    u.veteran_rank = c.veteran_rank;
    u.sight = c.sight;  // [RE: shroud.md §2] reveal radius; inert until Task 3
    if (c.veteran_rank > 0) {
        u.w_damage = veteran_firepower(u.w_damage, c.veteran_rank);
        u.max_hp = veteran_hp(u.max_hp, c.veteran_rank);
        u.hp = u.max_hp;
    }
    u.w_rof = c.rof;
    u.w_range = c.range;
    u.w_verses = c.verses;
    u.w_cell_spread = c.cell_spread;        // CB-2 [aircraft.md §4]: warhead splash
    u.w_percent_at_max = c.percent_at_max;  // radius + edge scale (0 = single-target)
    // Voxel Vehicles phase (docs/re/voxel-render.md §5): vehicle locomotion
    // state. ROT= is converted to a u16/sec turn rate; speed_cps == 0 keeps
    // the unit on the World-wide move_cps. Infantry leave all of these at
    // their defaults (is_vehicle=false, rot=0, speed_cps=0).
    u.is_vehicle = c.is_vehicle;
    u.speed_cps = c.speed_cps;
    u.rot_u16_per_sec = c.rot * kRotU16PerRotPerSec;
    // CB-1 [RE: transport-cargo.md §1]: cargo stats, inert unless the caller
    // resolved them (default passengers=0 = not a transport).
    u.passengers_max = c.passengers;
    u.size_limit = c.size_limit;
    u.size = c.size;
    // CB-2 [RE: aircraft.md]: flight stats, inert unless the caller resolved
    // them (default is_aircraft=false = a ground unit).
    u.is_aircraft = c.is_aircraft;
    u.cruise_altitude = c.cruise_altitude;
    u.climb_rate = c.climb_rate;
    u.descend_rate = c.descend_rate;
    u.flight_speed = c.flight_speed;
    u.flight_turn = c.flight_turn;
    u.balloon_hover = c.balloon_hover;
    u.omni_fire = c.omni_fire;
    // CB-3 [RE: naval.md §1]: naval flags, inert unless the caller resolved them
    // (default is_naval=false / movement_domain=Land = a ground unit).
    u.is_naval = c.is_naval;
    u.movement_domain = c.movement_domain;
    return u;
}

void World::set_anim(const std::string& image, const UnitAnimSeq& seq) { anims_[image] = seq; }

void World::set_water_terrain(const std::vector<ra2assets::LandType>& land_types) {
    // [RE-GATE APPLIED: naval.md §1] intersect with the Land grid (passable_) so
    // a ship never paths on a non-materialized water cell.
    water_passable_ = build_water_passability(land_types, passable_, grid_w_, grid_h_);
}

// --- Economy phase [RE: economy.md] ---

void World::set_economy(std::vector<BuildingTypeDef> types, std::vector<int> refinery_types,
                        OreValues values, double growth_interval_secs, double growth_pct,
                        double spread_pct) {
    types_ = std::move(types);
    refinery_types_ = std::move(refinery_types);
    ore_value_ore_ = values.ore;
    ore_value_gems_ = values.gems;
    growth_interval_ = growth_interval_secs;
    growth_pct_ = growth_pct;
    spread_pct_ = spread_pct;
}

double World::take_income(int house) {
    if (house < 0 || house > 1) return 0.0;
    const double v = house_credits_[house];
    house_credits_[house] = 0.0;
    return v;
}

Unit& World::add_harvester(int cell_x, int cell_y, int house, const UnitCombat& c, bool chrono,
                           int storage_bales) {
    Unit& u = add_combat_unit(cell_x, cell_y, house, c);
    u.is_harvester = true;
    u.chrono_miner = chrono;
    u.storage_bales = storage_bales;
    // A freshly built/spawned harvester starts hunting immediately, matching
    // the original -- see the header comment on add_harvester.
    u.harv = HarvState::SeekOre;
    return u;
}

void World::order_harvest(int x, int y) {
    for (std::size_t i = 0; i < units_.size(); ++i) {
        Unit& u = units_[i];
        // Player orders act on the player's (house-0) selected harvesters
        // only -- same house gate order_attack applies to its issuers.
        if (!u.selected || !u.is_harvester || u.house != 0 || !u.alive || u.die_time >= 0.0)
            continue;
        u.harv_cell_x = x;
        u.harv_cell_y = y;
        u.has_harv_cell = true;  // a player-directed cell is a real stamp too
        u.harv = HarvState::MoveToOre;
        u.harv_timer = 0.0;
        u.manual_hold = false;
        u.enter_target = -1;  // CB-1: a harvest order supersedes an enter order
        order_unit_move(static_cast<int>(i), x, y);
    }
}

void World::order_return() {
    for (Unit& u : units_) {
        // Same house-0 gate as order_harvest above.
        if (!u.selected || !u.is_harvester || u.house != 0 || !u.alive || u.die_time >= 0.0)
            continue;
        u.harv = HarvState::ReturnToRef;
        u.harv_timer = 0.0;
    }
}

std::vector<HarvestEvent> World::take_harvest_events() {
    std::vector<HarvestEvent> out;
    out.reserve(harvest_events_.size());
    for (const PendingHarvestEvent& e : harvest_events_)
        out.push_back({static_cast<HarvestEvent::Kind>(e.kind), e.unit});
    harvest_events_.clear();
    return out;
}

void World::clear_selection() {
    for (Unit& u : units_) u.selected = false;
}

int World::unit_at_cell(int x, int y) const {
    for (std::size_t i = 0; i < units_.size(); ++i) {
        const Unit& u = units_[i];
        if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) continue;  // carried: unpickable
        // CB-2 [RE-GATE APPLIED: aircraft.md §4/§6]: an airborne aircraft holds
        // no occ_ cell; do not pick it out of a cell scan (a Grounded aircraft
        // DOES hold its spawn cell and stays pickable). Data-driven no-op in
        // every existing demo (is_aircraft=false).
        if (u.is_aircraft && u.flight_state != FlightState::Grounded) continue;
        if (u.cell_x == x && u.cell_y == y) return static_cast<int>(i);
    }
    return -1;
}

void World::order_unit_move(int idx, int gx, int gy) {
    if (idx < 0 || idx >= static_cast<int>(units_.size())) return;
    Unit& u = units_[idx];
    // CB-2 Aircraft [RE-GATE APPLIED: aircraft.md §1/§6]: an aircraft order sets
    // a FLIGHT GOAL and takes off/cruises -- it never computes an A* path, never
    // tests passable_, and reserves no occ_ cell while airborne (straight-line
    // unpathed flight CONFIRMED; no RNG on the path). The ONE
    // aircraft-aware branch in order_unit_move; ground units (is_aircraft
    // false in every existing demo) take the exact existing path below.
    if (u.is_aircraft) {
        aircraft_any_ = true;                        // open the step_flight firewall
        u.flight_goal_x = gx;
        u.flight_goal_y = gy;
        u.has_flight_goal = true;
        if (u.flight_state == FlightState::Grounded) {
            // Takeoff: release the ground cell ONCE (spec §3.2) -- add_unit
            // claimed it at spawn; step_units skips aircraft so it is never
            // re-set. altitude climbs in step_flight's TakingOff branch.
            // keep=(-1,-1) releases ALL of this unit's cells including the
            // ground cell (the combat.cpp release_all_of(idx,-1,-1) precedent);
            // an airborne unit holds NO occupancy [spec §4].
            occ_.release_all_of(idx, -1, -1);
            u.flight_state = FlightState::TakingOff;
        } else if (u.flight_state == FlightState::Descending) {
            u.flight_state = FlightState::Cruising;  // re-launch mid-descent
        }
        u.path.clear();
        u.path_idx = 0;
        return;                                      // NO A*, NO occ reservation
    }
    // Drop any stale next-cell reservation from an in-flight order (the
    // occupancy may still mark a cell this unit reserved but will now never
    // enter, which would block other units forever); re-assert only the
    // current cell.
    occ_.release_all_of(idx, u.cell_x, u.cell_y);
    occ_.set(u.cell_x, u.cell_y, idx);
    // CB-3 Naval [RE-GATE APPLIED: naval.md §1]: select the domain grid -- Land
    // (the DEFAULT, every existing unit) resolves to the SAME passable_ object
    // (byte-identical); Water resolves to water_passable_. CONFIRMED: the naval
    // loco is Drive-shaped and shares the FootClass A* + one occ_ cell, so a
    // naval unit uses the EXACT ground occupancy path above (release_all_of + set
    // + find_path); only the grid differs. naval_any_ is a diagnostics flag.
    if (u.is_naval) naval_any_ = true;
    std::vector<Cellxy> path =
        find_path(grid_for(u.movement_domain), grid_w_, grid_h_, {u.cell_x, u.cell_y}, {gx, gy});
    if (path.size() <= 1) {
        // Assigned its own cell / unreachable: stay put, snap cleanly
        // (mirrors the single-unit Finding-1 cancel branch).
        u.path.clear();
        u.path_idx = 0;
        u.anim_time = 0.0;
        u.wait_time = 0.0;
        u.frame = seq_frame(anim_for_image(anims_, u.image).stand, u.facing, 0);
        u.fx = u.cell_x;
        u.fy = u.cell_y;
    } else {
        u.path = std::move(path);
        u.path_idx = 1;  // path[0] is the current cell
        u.anim_time = 0.0;
        u.wait_time = 0.0;
        // Do NOT reset fx/fy: preserves an in-flight walk across a reorder
        // (Task 12 Finding 1); step_units interpolates from any fx/fy.
    }
}

void World::order_move_selected(int goal_x, int goal_y) {
    std::vector<int> sel;
    std::vector<Cellxy> cur;
    for (std::size_t i = 0; i < units_.size(); ++i)
        if (units_[i].selected && units_[i].alive && units_[i].die_time < 0.0) {
            sel.push_back(static_cast<int>(i));
            cur.push_back({units_[i].cell_x, units_[i].cell_y});
        }
    if (sel.empty()) return;

    std::vector<Cellxy> targets =
        assign_formation({goal_x, goal_y}, cur, passable_, grid_w_, grid_h_, occ_);

    for (std::size_t k = 0; k < sel.size(); ++k) {
        Unit& u = units_[sel[k]];
        // An explicit move order CANCELS engagement (RTS convention, and the
        // spec's "idle/guarding" wording: a unit executing a player move is
        // neither): drop the target so the hold-at-range pass and pursuit
        // machinery never touch this unit's move path. Building targets
        // (Task 4) are cancelled the same way -- without this a unit ordered
        // away from a building it was attacking would keep firing at it
        // whenever back in range, forever (target_building never self-clears
        // while the building lives).
        u.target = -1;
        u.target_building = -1;
        u.attack_order = false;
        // CB-1: a new move order supersedes a pending enter/unload
        // [RE-GATE APPLIED: transport-cargo.md §3/§4 -- kept: our
        // order-model convention; the hunt pinned no contrary rule].
        // Provably no-op in every existing demo (fields hold their inert
        // defaults).
        u.enter_target = -1;
        u.unload_pending = false;
        // Economy phase [RE: economy.md §1 auto-resume]: a player move order
        // parks the harvester mission FSM (step_harvest's manual_hold branch)
        // for the duration of this walk; it resumes at SeekOre the instant
        // the unit arrives, load/dock state intact.
        if (u.is_harvester) u.manual_hold = true;
        const Cellxy tgt = targets[k];
        order_unit_move(sel[k], tgt.x, tgt.y);
    }
}

void World::order_attack(int target_idx) {
    if (target_idx < 0 || target_idx >= static_cast<int>(units_.size())) return;
    const Unit& tgt = units_[target_idx];
    // Can't order an attack on a dead/dying target, or on a house-0 unit
    // (order_attack is issued for the player's house-0 units against
    // hostiles -- a house-0 target would mean "attack a friendly").
    if (!tgt.alive || tgt.die_time >= 0.0 || tgt.house == 0) return;

    for (std::size_t i = 0; i < units_.size(); ++i) {
        Unit& u = units_[i];
        if (!u.selected || u.house != 0 || !u.alive || u.die_time >= 0.0) continue;
        // Player guard kept above (selected + house 0); the shared mutation
        // tail (target/attack_order + occupancy re-assert + path clear) is
        // extracted into commit_attack_target so there is ONE copy of the
        // target-commit body [RE-GATE APPLIED: ai-team.md §5].
        commit_attack_target(static_cast<int>(i), target_idx, /*target_building=*/-1);
    }
}

int World::add_building(int type, int cx, int cy, int fw, int fh, int house, int strength,
                        int armor_ix, double buildup_total, bool instant, int sight) {
    Building b;
    b.type = type;
    b.cx = cx;
    b.cy = cy;
    b.fw = fw;
    b.fh = fh;
    b.house = house;
    b.hp = b.max_hp = strength;
    b.armor_ix = armor_ix;
    b.sight = sight;  // [RE: shroud.md §2] reveal radius; inert until Task 3
    b.buildup_total = buildup_total;
    b.state = instant ? Building::State::Idle : Building::State::Buildup;
    buildings_.push_back(b);
    const int idx = static_cast<int>(buildings_.size()) - 1;
    for (const Cellxy& cell : foundation_cells(cx, cy, fw, fh)) {
        if (cell.x < 0 || cell.y < 0 || cell.x >= grid_w_ || cell.y >= grid_h_) continue;
        passable_[static_cast<std::size_t>(cell.y) * grid_w_ + cell.x] = 0;
    }
    return idx;
}

int World::building_at_cell(int x, int y) const {
    for (std::size_t i = 0; i < buildings_.size(); ++i) {
        const Building& b = buildings_[i];
        if (!b.alive || b.state == Building::State::Dead) continue;
        if (x >= b.cx && x < b.cx + b.fw && y >= b.cy && y < b.cy + b.fh)
            return static_cast<int>(i);
    }
    return -1;
}

void World::order_attack_building(int b_idx) {
    if (b_idx < 0 || b_idx >= static_cast<int>(buildings_.size())) return;
    const Building& tgt = buildings_[b_idx];
    // Can't order an attack on a dead/Dead-state/house-0 (friendly) building
    // -- mirrors order_attack's own guard against a house-0 target.
    if (!tgt.alive || tgt.state == Building::State::Dead || tgt.house == 0) return;

    for (std::size_t i = 0; i < units_.size(); ++i) {
        Unit& u = units_[i];
        if (!u.selected || u.house != 0 || !u.alive || u.die_time >= 0.0) continue;
        // Player guard kept above; shared mutation tail via commit_attack_target
        // [RE-GATE APPLIED: ai-team.md §5 — de-duplication].
        commit_attack_target(static_cast<int>(i), /*target_unit=*/-1, b_idx);
    }
}

// --- Production phase [RE: production.md §1.3/§1.4] ---

void World::set_rally(int b_idx, int x, int y) {
    if (b_idx < 0 || b_idx >= static_cast<int>(buildings_.size()) || !buildings_[b_idx].alive)
        return;
    buildings_[b_idx].rally_x = x;
    buildings_[b_idx].rally_y = y;
    buildings_[b_idx].has_rally = true;
}

void World::set_primary(int b_idx, const std::vector<int>& factory_types) {
    ra2engine::set_primary(buildings_, factory_types, b_idx);
}

int World::launch_unit(int factory_idx, const BuildingTypeDef& factory_def, const UnitCombat& c) {
    if (factory_idx < 0 || factory_idx >= static_cast<int>(buildings_.size())) return -1;
    const Building& f = buildings_[factory_idx];   // rally scalars are copied to locals below
    auto free_cell = [this](int x, int y) {
        // occ_.is_free sees BOTH committed cells AND a mid-hop unit's next-cell
        // reservation (step_units reserves occ.set(next, self) while cell_x
        // still holds the old cell). unit_at_cell only scans committed
        // cell_x/cell_y, so it would treat a reserved next-cell as free and
        // add_unit's unconditional occ_.set would then steal the mover's claim
        // -> two units in one cell. Corpses own nothing, matching the old
        // unit_at_cell corpse-skip.
        return walkable(x, y) && occ_.is_free(x, y);
    };
    // [RE-GATE APPLIED: production.md §1.3] 8-ring walk to the first free cell.
    const Cellxy ex = exit_cell(f, factory_def, free_cell);
    if (ex.x < 0) return -1;                        // no free ring cell -> HOLD
    const int house = f.house;
    const bool has_rally = f.has_rally;
    const int rx = f.rally_x, ry = f.rally_y;
    // Engineer/Spy Capture [RE: capture.md §B5]: if this house's factory was
    // spied, produced units spawn as veterans. produced_veteran_[house] is false
    // in every existing demo -> cv.veteran_rank stays 0 -> byte-identical.
    UnitCombat cv = c;
    cv.veteran_rank = produced_veteran(house) ? 1 : 0;
    add_combat_unit(ex.x, ex.y, house, cv);
    const int ui = static_cast<int>(units_.size()) - 1;
    int gx = rx, gy = ry;
    if (!has_rally) {
        // Default gather = nearest free cell from the building's own coord
        // [RE-GATE APPLIED: production.md §1.4, FUN_00500020]: the unit now
        // occupies the door cell, so re-running the ring walk yields the
        // exit-adjacent gather cell.
        const Cellxy g = exit_cell(buildings_[factory_idx], factory_def, free_cell);
        if (g.x < 0) return ui;                     // fully boxed in: stand at the door
        gx = g.x; gy = g.y;
    }
    order_unit_move(ui, gx, gy);   // cancels cleanly if unreachable/self
    return ui;
}

bool World::factory_category_lost(const std::vector<int>& factory_types, int house) const {
    for (const Building& b : buildings_) {
        if (!b.alive || b.state != Building::State::Idle || b.house != house) continue;
        for (int t : factory_types)
            if (b.type == t) return false;   // a survivor
    }
    return true;
}

// --- CB-1 Transports/Cargo [RE: transport-cargo.md] --------------------------

void World::order_enter_transport(int t_idx) {
    if (t_idx < 0 || t_idx >= static_cast<int>(units_.size())) return;
    const Unit& t = units_[t_idx];
    if (!t.alive || t.die_time >= 0.0 || t.house != 0 || t.passengers_max <= 0) return;
    // Gather eligible movers: selected house-0, alive, non-dying, not the
    // transport itself, passing can_load NOW (capacity re-checked at
    // boarding -- orders may oversubscribe) [spec §3].
    std::vector<int> movers;
    std::vector<Cellxy> cur;
    for (std::size_t i = 0; i < units_.size(); ++i) {
        const Unit& u = units_[i];
        if (!u.selected || u.house != 0 || !u.alive || u.die_time >= 0.0) continue;
        if (static_cast<int>(i) == t_idx) continue;
        if (!can_load(units_, t_idx, static_cast<int>(i))) continue;
        movers.push_back(static_cast<int>(i));
        cur.push_back({units_[i].cell_x, units_[i].cell_y});
    }
    if (movers.empty()) return;
    cargo_any_ = true;   // the step_cargo firewall opens on the FIRST order (spec §8)
    // Batch per transport into assign_formation around the transport's cell,
    // so each passenger gets a DISTINCT free nearby cell (the same batching
    // step() uses for combat approach requests) [spec §3].
    const std::vector<Cellxy> targets =
        assign_formation({t.cell_x, t.cell_y}, cur, passable_, grid_w_, grid_h_, occ_);
    for (std::size_t k = 0; k < movers.size(); ++k) {
        Unit& u = units_[movers[k]];
        u.target = -1;               // an enter order supersedes engagement
        u.target_building = -1;
        u.attack_order = false;
        u.unload_pending = false;    // ...and any pending unload on the mover
        u.enter_target = t_idx;
        // A harvester passenger parks its mission FSM for the approach --
        // data-unreachable with HTK (SizeLimit=2 < harvester Size=3), guarded
        // anyway [spec §4].
        if (u.is_harvester) u.manual_hold = true;
        order_unit_move(movers[k], targets[k].x, targets[k].y);
    }
}

void World::order_unload_selected() {
    for (Unit& u : units_) {
        if (!u.selected || u.house != 0 || !u.alive || u.die_time >= 0.0) continue;
        if (u.passengers_max <= 0 || u.cargo.empty()) continue;
        u.unload_pending = true;
        cargo_any_ = true;
    }
}

std::vector<CargoEvent> World::take_cargo_events() {
    std::vector<CargoEvent> out = std::move(cargo_events_);
    cargo_events_.clear();
    return out;
}

void World::step_cargo(double /*dt*/) {
    // FIREWALL (spec §8): until the first enter/unload order ever, this whole
    // function is one branch -- the only cost every existing demo pays.
    if (!cargo_any_) return;

    auto free_cell = [this](int x, int y) {
        // The SAME free predicate launch_unit uses: walkable + occ_.is_free,
        // which sees mid-hop reservations [spec §5].
        return walkable(x, y) && occ_.is_free(x, y);
    };

    // 1) Transport-death disposition [RE-GATE APPLIED: transport-cargo.md §5
    //    -- CONFIRMED: every death path in the original (FUN_00700DA0 /
    //    FUN_00707130 / ~UnitClass) deletes each passenger with NO survival
    //    branch; OpenTopped grants NO death survival (it governs fire-out
    //    while alive -- an RE-documented carry-forward for the fire-out
    //    phase, not CB-1 code). Every carried unit dies WITH the transport,
    //    tombstoned directly (alive=false, no die anim -- it was never
    //    on-screen), coords stamped to the transport's cell.] A transport
    //    killed by step_combat in frame N is handled here
    //    in frame N+1 (step_cargo precedes step_combat in the step order) --
    //    a one-frame lag, deterministic, documented.
    for (std::size_t ti = 0; ti < units_.size(); ++ti) {
        Unit& t = units_[ti];
        if (t.cargo.empty()) continue;
        if (t.alive && t.die_time < 0.0) continue;   // transport still fine
        for (int p : t.cargo) {
            if (p < 0 || p >= static_cast<int>(units_.size())) continue;
            Unit& u = units_[p];
            u.alive = false;          // tombstone directly (no die anim)
            u.die_time = -1.0;
            u.hp = 0;
            u.cargo_of = -1;
            u.cell_x = t.cell_x;      // coords stamped to the transport's cell
            u.cell_y = t.cell_y;
            u.fx = t.cell_x;
            u.fy = t.cell_y;
        }
        t.cargo.clear();
        t.unload_pending = false;
    }

    // 2) Boarding pass -- ascending unit index (the deterministic last-slot
    //    winner [RE-GATE APPLIED: transport-cargo.md §6 -- the original's
    //    container ops consult NO RNG (FUN_007C4D00 is ftol, not a draw);
    //    the last slot goes to whichever arrival is processed first, so
    //    ascending-index processing is the faithful analogue -- and under
    //    LIFO the LAST-processed boarder sits at the back and ejects FIRST]).
    for (std::size_t i = 0; i < units_.size(); ++i) {
        Unit& u = units_[i];
        if (u.enter_target < 0) continue;
        if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) {
            u.enter_target = -1;
            continue;
        }
        const int ti = u.enter_target;
        if (ti < 0 || ti >= static_cast<int>(units_.size())) {
            u.enter_target = -1;
            continue;
        }
        Unit& t = units_[ti];
        if (!t.alive || t.die_time >= 0.0) {   // transport died: cancel cleanly
            u.enter_target = -1;               // (unit stands) [spec §3]
            continue;
        }
        const int adx = u.cell_x - t.cell_x, ady = u.cell_y - t.cell_y;
        const int cheb = std::max(adx < 0 ? -adx : adx, ady < 0 ? -ady : ady);
        if (cheb <= 1) {
            // Boarding trigger point = Chebyshev adjacency [RE-GATE APPLIED:
            // transport-cargo.md §3 -- DOCUMENTED DEVIATION: the original
            // boards on CO-LOCATION (the passenger paths onto the
            // transport's own tile, FUN_004D8ED0 case 0x12); our
            // one-unit-per-cell grid has no walk-onto-occupied notion, so
            // adjacency <= 1 is the adjudicated stand-in (same reachable
            // set, deterministic)].
            if (can_load(units_, ti, static_cast<int>(i))) {
                // Remove-from-map: release ALL occupancy (current cell AND
                // any mid-hop reservation -- keep-cell (-1,-1) matches
                // nothing), clear path/selection/targets, THEN link [spec §3].
                occ_.release_all_of(static_cast<int>(i), -1, -1);
                u.path.clear();
                u.path_idx = 0;
                u.selected = false;        // boarding force-clears selection (spec §4)
                u.target = -1;
                u.target_building = -1;
                u.attack_order = false;
                u.wait_time = 0.0;
                t.cargo.push_back(static_cast<int>(i));
                u.cargo_of = static_cast<int>(ti);
                u.enter_target = -1;
                cargo_events_.push_back({CargoEvent::Kind::Board, static_cast<int>(i),
                                         static_cast<int>(ti),
                                         static_cast<int>(t.cargo.size()),
                                         t.passengers_max});   // x = COUNT after
                                                               // the board [§1]
            } else {
                // Arrived but REFUSED (full / no longer eligible): the enter
                // order cancels cleanly, the unit stands [spec §3; the §7
                // capacity-refusal witness].
                u.enter_target = -1;
                cargo_events_.push_back({CargoEvent::Kind::Refuse, static_cast<int>(i),
                                         static_cast<int>(ti),
                                         static_cast<int>(t.cargo.size()),
                                         t.passengers_max});   // x = COUNT (full)
            }
        } else if (!u.moving()) {
            // Not adjacent and stopped (its approach path completed/cancelled
            // while the transport moved): re-path to the nearest free cell
            // around the transport's CURRENT cell via the EXISTING pure
            // nearest_free_cells helper (group_move.hpp) -- deterministic
            // (our stand-in for the original's staggered pursuit re-path;
            // documented deviation) [spec §3]. No free cell nearby -> keep
            // standing, retried next frame.
            const std::vector<Cellxy> g = nearest_free_cells(
                {t.cell_x, t.cell_y}, 1, passable_, grid_w_, grid_h_, occ_);
            if (!g.empty()) order_unit_move(static_cast<int>(i), g[0].x, g[0].y);
        }
    }

    // 3) Unload pass -- ONE eject per transport per logic frame [RE-GATE
    //    APPLIED: transport-cargo.md §4 -- CONFIRMED one per mission tick
    //    (FUN_00415EE0: if a passenger remains, eject one and return). The
    //    DeployTime deploy-START delay is NOT implemented in CB-1 (its scale
    //    is unpinned -- documented carry-forward), so ejection begins
    //    immediately at one-per-tick].
    for (std::size_t ti = 0; ti < units_.size(); ++ti) {
        Unit& t = units_[ti];
        if (!t.unload_pending) continue;
        if (!t.alive || t.die_time >= 0.0) { t.unload_pending = false; continue; }
        if (t.cargo.empty()) { t.unload_pending = false; continue; }
        if (t.moving()) continue;              // a STATIONARY transport ejects [spec §5]
        const int p = next_eject(t);           // LIFO -- the LAST-boarded leaves
                                               // first [RE-GATE APPLIED:
                                               // transport-cargo.md §6 -- CONFIRMED]
        if (p < 0 || p >= static_cast<int>(units_.size())) {  // defensive: drop a
            t.cargo.pop_back();                               // corrupt entry (the
            continue;                                         // one next_eject saw)
        }
        // The recovered facing-scatter picker [RE-GATE APPLIED:
        // transport-cargo.md §4]: one step out along body_facing +/- 0x4000,
        // the side alternating per eject via eject_counter's low bit;
        // nearest-free-neighbour fallback. CB-1 transports are vehicles
        // (HTK/FV) so body_facing is live; a non-vehicle transport would
        // eject along its spawn default -- data-unreachable on our roster.
        const Cellxy cell = eject_cell(t.cell_x, t.cell_y, t.body_facing,
                                       t.eject_counter, free_cell);
        if (cell.x < 0) continue;              // boxed in: HOLD -- passenger stays
                                               // aboard, unload_pending stays set,
                                               // retried every frame [RE-GATE
                                               // APPLIED: transport-cargo.md §4 --
                                               // CONFIRMED put-back (AddPassenger
                                               // back), never a drop or an abort]
        Unit& u = units_[p];
        u.cell_x = cell.x;                     // stamp coords fresh on eject
        u.cell_y = cell.y;
        u.fx = cell.x;
        u.fy = cell.y;
        occ_.set(cell.x, cell.y, p);           // claim occupancy BEFORE combat
                                               // targeting sees the unit again
        u.cargo_of = -1;
        u.guard_x = cell.x;                    // guard anchor = the eject cell
        u.guard_y = cell.y;
        const int f = facing_from_delta(cell.x - t.cell_x, cell.y - t.cell_y);
        if (f >= 0) u.facing = f;              // eject facing = OUTWARD along the
                                               // eject delta (= the +/-90-deg
                                               // scatter side when the primary
                                               // cell was taken) [RE-GATE APPLIED:
                                               // transport-cargo.md §4 -- outward
                                               // CONFIRMED structurally; the final
                                               // facing is PROBABLE]
        if (u.is_vehicle) {
            u.body_facing = facing_u16_from_delta(cell.x - t.cell_x, cell.y - t.cell_y);
            u.turret_facing = u.body_facing;
        }
        if (const ra2assets::MapCell* mc = map_->at(cell.x, cell.y)) u.level = mc->level;
        t.cargo.pop_back();                    // LIFO pop (the entry next_eject saw)
        ++t.eject_counter;                     // alternate the scatter side next
                                               // eject [transport-cargo.md §4]
        cargo_events_.push_back({CargoEvent::Kind::Eject, p, static_cast<int>(ti),
                                 cell.x, cell.y});
        if (t.cargo.empty()) t.unload_pending = false;
    }
}

void World::step_flight(double dt) {
    // FIREWALL (spec §8): until the first aircraft is ordered, this whole
    // function is one branch -- the only cost every existing demo pays.
    if (!aircraft_any_) return;

    for (Unit& u : units_) {
        if (!u.is_aircraft) continue;
        if (!u.alive || u.die_time >= 0.0) continue;
        if (u.flight_state == FlightState::Grounded) continue;  // parked: nothing to do

        // 1) Altitude: ascend toward cruise (TakingOff) / descend toward 0
        //    (Descending) at the state's rate; hold at cruise when Cruising
        //    [RE-GATE APPLIED: aircraft.md §2].
        const bool descending = (u.flight_state == FlightState::Descending);
        const double target = descending ? 0.0 : u.cruise_altitude;
        const double rate = descending ? u.descend_rate : u.climb_rate;
        u.altitude = step_altitude(u.altitude, target, rate * dt);
        u.flight_state = advance_flight_state(u.flight_state, u.altitude, u.cruise_altitude);

        // 2) Cruise translation: turn toward the goal AND advance straight-line
        //    the SAME frame (no heading-complete gate -- Process turns and
        //    translates together; with OmniFire the flyer never needs to face
        //    the target). Takeoff CLIMB and horizontal TRAVEL OVERLAP: the unit
        //    translates while still ascending (TakingOff), it does NOT hold
        //    horizontal until airborne -- the Kirov leaves the factory climbing
        //    and moving at once [RE-GATE APPLIED: aircraft.md §1/§2/§3 -- D3].
        //    cell_x/cell_y track the ground projection of fx/fy for the shadow
        //    anchor + combat range, but claim NO occupancy [spec §4].
        if ((u.flight_state == FlightState::Cruising ||
             u.flight_state == FlightState::TakingOff) && u.has_flight_goal) {
            const double gx = static_cast<double>(u.flight_goal_x);
            const double gy = static_cast<double>(u.flight_goal_y);
            u.body_facing = heading_toward(u.body_facing, gx - u.fx, gy - u.fy,
                                           u.flight_turn * dt, u.omni_fire);
            const bool arrived = cruise_step(u.fx, u.fy, gx, gy, u.flight_speed * dt);
            u.cell_x = static_cast<int>(std::lround(u.fx));
            u.cell_y = static_cast<int>(std::lround(u.fy));
            if (arrived) u.has_flight_goal = false;   // hover at the goal (BalloonHover)
        }
    }
}

// --- AI-1 base-planning phase [RE: ai-base.md] ------------------------------

void World::enable_ai_house(int house, AiHouseConfig cfg) {
    if (house < 0) return;
    if ((int)ai_houses_.size() <= house) ai_houses_.resize(house + 1);
    HouseAiRec& rec = ai_houses_[house];
    rec.is_ai = true;
    rec.cfg = std::move(cfg);
    rec.state = AiBaseState{};
    rec.team_state = AiTeamState{};  // AI-2: fresh team state per enable
    rec.trigger_state = make_trigger_state(rec.cfg.team.aimd);  // AI-3: current
                                    // weights seeded from starting_weight
                                    // (NB reads rec.cfg — cfg was moved from)
    rec.state.difficulty = rec.cfg.difficulty;
    rec.queues = ProductionQueues{};
    rec.credits = rec.cfg.start_credits;
    rec.queues.set_credits(rec.credits);
    ai_any_ = true;
}

bool World::ai_enabled(int house) const {
    return house >= 0 && house < (int)ai_houses_.size() && ai_houses_[house].is_ai;
}

const AiBaseState* World::ai_state(int house) const {
    return ai_enabled(house) ? &ai_houses_[house].state : nullptr;
}

double World::ai_credits(int house) const {
    return ai_enabled(house) ? ai_houses_[house].credits : 0.0;
}

// --- CB-4 Superweapons [RE: superweapon.md] ---
void World::grant_superweapon(int house, SuperweaponType type, double recharge_secs,
                              int dmg, double cell_spread, double pmax,
                              const std::array<int, 11>& verses) {
    if (house < 0) return;
    if ((int)sw_houses_.size() <= house) sw_houses_.resize(house + 1);   // enable_ai_house precedent
    HouseSuperweapon& rec = sw_houses_[house];
    rec.has_sw = true;
    rec.sw = Superweapon{};
    rec.sw.type = type;
    rec.sw.recharge_secs = recharge_secs;
    rec.eff_damage = dmg;
    rec.eff_cell_spread = cell_spread;
    rec.eff_percent_at_max = pmax;
    rec.eff_verses = verses;
    sw_any_ = true;                                                       // open the firewall
}

bool World::fire_superweapon(int house, int cx, int cy) {
    if (house < 0 || house >= (int)sw_houses_.size()) return false;
    HouseSuperweapon& rec = sw_houses_[house];
    if (!rec.has_sw || !rec.sw.is_ready()) return false;   // refused unless READY (§4)

    // Transient virtual detonator [spec §5.1]: a value-initialized STACK Unit
    // (Unit struct UNCHANGED -- no persistent field). Its house + fx/fy (target
    // cell, in CELL-space: apply_spread_damage's dist_leptons scales by
    // kLeptonsPerCell internally, combat.cpp:50/95, and an idle unit's fx/fy IS
    // its integer cell) + the four warhead fields are EXACTLY and ONLY the fields
    // apply_spread_damage reads from its attacker/center args.
    Unit virt;
    virt.house = house;
    virt.fx = static_cast<double>(cx);
    virt.fy = static_cast<double>(cy);
    virt.w_damage = rec.eff_damage;                 // [NukePayload] Damage=600
    virt.w_cell_spread = rec.eff_cell_spread;        // [NUKE] CellSpread=10
    virt.w_percent_at_max = rec.eff_percent_at_max;  // [NUKE] PercentAtMax=.02
    virt.w_verses.assign(rec.eff_verses.begin(), rec.eff_verses.end());  // [NUKE] Verses

    // Detonate: REUSE the EXACT CB-2 applicator (combat.cpp) -- virt is both the
    // attacker (house + warhead) and the center (fx/fy). Every alive, non-carried,
    // non-aircraft ENEMY unit within round(CellSpread)*kLeptonsPerCell of the cell
    // takes damage_vs(static_cast<int>(Damage*scale), NUKE-Verses, armor) with scale
    // falling LINEARLY 1.0 -> PercentAtMax across the radius; enemies-only spares the
    // firing house. NO new damage math (spec §5/§8).
    apply_spread_damage(units_, occ_, virt, virt);

    // [RE-GATE APPLIED: superweapon.md §5.3] BUILDING SPLASH -- CONFIRMED: the
    // [NUKE] warhead area-damage (FUN_00489190) enumerates EVERY in-radius object
    // with IsActive && Strength>0 -- buildings included (Strength>0), corroborated
    // by [NUKE] WallAbsoluteDestroyer=yes + Wood=yes -- so the nuke levels
    // structures in the blast. apply_spread_damage above damages UNITS only (the
    // CB-2 aerial-bomb scope), so this SW-ONLY loop adds the building leg,
    // mirroring the unit falloff EXACTLY: the same round(CellSpread) radius, the
    // same linear 1.0->PercentAtMax scale, the same static_cast<int> damage
    // truncation, damage_vs against the building's armor, and the same enemies-only
    // rule (firing house spared -- the documented CB-4 divergence from vanilla
    // friendly-fire, consistent with the unit splash). It is an ADDED clause
    // reached ONLY by this fire_superweapon call, which no existing demo issues, so
    // every gate stays byte-identical (buildings_ is untouched by it everywhere
    // else). Distance is to the building's NEAREST foundation cell (a multi-cell
    // footprint is hit at its EDGE -- combat.cpp's building-range convention,
    // combat.cpp:233-244). hp<=0 -> kill_building (Dead + die_time=0, building.hpp);
    // World::step's existing Dead-building pass releases the foundation blocking
    // exactly once and step_buildings tombstones it -- no occ_ handling needed here.
    const double radius_cells = std::round(rec.eff_cell_spread);        // round(CellSpread)
    const double radius_lept = radius_cells * 256.0;                    // kLeptonsPerCell = 256
    const double b_pmax = rec.eff_percent_at_max;                       // [NUKE] PercentAtMax
    const std::vector<int> nuke_verses(rec.eff_verses.begin(), rec.eff_verses.end());
    for (Building& b : buildings_) {
        if (!b.alive || b.state == Building::State::Dead) continue;     // gone/already dying
        if (b.house == house) continue;                                // enemies only (firing house spared)
        double d = -1.0;                                               // nearest-foundation-cell distance (leptons)
        for (const Cellxy& cell : foundation_cells(b.cx, b.cy, b.fw, b.fh)) {
            const double dx = (static_cast<double>(cx) - static_cast<double>(cell.x)) * 256.0;
            const double dy = (static_cast<double>(cy) - static_cast<double>(cell.y)) * 256.0;
            const double dc = std::sqrt(dx * dx + dy * dy);
            if (d < 0.0 || dc < d) d = dc;
        }
        if (d < 0.0 || d > radius_lept) continue;                     // outside the splash
        const double frac = radius_lept > 0.0 ? d / radius_lept : 0.0;
        const double scale = 1.0 - (1.0 - b_pmax) * frac;             // 1.0 -> pmax linear (unit-identical, combat.cpp:98)
        const int base = static_cast<int>(rec.eff_damage * scale);    // trunc (combat.cpp:99)
        const int dmg = damage_vs(base, nuke_verses, b.armor_ix);     // [NUKE] Verses vs building armor
        b.hp -= dmg;
        if (b.hp <= 0) kill_building(b);                              // reuse the existing destroy path
    }

    rec.sw.on_fired();   // reset timer=0, clear ready (§4)
    return true;
}

void World::step_superweapon(double dt) {
    if (!sw_any_) return;                        // firewall (spec §8) -- no-op everywhere else
    for (HouseSuperweapon& rec : sw_houses_)     // house-id order; pure arithmetic, no RNG (§3.5)
        if (rec.has_sw) rec.sw.tick(dt);
}

bool World::sw_ready(int house) const {
    return house >= 0 && house < (int)sw_houses_.size() &&
           sw_houses_[house].has_sw && sw_houses_[house].sw.is_ready();
}

double World::sw_charge_fraction(int house) const {
    if (house < 0 || house >= (int)sw_houses_.size() || !sw_houses_[house].has_sw) return 0.0;
    return sw_houses_[house].sw.charge_fraction();
}

const Superweapon* World::sw_state(int house) const {
    if (house < 0 || house >= (int)sw_houses_.size() || !sw_houses_[house].has_sw) return nullptr;
    return &sw_houses_[house].sw;
}

// --- Engineer/Spy Capture phase [RE: capture.md] ---
bool World::capture_building(int b_idx, int new_house, int startup) {
    if (b_idx < 0 || b_idx >= (int)buildings_.size()) return false;
    Building& b = buildings_[b_idx];
    if (!b.alive || b.state == Building::State::Dead) return false;
    if (new_house < 0) return false;            // lower-bound guard [Task-2 review Minor]
    const int old_house = b.house;
    if (new_house == old_house) return false;   // ChangeOwner no-op if same owner

    capture_any_ = true;                        // open the firewall

    // (1) flip the flag -- power/economy/buildability/sidebar are per-frame
    //     derived over b.house, so they re-home automatically (Approach A).
    b.house = new_house;

    // (2) SW move (only a SW-granting building).
    if (b.grants_superweapon) transfer_superweapon(old_house, new_house);

    // (3) AI base-node resync both sides (reuse the AI-1 mechanism).
    ai_resync_captured_building(b_idx, old_house, new_house);

    // (4) primary-factory flag reset (see the header rationale).
    b.primary = false;

    // (5) ProduceCashStartup -- only when captured FROM the neutral house
    //     [RE: capture.md §A3]. Grant to the NEW owner once.
    if (grants_startup_from_neutral(old_house) && startup > 0 &&
        new_house >= 0 && new_house < 2) {
        house_credits_[new_house] += (double)startup;
    }
    return true;
}

bool World::infiltrate_building(int b_idx, int spy_house, InfiltratePurpose purpose,
                                double steal_pct, int blackout) {
    if (b_idx < 0 || b_idx >= (int)buildings_.size()) return false;
    Building& b = buildings_[b_idx];
    if (!b.alive || b.state == Building::State::Dead) return false;
    const int victim = b.house;
    if (victim == spy_house) return false;          // self-spy no-op [RE: capture.md §B1]
    capture_any_ = true;
    switch (purpose) {
        case InfiltratePurpose::Refinery: {          // [RE: capture.md §B3 CONFIRMED]
            if (victim >= 0 && victim < 2 && spy_house >= 0 && spy_house < 2) {
                const int steal = cash_steal_amount(house_credits_[victim], steal_pct);
                house_credits_[victim] -= (double)steal;
                house_credits_[spy_house] += (double)steal;
            }
            break;
        }
        case InfiltratePurpose::PowerPlant:          // [RE: capture.md §B2 CONFIRMED]
            if (victim >= 0 && victim < 3) blackout_frames_[victim] = blackout;
            break;
        case InfiltratePurpose::Superweapon:         // [RE: capture.md §B4 CONFIRMED]
            reset_superweapon(victim);
            break;
        case InfiltratePurpose::Factory:             // [RE: capture.md §B5] Task 6 wires the flag
            if (spy_house >= 0 && spy_house < 3) produced_veteran_[spy_house] = true;
            break;
        case InfiltratePurpose::RadarLab:            // [RE-FALLBACK: capture.md §B6] inert (no shroud)
        case InfiltratePurpose::None:
            break;
    }
    return true;
}

bool World::repair_building(int b_idx, int by_house) {
    if (b_idx < 0 || b_idx >= (int)buildings_.size()) return false;
    Building& b = buildings_[b_idx];
    if (!b.alive || b.state == Building::State::Dead) return false;
    if (b.house != by_house) return false;      // repair applies to OWN buildings
    if (b.hp >= b.max_hp) return false;         // full HP -> no action (engineer NOT consumed)
    capture_any_ = true;
    b.hp = b.max_hp;                            // heal to full [RE-GATE APPLIED §A5]
    return true;
}

void World::consume_unit(int idx) {
    if (idx < 0 || idx >= (int)units_.size()) return;
    Unit& u = units_[idx];
    if (!u.alive) return;                        // already tombstoned
    // Direct tombstone (no die anim -- the engineer walked INTO the building),
    // mirroring the transport carried-unit death path, then release ALL cells
    // (current + any mid-hop reservation) exactly as combat.cpp's kill path.
    occ_.release_all_of(idx, -1, -1);
    u.alive = false;
    u.die_time = -1.0;
    u.hp = 0;
    u.selected = false;
    u.target = -1;
    u.target_building = -1;
    u.path.clear();
    u.path_idx = 0;
}

// Nearest foundation cell of building `b` to (px,py), and the nearest free
// walkable cell adjacent to that foundation cell (the approach goal). Returns
// false (leaving g unset) only if the building has no foundation cells.
static bool engineer_approach_goal(const Building& b, int px, int py,
                                   const std::vector<uint8_t>& passable, int w, int h,
                                   const Occupancy& occ, Cellxy& g) {
    const auto cells = foundation_cells(b.cx, b.cy, b.fw, b.fh);
    if (cells.empty()) return false;
    int best = -1, nx = cells[0].x, ny = cells[0].y;
    for (const Cellxy& cell : cells) {
        const int dx = px - cell.x, dy = py - cell.y;
        const int cheb = std::max(dx < 0 ? -dx : dx, dy < 0 ? -dy : dy);
        if (best < 0 || cheb < best) { best = cheb; nx = cell.x; ny = cell.y; }
    }
    // A raw move onto the occupied foundation cell would cancel as unreachable,
    // so aim at the nearest FREE cell around it (same helper the transport
    // re-path uses); the foundation cell itself is not walkable -> the first hit
    // is adjacent to the footprint.
    const std::vector<Cellxy> free = nearest_free_cells({nx, ny}, 1, passable, w, h, occ);
    if (free.empty()) return false;
    g = free[0];
    return true;
}

void World::order_engineer_capture(int eng_idx, int b_idx) {
    if (eng_idx < 0 || eng_idx >= static_cast<int>(units_.size())) return;
    if (b_idx < 0 || b_idx >= static_cast<int>(buildings_.size())) return;
    Unit& u = units_[eng_idx];
    if (!u.alive || u.die_time >= 0.0) return;
    const Building& b = buildings_[b_idx];
    if (!b.alive || b.state == Building::State::Dead) return;
    capture_any_ = true;             // open the step_capture firewall
    // A capture order supersedes any engagement / pending enter (mirrors
    // order_enter_transport's supersede tail).
    u.target = -1;
    u.target_building = -1;
    u.attack_order = false;
    u.unload_pending = false;
    u.enter_target = -1;
    u.infiltrate_target = -1;        // a unit never carries both deferred intents
    u.capture_target = b_idx;
    Cellxy g;
    if (engineer_approach_goal(b, u.cell_x, u.cell_y, passable_, grid_w_, grid_h_, occ_, g))
        order_unit_move(eng_idx, g.x, g.y);
}

void World::order_spy_infiltrate(int spy_idx, int b_idx, InfiltratePurpose purpose,
                                 double steal_pct, int blackout) {
    if (spy_idx < 0 || spy_idx >= static_cast<int>(units_.size())) return;
    if (b_idx < 0 || b_idx >= static_cast<int>(buildings_.size())) return;
    Unit& u = units_[spy_idx];
    if (!u.alive || u.die_time >= 0.0) return;
    const Building& b = buildings_[b_idx];
    if (!b.alive || b.state == Building::State::Dead) return;
    capture_any_ = true;             // open the step_capture firewall
    // An infiltrate order supersedes any engagement / pending enter (mirrors
    // order_engineer_capture's supersede tail).
    u.target = -1;
    u.target_building = -1;
    u.attack_order = false;
    u.unload_pending = false;
    u.enter_target = -1;
    u.capture_target = -1;           // a unit never carries both deferred intents
    u.infiltrate_target = b_idx;
    u.infiltrate_purpose = static_cast<int>(purpose);   // pre-resolved at gesture layer
    u.infiltrate_steal_pct = steal_pct;
    u.infiltrate_blackout = blackout;
    Cellxy g;
    if (engineer_approach_goal(b, u.cell_x, u.cell_y, passable_, grid_w_, grid_h_, occ_, g))
        order_unit_move(spy_idx, g.x, g.y);
}

void World::step_capture(double dt) {
    (void)dt;
    // FIREWALL (spec §8): until the first capture/repair/infiltrate order ever,
    // this whole pass is one branch -- the only cost every existing demo pays.
    if (!capture_any_) return;
    for (std::size_t i = 0; i < units_.size(); ++i) {
        Unit& u = units_[i];
        // --- Engineer capture/repair arrival [RE: capture.md §A5] ---
        if (u.capture_target >= 0) {
            if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) {
                u.capture_target = -1;
                continue;
            }
            const int bi = u.capture_target;
            if (bi < 0 || bi >= static_cast<int>(buildings_.size())) {
                u.capture_target = -1;
                continue;
            }
            Building& b = buildings_[bi];
            if (!b.alive || b.state == Building::State::Dead) {     // target gone: cancel
                u.capture_target = -1;                             // cleanly (no crash)
                continue;
            }
            // Chebyshev adjacency to the NEAREST foundation cell -- the transport
            // boarding-trigger analogue (step_cargo: cheb <= 1).
            const auto cells = foundation_cells(b.cx, b.cy, b.fw, b.fh);
            int best = -1;
            for (const Cellxy& cell : cells) {
                const int dx = u.cell_x - cell.x, dy = u.cell_y - cell.y;
                const int cheb = std::max(dx < 0 ? -dx : dx, dy < 0 ? -dy : dy);
                if (best < 0 || cheb < best) best = cheb;
            }
            if (best >= 0 && best <= 1) {
                const int eng_house = u.house;
                bool consumed = false;
                if (b.house != eng_house) {
                    // ProduceCashStartup for the captured type (CAOILD=1000; 0 for
                    // normal buildings) [RE: capture.md §A3/§D1]. capture_building
                    // only grants it when captured FROM neutral, so this is inert
                    // for enemy captures.
                    const int startup = (b.type >= 0 && b.type < (int)types_.size())
                                            ? types_[b.type].produce_cash_startup : 0;
                    consumed = capture_building(bi, eng_house, startup);         // enemy/neutral
                }
                else
                    consumed = repair_building(bi, eng_house);                   // own damaged
                u.capture_target = -1;             // intent resolved on arrival regardless
                if (consumed) consume_unit(static_cast<int>(i));  // engineer always consumed
                // own-undamaged building -> repair_building returned false: the
                // engineer arrived, did nothing, NOT consumed (graceful fall-through).
            } else if (!u.moving()) {
                // Arrived-but-not-adjacent / path cancelled while standing: re-path
                // to a free cell adjacent to the footprint (the transport re-path
                // analogue). No free cell nearby -> keep standing, retried next frame.
                Cellxy g;
                if (engineer_approach_goal(b, u.cell_x, u.cell_y, passable_, grid_w_, grid_h_, occ_,
                                           g))
                    order_unit_move(static_cast<int>(i), g.x, g.y);
            }
            continue;   // a unit never holds both a capture and an infiltrate intent
        }
        // --- Spy infiltrate arrival [RE: capture.md §B] ---
        // Mirrors the engineer arrival exactly; only the act differs -- the
        // PRE-RESOLVED purpose/steal/blackout stored at order time drive
        // infiltrate_building, so no roster is needed here.
        if (u.infiltrate_target >= 0) {
            if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) {
                u.infiltrate_target = -1;
                continue;
            }
            const int bi = u.infiltrate_target;
            if (bi < 0 || bi >= static_cast<int>(buildings_.size())) {
                u.infiltrate_target = -1;
                continue;
            }
            Building& b = buildings_[bi];
            if (!b.alive || b.state == Building::State::Dead) {     // target gone: cancel
                u.infiltrate_target = -1;                          // cleanly (no crash)
                continue;
            }
            const auto cells = foundation_cells(b.cx, b.cy, b.fw, b.fh);
            int best = -1;
            for (const Cellxy& cell : cells) {
                const int dx = u.cell_x - cell.x, dy = u.cell_y - cell.y;
                const int cheb = std::max(dx < 0 ? -dx : dx, dy < 0 ? -dy : dy);
                if (best < 0 || cheb < best) best = cheb;
            }
            if (best >= 0 && best <= 1) {
                const int spy_house = u.house;
                const auto purpose = static_cast<InfiltratePurpose>(u.infiltrate_purpose);
                const bool acted = infiltrate_building(bi, spy_house, purpose,
                                                       u.infiltrate_steal_pct,
                                                       u.infiltrate_blackout);
                u.infiltrate_target = -1;          // intent resolved on arrival regardless
                if (acted) consume_unit(static_cast<int>(i));   // spy consumed on success
                // self-spy / no-op -> infiltrate_building returned false: the spy
                // arrived, did nothing, NOT consumed (graceful fall-through).
            } else if (!u.moving()) {
                Cellxy g;
                if (engineer_approach_goal(b, u.cell_x, u.cell_y, passable_, grid_w_, grid_h_, occ_,
                                           g))
                    order_unit_move(static_cast<int>(i), g.x, g.y);
            }
        }
    }
}

void World::transfer_superweapon(int old_house, int new_house) {
    if (old_house < 0 || old_house >= (int)sw_houses_.size()) return;
    if (new_house < 0) return;                  // lower-bound guard [Task-2 review Minor]
    if (!sw_houses_[old_house].has_sw) return;
    if ((int)sw_houses_.size() <= new_house) sw_houses_.resize(new_house + 1);
    sw_houses_[new_house] = sw_houses_[old_house];      // move the whole record
    sw_houses_[old_house] = HouseSuperweapon{};          // old house loses it
    sw_any_ = true;
}

void World::reset_superweapon(int house) {
    if (house < 0 || house >= (int)sw_houses_.size() || !sw_houses_[house].has_sw) return;
    sw_houses_[house].sw.on_fired();                     // timer->0, ready->false
}

void World::register_tech_income(int b_idx, int amount, int delay) {
    if (b_idx < 0 || amount <= 0 || delay <= 0) return;
    for (const TechIncome& t : tech_income_) if (t.building == b_idx) return;  // idempotent
    tech_income_.push_back(TechIncome{b_idx, amount, delay, 0});
    capture_any_ = true;
}

void World::set_disguise(int unit_idx, const std::string& as_type, int as_house) {
    if (unit_idx < 0 || unit_idx >= (int)units_.size()) return;
    Unit& u = units_[unit_idx];
    u.disguised = true; u.disguised_as_type = as_type; u.disguised_as_house = as_house;
    capture_any_ = true;                         // arm the detection firewall
}
void World::clear_disguise(int unit_idx) {
    if (unit_idx < 0 || unit_idx >= (int)units_.size()) return;
    units_[unit_idx].disguised = false;
}

void World::step_tech_structures(double dt) {
    if (!capture_any_) return;                 // firewall (spec §8) -- no-op elsewhere
    // Advance the logic-frame counter deterministically: one frame per dt at
    // kLogicFps (the existing 1/15 s tick). Use the same frame-quantized math
    // the SW timer uses so the gate is bit-stable across the dt accumulation.
    const int frames = (int)std::llround(dt * 15.0);
    // 1) Tech-structure income (oil derrick): every `delay` frames, credit the
    //    CURRENT owner `amount`.
    for (TechIncome& t : tech_income_) {
        if (t.building < 0 || t.building >= (int)buildings_.size()) continue;
        const Building& b = buildings_[t.building];
        if (!b.alive || b.state == Building::State::Dead) continue;
        if (b.house < 0 || b.house >= 2) continue;    // neutral pays nobody
        t.frames += frames;
        while (t.frames >= t.delay) {
            t.frames -= t.delay;
            house_credits_[b.house] += (double)t.amount;
        }
    }
    // 2) Blackout decay (index 2 inert -- never armed).
    for (int h = 0; h < 3; ++h)
        if (blackout_frames_[h] > 0) blackout_frames_[h] = std::max(0, blackout_frames_[h] - frames);
    // 3) Disguise detection [RE: capture.md §C4 CONFIRMED - DetectDisguise sees
    //    through PermaDisguise; deterministic, no RNG]. A disguised unit within
    //    kDetectDisguiseRange cells of an alive, non-dying ENEMY detector is
    //    revealed (disguise cleared). Ascending index order. NO reveal-on-attack
    //    -- the spy is PermaDisguise-exempt (§C6); a dog is the only defeat.
    for (Unit& d : units_) {
        if (!d.alive || d.die_time >= 0.0 || !d.detect_disguise) continue;
        for (Unit& s : units_) {
            if (!s.disguised || !s.alive || s.house == d.house) continue;
            const double dx = s.fx - d.fx, dy = s.fy - d.fy;
            if (dx * dx + dy * dy <= kDetectDisguiseRange * kDetectDisguiseRange)
                s.disguised = false;
        }
    }
}

void World::ai_resync_captured_building(int b_idx, int old_house, int new_house) {
    for (int h = 0; h < (int)ai_houses_.size(); ++h) {
        HouseAiRec& rec = ai_houses_[h];
        if (!rec.is_ai) continue;
        if (h == old_house) {                            // losing AI house drops it
            for (PlannedNode& n : rec.state.plan)
                if (n.building == b_idx) { n.building = -1; n.dead = true; }
        } else if (h == new_house) {                     // gaining AI house adopts it
            const Building& b = buildings_[b_idx];
            for (PlannedNode& n : rec.state.plan) {
                if (n.type == b.type && n.building < 0) {
                    n.cx = b.cx; n.cy = b.cy; n.building = b_idx; n.dead = false;
                    break;
                }
            }
        }
    }
}

int World::ai_producing(int house, int cat01) const {
    if (!ai_enabled(house) || cat01 < 0 || cat01 > 1) return -1;
    const QueueCat c = cat01 == 0 ? QueueCat::Structure : QueueCat::Defense;
    return ai_houses_[house].queues.producing(c);
}

BaseView World::make_ai_base_view(int house, const HouseAiRec& rec) const {
    BaseView v;
    bool conyard_center = false;  // a ConYard's cell locked the center
    bool any_center = false;      // some building set a provisional center
    long long tx = 0, ty = 0, tn = 0;
    std::vector<int> rival_counts;  // per-house alive-building counts (rivals)
    for (const Building& b : buildings_) {
        if (!b.alive || b.state == Building::State::Dead) continue;
        if (b.house == house) {
            BaseBuildingView bb;
            bb.type = b.type;
            bb.cx = b.cx; bb.cy = b.cy; bb.fw = b.fw; bb.fh = b.fh;
            bb.operational = b.state == Building::State::Idle;
            v.own.push_back(bb);
            // Base center = the FIRST ConYard's origin cell; if the house
            // owns none (yet), the FIRST building stands in.
            const bool is_cy = b.type >= 0 && b.type < (int)rec.cfg.roster.size() &&
                               rec.cfg.roster[b.type].conyard;
            if (is_cy && !conyard_center) {
                v.center_x = b.cx; v.center_y = b.cy;
                conyard_center = true;
            } else if (!conyard_center && !any_center) {
                v.center_x = b.cx; v.center_y = b.cy;
                any_center = true;
            }
        } else {
            tx += b.cx; ty += b.cy; ++tn;  // threat = other houses' centroid
            // rival_base_size = largest OTHER house's building count — the
            // BaseSizeAdd cap basis (AI base <= rival + 3) [RE: ai-base.md §2].
            if ((int)rival_counts.size() <= b.house) rival_counts.resize(b.house + 1, 0);
            v.rival_base_size = std::max(v.rival_base_size, ++rival_counts[b.house]);
        }
    }
    if (tn > 0) { v.threat_x = (int)(tx / tn); v.threat_y = (int)(ty / tn); }
    else { v.threat_x = grid_w_ / 2; v.threat_y = grid_h_ / 2; }
    v.credits = rec.credits;
    // Engineer/Spy Capture [RE: capture.md §B2]: a spied enemy power plant forces
    // THIS house's power OUTPUT to 0 while its blackout timer runs, so the AI's
    // own low-power view / build clamp go offline too. apply_blackout is IDENTITY
    // when !house_blacked_out(house), so every existing demo (never blacked out)
    // is byte-identical (the ai-base/ai-team/ai-trigger gates run this path).
    const PowerTotals pt =
        apply_blackout(power_totals(buildings_, rec.cfg.roster, house), house_blacked_out(house));
    v.power_output = pt.output;
    v.power_drain = pt.drain;
    for (const Unit& u : units_)
        // Carried units are off-map/invisible to census [CB-1 off-map invariant].
        if (u.alive && u.house == house && u.is_harvester && u.cargo_of < 0) ++v.harvesters;
    return v;
}

// Places queue `cat`'s READY building at its planned node's cell through the
// EXISTING validity + add_building path [RE: ai-base.md §3/§6]. Returns true
// if the slot was consumed (placed); false = HOLD (retry next frame).
bool World::ai_place_ready(int house, HouseAiRec& rec, QueueCat cat) {
    const int q = cat == QueueCat::Structure ? 0 : 1;
    if (!rec.queues.ready(cat)) return false;
    // pending[q] is the node's STABLE id, not a raw index: resolve it by
    // scanning the plan so a power-preemption insert that shifted vector
    // indices since production started can never place the wrong node
    // [fix wave 1]. Id no longer present (node legitimately removed) -> the
    // existing orphaned-slot path.
    const int pid = rec.state.pending[q];
    int node_i = -1;
    if (pid >= 0)
        for (int i = 0; i < (int)rec.state.plan.size(); ++i)
            if (rec.state.plan[i].id == pid) { node_i = i; break; }
    if (node_i < 0) {
        rec.queues.placed(cat);  // defensive: orphaned ready slot
        rec.state.pending[q] = -1;
        return true;
    }
    PlannedNode& node = rec.state.plan[node_i];
    const BuildingTypeDef& t = rec.cfg.roster[node.type];
    auto can_place = [&](int cx, int cy) {
        // AI validity [RE-GATE APPLIED: ai-base.md §3]: the AI BYPASSES the
        // Adjacent= proximity check (FUN_004a8dc0 early-returns for
        // non-local-human houses) -> bypass_adjacency=true; the shared
        // per-cell buildable/unoccupied grid still applies, probed over the
        // foundation expanded by the AIBaseSpacing(1) gap [RE-FALLBACK: gap
        // modeled as an expanded-foundation probe].
        const int s = rec.cfg.ai.base_spacing;
        return placement_check(cx - s, cy - s, t.fw + 2 * s, t.fh + 2 * s,
                               house, t.adjacent, passable_,
                               grid_w_, grid_h_, occ_, buildings_,
                               /*bypass_adjacency=*/true).ok;
    };
    int px = node.cx, py = node.cy;
    if (!can_place(px, py)) {
        // Something moved onto the node's cell: re-scan (deterministic).
        const BaseView v = make_ai_base_view(house, rec);
        const bool is_def = std::find(rec.cfg.catalog.defenses.begin(),
                                      rec.cfg.catalog.defenses.end(),
                                      node.type) != rec.cfg.catalog.defenses.end();
        const Cellxy alt = find_build_location(v, is_def, can_place);
        if (alt.x < 0) return false;  // boxed in -> HOLD the ready slot
        px = alt.x; py = alt.y;
        node.cx = px; node.cy = py;
    }
    const double buildup = node.type < (int)rec.cfg.buildup_totals.size()
                               ? rec.cfg.buildup_totals[node.type] : 0.0;
    const int idx = add_building(node.type, px, py, t.fw, t.fh, house, t.strength,
                                 t.armor_ix, buildup, /*instant=*/false);
    node.building = idx;
    node.dead = false;
    rec.queues.placed(cat);
    rec.state.pending[q] = -1;
    return true;
}

void World::step_ai_houses(double dt) {
    for (int h = 0; h < (int)ai_houses_.size(); ++h) {
        HouseAiRec& rec = ai_houses_[h];
        if (!rec.is_ai) continue;
        const AiHouseConfig& cfg = rec.cfg;

        // 0) Lazy base-plan build + STANDING-BUILDING SYNC [RE: ai-base.md §3 —
        //    FUN_0050a3e0 builds the node vector iff empty, THEN walks the
        //    house's already-placed buildings and records each one's cell (and
        //    World index) into its matching plan node. Build + sync are ONE
        //    step, mirroring the original. Without the sync a node covering a
        //    PRE-BUILT building (spawn_base / the seeded demo ConYard) stays
        //    cx=cy=0/building=-1, so its destruction is never death-detected
        //    (step 2) and the AI rebuilds it as a FRESH nearest-ring node
        //    instead of IN PLACE at the retained cell — breaking §3/§5 for the
        //    pre-built enemy base. (next_build's own lazy build then no-ops
        //    here: the plan is already non-empty; the pure ai_base unit tests
        //    still exercise that lazy path directly.)
        if (rec.state.plan.empty()) {
            const BaseView v0 = make_ai_base_view(h, rec);
            rec.state.plan = build_base_plan(v0, cfg.catalog, cfg.roster, cfg.ai, cfg.mods);
            // Continue the stable-id counter past the ids build_base_plan
            // assigned (identical to next_build's own sync).
            rec.state.next_node_id = 0;
            for (const PlannedNode& n : rec.state.plan)
                if (n.id + 1 > rec.state.next_node_id) rec.state.next_node_id = n.id + 1;
            // Match each STANDING building (deterministic iteration: ascending
            // World building index) to the FIRST UNCLAIMED plan node of the
            // SAME type; store the building's cell + index and link it — the
            // same linkage ai_place_ready makes for a freshly-placed building.
            // building < 0 == unclaimed, so one building claims one node and a
            // node is claimed at most once (multiplicities stay exact). Plan
            // ORDER / topo sort are untouched.
            for (int bi = 0; bi < (int)buildings_.size(); ++bi) {
                const Building& b = buildings_[bi];
                if (!b.alive || b.state == Building::State::Dead || b.house != h ||
                    b.type < 0)
                    continue;
                for (PlannedNode& n : rec.state.plan) {
                    if (n.type == b.type && n.building < 0) {
                        n.cx = b.cx;
                        n.cy = b.cy;
                        n.building = bi;
                        n.dead = false;
                        break;
                    }
                }
            }
        }

        // 1) Income (the economy phase accumulates per-house unload income).
        rec.credits += take_income(h);

        // 2) Node-death detection [RE: ai-base.md §5].
        for (PlannedNode& n : rec.state.plan) {
            if (n.building < 0) continue;
            const Building& b = buildings_[n.building];
            if (!b.alive || b.state == Building::State::Dead) {
                n.building = -1;
                n.dead = true;
            }
        }

        // 3) ConYard-lost cancels both building queues (the same rule the
        //    player's prod_tick applies) [RE: ai-base.md §6].
        bool conyard = false;
        for (const Building& b : buildings_) {
            if (b.alive && b.state == Building::State::Idle && b.house == h &&
                b.type >= 0 && b.type < (int)cfg.roster.size() &&
                cfg.roster[b.type].conyard) { conyard = true; break; }
        }
        if (!conyard) {
            for (QueueCat c : {QueueCat::Structure, QueueCat::Defense}) {
                const int q = c == QueueCat::Structure ? 0 : 1;
                if (rec.queues.producing(c) != -1) rec.queues.cancel(c);
                // Clear the pending node id too [fix wave — MINOR-2]:
                // ai_place_ready can't (ready() is false after a cancel), so an
                // un-cleared pending[q] would leave the queue permanently gated
                // by `pending[q]!=-1` (step 8) — a latent AI-2 wedge (an MCV
                // redeploy could never restart the queue).
                rec.state.pending[q] = -1;
            }
        }

        // 4) Tick the queues: SAME power clamp as the player [RE: ai-base.md
        //    §6], difficulty time handicap applied at start() (below).
        //    Engineer/Spy Capture [RE: capture.md §B2]: a spied enemy power plant
        //    forces this AI house's power OUTPUT to 0 while its blackout timer
        //    runs -> the same low-power build clamp the player pays. Identity when
        //    !house_blacked_out(h), so ai-base/ai-team/ai-trigger stay exact.
        const PowerTotals pt =
            apply_blackout(power_totals(buildings_, cfg.roster, h), house_blacked_out(h));
        const double pmul = build_speed_multiplier(pt, cfg.low_power_penalty);
        rec.queues.set_credits(rec.credits);
        const double sp[4] = {1.0, 1.0, 1.0, 1.0};  // one ConYard in AI-1 — no
                                                    // MultipleFactory speedup
        rec.queues.tick(dt, pmul, sp, cfg.roster);
        rec.credits = rec.queues.credits();

        // 5) Place READY slots (Structure then Defense — deterministic order).
        ai_place_ready(h, rec, QueueCat::Structure);
        ai_place_ready(h, rec, QueueCat::Defense);

        // 6) Build clock [RE-GATE APPLIED: ai-base.md §1 — two timers stack
        //    in the original: the AI tick SELF-RESCHEDULES 0x69 + rand(1,7)
        //    = 105–112 frames (~7 s at 15 Hz), and the per-house BuildDelay
        //    timer is seeded from the difficulty BuildDelay MINUTES
        //    [RE-FALLBACK: secs = min x 60]. The jitter draw uses the
        //    engine-local xorshift32 [RE-FALLBACK: ai-base.md deviations —
        //    stands in for the lockstep game RNG; NEVER wall-clock].
        rec.state.build_clock += dt;
        if (rec.state.build_clock < rec.state.build_gate) continue;
        rec.state.build_clock = 0.0;
        rec.state.build_gate = std::max(
            (double)(0x69 + 1 + (int)(ai_rand(rec.state.rng) % 7)) / 15.0,
            cfg.mods.build_delay_min * 60.0);
        if (!conyard) continue;  // no factory: nothing can start

        // 7) Decide.
        const BaseView view = make_ai_base_view(h, rec);
        const BuildDecision d =
            next_build(rec.state, view, cfg.catalog, cfg.roster, cfg.ai, cfg.mods);
        if (d.type < 0) continue;  // nothing needed, or wait/bank

        // 8) Route to the queue the player would use [RE: ai-base.md §6]:
        //    defenses -> Defense queue, everything else -> Structure queue.
        const bool is_def = std::find(cfg.catalog.defenses.begin(),
                                      cfg.catalog.defenses.end(),
                                      d.type) != cfg.catalog.defenses.end();
        const QueueCat cat = is_def ? QueueCat::Defense : QueueCat::Structure;
        const int q = is_def ? 1 : 0;
        if (rec.queues.producing(cat) != -1 || rec.state.pending[q] != -1)
            continue;  // slot busy — the single-slot queue IS the recovered
                       // one-AI-structure-at-a-time gate (house+0x564c stays
                       // set until placement) [RE-GATE APPLIED: ai-base.md §1]

        // 9) Same prereq/tech/factory gate as the player (can_start —
        //    EXISTING function) [RE: ai-base.md §6].
        if (!can_start(rec.queues.state(cat), d.type, cfg.roster, buildings_, h,
                       cfg.groups, conyard))
            continue;

        // 10) The decision names its plan node [RE-GATE APPLIED: ai-base.md
        //     §3/§5 — every start fills a node of the ordered plan; next_build
        //     already inserted/appended override nodes]. A REBUILD node keeps
        //     its stored cell (re-validated by ai_place_ready); a FRESH node
        //     ((0,0) = cell not yet chosen, the original's node+4 == 0) gets
        //     its cell from the deterministic scan NOW and stores it.
        const int node_i = d.node;
        PlannedNode& pn = rec.state.plan[node_i];
        if (!pn.dead && pn.cx == 0 && pn.cy == 0) {
            const BuildingTypeDef& t = cfg.roster[d.type];
            auto can_place = [&](int cx, int cy) {
                // Same AI validity as ai_place_ready: Adjacent= BYPASSED +
                // AIBaseSpacing(1) gap [RE-GATE APPLIED: ai-base.md §3].
                const int s = cfg.ai.base_spacing;
                return placement_check(cx - s, cy - s, t.fw + 2 * s, t.fh + 2 * s,
                                       h, t.adjacent, passable_,
                                       grid_w_, grid_h_, occ_, buildings_,
                                       /*bypass_adjacency=*/true).ok;
            };
            const Cellxy loc = find_build_location(view, is_def, can_place);
            if (loc.x < 0) continue;  // boxed in: try again next clock
            pn.cx = loc.x; pn.cy = loc.y;
        }

        // 11) START on the EXISTING queue economy: difficulty cost handicap
        //     on the charge, BuildTime handicap on the speed (Easy 0.8 =
        //     faster; Normal/Difficult 1.0) [RE: ai-base.md §4].
        //     (cfg.mods.build_slowdown is recorded but not consumed in AI-1
        //     — it lands with the unit-production AI sub-phase.)
        const BuildingTypeDef& t = cfg.roster[d.type];
        const int basis = t.charge > 0 ? t.charge : t.cost;
        const int ai_charge =
            std::max(1, (int)(basis * cfg.mods.cost_mul));
        rec.queues.set_credits(rec.credits);
        start(rec.queues.state(cat), t.cost, ai_charge,
              cfg.build_speed * cfg.mods.build_time_mul);
        rec.queues.state(cat).slot.type = d.type;
        rec.credits = rec.queues.credits();
        // Store the node's STABLE id (resolved back to an index in
        // ai_place_ready), NOT the raw vector index — a later preemption
        // insert can shift this node without corrupting the pending slot
        // [fix wave 1].
        rec.state.pending[q] = rec.state.plan[node_i].id;
    }
}

// --- AI-2 offensive-teams phase [RE: ai-team.md] ----------------------------

namespace {
// Quarry-kind -> unit-class match [RE-GATE APPLIED: ai-team.md §5 — the standard
// RA2 enumeration; class values from ai_team.hpp's kQuarry* constants].
bool quarry_unit_match(int quarry, const Unit& u) {
    switch (quarry) {
        case kQuarryHarvesters:  return u.is_harvester;
        case kQuarryInfantry:    return !u.is_vehicle && !u.is_harvester;
        case kQuarryVehicles:    return u.is_vehicle;
        case kQuarryBaseThreats: return true;   // nearest hostile [RE-FALLBACK]
        case kQuarryAnything:    return true;
        default:                 return false;
    }
}
// Kinds that resolve to buildings. 5 (factories) and 6 (base defenses) resolve
// as ANY building until the class split is mapped [RE-FALLBACK: ai-team.md §5].
bool quarry_wants_buildings(int quarry) {
    return quarry == kQuarryAnything || quarry == kQuarryBuildings ||
           quarry == kQuarryFactories || quarry == kQuarryDefenses;
}
}  // namespace

const AiTeamState* World::ai_team_state(int house) const {
    return ai_enabled(house) ? &ai_houses_[house].team_state : nullptr;
}

int World::ai_unit_producing(int house, int iv01) const {
    if (!ai_enabled(house) || iv01 < 0 || iv01 > 1) return -1;
    const QueueCat c = iv01 == 0 ? QueueCat::Infantry : QueueCat::Vehicle;
    return ai_houses_[house].queues.producing(c);
}

int World::ai_unit_type_of(const HouseAiRec& rec, const Unit& u) const {
    // Units carry no roster index — map via the resolved combat image
    // (unique per producible type; deterministic).
    const std::vector<UnitTypeDef>& r = rec.cfg.team.unit_roster;
    for (int i = 0; i < (int)r.size(); ++i)
        if (r[i].combat.image == u.image) return i;
    return -1;
}

std::vector<IdleUnitView> World::ai_idle_units(int house, const HouseAiRec& rec) const {
    // Recruitable = own, alive, non-dying, NOT a harvester, at rest, no
    // attack pursuit, typed in the unit roster, and not already claimed by
    // any team. ASCENDING unit index — the deterministic claim order
    // [RE: ai-team.md §2].
    std::vector<IdleUnitView> out;
    for (int i = 0; i < (int)units_.size(); ++i) {
        const Unit& u = units_[i];
        if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0 || u.house != house ||
            u.is_harvester || u.is_aircraft)  // CB-2: AI never fields/recruits aircraft
            continue;
        if (u.moving() || u.attack_order) continue;
        const int t = ai_unit_type_of(rec, u);
        if (t < 0) continue;
        bool in_team = false;
        for (const TeamInstance& tm : rec.team_state.teams) {
            for (int m : tm.members)
                if (m == i) { in_team = true; break; }
            if (in_team) break;
        }
        if (!in_team) out.push_back(IdleUnitView{i, t});
    }
    return out;
}

bool World::ai_quarry_exists(int house, int quarry) const {
    for (const Unit& u : units_)
        if (u.alive && u.die_time < 0.0 && u.cargo_of < 0 && !u.is_aircraft &&
            u.house != house && quarry_unit_match(quarry, u))  // CB-2: aircraft never a quarry
            return true;
    if (quarry_wants_buildings(quarry))
        for (const Building& b : buildings_)
            if (b.alive && b.state != Building::State::Dead && b.house != house)
                return true;
    return false;
}

int World::ai_pick_quarry_unit(int house, int quarry, int nx, int ny,
                               std::uint32_t& rng) const {
    // Class-filter + distance^2 x value x seeded-RNG jitter, keep the minimum
    // (nearest-and-most-valuable, with jitter); tie -> lowest index
    // [RE-GATE APPLIED: ai-team.md §5]. `value` is a per-target term (proxy 1
    // pre-map [RE-FALLBACK]); the jitter draws from ai_rand over `rng`.
    int best = -1;
    long long bestd = 0;
    for (int i = 0; i < (int)units_.size(); ++i) {
        const Unit& u = units_[i];
        if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0 || u.is_aircraft ||
            u.house == house)
            continue;  // CB-2: aircraft never a quarry
        if (!quarry_unit_match(quarry, u)) continue;
        const long long dx = u.cell_x - nx, dy = u.cell_y - ny;
        const long long value = 1;                      // [RE-FALLBACK] proxy
        const long long jitter = 90 + (ai_rand(rng) % 21);  // 0.90x .. 1.10x, x100
        const long long score = (dx * dx + dy * dy) * value * jitter;
        if (best < 0 || score < bestd) { best = i; bestd = score; }
    }
    return best;
}

int World::ai_pick_quarry_building(int house, int quarry, int nx, int ny,
                                   std::uint32_t& rng) const {
    if (!quarry_wants_buildings(quarry)) return -1;
    int best = -1;
    long long bestd = 0;
    for (int i = 0; i < (int)buildings_.size(); ++i) {
        const Building& b = buildings_[i];
        if (!b.alive || b.state == Building::State::Dead || b.house == house) continue;
        const long long dx = b.cx - nx, dy = b.cy - ny;
        const long long value = 1;                      // [RE-FALLBACK] proxy
        const long long jitter = 90 + (ai_rand(rng) % 21);
        const long long score = (dx * dx + dy * dy) * value * jitter;
        if (best < 0 || score < bestd) { best = i; bestd = score; }
    }
    return best;
}

void World::commit_attack_target(int unit_idx, int target_unit, int target_building) {
    // The identical mutation tail shared by the player selection-loops
    // (order_attack/order_attack_building) AND the AI per-unit entry points —
    // extracted so there is ONE copy of the target-commit body [RE-GATE APPLIED:
    // ai-team.md §5]. target_unit / target_building are mutually exclusive
    // (pass -1 for the unused one). Callers keep their OWN guards.
    Unit& u = units_[unit_idx];
    u.target = target_unit;
    u.target_building = target_building;
    u.attack_order = true;
    // CB-1: an attack order supersedes a pending enter/unload [RE-GATE
    // APPLIED: transport-cargo.md §3/§4 -- kept: our order-model
    // convention; the hunt pinned no contrary rule].
    u.enter_target = -1;
    u.unload_pending = false;
    // CB-2 [airborne-no-cell invariant]: only re-claim a ground cell when the
    // unit is NOT airborne. An airborne aircraft released its cell at takeoff
    // and nothing later re-releases it, so re-asserting occupancy here would
    // permanently strand its ground-projection cell. A ground unit
    // (!is_aircraft) still takes the release+set exactly as before.
    if (!u.is_aircraft || u.flight_state == FlightState::Grounded) {
        occ_.release_all_of(unit_idx, u.cell_x, u.cell_y);
        occ_.set(u.cell_x, u.cell_y, unit_idx);
    }
    u.path.clear();
    u.path_idx = 0;
}

void World::ai_order_attack(int unit_idx, int target_idx) {
    // Per-unit entry for AI-owned units: the existing order_attack drives
    // SELECTED house-0 units only; teams need per-unit orders for ANY house.
    // ADDITIVE — the player path is untouched (the six gates verify). Keeps its
    // OWN guard, then calls the shared commit_attack_target [RE: ai-team.md §5].
    if (unit_idx < 0 || unit_idx >= (int)units_.size()) return;
    if (target_idx < 0 || target_idx >= (int)units_.size()) return;
    const Unit& u = units_[unit_idx];
    const Unit& tgt = units_[target_idx];
    if (!u.alive || u.die_time >= 0.0) return;
    if (!tgt.alive || tgt.die_time >= 0.0 || tgt.house == u.house) return;  // AI guard
    commit_attack_target(unit_idx, target_idx, /*target_building=*/-1);
}

void World::ai_order_attack_building(int unit_idx, int b_idx) {
    // Per-unit entry (same rationale); own guard + shared commit_attack_target.
    if (unit_idx < 0 || unit_idx >= (int)units_.size()) return;
    if (b_idx < 0 || b_idx >= (int)buildings_.size()) return;
    const Unit& u = units_[unit_idx];
    const Building& tgt = buildings_[b_idx];
    if (!u.alive || u.die_time >= 0.0) return;
    if (!tgt.alive || tgt.state == Building::State::Dead || tgt.house == u.house) return;
    commit_attack_target(unit_idx, /*target_unit=*/-1, b_idx);
}

void World::ai_fill_team_view(int house, const HouseAiRec& rec,
                              const TeamInstance& team, TeamView& v) const {
    v.members.clear();
    for (int m : team.members) {
        TeamMemberView mv;
        mv.unit = m;
        if (m >= 0 && m < (int)units_.size()) {
            const Unit& u = units_[m];
            mv.alive = u.alive && u.die_time < 0.0;
            mv.cx = u.cell_x;
            mv.cy = u.cell_y;
            mv.idle = !u.moving() &&
                      !(u.attack_order && (u.target >= 0 || u.target_building >= 0));
        }
        v.members.push_back(mv);
    }
    v.waypoints = rec.cfg.team.waypoints;
    // Base center + enemy centroid reuse the AI-1 view (ConYard-first center,
    // rival-buildings threat centroid) — ONE source of truth.
    const BaseView bv = make_ai_base_view(house, rec);
    v.base_x = bv.center_x;
    v.base_y = bv.center_y;
    v.enemy_x = bv.threat_x;
    v.enemy_y = bv.threat_y;
    // quarry_exists is computed for the CURRENT attack action's EFFECTIVE quarry
    // class — the completion predicate the interpreter consults. It must match
    // the quarry class step_team sets on the emitted command [RE: ai-team.md
    // §3/§5]: AttackQuarry -> argument; AttackBuilding -> Buildings;
    // AttackHouse/AttackUnits -> Anything. Recomputed PER TICK (contract B).
    v.quarry_exists = false;
    const AimdData& aimd = rec.cfg.team.aimd;
    if (team.state == TeamState::Executing && team.teamtype >= 0 &&
        team.teamtype < (int)aimd.teams.size()) {
        const ScriptType& sc = aimd.scripts[aimd.teams[team.teamtype].script];
        if (team.script_pc >= 0 && team.script_pc < (int)sc.actions.size()) {
            const ScriptAction a = sc.actions[team.script_pc];
            const OpClass oc = classify_op(a.action);
            int q = -1;
            if (oc == OpClass::AttackQuarry)        q = a.argument;
            else if (oc == OpClass::AttackBuilding) q = kQuarryBuildings;
            else if (oc == OpClass::AttackHouse ||
                     oc == OpClass::AttackUnits)    q = kQuarryAnything;
            if (q >= 0) v.quarry_exists = ai_quarry_exists(house, q);
        }
    }
}

void World::ai_execute_team_command(int house, HouseAiRec& rec, TeamInstance& team,
                                    const TeamCommand& c) {
    switch (c.kind) {
        case TeamCommand::Kind::MoveTeam: {
            // Deterministic per-member spread (ring offsets) so members do not
            // stack one cell; scatter widens the ring [RE-FALLBACK: engine
            // formation stand-in; order_unit_move cancels cleanly if a spread
            // cell is unreachable].
            // CONTRACT A [ai-team.md §2]: the pure engine hands an IDEAL cell;
            // snap it to the nearest FREE (walkable+unoccupied) cell so members
            // can actually satisfy the Stray(2.0)/CloseEnough(2.25) predicate,
            // then RECORD the snapped cell back as the team's REALIZED move focus
            // (team.gather_x/gather_y) for ALL MoveTeam commands — gather AND
            // Executing move opcodes alike. step_team measures arrival against
            // that realized focus, so a plain UNBOUNDED nearest-free snap is now
            // correct for both (the previous CloseEnough-bounded Executing snap
            // was the fragile half of the seam — it is gone; matches the
            // original's snapped-Focus semantics FUN_006eb030/FUN_0056d750).
            auto free_cell = [this](int x, int y) {
                return walkable(x, y) && occ_.is_free(x, y);
            };
            auto nearest_free = [&](int cx, int cy, int maxr) -> Cellxy {
                for (int r = 0; r <= maxr; ++r)
                    for (int oy = -r; oy <= r; ++oy)
                        for (int ox = -r; ox <= r; ++ox) {
                            if (std::max(std::abs(ox), std::abs(oy)) != r) continue;
                            const int x = cx + ox, y = cy + oy;
                            if (free_cell(x, y)) return Cellxy{x, y};
                        }
                return Cellxy{-1, -1};
            };
            int gx = c.x, gy = c.y;
            const Cellxy s = nearest_free(c.x, c.y, 8);
            if (s.x >= 0) { gx = s.x; gy = s.y; }
            team.gather_x = gx;   // realized move focus (contract A)
            team.gather_y = gy;
            static const int dx[9] = {0, 1, -1, 0, 0, 1, -1, 1, -1};
            static const int dy[9] = {0, 0, 0, 1, -1, 1, -1, -1, 1};
            const int spread = c.scatter ? 3 : 1;
            for (std::size_t k = 0; k < team.members.size(); ++k) {
                const int m = team.members[k];
                if (m < 0 || m >= (int)units_.size()) continue;
                const Unit& u = units_[m];
                if (!u.alive || u.die_time >= 0.0) continue;
                order_unit_move(m, gx + dx[k % 9] * spread, gy + dy[k % 9] * spread);
            }
            break;
        }
        case TeamCommand::Kind::AttackQuarry: {
            for (int m : team.members) {
                if (m < 0 || m >= (int)units_.size()) continue;
                Unit& u = units_[m];
                if (!u.alive || u.die_time >= 0.0) continue;
                if (u.attack_order && (u.target >= 0 || u.target_building >= 0))
                    continue;  // already pursuing — no target thrash
                const int tu = ai_pick_quarry_unit(house, c.quarry, u.cell_x,
                                                   u.cell_y, rec.team_state.rng);
                if (tu >= 0) { ai_order_attack(m, tu); continue; }
                const int tb = ai_pick_quarry_building(house, c.quarry, u.cell_x,
                                                       u.cell_y, rec.team_state.rng);
                if (tb >= 0) ai_order_attack_building(m, tb);
            }
            break;
        }
        case TeamCommand::Kind::GuardArea: {
            // Guard anchors only — the EXISTING step_combat auto-acquire and
            // return-to-anchor machinery does the rest [RE: ai-team.md §3].
            for (int m : team.members) {
                if (m < 0 || m >= (int)units_.size()) continue;
                Unit& u = units_[m];
                if (!u.alive || u.die_time >= 0.0) continue;
                u.guard_x = c.x;
                u.guard_y = c.y;
            }
            break;
        }
        case TeamCommand::Kind::Inert:  // logged skip — pc already advanced
        case TeamCommand::Kind::Wait:
        case TeamCommand::Kind::None:
        default:
            break;
    }
}

bool World::ai_launch_unit_ready(int house, HouseAiRec& rec, QueueCat cat) {
    // Exit handoff via the EXISTING primary_factory + launch_unit path (the
    // player's run_interactive twin) [RE: ai-team.md §6]. Returns true if the
    // ready slot was consumed.
    if (!rec.queues.ready(cat)) return false;
    const AiTeamConfig& tc = rec.cfg.team;
    const std::vector<int>& ft =
        cat == QueueCat::Infantry ? tc.inf_factory_types : tc.veh_factory_types;
    const int fi = primary_factory(buildings_, ft, house);
    if (fi < 0) return false;  // no factory: the lost-check cancels the queue
    const int uslot = rec.queues.producing(cat);
    if (uslot < 0 || uslot >= (int)tc.unit_roster.size()) {
        rec.queues.placed(cat);  // defensive: orphaned ready slot
        return true;
    }
    const BuildingTypeDef& fdef = rec.cfg.roster[buildings_[fi].type];
    const int uidx = launch_unit(fi, fdef, tc.unit_roster[uslot].combat);
    if (uidx < 0) return false;  // exit blocked -> HOLD ready (retry next frame)
    // Claim into the FIRST (oldest) Recruiting team owing this type —
    // deterministic; an unclaimed unit stays idle and recruitable
    // [RE: ai-team.md §2].
    for (TeamInstance& t : rec.team_state.teams) {
        if (t.state != TeamState::Recruiting) continue;
        if (team_shortfall_type(t, tc.aimd) == uslot) {
            t.members.push_back(uidx);
            t.member_types.push_back(uslot);
            break;
        }
    }
    rec.queues.placed(cat);
    return true;
}

void World::step_ai_teams(double dt) {
    for (int h = 0; h < (int)ai_houses_.size(); ++h) {
        HouseAiRec& rec = ai_houses_[h];
        if (!rec.is_ai || !rec.cfg.team.enabled) continue;  // FIREWALL (spec §5)
        const AiTeamConfig& tc = rec.cfg.team;
        AiTeamState& st = rec.team_state;

        // 1) Member-death sweep [RE: ai-team.md §2]: prune dead/dying members
        //    (parallel vectors stay parallel); the pure lifecycle then sees
        //    the reopened slot (full=yes teams drop back to Recruiting).
        for (TeamInstance& t : st.teams) {
            for (int k = (int)t.members.size() - 1; k >= 0; --k) {
                const int m = t.members[k];
                if (m < 0 || m >= (int)units_.size() || !units_[m].alive ||
                    units_[m].die_time >= 0.0) {
                    t.members.erase(t.members.begin() + k);
                    t.member_types.erase(t.member_types.begin() + k);
                }
            }
        }

        // 2) Factory-lost cancel for the unit queues (the queues themselves
        //    already TICKED inside step_ai_houses's rec.queues.tick — no new
        //    production code) [RE: ai-team.md §6].
        if (rec.queues.producing(QueueCat::Infantry) != -1 &&
            factory_category_lost(tc.inf_factory_types, h))
            rec.queues.cancel(QueueCat::Infantry);
        if (rec.queues.producing(QueueCat::Vehicle) != -1 &&
            factory_category_lost(tc.veh_factory_types, h))
            rec.queues.cancel(QueueCat::Vehicle);
        rec.credits = rec.queues.credits();

        // 3) Launch READY unit slots (Infantry then Vehicle — deterministic).
        ai_launch_unit_ready(h, rec, QueueCat::Infantry);
        ai_launch_unit_ready(h, rec, QueueCat::Vehicle);

        // 4) Two-clock autocreate [RE-GATE APPLIED: ai-team.md §4]: TeamDelays
        //    (frames, per-difficulty) gates the management CHECK pass; within a
        //    check pass, AutocreateTime (minutes) gates the actual SPAWN; the
        //    TotalAITeamCap hard cap is enforced inside select_autocreate_team.
        //    At most ONE new team per spawn, highest priority first, deterministic.
        st.check_clock += dt;
        const double check_period = (double)tc.rules.team_delay_frames / 15.0;
        if (st.check_clock >= check_period) {
            st.check_clock = 0.0;
            st.spawn_clock += check_period;
            const double spawn_period = tc.rules.autocreate_time_min * 60.0;  // min->s
            if (st.spawn_clock >= spawn_period) {
                st.spawn_clock = 0.0;
                // CONTRACT F: select_autocreate_team takes a trailing seeded rng
                // (the house's AiTeamState::rng — deterministic, never wall-clock).
                const int pick = select_autocreate_team(tc.aimd, tc.house_name,
                                                        st.teams, rec.credits,
                                                        tc.rules, st.rng);
                if (pick >= 0) {
                    TeamInstance t;
                    t.teamtype = pick;
                    st.teams.push_back(t);
                    ++st.teams_created;
                }
            }
            // AI-3 trigger-eval step [RE-GATE APPLIED: ai-trigger.md §5 —
            // CONFIRMED: trigger eval lives INSIDE this team-management
            // pass; no separate AITrigger clock exists]. A SECOND creation
            // path BESIDE autocreate: both feed st.teams and reuse the whole
            // AI-2 recruit/produce/gather/execute machinery (lifecycle
            // transitions UNCHANGED) [spec §4]. BOTH sources respect
            // TotalAITeamCap. NO team->trigger link is stored — attribution
            // at disband is a Team1-identity scan [RE-GATE APPLIED:
            // ai-trigger.md §6]. FIREWALL: triggers_enabled defaults false,
            // so no existing gate ever reaches this block; the draw consumes
            // trigger_state.rng (a separate stream), never st.rng.
            if (tc.triggers_enabled && !tc.aimd.triggers.empty() &&
                (int)st.teams.size() < tc.rules.total_team_cap) {
                TriggerView tv;
                ai_fill_trigger_view(h, rec, tv);
                const TriggerDecision d =
                    select_trigger(tc.aimd, rec.trigger_state.weights, st.teams,
                                   tv, rec.trigger_state.rng);
                if (d.teamtype1 >= 0) {
                    // BOTH Team1 AND Team2 are created for the OWNING house
                    // [RE-GATE APPLIED: ai-trigger.md §3 — CONFIRMED; Team2 is
                    // NOT a reciprocal and may equal Team1 -> two instances].
                    // Each create still respects TotalAITeamCap + the
                    // per-TeamType Max (select_trigger pre-checked both; the
                    // Team2 push re-checks against the just-pushed Team1).
                    TeamInstance t1;
                    t1.teamtype = d.teamtype1;  // AI-2's EXACT create path: the
                    st.teams.push_back(t1);     // team enters st.teams and
                    ++st.teams_created;         // lives the whole AI-2 lifecycle
                    if (d.teamtype2 >= 0 &&
                        (int)st.teams.size() < tc.rules.total_team_cap) {
                        const int max2 = tc.aimd.teams[d.teamtype2].max;
                        int cnt2 = 0;
                        for (const TeamInstance& ti : st.teams)
                            if (ti.teamtype == d.teamtype2) ++cnt2;
                        if (max2 < 0 || cnt2 < max2) {   // Max<0 = uncapped
                            TeamInstance t2;
                            t2.teamtype = d.teamtype2;
                            st.teams.push_back(t2);
                            ++st.teams_created;
                        }
                    }
                    ++rec.trigger_state.triggers_fired;
                    rec.trigger_state.last_fired = d.trigger;
                }
            }
        }

        // 5) Recruit-idle first [RE: ai-team.md §2] (re-scanned per team so a
        //    unit claimed by an earlier team is not offered to a later one),
        //    then ONE production intent per house tick through the EXISTING
        //    queues — tab2 (Infantry) / tab3 (Vehicle) routing [RE: §6].
        //    ai_idle_units returns ASCENDING unit-key order (contract D).
        for (TeamInstance& t : st.teams) {
            if (t.state != TeamState::Recruiting) continue;
            recruit_idle(t, tc.aimd, ai_idle_units(h, rec));
        }
        const int want = next_produce_type(st.teams, tc.aimd, rec.credits,
                                           rec.cfg.mods.cost_mul, tc.unit_roster,
                                           tc.rules);
        if (want >= 0) {
            const bool veh = tc.unit_roster[want].is_vehicle;
            const QueueCat cat = veh ? QueueCat::Vehicle : QueueCat::Infantry;
            const std::vector<int>& ft = veh ? tc.veh_factory_types : tc.inf_factory_types;
            const bool factory = !factory_category_lost(ft, h);
            // BuildSlowdown OVER-QUEUE damp [RE-GATE APPLIED: ai-team.md §6]: on
            // Normal/Difficult, do NOT start another of a category the house
            // already has on-hand (contract E — YOU compute the flag). The wanted
            // type's queue is already guarded (producing == -1); the damp blocks
            // additionally if a unit of the wanted type is already standing
            // on-hand (launched, not yet team-consumed). BuildSlowdown is a
            // difficulty BYTE, NOT a build-time multiplier — no speed penalty.
            // "On-hand" = launched but NOT yet team-consumed. Exclude units
            // already claimed into a team (the SAME in_team filter ai_idle_units
            // uses) — otherwise the damp keys off OTHER teams' living members and
            // idles house production well beyond the "don't over-queue" intent
            // [FIX: over-damp — count only unclaimed on-hand units].
            bool onhand_of_type = false;
            for (int i = 0; i < (int)units_.size() && !onhand_of_type; ++i) {
                const Unit& u = units_[i];
                if (!u.alive || u.die_time >= 0.0 || u.house != h ||
                    ai_unit_type_of(rec, u) != want)
                    continue;
                bool in_team = false;
                for (const TeamInstance& tm : st.teams) {
                    for (int m : tm.members)
                        if (m == i) { in_team = true; break; }
                    if (in_team) break;
                }
                if (!in_team) onhand_of_type = true;
            }
            if (rec.queues.producing(cat) == -1 && factory &&
                !build_slowdown_blocks(tc.rules, onhand_of_type) &&
                can_start_unit(rec.queues.state(cat), want, tc.unit_roster,
                               buildings_, rec.cfg.roster, h, rec.cfg.groups,
                               factory)) {
                // Scalar start + slot restamp — the AI-1 building idiom: the
                // difficulty cost handicap on the charge and the BuildTime
                // handicap on the speed. No BuildSlowdown time-mul.
                const int cost = tc.unit_roster[want].cost;
                const int charge = std::max(1, (int)(cost * rec.cfg.mods.cost_mul));
                rec.queues.set_credits(rec.credits);
                start(rec.queues.state(cat), cost, charge,
                      rec.cfg.build_speed * rec.cfg.mods.build_time_mul);
                rec.queues.state(cat).slot.type = want;
                rec.credits = rec.queues.credits();
            }
        }

        // 6) Step every team (pure brain) + execute its command (impure glue);
        //    Disbanding teams release survivors (they simply stop being members
        //    -> idle, re-recruitable) and are erased [RE: ai-team.md §2].
        for (int i = 0; i < (int)st.teams.size();) {
            TeamInstance& t = st.teams[i];
            TeamView view;
            ai_fill_team_view(h, rec, t, view);
            const TeamCommand c =
                step_team(t, view, tc.aimd, st.rng, dt, tc.rules.multiplay);
            ai_execute_team_command(h, rec, t, c);
            // CONTRACT A backstop [wedge-freedom — no forever-stall in ANY
            // move_issued state]: if a move was issued but every live member has
            // stopped short of the REALIZED focus (team.gather_x/gather_y) — the
            // snapped cell was unreachable / boxed and order_unit_move cancelled
            // — the team must still make progress, never wedge:
            //   * GATHERING re-picks a fresh ring cell (new seeded angle) and
            //     re-issues next tick.
            //   * an EXECUTING move opcode re-issues the SAME move ONCE; if it is
            //     still stuck the next pass, the script pc is force-advanced so a
            //     cancelled/unreachable order can NEVER stall a script line
            //     forever (mirrors the original re-issuing Move to stragglers
            //     each pass, then giving up — docs/re/ai-team.md §2).
            if (t.state == TeamState::Gathering || t.state == TeamState::Executing) {
                if (!t.move_issued) {
                    t.stall_reissues = 0;  // no active move -> counter clear
                } else {
                    const double reach = (t.state == TeamState::Gathering)
                                             ? kAiStrayCells
                                             : kAiCloseEnoughCells;
                    bool any_alive = false, any_moving = false, all_within = true;
                    for (int m : t.members) {
                        if (m < 0 || m >= (int)units_.size()) continue;
                        const Unit& u = units_[m];
                        if (!u.alive || u.die_time >= 0.0) continue;
                        any_alive = true;
                        if (u.moving()) any_moving = true;
                        const long long ddx = u.cell_x - t.gather_x;
                        const long long ddy = u.cell_y - t.gather_y;
                        if ((double)(ddx * ddx + ddy * ddy) > reach * reach)
                            all_within = false;
                    }
                    if (any_alive && !any_moving && !all_within) {
                        if (t.state == TeamState::Gathering) {
                            const Cellxy g =
                                gather_cell(view.enemy_x, view.enemy_y, st.rng);
                            t.gather_x = g.x;
                            t.gather_y = g.y;
                            t.move_issued = false;
                        } else if (t.stall_reissues == 0) {
                            t.move_issued = false;   // re-issue the same move once
                            ++t.stall_reissues;
                        } else {
                            ++t.script_pc;           // give up: advance the line
                            t.move_issued = false;
                            t.stall_reissues = 0;
                        }
                    } else {
                        t.stall_reissues = 0;  // progressing/arrived -> reset
                    }
                }
            }
            if (t.state == TeamState::Disbanding) {
                // AI-3 outcome attribution [RE-GATE APPLIED: ai-trigger.md
                // §4/§6 — CONFIRMED]: NO team->trigger back-pointer exists;
                // the original's TeamClass dtor scans the WHOLE trigger array
                // and feeds EVERY trigger whose Team1 == the disbanding
                // team's TeamType. So: a shared Team1 feeds ALL its triggers;
                // a team created as some trigger's Team2 gives NO feedback
                // (only Team1 matches); and autocreate-created teams sharing
                // a trigger's Team1 ALSO feed it — exactly the original.
                // Success = the team ran its script to kOpDisband
                // (completed_success, the team+0x84 flag), NOT survivors
                // [RE: §4]. Adjustment happens ONLY here, at disband.
                if (tc.triggers_enabled) {
                    for (int j = 0; j < (int)tc.aimd.triggers.size(); ++j)
                        if (tc.aimd.triggers[j].team1 == t.teamtype)
                            adjust_weight(rec.trigger_state, tc.aimd, j,
                                          t.completed_success);
                }
                // Release survivors: stand each live member DOWN (stop any
                // in-flight move) so the freed unit is IMMEDIATELY idle &
                // re-recruitable — not left walking to an abandoned gather goal
                // (its `path` would otherwise persist, keeping it non-idle so a
                // replacement team wrongly PRODUCES instead of re-recruiting it)
                // [RE: ai-team.md §2 — "release survivors -> idle, re-recruitable"].
                for (int m : t.members) {
                    if (m < 0 || m >= (int)units_.size()) continue;
                    Unit& u = units_[m];
                    if (!u.alive || u.die_time >= 0.0) continue;
                    occ_.release_all_of(m, u.cell_x, u.cell_y);
                    occ_.set(u.cell_x, u.cell_y, m);
                    u.path.clear();
                    u.path_idx = 0;
                }
                st.teams.erase(st.teams.begin() + i);
            } else {
                ++i;
            }
        }
    }
}

// --- AI-3 AITriggerTypes phase [RE: ai-trigger.md] ---------------------------

const AiTriggerState* World::ai_trigger_state(int house) const {
    return ai_enabled(house) ? &ai_houses_[house].trigger_state : nullptr;
}

void World::ai_fill_trigger_view(int house, const HouseAiRec& rec,
                                 TriggerView& v) const {
    const AiTeamConfig& tc = rec.cfg.team;
    // Per-TechnoType counts, own vs enemy. "Enemy" aggregates EVERY other
    // house [RE-GATE APPLIED: ai-trigger.md §2/deviations — the original's
    // ConditionMet receives the house's ONE current enemy (house+0x5600);
    // our World aggregates all rival houses, the ai-team.md convention].
    // Unit typing maps through the house's OWN roster (ai_unit_type_of), so
    // enemy-UNIT conditions count only types this roster resolves; BUILDING
    // conditions index the full shared building roster directly (b.type), so
    // enemy-owns-building conditions see the player's base. No new state —
    // a per-pass snapshot [spec §4].
    v.own_units.assign(tc.unit_roster.size(), 0);
    v.enemy_units.assign(tc.unit_roster.size(), 0);
    v.own_buildings.assign(rec.cfg.roster.size(), 0);
    v.enemy_buildings.assign(rec.cfg.roster.size(), 0);
    // No neutral/civilian house exists in this engine, so the neutral slices
    // stay EMPTY and ct=7 (CivilianHouseOwns) evaluates FALSE [RE-GATE
    // APPLIED: ai-trigger.md §2 — implementable-if-neutral-house; see the
    // deviations list].
    v.neutral_units.clear();
    v.neutral_buildings.clear();
    for (const Unit& u : units_) {
        // Carried units are off-map/invisible to census [CB-1 off-map invariant].
        if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) continue;
        const int t = ai_unit_type_of(rec, u);
        if (t < 0) continue;
        if (u.house == house) ++v.own_units[t];
        else ++v.enemy_units[t];
    }
    for (const Building& b : buildings_) {
        if (!b.alive || b.state == Building::State::Dead) continue;
        if (b.type < 0 || b.type >= (int)rec.cfg.roster.size()) continue;
        if (b.house == house) ++v.own_buildings[b.type];
        else ++v.enemy_buildings[b.type];
    }
    v.credits = rec.credits;  // OWNER house credits — ct=4 HouseCredits reads
                              // the CALLING house's money scalar [RE-GATE
                              // APPLIED: ai-trigger.md §2 — NOT enemy credits]
    // Convert the engine's AiDifficulty enum (Easy=0/Normal=1/Difficult=2, i.e.
    // 0 = EASIEST) into the pure engine's TriggerView convention, where index
    // 0 = HARDEST (ai_trigger.hpp ~87) — the two are INVERTED relative to each
    // other. trigger_filters_pass then consults enabled[2-difficulty], so the
    // NET map is Easy->enabled[0], Normal->enabled[1], Difficult->enabled[2]
    // (the on-disk Easy/Normal/Hard slots) [RE: §5]. Normal (1) is the 2-x
    // fixed point and is unchanged; the raw fill silently gave a Difficult AI
    // the Easy pool (and vice-versa) — the RE-GATE non-inverted-difficulty drift.
    v.difficulty = 2 - (int)rec.cfg.difficulty;
    v.house_name = tc.house_name;
    v.side = tc.trigger_side;                 // 1=Allied/2=Soviet/3=Yuri [RE: §5]
    v.tech_level = tc.trigger_tech_level;
}

void World::step(double dt) {
    step_units(units_, passable_, water_passable_, grid_w_, grid_h_, occ_, dt, move_cps_, anims_);

    // CB-1 Transports/Cargo [RE: transport-cargo.md]: boarding resolves the
    // frame the passenger lands adjacent; ejects claim occupancy before
    // combat targeting. A pure INSERTION between step_units and step_turrets
    // -- no existing unit-step logic is reordered; `if (!cargo_any_) return;`
    // inside is the only cost every existing demo pays [spec §8].
    step_cargo(dt);

    // Engineer/Spy Capture [RE: capture.md §A5]: the capture-arrival pass --
    // an engineer with a live capture_target that has reached adjacency to its
    // target building captures/repairs it and is consumed. A pure INSERTION
    // beside step_cargo; `if (!capture_any_) return;` inside is the only cost
    // every existing demo pays (no demo issues a capture order) [spec §8].
    step_capture(dt);

    // CB-2 Aircraft [RE: aircraft.md §3.5/§6]: the airborne movement pass --
    // takeoff/cruise/descend runs AFTER step_cargo and BEFORE step_turrets/
    // step_combat, so the aircraft's post-move altitude/position is visible to
    // combat targeting the SAME frame. A pure INSERTION; `if (!aircraft_any_)
    // return;` inside is the only cost every existing demo pays [spec §8].
    step_flight(dt);

    // CB-4 Superweapons [RE: superweapon.md §3]: advance each granted house's
    // recharge timer. A pure INSERTION after step_flight; `if (!sw_any_) return;`
    // inside is the only cost every existing demo pays (no house is granted a SW
    // outside the new gate). The detonation is fire-triggered, not step-
    // triggered, so this pass's ORDER is behaviorally irrelevant (spec §3.3).
    step_superweapon(dt);

    // Engineer/Spy Capture [RE: capture.md §D1/§B2]: tech-structure income +
    // blackout decay (+ disguise detection). Firewalled behind capture_any_
    // (false in every existing demo -> a strict no-op).
    step_tech_structures(dt);

    // Slew vehicle turrets AFTER movement and BEFORE combat so the aim gate
    // (vehicle_aimed) in step_combat sees this frame's post-slew turret
    // facing (docs/re/voxel-render.md §5). No-op for infantry.
    step_turrets(units_, buildings_, dt);

    // Buildings & Construction Task 4: advance buildup/death timers, then
    // restore passability for any building that DIED this frame (state ==
    // Dead) and hasn't had its blocking released yet -- tracked per-building
    // via blocking_released so this only ever fires once per building, at
    // the moment of death (not at the later tombstone/alive=false point).
    step_buildings(buildings_, dt);
    for (Building& b : buildings_) {
        if (b.state == Building::State::Dead && !b.blocking_released) {
            for (const Cellxy& cell : foundation_cells(b.cx, b.cy, b.fw, b.fh)) {
                if (cell.x < 0 || cell.y < 0 || cell.x >= grid_w_ || cell.y >= grid_h_) continue;
                passable_[static_cast<std::size_t>(cell.y) * grid_w_ + cell.x] = 1;
            }
            b.blocking_released = true;
        }
    }

    const std::vector<MoveRequest> reqs = step_combat(units_, buildings_, occ_, dt);
    // Guard-return requests route directly. Approach requests are batched
    // per goal cell (one batch per target) and resolved to DISTINCT free
    // firing-position cells via assign_formation, so a group of attackers
    // fans out around the target instead of queueing single-file behind
    // whoever got in range first (spec: Combat Fidelity Fixes §B; the old
    // goal -- the target's own occupied cell -- was unreachable by
    // traffic-aware re-paths, which is exactly the reported stuck-queue
    // bug). A crowded-out attacker re-assigned its own cell re-requests
    // next frame: bounded churn, accepted for this unit count.
    std::map<std::pair<int, int>, std::vector<int>> approach_groups;
    for (const MoveRequest& r : reqs) {
        if (r.unit < 0 || r.unit >= static_cast<int>(units_.size())) continue;
        const Unit& u = units_[r.unit];
        if (!u.alive || u.die_time >= 0.0) continue;  // died this same step
        // CB-2 Aircraft [RE-GATE APPLIED: aircraft.md §4]: an approach request
        // for an aircraft routes straight to its FLIGHT GOAL (the target's cell) via
        // order_unit_move's aircraft branch (no A*, no assign_formation
        // firing-cell batching -- the flyer cruises nearly over the target,
        // BlimpBomb Range=1.5). The ONE aircraft-aware branch in the
        // MoveRequest dispatch; provably not taken in any existing demo
        // (is_aircraft=false).
        if (u.is_aircraft) {
            order_unit_move(r.unit, r.goal_x, r.goal_y);
            continue;
        }
        // CB-3 Naval [RE-GATE APPLIED: naval.md §1/§4]: an approach request for a
        // naval unit routes straight to a WATER A* path via order_unit_move's
        // grid_for(Water) selection (Task 4) -- bypassing the assign_formation
        // firing-cell batching, which paths over the LAND grid (passable_) and
        // would never find a water firing cell. The ONE naval-aware branch in
        // the MoveRequest dispatch, mirroring CB-2's aircraft branch above;
        // provably not taken in any existing demo (is_naval=false). CONFIRMED:
        // [HYD] sets no NavalTargeting/LandTargeting/CanPassiveAquire keys, so it
        // auto-acquires both land and naval targets by the ordinary Verses>0 rule
        // (no valid_target change -- a naval unit IS a valid target).
        if (u.is_naval) {
            order_unit_move(r.unit, r.goal_x, r.goal_y);
            continue;
        }
        if (r.approach)
            approach_groups[{r.goal_x, r.goal_y}].push_back(r.unit);
        else
            order_unit_move(r.unit, r.goal_x, r.goal_y);
    }
    for (const auto& group : approach_groups) {
        const std::vector<int>& movers = group.second;
        std::vector<Cellxy> cur;
        cur.reserve(movers.size());
        for (int idx : movers) cur.push_back({units_[idx].cell_x, units_[idx].cell_y});
        const std::vector<Cellxy> targets =
            assign_formation({group.first.first, group.first.second}, cur, passable_,
                             grid_w_, grid_h_, occ_);
        for (std::size_t k = 0; k < movers.size(); ++k)
            order_unit_move(movers[k], targets[k].x, targets[k].y);
    }

    // Economy phase [RE: economy.md]: ore growth/spread + harvester mission
    // stepping, gated on set_economy having been called (types_ non-empty).
    // A World that never calls set_economy (every pre-economy-phase caller,
    // including demo-base's World -- see the byte-identity note in
    // task-6-brief.md) steps with zero ore/harvest activity: ore_.step is
    // also gated (not just step_harvest), so ore().timer() stays exactly 0.
    if (!types_.empty()) {
        ore_.step(dt, growth_interval_, growth_pct_, spread_pct_, passable_, ore_rng_);
        std::vector<HarvestMove> h_moves;
        std::vector<HarvestEvent> h_events;
        step_harvest(units_, buildings_, types_, refinery_types_, ore_, passable_, grid_w_,
                    grid_h_, occ_, dt, OreValues{ore_value_ore_, ore_value_gems_},
                    house_credits_, h_moves, h_events);
        // Path each requested move the same way step_combat's MoveRequests
        // are executed above (order_unit_move mirrors that pattern).
        for (const HarvestMove& m : h_moves) order_unit_move(m.unit, m.gx, m.gy);
        for (const HarvestEvent& e : h_events)
            harvest_events_.push_back({static_cast<int>(e.kind), e.unit});
    }

    // Hold at weapon range: a unit under an EXPLICIT attack order that has
    // closed to within range of its live target stops pursuing (step_combat
    // fires independently of movement). Without this, the stale pursue-path
    // aimed at the target's cell would march a ranged unit into melee contact.
    // Gated on attack_order (defense-in-depth): with move orders cancelling
    // engagement (order_move_selected) and auto-acquire/retaliation only ever
    // targeting idle (non-moving) units (step_combat/try_fire), a unit with a
    // live path and a target is already provably an attack_order pursuer --
    // this makes that invariant explicit rather than relying on it silently.
    // Distance metric matches combat.cpp's dist_leptons (2D euclidean on
    // lepton coords; see docs/re/combat.md range divergence note).
    //
    // Checked against the unit's COMMITTED cell (cell_x/cell_y), not its
    // continuous fx/fy: fx/fy can be mid-segment (interpolated) between the
    // last fully-arrived cell and the next one, and this pass SNAPS onto
    // cell_x/cell_y when it stops (below) -- checking fx/fy while snapping to
    // cell_x/cell_y let the two disagree whenever the range boundary was
    // crossed mid-segment (the interpolated fx/fy already in range, but the
    // pre-segment cell_x/cell_y it reverts to still just outside it). That
    // mismatch re-triggers an "out of range, not moving" re-approach request
    // next frame (combat.cpp's attack_order branch) which recomputes the
    // SAME short path and re-crosses the SAME point, live-locking the unit a
    // hair outside range indefinitely (observed via an instrumented trace: a
    // single pursuer frozen ~500 frames at 4.06-4.12 cells, just past the
    // 4.0 weapon range, with Task 3's shorter near-target approach paths --
    // see task-3-report.md). Checking cell_x/cell_y instead makes the check
    // and the snap agree by construction: it only fires the frame a segment
    // FULLY completes into an in-range cell, so the unit always progresses
    // forward and settles first try.
    for (Unit& u : units_) {
        if (!u.alive || u.die_time >= 0.0) continue;
        if (u.target < 0 || !u.attack_order || u.target >= (int)units_.size()) continue;   // only explicit attackers hold at range
        const Unit& t = units_[u.target];
        if (!t.alive || t.die_time >= 0.0) continue;
        const double dx = (t.fx - static_cast<double>(u.cell_x)) * 256.0;
        const double dy = (t.fy - static_cast<double>(u.cell_y)) * 256.0;
        if (std::sqrt(dx * dx + dy * dy) <= u.w_range * 256.0 && !u.path.empty()) {
            // In range with a live pursue-path -> stop and hold (snap onto the
            // committed cell so we don't strand mid-segment; Finding-1 semantics).
            u.path.clear();
            u.path_idx = 0;
            u.fx = u.cell_x;
            u.fy = u.cell_y;
        }
    }

    // Fire/die animation override (Task 7, facing added Task 2): step_units
    // (above) already picked a stand/walk frame assuming the unit is simply
    // idle or walking; this pass OVERRIDES that choice for a unit that is
    // dying or mid-recoil from a shot, using each image's fire/die ranges
    // (anim_for_image) and seq_frame's facing-aware selection. Runs AFTER the
    // hold-at-range pass (so a unit that just stopped to fire this same frame
    // still gets its fire pose) and BEFORE the level patch (ordering vs. the
    // level patch is otherwise irrelevant -- this pass only touches
    // Unit::frame/Unit::facing). Dying units are already skipped by
    // step_units's stand/walk writes (see its
    // `if (!u.alive || u.die_time >= 0.0) continue;` guard), so there is no
    // earlier write this pass could be clobbering for that branch -- their
    // frame comes ONLY from here.
    for (Unit& u : units_) {
        if (!u.alive) continue;
        const UnitAnimSeq& seq = anim_for_image(anims_, u.image);

        // Aim facing (docs/re/infantry-anim.md §5): any unit with a valid
        // live target faces it, even BETWEEN shots (not just during the fire
        // pose) -- so a unit standing still tracking a target still visibly
        // turns to face it, matching the original's aim-facing behavior.
        if (u.target >= 0 && u.target < (int)units_.size()) {
            const Unit& t = units_[u.target];
            if (t.alive) {
                const int f = facing_from_delta(t.fx - u.fx, t.fy - u.fy);
                if (f >= 0) u.facing = f;
            }
        }

        if (u.die_time >= 0.0) {
            // Play the die frames once, forward, clamped on the last frame.
            // The unit tombstones (alive=false) shortly after via
            // combat.cpp's kDieAnimSecs, and unit_draw_order excludes
            // tombstoned units -- so "holding the last frame" is only ever
            // visible for whatever fraction of a second remains before the
            // corpse disappears, never a stuck/looping corpse. default_anim's
            // die entry (134,15,0) is EXACTLY 15 frames at kDieAnimFps
            // (15Hz) == combat.cpp's kDieAnimSecs (1.0s), so every frame is
            // reachable before that tombstone -- seq_frame's own clamp is
            // just a defensive backstop, not a pacing choice (contrast the
            // Task 7 table this replaced, which capped die_count to force
            // reachability -- see docs/re/infantry-anim.md §2/§3).
            const int adv = static_cast<int>(u.die_time * kDieAnimFps);
            u.frame = seq_frame(seq.die, u.facing, adv);
        } else if (u.fire_flash > 0.0) {
            // Play the fire/recoil sequence ONCE across the flash window.
            // fire_flash counts DOWN from kFireFlashSecs (set by
            // combat.cpp's try_fire on every shot) to 0. The fire sequence
            // plays once per shot at kFireAnimFps (15 Hz), so all fire.count
            // frames are visited during the 0.4s flash window.
            // kFireFlashSecs is combat.cpp's (anonymous-namespace, not
            // exposed via combat.hpp) constant, duplicated here at the same
            // value -- same already-tolerated pattern as this file's
            // kTileW/kTileH/kLevelPx duplicating iso_renderer.cpp/input.cpp
            // (task-12-report.md Minor). MUST stay in sync with combat.cpp's
            // kFireFlashSecs if that ever changes.
            const int adv = static_cast<int>((kFireFlashSecs - u.fire_flash) * kFireAnimFps);
            u.frame = seq_frame(seq.fire, u.facing, adv);
        }
        // else: leave step_units' stand/walk choice untouched.
    }

    // Patch terrain height for rendering (step_units is map-agnostic).
    // Dying units still render (die anim) so still get patched; tombstoned
    // (!alive) units are never drawn (unit_draw_order excludes them), so
    // their level is left as-is.
    for (Unit& u : units_) {
        if (!u.alive) continue;
        if (const ra2assets::MapCell* mc = map_->at(u.cell_x, u.cell_y)) u.level = mc->level;
    }

    // AI-1 base-planning firewall [RE: ai-base.md §2]: the planner runs AFTER
    // the existing production/economy step, and ONLY when a house is
    // AI-flagged. The five demo scripts flag no AI house, so ai_any_ stays
    // false and this is a no-op -> byte-identical to before this phase.
    if (ai_any_) {
        step_ai_houses(dt);
        step_ai_teams(dt);  // AFTER the base brain: base tick then team tick,
                            // fixed deterministic order [RE: ai-team.md; spec §5]
    }
}

std::vector<int> unit_draw_order(const std::vector<Unit>& units) {
    std::vector<int> order;
    order.reserve(units.size());
    for (int i = 0; i < static_cast<int>(units.size()); ++i)
        if (units[i].alive && units[i].cargo_of < 0)
            order.push_back(i);  // dying still renders; tombstoned/carried excluded
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        const double da = units[a].fx + units[a].fy;
        const double db = units[b].fx + units[b].fy;
        if (da != db) return da < db;
        if (units[a].fx != units[b].fx) return units[a].fx < units[b].fx;
        return a < b;
    });
    return order;
}

// Copy `passable` but mark every cell OWNED/RESERVED by a unit other than
// `self` impassable, so a re-path routes around the current traffic. The
// unit's own start cell is forced walkable (find_path needs a passable start).
static std::vector<std::uint8_t> passable_minus_units(const std::vector<std::uint8_t>& passable,
                                                      int w, int h, const Occupancy& occ, int self,
                                                      int start_x, int start_y) {
    std::vector<std::uint8_t> out = passable;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int o = occ.at(x, y);
            if (o != Occupancy::kEmpty && o != self)
                out[static_cast<std::size_t>(y) * w + x] = 0;
        }
    if (start_x >= 0 && start_y >= 0 && start_x < w && start_y < h)
        out[static_cast<std::size_t>(start_y) * w + start_x] = 1;
    return out;
}

void step_units(std::vector<Unit>& units, const std::vector<std::uint8_t>& passable,
                const std::vector<std::uint8_t>& water_passable, int w, int h,
                Occupancy& occ, double dt, double move_cps, const AnimTable& anims) {
    for (std::size_t i = 0; i < units.size(); ++i) {
        Unit& u = units[i];
        const int self = static_cast<int>(i);
        const UnitAnimSeq& seq = anim_for_image(anims, u.image);  // per-image anim table (Task 2)

        // Dead/dying units never move, never re-claim a cell, and never
        // occupy: combat.cpp's step_combat already released a unit's
        // occupancy the instant it died (die_time flips to >= 0.0), so
        // falling through to the idle branch below would resurrect a
        // corpse's claim on its cell and wrongly block living units from
        // entering it.
        // CB-2 [RE-GATE APPLIED: aircraft.md §4/§6]: an airborne unit runs NO
        // ground movement/occupancy; a Grounded aircraft keeps its spawn-set
        // occ_ cell (add_unit set it; step_units never re-touches it) until
        // takeoff releases it. Provably false in every existing demo
        // (is_aircraft=false).
        if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0 || u.is_aircraft) continue;

        if (!u.moving()) {
            occ.set(u.cell_x, u.cell_y, self);  // idle units own their cell
            u.fx = u.cell_x;
            u.fy = u.cell_y;
            u.frame = seq_frame(seq.stand, u.facing, 0);
            u.wait_time = 0.0;
            continue;
        }

        // Vehicle hull turn-then-move gate (docs/re/voxel-render.md §5): a
        // vehicle first rotates its body toward the next segment's direction
        // at its ROT rate; until the body is aligned (shortest arc within
        // kAimToleranceU16) it turns in place and does NOT advance this frame.
        // Infantry (!is_vehicle) skip this entirely -- byte-identical path.
        if (u.is_vehicle) {
            std::size_t pi = u.path_idx;
            while (pi < u.path.size() && u.path[pi].x == u.cell_x && u.path[pi].y == u.cell_y)
                ++pi;
            if (pi < u.path.size()) {
                const double seg_dx = u.path[pi].x - u.cell_x;
                const double seg_dy = u.path[pi].y - u.cell_y;
                const double want = facing_u16_from_delta(seg_dx, seg_dy);
                u.body_facing = turn_toward(u.body_facing, want, u.rot_u16_per_sec * dt);
                double d = std::fmod(want - u.body_facing, 65536.0);
                if (d > 32768.0) d -= 65536.0;
                if (d < -32768.0) d += 65536.0;
                if (std::abs(d) > kAimToleranceU16) continue;  // still turning in place
            }
        }

        // Per-unit speed: a vehicle rolls at its own speed_cps; infantry (and
        // any unit with speed_cps == 0) use the World-wide move_cps unchanged.
        const double cps = u.speed_cps > 0.0 ? u.speed_cps : move_cps;
        double budget = cps * dt;
        bool blocked = false;
        while (budget > 0.0 && u.path_idx < u.path.size()) {
            const Cellxy tgt = u.path[u.path_idx];
            if (tgt.x == u.cell_x && tgt.y == u.cell_y) {  // already here (e.g. path[0])
                ++u.path_idx;
                continue;
            }
            // Face the segment being entered (docs/re/infantry-anim.md §5) --
            // updated at segment SELECTION, before the reservation/blocked
            // check below, so a unit still visibly turns toward a blocked
            // direction even on a frame where it can't actually step yet.
            // Vehicles carry a continuous body_facing (updated by the turn
            // gate above) instead of the 8-way Unit::facing; renderers must
            // not use Unit::facing for them, so skip this update.
            if (!u.is_vehicle) {
                const int f = facing_from_delta(tgt.x - u.cell_x, tgt.y - u.cell_y);
                if (f >= 0) u.facing = f;
            }
            // Reserve the next cell before stepping into it.
            if (occ.at(tgt.x, tgt.y) != self) {
                if (occ.can_enter(tgt.x, tgt.y, self)) {
                    occ.set(tgt.x, tgt.y, self);  // reserve (now owns current + next)
                } else {
                    blocked = true;
                    break;
                }
            }
            const double dx = tgt.x - u.fx;
            const double dy = tgt.y - u.fy;
            const double dist = std::sqrt(dx * dx + dy * dy);
            if (dist <= 1e-9 || budget >= dist) {
                occ.release(u.cell_x, u.cell_y, self);  // leave the old cell
                u.fx = tgt.x;
                u.fy = tgt.y;
                u.cell_x = tgt.x;
                u.cell_y = tgt.y;  // occ already owns this cell (reserved above)
                budget -= (dist <= 1e-9 ? 0.0 : dist);
                ++u.path_idx;
                u.wait_time = 0.0;
            } else {
                const double t = budget / dist;
                u.fx += dx * t;
                u.fy += dy * t;
                budget = 0.0;
            }
        }

        if (u.path_idx >= u.path.size()) {  // arrived
            u.fx = u.cell_x;
            u.fy = u.cell_y;
            u.path.clear();
            u.path_idx = 0;
            u.anim_time = 0.0;
            u.frame = seq_frame(seq.stand, u.facing, 0);
            u.wait_time = 0.0;
            occ.set(u.cell_x, u.cell_y, self);
            continue;
        }

        if (blocked) {
            const double prev_wait = u.wait_time;
            u.wait_time += dt;
            u.frame = seq_frame(seq.stand, u.facing, 0);  // stand while waiting (reads as "holding")
            // Seniority stagger (breaks the mirror-symmetric re-path
            // livelock): if the unit blocking our next cell has a LOWER index,
            // it holds course and WE detour early (first attempt at
            // kBlockedWait); if it has a HIGHER index, it is expected to
            // detour first, so our first attempt is delayed one extra
            // interval. Without this, two facing units cross the re-path
            // threshold on the same frame every cycle and -- with symmetric
            // occupancy and deterministic A* tie-breaks -- each computes the
            // mirror image of the other's detour, oscillating between rows
            // forever (a stable 2-cycle: observed in a 200s trace, not
            // hypothetical).
            const Cellxy blk = u.path[u.path_idx];
            const int blocker = occ.at(blk.x, blk.y);
            const double first_try =
                (blocker != Occupancy::kEmpty && blocker < self)
                    ? kBlockedWait
                    : 2.0 * kBlockedWait;
            if (u.wait_time >= kGiveUpAfter) {
                // Continuously blocked with no route found for the whole
                // timeout (boxed-in / sealed corridor) -> give up cleanly.
                u.path.clear();
                u.path_idx = 0;
                u.fx = u.cell_x;
                u.fy = u.cell_y;
                u.wait_time = 0.0;
            } else if (u.wait_time >= first_try &&
                       (prev_wait < first_try ||
                        static_cast<int>((prev_wait - first_try) / kBlockedWait) !=
                            static_cast<int>((u.wait_time - first_try) / kBlockedWait))) {
                // On a re-path attempt boundary (first_try, then every
                // kBlockedWait after) -> attempt a re-path around the current
                // traffic. On FAILURE keep the original path and keep waiting:
                // transient jams (another unit mid-crossing of a funnel cell)
                // clear on their own, and the per-frame can_enter check above
                // resumes the walk the moment the blocker steps off. Only the
                // kGiveUpAfter timeout abandons the order.
                const Cellxy goal = u.path.back();
                // CB-3 Naval [RE-GATE APPLIED: naval.md §1/§3]: re-path over the
                // unit's domain grid -- Land (every existing unit) uses `passable`
                // byte-identically; Water uses `water_passable`. Keyed off
                // movement_domain so ALL THREE A* sites (order_unit_move, this
                // re-path, and the combat-approach naval branch) select on the
                // SAME field; inert today (is_naval == Water-domain). CONFIRMED:
                // one occ_ cell + shared FootClass repath (§1/§3).
                const std::vector<std::uint8_t>& g =
                    u.movement_domain == MovementDomain::Water ? water_passable : passable;
                std::vector<std::uint8_t> p2 =
                    passable_minus_units(g, w, h, occ, self, u.cell_x, u.cell_y);
                std::vector<Cellxy> np = find_path(p2, w, h, {u.cell_x, u.cell_y}, goal);
                if (np.size() > 1) {
                    u.path = std::move(np);
                    u.path_idx = 1;
                    u.wait_time = 0.0;
                }
            }
            continue;
        }

        // Still walking: cycle the walk frames.
        u.anim_time += dt;
        const int phase = static_cast<int>(u.anim_time * kWalkFps) % seq.walk.count;
        u.frame = seq_frame(seq.walk, u.facing, phase);
    }
}

}  // namespace ra2engine
