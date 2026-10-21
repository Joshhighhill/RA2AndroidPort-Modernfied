#include "doctest.h"
#include "touch_bar_layout.hpp"

TEST_CASE("touch shortcut dock stays inside the tactical viewport and below banners") {
    using L = ra2engine::TouchBarLayout;
    for (const int width : {480, 640, 800, 960, 1280}) {
        for (const int count : {9, 16, 17, 18}) {
            for (int i = 0; i < count; ++i) {
                const auto a = L::button(width, 548, count, i);
                CHECK(a.x >= L::margin);
                CHECK(a.x + a.w <= width - L::margin);
                CHECK(a.y >= L::top(548, count));
                CHECK(a.y + a.h <= 548 - L::margin);
                CHECK(a.w >= 48);
                CHECK(a.h == 56);
                for (int j = 0; j < i; ++j) {
                    const auto b = L::button(width, 548, count, j);
                    CHECK_FALSE((a.x < b.x + b.w && b.x < a.x + a.w &&
                                a.y < b.y + b.h && b.y < a.y + a.h));
                }
            }
            const auto first = L::button(width, 548, count, 0);
            const auto last = L::button(width, 548, count, 8);
            CHECK(std::abs(first.x - (width - last.x - last.w)) <= 1);
            // Expanding the extra row must not move the primary shortcuts.
            CHECK(first.x == L::button(width, 548, 9, 0).x);
            CHECK(first.y == L::button(width, 548, 9, 0).y);
        }
    }
}
