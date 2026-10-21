#pragma once
#include <algorithm>
namespace ra2engine {
struct TouchBarLayout {
    struct Rect { int x, y, w, h; };
    static constexpr int columns = 4, height = 56, gap = 4, margin = 8;
    static constexpr int panel_width = 300, header_height = 52;
    static int top(int viewport_height, int count) {
        const int content = count == 13 ? 164 : ((count + 3) / 4) * 60 - 4;
        return viewport_height - margin - content - header_height;
    }
    static Rect toggle(int viewport_width, int viewport_height, int count, bool collapsed) {
        if (collapsed) return {margin, viewport_height-margin-44, 44, 44};
        return {margin+std::min(panel_width,viewport_width-2*margin)-48,
                top(viewport_height,count)+4,44,44};
    }
    static Rect banner(int viewport_width, int panel_top) {
        return {margin,panel_top-52,std::min(panel_width,viewport_width-2*margin),48};
    }
    static Rect button(int viewport_width, int viewport_height, int count, int index) {
        const int pw = std::min(panel_width, viewport_width - 2 * margin);
        if (count == 13 && index >= 8) {
            const int w = (pw - 16) / 5;
            return {margin + (index - 8) * (w + gap), viewport_height - margin - 164,
                    w, 44};
        }
        const int rows = count == 13 ? 2 : (count + 3) / 4;
        const int w = (pw - 12) / 4;
        return {margin + (index % 4) * (w + gap),
                viewport_height - margin - (rows * 60 - 4) + (index / 4) * 60, w, height};
    }
};
// Capture complete instruction-card gestures before placement/map input.
struct TouchBannerCapture {
    enum Phase { Down, Up, Move, Pan, Other };
    enum Result { Pass, Consume, Cancel };
    bool held=false, cancel_held=false;
    static bool contains(const TouchBarLayout::Rect& r,int x,int y) {
        return x>=r.x && x<r.x+r.w && y>=r.y && y<r.y+r.h;
    }
    Result route(Phase phase,int x,int y,int x0,int y0,bool visible,
                 TouchBarLayout::Rect banner,TouchBarLayout::Rect cancel) {
        if(phase==Pan) return visible && contains(banner,x0,y0) ? Consume : Pass;
        if(phase==Down) {
            held=visible && contains(banner,x,y);
            cancel_held=held && contains(cancel,x,y);
            return held ? Consume : Pass;
        }
        if(phase==Up) {
            const bool was_held=held;
            const bool fire=held && cancel_held && visible && contains(cancel,x,y);
            held=cancel_held=false;
            return fire ? Cancel : was_held ? Consume : Pass;
        }
        return phase==Move && held ? Consume : Pass;
    }
};
}
