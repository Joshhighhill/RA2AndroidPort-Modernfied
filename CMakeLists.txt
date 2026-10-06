cmake_minimum_required(VERSION 3.20)
project(ra2assets LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Library + engine source lists live in cmake/ra2_sources.cmake, a single
# source of truth shared with the Android jni build (android/app/jni) so the
# two builds cannot drift. It defines RA2_LIB_SOURCES and RA2_ENGINE_SOURCES.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ra2_sources.cmake)

# Library sources, shared between the static `ra2assets` target and the
# optional `ra2assets_so` shared target below so both actually contain the
# real library code (not just the placeholder).
set(RA2_SOURCES ${RA2_LIB_SOURCES})

add_library(ra2assets STATIC)
target_include_directories(ra2assets PUBLIC include)
target_sources(ra2assets PRIVATE ${RA2_SOURCES})

# NOTE: src/mix_keyderiv.cpp previously needed per-file
# `-fno-strict-aliasing;-fsigned-char` because the old verbatim XCC port did
# `*(word*)` limb type-punning and relied on plain `char` being signed. The
# clean-room reimplementation uses only uint8_t byte handling and no
# type-punning, so those flags are no longer required and were removed.

option(RA2_BUILD_SHARED "Build ra2assets as a shared lib (for Android .so check)" OFF)
if (RA2_BUILD_SHARED)
    set_target_properties(ra2assets PROPERTIES POSITION_INDEPENDENT_CODE ON)
    # Built from the same RA2_SOURCES as the static `ra2assets` target (not a
    # thin wrapper that links the static archive) so the resulting
    # libra2assets.so actually contains the library's real compiled code —
    # this is what the Android ELF/portability check below inspects.
    add_library(ra2assets_so SHARED ${RA2_SOURCES})
    target_include_directories(ra2assets_so PUBLIC include)
    set_target_properties(ra2assets_so PROPERTIES OUTPUT_NAME ra2assets)
endif()

enable_testing()
add_executable(ra2assets_tests
    tests/test_main.cpp
)
target_sources(ra2assets_tests PRIVATE tests/test_blowfish.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_mix.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_pal.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_shp.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_pcx.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_oracle.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_lcw.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_mappack.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_lzo1x.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_ini.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_rules.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_roster.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_art.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_theater_name.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_sounds.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_map.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_pkt.cpp)
# maplist.cpp is pure C++17 (no SDL): the pkt -> existence -> mode -> CSF map
# catalog. Reached only by the skirmish setup UI and --demo-maps, never by a
# gate path (maps.md §7 rule 5).
target_sources(ra2assets_tests PRIVATE tests/test_maplist.cpp engine/maplist.cpp)
# G0 --play (agent G): the script language (ra2play) half of engine/play_script.cpp.
target_sources(ra2assets_tests PRIVATE tests/test_play_lang.cpp)
target_include_directories(ra2assets_tests PRIVATE engine)
target_sources(ra2assets_tests PRIVATE tests/test_theater.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_land_type.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_audio.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_wav.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_vxl.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_vpl.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_csf.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_fnt.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_bink.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_fileio.cpp)
# iso_project.cpp is pure C++17 (no SDL) and lives under engine/ so it can be
# compiled into BOTH this test binary and the ra2engine executable below
# from a single source of truth (see engine/iso_project.hpp for rationale).
target_sources(ra2assets_tests PRIVATE tests/test_projection.cpp engine/iso_project.cpp)
# pathfind.cpp is pure C++17 (no SDL) and lives under engine/ so it can be
# compiled into BOTH this test binary and the ra2engine executable below
# from a single source of truth (same pattern as iso_project.cpp above).
target_sources(ra2assets_tests PRIVATE tests/test_pathfind.cpp engine/pathfind.cpp)
# world.cpp is pure C++17 (no SDL) -- its passability-grid builder + coordinate
# reconciliation are unit-tested here (test_world.cpp) alongside the projection
# it must agree with. Compiled from the same single source of truth as the
# ra2engine executable below (same pattern as iso_project.cpp / pathfind.cpp).
target_sources(ra2assets_tests PRIVATE tests/test_world.cpp engine/world.cpp
                                       engine/world_veterancy.cpp  # P5 World members
                                       tests/test_world_veterancy.cpp)
# input.cpp is SDL-free (only InputEvent, screen_to_cell, World -- see
# engine/input.hpp) so it compiles into this test binary too (Task 12 review
# Finding 3: Input previously had zero test coverage); it calls
# terrain_screen_bounds, which is defined in engine/world.cpp (already a
# source above) rather than engine/iso_renderer.cpp so this stays SDL-free.
target_sources(ra2assets_tests PRIVATE tests/test_input.cpp engine/input.cpp engine/input_original.cpp
    tests/test_input_original.cpp)
# G-04: KeyboardMD.ini hotkey table.
target_sources(ra2assets_tests PRIVATE tests/test_hotkeys.cpp engine/hotkeys.cpp)
# G-70/G-71: touch gesture layer.
target_sources(ra2assets_tests PRIVATE tests/test_touch_gestures.cpp engine/touch_gestures.cpp)
# occupancy.cpp is a pure C++17 cell-ownership grid (no SDL) used by the
# multi-unit movement/collision logic; compiled into both this test binary and
# ra2engine from one source of truth (same pattern as pathfind.cpp/world.cpp).
target_sources(ra2assets_tests PRIVATE tests/test_occupancy.cpp engine/occupancy.cpp)
# group_move.cpp is pure C++17 (no SDL): formation target assignment for a
# group move order. Compiled into both binaries from one source of truth.
target_sources(ra2assets_tests PRIVATE tests/test_group_move.cpp engine/group_move.cpp)
# test_movement.cpp exercises the pure step_units collision stepper defined in
# engine/world.cpp (already a source above) against hand-built grids/units --
# no MapFile needed, so the head-on/boxed-in/funnel cases are deterministic.
target_sources(ra2assets_tests PRIVATE tests/test_movement.cpp)
# test_vehicle.cpp exercises the vehicle locomotion additions (ROT turn-in-
# place, per-unit speed, turret slew, aim-gated fire) in engine/world.cpp and
# engine/combat.cpp (both already sources above -- no new source needed).
target_sources(ra2assets_tests PRIVATE tests/test_vehicle.cpp)
# combat.cpp is pure C++17 (no SDL): armor_index/damage_vs/step_combat, the
# targeting/ROF/damage/death/pursuit-request core (docs/re/combat.md).
# Compiled into both this test binary and ra2engine from one source of truth
# (same pattern as pathfind.cpp/world.cpp/occupancy.cpp/group_move.cpp).
target_sources(ra2assets_tests PRIVATE tests/test_combat.cpp engine/combat.cpp)
# Gap fix C: weapon slots / per-target pick / slot resolution.
target_sources(ra2assets_tests PRIVATE engine/weapon.cpp engine/weapon_unit.cpp
                                       engine/weapon_resolve.cpp engine/building_combat.cpp
                                       engine/unit_orders.cpp
                                       tests/test_gap_combat.cpp)
# building.cpp is pure C++17 (no SDL): BuildingTypeDef/Building struct defs,
# foundation_cells row-major enumeration, placement_check validity (in-grid,
# passable, unoccupied, no overlap, adjacency), building_frame/buildup_frame
# for SHP frame selection, step_buildings state machine (Buildup->Idle->Dead->
# tombstone), and kill_building marking + die-anim timing (docs/re/building-
# render.md). Compiled into both this test binary and ra2engine from one source
# of truth (same pattern as combat.cpp above).
target_sources(ra2assets_tests PRIVATE tests/test_building.cpp engine/building.cpp)
# audio_mixer.cpp is pure C++17 (no SDL/asset deps): the software mixer --
# SoundBank storage, keyed voice registry, Limit/interrupt/priority pool
# rules, linear attenuation + GLOBAL floor, and the EVA FIFO queue
# (docs/re/audio-playback.md). Compiled into both this test binary and
# ra2engine from one source of truth (same pattern as combat.cpp above).
target_sources(ra2assets_tests PRIVATE tests/test_audio_mixer.cpp engine/audio_mixer.cpp)
# voxel_raster.cpp is the pure C++17 voxel rasterizer (no SDL): VXL/HVA model +
# yaw + voxels.vpl lighting + palette -> RGBA sprite (docs/re/voxel-render.md).
# Compiled into both this test binary and ra2engine from one source of truth
# (same pattern as combat.cpp above).
target_sources(ra2assets_tests PRIVATE tests/test_voxel_raster.cpp engine/voxel_raster.cpp)
# facing_cache.cpp is the pure C++17 load-time voxel facing cache (no SDL): it
# rasterizes each of kVehicleFacings quantized yaws once at load (build_facing_set)
# so the draw path can blit cached sprites (docs/re/voxel-render.md §2). Compiled
# into both this test binary and ra2engine from one source of truth (same pattern
# as voxel_raster.cpp above).
target_sources(ra2assets_tests PRIVATE tests/test_facing_cache.cpp engine/facing_cache.cpp)
# production.cpp is pure C++17 (no SDL): construction economy — the single
# BuildingType queue, credits, and power (docs/re/construction.md §§7-10).
# Compiled into both this test binary and ra2engine from one source of truth
# (same pattern as combat.cpp above).
target_sources(ra2assets_tests PRIVATE tests/test_production.cpp engine/production.cpp)
# sidebar.cpp is pure C++17 (no SDL): RA2 sidebar geometry + hit-testing
# (docs/re/construction.md §11). Compiled into both this test binary and ra2engine
# from one source of truth (same pattern as combat.cpp above).
target_sources(ra2assets_tests PRIVATE tests/test_sidebar.cpp engine/sidebar.cpp)
# ore.cpp is pure C++17 (no SDL): per-cell tiberium kind/density/variant from
# the map's overlay, harvester depletion, and the growth/spread timer
# (docs/re/economy.md §§4-5). Compiled into both this test binary and ra2engine
# from one source of truth (same pattern as combat.cpp above).
target_sources(ra2assets_tests PRIVATE tests/test_ore.cpp engine/ore.cpp)
# harvest.cpp is pure C++17 (no SDL): the harvester mission FSM -- seek ore/
# gulp/return/dock/unload, CMIN chrono warp (docs/re/economy.md §§1-3).
# Compiled into both this test binary and ra2engine from one source of truth
# (same pattern as ore.cpp/combat.cpp above).
target_sources(ra2assets_tests PRIVATE tests/test_harvest.cpp engine/harvest.cpp)
# factory.cpp is pure C++17 (no SDL): factory exit-cell (8-ring walk to the
# first free door cell + ExitCoord), primary-factory selection, and set-primary
# (docs/re/production.md §1). Compiled into both this test binary and ra2engine
# from one source of truth (same pattern as harvest.cpp above).
target_sources(ra2assets_tests PRIVATE tests/test_factory.cpp engine/factory.cpp)
# cargo.cpp is pure C++17 (no SDL): CB-1 transport/cargo -- the COUNT
# capacity can_load predicate, LIFO next_eject, the facing-scatter eject_cell
# picker, and the pip-only cargo_total_size metric
# (docs/re/transport-cargo.md). Compiled into both this test binary and
# ra2engine from one source of truth (same pattern as factory.cpp above).
target_sources(ra2assets_tests PRIVATE tests/test_cargo.cpp engine/cargo.cpp)
# flight.cpp is pure C++17 (no SDL): CB-2 Aircraft -- the step_altitude
# climb/descend integrator, the straight-line cruise_step, the shortest-arc
# heading_toward, altitude_to_screen_dy, and advance_flight_state
# (docs/re/aircraft.md). Compiled into both this test binary and ra2engine
# from one source of truth (same pattern as cargo.cpp above).
target_sources(ra2assets_tests PRIVATE tests/test_flight.cpp engine/flight.cpp)
# ai_base.cpp is pure C++17 (no SDL): the AI-1 base-planning brain --
# difficulty mods, urgency ordering (next_build), placement scan
# (find_build_location) over a read-only BaseView (docs/re/ai-base.md).
# Compiled into both this test binary and ra2engine from one source of truth
# (same pattern as factory.cpp above).
target_sources(ra2assets_tests PRIVATE tests/test_ai_base.cpp engine/ai_base.cpp)
# aimd_parse.cpp is pure C++17 (no SDL): the AI-2 aimd.ini parser --
# TaskForce/ScriptType/TeamType tables via the existing ra2assets::Ini
# ordered-section walk (docs/re/ai-team.md §1). Compiled into both this test
# binary and ra2engine from one source of truth (same pattern as ai_base.cpp).
target_sources(ra2assets_tests PRIVATE tests/test_aimd.cpp engine/aimd_parse.cpp)
# ai_team.cpp is pure C++17 (no SDL): the AI-2 offensive-team brain -- lifecycle
# (Recruiting/Gathering/Executing/Disbanding) + the ScriptType interpreter over
# a read-only TeamView (docs/re/ai-team.md §2/§3). Compiled into both this test
# binary and ra2engine from one source of truth (same pattern as aimd_parse.cpp).
target_sources(ra2assets_tests PRIVATE tests/test_ai_team.cpp engine/ai_team.cpp)
# ai_trigger.cpp is pure C++17 (no SDL): the AI-3 AITriggerTypes weighted
# trigger engine -- condition eval + enable filters + weighted-random selection
# + adaptive weight dynamics over a read-only TriggerView (docs/re/ai-trigger.md).
# Compiled into both this test binary and ra2engine from one source of truth
# (same pattern as ai_team.cpp above).
target_sources(ra2assets_tests PRIVATE tests/test_ai_trigger.cpp engine/ai_trigger.cpp)
# superweapon.cpp is pure C++17 (no SDL/World/RNG): the CB-4 SuperweaponType enum
# (recovered [SuperWeaponTypes] roster) + the per-house recharge-timer lifecycle
# (tick/is_ready/on_fired/charge_fraction) + the id<->Type mapping helpers
# (docs/re/superweapon.md §1/§4). Header-inline lifecycle; the .cpp holds only the
# enum<->id map, exercised by test_superweapon.cpp.
target_sources(ra2assets_tests PRIVATE tests/test_superweapon.cpp engine/superweapon.cpp)
# Engineer/Spy Capture phase [RE: capture.md]: pure capture arithmetic
# (cash-steal/blackout/veteran/startup). capture.cpp is header-inline (one anchor
# symbol); the World mutation entry points live in engine/world.cpp (already on
# test_world's line), so capture.cpp is NOT added to test_world's line.
target_sources(ra2assets_tests PRIVATE tests/test_capture.cpp engine/capture.cpp)
# Shroud/Fog phase [RE: shroud.md]: pure near-circular reveal math
# (engine/visibility.cpp) exercised by test_visibility.cpp; the World-layer
# visibility grids + step_visibility live in engine/world.cpp (already on
# test_world's line).
target_sources(ra2assets_tests PRIVATE tests/test_visibility.cpp engine/visibility.cpp)
# Repair & Sell phase [RE: repair-sell.md]: pure repair/sell arithmetic + the
# refusal predicates (14-frame global cadence, the two-integer-division
# stepCost, the FLAT refund, the survivor divisors, the mirrored buildup
# frame). World-free -- it sees only Building/BuildingTypeDef, like
# production.cpp. The World mutation entry points live in engine/world.cpp
# (already on test_world's line), so world.cpp is NOT added here.
target_sources(ra2assets_tests PRIVATE tests/test_repair_sell.cpp engine/repair_sell.cpp)
# Projectile flight phase (P2) [RE: projectile.md]: the pure BulletClass core --
# launch solver (Fire), ballistic/vertical integrators (Update), scatter, and the
# deterministic projectile RNG. World-free; the collision/detonation pass lives in
# engine/world.cpp (already on test_world's line).
target_sources(ra2assets_tests PRIVATE tests/test_projectile.cpp engine/projectile.cpp)
# P9 Map triggers [RE: map-triggers.md]: the pure map-trigger parser + runtime
# (TagClass/TriggerClass/TEvent/TAction semantics over an abstract TriggerHost).
# World-free; compiled into both this test binary and ra2engine.
target_sources(ra2assets_tests PRIVATE tests/test_map_trigger.cpp engine/map_trigger.cpp)
# P10 Campaign missions [RE: campaign.md]: control files, [Basic], object
# records, BorrowedTime mission end, map-team script interpreter. World-free.
target_sources(ra2assets_tests PRIVATE tests/test_campaign.cpp engine/campaign.cpp)
# P5 Veterancy + Crates [RE: veterancy.md]: the pure veterancy formulas and the
# crate RNG/powerup math. engine/world.cpp (already on test_world's line) calls
# into both, so they are compiled into the test binary here once.
target_sources(ra2assets_tests PRIVATE tests/test_veterancy.cpp engine/veterancy.cpp
                                       tests/test_crate.cpp engine/crate.cpp)
# P6 Roster breadth + remaining producers [RE: roster2.md]: the PURE wall
# (mask/frame/line-fill/3x time), water-bound naval yard (placement/exit/
# category split) and airfield (pad coords, 270-frame rearm, sortie) mechanics.
# World-free; factory.cpp/production.cpp are already on earlier test lines.
target_sources(ra2assets_tests PRIVATE tests/test_roster2.cpp engine/roster2.cpp)
# P4 Unit special abilities [RE: abilities.md]: the pure ability arithmetic +
# the radiation field (engine/abilities.cpp, World-free) and the World-side
# ability pass + orders (engine/world_abilities.cpp, a World member TU split out
# of world.cpp), exercised by test_abilities.cpp.
target_sources(ra2assets_tests PRIVATE tests/test_abilities.cpp tests/test_abilities_world.cpp engine/abilities.cpp engine/world_abilities.cpp engine/world_mcv.cpp tests/test_mcv_deploy.cpp)

# Skirmish Shell: the PURE config->World core (SkirmishConfig/configure_skirmish_
# houses/default_skirmish_config). engine/world.cpp is already on test_world's
# line above, so only the new module + its test are added here.
target_sources(ra2assets_tests PRIVATE tests/test_skirmish.cpp engine/skirmish.cpp)
# rectfont.cpp is pure C++17 (no SDL): the 5x7 rect-font glyph table + glyph_rows
# extending the kDigits technique to A-Z/0-9/space/basic punctuation for menu
# labels (Task 5, [RE: skirmish.md §UI]). Its coverage cases live in
# test_skirmish.cpp above; draw_text (the SDL consumer) lives in engine/main.cpp.
target_sources(ra2assets_tests PRIVATE engine/rectfont.cpp)
# P8 Options + persistence: the PURE settings layer (RA2.INI IniDoc round-trip,
# OptionsClass defaults/clamps/save list, slider/trackbar math, the recovered
# edge-scroll table) [RE: docs/re/options.md]. SDL-free; main.cpp owns the UI.
target_sources(ra2assets_tests PRIVATE tests/test_settings.cpp engine/settings.cpp)
# G0 --play harness: the pure script parser / assertion evaluator / play_hash.
target_sources(ra2assets_tests PRIVATE tests/test_play_script.cpp engine/play_script.cpp)
# P7 Save/Load [RE: docs/re/saveload.md]: the archive core, field visitors,
# file container + slots. SDL-free.
target_sources(ra2assets_tests PRIVATE tests/test_saveload.cpp engine/savegame.cpp)
# The saveload source-scan tripwire reads engine/*.hpp to prove every World /
# MapTriggerSystem data member is visited by the serializer.
target_compile_definitions(ra2assets_tests PRIVATE RA2_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(ra2assets_tests PRIVATE third_party include engine)
target_link_libraries(ra2assets_tests PRIVATE ra2assets)
add_test(NAME ra2assets_tests COMMAND ra2assets_tests)

add_executable(ra2extract tools/ra2extract.cpp)
target_include_directories(ra2extract PRIVATE third_party)
target_link_libraries(ra2extract PRIVATE ra2assets)

option(RA2_BUILD_ENGINE "Build the SDL2 engine/app (needs SDL2)" ON)
if (RA2_BUILD_ENGINE)
  include(FetchContent)
  FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.30.9)
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  set(SDL_SHARED ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(SDL2)
  # Engine source list comes from cmake/ra2_sources.cmake (shared with the
  # Android jni build so the two cannot drift).
  add_executable(ra2engine ${RA2_ENGINE_SOURCES})
  # audio_sdl.cpp is main.cpp's SDL audio-sink companion (Task 6 of the Audio
  # Playback phase, task-6-brief.md Step 1/Files). Added directly to this
  # target (and to android/app/jni/CMakeLists.txt) rather than into the
  # shared RA2_ENGINE_SOURCES list in cmake/ra2_sources.cmake, per the task
  # brief -- it is not part of ra2assets_tests either way.
  target_sources(ra2engine PRIVATE engine/audio_sdl.cpp)
  # third_party for the vendored stb_image_write.h (screenshot PNG output);
  # STB_IMAGE_WRITE_IMPLEMENTATION is defined in exactly one TU (engine/main.cpp).
  target_include_directories(ra2engine PRIVATE include engine third_party)
  # NOTE: link order deviates from the brief's literal listing (SDL2 then
  # SDL2main) to (SDL2main then SDL2). Under MinGW/GNU ld, static archives
  # are resolved in a single left-to-right pass: SDL2main.a's WinMain is
  # pulled in via `-Wl,--undefined=WinMain` and internally references SDL_*
  # symbols (SDL_Init, SDL_free, ...) that live in SDL2's import lib. If
  # SDL2main.a is placed after SDL2's import lib on the command line, GNU ld
  # has already finished scanning the SDL2 import lib by the time it
  # discovers those undefined refs and does not revisit it, so the link
  # fails ("undefined reference to SDL_ShowSimpleMessageBox" etc). Listing
  # SDL2main before SDL2 (the standard `-lSDL2main -lSDL2` convention used
  # by every MinGW SDL2 tutorial) fixes this; MSVC's linker is not
  # order-sensitive this way, which is why the brief's literal order can
  # look fine there. No SDL2 version/flags changed.
  target_link_libraries(ra2engine PRIVATE ra2assets SDL2::SDL2main SDL2::SDL2)
  # SDL_SHARED=ON builds SDL2.dll; copy it next to ra2engine.exe post-build
  # so the exe can find it at runtime (Windows has no rpath).
  add_custom_command(TARGET ra2engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:ra2engine>)
endif()
