#pragma once
#include <string>
#include <vector>

namespace ra2assets {

// A single candidate for resolving a NewTheater art name: try `file` inside
// nested mix `mix` (both lowercase, mix name relative to ra2.mix).
struct TheaterCandidate {
    std::string mix;   // e.g. "generic.mix"
    std::string file;  // e.g. "ggcnst.shp"
};

// NewTheater=yes filename resolution [RE-GATE APPLIED: building-render.md §1,
// recovered from FUN_005F8860/FUN_005F8EA0/FUN_005F8F00]. The munge applies
// ONLY when name[0] is one of {g,n,c,y} AND name[1] is one of {a,t} (case-
// insensitive): name[1] is replaced by the theater letter ('T' temperate,
// 'A' snow, 'U' urban); the lookup is a GLOBAL search over the loaded mixes;
// on miss, ONE retry with name[1]='G' (the generic art). There is NO third
// literal candidate for munged names (snow's letter IS 'A', so a literal
// spelling like "gacnst" is already the snow-themed name). Names that fail
// the pattern check get the literal name only.
// `image` is the artmd image/section name (e.g. "GACNST"); `ext` includes the
// dot (".shp"). iso_mix/theater_mix define the engine's walk ORDER over its
// open nested mixes (the binary searches all loaded mixes; we search these
// three per name): for each candidate NAME in order [themed, generic-'g']
// (or just [literal] for non-matching names), emit
//   { iso_mix, name+ext }, { theater_mix, name+ext }, { "generic.mix", name+ext }
// i.e. 6 entries for munged names, 3 for literal ones. Pinned real
// locations this must reproduce: ggcnst->generic.mix, gacnst->snow.mix,
// gtcnstmk->isotemp.mix.
// theater_mix: "temperat.mix" | "snow.mix" | "urban.mix".
std::vector<TheaterCandidate> theater_candidates(const std::string& image,
                                                 char theater_letter,
                                                 const std::string& iso_mix,
                                                 const std::string& theater_mix,
                                                 const std::string& ext);

} // namespace ra2assets
