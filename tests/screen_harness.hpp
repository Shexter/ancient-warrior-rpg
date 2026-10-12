#pragma once

// Drives real screens through App with key presses, against a throwaway save directory.

#include "app/app.hpp"
#include "app/screens/hub.hpp"
#include "app/screens/title.hpp"
#include "engine/framebuffer.hpp"

#include "shipped_content.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

namespace screen_test {

using namespace heroes;
using namespace heroes::app;
namespace fs = std::filesystem;
using engine::Framebuffer;
using engine::Key;
using engine::KeyEvent;

inline const SpriteBank& shippedSprites() {
    static const SpriteBank bank =
        loadSprites(game::shippedContent(), fs::path(HEROES_SOURCE_DIR) / "assets" / "sprites")
            .value();
    return bank;
}

inline const LargeSprites& shippedLargeSprites() {
    static const LargeSprites large =
        loadLargeSprites(game::shippedContent(),
                         fs::path(HEROES_SOURCE_DIR) / "assets" / "sprites" / "large")
            .value();
    return large;
}

class NullIo : public engine::LoopIo {
  public:
    std::pair<int, int> size() const override { return {80, 24}; }
    std::vector<KeyEvent> readKeys() override { return {}; }
    void write(std::string_view) override {}
};

inline std::string readFile(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// A session with its own empty save directory, and an App to drive with keys.
struct Harness {
    explicit Harness(const std::string& name) {
        dir = fs::temp_directory_path() / "heroes_screens_test" / name;
        fs::remove_all(dir);
        fs::create_directories(dir);
        session.saveFile = dir / "save.json";
        session.seed = 7;
    }

    void startAtTitle() { app.emplace(io, renderer, std::make_unique<TitleScreen>(session)); }
    void startAtHub(game::GameState state) {
        session.state = std::move(state);
        app.emplace(io, renderer, std::make_unique<HubScreen>(session));
    }

    // Feeds each key in its own step. Returns false if the app ended.
    bool press(std::initializer_list<KeyEvent> keys) {
        for (const KeyEvent& key : keys)
            if (!app->update(std::span(&key, 1), 1.0 / 60))
                return false;
        return true;
    }
    bool type(std::string_view text) {
        for (char c : text)
            if (!press({{Key::Char, c}}))
                return false;
        return true;
    }
    // Lets `seconds` of game time pass without input, one fixed step at a time.
    bool wait(double seconds) {
        for (int i = 0; i < static_cast<int>(seconds * 60 + 0.5); ++i)
            if (!app->update({}, 1.0 / 60))
                return false;
        return true;
    }
    Framebuffer frame() const { return frame(engine::kCompactCanvas); }
    // The top screen drawn into a canvas of `size` (what the loop does after canvasSize()).
    Framebuffer frame(engine::Size size) const {
        Framebuffer f(size.width, size.height);
        app->draw(f);
        return f;
    }

    fs::path dir;
    Session session{game::shippedContent(), shippedSprites(), {}, std::nullopt};
    NullIo io;
    engine::Renderer renderer{engine::ColorMode::TrueColor};
    std::optional<App> app;
};

const KeyEvent kEnter{Key::Enter};
const KeyEvent kEsc{Key::Escape};
inline KeyEvent ch(char c) { return {Key::Char, c}; }

inline game::GameState freshGame() {
    return game::newGame(game::shippedContent(), "Aria", "fighter", 7);
}

} // namespace screen_test
