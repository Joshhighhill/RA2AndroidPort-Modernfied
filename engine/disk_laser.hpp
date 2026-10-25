#pragma once
#include <array>
#include <cmath>
namespace ra2engine {
// Vanilla DiskLaser radius, in cell space. Attachment is fixed at creation;
// this helper changes only the presentation of an already-authoritative shot.
inline std::array<double,2> disk_laser_rim(double dx,double dy) {
    const double length=std::hypot(dx,dy),radius=240.0/256.0;
    if(length<0.000001)return {radius,0};
    return {radius*dx/length,radius*dy/length};
}
}
