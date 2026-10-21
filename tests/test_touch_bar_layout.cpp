#include "doctest.h"
#include "touch_bar_layout.hpp"

TEST_CASE("unit panel fits landscape viewports with separate header and group row") {
    using L = ra2engine::TouchBarLayout;
    for (int width : {320,480,640,800,1280}) for (int h : {520,548,720})
    for (int count : {13,16,17,18}) {
        const int top = L::top(h,count);
        CHECK(top >= 0);
        for (int i=0;i<count;++i) {
            const auto a=L::button(width,h,count,i);
            CHECK(a.x >= L::margin);
            CHECK(a.x+a.w <= width-L::margin);
            CHECK(a.y >= top+L::header_height);
            CHECK(a.y+a.h <= h-L::margin);
            CHECK(a.w >= 48);
            CHECK(a.h >= 44);
            for (int j=0;j<i;++j) {
                const auto b=L::button(width,h,count,j);
                CHECK_FALSE((a.x<b.x+b.w && b.x<a.x+a.w && a.y<b.y+b.h && b.y<a.y+a.h));
            }
        }
        CHECK(L::button(width,h,count,0).x == L::margin);
        if(count==13) {
            CHECK(L::button(width,h,count,8).y+44 < L::button(width,h,count,0).y);
            CHECK(L::button(width,h,count,12).x < 308);
        }
    }
}

TEST_CASE("panel arrow fits both states and clears the expanded action buttons") {
    using L=ra2engine::TouchBarLayout;
    for(int width : {320,480,800,1280}) for(int count : {13,17}) {
        const auto folded=L::toggle(width,548,count,true);
        CHECK(folded.x==L::margin);
        CHECK(folded.y+folded.h==548-L::margin);
        CHECK(folded.w==44);
        CHECK(folded.h==44);
        const auto open=L::toggle(width,548,count,false);
        CHECK(open.y>=L::top(548,count));
        CHECK(open.x+open.w<=width-L::margin);
        CHECK(open.y+open.h<=L::top(548,count)+L::header_height);
        for(int i=0;i<count;++i) CHECK(open.y+open.h<=L::button(width,548,count,i).y);
    }
}
