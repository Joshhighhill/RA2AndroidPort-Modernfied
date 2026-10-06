#pragma once
// P7 Save / Load: THE field lists. One visitor per struct, one line per field,
// shared by save (Writer), load (Reader) and the coverage tripwire (Counter).
//
// ADDING STATE (P4/P5/P6/P10 and later):
//   1. add `io(ar, x.your_field);` to the struct's visitor below (or to
//      SaveAccess::world for a new World member),
//   2. bump save::kFormatVersion in savegame.hpp,
//   3. if you added a NEW struct type, add a `fields` overload for it and a
//      CHECK_ARITY line in tests/test_saveload.cpp.
// tests/test_saveload.cpp FAILS (naming the struct) when a struct gains a
// member that its visitor does not visit.

#include "map_trigger.hpp"
#include "savegame.hpp"
#include "skirmish.hpp"
#include "world.hpp"

namespace ra2engine {

// The single friend every class with private save-state names. Static members
// only; visitors for private members and private nested types live here.
struct SaveAccess {
    template <class Ar> static void occupancy(Ar& ar, Occupancy& o) {
        save::io(ar, o.w_);
        save::io(ar, o.h_);
        save::io(ar, o.owner_);
    }
    template <class Ar> static void ore_field(Ar& ar, OreField& f) {
        save::io(ar, f.w_);
        save::io(ar, f.h_);
        save::io(ar, f.kind_);
        save::io(ar, f.density_);
        save::io(ar, f.variant_);
        save::io(ar, f.timer_);
    }
    template <class Ar> static void queues(Ar& ar, ProductionQueues& q) { save::io(ar, q.q_); }
    template <class Ar> static void game_random(Ar& ar, GameRandom& g) {   // P5 crate RNG
        save::io(ar, g.table_);
        save::io(ar, g.idx1_);
        save::io(ar, g.idx2_);
    }

    // --- World private nested types ---
    template <class Ar> static void house_ai(Ar& ar, World::HouseAiRec& r) {
        save::io(ar, r.is_ai);
        save::io(ar, r.cfg);
        save::io(ar, r.state);
        save::io(ar, r.queues);
        save::io(ar, r.credits);
        save::io(ar, r.team_state);
        save::io(ar, r.trigger_state);
    }
    template <class Ar> static void house_sw(Ar& ar, World::HouseSuperweapon& s) {
        save::io(ar, s.has_sw);
        save::io(ar, s.sw);
        save::io(ar, s.eff_damage);
        save::io(ar, s.eff_cell_spread);
        save::io(ar, s.eff_percent_at_max);
        save::io(ar, s.eff_verses);
    }
    template <class Ar> static void tech_income(Ar& ar, World::TechIncome& t) {
        save::io(ar, t.building);
        save::io(ar, t.amount);
        save::io(ar, t.delay);
        save::io(ar, t.frames);
    }
    template <class Ar> static void harvest_event(Ar& ar, World::PendingHarvestEvent& e) {
        save::io(ar, e.kind);
        save::io(ar, e.unit);
    }
    using HouseAiRec = World::HouseAiRec;
    using HouseSuperweapon = World::HouseSuperweapon;
    using TechIncome = World::TechIncome;
    using PendingHarvestEvent = World::PendingHarvestEvent;

