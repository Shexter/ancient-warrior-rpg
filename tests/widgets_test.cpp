#include "app/widgets.hpp"

#include "frame_text.hpp"
#include "state_helpers.hpp"

#include <gtest/gtest.h>

using namespace heroes;
using namespace heroes::app;
using engine::Framebuffer;
using engine::Key;
using engine::KeyEvent;

namespace {

KeyEvent ch(char c) { return {Key::Char, c}; }

Menu threeWithMiddleDisabled() { return Menu({{"One"}, {"Two", false, "locked"}, {"Three"}}); }

} // namespace

TEST(Menu, ArrowsAndWsMoveWrapAndSkipDisabled) {
    Menu menu = threeWithMiddleDisabled();
    EXPECT_EQ(menu.selected(), 0);
    menu.handle({Key::Down});
    EXPECT_EQ(menu.selected(), 2); // skipped the disabled row
    menu.handle({Key::Down});
    EXPECT_EQ(menu.selected(), 0); // wrapped
    menu.handle(ch('w'));
    EXPECT_EQ(menu.selected(), 2);
    menu.handle(ch('s'));
    EXPECT_EQ(menu.selected(), 0);
    menu.handle({Key::Left}); // horizontal keys do nothing in a vertical menu
    EXPECT_EQ(menu.selected(), 0);
}

TEST(Menu, EnterAndDigitsChooseEnabledItemsOnly) {
    Menu menu = threeWithMiddleDisabled();
    EXPECT_EQ(menu.handle({Key::Enter}), 0);
    EXPECT_EQ(menu.handle(ch('3')), 2);
    EXPECT_EQ(menu.selected(), 2);
    EXPECT_EQ(menu.handle(ch('2')), std::nullopt); // disabled
    EXPECT_EQ(menu.handle(ch('9')), std::nullopt); // no such item
    EXPECT_EQ(menu.handle({Key::Escape}), std::nullopt);
}

TEST(Menu, FirstEnabledItemStartsSelected) {
    Menu menu({{"A", false}, {"B"}});
    EXPECT_EQ(menu.selected(), 1);
}

TEST(Menu, HorizontalUsesLeftRightAndAd) {
    Menu menu({{"Attack"}, {"Skill"}, {"Flee"}}, Menu::Layout::Horizontal);
    menu.handle({Key::Right});
    EXPECT_EQ(menu.selected(), 1);
    menu.handle(ch('d'));
    EXPECT_EQ(menu.selected(), 2);
    menu.handle(ch('a'));
    EXPECT_EQ(menu.selected(), 1);
    menu.handle({Key::Down}); // vertical keys do nothing here
    EXPECT_EQ(menu.selected(), 1);

    Framebuffer frame(80, 24);
    menu.draw(frame, 0, 22);
    EXPECT_EQ(rowText(frame, 22).substr(0, 25), "  Attack    ▶ Skill    ");
}

TEST(Menu, DrawsArrowColorsAndNotes) {
    Menu menu = threeWithMiddleDisabled();
    Framebuffer frame(80, 24);
    menu.draw(frame, 10, 5);
    EXPECT_EQ(rowText(frame, 5).substr(10, 7), "▶ One");
    EXPECT_EQ(frame.at(12, 5).fg, palette::kAccent);
    EXPECT_EQ(frame.at(12, 6).fg, palette::kDim); // disabled
    EXPECT_NE(rowText(frame, 6).find("Two    locked"), std::string::npos);
    EXPECT_EQ(frame.at(12, 7).fg, palette::kText);
}

TEST(TextInput, EditsWithinMaxLengthAndSubmitsOrCancels) {
    TextInput input(3);
    for (char c : std::string("abcd"))
        EXPECT_EQ(input.handle(ch(c)), InputResult::Editing);
    EXPECT_EQ(input.value(), "abc");
    input.handle({Key::Backspace});
    EXPECT_EQ(input.value(), "ab");
    input.handle({Key::Up}); // ignored
    EXPECT_EQ(input.value(), "ab");
    EXPECT_EQ(input.handle({Key::Enter}), InputResult::Submitted);
    EXPECT_EQ(input.handle({Key::Escape}), InputResult::Cancelled);
}

TEST(TextInput, CursorBlinksEvery500Ms) {
    TextInput input(8);
    input.handle(ch('x'));
    Framebuffer frame(80, 24);
    input.draw(frame, 0, 0);
    EXPECT_EQ(rowText(frame, 0).substr(0, 2), "x_");
    input.tick(0.6);
    Framebuffer later(80, 24);
    input.draw(later, 0, 0);
    EXPECT_EQ(rowText(later, 0).substr(0, 2), "x ");
}

TEST(NumberInput, OnlyInRangeSubmits) {
    NumberInput input(1, 30);
    for (char c : std::string("3a1"))
        input.handle(ch(c));
    EXPECT_EQ(input.value(), 31); // the letter was ignored
    EXPECT_EQ(input.handle({Key::Enter}), InputResult::Editing);
    Framebuffer frame(80, 24);
    input.draw(frame, 0, 0);
    EXPECT_EQ(rowText(frame, 1).substr(0, 28), "Enter a number from 1 to 30 ");
    EXPECT_EQ(frame.at(0, 1).fg, palette::kDmg);

    input.handle({Key::Backspace});
    EXPECT_EQ(input.handle({Key::Enter}), InputResult::Submitted);
    EXPECT_EQ(input.value(), 3);
}

