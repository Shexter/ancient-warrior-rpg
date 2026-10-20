#include "app/held_keys.hpp"

#include <algorithm>

namespace heroes::app {

namespace {

using game::world::Dir;

std::optional<Dir> directionOf(const engine::KeyEvent& key) {
    switch (key.key) {
    case engine::Key::Up:
        return Dir::Up;
    case engine::Key::Down:
        return Dir::Down;
    case engine::Key::Left:
        return Dir::Left;
    case engine::Key::Right:
        return Dir::Right;
    case engine::Key::Char:
        switch (key.ch) {
        case 'w':
        case 'W':
            return Dir::Up;
        case 's':
        case 'S':
            return Dir::Down;
        case 'a':
        case 'A':
            return Dir::Left;
        case 'd':
        case 'D':
            return Dir::Right;
        default:
            return std::nullopt;
        }
    default:
        return std::nullopt;
    }
}

} // namespace

void HeldKeys::update(std::span<const engine::KeyEvent> keys) {
    tick_.reset();
    for (const engine::KeyEvent& key : keys) {
        const auto dir = directionOf(key);
        if (!dir)
            continue;
        const auto it = std::ranges::find(held_, *dir);
        if (key.action == engine::KeyAction::Release) {
            releases_ = true;
            if (it != held_.end())
                held_.erase(it);
            continue;
        }
        tick_ = *dir;
        if (key.action == engine::KeyAction::Press && it != held_.end())
            held_.erase(it); // pressed again: it becomes the most recent
        else if (it != held_.end())
            continue; // a repeat of a direction already held keeps its place
        held_.push_back(*dir);
    }
}

std::optional<game::world::Dir> HeldKeys::direction() const {
    if (releases_ && !held_.empty())
        return held_.back();
    return tick_; // also a tap pressed and released within one tick
}

} // namespace heroes::app
