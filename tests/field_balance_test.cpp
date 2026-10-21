// Task 6.4: a simple bot plays the field headlessly, to check zones are winnable at the hero's
// level and to measure where each starter path tops out (recorded in docs/benchmarks.md).

#include "game/world/world.hpp"
#include "game/world/world_loader.hpp"
#include "screen_harness.hpp"
#include "state_helpers.hpp"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <format>
#include <string>

using namespace heroes::game;
using namespace heroes::game::world;

namespace {

int manhattan(Tile a, Tile b) { return std::abs(a.x - b.x) + std::abs(a.y - b.y); }

// A 30x20 open meadow, spawn in the middle, one zone over all of it.
WorldDef meadow(int level) {
    std::string map;
    for (int y = 0; y < 20; ++y)
        map += std::string(30, '.') + '\n';
    auto world = parseWorld(
        map,
        std::format(R"({{"legend": {{".": {{"terrain": "grass", "walkable": true}}}},
                       "spawn": {{"x": 15, "y": 10}}, "landmarks": [],
                       "zones": [{{"name": "M", "x": 0, "y": 0, "w": 30, "h": 20,
                                  "level": {}, "count": 4}}]}})",
                    level),
        screen_test::readFile(std::filesystem::path(HEROES_SOURCE_DIR) / "data" / "world" /
                              "field.json"),
        shippedContent());
    EXPECT_TRUE(world.has_value());
    return std::move(*world);
}

Dir toward(Tile from, Tile to) {
    const int dx = to.x - from.x, dy = to.y - from.y;
    if (std::abs(dx) >= std::abs(dy))
        return dx > 0 ? Dir::Right : Dir::Left;
    return dy > 0 ? Dir::Down : Dir::Up;
}

// The bot: step out of a wind-up aimed at it, drink below 30% HP, otherwise walk up to the
// nearest monster and swing at it (Firebolt when it has Fervor to spare).
WorldInput policy(const World& w, const Hero& hero, const WorldDef& def) {
    WorldInput input;
    const Tile me = w.hero();
    for (const Monster& m : w.monsters())
        if (m.mode == Monster::Mode::WindUp && m.target == me) {
            for (const Dir d : {Dir::Up, Dir::Down, Dir::Left, Dir::Right}) {
                const Tile n = neighbor(me, d);
                const bool free = def.walkable(n) && std::ranges::none_of(w.monsters(), [&](const Monster& o) { return o.at == n; });
                if (free && manhattan(n, m.at) > 1) {
                    input.move = d;
                    return input;
                }
            }
        }
    if (hero.hp * 10 < hero.maxHp * 3)
        input.potion = true;
    const Monster* nearest = nullptr;
    for (const Monster& m : w.monsters())
        if (!nearest || manhattan(m.at, me) < manhattan(nearest->at, me))
            nearest = &m;
    if (!nearest)
        return input;
    const Dir d = toward(me, nearest->at);
    if (manhattan(me, nearest->at) == 1 && w.facing() == d)
        input.swing = true;
    else
        input.move = d; // walking into it only turns the hero to face it
    if (hero.mp >= 45 && w.facing() == d && (nearest->at.x == me.x || nearest->at.y == me.y))
        input.firebolt = true;
    return input;
}

// Plays one seed: true if the hero beats the zone's first four monsters within two minutes
// without falling.
bool clears(std::string_view path, int level, std::uint64_t seed) {
    const WorldDef def = meadow(level);
    GameState state = freshState(path, seed);
    World w(def, state, shippedContent());
    int kills = 0;
    for (int tick = 0; tick < 60 * 120 && kills < 4; ++tick) {
        for (const WorldEvent& e : w.step(policy(w, state.hero, def)))
            kills += std::holds_alternative<wev::MonsterDefeated>(e) ? 1 : 0;
        if (w.fallen())
            return false;
    }
    return kills >= 4;
}

int clearedOf10(std::string_view path, int level) {
    int cleared = 0;
    for (std::uint64_t seed = 1; seed <= 10; ++seed)
        cleared += clears(path, level, seed) ? 1 : 0;
    return cleared;
}

} // namespace

// A starter hero (level 1, starting gear) clears a level 1 zone in at least 8 of 10 seeds.
TEST(FieldBalance, EveryStarterPathClearsALevelOneZone) {
    for (const std::string_view path : {"fighter", "hunter", "rogue"})
        EXPECT_GE(clearedOf10(path, 1), 8) << path;
}

// The numbers for docs/benchmarks.md: each path's clears out of 10 at zone levels 1-6.
TEST(FieldBalance, StarterPathsByZoneLevel) {
    for (const std::string_view path : {"fighter", "hunter", "rogue"}) {
        std::string line = std::format("field bot {:8}", path);
        for (int level = 1; level <= 6; ++level)
            line += std::format("  L{} {:2}/10", level, clearedOf10(path, level));
        std::printf("%s\n", line.c_str());
    }
}
