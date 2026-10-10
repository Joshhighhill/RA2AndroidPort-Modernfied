// engine/net/command_bridge.hpp -- NET L1: the clone's ra2engine::Command
// (engine/command.hpp) <-> the wire-level NetCommand (event_map.hpp), the
// entry points event_map.hpp reserved for L1.
//
// A Command carries a whole Whom run (the units that got the same order on the
// same click); the original sends one EventClass per object and the codec
// run-length groups consecutive MEGAMISSIONs (01-original-protocol.md §3.4).
// to_net() expands a Command into its records (one per Whom object, in order);
// from_net() re-assembles consecutive records of one house / frame / payload
// back into one Command, so a group move computes the same formation on every
// peer. The Chronosphere's two picks travel as SPECIAL_PLACE(ChronoSphere) +
// SPECIAL_PLACE(ChronoWarp) and are paired again.
//
// Identities: a unit / building TargetRef id is the clone's World index (low 24
// bits); a cell Target keeps the CellStruct packing (x | y << 16); the SW index
// is the original's binary index (SuperweaponType - 1); PLACE / PRODUCE HeapID
// is the clone's roster index (peers run the same rules -- the session's data
// hash check guarantees it).
#pragma once

#include <vector>

#include "command.hpp"
#include "net/event_map.hpp"

namespace ra2::net {

std::vector<NetCommand> to_net(const ra2engine::Command& c);
// Re-assembles a frame's records (any order of houses is fine; records of one
// house must keep their send order). Non-sim records are skipped.
std::vector<ra2engine::Command> from_net(const std::vector<NetCommand>& records);

}  // namespace ra2::net
