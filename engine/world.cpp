#include "world.hpp"
#include "weapon_unit.hpp"  // Gap fix C

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
#include "roster2.hpp"        // naval_exit_cell (gap owner A: real-play Naval Yard)

namespace ra2engine {

// W2-SK-16 A* expansion budgets (closed nodes).
constexpr int kNearestPathBudget = 20000;
constexpr int kRepathBudget = 4000;

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
        } else if (u.attack_cell_x >= 0) {   // G16: force-fire at a ground cell
            want = facing_u16_from_delta(static_cast<double>(u.attack_cell_x) - u.fx,
                                         static_cast<double>(u.attack_cell_y) - u.fy);
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
// Multi-house / Alliances phase [RE: multihouse.md §1]: named MP-color indices
// (into the 8 MP colors). Consulted ONLY when houses_configured_ (render-only,
// unhashed), so any two distinct valid 0..7 values are acceptable.
constexpr int kMpColorBlue = 2;
constexpr int kMpColorRed  = 1;

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

// Multi-house / Alliances phase [RE: multihouse.md §2]: the 8 MP-color remap.
// Mirrors pal_for_house's band loop (indices 16..31), but instead of the fixed
// blue ratio it spreads the chosen MP color's HUE/SATURATION across the band at
// a Value taken from each source entry's brightness (so the art's shading ramp
// is preserved). Render-only; NEVER read by world_hash. Declared in
// iso_renderer.hpp; defined here (SDL-free TU) so ra2assets_tests can call it.
namespace {
std::vector<std::array<int, 3>>& color_schemes() {
    static std::vector<std::array<int, 3>> v;
    return v;
}
}  // namespace
void set_color_schemes(const std::vector<std::array<int, 3>>& hsv) { color_schemes() = hsv; }
int color_scheme_count() { return static_cast<int>(color_schemes().size()); }

ra2assets::Palette pal_for_color(const ra2assets::Palette& unit_pal, int color_ix) {
    ra2assets::Palette out = unit_pal;
    if (color_ix >= kColorSchemeBase) {   // W6-05-01: a rules [Colors] scheme
        const auto& cs = color_schemes();
        const int k = color_ix - kColorSchemeBase;
        if (k >= static_cast<int>(cs.size())) return out;
        return pal_for_hsv(unit_pal, cs[(std::size_t)k][0], cs[(std::size_t)k][1], cs[(std::size_t)k][2]);
    }
    if (color_ix < 0 || color_ix > 7) return out;   // out of range -> unchanged
    // The 8 MP colors as RA2 HSV triples (hue/sat/val, each 0..255).
    static const int kHsv[8][3] = {
        { 43, 239, 255},  // 0 Gold
        {  0, 230, 255},  // 1 DarkRed
        { 25, 230, 255},  // 2 Orange
        {221, 102, 255},  // 3 Magenta
        {201, 201, 189},  // 4 Purple
        {153, 214, 212},  // 5 DarkBlue
        {131, 200, 230},  // 6 DarkSky
        { 81, 200, 210},  // 7 DarkGreen
    };
    return pal_for_hsv(unit_pal, kHsv[color_ix][0], kHsv[color_ix][1], kHsv[color_ix][2]);
}

// W3-B: the same remap from an explicit RA2 HSV triple (rules [Colors]), e.g.
// ColorSchemes[0] LightGold=25,255,255 for AltPalette anims with no owner.
// W5-PRES R-103 [RE: ColorScheme 0x68C110 -> ramp 0x68BDB0, docs/re/
// w5-presentation.md §4]: indices 16..31 are REPLACED (the source ramp is not
// read): shade i takes S' = trunc(SIN(50deg + i*40/15deg) * S) and
// V' = trunc(COS(20deg + i*70/15deg) * V) (i == 0: COS(pi/16)) through the
// integer HSV2RGB 0x517260; SIN/COS are the game's 8192-step float table
// (0x84E004, index (n>>1)+(n&1), n = trunc(x * 16384/2pi)).
namespace {
float rmp_table(int k) { return std::sin(static_cast<float>(k) * 6.2831855f / 8192.0f); }
int rmp_index(float x) {
    const int n = static_cast<int>(x * 2607.594482421875f);
    int idx = n >> 1;
    if ((n & 1) && idx < 0x1FFF) ++idx;
    return idx & 0x1FFF;
}
float rmp_sin(float x) { return rmp_table(rmp_index(x)); }
float rmp_cos(float x) { return rmp_table(rmp_index(x) + 0x800); }
ra2assets::Rgba rmp_hsv2rgb(unsigned h, unsigned s, unsigned v, std::uint8_t a) {
    const unsigned f = (h * 6) % 255, sec = h * 6 / 255;
    const unsigned p = (255 - s) * v / 255;
    const unsigned q = (255 - f * s / 255) * v / 255;
    const unsigned t = (255 - (255 - f) * s / 255) * v / 255;
    unsigned r = v, g = t, b = p;
    switch (sec) {
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
        default: break;   // 0 and 6
    }
    return ra2assets::Rgba{static_cast<std::uint8_t>(std::min(255u, r)), static_cast<std::uint8_t>(std::min(255u, g)),
                           static_cast<std::uint8_t>(std::min(255u, b)), a};
}
}  // namespace

ra2assets::Palette pal_for_hsv(const ra2assets::Palette& unit_pal, int h, int s, int v) {
    ra2assets::Palette out = unit_pal;
    for (int i = 0; i < 16; ++i) {
        double ang_a = i * 0.08144869842640204 + 0.3490658503988659;
        if (i == 0) ang_a = 0.19634954084936207;
        const double ang_b = i * 0.046542113386515455 + 0.8726646259971648;
        const int s2 = static_cast<int>(rmp_sin(static_cast<float>(ang_b)) * static_cast<float>(s));
        const int v2 = static_cast<int>(rmp_cos(static_cast<float>(ang_a)) * static_cast<float>(v));
        out[16 + i] = rmp_hsv2rgb(static_cast<unsigned>(std::clamp(h, 0, 255)), static_cast<unsigned>(std::clamp(s2, 0, 255)),
                                  static_cast<unsigned>(std::clamp(v2, 0, 255)), unit_pal[16 + i].a);
    }
    return out;
}

// house_palette [RE: multihouse.md §2/D6]: the render entry. With
// houses_configured_ FALSE -> pal_for_house(unit_pal, house) EXACTLY (today's
// blue/red) -> byte-identical / pixel-identical. Configured -> the 8-MP-color
// spread from house_color(house). Render-only; NOT hashed.
ra2assets::Palette World::house_palette(const ra2assets::Palette& unit_pal, int house) const {
    if (!houses_configured_) return pal_for_house(unit_pal, house);   // today's derivation
    return pal_for_color(unit_pal, house_color(house));
}

World::World(const ra2assets::MapFile& map) : map_(&map) {
    passable_ = build_passability(map.cells(), grid_w_, grid_h_);
    occ_ = Occupancy(grid_w_, grid_h_);
    // Economy phase: built unconditionally (empty if the map has no overlay
    // section) so ore() is always valid; set_economy gates whether it ever
    // steps (byte-identity guard -- see types_'s comment in world.hpp).
    ore_ = OreField::from_map(map, grid_w_, grid_h_);
    // Multi-house / Alliances phase [RE: multihouse.md §1]: populate the 3
    // default houses so house_count()==3 and today's world is reproduced. The
    // firewall (houses_configured_) stays FALSE -> accessors use today's
    // derivation -> byte-identical. NOT hashed.
    ensure_houses_default();
}

// --- Multi-house / Alliances phase [RE: multihouse.md §1] -------------------
void World::ensure_houses_default() {
    if (!houses_.empty()) return;
    houses_.resize(3);
    houses_[0] = HouseRec{Side::Allied,  kMpColorBlue, "", 0, true};
    houses_[1] = HouseRec{Side::Soviet,  kMpColorRed,  "", 1, true};
    houses_[2] = HouseRec{Side::Neutral, kMpColorBlue, "", 2, false};
    // houses_configured_ stays FALSE -> accessors use today's derivation.
    grow_house_arrays(3);   // T3: size the six per-house vectors to 3 (indices 0/1/2).
}

void World::grow_house_arrays(int n) {
    // T3 [RE: multihouse.md §D2]: grow the six per-house vectors to n, preserving
    // existing values. With n==3 (default) each is length 3 -> byte-identical to
    // the old [3] arrays. NONE feeds world_hash.
    if ((int)blackout_frames_.size()  < n) blackout_frames_.resize(n, 0);
    if ((int)produced_veteran_.size() < n) produced_veteran_.resize(n, 0);
    if ((int)explored_.size()         < n) explored_.resize(n);   // empty grids (shroud off)
    if ((int)visible_.size()          < n) visible_.resize(n);
    if ((int)map_reveal_.size()       < n) map_reveal_.resize(n, 0);
    if ((int)house_credits_.size()    < n) house_credits_.resize(n, 0.0);
}

World::Side World::house_side(int h) const {
    if (!houses_configured_) return (h == 1) ? Side::Soviet : Side::Allied;  // today's derivation
    if (h < 0 || h >= (int)houses_.size()) return Side::Allied;
    return houses_[h].side;
}

int World::house_color(int h) const {
    if (!houses_configured_) return (h == 1) ? kMpColorRed : kMpColorBlue;   // today's red/blue
    if (h < 0 || h >= (int)houses_.size()) return kMpColorBlue;
    return houses_[h].color_ix;
}

int World::house_team(int h) const {
    if (h < 0 || h >= (int)houses_.size()) return h;   // unknown -> own id
    const int t = houses_[h].alliance_team;
    return (t < 0) ? h : t;                             // sentinel -> own id (FFA)
}

bool World::house_combatant(int h) const {
    if (h < 0 || h >= (int)houses_.size()) return true;   // unknown -> combatant (inert)
    return houses_[h].combatant;
}

// --- Multi-house / Alliances phase [RE: multihouse.md §3/§4, D3] -------------
// Pairwise relationship predicates. With houses_configured_ FALSE they reduce
// EXACTLY to today's identity comparisons -- is_enemy(a,b) == (a != b),
// are_allied(a,b) == (a == b) -- so every routed enmity site is byte-identical
// in every gate (the firewall is closed there). Neutral/non-combatant is NEVER
// an enemy of anyone (both directions) and is never allied to a combatant; self
// is always allied to self. NOT read by world_hash.
bool World::are_allied(int a, int b) const {
    if (a == b) return true;                       // self always allied to self
    if (!houses_configured_) return false;         // today: only same-house is friendly
    if (!house_combatant(a) || !house_combatant(b)) return false;   // neutral: not an ally
    const int n = (int)houses_.size();
    if (!ally_pairs_.empty() && a >= 0 && b >= 0 && a < n && b < n &&
        ally_pairs_.size() == (std::size_t)n * n)
        return ally_pairs_[(std::size_t)a * n + b] != 0;   // W2-CA-10
    return house_team(a) == house_team(b);
}

void World::set_pair_alliance(int a, int b, bool allied) {
    const int n = (int)houses_.size();
    if (a < 0 || b < 0 || a >= n || b >= n || a == b) return;
    const bool was_allied = are_allied(a, b);
    if (ally_pairs_.size() != (std::size_t)n * n) {
        std::vector<std::uint8_t> m((std::size_t)n * n, 0);
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                m[(std::size_t)i * n + j] = (i == j || house_team(i) == house_team(j)) ? 1 : 0;
        ally_pairs_ = std::move(m);
    }
    ally_pairs_[(std::size_t)a * n + b] = allied ? 1 : 0;
    ally_pairs_[(std::size_t)b * n + a] = allied ? 1 : 0;
    // W6-03-04 [RE: FUN_004f9990 MakeAlly -> FUN_005045b0(-anger) toward the
    // new ally; FUN_004f9db0 MakeEnemy -> FUN_005045b0(1)]: both directions.
    for (int k = 0; k < 2; ++k) {
        const int h = k ? b : a, o = k ? a : b;
        if (!ai_enabled(h)) continue;
        if (allied) ai_add_anger(h, o, -ai_anger(h, o));
        else if (was_allied) ai_add_anger(h, o, 1);
    }
}

void World::set_ally_view(int a, int b, bool allied) {
    const int n = (int)houses_.size();
    if (a < 0 || b < 0 || a >= n || b >= n || a == b) return;
    if (ally_pairs_.size() != (std::size_t)n * n) set_pair_alliance(a, b, are_allied(a, b));
    ally_pairs_[(std::size_t)a * n + b] = allied ? 1 : 0;
}

bool World::is_enemy(int a, int b) const {
    if (!houses_configured_) return a != b;        // EXACTLY today's behavior
    if (a == b) return false;
    if (!house_combatant(a) || !house_combatant(b)) return false;   // neutral NEVER an enemy
    return !are_allied(a, b);
}

// --- Victory / defeat [RE: multihouse.md §5, D5] ----------------------------
// Greenfield last-team-standing adjudication. Firewalled behind victory_enabled_
// (false in every existing gate -> step_victory early-returns, no state feeds
// world_hash). ShortGame=yes (default) -> a combatant house is defeated when its
// last building falls; =no -> zero buildings AND zero units. Neutral/non-
// combatant houses never win or lose. Computes only -- no side effects on the
// sim (the gate/UI reads house_defeated/winning_team).
void World::enable_victory(bool on, bool short_game) {
    victory_enabled_ = on;
    short_game_      = short_game;
    if (on) { house_defeated_.assign(house_count(), 0); winning_team_ = -1; }
}

bool World::house_defeated(int h) const {
    return h >= 0 && h < (int)house_defeated_.size() && house_defeated_[h] != 0;
}

int World::winning_team() const { return winning_team_; }

// W6-03-01 [RE: rulesmd [OverlayTypes] 3=GAWALL, 27=NAWALL -- a placed
// Wall=yes type is an overlay in the original, not a BuildingClass, so the
// ShortGame "owns a building" count never sees it; Insignificant=yes types
// (walls, tech/civilian structures, lamps; ObjectType +0x232) never keep a
// house alive]. Untyped test buildings (type out of range) count as before.
bool World::building_insignificant(const Building& b) const {
    const std::vector<BuildingTypeDef>& bts = btypes();
    if (b.type < 0 || b.type >= (int)bts.size()) return false;
    return bts[b.type].wall || bts[b.type].insignificant;
}

bool World::building_is_wall(const Building& b) const {
    const std::vector<BuildingTypeDef>& bts = btypes();
    return b.type >= 0 && b.type < (int)bts.size() && bts[b.type].wall;
}

void World::step_victory() {
    if (!victory_enabled_) return;                           // firewall (spec §Global)
    const int n = house_count();
    if ((int)house_defeated_.size() != n) house_defeated_.resize(n, 0);
    // Count alive buildings/units per house.
    std::vector<int> blds(n, 0), unts(n, 0);
    for (const Building& b : buildings_)
        if (b.alive && b.state != Building::State::Dead &&
            b.house >= 0 && b.house < n && !building_insignificant(b)) ++blds[b.house];
    // gF [RE: docs/re/mcv-deploy.md §5, HouseClass::AI FUN_004F8350]: under
    // ShortGame a house is defeated only when it owns no building AND no unit
    // of a [General] BaseUnit type (an MCV that can still found a base) --
    // otherwise every skirmish would end at frame 0 (MCV-only start).
    std::vector<int> mcvs(n, 0);
    const std::vector<BuildingTypeDef>& bts = btypes();
    for (const Unit& u : units_) {
        if (!(u.alive && u.die_time < 0.0 && u.house >= 0 && u.house < n)) continue;
        ++unts[u.house];
        if (u.deploys_into >= 0 && u.deploys_into < (int)bts.size() && bts[u.deploys_into].conyard)
            ++mcvs[u.house];
    }
    // Mark defeated combatant houses (sticky). Neutral never defeated.
    for (int h = 0; h < n; ++h) {
        if (!house_combatant(h) || house_defeated_[h]) continue;
        const bool defeated = short_game_ ? (blds[h] == 0 && mcvs[h] == 0)
                                          : (blds[h] == 0 && unts[h] == 0);
        if (defeated) house_defeated_[h] = 1;
    }
    // WIN = exactly one alliance-team of combatant houses survives (last-team-
    // standing). While >= 2 combatant teams survive -> -1. Neutral never counts.
    int surviving_team = -1; bool multiple = false;
    for (int h = 0; h < n; ++h) {
        if (!house_combatant(h) || house_defeated_[h]) continue;
        const int t = house_team(h);
        if (surviving_team == -1) surviving_team = t;
        else if (t != surviving_team) { multiple = true; break; }
    }
    winning_team_ = (!multiple && surviving_team != -1) ? surviving_team : -1;
}

void World::set_house(int h, HouseRec rec) {
    if (h < 0) return;
    ensure_houses_default();
    if (h >= (int)houses_.size()) {
        const int old = (int)houses_.size();
        houses_.resize(h + 1);
        for (int i = old; i <= h; ++i) houses_[i] = HouseRec{};   // default combatant
        grow_house_arrays((int)houses_.size());   // T3: resize the six vectors + house_defeated_
    }
    houses_[h] = rec;
    houses_configured_ = true;   // open the firewall
}

int World::cell_level(int x, int y) const {
    if (x < 0 || y < 0 || x >= grid_w_ || y >= grid_h_) return 0;
    const ra2assets::MapCell* mc = map_->at(x, y);
    return mc ? mc->level : 0;
}

void World::enable_subcells() {
    if (occ_.subcells()) return;
    occ_.enable_subcells();
    for (int i = 0; i < static_cast<int>(units_.size()); ++i) subcell_register(i);
    update_sub_offsets();
}

void World::subcell_register(int idx) {
    if (!occ_.subcells() || idx < 0 || idx >= static_cast<int>(units_.size())) return;
    const Unit& u = units_[static_cast<std::size_t>(idx)];
    // W5-B: only real [InfantryTypes] units take spots (an SHP vehicle such
    // as the landing craft is drawn like an infantry but holds a whole cell).
    const bool inf = u.is_infantry && !u.is_vehicle && !u.is_aircraft && !u.is_naval;
    const bool held = occ_.at(u.cell_x, u.cell_y) == idx || occ_.spot_of(u.cell_x, u.cell_y, idx) >= 0;
    if (held) occ_.release(u.cell_x, u.cell_y, idx);
    occ_.mark_infantry(idx, inf);
    if (held) occ_.set(u.cell_x, u.cell_y, idx);
}

void World::place_in_spot(int idx, int spot) {
    if (!occ_.subcells() || idx < 0 || idx >= static_cast<int>(units_.size()) || !occ_.is_infantry(idx))
        return;
    const Unit& u = units_[static_cast<std::size_t>(idx)];
    if (occ_.spot_unit(u.cell_x, u.cell_y, spot) != Occupancy::kEmpty &&
        occ_.spot_unit(u.cell_x, u.cell_y, spot) != idx)
        return;
    occ_.set_spot(u.cell_x, u.cell_y, idx, spot);
    update_sub_offsets();
}

void World::arm_land_speed(const std::vector<std::uint8_t>& land, const std::vector<std::uint8_t>& ramp,
                           const LandSpeedRules& r) {
    const std::size_t n = static_cast<std::size_t>(grid_w_) * grid_h_;
    speed_land_ = land;
    speed_land_.resize(n, 0);
    ramp_ = ramp;
    ramp_.resize(n, 0);
    level_grid_.assign(n, 0);
    for (int y = 0; y < grid_h_; ++y)
        for (int x = 0; x < grid_w_; ++x)
            level_grid_[static_cast<std::size_t>(y) * grid_w_ + x] = static_cast<std::int8_t>(cell_level(x, y));
    land_rules_ = r;
    land_speed_armed_ = true;
}

int World::speed_land_at(int x, int y) const {
    if (!land_speed_armed_ || x < 0 || y < 0 || x >= grid_w_ || y >= grid_h_) return 0;
    const std::size_t i = static_cast<std::size_t>(y) * grid_w_ + x;
    // Ore on a flat cell is Tiberium land (FUN_0047D1C0: a Tiberium overlay with
    // Land=Clear and ramp type < 5).
    if (ore_.has_ore(x, y) && ramp_[i] < 5) return 5;
    return speed_land_[i];
}

double World::unit_speed_cps(int i) const {
    if (i < 0 || i >= static_cast<int>(units_.size())) return 0.0;
    const Unit& u = units_[static_cast<std::size_t>(i)];
    return (u.speed_cps > 0.0 ? u.speed_cps : move_cps_) * u.speed_mult;
}

void World::update_sub_offsets() {
    if (!occ_.subcells()) return;
    for (int i = 0; i < static_cast<int>(units_.size()); ++i) {
        Unit& u = units_[static_cast<std::size_t>(i)];
        if (!occ_.is_infantry(i)) {
            u.spot = 0;
            u.sub_x = u.sub_y = 0.0;
            continue;
        }
        int s = occ_.spot_of(u.cell_x, u.cell_y, i);
        if (s < 0) s = 0;
        u.spot = s;
        int ax, ay;
        spot_offset(s, ax, ay);
        double ox = ax, oy = ay;
        // Mid-segment toward a reserved spot of the next path cell: blend.
        if (u.moving() && (u.fx != u.cell_x || u.fy != u.cell_y)) {
            std::size_t pi = u.path_idx;
            while (pi < u.path.size() && u.path[pi].x == u.cell_x && u.path[pi].y == u.cell_y) ++pi;
            if (pi < u.path.size()) {
                const Cellxy t = u.path[pi];
                const int ns = occ_.spot_of(t.x, t.y, i);
                if (ns >= 0) {
                    const double seg = std::hypot(t.x - u.cell_x, t.y - u.cell_y);
                    const double rem = std::hypot(t.x - u.fx, t.y - u.fy);
                    double f = seg > 0.0 ? 1.0 - rem / seg : 1.0;
                    if (f < 0.0) f = 0.0;
                    if (f > 1.0) f = 1.0;
                    int bx, by;
                    spot_offset(ns, bx, by);
                    ox = ax + (bx - ax) * f;
                    oy = ay + (by - ay) * f;
                }
            }
        }
        u.sub_x = ox;
        u.sub_y = oy;
    }
}

bool World::walkable(int x, int y) const {
    if (x < 0 || y < 0 || x >= grid_w_ || y >= grid_h_) return false;
    return passable_[static_cast<size_t>(y) * grid_w_ + x] != 0;
}

const std::vector<std::uint8_t>& World::crush_grid() const {
    crush_grid_ = passable_;
    const std::size_t n = std::min(crush_grid_.size(), crush_wall_.size());
    for (std::size_t i = 0; i < n; ++i)
        if (crush_wall_[i] == 1) crush_grid_[i] = 1;
    return crush_grid_;
}

const std::vector<std::uint8_t>* World::crush_leak() const {
    const std::vector<std::uint8_t>* l = wall_leak();
    if (!l) return nullptr;
    crush_leak_ = *l;
    const std::size_t n = std::min(crush_leak_.size(), crush_wall_.size());
    for (std::size_t i = 0; i < n; ++i)
        if (crush_wall_[i] == 1) crush_leak_[i] = 0;
    return &crush_leak_;
}

// P4: a Crusher land vehicle about to drive into a standing crushable wall
// cell (its next path cell, adjacent) flattens it: the cell opens for every
// unit and leaves the leak mask; the renderer stops drawing the overlay.
void World::mark_wall_overlay(int x, int y, int strength, int levels, bool wood) {
    if (x < 0 || y < 0 || x >= grid_w_ || y >= grid_h_ || passable_.empty()) return;
    const std::size_t n = passable_.size();
    if (wall_ov_type_.size() != n) wall_ov_type_.assign(n, 0);
    if (wall_ov_stage_.size() != n) wall_ov_stage_.assign(n, 0);
    std::size_t t = 0;
    for (; t < wall_ov_types_.size(); ++t)
        if (wall_ov_types_[t].strength == strength && wall_ov_types_[t].levels == levels &&
            wall_ov_types_[t].wood == wood)
            break;
    if (t == wall_ov_types_.size()) {
        if (t >= 255) return;
        wall_ov_types_.push_back({std::max(1, strength), std::max(1, levels), wood});
    }
    wall_ov_type_[static_cast<std::size_t>(y) * grid_w_ + x] = static_cast<std::uint8_t>(t + 1);
}

int World::wall_overlay_stage(int x, int y) const {
    if (x < 0 || y < 0 || x >= grid_w_ || y >= grid_h_ || wall_ov_stage_.size() != passable_.size()) return 0;
    return wall_ov_stage_[static_cast<std::size_t>(y) * grid_w_ + x];
}

void World::w6_overlay_wall_hit(int x, int y, int wall_flags, int damage) const {
    if (x < 0 || y < 0 || x >= grid_w_ || y >= grid_h_ || wall_ov_type_.size() != passable_.size()) return;
    const std::uint8_t t = wall_ov_type_[static_cast<std::size_t>(y) * grid_w_ + x];
    if (t == 0 || wall_crushed(x, y)) return;
    w6_overlay_reduce(x, y, wall_hit_damage(wall_flags, wall_ov_types_[t - 1u].wood, damage));
}

void World::w6_overlay_reduce(int x, int y, int dmg) const {
    if (dmg == 0) return;
    const std::size_t i = static_cast<std::size_t>(y) * grid_w_ + x;
    const std::uint8_t t = wall_ov_type_[i];
    if (t == 0 || wall_crushed(x, y)) return;
    if (std::find(w6_wall_kills_.begin(), w6_wall_kills_.end(), static_cast<int>(i)) != w6_wall_kills_.end()) return;
    const WallOvType& ty = wall_ov_types_[t - 1u];
    static const int kDx[4] = {0, 1, 0, -1}, kDy[4] = {-1, 0, 1, 0};   // N, E, S, W
    auto same = [&](int nx, int ny) {
        if (nx < 0 || ny < 0 || nx >= grid_w_ || ny >= grid_h_) return false;
        const std::size_t j = static_cast<std::size_t>(ny) * grid_w_ + nx;
        return wall_ov_type_[j] == t && !wall_crushed(nx, ny) &&
               std::find(w6_wall_kills_.begin(), w6_wall_kills_.end(), static_cast<int>(j)) == w6_wall_kills_.end();
    };
    bool connected = false;
    for (int k = 0; k < 4; ++k) connected = connected || same(x + kDx[k], y + kDy[k]);
    const WallStep r = reduce_wall(wall_ov_stage_[i], ty.levels, ty.strength, dmg, connected, combat_rng_ptr());
    if (!r.advanced) return;
    wall_ov_stage_[i] = static_cast<std::uint8_t>(r.stage);
    if (r.cascade)
        for (int k = 0; k < 4; ++k) {
            const int nx = x + kDx[k], ny = y + kDy[k];
            if (same(nx, ny) && wall_ov_stage_[static_cast<std::size_t>(ny) * grid_w_ + nx] < 1)
                w6_overlay_reduce(nx, ny, 200);
        }
    if (r.destroyed) w6_wall_kills_.push_back(static_cast<int>(i));
}

void World::w6_apply_wall_kills() {
    if (w6_wall_kills_.empty()) return;
    if (crush_wall_.size() != passable_.size()) crush_wall_.assign(passable_.size(), 0);
    for (int i : w6_wall_kills_) {
        if (i < 0 || static_cast<std::size_t>(i) >= passable_.size()) continue;
        crush_wall_[static_cast<std::size_t>(i)] = 2;   // gone: drawn and blocked no more
        passable_[static_cast<std::size_t>(i)] = 1;
        if (static_cast<std::size_t>(i) < wall_leak_.size()) wall_leak_[static_cast<std::size_t>(i)] = 0;
        if (static_cast<std::size_t>(i) < wall_ov_stage_.size()) wall_ov_stage_[static_cast<std::size_t>(i)] = 0;
    }
    w6_wall_kills_.clear();
}

void World::w6_step_powered_units() {
    bool any = false;
    for (const Unit& u : units_) any = any || (u.w6.t.powered_unit && u.alive);
    if (!any) return;
    const std::vector<BuildingTypeDef>& bt = btypes();
    for (Unit& u : units_) {
        if (!u.w6.t.powered_unit || !u.alive || u.die_time >= 0.0) continue;
        bool on = false;
        for (const Building& b : buildings_) {
            if (!b.alive || b.state != Building::State::Idle || b.house != u.house || b.power_off) continue;
            if (b.type < 0 || b.type >= static_cast<int>(bt.size())) continue;
            const std::string& pu = bt[static_cast<std::size_t>(b.type)].w6.powers_unit;
            if (pu.empty() || pu != u.image) continue;
            if (house_low_power(u.house)) continue;
            on = true;
            break;
        }
        if (!on && !u.w6.unpowered) {   // turned off: it stops where it is
            u.path.clear();
            u.path_idx = 0;
            u.target = -1;
            u.target_building = -1;
            u.attack_order = false;
        }
        u.w6.unpowered = !on;
        if (!u.abil_bound) u.frozen = !on;   // abilities recompute frozen for bound units (below)
    }
}

void World::step_wall_crush() {
    if (crush_wall_.size() != passable_.size()) return;
    for (const Unit& u : units_) {
        if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0 || !crush_paths(u)) continue;
        std::size_t pi = u.path_idx;
        while (pi < u.path.size() && u.path[pi].x == u.cell_x && u.path[pi].y == u.cell_y) ++pi;
        if (pi >= u.path.size()) continue;
        const Cellxy n = u.path[pi];
        if (std::abs(n.x - u.cell_x) > 1 || std::abs(n.y - u.cell_y) > 1) continue;
        if (n.x < 0 || n.y < 0 || n.x >= grid_w_ || n.y >= grid_h_) continue;
        const std::size_t i = static_cast<std::size_t>(n.y) * grid_w_ + n.x;
        if (crush_wall_[i] != 1) continue;
        crush_wall_[i] = 2;
        passable_[i] = 1;
        if (i < wall_leak_.size()) wall_leak_[i] = 0;
    }
}

