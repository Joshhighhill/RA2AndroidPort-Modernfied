#pragma once
#include <SDL.h>
namespace ra2engine {
inline void blit_terrain(SDL_Renderer* renderer, SDL_Texture* image, const SDL_Rect& rect, bool dim) {
    Uint8 r=255,g=255,b=255;
    if (dim) {
        SDL_GetTextureColorMod(image,&r,&g,&b);
        SDL_SetTextureColorMod(image,r/2,g/2,b/2);
    }
    SDL_RenderCopy(renderer,image,nullptr,&rect);
    if (dim) SDL_SetTextureColorMod(image,r,g,b);
}
}
