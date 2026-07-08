cmake_minimum_required(VERSION 3.20)
project(ra2assets LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Library sources, shared between the static `ra2assets` target and the
# optional `ra2assets_so` shared target below so both actually contain the
# real library code (not just the placeholder).
set(RA2_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/_placeholder.cpp
    src/blowfish.cpp
    src/mix_keyderiv.cpp
    src/mix.cpp
    src/pal.cpp
    src/shp.cpp
)

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
target_include_directories(ra2assets_tests PRIVATE third_party include)
target_link_libraries(ra2assets_tests PRIVATE ra2assets)
add_test(NAME ra2assets_tests COMMAND ra2assets_tests)

add_executable(ra2extract tools/ra2extract.cpp)
target_include_directories(ra2extract PRIVATE third_party)
target_link_libraries(ra2extract PRIVATE ra2assets)
