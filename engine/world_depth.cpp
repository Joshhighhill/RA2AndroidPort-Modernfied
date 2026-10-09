// G16 world depth (agent G): garrisonable buildings (G-48) and bridges (G-49).
// World members kept in their own TU (the building_combat.cpp precedent);
// world.cpp only calls step_depth and the engineer-at-hut hook.
// Every behaviour is cited to docs/re/world-depth.md (gamemd.exe anchors).
#include <algorithm>
#include <cmath>

#include "bridges.hpp"
#include "combat.hpp"       // deliver_hit_from, deliver_unit_hit, deliver_building_hit, rearm_secs
#include "group_move.hpp"   // nearest_free_cells
#include "veterancy.hpp"    // is_elite
#include "world.hpp"

namespace ra2engine {

namespace {
constexpr double kFireFlashSecs = 0.4;
int cheb_to_foundation(const Building& b, int x, int y) {
    int best = -1;
    for (const Cellxy& c : foundation_cells(b.cx, b.cy, b.fw, b.fh)) {
        const int d = std::max(std::abs(x - c.x), std::abs(y - c.y));
        if (best < 0 || d < best) best = d;
    }
    return best;
}
bool blocks_ground(ra2assets::LandType t) {   // == world.cpp land_type_blocks_ground
    return t == ra2assets::LandType::Water || t == ra2assets::LandType::Rock ||
           t == ra2assets::LandType::Wall;
}
}  // namespace

int World::neutral_house() const {
    for (int h = 0; h < house_count(); ++h)
        if (!house_combatant(h)) return h;
    return -1;
}

// ---------------------------------------------------------------- garrisons

// BuildingClass::CanBeOccupiedBy (FUN_00457CE0): CanBeOccupied, not mid-build
// / mid-sell, an Occupier=yes infantry, the building owned by the
// infantry's house OR by a neutral (MultiplayPassive) house, and room left
// (OccupantCount != MaxNumberOccupants). The Assaulter (+0xEB5) branch is a
// YR-only infantry flag no stock RA2 unit uses -- not modelled.
bool World::can_occupy(int ui, int bi) const {
    if (ui < 0 || ui >= static_cast<int>(units_.size())) return false;
    if (bi < 0 || bi >= static_cast<int>(buildings_.size())) return false;
    const Unit& u = units_[ui];
    const Building& b = buildings_[bi];
    if (!u.alive || u.die_time >= 0.0 || !u.occupier || u.is_vehicle) return false;
    if (!b.alive || b.state != Building::State::Idle || b.selling) return false;
    if (b.type < 0 || b.type >= static_cast<int>(btypes().size())) return false;
    const BuildingTypeDef& t = btypes()[b.type];
    if (!t.can_be_occupied) return false;
    const bool owner_ok = b.house == u.house || !house_combatant(b.house) || passive_house(b.house);
    return owner_ok && static_cast<int>(b.occupants.size()) != t.max_occupants;
}

bool World::campaign_can_occupy(int bi) const {
    if (bi < 0 || bi >= static_cast<int>(buildings_.size())) return false;
    const Building& b = buildings_[bi];
    if (!b.alive || b.state != Building::State::Idle || b.selling) return false;
    if (b.type < 0 || b.type >= static_cast<int>(btypes().size())) return false;
    const BuildingTypeDef& t = btypes()[b.type];
    return t.can_be_occupied && static_cast<int>(b.occupants.size()) < t.max_occupants;
}

bool World::campaign_occupy_now(int ui, int bi) {
    if (!can_occupy(ui, bi)) return false;
    garrison_any_ = true;
    occupy_now(ui, bi);
    return true;
}

void World::order_occupy(int ui, int bi) {
    if (!can_occupy(ui, bi)) return;
    garrison_any_ = true;
    Unit& u = units_[ui];
    u.target = -1;
    u.target_building = -1;
    u.attack_order = false;
    u.enter_target = -1;
    u.capture_target = -1;
    u.infiltrate_target = -1;
    u.attack_cell_x = u.attack_cell_y = -1;
    u.occupy_target = bi;
    const Building& b = buildings_[bi];
    if (cheb_to_foundation(b, u.cell_x, u.cell_y) <= 1) return;   // enters next step
    int nx = b.cx, ny = b.cy, best = -1;
    for (const Cellxy& c : foundation_cells(b.cx, b.cy, b.fw, b.fh)) {
        const int d = std::max(std::abs(u.cell_x - c.x), std::abs(u.cell_y - c.y));
        if (best < 0 || d < best) { best = d; nx = c.x; ny = c.y; }
    }
    // The nearest free cells around that foundation cell; the first one the
    // unit can actually path to (a cell in a walled-off pocket is skipped).
    const std::vector<Cellxy> free = nearest_free_cells({nx, ny}, 8, passable_, grid_w_, grid_h_, occ_);
    for (const Cellxy& g : free) {
        order_unit_move(ui, g.x, g.y);
        if (units_[ui].moving()) break;
    }
}

int World::order_occupy_selected(int bi) {
    int n = 0;
    for (int i = 0; i < static_cast<int>(units_.size()); ++i) {
        const Unit& u = units_[i];
        if (!u.selected || u.house != 0 || u.cargo_of >= 0) continue;
        if (!can_occupy(i, bi)) continue;
        // Room check against the orders already given this click.
        const Building& b = buildings_[bi];
        if (static_cast<int>(b.occupants.size()) + n >= btypes()[b.type].max_occupants) break;
        order_occupy(i, bi);
        ++n;
    }
    return n;
}

// InfantryClass::Occupy (FUN_00522640): the infantry leaves the map
// (Limbo) and is appended to Occupants; the first occupant raises the
// "structure garrisoned" EVA for the owner (FUN_0050B510 player check).
void World::occupy_now(int ui, int bi) {
    Unit& u = units_[ui];
    Building& b = buildings_[bi];
    occ_.release_all_of(ui, -1, -1);
    u.path.clear();
    u.path_idx = 0;
    u.selected = false;
    u.target = -1;
    u.target_building = -1;
    u.occupy_target = -1;
    u.cargo_of = kGarrisonCargo;
    u.garrison_of = bi;
    b.occupants.push_back(ui);
    garrison_events_.push_back({GarrisonEvent::Entered, bi, u.house});
    note_entered(EnteredEvent::Occupy, ui, bi, u.house, false);   // W2-CA-01
    if (b.occupants.size() == 1) garrison_events_.push_back({GarrisonEvent::Garrisoned, bi, u.house});
    sync_garrison_owner(bi);
}

// FUN_004581F0 (called each tick from FUN_0043FAA0 for CanBeOccupied types):
// empty and not neutral -> back to the neutral house ("structure abandoned"
// EVA for the old owner); occupied and neutral -> the FIRST occupant's house.
void World::sync_garrison_owner(int bi) {
    Building& b = buildings_[bi];
    const int neutral = neutral_house();
    if (neutral < 0) return;
    if (b.occupants.empty() && b.house != neutral) {
        garrison_events_.push_back({GarrisonEvent::Abandoned, bi, b.house});
        b.house = neutral;
        b.garrison_target = -1;
    } else if (!b.occupants.empty() && b.house == neutral) {
        b.house = units_[b.occupants.front()].house;
    }
}

// BuildingClass::UnloadOccupants (FUN_00457DD0): every occupant is put back
// on the map beside the foundation; FiringOccupantIndex resets. The original
// walks out from the foundation edge to ONE exit cell and stacks the infantry
// in its sub-cells; the clone's one-unit-per-cell occupancy places each on the
// nearest free cell to the foundation's bottom corner instead
// [RE-FALLBACK: exit walk order / sub-cell stacking]. No exit at all -> the
// occupants die (KillOccupants, FUN_004585B0) [PROBABLE].
void World::unload_occupants(int bi) {
    if (bi < 0 || bi >= static_cast<int>(buildings_.size())) return;
    Building& b = buildings_[bi];
    b.firing_occupant = 0;
    if (b.occupants.empty()) return;
    const Cellxy corner{b.cx + b.fw - 1, b.cy + b.fh};
    const std::vector<Cellxy> free =
        nearest_free_cells(corner, static_cast<int>(b.occupants.size()), passable_, grid_w_, grid_h_, occ_);
    const int house = b.house;
    for (std::size_t k = 0; k < b.occupants.size(); ++k) {
        const int ui = b.occupants[k];
        Unit& u = units_[ui];
        u.garrison_of = -1;
        u.cargo_of = -1;
        if (k >= free.size()) {   // no exit cell
            ability_kill_unit(ui);
            continue;
        }
        u.cell_x = free[k].x;
        u.cell_y = free[k].y;
        u.fx = u.cell_x;
        u.fy = u.cell_y;
        u.guard_x = u.cell_x;
        u.guard_y = u.cell_y;
        occ_.set(u.cell_x, u.cell_y, ui);
    }
    b.occupants.clear();
    garrison_events_.push_back({GarrisonEvent::Unloaded, bi, house});
    sync_garrison_owner(bi);
}

bool World::garrison_firing(int bi) const {   // vtable +0x400 = FUN_00458DC0
    if (bi < 0 || bi >= static_cast<int>(buildings_.size())) return false;
    const Building& b = buildings_[bi];
    if (b.type < 0 || b.type >= static_cast<int>(btypes().size())) return false;
    const BuildingTypeDef& t = btypes()[b.type];
    return t.can_be_occupied && t.can_occupy_fire && !b.occupants.empty();
}

// TechnoClass::GetWeaponRange (FUN_006F6750) for a garrison: the weapon's own
// Range is replaced by (vtable +0x404 = FUN_00458DF0: min(foundation W, H) / 2)
// + OccupyWeaponRange.
double World::garrison_range(int bi) const {
    const Building& b = buildings_[bi];
    return std::min(b.fw, b.fh) / 2 + depth_rules_.occupy_weapon_range;
}

std::vector<World::GarrisonEvent> World::take_garrison_events() {
    std::vector<GarrisonEvent> out;
    out.swap(garrison_events_);
    return out;
}

void World::step_garrison_combat(double dt) {
    const int nb = static_cast<int>(buildings_.size());
    const int nu = static_cast<int>(units_.size());
    for (int bi = 0; bi < nb; ++bi) {
        Building& b = buildings_[bi];
        if (!b.alive || b.state != Building::State::Idle || !garrison_firing(bi)) continue;
        b.garrison_rof_cool = std::max(0.0, b.garrison_rof_cool - dt);
        b.def_fire_flash = std::max(0.0, b.def_fire_flash - dt);
        if (b.firing_occupant < 0 || b.firing_occupant >= static_cast<int>(b.occupants.size()))
            b.firing_occupant = 0;
        // BuildingClass::GetWeapon (FUN_004526E0): the FiringOccupantIndex
        // occupant's OccupyWeapon (EliteOccupyWeapon when that occupant is elite).
        const Unit& occ = units_[b.occupants[b.firing_occupant]];
        const bool elite = occ.vet.recv ? is_elite(occ.vet.veterancy) : occ.veteran_rank >= 2;
        const WeaponSlot& w = (elite && occ.occupy_w_elite.valid) ? occ.occupy_w_elite : occ.occupy_w;
        if (!w.valid) continue;
        const double range = garrison_range(bi);
        const double cx = b.cx + (b.fw - 1) / 2.0, cy = b.cy + (b.fh - 1) / 2.0;
        auto engageable = [&](int j) {
            if (j < 0 || j >= nu) return false;
            const Unit& v = units_[j];
            if (!v.alive || v.die_time >= 0.0 || v.cargo_of >= 0) return false;
            if (!is_enemy(b.house, v.house) || v.disguised) return false;
            if (!(v.seen_by_mask & (1u << b.house))) return false;
            if (!slot_can_target(w, v.is_aircraft && v.altitude > 0.0, v.armor_ix)) return false;
            return std::hypot(v.fx - cx, v.fy - cy) <= range + 0.5;
        };
        if (!engageable(b.garrison_target)) {
            b.garrison_target = -1;
            double best = 0.0;
            for (int j = 0; j < nu; ++j) {
                if (!engageable(j)) continue;
                const double d = std::hypot(units_[j].fx - cx, units_[j].fy - cy);
                if (b.garrison_target < 0 || d < best) { b.garrison_target = j; best = d; }
            }
        }
        if (b.garrison_target < 0 || b.garrison_rof_cool > 0.0) continue;
        // TechnoClass::GetROF (FUN_006FC460): ROF / OccupantCount, then
        // ftol(ROF / OccupyROFMultiplier) when the multiplier is > 1.0.
        int rof = w.rof / static_cast<int>(b.occupants.size());
        if (depth_rules_.occupy_rof_mult > 1.0) rof = ftol(rof / depth_rules_.occupy_rof_mult);
        b.garrison_rof_cool = rearm_secs(rof, &combat_rng_);
        b.def_fire_flash = kFireFlashSecs;
        // TechnoClass::Fire (FUN_006FD210): damage x OccupyDamageMultiplier.
        Unit shooter;
        shooter.house = b.house;
        shooter.fx = cx;
        shooter.fy = cy;
        shooter.cell_x = static_cast<int>(std::lround(cx));
        shooter.cell_y = static_cast<int>(std::lround(cy));
        shooter.w_damage = ftol(w.damage * depth_rules_.occupy_damage_mult);
        shooter.w_rof = rof;
        shooter.w_range = range;
        shooter.w_verses = w.verses;
        shooter.w_cell_spread = w.cell_spread;
        shooter.w_percent_at_max = w.percent_at_max;
        shooter.w_inf_death = w.inf_death;          // W3-B
        shooter.w_prone_damage = w.prone_damage;
        shooter.alive = false;
        fire_events_.push_back({-1, bi, b.garrison_target, -1, 0, w.id});
        ++garrison_shots_;
        // UC weapons fly invisible projectiles: delivered as instant hits
        // [RE-FALLBACK: garrison shots are not routed through step_projectiles].
        if (w.cell_spread >= 1.0)
            apply_spread_damage(units_, occ_, shooter, units_[b.garrison_target]);
        else
            deliver_hit_from(units_, occ_, shooter, -1, bi, b.garrison_target);
        {   // Wave-5 (item 8): the firing occupant earns the kill (garrisoned
            // infantry gain rank in the original) [RE-FALLBACK: the credit
            // routing through BuildingClass::Fire was not traced].
            Unit& v = units_[b.garrison_target];
            const int fo = b.occupants[static_cast<std::size_t>(b.firing_occupant) % b.occupants.size()];
            if (v.die_time >= 0.0 && v.vet.killed_by < 0) v.vet.killed_by = fo;
        }
        // FiringOccupantIndex = (index + 1) % OccupantCount after every shot
        // (TechnoClass::Fire @0x6FE525).
        b.firing_occupant = (b.firing_occupant + 1) % static_cast<int>(b.occupants.size());
    }
}

// ------------------------------------------------------------------ bridges

void World::set_bridges(std::vector<int> overlay, std::vector<ra2assets::LandType> tile_land) {
    bridge_ovl_.assign(static_cast<std::size_t>(grid_w_) * grid_h_, -1);
    for (std::size_t i = 0; i < overlay.size() && i < bridge_ovl_.size(); ++i)
        if (is_low_bridge_overlay(overlay[i])) bridge_ovl_[i] = static_cast<std::int16_t>(overlay[i]);
    bridge_tile_land_.assign(bridge_ovl_.size(), static_cast<std::uint8_t>(ra2assets::LandType::Clear));
    for (std::size_t i = 0; i < tile_land.size() && i < bridge_tile_land_.size(); ++i)
        bridge_tile_land_[i] = static_cast<std::uint8_t>(tile_land[i]);
    ++bridge_version_;
}

int World::bridge_overlay_at(int x, int y) const {
    if (bridge_ovl_.empty() || x < 0 || y < 0 || x >= grid_w_ || y >= grid_h_) return -1;
    return bridge_ovl_[static_cast<std::size_t>(y) * grid_w_ + x];
}

// Destroyed low-bridge cells right now (maps may ship a bridge already blown,
// e.g. May Day's river crossings, to be repaired at the hut).
int World::bridge_cells_destroyed() const {
    int n = 0;
    for (int v : bridge_ovl_)
        if (v >= 0 && low_bridge_destroyed(v)) ++n;
    for (std::uint8_t v : hbridge_)
        if ((v & 3) && (v & 4)) ++n;   // W4-SQ-10: broken high-bridge cells
    return n;
}

// CellClass::Recalc_Attributes analogue for one bridge cell: an intact /
// damaged low section is Land=Road with NoUseTileLandType=true (ground
// passable, no ships); a destroyed one (LOBRDG27/28, NoUseTileLandType=false)
// falls back to the tile's land type (water under the river span).
void World::set_bridge_cell(int x, int y, int idx) {
    const std::size_t i = static_cast<std::size_t>(y) * grid_w_ + x;
    bridge_ovl_[i] = static_cast<std::int16_t>(idx);
    const ra2assets::LandType lt = low_bridge_destroyed(idx)
                                       ? static_cast<ra2assets::LandType>(bridge_tile_land_[i])
                                       : ra2assets::LandType::Road;
    if (i < passable_.size()) passable_[i] = blocks_ground(lt) ? 0 : 1;
    if (i < water_passable_.size()) water_passable_[i] = (lt == ra2assets::LandType::Water) ? 1 : 0;
    ++bridge_version_;
    if (!low_bridge_destroyed(idx)) return;
    // A ground unit left standing on a collapsed section dies with it
    // [PROBABLE: FUN_00654C00 runs on the three cells only in the destroy branch].
    const int ou = occ_.at(x, y);
    if (ou >= 0 && ou < static_cast<int>(units_.size())) {
        const Unit& u = units_[ou];
        if (u.alive && u.die_time < 0.0 && !u.is_naval && !u.is_aircraft && passable_[i] == 0)
            ability_kill_unit(ou);
    }
}

// MapClass::DamageArea's bridge part (FUN_00489190): an explosion in a low
// bridge cell with a Wall=yes warhead breaks the section when
// Random(1, BridgeStrength) < damage (the IonCannonWarhead always does); the
// section rewrite is FUN_0057B720 / FUN_0057BCE0 over the 3-cell
// cross-section FUN_0057B4D0 centres. BridgeDestruction=no disables it.
int World::bridge_explosion(int cx, int cy, int damage, bool wall_warhead) {
    if ((bridge_ovl_.empty() && hbridge_.empty()) || !wall_warhead || !depth_rules_.armed ||
        !depth_rules_.bridge_destruction)
        return 0;
    const int idx = bridge_overlay_at(cx, cy);
    if (!is_low_bridge_overlay(idx) || low_bridge_destroyed(idx)) {
        // W4-SQ-10 [RE-FALLBACK: the same Wall= warhead + Random(1, BridgeStrength)
        // < damage roll as a low section]: a deck hit breaks the high span.
        if (!high_bridge_cell(cx, cy) || high_bridge_destroyed(cx, cy)) return 0;
        if (!(bridge_rng_.random(1, std::max(1, depth_rules_.bridge_strength)) < damage)) return 0;
        return break_high_bridge(cx, cy);
    }
    if (!(bridge_rng_.random(1, std::max(1, depth_rules_.bridge_strength)) < damage)) return 0;
    auto at = [&](int x, int y) { return bridge_overlay_at(x, y); };
    int sx = cx, sy = cy;
    low_bridge_section_centre(cx, cy, at, sx, sy);
    const LowAxis ax = low_bridge_axis(bridge_overlay_at(sx, sy));
    if (ax == LowAxis::None) return 0;
    int xs[3], ys[3];
    low_bridge_section_cells(sx, sy, ax, xs, ys);
    int changed = 0;
    for (int k = 0; k < 3; ++k) {
        const int o = bridge_overlay_at(xs[k], ys[k]);
        if (!is_low_bridge_overlay(o)) continue;
        const int nx = low_bridge_next_stage(o);
        if (nx == o) continue;
        set_bridge_cell(xs[k], ys[k], nx);
        ++changed;
    }
    return changed;
}

// An engineer entering a BridgeRepairHut (InfantryClass Mission_Enter,
// FUN_00519449: engineer +0xEC3 && type +0x16B6): RepairBridgeSound, then the
// bridge found within +-2 cells is restored (FUN_00573070 / FUN_0056FB80).
// The clone restores every damaged/destroyed low section of the bridge run
// connected to a low-bridge cell within 2 cells of the hut's foundation to an
// intact piece (low_bridge_repaired) [RE-FALLBACK: the per-section restore
// walk of FUN_0057EE70 is not traced]. Returns the number of cells restored.
int World::repair_bridge_near(int bi) {
    if (bridge_ovl_.empty()) return repair_high_bridge_near(bi);   // W4-SQ-10
    if (bi < 0 || bi >= static_cast<int>(buildings_.size())) return 0;
    const Building& b = buildings_[bi];
    std::vector<Cellxy> stack;
    std::vector<std::uint8_t> seen(bridge_ovl_.size(), 0);
    for (int y = b.cy - 2; y < b.cy + b.fh + 2; ++y)
        for (int x = b.cx - 2; x < b.cx + b.fw + 2; ++x)
            if (is_low_bridge_overlay(bridge_overlay_at(x, y))) stack.push_back({x, y});
    int restored = 0;
    while (!stack.empty()) {
        const Cellxy c = stack.back();
        stack.pop_back();
        const std::size_t i = static_cast<std::size_t>(c.y) * grid_w_ + c.x;
        if (seen[i]) continue;
        seen[i] = 1;
        const int fixed = low_bridge_repaired(bridge_ovl_[i]);
        if (fixed != bridge_ovl_[i]) {
            set_bridge_cell(c.x, c.y, fixed);
            ++restored;
        }
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                const int x = c.x + dx, y = c.y + dy;
                if ((dx || dy) && is_low_bridge_overlay(bridge_overlay_at(x, y))) stack.push_back({x, y});
            }
    }
    if (restored == 0) restored = repair_high_bridge_near(bi);   // W4-SQ-10
    return restored;
}

