#pragma once

#include "engine/framebuffer.hpp"
#include "engine/input.hpp"
#include "engine/loop.hpp"
#include "engine/sprite.hpp"
#include "game/content.hpp"
#include "game/dungeon/dungeon_def.hpp"
#include "game/narration/templates.hpp"
#include "game/game_state.hpp"
#include "game/script/script_host.hpp"
#include "game/world/world_def.hpp"

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

// Optional large art for the large layouts, keyed like SpriteBank by the compact sprite's file
// name. Either look may be missing for any sprite.
struct LargeSprites {
    std::map<std::string, engine::Sprite, std::less<>> blocks;      // 48x48-class half-blocks
    std::map<std::string, engine::GlyphSprite, std::less<>> glyphs; // the same art as glyphs
};

// --layout: auto picks large layouts when they fit and the phone layout when the terminal is
// narrower than 80; compact always uses 80x24; portrait always uses the phone layout.
enum class Layout { Auto, Compact, Portrait };
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
    LargeSprites large{};
    game::script::ScriptHost* scripts = nullptr; // the game thread's script sandbox
    const game::world::WorldDef* world = nullptr; // the Overworld map, loaded at startup
    const game::dungeon::DungeonDef* dungeon = nullptr; // the expedition's rules, loaded at startup
    const game::narration::NarratorDef* narrator = nullptr; // narration lines; none shows no narration
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
    // Whether this screen gets key Repeat and Release events as they arrive. Other screens get
    // a Repeat as an ordinary press and never see a Release, as on a plain terminal.
    virtual bool wantsKeyActions() const { return false; }
};

} // namespace heroes::app
