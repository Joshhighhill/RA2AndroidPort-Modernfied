#pragma once
#include "ra2assets/ini.hpp"
#include <string>

namespace ra2assets {

// One [SequenceName] entry parsed from artmd.ini, e.g. GISequence's
// FireUp=164,6,6 -> {start=164, count=6, facing_mult=6}. Field 4+ (facing
// labels like "S"/"E", sound tags, etc.) are intentionally ignored -- YAGNI
// until the sim needs them. Defaults {0,1,0} mirror an absent/empty entry
// (a single frame at index 0, no facing variants).
struct ArtSequenceEntry {
    int start = 0;
    int count = 1;
    int facing_mult = 0;
};

// Thin, minimal reader for the artmd.ini fields the engine needs to resolve
// an infantry image's animation sequence: [image] Sequence= (the name of the
// [SequenceName] section) and that section's Start,Count,FacingMult entries
// (Ready=, Walk=, FireUp=, Die1=, ...). Still deliberately NOT a general
// art.ini model -- fields land only when the sim needs them (YAGNI).
//
// Art wraps an already-parsed Ini by reference (not by copy) to stay "thin"
// -- the caller must keep the Ini alive for at least as long as the Art
// object built from it. Mirrors Rules's load/by-reference lifetime contract
// (see rules.hpp).
class Art {
public:
    static Art load(const Ini& ini);

    // [image] Sequence=, the name of the [SequenceName] section describing
    // that image's animation frames, or "" if the image/key is absent.
    std::string sequence_name(const std::string& image) const;

    // Looks up `image`'s Sequence= section, then reads `name` (e.g. "Walk",
    // "FireUp", "Die1") from it as Start,Count,FacingMult. Tolerates extra
    // trailing comma-separated fields (facing labels, sound tags) and
    // embedded spaces around fields. Returns the {0,1,0} default if the
    // image, its sequence section, or the entry key is absent.
    ArtSequenceEntry entry(const std::string& image, const std::string& name) const;

    // [image] Voxel=, true iff the value is "yes"/"true" (case-insensitive);
    // false if absent. Distinguishes voxel-rendered units (vehicles) from
    // SHP-rendered ones.
    bool voxel(const std::string& image) const;

    // [image] Remapable=, true iff the value is "yes"/"true" (case-
    // insensitive); false if absent. Marks the image's house-color remap
    // (owner tint) as applicable.
    bool remapable(const std::string& image) const;

    // [image] PrimaryFireFLH=F,L,H (turret-relative Forward/Lateral/Height
    // muzzle offset) parsed into flh[0..2]. Returns false and leaves flh
    // untouched if the image/key is absent or doesn't have exactly 3
    // int-parseable comma-separated fields.
    bool fire_flh(const std::string& image, int flh[3]) const;

private:
    explicit Art(const Ini& ini) : ini_(&ini) {}
    const Ini* ini_;
};

} // namespace ra2assets
