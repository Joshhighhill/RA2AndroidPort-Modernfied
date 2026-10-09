// world_sw.cpp -- wave-2 W2-AN superweapons v2: one record per SW type per
// house, the seven classic RA2 effects, and the presentation event feed.
// Split out of world.cpp (the CB-4 single-Nuke registry lived there).
//
// RE notes (this fix, gamemd.exe; Ghidra pass of the auditor's w2air_sw1.py
// dump of FUN_006cba10 + a byte scan of TechnoTypeClass::LoadFromINI stores):
//  * case 0 MultiMissile: the silo branch records the target in the house and
//    plays the silo launch; the second branch creates the DOWN missile 20000
//    leptons above the target cell with velocity (0,0,-100) (cos/sin of pi/2
//    times DAT_007eca88 = -100.0) -> it falls 100 leptons/frame = 200 frames.
//    The up missile is [NukeCarrier] / [GiantNukeUp] Vertical=yes,
//    Acceleration=1 from launch speed 1 (projectile.md §6.2/§12.3) and detonates
//    above DetonationAltitude=20000: sum(1..200) = 20100 -> 200 frames.
//  * case 1 IronCurtain: the 3x3 offset table DAT_00b0afa8..cc around the
//    target; every object in those cells not already in limbo gets the
//    IronCurtain call with [General] IronCurtainDuration.
//  * case 4 ChronoWarp: the same 3x3 table around the source; an object is
//    teleported unless TechnoType+0xD97 (Organic) is set and +0xCD4
//    (Teleporter) is not -- that object takes ReceiveDamage(full Strength,
//    Rules+0xFA8 warhead), i.e. it dies.
#include <algorithm>
#include <cmath>

#include "combat.hpp"
#include "world.hpp"

