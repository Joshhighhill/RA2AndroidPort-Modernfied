#pragma once
#include <algorithm>
namespace ra2engine {
// Opening runs Start..LoopEnd before holding; closing uses its reverse span.
inline long bunker_motion_ticks(int start,int loop_end,int delay,bool reverse) {
    const long frames=reverse?loop_end:loop_end-start;
    return std::clamp<long>(frames*std::max(1,delay),1,900);
}
inline double transition_blend(long age,long duration,double sub_tick) {
    const double t=std::clamp((age+std::clamp(sub_tick,0.0,1.0))/std::max<long>(1,duration),0.0,1.0);
    return t*t*(3.0-2.0*t);
}
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
