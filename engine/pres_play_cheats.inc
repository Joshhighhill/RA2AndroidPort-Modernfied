// pres_play_cheats.inc -- W5-PRES --play verbs for the presentation
// acceptance scripts (scripts/play/pres-*.play). Included INSIDE
// GameScene::cheat() after play_art_cheats.inc; reachable only from a --play
// script.
//
//   cheat deck_spawn TYPE H [n] [land]  spawn TYPE for house H on the n-th
//                                 high-bridge deck cell over water (or over
//                                 land: the unit is UNDER the deck), camera on it
//   cheat deck_layers on|off      R-100 deck layering on/off (diagnostic)
auto pres_spawn_at = [&](const std::string& id, int h, int x, int y) -> bool {
    const ra2engine::UnitTypeDef* def = nullptr;
    for (const UnitRoster* r : {&ur_, &ur_ai_})
        for (const ra2engine::UnitTypeDef& d : r->defs)
            if (!def && d.id == id) def = &d;
    ra2engine::UnitTypeDef tmp;
    if (!def) {
        tmp.id = id;
        tmp.combat = resolve_combat_unit(rules_, id).combat;
        if (tmp.combat.image.empty()) return false;
        def = &tmp;
    }
    world_.add_combat_unit(x, y, h, def->combat);
    world_.set_anim(def->combat.image,
                    ua_.anims.count(def->combat.image) ? ua_.anims.at(def->combat.image) : ra2engine::default_anim());
    return true;
};
if (args.size() >= 3 && args.size() <= 5 && args[0] == "deck_spawn") {
    int want = args.size() >= 4 ? std::atoi(args[3].c_str()) : 0;
    const bool over_land = args.size() == 5 && args[4] == "land";
    const std::vector<std::uint8_t>& wg = world_.grid_for(ra2engine::MovementDomain::Water);
    for (int di : map_dressing_.decks()) {
        const PresMap::Obj& o = map_dressing_.overlays()[static_cast<std::size_t>(di)];
        const std::size_t i = static_cast<std::size_t>(o.y) * world_.grid_w() + o.x;
        if (i >= wg.size() || (wg[i] != 0) == over_land || want-- > 0) continue;
        if (!pres_spawn_at(args[1], std::atoi(args[2].c_str()), o.x, o.y)) return false;
        input_.center_on_cell(o.x, o.y);
        std::cerr << "[deck_spawn] " << args[1] << " at deck cell " << o.x << "," << o.y << " level=" << o.level << std::endl;
        return true;
    }
    std::cerr << "[deck_spawn] decks=" << map_dressing_.decks().size() << " water-grid=" << wg.size() << std::endl;
    for (std::size_t k = 0; k < map_dressing_.decks().size() && k < 40; ++k) {
        const PresMap::Obj& o = map_dressing_.overlays()[static_cast<std::size_t>(map_dressing_.decks()[k])];
        const std::size_t i = static_cast<std::size_t>(o.y) * world_.grid_w() + o.x;
        std::cerr << "  deck " << o.key << " " << o.x << "," << o.y << " L" << o.level << " fr=" << o.frame
                  << " water=" << (i < wg.size() ? (int)wg[i] : -1) << " pass=" << (int)world_.passable()[i] << std::endl;
    }
    return false;
}
if (args.size() == 2 && args[0] == "deck_layers") {
    map_dressing_.set_layer_decks(args[1] == "on");
    return true;
}
