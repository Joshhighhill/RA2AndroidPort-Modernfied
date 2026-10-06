// realplay_scene.inc -- GameScene member functions owned by gap owner A
// (economy / production / base). #included INSIDE `class GameScene` (private
// section) so the scene body in main.cpp stays a thin caller and parallel
// fixers do not collide on it.
public:
    friend struct RealPlayProbe;   // --probe-realplay drives the real scene

    // Is a building of roster type `type` placeable with its origin at
    // (cx, cy) for the human? The one rule set the placement cursor confirm
    // and the probe share [RE: construction.md §9].
    bool placement_ok(int type, int cx, int cy) const {
        if (type < 0 || type >= static_cast<int>(ba_.roster.size())) return false;
        const ra2engine::BuildingTypeDef& d = ba_.roster[type];
        const ra2engine::PlacementCheck chk = ra2engine::placement_check(
            cx, cy, d.fw, d.fh, 0, d.adjacent, world_.passable(), world_.grid_w(),
            world_.grid_h(), world_.occupancy(), world_.buildings(),
            /*bypass_adjacency=*/false);
        return chk.ok;
    }

    // Place a finished-in-the-queue building at (cx, cy): validity, then
    // add_building in Buildup (instant=false). Returns false (nothing placed)
    // when the cell is illegal.
    bool place_building(int type, int cx, int cy) {
        if (!placement_ok(type, cx, cy)) return false;
        const ra2engine::BuildingTypeDef& d = ba_.roster[type];
        world_.add_building(type, cx, cy, d.fw, d.fh, 0, d.strength, d.armor_ix,
                            ba_.art[type].buildup_total, /*instant=*/false, d.sight,
                            d.grants_map_reveal);
        return true;
    }
private:
