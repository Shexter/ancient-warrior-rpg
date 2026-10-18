#include "sim/sim.hpp"

#include "game/content.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <set>
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
    c = base;
    c.blessings = -1;
    expectError(c, "--blessings");
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
                                                "--blessings", "14",
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
    EXPECT_EQ(options->config.blessings, 14);
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

// The fun-pass band for an Act I boss, on the first 4,000 battles of seed 7 (a prefix of the
// 20,000 in docs/benchmarks.md, so CI stays fast): with its intended build, smart wins 50-80% and
// attack-only wins at least 30 points less; with starting gear smart wins under 5%.
// Returns why the data misses it, or "".
struct BossBuild {
    const char* boss;
    const char* weapon;
    int weaponLevel;
    const char* armor;
    int armorLevel;
    int blessings;
    std::vector<std::pair<std::string, int>> items;
    std::set<std::string> skills;
};

const BossBuild kActOne[] = {
    {"avaritia", "iron_sword", 5, "steel_shield", 5, 14, {{"health_potion", 2}},
     {"angel_wings", "meteor_fist"}},
    {"gula", "fire_claymore", 1, "steel_shield", 5, 17, {{"health_potion", 3}, {"mana_potion", 1}},
     {"angel_wings", "meteor_fist", "smite"}},
    {"ira", "fire_claymore", 3, "diamond_plate", 1, 21, {{"health_potion", 3}, {"mana_potion", 2}},
     {"angel_wings", "meteor_fist", "smite", "sanctuary"}},
};

std::string bandProblem(const std::filesystem::path& dataDir, const BossBuild& build) {
    for (const char* path : {"fighter", "hunter", "rogue"}) {
        SimConfig config;
        config.dataDir = dataDir;
        config.pathId = path;
        config.enemyId = build.boss;
        config.battles = 4000;
        config.seed = 7;
        config.threads = 8;
        config.policy = Policy::Smart;
        const auto start = runSimulation(config);
        config.weaponId = build.weapon;
        config.weaponLevel = build.weaponLevel;
        config.armorId = build.armor;
        config.armorLevel = build.armorLevel;
        config.skills.assign(build.skills.begin(), build.skills.end());
        config.blessings = build.blessings;
        config.items = build.items;
        const auto smart = runSimulation(config);
        config.policy = Policy::Attack;
        const auto attack = runSimulation(config);
        if (!start || !smart || !attack)
            return std::string(path) + ": " +
                   (!start   ? start.error()
                    : !smart ? smart.error()
                             : attack.error());
        const auto rate = [](const auto& r) {
            return 100.0 * static_cast<double>(r->tally.wins) / 4000;
        };
        if (rate(smart) < 50 || rate(smart) > 80 || rate(smart) - rate(attack) < 30 ||
            rate(start) >= 5)
            return std::format("{} {}: smart {:.2f}%, attack {:.2f}%, starting {:.2f}%", build.boss,
                               path, rate(smart), rate(attack), rate(start));
    }
    return {};
}

} // namespace

TEST(ActOneBalance, ShippedBossesStayInTheBand) {
    for (const BossBuild& build : kActOne)
        EXPECT_EQ(bandProblem(std::filesystem::path(HEROES_SOURCE_DIR) / "data", build), "");
}

TEST(ActOneBalance, TheCheckCatchesAStrongerAvaritia) {
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
    EXPECT_NE(bandProblem(root / "data", kActOne[0]).find("smart"), std::string::npos)
        << bandProblem(root / "data", kActOne[0]);
}

TEST(Sim, ItemsComeIntoEveryBattle) {
    const std::string_view args[] = {
        "--path", "fighter", "--enemy", "medusa",  "--battles",
        "10",     "--seed",  "7",       "--items", "health_potion:2,eternal_youth:1"};
    const auto options = parseArgs(args, std::filesystem::path(HEROES_SOURCE_DIR) / "data", 1);
    ASSERT_TRUE(options.has_value()) << options.error();
    EXPECT_EQ(options->config.items, (std::vector<std::pair<std::string, int>>{
                                         {"health_potion", 2}, {"eternal_youth", 1}}));
    EXPECT_TRUE(runSimulation(options->config).has_value());

    const std::string_view bad[] = {"--path", "fighter", "--enemy", "medusa",  "--battles",
                                    "1",      "--seed",  "7",       "--items", "health_potion"};
    EXPECT_EQ(parseArgs(bad, std::filesystem::path(HEROES_SOURCE_DIR) / "data", 1).error(),
              "--items expects id:count,... (got 'health_potion')");
    SimConfig notConsumable = startingFighter(1, 1);
    notConsumable.items = {{"iron_sword", 1}};
    EXPECT_EQ(runSimulation(notConsumable).error(), "unknown consumable 'iron_sword'");
}

TEST(Sim, BlessingsSpreadEvenlyWithRemaindersToHpFirst) {
    const auto plain = runSimulation(startingFighter(1, 1));
    SimConfig config = startingFighter(1, 1);
    config.blessings = 14; // 4 HP, 4 Fervor, 3 ATK, 3 DEF
    const auto blessed = runSimulation(config);
    ASSERT_TRUE(plain.has_value());
    ASSERT_TRUE(blessed.has_value());
    EXPECT_EQ(blessed->hp, plain->hp + 4 * 30);
    EXPECT_EQ(blessed->mp, plain->mp + 4 * 10);
    EXPECT_EQ(blessed->atk, plain->atk + 3 * 6);
    EXPECT_EQ(blessed->def, plain->def + 3 * 3);
}
