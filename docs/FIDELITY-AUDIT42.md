# Build 900042: wall, sensor intent and attack presentation

Baseline: android-touch 7d75307e3fa656d89ab5a7b10184a7276c1cc1b1.
Read RA2YRINST.txt and existing tracker. User clarification: dashed black-edged
lines refer to detected enemy intentions, not the sensor radius.

## New evidence

Diagnostics Drive 11qpSaCcf9ZEfQ6SlC2wdffBoctyq5VyD identify 900041 / -84-g7d75307.
All text members read: current engine 517,799 bytes / 4,187 lines; previous
780,506 bytes / 5,968 lines (same as the prior report); skirmish 600 lines;
device/report/exit/readme/running metadata. Asset logs empty, description absent,
low_memory=false, available memory 3,581,775,872 bytes. Current session ends at
frame 10473 with background pause and successful autosave (49 ms), not a recorded
clean quit. running.txt alone does not prove a crash. No fatal/assertion/signal
text marker found. Maximum sampled wait 3.07 s. Historical exit metadata is not
proof that the newest session crashed. No binary trace was newly symbolicated.

Both newest screenshots inspected at original download resolution:
1Fv8RtbT2rpV4YRi_x_4uYMo02OHtVlqR and 1lYxf2iIDlbFIpHYp4_uyEwm60Nwsh0Wd.
They show the pink Citadel wall and selected sensor radius. The user clarified
the requested line styling; the radius was not treated as the intent line.

Two newest reference MP4s decoded in full: 1_eJbAA2avzCuRcFVKxbTH-MDXxmL8Ekw,
5.75 s / 7,761,387 bytes, and 1fiwjINw8DxZ-74Y7w5avXFVg698XgKC-, 12.48 s /
15,181,318 bytes. Both are H.264 at reported 30.01 fps; source 1280x720 with a
720x1280 rotated decoded display. These film PC gameplay/reference video rather
than proving current Android behavior. First shows a Disc over a structure with
a circular effect; second shows direct pink laser combat. Contact sheets checked.
No sampled live DISK/V3/V3ROCKET instance or launched nuclear strike was found
in the new diagnostics, although those types were loaded. Do not infer that
their attacks were successfully exercised in this session.

## Confirmed causes and corrections

R-003 wall: classic RA2 generic.mix/ggfwll.shp is an unused French-wall placeholder
(50,656 bytes), while YR genermd.mix/ggfwll.shp replaces it with Yuri Citadel art
(115,720 bytes). The old roster resolver explicitly tried RA2 first. Rendering
the old asset privately reproduces the magenta shape in the screenshot. Added
a narrowly scoped GAFWLL expansion-art preference using existing AssetFS support.
Other assets retain their search order. A retail resolver regression checks both
distinct archives/contents. No wall art or palette is shipped with the APK.

Sensor intent: game_scene_controls.inc drew enemy intention as one solid line.
It now batches owner-coloured dashes with black outline geometry. The existing
detection, fog behavior, target selection and selection-radius drawing remain.
Bounded pure geometry tests cover gaps, direction, zero length and long lines.

Brute/infantry: finished SecondaryFire was replayed for every secondary weapon,
an overgeneralized designator path. Repeat is now gated by the existing Boris
airstrike profile. Ordinary secondary attacks finish between real shot events;
damage, ROF and targeting are unchanged. Combat preempts idle fidgets, and ground
force-fire is included in the no-idle target context. Tests distinguish one-shot
secondary attacks from sustained designators and prevent combat idle fidgets.
This is a requested presentation correction with source/test evidence; exact
Brute cadence versus paired vanilla footage still needs acceptance. The pinned
Gagarin Infantry.cpp slice contains related layout/sequence reads, but no complete
0x520810 sequence-finish body was established here. No GPL code copied.

Floating Disc direct fire: the old renderer drew several changing spokes from
a small arbitrary ring. The new shot presentation uses the documented vanilla
240-lepton radius and one target-facing rim point fixed at beam creation. Beam
targets include aircraft/parachute altitude. Pure tests verify radius and nearest
rim direction. This does not implement the full charge-up state machine or claim
pixel-perfect laser timing/attachment while the firer moves.
Reference: https://modenc.renegadeprojects.com/DiskLaser . Original retail
[DiskLaser] supplies colours, duration and damage. No generic shell added.

