# W8 interaction audit 02: ground infantry × terrain/layer × state

Auditor: w8int-02. Tree: D:\ra2-android master 34cb652, build\ra2engine.exe. Original: gamemd.exe (YR 1.000), Ghidra copy `D:\ra2-re\w8int-02` (deleted when the audit finished).
Evidence: `D:\ra2-re\w8int-02-out\` — `play\*.play` with their `play\sN\play.log` and shots, `re\a*.txt` (decompiles), and `beachscan.txt` (every map object or waypoint on a Beach cell in the 26 RA2 campaign maps, with LandType taken from TMP byte 0x29 through DAT_008278F4).

## Summary

Four new gaps. All of them are about the high-bridge deck layer, plus one RE contradiction in garrison unloading.

1. **W8-02-01 (MAJOR): a collapsing deck kills only one object per cell.**
   - The clone kills only the occupancy owner of each deck cell. The other infantry packed into that cell's sub-cells survive. They are left standing in the river at full HP with `on_deck=0`, and they cannot move.
   - In the original, FUN_0047DC80 handles every object in both cell lists:
     - everything on the deck falls and dies on landing (0x5F3970 → 0x5F3680);
     - everything under the deck is killed outright.
   - The clone also deliberately spares units under the deck.
2. **W8-02-02 (MAJOR): the deck and the water/ground under it share one occupancy plane.**
   - A GI on the deck is blocked by a ship parked under the bridge.
   - The parked ship, even a neutral one, is pushed out of the way.
   - An amphibious crusher (SAPC) swimming under the deck makes a civilian on the deck flee.
   - The original keeps separate occupation and object lists for the bridge layer.
3. **W8-02-03 (COSMETIC): effects for deck units are drawn 4 levels too low, at water level.**
   - This covers infantry death animations, corpses, muzzle flashes, beam and projectile origins, and impacts on deck targets.
   - Confirmed in play with an InfantryExplode on the deck.
4. **W8-02-04 (MINOR): garrison unloading does not match the original.**
   - Clone: an unbounded 2-D ring search for free cells, one occupant per cell, with no height gate.
   - Original (0x457DD0): ONE exit cell, found along the foundation border with the occupant's own Can_Enter (vtbl+0x1AC, which includes the height gate). Every occupant is unlimboed there. If no border cell passes, the occupants are killed.

### Beach check (the user asked to verify nothing legitimate is blocked)

- Two campaign maps place infantry on Beach cells:
  - ALL05 YellowTeam E2 at (83,50);
  - ALL11 Elania alligator `ALL` at (23,87).
- No map object has `OnBridge=1`.
- The clone places both units exactly where the map puts them, matching the original's InfantryClass INI read 0x51F810 (exact coordinates, Unlimbo, no relocation). [CONFIRMED-PLAY s5]
- Neither unit can path off its cell: `find_path*` rejects an impassable start cell (`pathfind.cpp:514/526`).
- The original's FindPath 0x42C880 returns no path when the start zone (FUN_0056CD60) differs from the goal zone, so the original very probably freezes them too. [PROBABLE-RE] **Not reported as a gap.**
- Start waypoints and team-move waypoints that sit on Beach (SOV02/06/07/08/10/11/12, TRN01/02, ALL11) end through the team "all idle" arrival rule (`campaign_runtime.inc:2568`), so they do not stall.

## Matrix

Legend: OK = matches the original (verified as tagged); GAP = see W8-02-NN; N-A = not applicable, or already covered by a known wave-7 item (cited).

| Terrain / layer / state | Reach | Speed / height | Anim | Sound | Fire / target | Damage / death | Draw |
|---|---|---|---|---|---|---|---|
| Clear / road / rough | OK (R-210) | OK; Walk has no land-speed factor [W7-11 VOK] | OK | OK | OK | OK | OK |
| Beach (Foot 0%) | OK: blocked; map-placed units are kept in place (s5) | N-A | OK (wet set only for Swim types) | OK | OK | OK | OK |
| Ice (Foot 50%, ignored by Walk) | OK | OK [W7-11 VOK] | OK | OK | OK | OK | OK |
| Tiberium / ore | OK | OK | OK | OK | OK | OK | OK |
| Walls / fences | OK (Wall LandType blocks) | N-A | N-A | N-A | N-A | N-A | N-A |
| Cliffs, ramps, slopes, height gate | OK (move-bridge-cliff `cliff_steps==0`) | OK | OK | OK | OK | OK | OK |
| High-bridge deck, intact | OK (crossing) | OK (`uheight` = Level+4) | OK (no Swim; fixed 3a7c551) | OK | OK | **GAP 03** (FX at water level) | OK (sprite lifted, sorted) |
| Deck with a unit under it | **GAP 02** | — | — | — | — | **GAP 02** (a crusher under the deck makes deck units flee) | OK |
| Deck destroyed while units on it | — | — | — | — | — | **GAP 01** | — |
| Deck repaired (engineer at the hut) | OK (s1–s4: `bridge_destroyed` 9→4, GIs cross) | OK | OK | OK | OK | OK | OK |
| Under a bridge (water) | **GAP 02** (shared plane) | OK | OK | OK | OK | **GAP 01** (original kills it; clone spares it) | OK |
| Sub-cell packing on a deck | OK (`max_cell_inf=3`, all `udeck=1`) | OK | OK | OK | OK | **GAP 01** | OK |
| Prone / crawl on a deck | OK | N-A (W7-11-05) | OK | OK | OK | OK | OK (lifted) |
| GI / GGI / Desolator deploy on deck or ramp | OK (no terrain test in either game) | N-A | N-A (W7-01-31) | N-A | OK | OK | OK |
| Desolator radiation on a deck | OK (per-cell rad level read by FootClass::AI 0x4DA440 in both games) | — | — | — | — | OK | — |
| Tesla Trooper charging a coil across a height difference | N-A (charging cannot be ordered at all: W7-02-02) | | | | | | |
| Death on a deck: corpse layer | — | — | OK | OK | — | OK | **GAP 03** |
| Crushing on a deck / on a slope | OK on the same layer; **GAP 02** across layers | | | | | | |
| Paradrop onto deck / cliff / water | OK: never into water. The original's SpawnParachuted 0x5F5150 refuses a cell that fails IsClearToMove; the clone uses walkable cells only | | | | | | |
| Engineer repairs a hut with units on the bridge | OK | | | | | | |
| Garrison destroyed or evacuated next to water / cliff | **GAP 04** | | | | | | |
| Transport unload onto blocked or height-gated cells (fv7c) | OK (`world.cpp:2126-2131` path_step_layer from the transport's layer) | | | | | | |
| Transport killed on a deck by the collapse | OK (the passengers cannot stand on the water cell → tombstoned) | | | | | | |
| Rocketeer landed | N-A (W7-01-21 / W7-01-27) | | | | | | |
| Civilians / animals as targets on the deck | see GAP 02 (CIV1 forced to flee) | | | | | | |

## Gaps

### W8-02-01 — A collapsing high-bridge deck kills only one object per cell; packed infantry survive in the river (MAJOR)

**Evidence:** [CONFIRMED-PLAY `play/s1-deck-collapse.play`] and [CONFIRMED-RE 0x0047DC80, 0x0047EAB0/0x0047EB50 (cell lists), 0x005F3970, 0x005F3680].

**Reproduction (ALL01T, Fort Bradley east bridge):**
1. The engineer repairs the bridge at the CABHUT (`bridge_destroyed` 9→4).
2. Three GIs are packed into deck cell (82,53): `max_cell_inf(0)=3`, and all three have `udeck=1`.
3. `cheat bridge_hit 82 53 1000000` breaks the span.
4. After 60 frames: `units(0,E1)=2`. Two GIs remain at (82,53), spots 4 and 3, with `uhpn=125`, `udeck=0`, `uheight=2` (river level) and `uland=2` (Water). Shot: `s1/after.png`.
5. Their path stays empty (`upath=-1`): they are stuck in the river for the rest of the game.

**Original:**
- FUN_0047DC80 (called by the high-bridge destroy walks FUN_00570FC0 / 0x56EA80 / 0x56EE20 / 0x47E380):
  - walks the WHOLE object chain at CellClass+0xE4 (the ground list; 0x47EAB0/0x47EB50 select +0xE4 for `alt=false` and +0xE8 for `alt=true`). Each object there gets `ReceiveDamage(&Health, 0, Rules+0xFA8, 0, 1, 1, 0)`, which is lethal;
  - then walks the WHOLE bridge-layer chain at +0xE8 and calls vtbl+0xEC = 0x5F3970 on each object. That sets IsFallingDown (+0x8D) and the die-on-landing flag (+0x8F), and clears OnBridge (+0x8C);
  - spawns the debris animations.
- 0x5F3680 (the fall tick): on touchdown, `if (+0x8F && Health > 0) ReceiveDamage(Health, …)`, and on a Water LandType it spawns the Rules+0xBC4 animation (the splash).
- So every unit on the deck falls and dies, and every object under the span in those cells is killed.

**Clone:**
- `world_depth.cpp:586-591` checks only `occ_.at(c.x, c.y)`, which is the vehicle owner or else the FIRST sub-cell infantry, and only when `u.on_deck`.
- The comment there ("an amphibious unit swimming under it lives") contradicts FUN_0047DC80, which kills the +0xE4 chain.
- Low bridges have the same one-occupant pattern at `world_depth.cpp:450-454`. [SUSPECTED for low bridges: the original's low-section kill path was not found. FUN_00654C00 only queues the cell into a list at 0x87E768+0x1228, and FUN_005862D0/0x5863C0 recompute zones.]

**Root cause:** a whole-cell occupancy query is used as "the objects in the cell". There are up to three infantry per cell (W5-B sub-cells), and the cell under the deck is a separate population.

**Suggested fix:**
- In `break_high_bridge`, iterate every live unit whose `cell_x/cell_y` is one of the broken cells (not `occ_.at`).
- Kill every unit with `on_deck=1` (as a fall: optionally a short fall, then the splash animation on Water). This is consistent with 0x5F3680, since everything that falls dies.
- Kill every non-aircraft unit with `on_deck=0` in those cells (debris), following the +0xE4 loop.
- Apply the same iteration in `set_bridge_cell`'s low-section branch.

**Test:**
- s1 as a unit test: three infantry with `on_deck=1` in one deck cell plus one amphibious unit under it. `break_high_bridge` → all four dead.
- `play/s1-deck-collapse.play` must end with `units(0,E1)==0`.

### W8-02-02 — The deck and the cell under it share one occupancy plane (MAJOR)

**Evidence:** [CONFIRMED-PLAY `play/s4-ship-under-deck.play`, `play/s2-deck-layers.play`, `play/s7b2.play` with control `play/s7c-control2.play`] and [CONFIRMED-RE: docs/re/movement-height.md §2 (IsClearToMove 0x4833B0 reads the bridge layer's occupation flags +0x128 on the deck), cell lists +0xE4/+0xE8 (0x47E9A0 RemoveContent(obj, onBridge), 0x47EAB0/0x47EB00/0x47EB50)].

**Reproduction 1 (s4):**
1. A neutral (house 4) DEST is parked under deck cell (82,53).
2. A GI is ordered from (82,56) over the deck to (82,49).
3. `RA2_LOCO_TRACE` shows: `blocked u=119 GI at (82,54) next (82,53) occ=114 … occupant DEST house=4`, repeated for 1.3 s.
4. The ship is then pushed off to (83,54): `ucell(4,DEST,0)` goes 82053 → 83054.
5. s2 shows the same with an own DEST.

**Reproduction 2 (s7b2, with control):**
1. A neutral CIV1 stands on deck cell (82,53).
2. The player's SAPC (Crusher=yes, Hover, Amphibious) swims from (78,53) to (87,53) under the deck.
3. The civilian is moved off to (82,54) (the crush warning makes it scatter).
4. In the control run, where the SAPC does not move, it stays at (82,53).
5. If the civilian had not moved, `squash_on_arrival` (`world_loco.cpp:332-352`) would have killed it from under the bridge.

**Original:**
- An object on the deck is in the cell's AltObject chain (+0xE8) and uses the bridge-layer occupation bits.
- `IsClearToMove` on the deck checks only those bits.
- A ship, or an amphibious unit, under the span is in the ground chain/flags.
- The two populations neither block, scatter nor crush each other.

**Clone:**
- `Occupancy` (`occupancy.hpp`) has one owner and three spots per cell.
- `reserve_cell` (`world_loco.cpp:239-326`) and the crush victims (`:258`) read it whatever the layer (`on_deck`).
- `build_occupancy_cost` and `on_blocked` also treat the unit under the deck as the blocker.

**Impact:**
- Bridges are choke points. Any naval unit idling under a deck, which is common on river maps such as ALL01, blocks or diverts infantry and vehicles crossing above.
- Deck traffic pushes friendly and neutral ships around.
- Hover crushers passing under a bridge scatter, or can kill, enemy infantry on it.

**Suggested fix:** a second occupancy plane for deck cells.
- `Occupancy` gets a `layer` argument on `at/can_enter/set/release/free_spot`. Layer 1 is used only for cells with `deck_grid != 0`, and is selected by `u.on_deck` and by the `layer` result of `path_step_layer`.
- The crusher victim list comes from the mover's layer only.

**Test:**
- Unit test: a ship holds (x,y) at layer 0, and an infantry steps onto (x,y) at layer 1 → `Reserve::Ok`, and the ship is not scattered.
- Play: s4 must keep `ucell(4,DEST,0)==82053`, and the GI reaches row 49 in about 150 frames with no `blocked` lines.

### W8-02-03 — Effects for units on a deck are drawn at water level, 4 levels (60 px) too low (COSMETIC)

**Evidence:** [CONFIRMED-PLAY `play/s3-deck-layers2.play`, shots `s3/fx_before.png`, `fx_t3.png`, and the montage `s3/fx_strip.png`] and [CONFIRMED-RE: an OnBridge object's Z includes BridgeHeight (0x51F810 adds DAT_00a8e19c when OnBridge; 0x5F5150 sets OnBridge on a 0x100 cell). Animations are created at the object's coordinates.]

**Reproduction:** a lone GI stands on deck cell (82,53) (`udeck=1`). `cheat infkill 0:E1:0 3` triggers InfantryExplode. Three frames later the explosion is centred about 60 px below the GI's sprite, against the bridge's side over the water.

**Clone:** every effect uses the ground level and ignores `pres_unit_lift_levels`:
- `pres_game.inc:1233` uses `level_at(w, u.fx, u.fy)` for the infantry death animation and the corpse (`:1272`);
- muzzle flash `:833`, beam origin `:841` and projectile origin `:860` use `u.level`;
- impacts on unit targets `:865` and `:919` use `t.level`.

The sprite itself is lifted correctly (`main.cpp:3928/4452`, `pres_layers.inc:42`).

**Root cause:** `Unit::level` is the cell's ground level. Only the sprite path adds the deck lift.

**Suggested fix:** a `pres_unit_fx_level(w,u) = u.level + pres_unit_lift_levels(w,u)` helper, used at those seven sites. The vehicle death `water_death` test already checks `on_deck`.

**Test:** in the play script, add a probe of the last spawned fx level, `pres(last_fx_level)`. After an infkill on the deck, assert it equals `uheight`. Alternatively, diff the shot crops as in s3.

### W8-02-04 — Garrison unload uses an unbounded 2-D nearest-free-cell search, not the original's single exit cell (MINOR)

**Evidence:** [CONFIRMED-RE 0x00457DD0 UnloadOccupants, 0x004585B0 KillOccupants]. [SUSPECTED impact: no play reproduction; no garrison with a fully blocked border was found in the maps that were checked.]

**Original (0x457DD0):**
1. It walks the cells along the foundation edges (down the left side, along the bottom, …) and calls the first occupant's vtbl+0x1AC on each. That is InfantryClass Can_Enter 0x51BD70, which includes the FootClass height gate 0x4D9B70 and `Ground[LandType][Foot]`.
2. It takes the first cell that passes.
3. If none passes, it calls KillOccupants(0x4585B0).
4. Otherwise every occupant is Unlimboed (vtbl+0xD8) at that one cell's centre. The infantry share its sub-cells, and a unit whose Unlimbo fails is deleted (vtbl+0xF8). Then the occupants are scattered.

**Clone (`world_depth.cpp:190-221`):**
- `nearest_free_cells(corner, n, passable_, …)`: Chebyshev rings out to the whole map, on the 2-D Land grid.
- One occupant per whole free cell.
- No height gate and no border limit.
- An occupant is killed only when the whole map has no free cell left.

So occupants of a building whose border is closed (water, cliff faces, other buildings) are teleported to the nearest free cell anywhere, including the far bank or the top or bottom of a cliff. The original kills them. The comment there already marks the walk and sub-cell order as RE-FALLBACK; the kill and "one exit cell" rules are new RE facts.

**Suggested fix:**
- Scan only the cells bordering the foundation, in the 0x457DD0 order.
- Test each with `unit_can_enter_cell` plus `path_step_layer` from the building's cell level.
- Place all occupants into that cell's three spots, and the rest into its neighbours (scatter).
- If there is none, use `ability_kill_unit` for all.

**Test:**
- Unit test: a 2×2 garrison surrounded by water/Rock → `unload_occupants` kills all occupants.
- A building with only one free border cell → all occupants end within 1 cell of it.

## Notes (not gaps)

- **Paradrop "drown or not".**
  - Original 0x5F5150 refuses a cell whose IsClearToMove fails for the passenger. On a bridge cell (0x100) it sets OnBridge and needs 0x200.
  - The clone drops only on walkable cells (`world_an.cpp:957-961`, `1336-1338`), so nobody drowns in either game. OK.
  - [SUSPECTED, untested] On a deck-over-land cell (`deck_land_ == 1`) a clone paratrooper lands with `on_deck=0` (under the bridge), whereas the original lands on the deck. The ALL01 decks are over water, and no over-land deck was found.
- **Units placed on impassable cells** (map Beach units, and the W8-02-01 survivors) can never path (`pathfind.cpp:514`). For map units this very probably matches the original (zone gate, 0x42C880). For the collapse survivors it disappears once W8-02-01 is fixed.

## Verified OK

- Bridge repair with an engineer at the CABHUT, then GIs crossing the deck: `bridge_destroyed` 9→4 and the crossing works (s1–s4). [CONFIRMED-PLAY]
- Sub-cell packing on a deck: three GIs share (82,53) and all have `udeck=1`. [CONFIRMED-PLAY s1]
- Deck-only cells keep `on_deck`; `uheight` = Level+4 on the deck. [CONFIRMED-PLAY]
- No Swim / wet set on decks (`world.cpp:6036`). The enter-water sound is gated on `on_deck` (`pres_audio_events.inc:623`).
- Map-placed infantry keep their exact cell and sub-cell, Beach cells included: ALL05 E2 at (83,50), spot 3, `uland=6`. This matches 0x51F810 (exact coordinates, Unlimbo). [CONFIRMED-PLAY s5, CONFIRMED-RE]
- Paradrop never lands in water (see Notes). [CONFIRMED-RE 0x5F5150]
- Transport eject from a deck uses the transport's layer in the height gate (fv7c). A collapse kills the passengers of a transport on the deck, because the unload cell becomes water.
- Radiation is a per-cell level in both games (FootClass::AI 0x4DA440), so deck and under-deck units both take it.
- Walk has no land-type speed factor on Ice, Tiberium, Rough or Railroad (W7-11 VOK).

## Evidence index (D:\ra2-re\w8int-02-out)

- `play/s1-deck-collapse.play`, `play/s1/` — W8-02-01.
- `play/s2-deck-layers.play`, `play/s4-ship-under-deck.play`, `play/s4.stderr` (loco trace), `play/s7b2.play` with control `play/s7c-control2.play` — W8-02-02.
- `play/s3-deck-layers2.play`, `play/s3/fx_strip.png` — W8-02-03.
- `play/s5-beach-placed.play`, `play/s6-beach-leave.play`, `beachscan.txt`, `scripts/beachscan.py` — the Beach check.
- `re/a1.txt` (0x47DC80, 0x654C00, 0x5F3680, 0x5F5150), `re/a2.txt` (0x457DD0, 0x4585B0, 0x51F810), `re/a3–a5` (low-bridge rewrite and zone recompute), `re/a6–a7` (FindPath 0x42C880 zone gate), `re/a8.txt` (cell list accessors), `re/a9–a10` (Infantry vtbl+0xEC = 0x5F3970), `re/a11.txt`.
