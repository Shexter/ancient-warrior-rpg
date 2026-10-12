#include "sim/sim.hpp"

#include "game/content.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <string>

using namespace heroes::sim;

namespace {

SimConfig startingFighter(std::uint64_t battles, unsigned threads) {
    SimConfig config;
    config.dataDir = std::filesystem::path(HEROES_SOURCE_DIR) / "data";
    config.pathId = "fighter";
    config.enemyId = "medusa";
    config.battles = battles;
    config.seed = 7;
    config.threads = threads;
    return config;
}

} // namespace

TEST(Sim, ThreadCountDoesNotChangeTheReport) {
    auto one = runSimulation(startingFighter(10000, 1));
    auto eight = runSimulation(startingFighter(10000, 8));
    ASSERT_TRUE(one.has_value());
    ASSERT_TRUE(eight.has_value());
    EXPECT_EQ(*one, *eight);
}

TEST(Sim, SmartPolicyIsAlsoThreadIndependent) {
    SimConfig config = startingFighter(2000, 1);
    config.policy = Policy::Smart;
    config.skills = {"meteor_fist", "golden_bell", "angel_wings"};
    config.bonusMp = 500;
    auto one = runSimulation(config);
    config.threads = 8;
    auto eight = runSimulation(config);
    ASSERT_TRUE(one.has_value());
    ASSERT_TRUE(eight.has_value());
    EXPECT_EQ(*one, *eight);
}

TEST(Sim, StartingFighterNeverBeatsMedusaWithAttack) {
    auto report = runSimulation(startingFighter(2000, 4));
    ASSERT_TRUE(report.has_value());
    EXPECT_EQ(report->tally.wins, 0u);
    EXPECT_EQ(report->tally.losses, 2000u);
    EXPECT_EQ(report->atk, 100);
    EXPECT_EQ(report->def, 25);
}

TEST(Sim, FormatMatchesTheSpecifiedLayout) {
    SimConfig config = startingFighter(500, 2);
    auto report = runSimulation(config);
    ASSERT_TRUE(report.has_value());
    const std::string text = formatReport(config, *report);
    EXPECT_TRUE(text.starts_with("battles: 500  seed: 7  threads: 2  policy: attack\n"
                                 "enemy: medusa  hero: fighter atk=100 def=25 life=0 hp=350 mp=50\n"
                                 "win: 0.00%  loss: 100.00%  draw: 0.00%  fled: 0.00%\n"
                                 "avg turns: "))
        << text;
    EXPECT_NE(text.find("  avg hp left on wins: 0.00\n"), std::string::npos);
}

TEST(Sim, SupernovaWinsAreCountedWithHpLeft) {
    SimConfig config = startingFighter(200, 2);
    config.policy = Policy::Smart;
    config.skills = {"supernova"};
    config.bonusMp = 100;
    config.bonusAtk = 200; // 300 ATK: Supernova halves 3000 HP then hits for 1500
    auto report = runSimulation(config);
    ASSERT_TRUE(report.has_value());
    EXPECT_EQ(report->tally.wins, 200u);
    EXPECT_GT(report->tally.hpLeftOnWins, 0u);
}

TEST(SimErrors, UnknownItemNamesTheItem) {
    SimConfig config = startingFighter(10, 1);
    config.weaponId = "iron_swrod";
    auto report = runSimulation(config);
    ASSERT_FALSE(report.has_value());
    EXPECT_NE(report.error().find("iron_swrod"), std::string::npos);
}

TEST(SimErrors, BadInputsRunNoBattles) {
    auto expectError = [](SimConfig config, std::string_view needle) {
        auto report = runSimulation(config);
        ASSERT_FALSE(report.has_value());
        EXPECT_NE(report.error().find(needle), std::string::npos) << report.error();
    };
    SimConfig base = startingFighter(10, 1);
    SimConfig c = base;
    c.pathId = "wizard";
    expectError(c, "wizard");
    c = base;
    c.enemyId = "dragon";
    expectError(c, "dragon");
    c = base;
    c.skills = {"fireball"};
    expectError(c, "fireball");
    c = base;
    c.battles = 0;
    expectError(c, "battles");
    c = base;
    c.threads = 0;
    expectError(c, "threads");
    c = base;
    c.weaponId = "steel_shield"; // exists but is armor
    expectError(c, "steel_shield");
    c = base;
    c.armorLevel = 11;
    expectError(c, "level");
    c = base;
    c.bonusAtk = -1;
    expectError(c, "--bonus-atk");
    c = base;
    c.bonusHp = heroes::game::kMaxStat + 1;
    expectError(c, "--bonus-hp");
}

