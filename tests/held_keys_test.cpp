#include "app/held_keys.hpp"

#include <gtest/gtest.h>

#include <vector>

using namespace heroes;
using engine::Key;
using engine::KeyAction;
using engine::KeyEvent;
using game::world::Dir;

namespace {

KeyEvent press(char c) { return {Key::Char, c}; }
KeyEvent repeat(char c) { return {Key::Char, c, 0, 0, KeyAction::Repeat}; }
KeyEvent release(char c) { return {Key::Char, c, 0, 0, KeyAction::Release}; }

std::optional<Dir> after(app::HeldKeys& held, std::vector<KeyEvent> keys) {
    held.update(keys);
    return held.direction();
}

} // namespace

TEST(HeldKeys, FallbackCountsOnlyThisTicksPresses) {
    app::HeldKeys held;
    EXPECT_EQ(after(held, {press('d')}), Dir::Right);
    EXPECT_EQ(after(held, {}), std::nullopt); // no release reporting seen: not held
    EXPECT_EQ(after(held, {press('d')}), Dir::Right); // an OS repeat arrives as a press
    EXPECT_EQ(after(held, {{Key::Up}}), Dir::Up);
}

TEST(HeldKeys, WithReleasesADirectionIsHeldUntilReleased) {
    app::HeldKeys held;
    after(held, {press('a'), release('a')}); // the first release turns tracking on
    EXPECT_EQ(after(held, {press('d')}), Dir::Right);
    EXPECT_EQ(after(held, {}), Dir::Right);
    EXPECT_EQ(after(held, {repeat('d')}), Dir::Right);
    EXPECT_EQ(after(held, {release('d')}), std::nullopt);
}

TEST(HeldKeys, MostRecentlyPressedHeldDirectionWins) {
    app::HeldKeys held;
    after(held, {press('a'), release('a')});
    EXPECT_EQ(after(held, {press('w')}), Dir::Up);
    EXPECT_EQ(after(held, {press('d')}), Dir::Right);
    EXPECT_EQ(after(held, {repeat('w')}), Dir::Right); // a repeat keeps W's place
    EXPECT_EQ(after(held, {release('d')}), Dir::Up);
    EXPECT_EQ(after(held, {release('W')}), std::nullopt); // case does not matter
}

TEST(HeldKeys, ATapWithinOneTickStillMoves) {
    app::HeldKeys held;
    after(held, {press('a'), release('a')});
    EXPECT_EQ(after(held, {press('s'), release('s')}), Dir::Down);
    EXPECT_EQ(after(held, {}), std::nullopt);
}

TEST(HeldKeys, OtherKeysAreIgnored) {
    app::HeldKeys held;
    EXPECT_EQ(after(held, {press('q'), {Key::Enter}}), std::nullopt);
}
