# Native build for the RA2 Android app.
#
# Produces libmain.so -- the shared object SDLActivity loads and whose SDL_main
# (our engine/main.cpp main(), renamed by SDL_main.h) is the entry point. It
# bundles the ra2engine sources + the ra2assets library, linked against SDL2.
#
# ABIs: arm64-v8a (phones) + x86_64 (emulator / Chromebooks), filtered in
# app/build.gradle. Optimised in both build types (app/build.gradle).

cmake_minimum_required(VERSION 3.6)
project(ra2_android CXX C)

# Repo root: android/app/jni -> android/app -> android -> <repo>.
get_filename_component(RA2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)

# SDL2 sources (W6-06-14: a fresh checkout builds with no desktop build dir):
#   1. -DSDL2_SRC_DIR=<SDL 2.30.9 source> or env RA2_SDL2_SRC_DIR;
#   2. the desktop build's FetchContent copy (<repo>/build/_deps/sdl2-src),
#      when it exists (saves a download);
#   3. else the pinned release tarball (cmake/sdl2_pin.cmake: URL + SHA-256),
#      downloaded once into this ABI's build dir.
# Built here as a subproject so we get libSDL2.so + its Android JNI glue
# (which dlopen()s libmain.so and calls SDL_main).
include("${RA2_ROOT}/cmake/sdl2_pin.cmake")
if(NOT DEFINED SDL2_SRC_DIR)
    if(DEFINED ENV{RA2_SDL2_SRC_DIR})
        set(SDL2_SRC_DIR "$ENV{RA2_SDL2_SRC_DIR}")
    elseif(EXISTS "${RA2_ROOT}/build-tools/SDL2-2.30.9/CMakeLists.txt")
        set(SDL2_SRC_DIR "${RA2_ROOT}/build-tools/SDL2-2.30.9")
    elseif(EXISTS "${RA2_ROOT}/build/_deps/sdl2-src/CMakeLists.txt")
        set(SDL2_SRC_DIR "${RA2_ROOT}/build/_deps/sdl2-src")
    else()
        include(FetchContent)
        FetchContent_Declare(sdl2_android
            URL "${RA2_SDL2_URL}"
            URL_HASH SHA256=${RA2_SDL2_SHA256})
        FetchContent_GetProperties(sdl2_android)
        if(NOT sdl2_android_POPULATED)
            FetchContent_Populate(sdl2_android)
        endif()
        set(SDL2_SRC_DIR "${sdl2_android_SOURCE_DIR}")
    endif()
endif()
get_filename_component(SDL2_SRC_DIR "${SDL2_SRC_DIR}" ABSOLUTE)
if(NOT EXISTS "${SDL2_SRC_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "SDL2 sources not found at ${SDL2_SRC_DIR}. Pass -DSDL2_SRC_DIR=<path to "
        "SDL ${RA2_SDL2_VERSION} source> (or set RA2_SDL2_SRC_DIR), or allow the "
        "download of ${RA2_SDL2_URL}.")
endif()
message(STATUS "SDL2 source: ${SDL2_SRC_DIR}")

# W2-AR-30: SDL internal asserts -- app/build.gradle passes SDL_ASSERTIONS per
# build type (debug: enabled, release: release = SDL_assert compiled out).
if(NOT DEFINED SDL_ASSERTIONS)
    set(SDL_ASSERTIONS "release" CACHE STRING "SDL internal assertions")
endif()
add_subdirectory("${SDL2_SRC_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/SDL2")

# Shared source lists (single source of truth with the desktop build).
include("${RA2_ROOT}/cmake/ra2_sources.cmake")

add_library(main SHARED ${RA2_ENGINE_SOURCES} ${RA2_LIB_SOURCES})
# audio_sdl.cpp is main.cpp's SDL audio-sink companion (Task 6 of the Audio
# Playback phase). Added directly here (mirrors the top-level CMakeLists.txt)
# rather than into the shared RA2_ENGINE_SOURCES list in cmake/ra2_sources.cmake.
target_sources(main PRIVATE "${RA2_ROOT}/engine/audio_sdl.cpp")
# W2-AR-23 version; Q3: -ffp-contract=off -- arm64 clang would otherwise fuse
# a*b+c into FMA at -O2 and the float math (and saves) would drift from the
# desktop build.
target_compile_definitions(main PRIVATE RA2_VERSION="${RA2_VERSION}")
target_compile_options(main PRIVATE -ffp-contract=off -gline-tables-only)
set_target_properties(main PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)
target_include_directories(main PRIVATE
    "${RA2_GENERATED_DIR}"   # W6-06-16: ra2_notices.inc (cmake/ra2_sources.cmake)
    "${RA2_ROOT}/include"
    "${RA2_ROOT}/engine"
    "${RA2_ROOT}/third_party")
# Link the SDL2 shared lib; SDL2main is harmless on Android and keeps parity
# with the desktop link line.
target_link_libraries(main SDL2 SDL2main)
