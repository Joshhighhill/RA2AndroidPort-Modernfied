# Build 900040: evidence, corrections and open-source references

2026-10-25. Baseline: `b2537f073f90689ad815368f5774bbab73b268f7`
on `android-touch`. New user evidence was read before implementing this batch:

- Build 900039 diagnostic ZIP, Drive ID `1hbWEmBtBQCKjM6KnXXoHaEmpEPN-jTu2`.
- Bunker video `Screenrecorder-2026-10-25-21-46-07-383.mp4`, fully decoded:
  9.85 seconds, 1280x720 HEVC, reported 61.48 fps.
- Skirmish screenshot `Screenshot_2026-10-25-21-49-37-074_org.ra2.engine.jpg`.

## Confirmed faults and changes

**Reservations:** frame 13500 reports LTNK unit 1 physically idle at (141,88),
while TRAFFIC_BLOCK reports it owning the SHK's next cell (140,86).
Strict repathing only considered the owner's current cell, so it could
repeatedly select the supposedly free remote cell. Completed movement and
idle ownership now release unused claims. A blocked mover also discards an
orphan claim when the owner is idle, centered, has no active track and is
physically elsewhere. Live curved-track reservations and bridge layers are
preserved. Regression fixtures recreate remote idle claims and walk completion.

**Combat approach:** HTNK 11 waits behind HTNK 12 at goal (160,80), reaching
18.53 seconds while attacking building 13. A path goal remaining in weapon
range previously prevented choosing a new firing position even after another
unit occupied it. After a sustained block, unit and building approaches now
request another legal firing position. This preserves weapon range and target
rules rather than forcing attackers through the blocker.

**Bunker socket:** the video shows the hull below the platform, not merely a
missing hull animation. The previous southeast cell of the 2x2 foundation was
also used as the occupant render socket. Approach, attachment, rendering and
exit now share the art-aligned docking cell. Moving from (cx+1,cy+1) to (cx,cy)
removes a 30-pixel downward projection offset before zoom. Retail NATBNK art
lists a (-1,-1) docking offset but explicitly comments that it is unused;
therefore this is an independent calibration against the clip/art pivot, not
proof that the original engine reads that field. Exact appearance remains a
phone acceptance check. Existing lower-before-exit and normal path motion stay.

**Skirmish layout:** the independent panel incorrectly used the original
Yuri shell's nonlinear rectangle transform. Each control sub-rectangle could
move differently, including dropdown arrows and slider labels/readouts.
The panel now uses a uniform 640x480 fit for drawing and input. Flags fit their
row bounds; Yuri has an independent letter badge instead of the random icon.
Compact Team/Start controls reserve appropriate arrow space. Slider labels and
controls have separate vertical bands. Preview/options columns no longer
overlap, all spawn badges stay inside the fitted image, and the map name is
shown above the preview. Model, teams, start selection and persistence remain.

## Research sources worth studying

| Project | Useful reference | Licensing / scope boundary |
| --- | --- | --- |
| [OpenRA](https://github.com/OpenRA/OpenRA) | [Move activity](https://github.com/OpenRA/OpenRA/blob/bleed/OpenRA.Mods.Common/Activities/Move/Move.cs): blocked destinations, moving aside, retrying and terrain orientation. Its included RA/TD/D2K mods show declarative rules and activities. | GPLv3 engine. Study behavior and architecture; no source transplant into this 0BSD fork. Not an original YR behavior oracle. |
| [OpenHV](https://github.com/OpenHV/OpenHV) | Complete OpenRA-based RTS/mod with MiniYAML rules, Lua missions, bots and editor; a concrete moddable game reference. | Source GPLv3; artwork/audio/rules have separate Creative Commons notices by folder. Do not assume all content has one license. |
| [EA Generals / Zero Hour](https://github.com/electronicarts/CnC_Generals_Zero_Hour) | [AIPathfind.cpp](https://github.com/electronicarts/CnC_Generals_Zero_Hour/blob/main/Generals/Code/GameEngine/Source/GameLogic/AI/AIPathfind.cpp) and nearby movement systems offer original RTS traffic/pathfinding research. | GPLv3 with additional terms; original Generals logic is not original YR logic and is not a drop-in port. No code copied. |
| [EA Red Alert](https://github.com/electronicarts/CnC_Red_Alert) | Official earlier Westwood source: missions, cell occupancy and movement life cycles. | GPLv3 with additional terms; this is RA1, not a released RA2/YR source tree. |
| [Phobos](https://github.com/Phobos-developers/Phobos) | [Documented fixes](https://phobos.readthedocs.io/en/latest/Fixed-or-Improved-Logics.html), including bunker eligibility/logics and mod-facing configuration. | GPLv3 Windows YR extension using injection/addresses. Reference only here, not an ARM64 plugin or a license-free code library. |
| [CnCNet client](https://github.com/CnCNet/xna-cncnet-client) | Skirmish lobby organization and mod-client configuration; separates setup from the game spawner. | GPLv3 MonoGame/XNA client. Not the in-match rendering/pathfinding engine. No skin/code imported. |

OpenRA's separation of blocked movement, yielding and retrying supports the
architecture distinction used here. The concrete reservation cleanup fix was
derived from this fork's logs and occupancy implementation, not copied from
OpenRA. Original YR 1.001, retail rules/art and pinned Gagarin research remain
the reference for YR-specific fidelity. No borrowed implementation is included.

## Validation boundaries

The final native and focused counts, Android build, exact commits and APK
checksum are recorded in the same Drive research tracker and release metadata.
Final native run: 1259 cases / 276943 assertions passed, zero failures; one
optional missing-retail landmark fixture excluded. Focused run: 12 cases /
261 assertions passed. Android syntax passed with the existing waypoint_cell
missing-override warning only.
The first full native pass succeeded before the additional combat regression;
the final full suite is repeated after that test is compiled.

Save schema stays 54. MCV packing/audio, working generic projectiles, touch
controls, previous interpolation and animation improvements are retained.
No all-map pathfinding, pixel-perfect bunker/bridge appearance, UI screenshot
parity, GPU/thermal performance or mod binary compatibility is claimed without
device verification. Existing report samples 57.91–61.63 render calls/sec and
no fatal marker; a menu at 0 logic/s after returning to lobby is not a freeze.
Historical binary exit traces were preserved, not newly symbolicated.

No retail assets, incompatible source, DLL or signing secret is bundled.
Omar El Arab's 0BSD notices, Bryle13 credit, third-party notices and game-file
ownership/non-affiliation terms remain. Delivery is Google Drive only.
