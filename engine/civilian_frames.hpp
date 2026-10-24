#pragma once
namespace ra2engine {
// Stock four-image civilian set: intact, damaged, occupied, rubble.
// Its shadow half is separate. Occupied + damaged must not select rubble.
inline int civilian_main_frame(bool damaged,bool occupied) {
    return damaged ? 1 : occupied ? 2 : 0;
}
}
