#pragma once
#include <algorithm>
namespace ra2engine {
// A visual transition never changes simulation state or command timing.
inline bool transition_active(long start,long duration,long now) {
    return duration>0 && now>=start && now-start<duration;
}
inline int packing_frame(int frames,long start,long duration,long now) {
    if(frames<=0 || !transition_active(start,duration,now)) return -1;
    return frames-1-std::min<long>(frames-1,(now-start)*frames/duration);
}
// Opening uses Special/SpecialTwo, closing uses the art-defined reverse
// SpecialThree/SpecialFour; steady state uses the opening animation's loop.
inline int bunker_animation_slot(bool front,int transition) {
    return transition<0 ? (front?12:13) : (front?10:11);
}
}
