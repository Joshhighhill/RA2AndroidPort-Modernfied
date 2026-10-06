// P7 Save / Load [RE: docs/re/saveload.md]. See savegame.hpp for the format.
#include "savegame.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <system_error>

#include "save_fields.hpp"

namespace ra2engine {
namespace save {

namespace {
constexpr char kMagic[8] = {'R', 'A', '2', 'S', 'A', 'V', 'E', '\0'};

template <class T> void put(Writer& w, T v) { io(w, v); }
}  // namespace

std::vector<std::uint8_t> save_world(const World& w) {
    Writer wr;
    // The Writer only reads; const_cast lets save and load share one visitor.
    SaveAccess::world(wr, const_cast<World&>(w));
    return std::move(wr.buf);
}

bool load_world(World& w, const std::vector<std::uint8_t>& bytes) {
    Reader r(bytes);
    SaveAccess::world(r, w);
    return r.ok() && r.at_end();
}

std::vector<std::uint8_t> save_triggers(const MapTriggerSystem& s) {
    Writer wr;
    SaveAccess::triggers(wr, const_cast<MapTriggerSystem&>(s));
    return std::move(wr.buf);
}

bool load_triggers(MapTriggerSystem& s, const std::vector<std::uint8_t>& bytes) {
    Reader r(bytes);
    SaveAccess::triggers(r, s);
    return r.ok() && r.at_end();
}

std::uint64_t fnv1a(const std::vector<std::uint8_t>& bytes, std::uint64_t h) {
    for (std::uint8_t b : bytes) {
        h ^= b;
        h *= 0x100000001b3ull;
    }
    return h;
}

// --- container -----------------------------------------------------------------
std::vector<std::uint8_t> encode_save(const SaveFile& f) {
    Writer w;
    w.bytes(kMagic, 8);
    std::uint32_t version = kFormatVersion;
    w.bytes(&version, 4);
    SaveHeader h = f.header;
    h.description = clean_description(h.description);
    std::vector<std::uint8_t> hb = to_bytes(h);
    io(w, hb);
    std::uint64_t n = f.sections.size();
    io(w, n);
    for (const auto& kv : f.sections) {
        std::uint32_t t = kv.first;
        w.bytes(&t, 4);
        std::vector<std::uint8_t> body = kv.second;
        io(w, body);
    }
    return std::move(w.buf);
}

bool decode_save(const std::vector<std::uint8_t>& bytes, SaveFile& out, bool header_only) {
    out = SaveFile{};
    Reader r(bytes);
    char magic[8];
    r.bytes(magic, 8);
    if (!r.ok() || std::memcmp(magic, kMagic, 8) != 0) return false;
    std::uint32_t version = 0;
    r.bytes(&version, 4);
    if (!r.ok()) return false;
    std::vector<std::uint8_t> hb;
    io(r, hb);
    if (!r.ok()) return false;
    {
        // The header block is length-prefixed, so it is skippable even when its
        // layout is from another version; a wrong-version header that fails to
        // parse just shows an empty description.
        SaveHeader h;
        Reader hr(hb);
        io(hr, h);
        if (hr.ok()) out.header = h;
        out.header.version = version;
    }
    if (version != kFormatVersion || header_only) return true;
    std::uint64_t n = 0;
    io(r, n);
    for (std::uint64_t i = 0; i < n && r.ok(); ++i) {
        std::uint32_t t = 0;
        r.bytes(&t, 4);
        std::vector<std::uint8_t> body;
        io(r, body);
        if (r.ok()) out.sections[t] = std::move(body);
    }
    return r.ok() && r.at_end();
}

bool write_file(const std::string& path, const std::vector<std::uint8_t>& bytes) {
    // Write-then-rename so a crash mid-save never leaves a torn file in place
    // of a good one.
    const std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!f) return false;
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        std::filesystem::remove(path, ec);
        ec.clear();
        std::filesystem::rename(tmp, path, ec);
    }
    return !ec;
}

bool read_file_bytes(const std::string& path, std::vector<std::uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    const std::streamoff n = f.tellg();
    if (n < 0) return false;
    f.seekg(0, std::ios::beg);
    out.resize(static_cast<std::size_t>(n));
    if (n) f.read(reinterpret_cast<char*>(out.data()), n);
    return static_cast<bool>(f);
}

// --- slots ---------------------------------------------------------------------
namespace {
std::string upper(std::string s) {
    for (char& c : s)
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return s;
}
}  // namespace

std::vector<SlotEntry> list_saves(const std::string& dir) {
    std::vector<SlotEntry> out;
    std::error_code ec;
    if (dir.empty() || !std::filesystem::is_directory(dir, ec)) return out;
    for (const auto& de : std::filesystem::directory_iterator(dir, ec)) {
        if (!de.is_regular_file(ec)) continue;
        const std::string name = de.path().filename().string();
        if (name.size() < 4 || upper(name.substr(name.size() - 4)) != ".SAV") continue;
        std::vector<std::uint8_t> bytes;
        if (!read_file_bytes(de.path().string(), bytes)) continue;
        SaveFile f;
        if (!decode_save(bytes, f, /*header_only=*/true)) continue;
        SlotEntry e;
        e.path = de.path().string();
        e.file_name = name;
        e.header = f.header;
        e.wrong_version = (f.header.version != kFormatVersion);
        const auto t = std::filesystem::last_write_time(de.path(), ec);
        e.mtime = ec ? 0 : static_cast<std::int64_t>(t.time_since_epoch().count());
        out.push_back(std::move(e));
    }
    // FileEntryClass::Compare: newest first; name breaks ties deterministically.
    std::sort(out.begin(), out.end(), [](const SlotEntry& a, const SlotEntry& b) {
        if (a.mtime != b.mtime) return a.mtime > b.mtime;
        return a.file_name < b.file_name;
    });
    return out;
}

std::string new_save_name(const std::string& dir, std::uint32_t seed) {
    // SAVE%04lX.%3s with rand() retried until free (saveload.md §2). xorshift32
    // stands in for the CRT rand(); seed 0 is remapped (xorshift fixed point).
    std::uint32_t s = seed ? seed : 0x5A17u;
    for (int tries = 0; tries < 70000; ++tries) {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        char buf[16];
        std::snprintf(buf, sizeof buf, "SAVE%04X.SAV", static_cast<unsigned>(s & 0xFFFFu));
        std::error_code ec;
        const std::filesystem::path p = std::filesystem::path(dir) / buf;
        if (!std::filesystem::exists(p, ec)) return buf;
    }
    return "SAVE0000.SAV";
}

bool delete_save(const std::string& path) {
    std::error_code ec;
    return std::filesystem::remove(path, ec) && !ec;
}

std::string clean_description(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (static_cast<unsigned char>(c) < 0x20) continue;
        out += c;
        if (out.size() >= kMaxDescription) break;
    }
    return out;
}

}  // namespace save
}  // namespace ra2engine
