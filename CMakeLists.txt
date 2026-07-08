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

enable_testing()
add_executable(ra2assets_tests
    tests/test_main.cpp
)
target_sources(ra2assets_tests PRIVATE tests/test_blowfish.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_mix.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_pal.cpp)
target_sources(ra2assets_tests PRIVATE tests/test_shp.cpp)
target_include_directories(ra2assets_tests PRIVATE third_party include)
target_link_libraries(ra2assets_tests PRIVATE ra2assets)
add_test(NAME ra2assets_tests COMMAND ra2assets_tests)
