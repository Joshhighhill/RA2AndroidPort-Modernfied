#include "doctest.h"
#include "ra2assets/asset_fs.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>

namespace {
std::vector<uint8_t> mix(const std::string& name, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> bytes;
    auto put = [&](uint32_t n, int count) {
        for (int i = 0; i < count; ++i) bytes.push_back(uint8_t(n >> (8 * i)));
    };
    put(0, 4); put(1, 2); put(uint32_t(data.size()), 4);
    put(ra2assets::mix_id(name), 4); put(0, 4); put(uint32_t(data.size()), 4);
    bytes.insert(bytes.end(), data.begin(), data.end());
    return bytes;
}
}

TEST_CASE("Yuri art overrides classic duplicates only when requested") {
    const auto path = std::filesystem::temp_directory_path() /
        ("ra2-yr-art-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(path);
    auto write = [&](const char* name, const std::vector<uint8_t>& data) {
        std::ofstream f(path / name, std::ios::binary);
        f.write(reinterpret_cast<const char*>(data.data()), data.size());
        REQUIRE(f.good());
    };
    write("ra2.mix", mix("local.mix", mix("ltnk.vxl", {1})));
    write("ra2md.mix", mix("localmd.mix", mix("ltnk.vxl", {2})));
    const ra2assets::MixFile* owner = nullptr;
    std::string label;
    const ra2assets::MixEntry* entry = nullptr;
    {
    const ra2assets::AssetFS fs(path.string() + "/", nullptr);
    entry = fs.find("LTNK.VXL", &owner, &label);
    REQUIRE(entry);
    CHECK(owner->read(*entry) == std::vector<uint8_t>{1});
    entry = fs.find("ltnk.vxl", &owner, &label, false, true);
    REQUIRE(entry);
    CHECK(owner->read(*entry) == std::vector<uint8_t>{2});
    CHECK(label == "ra2md.mix/localmd.mix");
    entry = fs.find("ltnk.vxl", &owner, nullptr, true, true);
    REQUIRE(entry);
    CHECK(owner->read(*entry) == std::vector<uint8_t>{1});
    CHECK_FALSE(fs.find("missing.vxl", nullptr, nullptr, false, true));
    }
    std::filesystem::remove(path / "ra2md.mix");
    {
    const ra2assets::AssetFS classic(path.string() + "/", nullptr);
    entry = classic.find("ltnk.vxl", &owner, nullptr, false, true);
    REQUIRE(entry);
    CHECK(owner->read(*entry) == std::vector<uint8_t>{1});
    }
    std::filesystem::remove(path / "ra2.mix");
    std::filesystem::remove(path);
}

TEST_CASE("Yuri art lookup falls back to classic when expansion is absent") {
    // An empty install also exercises the empty-YR-range bounds.
    const ra2assets::AssetFS fs("build-tools/no-such-install/", nullptr);
    CHECK_FALSE(fs.find("ltnk.vxl", nullptr, nullptr, false, true));
}
