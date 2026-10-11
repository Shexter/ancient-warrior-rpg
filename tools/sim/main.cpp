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
