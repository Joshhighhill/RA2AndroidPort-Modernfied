#pragma once
#include "projectile.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <tuple>
#include <vector>

namespace ra2engine {
// Previous/current authoritative coordinates, owned only by presentation.
// In particular the first interval begins at the actual Launch event, not at
// the first already-integrated bullet position. No extrapolation or new RNG.
class ProjectilePresentation {
    using Key=std::tuple<int,int,int,int,int,std::string,int>;
    struct Segment { std::array<double,3> from,to; };
    std::map<Key,Segment> live_;
    std::vector<Segment> draw_;
    static Key key(const Projectile& p,int ordinal) {
        return {p.attacker,p.attacker_building,p.launch_frame,p.weapon_slot,p.uid,p.weapon_id,ordinal};
    }
    static std::array<double,3> pos(const Projectile& p) {
        return {static_cast<double>(p.x),static_cast<double>(p.y),static_cast<double>(p.z)};
    }
public:
    void clear() { live_.clear();draw_.clear(); }
    void capture(const std::vector<Projectile>& bullets,const std::vector<ProjectileEvent>& events) {
        std::map<Key,Segment> next;
        std::map<Key,int> ordinals;
        std::vector<bool> used(events.size(),false);
        draw_.clear();draw_.reserve(bullets.size());
        for(const auto& p:bullets) {
            const auto k=key(p,ordinals[key(p,0)]++);
            Segment s{pos(p),pos(p)};
            // Ballistic salvos can share a launch frame and have no unique
            // simulation UID. If their first bullet disappears, vector order
            // changes. Match the remaining endpoints by the current velocity,
            // not by their old ordinal, so no survivor jumps to its sibling.
            const auto base=key(p,0);
            auto old=live_.end();double nearest=std::numeric_limits<double>::max();
            for(auto candidate=live_.lower_bound(base);candidate!=live_.end();++candidate) {
                auto group=candidate->first;std::get<6>(group)=0;
                if(group!=base)break;
                const auto& q=candidate->second.to;
                const double dx=s.to[0]-p.vx-q[0],dy=s.to[1]-p.vy-q[1],dz=s.to[2]-p.vz-q[2];
                const double d=dx*dx+dy*dy+dz*dz;
                if(d<nearest){nearest=d;old=candidate;}
            }
            if(old!=live_.end()) {s.from=old->second.to;live_.erase(old);}
            else {
                // A salvo may contain several launches from one firer. Match
                // each event once, nearest to its integrated bullet position.
                std::size_t best=events.size();double dist=std::numeric_limits<double>::max();
                for(std::size_t i=0;i<events.size();++i) {
                    const auto& e=events[i];
                    if(used[i] || e.kind!=ProjectileEvent::Launch || e.frame!=p.launch_frame || e.attacker!=p.attacker)continue;
                    const double dx=s.to[0]-e.x,dy=s.to[1]-e.y,dz=s.to[2]-e.z;
                    const double d=dx*dx+dy*dy+dz*dz;
                    if(d<dist){dist=d;best=i;}
                }
                if(best<events.size()) {
                    used[best]=true;const auto& e=events[best];
                    s.from={static_cast<double>(e.x),static_cast<double>(e.y),static_cast<double>(e.z)};
                }
            }
            next.emplace(k,s);draw_.push_back(s);
        }
        live_=std::move(next); // retired bullets cannot accumulate in the cache
    }
    std::array<double,3> sample(std::size_t i,const Projectile& fallback,double fraction) const {
        if(i>=draw_.size())return pos(fallback);
        const auto& s=draw_[i];
        const double t=std::isfinite(fraction)?std::clamp(fraction,0.0,1.0):0.0;
        return {s.from[0]+(s.to[0]-s.from[0])*t,s.from[1]+(s.to[1]-s.from[1])*t,
                s.from[2]+(s.to[2]-s.from[2])*t};
    }
    std::size_t size() const { return live_.size(); }
};
}
