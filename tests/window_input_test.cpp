#include "window/input.hpp"

#include "raylib.h"

#include <gtest/gtest.h>

using namespace heroes;
using namespace heroes::window;

TEST(WindowInput, SpecialKeysMapToGameKeys) {
    EXPECT_EQ(specialKey(KEY_UP), engine::Key::Up);
    EXPECT_EQ(specialKey(KEY_DOWN), engine::Key::Down);
    EXPECT_EQ(specialKey(KEY_LEFT), engine::Key::Left);
    EXPECT_EQ(specialKey(KEY_RIGHT), engine::Key::Right);
    EXPECT_EQ(specialKey(KEY_ENTER), engine::Key::Enter);
    EXPECT_EQ(specialKey(KEY_KP_ENTER), engine::Key::Enter);
    EXPECT_EQ(specialKey(KEY_ESCAPE), engine::Key::Escape);
    EXPECT_EQ(specialKey(KEY_BACKSPACE), engine::Key::Backspace);
    EXPECT_EQ(specialKey(KEY_TAB), engine::Key::Tab);
    EXPECT_EQ(specialKey(KEY_A), std::nullopt);
    EXPECT_EQ(specialKey(KEY_F1), std::nullopt);
}

TEST(WindowInput, LetterDigitAndSpaceKeysType) {
    EXPECT_EQ(charForKey(KEY_W, false), 'w');
    EXPECT_EQ(charForKey(KEY_W, true), 'W');
    EXPECT_EQ(charForKey(KEY_ONE, false), '1');
    EXPECT_EQ(charForKey(KEY_ONE, true), std::nullopt); // '!' comes from the character queue
    EXPECT_EQ(charForKey(KEY_SPACE, false), ' ');
    EXPECT_EQ(charForKey(KEY_MINUS, false), std::nullopt);
}

TEST(WindowInput, TheCharacterQueueSkipsWhatKeyEventsAlreadyType) {
    for (const char c : std::string_view("azAZ09 "))
        EXPECT_TRUE(typedByKeyEvents(c)) << c;
    for (const char c : std::string_view("!-.,?'"))
        EXPECT_FALSE(typedByKeyEvents(c)) << c;
}
