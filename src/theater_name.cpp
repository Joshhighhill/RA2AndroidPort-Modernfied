#include "ra2assets/theater_name.hpp"
#include <cctype>

namespace ra2assets {

static std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

std::vector<TheaterCandidate> theater_candidates(const std::string& image,
                                                 char theater_letter,
                                                 const std::string& iso_mix,
                                                 const std::string& theater_mix,
                                                 const std::string& ext) {
    std::vector<TheaterCandidate> out;
    if (image.empty()) return out;
    std::string base = lower(image);
    // [RE-GATE: building-render.md §1] munge only when name[0] in {g,n,c,y}
    // and name[1] in {a,t} (FUN_005F8EA0's pattern check); candidate NAMES
    // are [themed, generic-'g'] (FUN_005F8F00's one 'G' retry), each searched
    // across the walk-order mixes; non-matching names stay literal.
    auto push_name = [&](const std::string& name) {
        out.push_back({iso_mix, name + ext});
        out.push_back({theater_mix, name + ext});
        out.push_back({"generic.mix", name + ext});
    };
    bool munge = base.size() >= 2 &&
                 (base[0] == 'g' || base[0] == 'n' || base[0] == 'c' || base[0] == 'y') &&
                 (base[1] == 'a' || base[1] == 't');
    if (munge) {
        std::string themed = base;
        themed[1] = (char)std::tolower((unsigned char)theater_letter);
        std::string generic = base;
        generic[1] = 'g';
        push_name(themed);
        push_name(generic);   // real theater letters T/A/U never equal 'g'
    } else {
        push_name(base);
    }
    return out;
}

} // namespace ra2assets
