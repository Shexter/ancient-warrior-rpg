#include "game/training.hpp"

#include "event_format.hpp"
#include "state_helpers.hpp"

#include <gtest/gtest.h>

#include <fstream>

#include <nlohmann/json.hpp>

using namespace heroes::game;

namespace {

std::string eventLog(const std::vector<Event>& events) {
    std::string log;
    for (const Event& e : events)
        log += toString(e) + '\n';
    return log;
}

namespace fs = std::filesystem;

// Shipped data with every dropRate zeroed, so no item can ever drop.
fs::path dropFreeDataDir() {
    const fs::path root = fs::temp_directory_path() / "heroes_training_nodrop";
    fs::remove_all(root);
    fs::create_directories(root / "assets" / "sprites");
    fs::copy_file(fs::path(HEROES_SOURCE_DIR) / "assets" / "sprites" / "medusa.sprite",
                  root / "assets" / "sprites" / "medusa.sprite");
    fs::create_directories(root / "data");
    for (const auto& entry : fs::directory_iterator(fs::path(HEROES_SOURCE_DIR) / "data")) {
        if (entry.path().filename() != "items.json") {
            fs::copy_file(entry.path(), root / "data" / entry.path().filename());
            continue;
        }
        std::ifstream in(entry.path());
        nlohmann::json items = nlohmann::json::parse(in);
        for (auto& item : items["items"])
            item["dropRate"] = 0;
        std::ofstream(root / "data" / "items.json") << items.dump();
    }
    return root / "data";
}

} // namespace

TEST(Training, MonsterVector) {
    Rng rng = Rng::stream(5, 0);
    const TrainingMonster m = rollTrainingMonster(rng, 10);
    EXPECT_EQ(m.atk, 320);
    EXPECT_EQ(m.def, 60);
    EXPECT_EQ(m.goldReward, 1000);
    EXPECT_EQ(m.maxHp, 1000);
    EXPECT_EQ(m.expReward, 1000);
    EXPECT_EQ(m.name, "Bat");
}

TEST(Training, MonsterNameTiers) {
    for (const auto& [level, name] : {std::pair{1, "Fox"}, {9, "Fox"}, {10, "Bat"}, {20, "Horse"},
                                      {29, "Horse"}, {30, "Bull"}}) {
        Rng rng = Rng::stream(1, 0);
        EXPECT_EQ(rollTrainingMonster(rng, level).name, name) << level;
    }
}

TEST(Training, DropVectors) {
    Rng rng = Rng::stream(5, 1);
    const Content& content = shippedContent();
    EXPECT_EQ(rollDrop(rng, content, 1), std::nullopt);
    EXPECT_EQ(rollDrop(rng, content, 1), "gold_leaf");
    EXPECT_EQ(rollDrop(rng, content, 1), std::nullopt);
    EXPECT_EQ(rollDrop(rng, content, 30), "gold_dust");
    EXPECT_EQ(rollDrop(rng, content, 30), std::nullopt);
}

TEST(Training, EmptyDropTableTerminatesWithoutDraws) {
    const auto loaded = loadContent(dropFreeDataDir());
    ASSERT_TRUE(loaded.has_value());
    Rng rng = Rng::stream(5, 1);
    const Rng untouched = rng;
    EXPECT_EQ(rollDrop(rng, *loaded, 30), std::nullopt);
    EXPECT_EQ(rng, untouched);

    GameState state = newGame(*loaded, "Aria", "fighter", 3);
    const auto rounds = runTraining(state, *loaded, 1, 5);
    ASSERT_TRUE(rounds.has_value());
    for (const TrainingRound& r : *rounds)
        EXPECT_FALSE(r.drop.has_value());
}

TEST(Training, RejectsBadInputsLeavingStateUnchanged) {
    for (const auto& [cave, rounds] : {std::pair{0, 1}, {31, 1}, {1, 0}, {1, 101}}) {
        GameState state = freshState();
        const GameState before = state;
        const auto result = runTraining(state, shippedContent(), cave, rounds);
        ASSERT_FALSE(result.has_value()) << cave << "," << rounds;
        EXPECT_EQ(result.error(), TownError::InvalidQuantity);
        EXPECT_EQ(state, before);
    }
}

