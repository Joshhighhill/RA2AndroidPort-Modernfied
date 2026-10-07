// play_art_cheats.inc -- W3-FIX-A --play harness verbs for the art/asset
// acceptance scripts (scripts/play/art-*.play). Included INSIDE
// GameScene::cheat(); reachable only from a --play script (the Scene::cheat
// hook is driven by the play driver, never by player input).
//
//   cheat reveal                 reveal the whole map for the local player
//   cheat art_dump               log every live building type (art ok,
//                                invisible, own palette) and unit image
//   cheat view_building TYPE [n] centre the camera on the n-th live TYPE
//   cheat view_unit IMAGE [n]    centre the camera on the n-th live unit image
//   cheat view_dressing IMAGE [n] centre on the n-th civilian map-dressing object
//   cheat view_key PREFIX [n]    W4-FIX-POS: centre on the n-th map object
//                                (terrain / civ / overlay / smudge) whose key
//                                starts with PREFIX (terr:, civ:, ovl:, smudge:)
if (args.size() == 1 && args[0] == "reveal") {
    world_.reveal_all_house(0);
    return true;
}
if (args.size() == 1 && args[0] == "art_dump") {
    std::map<std::string, int> bc, uc;
    for (const ra2engine::Building& b : world_.buildings()) {
        if (!b.alive || b.type < 0 || b.type >= static_cast<int>(ba_.art.size())) continue;
        const BuildingArt& a = ba_.art[static_cast<std::size_t>(b.type)];
        const ra2engine::BuildingTypeDef& d = ba_.roster[static_cast<std::size_t>(b.type)];
        ++bc[d.id + " img=" + d.image + " h=" + std::to_string(b.house) +
             (a.ok ? " ok frames=" + std::to_string(a.shp.count()) : " NOART") + (a.invisible ? " invisible" : "") +
             (a.own_pal ? " ownpal" : "")];
    }
    for (const ra2engine::Unit& u : world_.units()) {
        if (!u.alive) continue;
        ++uc[u.image + " h=" + std::to_string(u.house) + (u.is_aircraft ? " air alt=" + std::to_string((int)u.altitude)
                                                                         : u.is_vehicle ? " veh" : " inf")];
    }
    std::map<std::string, int> dc;
    for (const PresMap::Obj& o : map_dressing_.objects())
        if (o.key.rfind("civ:", 0) == 0) ++dc[o.key.substr(4) + (o.own_pal ? " ownpal" : "")];
    for (const auto& kv : dc) std::cerr << "[art-dump] dressing " << kv.first << " x" << kv.second << "\n";
    for (const auto& kv : bc) std::cerr << "[art-dump] bld " << kv.first << " x" << kv.second << "\n";
    for (const auto& kv : uc) std::cerr << "[art-dump] unit " << kv.first << " x" << kv.second << "\n";
    return true;
}
if ((args.size() == 2 || args.size() == 3) && (args[0] == "view_building" || args[0] == "view_unit")) {
    int want = args.size() == 3 ? std::atoi(args[2].c_str()) : 0;
    if (args[0] == "view_building") {
        for (const ra2engine::Building& b : world_.buildings()) {
            if (!b.alive || b.type < 0 || b.type >= static_cast<int>(ba_.roster.size())) continue;
            if (ba_.roster[static_cast<std::size_t>(b.type)].id != args[1] || want-- > 0) continue;
            input_.center_on_cell(b.cx + b.fw / 2.0, b.cy + b.fh / 2.0);
            return true;
        }
        return false;
    }
    for (const ra2engine::Unit& u : world_.units()) {
        if (!u.alive || u.die_time >= 0.0 || u.image != args[1] || want-- > 0) continue;
        input_.center_on_cell(u.fx, u.fy);
        return true;
    }
    return false;
}
if ((args.size() == 2 || args.size() == 3) && args[0] == "view_dressing") {
    int want = args.size() == 3 ? std::atoi(args[2].c_str()) : 0;
    for (const PresMap::Obj& o : map_dressing_.objects()) {
        if (o.key != "civ:" + args[1] || want-- > 0) continue;
        input_.center_on_cell(o.x + o.fw / 2.0, o.y + o.fh / 2.0);
        return true;
    }
    return false;
}

if ((args.size() == 2 || args.size() == 3) && args[0] == "view_key") {
    int want = args.size() == 3 ? std::atoi(args[2].c_str()) : 0;
    for (const std::vector<PresMap::Obj>* v : {&map_dressing_.objects(), &map_dressing_.overlays(), &map_dressing_.smudges()})
        for (const PresMap::Obj& o : *v) {
            if (o.key.rfind(args[1], 0) != 0 || !o.shp || want-- > 0) continue;
            input_.center_on_cell(o.x + o.fw / 2.0, o.y + o.fh / 2.0);
            std::cerr << "[view_key] " << o.key << " cell=" << o.x << "," << o.y << " level=" << o.level
                      << " canvas=" << o.shp->width() << "x" << o.shp->height() << " frame=" << o.frame << std::endl;
            return true;
        }
    return false;
}
