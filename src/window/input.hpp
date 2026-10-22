#pragma once

#include "engine/input.hpp"

#include <optional>
#include <vector>

namespace heroes::window {

// The game key for a raylib key that isn't a character: arrows, Enter, Esc, Backspace, Tab.
std::optional<engine::Key> specialKey(int raylibKey);
// The character a letter, digit or space key types (letters upper-cased with Shift). Digits
// with Shift type symbols, which come through the character queue instead, so they are nullopt.
std::optional<char> charForKey(int raylibKey, bool shift);
// True for a character charForKey can produce, which pollKeys therefore takes from key events
// instead of raylib's character queue (that queue can't say press from repeat or release).
bool typedByKeyEvents(char c);

// This frame's key events from raylib: special and letter/digit/space keys with press, repeat
// and release; other printable characters from the character queue as presses.
std::vector<engine::KeyEvent> pollKeys();

} // namespace heroes::window
