#pragma once
#include <atomic>
#include <cstdint>
#include <string>
namespace ra2assets {
using AssetTrace=void(*)(const char*,const std::string&,const std::string&,std::uint64_t);
inline std::atomic<AssetTrace>& asset_trace_callback(){static std::atomic<AssetTrace> fn{nullptr};return fn;}
inline void trace_asset(const char* kind,const std::string& name,const std::string& source,std::uint64_t size){
    if(auto fn=asset_trace_callback().load(std::memory_order_relaxed))fn(kind,name,source,size);
}
}
