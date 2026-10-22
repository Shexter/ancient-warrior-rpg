#include "app/screens/expedition.hpp"
#include "app/screens/mission.hpp"
#include "frame_text.hpp"
#include "screen_harness.hpp"

#include <gtest/gtest.h>

using namespace screen_test;
namespace dungeon = heroes::game::dungeon;

namespace {

const KeyEvent kKeyEsc{Key::Escape};

dungeon::Pos heroOn(const Framebuffer& frame) {
    for (int y = 1; y < 20; ++y)
        for (int x = 0; x < frame.width(); ++x)
            if (frame.at(x, y).glyph == U'@')
                return {x, y};
    return {-1, -1};
}

// A game whose saved expedition has a Giant Rat standing just east of the hero on floor 1, and
// the key that walks into it.
game::GameState besideARat(KeyEvent& into) {
    game::GameState state = freshGame();
    game::GameState scratch = state;
    auto exp = dungeon::startExpedition(scratch, shippedDungeon()).value();
    exp.floor.enemies.clear();
    const dungeon::Pos h = exp.hero;
    for (const auto& [key, at] : {std::pair{ch('d'), dungeon::Pos{h.x + 1, h.y}}, std::pair{ch('a'), dungeon::Pos{h.x - 1, h.y}},
                                  std::pair{ch('s'), dungeon::Pos{h.x, h.y + 1}}, std::pair{ch('w'), dungeon::Pos{h.x, h.y - 1}}})
        if (exp.floor.walkable(at)) {
            exp.floor.enemies.push_back({"rat", at});
            into = key;
            break;
        }
    state.expedition = dungeon::toSave(exp);
    return state;
}

} // namespace

TEST(ExpeditionScreen, TheHubOpensFloorOneAroundTheHero) {
    Harness h("expedition_open");
    h.startAtHub(freshGame());
    ASSERT_TRUE(h.press({kHubExpedition}));
    const Framebuffer frame = h.frame();
    EXPECT_NE(rowText(frame, 0).find("THE CATACOMBS — Floor 1/5"), std::string::npos) << rowText(frame, 0);
    EXPECT_NE(heroOn(frame).x, -1);
    EXPECT_EQ(frame.at(0, 20).glyph, U'─');
    EXPECT_NE(rowText(frame, 21).find("ARIA"), std::string::npos);
    EXPECT_NE(rowText(frame, 23).find("Esc menu"), std::string::npos);
    ASSERT_TRUE(h.session.state->expedition.has_value()); // kept in the game, ready to save
    EXPECT_EQ(h.session.state->expedition->floor, 1);
}

TEST(ExpeditionScreen, SavingMidRunAndContinuingResumesIt) {
    Harness h("expedition_save");
    h.startAtHub(freshGame());
    ASSERT_TRUE(h.press({kHubExpedition}));
    for (const char c : std::string("dddssaw"))
        ASSERT_TRUE(h.press({ch(c)}));
    ASSERT_TRUE(h.press({kKeyEsc, ch('2')})); // Esc menu: Save
    EXPECT_TRUE(frameContains(h.frame(), "Game saved."));
    ASSERT_TRUE(h.press({kEnter, kKeyEsc, ch('1')})); // dismiss, then Esc menu: Resume
    const Framebuffer before = h.frame();
    const game::ExpeditionSave saved = *h.session.state->expedition;

    Harness again("expedition_save_again");
    again.session.saveFile = h.session.saveFile;
    again.startAtTitle();
    ASSERT_TRUE(again.press({ch('2')})); // Continue
    ASSERT_TRUE(again.session.state.has_value());
    EXPECT_EQ(*again.session.state->expedition, saved);
    EXPECT_TRUE(frameContains(again.frame(), "In progress"));
    ASSERT_TRUE(again.press({kHubExpedition}));
    const Framebuffer after = again.frame();
    for (int y = 0; y <= 21; ++y) // the title, the map and the status row; not the last message
        EXPECT_EQ(rowText(after, y), rowText(before, y)) << y;
}

TEST(ExpeditionScreen, AFightOpensTheBattleAndFleeingComesBackToTheMap) {
    KeyEvent into{Key::Char};
    Harness h("expedition_flee");
    h.startAtHub(besideARat(into));
    ASSERT_TRUE(h.press({kHubExpedition}));
    const dungeon::Pos at = heroOn(h.frame());
    ASSERT_TRUE(h.press({into}));
    ASSERT_TRUE(h.press({{Key::Char}})); // the next step opens the battle
    ASSERT_NE(dynamic_cast<const BattleScreen*>(h.app->top()), nullptr);
    EXPECT_TRUE(frameContains(h.frame(), "The Catacombs · Floor 1"));
    ASSERT_TRUE(h.press({ch('5')})); // Flee
    ASSERT_TRUE(h.wait(5));          // the events play out, then it pops back
    ASSERT_NE(dynamic_cast<const ExpeditionScreen*>(h.app->top()), nullptr);
    EXPECT_EQ(heroOn(h.frame()), at); // the same place on the map
    EXPECT_EQ(h.session.state->expedition->enemies[0].sleepTurns, 3);
}

TEST(ExpeditionScreen, FallingReturnsToTheHubWithNoRunLeft) {
    KeyEvent into{Key::Char};
    game::GameState state = besideARat(into);
    state.hero.hp = 1; // any hit is the last
    Harness h("expedition_fall");
    h.startAtHub(std::move(state));
    ASSERT_TRUE(h.press({kHubExpedition, into, {Key::Char}}));
    ASSERT_NE(dynamic_cast<const BattleScreen*>(h.app->top()), nullptr);
    ASSERT_TRUE(h.press({ch('4')})); // Defend: the rat's hit takes the last HP
    ASSERT_TRUE(h.wait(5));
    ASSERT_TRUE(h.wait(0.1));
    EXPECT_EQ(h.app->depth(), 1u);
    EXPECT_EQ(h.session.state->hero.hp, 0);
    EXPECT_FALSE(h.session.state->expedition.has_value());
    EXPECT_NE(h.session.lastMessage.find("You fell in the Catacombs"), std::string::npos);
}
