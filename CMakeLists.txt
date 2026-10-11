cmake_minimum_required(VERSION 3.25)
project(heroes VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/Warnings.cmake)
include(cmake/Dependencies.cmake)

enable_testing()

add_library(heroes_game STATIC src/game/rng.cpp src/game/content_loader.cpp
    src/game/inventory.cpp src/game/hero.cpp src/game/battle.cpp src/game/training.cpp src/game/town.cpp)
target_include_directories(heroes_game PUBLIC src)
target_link_libraries(heroes_game PUBLIC nlohmann_json::nlohmann_json PRIVATE heroes_warnings)

find_package(Threads REQUIRED)
add_library(heroes_simlib STATIC tools/sim/sim.cpp)
target_include_directories(heroes_simlib PUBLIC tools)
target_link_libraries(heroes_simlib PUBLIC heroes_game Threads::Threads PRIVATE heroes_warnings)

add_executable(heroes-sim tools/sim/main.cpp)
target_link_libraries(heroes-sim PRIVATE heroes_simlib heroes_warnings)
target_compile_definitions(heroes-sim PRIVATE HEROES_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

add_executable(heroes src/app/main.cpp)
target_link_libraries(heroes PRIVATE heroes_game heroes_warnings)
target_compile_definitions(heroes PRIVATE HEROES_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

add_executable(heroes_tests tests/smoke_test.cpp tests/rng_test.cpp tests/content_test.cpp
    tests/content_validation_test.cpp tests/inventory_test.cpp
    tests/hero_test.cpp tests/battle_test.cpp tests/battle_golden_test.cpp
    tests/battle_status_test.cpp tests/sim_test.cpp tests/training_test.cpp
    tests/town_merchant_test.cpp tests/town_blacksmith_test.cpp tests/town_guild_test.cpp)
target_link_libraries(heroes_tests PRIVATE heroes_simlib heroes_warnings GTest::gtest_main)
target_compile_definitions(heroes_tests PRIVATE HEROES_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
include(GoogleTest)
gtest_discover_tests(heroes_tests)
