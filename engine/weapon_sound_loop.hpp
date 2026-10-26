#pragma once
#include <cstdint>
#include <string>
namespace ra2engine {
// A Gatling loop survives the short muzzle-flash gap between charged shots.
inline bool weapon_loop_active(bool enabled,bool target,double flash,bool staged,int charge) {
    return enabled && target && (flash>0 || (staged && charge>0));
}

// Building cell attacks have no unit/building target pointer. Keep the
// report controller alive for those orders too, but never through low power.
inline bool building_weapon_loop_active(bool enabled,bool offline,bool target,
                                       int order_kind,double flash,bool staged,int charge) {
    return weapon_loop_active(enabled && !offline,target || order_kind==3,flash,staged,charge);
}

// One report loop per shooter. Audio owns attenuation and handle lifetime.
struct WeaponSoundLoop {
    std::uint32_t handle=0;
    std::string key;
    template<class Audio> void shot(Audio& audio,const std::string& report,double x,double y) {
        if (!audio.sound_loops(report)) {
            if (handle) audio.stop_loop(handle);
            handle=0;key.clear();audio.play_at(report,x,y,"report");return;
        }
        if (key!=report) { if(handle)audio.stop_loop(handle);handle=0;key=report; }
        if (!handle) handle=audio.loop_at(report,x,y);
    }
    template<class Audio> void update(Audio& audio,bool firing,double x,double y) {
        if (!handle) return;
        if (!firing) { audio.stop_loop(handle);handle=0;key.clear(); }
        else if (!audio.update_loop(handle,x,y)) { handle=0;key.clear(); }
    }
};
}
