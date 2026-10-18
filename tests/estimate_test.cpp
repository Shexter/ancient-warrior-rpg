#include "game/estimate.hpp"

#include "state_helpers.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <string>

using namespace heroes::game;

TEST(Estimate, CavesLeaveTheStateAndItsRngUntouched) {
    GameState state = freshState("hunter", 5);
    const GameState before = state;
    const auto start = std::chrono::steady_clock::now();
    for (int cave = 1; cave <= 30; ++cave)
        (void)caveWinChance(state, shippedContent(), cave);
    const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start);
    RecordProperty("all_30_caves_ms", std::to_string(ms.count()));
    std::printf("all 30 cave estimates: %.1f ms\n", ms.count());
    EXPECT_EQ(state, before);
}

TEST(Estimate, RepeatsForTheSameState) {
    const GameState state = freshState();
    for (int cave : {1, 4, 9, 20})
        EXPECT_EQ(caveWinChance(state, shippedContent(), cave),
                  caveWinChance(state, shippedContent(), cave));
}

TEST(Estimate, FollowsTheHerosStrengthAndHp) {
    GameState state = freshState();
    EXPECT_EQ(caveWinChance(state, shippedContent(), 1), 100);
    EXPECT_EQ(caveWinChance(state, shippedContent(), 30), 0);
    const int full = caveWinChance(state, shippedContent(), 3);
    state.hero.hp = 1;
    EXPECT_LT(caveWinChance(state, shippedContent(), 3), full);
}

// ---- Missions and the Hub's "Next:" line ----

TEST(MissionEstimate, LeavesTheStateUntouchedAndRepeats) {
    GameState state = freshState();
    state.hero.level = 15;
    const GameState before = state;
    const auto start = std::chrono::steady_clock::now();
    const int odds = missionWinChance(state, shippedContent(), "avaritia", &shippedScripts());
    const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start);
    RecordProperty("mission_estimate_ms", std::to_string(ms.count()));
    std::printf("mission estimate: %.1f ms\n", ms.count());
    EXPECT_EQ(state, before);
    EXPECT_EQ(odds, missionWinChance(state, shippedContent(), "avaritia", &shippedScripts()));
}

TEST(NextGoal, FollowsTheWordingTable) {
    const Content& content = shippedContent();
    auto goal = [&](const GameState& s) { return nextGoal(s, content, &shippedScripts()); };
    GameState state = freshState();
    state.hero.blessings = 0;

    state.hero.hp = 0;
    state.hero.blessings = 2;
    EXPECT_EQ(goal(state), "Next: visit the Prior to be revived");

    state.hero.hp = state.hero.maxHp;
    EXPECT_EQ(goal(state), "Next: spend 2 blessings at the Prior");
    state.hero.blessings = 1;
    EXPECT_EQ(goal(state), "Next: spend 1 blessing at the Prior");

    state.hero.blessings = 0;
    EXPECT_EQ(goal(state), "Next: train to Lv 15 for Act I: \"Avaritia\"");

    state.hero.level = 15;
    const std::string ready = goal(state);
    EXPECT_TRUE(ready.starts_with("Next: Avaritia (Lv 15) — your odds ≈")) << ready;
    EXPECT_TRUE(ready.ends_with("upgrade gear, learn skills, or train") ||
                ready.ends_with("you're ready"))
        << ready;
    const int odds = missionWinChance(state, content, "avaritia", &shippedScripts());
    EXPECT_EQ(ready.ends_with("you're ready"), odds >= 50);

    state.hero.completedMissions = {"avaritia"};
    EXPECT_EQ(goal(state), "Next: train to Lv 18 for Act I: \"Gula\"");
    state.hero.completedMissions = {"avaritia", "gula", "ira"};
    EXPECT_EQ(goal(state), "Next: the Gorgon awaits (Lv 20), if you dare");
    state.hero.completedMissions.insert("the_gorgon");
    EXPECT_EQ(goal(state), "Act I is complete.");
}

TEST(NextGoal, QuotesBothSidesOfThe50PercentLine) {
    GameState state = freshState();
    state.hero.level = 22;
    state.hero.completedMissions = {"avaritia", "gula"};
    // A fresh level-22 hero with starting gear is far below 50% against Ira.
    EXPECT_TRUE(nextGoal(state, shippedContent(), &shippedScripts()).ends_with("upgrade gear, learn skills, or train"));
    state.hero.atk = 2000;
    state.hero.def = 500;
    state.hero.maxHp = state.hero.hp = 20000;
    EXPECT_TRUE(nextGoal(state, shippedContent(), &shippedScripts()).ends_with("you're ready"));
}
