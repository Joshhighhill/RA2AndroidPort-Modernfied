#include "world.hpp"

#include <algorithm>
#include <cmath>

#include "combat.hpp"         // armor_index, step_combat, MoveRequest
#include "group_move.hpp"     // assign_formation
#include "iso_project.hpp"   // cell_to_screen (terrain_screen_bounds)
#include "iso_renderer.hpp"  // terrain_screen_bounds / pal_for_house declarations
#include "pathfind.hpp"
#include "ra2assets/pal.hpp"  // Palette/Rgba (pal_for_house)

namespace ra2engine {

// Per-image animation sequence tables (Task 7 -- Combat Phase renderer
// feedback). Replaces the GI-only file-static constants Phase 2 introduced
// (kStandFrame/kWalkFirst/kWalkCount) with a small per-image lookup so
// Conscripts (CONS.SHP) get their own, independently-derived ranges.
//
// EMPIRICAL DERIVATION (dump evidence -- see combat-task-7-report.md for the
// contact-sheet screenshots this was read from): both gi.shp (744 frames) and
// cons.shp (602 frames) were dumped frame-by-frame via `ra2extract <mix>
// conquer.mix/<gi|cons>.shp cache.mix/unittem.pal <out.png> <frame>` (the
// frame-index arg this task added) and inspected by eye. Both SHPs have real
// (non-degenerate) content only up to ~frame 300-370; every frame beyond that
// decodes as a 0x0 stub (an empty/invisible padding frame -- confirmed, not a
// dump bug). RA2 infantry SHPs lay every sequence out as a block of 8
// consecutive per-facing frames; only facing 0's block is used here (same
// simplification Phase 2 already made for the walk cycle: "facing not
// modelled").
//
//  GI.SHP (E1):
//   - stand=0: the stock "Ready" pose (Phase 2, task-10-report.md).
//   - walk=8..13 (6 frames): the stock "Walk" cycle (Phase 2, confirmed).
//   - fire=164..169 (6 frames): a clean 8-facing x 6-frame block (158..211)
//     of a standing side-on aim pose; frame 164 (facing 0's first frame)
//     shows a bright yellow muzzle-flash sprite at the M60's barrel, frames
//     165..169 the recoil/hold pose with no flash -- the classic
//     "flash-then-hold" cadence `fire_flash`'s progress formula expects.
//     HIGH confidence (the flash pixel is unambiguous and the 6-frame
//     facing-block cadence matches the walk cycle's own).
//   - die=80..87 (8 of the ~12 frames the dump shows, 80..91): a continuous
//     standing (back view) -> crumpling -> nearly-flat progression, no facing
//     repetition (a single death, not per-facing). The dump's fuller range
//     continues settling through frame 91, but die_count is capped at 8 so
//     the WHOLE sequence is reachable at kWalkFps(9.0, world.cpp) before
//     combat.cpp's kDieAnimSecs(1.0) tombstones the unit: the anim-override
//     pass's clamp formula (`die_first + min(die_count-1, die_time*kWalkFps)`)
//     only ever advances while `alive` is still true, and alive flips false
//     the instant die_time reaches 1.0s -- so any die_count > kWalkFps *
//     kDieAnimSecs (9 frames) would reserve frames that are provably never
//     drawn (a !alive unit is skipped both by this pass and by
//     unit_draw_order). MEDIUM confidence (the fall motion read is clear;
//     the exact frame where the game's OWN sequence table would end the
//     block wasn't independently cross-checked against an art.ini -- the
//     8-frame cap here is a pacing choice, not a re-derivation).
//  CONS.SHP (E2):
//   - stand=0: same convention as GI (frame 0 -- the Conscript's neutral
//     stance happens to already be a combat crouch holding the rifle
//     diagonally, unlike GI's arms-down Ready, but it's still frame 0).
//   - walk=8..13 (6 frames): LOW-MEDIUM confidence -- cons.shp's early
//     frames (0..69) are a continuous crouch-walk cycle with no visually
//     obvious "calm standing still" pose distinct from "mid-stride" (the
//     Conscript's art keeps a bent-knee combat crouch throughout), so the
//     exact Ready/Walk boundary couldn't be pinned by eye the way GI's could.
//     Reused GI's offsets on the (corroborated, see below) hypothesis that
//     both SHPs share one base template/frame layout.
//   - fire=158..163 (6 frames): a standing aim-and-crouch pose repeating in
//     a clean 6-frame facing cadence from 158 through ~259, with a much
//     subtler flash than GI's (small light-gray/white spark pixels near the
//     muzzle, e.g. frames 165/171/189/195-196) rather than one bright burst
//     -- consistent with a weaker M1 Carbine report. MEDIUM confidence (the
//     pose and cadence are clear; the flash pixel is small and its exact
//     in-block position less crisp than GI's).
//   - die=80..87 (same 8-of-12 cap as GI's, same reason): the SAME offsets as
//     GI's -- independently re-derived by eye from cons.shp's own dump (a
//     standing-back-view -> crumple -> lying-flat progression, frame-for-
//     frame at the same indices), which also corroborates the walk=8..13
//     reuse above (both SHPs evidently share one skeleton/frame-layout
//     template). HIGH confidence.
namespace {
const UnitAnimSeq kGIAnim{
    /*stand=*/0, /*walk_first=*/8, /*walk_count=*/6,
    /*fire_first=*/164, /*fire_count=*/6,
    /*die_first=*/80, /*die_count=*/8,
};
const UnitAnimSeq kConsAnim{
    /*stand=*/0, /*walk_first=*/8, /*walk_count=*/6,
    /*fire_first=*/158, /*fire_count=*/6,
    /*die_first=*/80, /*die_count=*/8,
};
}  // namespace

const UnitAnimSeq& anim_for_image(const std::string& image) {
    if (image == "CONS") return kConsAnim;
    return kGIAnim;  // "GI" and any unrecognized image -> GI table (documented fallback)
}

static constexpr double kWalkFps = 9.0;  // walk-cycle frame rate (shared cadence, all images)
static constexpr double kFireAnimFps = 15.0;  // Fire sequence pacing: the 6-frame recoil plays through ONCE per shot, spanning the kFireFlashSecs (0.4s) flash window exactly at the game's ~15Hz animation rate (6 frames / 0.4s = 15fps). Distinct from kWalkFps: walk cycles loop, fire plays once.

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
    return u;
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
        u.frame = anim_for_image(u.image).stand;
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
        // machinery never touch this unit's move path.
        u.target = -1;
        u.attack_order = false;
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

void World::step(double dt) {
    step_units(units_, passable_, grid_w_, grid_h_, occ_, dt, move_cps_);

    const std::vector<MoveRequest> reqs = step_combat(units_, occ_, dt);
    for (const MoveRequest& r : reqs) {
        if (r.unit < 0 || r.unit >= static_cast<int>(units_.size())) continue;
        const Unit& u = units_[r.unit];
        if (!u.alive || u.die_time >= 0.0) continue;  // died/started dying this same step
        order_unit_move(r.unit, r.goal_x, r.goal_y);
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
    for (Unit& u : units_) {
        if (!u.alive || u.die_time >= 0.0) continue;
        if (u.target < 0 || !u.attack_order || u.target >= (int)units_.size()) continue;   // only explicit attackers hold at range
        const Unit& t = units_[u.target];
        if (!t.alive || t.die_time >= 0.0) continue;
        const double dx = (t.fx - u.fx) * 256.0;
        const double dy = (t.fy - u.fy) * 256.0;
        if (std::sqrt(dx * dx + dy * dy) <= u.w_range * 256.0 && !u.path.empty()) {
            // In range with a live pursue-path -> stop and hold (snap onto the
            // committed cell so we don't strand mid-segment; Finding-1 semantics).
            u.path.clear();
            u.path_idx = 0;
            u.fx = u.cell_x;
            u.fy = u.cell_y;
        }
    }

    // Fire/die animation override (Task 7): step_units (above) already picked
    // a stand/walk frame assuming the unit is simply idle or walking; this
    // pass OVERRIDES that choice for a unit that is dying or mid-recoil from
    // a shot, using each image's fire/die ranges (anim_for_image). Runs AFTER
    // the hold-at-range pass (so a unit that just stopped to fire this same
    // frame still gets its fire pose) and BEFORE the level patch (ordering
    // vs. the level patch is otherwise irrelevant -- this pass only touches
    // Unit::frame). Dying units are already skipped by step_units's
    // stand/walk writes (see its `if (!u.alive || u.die_time >= 0.0) continue;`
    // guard), so there is no earlier write this pass could be clobbering for
    // that branch -- their frame comes ONLY from here.
    for (Unit& u : units_) {
        if (!u.alive) continue;
        const UnitAnimSeq& seq = anim_for_image(u.image);
        if (u.die_time >= 0.0) {
            // Play the die frames once, forward, clamped on the last frame.
            // The unit tombstones (alive=false) shortly after via
            // combat.cpp's kDieAnimSecs, and unit_draw_order excludes
            // tombstoned units -- so "holding the last frame" is only ever
            // visible for whatever fraction of a second remains before the
            // corpse disappears, never a stuck/looping corpse.
            const int adv = static_cast<int>(u.die_time * kWalkFps);
            u.frame = seq.die_first + std::min(seq.die_count - 1, adv);
        } else if (u.fire_flash > 0.0) {
            // Play the fire/recoil sequence ONCE across the flash window.
            // fire_flash counts DOWN from kFireFlashSecs (set by
            // combat.cpp's try_fire on every shot) to 0. The fire sequence
            // plays once per shot at kFireAnimFps (15 Hz), so all fire_count
            // frames are visited during the 0.4s flash window.
            // kFireFlashSecs is combat.cpp's (anonymous-namespace, not
            // exposed via combat.hpp) constant, duplicated here at the same
            // value -- same already-tolerated pattern as this file's
            // kTileW/kTileH/kLevelPx duplicating iso_renderer.cpp/input.cpp
            // (task-12-report.md Minor). MUST stay in sync with combat.cpp's
            // kFireFlashSecs if that ever changes.
            const int adv = static_cast<int>((kFireFlashSecs - u.fire_flash) * kFireAnimFps);
            const UnitAnimSeq& seq = anim_for_image(u.image);
            u.frame = seq.fire_first + std::min(seq.fire_count - 1, adv);
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
                Occupancy& occ, double dt, double move_cps) {
    for (std::size_t i = 0; i < units.size(); ++i) {
        Unit& u = units[i];
        const int self = static_cast<int>(i);
        const UnitAnimSeq& seq = anim_for_image(u.image);  // per-image walk/stand table (Task 7)

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
            u.frame = seq.stand;
            u.wait_time = 0.0;
            continue;
        }

        double budget = move_cps * dt;
        bool blocked = false;
        while (budget > 0.0 && u.path_idx < u.path.size()) {
            const Cellxy tgt = u.path[u.path_idx];
            if (tgt.x == u.cell_x && tgt.y == u.cell_y) {  // already here (e.g. path[0])
                ++u.path_idx;
                continue;
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
            u.frame = seq.stand;
            u.wait_time = 0.0;
            occ.set(u.cell_x, u.cell_y, self);
            continue;
        }

        if (blocked) {
            const double prev_wait = u.wait_time;
            u.wait_time += dt;
            u.frame = seq.stand;  // stand while waiting (reads as "holding")
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
        const int phase = static_cast<int>(u.anim_time * kWalkFps) % seq.walk_count;
        u.frame = seq.walk_first + phase;
    }
}

}  // namespace ra2engine
