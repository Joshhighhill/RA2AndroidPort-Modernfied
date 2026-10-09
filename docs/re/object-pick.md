# Object pick under the pointer (W6-01-01)

How gamemd.exe decides which object a click lands on, and how the engine's touch
pick deviates from it. Hunt: Ghidra headless on a private copy of the p4 project
(`D:\ra2-re\w6fix-touch`), scripts `D:\ra2-re\w6fix-touch-scripts\w6t_1..4.py`,
output `D:\ra2-re\w6fix-touch-out\`.

## The chain

`FUN_00691d00` (the tactical "what is at this screen point" helper, called by the
WndProc FUN_00692ab0, the edge-scroll / cursor update FUN_00692940, FUN_004ae400 and
FUN_004faf00) resolves the cell with `FUN_006d5b00` and the object with
`FUN_006d98f0(screen point)`. It then drops the object if it is hidden from the
player (a cloaked / disguised techno, or a building whose type byte `+0x1701` is set).

`FUN_006d98f0` (TacticalClass, `this` = the tactical):

1. **The drawn-object list.** It walks `count = tactical+0xDB0` entries of the
   array at `DAT_00b0be30`, 12 bytes each: `{ObjectClass* obj, int x, int y}`
   (`x`, `y` = screen point + the tactical's scroll `+0xB0` / `+0xB4`). For each
   visible, not-in-limbo entry it computes
   `d = (y - py)^2 * 0.5 + (x - px)^2` (`0.5` = the double at `0x7e0738`;
   the FPU sequence at `0x6d99b4..0x6d99e7`, then ftol `FUN_007c4d00`), and keeps
   the smallest `d` with `d < 200` (`CMP EAX, 0xc8`). So an object is hit inside an
   ellipse about **14 px wide and 20 px tall** (half-axes) around its registered
   point; the nearest wins.
2. **Fallback.** No hit: the cell under the point (`FUN_006d5b00` -> `FUN_005653d0`)
   and its first object (`cell + 0xE4`, the occupier chain).

## Who is in the list

The list is rebuilt during the redraw:

- `FUN_006d9250` (called from the tactical draw `FUN_006d3280`) adds every object
  of the vector `0xA8DAA8` (Items `0xA8DAAC`, Count `0xA8DAB8`) that is alive
  (`+0x90`), on the map (`+0x74`) and whose type byte `+0x520 -> +0x1701` is clear,
  when the screen point of its `Location` (`+0x9C/+0xA0/+0xA4`, the iso projection
  `(x - y) * 30 / 256`, `(x + y) * 15 / 256 - z`) lies in the redraw rectangle.
  `0xA8DAA8` is the **BuildingClass** vector: the BuildingClass constructor
  `FUN_0043b6c0` stores the type at `+0x520` (`this[0x148] = type`) and the
  destructor `FUN_0043bc70` removes the object from `0xA8DAA8`. A building's
  `Location` is its origin cell's centre (draw point = `Location - (128,128)`,
  docs/re/building-render.md), so the hotspot is the **origin cell's centre**.
- `FUN_006d9460(obj, x, y)` adds one entry; it is called from the infantry draw
  (`0x518da9`, InfantryClass) and two other draw sites (`0x4148cc`, `0x73be9b`:
  aircraft / unit draw code) with the object's draw point.
  [RE-FALLBACK: the exact point a foot unit registers (via its vtbl `+0x1C0`) was
  not traced to the end.]

## Consequence

A mouse picks a building by its **foundation** (the cell fallback) plus a small
ellipse around the origin cell's centre. The upper art of a tall building (a
Prism Tower's head, a Battle Lab's dome) is **not** pickable in the original; a
mouse player sees the cursor change before clicking and adjusts.

## Engine

`Input::pick_building(px, py, touch)` (engine/input.cpp):

- mouse = the original: the building-hotspot ellipse (`dx^2 + 0.5 dy^2 < 200` around
  the origin cell's centre at its ground level), then the level-aware cell's
  building;
- touch [engine deviation, W6-01-01]: first the building whose **drawn art** has an
  opaque pixel under the finger (the main frame draw_building draws, plus the
  always-on slot anims ActiveAnim..Four / IdleAnim / SuperAnim..Four), front-most by
  the render depth key; then the mouse rule. A finger has no cursor preview, and a
  1x1 defence's foundation is only ~45x23 dp on a phone.
- touch, units [engine deviation, W6-01-01/06]: the clone's generous unit selection
  box (65x63 px) only claims a tap that lands on nothing better. Outside the unit's
  **body** (the original's hotspot ellipse, centred 10 px above the ground point), a
  tap on a building's art picks the building, and -- with own units selected -- a tap
  beside an own unit is a ground tap (the order).
