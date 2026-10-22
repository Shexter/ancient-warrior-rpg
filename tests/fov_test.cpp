#include "game/dungeon/fov.hpp"
#include "game/rng.hpp"
#include "shipped_content.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace heroes::game;
using namespace heroes::game::dungeon;

namespace {

// A floor from rows of '#' and '.'.
Floor floorOf(const std::vector<std::string>& rows) {
    Floor f;
    f.width = static_cast<int>(rows[0].size());
    f.height = static_cast<int>(rows.size());
    for (const std::string& row : rows)
        for (const char c : row)
            f.tiles.push_back(c == '#' ? Tile::Wall : Tile::Floor);
    return f;
}

bool seen(const std::vector<bool>& visible, const Floor& f, Pos p) {
    return visible[static_cast<std::size_t>(p.y * f.width + p.x)];
}

const DungeonDef& shippedDungeon() {
    static const DungeonDef def =
        loadDungeon(std::filesystem::path(HEROES_SOURCE_DIR) / "data", shippedContent()).value();
    return def;
}

} // namespace

TEST(Fov, FloorDivRoundsDown) {
    EXPECT_EQ(floorDiv(7, 2), 3);
    EXPECT_EQ(floorDiv(-7, 2), -4);
    EXPECT_EQ(floorDiv(-6, 2), -3);
    EXPECT_EQ(floorDiv(0, 5), 0);
}

TEST(Fov, TheOriginIsAlwaysSeen) {
    const Floor f = floorOf({"###", "#.#", "###"});
    std::vector<bool> visible;
    computeFov(f, {1, 1}, 0, visible);
    EXPECT_TRUE(seen(visible, f, {1, 1}));
    computeFov(f, {1, 1}, 8, visible);
    EXPECT_TRUE(seen(visible, f, {1, 1}));
    EXPECT_TRUE(seen(visible, f, {0, 0})); // and the walls round it
}

TEST(Fov, AnOpenRoomShowsACircle) {
    const Floor f = floorOf(std::vector<std::string>(41, std::string(41, '.')));
    std::vector<bool> visible;
    computeFov(f, {20, 20}, 8, visible);
    for (int y = 0; y < f.height; ++y)
        for (int x = 0; x < f.width; ++x) {
            const int dx = x - 20, dy = y - 20;
            EXPECT_EQ(seen(visible, f, {x, y}), dx * dx + dy * dy <= 64) << x << "," << y;
        }
}

TEST(Fov, APillarCastsItsShadow) {
    const Floor f = floorOf({"###########",
                             "#.........#",
                             "#.........#",
                             "#.....#...#",
                             "#.........#",
                             "#.........#",
                             "###########"});
    std::vector<bool> visible;
    computeFov(f, {3, 3}, 8, visible);
    std::string drawn;
    for (int y = 0; y < f.height; ++y) {
        for (int x = 0; x < f.width; ++x)
            drawn += Pos{x, y} == Pos{3, 3} ? '@' : !seen(visible, f, {x, y}) ? ' ' : f.at({x, y}) == Tile::Wall ? '#' : '.';
        drawn += '\n';
    }
    std::ifstream in(std::filesystem::path(HEROES_SOURCE_DIR) / "tests/fixtures/dungeon/fov_pillar.txt", std::ios::binary);
    std::ostringstream expected;
    expected << in.rdbuf();
    EXPECT_EQ(drawn, expected.str());
}

TEST(Fov, SightIsSymmetric) {
    // On 200 generated floors, 50 random pairs of open tiles within sight range: A sees B exactly
    // when B sees A.
    Rng rng = Rng::stream(2024, 0);
    std::vector<bool> fromA, fromB;
    for (std::uint64_t seed = 0; seed < 200; ++seed) {
        const Floor f = generateFloor(shippedDungeon(), seed, 1 + static_cast<int>(seed % 5));
        std::vector<Pos> open;
        for (int y = 0; y < f.height; ++y)
            for (int x = 0; x < f.width; ++x)
                if (f.at({x, y}) != Tile::Wall)
                    open.push_back({x, y});
        for (int pair = 0; pair < 50;) {
            const Pos a = open[rng.below(static_cast<std::uint32_t>(open.size()))];
            const Pos b = open[rng.below(static_cast<std::uint32_t>(open.size()))];
            const int dx = a.x - b.x, dy = a.y - b.y;
            if (dx * dx + dy * dy > 64)
                continue;
            ++pair;
            computeFov(f, a, 8, fromA);
            computeFov(f, b, 8, fromB);
            ASSERT_EQ(seen(fromA, f, b), seen(fromB, f, a))
                << "seed " << seed << " A " << a.x << "," << a.y << " B " << b.x << "," << b.y;
        }
    }
}
