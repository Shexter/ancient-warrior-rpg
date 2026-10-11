cmake_minimum_required(VERSION 3.25)
project(heroes VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/Warnings.cmake)
include(cmake/Dependencies.cmake)

enable_testing()

add_executable(heroes src/app/main.cpp)
target_link_libraries(heroes PRIVATE heroes_warnings)
target_compile_definitions(heroes PRIVATE HEROES_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

add_executable(heroes_tests tests/smoke_test.cpp)
target_link_libraries(heroes_tests PRIVATE heroes_warnings GTest::gtest_main)
target_compile_definitions(heroes_tests PRIVATE HEROES_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
include(GoogleTest)
gtest_discover_tests(heroes_tests)