// ------------------------------------------------------------- high bridges
// W4-SQ-10. A high bridge is its deck overlays (BRIDGE1/2, BRIDGEB1/B2) over
// the river plus the Ovrps/Ovrpsb ramp tiles at its ends (temperat.ini
// BridgeSet 19 / WoodBridgeSet 80). The destroy functions (FUN_005766D0
// concrete, FUN_00570FC0 wood) walk the span both ways, swap the ramp tiles
// for broken ends, take the deck away and kill what stands on it
// (FUN_0047DC80 on each section). [RE-FALLBACK: the clone takes the whole
// connected deck at once; the ramp tiles keep their art; the deck overlays
// stop drawing.]
void World::set_high_bridges(std::vector<std::uint8_t> cells) {
    hbridge_ = std::move(cells);
    hbridge_.resize(static_cast<std::size_t>(grid_w_) * grid_h_, 0);
    for (std::size_t i = 0; i < hbridge_.size() && i < passable_.size(); ++i)
        if ((hbridge_[i] & 3) == 1) passable_[i] = 1;   // the deck carries ground units (water grid kept)
    ++bridge_version_;
}

bool World::high_bridge_cell(int x, int y) const {
    if (hbridge_.empty() || x < 0 || y < 0 || x >= grid_w_ || y >= grid_h_) return false;
    return (hbridge_[static_cast<std::size_t>(y) * grid_w_ + x] & 3) != 0;
}