    // --- World: every member except map_ (the caller-owned MapFile) ---
    template <class Ar> static void world(Ar& ar, World& w) {
        save::io(ar, w.ally_reveal_);
        // Projectile phase (P2)
        save::io(ar, w.projectiles_enabled_);
        save::io(ar, w.proj_rules_);
        save::io(ar, w.proj_rng_);
        save::io(ar, w.projectiles_);
        save::io(ar, w.proj_events_);
        save::io(ar, w.proj_accum_);
        save::io(ar, w.proj_frame_);
        // AI-1/2/3
        save::io(ar, w.ai_houses_);
        save::io(ar, w.ai_any_);
        // CB-4 superweapons
        save::io(ar, w.sw_houses_);
        save::io(ar, w.sw_any_);
        // Capture
        save::io(ar, w.capture_any_);
        save::io(ar, w.tech_income_);
        save::io(ar, w.blackout_frames_);
        save::io(ar, w.produced_veteran_);
        // Shroud
        save::io(ar, w.explored_);
        save::io(ar, w.visible_);
        save::io(ar, w.shroud_any_);
        save::io(ar, w.map_reveal_);
        // Multi-house / victory
        save::io(ar, w.houses_);
        save::io(ar, w.houses_configured_);
        save::io(ar, w.victory_enabled_);
        save::io(ar, w.short_game_);
        save::io(ar, w.house_defeated_);
        save::io(ar, w.winning_team_);
        // Cargo / flight
        save::io(ar, w.cargo_any_);
        save::io(ar, w.cargo_events_);
        save::io(ar, w.aircraft_any_);
        // Core
        save::io(ar, w.units_);
        save::io(ar, w.buildings_);
        save::io(ar, w.passable_);
        save::io(ar, w.water_passable_);
        save::io(ar, w.naval_any_);
        save::io(ar, w.occ_);
        save::io(ar, w.grid_w_);
        save::io(ar, w.grid_h_);
        save::io(ar, w.move_cps_);
        save::io(ar, w.anims_);
        // Economy
        save::io(ar, w.ore_);
        save::io(ar, w.ore_rng_);
        save::io(ar, w.types_);
        save::io(ar, w.refinery_types_);
        save::io(ar, w.lookup_types_);
        save::io(ar, w.ore_value_ore_);
        save::io(ar, w.ore_value_gems_);
        save::io(ar, w.growth_interval_);
        save::io(ar, w.growth_pct_);
        save::io(ar, w.spread_pct_);
        save::io(ar, w.house_credits_);
        save::io(ar, w.harvest_events_);
        // Repair & Sell
        save::io(ar, w.repair_rules_);
        save::io(ar, w.repair_sell_any_);
        save::io(ar, w.repair_frame_);
        save::io(ar, w.survivor_crew_);
        save::io(ar, w.survivor_crew_set_);
        // P5 Veterancy + crates
        save::io(ar, w.vet_any_);
        save::io(ar, w.vet_rules_);
        save::io(ar, w.vet_building_costs_);
        save::io(ar, w.vet_frame_);
        save::io(ar, w.vet_events_);
        save::io(ar, w.crates_any_);
        save::io(ar, w.crate_rules_);
        save::io(ar, w.crate_rng_);
        save::io(ar, w.crate_pool_);
        save::io(ar, w.crates_);
        save::io(ar, w.crate_events_);
    }