TEST(NumberInput, EmptyIsRejectedAndDigitsAreCapped) {
    NumberInput input(1, 100);
    EXPECT_EQ(input.handle({Key::Enter}), InputResult::Editing);
    EXPECT_EQ(input.value(), std::nullopt);
    for (char c : std::string("99999"))
        input.handle(ch(c));
    EXPECT_EQ(input.value(), 999); // no more digits than the maximum has
    EXPECT_EQ(input.handle({Key::Escape}), InputResult::Cancelled);
}

TEST(WordWrap, BreaksOnSpacesAndCutsLongWords) {
    EXPECT_EQ(wordWrap("one two three", 7), (std::vector<std::string>{"one two", "three"}));
    EXPECT_EQ(wordWrap("abcdefghij", 4), (std::vector<std::string>{"abcd", "efgh", "ij"}));
    EXPECT_EQ(wordWrap("", 10), (std::vector<std::string>{""}));
}

TEST(MessageBox, CenteredWrappedAndDismissedByEnterOrEsc) {
    const std::string longText(130, 'a');
    MessageBox box("Game saved. " + longText);
    Framebuffer frame(80, 24);
    box.draw(frame);
    EXPECT_TRUE(frameContains(frame, "Game saved."));
    EXPECT_TRUE(frameContains(frame, "[Enter]"));
    for (int y = 0; y < 24; ++y) // never wider than 60 columns: columns 0-9 and 70-79 stay empty
        EXPECT_EQ(frame.at(9, y).glyph, U' ');
    EXPECT_FALSE(box.handle(ch('x')));
    EXPECT_TRUE(box.handle({Key::Enter}));
    EXPECT_TRUE(box.handle({Key::Escape}));
}

TEST(ConfirmBox, YesNoAndEscape) {
    ConfirmBox box("Quit without saving?");
    EXPECT_EQ(box.handle(ch('y')), true);
    EXPECT_EQ(box.handle(ch('N')), false);
    EXPECT_EQ(box.handle({Key::Escape}), false);
    EXPECT_EQ(box.handle({Key::Enter}), std::nullopt);
    Framebuffer frame(80, 24);
    box.draw(frame);
    EXPECT_TRUE(frameContains(frame, "[Y]es / [N]o"));
}

TEST(Typewriter, RevealsFortyCharsPerSecondThenEnterSkipsThenFinishes) {
    Typewriter writer({std::string(100, 'x')}, 60);
    writer.tick(0.5); // 20 characters
    Framebuffer frame(80, 24);
    writer.draw(frame, 0, 0, palette::kText);
    EXPECT_EQ(rowText(frame, 0).find(' '), 20u);
    EXPECT_FALSE(writer.handle({Key::Enter})); // first Enter reveals everything
    EXPECT_TRUE(writer.revealed());
    EXPECT_FALSE(writer.handle(ch('a')));
    EXPECT_TRUE(writer.handle({Key::Enter})); // second Enter finishes
}

TEST(StatusPanel, ShowsStatsWithEquipmentBarsAndGear) {
    game::GameState state = game::freshState();
    game::Hero& hero = state.hero;
    hero.hp = 250;
    hero.mp = 30;
    hero.weapon->upgradeLevel = 2;
    hero.armor.reset();
    Framebuffer frame(80, 24);
    drawStatusPanel(frame, hero, game::shippedContent());

    const std::string row1 = rowText(frame, 1);
    EXPECT_NE(row1.find("ARIA  Knight Lv 1   HP "), std::string::npos) << row1;
    EXPECT_NE(row1.find("███████████████░░░░░ 250/350"), std::string::npos) << row1;
    EXPECT_NE(row1.find("FV █████░░░ 30/50"), std::string::npos) << row1;
    const std::string row2 = rowText(frame, 2);
    // 50 base + 50 x 2 from the +2 Iron Sword; no armor.
    EXPECT_NE(row2.find("ATK 150  DEF 5  LIFE 0   EXP 0  Next 100   Gold 20000"), std::string::npos)
        << row2;
    EXPECT_NE(rowText(frame, 3).find("Weapon Sword+2   Armor —"), std::string::npos);
    EXPECT_EQ(rowText(frame, 0).substr(0, 9), "┌──");
    EXPECT_EQ(rowText(frame, 4).substr(0, 3), "└");
}

TEST(StatusPanel, LevelOneGearHasNoPlusAndHpColorsFollowThresholds) {
    game::GameState state = game::freshState();
    Framebuffer frame(80, 24);
    drawStatusPanel(frame, state.hero, game::shippedContent());
    EXPECT_NE(rowText(frame, 3).find("Weapon Sword   Armor Iron Shield"), std::string::npos);
    EXPECT_EQ(hpColor(51, 100), palette::kHpHigh);
    EXPECT_EQ(hpColor(50, 100), palette::kHpMid);
    EXPECT_EQ(hpColor(21, 100), palette::kHpMid);
    EXPECT_EQ(hpColor(20, 100), palette::kHpLow);
}
