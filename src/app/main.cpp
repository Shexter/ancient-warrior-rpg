#include "app/app.hpp"
#include "app/screens/title.hpp"
#include "game/content_loader.hpp"
#include "game/script/script_host.hpp"
#include "platform/env.hpp"
#include "platform/paths.hpp"
#include "platform/terminal.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

int main(int argc, char** argv) {
    using namespace heroes;

    const std::vector<std::string_view> args(argv + 1, argv + argc);
    const auto options = app::parseOptions(args);
    if (!options) {
        std::cerr << "heroes: error: " << options.error()
                  << "\nusage: heroes [--seed N] [--data-dir PATH] [--layout auto|compact|portrait] "
                     "[--art glyph|blocks]\n";
        return 2;
    }

    // Content errors are reported before the terminal changes, so they stay readable.
    const auto dataDir =
        app::resolveDataDir(*options, platform::getEnv("HEROES_DATA_DIR"), HEROES_SOURCE_DIR);
    // Mods: HEROES_MODS_DIR, else mods/ beside the data folder. Their log lines (skips,
    // overrides, loaded) go to stderr before the terminal changes.
    const auto modsEnv = platform::getEnv("HEROES_MODS_DIR");
    const std::filesystem::path modsDir =
        modsEnv ? std::filesystem::path(*modsEnv) : dataDir / ".." / "mods";
    std::vector<std::string> modLog;
    auto content = game::loadContent(dataDir, modsDir, &modLog);
    for (const std::string& line : modLog)
        std::cerr << line << '\n';
    if (!content) {
        for (const game::ContentError& error : content.error())
            std::cerr << game::formatError(error) << '\n';
        return 2;
    }
    const auto sprites = app::loadSprites(*content, dataDir / ".." / "assets" / "sprites");
    if (!sprites) {
        std::cerr << sprites.error() << '\n';
        return 2;
    }
    auto large = app::loadLargeSprites(*content, dataDir / ".." / "assets" / "sprites" / "large");
    if (!large) {
        std::cerr << large.error() << '\n';
        return 2;
    }
    const auto saveFile = platform::saveFilePath();
    if (!saveFile) {
        std::cerr << "heroes: error: cannot find a place to save; set HEROES_SAVE_DIR\n";
        return 2;
    }

    try {
        // The Terminal is destroyed (restoring the screen) before the catch block prints.
        platform::Terminal terminal;
        engine::Renderer renderer(platform::detectColorMode());
        app::Session session{*content, *sprites, *saveFile, options->seed};
        session.layout = options->layout;
        session.art = options->art;
        session.large = std::move(*large);
        game::script::ScriptHost scripts(*content); // content validation checked every script
        session.scripts = &scripts;
        app::App game(terminal, renderer, std::make_unique<app::TitleScreen>(session));
        return game.run();
    } catch (const std::exception& e) {
        std::cerr << "heroes: error: " << e.what() << '\n';
        return 1;
    }
}
