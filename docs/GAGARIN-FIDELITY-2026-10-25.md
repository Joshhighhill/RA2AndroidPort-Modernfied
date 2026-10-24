# Fidelity review and implementation — 25 October 2026

EA has not endorsed and does not support this product.

Starting fork: `bf790336ebd92caf2c04818b4d9db53eb1be7917` on the published
`android-touch` branch. Gagarin reference:
`03f19f62e61c731ff804cfbd0471d2d11c931aa5` at
https://github.com/r-melvin/gagarin . The existing private research report is
the issue baseline; its research-only instructions are historical, not this
phase's scope. No Gagarin source is incorporated. Algorithms below are small
changes to the existing port, based on the behavior specification and tests.

## Supported changes

R-001 / R-017: `engine/world_depth.cpp` previously ran the civilian
ConditionRed evacuation branch for `NABNKR`. It now retains that structure's
infantry at critical health. Civilian evacuation, the existing low-health
entry rejection, manual unload and Yuri Tank Bunker rules remain unchanged.
Destruction/occupant death parity remains a separate unresolved question;
this patch does not claim to correct it.

R-002 / R-026: `sync_garrison_owner` previously neutralized an empty Soviet
Battle Bunker when a neutral house existed. After automatic or manual unload,
the owner-only `set_repair` path could consequently reject a player click.
`NABNKR` now retains its owner just as constructed Yuri Tank Bunkers already
did. Tests reproduce empty ownership after unloading, repair eligibility at
1 HP and rejection of a different house. This addresses the identified owner
loss; it does not establish that every reported repair-click failure had the
same cause.

The exception is deliberately limited to the stock `NABNKR` type ID. It does
not classify every occupied or YuriCountry-owned civilian as a military
building and does not add fields to the save/network format. Custom bunker
types are not automatically given this exception.

Tests compare civilian and military occupancy after damage, verify original
civilian ownership/evacuation behavior, and advance original/resumed worlds
for 120 fixed ticks before comparing their full serialized states. The full
native suite also exercises existing deterministic simulation/network checks.
These are engine tests, not proof of original PC visual parity.

## Comparison evidence and limits

Pinned Gagarin `match/economy/tu038_building_class.cpp` and
`match/combat/building_class_occupy_methods.h` describe occupancy eligibility;
the inspected path retains a red-health entry check. Its building update
source separates building damage visuals from Gatling frame advancement.
Its `STATUS.md` records 10,204 matched rows / 18.2% of listed function bytes,
with M2 hybrid runnable build not started. These statistics are the status
file's dated snapshot, not independently rerun matching results.

The research report's ClearBunker/UnloadBunker, slope-transition, particle
and animation-owner crosswalks remain useful investigation pointers. No
full Gagarin build, original gamemd execution, aligned screenshot comparison,
emulator run or real POCO X7 Pro test was performed in this phase.

| Research items | Result in this phase |
| --- | --- |
| R-001, R-017 | Battle Bunker low-health retention patched and engine-tested; PC comparison still needed |
| R-002, R-026 | Owner-loss repair cause patched; full UI hitbox diagnosis remains open |
| R-003, R-025 | Purple walls/palette not established as an error; no speculative recoloring |
| R-004, R-013, R-014 | Slope/voxel anchoring requires matched ground/bridge captures; unchanged |
| R-005, R-015, R-016 | Other occupancy, building slot and construction parity remains open |
| R-006, R-018 | Existing tank entry/exit patches preserved; original timing/offset parity unverified |
| R-007 | Miner defensive-attack mission resume needs a reproducible trace; unchanged |
| R-008, R-022 | Psychic Reveal exact feedback art and broader powers remain unverified |
| R-009, R-019, R-023 | Existing test 28 projectile/Gatling patches preserved; no claim of all FX parity |
| R-010 | Soviet defense composition not proven erroneous; unchanged |
| R-011, R-012 | Anim lifetime and infantry sequence audit remains incomplete |
| R-020, R-021, R-024 | Particles, attached FX and special locomotion remain open |

## Launcher and notices

First-use terms are versioned locally and require an initially unchecked
file-rights confirmation before import/start. Decline exits; reopening terms
and full notices is supported. No proof-of-purchase or game archive upload is
requested. Diagnostic sharing stays user initiated. The confirmation is not
an ownership verification or immunity from infringement claims.

Omar's original LICENSE is unchanged. Bryle13 is credited for Android fork
modifications. Existing SDL/stb notices are retained and LLVM's full runtime
license text is added to the APK. No claim is made of a comprehensive upstream
provenance audit, legal clearance or confirmed EA authorization for the port.

## Delivery

Build 900029, ARM64, existing `org.ra2.engine` application ID and configured
signer. Save format stays 53. APK delivery is Google Drive only; no new GitHub
Release is created. Exact commits, automated results, APK hash/size/version
and verified Drive location are recorded in the existing Google Docs report
and the local delivery manifest after the build completes.