bool World::high_bridge_destroyed(int x, int y) const {
    if (!high_bridge_cell(x, y)) return false;
    return (hbridge_[static_cast<std::size_t>(y) * grid_w_ + x] & 4) != 0;
}

int World::break_high_bridge(int x, int y) {
    if (hbridge_.empty()) return 0;
    int sx = -1, sy = -1;
    for (int r = 0; r <= 1 && sx < 0; ++r)
        for (int dy = -r; dy <= r && sx < 0; ++dy)
            for (int dx = -r; dx <= r && sx < 0; ++dx)
                if (high_bridge_cell(x + dx, y + dy) && !high_bridge_destroyed(x + dx, y + dy)) {
                    sx = x + dx;
                    sy = y + dy;
                }
    if (sx < 0) return 0;
    std::vector<Cellxy> stack{{sx, sy}};
    int n = 0;
    while (!stack.empty()) {
        const Cellxy c = stack.back();
        stack.pop_back();
        const std::size_t i = static_cast<std::size_t>(c.y) * grid_w_ + c.x;
        if (hbridge_[i] & 4) continue;
        hbridge_[i] |= 4;
        ++n;
        if ((hbridge_[i] & 3) == 1) passable_[i] = 0;
        // The objects on the deck die with it (FUN_0047DC80). R-210: with the
        // heights armed that is every unit standing ON the deck (a deck over
        // land too; an amphibious unit swimming under it lives); otherwise the
        // historical rule (a ground unit on a deck over water).
        if (terrain_armed() || (hbridge_[i] & 3) == 1) {
            const int ou = occ_.at(c.x, c.y);
            if (ou >= 0 && ou < static_cast<int>(units_.size())) {
                const Unit& u = units_[ou];
                const bool on = terrain_armed() ? u.on_deck != 0 : (!u.is_naval && !u.is_aircraft);
                if (u.alive && u.die_time < 0.0 && on && !u.is_naval && !u.is_aircraft) ability_kill_unit(ou);
            }
        }
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                if ((dx || dy) && high_bridge_cell(c.x + dx, c.y + dy) && !high_bridge_destroyed(c.x + dx, c.y + dy))
                    stack.push_back({c.x + dx, c.y + dy});
    }
    ++bridge_version_;
    return n;
}

