#pragma once

#include "app/screen.hpp"
#include "engine/loop.hpp"
#include "engine/renderer.hpp"
#include "game/save.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace heroes::app {

struct Options {
    std::optional<std::uint64_t> seed;
    std::optional<std::filesystem::path> dataDir;
};

// `heroes [--seed N] [--data-dir PATH]` (arguments after argv[0]).
std::expected<Options, std::string> parseOptions(std::span<const std::string_view> args);

// --data-dir, then the HEROES_DATA_DIR value, then `<sourceDir>/data`.
std::filesystem::path resolveDataDir(const Options& options,
                                     const std::optional<std::string>& envDataDir,
                                     const std::filesystem::path& sourceDir);

// Player-facing sentence for a save or load failure.
std::string_view describe(game::SaveError error);

// Loads every sprite the screens draw: hero_<path>.sprite for each path, each enemy's sprite,
// and monster.sprite (training). The error names the file and the problem.
std::expected<SpriteBank, std::string> loadSprites(const game::Content& content,
                                                   const std::filesystem::path& spriteDir);

// Runs the screen stack on the fixed-step loop until a screen quits or the stack empties.
class App {
  public:
    App(engine::LoopIo& io, engine::Renderer& renderer, std::unique_ptr<Screen> first);

    int run(); // returns the exit code

    // One step for the top screen, applying its transition. False once the app should exit.
    bool update(std::span<const engine::KeyEvent> keys, double dtSeconds);
    void draw(engine::Framebuffer& frame) const;
    std::size_t depth() const { return stack_.size(); }

  private:
    engine::LoopIo& io_;
    engine::Renderer& renderer_;
    std::vector<std::unique_ptr<Screen>> stack_;
};

} // namespace heroes::app