TEST(SimArgs, ParsesTheFullCommandLine) {
    const std::vector<std::string_view> args = {"--path",      "rogue",
                                                "--enemy",     "medusa",
                                                "--battles",   "50",
                                                "--seed",      "9",
                                                "--threads",   "3",
                                                "--policy",    "smart",
                                                "--weapon",    "fire_claymore:4",
                                                "--skills",    "angel_wings,supernova",
                                                "--bonus-atk", "20",
                                                "--bench"};
    auto options = parseArgs(args, "somewhere", 8);
    ASSERT_TRUE(options.has_value()) << options.error();
    EXPECT_EQ(options->config.pathId, "rogue");
    EXPECT_EQ(options->config.battles, 50u);
    EXPECT_EQ(options->config.threads, 3u);
    EXPECT_EQ(options->config.policy, Policy::Smart);
    EXPECT_EQ(options->config.weaponId, "fire_claymore");
    EXPECT_EQ(options->config.weaponLevel, 4);
    EXPECT_EQ(options->config.skills, (std::vector<std::string>{"angel_wings", "supernova"}));
    EXPECT_EQ(options->config.bonusAtk, 20);
    EXPECT_TRUE(options->bench);
    EXPECT_EQ(options->config.dataDir, "somewhere");
}

TEST(SimArgs, RejectsBadCommandLines) {
    auto error = [](std::vector<std::string_view> args) {
        auto options = parseArgs(args, "d", 1);
        return options ? std::string() : options.error();
    };
    EXPECT_NE(error({"--path", "fighter", "--enemy", "medusa", "--battles", "5", "--seed", "1",
                     "--policy", "berserk"})
                  .find("attack, smart"),
              std::string::npos);
    EXPECT_NE(error({"--path", "fighter", "--enemy", "medusa", "--seed", "1"}).find("--battles"),
              std::string::npos);
    EXPECT_NE(error({"--path", "fighter", "--enemy", "medusa", "--battles", "x", "--seed", "1"})
                  .find("number"),
              std::string::npos);
    EXPECT_NE(error({"--bogus", "1"}).find("--bogus"), std::string::npos);
    EXPECT_NE(error({"--path"}).find("needs a value"), std::string::npos);
}

namespace {

// The balance-simulator spec's Avaritia band, on the first 4,000 battles of seed 7 (a prefix of
// the 20,000 in docs/benchmarks.md, so CI stays fast). Returns why the data misses it, or "".
std::string avaritiaBandProblem(const std::filesystem::path& dataDir) {
    for (const char* path : {"fighter", "hunter", "rogue"}) {
        SimConfig config;
        config.dataDir = dataDir;
        config.pathId = path;
        config.enemyId = "avaritia";
        config.battles = 4000;
        config.seed = 7;
        config.threads = 8;
        config.policy = Policy::Smart;
        const auto start = runSimulation(config);
        config.weaponLevel = 5;
        config.armorLevel = 5;
        config.skills = {"angel_wings", "meteor_fist"};
        const auto mid = runSimulation(config);
        if (!start || !mid)
            return std::string(path) + ": " + (start ? mid.error() : start.error());
        const double midRate = 100.0 * static_cast<double>(mid->tally.wins) / 4000;
        const double startRate = 100.0 * static_cast<double>(start->tally.wins) / 4000;
        if (midRate < 40 || midRate > 80 || startRate >= 5)
            return std::format("{}: mid-game {:.2f}%, starting {:.2f}%", path, midRate, startRate);
    }
    return {};
}

} // namespace

TEST(AvaritiaBalance, ShippedStatsStayInTheBand) {
    EXPECT_EQ(avaritiaBandProblem(std::filesystem::path(HEROES_SOURCE_DIR) / "data"), "");
}

TEST(AvaritiaBalance, TheCheckCatchesAStrongerAvaritia) {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "heroes_avaritia_band";
    fs::remove_all(root);
    fs::create_directories(root);
    fs::copy(fs::path(HEROES_SOURCE_DIR) / "data", root / "data", fs::copy_options::recursive);
    fs::copy(fs::path(HEROES_SOURCE_DIR) / "assets", root / "assets", fs::copy_options::recursive);
    std::ifstream in(root / "data" / "enemies.json");
    nlohmann::json enemies = nlohmann::json::parse(in);
    in.close();
    for (auto& enemy : enemies["enemies"])
        if (enemy["id"] == "avaritia")
            enemy["atk"] = enemy["atk"].get<int>() + 50;
    std::ofstream(root / "data" / "enemies.json") << enemies.dump();
    // Caught by the band itself, not by the copy failing to load.
    EXPECT_NE(avaritiaBandProblem(root / "data").find("mid-game"), std::string::npos)
        << avaritiaBandProblem(root / "data");
}
