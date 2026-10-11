#include "engine/input.hpp"

#include <gtest/gtest.h>

#include <vector>

using namespace heroes::engine;

namespace {

std::vector<KeyEvent> decode(std::string_view bytes) {
    InputDecoder d;
    d.feed(bytes);
    std::vector<KeyEvent> keys;
    while (const auto k = d.pop())
        keys.push_back(*k);
    return keys;
}

} // namespace

TEST(Input, ArrowsInBothForms) {
    EXPECT_EQ(decode("\x1b[A\x1b[B\x1b[C\x1b[D"),
              (std::vector<KeyEvent>{{Key::Up}, {Key::Down}, {Key::Right}, {Key::Left}}));
    EXPECT_EQ(decode("\x1bOA\x1bOB\x1bOC\x1bOD"),
              (std::vector<KeyEvent>{{Key::Up}, {Key::Down}, {Key::Right}, {Key::Left}}));
}

TEST(Input, LoneEscapeIsEscape) {
    EXPECT_EQ(decode("\x1b"), (std::vector<KeyEvent>{{Key::Escape}}));
}

TEST(Input, EscapeBeforeOrdinaryByteIsEscapeThenThatByte) {
    EXPECT_EQ(decode("\x1b"
                     "x"),
              (std::vector<KeyEvent>{{Key::Escape}, {Key::Char, 'x'}}));
}

TEST(Input, EnterBackspaceTabAndChars) {
    EXPECT_EQ(decode("\r"), (std::vector<KeyEvent>{{Key::Enter}}));
    EXPECT_EQ(decode("\n"), (std::vector<KeyEvent>{{Key::Enter}}));
    EXPECT_EQ(decode("\x7f"), (std::vector<KeyEvent>{{Key::Backspace}}));
    EXPECT_EQ(decode("\x08"), (std::vector<KeyEvent>{{Key::Backspace}}));
    EXPECT_EQ(decode("\t"), (std::vector<KeyEvent>{{Key::Tab}}));
    EXPECT_EQ(decode("a Z~"),
              (std::vector<KeyEvent>{
                  {Key::Char, 'a'}, {Key::Char, ' '}, {Key::Char, 'Z'}, {Key::Char, '~'}}));
}

TEST(Input, UnknownBytesAreDropped) {
    EXPECT_TRUE(decode(std::string_view("\x01\x02\x7f\x80\xff", 5)).size() ==
                1); // only 0x7f survives
    EXPECT_TRUE(decode(std::string_view("\x00\x01", 2)).empty());
    EXPECT_EQ(decode("\xc3\xa9"), std::vector<KeyEvent>{}); // UTF-8 text is not a key
}

TEST(Input, UnsupportedEscapeSequencesAreDroppedWhole) {
    EXPECT_TRUE(decode("\x1b[3~").empty());       // Delete
    EXPECT_TRUE(decode("\x1b[1;5C").size() == 1); // Ctrl+Right still reads as Right
    EXPECT_TRUE(decode("\x1b[").empty());         // truncated
    EXPECT_EQ(decode("\x1b[3~q"), (std::vector<KeyEvent>{{Key::Char, 'q'}}));
}

TEST(Input, PopReturnsKeysInOrderAcrossFeeds) {
    InputDecoder d;
    d.feed("ab");
    d.feed("\x1b[A");
    EXPECT_EQ(d.pop(), (KeyEvent{Key::Char, 'a'}));
    EXPECT_EQ(d.pop(), (KeyEvent{Key::Char, 'b'}));
    EXPECT_EQ(d.pop(), (KeyEvent{Key::Up}));
    EXPECT_FALSE(d.pop().has_value());
}