TEST(Training, RefusesDeadOrUnequippedHero) {
    GameState dead = freshState();
    dead.hero.hp = 0;
    const GameState deadBefore = dead;
    EXPECT_EQ(runTraining(dead, shippedContent(), 1, 1).error(), TownError::HeroDead);
    EXPECT_EQ(dead, deadBefore);

    GameState bare = freshState();
    bare.hero.weapon.reset();
    const GameState bareBefore = bare;
    EXPECT_EQ(runTraining(bare, shippedContent(), 1, 1).error(), TownError::NotEquipped);
    EXPECT_EQ(bare, bareBefore);
}

TEST(Training, VictoryPaysGoldAndExp) {
    GameState state = freshState();
    const auto rounds = runTraining(state, shippedContent(), 1, 1);
    ASSERT_TRUE(rounds.has_value());
    ASSERT_EQ(rounds->size(), 1u);
    const TrainingRound& r = rounds->front();
    EXPECT_EQ(r.outcome, Outcome::Victory);
    EXPECT_EQ(r.expGained, 100);
    EXPECT_GE(r.goldGained, 100);
    EXPECT_LE(r.goldGained, 190);
    EXPECT_EQ(state.hero.gold, 20000 + r.goldGained);
    EXPECT_EQ(state.hero.exp, 100);
    EXPECT_EQ(r.levelsGained, 1);
    EXPECT_GT(state.hero.hp, 0);
}

TEST(Training, DeathStopsLaterRoundsAndDeductsExp) {
    GameState state = freshState();
    state.hero.hp = 1;
    state.hero.exp = 5000;
    const auto rounds = runTraining(state, shippedContent(), 30, 5);
    ASSERT_TRUE(rounds.has_value());
    ASSERT_EQ(rounds->size(), 1u);
    EXPECT_EQ(rounds->front().outcome, Outcome::Defeat);
    EXPECT_EQ(state.hero.hp, 0);
    EXPECT_EQ(state.hero.exp, 5000 - 3000);
    EXPECT_EQ(state.hero.gold, 20000);
}

TEST(Training, DeathNeverMakesExpNegative) {
    GameState state = freshState();
    state.hero.hp = 1;
    state.hero.exp = 10;
    ASSERT_TRUE(runTraining(state, shippedContent(), 30, 1).has_value());
    EXPECT_EQ(state.hero.exp, 0);
}

TEST(Training, AdvancesMasterRngByExactlyOneFork) {
    GameState state = freshState("fighter", 9);
    const Rng expected = afterOneFork(state.rng);
    ASSERT_TRUE(runTraining(state, shippedContent(), 1, 7).has_value());
    EXPECT_EQ(state.rng, expected);
}

TEST(Training, SameSeedSameResult) {
    GameState a = freshState("fighter", 4);
    GameState b = freshState("fighter", 4);
    const auto ra = runTraining(a, shippedContent(), 2, 10);
    const auto rb = runTraining(b, shippedContent(), 2, 10);
    ASSERT_TRUE(ra && rb);
    EXPECT_EQ(a, b);
    ASSERT_EQ(ra->size(), rb->size());
    for (std::size_t i = 0; i < ra->size(); ++i)
        EXPECT_EQ(eventLog((*ra)[i].events), eventLog((*rb)[i].events));
}

TEST(Training, FullBagLosesTheDrop) {
    // Find a seed whose first round drops something, with a bag that cannot hold it.
    for (std::uint64_t seed = 1; seed < 500; ++seed) {
        GameState probe = freshState("fighter", seed);
        const auto first = runTraining(probe, shippedContent(), 1, 1);
        ASSERT_TRUE(first.has_value());
        if (!first->front().drop)
            continue;
        GameState state = freshState("fighter", seed);
        for (std::string_view id : kFillers)
            if (id != *first->front().drop && state.hero.bag.canAdd(id)) {
                ASSERT_TRUE(state.hero.bag.add(id, 1));
            }
        for (std::string_view id : {"iron_sword", "steel_shield", "health_potion", "mana_potion"})
            if (id != *first->front().drop && state.hero.bag.canAdd(id)) {
                ASSERT_TRUE(state.hero.bag.add(id, 1));
            }
        const auto result = runTraining(state, shippedContent(), 1, 1);
        ASSERT_TRUE(result.has_value());
        EXPECT_TRUE(result->front().dropLostBagFull);
        EXPECT_EQ(state.hero.bag.count(*result->front().drop), 0);
        return;
    }
    FAIL() << "no seed produced a drop";
}