int World::repair_high_bridge_near(int bi) {
    if (hbridge_.empty() || bi < 0 || bi >= static_cast<int>(buildings_.size())) return 0;
    const Building& b = buildings_[bi];
    std::vector<Cellxy> stack;
    // [RE: FUN_00519449 scans +-2 around the hut for the bridge kind; a high
    // deck starts past the ramp tiles, so the clone looks 4 cells out]
    for (int y = b.cy - 4; y < b.cy + b.fh + 4; ++y)
        for (int x = b.cx - 4; x < b.cx + b.fw + 4; ++x)
            if (high_bridge_destroyed(x, y)) stack.push_back({x, y});
    int n = 0;
    while (!stack.empty()) {
        const Cellxy c = stack.back();
        stack.pop_back();
        const std::size_t i = static_cast<std::size_t>(c.y) * grid_w_ + c.x;
        if (!(hbridge_[i] & 4)) continue;
        hbridge_[i] = static_cast<std::uint8_t>(hbridge_[i] & 3);
        ++n;
        if ((hbridge_[i] & 3) == 1) passable_[i] = 1;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                if ((dx || dy) && high_bridge_destroyed(c.x + dx, c.y + dy)) stack.push_back({c.x + dx, c.y + dy});
    }
    if (n) ++bridge_version_;
    return n;
}

