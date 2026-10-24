#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <algorithm>
#include <vector>
#include "voxel_raster.hpp"

namespace ra2engine {
// CPU raster cache. References survive every insertion within a render frame;
// eviction happens only at the next frame boundary, before drawing any part.
// Current and previous frame working sets may exceed the soft budget. Keeping
// recently drawn parts also avoids rerasterizing a busy scene every frame.
class VoxelAnimationCache {
public:
    explicit VoxelAnimationCache(std::size_t budget = 8u << 20) : budget_(budget) {}
    void begin_frame(std::uint64_t renderer, std::uint64_t frame) {
        if (started_ && renderer == renderer_ && frame == frame_) return;
        started_=true;renderer_=renderer;frame_=frame;++epoch_;
        if (bytes_ > budget_) {
            std::vector<decltype(entries_)::iterator> order;
            order.reserve(entries_.size());
            for(auto it=entries_.begin();it!=entries_.end();++it) order.push_back(it);
            std::stable_sort(order.begin(),order.end(),[](auto a,auto b){return a->second.used<b->second.used;});
            for(auto it:order) {
                if(bytes_<=budget_) break;
                if(it->second.used >= epoch_-1) break;
                bytes_-=it->second.bytes;entries_.erase(it);++evicted_;
            }
        }
    }
    const VoxelSprite* find(const std::string& key) {
        const auto it=entries_.find(key);
        if(it==entries_.end()) return nullptr;
        it->second.used=epoch_;return &it->second.sprite;
    }
    const VoxelSprite& insert(const std::string& key,VoxelSprite sprite) {
        if(const auto* hit=find(key)) return *hit;
        const std::size_t bytes=sizeof(Entry)+key.capacity()+sprite.rgba.capacity();
        auto it=entries_.emplace(key,Entry{std::move(sprite),epoch_,bytes}).first;
        bytes_+=bytes;return it->second.sprite;
    }
    std::size_t bytes() const { return bytes_; }
    std::size_t size() const { return entries_.size(); }
    std::size_t evicted() const { return evicted_; }
private:
    struct Entry { VoxelSprite sprite;std::uint64_t used;std::size_t bytes; };
    std::map<std::string,Entry> entries_;
    std::size_t budget_,bytes_=0,evicted_=0;
    std::uint64_t renderer_=0,frame_=0,epoch_=0;
    bool started_=false;
};
}
