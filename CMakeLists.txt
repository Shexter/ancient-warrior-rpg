cmake_minimum_required(VERSION 3.25)
project(heroes VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/Warnings.cmake)
include(cmake/Dependencies.cmake)

enable_testing()

add_library(heroes_game STATIC src/game/rng.cpp src/game/content_loader.cpp
    src/game/inventory.cpp src/game/hero.cpp src/game/battle.cpp src/game/training.cpp src/game/estimate.cpp src/game/town.cpp
    src/game/save.cpp src/game/autoplay.cpp src/game/script/script_host.cpp
    src/game/script/battle_api.cpp src/game/world/world_loader.cpp src/game/world/world.cpp)
target_include_directories(heroes_game PUBLIC src)
target_link_libraries(heroes_game PUBLIC nlohmann_json::nlohmann_json
    PRIVATE heroes_warnings lua_static sol2::sol2)
target_compile_definitions(heroes_game PRIVATE SOL_ALL_SAFETIES_ON=1)

add_library(heroes_engine STATIC src/engine/color.cpp src/engine/framebuffer.cpp
    src/engine/renderer.cpp src/engine/input.cpp src/engine/loop.cpp src/engine/sprite.cpp)
target_include_directories(heroes_engine PUBLIC src)
target_link_libraries(heroes_engine PRIVATE heroes_warnings)

if(WIN32)
    set(HEROES_TERMINAL_SOURCE src/platform/terminal_windows.cpp)
else()
    set(HEROES_TERMINAL_SOURCE src/platform/terminal_posix.cpp)
endif()
add_library(heroes_platform STATIC src/platform/paths.cpp src/platform/env.cpp
    ${HEROES_TERMINAL_SOURCE})
target_include_directories(heroes_platform PUBLIC src)
target_link_libraries(heroes_platform PUBLIC heroes_engine PRIVATE heroes_warnings)

find_package(Threads REQUIRED)
add_library(heroes_simlib STATIC tools/sim/sim.cpp)
target_include_directories(heroes_simlib PUBLIC tools)
target_link_libraries(heroes_simlib PUBLIC heroes_game Threads::Threads PRIVATE heroes_warnings)

add_library(heroes_bakelib STATIC tools/sprite_bake/bake.cpp)
target_include_directories(heroes_bakelib PUBLIC tools/sprite_bake)
target_include_directories(heroes_bakelib SYSTEM PRIVATE tools/sprite_bake/third_party)
target_link_libraries(heroes_bakelib PUBLIC heroes_engine PRIVATE heroes_warnings)

add_executable(sprite_bake tools/sprite_bake/main.cpp)
target_link_libraries(sprite_bake PRIVATE heroes_bakelib heroes_warnings)

add_executable(heroes-sim tools/sim/main.cpp)
target_link_libraries(heroes-sim PRIVATE heroes_simlib heroes_warnings)
target_compile_definitions(heroes-sim PRIVATE HEROES_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

add_library(heroes_app STATIC src/app/app.cpp src/app/widgets.cpp src/app/screens/title.cpp
    src/app/screens/new_game.cpp src/app/screens/hub.cpp
    src/app/screens/training.cpp src/app/screens/town.cpp src/app/screens/merchant.cpp
    src/app/screens/blacksmith.cpp src/app/screens/guildmaster.cpp
    src/app/screens/mission.cpp src/app/battle_view.cpp src/app/art.cpp
    src/app/held_keys.cpp src/app/screens/overworld.cpp)
target_include_directories(heroes_app PUBLIC src)
target_link_libraries(heroes_app PUBLIC heroes_game heroes_engine PRIVATE heroes_warnings)

add_executable(heroes src/app/main.cpp)
target_link_libraries(heroes PRIVATE heroes_app heroes_platform heroes_warnings)
target_compile_definitions(heroes PRIVATE HEROES_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

add_executable(heroes_tests tests/smoke_test.cpp tests/rng_test.cpp tests/content_test.cpp
    tests/content_validation_test.cpp tests/inventory_test.cpp
    tests/hero_test.cpp tests/battle_test.cpp tests/battle_golden_test.cpp
    tests/battle_status_test.cpp tests/sim_test.cpp tests/training_test.cpp tests/estimate_test.cpp tests/terminal_test.cpp tests/portrait_test.cpp tests/portrait_play_test.cpp
    tests/town_merchant_test.cpp tests/town_blacksmith_test.cpp tests/town_guild_test.cpp
    tests/save_test.cpp tests/paths_test.cpp tests/framebuffer_test.cpp
    tests/renderer_test.cpp tests/input_test.cpp tests/loop_test.cpp tests/sprite_test.cpp
    tests/glyph_sprite_test.cpp tests/script_sandbox_test.cpp tests/script_battle_test.cpp
    tests/mods_test.cpp
    tests/sprite_bake_test.cpp tests/widgets_test.cpp tests/app_test.cpp tests/screens_test.cpp
    tests/training_screen_test.cpp tests/town_screens_test.cpp
    tests/battle_screen_test.cpp tests/large_layout_test.cpp tests/world_test.cpp tests/held_keys_test.cpp
    tests/overworld_screen_test.cpp)
target_link_libraries(heroes_tests PRIVATE heroes_simlib heroes_platform heroes_bakelib heroes_app
    heroes_warnings GTest::gtest_main sol2::sol2)
# Lua's headers for the sandbox tests; the library itself comes through heroes_game.
target_include_directories(heroes_tests SYSTEM PRIVATE ${lua_SOURCE_DIR}/src)
target_compile_definitions(heroes_tests PRIVATE SOL_USING_CXX_LUA=1)
target_compile_definitions(heroes_tests PRIVATE HEROES_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
include(GoogleTest)
gtest_discover_tests(heroes_tests)

# Content errors must be printed (and the game exit) before the terminal is touched.
add_test(NAME heroes_reports_bad_content
    COMMAND heroes --data-dir ${PROJECT_SOURCE_DIR}/tests/fixtures/content_bad/negative_price)
set_tests_properties(heroes_reports_bad_content PROPERTIES
    PASS_REGULAR_EXPRESSION "items.json: item 'iron_sword': key 'price' must be between")
add_test(NAME heroes_reports_bad_world
    COMMAND ${CMAKE_COMMAND} -DHEROES=$<TARGET_FILE:heroes> -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/bad_world -P ${PROJECT_SOURCE_DIR}/tests/bad_world_cli.cmake)
add_test(NAME heroes_rejects_bad_layout COMMAND heroes --layout big)
set_tests_properties(heroes_rejects_bad_layout PROPERTIES
    PASS_REGULAR_EXPRESSION "--layout expects auto, compact or portrait, got 'big'.*usage: heroes .*--layout auto\\|compact\\|portrait")
