#pragma once

#include "engine/framebuffer.hpp"
#include "engine/input.hpp"
#include "engine/sprite.hpp"
#include "game/content.hpp"
#include "game/game_state.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <variant>

namespace heroes::app {

// Sprites by file name ("hero_fighter.sprite", "medusa.sprite", ...), loaded at startup.
using SpriteBank = std::map<std::string, engine::Sprite, std::less<>>;

// Shared by every screen. Only screens change `state`, and only through game:: functions.
struct Session {
    const game::Content& content;
    const SpriteBank& sprites;
    std::filesystem::path saveFile;
    std::optional<std::uint64_t> seed; // --seed for new games; random when unset
    std::optional<game::GameState> state{};
    bool dirty = false;        // changed since the last successful save
    bool saveOwned = false;    // this session loaded or wrote saveFile, so saving needs no confirm
    std::string lastMessage{}; // shown on the Hub's bottom row
};

class Screen;
struct Stay {};
struct Pop {};
struct Quit {};
struct Push {
    std::unique_ptr<Screen> screen;
};
struct Replace { // pop the current screen, push this one
    std::unique_ptr<Screen> screen;
};
using Transition = std::variant<Stay, Push, Pop, Replace, Quit>;

class Screen {
  public:
    virtual ~Screen() = default;
    // Called once per fixed step with the keys that arrived since the previous step.
    virtual Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) = 0;
    // Draws the full 80x24 canvas.
    virtual void draw(engine::Framebuffer& frame) const = 0;
};

} // namespace heroes::app
