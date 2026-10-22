#include "event_format.hpp"
#include "game/battle.hpp"
#include "game/dungeon/dungeon_def.hpp"
#include "game/dungeon/floor.hpp"
#include "platform/env.hpp"
#include "shipped_content.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace heroes::game;
using namespace heroes::game::dungeon;

namespace {

const std::filesystem::path kSource = HEROES_SOURCE_DIR;

const DungeonDef& shippedDungeon() {
    static const DungeonDef def = loadDungeon(kSource / "data", shippedContent()).value();
    return def;
}

std::string readFile(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

// The errors from parsing the shipped dungeon.json after `change`, one per line.
template <class Change> std::string errorsWith(Change change) {
    nlohmann::json j = nlohmann::json::parse(readFile(kSource / "data" / "dungeon.json"));
    change(j["dungeon"]);
    const auto parsed = parseDungeon(j.dump(), shippedContent());
    EXPECT_FALSE(parsed.has_value());
    std::string out;
    if (!parsed)
        for (const ContentError& e : parsed.error())
            out += e.file + ": " + e.entry + ": " + e.message + "\n";
    return out;
}

int distanceAt(const std::vector<int>& dist, const Floor& f, Pos p) {
    return dist[static_cast<std::size_t>(p.y * f.width + p.x)];
}

} // namespace

TEST(DungeonDef, TheShippedDungeonLoads) {
    const auto def = loadDungeon(kSource / "data", shippedContent());
    ASSERT_TRUE(def.has_value());
    EXPECT_EQ(def->floors, 5);
    EXPECT_EQ(def->poolFor(3).enemies[1].id, "skeleton");
    EXPECT_EQ(def->chestsFor(5).entries[0].item, "phoenix_feather");
}

TEST(DungeonDef, AnUnknownPoolEnemyIsNamed) {
    const std::string errors = errorsWith([](nlohmann::json& d) { d["pools"][0]["enemies"][0]["id"] = "giant_rat"; });
    EXPECT_NE(errors.find("dungeon.json: pool #1: unknown enemy 'giant_rat'"), std::string::npos) << errors;
}

TEST(DungeonDef, AFloorWithoutAPoolIsNamed) {
    const std::string errors = errorsWith([](nlohmann::json& d) { d["pools"][1]["floors"] = nlohmann::json::array(); });
    EXPECT_NE(errors.find("floor 3 is in 0 pools (needs exactly one)"), std::string::npos) << errors;
}

TEST(DungeonDef, MinAboveMaxIsNamed) {
    const std::string errors = errorsWith([](nlohmann::json& d) { d["chestTables"][0]["entries"][1]["min"] = 2; });
    EXPECT_NE(errors.find("dungeon.json: chest table #1: min 2 is more than max 1"), std::string::npos) << errors;
}

TEST(DungeonDef, FloorScalingGrowsByAQuarterAFloor) {
    const EnemyDef& rat = shippedContent().enemy("rat");
    const EnemyDef five = scaledEnemy(rat, shippedDungeon(), 5); // x 200 / 100
    EXPECT_EQ(five.maxHp, 400);
    EXPECT_EQ(five.atk, 120);
    EXPECT_EQ(five.rewardGold, 200);
    EXPECT_EQ(scaledEnemy(rat, shippedDungeon(), 2).def, 6); // 5 x 125 / 100, rounded down
}

TEST(BasicEnemy, AttacksEveryTurnAndDrawsNothing) {
    // A Giant Rat fights a hero who only defends: whatever the seed, every enemy turn is one plain
    // attack and the whole battle comes out the same, so no random draw was made.
    const auto fight = [](std::uint64_t seed) {
        Combatant hero{"Aria", 2000, 2000, 200, 200, 400, 200, 50, {}};
        Battle battle(shippedContent(), hero, shippedContent().enemy("rat"), {}, Rng::stream(seed, 0));
        std::vector<std::string> log;
        for (const Event& e : battle.start())
            log.push_back(toString(e));
        for (int turn = 0; turn < 5; ++turn) {
            const std::vector<Event> events = battle.act(Defend{}).value_or(std::vector<Event>{});
            for (const Event& e : events)
                log.push_back(toString(e));
        }
        return log;
    };
    const std::vector<std::string> log = fight(1);
    EXPECT_EQ(log, fight(99));
    int enemyAttacks = 0;
    for (const std::string& line : log)
        enemyAttacks += line.starts_with("Attacked Enemy");
    EXPECT_EQ(enemyAttacks, 5);
}

TEST(FloorGen, EveryFloorIsConnectedWalledAndHasItsStairs) {
    const DungeonDef& def = shippedDungeon();
    for (std::uint64_t seed = 0; seed < 10'000; ++seed) {
        const int index = 1 + static_cast<int>(seed % 5);
        const Floor f = generateFloor(def, seed, index);
        const std::vector<int> dist = bfsDistances(f, f.entry);
        for (int y = 0; y < f.height; ++y)
            for (int x = 0; x < f.width; ++x) {
                const Pos p{x, y};
                if (x == 0 || y == 0 || x == f.width - 1 || y == f.height - 1) {
                    ASSERT_EQ(f.at(p), Tile::Wall) << seed;
                }
                if (f.at(p) != Tile::Wall) {
                    ASSERT_GE(distanceAt(dist, f, p), 0) << "seed " << seed << " tile " << x << "," << y;
                }
            }
        ASSERT_EQ(f.stairsDown.has_value(), index < def.floors) << seed;
        if (f.stairsDown) {
            ASSERT_EQ(f.at(*f.stairsDown), Tile::StairsDown) << seed;
        }
        ASSERT_EQ(f.at(f.entry), index == 1 ? Tile::StairsUp : Tile::Floor) << seed;
    }
}

TEST(FloorGen, TheSameSeedGivesTheSameFloor) {
    EXPECT_EQ(generateFloor(shippedDungeon(), 42, 3), generateFloor(shippedDungeon(), 42, 3));
    EXPECT_NE(generateFloor(shippedDungeon(), 42, 3), generateFloor(shippedDungeon(), 43, 3));
}

TEST(FloorGen, EntitiesKeepApartAndAwayFromTheEntry) {
    const DungeonDef& def = shippedDungeon();
    for (std::uint64_t seed = 0; seed < 2'000; ++seed) {
        const int index = 1 + static_cast<int>(seed % 5);
        const Floor f = generateFloor(def, seed, index);
        const std::vector<int> dist = bfsDistances(f, f.entry);
        std::set<Pos> taken{f.entry};
        if (f.stairsDown)
            taken.insert(*f.stairsDown);
        int elites = 0;
        for (const MapEnemy& e : f.enemies) {
            ASSERT_TRUE(taken.insert(e.pos).second) << seed;
            ASSERT_GT(distanceAt(dist, f, e.pos), def.safeRadius) << seed;
            elites += e.elite;
        }
        for (const MapChest& c : f.chests)
            ASSERT_TRUE(taken.insert(c.pos).second) << seed;
        ASSERT_EQ(elites, index == def.floors ? 1 : 0) << seed;
        ASSERT_GE(f.chests.size(), static_cast<std::size_t>(def.chestCount.base)) << seed;
    }
}

TEST(FloorGen, DeeperFloorsHoldTougherEnemies) {
    const DungeonDef& def = shippedDungeon();
    const auto averageHp = [&](int index) {
        long long total = 0, count = 0;
        for (std::uint64_t seed = 0; seed < 1'000; ++seed)
            for (const MapEnemy& e : generateFloor(def, seed, index).enemies) {
                total += scaledEnemy(shippedContent().enemy(e.enemyId), def, index).maxHp;
                ++count;
            }
        return static_cast<double>(total) / static_cast<double>(count);
    };
    EXPECT_GT(averageHp(5), averageHp(1));
}

// Regenerate with HEROES_WRITE_FIXTURES=1 after an intended change to generation.
TEST(FloorGen, MatchesTheCheckedInFloors) {
    for (const int index : {1, 5}) {
        const auto file = kSource / "tests" / "fixtures" / "dungeon" / std::format("seed1234_f{}.txt", index);
        const std::string drawn = renderAscii(generateFloor(shippedDungeon(), 1234, index));
        if (heroes::platform::getEnv("HEROES_WRITE_FIXTURES"))
            std::ofstream(file, std::ios::binary) << drawn;
        EXPECT_EQ(drawn, readFile(file)) << file;
    }
}
