#pragma once

#include "engine/framebuffer.hpp"
#include "engine/input.hpp"
#include "engine/loop.hpp"
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

enum class Layout { Auto, Compact }; // --layout: auto uses large layouts when they fit
enum class ArtStyle { Glyph, Blocks }; // --art: which large sprites the large layouts draw

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
    Layout layout = Layout::Auto;
    ArtStyle art = ArtStyle::Glyph;
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
    // Draws the full canvas, sized by canvasSize().
    virtual void draw(engine::Framebuffer& frame) const = 0;
    // The canvas this screen wants for a terminal of the given size. Screens with a large
    // layout override it; the loop falls back to 80x24 if the answer can't fit.
    virtual engine::Size canvasSize(engine::Size /*terminal*/) const {
        return engine::kCompactCanvas;
    }
};

} // namespace heroes::app
