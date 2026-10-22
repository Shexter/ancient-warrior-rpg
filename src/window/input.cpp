#include "window/input.hpp"

#include "raylib.h"

#include <array>

namespace heroes::window {

namespace {

// Every key pollKeys watches for press, repeat and release.
constexpr std::array kWatched = {
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_ENTER, KEY_KP_ENTER, KEY_ESCAPE, KEY_BACKSPACE,
    KEY_TAB, KEY_SPACE, KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
    KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W,
    KEY_X, KEY_Y, KEY_Z, KEY_ZERO, KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE, KEY_SIX,
    KEY_SEVEN, KEY_EIGHT, KEY_NINE};

} // namespace

std::optional<engine::Key> specialKey(int key) {
    switch (key) {
    case KEY_UP:
        return engine::Key::Up;
    case KEY_DOWN:
        return engine::Key::Down;
    case KEY_LEFT:
        return engine::Key::Left;
    case KEY_RIGHT:
        return engine::Key::Right;
    case KEY_ENTER:
    case KEY_KP_ENTER:
        return engine::Key::Enter;
    case KEY_ESCAPE:
        return engine::Key::Escape;
    case KEY_BACKSPACE:
        return engine::Key::Backspace;
    case KEY_TAB:
        return engine::Key::Tab;
    default:
        return std::nullopt;
    }
}

std::optional<char> charForKey(int key, bool shift) {
    if (key >= KEY_A && key <= KEY_Z)
        return static_cast<char>((shift ? 'A' : 'a') + (key - KEY_A));
    if (key >= KEY_ZERO && key <= KEY_NINE && !shift)
        return static_cast<char>('0' + (key - KEY_ZERO));
    if (key == KEY_SPACE)
        return ' ';
    return std::nullopt;
}

bool typedByKeyEvents(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ';
}

std::vector<engine::KeyEvent> pollKeys() {
    std::vector<engine::KeyEvent> keys;
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    const auto add = [&](int key, engine::KeyAction action) {
        if (const auto special = specialKey(key))
            keys.push_back({*special, 0, 0, 0, action});
        else if (const auto ch = charForKey(key, shift))
            keys.push_back({engine::Key::Char, *ch, 0, 0, action});
    };
    for (const int key : kWatched) {
        if (IsKeyPressed(key))
            add(key, engine::KeyAction::Press);
        else if (IsKeyPressedRepeat(key))
            add(key, engine::KeyAction::Repeat);
        if (IsKeyReleased(key))
            add(key, engine::KeyAction::Release);
    }
    for (int c = GetCharPressed(); c != 0; c = GetCharPressed())
        if (c >= 0x20 && c <= 0x7E && !typedByKeyEvents(static_cast<char>(c)))
            keys.push_back({engine::Key::Char, static_cast<char>(c)});
    return keys;
}

} // namespace heroes::window
