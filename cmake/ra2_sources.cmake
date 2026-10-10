# Single source of truth for the ra2 source-file lists.
#
# Included by BOTH builds so they cannot drift:
#   * the desktop build  (top-level CMakeLists.txt)
#   * the Android build  (android/app/jni/CMakeLists.txt, packaged into libmain.so)
#
# RA2_ROOT is derived from this file's own location (cmake/ -> repo root), so it
# resolves correctly no matter which CMakeLists includes it.
get_filename_component(RA2_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# W2-AR-23: one version for both builds -- VERSION (semver) + the git commit,
# e.g. "0.9.0+g1a2b3c4" ("-dirty" with local edits). Shown in the window
# title, the Android logcat banner and `ra2engine --version`; the APK's
# versionName/versionCode come from the same VERSION file (app/build.gradle).
if(NOT DEFINED RA2_VERSION)
    file(READ "${RA2_ROOT}/VERSION" RA2_VERSION_BASE)
    string(STRIP "${RA2_VERSION_BASE}" RA2_VERSION_BASE)
    set(RA2_VERSION "${RA2_VERSION_BASE}")
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${RA2_ROOT}" describe --always --dirty --abbrev=7
                        OUTPUT_VARIABLE RA2_GIT_DESC OUTPUT_STRIP_TRAILING_WHITESPACE
                        RESULT_VARIABLE RA2_GIT_RC ERROR_QUIET)
        if(RA2_GIT_RC EQUAL 0 AND RA2_GIT_DESC)
            set(RA2_VERSION "${RA2_VERSION_BASE}+g${RA2_GIT_DESC}")
        endif()
    endif()
endif()

# ra2assets library (pure C++17, no SDL/OS deps).
set(RA2_LIB_SOURCES
    ${RA2_ROOT}/src/_placeholder.cpp
    ${RA2_ROOT}/src/blowfish.cpp
    ${RA2_ROOT}/src/mix_keyderiv.cpp
    ${RA2_ROOT}/src/mix.cpp
    ${RA2_ROOT}/src/pal.cpp
    ${RA2_ROOT}/src/shp.cpp
    ${RA2_ROOT}/src/pcx.cpp
    ${RA2_ROOT}/src/fnt.cpp
    ${RA2_ROOT}/src/ufnt.cpp
    ${RA2_ROOT}/src/lcw.cpp
    ${RA2_ROOT}/src/lzo1x.cpp
    ${RA2_ROOT}/src/mappack.cpp
    ${RA2_ROOT}/src/ini.cpp
    ${RA2_ROOT}/src/rules.cpp
    ${RA2_ROOT}/src/art.cpp
    ${RA2_ROOT}/src/theater_name.cpp
    ${RA2_ROOT}/src/asset_fs.cpp
    ${RA2_ROOT}/src/sounds.cpp
    ${RA2_ROOT}/src/map.cpp
    ${RA2_ROOT}/src/pkt.cpp
    ${RA2_ROOT}/src/theater.cpp
    ${RA2_ROOT}/src/land_type.cpp
    ${RA2_ROOT}/src/audio_bag.cpp
    ${RA2_ROOT}/src/wav.cpp
    ${RA2_ROOT}/src/hva.cpp
    ${RA2_ROOT}/src/vxl.cpp
    ${RA2_ROOT}/src/vpl.cpp
    ${RA2_ROOT}/src/csf.cpp
    ${RA2_ROOT}/src/fileio.cpp
    ${RA2_ROOT}/src/bink.cpp
    ${RA2_ROOT}/src/bink_audio.cpp
)

