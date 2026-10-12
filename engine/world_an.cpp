// world_an.cpp -- wave-2 W2-AN naval + aircraft World pass (step_air_naval):
// submarine submerge/sonar (W2-AN-13), Naval Yard repair (W2-AN-11), ship
// sinking (W2-AN-25), Iron Curtain / Chrono timers (W2-AN-04), the spawner
// weapons -- Carrier Hornets, Destroyer ASW Osprey, Dreadnought/V3 missiles
// (W2-AN-12), fixed-wing aircraft sorties from Airforce Command pads
// (W2-AN-01/02), the paradrop plane (W2-AN-17), Night Hawk landing (W2-AN-08)
// and Crashable deaths (W2-AN-07). Split out of world.cpp.
#include <algorithm>
#include <cmath>

#include "combat.hpp"
#include "orig_math.hpp"   // Net L0: the original's FastMath, never libm in the sim
#include "flight.hpp"
#include "group_move.hpp"   // nearest_free_cells (W4-SQ-06)
#include "roster2.hpp"
#include "weapon_unit.hpp"
#include "world.hpp"

namespace ra2engine {

namespace {
constexpr double kLept = 256.0;
constexpr double kFlyClimbLps = 100.0 * 15.0;   // [RE-FALLBACK roster2.md §C] 100 leptons/frame
// W4: a rocket on its launcher sits this high (the deck) [RE-FALLBACK: the
// spawn's coords on the launcher were not traced]; the safety lifetime.
constexpr double kRocketDeckZ = 40.0;
constexpr int kRocketMaxFrames = 1800;
enum Sortie : int { kDocked = 0, kOutbound = 1, kMoveTo = 2, kReturn = 3, kLanding = 4, kParadrop = 5 };
bool live(const Unit& u) { return u.alive && u.die_time < 0.0 && u.cargo_of < 0; }
double cdist(double ax, double ay, double bx, double by) { return om::hypot(ax - bx, ay - by); }
}  // namespace

// --------------------------------------------------------------- targeting --

int an_naval_slot(const AnType& a, int want, bool has_secondary, bool victim_in_water,
                  bool victim_submerged, bool victim_organic, bool victim_airborne,
                  bool victim_is_building) {
    // "This has no impact on anti/not anti air weapons" (rules.ini comment).
    if (victim_airborne) return want;
    if (victim_is_building || !victim_in_water) {
        if (a.land_targeting == 1) return -1;                 // LAND_NOT_OKAY
        if (a.naval_targeting == kNavUnderwaterOnly) return -1;
        return want;
    }
    switch (a.naval_targeting) {
        case kNavNavalNone: return -1;
        case kNavUnderwaterNever: return victim_submerged ? -1 : want;
        case kNavUnderwaterSecondary:
            if (victim_submerged) return has_secondary ? 1 : -1;
            return want;
        case kNavUnderwaterOnly: return victim_submerged ? (want >= 0 ? want : 0) : -1;
        case kNavOrganicSecondary:
            if (victim_organic && has_secondary) return 1;
            return want;
        case kNavSealSpecial:
            if (victim_organic) return 0;
            return has_secondary ? 1 : -1;
        case kNavNavalAll:
        default: return want >= 0 ? want : 0;
    }
}

// ------------------------------------------------------------- spawning ----

int World::an_spawn(const std::string& id, int house, int x, int y) {
    auto it = an_.types.find(id);
    if (it == an_.types.end()) return -1;
    const UnitCombat& c = it->second;
    add_combat_unit(std::max(0, std::min(grid_w_ - 1, x)), std::max(0, std::min(grid_h_ - 1, y)),
                    house, c);
    const int ui = static_cast<int>(units_.size()) - 1;
    Unit& u = units_[ui];
    // A flier holds no ground cell (add_unit claimed one).
    if (u.is_aircraft) occ_.release_all_of(ui, -1, -1);
    if (!anims_.count(u.image)) anims_[u.image] = default_anim();
    an_.any = true;
    return ui;
}

bool World::order_fixed_wing(int idx, int unit_target, int building_target, int gx, int gy) {
    if (idx < 0 || idx >= static_cast<int>(units_.size())) return false;
    Unit& u = units_[idx];
    if (!u.an.t.fixed_wing || u.an.t.spawned) return false;
    an_.any = true;
    u.an.attack_unit = unit_target;
    u.an.attack_building = building_target;
    u.an.attack_x = gx;
    u.an.attack_y = gy;
    u.target = -1;
    u.target_building = -1;
    u.attack_order = false;
    u.path.clear();
    u.path_idx = 0;
    u.has_flight_goal = false;
    if (unit_target >= 0 || building_target >= 0) {
        u.an.strafe_left = 0;              // W7-05-02: a new order starts a fresh run
        if (u.an.ammo <= 0) return true;   // empty: stays docked until rearmed
        u.an.sortie = kOutbound;
    } else {
        u.an.sortie = kMoveTo;
    }
    if (u.flight_state == FlightState::Grounded) {
        occ_.release_all_of(idx, -1, -1);
        u.flight_state = FlightState::TakingOff;
    }
    return true;
}

// ------------------------------------------------------------------ naval ---

void World::an_step_naval(double dt) {
    const SwParams& P = an_.sw;
    const int n = static_cast<int>(units_.size());
    const auto& water = water_passable_;
    for (int i = 0; i < n; ++i) {
        Unit& u = units_[i];
        if (!u.alive) continue;
        if (u.is_naval) {
            u.an.in_water = true;
        } else if (u.movement_domain == MovementDomain::Amphibious) {
            const std::size_t c = static_cast<std::size_t>(u.cell_y) * grid_w_ + u.cell_x;
            u.an.in_water = c < water.size() && water[c] != 0 &&
                            (c >= passable_.size() || passable_[c] == 0);
        }
        // W2-AN-13 [RE: rules [General] CloakDelay=.02 -> 18 frames, "forced
        // delay that subs will remain on surface before allowing to submerge"]:
        // an Underwater+Cloakable unit is submerged unless it fired within the
        // last CloakDelay frames (firing surfaces it).
        if (u.an.t.underwater && u.an.t.cloakable) {
            if (u.die_time >= 0.0) {
                u.an.submerged = false;
            } else {
                // W6-04-25: a DecloakToFire=no weapon fires without surfacing.
                if (u.fire_flash >= 0.4 - 1e-9 && u.w6.t.decloak_to_fire)
                    u.an.surface_left_frames = P.cloak_delay_frames;
                if (u.an.surface_left_frames > 0) --u.an.surface_left_frames;   // Net L0: whole frames
                u.an.submerged = u.an.surface_left_frames <= 0;
            }
        }
        // Iron Curtain / Chrono freeze countdowns.
        if (u.an.iron_left_frames > 0) --u.an.iron_left_frames;   // Net L0: whole frames
        if (u.an.chrono_left_frames > 0) {
            --u.an.chrono_left_frames;
            if (u.an.chrono_left_frames <= 0 && !u.abil_bound) u.frozen = false;
        }
    }
    for (Building& b : buildings_)
        if (b.iron_left_frames > 0) --b.iron_left_frames;
    // Sonar [RE: naval.md Deferred, FUN_00740780; rules Sensors=yes
    // SensorsSight=]: a submerged sub is visible (and so targetable) to a house
    // that has a live Sensors unit within SensorsSight cells of it.
    for (int j = 0; j < n; ++j) {
        Unit& v = units_[j];
        v.an.detected_mask = 0;
        if (!v.an.submerged || !live(v)) continue;
        v.an.detected_mask = static_cast<std::uint8_t>(1u << (v.house & 7));
        for (int i = 0; i < n; ++i) {
            const Unit& s = units_[i];
            if (i == j || !s.an.t.sensors || !live(s) || s.house < 0 || s.house > 7) continue;
            if (cdist(s.fx, s.fy, v.fx, v.fy) <= s.an.t.sensors_sight)
                v.an.detected_mask |= static_cast<std::uint8_t>(1u << s.house);
        }
    }
    // W2-AN-11 Naval Yard UnitRepair=yes: a damaged ship of the yard's house,
    // stopped within one cell of the yard, is repaired RepairStep hit points
    // every RepairRate. [RE-FALLBACK: the service-depot cadence; the UnitRepair
    // dock cost per step is not charged -- the ship repairs free.]
    if (!an_.repair_btype.empty()) {
        const bool tick = (an_.frame % std::max(1, P.repair_rate_frames)) == 0;
        if (tick)
            for (const Building& b : buildings_) {
                if (!b.alive || b.state != Building::State::Idle) continue;
                if (b.type < 0 || b.type >= static_cast<int>(an_.repair_btype.size()) ||
                    !an_.repair_btype[b.type])
                    continue;
                for (Unit& u : units_) {
                    if (!live(u) || !u.is_naval || u.house != b.house || u.hp >= u.max_hp) continue;
                    if (u.moving() || u.target >= 0) continue;
                    const int dx = u.cell_x < b.cx ? b.cx - u.cell_x : (u.cell_x >= b.cx + b.fw ? u.cell_x - (b.cx + b.fw - 1) : 0);
                    const int dy = u.cell_y < b.cy ? b.cy - u.cell_y : (u.cell_y >= b.cy + b.fh ? u.cell_y - (b.cy + b.fh - 1) : 0);
                    if (std::max(dx, dy) > 1) continue;
                    u.hp = std::min(u.max_hp, u.hp + P.repair_step);
                    // W6-04-09 [RE-FALLBACK: the yard repair taken as a healing
                    // (negative) hit, which ejects a parasite in FUN_004D7250]
                    if (u.parasite_on_me >= 0) u.w6.heal_hit = true;
                }
            }
    }
}

// --------------------------------------------------------------- spawners ---

// W4: a rocket's target point in cells (+ its height in leptons): a live
// unit (its altitude when airborne) or a building's centre.
bool World::rocket_target_point(int tu, int tb, double& x, double& y, double& z) const {
    const int n = static_cast<int>(units_.size());
    if (tu >= 0 && tu < n && live(units_[tu])) {
        const Unit& t = units_[tu];
        x = t.fx;
        y = t.fy;
        z = (t.is_aircraft && t.altitude > 0.0) ? t.altitude : 0.0;
        return true;
    }
    if (tb >= 0 && tb < static_cast<int>(buildings_.size()) && buildings_[tb].alive &&
        buildings_[tb].state != Building::State::Dead) {
        const Building& b = buildings_[tb];
        x = b.cx + (b.fw - 1) / 2.0;
        y = b.cy + (b.fh - 1) / 2.0;
        z = 0.0;
        return true;
    }
    return false;
}

// W4: a rocket explodes [FUN_00662A70]: its own block's warhead (elite when
// the launcher was elite) at the nose point; then it is gone. A rocket SHOT
// DOWN dies the ordinary TechnoClass death instead: Explodes=no on
// V3ROCKET/DMISL, so no warhead damage -- only its Explosion= anims (the
// Process tail's Strength<1 -> FUN_00662A70 path is the defensive case for a
// rocket left alive at 0 HP) [PROBABLE: ALL01's Patriots, 7 cells from the
// fleet, would otherwise blow up the Dreadnoughts with their own missiles].
void World::rocket_explode(int i, int end) {
    Unit& u = units_[i];
    const RocketBlock& blk = an_.sw.rockets.block[static_cast<std::size_t>(
        std::clamp(u.an.t.rocket_kind < 0 ? 1 : u.an.t.rocket_kind, 0, 2))];
    const RocketWarhead& wh = rocket_warhead(blk, u.an.rocket_elite);
    double nx = 0, ny = 0, nz = 0;
    rocket_nose(blk, u.an.rocket, nx, ny, nz);
    if (end == 2) {   // shot down: the explosion is where the rocket is
        nx = u.an.rocket.x;
        ny = u.an.rocket.y;
        nz = u.an.rocket.z;
    }
    u.an.rocket_end = end;
    u.fx = nx / kLept;
    u.fy = ny / kLept;
    u.altitude = std::max(0.0, nz);
    const int house = u.house;
    if (end == 1)
        an_area_damage(house, nx / kLept, ny / kLept, rocket_damage(blk, u.an.rocket_elite), wh.spread,
                       wh.pmax, wh.verses, /*hit_air=*/false, /*spare_house=*/false, std::max(0.0, nz),
                       wh.wall_flags);   // W6-04-04 (V3WH / DMISLWH: Wall + Wood)
    Unit& r = units_[i];   // (an_area_damage never reallocates units_; re-fetch anyway)
    if (end == 1) {
        fire_events_.push_back({i, -1, -1, -1, 0, ""});
        r.alive = false;   // the rocket is spent (no wreck); DontScore
        r.die_time = 1.0;
    }   // (shot down: the kill already started its death)
    const int ow = r.an.owner;
    if (ow >= 0 && ow < static_cast<int>(units_.size()) && units_[ow].an.spawn_out > 0)
        --units_[ow].an.spawn_out;
}

void World::an_step_spawners(double dt) {
    (void)dt;
    const int n = static_cast<int>(units_.size());
    for (int i = 0; i < n; ++i) {
        Unit& o = units_[i];
        if (o.an.t.spawner_slot < 0) continue;
        if (!live(o)) {
            o.an.spawn_fire_unit = o.an.spawn_fire_building = -1;
            continue;
        }
        const AnType& t = o.an.t;
        // SpawnRegenRate: a lost spawn is rebuilt; SpawnReloadRate: a returned
        // one is rearmed [rules [CARRIER]/[DRED]/[DEST]].
        // W4 [PROBABLE: SpawnManagerClass, MissileSpawn nodes -- "missile
        // spawn don't come back" (SpawnReloadRate=0)]: a launched rocket's
        // node is spent at once, so SpawnRegenRate runs from the launch, not
        // from its detonation (ALL01: each Dreadnought rebuilds a missile
        // every 80 frames while the last ones are still in the air).
        const auto st = an_.types.find(t.spawns);
        const bool missile_spawns = st != an_.types.end() && st->second.an.missile;
        const int out_held = missile_spawns ? 0 : o.an.spawn_out;
        if (o.an.spawn_ready + out_held + o.an.spawn_pending < t.spawns_number) {
            if (++o.an.spawn_regen_timer >= std::max(1, t.spawn_regen_frames)) {
                o.an.spawn_regen_timer = 0;
                ++o.an.spawn_ready;
            }
        } else {
            o.an.spawn_regen_timer = 0;
        }
        if (o.an.spawn_pending > 0) {
            if (++o.an.spawn_reload_timer >= std::max(1, t.spawn_reload_frames)) {
                o.an.spawn_reload_timer = 0;
                --o.an.spawn_pending;
                ++o.an.spawn_ready;
            }
        }
        // A spawner "shot" (combat.cpp try_fire) launches spawns at the target:
        // missiles leave as a full volley, aircraft one per weapon ROF.
        const int tu = o.an.spawn_fire_unit, tb = o.an.spawn_fire_building;
        o.an.spawn_fire_unit = o.an.spawn_fire_building = -1;
        if ((tu < 0 && tb < 0) || o.an.spawn_ready <= 0) continue;
        auto it = an_.types.find(t.spawns);
        if (it == an_.types.end()) continue;
        const bool missile = it->second.an.missile;
        const int count = missile ? o.an.spawn_ready : 1;
        for (int k = 0; k < count; ++k) {
            const int si = an_spawn(t.spawns, o.house, o.cell_x, o.cell_y);
            if (si < 0) break;
            Unit& s = units_[si];
            Unit& own = units_[i];   // re-fetch: units_ may have reallocated
            s.fx = own.fx + 0.2 * k;
            s.fy = own.fy;
            s.cell_x = own.cell_x;
            s.cell_y = own.cell_y;
            s.an.owner = i;
            s.an.attack_unit = tu;
            s.an.attack_building = tb;
            s.an.sortie = kOutbound;
            s.an.ammo = s.an.t.ammo > 0 ? s.an.t.ammo : 1;
            s.flight_state = FlightState::TakingOff;
            s.altitude = 1.0;
            s.body_facing = own.body_facing;
            // W4: locomotor +0x4D -- the launcher's elite rank picks the elite warhead.
            s.an.rocket_elite = missile && is_elite(own.vet.veterancy);
            if (missile) {
                // W4 RocketLocomotion [FUN_00661D00]: on the launcher's deck,
                // PauseFrames + TiltFrames from the rocket's own block.
                double gx = s.fx, gy = s.fy, gz = 0.0;
                rocket_target_point(tu, tb, gx, gy, gz);
                const RocketBlock& blk = an_.sw.rockets.block[static_cast<std::size_t>(
                    std::clamp(s.an.t.rocket_kind < 0 ? 1 : s.an.t.rocket_kind, 0, 2))];
                rocket_launch(blk, s.an.rocket, s.fx * kLept, s.fy * kLept, kRocketDeckZ, gx * kLept,
                              gy * kLept);
                s.an.rocket.tx = gx * kLept;
                s.an.rocket.ty = gy * kLept;
                s.an.rocket.tz = gz;
                s.altitude = kRocketDeckZ;
                s.body_facing = facing_u16_from_delta(om::cos(s.an.rocket.yaw), om::sin(s.an.rocket.yaw));
            }
            --own.an.spawn_ready;
            ++own.an.spawn_out;
        }
    }
}

// ------------------------------------------------------------- fixed wing ---

void World::an_step_fixed_wing(double dt) {
    const SwParams& P = an_.sw;
    const int n = static_cast<int>(units_.size());
    for (int i = 0; i < n; ++i) {
        Unit& u = units_[i];
        if (!(u.an.t.fixed_wing || u.an.t.missile) || !u.alive || u.die_time >= 0.0) continue;
        // (Spawned=yes: not player controllable -- Selection::prune drops it)
        // A player/AI attack order lands in target/target_building (commit_attack_target):
        // turn it into a sortie.
        if (!u.an.t.spawned && (u.target >= 0 || u.target_building >= 0)) {
            const int tu = u.target, tb = u.target_building;
            order_fixed_wing(i, tu, tb, -1, -1);
        }
        if (u.rearm_frames > 0) --u.rearm_frames;   // Net L0: whole frames
        u.fire_flash = std::max(0.0, u.fire_flash - dt);
        // --- where to fly ---
        double gx = u.fx, gy = u.fy;
        bool have_goal = false, target_alive = false;
        int tu = u.an.attack_unit, tb = u.an.attack_building;
        if (tu >= 0 && (tu >= n || !live(units_[tu]))) tu = u.an.attack_unit = -1;
        if (tb >= 0 && (tb >= static_cast<int>(buildings_.size()) || !buildings_[tb].alive ||
                        buildings_[tb].state == Building::State::Dead))
            tb = u.an.attack_building = -1;
        if (tu >= 0) {
            gx = units_[tu].fx; gy = units_[tu].fy; have_goal = target_alive = true;
        } else if (tb >= 0) {
            const Building& b = buildings_[tb];
            gx = b.cx + (b.fw - 1) / 2.0; gy = b.cy + (b.fh - 1) / 2.0; have_goal = target_alive = true;
        }
        // W4 RocketLocomotion [FUN_00661D00]: pause, tilt, climb to the block
        // Altitude, LazyCurve arc or level-off + dive, nose-contact explode.
        if (u.an.t.missile) {
            RocketFlight& f = u.an.rocket;
            if (have_goal) {   // the live target keeps being tracked
                double px = gx, py = gy, pz = 0.0;
                rocket_target_point(tu, tb, px, py, pz);
                f.tx = px * kLept;
                f.ty = py * kLept;
                f.tz = pz;
            }
            const RocketBlock& blk = P.rockets.block[static_cast<std::size_t>(
                std::clamp(u.an.t.rocket_kind < 0 ? 1 : u.an.t.rocket_kind, 0, 2))];
            const int r = rocket_step(blk, u.an.t.rocket_speed, f, f.tx, f.ty, f.tz);
            u.fx = f.x / kLept;
            u.fy = f.y / kLept;
            u.altitude = std::max(0.0, f.z);
            u.cell_x = std::max(0, std::min(grid_w_ - 1, static_cast<int>(std::lround(u.fx))));
            u.cell_y = std::max(0, std::min(grid_h_ - 1, static_cast<int>(std::lround(u.fy))));
            u.body_facing = facing_u16_from_delta(om::cos(f.yaw), om::sin(f.yaw));
            u.flight_state = f.phase >= kRocketClimb ? FlightState::Cruising : FlightState::TakingOff;
            const bool off_map = grid_w_ > 0 && grid_h_ > 0 &&
                                 (f.x < -kLept || f.y < -kLept || f.x > grid_w_ * kLept || f.y > grid_h_ * kLept);
            if (r == kRocketDetonate || off_map || f.frames > kRocketMaxFrames) rocket_explode(i, 1);
            continue;
        }
        // Home airfield (a spawned plane's home is its spawner).
        const bool spawned = u.an.t.spawned;
        double hx = u.fx, hy = u.fy;
        bool home_ok = false;
        if (spawned && u.an.drop_left <= 0 && u.an.sortie != kParadrop) {
            const int ow = u.an.owner;
            if (ow >= 0 && ow < n && live(units_[ow])) {
                hx = units_[ow].fx; hy = units_[ow].fy; home_ok = true;
            }
        } else if (!spawned) {
            const int hb = u.an.home;
            if (hb >= 0 && hb < static_cast<int>(buildings_.size()) && buildings_[hb].alive &&
                buildings_[hb].state != Building::State::Dead && buildings_[hb].house == u.house) {
                const Building& b = buildings_[hb];
                if (u.an.pad >= 0 && u.an.pad < static_cast<int>(an_.dock_offsets.size())) {
                    hx = (b.cx * kLept + b.fw * 128.0 + an_.dock_offsets[u.an.pad].first) / kLept - 0.5;
                    hy = (b.cy * kLept + b.fh * 128.0 + an_.dock_offsets[u.an.pad].second) / kLept - 0.5;
                } else {
                    hx = b.cx + (b.fw - 1) / 2.0; hy = b.cy + (b.fh - 1) / 2.0;
                }
                home_ok = true;
            }
        }
        if (!spawned && !home_ok && u.an.sortie != kDocked) {
            // Lost its airfield: claim a free pad on another own Helipad.
            for (int bi = 0; bi < static_cast<int>(buildings_.size()) && !home_ok; ++bi) {
                const Building& b = buildings_[bi];
                if (!b.alive || b.house != u.house || b.state == Building::State::Dead) continue;
                if (b.type < 0 || b.type >= static_cast<int>(an_.airfield_btype.size()) ||
                    !an_.airfield_btype[b.type])
                    continue;
                for (int pad = 0; pad < static_cast<int>(an_.dock_offsets.size()); ++pad) {
                    bool used = false;
                    for (const Unit& o : units_)
                        if (&o != &u && live(o) && o.an.home == bi && o.an.pad == pad) used = true;
                    if (used) continue;
                    u.an.home = bi;
                    u.an.pad = pad;
                    home_ok = true;
                    break;
                }
            }
        }
        // --- state machine ---
        double goal_x = u.fx, goal_y = u.fy;
        bool fly = false;
        switch (u.an.sortie) {
            case kDocked:
                // Docked: rearm one ammo point every ReloadRate (270 frames).
                if (u.an.ammo < u.an.t.ammo && home_ok) {
                    if (++u.an.reload_timer >= std::max(1, P.reload_frames)) {
                        u.an.reload_timer = 0;
                        ++u.an.ammo;
                    }
                }
                break;
            case kOutbound: {
                if (!target_alive || (u.an.ammo <= 0 && u.an.strafe_left <= 0)) {
                    u.an.strafe_left = 0;
                    u.an.sortie = kReturn;
                    break;
                }
                goal_x = gx; goal_y = gy; fly = true;
                // Attack pass: in weapon range -> fire one ammo point.
                // W7-05-02 [RE: Mission_Attack FUN_00417FE0 status 3]: a plane
                // that is neither Fighter nor Strafe fires only when it is over
                // the target (within 0xF leptons; it turns to face it inside
                // 0x200); Fighter / Strafe planes go straight to status 4
                // (fire when GetFireError allows: in range, rearmed).
                const bool pass_ok = u.an.t.fighter || u.an.t.strafe ||
                                     cdist(u.fx, u.fy, gx, gy) * kLept <= 15.0;
                if (pass_ok && cdist(u.fx, u.fy, gx, gy) <= std::max(1.0, u.w_range) && u.rearm_frames <= 0.0 &&
                    u.altitude >= P.flight_level * 0.5) {
                    // W7-05-02 [RE: FUN_00417FE0 statuses 6..9]: a Strafe
                    // weapon's run fires FOUR more single shots, one per ROF,
                    // through Fire (+0x3CC) without the fired flag -- no Ammo.
                    const bool strafe_shot = u.an.strafe_left > 0;
                    // W4: a flying projectile (the Harrier / Black Eagle
                    // Maverick, the Hornet / Osprey bombs: ROT>0 homing) is
                    // launched behind the P2 firewall and hits at detonation.
                    // W6-04-01 [RE: AircraftClass::Mission_Attack FUN_00417FE0
                    // case 4: on GetFireError OK it sets the fired flag (+0x6C8)
                    // and calls Fire Burst (+0x9C) times in the same frame; the
                    // flag is consumed as ONE Ammo (+0x2FC) decrement]: Boris's
                    // MiG (Maverick3 Ammo=1 Burst=2) puts both missiles in one pass.
                    if (tu >= 0) {
                        const int slot = unit_weapon_for_unit(u, units_[tu]);
                        if (slot >= 0) {
                            activate_weapon_slot(u, slot, veterancy_enabled() ? &veterancy_rules() : nullptr);
                            for (int k = 0; k < (strafe_shot ? 1 : std::max(1, u.w6.burst)); ++k) {
                                if (k > 0 && !live(units_[tu])) break;
                                if (projectiles_enabled_ && u.w_proj.flies()) launch_projectile(i, tu, -1);
                                else if (u.w_cell_spread >= 1.0) apply_spread_damage(units_, occ_, u, units_[tu]);
                                else deliver_unit_hit(units_, occ_, i, tu);
                            }
                        }
                    } else if (tb >= 0) {
                        const int slot = unit_weapon_for_building(u, buildings_[tb].armor_ix, buildings_[tb].bomb_frame >= 0);
                        if (slot >= 0) {
                            activate_weapon_slot(u, slot, veterancy_enabled() ? &veterancy_rules() : nullptr);
                            for (int k = 0; k < (strafe_shot ? 1 : std::max(1, u.w6.burst)); ++k) {
                                if (k > 0 && (!buildings_[tb].alive || buildings_[tb].hp <= 0)) break;
                                if (projectiles_enabled_ && u.w_proj.flies()) launch_projectile(i, -1, tb);
                                else if (buildings_[tb].iron_left_frames <= 0.0 &&
                                         !w6_wall_building_hit(*this, units_, buildings_, tb, u.w6.wall_flags, u.w_damage))   // W6-04-04
                                    deliver_building_hit(units_, buildings_, i, tb);
                            }
                        }
                    }
                    u.rearm_frames = std::max(1, u.w_rof);
                    u.fire_flash = 0.4;
                    fire_events_.push_back({i, -1, tu, tb, u.w_active, ""});
                    if (strafe_shot) {
                        --u.an.strafe_left;
                    } else {
                        --u.an.ammo;
                        if (u.an.t.strafe) u.an.strafe_left = 4;
                    }
                    // W7-05-02 [RE: FUN_00417FE0 statuses 1/5/10]: a Fighter
                    // re-attacks and any other plane turns and fires again
                    // while it has Ammo; at 0 (and the strafe run done) it
                    // returns to base.
                    if (u.an.ammo <= 0 && u.an.strafe_left <= 0) u.an.sortie = kReturn;
                }
                break;
            }
            case kMoveTo:
                goal_x = u.an.attack_x; goal_y = u.an.attack_y; fly = true;
                if (u.an.attack_x < 0 || cdist(u.fx, u.fy, goal_x, goal_y) < 0.3) {
                    // [RE-FALLBACK: a fixed-wing move order flies to the point,
                    // then returns to its airfield -- it cannot hover.]
                    u.an.attack_x = u.an.attack_y = -1;
                    u.an.sortie = kReturn;
                }
                break;
            case kParadrop: {
                // PDPLANE: straight run over the drop site, then off the map.
                // W4-SQ-06: a team plane drops its CARRIED members instead.
                if (!u.cargo.empty()) {
                    goal_x = u.an.drop_x;
                    goal_y = u.an.drop_y;
                    fly = true;
                    if (cdist(u.fx, u.fy, u.an.drop_x, u.an.drop_y) * kLept <= P.paradrop_radius &&
                        !an_drop_passenger(i)) {
                        // No landing room left: the plane flies on and tries
                        // again over its next cells (the original keeps
                        // looping over the target; RE-FALLBACK).
                        Unit& pl = units_[i];
                        pl.an.drop_x = std::max(0, std::min(grid_w_ - 1, pl.an.drop_x + (pl.an.exit_x > pl.an.drop_x ? 1 : pl.an.exit_x < pl.an.drop_x ? -1 : 0)));
                        pl.an.drop_y = std::max(0, std::min(grid_h_ - 1, pl.an.drop_y + (pl.an.exit_y > pl.an.drop_y ? 1 : pl.an.exit_y < pl.an.drop_y ? -1 : 0)));
                    }
                    break;
                }
                const bool dropping = u.an.drop_left > 0;
                goal_x = dropping ? u.an.drop_x : u.an.exit_x;
                goal_y = dropping ? u.an.drop_y : u.an.exit_y;
                fly = true;
                if (dropping && cdist(u.fx, u.fy, u.an.drop_x, u.an.drop_y) * kLept <= P.paradrop_radius) {
                    // One paratrooper per frame onto free land cells near the site.
                    int placed = 0;
                    for (int r = 0; r <= 4 && !placed; ++r)
                        for (int oy = -r; oy <= r && !placed; ++oy)
                            for (int ox = -r; ox <= r && !placed; ++ox) {
                                if (std::max(std::abs(ox), std::abs(oy)) != r) continue;
                                const int cx = u.an.drop_x + ox, cy = u.an.drop_y + oy;
                                if (!walkable(cx, cy) || !occ_.is_free(cx, cy) || building_at_cell(cx, cy) >= 0) continue;
                                const int pi = an_spawn(u.an.drop_type, u.house, cx, cy);
                                if (pi >= 0) {
                                    placed = 1;
                                    an_event(SwEvent::Dropped, units_[i].house, SuperweaponType::ParaDrop, cx, cy);
                                }
                            }
                    Unit& pl = units_[i];
                    if (placed) --pl.an.drop_left;
                    else pl.an.drop_left = 0;   // no room: the rest stay aboard
                } else if (!dropping && cdist(u.fx, u.fy, goal_x, goal_y) < 0.5) {
                    units_[i].alive = false;   // left the map
                    units_[i].die_time = 1.0;
                    units_[i].deployed_away = true;   // W4-SQ-03: gone, not destroyed (no loss)
                    continue;
                }
                break;
            }
            case kReturn:
                if (!home_ok) {
                    // AirportBound=yes with no airfield (or a spawn whose carrier
                    // died): it crashes.
                    u.an.crashing = false;
                    an_damage_unit(i, u.hp, -1);
                    break;
                }
                goal_x = hx; goal_y = hy; fly = true;
                if (cdist(u.fx, u.fy, hx, hy) < 0.25) {
                    if (spawned) {   // back aboard the carrier: rearm there
                        const int ow = u.an.owner;
                        units_[i].alive = false;
                        units_[i].die_time = 1.0;
                        // W6-04-26: back aboard, not destroyed -- no unit lost on
                        // the score screen (the W4-SQ-03 deployed_away rule).
                        units_[i].deployed_away = true;
                        if (ow >= 0 && ow < n) {
                            if (units_[ow].an.spawn_out > 0) --units_[ow].an.spawn_out;
                            ++units_[ow].an.spawn_pending;
                        }
                        continue;
                    }
                    u.an.sortie = kLanding;
                }
                break;
            case kLanding:
                goal_x = hx; goal_y = hy;
                u.fx = hx; u.fy = hy;
                break;
            default:
                break;
        }
        // --- altitude + translation ---
        // W7-05-02 FlyLocomotionClass [RE: movement FUN_004CD520]: the plane
        // moves ftol(type+0x678 x CurrentSpeed) leptons per frame along its
        // CURRENT body facing (@0x4CD9A4) while the FacingClass turns it at
        // ROT<<8 u16/frame toward the goal -- so it overflies and loops back
        // instead of reversing on the spot. Height: climb min(d, 20), descend
        // (cruise and landing) clamp(d/20, 20, 50) leptons/frame. The throttle
        // ramps 0.1/frame; an IsTakingOff plane gets no throttle until it is
        // at half the FlightLevel, and a plane not on an attack run slows over
        // its SlowdownDistance (FUN_004D00A0).
        const double fr = dt * 15.0;
        const double fl = P.flight_level;
        bool stopped = false;
        if (u.an.sortie == kLanding) {
            u.altitude = fly_altitude_step(u.altitude, 0.0, fr);
            u.flight_state = u.altitude > 0.0 ? FlightState::Descending : FlightState::Grounded;
            u.an.fly_throttle = 0.0;
            if (u.altitude <= 0.0) {
                u.an.sortie = kDocked;
                u.an.reload_timer = 0;
            }
        } else if (fly) {
            const bool taking_off = u.flight_state == FlightState::TakingOff ||
                                    u.flight_state == FlightState::Grounded;
            u.altitude = fly_altitude_step(u.altitude, fl, fr);
            if (u.altitude >= fl) u.flight_state = FlightState::Cruising;
            else if (taking_off) u.flight_state = FlightState::TakingOff;
            const double dist = cdist(u.fx, u.fy, goal_x, goal_y) * kLept;
            const bool slow = u.an.sortie == kReturn || u.an.sortie == kMoveTo;
            double tgt = fly_throttle_target(dist, u.an.t.slowdown_distance, slow, &stopped);
            if (stopped) u.an.fly_throttle *= 0.5;            // DAT_007E0738 = 0.5
            if (u.flight_state == FlightState::TakingOff && u.altitude < fl / 2.0) tgt = 0.0;
            u.an.fly_throttle = fly_throttle_ramp(u.an.fly_throttle, tgt, fr);
            if (dist > 0.0)
                u.body_facing = heading_toward(u.body_facing, goal_x - u.fx, goal_y - u.fy,
                                               u.flight_turn * dt, false);
            const double full = u.an.t.fly_speed_cps * kLept / 15.0;   // type+0x678 (vet-scaled)
            const double step = std::trunc(full * u.an.fly_throttle) * fr;
            step_along_facing(u.fx, u.fy, u.body_facing, step);
            // The plane may loop outside the map edge; it never leaves the grid.
            u.fx = std::clamp(u.fx, -4.0, static_cast<double>(grid_w_ + 3));
            u.fy = std::clamp(u.fy, -4.0, static_cast<double>(grid_h_ + 3));
            // Arrival at a POINT the plane has slowed to a stop near (it can
            // stop just outside the snap radius) counts as reached.
            if ((u.an.sortie == kReturn || u.an.sortie == kMoveTo) && stopped &&
                u.an.fly_throttle < 0.05) {
                if (u.an.sortie == kReturn) {
                    u.fx = hx;
                    u.fy = hy;
                } else {
                    u.an.attack_x = u.an.attack_y = -1;
                    u.an.sortie = kReturn;
                }
            }
        }
        u.cell_x = std::max(0, std::min(grid_w_ - 1, static_cast<int>(std::lround(u.fx))));
        u.cell_y = std::max(0, std::min(grid_h_ - 1, static_cast<int>(std::lround(u.fy))));
        if (fly) {
            const double a = u.body_facing / 65536.0 * 6.283185307179586;
            const int f = facing_from_delta(om::sin(a), -om::cos(a));
            if (f >= 0) u.facing = f;
        }
    }
}

// --------------------------------------------------------- death variants ---

void World::an_step_deaths(double dt) {
    const SwParams& P = an_.sw;
    const int n = static_cast<int>(units_.size());
    for (int i = 0; i < n; ++i) {
        Unit& u = units_[i];
        if (!u.alive || u.die_time < 0.0) continue;
        // W4: a rocket killed in flight (FUN_00661D00: Strength < 1 ->
        // FUN_00662A70) -- shot down; its launcher may rebuild it
        // (SpawnRegenRate counts it lost, not out).
        if (u.an.t.missile) {
            if (u.an.rocket_end == 0) {
                // FUN_00661D00 tail: Strength < 1 -> FUN_00662A70 at the rocket.
                rocket_explode(i, 2);
            }
            continue;
        }
        // W2-AN-07 [RE: aircraft.md §2 Crashable +0xD95; JumpjetCrash]: a
        // Crashable flier (and every fixed-wing aircraft) killed in the air
        // falls at its crash rate, then explodes where it hits the ground.
        if (u.is_aircraft && u.altitude > 0.0 && (u.an.t.crashable || u.an.t.fixed_wing) &&
            !u.an.t.missile) {
            u.an.crashing = true;
            u.die_time = 0.0;   // hold the death anim until impact
            const double rate = u.descend_rate > 0.0 ? u.descend_rate : kFlyClimbLps;
            u.altitude = std::max(0.0, u.altitude - std::max(rate, 12.0 * 15.0) * dt);
            u.flight_state = FlightState::Descending;
            u.has_flight_goal = false;
            if (u.an.t.fixed_wing) {   // keeps gliding forward while falling
                const double a = u.body_facing / 65536.0 * 2.0 * 3.14159265358979;
                u.fx += om::sin(a) * u.an.t.fly_speed_cps * 0.5 * dt;
                u.fy -= om::cos(a) * u.an.t.fly_speed_cps * 0.5 * dt;
                u.cell_x = std::max(0, std::min(grid_w_ - 1, static_cast<int>(std::lround(u.fx))));
                u.cell_y = std::max(0, std::min(grid_h_ - 1, static_cast<int>(std::lround(u.fy))));
            } else {
                // W7-05-15 [RE: Jumpjet Crashing FUN_0054C7C0 -- desired facing
                // = current + 0x2D00 every frame, so it spins at its
                // JumpJetTurnRate<<8; the integrator FUN_0054CDA0 keeps moving
                // ftol(CurSpeed) leptons along the facing while it falls].
                const double fr = dt * 15.0;
                u.body_facing = std::fmod(u.body_facing + std::min(u.flight_turn / 15.0, 11520.0) * fr,
                                          65536.0);
                step_along_facing(u.fx, u.fy, u.body_facing, std::trunc(u.an.jj_speed) * fr);
                u.cell_x = std::max(0, std::min(grid_w_ - 1, static_cast<int>(std::lround(u.fx))));
                u.cell_y = std::max(0, std::min(grid_h_ - 1, static_cast<int>(std::lround(u.fy))));
            }
            if (u.altitude <= 0.0) {
                u.an.crashing = false;
                u.flight_state = FlightState::Grounded;
                // [RE-FALLBACK: the crash impact damage was not traced] the
                // wreck deals a quarter of its Strength over one cell.
                std::array<int, 11> verses{};
                verses.fill(100);
                const int dmg = std::max(10, u.max_hp / 4);
                const double fx = u.fx, fy = u.fy;
                const int house = u.house;
                // W6-04-04 [RE-FALLBACK: the crash blast's warhead is not
                // traced; taken as the Kirov's BlimpHE, Wall + Wood].
                an_area_damage(house, fx, fy, dmg, 1.0, 0.5, verses, false, false, 0.0, kW6Wall | kW6Wood);
            }
            continue;
        }
        // W2-AN-25 [rules [General] ShipSinkingWeight=3.0: "Surface ships of
        // this or greater weight will sink, not explode"].
        if (u.is_naval && !u.an.t.underwater && u.an.t.weight >= P.ship_sinking_weight)
            u.an.sinking = true;
    }
}

// ------------------------------------------------------------- paradrop ----

void World::an_paradrop(int house, SuperweaponType type, int cx, int cy) {
    const SwParams& P = an_.sw;
    std::string inf;
    int num = 0;
    if (type == SuperweaponType::AmerParaDrop) {
        inf = P.amer_para_inf;
        num = P.amer_para_num;
    } else {
        const bool soviet = house_side(house) == Side::Soviet;
        inf = soviet ? P.sov_para_inf : P.ally_para_inf;
        num = soviet ? P.sov_para_num : P.ally_para_num;
    }
    // [RE-FALLBACK: the entry edge] the plane enters from the map edge nearest
    // the drop site along x and leaves across the map on the same heading.
    const bool from_left = cx < grid_w_ / 2;
    const int sx = from_left ? 0 : grid_w_ - 1;
    const int ex = from_left ? grid_w_ - 1 : 0;
    const int pi = an_spawn("PDPLANE", house, sx, cy);
    if (pi < 0) {
        // No plane art/type: drop straight onto the site.
        for (int k = 0; k < num; ++k) {
            bool placed = false;
            for (int r = 0; r <= 4 && !placed; ++r)
                for (int oy = -r; oy <= r && !placed; ++oy)
                    for (int ox = -r; ox <= r && !placed; ++ox) {
                        if (std::max(std::abs(ox), std::abs(oy)) != r) continue;
                        if (!walkable(cx + ox, cy + oy) || !occ_.is_free(cx + ox, cy + oy)) continue;
                        placed = an_spawn(inf, house, cx + ox, cy + oy) >= 0;
                    }
        }
        return;
    }
    Unit& p = units_[pi];
    p.fx = sx;
    p.fy = cy;
    p.altitude = an_.sw.flight_level;
    p.flight_state = FlightState::Cruising;
    p.an.sortie = kParadrop;
    p.an.drop_left = num;
    p.an.drop_type = inf;
    p.an.drop_x = cx;
    p.an.drop_y = cy;
    p.an.exit_x = ex;
    p.an.exit_y = cy;
    if (!an_.types.count(inf)) {
        // the infantry type must be registered for an_spawn
        p.an.drop_left = 0;
    }
}

// ------------------------------------------------- W4-SQ-06 team paradrop --

int World::spawn_drop_plane(const std::string& plane_id, int house, int sx, int sy, int dx, int dy,
                            int ex, int ey) {
    const int pi = an_spawn(plane_id, house, sx, sy);
    if (pi < 0) return -1;
    Unit& p = units_[pi];
    p.fx = p.cell_x;
    p.fy = p.cell_y;
    p.altitude = an_.sw.flight_level;
    p.flight_state = FlightState::Cruising;
    p.an.sortie = kParadrop;
    p.an.drop_left = 0;
    p.an.drop_type.clear();
    p.an.drop_x = std::max(0, std::min(grid_w_ - 1, dx));
    p.an.drop_y = std::max(0, std::min(grid_h_ - 1, dy));
    p.an.exit_x = std::max(0, std::min(grid_w_ - 1, ex));
    p.an.exit_y = std::max(0, std::min(grid_h_ - 1, ey));
    const int f = facing_from_delta(dx - sx, dy - sy);
    if (f >= 0) p.facing = f;
    p.body_facing = facing_u16_from_delta(dx - sx, dy - sy);
    return pi;
}

// W4-SQ-03 [RE: AircraftClass mission FUN_004176F0 -- an AirportBound
// aircraft that finds no free dock (vtbl +0x528) takes the no-dock exit
// (vtbl +0x3DC), and an emptied team transport plane with no team is put on
// Mission Retreat (4); RE-FALLBACK: both leave over the nearest map edge]:
// a fixed-wing aircraft with nowhere to land flies off the map.
bool World::order_leave_map(int idx) {
    if (idx < 0 || idx >= static_cast<int>(units_.size())) return false;
    Unit& u = units_[idx];
    if (!u.alive || u.die_time >= 0.0 || !u.is_aircraft || !u.an.t.fixed_wing || u.an.t.spawned) return false;
    const int W = grid_w_, H = grid_h_;
    const int dl = u.cell_x, dr = W - 1 - u.cell_x, dt = u.cell_y, db = H - 1 - u.cell_y;
    int ex = u.cell_x, ey = u.cell_y;
    const int m = std::min(std::min(dl, dr), std::min(dt, db));
    if (m == dl) ex = 0; else if (m == dr) ex = W - 1; else if (m == dt) ey = 0; else ey = H - 1;
    an_.any = true;
    u.target = -1;
    u.target_building = -1;
    u.attack_order = false;
    u.path.clear();
    u.path_idx = 0;
    u.has_flight_goal = false;
    u.an.attack_unit = u.an.attack_building = -1;
    u.an.sortie = kParadrop;   // the straight run off the map (no cargo, nothing to drop)
    u.an.drop_left = 0;
    u.an.drop_type.clear();
    u.an.drop_x = u.cell_x;
    u.an.drop_y = u.cell_y;
    u.an.exit_x = ex;
    u.an.exit_y = ey;
    if (u.flight_state == FlightState::Grounded) {
        occ_.release_all_of(idx, -1, -1);
        u.flight_state = FlightState::TakingOff;
    }
    return true;
}

// One carried member jumps: it leaves the plane over the nearest free land
// cell around the drop site and falls under its parachute from the plane's
// height [RE-FALLBACK: the per-jump cadence and the landing-cell scan of the
// ParadropApproach mission were not traced; one jump per frame onto the
// first free cell of the rings around the target, as the SW plane does].
bool World::an_drop_passenger(int i) {
    Unit& pl = units_[i];
    if (pl.cargo.empty()) return false;
    for (int r = 0; r <= 6; ++r)
        for (int oy = -r; oy <= r; ++oy)
            for (int ox = -r; ox <= r; ++ox) {
                if (std::max(std::abs(ox), std::abs(oy)) != r) continue;
                const int cx = pl.an.drop_x + ox, cy = pl.an.drop_y + oy;
                if (!walkable(cx, cy) || !occ_.is_free(cx, cy) || building_at_cell(cx, cy) >= 0) continue;
                const int p = pl.cargo.front();
                pl.cargo.erase(pl.cargo.begin());
                if (p < 0 || p >= static_cast<int>(units_.size())) return true;
                Unit& u = units_[p];
                u.cargo_of = kParachuteCargo;
                u.cell_x = cx;
                u.cell_y = cy;
                u.fx = cx;
                u.fy = cy;
                u.guard_x = cx;
                u.guard_y = cy;
                u.an.para_alt = std::max(1.0, pl.altitude);
                if (const ra2assets::MapCell* mc = map_->at(cx, cy)) u.level = mc->level;
                occ_.set(cx, cy, p);   // the landing cell is taken from the jump on
                u.ia.seq = -1;         // W3-B: the machine restarts on landing
                an_event(SwEvent::Dropped, pl.house, SuperweaponType::ParaDrop, cx, cy);
                return true;
            }
    return false;
}

void World::an_step_parachutes(double dt) {
    const double fall = std::max(1, an_.sw.parachute_fall) * 15.0 * dt;
    for (int i = 0; i < static_cast<int>(units_.size()); ++i) {
        Unit& u = units_[i];
        if (u.cargo_of != kParachuteCargo) continue;
        if (!u.alive) { u.cargo_of = -1; u.an.para_alt = 0.0; continue; }
        u.an.para_alt -= fall;
        // The art Paradrop= sequence while it hangs under the chute (W3-B
        // tables; the stage cycles at the DoControls rate).
        if (const InfTable* it = inf_table(u.image)) {
            const InfSeqEntry& e = it->e[kSqParadrop];
            if (e.has) {
                const int n = std::max(1, e.count);
                const int stage = om::ftol((u.an.para_alt / std::max(1.0, fall)) / 2.0) % n;
                u.frame = e.first + (e.fmult ? ((u.facing % 8 + 8) % 8) * e.fmult : 0) + (n - 1 - stage);
            }
        }
        if (u.an.para_alt > 0.0) continue;
        u.an.para_alt = 0.0;
        u.cargo_of = -1;   // touched down: an ordinary unit on its cell
        if (occ_.at(u.cell_x, u.cell_y) != i) {
            if (occ_.is_free(u.cell_x, u.cell_y)) occ_.set(u.cell_x, u.cell_y, i);
            else {
                const std::vector<Cellxy> g = nearest_free_cells({u.cell_x, u.cell_y}, 1, passable_, grid_w_,
                                                                 grid_h_, occ_);
                if (!g.empty()) {
                    u.cell_x = g[0].x;
                    u.cell_y = g[0].y;
                    u.fx = u.cell_x;
                    u.fy = u.cell_y;
                    occ_.set(u.cell_x, u.cell_y, i);
                }
            }
        }
        u.guard_x = u.cell_x;
        u.guard_y = u.cell_y;
    }
}

// ---------------------------------------------------------- AI naval yard --

bool World::ai_naval_spot_ok(int house, const HouseAiRec& rec, const BuildingTypeDef& t, int cx,
                             int cy) const {
    if (water_passable_.empty()) return false;
    for (const Cellxy& c : foundation_cells(cx, cy, t.fw, t.fh)) {
        if (c.x < 0 || c.y < 0 || c.x >= grid_w_ || c.y >= grid_h_) return false;
        if (!water_passable_[static_cast<std::size_t>(c.y) * grid_w_ + c.x]) return false;
        if (building_at_cell(c.x, c.y) >= 0 || !occ_.is_free(c.x, c.y)) return false;
    }
    const int adj = rec.cfg.catalog.naval_yard_adjacency;
    for (const Building& b : buildings_) {
        if (!b.alive || b.house != house || b.state == Building::State::Dead) continue;
        if (b.type < 0 || b.type >= static_cast<int>(rec.cfg.roster.size()) ||
            !rec.cfg.roster[b.type].conyard)
            continue;
        if (std::max(std::abs(cx - b.cx), std::abs(cy - b.cy)) <= adj) return true;
    }
    return false;
}

Cellxy World::ai_naval_spot_scan(int house, const HouseAiRec& rec, const BuildingTypeDef& t) const {
    int ax = -1, ay = -1;
    for (const Building& b : buildings_)
        if (b.alive && b.house == house && b.state != Building::State::Dead && b.type >= 0 &&
            b.type < static_cast<int>(rec.cfg.roster.size()) && rec.cfg.roster[b.type].conyard) {
            ax = b.cx;
            ay = b.cy;
            break;
        }
    if (ax < 0) return Cellxy{-1, -1};
    const int adj = rec.cfg.catalog.naval_yard_adjacency;
    for (int r = 1; r <= adj; ++r)   // nearest ring first, deterministic order
        for (int y = ay - r; y <= ay + r; ++y)
            for (int x = ax - r; x <= ax + r; ++x) {
                if (std::max(std::abs(x - ax), std::abs(y - ay)) != r) continue;
                if (ai_naval_spot_ok(house, rec, t, x, y)) return Cellxy{x, y};
            }
    return Cellxy{-1, -1};
}

// ------------------------------------------------------------------ driver --

void World::step_air_naval(double dt) {
    if (!an_.any) {
        // Open the firewall lazily: any unit carrying W2-AN type data.
        for (const Unit& u : units_)
            if (u.an.t.underwater || u.an.t.sensors || u.an.t.spawner_slot >= 0 || u.an.t.fixed_wing ||
                u.an.t.crashable || (u.is_naval && u.an.t.weight > 1.0) || u.an.iron_left_frames > 0.0 ||
                u.movement_domain == MovementDomain::Amphibious) {
                an_.any = true;
                break;
            }
        if (!an_.any) return;
    }
    ++an_.frame;
    an_step_naval(dt);
    an_step_spawners(dt);
    an_step_fixed_wing(dt);
    an_step_parachutes(dt);   // W4-SQ-06
    an_step_deaths(dt);
    // Night Hawk (W2-AN-08) [RE: aircraft.md §2, BalloonHover +0xD6A]: a
    // non-BalloonHover Landable jumpjet with no goal and no target lands.
    for (int i = 0; i < static_cast<int>(units_.size()); ++i) {
        Unit& u = units_[i];
        if (!u.is_aircraft || u.an.t.fixed_wing || u.balloon_hover || !u.an.t.landable) continue;
        if (!live(u)) continue;
        if (u.flight_state == FlightState::Cruising && !u.has_flight_goal && u.target < 0 &&
            u.target_building < 0) {
            auto pad_ok = [&](int x, int y) {
                return walkable(x, y) && occ_.is_free(x, y) && building_at_cell(x, y) < 0;
            };
            if (pad_ok(u.cell_x, u.cell_y)) {
                u.flight_state = FlightState::Descending;
                u.an.landing = true;
                aircraft_any_ = true;
            } else {
                // Blocked landing spot: fly to the nearest free ground cell.
                bool found = false;
                for (int r = 1; r <= 4 && !found; ++r)
                    for (int oy = -r; oy <= r && !found; ++oy)
                        for (int ox = -r; ox <= r && !found; ++ox) {
                            if (std::max(std::abs(ox), std::abs(oy)) != r) continue;
                            if (!pad_ok(u.cell_x + ox, u.cell_y + oy)) continue;
                            u.flight_goal_x = u.cell_x + ox;
                            u.flight_goal_y = u.cell_y + oy;
                            u.has_flight_goal = true;
                            u.an.landing = true;   // a landing redirect (kept by step_cargo)
                            aircraft_any_ = true;
                            found = true;
                        }
            }
        }
        if (u.an.landing && u.flight_state == FlightState::Grounded) {
            u.an.landing = false;
            u.fx = u.cell_x;
            u.fy = u.cell_y;
            if (occ_.is_free(u.cell_x, u.cell_y)) occ_.set(u.cell_x, u.cell_y, i);
        }
    }
}

}  // namespace ra2engine
