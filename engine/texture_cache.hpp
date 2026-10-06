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

    // Upload an already-palettized RGBA buffer (`rgba` = w*h*4 bytes, R,G,B,A
    // memory order, alpha 0 = transparent) directly as a cached SDL_Texture,
    // keyed by `key`. Used by the voxel draw path, whose rasterizer already
    // produces final RGBA (VoxelSprite::rgba) rather than palette indices. On
    // cache hit returns the previously created texture. Trivial variant of the
    // indexed `get` above (skips the palettize loop).
    void* get_rgba(const std::string& key, const uint8_t* rgba, int w, int h);

    // Return the cached SDL_Texture* for `key`, or nullptr if not cached.
    // Lets callers skip decoding/palettizing work on a cache hit.
    // G-84/G-90: index `shadow_index` -> black at `alpha` (baked SHP shadows).
    void* get_shadowed(const std::string& key, const uint8_t* indices, int w, int h,
                       const ra2assets::Palette& pal, int shadow_index, int alpha);

    void* peek(const std::string& key) const;

private:
    void* renderer_;  // SDL_Renderer*, non-owning
    std::unordered_map<std::string, void*> cache_;  // key -> SDL_Texture*
};

}  // namespace ra2engine