Nuclear/missile effects: nuclear strikes live outside World::projectiles(), so
the ordinary trail observer never emitted retail NKMSLUP Trailer=NUKEPUFF.
It now observes the existing upward-strike clock once per logic tick and applies
the configured trailer/cadence, without changing strike timing or damage.
Normal projectile trails use the immutable fired weapon ID where available,
instead of re-reading a firer's possibly changed weapon. Generic vehicle flight
integration and existing shell rendering remain intact. V3/Spawner/rocket-pitch
and Nuke carrier/payload rendering were inspected, not rewritten wholesale.

## Important unimplemented mechanics

No DrainWeapon/DiskDrain handling was found in the native parser/runtime.
The Floating Disc power shutdown/refinery drain gap is therefore gameplay plus
presentation, not merely a missing beam. A safe implementation needs saved
unit/building links, target eligibility, legal hover/approach, power/economy
integration, interruption/ownership/death cleanup, audio and deterministic tests.
No cosmetic beam is presented as a completed drain mechanic.

V3 launch choreography, nuclear launch/impact parity, exact Disc charging and
all specialist projectile effects remain open. The newest log lacks executed
examples to validate each. Existing missile-launcher and superweapon regressions
remain the automated baseline. More original/Android paired captures are needed
before calling full visual parity complete.

## Mental Omega sample audit (read-only)

Actual directory: D:/Games/Mental Omega/mod, not the descriptive path suffix
"mod Mental Omega mod". Its readme identifies 3.3.6. ClientDefinitions.ini launches
gamemd.exe through Syringe.exe. Ares.dll (875,520 bytes), Syringe.exe (326,144),
and MentalOmegaClient.exe (287,232) all have PE machine 0x14c, Windows x86.

Read rulesmo.ini (2,290,353 bytes) and artmo.ini (898,681) privately from
expandmo99.mix. Sample includes FourthSide/Foehn, custom ArmorTypes, 820
AttachEffect.* assignments and 34 Convert.* assignments. The native fixed
faction/armor/ability model and rulesmd/artmd resolver do not supply those
extension semantics or the mod's INI/profile/archive bootstrap. Files named
mapsmo03.mix, movmo03.mix and multimo.mix are valid 97-byte archives containing
only their MIX database, not actual map/movie content. No gamemd.exe is present
in this selected sample folder; it is a mod package, not a complete game install.

Compatibility matrix:

| Layer | Result |
| --- | --- |
| MIX container reading | Parsed sample archives and read rules/art successfully. |
| Artwork extraction | Technically readable; does not imply gameplay compatibility. |
| End-to-end mod profile/import | Not implemented or run; no mod enabled. |
| Fourth faction/custom armor/extension behavior | Unsupported for this package. |
| Ares/Syringe native loading | Windows x86 binaries cannot be native ARM64 plugins. |
| Mental Omega playable compatibility | NO; not advertised as supported. |

A separate original-Windows-game container is another execution architecture;
this audit did not install/run one or test the mod on a phone. Do not rename
archives or copy its files into the retail folder as a workaround. The readme
forbids reupload without permission; no mod file is redistributed or modified.

Primary references: https://mentalomega.com/index.php?page=install and
https://ares-developers.github.io/Ares-docs/ . These confirm original YR 1.001
and the Windows executable/injection dependency, not compatibility with this
native reconstruction. No proprietary or GPL implementation imported.

## Verification boundaries

Exact final counts, commits and Drive APK checksum are in the existing tracker
and BUILD_INFO.json. Native regressions and NDK compilation do not prove correct
phone pixels/audio. No connected-device/emulator/GPU/thermal test is claimed.
Save schema remains 55. All peers still need this build because animation behavior
changed. Credits, app ID and configured signing retained; no retail/mod assets,
GPL reference source or signing credentials included. APK delivery is Drive only.