namespace ra2engine {

namespace {
constexpr double kLept = 256.0;
// DAT_00b0afa8 [RE: this audit]: the nine (dx,dy) offsets of a 3x3 area.
constexpr int kArea3x3[9][2] = {{0, 0}, {-1, -1}, {0, -1}, {1, -1}, {1, 0},
                                {1, 1}, {0, 1},   {-1, 1}, {-1, 0}};
int frames_of(double dt) { return std::max(1, static_cast<int>(std::lround(dt * 15.0))); }
}  // namespace

// ---------------------------------------------------------------- registry --

World::HouseSuperweapon* World::sw_rec(int house, SuperweaponType type) {
    if (house < 0 || house >= static_cast<int>(sw_houses_.size())) return nullptr;
    for (HouseSuperweapon& r : sw_houses_[house])
        if (r.has_sw && r.sw.type == type) return &r;
    return nullptr;
}
const World::HouseSuperweapon* World::sw_rec(int house, SuperweaponType type) const {
    if (house < 0 || house >= static_cast<int>(sw_houses_.size())) return nullptr;
    for (const HouseSuperweapon& r : sw_houses_[house])
        if (r.has_sw && r.sw.type == type) return &r;
    return nullptr;
}

void World::an_event(int kind, int house, SuperweaponType type, int x, int y, int x2, int y2) {
    SwEvent e;
    e.seq = ++an_.event_seq;
    e.kind = kind;
    e.house = house;
    e.type = static_cast<int>(type);
    e.x = x;
    e.y = y;
    e.x2 = x2;
    e.y2 = y2;
    an_.events.push_back(e);
    if (an_.events.size() > 64) an_.events.erase(an_.events.begin(), an_.events.begin() + 32);
}

std::uint32_t World::an_rand() {
    // Numerical Recipes LCG; World-owned, saved, no wall clock.
    an_.rng = an_.rng * 1664525u + 1013904223u;
    return an_.rng >> 8;
}

// The CB-4 single-SW call: upsert the record of `type` (fresh charge).
void World::grant_superweapon(int house, SuperweaponType type, double recharge_secs, int dmg,
                              double cell_spread, double pmax, const std::array<int, 11>& verses) {
    if (house < 0) return;
    if ((int)sw_houses_.size() <= house) sw_houses_.resize(house + 1);
    HouseSuperweapon* rec = sw_rec(house, type);
    if (!rec) {
        sw_houses_[house].push_back(HouseSuperweapon{});
        rec = &sw_houses_[house].back();
    }
    rec->has_sw = true;
    rec->sw = Superweapon{};
    rec->sw.type = type;
    rec->sw.recharge_secs = recharge_secs;
    rec->eff_damage = dmg;
    rec->eff_cell_spread = cell_spread;
    rec->eff_percent_at_max = pmax;
    rec->eff_verses = verses;
    if (type == SuperweaponType::Nuke) {
        an_.nuke_set = true;
        an_.nuke_damage = dmg;
        an_.nuke_spread = cell_spread;
        an_.nuke_pmax = pmax;
        an_.nuke_verses = verses;
    }
    sw_any_ = true;
}

void World::grant_superweapon_type(int house, SuperweaponType type, double recharge_secs,
                                   bool is_powered, int dmg, double cell_spread, double pmax,
                                   const std::array<int, 11>& verses) {
    if (house < 0 || type == SuperweaponType::None) return;
    if (sw_rec(house, type)) {   // already held: keep its charge
        sw_rec(house, type)->is_powered = is_powered;
        return;
    }
    grant_superweapon(house, type, recharge_secs, dmg, cell_spread, pmax, verses);
    sw_rec(house, type)->is_powered = is_powered;
    an_event(SwEvent::Granted, house, type, -1, -1);
}

void World::revoke_superweapon_type(int house, SuperweaponType type) {
    if (house < 0 || house >= static_cast<int>(sw_houses_.size())) return;
    auto& v = sw_houses_[house];
    v.erase(std::remove_if(v.begin(), v.end(),
                           [&](const HouseSuperweapon& r) { return r.sw.type == type; }),
            v.end());
}

const Superweapon* World::sw_state(int house, SuperweaponType type) const {
    const HouseSuperweapon* r = sw_rec(house, type);
    return r ? &r->sw : nullptr;
}
bool World::sw_ready(int house, SuperweaponType type) const {
    const HouseSuperweapon* r = sw_rec(house, type);
    return r && r->sw.is_ready();
}
double World::sw_charge_fraction(int house, SuperweaponType type) const {
    const HouseSuperweapon* r = sw_rec(house, type);
    return r ? r->sw.charge_fraction() : 0.0;
}
std::vector<SuperweaponType> World::sw_types(int house) const {
    std::vector<SuperweaponType> out;
    if (house < 0 || house >= static_cast<int>(sw_houses_.size())) return out;
    for (const HouseSuperweapon& r : sw_houses_[house])
        if (r.has_sw) out.push_back(r.sw.type);
    return out;
}

// --- single-SW (first record) API, the CB-4 surface gates and tests use ---
bool World::sw_ready(int house) const {
    return house >= 0 && house < (int)sw_houses_.size() && !sw_houses_[house].empty() &&
           sw_houses_[house].front().has_sw && sw_houses_[house].front().sw.is_ready();
}
double World::sw_charge_fraction(int house) const {
    if (house < 0 || house >= (int)sw_houses_.size() || sw_houses_[house].empty()) return 0.0;
    return sw_houses_[house].front().sw.charge_fraction();
}
const Superweapon* World::sw_state(int house) const {
    if (house < 0 || house >= (int)sw_houses_.size() || sw_houses_[house].empty() ||
        !sw_houses_[house].front().has_sw)
        return nullptr;
    return &sw_houses_[house].front().sw;
}
bool World::fire_superweapon(int house, int cx, int cy) {
    const Superweapon* s = sw_state(house);
    if (!s) return false;
    return fire_superweapon_at(house, s->type, cx, cy);
}

void World::transfer_superweapon(int old_house, int new_house) {
    if (old_house < 0 || old_house >= (int)sw_houses_.size()) return;
    if (new_house < 0) return;
    if (sw_houses_[old_house].empty()) return;
    if ((int)sw_houses_.size() <= new_house) sw_houses_.resize(new_house + 1);
    for (const HouseSuperweapon& r : sw_houses_[old_house])
        if (!sw_rec(new_house, r.sw.type)) sw_houses_[new_house].push_back(r);
    sw_houses_[old_house].clear();
    sw_any_ = true;
}

void World::reset_superweapon(int house) {
    if (house < 0 || house >= (int)sw_houses_.size()) return;
    for (HouseSuperweapon& r : sw_houses_[house]) r.sw.on_fired();
}

void World::step_superweapon(double dt) {
    if (!sw_any_) return;   // firewall (spec §8)
    for (int h = 0; h < static_cast<int>(sw_houses_.size()); ++h) {
        bool low = false, low_known = false;
        for (HouseSuperweapon& rec : sw_houses_[h]) {
            if (!rec.has_sw) continue;
            // [RE: superweapon.md §1] IsPowered=true suspends the charge on low power.
            if (rec.is_powered) {
                if (!low_known) { low = house_low_power(h); low_known = true; }
                if (low) continue;
            }
            const bool was = rec.sw.ready;
            rec.sw.tick(dt);
            if (!was && rec.sw.ready) an_event(SwEvent::Ready, h, rec.sw.type, -1, -1);
        }
    }
    an_step_strikes(dt);
}

// ------------------------------------------------------------------ firing --

bool World::fire_superweapon_at(int house, SuperweaponType type, int cx, int cy, int cx2, int cy2) {
    HouseSuperweapon* rec = sw_rec(house, type);
    if (!rec || !rec->sw.is_ready()) return false;   // refused unless READY (§4)
    if (cx < 0 || cy < 0 || cx >= grid_w_ || cy >= grid_h_) return false;
    switch (type) {
        case SuperweaponType::Nuke: {
            // The silo: the house's live building whose type grants the Nuke.
            int ox = cx, oy = cy;
            for (const Building& b : buildings_) {
                if (!b.alive || b.house != house || b.state == Building::State::Dead) continue;
                if (b.type < 0 || b.type >= static_cast<int>(an_.sw_btype.size())) continue;
                if (an_.sw_btype[b.type] != static_cast<int>(SuperweaponType::Nuke)) continue;
                ox = b.cx + b.fw / 2;
                oy = b.cy + b.fh / 2;
                break;
            }
            SwStrike s;
            s.kind = 1;
            s.house = house;
            s.x = cx;
            s.y = cy;
            s.ox = ox;
            s.oy = oy;
            an_.strikes.push_back(s);
            an_event(SwEvent::Launched, house, type, cx, cy, ox, oy);
            break;
        }
        case SuperweaponType::IronCurtain:
            an_iron_curtain(house, cx, cy);
            an_event(SwEvent::Activated, house, type, cx, cy);
            break;
        case SuperweaponType::ChronoSphere:
        case SuperweaponType::ChronoWarp:
            if (cx2 < 0 || cy2 < 0 || cx2 >= grid_w_ || cy2 >= grid_h_) return false;
            an_chrono(house, cx, cy, cx2, cy2);
            an_event(SwEvent::Activated, house, type, cx, cy, cx2, cy2);
            break;
        case SuperweaponType::LightningStorm: {
            SwStrike s;
            s.kind = 2;
            s.house = house;
            s.x = cx;
            s.y = cy;
            s.rng = 0x9E3779B9u ^ (static_cast<std::uint32_t>(cx) * 73856093u) ^
                    (static_cast<std::uint32_t>(cy) * 19349663u) ^ an_rand();
            an_.strikes.push_back(s);
            an_event(SwEvent::Activated, house, type, cx, cy);
            break;
        }
        case SuperweaponType::ParaDrop:
        case SuperweaponType::AmerParaDrop:
            an_paradrop(house, type, cx, cy);
            an_event(SwEvent::Activated, house, type, cx, cy);
            break;
        default:
            return false;   // YR-only types: never granted in a classic game
    }
    rec->sw.on_fired();   // full recharge (§1)
    an_.any = true;
    return true;
}

// ------------------------------------------------------------------ damage --

void World::an_damage_unit(int idx, int dmg, int attacker) {
    if (idx < 0 || idx >= static_cast<int>(units_.size())) return;
    Unit& v = units_[idx];
    if (!v.alive || v.die_time >= 0.0 || dmg <= 0) return;
    if (v.an.iron_left > 0.0) return;   // W2-AN-04 Iron Curtain
    v.hp -= dmg;
    if (v.hp <= 0) {
        v.die_time = 0.0;
        v.vet.killed_by = attacker;
        occ_.release_all_of(idx, -1, -1);
        v.target = -1;
        v.target_building = -1;
        v.path.clear();
        v.path_idx = 0;
    }
}

void World::an_damage_building(int b, int dmg) {
    if (b < 0 || b >= static_cast<int>(buildings_.size())) return;
    Building& B = buildings_[b];
    if (!B.alive || B.state == Building::State::Dead || dmg <= 0) return;
    if (B.iron_left > 0.0) return;
    B.hp -= dmg;
    if (B.hp <= 0) kill_building(B);
}

// A warhead blast at cell-space (cx, cy): every unit (airborne ones only when
// hit_air) and every building within round(spread) cells, linear falloff
// 1.0 -> pmax (combat.cpp's adjudicated CellSpread curve), Verses per armor.
// [RE: superweapon.md §2/§5 -- FUN_00489190 hits EVERY object in radius, the
// firing house included; spare_house keeps the CB-4 enemies-only rule for
// callers that want it.]
void World::an_area_damage(int house, double cx, double cy, int damage, double spread, double pmax,
                           const std::array<int, 11>& verses, bool hit_air, bool spare_house,
                           double z, int wall_flags) {
    const double radius = std::max(0.5, std::round(spread)) * kLept;
    const std::vector<int> vv(verses.begin(), verses.end());
    for (int j = 0; j < static_cast<int>(units_.size()); ++j) {
        const Unit& w = units_[j];
        if (!w.alive || w.die_time >= 0.0 || w.cargo_of >= 0) continue;
        if (!hit_air && w.is_aircraft && w.altitude > 0.0) continue;
        if (spare_house && w.house == house) continue;
        double d = std::hypot((w.fx - cx) * kLept, (w.fy - cy) * kLept);
        // W4: an explosion above the ground (a rocket shot down in flight) is
        // measured in 3-D [FUN_00489190 distance], so ground objects far below
        // take nothing. z = 0 (every other caller) is the old 2-D test.
        if (z > 0.0) d = std::hypot(d, z - ((w.is_aircraft && w.altitude > 0.0) ? w.altitude : 0.0));
        if (d > radius) continue;
        const double scale = 1.0 - (1.0 - pmax) * (d / radius);
        const int dmg = damage_vs(static_cast<int>(damage * scale), vv, w.armor_ix);
        an_damage_unit(j, dmg, -1);
    }
    // W6-04-04/05 [RE: FUN_00489190 @0x489581]: wall cells inside the blast
    // take Reduce_Wall per the warhead's wall keys (a Nuke's
    // WallAbsoluteDestroyer erases them), never an HP hit.
    const int wr = static_cast<int>(std::max(0.5, std::round(spread)));
    if (has_wall_overlays() && z <= 0.0)
        for (int dy = -wr; dy <= wr; ++dy)
            for (int dx = -wr; dx <= wr; ++dx) {
                const int x = static_cast<int>(std::lround(cx)) + dx, y = static_cast<int>(std::lround(cy)) + dy;
                if (std::hypot((x - cx) * kLept, (y - cy) * kLept) <= radius)
                    w6_overlay_wall_hit(x, y, wall_flags, damage);
            }
    for (int bi = 0; bi < static_cast<int>(buildings_.size()); ++bi) {
        const Building& b = buildings_[bi];
        if (!b.alive || b.state == Building::State::Dead) continue;
        if (spare_house && b.house == house) continue;
        if (b.type >= 0 && b.type < static_cast<int>(btypes().size()) &&
            btypes()[static_cast<std::size_t>(b.type)].w6.wall_levels > 0) {   // W6-04-04
            if (z <= 0.0 && std::hypot((b.cx - cx) * kLept, (b.cy - cy) * kLept) <= radius)
                w6_wall_building_hit(*this, units_, buildings_, bi, wall_flags, damage);
            continue;
        }
        double d = -1.0;
        for (const Cellxy& c : foundation_cells(b.cx, b.cy, b.fw, b.fh)) {
            const double dc = std::hypot((c.x - cx) * kLept, (c.y - cy) * kLept);
            if (d < 0.0 || dc < d) d = dc;
        }
        if (z > 0.0 && d >= 0.0) d = std::hypot(d, z);   // W4: 3-D (see above)
        if (d < 0.0 || d > radius) continue;
        const double scale = 1.0 - (1.0 - pmax) * (d / radius);
        an_damage_building(bi, damage_vs(static_cast<int>(damage * scale), vv, b.armor_ix));
    }
}

// ----------------------------------------------------------------- effects --

void World::an_nuke_detonate(const SwStrike& s) {
    const HouseSuperweapon* rec = sw_rec(s.house, SuperweaponType::Nuke);
    // The blast params travel with the grant; a silo sold mid-flight still
    // lands its missile with the stock [NukePayload]/[NUKE] values.
    int dmg = 600;
    double spread = 10.0, pmax = 0.02;
    std::array<int, 11> verses{};
    verses.fill(100);
    if (rec) {
        dmg = rec->eff_damage;
        spread = rec->eff_cell_spread;
        pmax = rec->eff_percent_at_max;
        verses = rec->eff_verses;
    } else if (an_.nuke_set) {
        dmg = an_.nuke_damage;
        spread = an_.nuke_spread;
        pmax = an_.nuke_pmax;
        verses = an_.nuke_verses;
    }
    // W2-AN-06: everything in the blast, the firer's own units and airborne
    // units included (FUN_00489190 has no house or layer filter).
    an_area_damage(s.house, s.x, s.y, dmg, spread, pmax, verses, /*hit_air=*/true,
                   /*spare_house=*/false, 0.0, an_.sw.nuke_wall_flags);   // W6-04-04: walls erased
    // [NukePayload] RadLevel=500: a radiation site over the blast (P4 RadField;
    // spread = the warhead CellSpread, as BulletClass::Detonate's RadSite block).
    if (an_.sw.nuke_rad_level > 0 && grid_w_ > 0 && grid_h_ > 0) {
        if (rad_.empty()) rad_.init(grid_w_, grid_h_);
        rad_.detonate(s.x, s.y, static_cast<int>(spread), an_.sw.nuke_rad_level, ability_frame_,
                      ability_rules_);
    }
    an_event(SwEvent::Detonated, s.house, SuperweaponType::Nuke, s.x, s.y, s.ox, s.oy);
}

void World::an_iron_curtain(int house, int cx, int cy) {
    (void)house;
    const double secs = an_.sw.iron_frames / 15.0;
    for (const auto& o : kArea3x3) {
        const int x = cx + o[0], y = cy + o[1];
        for (int i = 0; i < static_cast<int>(units_.size()); ++i) {
            Unit& u = units_[i];
            if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) continue;
            if (u.cell_x != x || u.cell_y != y) continue;
            if (u.an.t.organic) {
                // [RE-FALLBACK: the IronCurtain virtual (+0x154) body was not
                // decompiled; classic RA2 kills infantry caught in the curtain.]
                u.an.iron_left = 0.0;
                an_damage_unit(i, u.hp, -1);
                continue;
            }
            u.an.iron_left = secs;
        }
        const int b = building_at_cell(x, y);
        if (b >= 0) buildings_[b].iron_left = secs;
    }
}

