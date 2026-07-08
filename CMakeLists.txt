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

# mix_keyderiv.cpp is a verbatim XCC port that reinterprets bignum limbs via
# `*(word*)` casts; this is strict-aliasing UB that only bites under
# optimization (host dev builds are -O0, but Android/NDK builds default to
# Release), so disable strict-aliasing for this translation unit specifically.
# -fsigned-char is also needed here: its `char2num` lookup table is
# initialized with -1 sentinels assuming plain `char` is signed (true on
# x86-64), but AAPCS64 makes plain `char` unsigned by default, which turns
# those -1 literals into an illegal narrowing conversion on Android/arm64;
# forcing signed char keeps the file byte-for-byte identical to upstream.
set_source_files_properties(src/mix_keyderiv.cpp PROPERTIES COMPILE_OPTIONS "-fno-strict-aliasing;-fsigned-char")

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
