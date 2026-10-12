#include "game/training.hpp"

#include "frame_text.hpp"
#include "screen_harness.hpp"

#include <format>

using namespace screen_test;

namespace {

// What runTraining does to a fresh game, computed directly for comparison.
struct Expected {
    game::GameState after = freshGame();
    std::vector<game::TrainingRound> rounds;
};

Expected expected(int cave, int rounds, int hp = -1) {
    Expected e;
    if (hp >= 0)
        e.after.hero.hp = hp;
    e.rounds = game::runTraining(e.after, game::shippedContent(), cave, rounds).value();
    return e;
}

// Hub -> Training Setup -> cave and rounds -> Playback.
void startTraining(Harness& h, int cave, int rounds, int hp = -1) {
    game::GameState state = freshGame();
    if (hp >= 0)
        state.hero.hp = hp;
    h.startAtHub(std::move(state));
    ASSERT_TRUE(h.press({ch('1')}));
    ASSERT_TRUE(h.type(std::to_string(cave)));
    ASSERT_TRUE(h.press({kEnter}));
    ASSERT_TRUE(h.type(std::to_string(rounds)));
    ASSERT_TRUE(h.press({kEnter}));
}

} // namespace

TEST(TrainingScreen, SetupRejectsOutOfRangeNumbersAndEscGoesBack) {
    Harness h("training_setup");
    h.startAtHub(freshGame());
    ASSERT_TRUE(h.press({ch('1')}));
    EXPECT_TRUE(frameContains(h.frame(), "Cave level (1–30):"));
    ASSERT_TRUE(h.type("31"));
    ASSERT_TRUE(h.press({kEnter}));
    EXPECT_TRUE(frameContains(h.frame(), "Enter a number from 1 to 30"));
    ASSERT_TRUE(h.press({{Key::Backspace}, {Key::Backspace}, ch('1'), kEnter, ch('0'), kEnter}));
    EXPECT_TRUE(frameContains(h.frame(), "Enter a number from 1 to 100"));
    ASSERT_TRUE(h.press({kEsc}));
    EXPECT_EQ(h.app->depth(), 1u); // back at the Hub, nothing trained
    EXPECT_EQ(*h.session.state, freshGame());
}

TEST(TrainingScreen, ResultsAreAppliedAtOnceAndPlaybackFollowsTheEvents) {
    Harness h("training_play");
    startTraining(h, 1, 5);
    const Expected e = expected(1, 5);
    EXPECT_EQ(*h.session.state, e.after); // applied before any playback
    EXPECT_TRUE(h.session.dirty);

    // 10 events per second: after 0.1 s the first hit has landed.
    const auto& first = std::get<game::ev::Damaged>(e.rounds[0].events[0]);
    ASSERT_EQ(first.target, game::Side::Enemy);
    EXPECT_EQ(cellText(h.frame(), 56, 17, 7), "100/100");
    ASSERT_TRUE(h.wait(0.1));
    EXPECT_EQ(cellText(h.frame(), 56, 17, 6), std::format("{}/100 ", first.hpAfter).substr(0, 6));
}

TEST(TrainingScreen, EnterFinishesTheRoundAndShowsItsSummary) {
    Harness h("training_enter");
    startTraining(h, 1, 5);
    const Expected e = expected(1, 5);
    const game::TrainingRound& r = e.rounds[0];
    ASSERT_TRUE(h.press({kEnter}));
    std::string line = std::format("Round 1/5 · {} defeated · +{} gold · +{} exp", r.monsterName,
                                   r.goldGained, r.expGained);
    EXPECT_NE(rowText(h.frame(), 22).find(line), std::string::npos) << rowText(h.frame(), 22);
    EXPECT_EQ(cellText(h.frame(), 56, 17, 5), "0/100");
}

TEST(TrainingScreen, TotalsMatchTheStatusPanelAndReturnToTheHub) {
    Harness h("training_totals");
    startTraining(h, 1, 5);
    const Expected e = expected(1, 5);
    ASSERT_TRUE(h.press({kEsc}));
    const Framebuffer totals = h.frame();
    EXPECT_TRUE(frameContains(totals, "CATACOMB RESULTS"));
    const std::int64_t gold = e.after.hero.gold - 20000;
    EXPECT_TRUE(frameContains(totals, std::format("+{}", gold)));
    EXPECT_TRUE(frameContains(totals, std::format("Gold {}", e.after.hero.gold))); // panel
    EXPECT_TRUE(frameContains(totals, std::format("EXP           +{}", e.after.hero.exp)));
    EXPECT_TRUE(frameContains(totals, std::format("EXP {}", e.after.hero.exp))); // panel

    ASSERT_TRUE(h.press({kEnter}));
    EXPECT_EQ(h.app->depth(), 1u);
    EXPECT_TRUE(frameContains(h.frame(), "Training: won 5 of 5 rounds."));
}

TEST(TrainingScreen, PlaybackReachesTheTotalsOnItsOwn) {
    Harness h("training_auto");
    startTraining(h, 1, 2);
    ASSERT_TRUE(h.wait(10));
    EXPECT_TRUE(frameContains(h.frame(), "CATACOMB RESULTS"));
}

TEST(TrainingScreen, DefeatIsShownAndTheHubThenBlocksTraining) {
    Harness h("training_defeat");
    startTraining(h, 30, 3, 1); // 1 HP in the hardest cave
    ASSERT_TRUE(h.press({kEsc}));
    EXPECT_TRUE(frameContains(h.frame(), "ARIA has been injured badly"));
    EXPECT_EQ(h.session.state->hero.hp, 0);
    ASSERT_TRUE(h.press({kEnter}));
    ASSERT_TRUE(h.wait(1.0 / 60)); // the Hub refreshes its menu on its first step back
    EXPECT_NE(rowText(h.frame(), 7).find("Fallen: visit the Prior"), std::string::npos);
}
