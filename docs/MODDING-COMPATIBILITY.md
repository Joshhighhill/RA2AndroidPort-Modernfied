# Yuri’s Revenge modding and interface research — 25 October 2026

This is a compatibility assessment, not a claim that existing Windows mods run on Android.

## What defines a unit

- `rulesmd.ini`: type registries and IDs; Strength/Armor; Speed/ROT; Locomotor CLSID; SpeedType and MovementZone; weapon slots, range/ROF/burst, projectiles, warheads and Verses; prerequisites, deployment, ownership and slave manager properties.
- `artmd.ini`: Image/Voxel, SHP sequences and timing, turret/barrel composition, HVA animation, FLH firing positions, buildup and special building animations.
- MIX archives contain the referenced SHP/VXL/HVA/TMP/PAL/CSF/audio resources. Maps supply cells, height/ramp data, bridges, starts, triggers and sometimes scenario INI overrides. `aimd.ini` supplies AI teams, scripts, task forces and triggers. These data categories are distinct from engine support for each tag.

Movement is not just Speed: terrain admissibility, locomotor turning/tracks, occupancy/reservations, path search and mission decisions must agree. Original CLSIDs choose built-in behavior; assigning a new CLSID cannot load a Windows locomotor into this Android binary. Preserve deterministic ticks; interpolate presentation separately.

Firing positions must resolve the selected normal/elite/secondary weapon and art image, rotate the appropriate FLH by turret orientation, include terrain/deck height, then use that launch event for both projectile and effects. A weapon with an invisible instantaneous projectile needs its actual beam/particle logic; adding a cannon ball to every attack would be wrong. Existing tank shells must remain protected by regression tests.

## Actual current fork seams

`src/asset_fs.cpp` supports top-level expandNN/ecacheNN/elocalNN artwork search and nested retail archives. This is not a complete mod profile loader. `engine/main.cpp::read_ra2md_file` reads rules/art/sound INIs from ra2md.mix and its explicit nested fallback list; it does not generally select a loose rulesmd.ini from a mod folder. Campaign map overrides are a separate existing path. Thus copying a mod beside the files does not imply its rules, artwork and multiplayer fingerprint are all active.

`src/rules.cpp`, `src/art.cpp`, `engine/locomotion.cpp`, `world_loco.cpp`, `weapon_resolve.cpp`, `weapon_art.hpp`, `voxel_raster.cpp`, `pres_bldanim.inc` and the mission/AI modules are extension points. A supported INI tag must be parsed, bound into saved simulation data, executed, presented and regression-tested; silently accepting a key is not compatibility.

## Safe staged mod support

1. Read-only base game plus an explicit separate mod overlay selected in the launcher. No edits to retail archives.
2. Validate archive paths, duplicate filenames, type references and required assets. Display unsupported tags/logics before launching; do not silently label an unsupported total conversion playable.
3. One consistent resolver for rules, art, audio, map and asset overrides. Record profile ID/version, ordered source hashes and engine version in diagnostics, saves and multiplayer compatibility checks.
4. First support small data-only balance/art/map packs using already implemented tags. Test load precedence, disable/revert, invalid references and deterministic same-profile save/load.
5. Add complex mechanics independently, feature by feature. Keep a compatibility matrix with complete/partial/unsupported states. Never advertise Mental Omega/Ares/Phobos compatibility just because an archive loads.

No general mod importer is added in build 900036. This roadmap requires a separate validated implementation.

## Ares, Phobos and Gagarin

Ares documentation says Syringe injects its DLL into the original Yuri’s Revenge 1.001 `gamemd.exe`. Phobos is an Ares-compatible engine extension. Their documented INI extensions describe new engine features; those features are not automatically present in the native fork. Their x86 Windows hooks are not loadable ARM64 Android engine plugins. A future emulation/container project would be a different execution architecture.

Use original owned retail data and captures for vanilla behavior; use community documentation as cross-checks, distinguishing extension defaults from original rules. Gagarin remains behavioral research only, pinned here to aedf638cea5e96a3ee8be5151296f41936192e44. Drive slope methods at 0x4AFAA0/0x4AF9B0 are partial orientation/timing references, not a complete Android renderer. BuildingClass UnloadBunker 0x4593A0 and ClearBunker 0x459470 are exit/animation-slot references, not proof of every entry mission. This pass does not run Gagarin’s byte-match harness or the original PC executable. No GPL implementation source is copied into this 0BSD fork.

## CnCNet interface

The CnCNet XNA/MonoGame client is GPL-3.0-or-later and supports moddable lobby graphics, game setup and spawning. Its Yuri package is an add-on to that client; public repository visibility alone is not permission to redistribute every skin/game asset. The client uses a spawner to launch the original game rather than providing that game’s in-match terrain/particle renderer.

Build 900036 offers an independently drawn **Command Center (experimental)** main menu via the Android launcher’s Game main menu selector. It uses no CnCNet source, branding or skin files. Original Yuri’s Revenge remains the default; existing submenus and actions remain available. This is a broad RTS command-hub presentation, not a pixel-identical CnCNet port or CnCNet multiplayer integration. Physical-device visual/accessibility verification is still required.

## Sources consulted

- https://github.com/CnCNet/xna-cncnet-client (README, licensing, runtime and interface role)
- https://github.com/CnCNet/cncnet-yr-client-package (official package role)
- https://github.com/CnCNet/client-mod-base (mod client package)
- https://github.com/CnCNet/yrpp-spawner (game launch/spawn role)
- https://ares-developers.github.io/Ares-docs/ (injection architecture)
- https://phobos.readthedocs.io/en/latest/New-or-Enhanced-Logics.html (extension logic and slave ownership options)
- https://modenc.renegadeprojects.com/Locomotor
- https://modenc.renegadeprojects.com/MovementZone
- https://modenc.renegadeprojects.com/SpeedType
- https://modenc.renegadeprojects.com/PrimaryFireFLH
- https://modenc.renegadeprojects.com/Enslaves
- https://modenc.renegadeprojects.com/HarvestRate
- https://github.com/r-melvin/gagarin/tree/aedf638cea5e96a3ee8be5151296f41936192e44

ModEnc and Phobos tags can describe bugs/extensions; they are references, not substitute proof of original PC behavior. The newly supplied slope/bunker videos do not show slave-worker collision. The user separately confirmed both mobile and deployed forms. Build 900036 implements an independently written, manager-linked passage exception with corridor, ore-scan, ownership, bridge-layer and saved occupancy regressions. This is not a general collision bypass or a byte-matched Gagarin implementation.
