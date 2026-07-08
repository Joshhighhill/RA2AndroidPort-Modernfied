#include "texture_cache.hpp"

#include <SDL.h>
#include <stdexcept>
#include <vector>

namespace ra2engine {

TextureCache::TextureCache(void* sdl_renderer) : renderer_(sdl_renderer) {}

TextureCache::~TextureCache() {
    for (auto& kv : cache_) {
        SDL_DestroyTexture(static_cast<SDL_Texture*>(kv.second));
    }
}

void* TextureCache::get(const std::string& key, const uint8_t* indices, int w,
                         int h, const ra2assets::Palette& pal) {
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        return it->second;
    }

    std::vector<ra2assets::Rgba> rgba(static_cast<size_t>(w) * h);
    for (int i = 0; i < w * h; ++i) {
        uint8_t idx = indices[i];
        if (idx == 0) {
            rgba[i] = ra2assets::Rgba{0, 0, 0, 0};
        } else {
            ra2assets::Rgba c = pal[idx];
            c.a = 255;
            rgba[i] = c;
        }
    }

    // ra2assets::Rgba is a tightly packed {r,g,b,a} byte struct, so its
    // in-memory byte order is always R,G,B,A regardless of host endianness.
    // SDL_PIXELFORMAT_RGBA32 is SDL's "byte array" alias that resolves to
    // whichever packed format matches that R,G,B,A memory order on the
    // current platform (SDL_PIXELFORMAT_ABGR8888 on little-endian, which is
    // what both Windows desktop and Android targets use). Using the *32
    // alias keeps this correct without hardcoding the little-endian-only
    // ABGR8888 constant.
    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(renderer_);
    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                          SDL_TEXTUREACCESS_STATIC, w, h);
    if (!tex) {
        throw std::runtime_error(SDL_GetError());
    }

    SDL_UpdateTexture(tex, nullptr, rgba.data(), w * 4);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    cache_.emplace(key, tex);
    return tex;
}

}  // namespace ra2engine