const std::vector<std::uint8_t>& World::amphib_grid() const {
    amphib_passable_ = passable_;
    const std::size_t n = std::min(amphib_passable_.size(), water_passable_.size());
    for (std::size_t i = 0; i < n; ++i)
        if (water_passable_[i]) amphib_passable_[i] = 1;
    return amphib_passable_;
}

bool World::unit_can_enter_cell(const Unit& u, int x, int y) const {
    if (x < 0 || y < 0 || x >= grid_w_ || y >= grid_h_) return false;
    if (u.is_aircraft) return true;   // flight ignores the ground grids
    const std::size_t i = static_cast<std::size_t>(y) * grid_w_ + x;
    const bool land = i < passable_.size() && passable_[i] != 0;
    const bool water = i < water_passable_.size() && water_passable_[i] != 0;
    switch (u.movement_domain) {
        case MovementDomain::Water: return water;
        case MovementDomain::Amphibious: return land || water;
        default: return land;
    }
}

bool World::group_can_enter(const std::vector<int>& ids, int house, int x, int y) const {
    bool any = false;
    for (int i : ids) {
        if (i < 0 || i >= static_cast<int>(units_.size())) continue;
        const Unit& u = units_[static_cast<std::size_t>(i)];
        if (u.house != house || !u.alive || u.die_time >= 0.0 || u.cargo_of >= 0)
            continue;
        any = true;
        if (unit_can_enter_cell(u, x, y)) return true;
    }
    return !any && walkable(x, y);
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
    occ_.mark_infantry(idx, true);   // W5-B: re-registered by add_combat_unit once typed
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
    // P5 [RE: veterancy.md V15]: in a veterancy-enabled World a produced
    // veteran is Veterancy=1.0f (applied below), NOT the legacy spawn-baked
    // HP/damage. The legacy baking is kept verbatim for every World that never
    // enabled veterancy (the capture gate) -> byte-identical there.
    if (c.veteran_rank > 0 && !vet_any_) {
        u.w_damage = veteran_firepower(u.w_damage, c.veteran_rank);
        u.max_hp = veteran_hp(u.max_hp, c.veteran_rank);
        u.hp = u.max_hp;
    }
    u.w_rof = c.rof;
    u.w6.t = c.w6;                          // wave-6 #4 type data
    u.w6.burst = c.w6.burst;                // W6-04-01: the active weapon's Burst=
    u.w6.omni_fire = c.w6.omni_fire;        // W6-04-06: ... and OmniFire=
    u.w6.sonic = c.w6.sonic;                // W6-04-09: ... and Sonic=
    u.w6.wall_flags = c.w6.wall_flags;      // W6-04-04: ... and its wall keys
    u.w_range = c.range;
    u.w_verses = c.verses;
    u.w_inf_death = c.inf_death;            // W3-B
    u.w_prone_damage = c.prone_damage;
    u.w_cell_spread = c.cell_spread;        // CB-2 [aircraft.md §4]: warhead splash
    u.w_percent_at_max = c.percent_at_max;  // radius + edge scale (0 = single-target)
    u.w_affects_allies = c.affects_allies;  // W5-B R-060
    u.speed_type = c.speed_type;            // W5-B R-062
    // W6 fix #2: rules id + targeting type data; IsInPlayfield from the cell
    // [RE: Unlimbo FUN_006F61D0 -> IsWithinUsableArea FUN_00577E90].
    u.tg.t = c.tg;
    u.tg.in_playfield = cell_in_playfield(cell_x, cell_y);
    // Projectile phase (P2) [RE: projectile.md]: the primary weapon's projectile
    // spec. Copied unconditionally but READ ONLY behind projectiles_enabled_
    // (the firewall) -- inert in every existing gate.
    u.w_proj = c.proj;
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
    // W2-AN: naval/air type data; a spawner starts with its full complement
    // and a fixed-wing aircraft with full ammo.
    u.an.t = c.an;
    u.an.spawn_ready = c.an.spawner_slot >= 0 ? c.an.spawns_number : 0;
    u.an.ammo = c.an.ammo;
    u.an.in_water = c.is_naval;
    u.an.submerged = c.an.underwater && c.an.cloakable;
    // Gap fix C [RE: docs/re/weapons.md]: the secondary slot + AA/AG flags.
    // Inert defaults for any hand-built UnitCombat (no secondary, ground-only).
    u.w2 = c.secondary;
    u.w2_elite = c.elite_secondary;
    u.p_aa = u.w_aa = c.primary_aa;
    u.p_ag = u.w_ag = c.primary_ag;
    u.p_proj = c.proj;
    u.p_proj_elite = c.elite_proj;
    u.p_proj_elite_set = c.has_elite_proj;
    u.deploy_fire = c.deploy_fire;
    u.deploy_fire_slot = c.deploy_fire_slot;
    u.crusher = c.crusher;
    u.auto_crush = c.auto_crush;   // W5-B R-050
    u.is_infantry = c.is_infantry; // W5-B R-061
    u.land_speed_loco = c.land_speed_loco;   // W5-B R-062
    u.crushable = c.crushable;
    // gF [RE: docs/re/mcv-deploy.md]: DeploysInto= -> roster index (inert -1
    // for every unit type without the key, i.e. everything but AMCV/SMCV).
    if (!c.deploys_into.empty()) {
        const std::vector<BuildingTypeDef>& bt = btypes();
        for (std::size_t k = 0; k < bt.size(); ++k)
            if (bt[k].id == c.deploys_into) { u.deploys_into = static_cast<int>(k); break; }
    }
    u.p_wall = c.primary_wall;                 // G16 (agent G)
    u.occupier = c.occupier;
    u.occupy_w = c.occupy_weapon;
    u.occupy_w_elite = c.elite_occupy_weapon;
    u.shrapnel_count = c.shrapnel_count;          // W2-SK-22
    u.shrapnel_damage = c.shrapnel_damage;
    u.shrapnel_range = c.shrapnel_range;
    u.shrapnel_verses = c.shrapnel_verses;
    u.disguise_when_still = c.disguise_when_still;
    // --- P5 Veterancy + Crates [RE: veterancy.md] --- copy the type data and
    // snapshot the base stats ALWAYS (inert copies: nothing reads Unit::vet
    // past this point unless vet_any_). Only a vet-enabled World arms the
    // combat hook, applies a requested rank and refreshes the stats.
    u.vet.cost = c.cost;
    u.vet.trainable = c.trainable;
    u.vet.dont_score = c.dont_score;
    u.vet.self_healing = c.self_healing;
    u.vet.vet_abilities = c.vet_abilities;
    u.vet.elite_abilities = c.elite_abilities;
    u.vet.rules_speed = c.rules_speed;
    u.vet.base.damage = c.damage;
    u.vet.base.rof = c.rof;
    u.vet.base.range = c.range;
    u.vet.base.verses = c.verses;
    u.vet.base.cell_spread = c.cell_spread;
    u.vet.base.percent_at_max = c.percent_at_max;
    u.vet.base.affects_allies = c.affects_allies;
    u.vet.base.inf_death = c.inf_death;     // W3-B
    u.vet.base.prone_damage = c.prone_damage;
    u.vet.base.burst = c.w6.burst;          // W6-04-01
    u.vet.base.omni_fire = c.w6.omni_fire;  // W6-04-06
    u.vet.base.sonic = c.w6.sonic;          // W6-04-09
    u.vet.base.wall_flags = c.w6.wall_flags;   // W6-04-04
    u.vet.base.shrapnel_count = c.shrapnel_count;   // W6-04-02
    u.vet.base.shrapnel_damage = c.shrapnel_damage;
    u.vet.base.shrapnel_range = c.shrapnel_range;
    u.vet.base.shrapnel_verses = c.shrapnel_verses;
    u.vet.has_elite = c.has_elite;
    u.vet.elite = c.elite;
    u.vet.base_speed_cps = c.speed_cps;
    u.vet.base_sight = c.sight;
    if (vet_any_) {
        // [V15] SetVeteran only if Trainable (FUN_00734490 L107-111).
        if (c.veteran_rank > 0 && c.trainable)
            u.vet.veterancy = c.veteran_rank >= 2 ? 2.0f : 1.0f;
        init_unit_vet(u);
    }
    // P4 [RE: abilities.md]: bind the ability profile at spawn so the unit's
    // very first shot is already intercepted. A strict no-op unless
    // abilities_any_ (no pre-P4 gate registers a profile).
    if (abilities_any_) ability_bind_unit(u);
    subcell_register(static_cast<int>(&u - units_.data()));   // W5-B R-061
    return u;
}

void World::set_anim(const std::string& image, const UnitAnimSeq& seq) { anims_[image] = seq; }

void World::set_water_terrain(const std::vector<ra2assets::LandType>& land_types) {
    // [RE-GATE APPLIED: naval.md §1] intersect with the Land grid (passable_) so
    // a ship never paths on a non-materialized water cell.
    water_passable_ = build_water_passability(land_types, passable_, grid_w_, grid_h_);
}

static bool land_type_blocks_ground(ra2assets::LandType t) {
    // [RE: naval.md §2 Ground[LandType][SpeedType]; rulesmd [Water]/[Rock]/
    // [Wall] Foot=Track=Wheel=0%] -- the three ground SpeedTypes agree on
    // every row, so one Land grid serves Foot/Track/Wheel.
    // [Beach] is also 0% for them, but the clone's LandType is a per-TILE-SET
    // classification (land_type.hpp): the ShorePieces set mixes land and water
    // sub-tiles, so blocking it would wall off coasts the original lets units
    // walk. Shore stays passable [RE-FALLBACK: per-sub-tile land type].
    return t == ra2assets::LandType::Water || t == ra2assets::LandType::Rock ||
           t == ra2assets::LandType::Wall;
}

void World::set_land_terrain(const std::vector<ra2assets::LandType>& land_types) {
    // Water grid first (it is the materialized-Land intersection), then close
    // the Land grid on water / cliffs (CliffSet + WaterCliffs -> Rock) / walls.
    set_water_terrain(land_types);
    const std::size_t n = std::min(land_types.size(), passable_.size());
    for (std::size_t i = 0; i < n; ++i)
        if (land_type_blocks_ground(land_types[i])) passable_[i] = 0;
}

