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
#include "campaign.hpp"   // P10 CampaignHostSave
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
        save::io(ar, f.mines_);   // W2-SK-01 ore mines (v9)
        save::io(ar, f.growth_frames_);
        save::io(ar, f.spread_frames_);
        save::io(ar, f.spread_timer_);
    }
    template <class Ar> static void queues(Ar& ar, ProductionQueues& q) { save::io(ar, q.q_); }
    template <class Ar> static void rad_field(Ar& ar, RadField& f) {   // P4 radiation
        save::io(ar, f.cell_);
        save::io(ar, f.sites_);
        save::io(ar, f.w_);
        save::io(ar, f.h_);
    }
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
        save::io(ar, r.enemy_house);   // W2-SK-08 (v9)
    }
    template <class Ar> static void house_sw(Ar& ar, World::HouseSuperweapon& s) {
        save::io(ar, s.has_sw);
        save::io(ar, s.sw);
        save::io(ar, s.eff_damage);
        save::io(ar, s.eff_cell_spread);
        save::io(ar, s.eff_percent_at_max);
        save::io(ar, s.eff_verses);
        save::io(ar, s.is_powered);   // gap owner A (G-29)
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
        save::io(ar, w.wallet_hint_);   // W2-SK-20 (v9)
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
        save::io(ar, w.purifier_bonus_);   // gap owner A (G-27)
        save::io(ar, w.harvest_scan_);     // W2-SK-02 (v9)
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
        // P4 abilities
        save::io(ar, w.abilities_any_);
        save::io(ar, w.ability_frame_);
        save::io(ar, w.ability_rules_);
        save::io(ar, w.ability_profiles_);
        save::io(ar, w.rad_);
        save::io(ar, w.no_c4_types_);
        save::io(ar, w.ability_events_);
        save::io(ar, w.fire_events_);   // Gap fix C
        save::io(ar, w.combat_rng_);
        // gF MCV deploy
        save::io(ar, w.deploy_any_);
        save::io(ar, w.deploy_events_);
        save::io(ar, w.buildup_totals_);
        save::io(ar, w.undeploy_types_);
        save::io(ar, w.undeploy_units_);
        save::io(ar, w.mcv_redeploys_);
        save::io(ar, w.skirmish_session_);
        // G16 world depth (agent G)
        save::io(ar, w.depth_rules_);
        save::io(ar, w.garrison_any_);
        save::io(ar, w.garrison_shots_);
        save::io(ar, w.garrison_events_);
        save::io(ar, w.bridge_ovl_);
        save::io(ar, w.bridge_tile_land_);
        save::io(ar, w.bridge_version_);
        save::io(ar, w.bridge_rng_);
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
        save::io(ar, s.campaign_mode_);   // P10
        save::io(ar, s.semi_detached_);   // P10
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
template <class Ar> void fields(Ar& ar, RadField& v) { SaveAccess::rad_field(ar, v); }
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
template <class Ar> void fields(Ar& ar, HarvestScan& v) {   // W2-SK-02 (v9)
    io(ar, v.long_scan);
    io(ar, v.short_scan);
}
template <class Ar> void fields(Ar& ar, OreMine& v) {   // W2-SK-01 (v9)
    io(ar, v.x);
    io(ar, v.y);
    io(ar, v.half_frames);
    io(ar, v.anim_rate);
    io(ar, v.anim_prob);
    io(ar, v.stage);
    io(ar, v.rate);
    io(ar, v.timer);
    io(ar, v.spawned);
}
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
    io(ar, v.weapon_slot);   // Gap fix C
    io(ar, v.attacker_building);
}
// Gap fix C: one shot event (World::FireEvent).
template <class Ar> void fields(Ar& ar, World::FireEvent& v) {
    io(ar, v.unit); io(ar, v.building); io(ar, v.target_unit); io(ar, v.target_building);
    io(ar, v.slot); io(ar, v.weapon);
}
// Gap fix C: one resolved weapon slot.
template <class Ar> void fields(Ar& ar, WeaponSlot& v) {
    io(ar, v.valid);
    io(ar, v.id);
    io(ar, v.damage);
    io(ar, v.rof);
    io(ar, v.range);
    io(ar, v.verses);
    io(ar, v.cell_spread);
    io(ar, v.percent_at_max);
    io(ar, v.proj);
    io(ar, v.aa);
    io(ar, v.ag);
    io(ar, v.omni_fire);
    io(ar, v.wall);   // G16
}
// G16 world depth (agent G)
template <class Ar> void fields(Ar& ar, World::DepthRules& v) {
    io(ar, v.occupy_damage_mult); io(ar, v.occupy_rof_mult); io(ar, v.occupy_weapon_range);
    io(ar, v.bridge_strength); io(ar, v.bridge_destruction); io(ar, v.armed);
}
template <class Ar> void fields(Ar& ar, World::GarrisonEvent& v) {
    io(ar, v.kind); io(ar, v.building); io(ar, v.house);
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

// --- P4 abilities ---
template <class Ar> void fields(Ar& ar, WeaponSpec& v) {
    io(ar, v.valid);
    io(ar, v.damage);
    io(ar, v.rof);
    io(ar, v.range);
    io(ar, v.verses);
    io(ar, v.cell_spread);
    io(ar, v.percent_at_max);
    io(ar, v.rad_level);
    io(ar, v.kind);
}
template <class Ar> void fields(Ar& ar, AbilityProfile& v) {
    io(ar, v.fire_kind);
    io(ar, v.c4);
    io(ar, v.c4_range);
    io(ar, v.teleporter);
    io(ar, v.explodes);
    io(ar, v.death);
    io(ar, v.deployer);
    io(ar, v.deploy);
    io(ar, v.immune_radiation);
    io(ar, v.parasiteable);
    io(ar, v.warpable);
    io(ar, v.no_passive_acquire);
    io(ar, v.no_retaliate);
    io(ar, v.suppression_threshold);
    io(ar, v.ifv_mode);
    io(ar, v.gunner);
    io(ar, v.ifv_weapons);
    io(ar, v.infantry);
}
template <class Ar> void fields(Ar& ar, AbilityRules& v) {
    io(ar, v.rad_duration_multiple);
    io(ar, v.rad_application_delay);
    io(ar, v.rad_level_max);
    io(ar, v.rad_level_delay);
    io(ar, v.rad_level_factor);
    io(ar, v.rad_verses);
    io(ar, v.chrono_distance_factor);
    io(ar, v.chrono_trigger);
    io(ar, v.chrono_minimum_delay);
    io(ar, v.chrono_range_minimum);
    io(ar, v.c4_delay_minutes);
    io(ar, v.ivan_damage);
    io(ar, v.ivan_timed_delay);
    io(ar, v.ivan_warhead);
}
template <class Ar> void fields(Ar& ar, RadSite& v) {
    io(ar, v.cx); io(ar, v.cy);
    io(ar, v.spread);
    io(ar, v.spread_leptons);
    io(ar, v.rad_level);
    io(ar, v.level_steps);
    io(ar, v.duration);
    io(ar, v.time_left);
    io(ar, v.timer_start);
    io(ar, v.timer_left);
    io(ar, v.alive);
}
template <class Ar> void fields(Ar& ar, World::DeployEvent& v) {   // gF
    io(ar, v.kind);
    io(ar, v.unit);
    io(ar, v.building);
    io(ar, v.house);
}
template <class Ar> void fields(Ar& ar, World::AbilityEvent& v) {
    io(ar, v.kind); io(ar, v.a); io(ar, v.b); io(ar, v.x); io(ar, v.y); io(ar, v.building);
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
    // Gap fix C
    io(ar, v.secondary);
    io(ar, v.elite_secondary);
    io(ar, v.primary_aa);
    io(ar, v.primary_ag);
    io(ar, v.elite_proj);
    io(ar, v.has_elite_proj);
    io(ar, v.deploy_fire);
    io(ar, v.deploy_fire_slot);
    io(ar, v.crusher);
    io(ar, v.crushable);
    io(ar, v.deploys_into);   // gF
    io(ar, v.primary_wall);   // G16
    io(ar, v.occupier);
    io(ar, v.occupy_weapon);
    io(ar, v.elite_occupy_weapon);
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
    io(ar, v.naval);       // gap owner A (G-22/G-24)
    io(ar, v.aircraft);
    io(ar, v.harvester);
    io(ar, v.storage);
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
    io(ar, v.deploy_facing);   // gF
    io(ar, v.ore_purifier);   // gap owner A (G-27)
    io(ar, v.capturable);     // W2-SK-12 (v9)
    io(ar, v.spyable);        // W2-SK-20 (v9)
    io(ar, v.unit_repair);    // W2-SK-19 (v9)
    io(ar, v.factory_kind);   // W2-SK-20 (v9)
    io(ar, v.soylent);
    // Gap fix C: defense weapons
    io(ar, v.weapon);
    io(ar, v.armed);
    io(ar, v.powered);
    io(ar, v.rot);
    io(ar, v.turret);
    io(ar, v.guard_range);
    io(ar, v.can_be_occupied);   // G16
    io(ar, v.can_occupy_fire);
    io(ar, v.max_occupants);
    io(ar, v.bridge_repair_hut);
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
    io(ar, v.harv_skip_x);   // W2-SK-04 (v9)
    io(ar, v.harv_skip_y);
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
    // P4 abilities
    io(ar, v.abil);
    io(ar, v.abil_bound);
    io(ar, v.frozen);
    io(ar, v.warp_until);
    io(ar, v.ability_fired_unit);
    io(ar, v.ability_fired_building);
    io(ar, v.parasite_on_me);
    io(ar, v.parasite_next_tick);
    io(ar, v.suppress_until);
    io(ar, v.last_hp);
    io(ar, v.temporal_target);
    io(ar, v.temporal_target_building);
    io(ar, v.warp_remaining);
    io(ar, v.bomb_frame);
    io(ar, v.bomb_owner);
    io(ar, v.bomb_house);
    io(ar, v.deployed);
    io(ar, v.deploy_next_fire);
    io(ar, v.ifv_current);
    io(ar, v.death_done);
    // Gap fix C: weapon slots / deploy-fire / crush
    io(ar, v.w2);
    io(ar, v.w2_elite);
    io(ar, v.p_aa); io(ar, v.p_ag);
    io(ar, v.p_proj);
    io(ar, v.p_proj_elite);
    io(ar, v.p_proj_elite_set);
    io(ar, v.w_active);
    io(ar, v.w_aa); io(ar, v.w_ag);
    io(ar, v.deploy_fire);
    io(ar, v.deploy_fire_slot);
    io(ar, v.crusher);
    io(ar, v.crushable);
    // gF MCV deploy
    io(ar, v.deploys_into);
    io(ar, v.deploy_mission);
    io(ar, v.deploy_goal_x); io(ar, v.deploy_goal_y);
    io(ar, v.deployed_away);
    // G16 world depth
    io(ar, v.p_wall);
    io(ar, v.occupier);
    io(ar, v.occupy_w);
    io(ar, v.occupy_w_elite);
    io(ar, v.garrison_of);
    io(ar, v.occupy_target);
    io(ar, v.attack_cell_x); io(ar, v.attack_cell_y);
    io(ar, v.cell_shot);
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
    // P4 abilities
    io(ar, v.c4_applied);
    io(ar, v.c4_frame);
    io(ar, v.c4_by);
    io(ar, v.bomb_frame);
    io(ar, v.bomb_owner);
    io(ar, v.bomb_house);
    io(ar, v.warp_remaining);
    io(ar, v.ability_death_done);
    // Gap fix C: defense combat state
    io(ar, v.def_target);
    io(ar, v.def_rof_cool);
    io(ar, v.def_turret_facing);
    io(ar, v.def_slot);
    io(ar, v.def_fire_flash);
    io(ar, v.def_offline);
    // G16 garrison
    io(ar, v.occupants);
    io(ar, v.firing_occupant);
    io(ar, v.garrison_rof_cool);
    io(ar, v.garrison_target);
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
    io(ar, v.firepower_mul); io(ar, v.rof_mul); io(ar, v.repair_delay_min);
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
    io(ar, v.condition_red);
    io(ar, v.sw_value_conyard); io(ar, v.sw_value_factory); io(ar, v.sw_value_power);
    io(ar, v.sw_value_tech); io(ar, v.sw_value_defense); io(ar, v.sw_value_harvester);
    io(ar, v.sw_value_building); io(ar, v.sw_value_unit);
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
    io(ar, v.repair_clock);
    io(ar, v.pending);
    io(ar, v.next_node_id);
}
template <class Ar> void fields(Ar& ar, AiCatalog& v) {
    io(ar, v.conyard); io(ar, v.power); io(ar, v.refinery); io(ar, v.barracks);
    io(ar, v.war_factory); io(ar, v.radar); io(ar, v.defenses); io(ar, v.wall);
    io(ar, v.tech); io(ar, v.superweapons);
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
    io(ar, v.best_focus_d2);
    io(ar, v.no_progress_secs);
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
    io(ar, v.building_type_roster);
    io(ar, v.building_class);
    io(ar, v.harvesters_per_refinery);
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
    io(ar, v.multiple_factory);   // W2-SK-06 (v9)
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
    io(ar, v.campaign_map);          // P10: campaign mission identity ("" = skirmish)
    io(ar, v.campaign_side);
    io(ar, v.campaign_difficulty);
    io(ar, v.game_speed);            // skirmish page: lobby Game Speed (-1 = unset)
    io(ar, v.mcv_redeploys);         // skirmish page: MCV Repacks (-1 = unset)
}

// --- P10 campaign runtime state (the 'HOST' section of a campaign save) ---
template <class Ar> void fields(Ar& ar, CampaignHostSave& v) {
    io(ar, v.u_tag); io(ar, v.u_cx); io(ar, v.u_cy); io(ar, v.u_hp); io(ar, v.u_team);
    io(ar, v.u_type); io(ar, v.u_flags);
    io(ar, v.b_tag); io(ar, v.b_hp); io(ar, v.b_type); io(ar, v.b_flags);
    io(ar, v.t_type); io(ar, v.t_script); io(ar, v.t_house); io(ar, v.t_pc);
    io(ar, v.t_goal_x); io(ar, v.t_goal_y); io(ar, v.t_members);
    io(ar, v.t_wait_until); io(ar, v.t_issued_frame); io(ar, v.t_created); io(ar, v.t_flags);
    io(ar, v.destroyed_units); io(ar, v.destroyed_buildings); io(ar, v.last_built);
    io(ar, v.obj_frames); io(ar, v.obj_texts);
    io(ar, v.end_outcome); io(ar, v.end_flagged_at); io(ar, v.frame); io(ar, v.accum);
    io(ar, v.logged); io(ar, v.teams_created); io(ar, v.raises);
}

// --- GameScene / UI state (the 'SCNE' section) ---------------------------
// Everything GameScene holds OUTSIDE the World that the simulation or the
// resumed screen depends on. Transient input (drag box, armed repair/sell/
// placement cursor, hover) is NOT saved -- load returns with no armed mode,
// as the original's dialog does. Anyone adding a sim-relevant GameScene member
// adds it here + to GameScene::capture_save/apply_save (main.cpp).
struct SceneSave {
    int cam_x = 0, cam_y = 0;          // Camera
    int active_tab = 0;                // sidebar tab (0..3)
    ProductionQueues queues;           // the player's four queues
    double player_credits = 0.0;       // the wallet (outside the World)
    double credits_shown = 0.0;        // the rolling credits display
    QueueCat place_cat = QueueCat::Structure;  // queue a pending placement drains
    std::vector<int> bstates;          // FreeUnit grand-opening tracker
};
template <class Ar> void fields(Ar& ar, SceneSave& v) {
    io(ar, v.cam_x); io(ar, v.cam_y);
    io(ar, v.active_tab);
    io(ar, v.queues);
    io(ar, v.player_credits);
    io(ar, v.credits_shown);
    io(ar, v.place_cat);
    io(ar, v.bstates);
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