    // --- MapTriggerSystem: every member except d_ (the parsed map data, a
    // reference re-derived from the map on load) ---
    using Trig = MapTriggerSystem::Trig;
    using Tag = MapTriggerSystem::Tag;
    template <class Ar> static void trig(Ar& ar, Trig& t) {
        save::io(ar, t.type);
        save::io(ar, t.house_override);
        save::io(ar, t.destroyed);
        save::io(ar, t.t_start);
        save::io(ar, t.t_left);
        save::io(ar, t.bits);
        save::io(ar, t.enabled);
    }
    template <class Ar> static void tag(Ar& ar, Tag& t) {
        save::io(ar, t.type);
        save::io(ar, t.trigs);
        save::io(ar, t.instance_count);
        save::io(ar, t.destroyed);
        save::io(ar, t.executing);
        save::io(ar, t.pending_delete);
    }
    template <class Ar> static void triggers(Ar& ar, MapTriggerSystem& s) {
        save::io(ar, s.difficulty_);
        save::io(ar, s.rng_);
        save::io(ar, s.frame_);
        save::io(ar, s.globals_);
        save::io(ar, s.locals_);
        save::io(ar, s.vars_changed_);
        save::io(ar, s.mt_start_);
        save::io(ar, s.mt_left_);
        save::io(ar, s.outcome_);
        save::io(ar, s.tags_);
        save::io(ar, s.tag_of_type_);
        save::io(ar, s.map_list_);
        save::io(ar, s.logic_list_);
        save::io(ar, s.house_lists_);
        save::io(ar, s.ev_house_);
        save::io(ar, s.ev_base_);
        save::io(ar, s.house_flags_);
        save::io(ar, s.log_);
        save::io(ar, s.fired_);
        save::io(ar, s.logic_fired_);
        save::io(ar, s.house_fired_);
        save::io(ar, s.variable_fired_);
        save::io(ar, s.raise_depth_kind_);
        save::io(ar, s.force_depth_);
        save::io(ar, s.cur_trigger_);
    }
    static std::size_t trig_arity() { return save::aggregate_arity<Trig>(); }
    static std::size_t tag_arity() { return save::aggregate_arity<Tag>(); }
    static std::size_t house_ai_arity() { return save::aggregate_arity<HouseAiRec>(); }
    static std::size_t house_sw_arity() { return save::aggregate_arity<HouseSuperweapon>(); }
    static std::size_t tech_income_arity() { return save::aggregate_arity<TechIncome>(); }
    static std::size_t harvest_event_arity() { return save::aggregate_arity<PendingHarvestEvent>(); }
};

namespace save {

// Private-state forwarding (ADL on Ar finds these).
template <class Ar> void fields(Ar& ar, Occupancy& v) { SaveAccess::occupancy(ar, v); }
template <class Ar> void fields(Ar& ar, OreField& v) { SaveAccess::ore_field(ar, v); }
template <class Ar> void fields(Ar& ar, ProductionQueues& v) { SaveAccess::queues(ar, v); }
template <class Ar> void fields(Ar& ar, GameRandom& v) { SaveAccess::game_random(ar, v); }
template <class Ar> void fields(Ar& ar, SaveAccess::HouseAiRec& v) { SaveAccess::house_ai(ar, v); }
template <class Ar> void fields(Ar& ar, SaveAccess::HouseSuperweapon& v) { SaveAccess::house_sw(ar, v); }
template <class Ar> void fields(Ar& ar, SaveAccess::TechIncome& v) { SaveAccess::tech_income(ar, v); }
template <class Ar> void fields(Ar& ar, SaveAccess::PendingHarvestEvent& v) { SaveAccess::harvest_event(ar, v); }
template <class Ar> void fields(Ar& ar, SaveAccess::Trig& v) { SaveAccess::trig(ar, v); }
template <class Ar> void fields(Ar& ar, SaveAccess::Tag& v) { SaveAccess::tag(ar, v); }

// --- small leaf structs ---
template <class Ar> void fields(Ar& ar, Cellxy& v) { io(ar, v.x); io(ar, v.y); }
template <class Ar> void fields(Ar& ar, SeqEntry& v) { io(ar, v.first); io(ar, v.count); io(ar, v.fmult); }
template <class Ar> void fields(Ar& ar, UnitAnimSeq& v) {
    io(ar, v.stand); io(ar, v.walk); io(ar, v.fire); io(ar, v.die);
}
template <class Ar> void fields(Ar& ar, OreRng& v) { io(ar, v.s); }
template <class Ar> void fields(Ar& ar, ProjRng& v) { io(ar, v.s); }
template <class Ar> void fields(Ar& ar, ProjectileRules& v) { io(ar, v.gravity); io(ar, v.ballistic_scatter); }
template <class Ar> void fields(Ar& ar, Superweapon& v) {
    io(ar, v.type); io(ar, v.recharge_secs); io(ar, v.timer); io(ar, v.ready);
}
template <class Ar> void fields(Ar& ar, RepairSellRules& v) {
    io(ar, v.repair_step);
    io(ar, v.repair_percent);
    io(ar, v.repair_rate);
    io(ar, v.refund_percent);
    io(ar, v.allied_survivor_divisor);
    io(ar, v.soviet_survivor_divisor);
    io(ar, v.third_survivor_divisor);
}
template <class Ar> void fields(Ar& ar, World::HouseRec& v) {
    io(ar, v.side); io(ar, v.color_ix); io(ar, v.country); io(ar, v.alliance_team); io(ar, v.combatant);
}
template <class Ar> void fields(Ar& ar, CargoEvent& v) {
    io(ar, v.kind); io(ar, v.unit); io(ar, v.transport); io(ar, v.x); io(ar, v.y);
}

// --- projectiles ---
template <class Ar> void fields(Ar& ar, ProjectileSpec& v) {
    io(ar, v.kind);
    io(ar, v.arcing);
    io(ar, v.inaccurate);
    io(ar, v.flak_scatter);
    io(ar, v.inviso);
    io(ar, v.floater);
    io(ar, v.rot);
    io(ar, v.speed_cap);
    io(ar, v.acceleration);
    io(ar, v.detonation_altitude);
    io(ar, v.weapon_range);
    io(ar, v.flh);
}
template <class Ar> void fields(Ar& ar, Projectile& v) {
    io(ar, v.kind);
    io(ar, v.attacker);
    io(ar, v.target_unit);
    io(ar, v.target_building);
    io(ar, v.x); io(ar, v.y); io(ar, v.z);
    io(ar, v.vx); io(ar, v.vy); io(ar, v.vz);
    io(ar, v.tx); io(ar, v.ty); io(ar, v.tz);
    io(ar, v.speed_cap);
    io(ar, v.acceleration);
    io(ar, v.detonation_altitude);
    io(ar, v.inaccurate);
    io(ar, v.floater);
    io(ar, v.slow);
    io(ar, v.launch_frame);
    io(ar, v.launch_speed);
    io(ar, v.max_z);
}
template <class Ar> void fields(Ar& ar, ProjectileEvent& v) {
    io(ar, v.kind);
    io(ar, v.frame);
    io(ar, v.attacker);
    io(ar, v.x); io(ar, v.y); io(ar, v.z);
    io(ar, v.speed);
    io(ar, v.scatter_dx); io(ar, v.scatter_dy);
    io(ar, v.victim);
    io(ar, v.building);
    io(ar, v.damage);
    io(ar, v.flight_frames);
    io(ar, v.apex);
}

// --- P5 veterancy + crates ---
template <class Ar> void fields(Ar& ar, VeterancyRules& v) {
    io(ar, v.ratio);
    io(ar, v.combat);
    io(ar, v.speed);
    io(ar, v.sight);
    io(ar, v.armor);
    io(ar, v.rof);
    io(ar, v.cap);
    io(ar, v.repair_rate);
    io(ar, v.elite_flash);
}
template <class Ar> void fields(Ar& ar, WeaponStats& v) {
    io(ar, v.damage); io(ar, v.rof); io(ar, v.range); io(ar, v.verses);
    io(ar, v.cell_spread); io(ar, v.percent_at_max);
}
template <class Ar> void fields(Ar& ar, UnitVet& v) {
    io(ar, v.veterancy);
    io(ar, v.last_rank);
    io(ar, v.elite_flash);
    io(ar, v.firepower_mult);
    io(ar, v.armor_mult);
    io(ar, v.speed_mult);
    io(ar, v.recv);
    io(ar, v.vet_armor);
    io(ar, v.killed_by);
    io(ar, v.credited);
    io(ar, v.cost);
    io(ar, v.trainable);
    io(ar, v.dont_score);
    io(ar, v.self_healing);
    io(ar, v.vet_abilities);
    io(ar, v.elite_abilities);
    io(ar, v.rules_speed);
    io(ar, v.base);
    io(ar, v.has_elite);
    io(ar, v.elite);
    io(ar, v.base_speed_cps);
    io(ar, v.base_sight);
}
template <class Ar> void fields(Ar& ar, PowerupDef& v) {
    io(ar, v.weight); io(ar, v.anim); io(ar, v.naval); io(ar, v.arg);
}
template <class Ar> void fields(Ar& ar, CrateRules& v) {
    io(ar, v.minimum); io(ar, v.maximum); io(ar, v.radius_leptons); io(ar, v.regen); io(ar, v.powerups);
}
template <class Ar> void fields(Ar& ar, World::Crate& v) {
    io(ar, v.x); io(ar, v.y); io(ar, v.powerup); io(ar, v.start); io(ar, v.lifetime);
}
template <class Ar> void fields(Ar& ar, World::CrateEvent& v) {
    io(ar, v.picked); io(ar, v.applied); io(ar, v.house); io(ar, v.unit);
    io(ar, v.x); io(ar, v.y); io(ar, v.amount); io(ar, v.affected);
}
template <class Ar> void fields(Ar& ar, World::VetEvent& v) {
    io(ar, v.unit); io(ar, v.old_rank); io(ar, v.new_rank);
}

// --- unit / building types ---
template <class Ar> void fields(Ar& ar, UnitCombat& v) {
    io(ar, v.strength);
    io(ar, v.armor);
    io(ar, v.image);
    io(ar, v.damage);
    io(ar, v.rof);
    io(ar, v.range);
    io(ar, v.verses);
    io(ar, v.cell_spread);
    io(ar, v.percent_at_max);
    io(ar, v.proj);
    io(ar, v.rot);
    io(ar, v.turret);
    io(ar, v.speed_cps);
    io(ar, v.is_vehicle);
    io(ar, v.passengers);
    io(ar, v.size_limit);
    io(ar, v.size);
    io(ar, v.is_aircraft);
    io(ar, v.cruise_altitude);
    io(ar, v.climb_rate);
    io(ar, v.descend_rate);
    io(ar, v.flight_speed);
    io(ar, v.flight_turn);
    io(ar, v.balloon_hover);
    io(ar, v.omni_fire);
    io(ar, v.is_naval);
    io(ar, v.movement_domain);
    io(ar, v.veteran_rank);
    io(ar, v.sight);
    // P5
    io(ar, v.cost);
    io(ar, v.trainable);
    io(ar, v.dont_score);
    io(ar, v.self_healing);
    io(ar, v.crate_goodie);
    io(ar, v.vet_abilities);
    io(ar, v.elite_abilities);
    io(ar, v.rules_speed);
    io(ar, v.has_elite);
    io(ar, v.elite);
}
template <class Ar> void fields(Ar& ar, UnitTypeDef& v) {
    io(ar, v.id);
    io(ar, v.cost);
    io(ar, v.tech_level);
    io(ar, v.is_vehicle);
    io(ar, v.chrono_miner);
    io(ar, v.prerequisite);
    io(ar, v.cameo);
    io(ar, v.combat);
}
template <class Ar> void fields(Ar& ar, BuildingTypeDef& v) {
    io(ar, v.id);
    io(ar, v.fw); io(ar, v.fh);
    io(ar, v.strength);
    io(ar, v.armor_ix);
    io(ar, v.cost);
    io(ar, v.charge);
    io(ar, v.power);
    io(ar, v.adjacent);
    io(ar, v.tech_level);
    io(ar, v.conyard);
    io(ar, v.prerequisite);
    io(ar, v.image);
    io(ar, v.buildup);
    io(ar, v.cameo);
    io(ar, v.explosion);
    io(ar, v.produce_cash_startup);
    io(ar, v.produce_cash_amount);
    io(ar, v.produce_cash_delay);
    io(ar, v.sight);
    io(ar, v.grants_map_reveal);
    io(ar, v.unsellable);
    io(ar, v.click_repairable);
    io(ar, v.crewed);
    io(ar, v.soylent);
}

// --- the live objects ---
template <class Ar> void fields(Ar& ar, Unit& v) {
    io(ar, v.cell_x); io(ar, v.cell_y); io(ar, v.level); io(ar, v.frame); io(ar, v.image);
    io(ar, v.selected);
    io(ar, v.path);
    io(ar, v.path_idx);
    io(ar, v.fx); io(ar, v.fy);
    io(ar, v.anim_time);
    io(ar, v.wait_time);
    io(ar, v.facing);
    // combat
    io(ar, v.house);
    io(ar, v.hp); io(ar, v.max_hp);
    io(ar, v.armor_ix);
    io(ar, v.w_damage);
    io(ar, v.w_rof);
    io(ar, v.w_range);
    io(ar, v.w_verses);
    io(ar, v.w_cell_spread);
    io(ar, v.w_percent_at_max);
    io(ar, v.w_proj);
    io(ar, v.target);
    io(ar, v.target_building);
    io(ar, v.attack_order);
    io(ar, v.rof_cool);
    io(ar, v.fire_flash);
    io(ar, v.guard_x); io(ar, v.guard_y);
    io(ar, v.alive);
    io(ar, v.die_time);
    // vehicles
    io(ar, v.is_vehicle);
    io(ar, v.body_facing);
    io(ar, v.turret_facing);
    io(ar, v.rot_u16_per_sec);
    io(ar, v.speed_cps);
    // economy
    io(ar, v.is_harvester);
    io(ar, v.chrono_miner);
    io(ar, v.storage_bales);
    io(ar, v.bales_ore); io(ar, v.bales_gems);
    io(ar, v.harv);
    io(ar, v.harv_cell_x); io(ar, v.harv_cell_y);
    io(ar, v.has_harv_cell);
    io(ar, v.dock_refinery);
    io(ar, v.harv_timer);
    io(ar, v.manual_hold);
    // cargo
    io(ar, v.passengers_max);
    io(ar, v.size_limit);
    io(ar, v.size);
    io(ar, v.cargo);
    io(ar, v.cargo_of);
    io(ar, v.enter_target);
    io(ar, v.unload_pending);
    io(ar, v.eject_counter);
    // aircraft
    io(ar, v.is_aircraft);
    io(ar, v.altitude);
    io(ar, v.flight_state);
    io(ar, v.cruise_altitude);
    io(ar, v.climb_rate);
    io(ar, v.descend_rate);
    io(ar, v.flight_speed);
    io(ar, v.flight_turn);
    io(ar, v.balloon_hover);
    io(ar, v.omni_fire);
    io(ar, v.has_flight_goal);
    io(ar, v.flight_goal_x); io(ar, v.flight_goal_y);
    // naval
    io(ar, v.is_naval);
    io(ar, v.movement_domain);
    // capture / spy
    io(ar, v.capture_target);
    io(ar, v.infiltrate_target);
    io(ar, v.infiltrate_purpose);
    io(ar, v.infiltrate_steal_pct);
    io(ar, v.infiltrate_blackout);
    io(ar, v.veteran_rank);
    // shroud
    io(ar, v.sight);
    io(ar, v.seen_by_mask);
    // disguise
    io(ar, v.disguised);
    io(ar, v.disguised_as_type);
    io(ar, v.disguised_as_house);
    io(ar, v.detect_disguise);
    io(ar, v.vet);   // P5
}
template <class Ar> void fields(Ar& ar, Building& v) {
    io(ar, v.type);
    io(ar, v.cx); io(ar, v.cy);
    io(ar, v.fw); io(ar, v.fh);
    io(ar, v.house);
    io(ar, v.hp); io(ar, v.max_hp);
    io(ar, v.armor_ix);
    io(ar, v.sight);
    io(ar, v.grants_map_reveal);
    io(ar, v.state);
    io(ar, v.buildup_time);
    io(ar, v.buildup_total);
    io(ar, v.die_time);
    io(ar, v.alive);
    io(ar, v.blocking_released);
    io(ar, v.primary);
    io(ar, v.rally_x); io(ar, v.rally_y);
    io(ar, v.has_rally);
    io(ar, v.grants_superweapon);
    io(ar, v.being_repaired);
    io(ar, v.selling);
    io(ar, v.sell_time);
    io(ar, v.sold);
    // P5
    io(ar, v.vet_recv);
    io(ar, v.armor_mult);
    io(ar, v.killed_by);
    io(ar, v.vet_credited);
}

// --- production ---
template <class Ar> void fields(Ar& ar, QueueSlot& v) {
    io(ar, v.type);
    io(ar, v.stage);
    io(ar, v.step_ticks);
    io(ar, v.frames_accum);
    io(ar, v.total_frames);
    io(ar, v.spent);
    io(ar, v.ready);
    io(ar, v.charge_basis);
}
template <class Ar> void fields(Ar& ar, ProductionState& v) { io(ar, v.credits); io(ar, v.slot); }

// --- AI-1 ---
template <class Ar> void fields(Ar& ar, DifficultyMods& v) {
    io(ar, v.cost_mul); io(ar, v.build_time_mul); io(ar, v.build_delay_min);
    io(ar, v.build_slowdown); io(ar, v.defense_target);
}
template <class Ar> void fields(Ar& ar, AiRules& v) {
    io(ar, v.power_surplus);
    io(ar, v.power_emergency);
    io(ar, v.base_size_add);
    io(ar, v.base_spacing);
    io(ar, v.refinery_ratio);
    io(ar, v.refinery_limit);
    io(ar, v.war_ratio);
    io(ar, v.war_limit);
    io(ar, v.defense_ratio);
    io(ar, v.defense_limit);
    io(ar, v.credit_reserve);
}
template <class Ar> void fields(Ar& ar, PlannedNode& v) {
    io(ar, v.id); io(ar, v.type); io(ar, v.cx); io(ar, v.cy); io(ar, v.building); io(ar, v.dead);
}
template <class Ar> void fields(Ar& ar, AiBaseState& v) {
    io(ar, v.difficulty);
    io(ar, v.build_clock);
    io(ar, v.build_gate);
    io(ar, v.plan);
    io(ar, v.rng);
    io(ar, v.pending);
    io(ar, v.next_node_id);
}
template <class Ar> void fields(Ar& ar, AiCatalog& v) {
    io(ar, v.conyard); io(ar, v.power); io(ar, v.refinery); io(ar, v.barracks);
    io(ar, v.war_factory); io(ar, v.radar); io(ar, v.defenses); io(ar, v.wall);
}
// --- AI-2 ---
template <class Ar> void fields(Ar& ar, TeamInstance& v) {
    io(ar, v.teamtype);
    io(ar, v.state);
    io(ar, v.members);
    io(ar, v.member_types);
    io(ar, v.script_pc);
    io(ar, v.script_timer);
    io(ar, v.recruit_timer);
    io(ar, v.gather_x); io(ar, v.gather_y);
    io(ar, v.move_issued);
    io(ar, v.stall_reissues);
    io(ar, v.completed_success);
}
template <class Ar> void fields(Ar& ar, AiTeamState& v) {
    io(ar, v.teams); io(ar, v.check_clock); io(ar, v.spawn_clock); io(ar, v.rng); io(ar, v.teams_created);
}
template <class Ar> void fields(Ar& ar, AiTeamRules& v) {
    io(ar, v.team_delay_frames);
    io(ar, v.autocreate_time_min);
    io(ar, v.total_team_cap);
    io(ar, v.dissolve_frames);
    io(ar, v.multiplay);
    io(ar, v.credit_reserve);
    io(ar, v.infantry_reserve);
    io(ar, v.build_slowdown);
}
// --- aimd ---
template <class Ar> void fields(Ar& ar, TaskForceMember& v) { io(ar, v.count); io(ar, v.unit); }
template <class Ar> void fields(Ar& ar, TaskForce& v) {
    io(ar, v.id); io(ar, v.name); io(ar, v.group); io(ar, v.members);
}
template <class Ar> void fields(Ar& ar, ScriptAction& v) { io(ar, v.action); io(ar, v.argument); }
template <class Ar> void fields(Ar& ar, ScriptType& v) { io(ar, v.id); io(ar, v.name); io(ar, v.actions); }
template <class Ar> void fields(Ar& ar, TeamType& v) {
    io(ar, v.id);
    io(ar, v.name);
    io(ar, v.house);
    io(ar, v.taskforce);
    io(ar, v.script);
    io(ar, v.priority);
    io(ar, v.max);
    io(ar, v.full);
    io(ar, v.autocreate);
    io(ar, v.aggressive);
    io(ar, v.reinforce);
    io(ar, v.suicide);
    io(ar, v.tech_level);
    io(ar, v.is_base_defense);
    io(ar, v.waypoint);
}
template <class Ar> void fields(Ar& ar, TriggerObject& v) { io(ar, v.kind); io(ar, v.index); }
template <class Ar> void fields(Ar& ar, TriggerComparator& v) { io(ar, v.value); io(ar, v.op); }
template <class Ar> void fields(Ar& ar, AITriggerType& v) {
    io(ar, v.id);
    io(ar, v.name);
    io(ar, v.team1); io(ar, v.team2);
    io(ar, v.owner_house);
    io(ar, v.tech_level);
    io(ar, v.condition_type);
    io(ar, v.condition_object);
    io(ar, v.comparator);
    io(ar, v.starting_weight);
    io(ar, v.min_weight);
    io(ar, v.max_weight);
    io(ar, v.is_for_skirmish);
    io(ar, v.side);
    io(ar, v.base_defense);
    io(ar, v.enabled);
    io(ar, v.is_enabled);
}
template <class Ar> void fields(Ar& ar, AimdData& v) {
    io(ar, v.taskforces); io(ar, v.scripts); io(ar, v.teams); io(ar, v.triggers);
}
// --- AI-3 ---
template <class Ar> void fields(Ar& ar, AiTriggerState& v) {
    io(ar, v.weights);
    io(ar, v.times_completed);
    io(ar, v.times_executed);
    io(ar, v.rng);
    io(ar, v.triggers_fired);
    io(ar, v.last_fired);
    io(ar, v.adjustments);
    io(ar, v.last_adjusted);
    io(ar, v.last_success);
    io(ar, v.last_old_weight);
    io(ar, v.last_new_weight);
}
// --- AI config (resolved by main.cpp, saved so load needs no re-resolution) ---
template <class Ar> void fields(Ar& ar, AiTeamConfig& v) {
    io(ar, v.enabled);
    io(ar, v.aimd);
    io(ar, v.unit_roster);
    io(ar, v.inf_factory_types);
    io(ar, v.veh_factory_types);
    io(ar, v.waypoints);
    io(ar, v.house_name);
    io(ar, v.rules);
    io(ar, v.triggers_enabled);
    io(ar, v.trigger_side);
    io(ar, v.trigger_tech_level);
}
template <class Ar> void fields(Ar& ar, AiHouseConfig& v) {
    io(ar, v.difficulty);
    io(ar, v.catalog);
    io(ar, v.ai);
    io(ar, v.mods);
    io(ar, v.roster);
    io(ar, v.buildup_totals);
    io(ar, v.groups);
    io(ar, v.build_speed);
    io(ar, v.low_power_penalty);
    io(ar, v.start_credits);
    io(ar, v.team);
}

// --- skirmish config (the 'CONF' section: rebuilds the scene's assets) ---
template <class Ar> void fields(Ar& ar, SkirmishSlot& v) {
    io(ar, v.side); io(ar, v.color_ix); io(ar, v.country); io(ar, v.alliance_team);
    io(ar, v.combatant); io(ar, v.is_ai); io(ar, v.diff);
}
template <class Ar> void fields(Ar& ar, SkirmishConfig& v) {
    io(ar, v.map_id);
    io(ar, v.map_file);
    io(ar, v.use_map_starts);
    io(ar, v.slots);
    io(ar, v.starting_cash);
    io(ar, v.unit_count);
    io(ar, v.tech_level);
    io(ar, v.shroud);
    io(ar, v.crates);
    io(ar, v.superweapons);
    io(ar, v.short_game);
}

// --- header ---
template <class Ar> void fields(Ar& ar, SaveHeader& v) {
    io(ar, v.description);
    io(ar, v.kind);
    io(ar, v.map_file);
    io(ar, v.map_id);
    io(ar, v.map_w); io(ar, v.map_h); io(ar, v.map_cells);
    io(ar, v.frame);
    io(ar, v.unix_time);
}

// Members the visitor for T visits (the arity tripwire's right-hand side).
template <class T> std::size_t visited_fields() {
    Counter c;
    T t{};
    fields(c, t);
    return c.n;
}

// A generic blob helper: serialize any visitable value to bytes and back.
template <class T> std::vector<std::uint8_t> to_bytes(T& v) {
    Writer w;
    io(w, v);
    return std::move(w.buf);
}
template <class T> bool from_bytes(const std::vector<std::uint8_t>& b, T& v) {
    Reader r(b);
    io(r, v);
    return r.ok() && r.at_end();
}

}  // namespace save
}  // namespace ra2engine