int World::force_bridge_break(int x, int y) {
    const int idx = bridge_overlay_at(x, y);
    if (is_low_bridge_overlay(idx) && !low_bridge_destroyed(idx)) {
        auto at = [&](int ax, int ay) { return bridge_overlay_at(ax, ay); };
        int sx = x, sy = y;
        low_bridge_section_centre(x, y, at, sx, sy);
        const LowAxis ax = low_bridge_axis(bridge_overlay_at(sx, sy));
        if (ax == LowAxis::None) return 0;
        int xs[3], ys[3];
        low_bridge_section_cells(sx, sy, ax, xs, ys);
        int changed = 0;
        for (int k = 0; k < 3; ++k) {
            const int o = bridge_overlay_at(xs[k], ys[k]);
            if (!is_low_bridge_overlay(o)) continue;
            const int nx = low_bridge_next_stage(o);
            if (nx == o) continue;
            set_bridge_cell(xs[k], ys[k], nx);
            ++changed;
        }
        return changed;
    }
    return break_high_bridge(x, y);
}

void World::order_attack_cell(int ui, int cx, int cy) {
    if (ui < 0 || ui >= static_cast<int>(units_.size())) return;
    Unit& u = units_[ui];
    if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0 || u.w_damage <= 0) return;
    u.target = -1;
    u.target_building = -1;
    u.occupy_target = -1;
    u.attack_order = true;
    u.attack_cell_x = cx;
    u.attack_cell_y = cy;
}

