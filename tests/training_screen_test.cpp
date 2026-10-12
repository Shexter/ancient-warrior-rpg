#include "game/estimate.hpp"
#include "game/training.hpp"

#include "frame_text.hpp"
#include "screen_harness.hpp"

#include <format>

using namespace screen_test;

namespace {

// What a session at `cave` does to a fresh game when the player plays `rounds` and leaves.
struct Expected {
    game::GameState after = freshGame();
    std::vector<game::TrainingRound> rounds;
    std::int64_t banked = 0;
};

Expected expected(int cave, int rounds, bool leave = true) {
    Expected e;
    auto session = game::TrainingSession::start(e.after, game::shippedContent(), cave).value();
    for (int i = 0; i < rounds && !session.over(); ++i)
        e.rounds.push_back(session.next());
    if (leave)
        e.banked = session.leave();
    return e;
}

// Hub -> cave list -> cave -> Playback of round 1.
void startTraining(Harness& h, int cave, int hp = -1) {
    game::GameState state = freshGame();
    if (hp >= 0)
        state.hero.hp = hp;
    h.startAtHub(std::move(state));
    ASSERT_TRUE(h.press({ch('1')}));
    ASSERT_TRUE(h.type(std::to_string(cave)));
    ASSERT_TRUE(h.press({kEnter}));
}

} // namespace

TEST(TrainingScreen, CaveListShowsTheOddsAndRejectsOutOfRangeNumbers) {
    Harness h("training_setup");
    h.startAtHub(freshGame());
    ASSERT_TRUE(h.press({ch('1')}));
    const Framebuffer frame = h.frame();
    EXPECT_TRUE(frameContains(frame, "Cave level (1–30):"));
    EXPECT_TRUE(frameContains(frame, "Cave  1  ≈100%"));
    EXPECT_TRUE(frameContains(frame, "Cave 30  ≈0%"));
    const int odds = game::caveWinChance(freshGame(), game::shippedContent(), 5);
    EXPECT_TRUE(frameContains(frame, std::format("Cave  5  ≈{}%", odds)));
    ASSERT_TRUE(h.type("31"));
    ASSERT_TRUE(h.press({kEnter}));
    EXPECT_TRUE(frameContains(h.frame(), "Enter a number from 1 to 30"));
    ASSERT_TRUE(h.press({kEsc}));
    EXPECT_EQ(h.app->depth(), 1u); // back at the Hub, nothing trained
    EXPECT_EQ(*h.session.state, freshGame());
}

TEST(TrainingScreen, TheRoundIsAppliedAtOnceAndPlaybackFollowsTheEvents) {
    Harness h("training_play");
    startTraining(h, 1);
    const Expected e = expected(1, 1, false);
    EXPECT_EQ(*h.session.state, e.after); // applied before any playback
    EXPECT_TRUE(h.session.dirty);

    // 10 events per second: after 0.1 s the first hit has landed.
    const auto& first = std::get<game::ev::Damaged>(e.rounds[0].events[0]);
    ASSERT_EQ(first.target, game::Side::Enemy);
    EXPECT_EQ(cellText(h.frame(), 56, 17, 7), "100/100");
    ASSERT_TRUE(h.wait(0.1));
    EXPECT_EQ(cellText(h.frame(), 56, 17, 6), std::format("{}/100 ", first.hpAfter).substr(0, 6));
}

TEST(TrainingScreen, AfterARoundThePlayerChoosesWithTheStakesInView) {
    Harness h("training_choice");
    startTraining(h, 1);
    const Expected e = expected(1, 1, false);
    const game::TrainingRound& r = e.rounds[0];
    ASSERT_TRUE(h.press({kEnter})); // finish the round
    const Framebuffer frame = h.frame();
    const std::string line = std::format("Round 1 · {} defeated · +{} gold · +{} exp",
                                         r.monsterName, r.goldGained, r.expGained);
    EXPECT_NE(rowText(frame, 22).find(line), std::string::npos) << rowText(frame, 22);
    EXPECT_EQ(cellText(frame, 56, 17, 5), "0/100");
    EXPECT_TRUE(frameContains(frame, "Streak 1 · Pot 0 gold"));
    const int odds = game::caveWinChance(e.after, game::shippedContent(), 1);
    EXPECT_NE(rowText(frame, 23).find("Continue"), std::string::npos);
    EXPECT_NE(rowText(frame, 23).find(std::format("Next round ≈{}%", odds)), std::string::npos);
    ASSERT_TRUE(h.wait(5)); // nothing happens until the player chooses
    EXPECT_FALSE(frameContains(h.frame(), "CATACOMB RESULTS"));
}

