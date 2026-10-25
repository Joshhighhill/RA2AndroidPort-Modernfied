# Rendering and movement audit — Android build 900039

Source baseline: `1ec1b388b8b77e405fbd5b562ef14c5e863058c4`, branch
`android-touch`. Research date: 2026-10-25.

The Google Drive rendering research report and build 900038 diagnostics were
read before these changes. This is an independently written native C++/SDL
implementation. No Chrono Divide client or Gagarin implementation was copied.
Original YR 1.001 retail art and the repository's original-binary research
remain the fidelity reference. Chrono Divide is a visual benchmark for shared
RA2 content, not a Yuri-specific behavioral authority.

## Evidence and corrections

| Area | Verified fork evidence | Correction / status |
| --- | --- | --- |
| Bunker entry | 900038 log frame 657 physically arrives at 154,80 then draws toward 153.5,79.5; repeated at 1300 and 2420 | Use the physical center consistently for occupant drawing and transitions. Normal movement and horizontal hull alignment remain authoritative. |
| Bunker exit | Frames 1064 and 2199 immediately unload to 155,81 before cosmetic movement | Save a lowering timer, keep the occupant attached while lowering, verify an exit route, then use normal reservations, turning and movement from the center. Closed exits retain the occupant. Emergency death/sale/control-loss ejections retain their previous behavior. |
| Friendly traffic | Strict recovery could return before the friendly scatter action; forced clearance rejected moving but blocked infantry | Request friendly clearance before strict detours; permit blocked walkers to yield. Unit ID breaks symmetry between waiting walkers. New bounded diagnostics identify the actual blocker and layer. This does not prove the recorded BRUTE/HTNK cases are eliminated on-device. |
| Miner planning | Mobile wake-up routed onto ore before finding a legal deployment foundation | Choose reachable legal sites within two cells of the selected ore, ordered by travel distance and proximity; use the saved quiet walk/deploy mission. This is a local heuristic, not a claim of a global economic optimum. |
| MCV audio | Packing uses Selling internally, which triggered EVA_StructureSold | Suppress sell audio for a valid pending human skirmish ConYard conversion; retain ordinary sale announcements. |
| Rendering cadence | Fixed-tick positions/facings were used directly for unit drawing | Add previous/current display history for position, shortest-arc body/turret facing, ground height and altitude. Reset across death, cargo/parent, owner, layer, freeze, model and large displacement boundaries. Rendering never writes back into World, collision, RNG, damage, saves or commands. |
| Depth | Object sorting could use a different temporal position from the drawn unit | Use the same interpolated XY in the unit depth key. Existing bridge/deck and static object layering policy is preserved pending visual capture. |
| Voxel shadows | Flat shadow silhouette ignored body terrain rotation; default ShadowIndex=0 failed to isolate section zero | Apply the terrain transform before ground projection, select the declared section including index zero, keep original frame-zero shadow semantics, and reuse bounded animation-cache entries. |
| HVA | Existing part-specific HVA frame selection, section-name lookup and bounded raster caches are implemented | Preserve them. Chrono Divide SDK's first-frame-only HVA behavior is not adopted. Original-binary Drive DrawMatrix remains incomplete in the pinned Gagarin reference. |
| Infantry | Existing retail art sequence tables drive discrete frames at logic cadence | Preserve sequence timing and discrete actions; interpolate ground position/height only. SHP infantry are not tilted as though they were voxel vehicles. |
| Factories / superweapons | Existing art-driven building slot machine drives factory and superweapon phases | Preserved; no new universal factory-animation fix is claimed. Requires per-building reference recordings for remaining discrepancies. |

## Cross-reference limits

Chrono Divide official SDK: <https://github.com/chronodivide/mod-sdk>.
It documents a different 3D rendering treatment and incomplete YR/HVA support.
No new side-by-side Chrono Divide capture or throughput comparison was made.

Gagarin research pin: `aedf638cea5e96a3ee8be5151296f41936192e44`.
`match/movement/drive_locomotion.cpp` identifies original slope-related leads
but explicitly leaves the large DrawMatrix/Process routines deferred.
MapClass::Update_Pathfinding_2 at 0x586990 updates dirty subzones; it is not a
complete replacement pathfinder. ClearBunker 0x459470 and UnloadBunker
0x4593A0 support state/slot research, not exact full entry/exit timing.
The 18-tick lowering span follows the local retail reverse-art cadence;
exact visual timing still requires phone/PC comparison.

## Validation and remaining work

The complete native suite passed 1255 cases and 276808 assertions, zero
failures; one optional missing-retail landmark fixture was excluded. Focused
validation passed 15 cases / 618 assertions. Android syntax passed with the
existing waypoint_cell missing-override warning. The first broad run caught
five failures (old instant-exit/direct-ore expectations and a standalone
traffic detour regression); these were corrected before the final full run.
The release record and Google Drive tracker contain the final test counts,
source commits, APK checksum and upload readback. Tests cover physical bunker
travel, save/load during lowering and movement, render-rate independence,
teleport/parent boundaries, quiet planned miner deployment and shadow
transforms, alongside the complete existing regression suite.

Save format changes from 53 to 54 for the lowering timer and exit passage.
Older saves are refused by the existing version check; retain backups and
start a fresh skirmish. All multiplayer peers need the same build.

Not verified by automated tests: pixel-perfect bridges/cliff occlusion,
all-map seams, exact retail slope matrices, bunker appearance/timing on the
phone, every infantry/factory sequence, or equivalent-device superiority
over Chrono Divide. No real-device, emulator or thermal profiling is claimed.
The 900038 report samples 57.93–61.49 render calls/sec but also a 51.6-second
BRUTE wait: rendering cadence and movement correctness remain separate.

Working generic projectiles, FLH rules, weapon damage/timing, Android touch
controls and launcher are preserved. No retail assets, GPL implementation,
Windows DLLs or signing credentials belong in this release.
