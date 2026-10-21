#pragma once

#include <cstddef>
#include <string>
#include <variant>
#include <vector>

namespace heroes::game::world {

inline constexpr int kTicksPerSecond = 60; // the loop's fixed step

struct Tile {
    int x = 0, y = 0;
    bool operator==(const Tile&) const = default;
};

enum class Dir { Up, Down, Left, Right };

inline Tile neighbor(Tile t, Dir d) {
    switch (d) {
    case Dir::Up: return {t.x, t.y - 1};
    case Dir::Down: return {t.x, t.y + 1};
    case Dir::Left: return {t.x - 1, t.y};
    case Dir::Right: return {t.x + 1, t.y};
    }
    return t;
}

enum class Terrain { Grass, TallGrass, Tree, Water, Path, Sand, Rock, Wall, Floor };

struct TileType {
    char key;
    Terrain terrain;
    bool walkable;
};

enum class Service { Almonry, Forge, Prior, Thesaurus };
struct OpensService {
    Service service;
    bool operator==(const OpensService&) const = default;
};
struct OpensTraining {
    bool operator==(const OpensTraining&) const = default;
};
struct OpensMission {
    std::string missionId;
    bool operator==(const OpensMission&) const = default;
};
using LandmarkTarget = std::variant<OpensService, OpensTraining, OpensMission>;

struct Landmark {
    std::string name;
    Tile at;
    LandmarkTarget opens;
};

struct Zone {
    std::string name;
    int x, y, w, h;
    int level; // 1..30, the training-monster level of its monsters
    int count; // monsters alive at once
};

// field.json, durations already in ticks.
struct FieldTuning {
    int ticksPerTile = 1;        // the hero's walking pace
    int wanderTicks = 1;         // between a wandering monster's steps
    int chaseTicks = 1;          // between a chasing monster's steps
    int noticeRadius = 1;        // tiles (Chebyshev); a chase gives up beyond twice this
    int windUpTicks = 1;         // from a monster's wind-up to its hit
    int attackCooldownTicks = 1; // after a monster's hit before it acts again
    int lifeRegenTicks = 1;      // the hero regains Life HP this often
    int fervorRegenTicks = 1;    // the hero regains 1 Fervor this often
    int respawnTicks = 1;        // a zone below its count gets a monster back after this
    int respawnDistance = 0;     // tiles (Chebyshev) from the hero, at least, for a new monster
};

// The loaded, validated world: the map grid and everything placed on it.
struct WorldDef {
    int width = 0, height = 0;
    std::vector<TileType> legend;
    std::vector<int> grid; // row-major index into `legend`
    Tile spawn;
    std::vector<Landmark> landmarks;
    std::vector<Zone> zones;
    FieldTuning field;

    bool inBounds(Tile t) const { return t.x >= 0 && t.y >= 0 && t.x < width && t.y < height; }
    // Precondition: inBounds(t).
    const TileType& at(Tile t) const {
        return legend[static_cast<std::size_t>(grid[static_cast<std::size_t>(t.y * width + t.x)])];
    }
    bool walkable(Tile t) const { return inBounds(t) && at(t).walkable; }
};

} // namespace heroes::game::world
