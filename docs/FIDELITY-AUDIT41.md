# Build 900041: bunker, commands and faction presentation

Baseline: android-touch ea7ad958800724abc71cd690a0d501f45578e229.
Original-game authority: user-owned Yuri's Revenge v1.001 art/rules and the
provided PC bunker recording. No retail assets or reconstructed source included.

## Evidence read

RA2YRINST.txt (42,952 characters), existing living tracker and latest handoff
`RA2AndroidPort — Latest Release Visuals, Force Fire, Miner EVA & Mod Support`
were read. New diagnostics: Drive 1UuQvlrRjNkA8vjK1Wp44-A-uoVpwvQvH,
created 2026-10-25T15:05:41Z, version 900040 / -80-gea7ad95, Android 16.
All text members reviewed: current engine 780,506 bytes / 5,968 lines,
previous 2,024 bytes / 27 lines, skirmish 600 lines, report/device/exit metadata.
Asset logs empty. Description absent, low_memory=false, available 4,269,654,016
bytes. Current log ends ENGINE NORMAL EXIT code=0 at frame 14880. No fatal,
assertion-failed or signal marker found in text. Largest sampled wait=3 seconds;
this is not proof that every traffic problem is solved. Historical binary traces
were preserved but not re-symbolicated. Existing capability warnings include
SpyPlane, ForceShield, GeneticConverter and PsychicDominator.

Newest MP4 1UP5_qZ4854thre-yHYGY5fC06f7Z8G-K is 6,492,721 bytes, 6.64 seconds,
1280x720 HEVC, reported 61.72 fps. Entire recording decoded; contact sheet and
1/1.5/2/3/3.5/4/4.5/5-second close-ups inspected. At 1.5–3 seconds the tank
sits high on the platform; at 4–4.5 seconds the base draws over its departing
body. The user identifies this as latest-build footage. The older 13.15-second
PC reference and earlier Android footage remain distinct evidence.

## R-054 / R-006: bunker rendering

Confirmed source defect: physical departures introduced in 900039 were excluded
from the old synthetic departure pass, yet remained in ordinary object sorting.
Their key can precede the whole building, causing the platform to overpaint them.
Approaching/physical departing vehicles now share the building composite while
inside its footprint. Front-half travel draws after the lowered lip; attached
occupants remain between rear and front walls. Normal pathfinding, ownership,
arrival/hull alignment and lower-before-release behavior remain authoritative.

The integral path socket and art platform centre are distinct: display-only
half-cell southeast alignment adds 15 projected vertical pixels at the socket,
fading continuously over the final cell. It is shared by approach/attachment/
departure without changing authoritative coordinates or gameplay. This is an
independent visual calibration against the supplied footage, not a claim that
the original reads its explicitly-unused DockingOffset0 field. Exact appearance,
large vehicles, damaged art and all approach directions require phone acceptance.

Gagarin research pin: aedf638cea5e96a3ee8be5151296f41936192e44.
`match/combat/building_class_clear_bunker_methods.cpp`, ClearBunker 0x459470,
declares bytes diffed against retail and manages slots 10/11 then 12/13.
`building_class_unload_bunker_methods.h`, UnloadBunker 0x4593A0, provides layouts
and calls; prior crosswalk describes exit coordinates. These do not prove entry
path, platform projection or full renderer timing. No match harness was rerun.
GPL reference implementation was not copied into this 0BSD project.

## R-064: explicit building attack / Force Fire

Confirmed gaps: selected non-garrison defenses had no attack input path; command
dispatch only accepted firing garrisons and rejected friendly/ground targets.
Explicit building attack state now travels through the existing command stream,
including an explicit-force sentinel in Attack's unused destination field.
Final codec review found that the generic value field is not transmitted for
MegaMission; the destination survives both wire encoding and run grouping.
Range, target
domain, armor, owner, power, turret rotation, reload and Stop remain gates.
Capture cancels an old owner's order. Non-weapon structures remain unable to fire.
Android FIRE and STOP are enabled for selected combat structures; Ctrl-click is
the desktop equivalent. Ordinary auto-acquisition is unchanged without an order.

