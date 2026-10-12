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
