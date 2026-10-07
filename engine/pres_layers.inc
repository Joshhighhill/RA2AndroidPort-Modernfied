// pres_layers.inc -- W5-PRES R-100 / R-101: render layers for the real game.
// Included by main.cpp after pres_map.inc (needs g_map_dressing).
//
// Ground layer: one depth sort (key = cell x + y of the object's centre) over
// buildings, map objects, ground units AND the high-bridge decks:
//   * a deck at cell (x, y) sorts at x + y + kPresDeckBias, after every ground
//     object in or behind its cell -> a ship / unit UNDER the deck is covered
//     by the deck above it and drawn over the decks behind it;
//   * a ground unit ON a deck (a non-naval unit on a deck cell over water)
//     draws BridgeHeight higher and sorts at fx + fy + kPresOnDeckBias, after
//     its own deck and before the next one in front.
// Air layer: airborne aircraft / jumpjets draw after the whole ground layer
// (sorted by fx + fy among themselves), so a plane over a building is never
// hidden behind it. [RE: docs/re/w5-presentation.md §1-2]

constexpr double kPresDeckBias = 0.99;
constexpr double kPresOnDeckBias = 1.001;
// BridgeHeight = 416 leptons = 4 cell levels (104 leptons / 15 px each)
// [RE: world-depth.md B3; docs/re/w5-presentation.md §1].
constexpr int kPresBridgeLevels = 4;

inline bool pres_airborne(const ra2engine::Unit& u) { return u.is_aircraft && u.altitude > 0.0; }

// A ground unit standing on a high-bridge deck.
inline bool pres_on_high_deck(const ra2engine::World& w, const ra2engine::Unit& u) {
    if (!g_map_dressing || !g_map_dressing->layer_decks()) return false;
    if (u.is_naval || u.is_aircraft || u.cargo_of >= 0) return false;
    if (!g_map_dressing->deck_cell(u.cell_x, u.cell_y)) return false;
    if (pres_high_deck_destroyed(w, u.cell_x, u.cell_y)) return false;
    const std::vector<std::uint8_t>& wg = w.grid_for(ra2engine::MovementDomain::Water);
    const std::size_t i = static_cast<std::size_t>(u.cell_y) * w.grid_w() + u.cell_x;
    return u.cell_x >= 0 && u.cell_y >= 0 && u.cell_x < w.grid_w() && i < wg.size() && wg[i] != 0;
}

// The unit's ground-layer sort key.
inline double pres_unit_key(const ra2engine::World& w, const ra2engine::Unit& u) {
    return u.fx + u.fy + (pres_on_high_deck(w, u) ? kPresOnDeckBias : 0.0);
}

// Levels the draw position is raised by (bridge deck).
inline int pres_unit_lift_levels(const ra2engine::World& w, const ra2engine::Unit& u) {
    return pres_on_high_deck(w, u) ? kPresBridgeLevels : 0;
}
