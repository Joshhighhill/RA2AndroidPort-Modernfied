cmake_minimum_required(VERSION 3.20)
project(ra2assets LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Library sources are added by later tasks; start empty via a stub object.
add_library(ra2assets STATIC)
target_include_directories(ra2assets PUBLIC include)
# Keeps the target valid before any .cpp exists:
target_sources(ra2assets PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/_placeholder.cpp)
target_sources(ra2assets PRIVATE src/blowfish.cpp)
target_sources(ra2assets PRIVATE src/mix_keyderiv.cpp)
target_sources(ra2assets PRIVATE src/mix.cpp)
target_sources(ra2assets PRIVATE src/pal.cpp)
target_sources(ra2assets PRIVATE src/shp.cpp)

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
    # Source-having shared target (CMake requires >=1 source/object at
    # generate time) that statically links ra2assets; it exists purely so an
    # inspectable libra2assets.so lands in the build dir for the ELF
    # machine-type check below — no whole-archive linkage needed for that.
    add_library(ra2assets_so SHARED src/_placeholder.cpp)
    target_include_directories(ra2assets_so PUBLIC include)
    target_link_libraries(ra2assets_so PRIVATE ra2assets)
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