void World::block_water_footprint(int cx, int cy, int fw, int fh) {
    if (water_passable_.empty()) return;
    for (const Cellxy& c : foundation_cells(cx, cy, fw, fh)) {
        if (c.x < 0 || c.y < 0 || c.x >= grid_w_ || c.y >= grid_h_) continue;
        water_passable_[static_cast<std::size_t>(c.y) * grid_w_ + c.x] = 0;
    }
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
    if (house < 0 || house >= house_count()) return 0.0;   // T3: was house > 1
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

void World::group_harvest(int issuer, const std::vector<int>& ids, int x, int y) {
    for (int i : ids) {
        if (i < 0 || i >= static_cast<int>(units_.size())) continue;
        Unit& u = units_[static_cast<std::size_t>(i)];
        // Player orders act on the issuer's OWN harvesters only (not its
        // PlayerControl houses' -- the pre-L1 house-0 gate).
        if (!u.is_harvester || u.house != issuer || !u.alive || u.die_time >= 0.0)
            continue;
        u.harv_cell_x = x;
        u.harv_cell_y = y;
        u.has_harv_cell = true;  // a player-directed cell is a real stamp too
        u.harv = HarvState::MoveToOre;
        u.harv_timer = 0.0;
        u.manual_hold = false;
        u.enter_target = -1;  // CB-1: a harvest order supersedes an enter order
        order_unit_move(i, x, y);
    }
}

void World::group_return(int issuer, const std::vector<int>& ids) {
    for (int i : ids) {
        if (i < 0 || i >= static_cast<int>(units_.size())) continue;
        Unit& u = units_[static_cast<std::size_t>(i)];
        // Same own-house gate as group_harvest above.
        if (!u.is_harvester || u.house != issuer || !u.alive || u.die_time >= 0.0)
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
    u.tg.area_guard = false;   // W6-02-11: a move order leaves Area_Guard
    // P4 [RE: abilities.md §C1/§C2/§A]: a FROZEN unit (warping in, being
    // erased, deployed) takes no move; a Teleporter's move is a TELEPORT.
    // Both fields are inert (false) unless a P4 profile is bound, which no
    // pre-P4 gate does -> the existing body below runs unchanged.
    if (u.frozen) return;
    // Gap fix C (G-45): a deployed DeployFire infantry packs up and walks
    // (the original undeploys on a move order).
    if (u.deploy_fire && u.deployed) u.deployed = false;
    if (u.abil.teleporter && !u.is_aircraft) {
        teleport_unit(idx, gx, gy);
        return;
    }
    // W2-AN-02: a fixed-wing aircraft flies a sortie to the cell (then RTB).
    if (u.an.t.fixed_wing) {
        if (!u.an.t.spawned) order_fixed_wing(idx, -1, -1, gx, gy);
        return;
    }
    // CB-2 Aircraft [RE-GATE APPLIED: aircraft.md §1/§6]: an aircraft order sets
    // a FLIGHT GOAL and takes off/cruises -- it never computes an A* path, never
    // tests passable_, and reserves no occ_ cell while airborne (straight-line
    // unpathed flight CONFIRMED; no RNG on the path). The ONE
    // aircraft-aware branch in order_unit_move; ground units (is_aircraft
    // false in every existing demo) take the exact existing path below.
    if (u.is_aircraft) {
        aircraft_any_ = true;                        // open the step_flight firewall
        u.an.landing = false;                        // W2-AN-08: a new goal ends a landing
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
    // W2-SK-05/16: a ground (Land-domain) unit ordered onto an impassable or
    // unreachable cell walks to the nearest reachable one (budgeted search);
    // ships keep the exact search (a land click is refused, naval.md).
    const bool crush = crush_paths(u);   // P4: a crusher routes over crushable walls
    // R-210: the height gate, high-bridge decks and allied gates (null = 2-D).
    sync_deck_layer(u);
    PathTerrain terr;
    std::vector<int> gates;
    const PathTerrain* tp = terrain_for(u, terr, gates);
    std::vector<Cellxy> path =
        u.movement_domain == MovementDomain::Land
            ? find_path_nearest_t(crush ? crush_grid() : grid_for(u.movement_domain), grid_w_, grid_h_,
                                  {u.cell_x, u.cell_y}, {gx, gy}, kNearestPathBudget,
                                  crush ? crush_leak() : wall_leak(), tp)
            : find_path_t(grid_for(u.movement_domain), grid_w_, grid_h_, {u.cell_x, u.cell_y}, {gx, gy},
                          u.movement_domain == MovementDomain::Water ? nullptr : wall_leak(), tp);
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

void World::group_move(int issuer, const std::vector<int>& ids, int goal_x, int goal_y) {
    std::vector<int> sel;
    std::vector<Cellxy> cur;
    for (int i : ids) {
        if (i < 0 || i >= static_cast<int>(units_.size())) continue;
        const Unit& u = units_[static_cast<std::size_t>(i)];
        if (u.alive && u.die_time < 0.0 && can_command(issuer, u.house)) {
            sel.push_back(i);
            cur.push_back({u.cell_x, u.cell_y});
        }
    }
    if (sel.empty()) return;

    // W2-AN-09: one formation per movement domain -- ships spread over the
    // water grid, amphibious craft over land OR water, aircraft anywhere, the
    // rest over the land grid (the pre-fix single call, byte-identical for an
    // all-ground selection).
    std::vector<Cellxy> targets(sel.size(), Cellxy{goal_x, goal_y});
    {
        std::vector<std::uint8_t> air_grid;
        for (int dom = 0; dom < 4; ++dom) {
            std::vector<std::size_t> ks;
            std::vector<Cellxy> dcur;
            for (std::size_t k = 0; k < sel.size(); ++k) {
                const Unit& u = units_[sel[k]];
                const int d = u.is_aircraft ? 3 : static_cast<int>(u.movement_domain);
                if (d != dom) continue;
                ks.push_back(k);
                dcur.push_back(cur[k]);
            }
            if (ks.empty()) continue;
            const std::vector<std::uint8_t>* g = &passable_;
            if (dom == 1) g = &water_passable_;
            else if (dom == 2) g = &amphib_grid();
            else if (dom == 3) {
                air_grid.assign(static_cast<std::size_t>(grid_w_) * grid_h_, 1);
                g = &air_grid;
            }
            if (dom == 0 && occ_.subcells() && !g->empty()) {
                // W5-B R-061: vehicles take whole cells first; the infantry then
                // pack three to a cell around the goal (sub-cell spots).
                std::vector<std::size_t> vk, ik;
                std::vector<Cellxy> vcur, icur;
                std::vector<int> iid;
                for (std::size_t j = 0; j < ks.size(); ++j) {
                    const int ui = sel[ks[j]];
                    if (occ_.is_infantry(ui)) { ik.push_back(ks[j]); icur.push_back(dcur[j]); iid.push_back(ui); }
                    else { vk.push_back(ks[j]); vcur.push_back(dcur[j]); }
                }
                std::vector<Cellxy> vt;
                if (!vk.empty()) vt = assign_formation({goal_x, goal_y}, vcur, *g, grid_w_, grid_h_, occ_);
                for (std::size_t j = 0; j < vk.size(); ++j) targets[vk[j]] = vt[j];
                if (!ik.empty()) {
                    const std::vector<Cellxy> it =
                        assign_infantry_formation({goal_x, goal_y}, icur, iid, *g, grid_w_, grid_h_, occ_, vt);
                    for (std::size_t j = 0; j < ik.size(); ++j) targets[ik[j]] = it[j];
                }
                continue;
            }
            const std::vector<Cellxy> t =
                (g->empty()) ? dcur : assign_formation({goal_x, goal_y}, dcur, *g, grid_w_, grid_h_, occ_);
            for (std::size_t j = 0; j < ks.size(); ++j) targets[ks[j]] = t[j];
        }
    }

    for (std::size_t k = 0; k < sel.size(); ++k) {
        Unit& u = units_[sel[k]];
        u.attack_cell_x = u.attack_cell_y = -1;   // G16: a move ends a ground fire order
        u.occupy_target = -1;
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

void World::group_attack(int issuer, const std::vector<int>& ids, int target_idx) {
    if (target_idx < 0 || target_idx >= static_cast<int>(units_.size())) return;
    const Unit& tgt = units_[target_idx];
    // Can't order an attack on a dead/dying target, or on a unit the issuer
    // commands itself (that would mean "attack a friendly").
    if (!tgt.alive || tgt.die_time >= 0.0 || can_command(issuer, tgt.house)) return;   // W5-A: any player-commanded house

    for (int ii : ids) {
        if (ii < 0 || ii >= static_cast<int>(units_.size())) continue;
        const std::size_t i = static_cast<std::size_t>(ii);
        Unit& u = units_[i];
        if (!can_command(issuer, u.house) || !u.alive || u.die_time >= 0.0) continue;   // W5-A: PlayerControl houses too
        // W2-SK-22 Spy disguise [RE: capture.md §C; [SPY] Primary=MakeupKit
        // "virtual weapon that picks disguise"]: a spy "attacking" an enemy
        // infantry takes on its look instead.
        if (u.image == "SPY") {
            if (!tgt.is_vehicle && !tgt.is_aircraft) set_disguise(static_cast<int>(i), tgt.image, tgt.house);
            continue;
        }
        u.attack_cell_x = u.attack_cell_y = -1;   // G16
        // Player guard kept above (selected + house 0); the shared mutation
        // tail (target/attack_order + occupancy re-assert + path clear) is
        // extracted into commit_attack_target so there is ONE copy of the
        // target-commit body [RE-GATE APPLIED: ai-team.md §5].
        commit_attack_target(static_cast<int>(i), target_idx, /*target_building=*/-1);
    }
}

int World::add_building(int type, int cx, int cy, int fw, int fh, int house, int strength,
                        int armor_ix, double buildup_total, bool instant, int sight,
                        bool grants_map_reveal) {
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
    b.grants_map_reveal = grants_map_reveal;  // [RE: shroud.md §7A] SpySat/GPS reveal-all flag
    b.buildup_total = buildup_total;
    b.state = instant ? Building::State::Idle : Building::State::Buildup;
    b.vet_recv = vet_any_;   // P5 [RE: veterancy.md V6]: false in every legacy World
    b.tg.in_playfield = cell_in_playfield(cx + fw / 2, cy + fh / 2);   // W6 fix #2 (Unlimbo)
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

void World::group_attack_building(int issuer, const std::vector<int>& ids, int b_idx) {
    if (b_idx < 0 || b_idx >= static_cast<int>(buildings_.size())) return;
    const Building& tgt = buildings_[b_idx];
    // Can't order an attack on a dead/Dead-state/own (friendly) building
    // -- mirrors group_attack's own guard against a friendly target.
    if (!tgt.alive || tgt.state == Building::State::Dead || can_command(issuer, tgt.house)) return;   // W5-A

    for (int ii : ids) {
        if (ii < 0 || ii >= static_cast<int>(units_.size())) continue;
        const std::size_t i = static_cast<std::size_t>(ii);
        Unit& u = units_[i];
        if (!can_command(issuer, u.house) || !u.alive || u.die_time >= 0.0) continue;   // W5-A: PlayerControl houses too
        u.attack_cell_x = u.attack_cell_y = -1;   // G16
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
    // Gap owner A (G-24) [RE: roster2.md TL;DR 8]: a ship leaves its Naval Yard
    // onto the first free WATER ring cell (roster2's naval_exit_cell); every
    // other unit keeps the land walk below.
    // W2-AN-10: an amphibious craft (LCRF/SAPC, built at the yard) also
    // launches onto the water ring.
    // W4-MC-01: only a craft built at a NAVAL factory takes the water ring; an
    // amphibious infantry (Tanya/SEAL from the barracks) walks out on land.
    if (c.is_naval || (c.movement_domain == MovementDomain::Amphibious &&
                       factory_def.factory_kind == 3)) {
        if (water_passable_.empty()) return -1;
        const Cellxy wx = naval_exit_cell(f, factory_def, water_passable_, grid_w_, grid_h_,
                                          [this](int x, int y) {
                                              return occ_.is_free(x, y) && building_at_cell(x, y) < 0;
                                          });
        if (wx.x < 0) return -1;                    // boxed in: HOLD
        UnitCombat cv = c;
        cv.veteran_rank = produced_veteran_kind(f.house, factory_def.factory_kind) ? 1 : 0;
        const bool has_rally = f.has_rally;
        const int rx = f.rally_x, ry = f.rally_y;
        add_combat_unit(wx.x, wx.y, f.house, cv);
        const int ui = static_cast<int>(units_.size()) - 1;
        if (has_rally) order_unit_move(ui, rx, ry);
        return ui;
    }
    // W2-AN-01/02 [RE: roster2.md §C]: a fixed-wing aircraft is built ONTO a
    // free dock pad of its Helipad factory (GAAIRC/AMRADR: pad k = centre +
    // DockingOffset[k]); with every pad taken the queue HOLDS.
    if (c.an.fixed_wing) {
        int pad = -1;
        for (int k = 0; k < static_cast<int>(an_.dock_offsets.size()) && pad < 0; ++k) {
            bool used = false;
            for (const Unit& o : units_)
                if (o.alive && o.die_time < 0.0 && o.an.home == factory_idx && o.an.pad == k) used = true;
            if (!used) pad = k;
        }
        if (pad < 0 && !an_.dock_offsets.empty()) return -1;
        const double px = (f.cx * 256.0 + f.fw * 128.0 + (pad >= 0 ? an_.dock_offsets[pad].first : 0)) / 256.0 - 0.5;
        const double py = (f.cy * 256.0 + f.fh * 128.0 + (pad >= 0 ? an_.dock_offsets[pad].second : 0)) / 256.0 - 0.5;
        UnitCombat cv = c;
        cv.veteran_rank = produced_veteran(f.house) ? 1 : 0;
        const int house = f.house;
        add_combat_unit(static_cast<int>(std::lround(px)), static_cast<int>(std::lround(py)), house, cv);
        const int ui = static_cast<int>(units_.size()) - 1;
        occ_.release_all_of(ui, -1, -1);   // parked on the pad: no ground cell
        Unit& a = units_[ui];
        a.fx = px;
        a.fy = py;
        a.an.home = factory_idx;
        a.an.pad = pad;
        a.an.sortie = 0;
        an_.any = true;
        return ui;
    }
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
    cv.veteran_rank = produced_veteran_kind(house, factory_def.factory_kind) ? 1 : 0;
    add_combat_unit(ex.x, ex.y, house, cv);
    const int ui = static_cast<int>(units_.size()) - 1;
    // W2-AN-07: a jumpjet (Kirov, Rocketeer, Night Hawk) lifts off as it
    // leaves the factory instead of standing on the apron as a ground target.
    if (c.is_aircraft) {
        aircraft_any_ = true;
        occ_.release_all_of(ui, -1, -1);
        units_[ui].flight_state = FlightState::TakingOff;
    }
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

void World::group_enter_transport(int issuer, const std::vector<int>& ids, int t_idx) {
    if (t_idx < 0 || t_idx >= static_cast<int>(units_.size())) return;
    const Unit& t = units_[t_idx];
    // Wave-5 (R-052): an ALLIED transport admits the player's infantry too.
    if (!t.alive || t.die_time >= 0.0 || !are_allied(issuer, t.house) || t.passengers_max <= 0) return;
    const auto allied = [this](int a, int b) { return are_allied(a, b); };
    // Gather eligible movers: selected house-0, alive, non-dying, not the
    // transport itself, passing can_load NOW (capacity re-checked at
    // boarding -- orders may oversubscribe) [spec §3].
    std::vector<int> movers;
    std::vector<Cellxy> cur;
    for (int ii : ids) {
        if (ii < 0 || ii >= static_cast<int>(units_.size())) continue;
        const std::size_t i = static_cast<std::size_t>(ii);
        const Unit& u = units_[i];
        if (!can_command(issuer, u.house) || !u.alive || u.die_time >= 0.0) continue;   // W5-A: PlayerControl houses too
        if (static_cast<int>(i) == t_idx) continue;
        if (!can_load(units_, t_idx, static_cast<int>(i), allied)) continue;
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

void World::group_unload(int issuer, const std::vector<int>& ids) {
    for (int i : ids) {
        if (i < 0 || i >= static_cast<int>(units_.size())) continue;
        Unit& u = units_[static_cast<std::size_t>(i)];
        // W4-SQ-01: any house the player commands (PlayerControl, W2-CA-02).
        if (!can_command(issuer, u.house) || !u.alive || u.die_time >= 0.0) continue;
        if (u.passengers_max <= 0 || u.cargo.empty()) continue;
        u.unload_pending = true;
        cargo_any_ = true;
    }
}

void World::order_unload(int idx) {
    if (idx < 0 || idx >= static_cast<int>(units_.size())) return;
    Unit& u = units_[idx];
    if (!u.alive || u.die_time >= 0.0 || u.passengers_max <= 0 || u.cargo.empty()) return;
    u.unload_pending = true;
    cargo_any_ = true;
}

int World::add_unit_in_transport(int t_idx, int house, const UnitCombat& c) {
    if (t_idx < 0 || t_idx >= static_cast<int>(units_.size())) return -1;
    if (!units_[t_idx].alive || units_[t_idx].die_time >= 0.0) return -1;
    const int cx = units_[t_idx].cell_x, cy = units_[t_idx].cell_y;
    const int prev = occ_.at(cx, cy);
    add_combat_unit(cx, cy, house, c);
    const int p = static_cast<int>(units_.size()) - 1;
    if (prev == Occupancy::kEmpty) occ_.clear(cx, cy);   // the passenger never claims the cell
    else occ_.set(cx, cy, prev);
    Unit& u = units_[p];
    u.fx = units_[t_idx].fx;
    u.fy = units_[t_idx].fy;
    u.cargo_of = t_idx;
    units_[t_idx].cargo.push_back(p);
    cargo_any_ = true;
    return p;
}

bool World::order_board(int p_idx, int t_idx) {
    if (p_idx < 0 || t_idx < 0 || p_idx >= static_cast<int>(units_.size()) ||
        t_idx >= static_cast<int>(units_.size()))
        return false;
    if (!can_load(units_, t_idx, p_idx)) return false;
    cargo_any_ = true;
    Unit& u = units_[p_idx];
    u.target = -1;
    u.target_building = -1;
    u.attack_order = false;
    u.unload_pending = false;
    u.enter_target = t_idx;
    const Unit& t = units_[t_idx];
    const int adx = u.cell_x - t.cell_x, ady = u.cell_y - t.cell_y;
    if (std::max(adx < 0 ? -adx : adx, ady < 0 ? -ady : ady) <= 1) return true;   // boards next step
    const std::vector<Cellxy> g = nearest_free_cells({t.cell_x, t.cell_y}, 1, passable_, grid_w_, grid_h_, occ_);
    if (!g.empty()) order_unit_move(p_idx, g[0].x, g[0].y);
    units_[p_idx].enter_target = t_idx;
    return true;
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
        // W2-AN-08: an AIRBORNE transport (Night Hawk) takes passengers only
        // once it has landed; it lands where it hovers (step_air_naval).
        if (cheb <= 1 && t.is_aircraft && t.flight_state != FlightState::Grounded) {
            if (t.an.t.landable && !t.balloon_hover && !t.an.landing) t.has_flight_goal = false;
            continue;
        }
        if (cheb <= 1) {
            // Boarding trigger point = Chebyshev adjacency [RE-GATE APPLIED:
            // transport-cargo.md §3 -- DOCUMENTED DEVIATION: the original
            // boards on CO-LOCATION (the passenger paths onto the
            // transport's own tile, FUN_004D8ED0 case 0x12); our
            // one-unit-per-cell grid has no walk-onto-occupied notion, so
            // adjacency <= 1 is the adjudicated stand-in (same reachable
            // set, deterministic)].
            if (can_load(units_, ti, static_cast<int>(i),
                         [this](int a, int b) { return are_allied(a, b); })) {   // wave-5 R-052
                // Remove-from-map: release ALL occupancy (current cell AND
                // any mid-hop reservation -- keep-cell (-1,-1) matches
                // nothing), clear path/selection/targets, THEN link [spec §3].
                occ_.release_all_of(static_cast<int>(i), -1, -1);
                u.path.clear();
                u.path_idx = 0;
                // (boarding drops the unit from every selection: Selection::prune, spec §4)
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
        // W2-AN-08: an airborne transport lands first, then unloads.
        if (t.is_aircraft && t.flight_state != FlightState::Grounded) {
            if (!t.an.t.landable || t.balloon_hover) t.unload_pending = false;   // can never land
            else if (!t.an.landing) t.has_flight_goal = false;
            continue;
        }
        // W2-AN-10: a landing craft unloads only once beached on a LAND cell
        // (the eject ring is land-only); an unload order at sea is dropped.
        if (t.movement_domain == MovementDomain::Amphibious && !walkable(t.cell_x, t.cell_y)) {
            t.unload_pending = false;
            continue;
        }
        // Wave-5 (R-051) [RE: abilities-w5.md §7, FUN_004A5150 armed from the
        // transport's TechnoType DeployTime (+0x3C8, minutes x 900)]: the
        // doors open DeployTime frames after the unload starts; passengers
        // leave once they are open.
        if (t.w5.p.deploy_frames > 0 && w5_.armed) {
            const std::int64_t now = static_cast<std::int64_t>(w5_.frame);
            if (t.w5.door_until < 0) t.w5.door_until = now + t.w5.p.deploy_frames;
            if (now < t.w5.door_until) continue;
        }
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
        if (!t.unload_pending) t.w5.door_until = -1;   // wave-5: doors shut
    }
}

void World::step_flight(double dt) {
    // FIREWALL (spec §8): until the first aircraft is ordered, this whole
    // function is one branch -- the only cost every existing demo pays.
    if (!aircraft_any_) return;

    for (Unit& u : units_) {
        if (!u.is_aircraft) continue;
        if (!u.alive || u.die_time >= 0.0) continue;
        if (u.an.t.fixed_wing || u.an.t.missile) continue;   // W2-AN: world_an.cpp flies these
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

void World::set_ai_base_nodes(int house, const std::vector<PlannedNode>& nodes) {
    if (!ai_enabled(house) || nodes.empty()) return;
    HouseAiRec& rec = ai_houses_[house];
    rec.state.plan = nodes;
    rec.state.next_node_id = 0;
    for (int i = 0; i < (int)rec.state.plan.size(); ++i) {
        PlannedNode& n = rec.state.plan[i];
        n.id = i;
        n.building = -1;
        n.dead = false;
        rec.state.next_node_id = i + 1;
    }
    // Standing-building sync (the same linkage step_ai_houses makes for a
    // catalog plan): ascending building index, nearest unclaimed node of
    // the same type. A node whose building is not standing is BUILT at its
    // stored cell (ai_place_ready re-validates it) -- the campaign base
    // build-out / rebuild.
    for (int bi = 0; bi < (int)buildings_.size(); ++bi) {
        const Building& b = buildings_[bi];
        if (!b.alive || b.state == Building::State::Dead || b.house != house) continue;
        int best = -1;
        long long bd = 0;
        for (int i = 0; i < (int)rec.state.plan.size(); ++i) {
            const PlannedNode& n = rec.state.plan[i];
            if (n.type != b.type || n.building >= 0) continue;
            const long long dx = n.cx - b.cx, dy = n.cy - b.cy, d = dx * dx + dy * dy;
            if (best < 0 || d < bd) { best = i; bd = d; }
        }
        if (best >= 0) {
            rec.state.plan[best].building = bi;
            rec.state.plan[best].cx = b.cx;
            rec.state.plan[best].cy = b.cy;
        }
    }
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

// --- CB-4 Superweapons: moved to world_sw.cpp (W2-AN-04 multi-SW v2) ---

bool World::house_low_power(int house) const {
    const PowerTotals p = apply_blackout(power_totals(buildings_, btypes(), house),
                                         house_blacked_out(house));
    return p.output < p.drain;   // [RE: construction.md §8] low power = output < drain
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
    // W2-SK-21: "neutral" is the real neutral house (the lobby appends it
    // LAST; kNeutralHouse=2 is an AI in 3+ player games), and any capturing
    // house is paid (an AI into its AI-1 purse).
    const int nh = neutral_house();
    const bool from_neutral = nh >= 0 ? old_house == nh : grants_startup_from_neutral(old_house);
    if (from_neutral && startup > 0 && new_house >= 0 && new_house < house_count()) {
        if (ai_enabled(new_house)) ai_houses_[(std::size_t)new_house].credits += (double)startup;
        else house_credits_[new_house] += (double)startup;
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
            // W2-SK-20: steal from the victim's REAL wallet, any house (an AI
            // house's AI-1 purse; a human's GameScene wallet, published every
            // frame through set_wallet_hint). The old code took half of the
            // per-frame income pool -- ~0 -- and only for houses 0/1.
            if (victim >= 0 && victim < house_count() && spy_house >= 0 && spy_house < house_count()) {
                const double bal = ai_enabled(victim)
                                       ? ai_houses_[(std::size_t)victim].credits
                                       : house_credits_[(std::size_t)victim] + wallet_hint(victim);
                const int steal = cash_steal_amount(std::max(0.0, bal), steal_pct);
                if (ai_enabled(victim)) ai_houses_[(std::size_t)victim].credits -= (double)steal;
                else house_credits_[(std::size_t)victim] -= (double)steal;
                if (ai_enabled(spy_house)) ai_houses_[(std::size_t)spy_house].credits += (double)steal;
                else house_credits_[(std::size_t)spy_house] += (double)steal;
            }
            break;
        }
        case InfiltratePurpose::PowerPlant:          // [RE: capture.md §B2 CONFIRMED]
            if (victim >= 0 && victim < house_count()) blackout_frames_[victim] = blackout;   // W2-SK-20: any house
            break;
        case InfiltratePurpose::Superweapon: {       // [RE: capture.md §B4 CONFIRMED]
            // Wave-5: FUN_006CD620 resets the SuperClass of THIS building's
            // SuperWeapon= type, not every superweapon the victim holds.
            const int swt = (b.type >= 0 && b.type < (int)an_.sw_btype.size()) ? an_.sw_btype[(std::size_t)b.type] : 0;
            HouseSuperweapon* r = swt > 0 ? sw_rec(victim, static_cast<SuperweaponType>(swt)) : nullptr;
            if (r) r->sw.on_fired();
            else reset_superweapon(victim);
            break;
        }
        case InfiltratePurpose::Factory: {           // [RE: capture.md §B5] Task 6 wires the flag
            // W2-SK-20: veterans only from THAT factory category (barracks ->
            // infantry, war factory -> vehicles, yard -> ships, airfield ->
            // aircraft), not every unit the house ever builds.
            int kind = 0;
            if (b.type >= 0 && b.type < (int)btypes().size()) kind = btypes()[(std::size_t)b.type].factory_kind;
            // Wave-5 [RE: abilities-w5.md §1, FUN_004571E0]: the branch keys on
            // BuildingType Factory= (+0xEB8): InfantryType (0x10) sets
            // BarracksInfiltrated (+0x2BF), UnitType (0x28) WarFactoryInfiltrated
            // (+0x2C0) -- a Naval Yard is Factory=UnitType too, so a war
            // factory and a yard set the same flag (vehicles AND ships).
            std::uint8_t bits = 0;
            if (kind == 1) bits = 1u << 1;
            else if (kind == 2 || kind == 3) bits = (1u << 2) | (1u << 3);
            else bits = (std::uint8_t)(kind > 0 ? (1u << kind) : 0x1Eu);
            if (spy_house >= 0 && spy_house < house_count())
                produced_veteran_[spy_house] |= bits;
            break;
        }
        case InfiltratePurpose::RadarLab:            // [RE: shroud.md §7B] radar sabotage
            reshroud_house(victim);                  // no-op if shroud off or SpySat-latched
            break;
        // Wave-5 (R-042) [RE: abilities-w5.md §1, FUN_004571E0 0x4572B4..]: a
        // type listed in Rules [AI] BuildTech (+0x920/+0x92C) sets the spy
        // house's Side<AIBasePlanningSide>TechInfiltrated -> the
        // RequiresStolen*Tech units become buildable for it.
        case InfiltratePurpose::StolenAllied: grant_stolen_tech(spy_house, kStolenAllied); break;
        case InfiltratePurpose::StolenSoviet: grant_stolen_tech(spy_house, kStolenSoviet); break;
        case InfiltratePurpose::StolenThird: grant_stolen_tech(spy_house, kStolenThird); break;
        case InfiltratePurpose::None:
            return false;   // W2-SK-20: a non-spyable building refuses the spy (not consumed)
    }
    return true;
}

bool World::repair_building(int b_idx, int by_house) {
    if (b_idx < 0 || b_idx >= (int)buildings_.size()) return false;
    Building& b = buildings_[b_idx];
    if (!b.alive || b.state == Building::State::Dead) return false;
    if (!are_allied(by_house, b.house)) return false;  // repair OWN + ALLIED (self-ally holds)
    if (b.hp >= b.max_hp) return false;         // full HP -> no action (engineer NOT consumed)
    capture_any_ = true;
    b.hp = b.max_hp;                            // heal to full [RE-GATE APPLIED §A5]
    b.being_repaired = false;   // off-ramp 5 [RE: repair-sell.md §A4/§A6]:
                                // Repair_To_Full (vtbl+0x40C) calls Repair(0).
                                // A no-op wherever the flag was never set, so
                                // --demo-capture stays byte-identical.
    return true;
}

bool World::set_repair(int b_idx, int by_house, int control) {
    if (b_idx < 0 || b_idx >= static_cast<int>(buildings_.size())) return false;
    Building& b = buildings_[b_idx];
    if (!b.alive || b.state == Building::State::Dead) return false;
    // Ownership [RE: repair-sell.md §C1] -- the network REPAIR event resolves
    // the target and the SELL event checks Owner->[0x30] == ev.HouseIndex.
    // OWN house only: unlike the engineer path (repair_building), the wrench is
    // not an allied action.
    if (b.house != by_house) return false;
    // btypes(): the interactive launcher populates lookup_types_, not types_
    // (types_.empty() is the economy firewall) -- see world.hpp.
    const std::vector<BuildingTypeDef>& bt = btypes();
    if (b.type < 0 || b.type >= static_cast<int>(bt.size())) return false;
    const bool cur = b.being_repaired;
    bool next = cur;
    if (control < 0) next = !cur;              // -1 toggle
    else if (control == 0) next = false;       //  0 off
    else next = true;                          //  1 on
    if (next == cur) return false;
    if (next) {
        // [RE: §A4, FUN_00452620] Only turning ON is gated -- turning OFF must
        // always work (off-ramps 2 and 5 call Repair(0) unconditionally).
        if (!can_be_click_repaired(b, bt[b.type])) return false;
        if (b.selling) return false;           // [§B2] a selling building cannot repair
        b.being_repaired = true;
        repair_sell_any_ = true;               // open the firewall
    } else {
        b.being_repaired = false;
    }
    return true;
}

void World::set_survivor_crew(int side_ix, const UnitCombat& c) {
    if (side_ix < 0 || side_ix > 3) return;
    if (static_cast<int>(survivor_crew_.size()) <= side_ix) {
        survivor_crew_.resize(side_ix + 1);
        survivor_crew_set_.resize(side_ix + 1, 0);
    }
    survivor_crew_[side_ix] = c;
    survivor_crew_set_[side_ix] = 1;
}

bool World::begin_sell(int b_idx, int by_house, bool c4_applied) {
    if (b_idx < 0 || b_idx >= static_cast<int>(buildings_.size())) return false;
    Building& b = buildings_[b_idx];
    // Ownership [RE: repair-sell.md §C1] -- the SELL event checks
    // Owner->[0x30] == ev.HouseIndex: you cannot sell someone else's building
    // even if the cursor lets you click it.
    if (b.house != by_house) return false;
    const std::vector<BuildingTypeDef>& bt = btypes();
    if (b.type < 0 || b.type >= static_cast<int>(bt.size())) return false;
    // [RE: §B5, CONFIRMED] Unsellable / under construction / already selling /
    // C4. NO last-ConYard rule and NO captured rule -- both were searched for
    // in the binary and neither exists. DO NOT ADD EITHER. (The mind-control
    // veto is deliberately omitted -- §D6.)
    // P4 [RE: abilities.md §D3]: the C4 hook is now live -- a building with C4
    // planted (Building::c4_applied, inert false pre-P4) refuses the sale.
    if (!can_be_sold(b, bt[b.type], c4_applied || b.c4_applied)) return false;
    repair_sell_any_ = true;
    // [RE: §B2, 0x00449B81] Mission_Selling's FIRST act is Repair(0).
    set_repair(b_idx, by_house, 0);
    b.selling = true;
    b.sell_time = 0.0;
    // [RE: §B5 item 6] A type with no buildup art takes the INSTANT-removal
    // branch at 0x00447115 instead of the mission (the wall/fence case).
    // [RE: §D4] Nothing else here: queue-cancel + primary-fallback ride the
    // existing building-death path (state != Idle) at completion.
    if (b.buildup_total <= 0.0) complete_sell(b_idx);
    return true;
}

void World::complete_sell(int b_idx) {
    Building& b = buildings_[b_idx];
    const BuildingTypeDef& t = btypes()[b.type];
    const int house = b.house;

    // (1) Free the footprint FIRST. World::step's own release loop only fires
    // on a building that was ALREADY Dead when the loop ran, i.e. next frame --
    // but the survivors must stand on this footprint THIS frame (§B3: they exit
    // onto the building's own foundation cells), so the same release runs here
    // and blocking_released is latched so the loop never repeats it.
    for (const Cellxy& cell : foundation_cells(b.cx, b.cy, b.fw, b.fh)) {
        if (cell.x < 0 || cell.y < 0 || cell.x >= grid_w_ || cell.y >= grid_h_) continue;
        passable_[static_cast<std::size_t>(cell.y) * grid_w_ + cell.x] = 1;
    }
    b.blocking_released = true;

    // gF [RE: repair-sell.md §B6]: a human skirmish ConYard with MCVRedeploys
    // packs back into its MCV instead of paying out (no EVA_StructureSold, no
    // survivors). Placement failure falls through to the plain refund.
    if (try_undeploy(b_idx)) {
        Building& ub = buildings_[b_idx];
        kill_building(ub);
        ub.sold = true;
        ub.selling = false;
        ub.being_repaired = false;
        return;
    }

    // (2) The refund [RE: §B1, CONFIRMED]. FLAT -- no health term anywhere.
    // RefundPercent applies ONLY to a human-controlled house; an AI house gets
    // the UNSCALED value. Soylent overrides (§D0.2). The two cost multipliers
    // are the FactoryPlant/difficulty modifiers, 1.0 in this engine.
    const bool human = !ai_enabled(house);
    const int refund = sell_refund(t.cost, t.soylent, repair_rules_.refund_percent, human);

    // (3) Survivors [RE: §B3, CONFIRMED]: clamp(refund / divisor, 1, 5), zero
    // when the type is not Crewed. They exit onto the foundation cells in
    // foundation_cells order; a survivor whose cell is blocked is DELETED, not
    // displaced (the recovered behaviour, not a shortcut).
    // [RE-FALLBACK: captured-flag plumbing -- capture_building records no
    // captured flag on the building, so the x2 divisor is unit-tested in
    // repair_sell but never triggered here.]
    const int side_ix = static_cast<int>(house_side(house));
    const int n = survivor_count(refund, survivor_divisor(repair_rules_, side_ix), t.crewed,
                                 /*has_been_captured=*/false);
    const int cx = b.cx, cy = b.cy, fw = b.fw, fh = b.fh;
    if (n > 0 && side_ix >= 0 && side_ix < static_cast<int>(survivor_crew_.size()) &&
        survivor_crew_set_[side_ix]) {
        const UnitCombat crew = survivor_crew_[side_ix];
        int placed = 0;
        for (const Cellxy& cell : foundation_cells(cx, cy, fw, fh)) {
            if (placed >= n) break;
            // occ_.is_free (not unit_at_cell) so a mid-hop unit's reserved next
            // cell is respected -- the launch_unit free_cell rule.
            if (!walkable(cell.x, cell.y) || !occ_.is_free(cell.x, cell.y)) continue;
            add_combat_unit(cell.x, cell.y, house, crew);
            ++placed;
        }
    }

    // (4) Pay, then remove. Re-fetch the building (robust to any future
    // container change in add_combat_unit).
    Building& bb = buildings_[b_idx];
    if (house >= 0 && house < house_count())
        house_credits_[house] += static_cast<double>(refund);
    kill_building(bb);          // Dead + die_time 0; step_buildings tombstones it
    bb.sold = true;             // suppresses the death explosion (sold, not destroyed)
    bb.selling = false;
    bb.being_repaired = false;
}

void World::step_repair_sell(double dt) {
    // The cadence counter advances UNCONDITIONALLY, above the firewall, so the
    // phase is ABSOLUTE [RE: repair-sell.md §A2 -- `Frame % interval`, not a
    // per-building timer]. Incrementing a counter nothing hashes is byte-safe.
    const std::uint64_t frame = repair_frame_++;
    // FIREWALL (spec §Global Constraints): until the first repair/sell order
    // ever, this whole pass is one branch -- the only cost every existing demo
    // pays. world_hash is untouched either way.
    if (!repair_sell_any_) return;

    // --- Sell missions [RE: repair-sell.md §B2]: the buildup runs BACKWARDS
    // over the same frame count at the same rate, and ONLY on completion do the
    // credits land and the survivors spawn. Ascending index, deterministic.
    // Indexed (not range-for) because complete_sell spawns units and re-reads
    // buildings_ inside it. Runs every frame (NOT behind the repair cadence).
    for (int i = 0; i < static_cast<int>(buildings_.size()); ++i) {
        Building& b = buildings_[i];
        if (!b.alive || !b.selling) continue;
        if (b.state == Building::State::Dead) { b.selling = false; continue; }
        b.sell_time += dt;
        // sell duration == buildup_total: image_frames * max(1,
        // trunc(BuildupTime*900/image_frames)) / 15 -- the SAME value
        // main.cpp already computed for construction (building-render.md §5).
        if (b.sell_time >= b.buildup_total) complete_sell(i);
    }

    // [RE: §A2] interval = trunc(RepairRate * 900) = 14 at .016 -- a GLOBAL
    // phase shared by every repairing building of every house.
    const int interval = repair_interval_frames(repair_rules_.repair_rate);
    if (!repair_due(frame, interval)) return;

    // Ascending building index -- deterministic, and it matches the original's
    // object-order walk (one shared Available_Money() consulted per building,
    // so an early building CAN drain the pool and switch a later one off the
    // same frame; §A3).
    for (Building& b : buildings_) {
        if (!b.alive || b.state == Building::State::Dead) continue;
        if (!b.being_repaired) continue;
        if (b.selling) { b.being_repaired = false; continue; }   // [§B2 entry: Repair(0)]
        if (b.type < 0 || b.type >= static_cast<int>(btypes().size())) {
            b.being_repaired = false;
            continue;
        }
        const BuildingTypeDef& t = btypes()[b.type];
        // [RE: §A2, FUN_00711470] BOTH divisions integer. Depends only on the
        // type, so this is a pure function of (Cost, Strength, rules).
        const int cost =
            repair_step_cost(t.cost, t.strength, repair_rules_.repair_step,
                             repair_rules_.repair_percent);
        if (b.house < 0 || b.house >= house_count()) { b.being_repaired = false; continue; }
        // [RE: §A3, CONFIRMED] The affordability check is BEFORE the spend:
        // too poor -> the flag is CLEARED, no partial step, no free repair, no
        // EVA line. The player must click the wrench again after refilling.
        // [G-65] an AI house spends from its own AI-1 purse (house_credits_
        // is drained into it every frame by take_income).
        double& pool = ai_enabled(b.house) ? ai_houses_[b.house].credits : house_credits_[b.house];
        if (pool < static_cast<double>(cost)) {
            b.being_repaired = false;
            continue;
        }
        pool -= static_cast<double>(cost);
        // [RE: §A2] +RepairStep HP flat (8), clamped to Strength. The original
        // advances Health AND EstimatedHealth; the clone has one hp field.
        b.hp += repair_rules_.repair_step;
        if (b.hp >= b.max_hp) {
            b.hp = b.max_hp;
            b.being_repaired = false;          // AUTO-STOP at full health [§A2]
        }
    }
}

int World::depot_client(int b_idx) const {
    if (b_idx < 0 || b_idx >= (int)buildings_.size()) return -1;
    const Building& b = buildings_[(std::size_t)b_idx];
    if (!b.alive || b.state != Building::State::Idle || b.type < 0 ||
        b.type >= (int)btypes().size() || !btypes()[(std::size_t)b.type].unit_repair)
        return -1;
    for (int i = 0; i < (int)units_.size(); ++i) {
        const Unit& u = units_[(std::size_t)i];
        if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0 || u.house != b.house) continue;
        if (!u.is_vehicle || u.is_aircraft || u.is_naval || u.moving()) continue;
        if (u.hp >= u.max_hp && u.parasite_on_me < 0) continue;
        const int dx = u.cell_x < b.cx ? b.cx - u.cell_x : (u.cell_x >= b.cx + b.fw ? u.cell_x - (b.cx + b.fw - 1) : 0);
        const int dy = u.cell_y < b.cy ? b.cy - u.cell_y : (u.cell_y >= b.cy + b.fh ? u.cell_y - (b.cy + b.fh - 1) : 0);
        if (std::max(dx, dy) <= 1) return i;
    }
    return -1;
}

bool World::depot_servicing(int house) const {
    for (int bi = 0; bi < (int)buildings_.size(); ++bi)
        if (buildings_[(std::size_t)bi].house == house && depot_client(bi) >= 0) return true;
    return false;
}

void World::step_service_depots() {
    const std::uint64_t frame = repair_frame_ > 0 ? repair_frame_ - 1 : 0;   // step_repair_sell's phase
    const int interval = repair_interval_frames(repair_rules_.repair_rate);
    const bool due = repair_due(frame, interval);
    for (int bi = 0; bi < (int)buildings_.size(); ++bi) {
        const int ui = depot_client(bi);
        if (ui < 0) continue;
        Unit& u = units_[(std::size_t)ui];
        // A riding Terror Drone dies at the depot (every frame, not phased).
        if (u.parasite_on_me >= 0 && u.parasite_on_me < (int)units_.size()) {
            Unit& d = units_[(std::size_t)u.parasite_on_me];
            if (d.alive && d.die_time < 0.0) ability_kill_unit(u.parasite_on_me);
            u.parasite_on_me = -1;
        }
        if (!due || u.hp >= u.max_hp) continue;
        const int h = buildings_[(std::size_t)bi].house;
        if (h < 0 || h >= house_count()) continue;
        const int cost = repair_step_cost(u.vet.cost > 0 ? u.vet.cost : u.max_hp, u.max_hp,
                                          repair_rules_.repair_step, repair_rules_.repair_percent);
        double& pool = ai_enabled(h) ? ai_houses_[(std::size_t)h].credits : house_credits_[(std::size_t)h];
        if (pool < (double)cost) continue;   // broke: no partial step, no free repair
        pool -= (double)cost;
        u.hp = std::min(u.max_hp, u.hp + repair_rules_.repair_step);
    }
}

// W2-SK-22 Mirage Tank [RE: rulesmd [MGTK] CanDisguise=yes DisguiseWhenStill=yes;
// [General] DefaultMirageDisguises=TREE01..]: a Mirage that has stood still
// and not fired for a moment turns into a tree -- enemies do not auto-acquire
// it (the disguise skip in step_combat) -- and drops the disguise the moment
// it moves or fires. [RE-FALLBACK: 15 frames still. PARTIAL: it is still
// drawn as the tank -- the tree is terrain art the unit renderer cannot
// draw.] Dogs (DetectDisguise) still reveal it.
void World::step_disguises() {
    for (Unit& u : units_) {
        if (!u.disguise_when_still || !u.alive || u.die_time >= 0.0) continue;
        if (u.moving() || u.fire_flash > 0.0 || u.cargo_of >= 0) {
            u.still_frames = 0;
            if (u.disguised) u.disguised = false;
            continue;
        }
        if (++u.still_frames >= 15 && !u.disguised) {
            u.disguised = true;
            // Drawn as itself (the tree art is terrain, not a unit SHP).
            u.disguised_as_type = u.image;
            u.disguised_as_house = u.house;
            // Wave-5 (R-047) [RE: abilities-w5.md §9, UnitClass::UpdateDisguise
            // FUN_007456F0 0x745870]: DisguisedAsType = DefaultMirageDisguises
            // [Random(0, n-1)] (a TerrainType), DisguisedAsHouse = none. The
            // presentation (agent D) draws the tree for enemies.
            if (w5_.armed && !w5_.rules.mirage_trees.empty()) {
                const int n = (int)w5_.rules.mirage_trees.size();
                // [RE-FALLBACK: a per-unit hash stands in for the scenario Random draw.]
                const int k = std::min(n - 1, static_cast<int>(((std::uint32_t)(&u - units_.data()) * 2654435761u) % (std::uint32_t)n));
                // Exposed in Unit::w5.mirage_tree (disguised_as_type keeps the
                // tank image so today's draw path still finds an SHP).
                u.w5.mirage_tree = w5_.rules.mirage_trees[(std::size_t)k];
            }
            capture_any_ = true;   // arm the DetectDisguise pass
        }
    }
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
                if (b.type >= 0 && b.type < static_cast<int>(btypes().size()) &&
                    btypes()[b.type].bridge_repair_hut) {
                    // G16 (agent G) [RE: world-depth.md §B4, FUN_00519449]: an engineer
                    // entering a BridgeRepairHut repairs the bridge and is consumed;
                    // the hut itself is never captured.
                    repair_bridge_near(bi);
                    consumed = true;
                    note_entered(EnteredEvent::BridgeHut, (int)i, bi, eng_house, true);
                } else if (!are_allied(eng_house, b.house) && !building_capturable(bi)) {
                    // W2-SK-12 [RE: capture.md §A3]: Capturable=false (pillbox,
                    // tesla coil, prism tower, SAM, superweapons...) refuses the
                    // engineer, who is NOT consumed.
                    consumed = false;
                } else if (!are_allied(eng_house, b.house)) {
                    // Capture ENEMY *and NEUTRAL* buildings (neutral is not an
                    // enemy but IS capturable -- the oil-derrick case); own/ally
                    // -> repair. are_allied(eng, neutral) is false -> captured.
                    // ProduceCashStartup for the captured type (CAOILD=1000; 0 for
                    // normal buildings) [RE: capture.md §A3/§D1]. capture_building
                    // only grants it when captured FROM neutral, so this is inert
                    // for enemy captures.
                    const int startup = (b.type >= 0 && b.type < (int)types_.size())
                                            ? types_[b.type].produce_cash_startup : 0;
                    consumed = capture_building(bi, eng_house, startup);         // enemy/neutral
                    if (consumed) note_entered(EnteredEvent::Capture, (int)i, bi, eng_house, true);
                }
                else {
                    consumed = repair_building(bi, eng_house);                   // own damaged
                    if (consumed) note_entered(EnteredEvent::Repair, (int)i, bi, eng_house, true);
                }
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
                if (acted) note_entered(EnteredEvent::Infiltrate, (int)i, bi, spy_house, true);
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
        // W2-SK-21: the real neutral house pays nobody; every other house is
        // paid (was: houses 0/1 only, so a 3rd/4th player's derrick paid 0).
        const int nh = neutral_house();
        if (b.house < 0 || b.house >= house_count() || (nh >= 0 ? b.house == nh : b.house >= 2)) continue;
        t.frames += frames;
        while (t.frames >= t.delay) {
            t.frames -= t.delay;
            house_credits_[b.house] += (double)t.amount;
        }
    }
    // 2) Blackout decay (index 2 inert -- never armed).
    for (int h = 0; h < house_count(); ++h)
        if (blackout_frames_[h] > 0) blackout_frames_[h] = std::max(0, blackout_frames_[h] - frames);
    // 3) Disguise detection [RE: capture.md §C4 CONFIRMED - DetectDisguise sees
    //    through PermaDisguise; deterministic, no RNG]. A disguised unit within
    //    kDetectDisguiseRange cells of an alive, non-dying ENEMY detector is
    //    revealed (disguise cleared). Ascending index order. NO reveal-on-attack
    //    -- the spy is PermaDisguise-exempt (§C6); a dog is the only defeat.
    for (Unit& d : units_) {
        if (!d.alive || d.die_time >= 0.0 || !d.detect_disguise) continue;
        for (Unit& s : units_) {
            if (!s.disguised || !s.alive || !is_enemy(d.house, s.house)) continue;  // detected only by an ENEMY observer
            const double dx = s.fx - d.fx, dy = s.fy - d.fy;
            if (dx * dx + dy * dy <= kDetectDisguiseRange * kDetectDisguiseRange)
                s.disguised = false;
        }
    }
}

void World::enable_shroud(bool on) {
    shroud_any_ = on;
    for (int h = 0; h < house_count(); ++h) {
        if (on) {
            explored_[h].assign((size_t)grid_w_ * grid_h_, 0);
            visible_[h].assign((size_t)grid_w_ * grid_h_, 0);
        } else {
            explored_[h].clear();
            visible_[h].clear();
        }
    }
}

bool World::visible_at(int house, int x, int y) const {
    if (house < 0 || house >= house_count()) return true; // inert
    if (visible_[house].empty()) return true;             // shroud off -> all-visible
    if (x < 0 || y < 0 || x >= grid_w_ || y >= grid_h_) return false;
    return visible_[house][(size_t)y * grid_w_ + x] != 0;
}

bool World::explored_at(int house, int x, int y) const {
    if (house < 0 || house >= house_count()) return true; // inert
    if (explored_[house].empty()) return true;            // shroud off -> all-explored
    if (x < 0 || y < 0 || x >= grid_w_ || y >= grid_h_) return false;
    return explored_[house][(size_t)y * grid_w_ + x] != 0;
}

void World::reveal_source(int house, int cx, int cy, int sight) {
    if (house < 0 || house >= house_count() || visible_[house].empty()) return;
    reveal_disc(explored_[house], visible_[house], grid_w_, grid_h_, cx, cy, sight);
}

void World::reshroud_house(int h) {
    if (h < 0 || h >= house_count()) return;
    if (map_reveal_[h]) return;                 // §7B: SpySat-protected -> no sabotage
    if (!explored_[h].empty()) std::fill(explored_[h].begin(), explored_[h].end(), (std::uint8_t)0);
    if (!visible_[h].empty())  std::fill(visible_[h].begin(),  visible_[h].end(),  (std::uint8_t)0);
}

void World::reveal_all_house(int h) {
    if (h < 0 || h >= house_count()) return;
    if (!explored_[h].empty()) std::fill(explored_[h].begin(), explored_[h].end(), (std::uint8_t)1);
    if (!visible_[h].empty())  std::fill(visible_[h].begin(),  visible_[h].end(),  (std::uint8_t)1);
}

void World::step_visibility() {
    if (!shroud_any_) return;                             // firewall (spec §Global)
    // Shroud/Fog [RE: shroud.md §7A]: recompute the SpySat/GPS reveal latch from
    // each house's alive grants_map_reveal buildings.
    for (int h = 0; h < house_count(); ++h) map_reveal_[h] = false;
    for (const Building& b : buildings_) {
        if (!b.alive || b.state == Building::State::Dead) continue;
        if (b.house < 0 || b.house >= house_count() || !b.grants_map_reveal) continue;
        // Wave-5 (GASPYSAT, Powered=true): the uplink works once built and
        // only while its house has the power for it (Powered= buildings go
        // offline at low power -- the defenses.md §2 predicate).
        if (b.state != Building::State::Idle) continue;
        if (b.type >= 0 && b.type < (int)btypes().size() && btypes()[(std::size_t)b.type].powered &&
            house_low_power(b.house))
            continue;
        map_reveal_[b.house] = true;
    }
    // 1) Clear visible_ (explored_ is sticky -- fog no-regrow §4).
    for (int h = 0; h < house_count(); ++h)
        if (!visible_[h].empty())
            std::fill(visible_[h].begin(), visible_[h].end(), (std::uint8_t)0);
    // 2) Reveal from every ALIVE unit (near-circular, radius = its sight).
    for (const Unit& u : units_) {
        if (!u.alive) continue;
        reveal_source(u.house, u.cell_x, u.cell_y, u.sight);
    }
    // 3) Reveal from every ALIVE building, ONCE PER FOOTPRINT CELL (§3).
    for (const Building& b : buildings_) {
        if (!b.alive || b.state == Building::State::Dead) continue;
        for (int fy = 0; fy < b.fh; ++fy)
            for (int fx = 0; fx < b.fw; ++fx)
                reveal_source(b.house, b.cx + fx, b.cy + fy, b.sight);
    }
    // Wave-5 (R-044): enemy Gap Generators shroud their radius (world_w5.cpp).
    w5_gap_pass(0);
    // 4) Stamp each unit's seen_by_mask: bit h set iff h can see an object in its
    //    cell (object_visible_at: explored with FogOfWar off, visible with it on).
    for (Unit& u : units_) {
        if (!u.alive) continue;
        std::uint8_t m = 0;
        // T3: seen_by_mask bit-stamp stays capped at 8 combatant houses (the
        // uint8_t mask, D7) even as house_count() may exceed 3.
        for (int h = 0; h < house_count() && h < 8; ++h)
            if (object_visible_at(h, u.cell_x, u.cell_y)) m |= (std::uint8_t)(1u << h);
        u.seen_by_mask = m;
    }
    // Shroud/Fog [RE: shroud.md §7A]: a latched house sees the whole map.
    for (int h = 0; h < house_count(); ++h)
        if (map_reveal_[h]) reveal_all_house(h);
    w5_gap_pass(1);   // wave-5: a SpySat does not lift an enemy gap
    // AllyReveal [RE: multihouse.md §3, D4]: OR-merge allied houses' explored_/
    // visible_ (shroud sharing) AFTER per-house sight + seen_by_mask are computed,
    // so targeting still uses each house's OWN sight (seen_by_mask), while
    // rendering/reveal reflects shared sight. Firewalled: OFF unless alliances are
    // configured AND AllyReveal is on. Snapshot-then-merge = order-independent.
    // P5 [RE: FUN_00566fd0, the per-object sight reveal]: a source object of
    // house S reveals for the viewer V only when S == V, or S's own Allies view
    // holds V (FUN_004f9870 = S->IsAlliedWith(V)) with AllyReveal on. It is ONE
    // hop from the object's own sight this frame: an ally's ACCUMULATED explored
    // map (which already holds ITS allies' sight) is not passed on, so sight no
    // longer chains Civilian -> Other1 -> Player (SOV10 discovered BadGuy3's
    // harbour at f3 and started 'Bad Guy 3 Startup' through such a chain).
    if (houses_configured_ && ally_reveal_) {
        const int n = house_count();
        const std::vector<std::vector<std::uint8_t>> vis_src = visible_;   // own sight, this frame
        for (int a = 0; a < n; ++a) {
            if (a >= (int)explored_.size() || explored_[a].empty()) continue;
            for (int b = 0; b < n; ++b) {
                if (a == b || !are_allied(b, a)) continue;
                if (b >= (int)vis_src.size() || vis_src[b].size() != explored_[a].size()) continue;
                for (size_t i = 0; i < explored_[a].size(); ++i) {
                    explored_[a][i] |= vis_src[b][i];
                    if (!visible_[a].empty()) visible_[a][i] |= vis_src[b][i];
                }
            }
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
        } else if (is_enemy(house, b.house)) {
            // [G-67] threat / rival = ENEMY houses only: allies and the
            // neutral house never pull the defense-facing centroid or the team
            // gather ring (is_enemy is a != b in an unconfigured gate World,
            // so the 2-house AI gates are unaffected).
            // W2-SK-08: the threat centroid is the ONE current enemy's base.
            if (rec.enemy_house < 0 || b.house == rec.enemy_house) { tx += b.cx; ty += b.cy; ++tn; }
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
        if (node.type == rec.cfg.catalog.naval_yard) return ai_naval_spot_ok(house, rec, t, cx, cy);
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
    if (node.type == rec.cfg.catalog.naval_yard) block_water_footprint(px, py, t.fw, t.fh);   // W2-AN-16
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
            rec.state.plan = build_base_plan(v0, cfg.catalog, cfg.roster, cfg.ai, cfg.mods, &rec.state.rng);
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
        ai_pick_enemy_house(h, rec);   // W2-SK-08
        // W2-SK-10 base defence [RE-FALLBACK: the IsBaseDefense teams and
        // [AI] ComputerBaseDefenseResponse were not decoded]: every ~7 s, idle
        // un-teamed combat units engage enemies inside 12 cells of the base.
        if ((repair_frame_ + (std::uint64_t)h) % 105u == 0u) {
            const BaseView bv = make_ai_base_view(h, rec);
            std::vector<int> intruders;
            for (int i = 0; i < (int)units_.size(); ++i) {
                const Unit& e = units_[(std::size_t)i];
                if (!e.alive || e.die_time >= 0.0 || e.cargo_of >= 0 || e.is_aircraft || !is_enemy(h, e.house)) continue;
                const int dx = e.cell_x - bv.center_x, dy = e.cell_y - bv.center_y;
                if (dx * dx + dy * dy <= 144) intruders.push_back(i);
            }
            if (!intruders.empty())
                for (const IdleUnitView& iv : ai_idle_units(h, rec)) {
                    const Unit& u = units_[(std::size_t)iv.unit];
                    if (u.w_damage <= 0 || u.deploys_into >= 0 || u.image == "ENGINEER" || u.image == "SPY")
                        continue;
                    int best = -1;
                    long long bd = 0;
                    for (int e : intruders) {
                        const long long dx = units_[(std::size_t)e].cell_x - u.cell_x,
                                        dy = units_[(std::size_t)e].cell_y - u.cell_y;
                        if (best < 0 || dx * dx + dy * dy < bd) { best = e; bd = dx * dx + dy * dy; }
                    }
                    if (best >= 0) ai_order_attack(iv.unit, best);
                }
        }

        // 1b) [G-65] AI repair [RE: FUN_00450620, BuildingClass AI -- gated by
        //     Rules.IQ RepairSell (=1) <= house IQ (skirmish AI = MaxIQLevels
        //     5): with money >= [AI] CreditReserve and no repair started in the
        //     current window (house+0x245, re-armed by the +0x280 timer from the
        //     DifficultyClass RepairDelay), start repairing a damaged building.
        //     RE-FALLBACK: the window is exactly RepairDelay minutes (the
        //     rand(lo,hi) bounds were not decoded); "damaged" = hp < Strength;
        //     the lowest-index damaged Idle building is picked.]
        rec.state.repair_clock += dt;
        if (rec.state.repair_clock >= cfg.mods.repair_delay_min * 60.0 &&
            rec.credits >= cfg.ai.credit_reserve) {
            for (int bi = 0; bi < (int)buildings_.size(); ++bi) {
                const Building& b = buildings_[bi];
                if (!b.alive || b.house != h || b.state != Building::State::Idle ||
                    b.being_repaired || b.selling || b.hp >= b.max_hp)
                    continue;
                if (set_repair(bi, h, 1)) { rec.state.repair_clock = 0.0; break; }
            }
        }

        // 2) Node-death detection [RE: ai-base.md §5].
        for (PlannedNode& n : rec.state.plan) {
            if (n.building < 0) continue;
            const Building& b = buildings_[n.building];
            if (!b.alive || b.state == Building::State::Dead) {
                n.building = -1;
                n.dead = true;
            }
        }
        // 2b) W2-SK-07: a DEAD node whose type stands again without going
        //     through the queue (a redeployed MCV's ConYard, a captured
        //     building) is re-linked, so the head of the plan is satisfied and
        //     the base build-out resumes instead of retrying the dead node.
        for (PlannedNode& n : rec.state.plan) {
            if (!n.dead || n.type < 0) continue;
            for (int bi = 0; bi < (int)buildings_.size(); ++bi) {
                const Building& b = buildings_[(std::size_t)bi];
                if (!b.alive || b.state == Building::State::Dead || b.house != h || b.type != n.type) continue;
                bool linked = false;
                for (const PlannedNode& o : rec.state.plan)
                    if (o.building == bi) { linked = true; break; }
                if (linked) continue;
                n.building = bi;
                n.cx = b.cx;
                n.cy = b.cy;
                n.dead = false;
                break;
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
        // W2-SK-06: MultipleFactory for the AI too (it was pinned to x1.0).
        double sp[4] = {1.0, 1.0, 1.0, 1.0};
        if (cfg.multiple_factory > 0.0 && cfg.multiple_factory < 1.0) {
            int ncy = 0, ninf = 0, nveh = 0;
            for (const Building& b : buildings_) {
                if (!b.alive || b.state != Building::State::Idle || b.house != h || b.type < 0) continue;
                if (b.type < (int)cfg.roster.size() && cfg.roster[b.type].conyard) ++ncy;
                for (int t : cfg.team.inf_factory_types) if (t == b.type) { ++ninf; break; }
                for (int t : cfg.team.veh_factory_types) if (t == b.type) { ++nveh; break; }
            }
            auto spd = [&](int n) { return n <= 1 ? 1.0 : std::pow(1.0 / cfg.multiple_factory, n - 1); };
            sp[0] = sp[1] = spd(ncy);
            sp[2] = spd(ninf);
            sp[3] = spd(nveh);
        }
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
        // 6b) [G-65] AI sell [RE: FUN_00450620 -- money < [AI] CreditReserve,
        //     health ratio < ConditionRed (Rules+0x1708) and rand(0,50) <
        //     house+0x1d4 (the house TechLevel, ai-trigger.md; 10 in skirmish)
        //     -> Sell. Evaluated on the house AI tick; the conyard is never
        //     sold (the ConYard-loss path would end the house).]
        if (rec.credits < cfg.ai.credit_reserve) {
            for (int bi = 0; bi < (int)buildings_.size(); ++bi) {
                const Building& b = buildings_[bi];
                if (!b.alive || b.house != h || b.state != Building::State::Idle || b.selling ||
                    b.max_hp <= 0 || (double)b.hp / (double)b.max_hp >= cfg.ai.condition_red)
                    continue;
                if (b.type >= 0 && b.type < (int)cfg.roster.size() && cfg.roster[b.type].conyard)
                    continue;
                if ((int)(ai_rand(rec.state.rng) % 51u) >= cfg.team.trigger_tech_level) continue;
                begin_sell(bi, h);
                break;
            }
        }
        // 6c') W6-03-08 raise money [RE: FUN_004fd320 -> FUN_004fd7c0 urgency,
        //     FUN_004fdb30]: a computer house with no refinery (house+0x15c <
        //     1) that cannot afford its BuildRefinery, or with refineries but
        //     no harvester at work that cannot afford its HarvesterUnit, walks
        //     its base plan from the TAIL backwards selling each standing
        //     building (vt+0x1a0, refund vt+700) until credits + refunds
        //     exceed that cost; the ordinary rebuild/miner logic then buys it.
        //     [RE-FALLBACK: the ConYard and refineries are never sold; the
        //     refund is the AI's unscaled GetRefund (repair_sell.hpp).]
        if (rec.cfg.team.enabled) ai_raise_money(h, rec);
        // 6c) [G-66] superweapon autofire on the house AI tick (FUN_004fd320 ->
        //     FUN_00509710, IQ-gated; always on for skirmish AI).
        if (cfg.fire_superweapons) ai_try_fire_superweapon(h, rec);   // W5-A IQ gate
        if (!conyard) continue;  // no factory: nothing can start

        // 7) Decide.
        const BaseView view = make_ai_base_view(h, rec);
        BuildDecision d =
            next_build(rec.state, view, cfg.catalog, cfg.roster, cfg.ai, cfg.mods);
        // W6-03-08 [RE: FUN_004fdb30 -> house+0x564c]: the recovery refinery
        // goes ahead of the plan head (its first unsatisfied node, or a new one).
        if (rec.state.raise_building >= 0) {
            const int rt = rec.state.raise_building;
            int ni = -1;
            for (int i = 0; i < (int)rec.state.plan.size() && ni < 0; ++i) {
                const PlannedNode& n = rec.state.plan[(std::size_t)i];
                if (n.type != rt) continue;
                const bool standing = n.building >= 0 && n.building < (int)buildings_.size() &&
                                      buildings_[(std::size_t)n.building].alive &&
                                      buildings_[(std::size_t)n.building].house == h &&
                                      buildings_[(std::size_t)n.building].state != Building::State::Dead;
                if (!standing) ni = i;
            }
            if (ni < 0) {
                PlannedNode n;
                n.id = rec.state.next_node_id++;
                n.type = rt;
                rec.state.plan.push_back(n);
                ni = (int)rec.state.plan.size() - 1;
            }
            d.type = rt;
            d.node = ni;
        }
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
                if (d.type == cfg.catalog.naval_yard) return ai_naval_spot_ok(h, rec, t, cx, cy);
                const int s = cfg.ai.base_spacing;
                return placement_check(cx - s, cy - s, t.fw + 2 * s, t.fh + 2 * s,
                                       h, t.adjacent, passable_,
                                       grid_w_, grid_h_, occ_, buildings_,
                                       /*bypass_adjacency=*/true).ok;
            };
            Cellxy loc{-1, -1};
            // W2-SK-23 [RE: ai-base.md §3 FUN_0050C160 rings each defence's
            // perimeter with wall]: a wall node takes the first free cell on
            // the ring around an own base defence (no AIBaseSpacing gap), and
            // only falls back to the generic scan when every ring is closed.
            if (d.type == cfg.catalog.wall) {
                for (const Building& db : buildings_) {
                    if (loc.x >= 0) break;
                    if (!db.alive || db.house != h || db.state == Building::State::Dead) continue;
                    if (std::find(cfg.catalog.defenses.begin(), cfg.catalog.defenses.end(), db.type) ==
                        cfg.catalog.defenses.end())
                        continue;
                    for (int y = db.cy - 1; y <= db.cy + db.fh && loc.x < 0; ++y)
                        for (int x = db.cx - 1; x <= db.cx + db.fw && loc.x < 0; ++x) {
                            const bool edge = y == db.cy - 1 || y == db.cy + db.fh || x == db.cx - 1 || x == db.cx + db.fw;
                            if (!edge) continue;
                            if (placement_check(x, y, t.fw, t.fh, h, t.adjacent, passable_, grid_w_, grid_h_,
                                                occ_, buildings_, /*bypass_adjacency=*/true).ok)
                                loc = Cellxy{x, y};
                        }
                }
            }
            if (loc.x < 0) loc = find_build_location(view, is_def, can_place);
            if (loc.x < 0 && d.type == cfg.catalog.naval_yard) loc = ai_naval_spot_scan(h, rec, t);
            if (loc.x < 0 && d.type == cfg.catalog.naval_yard) {
                // W2-AN-16: no water within AINavalYardAdjacency -- drop the
                // node (a gap) so the head-only walk is never wedged on it.
                pn.type = -1;
                continue;
            }
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

bool World::ai_house_alive(int house) const {
    for (const Building& b : buildings_)
        if (b.alive && b.house == house && b.state != Building::State::Dead &&
            !building_insignificant(b))   // W6-03-01
            return true;
    for (const Unit& u : units_)
        if (u.alive && u.die_time < 0.0 && u.house == house) return true;
    return false;
}

// The base centre of house `hh` [RE: house+0x5490, set to the ConYard cell when
// it unfolds (FUN_0050a3e0); RE-FALLBACK: else its first significant building,
// else its first unit]. false = the house owns nothing.
bool World::ai_base_centre(int hh, int& x, int& y) const {
    const std::vector<BuildingTypeDef>& bts = btypes();
    bool got = false;
    for (const Building& b : buildings_) {
        if (!b.alive || b.house != hh || b.state == Building::State::Dead || building_insignificant(b))
            continue;
        const bool cy = b.type >= 0 && b.type < (int)bts.size() && bts[b.type].conyard;
        if (cy || !got) { x = b.cx; y = b.cy; got = true; if (cy) return true; }
    }
    if (got) return true;
    for (const Unit& u : units_)
        if (u.alive && u.die_time < 0.0 && u.house == hh) { x = u.cell_x; y = u.cell_y; return true; }
    return false;
}

// W2-SK-08 / W6-03-04: the ONE current Enemy per computer house
// (house+0x5600) that the trigger conditions, the threat view, the team
// targeting and the superweapon AI read.
//  * [RE: FUN_005045b0] anger: every damage a house's object takes adds
//    anger toward the attacker's house (ReceiveDamage FUN_00700da0
//    @0x7012FD, ai_note_damage); the Enemy is the living, non-allied house
//    with the highest anger.
//  * [RE: FUN_004fd320 lines 20-90] once the AIHateDelays timer (armed at the
//    game start, FUN_006862d0) has run out, a computer house in a
//    multiplayer session with no Enemy adds 1 anger toward the NEAREST
//    other living house (base centre to base centre) on every AI tick --
//    allies included, which the arg-max then filters out.
//  * [RE: FUN_004fd320] an Enemy that is defeated has its anger removed
//    and the Enemy is cleared to -1.
void World::ai_raise_money(int h, HouseAiRec& rec) {
    const AiHouseConfig& cfg = rec.cfg;
    const AiTeamConfig& tc = cfg.team;
    auto is_ref = [&](int type) {
        return std::find(refinery_types_.begin(), refinery_types_.end(), type) != refinery_types_.end();
    };
    int refineries = 0, miners = 0;
    for (const Building& b : buildings_)
        if (b.alive && b.house == h && b.state == Building::State::Idle && is_ref(b.type)) ++refineries;
    for (const Unit& u : units_)
        if (u.alive && u.die_time < 0.0 && u.house == h && u.is_harvester) ++miners;
    // Already in production: the original compares the remaining charge with
    // the money instead; the clone's queues pay as they go, so it waits.
    // A recovery target stays set (house+0x564c / +0x5650) until it stands.
    if (rec.state.raise_building >= 0 && refineries >= 1) rec.state.raise_building = -1;
    if (rec.state.raise_unit >= 0 && miners >= 1) rec.state.raise_unit = -1;
    double need = 0.0;
    int nb = -1, nu = -1;
    if (refineries < 1) {
        const int t = cfg.catalog.refinery >= 0 ? cfg.catalog.refinery : cfg.catalog.war_factory;
        if (t >= 0 && t < (int)cfg.roster.size() && rec.queues.producing(QueueCat::Structure) != t) {
            nb = t;
            need = cfg.roster[(std::size_t)t].cost * cfg.mods.cost_mul;
        }
    } else if (miners < 1) {
        for (int k = 0; k < (int)tc.unit_roster.size(); ++k)
            if (tc.unit_roster[(std::size_t)k].harvester) {
                if (rec.queues.producing(QueueCat::Vehicle) != k) {
                    nu = k;
                    need = tc.unit_roster[(std::size_t)k].cost * cfg.mods.cost_mul;
                }
                break;
            }
    }
    // FUN_004fd7c0: urgent only when the house cannot afford it.
    if (need <= 0.0 || rec.credits >= need) return;
    if (nb >= 0) rec.state.raise_building = nb;
    if (nu >= 0) rec.state.raise_unit = nu;
    const std::vector<BuildingTypeDef>& bts = btypes();
    double raised = 0.0;   // refunds still on their way count already
    for (const Building& b : buildings_)
        if (b.alive && b.house == h && b.selling && b.type >= 0 && b.type < (int)bts.size())
            raised += sell_refund(bts[(std::size_t)b.type].cost, bts[(std::size_t)b.type].soylent, 0.5, false);
    if (rec.credits + raised > need) return;
    for (int i = (int)rec.state.plan.size() - 1; i >= 0; --i) {
        const int bi = rec.state.plan[(std::size_t)i].building;
        if (bi < 0 || bi >= (int)buildings_.size()) continue;
        const Building& b = buildings_[(std::size_t)bi];
        if (!b.alive || b.house != h || b.state != Building::State::Idle || b.selling) continue;
        if (b.type < 0 || b.type >= (int)bts.size() || bts[(std::size_t)b.type].conyard || is_ref(b.type))
            continue;
        const BuildingTypeDef& t = bts[(std::size_t)b.type];
        if (!begin_sell(bi, h)) continue;
        raised += sell_refund(t.cost, t.soylent, 0.5, /*owner_is_human=*/false);
        if (rec.credits + raised > need) break;
    }
}

// W6-03-05 [RE: FUN_006ef1d0 / FUN_006ef3c0 / FUN_006ef690]: script 55-57
// line state for `house`'s Iron Curtain / Chronosphere (see TeamView).
int World::ai_team_sw_state(int house, const HouseAiRec& rec, SuperweaponType t) const {
    bool has = false;
    for (SuperweaponType k : sw_types(house)) has |= k == t;
    if (!has) return 0;
    if (sw_ready(house, t)) return house_low_power(house) ? 0 : 1;
    return (float)(1.0 - rec.cfg.ai.minor_super_ready) >= (float)(1.0 - sw_charge_fraction(house, t)) ? 2 : 0;
}

bool World::ai_house_out(int h) const {
    if (h < 0 || h >= house_count()) return true;
    if (victory_enabled_) return house_defeated(h);
    return !ai_house_alive(h);
}

void World::ai_add_anger(int house, int toward, int delta) {
    if (!ai_enabled(house)) return;
    HouseAiRec& rec = ai_houses_[house];
    const int n = house_count();
    if ((int)rec.anger.size() < n) rec.anger.resize((std::size_t)n, 0);
    if (toward >= 0 && toward < n) rec.anger[(std::size_t)toward] += delta;
    int best = 0, pick = -1;
    for (int e = 0; e < n; ++e) {
        const int a = rec.anger[(std::size_t)e];
        if (a <= best || e == house || !is_enemy(house, e) || ai_house_out(e)) continue;
        best = a;
        pick = e;
    }
    rec.enemy_house = pick;
}

void World::ai_damage_feed_cb(void* ctx, bool building, int victim, int attacker_house, int dmg) {
    static_cast<World*>(ctx)->ai_note_damage(building, victim, attacker_house, dmg);
}

// [RE: FUN_00700da0 @0x7012CD-0x7012FD] anger delta =
// ftol(type->vt+0xAC() * (damage / type+0xA0 Strength)); the +0xAC slot is
// read as the type's Cost [RE-FALLBACK: slot identity inferred; a unit whose
// type is not in the house's roster uses its strength, i.e. the damage].
void World::ai_note_damage(bool building, int victim, int attacker_house, int dmg) {
    int vh = -1, cost = 0, strength = 1;
    if (building) {
        if (victim < 0 || victim >= (int)buildings_.size()) return;
        const Building& b = buildings_[(std::size_t)victim];
        vh = b.house;
        if (!ai_enabled(vh)) return;
        const std::vector<BuildingTypeDef>& bts = btypes();
        if (b.type >= 0 && b.type < (int)bts.size()) {
            cost = bts[(std::size_t)b.type].cost;
            strength = bts[(std::size_t)b.type].strength;
        } else {
            cost = strength = b.max_hp;
        }
    } else {
        if (victim < 0 || victim >= (int)units_.size()) return;
        const Unit& u = units_[(std::size_t)victim];
        vh = u.house;
        if (!ai_enabled(vh)) return;
        const HouseAiRec& rec = ai_houses_[(std::size_t)vh];
        const int t = ai_unit_type_of(rec, u);
        strength = u.max_hp;
        cost = (t >= 0) ? rec.cfg.team.unit_roster[(std::size_t)t].cost : u.max_hp;
    }
    if (strength <= 0) strength = 1;
    const int delta = (int)((double)cost * ((double)dmg / (double)strength));
    if (delta != 0) ai_add_anger(vh, attacker_house, delta);
}

void World::ai_pick_enemy_house(int house, HouseAiRec& rec) {
    const int n = house_count();
    if ((int)rec.anger.size() < n) rec.anger.resize((std::size_t)n, 0);
    if (rec.hate_frames < (1 << 30)) ++rec.hate_frames;
    // The hate-delay seed, on the house AI tick (FUN_004fd320 runs every
    // 0x69 + rand(1,7) frames; RE-FALLBACK: a fixed 105-frame cadence).
    if (rec.enemy_house < 0 && rec.cfg.hate_delay_frames >= 0 &&
        rec.hate_frames >= rec.cfg.hate_delay_frames &&
        (repair_frame_ + (std::uint64_t)house) % 105u == 0u) {
        int ox = 0, oy = 0;
        if (ai_base_centre(house, ox, oy)) {
            int best = -1;
            long long bestd = 0;
            for (int e = 0; e < n; ++e) {
                if (e == house || !house_combatant(e) || ai_house_out(e)) continue;
                int ex = 0, ey = 0;
                if (!ai_base_centre(e, ex, ey)) continue;
                const long long d = (long long)(ex - ox) * (ex - ox) + (long long)(ey - oy) * (ey - oy);
                if (best < 0 || d < bestd) { best = e; bestd = d; }
            }
            if (best >= 0) ai_add_anger(house, best, 1);
        }
    }
    const int cur = rec.enemy_house;
    if (cur >= 0 && (cur >= n || ai_house_out(cur) || !is_enemy(house, cur))) {
        if (cur < n) ai_add_anger(house, cur, -rec.anger[(std::size_t)cur]);
        rec.enemy_house = -1;
    }
}

namespace {
bool quarry_unit_match(int quarry, const Unit& u);
bool quarry_building_match(int quarry, const Building& b, const AiTeamConfig& tc);
}  // namespace

void World::ai_try_fire_superweapon(int house, HouseAiRec& rec) {
    // W6-03-05 / W6-02-09 / W6-02-10 [RE: FUN_00509710, the house superweapon
    // AI run from FUN_004fd320, walks every charged SW and switches on its
    // type]:
    //  * 0 Nuke: only with an Enemy (house+0x5600 != -1); the target is the
    //    PreferredTarget quarry pick (FUN_0050cf90) or FUN_0050ca10.
    //  * 2 Lightning Storm (FUN_00509c20): not while a storm rages
    //    (FUN_00539e30), only with an Enemy, same pickers.
    //  * 5/6 ParaDrop / AmerParaDrop, 8 SpyPlane, 11 PsychicReveal
    //    (FUN_00509af0): the Enemy's base centre (house+0x5494, else +0x5490),
    //    or this house's own base when it has no Enemy, nearest free cell
    //    (FUN_0056d750) + (2,2).
    //  * There is NO case for 1 IronCurtain, 3 ChronoSphere or 4 ChronoWarp:
    //    the house AI never fires them; they are used only through team
    //    script actions 55/56/57 and the ct=5/6 AITriggers.
    // FUN_0050ca10 scores ONLY objects owned by HouseArray[house+0x5600] from
    // the [General] AIIonCannon*Value arrays (see ai_sw_score) and fires at a
    // random one of the best-scoring cells.
    const int enemy = rec.enemy_house;
    const int quarry = (house >= 0 && house < (int)sw_quarry_.size()) ? sw_quarry_[house] : 1;
    for (SuperweaponType t : sw_types(house)) {
        if (!sw_ready(house, t)) continue;
        switch (t) {
            case SuperweaponType::Nuke:
            case SuperweaponType::LightningStorm: {
                if (enemy < 0) break;
                if (t == SuperweaponType::LightningStorm) {
                    bool raging = false;   // LightningStorm::IsActive (FUN_00539e30)
                    for (const SwStrike& s : an_.strikes)
                        if (s.kind == 2 && !s.done) raging = true;
                    if (raging) break;
                }
                Cellxy c{-1, -1};
                if (!ai_sw_pick_target(house, rec, enemy, quarry, c)) break;
                fire_superweapon_at(house, t, c.x, c.y);
                break;
            }
            case SuperweaponType::ParaDrop:
            case SuperweaponType::AmerParaDrop:
            case SuperweaponType::SpyPlane:
            case SuperweaponType::PsychicReveal: {
                int x = 0, y = 0;
                const int th = enemy >= 0 ? enemy : house;
                if (quarry != 1 && enemy >= 0) {
                    Cellxy c{-1, -1};
                    if (!ai_sw_pick_target(house, rec, enemy, quarry, c)) break;
                    x = c.x; y = c.y;
                } else {
                    if (!ai_base_centre(th, x, y)) break;
                    x = std::min(grid_w_ - 1, x + 2);   // FUN_00509af0: nearby cell + (2,2)
                    y = std::min(grid_h_ - 1, y + 2);
                }
                fire_superweapon_at(house, t, x, y);
                break;
            }
            default:
                break;   // IronCurtain / Chrono / Yuri types: no house-AI case
        }
    }
}

// FUN_0050ca10 / FUN_0050cf90: the best-valued cell among `enemy`'s objects on
// the ground (alive, not carried, not airborne); quarry != 1 (PreferredTarget,
// map action 35) restricts to that QuarryType [RE-FALLBACK: the highest-valued
// object of the quarry stands in for FUN_0050cf90's Greatest_Threat]. A hidden
// (cloaked/disguised) object scores rand(0, best + 10). Ties -> seeded random.
bool World::ai_sw_pick_target(int house, HouseAiRec& rec, int enemy, int quarry, Cellxy& out) {
    // FUN_0050ca10 also scores 0 for an object whose cell is outside the usable
    // map area (FUN_00577E90) -- World::cell_in_playfield (W6 fix #2; always true
    // without a campaign usable area).
    const AiTeamConfig& tc = rec.cfg.team;
    int best = 0;
    std::vector<Cellxy> ties;
    auto consider = [&](int v, Cellxy c) {
        if (v > best) { best = v; ties.clear(); }
        if (v == best && v > 0) ties.push_back(c);
    };
    for (const Building& b : buildings_) {
        if (!b.alive || b.state == Building::State::Dead || b.house != enemy) continue;
        if (building_is_wall(b)) continue;   // W6-03-01: walls are overlays
        if (quarry != 1 && !quarry_building_match(quarry, b, tc)) continue;
        const Cellxy bc{b.cx + b.fw / 2, b.cy + b.fh / 2};
        if (!cell_in_playfield(bc.x, bc.y)) continue;   // score 0
        consider(ai_sw_score_building(rec, b), bc);
    }
    for (const Unit& u : units_) {
        if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0 || u.house != enemy) continue;
        if (u.is_aircraft && u.flight_state != FlightState::Grounded) continue;   // ground layer only
        if (quarry != 1 && !quarry_unit_match(quarry, u)) continue;
        if (!cell_in_playfield(u.cell_x, u.cell_y)) continue;   // score 0 (FUN_00577E90)
        int v = ai_sw_score_unit(rec, u);
        if (unit_hidden_from(house, u))
            v = (int)(ai_rand(rec.state.rng) % (std::uint32_t)(best + 11));   // rand(0, best+10)
        consider(v, Cellxy{u.cell_x, u.cell_y});
    }
    if (ties.empty()) return false;
    out = ties[ai_rand(rec.state.rng) % (std::uint32_t)ties.size()];
    return true;
}

// FUN_0050ca10 building score, in the original's order: ConYard (Factory=
// BuildingType) -> War Factory (Factory=UnitType, not Naval) -> Power (output
// > 0) -> base defence -> tech centre -> any other building 4.
int World::ai_sw_score_building(const HouseAiRec& rec, const Building& b) const {
    const AiRules& ai = rec.cfg.ai;
    const std::vector<BuildingTypeDef>& bts = btypes();
    if (b.type < 0 || b.type >= (int)bts.size()) return ai.sw_value_building;
    const BuildingTypeDef& t = bts[b.type];
    const std::uint8_t bits = b.type < (int)rec.cfg.team.building_class.size()
                                  ? rec.cfg.team.building_class[b.type] : 0;
    if (t.conyard) return ai.sw_value_conyard;
    if (t.factory_kind == 2) return ai.sw_value_factory;
    if (t.power > 0) return ai.sw_value_power;
    if (bits & kAiBldDefense) return ai.sw_value_defense;
    if (t.id == "GATECH" || t.id == "NATECH" || t.id == "YATECH")
        return ai.sw_value_tech;   // [RE-FALLBACK: Rules+0x920 tech list by id]
    return ai.sw_value_building;
}

// FUN_0050ca10 unit score: vehicles -- harvester / MCV (deploys into a
// ConYard) / transport (Passengers > 0) values, else 2; infantry -- engineer
// / thief values, else 2; aircraft 1.
int World::ai_sw_score_unit(const HouseAiRec& rec, const Unit& u) const {
    const AiRules& ai = rec.cfg.ai;
    if (u.is_aircraft) return 1;
    if (u.is_harvester) return ai.sw_value_harvester;
    const std::vector<BuildingTypeDef>& bts = btypes();
    if (u.deploys_into >= 0 && u.deploys_into < (int)bts.size() && bts[u.deploys_into].conyard)
        return ai.sw_value_mcv;
    if (u.is_vehicle) return u.passengers_max > 0 ? ai.sw_value_apc : ai.sw_value_unit;
    if (u.image == "ENGINEER" || u.image == "SENGINEER") return ai.sw_value_engineer;
    if (u.image == "SPY" || u.image == "THIEF") return ai.sw_value_thief;
    return ai.sw_value_unit;
}

// --- AI-2 offensive-teams phase [RE: ai-team.md] ----------------------------

namespace {
// Quarry-kind -> unit-class match [G-61; RE: script_actions.hpp -- YRpp
// QuarryType: 1 Anything, 2 Buildings, 3 Harvesters, 4 Infantry, 5 Vehicles,
// 6 Factories, 7 BaseDefenses, 8 Threats, 9 Power, 10 Occupiable, 11 Tech].
bool quarry_unit_match(int quarry, const Unit& u) {
    switch (quarry) {
        case kQuarryHarvesters:  return u.is_harvester;
        case kQuarryInfantry:    return !u.is_vehicle && !u.is_harvester;
        case kQuarryVehicles:    return u.is_vehicle;
        case kQuarryThreats:     return true;   // any armed hostile unit [RE-FALLBACK]
        case kQuarryAnything:    return true;
        default:                 return false;
    }
}
// Building-class match through the house's resolved class bits
// (AiTeamConfig::building_class, main.cpp: Factory= / defense list / Power>0 /
// Capturable=). Occupiable (10): no garrison model -> never matches.
bool quarry_building_match(int quarry, const Building& b, const AiTeamConfig& tc) {
    const std::uint8_t bits =
        (b.type >= 0 && b.type < (int)tc.building_class.size()) ? tc.building_class[b.type] : 0;
    switch (quarry) {
        case kQuarryAnything:
        case kQuarryBuildings:   return true;
        case kQuarryFactories:   return (bits & kAiBldFactory) != 0;
        case kQuarryDefenses:    return (bits & kAiBldDefense) != 0;
        case kQuarryPower:       return (bits & kAiBldPower) != 0;
        case kQuarryTech:        return (bits & kAiBldTech) != 0;
        default:                 return false;
    }
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
            u.is_harvester || u.an.t.missile)  // W2-AN-16: jumpjets recruit
            continue;
        // Wave-5 (R-080): a docked, armed fixed-wing aircraft is recruitable;
        // spawned ones (Hornets) and planes on a sortie are not.
        if (u.an.t.fixed_wing && (u.an.t.spawned || u.an.sortie != 0 || u.an.ammo <= 0)) continue;
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

bool World::ai_quarry_exists(int house, const HouseAiRec& rec, int quarry) const {
    // W6-02-07/08: only targets a threat scan may pick (legal_threat_*).
    for (int i = 0; i < (int)units_.size(); ++i) {
        const Unit& u = units_[(std::size_t)i];
        if (u.alive && u.die_time < 0.0 && u.cargo_of < 0 && !u.an.t.fixed_wing && !u.an.t.missile &&
            is_enemy(house, u.house) && quarry_unit_match(quarry, u) && legal_threat_unit(house, i))  // W2-AN-16: jumpjets count
            return true;
    }
    for (int i = 0; i < (int)buildings_.size(); ++i) {
        const Building& b = buildings_[(std::size_t)i];
        if (b.alive && b.state != Building::State::Dead && is_enemy(house, b.house) &&
            !building_is_wall(b) &&   // W6-03-01: walls are overlays, never a quarry
            quarry_building_match(quarry, b, rec.cfg.team) && legal_threat_building(house, i))
            return true;
    }
    return false;
}

int World::ai_pick_quarry_unit(int house, int quarry, int nx, int ny,
                               std::uint32_t& rng, bool can_aa) const {
    // Class-filter + distance^2 x value x seeded-RNG jitter, keep the minimum
    // (nearest-and-most-valuable, with jitter); tie -> lowest index
    // [RE-GATE APPLIED: ai-team.md §5]. `value` is a per-target term (proxy 1
    // pre-map [RE-FALLBACK]); the jitter draws from ai_rand over `rng`.
    int best = -1;
    long long bestd = 0;
    for (int i = 0; i < (int)units_.size(); ++i) {
        const Unit& u = units_[i];
        if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0 || !is_enemy(house, u.house))
            continue;  // skip self+ally+neutral
        // An airborne aircraft is a quarry only for a member whose active
        // weapon is AA= (gap C's projectile AA/AG); a landed one is a ground
        // target [weapons.md §2].
        if (u.is_aircraft && u.flight_state != FlightState::Grounded && !can_aa) continue;
        if (!quarry_unit_match(quarry, u)) continue;
        if (!legal_threat_unit(house, i)) continue;   // W6-02-07/08
        const long long dx = u.cell_x - nx, dy = u.cell_y - ny;
        const long long value = 1;                      // [RE-FALLBACK] proxy
        const long long jitter = 90 + (ai_rand(rng) % 21);  // 0.90x .. 1.10x, x100
        long long score = (dx * dx + dy * dy) * value * jitter;
        // W2-SK-08: targets of the current enemy house always rank first.
        if (ai_enabled(house) && ai_houses_[house].enemy_house >= 0 &&
            u.house != ai_houses_[house].enemy_house)
            score += 1000000000000LL;
        if (best < 0 || score < bestd) { best = i; bestd = score; }
    }
    return best;
}

int World::ai_pick_quarry_building(int house, const HouseAiRec& rec, int quarry, int nx,
                                   int ny, std::uint32_t& rng) const {
    int best = -1;
    long long bestd = 0;
    for (int i = 0; i < (int)buildings_.size(); ++i) {
        const Building& b = buildings_[i];
        if (!b.alive || b.state == Building::State::Dead || !is_enemy(house, b.house)) continue;
        if (building_is_wall(b)) continue;   // W6-03-01: walls are overlays, never a quarry
        if (!quarry_building_match(quarry, b, rec.cfg.team)) continue;
        if (!legal_threat_building(house, i)) continue;   // W6-02-07/08
        const long long dx = b.cx - nx, dy = b.cy - ny;
        const long long value = 1;                      // [RE-FALLBACK] proxy
        const long long jitter = 90 + (ai_rand(rng) % 21);
        long long score = (dx * dx + dy * dy) * value * jitter;
        if (rec.enemy_house >= 0 && b.house != rec.enemy_house) score += 1000000000000LL;   // W2-SK-08
        if (best < 0 || score < bestd) { best = i; bestd = score; }
    }
    return best;
}

int World::ai_find_typed_building(int house, const HouseAiRec& rec, int arg, bool enemy,
                                  int nx, int ny) const {
    // Actions 46/47/58 [G-61; RE: script_actions.hpp]: arg low word = rulesmd
    // [BuildingTypes] index (mapped to the roster by main.cpp), high word =
    // which instance: 0 least threat / 1 highest threat / 2 nearest /
    // 3 farthest [ModEnc]. Threat is not modelled: 0 resolves as nearest and
    // 1 as farthest [RE-FALLBACK]. Ties -> lowest index (deterministic).
    const AiTeamConfig& tc = rec.cfg.team;
    const int bt = script_building_type(arg);
    if (bt < 0 || bt >= (int)tc.building_type_roster.size()) return -1;
    const int type = tc.building_type_roster[bt];
    if (type < 0) return -1;
    const int sel = script_building_selector(arg);
    const bool far = sel == kBldSelFarthest || sel == kBldSelHighestThreat;
    int best = -1;
    long long bestd = 0;
    for (int i = 0; i < (int)buildings_.size(); ++i) {
        const Building& b = buildings_[i];
        if (!b.alive || b.state == Building::State::Dead || b.type != type) continue;
        // W2-SK-09: the neutral (civilian) house's tech buildings are fair
        // game for 46/47 (the stock 'Tech Oil Capture' scripts target CAOILD).
        if (enemy ? (!is_enemy(house, b.house) && !(b.house != house && b.house == neutral_house()))
                  : b.house != house)
            continue;
        const long long dx = b.cx - nx, dy = b.cy - ny;
        long long d = dx * dx + dy * dy;
        // W6-03-04: an attack/capture order goes for the designated Enemy's
        // (or the neutral house's) building first; another enemy's only as a
        // fallback [RE-FALLBACK: the same Enemy-first rank as the quarry
        // pickers (W2-SK-08)].
        const bool other = enemy && rec.enemy_house >= 0 && b.house != rec.enemy_house &&
                           b.house != neutral_house();
        if (other) d = far ? d - 1000000000000LL : d + 1000000000000LL;
        if (best < 0 || (far ? d > bestd : d < bestd)) { best = i; bestd = d; }
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
    u.tg.area_guard = false;   // W6-02-11: an attack order leaves Area_Guard
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
    if (!tgt.alive || tgt.die_time >= 0.0 || !is_enemy(u.house, tgt.house)) return;  // AI guard: never attack self/ally/neutral
    commit_attack_target(unit_idx, target_idx, /*target_building=*/-1);
}

void World::ai_order_attack_building(int unit_idx, int b_idx) {
    // Per-unit entry (same rationale); own guard + shared commit_attack_target.
    if (unit_idx < 0 || unit_idx >= (int)units_.size()) return;
    if (b_idx < 0 || b_idx >= (int)buildings_.size()) return;
    const Unit& u = units_[unit_idx];
    const Building& tgt = buildings_[b_idx];
    if (!u.alive || u.die_time >= 0.0) return;
    if (!tgt.alive || tgt.state == Building::State::Dead || !is_enemy(u.house, tgt.house)) return;
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
            // W2-SK-09: a member riding the team's transport is where it is.
            if (u.cargo_of >= 0 && u.cargo_of < (int)units_.size()) {
                mv.cx = units_[(std::size_t)u.cargo_of].cell_x;
                mv.cy = units_[(std::size_t)u.cargo_of].cell_y;
            }
            mv.idle = !u.moving() &&
                      !(u.attack_order && (u.target >= 0 || u.target_building >= 0));
        }
        v.members.push_back(mv);
    }
    v.waypoints = rec.cfg.team.waypoints;
    {   // W2-SK-09: is everybody aboard the team's transport?
        const int tr = ai_team_transport(team);
        v.all_loaded = true;
        if (tr >= 0)
            for (int m : team.members) {
                if (m == tr || m < 0 || m >= (int)units_.size()) continue;
                const Unit& u = units_[(std::size_t)m];
                if (!u.alive || u.die_time >= 0.0) continue;
                if (u.cargo_of != tr && can_load(units_, tr, m)) { v.all_loaded = false; break; }
            }
    }
    // Base center + enemy centroid reuse the AI-1 view (ConYard-first center,
    // rival-buildings threat centroid) — ONE source of truth.
    const BaseView bv = make_ai_base_view(house, rec);
    v.base_x = bv.center_x;
    v.base_y = bv.center_y;
    v.enemy_x = bv.threat_x;
    v.enemy_y = bv.threat_y;
    // quarry_exists / building_found are computed for the CURRENT line --
    // the completion predicates the interpreter consults [G-61; RE:
    // ai-team.md §3/§5]. Recomputed PER TICK (contract B).
    v.quarry_exists = false;
    v.building_found = false;
    const AimdData& aimd = rec.cfg.team.aimd;
    if (team.state == TeamState::Executing && team.teamtype >= 0 &&
        team.teamtype < (int)aimd.teams.size()) {
        const ScriptType& sc = aimd.scripts[aimd.teams[team.teamtype].script];
        if (team.script_pc >= 0 && team.script_pc < (int)sc.actions.size()) {
            const ScriptAction a = sc.actions[team.script_pc];
            const OpClass oc = classify_op(a.action);
            // The lead member (first live) anchors nearest/farthest picks.
            int lx = v.base_x, ly = v.base_y;
            for (const TeamMemberView& m : v.members)
                if (m.alive) { lx = m.cx; ly = m.cy; break; }
            if (oc == OpClass::AttackQuarry) {
                v.quarry_exists = ai_quarry_exists(house, rec, a.argument);
            } else if (oc == OpClass::AttackBuilding) {
                v.quarry_exists = ai_find_typed_building(house, rec, a.argument,
                                                         /*enemy=*/true, lx, ly) >= 0;
            } else if (oc == OpClass::IronCurtainMe || oc == OpClass::ChronoPrep) {
                v.sw_state = ai_team_sw_state(house, rec, oc == OpClass::IronCurtainMe
                                                              ? SuperweaponType::IronCurtain
                                                              : SuperweaponType::ChronoSphere);
            } else if (oc == OpClass::MoveEnemyBuilding ||
                       oc == OpClass::MoveFriendlyBuilding) {
                const int bi = ai_find_typed_building(
                    house, rec, a.argument, oc == OpClass::MoveEnemyBuilding, lx, ly);
                if (bi >= 0) {
                    v.building_found = true;
                    v.building_x = buildings_[bi].cx;
                    v.building_y = buildings_[bi].cy;
                }
            }
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
                if (u.cargo_of >= 0) continue;   // W2-SK-09: rides the transport
                order_unit_move(m, gx + dx[k % 9] * spread, gy + dy[k % 9] * spread);
            }
            break;
        }
        case TeamCommand::Kind::AttackQuarry: {
            // W2-SK-11 [RE: ai-team.md §5 -- one target from the team anchor]:
            // pick ONCE from the lead (first live) member and give every idle
            // member the same target, instead of each member's own nearest.
            int lead = -1;
            for (int m : team.members)
                if (m >= 0 && m < (int)units_.size() && units_[m].alive && units_[m].die_time < 0.0) {
                    lead = m;
                    break;
                }
            int team_tu = -1, team_tb = -1;
            if (lead >= 0) {
                const Unit& lu = units_[lead];
                if (c.building_arg >= 0) {
                    team_tb = ai_find_typed_building(house, rec, c.building_arg, true, lu.cell_x, lu.cell_y);
                } else {
                    team_tu = ai_pick_quarry_unit(house, c.quarry, lu.cell_x, lu.cell_y,
                                                  rec.team_state.rng, lu.w_aa);
                    if (team_tu < 0)
                        team_tb = ai_pick_quarry_building(house, rec, c.quarry, lu.cell_x, lu.cell_y,
                                                          rec.team_state.rng);
                }
            }
            for (int m : team.members) {
                if (m < 0 || m >= (int)units_.size()) continue;
                Unit& u = units_[m];
                if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) continue;
                // W2-SK-09: an engineer captures / a spy infiltrates the team's
                // building target instead of "attacking" it (stock scripts
                // 'Tech Oil Capture', 'Spy vs Power', 'Engineer vs Ally').
                if (u.image == "ENGINEER" || u.image == "SPY") {
                    if (u.capture_target >= 0 || u.infiltrate_target >= 0) continue;
                    int tb = team_tb;
                    if (tb < 0)
                        tb = ai_find_typed_building(house, rec, c.building_arg >= 0 ? c.building_arg : -1,
                                                    true, u.cell_x, u.cell_y);
                    if (tb < 0) continue;
                    if (u.image == "ENGINEER") {
                        if (building_capturable(tb)) order_engineer_capture(m, tb);
                    } else {
                        order_spy_infiltrate(m, tb, ai_infiltrate_purpose(tb), 0.5, 1000);
                    }
                    continue;
                }
                if (u.attack_order && (u.target >= 0 || u.target_building >= 0)) continue;
                if (team_tu >= 0 && (!units_[team_tu].is_aircraft || u.w_aa)) { ai_order_attack(m, team_tu); continue; }
                if (team_tb >= 0) { ai_order_attack_building(m, team_tb); continue; }
            }
            for (int m : team.members) {
                if (m < 0 || m >= (int)units_.size()) continue;
                Unit& u = units_[m];
                if (!u.alive || u.die_time >= 0.0) continue;
                if (u.attack_order && (u.target >= 0 || u.target_building >= 0))
                    continue;  // already pursuing — no target thrash
                if (c.building_arg >= 0) {   // action 46: THAT building type
                    const int tb = ai_find_typed_building(house, rec, c.building_arg,
                                                          /*enemy=*/true, u.cell_x, u.cell_y);
                    if (tb >= 0) ai_order_attack_building(m, tb);
                    continue;
                }
                const int tu = ai_pick_quarry_unit(house, c.quarry, u.cell_x,
                                                   u.cell_y, rec.team_state.rng, u.w_aa);
                if (tu >= 0) { ai_order_attack(m, tu); continue; }
                const int tb = ai_pick_quarry_building(house, rec, c.quarry, u.cell_x,
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
        case TeamCommand::Kind::Load: {   // W2-SK-09: board the team's transport
            const int tr = ai_team_transport(team);
            if (tr < 0) break;
            const Unit& t = units_[(std::size_t)tr];
            for (int m : team.members) {
                if (m == tr || m < 0 || m >= (int)units_.size()) continue;
                Unit& u = units_[(std::size_t)m];
                if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) continue;
                if (!can_load(units_, tr, m)) continue;
                cargo_any_ = true;
                u.target = -1;
                u.target_building = -1;
                u.attack_order = false;
                u.unload_pending = false;
                u.enter_target = tr;
                order_unit_move(m, t.cell_x, t.cell_y);
            }
            break;
        }
        case TeamCommand::Kind::Unload: {   // W2-SK-09
            const int tr = ai_team_transport(team);
            if (tr >= 0 && !units_[(std::size_t)tr].cargo.empty()) {
                units_[(std::size_t)tr].unload_pending = true;
                cargo_any_ = true;
            }
            break;
        }
        case TeamCommand::Kind::Deploy:   // W2-SK-10: GI sandbags, Desolator rad
            for (int m : team.members)
                if (m >= 0 && m < (int)units_.size() && units_[(std::size_t)m].alive &&
                    units_[(std::size_t)m].cargo_of < 0 && !is_mcv(m))
                    order_deploy(m);
            break;
        case TeamCommand::Kind::IronCurtain:
        case TeamCommand::Kind::ChronoShift: {
            // W6-03-05 [RE: FUN_006ef1d0 / FUN_006ef3c0 / FUN_006ef690]: the
            // leader is the vehicle member with the highest +0x5FC rating
            // [RE-FALLBACK: the first live vehicle member, else the first
            // live member]. 55 curtains the team at the leader; 57 asks the
            // leader's Greatest_Threat of quarry <arg> (56: the building of
            // type <arg>) and chronoshifts the team there -- Sphere at the
            // leader, Warp at the target -- then the line completes.
            int lead = -1;
            for (int pass = 0; pass < 2 && lead < 0; ++pass)
                for (int m : team.members)
                    if (m >= 0 && m < (int)units_.size() && units_[m].alive && units_[m].die_time < 0.0 &&
                        units_[m].cargo_of < 0 && (pass == 1 || units_[m].is_vehicle)) {
                        lead = m;
                        break;
                    }
            if (lead < 0) break;
            const Unit& lu = units_[(std::size_t)lead];
            if (c.kind == TeamCommand::Kind::IronCurtain) {
                fire_superweapon_at(house, SuperweaponType::IronCurtain, lu.cell_x, lu.cell_y);
                break;
            }
            int tx = -1, ty = -1;
            if (c.building_arg >= 0) {
                const int tb = ai_find_typed_building(house, rec, c.building_arg, true, lu.cell_x, lu.cell_y);
                if (tb >= 0) { tx = buildings_[(std::size_t)tb].cx; ty = buildings_[(std::size_t)tb].cy; }
            } else {
                const int tb = ai_pick_quarry_building(house, rec, c.quarry, lu.cell_x, lu.cell_y,
                                                       rec.team_state.rng);
                if (tb >= 0) {
                    tx = buildings_[(std::size_t)tb].cx; ty = buildings_[(std::size_t)tb].cy;
                } else {
                    const int tu = ai_pick_quarry_unit(house, c.quarry, lu.cell_x, lu.cell_y,
                                                       rec.team_state.rng, lu.w_aa);
                    if (tu >= 0) { tx = units_[(std::size_t)tu].cell_x; ty = units_[(std::size_t)tu].cell_y; }
                }
            }
            if (tx < 0) break;   // no target: the line completes without a shift
            // Land next to the target, not inside its foundation.
            tx = std::max(1, std::min(grid_w_ - 2, tx - 2));
            ty = std::max(1, std::min(grid_h_ - 2, ty - 2));
            fire_superweapon_at(house, SuperweaponType::ChronoSphere, lu.cell_x, lu.cell_y, tx, ty);
            break;
        }
        case TeamCommand::Kind::Inert:  // logged skip — pc already advanced
        case TeamCommand::Kind::Wait:
        case TeamCommand::Kind::None:
        default:
            break;
    }
}

int World::ai_team_transport(const TeamInstance& t) const {
    for (int m : t.members)
        if (m >= 0 && m < (int)units_.size() && units_[(std::size_t)m].alive &&
            units_[(std::size_t)m].die_time < 0.0 && units_[(std::size_t)m].passengers_max > 0)
            return m;
    return -1;
}

// The gesture layer's purpose table (main.cpp infiltrate_purposes_for),
// derived from the type data the World holds.
World::InfiltratePurpose World::ai_infiltrate_purpose(int b_idx) const {
    if (b_idx < 0 || b_idx >= (int)buildings_.size()) return InfiltratePurpose::None;
    const int t = buildings_[(std::size_t)b_idx].type;
    if (t < 0 || t >= (int)btypes().size()) return InfiltratePurpose::None;
    const BuildingTypeDef& d = btypes()[(std::size_t)t];
    if (!d.spyable) return InfiltratePurpose::None;
    if (d.w5.spy_purpose >= 0) return static_cast<InfiltratePurpose>(d.w5.spy_purpose);   // wave-5
    if (std::find(refinery_types_.begin(), refinery_types_.end(), t) != refinery_types_.end())
        return InfiltratePurpose::Refinery;
    if (d.power > 0) return InfiltratePurpose::PowerPlant;
    if (buildings_[(std::size_t)b_idx].grants_superweapon) return InfiltratePurpose::Superweapon;
    if (d.factory_kind > 0) return InfiltratePurpose::Factory;
    return InfiltratePurpose::RadarLab;
}

int World::ai_team_need(const TeamInstance& t, const AimdData& aimd, int ut) {
    if (t.teamtype < 0 || t.teamtype >= (int)aimd.teams.size()) return 0;
    const int tfi = aimd.teams[t.teamtype].taskforce;
    if (tfi < 0 || tfi >= (int)aimd.taskforces.size()) return 0;
    int want = 0;
    for (const TaskForceMember& m : aimd.taskforces[tfi].members)
        if (m.unit == ut) want += m.count;
    int have = 0;
    for (int mt : t.member_types)
        if (mt == ut) ++have;
    return want - have;
}

bool World::ai_launch_unit_ready(int house, HouseAiRec& rec, QueueCat cat) {
    // Exit handoff via the EXISTING primary_factory + launch_unit path (the
    // player's run_interactive twin) [RE: ai-team.md §6]. Returns true if the
    // ready slot was consumed.
    if (!rec.queues.ready(cat)) return false;
    const AiTeamConfig& tc = rec.cfg.team;
    // W2-AN-16: a ship (naval or amphibious) leaves the house's Naval Yard.
    const int pslot = rec.queues.producing(cat);
    const bool naval_unit = cat == QueueCat::Vehicle && pslot >= 0 &&
                            pslot < (int)tc.unit_roster.size() &&
                            (tc.unit_roster[pslot].combat.is_naval ||
                             tc.unit_roster[pslot].combat.movement_domain == MovementDomain::Amphibious);
    // Wave-5 (R-080): a fixed-wing aircraft is built onto an own airfield pad
    // (any Helipad=yes AircraftType factory the World knows of).
    const bool air_unit = cat == QueueCat::Vehicle && pslot >= 0 && pslot < (int)tc.unit_roster.size() &&
                          tc.unit_roster[pslot].combat.an.fixed_wing;
    std::vector<int> air_ft;
    if (air_unit)
        for (int bt = 0; bt < (int)an_.airfield_btype.size(); ++bt)
            if (an_.airfield_btype[(std::size_t)bt]) air_ft.push_back(bt);
    const std::vector<int>& ft = air_unit ? air_ft
                                 : naval_unit ? tc.nav_factory_types
                                 : cat == QueueCat::Infantry ? tc.inf_factory_types : tc.veh_factory_types;
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
    if (units_[(std::size_t)uidx].image == "SPY") {   // W2-SK-22: AI spies go out disguised
        const int foe = rec.enemy_house;
        std::string look;
        int look_house = foe;
        for (const Unit& e : units_)
            if (e.alive && e.die_time < 0.0 && e.house == foe && !e.is_vehicle && !e.is_aircraft &&
                e.image != "SPY") { look = e.image; break; }
        if (!look.empty()) set_disguise(uidx, look, look_house);
    }
    if (!tc.unit_roster[uslot].combat.deploys_into.empty()) {   // W2-SK-07: unfold it
        order_deploy_mcv(uidx);
        rec.queues.placed(cat);
        return true;
    }
    if (tc.unit_roster[uslot].harvester) {   // [G-68] a bought miner goes to work
        const UnitTypeDef& d = tc.unit_roster[uslot];
        make_harvester(uidx, d.chrono_miner, d.storage > 0 ? d.storage : 20);
        rec.queues.placed(cat);
        return true;
    }
    // Claim into the FIRST (oldest) team owing this type -- deterministic; an
    // unclaimed unit stays idle and recruitable [RE: ai-team.md §2].
    // W2-SK-06 [RE: ai-team.md §2 "an unfilled Full=no team keeps recruiting /
    // producing toward strength" while it acts]: Gathering teams
    // that are still under strength claim too; the recruit drives to the team.
    for (TeamInstance& t : rec.team_state.teams) {
        // Recruiting and Gathering only: an Executing team runs its script
        // with what it has (a late recruit held its CloseEnough move line).
        if (t.state != TeamState::Recruiting && t.state != TeamState::Gathering) continue;
        if (ai_team_need(t, tc.aimd, uslot) <= 0) continue;
        t.members.push_back(uidx);
        t.member_types.push_back(uslot);
        if (t.state != TeamState::Recruiting) {
            int fx = t.gather_x, fy = t.gather_y;
            if (t.state == TeamState::Executing) {
                long long sx = 0, sy = 0, n = 0;
                for (int m : t.members)
                    if (m >= 0 && m < (int)units_.size() && m != uidx && units_[m].alive) {
                        sx += units_[m].cell_x; sy += units_[m].cell_y; ++n;
                    }
                if (n) { fx = (int)(sx / n); fy = (int)(sy / n); }
            }
            order_unit_move(uidx, fx, fy);
        }
        break;
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
            // [G-60] per-TeamType buildable test (FUN_0041ff00): every
            // TaskForce member type has TechLevel > 0 and its prerequisites
            // standing for this house. Shared by autocreate and AI-3.
            std::vector<char> unit_ok(tc.unit_roster.size(), 0);
            for (int ut = 0; ut < (int)tc.unit_roster.size(); ++ut)
                unit_ok[ut] = tc.unit_roster[ut].tech_level > 0 &&
                              prerequisites_met(tc.unit_roster[ut].prerequisite, buildings_,
                                                rec.cfg.roster, h, rec.cfg.groups);
            // A type the house cannot build but already has standing idle
            // counts too (recruit-idle-first, ai-team.md §2) [RE-FALLBACK].
            for (const IdleUnitView& iv : ai_idle_units(h, rec))
                if (iv.unit_type >= 0 && iv.unit_type < (int)unit_ok.size()) unit_ok[iv.unit_type] = 1;
            std::vector<char> team_ok(tc.aimd.teams.size(), 1);
            for (int ti = 0; ti < (int)tc.aimd.teams.size(); ++ti) {
                const int tfi = tc.aimd.teams[ti].taskforce;
                if (tfi < 0 || tfi >= (int)tc.aimd.taskforces.size()) { team_ok[ti] = 0; continue; }
                for (const TaskForceMember& m : tc.aimd.taskforces[tfi].members)
                    if (m.unit < 0 || m.unit >= (int)unit_ok.size() || !unit_ok[m.unit])
                        team_ok[ti] = 0;
            }
            st.spawn_clock += check_period;
            const double spawn_period = tc.rules.autocreate_time_min * 60.0;  // min->s
            if (st.spawn_clock >= spawn_period) {
                st.spawn_clock = 0.0;
                // CONTRACT F: select_autocreate_team takes a trailing seeded rng
                // (the house's AiTeamState::rng — deterministic, never wall-clock).
                const int pick = select_autocreate_team(tc.aimd, tc.house_name,
                                                        st.teams, rec.credits,
                                                        tc.rules, st.rng, &team_ok);
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
                tv.team_buildable = team_ok;
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
        // W2-SK-06 [RE: ai-team.md §6 -- produce for every team shortfall in
        // parallel factories]: one pick per IDLE factory category (infantry
        // and vehicle start in the same tick), the type most owed across ALL
        // under-strength live teams that this house can start and afford now
        // (the old oldest-Recruiting-team-only pick wedged every factory
        // behind one unbuildable or unaffordable slot).
        int want = -1;
        int want_inf = -1, want_veh = -1;
        // W2-SK-06 base priority [RE-FALLBACK: the original AI keeps building
        // its base while it trains; with every factory now producing, a unit
        // start must not eat the money AI-1 is banking for its next structure
        // (the base froze at 7 buildings / 1 refinery in the first soak)].
        double base_hold = 0.0;
        {
            AiBaseState probe = rec.state;
            const BaseView bv = make_ai_base_view(h, rec);
            const BuildDecision bd = next_build(probe, bv, rec.cfg.catalog, rec.cfg.roster,
                                                rec.cfg.ai, rec.cfg.mods);
            const int ht = bd.type >= 0 ? bd.type : bd.wait_type;
            const bool cy = std::any_of(buildings_.begin(), buildings_.end(), [&](const Building& b) {
                return b.alive && b.house == h && b.state == Building::State::Idle && b.type >= 0 &&
                       b.type < (int)rec.cfg.roster.size() && rec.cfg.roster[b.type].conyard;
            });
            // Core economy structures only (power / refinery / barracks / war
            // factory / radar): banking for a 5000-credit superweapon or the
            // tech centre would starve the army for minutes.
            const AiCatalog& ct = rec.cfg.catalog;
            const bool core = ht == ct.power || ht == ct.refinery || ht == ct.barracks ||
                              ht == ct.war_factory || ht == ct.radar;
            if (cy && core && ht >= 0 && ht < (int)rec.cfg.roster.size() &&
                rec.queues.producing(QueueCat::Structure) == -1)
                base_hold = rec.cfg.roster[ht].cost * rec.cfg.mods.cost_mul;
        }
        {
            std::vector<int> need(tc.unit_roster.size(), 0);
            for (const TeamInstance& t : st.teams) {
                if (t.state != TeamState::Recruiting && t.state != TeamState::Gathering) continue;
                for (int ut = 0; ut < (int)tc.unit_roster.size(); ++ut)
                    need[ut] += std::max(0, ai_team_need(t, tc.aimd, ut));
            }
            // W4-SQ-12: the campaign's scripted teams of this house.
            if (h < (int)ai_demand_ids_.size())
                for (std::size_t k = 0; k < ai_demand_ids_[(std::size_t)h].size(); ++k)
                    for (int ut = 0; ut < (int)tc.unit_roster.size(); ++ut)
                        if (tc.unit_roster[ut].id == ai_demand_ids_[(std::size_t)h][k]) {
                            need[ut] += ai_demand_counts_[(std::size_t)h][k];
                            break;
                        }
            int best_inf = 0, best_veh = 0;
            for (int ut = 0; ut < (int)tc.unit_roster.size(); ++ut) {
                if (need[ut] <= 0) continue;
                const UnitTypeDef& d = tc.unit_roster[ut];
                const bool veh = d.is_vehicle;
                const QueueCat cat = veh ? QueueCat::Vehicle : QueueCat::Infantry;
                const bool free_inf = !veh && rec.credits > tc.rules.infantry_reserve;
                if (!free_inf &&
                    rec.credits < d.cost * rec.cfg.mods.cost_mul + tc.rules.credit_reserve + base_hold)
                    continue;
                const std::vector<int>& ft = veh ? tc.veh_factory_types : tc.inf_factory_types;
                if (factory_category_lost(ft, h) ||
                    !can_start_unit(rec.queues.state(cat), ut, tc.unit_roster, buildings_,
                                    rec.cfg.roster, h, rec.cfg.groups, true))
                    continue;
                if (veh ? need[ut] > best_veh : need[ut] > best_inf) {
                    (veh ? best_veh : best_inf) = need[ut];
                    (veh ? want_veh : want_inf) = ut;
                }
            }
        }
        // [G-68] IQ Harvester: miners below refineries x HarvestersPerRefinery
        // -> the miner comes first [RE: rulesmd [IQ] Harvester=2 "automatically
        // replace lost harvesters"; HarvestersPerRefinery "formerly
        // hardcoded"]. RE-FALLBACK: checked every frame the vehicle queue is
        // idle; affordability as any unit (cost + CreditReserve).
        // W2-SK-07 [RE-FALLBACK, RE-Q4 open: the original skirmish AI rebuys
        // an MCV when it has no Construction Yard and redeploys it]: with no
        // ConYard, no MCV standing or in production, and a war factory, the
        // vehicle factory builds the side's MCV ahead of everything else.
        {
            bool cy = false, mcv = false;
            for (const Building& b : buildings_)
                if (b.alive && b.house == h && b.state == Building::State::Idle && b.type >= 0 &&
                    b.type < (int)rec.cfg.roster.size() && rec.cfg.roster[b.type].conyard) { cy = true; break; }
            for (const Unit& u : units_)
                if (u.alive && u.die_time < 0.0 && u.house == h && u.deploys_into >= 0) { mcv = true; break; }
            const int vp = rec.queues.producing(QueueCat::Vehicle);
            if (vp >= 0 && vp < (int)tc.unit_roster.size() && !tc.unit_roster[vp].combat.deploys_into.empty())
                mcv = true;
            if (!cy && !mcv && !factory_category_lost(tc.veh_factory_types, h))
                for (int ut = 0; ut < (int)tc.unit_roster.size(); ++ut) {
                    const UnitTypeDef& d = tc.unit_roster[ut];
                    if (d.combat.deploys_into.empty()) continue;
                    if (rec.credits < d.cost * rec.cfg.mods.cost_mul + tc.rules.credit_reserve) break;
                    if (!can_start_unit(rec.queues.state(QueueCat::Vehicle), ut, tc.unit_roster, buildings_,
                                        rec.cfg.roster, h, rec.cfg.groups, true))
                        continue;
                    want_veh = ut;
                    break;
                }
        }
        // W6-03-08 [RE: FUN_004fdb30 abandons the house's production when it
        // raises money]: while a refinery / harvester is owed, only the
        // harvester (pass 1 below) may start.
        if (rec.state.raise_building >= 0 || rec.state.raise_unit >= 0) want_inf = want_veh = -1;
        for (int pass = 0; pass < 2; ++pass) {
        want = pass == 0 ? want_inf : want_veh;
        if (pass == 1 && tc.harvesters_per_refinery > 0 &&
            !(want >= 0 && !tc.unit_roster[want].combat.deploys_into.empty())) {
            int refineries = 0, miners = 0;
            for (const Building& b : buildings_)
                if (b.alive && b.house == h && b.state == Building::State::Idle &&
                    std::find(refinery_types_.begin(), refinery_types_.end(), b.type) !=
                        refinery_types_.end())
                    ++refineries;
            for (const Unit& u : units_)
                if (u.alive && u.die_time < 0.0 && u.house == h && u.is_harvester) ++miners;
            if (miners < refineries * tc.harvesters_per_refinery)
                for (int t = 0; t < (int)tc.unit_roster.size(); ++t)
                    if (tc.unit_roster[t].harvester &&
                        rec.credits >= tc.unit_roster[t].cost * rec.cfg.mods.cost_mul +
                                           tc.rules.credit_reserve &&
                        prerequisites_met(tc.unit_roster[t].prerequisite, buildings_,
                                          rec.cfg.roster, h, rec.cfg.groups)) {
                        want = t;
                        break;
                    }
        }
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
            // [G-68] miners never join a team, so "on hand" is meaningless for
            // them -- the IQ Harvester count above is their only gate.
            for (int i = 0; i < (int)units_.size() && !onhand_of_type &&
                            !tc.unit_roster[want].harvester; ++i) {
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
        }   // W2-SK-06 per-category pass

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
                    t.best_focus_d2 = -1;
                    t.no_progress_secs = 0.0;
                } else {
                    int n_alive = 0;
                    for (int m : t.members)
                        if (m >= 0 && m < (int)units_.size() && units_[m].alive &&
                            units_[m].die_time < 0.0) ++n_alive;
                    const double reach = team_reach_cells(
                        t.state == TeamState::Gathering ? kAiStrayCells : kAiCloseEnoughCells, n_alive);
                    bool any_alive = false, any_moving = false, all_within = true;
                    long long sum_d2 = 0;
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
                        sum_d2 += ddx * ddx + ddy * ddy;
                    }
                    // [G-60] progress backstop [RE-FALLBACK: 10 s without the
                    // team getting any closer counts as stalled even while
                    // members still report moving (blocked in a crowd)].
                    if (t.best_focus_d2 < 0 || sum_d2 < t.best_focus_d2) {
                        t.best_focus_d2 = sum_d2;
                        t.no_progress_secs = 0.0;
                    } else {
                        t.no_progress_secs += dt;
                    }
                    const bool stuck = t.no_progress_secs >= 10.0;
                    if (any_alive && (!any_moving || stuck) && !all_within) {
                        t.best_focus_d2 = -1;
                        t.no_progress_secs = 0.0;
                        if (t.state == TeamState::Gathering) {
                            const Cellxy g =
                                base_gather_cell(view.base_x, view.base_y, st.rng);   // W2-SK-06
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
    // W2-SK-08/09: "enemy" = the ONE current enemy house when picked (the
    // original's house+0x5600); the skirmish lobby appends a real neutral
    // house, whose holdings feed ct=7 CivilianHouseOwns.
    const int neutral = neutral_house();
    if (neutral >= 0) {
        v.neutral_units.assign(tc.unit_roster.size(), 0);
        v.neutral_buildings.assign(rec.cfg.roster.size(), 0);
    }
    auto is_foe = [&](int hh) {
        return rec.enemy_house >= 0 ? hh == rec.enemy_house : is_enemy(house, hh);
    };
    for (const Unit& u : units_) {
        // Carried units are off-map/invisible to census [CB-1 off-map invariant].
        if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) continue;
        const int t = ai_unit_type_of(rec, u);
        if (t < 0) continue;
        if (u.house == house) ++v.own_units[t];
        else if (neutral >= 0 && u.house == neutral) ++v.neutral_units[t];
        else if (is_foe(u.house)) ++v.enemy_units[t];   // [G-67] not allies/neutral
    }
    for (const Building& b : buildings_) {
        if (!b.alive || b.state == Building::State::Dead) continue;
        if (b.type < 0 || b.type >= (int)rec.cfg.roster.size()) continue;
        if (b.house == house) ++v.own_buildings[b.type];
        else if (neutral >= 0 && b.house == neutral) ++v.neutral_buildings[b.type];
        else if (is_foe(b.house)) ++v.enemy_buildings[b.type];   // [G-67]
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
    // W6-03-05: ct=5/6 read the owner's Iron Curtain / Chronosphere charge.
    v.minor_ready = rec.cfg.ai.minor_super_ready;
    v.iron_charge = v.chrono_charge = -1.0;
    for (SuperweaponType t : sw_types(house)) {
        if (t == SuperweaponType::IronCurtain) v.iron_charge = sw_charge_fraction(house, t);
        if (t == SuperweaponType::ChronoSphere) v.chrono_charge = sw_charge_fraction(house, t);
    }
}

void World::step(double dt) {
    // W6-03-04: arm the damage -> anger feed for this step (computer houses
    // only; a strict no-op otherwise), disarmed on every exit.
    struct FeedGuard {
        DamageFeed saved;
        FeedGuard(World* w, bool on) : saved(damage_feed()) {
            if (on) { damage_feed().ctx = w; damage_feed().fn = &World::ai_damage_feed_cb; }
        }
        ~FeedGuard() { damage_feed() = saved; }
    } feed_guard(this, ai_any_);
    // W2-SK-15: tombstoned units keep their slot (every target / team member /
    // cargo / parasite / projectile owner is a stable unit INDEX, so slots are
    // never erased or reused), but a dead slot releases its heap storage: the
    // path and cargo vectors. Swept every 150 frames; nothing reads either
    // field of a dead unit.
    if ((repair_frame_ % 150u) == 0u)
        for (Unit& u : units_)
            if (!u.alive) {
                if (u.path.capacity()) { std::vector<Cellxy>().swap(u.path); u.path_idx = 0; }
                if (u.cargo.capacity() && u.cargo.empty()) std::vector<int>().swap(u.cargo);
            }
    {
        // W2-AN-10: an amphibious unit re-paths over Land OR Water.
        bool amph = false;
        for (const Unit& u : units_)
            if (u.movement_domain == MovementDomain::Amphibious && u.alive) { amph = true; break; }
        // W5-B R-062: the land speed inputs (armed by w5_arm_combat only).
        MoveSpeedCtx sc;
        if (land_speed_armed_) {
            sc.armed = true;
            sc.land = &speed_land_;
            sc.ramp = &ramp_;
            sc.level = &level_grid_;
            sc.ore = &ore_;
            for (int a = 0; a < 12; ++a)
                for (int b = 0; b < 8; ++b) sc.cost[a][b] = land_rules_.cost[a][b];
            sc.up[0] = land_rules_.tracked_up;
            sc.up[1] = land_rules_.wheeled_up;
            sc.down[0] = land_rules_.tracked_down;
            sc.down[1] = land_rules_.wheeled_down;
            sc.condition_red = land_rules_.condition_red;
        }
        std::vector<int> scatter;
        if (occ_.subcells()) {
            sc.scatter_out = &scatter;
            if (!land_speed_armed_) sc.armed = false;
        }
        w6_apply_wall_kills();   // W6-04-05: walls shot down last step open now
        w6_step_powered_units();   // W6-04-18: robots without a powered control centre stop
        step_wall_crush();   // P4 (no-op without crushable walls)
        const bool crush = !crush_wall_.empty();
        // R-210 bridges and cliffs: the height gate / deck layer / allied gates.
        StepTerrain st;
        std::vector<std::vector<int>> gates;
        bool any_gate = false;
        {
            const std::vector<BuildingTypeDef>& bt = btypes();
            for (const Building& b : buildings_)
                if (b.alive && b.type >= 0 && b.type < static_cast<int>(bt.size()) &&
                    bt[static_cast<std::size_t>(b.type)].w6.gate) { any_gate = true; break; }
        }
        if (any_gate) {
            gates.resize(static_cast<std::size_t>(std::max(1, house_count())));
            for (int h = 0; h < static_cast<int>(gates.size()); ++h) open_gates_for(h, gates[(std::size_t)h]);
            st.gates = &gates;
        }
        if (terrain_armed()) {
            for (Unit& u : units_)
                if (u.alive) sync_deck_layer(u);
            PathTerrain base;
            base.level = &level_grid_;
            base.slope = ramp_.size() == level_grid_.size() ? &ramp_ : nullptr;
            base.version = bridge_version_;
            st.water = base;
            st.land = base;
            st.land.deck = deck_grid(MovementDomain::Land);
            st.amph = base;
            st.amph.deck = deck_grid(MovementDomain::Amphibious);
        }
        step_units(units_, passable_, water_passable_, grid_w_, grid_h_, occ_, dt, move_cps_, anims_,
                   amph ? &amphib_grid() : nullptr, (land_speed_armed_ || occ_.subcells()) ? &sc : nullptr, wall_leak(),
                   crush ? &crush_grid() : nullptr, crush ? crush_leak() : nullptr,
                   (any_gate || terrain_armed()) ? &st : nullptr);
        std::sort(scatter.begin(), scatter.end());
        scatter.erase(std::unique(scatter.begin(), scatter.end()), scatter.end());
        for (int s : scatter) order_scatter(s);
    }

    // P5 Crates [RE: veterancy.md C4/C5]: pickup by any unit now standing on a
    // crate cell, then lifetime expiry -> relocation. A pure INSERTION;
    // `if (!crates_any_) return;` inside is the only cost every existing gate
    // pays (no existing demo calls enable_crates).
    step_crates();
    step_playfield();   // W6 fix #2: units moving into the usable area (no-op without one)

    // Shroud/Fog [RE: shroud.md §3]: recompute per-house visibility after
    // movement and before combat reads it (via seen_by_mask). Firewalled behind
    // shroud_any_ (false in every existing demo -> a strict no-op).
    step_visibility();

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
    step_service_depots();   // W2-SK-19
    step_disguises();        // W2-SK-22

    // gF MCV deploy [RE: docs/re/mcv-deploy.md]: the turn-then-unfold mission.
    // `if (!deploy_any_) return;` inside -- no gate issues a deploy order.
    step_deploy(dt);

    // CB-2 Aircraft [RE: aircraft.md §3.5/§6]: the airborne movement pass --
    // takeoff/cruise/descend runs AFTER step_cargo and BEFORE step_turrets/
    // step_combat, so the aircraft's post-move altitude/position is visible to
    // combat targeting the SAME frame. A pure INSERTION; `if (!aircraft_any_)
    // return;` inside is the only cost every existing demo pays [spec §8].
    step_flight(dt);

    step_air_naval(dt);   // W2-AN: subs/sonar, spawners, fixed-wing, crashes (world_an.cpp)

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

    // Repair & Sell [RE: repair-sell.md §A2/§B2]: the wrench tick + the sell
    // missions. A pure INSERTION beside step_cargo/step_capture -- it runs
    // AFTER step_buildings (so a building that flipped Buildup->Idle this frame
    // is already repairable, and a building sold LAST frame has already had its
    // foundation released above) and BEFORE step_combat (so a completed sale is
    // Dead before targeting runs). `if (!repair_sell_any_) return;` inside is
    // the only cost every existing demo pays -- no demo issues a repair or sell
    // order [spec §Global Constraints].
    step_repair_sell(dt);

    // Projectile phase (P2) [RE: projectile.md §6/§7]: advance every projectile
    // in flight one logic frame per accumulated 1/15 s, detonating through the
    // EXISTING damage layer. A pure INSERTION before step_combat (a shell
    // launched this frame first moves next frame -- §6.3 "detonates on the next
    // Update"); `if (!projectiles_enabled_) return;` inside is the only cost
    // every existing demo pays (none calls enable_projectiles).
    step_projectiles(dt);

    // P2 firewall: the launch sink is passed ONLY when projectiles are enabled;
    // nullptr keeps step_combat's instant-hit path byte-identical.
    std::vector<ProjectileLaunch> launches;
    const std::vector<MoveRequest> reqs = step_combat(*this, units_, buildings_, occ_, dt,
                                                      projectiles_enabled_ ? &launches : nullptr,
                                                      &combat_rng_);   // G-50 ROF jitter
    for (const ProjectileLaunch& l : launches)
        launch_projectile(l.attacker, l.target_unit, l.target_building);
    // Gap fix C: a FireEvent for every unit that fired this step (try_fire /
    // the building branch stamp fire_flash = 0.4 exactly at the shot).
    for (int i = 0; i < static_cast<int>(units_.size()); ++i) {
        const Unit& u = units_[i];
        if (u.fire_flash == 0.4 && u.alive && u.die_time < 0.0)
            fire_events_.push_back({i, -1, u.target, u.target_building, u.w_active, ""});
    }
    // Gap fix C (G-40) [RE: docs/re/defenses.md]: base defenses acquire, aim
    // and fire. Only types resolved with a weapon (BuildingTypeDef::armed).
    step_building_combat(dt);
    // G16 world depth (agent G): garrisons + ground shots / bridges. Inert
    // unless a garrison order was given or a unit has a ground target.
    step_depth(dt);
    // Wave-5 abilities (agent C, world_w5.cpp): stolen tech, prism/tesla/gap,
    // tech buildings, transports. Inert without wave-5 type data.
    step_w5(dt);
    // P5 Veterancy [RE: veterancy.md V2/V3/V11/V12]: kill credit for every unit/
    // building that died this step, rank-change detection + stat refresh, and
    // the 14-frame self-heal. A pure INSERTION right after step_combat; the
    // frame counter advances unconditionally (nothing hashes it) and
    // `if (!vet_any_) return;` is the only other cost every existing gate pays.
    step_veterancy();
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
    std::map<std::pair<int, int>, std::vector<int>> amphib_groups;   // W6-04-07
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
            // W5-B: an approach request names the TARGET's (occupied) cell;
            // a ship heads for the nearest free cell of its own domain grid
            // beside it (the exact water A* to the target's cell itself could
            // fail and left the Giant Squid parked out of its 1.5-cell range).
            int gx = r.goal_x, gy = r.goal_y;
            if (r.approach) {
                const std::vector<Cellxy> g =
                    nearest_free_cells({r.goal_x, r.goal_y}, 1, grid_for(u.movement_domain), grid_w_, grid_h_, occ_);
                if (!g.empty()) { gx = g[0].x; gy = g[0].y; }
            }
            order_unit_move(r.unit, gx, gy);
            continue;
        }
        // W6-04-07 [RE: naval.md §1/§4 -- the approach uses the unit's own
        // MovementZone; Tanya / SEAL are AmphibiousDestroyer]: an amphibious
        // unit's firing position is chosen on the Land-OR-Water grid, so a
        // swimming Tanya reaches a Destroyer and a SEAL on the shore enters the
        // water to plant C4 (the Land-grid batch found no cell next to a ship).
        // Only an EXPLICIT attack order on a target the land grid cannot
        // reach (a ship on water) switches grids [RE-FALLBACK scope: the idle
        // guard-leash chase (W2-SK-25) stays on land, so a SEAL raid does not
        // wander off after ships -- ALL11 depends on it]; a land target keeps
        // the land batch exactly as before.
        if (r.approach && u.attack_order && u.movement_domain == MovementDomain::Amphibious &&
            r.goal_x >= 0 && r.goal_y >= 0 && r.goal_x < grid_w_ && r.goal_y < grid_h_ &&
            !passable_[static_cast<std::size_t>(r.goal_y) * grid_w_ + r.goal_x] &&
            !water_passable_.empty() && water_passable_[static_cast<std::size_t>(r.goal_y) * grid_w_ + r.goal_x])
            amphib_groups[{r.goal_x, r.goal_y}].push_back(r.unit);
        else if (r.approach)
            approach_groups[{r.goal_x, r.goal_y}].push_back(r.unit);
        else
            order_unit_move(r.unit, r.goal_x, r.goal_y);
    }
    auto dispatch_groups = [&](const std::map<std::pair<int, int>, std::vector<int>>& groups,
                               const std::vector<std::uint8_t>& grid) {
        for (const auto& group : groups) {
            const std::vector<int>& movers = group.second;
            std::vector<Cellxy> cur;
            cur.reserve(movers.size());
            for (int idx : movers) cur.push_back({units_[idx].cell_x, units_[idx].cell_y});
            const std::vector<Cellxy> targets =
                assign_formation({group.first.first, group.first.second}, cur, grid,
                                 grid_w_, grid_h_, occ_);
            for (std::size_t k = 0; k < movers.size(); ++k)
                order_unit_move(movers[k], targets[k].x, targets[k].y);
        }
    };
    dispatch_groups(approach_groups, passable_);
    if (!amphib_groups.empty()) {
        const std::vector<std::uint8_t> amphib = amphib_grid();   // cached buffer: copy once
        dispatch_groups(amphib_groups, amphib);
    }

    // Economy phase [RE: economy.md]: ore growth/spread + harvester mission
    // stepping, gated on set_economy having been called (types_ non-empty).
    // A World that never calls set_economy (every pre-economy-phase caller,
    // including demo-base's World -- see the byte-identity note in
    // task-6-brief.md) steps with zero ore/harvest activity: ore_.step is
    // also gated (not just step_harvest), so ore().timer() stays exactly 0.
    if (!types_.empty()) {
        ore_.step(dt, growth_interval_, growth_pct_, spread_pct_, passable_, ore_rng_, &occ_);
        // W2-SK-01: ore mines (TerrainClass::AI per logic frame).
        if (!ore_.mines().empty())
            ore_.step_mines(std::max(1, (int)std::llround(dt * 15.0)), passable_, ore_rng_);
        std::vector<HarvestMove> h_moves;
        std::vector<HarvestEvent> h_events;
        // Gap owner A (G-27): own operational Ore Purifiers per house.
        std::vector<int> purifiers(house_credits_.size(), 0);
        bool any_purifier = false;
        for (const Building& b : buildings_)
            if (b.alive && b.state == Building::State::Idle && b.type >= 0 &&
                b.type < static_cast<int>(types_.size()) && types_[b.type].ore_purifier &&
                b.house >= 0 && b.house < static_cast<int>(purifiers.size())) {
                ++purifiers[b.house];
                any_purifier = true;
            }
        // W6-03-03 [RE: FUN_00522a80]: + AIVirtualPurifiers for a computer
        // house (0 outside skirmish -> the purifier vector is unchanged).
        for (int h = 0; h < (int)ai_houses_.size() && h < (int)purifiers.size(); ++h)
            if (ai_houses_[h].is_ai && ai_houses_[h].cfg.virtual_purifiers > 0) {
                purifiers[h] += ai_houses_[h].cfg.virtual_purifiers;
                any_purifier = true;
            }
        const std::vector<double> before_harvest = house_credits_;   // W2-SK income witness
        step_harvest(units_, buildings_, types_, refinery_types_, ore_, passable_, grid_w_,
                    grid_h_, occ_, dt, OreValues{ore_value_ore_, ore_value_gems_},
                    house_credits_.data(), h_moves, h_events,
                    static_cast<int>(house_credits_.size()),
                    any_purifier ? purifiers.data() : nullptr, purifier_bonus_, harvest_scan_);
        if (harvest_income_.size() < house_credits_.size()) harvest_income_.resize(house_credits_.size(), 0.0);
        for (std::size_t h = 0; h < house_credits_.size() && h < before_harvest.size(); ++h)
            harvest_income_[h] += house_credits_[h] - before_harvest[h];
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
        if (u.crushing) continue;   // W5-B R-050: driving onto a crush target, not holding at range
        if (u.target < 0 || !u.attack_order || u.target >= (int)units_.size()) continue;   // only explicit attackers hold at range
        const Unit& t = units_[u.target];
        if (!t.alive || t.die_time >= 0.0) continue;
        // W5-B R-063: the same 3-D / elevation / cliff test step_combat fires
        // on, measured from the committed cell (Finding-1 semantics above).
        if (!u.path.empty() &&
            weapon_in_range_from(*this, u, u.cell_x * 256 + static_cast<int>(u.sub_x),
                                 u.cell_y * 256 + static_cast<int>(u.sub_y), unit_lept_z(u),
                                 unit_lept_x(t), unit_lept_y(t), unit_lept_z(t))) {
            // In range with a live pursue-path -> stop and hold (snap onto the
            // committed cell so we don't strand mid-segment; Finding-1 semantics).
            u.path.clear();
            u.path_idx = 0;
            u.fx = u.cell_x;
            u.fy = u.cell_y;
        }
    }
    // R3 [RE: infantry-anim.md §7 -- Mission_Attack re-checks weapon range
    // every frame while approaching ANY target and stops as soon as it is in
    // range; FUN_006F6750 in-range test, a building measured to its centre
    // with the range widened by (w+h)*64]: the same hold for an explicit
    // attack on a BUILDING. The clone held only unit targets, so a unit
    // ordered onto a structure out of range drove up to its foundation (V3s
    // into SOV08's base, an IFV into SOV09's Prism compound). A C4 sapper is
    // left alone: its approach goal is already a plant position.
    for (Unit& u : units_) {
        if (!u.alive || u.die_time >= 0.0 || u.crushing || u.path.empty()) continue;
        if (u.target >= 0 || !u.attack_order || u.abil.c4) continue;
        const int tb = u.target_building;
        if (tb < 0 || tb >= static_cast<int>(buildings_.size())) continue;
        const Building& b = buildings_[static_cast<std::size_t>(tb)];
        if (!b.alive || b.state == Building::State::Dead) continue;
        int bax = 0, bay = 0, baz = 0;
        building_aim_lept(b, cell_level(b.cx, b.cy), bax, bay, baz);
        if (weapon_in_range_from(*this, u, u.cell_x * 256 + static_cast<int>(u.sub_x),
                                 u.cell_y * 256 + static_cast<int>(u.sub_y), unit_lept_z(u), bax, bay, baz,
                                 (b.fw + b.fh) * 64)) {
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

        // W3-B: the live game's infantry sequence machine (inf_anim.cpp),
        // one step per 15 Hz logic tick of accumulated time.
        if (inf_anims_ && !u.is_vehicle && !u.is_aircraft && !u.is_naval && u.cargo_of < 0) {
            const InfTable* it = inf_table(u.image);
            if (it) {
                const int ui = static_cast<int>(&u - units_.data());
                if (!u.ia.armed) {
                    u.ia.armed = true;
                    u.ia.fearless = it->fearless;
                    u.ia.fraidycat = it->fraidycat;
                    if (u.ia.rng == 0) u.ia.rng = static_cast<std::uint32_t>(ui) * 2654435761u + 0x9E3779B9u;
                }
                InfTickCtx c;
                c.moving = u.moving();
                c.has_destination = !u.path.empty();
                c.human = u.house == inf_human_;
                const std::size_t ci = static_cast<std::size_t>(u.cell_y) * grid_w_ + u.cell_x;
                c.in_water = u.cell_x >= 0 && u.cell_y >= 0 && ci < water_passable_.size() && water_passable_[ci];
                c.has_target = u.target >= 0 || u.target_building >= 0;
                c.dying = u.die_time >= 0.0;
                c.deployed = u.deployed;
                c.fire_flash = u.fire_flash;
                c.idle_frequency = inf_idle_freq_;
                if (it->fraidycat) c.scatter = [this, ui]() { order_scatter(ui); };
                u.ia.acc += dt * 15.0;
                bool stepped = false;
                while (u.ia.acc >= 1.0 - 1e-9) {
                    u.ia.acc -= 1.0;
                    u.frame = inf_anim_tick(u.ia, *it, u.facing, c);
                    stepped = true;
                }
                if (!stepped) u.frame = inf_anim_frame(u.ia, *it, u.facing);
                continue;
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

    // Multi-house [RE: multihouse.md §5]: evaluate last-team-standing victory
    // AFTER combat has resolved this frame's deaths. Firewalled behind
    // victory_enabled_ (false in every existing gate -> a strict no-op).
    //
    // P4 Unit special abilities [RE: abilities.md]: the ability pass runs
    // AFTER combat (so this frame's intercepted fire events and deaths are
    // resolved the same frame) and BEFORE victory (so an ability kill counts).
    // A pure INSERTION; `if (!abilities_any_) return;` (after the frame tick)
    // is the only cost every pre-P4 demo pays -- none registers a profile.
    step_abilities(dt);
    step_victory();
    update_sub_offsets();   // W5-B R-061: derive the infantry sub-cell offsets
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
            // W5-B R-061: "can this unit enter" -- an infantry may share a
            // cell that still has a free sub-cell spot (whole-cell otherwise).
            if (!occ.can_enter(x, y, self))
                out[static_cast<std::size_t>(y) * w + x] = 0;
        }
    if (start_x >= 0 && start_y >= 0 && start_x < w && start_y < h)
        out[static_cast<std::size_t>(start_y) * w + start_x] = 1;
    return out;
}

double land_speed_mult(const MoveSpeedCtx& c, const Unit& u, int w, int h, int nx, int ny) {
    // Only the Drive/Ship/Hover locomotors read the GroundType table for speed
    // (FUN_004B2550 / FUN_006A1310); the Walk locomotor (infantry) does not.
    if (!c.armed || !c.land || (!u.is_vehicle && !u.is_naval) || u.is_aircraft || !u.land_speed_loco)
        return 1.0;
    if (nx < 0 || ny < 0 || nx >= w || ny >= h) return 1.0;
    const std::size_t i = static_cast<std::size_t>(ny) * w + nx;
    if (i >= c.land->size()) return 1.0;
    int land = (*c.land)[i];
    if (c.ore && c.ramp && i < c.ramp->size() && c.ore->has_ore(nx, ny) && (*c.ramp)[i] < 5) land = 5;
    if (land < 0 || land > 11) land = 0;
    int st = u.speed_type;
    if (st < 0 || st > 7) st = u.is_naval ? 5 : 1;   // default: Track (Drive), Float (ship)
    double m = c.cost[land][st];
    if (m > 1.0) m = 1.0;
    // Slope (UnitClass only): the next cell higher -> *Uphill, lower -> *Downhill;
    // SpeedType Track uses the Tracked pair, every other type the Wheeled pair.
    if (c.level && !u.is_naval) {
        const std::size_t ci = static_cast<std::size_t>(u.cell_y) * w + u.cell_x;
        if (u.cell_x >= 0 && u.cell_y >= 0 && u.cell_x < w && u.cell_y < h && ci < c.level->size()) {
            const int cur = (*c.level)[ci], nxt = (*c.level)[i];
            const int k = st == 1 ? 0 : 1;
            if (cur < nxt) m *= c.up[k];
            else if (nxt < cur) m *= c.down[k];
        }
    }
    if (m == 0.0) m = 0.5;
    if (u.max_hp > 0 && static_cast<double>(u.hp) / u.max_hp <= c.condition_red) m *= 0.75;
    return m;
}

void step_units(std::vector<Unit>& units, const std::vector<std::uint8_t>& passable,
                const std::vector<std::uint8_t>& water_passable, int w, int h,
                Occupancy& occ, double dt, double move_cps, const AnimTable& anims,
                const std::vector<std::uint8_t>* amphib_passable, const MoveSpeedCtx* speed,
                const std::vector<std::uint8_t>* wall_leak, const std::vector<std::uint8_t>* crush_passable,
                const std::vector<std::uint8_t>* crush_leak, const StepTerrain* terrain) {
    // R-210: unit u's terrain (null without one -> the 2-D walk).
    auto terrain_of_unit = [&](const Unit& u, PathTerrain& t) -> const PathTerrain* {
        if (!terrain) return nullptr;
        t = u.movement_domain == MovementDomain::Water ? terrain->water
            : u.movement_domain == MovementDomain::Amphibious ? terrain->amph : terrain->land;
        t.start_layer = t.deck ? u.on_deck : 0;
        t.open_cells = nullptr;
        if (terrain->gates && u.movement_domain != MovementDomain::Water && u.house >= 0 &&
            u.house < static_cast<int>(terrain->gates->size()) && !(*terrain->gates)[(std::size_t)u.house].empty())
            t.open_cells = &(*terrain->gates)[(std::size_t)u.house];
        return (t.level || t.open_cells) ? &t : nullptr;
    };
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
        // W5-B R-062: x the land/slope/damage multiplier the locomotor set
        // when it committed to the cell being entered (1.0 when unarmed).
        const double cps = (u.speed_cps > 0.0 ? u.speed_cps : move_cps) * u.speed_mult;
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
            // Gap fix C (G-47) [RE: rulesmd Crusher=/Crushable=; crush branch of
            // the Drive locomotor not unwound -- RE-W6 open]: a Crusher vehicle
            // driving into a cell held by enemy Crushable infantry squashes it
            // (the infantry dies, the cell frees) instead of waiting for it.
            // W5-B R-061: with sub-cells, every crushable enemy infantry in the
            // cell's three spots is squashed.
            if (u.crusher && u.is_vehicle) {
                int victims[4] = {occ.at(tgt.x, tgt.y), -1, -1, -1};
                if (occ.subcells())
                    for (int s = Occupancy::kFirstSpot; s < Occupancy::kFirstSpot + Occupancy::kSpots; ++s)
                        victims[s - 1] = occ.spot_unit(tgt.x, tgt.y, s);
                for (int o : victims) {
                    if (o < 0 || o == self || o >= static_cast<int>(units.size())) continue;
                    Unit& v = units[o];
                    if (can_crush(u, v) && v.house != u.house && v.alive &&   // W6-04-14/29
                        v.die_time < 0.0 && v.cargo_of < 0) {
                        v.hp = 0;
                        v.die_time = 0.0;
                        v.vet.killed_by = self;
                        v.target = -1;
                        v.target_building = -1;
                        v.path.clear();
                        v.path_idx = 0;
                        occ.release_all_of(o, -1, -1);
                    }
                }
            }
            // R-210 [RE: FUN_004D9B70]: the height gate on every step -- a cliff
            // (or a deck that is gone) refuses it; the re-path below finds a
            // legal way. `step_layer` is the deck layer the unit lands on.
            int step_layer = u.on_deck;
            if (terrain) {
                PathTerrain t;
                if (const PathTerrain* tp = terrain_of_unit(u, t); tp && tp->level) {
                    const int dx = tgt.x - u.cell_x, dy = tgt.y - u.cell_y;
                    step_layer = (dx < -1 || dx > 1 || dy < -1 || dy > 1)
                                     ? -1
                                     : path_step_layer(*tp, w, u.cell_x, u.cell_y, u.on_deck, tgt.x, tgt.y);
                    if (step_layer < 0) {
                        blocked = true;
                        break;
                    }
                }
            }
            // Reserve the next cell before stepping into it.
            if (occ.is_infantry(self)) {
                // W5-B R-061 [RE: FindInfantrySubposition FUN_00481090]: an
                // infantry reserves a SPOT in the next cell -- the quadrant of
                // its current in-cell position (it keeps its lane), else the
                // spot preference row; no free spot (or a vehicle there) blocks.
                if (occ.spot_of(tgt.x, tgt.y, self) < 0) {
                    int cur = occ.spot_of(u.cell_x, u.cell_y, self);
                    if (cur < 0) cur = 0;
                    const int sp = occ.free_spot(tgt.x, tgt.y, cur, self & 3);
                    if (sp < 0) {
                        blocked = true;
                        break;
                    }
                    occ.set_spot(tgt.x, tgt.y, self, sp);
                }
            } else if (occ.at(tgt.x, tgt.y) != self || !occ.can_enter(tgt.x, tgt.y, self)) {
                if (occ.can_enter(tgt.x, tgt.y, self)) {
                    occ.set(tgt.x, tgt.y, self);  // reserve (now owns current + next)
                    if (speed) u.speed_mult = land_speed_mult(*speed, u, w, h, tgt.x, tgt.y);   // W5-B R-062
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
                if (terrain) u.on_deck = static_cast<std::uint8_t>(step_layer > 0 ? 1 : 0);   // R-210
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
            // W5-B R-050: a vehicle held up by idle infantry of its own house
            // asks them to scatter out of the way (the original's
            // Map[cell].Incoming -> Scatter for friendly occupants).
            if (speed && speed->scatter_out && u.is_vehicle && occ.subcells() && u.wait_time == 0.0 &&
                u.path_idx < u.path.size()) {   // once per hold-up, not every frame
                const Cellxy bc = u.path[u.path_idx];
                for (int s = Occupancy::kFirstSpot; s < Occupancy::kFirstSpot + Occupancy::kSpots; ++s) {
                    const int o = occ.spot_unit(bc.x, bc.y, s);
                    if (o >= 0 && o < static_cast<int>(units.size()) && units[o].house == u.house &&
                        !units[o].moving() && units[o].target < 0 && units[o].enter_target < 0)
                        speed->scatter_out->push_back(o);
                }
            }
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
                const bool crush = crush_passable && u.crusher && u.is_vehicle &&
                                   u.movement_domain == MovementDomain::Land;   // P4
                const std::vector<std::uint8_t>& g =
                    u.movement_domain == MovementDomain::Water ? water_passable
                    : (u.movement_domain == MovementDomain::Amphibious && amphib_passable)
                        ? *amphib_passable : crush ? *crush_passable : passable;
                std::vector<std::uint8_t> p2 =
                    passable_minus_units(g, w, h, occ, self, u.cell_x, u.cell_y);
                // W2-SK-16: a detour try is budgeted (a crowd-sealed goal used
                // to flood the whole grid every 0.3 s per blocked unit).
                PathTerrain rt;
                std::vector<Cellxy> np =
                    find_path_bounded_t(p2, w, h, {u.cell_x, u.cell_y}, goal, kRepathBudget,
                                        u.movement_domain == MovementDomain::Water ? nullptr
                                        : crush ? crush_leak : wall_leak,
                                        terrain_of_unit(u, rt));
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

// ============================================================================
// Projectile phase (P2) [RE: projectile.md + §12] -- the World half of
// BulletClass: launch (muzzle/target coords), per-frame termination tests
// (§7 table, spec C2) and detonation through the EXISTING damage layer
// (spec C4). Firewalled behind projectiles_enabled_.
// ============================================================================
namespace {
constexpr double kProjFrameSecs = 1.0 / 15.0;  // one logic frame (§3: lept/frame)
// Engine guard (not RE'd): a projectile still airborne after 2 minutes of
// logic frames is removed silently -- reachable only by a degenerate level
// shot in a gridless (unit-test) World that has no map edge.
constexpr int kProjMaxLifetime = 1800;
constexpr double kProjPi = 3.14159265358979323846;

int unit_lx(const Unit& u) { return ftol(u.fx * 256.0 + u.sub_x); }   // W5-B: + sub-cell spot
int unit_ly(const Unit& u) { return ftol(u.fy * 256.0 + u.sub_y); }
int unit_lz(const Unit& u) { return u.is_aircraft ? ftol(u.altitude) : 0; }
int dist3(int ax, int ay, int az, int bx, int by, int bz) {
    const double dx = ax - bx, dy = ay - by, dz = az - bz;
    return ftol(std::sqrt(dx * dx + dy * dy + dz * dz));
}
}  // namespace

void World::enable_projectiles(const ProjectileRules& rules) {
    projectiles_enabled_ = true;
    proj_rules_ = rules;
}

std::vector<ProjectileEvent> World::take_projectile_events() {
    std::vector<ProjectileEvent> out;
    out.swap(proj_events_);
    return out;
}

void World::step_projectiles(double dt) {
    if (!projectiles_enabled_) return;  // the firewall
    proj_accum_ += dt;
    // Whole logic frames only (spec D2): the integrator has no dt (§6.1).
    while (proj_accum_ >= kProjFrameSecs - 1e-9) {
        proj_accum_ -= kProjFrameSecs;
        ++proj_frame_;
        step_projectiles_frame();
    }
}

void World::launch_projectile(int attacker, int target_unit, int target_building) {
    if (attacker < 0 || attacker >= static_cast<int>(units_.size())) return;
    const Unit& u = units_[attacker];
    const ProjectileSpec& spec = u.w_proj;

    // Target coords (MoveTo: Target->GetTargetCoords(), §12.4). A building is
    // aimed at its NEAREST foundation cell -- the same cell step_combat ranged.
    Vec3i tgt;
    if (target_unit >= 0 && target_unit < static_cast<int>(units_.size())) {
        const Unit& v = units_[target_unit];
        tgt = Vec3i{unit_lx(v), unit_ly(v), unit_lz(v)};
    } else if (target_building >= 0 && target_building < static_cast<int>(buildings_.size())) {
        const Building& b = buildings_[target_building];
        double best = -1.0;
        int nx = b.cx, ny = b.cy;
        for (const Cellxy& c : foundation_cells(b.cx, b.cy, b.fw, b.fh)) {
            const double dx = (c.x - u.fx) * 256.0, dy = (c.y - u.fy) * 256.0;
            const double d = std::hypot(dx, dy);
            if (best < 0.0 || d < best) { best = d; nx = c.x; ny = c.y; }
        }
        tgt = Vec3i{nx * 256, ny * 256, 0};
    } else {
        return;
    }

    // Muzzle = firer + PrimaryFireFLH (§4 step 1; voxel-render.md §6). F runs
    // along the turret yaw (a vehicle only fires once vehicle_aimed, so it points
    // at the target); facing u16 -> cell-space direction (sin a, -cos a), the
    // facing_u16_from_delta convention. Non-vehicles aim F at the target.
    const double ux = u.fx * 256.0, uy = u.fy * 256.0;
    double fwx = 0.0, fwy = 1.0;
    if (u.is_vehicle) {
        const double a = u.turret_facing * (2.0 * kProjPi / 65536.0);
        fwx = std::sin(a);
        fwy = -std::cos(a);
    } else {
        const double dx = tgt.x - ux, dy = tgt.y - uy, len = std::hypot(dx, dy);
        if (len > 0.0) { fwx = dx / len; fwy = dy / len; }
    }
    Vec3i src;
    src.x = ftol(ux + spec.flh[0] * fwx - spec.flh[1] * fwy);
    src.y = ftol(uy + spec.flh[0] * fwy + spec.flh[1] * fwx);
    src.z = unit_lz(u) + spec.flh[2];

    const LaunchResult lr = compute_launch(spec, proj_rules_, src, tgt, proj_rng_);
    ProjectileEvent ev;
    ev.frame = proj_frame_;
    ev.attacker = attacker;
    ev.x = src.x; ev.y = src.y; ev.z = src.z;
    ev.speed = lr.speed;
    ev.scatter_dx = lr.scatter_dx;
    ev.scatter_dy = lr.scatter_dy;
    if (!lr.ok) {
        // §4 step 7: the arcing solver had no root -> the bullet is destroyed and
        // the shot does not happen (ROF was already reset, as in Fire).
        ev.kind = ProjectileEvent::Refused;
        proj_events_.push_back(ev);
        return;
    }
    ev.kind = ProjectileEvent::Launch;
    proj_events_.push_back(ev);

    Projectile p;
    p.kind = spec.kind;
    p.attacker = attacker;
    p.target_unit = target_unit;
    p.target_building = target_building;
    p.x = src.x; p.y = src.y; p.z = src.z;
    p.vx = lr.vx; p.vy = lr.vy; p.vz = lr.vz;
    p.tx = tgt.x; p.ty = tgt.y; p.tz = tgt.z;
    p.speed_cap = spec.speed_cap;
    p.acceleration = spec.acceleration;
    p.detonation_altitude = spec.detonation_altitude;
    p.inaccurate = spec.inaccurate;
    p.floater = spec.floater;
    p.launch_frame = proj_frame_;
    p.launch_speed = lr.speed;
    p.max_z = src.z;
    p.weapon_slot = u.w_active;   // Gap fix C: detonate with the slot that fired
    if (p.kind == ProjKind::Homing) {
        init_homing(p, spec, lr.speed);
        p.target_air = target_unit >= 0 && unit_airborne(units_[target_unit]);
    }
    projectiles_.push_back(p);
}

void World::init_homing(Projectile& p, const ProjectileSpec& spec, double launch_speed) {
    p.uid = proj_seq_++;
    p.rot = spec.rot;
    p.speed = launch_speed;   // §12.3: a homing bullet leaves at 1
    p.level = spec.level;
    p.airburst = spec.airburst;
    p.very_high = spec.very_high;
    p.course_lock = spec.course_lock;
    p.lock_frames = 0;
    p.locked = true;
    p.giveup_n = 0;
    p.giveup_acc = 0.0;
    p.prev_dist = -1;
    p.lost = false;
    if (p.level) p.vz = 0.0;   // torpedoes run level
    else if (p.z < 1) p.z = 1;   // [RE-FALLBACK] a muzzle with no FLH height (H=0) still starts in the air
}

bool World::homing_target(Projectile& p, Vec3i& out) const {
    if (p.target_unit >= 0 && p.target_unit < static_cast<int>(units_.size())) {
        const Unit& t = units_[p.target_unit];
        if (!t.alive || t.die_time >= 0.0 || t.cargo_of >= 0) return false;
        out = Vec3i{unit_lx(t), unit_ly(t), unit_lz(t)};
        return true;
    }
    if (p.target_building >= 0 && p.target_building < static_cast<int>(buildings_.size())) {
        const Building& b = buildings_[p.target_building];
        if (!b.alive || b.state == Building::State::Dead) return false;
        out = Vec3i{p.tx, p.ty, 0};   // the foundation cell aimed at launch
        return true;
    }
    return false;
}

void World::step_projectiles_frame() {
    std::vector<Projectile> live;
    live.reserve(projectiles_.size());
    // Launch order (ascending attacker index within a frame): deterministic.
    // Detonations apply damage immediately, so a later projectile this frame
    // sees the updated HP -- the original's sequential Logic-layer update.
    const std::vector<Projectile> flying = projectiles_;
    // W4 (W4-PJ-16): one O(U) pass builds each cell's ground occupants in
    // ascending unit index; first_unit_in_cell then reads only its cell
    // instead of scanning every unit per projectile. Nothing moves during the
    // projectile pass, so the answer is the old ascending scan's exactly.
    std::vector<int> cell_head, cell_next;
    const bool grid = grid_w_ > 0 && grid_h_ > 0 && !flying.empty();
    if (grid) {
        cell_head.assign(static_cast<std::size_t>(grid_w_) * grid_h_, -1);
        cell_next.assign(units_.size(), -1);
        for (int j = static_cast<int>(units_.size()) - 1; j >= 0; --j) {
            const Unit& w = units_[j];
            if (!w.alive || w.die_time >= 0.0 || w.cargo_of >= 0 || w.is_aircraft) continue;
            const int ux = lepton_to_cell(unit_lx(w)), uy = lepton_to_cell(unit_ly(w));
            if (ux < 0 || uy < 0 || ux >= grid_w_ || uy >= grid_h_) continue;
            const std::size_t c = static_cast<std::size_t>(uy) * grid_w_ + ux;
            cell_next[static_cast<std::size_t>(j)] = cell_head[c];
            cell_head[c] = j;
        }
    }
    const HomingRules hr{proj_rules_.rot_var, proj_rules_.safety_altitude};
    for (Projectile p : flying) {
        if (p.kind == ProjKind::Homing) {
            // W4: the homing branch (FUN_004666D0 ROT>=1 + FUN_005B1B20).
            Vec3i tgt;
            if (!p.lost && homing_target(p, tgt)) {
                p.tx = tgt.x; p.ty = tgt.y; p.tz = tgt.z;
            } else if (!p.lost && p.target_air) {
                p.lost = true;   // an air target died: climb out to MissileSafetyAltitude
            }
            const int r = integrate_homing(p, Vec3i{p.tx, p.ty, p.tz}, proj_frame_, hr);
            const int hcx = lepton_to_cell(p.x), hcy = lepton_to_cell(p.y);
            const int age = proj_frame_ - p.launch_frame;
            const bool off_map = grid_w_ > 0 && grid_h_ > 0 &&
                                 (hcx < 0 || hcy < 0 || hcx >= grid_w_ || hcy >= grid_h_);
            if (r == 0 && (off_map || age >= kProjMaxLifetime)) {
                ProjectileEvent ev;
                ev.kind = ProjectileEvent::Removed;
                ev.frame = proj_frame_;
                ev.attacker = p.attacker;
                ev.x = p.x; ev.y = p.y; ev.z = p.z;
                ev.flight_frames = age;
                ev.apex = p.max_z;
                proj_events_.push_back(ev);
                continue;
            }
            if (r != 0) {
                if (p.z < 0) p.z = 0;
                int victim = -1;
                if (r == 1 && p.target_unit >= 0 && p.target_unit < static_cast<int>(units_.size()) &&
                    units_[p.target_unit].alive && units_[p.target_unit].die_time < 0.0)
                    victim = p.target_unit;
                detonate_projectile(p, victim);
                continue;
            }
            live.push_back(p);
            continue;
        }
        if (p.kind == ProjKind::Vertical)
            integrate_vertical(p);
        else
            integrate_ballistic(p, proj_rules_.gravity);

        const int cx = lepton_to_cell(p.x), cy = lepton_to_cell(p.y);
        int owner_house =
            (p.attacker >= 0 && p.attacker < static_cast<int>(units_.size())) ? units_[p.attacker].house
                                                                               : -1;
        // Gap fix C: a base defense's shell belongs to the building's house.
        if (p.attacker < 0 && p.attacker_building >= 0 &&
            p.attacker_building < static_cast<int>(buildings_.size()))
            owner_house = buildings_[p.attacker_building].house;
        // ObjectAtCoord: the FIRST object in the bullet's cell (ascending unit
        // index stands in for the cell's occupier list). Carried units and
        // aircraft are not ground occupants (no AA in this phase).
        auto first_unit_in_cell = [&]() {
            if (grid) {
                if (cx < 0 || cy < 0 || cx >= grid_w_ || cy >= grid_h_) return -1;
                for (int j = cell_head[static_cast<std::size_t>(cy) * grid_w_ + cx]; j >= 0;
                     j = cell_next[static_cast<std::size_t>(j)]) {
                    const Unit& w = units_[j];
                    if (!w.alive || w.die_time >= 0.0 || w.cargo_of >= 0 || w.is_aircraft) continue;
                    return j;
                }
                return -1;
            }
            for (int j = 0; j < static_cast<int>(units_.size()); ++j) {
                const Unit& w = units_[j];
                if (!w.alive || w.die_time >= 0.0 || w.cargo_of >= 0 || w.is_aircraft) continue;
                if (lepton_to_cell(unit_lx(w)) == cx && lepton_to_cell(unit_ly(w)) == cy) return j;
            }
            return -1;
        };
        // "not the owner, not an ally" (§7 collision row).
        auto hostile = [&](int j) {
            return j != p.attacker && is_enemy(owner_house, units_[j].house);
        };

        bool det = false;
        int victim = -1;
        if (p.kind == ProjKind::Ballistic) {
            // §7 ground impact (floor = 0, the flat combat plane, §11.6; §12.5).
            if (p.z < 0) {
                det = true;
                p.z = 0;
            } else if (p.z < kGroundObjectBand) {
                // §12.2: within 150 above the floor over a non-owner, non-allied
                // object in the cell -> detonate.
                const int j = first_unit_in_cell();
                if (j >= 0) {
                    if (hostile(j)) { det = true; victim = j; }
                } else {
                    for (const Building& b : buildings_) {
                        if (!b.alive || b.state == Building::State::Dead) continue;
                        if (cx < b.cx || cy < b.cy || cx >= b.cx + b.fw || cy >= b.cy + b.fh) continue;
                        if (is_enemy(owner_house, b.house)) det = true;
                        break;
                    }
                }
            }
        } else {
            // §6.2: Vertical ends above DetonationAltitude or below the ground.
            if (p.detonation_altitude > 0 && p.z > p.detonation_altitude) {
                det = true;
            } else if (p.z < 0) {
                det = true;
                p.z = 0;
            }
        }
        // §7 arrival: in the TRUE target's cell (§12.4) and below 2*LevelHeight.
        if (!det && p.kind == ProjKind::Ballistic && cx == lepton_to_cell(p.tx) &&
            cy == lepton_to_cell(p.ty) && p.z < kArrivalHeight)
            det = true;
        // §5.4/§7 collision: the cell's first object, hostile, within 127 (3-D)
        // -> detonate, snapping onto it unless Inaccurate.
        if (!det) {
            const int j = first_unit_in_cell();
            if (j >= 0 && hostile(j)) {
                const Unit& w = units_[j];
                if (dist3(p.x, p.y, p.z, unit_lx(w), unit_ly(w), unit_lz(w)) <= kCollisionRadius) {
                    det = true;
                    victim = j;
                    if (!p.inaccurate) { p.x = unit_lx(w); p.y = unit_ly(w); p.z = unit_lz(w); }
                }
            }
        }
        const int age = proj_frame_ - p.launch_frame;
        // §7 off map: removed silently, no detonation. A gridless World (the
        // unit-test harness) has no map edge.
        const bool off_map = grid_w_ > 0 && grid_h_ > 0 &&
                             (cx < 0 || cy < 0 || cx >= grid_w_ || cy >= grid_h_);
        if (!det && (off_map || age >= kProjMaxLifetime)) {
            ProjectileEvent ev;
            ev.kind = ProjectileEvent::Removed;
            ev.frame = proj_frame_;
            ev.attacker = p.attacker;
            ev.x = p.x; ev.y = p.y; ev.z = p.z;
            ev.flight_frames = age;
            ev.apex = p.max_z;
            proj_events_.push_back(ev);
            continue;
        }
        // §7 out of speed: |V| < 8 (sampled pre-gravity), or |V| < 10 and height <= 9.
        if (!det && p.kind == ProjKind::Ballistic) {
            const double mag = std::sqrt(p.vx * p.vx + p.vy * p.vy + p.vz * p.vz);
            if (p.slow || (mag < kRestSpeed && p.z <= 9)) det = true;
        }
        // §12.2 IsBlocked: height <= -4*LevelHeight.
        if (!det && p.z <= kBlockedHeight) det = true;

        if (det) {
            detonate_projectile(p, victim);
            continue;
        }
        live.push_back(p);
    }
    projectiles_.swap(live);
}

void World::detonate_projectile(Projectile& p, int victim) {
    int ix = p.x, iy = p.y, iz = p.z;
    // Explode (§5.4): within 32 leptons of a live target, snap onto it --
    // suppressed by Inaccurate.
    if (!p.inaccurate && p.target_unit >= 0 && p.target_unit < static_cast<int>(units_.size())) {
        const Unit& t = units_[p.target_unit];
        if (t.alive && t.die_time < 0.0 &&
            dist3(ix, iy, iz, unit_lx(t), unit_ly(t), unit_lz(t)) <= kSnapRadius) {
            ix = unit_lx(t); iy = unit_ly(t); iz = unit_lz(t);
        }
        // W5-B [RE: BulletClass::Update tail @0x467D60..0x467E3E -- the
        // "close enough" re-aim]: a non-Inaccurate, non-Airburst shell whose
        // cell holds its target re-aims onto it when dist/3 <= max(2*|V|, 128)
        // (FUN_004666D0, DAT_007E1808 = 128.0). With infantry on sub-cell
        // spots this is what lets a tank shell that bursts on entering the
        // cell still land on the soldier.
        else if (!p.airburst && t.alive && t.die_time < 0.0 &&
                 lepton_to_cell(unit_lx(t)) == lepton_to_cell(ix) &&
                 lepton_to_cell(unit_ly(t)) == lepton_to_cell(iy)) {
            const double lim = std::max(2.0 * p.speed, 128.0);
            if (dist3(ix, iy, iz, unit_lx(t), unit_ly(t), unit_lz(t)) / 3 <= lim) {
                ix = unit_lx(t); iy = unit_ly(t); iz = unit_lz(t);
            }
        }
    }
    const int icx = lepton_to_cell(ix), icy = lepton_to_cell(iy);

    ProjectileEvent ev;
    ev.kind = ProjectileEvent::Detonate;
    ev.frame = proj_frame_;
    ev.attacker = p.attacker;
    ev.x = ix; ev.y = iy; ev.z = iz;
    ev.flight_frames = proj_frame_ - p.launch_frame;
    ev.apex = p.max_z;

    std::vector<int> hp_before(units_.size());
    for (std::size_t k = 0; k < units_.size(); ++k) hp_before[k] = units_[k].hp;
    int building_dmg = 0;

    // Gap fix C: the firer may have switched weapon slot while the shell flew;
    // detonate with the slot it was fired from, then restore.
    int restore_slot = -1;
    if (p.attacker >= 0 && p.attacker < static_cast<int>(units_.size()) &&
        units_[p.attacker].w_active != p.weapon_slot) {
        restore_slot = units_[p.attacker].w_active;
        activate_weapon_slot(units_[p.attacker], p.weapon_slot, vet_any_ ? &vet_rules_ : nullptr);
    }
    // W5-B R-060 [RE: BulletClass::Detonate FUN_004690A0 -> DamageArea
    // FUN_00489190]: every shell detonates through the one area applicator at
    // its impact point -- the target (snapped within 32 leptons), bystanders,
    // own/allied units and buildings in the CellSpread alike. The projectile Z
    // is terrain-relative; the applicator's is absolute (level*104 added).
    const int impact_z = iz + cell_level(icx, icy) * 104;
    const int tb = (p.target_building >= 0 && p.target_building < static_cast<int>(buildings_.size()))
                       ? p.target_building : -1;
    const int tb_before = tb >= 0 ? buildings_[tb].hp : 0;
    AreaResult ar;
    if (p.attacker >= 0 && p.attacker < static_cast<int>(units_.size())) {
        ar = damage_area(*this, units_, buildings_, occ_, units_[p.attacker], p.attacker, -1, ix, iy,
                         impact_z);
    } else if (p.attacker < 0 && p.attacker_building >= 0 &&
               p.attacker_building < static_cast<int>(buildings_.size()) &&
               buildings_[p.attacker_building].type >= 0 &&
               buildings_[p.attacker_building].type < static_cast<int>(btypes().size())) {
        // Gap fix C (G-40): a base defense's shell -- a transient shooter
        // carrying the building's weapon slot.
        const Unit shooter = defense_shooter(p.attacker_building, p.weapon_slot);
        ar = damage_area(*this, units_, buildings_, occ_, shooter, -1, p.attacker_building, ix, iy,
                         impact_z);
    }
    if (tb >= 0 && buildings_[tb].hp != tb_before) {
        building_dmg = tb_before - buildings_[tb].hp;
        ev.building = tb;
    }
    if (ar.first_unit >= 0)
        ev.victim = (p.target_unit >= 0 && p.target_unit < static_cast<int>(units_.size()) &&
                     units_[p.target_unit].hp != hp_before[static_cast<std::size_t>(p.target_unit)])
                        ? p.target_unit : ar.first_unit;
    // G16 (agent G) [RE: world-depth.md §B2, FUN_00489190]: the shell's
    // explosion may break a low bridge section in the impact cell (inert unless
    // GameScene armed the depth rules and the map has low bridges).
    if (depth_rules_.armed && !bridge_ovl_.empty() && p.attacker >= 0 &&
        p.attacker < static_cast<int>(units_.size())) {
        const Unit& att = units_[p.attacker];
        bridge_explosion(icx, icy, att.w_damage, att.w_active == 1 ? att.w2.wall : att.p_wall);
    }
    if (restore_slot >= 0)
        activate_weapon_slot(units_[p.attacker], restore_slot, vet_any_ ? &vet_rules_ : nullptr);
    (void)victim;   // the collided unit is simply inside the explosion (W5-B)
    (void)building_dmg;
    ev.damage = ar.damage;   // W5-B: every object the explosion hurt
    proj_events_.push_back(ev);
}

bool World::set_building_power(int b_idx, int control) {
    if (b_idx < 0 || b_idx >= static_cast<int>(buildings_.size())) return false;
    Building& b = buildings_[static_cast<std::size_t>(b_idx)];
    if (!b.alive || b.state == Building::State::Dead) return false;
    const auto& ts = btypes();
    if (b.type < 0 || b.type >= static_cast<int>(ts.size()) || ts[static_cast<std::size_t>(b.type)].power >= 0)
        return false;   // only consumers (PowerDrain > 0) can be powered down
    const bool off = control < 0 ? !b.power_off : control == 0;
    if (off == b.power_off) return false;
    b.power_off = off;
    return true;
}

}  // namespace ra2engine
