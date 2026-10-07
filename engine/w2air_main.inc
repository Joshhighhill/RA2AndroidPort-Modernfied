// w2air_main.inc -- wave-2 air/naval/SW (W2-AN-*) rules resolution, #included
// by main.cpp right before resolve_combat_unit (needs speed_to_cps/to_lower).

inline std::array<int, 11> w2_verses11(const Rules& rules, const std::string& warhead) {
    std::array<int, 11> a{};
    a.fill(100);
    const std::vector<int> v = rules.verses(warhead);
    for (std::size_t i = 0; i < v.size() && i < a.size(); ++i) a[i] = v[i];
    return a;
}

// The per-type naval-targeting, submarine, sonar, spawner, fixed-wing and
// missile keys [rules.ini; TechnoTypeClass offsets in engine/air_naval.hpp].
// Every key absent -> the inert AnType defaults.
void resolve_an_type(const Rules& rules, const std::string& unit_id, const std::string& primary,
                     const std::string& secondary, ResolvedUnit& r) {
    ra2engine::AnType& a = r.combat.an;
    a.naval_targeting = static_cast<int>(rules.section_double(unit_id, "NavalTargeting", 0.0));
    a.land_targeting = static_cast<int>(rules.section_double(unit_id, "LandTargeting", 0.0));
    a.no_passive_acquire = !rules.section_flag(unit_id, "CanPassiveAquire", true);
    // InfantryTypes are organic by type (the Chronosphere and the Iron Curtain
    // kill infantry); vehicles only with Organic=yes (DLPH, SQD).
    a.organic = rules.section_flag(unit_id, "Organic", !r.combat.is_vehicle);
    a.teleporter = rules.section_flag(unit_id, "Teleporter", false);
    a.underwater = rules.section_flag(unit_id, "Underwater", false);
    a.cloakable = rules.section_flag(unit_id, "Cloakable", false);
    a.sensors = rules.section_flag(unit_id, "Sensors", false);
    a.sensors_sight = static_cast<int>(rules.section_double(unit_id, "SensorsSight", 0.0));
    a.crashable = rules.section_flag(unit_id, "Crashable", false);
    a.weight = rules.section_double(unit_id, "Weight", 1.0);
    a.landable = rules.section_flag(unit_id, "Landable", false);
    a.min_range = rules.section_double(primary, "MinimumRange", 0.0);
    a.spawned = rules.section_flag(unit_id, "Spawned", false);
    a.missile = rules.section_flag(unit_id, "MissileSpawn", false);
    // Spawner weapon slot (Spawner=yes) + the Spawns= complement.
    if (!primary.empty() && rules.section_flag(primary, "Spawner", false)) a.spawner_slot = 0;
    else if (!secondary.empty() && rules.section_flag(secondary, "Spawner", false)) a.spawner_slot = 1;
    if (a.spawner_slot >= 0) {
        a.spawns = rules.section_string(unit_id, "Spawns", "");
        a.spawns_number = static_cast<int>(rules.section_double(unit_id, "SpawnsNumber", 1.0));
        a.spawn_regen_frames = static_cast<int>(rules.section_double(unit_id, "SpawnRegenRate", 0.0));
        a.spawn_reload_frames = static_cast<int>(rules.section_double(unit_id, "SpawnReloadRate", 0.0));
        if (a.spawns.empty()) a.spawner_slot = -1;   // V3 tank etc: [General] V3RocketType
    }
    // [ASWLauncher] Range=-2 ";infinite": a negative weapon range never limits
    // the shot (the sonar contact does) -- 30 cells stands in for infinite.
    if (r.combat.range < 0.0) r.combat.range = 30.0;
    if (r.combat.secondary.valid && r.combat.secondary.range < 0.0) r.combat.secondary.range = 30.0;
    // [V3Launcher]/[DredLauncher] spawn the [General] rocket types.
    if (a.spawner_slot < 0 && !primary.empty() && rules.section_flag(primary, "Spawner", false)) {
        a.spawner_slot = 0;
        a.spawns = to_lower(primary).find("dred") != std::string::npos
                       ? rules.section_string("General", "DMislType", "DMISL")
                       : rules.section_string("General", "V3RocketType", "V3ROCKET");
        a.spawns_number = std::max(1, static_cast<int>(rules.section_double(unit_id, "SpawnsNumber", 1.0)));
        a.spawn_regen_frames = static_cast<int>(rules.section_double(unit_id, "SpawnRegenRate", 0.0));
    }
    const std::string loco = to_lower(rules.locomotor(unit_id));
    // FlyLocomotion {4A582746} (ORCA/BEAG/HORNET/ASW/PDPLANE) [roster2.md §C].
    static const std::string kFly = "{4a582746-9839-11d1-b709-00a024ddafd1}";
    // RocketLocomotion {B7B49766} (V3ROCKET/DMISL).
    static const std::string kRocket = "{b7b49766-e576-11d3-9bd9-00104b972fe8}";
    if (loco == kFly || a.missile || loco == kRocket) {
        r.combat.is_aircraft = true;
        r.combat.is_vehicle = true;
        a.fly_speed_cps = speed_to_cps(static_cast<double>(r.speed));
        r.combat.flight_speed = a.fly_speed_cps;
        r.combat.flight_turn = rules.rot(unit_id) * 2.0 * 256.0 * 15.0;
        r.combat.cruise_altitude = rules.general_double("FlightLevel", 1500.0);
        r.combat.climb_rate = 1500.0;
        r.combat.descend_rate = 1500.0;
        r.combat.omni_fire = true;
        if (loco == kFly && !a.missile) {
            a.fixed_wing = true;
            a.ammo = static_cast<int>(rules.section_double(unit_id, "Ammo", 1.0));
        }
        if (a.missile || loco == kRocket) {
            a.missile = true;
            const bool dred = to_lower(unit_id) == to_lower(rules.section_string("General", "DMislType", "DMISL"));
            a.missile_damage = static_cast<int>(
                rules.general_double(dred ? "DMislDamage" : "V3RocketDamage", dred ? 300.0 : 200.0));
        }
    }
}

