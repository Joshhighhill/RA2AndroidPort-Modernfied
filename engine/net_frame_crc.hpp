#pragma once
// W9-01-04 / W9-02-04: the per-frame lockstep sync CRC (the FRAMEINFO CRC).
//
// [RE: ComputeFrameCRC FUN_0064D520, dump D:\ra2-re\net1-out\dec2.c] The
// original folds, EVERY frame, `CRC = rotl1(CRC) + term` (the decompile's
// `CRC*2 - (CRC>>31)` with an arithmetic shift) over
//   - every infantry   (+0x9C X / 10) + (+0xA0 Y / 10) << 16 + facing8,
//   - every unit       the same with BOTH facings (body + turret),
//   - every building   the same,
//   - the five map layers and the Logic list (X/10, Y/10, WhatAmI),
//   - one Scenario->Random draw (FUN_0065C1C0),
// where facing8 = ((DirStruct >> 7) + 1) >> 1 & 0xFF (FUN_004C92F0). It costs
// microseconds and never allocates.
//
// The clone keeps that shape (rotl1 + term, objects in array order, then the
// RNG) and is deliberately STRONGER, because the clone's sim is not the
// original's and real divergences must be caught quickly (brief W9 step 1):
//   * positions are folded as the exact cell AND the bit pattern of the
//     continuous position (fx/fy), so a sub-lepton drift is caught at once;
//   * facing (8-way, body, turret), health, target / mission handles, cargo,
//     harvester load;
//   * buildings: type, owner, origin, health, state;
//   * diplomacy: the one-way alliance matrix, alliance teams, the lobby
//     diplomacy rules, game over / observer state (w9fix-match);
//   * every house's economy: wallet, the legacy credit pool, every
//     production slot (type / stage / progress / spent), defeat / outcome;
//   * projectiles in flight (position, target), crates;
//   * EVERY sim RNG stream's state (crate R250 table position + words,
//     projectile / combat / ore / bridge / air-naval xorshifts, each AI
//     house's base / team / trigger stream) -- PEEKED, never drawn: the
//     original's draw advances the shared Randomizer on every peer alike, the
//     clone's streams are per subsystem and must not move outside the sim;
//   * the sim frame clocks.
// DEVIATION (documented): the original draws the RNG inside the CRC; the clone
// folds the RNG states instead (identical detection power, no sim side effect).
//
// Self-contained on purpose: the input set is listed in ONE place
// (net_frame_crc.cpp) and every scalar goes through the canonical folds below,
// so hash-hardening rules (NaN canonicalisation, derived caches excluded --
// the w9fix-robust work) apply to it by construction. Derived scratch grids
// (occupancy, amphib/crush/deck caches) are NOT read.
//
// The full-World hash (fnv1a over save_world, the pre-W9 FRAMEINFO CRC) stays
// available for debugging: RA2.INI [Net] FullHash=yes (or RA2_NET_FULL_HASH=1)
// -- see GameScene::net_state_hash -- and the desync dump still writes the
// full save plus per-member hashes.

#include <cstdint>
#include <cstring>
#include <vector>

namespace ra2engine {

class World;

// The rolling fold. rotl1 + term exactly like FUN_0064D520.
struct FrameCrc {
    std::uint32_t v = 0;
    void add(std::uint32_t t) { v = ((v << 1) | (v >> 31)) + t; }
    void add_i(int x) { add(static_cast<std::uint32_t>(x)); }
    void add_u64(std::uint64_t x) {
        add(static_cast<std::uint32_t>(x));
        add(static_cast<std::uint32_t>(x >> 32));
    }
    // Canonical double: every NaN folds as the one quiet NaN, -0 as +0 (the
    // bit patterns a different FPU / compiler may legally produce for the
    // same value). Everything else folds its exact IEEE bits.
    void add_d(double d) {
        std::uint64_t b = 0;
        if (d != d) b = 0x7FF8000000000000ull;
        else if (d == 0.0) b = 0;
        else std::memcpy(&b, &d, sizeof b);
        add_u64(b);
    }
};

// The per-frame CRC of the World's sim state (see above). `extra` is folded
// last (GameScene passes its sim-side FreeUnit tracker).
std::uint32_t net_frame_crc(const World& w, const std::vector<int>* extra = nullptr);

}  // namespace ra2engine
