// engine/net_frame_crc.cpp -- see net_frame_crc.hpp. SDL-free.
#include "net_frame_crc.hpp"

#include "world.hpp"

namespace ra2engine {

// World's private sim state is read through this friend (world.hpp), the
// SaveAccess pattern. Read-only.
struct NetCrcAccess {
    static std::uint32_t crc(const World& w, const std::vector<int>* extra) {
        FrameCrc c;
        // --- units [RE: FUN_0064D520 infantry / unit loops]: position, facing
        // (body + turret), plus the clone's stronger terms.
        c.add_i(static_cast<int>(w.units_.size()));
        for (const Unit& u : w.units_) {
            if (!u.alive) {   // a tombstone only counts as one (never erased)
                c.add(0xDEADu);
                continue;
            }
            // The original's term: X/10 + (Y/10) << 16 + facing8.
            c.add(static_cast<std::uint32_t>(u.cell_x) + (static_cast<std::uint32_t>(u.cell_y) << 16) +
                  static_cast<std::uint32_t>(u.facing & 0xFF));
            c.add_d(u.fx);
            c.add_d(u.fy);
            c.add_d(u.body_facing);
            c.add_d(u.turret_facing);
            c.add_d(u.altitude);
            c.add_i(u.house);
            c.add_i(u.hp);
            c.add_i(u.target);
            c.add_i(u.target_building);
            c.add_i(u.attack_order ? 1 : 0);
            c.add_i(static_cast<int>(u.path.size()));
            c.add_i(static_cast<int>(u.path_idx));
            c.add_i(u.cargo_of);
            c.add_i(u.capture_target);
            c.add_i(u.infiltrate_target);
            c.add_i(u.enter_target);
            c.add_i(u.rearm_frames);
            c.add_d(u.die_time);
            c.add_i(static_cast<int>(u.harv));
            c.add_d(u.bales_ore);
            c.add_d(u.bales_gems);
        }
        // --- buildings [RE: FUN_0064D520 building loop]
        c.add_i(static_cast<int>(w.buildings_.size()));
        for (const Building& b : w.buildings_) {
            if (!b.alive) {
                c.add(0xDEADu);
                continue;
            }
            c.add(static_cast<std::uint32_t>(b.cx) + (static_cast<std::uint32_t>(b.cy) << 16) +
                  static_cast<std::uint32_t>(b.type & 0xFF));
            c.add_i(b.house);
            c.add_i(b.hp);
            c.add_i(static_cast<int>(b.state));
            c.add_d(b.buildup_time);
            c.add_d(b.die_time);
        }
        // --- projectiles and crates (the original's Logic / layer lists)
        c.add_i(static_cast<int>(w.projectiles_.size()));
        for (const Projectile& p : w.projectiles_) {
            c.add(static_cast<std::uint32_t>(p.x) + (static_cast<std::uint32_t>(p.y) << 16) +
                  static_cast<std::uint32_t>(p.z));
            c.add_i(p.target_unit);
            c.add_i(p.target_building);
            c.add_i(static_cast<int>(p.kind));
        }
        c.add_i(static_cast<int>(w.crates_.size()));
        for (const World::Crate& k : w.crates_) c.add(static_cast<std::uint32_t>(k.x) + (static_cast<std::uint32_t>(k.y) << 16) + static_cast<std::uint32_t>(k.powerup));
        // --- house economy: wallets, every production slot, the legacy pool,
        // defeat / outcome.
        c.add_i(static_cast<int>(w.econ_.size()));
        for (const HouseEcon& e : w.econ_) {
            c.add_d(e.credits);
            c.add_i(e.held);
            for (int q = 0; q < kQueueCats; ++q) {
                const QueueSlot& s = e.queues.state(static_cast<QueueCat>(q)).slot;
                c.add_i(s.type);
                c.add_i(s.stage);
                c.add_d(s.frames_accum);
                c.add_d(s.spent);
                c.add_i(s.ready ? 1 : 0);
                c.add_i(static_cast<int>(e.backlog[static_cast<std::size_t>(q)].size()));
            }
        }
        for (double v : w.house_credits_) c.add_d(v);
        for (std::uint8_t v : w.house_defeated_) c.add(v);
        for (int v : w.house_outcome_) c.add_i(v);
        // --- every sim RNG stream (peeked, never drawn; see the header) and
        // the subsystem frame clocks.
        c.add_i(w.crate_rng_.idx1());
        c.add_i(w.crate_rng_.idx2());
        for (int i = 0; i < 250; ++i) c.add(w.crate_rng_.table_word(i));
        c.add(w.proj_rng_.s);
        c.add(w.combat_rng_.s);
        c.add(w.ore_rng_.s);
        c.add(w.bridge_rng_.s);
        c.add(w.an_.rng);
        c.add_i(w.an_.frame);
        for (const World::HouseAiRec& r : w.ai_houses_) {
            if (!r.is_ai) continue;
            c.add(r.state.rng);
            c.add(r.team_state.rng);
            c.add(r.trigger_state.rng);
            c.add_i(r.enemy_house);
            c.add_i(static_cast<int>(r.team_state.teams.size()));
        }
        c.add_i(w.proj_frame_);
        c.add_u64(w.vet_frame_);
        c.add_u64(w.ability_frame_);
        c.add_u64(w.repair_frame_);
        if (extra)
            for (int v : *extra) c.add_i(v);
        return c.v;
    }
};

std::uint32_t net_frame_crc(const World& w, const std::vector<int>* extra) { return NetCrcAccess::crc(w, extra); }

}  // namespace ra2engine