void World::an_chrono(int house, int sx, int sy, int tx, int ty) {
    (void)house;
    const double freeze = an_.sw.chrono_delay_frames / 15.0;
    // Collect first (a teleported unit must not be picked up again at its
    // destination when the two areas overlap).
    std::vector<std::pair<int, int>> movers;   // unit, offset index
    for (int k = 0; k < 9; ++k) {
        const int x = sx + kArea3x3[k][0], y = sy + kArea3x3[k][1];
        for (int i = 0; i < static_cast<int>(units_.size()); ++i) {
            const Unit& u = units_[i];
            if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) continue;
            if (u.is_aircraft && u.altitude > 0.0) continue;   // airborne: not in the cell layer
            if (u.cell_x == x && u.cell_y == y) movers.push_back({i, k});
        }
    }
    for (const auto& m : movers) {
        const int i = m.first;
        Unit& u = units_[i];
        // The chrono-kill branch: Organic && !Teleporter -> full-Strength damage.
        if (u.an.t.organic && !u.an.t.teleporter) {
            u.an.iron_left = 0.0;
            an_damage_unit(i, u.hp, -1);
            continue;
        }
        const int dx = tx + kArea3x3[m.second][0], dy = ty + kArea3x3[m.second][1];
        // [RE-FALLBACK: vtable +0x480 (the warp-in) not decompiled] a unit that
        // lands where its domain cannot stand (a tank in the sea, a ship on
        // land) is destroyed; an occupied landing cell slides to the nearest
        // free cell of its domain within 2 cells.
        int fx = -1, fy = -1;
        if (unit_can_enter_cell(u, dx, dy)) {
            for (int r = 0; r <= 2 && fx < 0; ++r)
                for (int oy = -r; oy <= r && fx < 0; ++oy)
                    for (int ox = -r; ox <= r && fx < 0; ++ox) {
                        if (std::max(std::abs(ox), std::abs(oy)) != r) continue;
                        const int cx = dx + ox, cy = dy + oy;
                        if (!unit_can_enter_cell(u, cx, cy)) continue;
                        if (!occ_.is_free(cx, cy) || building_at_cell(cx, cy) >= 0) continue;
                        fx = cx;
                        fy = cy;
                    }
        }
        if (fx < 0) {
            u.an.iron_left = 0.0;
            an_damage_unit(i, u.hp, -1);
            continue;
        }
        occ_.release_all_of(i, -1, -1);
        u.cell_x = fx;
        u.cell_y = fy;
        u.fx = fx;
        u.fy = fy;
        u.path.clear();
        u.path_idx = 0;
        u.target = -1;
        u.target_building = -1;
        u.attack_order = false;
        u.guard_x = fx;
        u.guard_y = fy;
        if (!u.is_aircraft) occ_.set(fx, fy, i);
        if (const ra2assets::MapCell* mc = map_ ? map_->at(fx, fy) : nullptr) u.level = mc->level;
        u.an.chrono_left = freeze;
        u.frozen = true;
    }
}

