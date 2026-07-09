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
target_sources(ra2assets_tests PRIVATE tests/test_oracle.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_lcw.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_mappack.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_lzo1x.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_ini.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_rules.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_map.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_theater.cpp)
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
target_sources(ra2assets_tests PRIVATE tests/test_world.cpp engine/world.cpp)
# input.cpp is SDL-free (only InputEvent, screen_to_cell, World -- see
# engine/input.hpp) so it compiles into this test binary too (Task 12 review
# Finding 3: Input previously had zero test coverage); it calls
# terrain_screen_bounds, which is defined in engine/world.cpp (already a
# source above) rather than engine/iso_renderer.cpp so this stays SDL-free.
target_sources(ra2assets_tests PRIVATE tests/test_input.cpp engine/input.cpp)
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
