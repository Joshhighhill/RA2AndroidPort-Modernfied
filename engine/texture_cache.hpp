#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

#include "ra2assets/pal.hpp"

namespace ra2engine {

// Palettizes indexed (8-bit) image buffers into RGBA and caches the result
// as an SDL_Texture, keyed by an asset+frame identifier string. Kept
// SDL-free in the header (renderer/texture handles are passed/returned as
// void*) so this header doesn't force SDL.h on every includer.
class TextureCache {
public:
    explicit TextureCache(void* sdl_renderer);
    ~TextureCache();

    TextureCache(const TextureCache&) = delete;
    TextureCache& operator=(const TextureCache&) = delete;

    // Palettize indexed w*h -> RGBA (index 0 transparent) -> cached
    // SDL_Texture. key identifies the asset+frame. Returns an SDL_Texture*
    // (as void*); on cache hit, returns the previously created texture.
    void* get(const std::string& key, const uint8_t* indices, int w, int h,
              const ra2assets::Palette& pal);

private:
    void* renderer_;  // SDL_Renderer*, non-owning
    std::unordered_map<std::string, void*> cache_;  // key -> SDL_Texture*
};

}  // namespace ra2engine