bool World::trigger_lightning_strike(int house, int cx, int cy) {
    if (cx < 0 || cy < 0 || cx >= grid_w_ || cy >= grid_h_) return false;
    SwStrike s;
    s.kind = 2;
    s.house = house;
    s.x = cx;
    s.y = cy;
    an_storm_bolt(s, cx, cy);
    an_.any = true;
    return true;
}

bool World::trigger_lightning_storm(int house, int cx, int cy) {
    if (cx < 0 || cy < 0 || cx >= grid_w_ || cy >= grid_h_) return false;
    for (const SwStrike& s : an_.strikes)
        if (s.kind == 2 && !s.done) return false;   // LightningStorm::IsActive
    SwStrike s;
    s.kind = 2;
    s.house = house;
    s.x = cx;
    s.y = cy;
    s.rng = 0x9E3779B9u ^ (static_cast<std::uint32_t>(cx) * 73856093u) ^
            (static_cast<std::uint32_t>(cy) * 19349663u) ^ an_rand();
    an_.strikes.push_back(s);
    an_event(SwEvent::Activated, house, SuperweaponType::LightningStorm, cx, cy);
    an_.any = true;
    sw_any_ = true;   // step_superweapon runs the strike list
    return true;
}

void World::an_storm_bolt(SwStrike& s, int x, int y) {
    std::array<int, 11> verses = an_.sw.storm_verses;
    bool any = false;
    for (int v : verses) any |= v != 0;
    if (!any) verses.fill(100);
    an_area_damage(s.house, x, y, an_.sw.storm_damage, an_.sw.storm_wh_spread, an_.sw.storm_wh_pmax,
                   verses, /*hit_air=*/false, /*spare_house=*/false, 0.0, an_.sw.storm_wall_flags);   // W6-04-04
    an_event(SwEvent::Bolt, s.house, SuperweaponType::LightningStorm, x, y);
    s.recent.push_back(x);
    s.recent.push_back(y);
    if (s.recent.size() > 12) s.recent.erase(s.recent.begin(), s.recent.begin() + 2);
}