Ground fire has an explicit visual endpoint. Defense projectile launches use
their resolved turret FLH and alternating burst lateral offset. Ordinary mobile
vehicle projectile code is unchanged. Firing garrisons keep their existing
instant-hit delivery; full visible bunker projectile flight is still deferred.
Charged/support Prism explicit orders are deliberately refused while their
original auto-fire pipeline is preserved; this remains an incomplete case.
Psychic Tower accepts only a valid unit target, never ground/building control.
Original PC command legality is the behavioral target; this pass did not run
the original executable or prove complete Gagarin combat parity.

## R-065: Yuri faction picker and loading screen

Verified privately from retail archives: `ra2md.mix/localmd.mix/yrii.pcx`
(1,385 bytes), `ra2md.mix/loadmd.mix/ls800yuri.shp` (481,232 bytes), and default
`ra2.mix/cache.mix/mpls.pal` (768 bytes). The old table omitted Yuri entirely.
Picker uses the authentic flag; loading resolves Yuri instead of Allied fallback
and reads its actual palette location. Missing flag retains an independent Y
badge and warning. All files are loaded from user installation, never bundled.
Other countries' palettes and aspect fitting are preserved.

## R-066: deployed Slave Miner alert

Building damage events formerly always selected Base Under Attack. Slave-manager
types now route to authentic EVA_OreMinerUnderAttack and radar category 4, without
the base siren. Other buildings and allied alerts retain their existing path.
This is the requested harvesting-role UX correction; retail deployed-miner EVA
parity has not been established. Health/type/combat and per-event throttling
are unchanged. No broad three-second mute was added.
The requested role alert is enabled by default; advanced configuration
`RA2.INI [Audio] MinerRoleAlerts=no` restores the prior building alert. This
preference is presentation-only and is not part of the network simulation.

## Specialized FX and mod compatibility

Scoped correction covers defense muzzle coordinates and explicit beam endpoints;
it does not claim Boomer/V3/Nuke/Magnetron/Disc/Chaos or all particle visuals fixed.
Newest logs do not establish a distinct root cause for each of those categories.
Preserve existing ordinary shells and verify them on-device alongside the bunker.

Mod investigation remains separate: current src/asset_fs.cpp handles artwork
archive precedence, while main.cpp read_ra2md_file uses its own INI path. A loose
mod folder is not consistently applied to all game data. Tier A small data-only
mods may be feasible after a unified resolver/profile/hash/import validation;
tier B complete vanilla rule compatibility remains partial; tier C Ares/Phobos/
Mental Omega is unsupported. No mod loading behavior changed in this release.
No complex mod or third-party mod package was tested. See MODDING-COMPATIBILITY.md.

Sources: https://github.com/chronodivide/mod-sdk (documented engine-specific
rules/art and constraints, not proprietary renderer source),
https://phobos.readthedocs.io/en/latest/Fixed-or-Improved-Logics.html (Windows
extension fixes, not native ARM64 plugins), pinned Gagarin sources above.

## Validation and delivery

Exact final test counts, commit IDs, APK checksum/size/version and verified Drive
link are recorded in the same research tracker and delivered BUILD_INFO.json.
Initial full run caught Stop routing, synthetic-map ground validation and omitted
FireEvent save visitor fields. Corrected before final regression and delivery.
New serialized commands and ground-fire endpoints require save schema 55;
schema-54 saves are refused by the existing version gate. Keep backups, start a
fresh skirmish and use the same build on all network peers.

No phone/emulator/GPU/thermal verification is claimed. Credits, license notices,
app ID and existing configured signing are preserved. No GPL/proprietary source,
retail content or credentials included. Google Drive is the APK delivery target;
no GitHub Release or other file host upload is authorized for this workflow.
