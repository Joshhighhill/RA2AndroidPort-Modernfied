#define SDL_MAIN_HANDLED
#include "terrain_blit.hpp"
#include "viewport_bounds.hpp"
#include <cstdio>
#include <cstdlib>
static void check(bool ok,const char* message) {
    if(!ok) {std::fprintf(stderr,"FAIL: %s (%s)\n",message,SDL_GetError());std::exit(1);}
}
int main() {
    SDL_SetMainReady();check(SDL_Init(0)==0,"SDL initialization");
    auto* surface=SDL_CreateRGBSurfaceWithFormat(0,8,8,32,SDL_PIXELFORMAT_RGBA32);
    check(surface!=nullptr,"surface");
    auto* renderer=SDL_CreateSoftwareRenderer(surface);check(renderer!=nullptr,"software renderer");
    auto* image=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,2,2);
    check(image!=nullptr,"texture");
    const Uint8 pixels[]={0,0,0,0, 200,100,50,255, 200,100,50,255, 0,0,0,0};
    check(SDL_UpdateTexture(image,nullptr,pixels,8)==0,"upload");
    SDL_SetTextureBlendMode(image,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer,20,40,180,255);SDL_RenderClear(renderer);
    ra2engine::blit_terrain(renderer,image,SDL_Rect{0,0,2,2},true);
    ra2engine::blit_terrain(renderer,image,SDL_Rect{4,0,2,2},false);
    Uint8 result[8*8*4];check(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_RGBA32,result,8*4)==0,"readback");
    std::printf("fog_rgb=%u,%u,%u background_rgb=%u,%u,%u\n",result[4],result[5],result[6],result[0],result[1],result[2]);
    check(result[0]==20 && result[1]==40 && result[2]==180,"fog preserves transparent corner background");
    check(result[4]>=99 && result[4]<=100 && result[5]>=49 && result[5]<=50 && result[6]>=24 && result[6]<=25,"fog dims opaque terrain pixels");
    const int plain=(0*8+5)*4;
    check(result[plain]==200 && result[plain+1]==100 && result[plain+2]==50,"fog restores texture modulation for later tiles");
    check(!ra2engine::viewport_intersects(0,-30,2,2,8,8),"offscreen base");
    check(ra2engine::viewport_intersects(0,0,2,2,8,8),"visible cliff extension");
    SDL_DestroyTexture(image);SDL_DestroyRenderer(renderer);SDL_FreeSurface(surface);SDL_Quit();
    std::puts("PASS: software pixel readback: transparent corners, fog tint, shared texture restoration, independent cliff bounds");
}
