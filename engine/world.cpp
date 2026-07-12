#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

#include "combat.hpp"         // armor_index, step_combat, MoveRequest
#include "factory.hpp"        // exit_cell/primary_factory/set_primary (Production phase)
#include "group_move.hpp"     // assign_formation
#include "harvest.hpp"        // step_harvest/HarvestMove/HarvestEvent/OreValues (Economy phase)
#include "iso_project.hpp"   // cell_to_screen (terrain_screen_bounds)
#include "iso_renderer.hpp"  // terrain_screen_bounds / pal_for_house declarations
#include "pathfind.hpp"
#include "ra2assets/pal.hpp"  // Palette/Rgba (pal_for_house)

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
        if (!u.is_vehicle || !u.alive || u.die_time >= 0.0) continue;
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
    u.w_rof = c.rof;
    u.w_range = c.range;
    u.w_verses = c.verses;
    // Voxel Vehicles phase (docs/re/voxel-render.md §5): vehicle locomotion
    // state. ROT= is converted to a u16/sec turn rate; speed_cps == 0 keeps
    // the unit on the World-wide move_cps. Infantry leave all of these at
    // their defaults (is_vehicle=false, rot=0, speed_cps=0).
    u.is_vehicle = c.is_vehicle;
    u.speed_cps = c.speed_cps;
    u.rot_u16_per_sec = c.rot * kRotU16PerRotPerSec;
    return u;
}

void World::set_anim(const std::string& image, const UnitAnimSeq& seq) { anims_[image] = seq; }

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
        if (!u.alive || u.die_time >= 0.0) continue;  // corpses/dying: unpickable
        if (u.cell_x == x && u.cell_y == y) return static_cast<int>(i);
    }
    return -1;
}

void World::order_unit_move(int idx, int gx, int gy) {
    if (idx < 0 || idx >= static_cast<int>(units_.size())) return;
    Unit& u = units_[idx];
    // Drop any stale next-cell reservation from an in-flight order (the
    // occupancy may still mark a cell this unit reserved but will now never
    // enter, which would block other units forever); re-assert only the
    // current cell.
    occ_.release_all_of(idx, u.cell_x, u.cell_y);
    occ_.set(u.cell_x, u.cell_y, idx);
    std::vector<Cellxy> path =
        find_path(passable_, grid_w_, grid_h_, {u.cell_x, u.cell_y}, {gx, gy});
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
        u.target = target_idx;
        u.target_building = -1;  // mutually exclusive with a unit target
        u.attack_order = true;
        // Same occupancy cleanup as order_move_selected: drop any stale
        // next-cell reservation from an in-flight move order, re-assert only
        // the current cell, and clear the path -- step_combat's next
        // MoveRequest (pursuit, now that attack_order is true) re-paths it.
        occ_.release_all_of(static_cast<int>(i), u.cell_x, u.cell_y);
        occ_.set(u.cell_x, u.cell_y, static_cast<int>(i));
        u.path.clear();
        u.path_idx = 0;
    }
}

int World::add_building(int type, int cx, int cy, int fw, int fh, int house, int strength,
                        int armor_ix, double buildup_total, bool instant) {
    Building b;
    b.type = type;
    b.cx = cx;
    b.cy = cy;
    b.fw = fw;
    b.fh = fh;
    b.house = house;
    b.hp = b.max_hp = strength;
    b.armor_ix = armor_ix;
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
        u.target_building = b_idx;
        u.target = -1;  // mutually exclusive with a unit target
        u.attack_order = true;
        // Same occupancy-release/path-clear dance as order_attack.
        occ_.release_all_of(static_cast<int>(i), u.cell_x, u.cell_y);
        occ_.set(u.cell_x, u.cell_y, static_cast<int>(i));
        u.path.clear();
        u.path_idx = 0;
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
    add_combat_unit(ex.x, ex.y, house, c);
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

// --- AI-1 base-planning phase [RE: ai-base.md] ------------------------------

void World::enable_ai_house(int house, AiHouseConfig cfg) {
    if (house < 0) return;
    if ((int)ai_houses_.size() <= house) ai_houses_.resize(house + 1);
    HouseAiRec& rec = ai_houses_[house];
    rec.is_ai = true;
    rec.cfg = std::move(cfg);
    rec.state = AiBaseState{};
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
    const PowerTotals pt = power_totals(buildings_, rec.cfg.roster, house);
    v.power_output = pt.output;
    v.power_drain = pt.drain;
    for (const Unit& u : units_)
        if (u.alive && u.house == house && u.is_harvester) ++v.harvesters;
    return v;
}

// Places queue `cat`'s READY building at its planned node's cell through the
// EXISTING validity + add_building path [RE: ai-base.md §3/§6]. Returns true
// if the slot was consumed (placed); false = HOLD (retry next frame).
bool World::ai_place_ready(int house, HouseAiRec& rec, QueueCat cat) {
    const int q = cat == QueueCat::Structure ? 0 : 1;
    if (!rec.queues.ready(cat)) return false;
    const int node_i = rec.state.pending[q];
    if (node_i < 0 || node_i >= (int)rec.state.plan.size()) {
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
            for (QueueCat c : {QueueCat::Structure, QueueCat::Defense})
                if (rec.queues.producing(c) != -1) rec.queues.cancel(c);
        }

        // 4) Tick the queues: SAME power clamp as the player [RE: ai-base.md
        //    §6], difficulty time handicap applied at start() (below).
        const PowerTotals pt = power_totals(buildings_, cfg.roster, h);
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
        rec.state.pending[q] = node_i;
    }
}

void World::step(double dt) {
    step_units(units_, passable_, grid_w_, grid_h_, occ_, dt, move_cps_, anims_);

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
    if (ai_any_) step_ai_houses(dt);
}

std::vector<int> unit_draw_order(const std::vector<Unit>& units) {
    std::vector<int> order;
    order.reserve(units.size());
    for (int i = 0; i < static_cast<int>(units.size()); ++i)
        if (units[i].alive) order.push_back(i);  // dying still renders; tombstoned excluded
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

void step_units(std::vector<Unit>& units, const std::vector<std::uint8_t>& passable, int w, int h,
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
        if (!u.alive || u.die_time >= 0.0) continue;

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
                std::vector<std::uint8_t> p2 =
                    passable_minus_units(passable, w, h, occ, self, u.cell_x, u.cell_y);
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
