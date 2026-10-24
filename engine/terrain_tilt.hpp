#pragma once
#include <array>
#include <cmath>

namespace ra2engine {
// Presentation-only terrain orientation, in the voxel renderer's world axes.
// TMP ramp conventions: 1..4 edges, 5..12 corners, 13..16 full diagonals.
// Saddle ramps (17..20) have no single plane, so retain an upright average.
// Angles follow the public TS/RA2 ramp convention, pending original-matrix parity.
struct TerrainTilt {
    std::array<double,9> m{1,0,0,0,1,0,0,0,1};
    std::array<double,3> apply(double x,double y,double z) const {
        return {m[0]*x+m[1]*y+m[2]*z,m[3]*x+m[4]*y+m[5]*z,m[6]*x+m[7]*y+m[8]*z};
    }
    std::array<double,3> inverse(double x,double y,double z) const {
        return {m[0]*x+m[3]*y+m[6]*z,m[1]*x+m[4]*y+m[7]*z,m[2]*x+m[5]*y+m[8]*z};
    }
};
inline TerrainTilt terrain_tilt(int ramp) {
    TerrainTilt t;
    if(ramp<1 || ramp>16) return t;
    double dx=0,dy=0;
    if(ramp<=4) {
        if(ramp==1) dx=1;
        if(ramp==2) dy=-1;
        if(ramp==3) dx=-1;
        if(ramp==4) dy=1;
    } else {
        const int q=(ramp-5)%4;
        dx=(q==0 || q==3) ? 1 : -1;
        dy=(q==0 || q==1) ? -1 : 1;
        dx/=std::sqrt(2.0);dy/=std::sqrt(2.0);
    }
    const double angle=(ramp>=5 && ramp<=12 ? 16.875 : 22.5)*3.14159265358979323846/180;
    // Rodrigues rotation taking the upright axis toward the downhill normal.
    const double ax=dy,ay=-dx,c=std::cos(angle),s=std::sin(angle),v=1-c;
    t.m={c+ax*ax*v,ax*ay*v,ay*s, ax*ay*v,c+ay*ay*v,-ax*s, -ay*s,ax*s,c};
    return t;
}
}