int World::order_attack_cell_selected(int cx, int cy) {
    // R3: every selected unit the player commands -- house 0 and the
    // PlayerControl houses (W2-CA-02), as order_attack / order_move do since
    // W5-A. The original issues the order for each CurrentObjects entry the
    // player owns; there is no Owner == PlayerPtr test (see order_stop_selected).
    int n = 0;
    for (int i = 0; i < static_cast<int>(units_.size()); ++i)
        if (units_[i].selected && controllable(units_[i].house) && units_[i].cargo_of < 0) {
            order_attack_cell(i, cx, cy);
            ++n;
        }
    return n;
}

// ------------------------------------------------------------------ the pass

void World::step_depth(double dt) {
    // Ground shots fired by step_combat this step: the explosion lands on the
    // cell (unit / building there + the bridge roll) [RE-FALLBACK: a ground
    // target is hit at the ROF tick, no projectile flight].
    for (int i = 0; i < static_cast<int>(units_.size()); ++i) {
        Unit& u = units_[i];
        if (!u.cell_shot) continue;
        u.cell_shot = false;
        const int x = u.attack_cell_x, y = u.attack_cell_y;
        const bool wall = u.w_active == 1 ? u.w2.wall : u.p_wall;
        bridge_explosion(x, y, u.w_damage, wall);
        const int victim = occ_.at(x, y);
        if (victim >= 0 && victim != i && victim < static_cast<int>(units_.size()))
            deliver_unit_hit(units_, occ_, i, victim);
        const int bb = building_at_cell(x, y);
        // W6-04-04/05: a wall (built or a map overlay) takes Reduce_Wall per
        // the warhead's wall keys, not an HP hit.
        if (bb >= 0 && !w6_wall_building_hit(*this, units_, buildings_, bb, u.w6.wall_flags, u.w_damage))
            deliver_building_hit(units_, buildings_, i, bb);
        w6_overlay_wall_hit(x, y, u.w6.wall_flags, u.w_damage);
    }
    if (!garrison_any_) return;
    // Occupy arrivals (the step_capture analogue).
    for (int i = 0; i < static_cast<int>(units_.size()); ++i) {
        Unit& u = units_[i];
        if (u.occupy_target < 0) continue;
        const int bi = u.occupy_target;
        if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0 || !can_occupy(i, bi)) {
            u.occupy_target = -1;
            continue;
        }
        if (cheb_to_foundation(buildings_[bi], u.cell_x, u.cell_y) <= 1) {
            occupy_now(i, bi);
        } else if (!u.moving()) {
            order_occupy(i, bi);   // re-path adjacent
        }
    }
    for (int bi = 0; bi < static_cast<int>(buildings_.size()); ++bi) {
        Building& b = buildings_[bi];
        if (b.type < 0 || b.type >= static_cast<int>(btypes().size()) ||
            !btypes()[b.type].can_be_occupied)
            continue;
        // Dead occupants leave the list (they cannot be hurt inside, but a
        // trigger/ability may still erase them).
        b.occupants.erase(std::remove_if(b.occupants.begin(), b.occupants.end(),
                                         [&](int ui) { return !units_[ui].alive; }),
                          b.occupants.end());
        // The building died or is being sold: its occupants walk out
        // (FUN_004421B0 / FUN_00449B70 both call UnloadOccupants).
        if (!b.occupants.empty() && (!b.alive || b.state == Building::State::Dead || b.selling)) {
            unload_occupants(bi);
            continue;
        }
        if (b.alive && b.state != Building::State::Dead) sync_garrison_owner(bi);
    }
    step_garrison_combat(dt);
}

