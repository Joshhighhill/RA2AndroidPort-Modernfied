// W5-B (wave-5 agent B, combat and movement core): arms the real game's World
// with the RE'd movement/combat rules. Called from the GameScene ctor / load
// path and the save-load gate World (never a unit-test World, which keeps the
// pre-W5 one-unit-per-cell model unless a test opts in).
//   R-061 infantry sub-cells (three infantry per cell, FUN_00481090).
//   R-062 land speed: [LandType] Foot..FloatBeach (GroundType FUN_00673A40),
//         TMP terrain-type -> LandType (DAT_008278F4), slope and damage
//         modifiers (DriveLocomotionClass FUN_004B2550).
//   R-050 crush steering: [CombatDamage] Crush / PlayerAutoCrush (FUN_00742720).

// "75%" -> 0.75, ".5" -> 0.5; absent -> fallback.
inline double w5_rules_fraction(const Rules& rules, const std::string& sec, const std::string& key,
                                double fallback) {
    std::string v = rules.section_string(sec, key, "");
    const std::size_t sc = v.find(';');
    if (sc != std::string::npos) v = v.substr(0, sc);
    if (v.find_first_not_of(" \t") == std::string::npos) return fallback;
    const double d = std::strtod(v.c_str(), nullptr);
    return v.find('%') != std::string::npos ? d / 100.0 : d;
}

// TMP image header byte 41 (terrain type) -> LandType: CellClass's land
// recalculation FUN_0047D1C0 -> FUN_00544910 -> table DAT_008278F4.
inline int w5_land_of_terrain_type(int tt) {
    static const int kTab[16] = {0, 8, 8, 8, 8, 10, 9, 3, 3, 2, 6, 1, 1, 0, 7, 3};
    return kTab[tt & 15];
}

void w5_arm_combat(ra2engine::World& world, const Rules& rules, const ra2assets::MapFile* map = nullptr,
                   ra2engine::Theater* theater = nullptr) {
    world.enable_subcells();
    // --- R-062 land speed ---------------------------------------------------
    ra2engine::World::LandSpeedRules r;
    static const char* kLand[12] = {"Clear", "Road", "Water", "Rock", "Wall", "Tiberium",
                                    "Beach", "Rough", "Ice", "Railroad", "Tunnel", "Weeds"};
    static const char* kSpeed[8] = {"Foot", "Track", "Wheel", "Hover", "", "Float", "Amphibious", "FloatBeach"};
    for (int l = 0; l < 12; ++l)
        for (int s = 0; s < 8; ++s) {
            double v = (s == 4) ? 1.0 : w5_rules_fraction(rules, kLand[l], kSpeed[s], 1.0);   // Winged = 1.0
            if (v > 1.0) v = 1.0;   // FUN_00673A40 caps every column at 1.0
            r.cost[l][s] = static_cast<float>(v);
        }
    r.tracked_up = rules.general_double("TrackedUphill", 1.0);
    r.tracked_down = rules.general_double("TrackedDownhill", 1.0);
    r.wheeled_up = rules.general_double("WheeledUphill", 1.0);
    r.wheeled_down = rules.general_double("WheeledDownhill", 1.0);
    r.condition_red = w5_rules_fraction(rules, "AudioVisual", "ConditionRed", 0.25);
    const int gw = world.grid_w(), gh = world.grid_h();
    std::vector<std::uint8_t> land(static_cast<std::size_t>(gw) * gh, 0), ramp(land.size(), 0);
    if (map && theater) {
        for (const ra2assets::MapCell& c : map->cells()) {
            if (c.x < 0 || c.y < 0 || c.x >= gw || c.y >= gh) continue;
            const std::size_t i = static_cast<std::size_t>(c.y) * gw + c.x;
            const ra2assets::TmpFile* t = theater->tmp_for_tile(c.tile);
            if (!t || c.subtile >= t->count()) continue;
            const std::uint8_t* hdr = nullptr;
            try { hdr = t->image_header(c.subtile); } catch (const std::exception&) { hdr = nullptr; }
            if (!hdr) continue;
            land[i] = static_cast<std::uint8_t>(w5_land_of_terrain_type(hdr[41]));
            ramp[i] = hdr[42];
        }
    }
    world.arm_land_speed(land, ramp, r);
    // --- R-050 crush steering ------------------------------------------------
    ra2engine::World::CrushRules cr;
    cr.armed = true;
    // [CombatDamage] Crush= cells -> leptons (FUN_00474550: ftol(cells*256)).
    cr.crush_leptons = static_cast<int>(rules.section_double("CombatDamage", "Crush", 1.8) * 256.0);
    cr.player_auto_crush = rules.section_flag("CombatDamage", "PlayerAutoCrush", false);
    world.arm_crush(cr);
}
