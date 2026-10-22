#pragma once

#include "game/content.hpp"
#include "game/content_loader.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace heroes::game::dungeon {

struct WeightedEnemy {
    std::string id;
    int weight;
};
struct EnemyPool {
    std::vector<int> floors;
    std::vector<WeightedEnemy> enemies;
};
struct ChestEntry {
    std::string item;
    int weight, min, max;
};
struct ChestTable {
    std::vector<int> floors;
    std::vector<ChestEntry> entries;
};

// data/dungeon.json: the expedition's size, its floor layout rules, and what each floor holds.
struct DungeonDef {
    std::string name;
    int width, height, floors, fovRadius;
    struct {
        int minLeafWidth, minLeafHeight, maxDepth, minRoomWidth, minRoomHeight;
    } bsp;
    int scalingPercentPerFloor;
    struct {
        int base, perFloor, random;
    } enemyCount;
    struct {
        int base, random, lastFloorBonus;
    } chestCount;
    int safeRadius;
    std::vector<EnemyPool> pools;
    struct {
        int floor;
        std::string enemy;
    } elite;
    std::vector<ChestTable> chestTables;

    // Precondition: 1 <= floor <= floors (the loader checks each floor has exactly one).
    const EnemyPool& poolFor(int floor) const;
    const ChestTable& chestsFor(int floor) const;
};

// Parses dungeon.json and validates it against `content` (enemy and item ids). Returns every
// error found, in the file "dungeon.json".
std::expected<DungeonDef, std::vector<ContentError>> parseDungeon(std::string_view json,
                                                                  const Content& content);
// Reads <dataDir>/dungeon.json and parses it.
std::expected<DungeonDef, std::vector<ContentError>> loadDungeon(const std::filesystem::path& dataDir,
                                                                 const Content& content);

// `enemy` as met on `floor`: maxHp, atk, def, life and the rewards each times
// (100 + scalingPercentPerFloor x (floor - 1)) / 100, with no reward items.
EnemyDef scaledEnemy(const EnemyDef& enemy, const DungeonDef& def, int floor);

} // namespace heroes::game::dungeon