# ra2engine app/library sources (uses SDL). Same set feeds the desktop
# `ra2engine` executable and the Android `libmain.so`.
set(RA2_ENGINE_SOURCES
    ${RA2_ROOT}/engine/main.cpp
    ${RA2_ROOT}/engine/platform.cpp
    ${RA2_ROOT}/engine/platform_os.cpp
    ${RA2_ROOT}/engine/os_profiler.cpp
    ${RA2_ROOT}/engine/texture_cache.cpp
    ${RA2_ROOT}/engine/iso_project.cpp
    ${RA2_ROOT}/engine/iso_renderer.cpp
    ${RA2_ROOT}/engine/world.cpp
    ${RA2_ROOT}/engine/world_veterancy.cpp
    ${RA2_ROOT}/engine/pathfind.cpp
    ${RA2_ROOT}/engine/orig_math.cpp
    ${RA2_ROOT}/engine/input.cpp
    ${RA2_ROOT}/engine/input_original.cpp
    ${RA2_ROOT}/engine/hotkeys.cpp
    ${RA2_ROOT}/engine/touch_gestures.cpp
    ${RA2_ROOT}/engine/occupancy.cpp
    ${RA2_ROOT}/engine/group_move.cpp
    ${RA2_ROOT}/engine/combat.cpp
    ${RA2_ROOT}/engine/building.cpp
    ${RA2_ROOT}/engine/production.cpp
    ${RA2_ROOT}/engine/audio_mixer.cpp
    ${RA2_ROOT}/engine/music_stream.cpp
    ${RA2_ROOT}/engine/voxel_raster.cpp
    ${RA2_ROOT}/engine/facing_cache.cpp
    ${RA2_ROOT}/engine/sidebar.cpp
    ${RA2_ROOT}/engine/ore.cpp
    ${RA2_ROOT}/engine/harvest.cpp
    ${RA2_ROOT}/engine/factory.cpp
    ${RA2_ROOT}/engine/cargo.cpp
    ${RA2_ROOT}/engine/flight.cpp
    ${RA2_ROOT}/engine/ai_base.cpp
    ${RA2_ROOT}/engine/aimd_parse.cpp
    ${RA2_ROOT}/engine/ai_team.cpp
    ${RA2_ROOT}/engine/ai_trigger.cpp
    ${RA2_ROOT}/engine/superweapon.cpp
    ${RA2_ROOT}/engine/capture.cpp
    ${RA2_ROOT}/engine/visibility.cpp
    ${RA2_ROOT}/engine/skirmish.cpp
    ${RA2_ROOT}/engine/skirmish_lobby.cpp
    ${RA2_ROOT}/engine/rectfont.cpp
    ${RA2_ROOT}/engine/maplist.cpp
    ${RA2_ROOT}/engine/repair_sell.cpp
    ${RA2_ROOT}/engine/settings.cpp

    ${RA2_ROOT}/engine/projectile.cpp
    ${RA2_ROOT}/engine/rocket.cpp
    ${RA2_ROOT}/engine/map_trigger.cpp
    ${RA2_ROOT}/engine/campaign.cpp
    ${RA2_ROOT}/engine/veterancy.cpp
    ${RA2_ROOT}/engine/crate.cpp
    ${RA2_ROOT}/engine/roster2.cpp
    ${RA2_ROOT}/engine/abilities.cpp
    ${RA2_ROOT}/engine/world_abilities.cpp
    ${RA2_ROOT}/engine/world_w5.cpp
    ${RA2_ROOT}/engine/world_w6.cpp
    ${RA2_ROOT}/engine/abil_w5_rules.cpp
    ${RA2_ROOT}/engine/world_mcv.cpp
    ${RA2_ROOT}/engine/world_sw.cpp      # W2-AN superweapons v2
    ${RA2_ROOT}/engine/world_an.cpp      # W2-AN naval / aircraft
    ${RA2_ROOT}/engine/savegame.cpp
    ${RA2_ROOT}/engine/bridges.cpp
    ${RA2_ROOT}/engine/world_depth.cpp
    ${RA2_ROOT}/engine/inf_anim.cpp      # W3-B infantry DoType machine
    # Gap fix C (combat/units/movement)
    ${RA2_ROOT}/engine/weapon.cpp
    ${RA2_ROOT}/engine/weapon_unit.cpp
    ${RA2_ROOT}/engine/weapon_resolve.cpp
    ${RA2_ROOT}/engine/building_combat.cpp
    ${RA2_ROOT}/engine/unit_orders.cpp
    ${RA2_ROOT}/engine/play_script.cpp
)

# W6-06-16: THIRD_PARTY_NOTICES.md + LICENSE embedded in the engine for the
# Options > About page (a raw string literal per file, regenerated on every
# configure). Both builds add RA2_GENERATED_DIR to the engine's include path.
set(RA2_GENERATED_DIR "${CMAKE_BINARY_DIR}/ra2_generated")
file(MAKE_DIRECTORY "${RA2_GENERATED_DIR}")
file(READ "${RA2_ROOT}/THIRD_PARTY_NOTICES.md" _ra2_notices)
file(READ "${RA2_ROOT}/LICENSE" _ra2_license)
file(WRITE "${RA2_GENERATED_DIR}/ra2_notices.inc.tmp"
     "// Generated by cmake/ra2_sources.cmake (W6-06-16). Do not edit.\n"
     "static const char kRa2ThirdPartyNotices[] = R\"RA2NOTICES(${_ra2_notices})RA2NOTICES\";\n"
     "static const char kRa2License[] = R\"RA2NOTICES(${_ra2_license})RA2NOTICES\";\n")
configure_file("${RA2_GENERATED_DIR}/ra2_notices.inc.tmp" "${RA2_GENERATED_DIR}/ra2_notices.inc" COPYONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${RA2_ROOT}/THIRD_PARTY_NOTICES.md" "${RA2_ROOT}/LICENSE")
