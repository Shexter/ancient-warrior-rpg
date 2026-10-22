#include "game/content_loader.hpp"
#include "game/dungeon/floor.hpp"
#include "sim/sim.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

int main(int argc, char** argv) {
    using namespace heroes::sim;

    const std::filesystem::path defaultDataDir = std::filesystem::path(HEROES_SOURCE_DIR) / "data";
    const unsigned defaultThreads = std::max(1u, std::thread::hardware_concurrency());

    const std::vector<std::string_view> args(argv + 1, argv + argc);
    auto options = parseArgs(args, defaultDataDir, defaultThreads);
    if (!options) {
        std::cerr << "heroes-sim: error: " << options.error() << '\n';
        return 2;
    }

    if (const int floors = options->benchDungeon; floors > 0) {
        namespace dungeon = heroes::game::dungeon;
        const auto content = heroes::game::loadContent(options->config.dataDir);
        const auto def = content ? dungeon::loadDungeon(options->config.dataDir, *content)
                                 : std::unexpected(content.error());
        if (!def) {
            std::cerr << "heroes-sim: error: " << def.error().front().file << ": "
                      << def.error().front().message << '\n';
            return 2;
        }
        std::size_t tiles = 0; // read after timing, so the work can't be skipped
        const auto begin = std::chrono::steady_clock::now();
        for (int i = 0; i < floors; ++i)
            tiles += dungeon::generateFloor(*def, static_cast<std::uint64_t>(i), 1 + i % def->floors).tiles.size();
        const std::chrono::duration<double, std::micro> took = std::chrono::steady_clock::now() - begin;
        std::cout << std::format("dungeon: {} floors, {:.1f} us per floor ({} tiles)\n", floors,
                                 took.count() / floors, tiles);
        return 0;
    }

    const auto start = std::chrono::steady_clock::now();
    auto report = runSimulation(options->config);
    const std::chrono::duration<double> wall = std::chrono::steady_clock::now() - start;
    if (!report) {
        std::cerr << "heroes-sim: error: " << report.error() << '\n';
        return 2;
    }

    std::cout << formatReport(options->config, *report);
    if (options->bench) {
        std::cout << std::format("wall: {:.3f} s  throughput: {:.0f} battles/s\n", wall.count(),
                                 static_cast<double>(report->battles) / wall.count());
    }
    return 0;
}