TEST(TrainingScreen, ContinueTwiceThenLeaveBanksThePot) {
    Harness h("training_leave");
    startTraining(h, 1);
    ASSERT_TRUE(h.press({kEnter, ch('1'), kEnter, ch('1'), kEnter, ch('2')}));
    const Expected e = expected(1, 3);
    ASSERT_GT(e.banked, 0);
    EXPECT_EQ(*h.session.state, e.after);
    const Framebuffer totals = h.frame();
    EXPECT_TRUE(frameContains(totals, "CATACOMB RESULTS"));
    EXPECT_TRUE(frameContains(totals, std::format("+{}", e.after.hero.gold - 20000)));
    EXPECT_TRUE(frameContains(totals, std::format("Gold {}", e.after.hero.gold))); // panel
    EXPECT_TRUE(frameContains(totals, std::format("Pot banked    +{} gold", e.banked)));
    EXPECT_TRUE(frameContains(totals, std::format("EXP           +{}", e.after.hero.exp)));
    EXPECT_TRUE(frameContains(totals, "Levels gained 2 · 2 blessings at the Prior"));

    ASSERT_TRUE(h.press({kEnter}));
    EXPECT_EQ(h.app->depth(), 1u);
    EXPECT_TRUE(frameContains(
        h.frame(), std::format("Training: won 3 of 3 rounds, banked a {} gold pot.", e.banked)));
}

TEST(TrainingScreen, EscAtTheChoiceLeaves) {
    Harness h("training_esc");
    startTraining(h, 1);
    ASSERT_TRUE(h.press({kEsc, kEsc})); // finish the round, then leave
    EXPECT_TRUE(frameContains(h.frame(), "CATACOMB RESULTS"));
    EXPECT_EQ(*h.session.state, expected(1, 1).after);
}

TEST(TrainingScreen, ADefeatLosesThePot) {
    Harness h("training_pot_lost");
    startTraining(h, 1);
    ASSERT_TRUE(h.press({kEnter, ch('1'), kEnter, ch('1'), kEnter}));
    const std::int64_t gold = h.session.state->hero.gold;
    // Weaken the hero so the next round is lost.
    game::Hero& hero = h.session.state->hero;
    hero.atk = 0;
    hero.weapon.reset();
    hero.hp = 1;
    ASSERT_TRUE(h.press({ch('1'), kEnter}));
    EXPECT_TRUE(frameContains(h.frame(), "gold pot is lost"));
    ASSERT_TRUE(h.press({kEnter}));
    const Framebuffer totals = h.frame();
    EXPECT_TRUE(frameContains(totals, "ARIA has been injured badly"));
    EXPECT_TRUE(frameContains(totals, "gold pot is lost"));
    EXPECT_EQ(h.session.state->hero.gold, gold); // nothing banked
    ASSERT_TRUE(h.press({kEnter}));
    EXPECT_TRUE(frameContains(h.frame(), "Training: won 3 of 4 rounds, then fell."));
}

TEST(TrainingScreen, DefeatGoesToTheTotalsAndTheHubThenBlocksTraining) {
    Harness h("training_defeat");
    startTraining(h, 30, 1); // 1 HP in the hardest cave
    ASSERT_TRUE(h.wait(10)); // the round plays out, then the totals follow on their own
    EXPECT_TRUE(frameContains(h.frame(), "ARIA has been injured badly"));
    EXPECT_EQ(h.session.state->hero.hp, 0);
    ASSERT_TRUE(h.press({kEnter}));
    ASSERT_TRUE(h.wait(1.0 / 60)); // the Hub refreshes its menu on its first step back
    EXPECT_NE(rowText(h.frame(), 7).find("Fallen: visit the Prior"), std::string::npos);
}