// ------------------------------------------------------- R-210 terrain
// [RE: FootClass height gate FUN_004D9B70; CellClass::IsClearToMove
// FUN_004833B0 (a bridge cell's deck at Level + 4 skips the land-type test,
// the ground under it keeps it); InfantryClass FUN_0051BD70 / UnitClass
// FUN_0073DFA0 IsCellOccupied: a Gate=yes building (+0x16B7) opens
// (Move::ClosedGate) when its owner IsAlliedWith (FUN_004F9870) the mover].
const std::vector<std::uint8_t>* World::deck_grid(MovementDomain d) const {
    if (hbridge_.empty() || d == MovementDomain::Water) return nullptr;
    const std::size_t n = static_cast<std::size_t>(grid_w_) * grid_h_;
    if (deck_ver_ != bridge_version_ || deck_land_.size() != n || deck_amph_.size() != n) {
        deck_land_.assign(n, 0);
        deck_amph_.assign(n, 0);
        for (std::size_t i = 0; i < n && i < hbridge_.size(); ++i) {
            const std::uint8_t v = hbridge_[i];
            if ((v & 3) == 0 || (v & 4)) continue;   // no deck / broken span
            // Over water / cliff (1) the ground is not a land mover's: deck
            // only; over land (2) both the deck and the ground under it.
            deck_land_[i] = (v & 3) == 1 ? 2 : 1;
            deck_amph_[i] = 1;   // an amphibious mover swims under or walks over
        }
        deck_ver_ = bridge_version_;
    }
    return d == MovementDomain::Amphibious ? &deck_amph_ : &deck_land_;
}

