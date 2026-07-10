#include "ra2assets/art.hpp"
#include <cstdlib>

namespace ra2assets {

Art Art::load(const Ini& ini) { return Art(ini); }

std::string Art::sequence_name(const std::string& image) const {
    return ini_->get(image, "Sequence", "");
}

ArtSequenceEntry Art::entry(const std::string& image, const std::string& name) const {
    ArtSequenceEntry e;
    const std::string seq = sequence_name(image);
    if (seq.empty()) return e;
    const std::string v = ini_->get(seq, name, "");
    if (v.empty()) return e;
    // Parse up to three leading comma-separated integers; later fields
    // (facing labels, sound tags -- e.g. "Idle1=56,15,0,S") are ignored.
    int out[3] = {0, 1, 0};
    std::size_t pos = 0;
    for (int i = 0; i < 3 && pos <= v.size(); ++i) {
        const std::size_t comma = v.find(',', pos);
        const std::string field = v.substr(pos, comma == std::string::npos ? std::string::npos
                                                                           : comma - pos);
        out[i] = std::atoi(field.c_str());
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    e.start = out[0];
    e.count = out[1] > 0 ? out[1] : 1;
    e.facing_mult = out[2];
    return e;
}

}  // namespace ra2assets
