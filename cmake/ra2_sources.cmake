# Single source of truth for the ra2 source-file lists.
#
# Included by BOTH builds so they cannot drift:
#   * the desktop build  (top-level CMakeLists.txt)
#   * the Android build  (android/app/jni/CMakeLists.txt, packaged into libmain.so)
#
# RA2_ROOT is derived from this file's own location (cmake/ -> repo root), so it
# resolves correctly no matter which CMakeLists includes it.
get_filename_component(RA2_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

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
    ${RA2_ROOT}/src/lcw.cpp
    ${RA2_ROOT}/src/lzo1x.cpp
    ${RA2_ROOT}/src/mappack.cpp
    ${RA2_ROOT}/src/ini.cpp
    ${RA2_ROOT}/src/rules.cpp
    ${RA2_ROOT}/src/art.cpp
    ${RA2_ROOT}/src/theater_name.cpp
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
)

# ra2engine app/library sources (uses SDL). Same set feeds the desktop
# `ra2engine` executable and the Android `libmain.so`.
set(RA2_ENGINE_SOURCES
    ${RA2_ROOT}/engine/main.cpp
    ${RA2_ROOT}/engine/platform.cpp
    ${RA2_ROOT}/engine/texture_cache.cpp
    ${RA2_ROOT}/engine/iso_project.cpp
    ${RA2_ROOT}/engine/iso_renderer.cpp
    ${RA2_ROOT}/engine/world.cpp
    ${RA2_ROOT}/engine/world_veterancy.cpp
    ${RA2_ROOT}/engine/pathfind.cpp
    ${RA2_ROOT}/engine/input.cpp
    ${RA2_ROOT}/engine/input_original.cpp
    ${RA2_ROOT}/engine/occupancy.cpp
    ${RA2_ROOT}/engine/group_move.cpp
    ${RA2_ROOT}/engine/combat.cpp
    ${RA2_ROOT}/engine/building.cpp
    ${RA2_ROOT}/engine/production.cpp
    ${RA2_ROOT}/engine/audio_mixer.cpp
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
    ${RA2_ROOT}/engine/rectfont.cpp
    ${RA2_ROOT}/engine/maplist.cpp
    ${RA2_ROOT}/engine/repair_sell.cpp
    ${RA2_ROOT}/engine/settings.cpp

    ${RA2_ROOT}/engine/projectile.cpp
    ${RA2_ROOT}/engine/map_trigger.cpp
    ${RA2_ROOT}/engine/campaign.cpp
    ${RA2_ROOT}/engine/veterancy.cpp
    ${RA2_ROOT}/engine/crate.cpp
    ${RA2_ROOT}/engine/roster2.cpp
    ${RA2_ROOT}/engine/abilities.cpp
    ${RA2_ROOT}/engine/world_abilities.cpp
    ${RA2_ROOT}/engine/savegame.cpp
    ${RA2_ROOT}/engine/play_script.cpp
)
