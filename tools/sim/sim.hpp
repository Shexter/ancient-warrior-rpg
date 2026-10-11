#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace heroes::sim {

enum class Policy { Attack, Smart };

struct SimConfig {
    std::filesystem::path dataDir;
    std::string pathId;
    std::string enemyId;
    std::uint64_t battles = 0;
    std::uint64_t seed = 0;
    unsigned threads = 1;
    Policy policy = Policy::Attack;
    std::string weaponId = "iron_sword";
    int weaponLevel = 1;
    std::string armorId = "steel_shield";
    int armorLevel = 1;
    std::vector<std::string> skills;
    int bonusHp = 0, bonusMp = 0, bonusAtk = 0, bonusDef = 0;
};

struct Tally {
    std::uint64_t wins = 0, losses = 0, draws = 0, fled = 0;
    std::uint64_t totalTurns = 0;
    std::uint64_t hpLeftOnWins = 0;
    bool operator==(const Tally&) const = default;
};

// Independent of the thread count: the same inputs always give an equal Report.
struct Report {
    std::uint64_t battles = 0;
    std::uint64_t seed = 0;
    std::string pathId, enemyId;
    int atk = 0, def = 0, life = 0, hp = 0, mp = 0; // hero stats entering each battle
    Tally tally;
    bool operator==(const Report&) const = default;
};

// Validates the config, then runs every battle. Returns an error message if nothing could run.
std::expected<Report, std::string> runSimulation(const SimConfig& config);

// The exact four-line report from interfaces.md §13.
std::string formatReport(const SimConfig& config, const Report& report);

struct CliOptions {
    SimConfig config;
    bool bench = false;
};

// Parses the heroes-sim command line (without argv[0]).
std::expected<CliOptions, std::string> parseArgs(std::span<const std::string_view> args,
                                                 const std::filesystem::path& defaultDataDir,
                                                 unsigned defaultThreads);

} // namespace heroes::sim
