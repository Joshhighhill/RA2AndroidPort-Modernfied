# P6 Roster breadth + remaining producers — design

Calibration authority: `docs/re/roster2.md` (all `[RE: roster2.md §x]` refs below).

## Goals
1. Append the remaining Allied/Soviet buildings to `kRoster` (indices 24..38) and a new
   unit content table `kRoster2Units` (vehicles, naval, BEAG, infantry bodies).
2. Naval Yard (GAYARD/NAYARD) is a real producer: water-bound placement, naval-only
   production category, water exit.
3. Airforce Command (GAAIRC/AMRADR) docking + rearm for Harrier (ORCA) / Black Eagle (BEAG).
4. Walls: wall-line fill, N/E/S/W connection frames (+ damage stage), 3× wall build time.
5. 21st byte-identity gate `--demo-roster2` exercising all of the above.

## Non-goals
Unit special abilities (other branch), spawner carriers/dreadnought missiles, live
interactive sidebar wiring of the naval/aircraft queues (carry-forward), walls as
overlays (walls stay 1×1 Buildings — AI-base gate depends on it).

## Byte-identity contract
- `kRoster` APPEND-ONLY; indices 0..23 resolve exactly as before (the new `Image=`
  honouring applies to index ≥ 24 only). kUnits/kSoviet/kRosterUnits untouched.
- New mechanics live in a NEW pure module `engine/roster2.{hpp,cpp}` + new World
  methods that no existing demo calls. No existing hash function changes.
- Wall connection frames change only the RENDER of NAWALL/GAWALL (not hashed).
- `load_vehicle_assets` gains a `localmd.mix` fallback that only triggers when the
  hull is absent from `local.mix` (no existing caller loads such a unit).

## Components
`engine/roster2.hpp/.cpp` (pure, no SDL):
- `wall_mask(x,y,same)`, `wall_frame(mask,stage)`, `wall_stage(hp,max,yellow)`,
  `wall_line_fill(x,y,range,own_wall,can_place)`, `wall_build_frames(cost,bs,coef)`.
- `water_bound_ok(cx,cy,fw,fh,water,w,h)`, `naval_factory(def_naval flags)` helpers.
- `airfield_pad_lepton(b,pad,offsets)`, `airfield_pad_cell`, `reload_cycle_frames(rate)`,
  `struct Sortie` + `step_sortie(...)` fixed-frame fixed-wing state machine.
World: `add_water_building` (also blocks the water grid), `launch_naval_unit`.
main.cpp: kRoster append + Image= for ≥24, `kRoster2Units` table, wall frame render,
`--demo-roster2` (two-run self-compare, `roster2_hash`, wedges).

## Testing
`tests/test_roster2.cpp` covers every pure function with RE-pinned literals; the gate
pins the integration. Scoped runs only (`-tc="roster2*"`).
