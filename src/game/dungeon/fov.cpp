#include "game/dungeon/fov.hpp"

#include <utility>

namespace heroes::game::dungeon {

namespace {

struct Slope {
    int num, den; // den > 0
};

struct Quadrant {
    const Floor& floor;
    Pos origin;
    int dx, dy; // the direction depth runs in; col runs across it
    int radius;
    std::vector<bool>& visible;

    Pos at(int depth, int col) const {
        return dx != 0 ? Pos{origin.x + dx * depth, origin.y + col} : Pos{origin.x + col, origin.y + dy * depth};
    }
    bool isWall(int depth, int col) const {
        const Pos p = at(depth, col);
        return !floor.inBounds(p) || floor.at(p) == Tile::Wall;
    }
    void reveal(int depth, int col) const {
        const Pos p = at(depth, col);
        if (floor.inBounds(p) && col * col + depth * depth <= radius * radius)
            visible[static_cast<std::size_t>(p.y * floor.width + p.x)] = true;
    }

    void scan(int depth, Slope start, Slope end) const {
        if (depth > radius)
            return;
        // depth x start rounded half up, depth x end rounded half down.
        const int minCol = floorDiv(2 * depth * start.num + start.den, 2 * start.den);
        const int maxCol = -floorDiv(-(2 * depth * end.num - end.den), 2 * end.den);
        int prev = -1; // -1 none yet, 0 floor, 1 wall
        for (int col = minCol; col <= maxCol; ++col) {
            const bool wall = isWall(depth, col);
            // Symmetric: col / depth lies within [start, end], compared as exact fractions.
            const bool symmetric = col * start.den >= depth * start.num && col * end.den <= depth * end.num;
            if (wall || symmetric)
                reveal(depth, col);
            if (prev == 1 && !wall)
                start = {2 * col - 1, 2 * depth};
            if (prev == 0 && wall)
                scan(depth + 1, start, {2 * col - 1, 2 * depth});
            prev = wall ? 1 : 0;
        }
        if (prev == 0)
            scan(depth + 1, start, end);
    }
};

} // namespace

void computeFov(const Floor& floor, Pos origin, int radius, std::vector<bool>& visible) {
    visible.assign(floor.tiles.size(), false);
    if (!floor.inBounds(origin))
        return;
    visible[static_cast<std::size_t>(origin.y * floor.width + origin.x)] = true;
    for (const auto& [dx, dy] : {std::pair{0, -1}, std::pair{1, 0}, std::pair{0, 1}, std::pair{-1, 0}})
        Quadrant{floor, origin, dx, dy, radius, visible}.scan(1, {-1, 1}, {1, 1});
}

} // namespace heroes::game::dungeon