void World::open_gates_for(int h, std::vector<int>& out) const {
    out.clear();
    const std::vector<BuildingTypeDef>& bt = btypes();
    for (const Building& b : buildings_) {
        if (!b.alive || b.type < 0 || b.type >= static_cast<int>(bt.size())) continue;
        if (b.state == Building::State::Dead || !bt[static_cast<std::size_t>(b.type)].w6.gate) continue;
        if (!are_allied(b.house, h)) continue;   // the gate owner's view (FUN_004F9870)
        for (const Cellxy& c : foundation_cells(b.cx, b.cy, b.fw, b.fh))
            if (c.x >= 0 && c.y >= 0 && c.x < grid_w_ && c.y < grid_h_)
                out.push_back(c.y * grid_w_ + c.x);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
}

const PathTerrain* World::terrain_for(const Unit& u, PathTerrain& out, std::vector<int>& gates) const {
    out = PathTerrain{};
    gates.clear();
    if (u.movement_domain != MovementDomain::Water) open_gates_for(u.house, gates);
    if (!gates.empty()) out.open_cells = &gates;
    if (!terrain_armed()) return gates.empty() ? nullptr : &out;
    out.level = &level_grid_;
    out.slope = ramp_.size() == level_grid_.size() ? &ramp_ : nullptr;
    out.deck = deck_grid(u.movement_domain);
    out.version = bridge_version_;
    out.start_layer = out.deck ? u.on_deck : 0;
    return &out;
}

void World::sync_deck_layer(Unit& u) const {
    if (!terrain_armed()) return;
    const std::vector<std::uint8_t>* dk = deck_grid(u.movement_domain);
    const int x = u.cell_x, y = u.cell_y;
    if (!dk || x < 0 || y < 0 || x >= grid_w_ || y >= grid_h_) { u.on_deck = 0; return; }
    const std::uint8_t v = (*dk)[static_cast<std::size_t>(y) * grid_w_ + x];
    if (v == 0) u.on_deck = 0;
    else if (v == 2) u.on_deck = 1;
}

}  // namespace ra2engine
