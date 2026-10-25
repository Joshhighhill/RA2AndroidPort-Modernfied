#pragma once
#include "sidebar.hpp"
#include <algorithm>
namespace ra2engine {
inline SbRect fit_skirmish_image(int w,int h,SbRect box) {
    if(w<=0 || h<=0 || box.w<=0 || box.h<=0)return {box.x,box.y,0,0};
    const double scale=std::min(double(box.w)/w,double(box.h)/h);
    const int dw=std::max(1,int(w*scale)),dh=std::max(1,int(h*scale));
    return {box.x+(box.w-dw)/2,box.y+(box.h-dh)/2,dw,dh};
}
inline SbRect skirmish_marker(int x,int y,SbRect image) {
    const int w=std::min(15,image.w),h=std::min(17,image.h);
    if(w<=0 || h<=0)return {image.x,image.y,0,0};
    return {std::clamp(x-w/2,image.x,image.x+image.w-w),
            std::clamp(y-h/2,image.y,image.y+image.h-h),w,h};
}
}