// The [General] superweapon / aircraft / naval values for World::set_sw_params.
ra2engine::SwParams resolve_sw_params(const Rules& rules) {
    ra2engine::SwParams p;
    auto gd = [&](const char* k, double d) { return rules.general_double(k, d); };
    auto gs = [&](const char* k, const char* d) {
        std::string v = rules.section_string("General", k, d);
        const std::size_t c = v.find(',');
        if (c != std::string::npos) v = v.substr(0, c);
        return v;
    };
    p.iron_frames = static_cast<int>(gd("IronCurtainDuration", 750.0));
    p.chrono_delay_frames = static_cast<int>(gd("ChronoDelay", 60.0));
    p.storm_deferment = static_cast<int>(gd("LightningDeferment", 250.0));
    p.storm_damage = static_cast<int>(gd("LightningDamage", 250.0));
    p.storm_duration = static_cast<int>(gd("LightningStormDuration", 180.0));
    p.storm_hit_delay = static_cast<int>(gd("LightningHitDelay", 10.0));
    p.storm_scatter_delay = static_cast<int>(gd("LightningScatterDelay", 5.0));
    p.storm_cell_spread = static_cast<int>(gd("LightningCellSpread", 10.0));
    p.storm_separation = static_cast<int>(gd("LightningSeparation", 3.0));
    const std::string swh = gs("LightningWarhead", "IonWH");
    p.storm_wh_spread = rules.section_double(swh, "CellSpread", 2.0);
    p.storm_wh_pmax = rules.section_double(swh, "PercentAtMax", 0.5);
    p.storm_verses = w2_verses11(rules, swh);
    p.amer_para_inf = gs("AmerParaDropInf", "E1");
    p.amer_para_num = static_cast<int>(gd("AmerParaDropNum", 8.0));
    p.ally_para_inf = gs("AllyParaDropInf", "E1");
    p.ally_para_num = static_cast<int>(gd("AllyParaDropNum", 6.0));
    p.sov_para_inf = gs("SovParaDropInf", "E2");
    p.sov_para_num = static_cast<int>(gd("SovParaDropNum", 9.0));
    p.paradrop_radius = static_cast<int>(gd("ParadropRadius", 1024.0));
    p.cloak_delay_frames = std::max(1, static_cast<int>(gd("CloakDelay", 0.02) * 900.0));
    p.ship_sinking_weight = gd("ShipSinkingWeight", 3.0);
    p.reload_frames = std::max(1, static_cast<int>(gd("ReloadRate", 0.3) * 900.0));
    p.flight_level = static_cast<int>(gd("FlightLevel", 1500.0));
    const std::string v3wh = gs("V3Warhead", "V3WH");
    p.missile_spread = rules.section_double(v3wh, "CellSpread", 1.0);
    p.missile_pmax = rules.section_double(v3wh, "PercentAtMax", 0.5);
    p.missile_verses = w2_verses11(rules, v3wh);
    p.repair_step = static_cast<int>(gd("RepairStep", 7.0));
    p.repair_rate_frames = std::max(1, static_cast<int>(gd("RepairRate", 0.016) * 900.0));
    p.repair_percent = gd("RepairPercent", 0.15);
    const std::string payload = "NukePayload";
    p.nuke_rad_level = static_cast<int>(rules.section_double(payload, "RadLevel", 500.0));
    return p;
}
