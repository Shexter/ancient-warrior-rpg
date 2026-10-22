#pragma once

#include "game/dungeon/dungeon_def.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace heroes::game::dungeon {

enum class Tile : std::uint8_t { Wall, Floor, StairsDown, StairsUp };

struct Pos {
    int x, y;
    auto operator<=>(const Pos&) const = default;
};
struct Rect {
    int x, y, w, h;
    bool operator==(const Rect&) const = default;
};
struct MapEnemy {
    std::string enemyId;
    Pos pos;
    bool awake = false;
    int sleepTurns = 0;
    bool elite = false;
    bool operator==(const MapEnemy&) const = default;
};
struct MapChest {
    Pos pos;
    bool opened = false;
    bool operator==(const MapChest&) const = default;
};

// One generated dungeon floor: walls and floor tiles, its rooms, where the hero arrives, the
// stairs down (none on the last floor), and what stands on it.
struct Floor {
    int width = 0, height = 0, index = 0; // index is 1-based
    std::vector<Tile> tiles;              // row-major
    std::vector<Rect> rooms;
    Pos entry{};
    std::optional<Pos> stairsDown;
    std::vector<MapEnemy> enemies;
    std::vector<MapChest> chests;

    bool inBounds(Pos p) const { return p.x >= 0 && p.y >= 0 && p.x < width && p.y < height; }
    // Precondition: inBounds(p).
    Tile at(Pos p) const { return tiles[static_cast<std::size_t>(p.y * width + p.x)]; }
    bool walkable(Pos p) const { return inBounds(p) && at(p) != Tile::Wall; }
    bool operator==(const Floor&) const = default;
};

// Floor `floorIndex` (1-based) of the expedition `expeditionSeed`. Every draw comes from
// Rng::stream(expeditionSeed, floorIndex) in a fixed order, so a floor can be rebuilt from those
// two numbers alone. Precondition: `def` is validated and 1 <= floorIndex <= def.floors.
Floor generateFloor(const DungeonDef& def, std::uint64_t expeditionSeed, int floorIndex);

// Steps from `from` to every tile (4-neighbour, walls block), row-major; -1 when unreachable.
std::vector<int> bfsDistances(const Floor& floor, Pos from);

// One line per row: '#' wall, '.' floor, '>' stairs down, '<' stairs up, 'e' an enemy, 'E' the
// elite, '$' a chest.
std::string renderAscii(const Floor& floor);

} // namespace heroes::game::dungeon
