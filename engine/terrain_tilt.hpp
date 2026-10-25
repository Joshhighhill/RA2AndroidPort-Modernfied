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
    // Bounded, quantized presentation variants. Interpolate surface normals
    // then construct a rigid rotation; matrix lerp would squash the voxel.
    if(ramp>=1024 && ramp<1024+17*17*9) {
        const int key=ramp-1024,a=key/(17*9),b=(key/9)%17,step=key%9;
        const auto p=terrain_tilt(a).apply(0,0,1),q=terrain_tilt(b).apply(0,0,1);
        const double f=step/8.0;
        double x=p[0]+(q[0]-p[0])*f,y=p[1]+(q[1]-p[1])*f,z=p[2]+(q[2]-p[2])*f;
        const double length=std::sqrt(x*x+y*y+z*z);x/=length;y/=length;z/=length;
        const double v=1.0/(1.0+z);
        t.m={1-x*x*v,-x*y*v,x, -x*y*v,1-y*y*v,y, -x,-y,z};
        return t;
    }
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
inline int terrain_tilt_transition(int from,int to,double fraction) {
    from=from>=1 && from<=16?from:0;to=to>=1 && to<=16?to:0;
    if(from==to)return from;
    const int step=std::max(0,std::min(8,int(std::floor(fraction*8+0.5))));
    if(step==0)return from;if(step==8)return to;
    return 1024+(from*17+to)*9+step;
}
}