void World::an_step_strikes(double dt) {
    if (an_.strikes.empty()) return;
    const int nf = frames_of(dt);
    const SwParams& P = an_.sw;
    for (std::size_t k = 0; k < an_.strikes.size(); ++k) {
        for (int f = 0; f < nf && !an_.strikes[k].done; ++f) {
            SwStrike& s = an_.strikes[k];
            ++s.frame;
            if (s.kind == 1) {   // nuke: up leg, then the down leg, then the blast
                if (s.frame == P.nuke_up_frames)
                    an_event(SwEvent::NukeFalling, s.house, SuperweaponType::Nuke, s.x, s.y, s.ox, s.oy);
                if (s.frame >= P.nuke_up_frames + P.nuke_down_frames) {
                    const SwStrike copy = s;
                    an_.strikes[k].done = true;
                    an_nuke_detonate(copy);
                }
            } else if (s.kind == 2) {   // lightning storm
                // [RE-FALLBACK: FUN_00539be0/00539e10 scheduler not unwound]
                // LightningDeferment frames of gathering clouds, then
                // LightningStormDuration frames of bolts: one on the target every
                // LightningHitDelay frames, one random bolt every
                // LightningScatterDelay frames inside the LightningCellSpread
                // square, kept LightningSeparation (city-block) from recent bolts.
                if (s.frame <= P.storm_deferment) continue;
                const int t = s.frame - P.storm_deferment;
                if (t > P.storm_duration) {
                    s.done = true;
                    continue;
                }
                if (++s.hit_timer >= P.storm_hit_delay) {
                    s.hit_timer = 0;
                    an_storm_bolt(s, s.x, s.y);
                }
                if (++s.scatter_timer >= P.storm_scatter_delay) {
                    s.scatter_timer = 0;
                    const int half = std::max(1, P.storm_cell_spread / 2);
                    for (int tries = 0; tries < 8; ++tries) {
                        s.rng = s.rng * 1664525u + 1013904223u;
                        const int rx = s.x - half + static_cast<int>((s.rng >> 8) % (2 * half));
                        s.rng = s.rng * 1664525u + 1013904223u;
                        const int ry = s.y - half + static_cast<int>((s.rng >> 8) % (2 * half));
                        if (rx < 0 || ry < 0 || rx >= grid_w_ || ry >= grid_h_) continue;
                        bool close = false;
                        for (std::size_t r = 0; r + 1 < s.recent.size(); r += 2)
                            if (std::abs(s.recent[r] - rx) + std::abs(s.recent[r + 1] - ry) <
                                P.storm_separation)
                                close = true;
                        if (close) continue;
                        an_storm_bolt(s, rx, ry);
                        break;
                    }
                }
            }
        }
    }
    an_.strikes.erase(std::remove_if(an_.strikes.begin(), an_.strikes.end(),
                                     [](const SwStrike& s) { return s.done; }),
                      an_.strikes.end());
}

}  // namespace ra2engine
