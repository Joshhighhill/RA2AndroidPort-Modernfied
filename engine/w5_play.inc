// w5_play.inc -- wave-5 agent B (combat and movement core) --play support,
// #included INSIDE the GameScene class body right after w2air_play.inc.
// Probe keys for scripts/play/w5-*.play. Interactive-only: no gate reaches it.
//   subcells                 1 when infantry sub-cells are armed
//   max_cell_inf(h)          most house-h infantry standing in one cell
//   cells3(h)                cells holding three house-h infantry
//   uhpn(h,ID,n) ucx/ucy/uspot/ulx/uly(h,ID,n)  n-th live unit of the type:
//                            HP, cell, sub-cell spot, lepton position (+sub)
//   bhp(h,ID) bmaxhp(h,ID)   first live building of the type
//   land(x,y)                LandType index of a cell; uspeed(h,ID,n) = the
//                            n-th unit's current cells/s (W5-B R-062)
    void w5_play_probe(ra2play::Probe& p) const {
        for (const char* f : {"max_cell_inf", "cells3", "uhpn", "ucx", "ucy", "uspot", "ulx", "uly",
                              "bhp", "bmaxhp", "uspeed"})
            p.zero_families.insert(f);
        p.set("subcells", world_.subcells_enabled() ? 1 : 0);
        std::map<std::pair<int, int>, int> per_cell[16];
        std::map<std::string, int> nth;
        const auto& us = world_.units();
        for (int i = 0; i < static_cast<int>(us.size()); ++i) {
            const ra2engine::Unit& u = us[(std::size_t)i];
            if (!u.alive || u.die_time >= 0.0 || u.cargo_of >= 0) continue;
            const std::string id = play_unit_id(u);
            const std::string base = std::to_string(u.house) + "," + id;
            const int n = nth[base]++;
            const std::string k = base + "," + std::to_string(n);
            p.set("uhpn(" + k + ")", u.hp);
            p.set("ucx(" + k + ")", u.cell_x);
            p.set("ucy(" + k + ")", u.cell_y);
            p.set("uspot(" + k + ")", u.spot);
            p.set("ulx(" + k + ")", u.fx * 256.0 + u.sub_x);
            p.set("uly(" + k + ")", u.fy * 256.0 + u.sub_y);
            p.set("uspeed(" + k + ")", world_.unit_speed_cps(i));
            if (!u.is_vehicle && !u.is_aircraft && !u.is_naval && u.house >= 0 && u.house < 16)
                ++per_cell[u.house][{u.cell_x, u.cell_y}];
        }
        for (int h = 0; h < world_.house_count() && h < 16; ++h) {
            int mx = 0, c3 = 0;
            for (const auto& kv : per_cell[h]) {
                mx = std::max(mx, kv.second);
                c3 += kv.second >= 3;
            }
            p.set("max_cell_inf(" + std::to_string(h) + ")", mx);
            p.set("cells3(" + std::to_string(h) + ")", c3);
        }
        const auto& bs = world_.buildings();
        for (const ra2engine::Building& b : bs) {
            if (!b.alive || b.state == ra2engine::Building::State::Dead) continue;
            if (b.type < 0 || b.type >= (int)world_.btypes().size()) continue;
            const std::string k = std::to_string(b.house) + "," + world_.btypes()[(std::size_t)b.type].id;
            if (p.n.count("bhp(" + k + ")")) continue;
            p.set("bhp(" + k + ")", b.hp);
            p.set("bmaxhp(" + k + ")", b.max_hp);
        }
    }
