#include "maplist.hpp"
#include "ra2assets/pkt.hpp"
#include "ra2assets/fileio.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace ra2engine {
namespace {

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// CSF values are UTF-16. Every stock map name is ASCII; narrow to char and
// replace any non-ASCII code unit with '?' rather than mangling the string.
std::string narrow(const std::u16string& u) {
    std::string out;
    out.reserve(u.size());
    for (char16_t c : u) out.push_back(c < 0x80 ? (char)c : '?');
    return out;
}

}  // namespace

std::vector<MapEntry> build_map_catalog(const std::string& pkt_text,
                                        const ra2assets::MixFile& maps,
                                        const ra2assets::CsfStrings* csf,
                                        const std::string& mode_tag) {
    std::vector<MapEntry> out;
    const ra2assets::PktFile pkt = ra2assets::PktFile::parse(pkt_text);   // step 1
    for (const ra2assets::PktMapEntry& p : pkt.entries()) {
        if (maps.find(p.file_name) == nullptr) continue;                  // step 2
        if (!mode_tag.empty() && !p.has_mode(mode_tag)) continue;         // step 3
        MapEntry e;
        e.file_name   = p.file_name;
        e.base_name   = p.base_name;
        e.csf_label   = p.description;
        e.game_modes  = p.game_modes;
        e.min_players = p.min_players;
        e.max_players = p.max_players;
        e.display_name = p.base_name;                                     // step 4
        if (csf && !p.description.empty()) {
            if (const std::u16string* v = csf->find(p.description)) {
                const std::string n = narrow(*v);
                if (!n.empty()) e.display_name = n;
            }
        }
        out.push_back(std::move(e));
    }
    std::stable_sort(out.begin(), out.end(),                              // step 5
                     [](const MapEntry& a, const MapEntry& b) {
                         const std::string la = lower(a.display_name), lb = lower(b.display_name);
                         if (la != lb) return la < lb;
                         return a.file_name < b.file_name;
                     });
    return out;
}

MapCatalog load_yr_map_catalog(const std::string& dir, const std::string& mode_tag) {
    // maps.md §1: the YR multiplayer maps live in multimd.mix; the manifest
    // (missionsmd.pkt) and the string table (ra2md.csf) live in langmd.mix.
    ra2assets::MixFile maps = ra2assets::MixFile::open(ra2assets::read_file(dir + "multimd.mix"));
    ra2assets::MixFile lang = ra2assets::MixFile::open(ra2assets::read_file(dir + "langmd.mix"));

    const ra2assets::MixEntry* pe = lang.find(std::string("missionsmd.pkt"));
    if (!pe) throw std::runtime_error("missionsmd.pkt not found in langmd.mix");
    const auto praw = lang.read(*pe);
    const std::string pkt_text(praw.begin(), praw.end());

    // CSF is OPTIONAL: without it every display_name falls back to the pkt
    // base name, which still yields a usable (if ugly) list.
    ra2assets::CsfStrings csf;
    bool have_csf = false;
    if (const ra2assets::MixEntry* ce = lang.find(std::string("ra2md.csf"))) {
        try {
            const auto craw = lang.read(*ce);
            csf = ra2assets::CsfStrings::load(craw.data(), craw.size());
            have_csf = true;
        } catch (const std::exception&) { have_csf = false; }
    }

    MapCatalog cat{build_map_catalog(pkt_text, maps, have_csf ? &csf : nullptr, mode_tag),
                   std::move(maps)};
    return cat;
}

}  // namespace ra2engine
