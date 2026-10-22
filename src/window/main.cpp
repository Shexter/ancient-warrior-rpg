#include "app/screens/narrator_setup.hpp"
#include "app/screens/title.hpp"
#include "app/startup.hpp"
#include "game/script/script_host.hpp"
#include "net/http.hpp"
#include "platform/env.hpp"
#include "platform/paths.hpp"
#include "window/models.hpp"
#include "window/run.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

int main(int argc, char** argv) {
    using namespace heroes;

    const std::vector<std::string_view> args(argv + 1, argv + argc);
    const app::StartupResult loaded = app::loadStartup(
        {args, "heroes-window", false, platform::getEnv("HEROES_DATA_DIR"),
         platform::getEnv("HEROES_MODS_DIR"), platform::saveFilePath(), HEROES_SOURCE_DIR});
    for (const std::string& line : loaded.messages)
        std::cerr << line << '\n';
    if (!loaded.startup)
        return 2;
    const app::Startup& s = *loaded.startup;
    // The font sits beside the sprites, under assets/ next to the data folder.
    const auto dataDir = app::resolveDataDir(s.options, platform::getEnv("HEROES_DATA_DIR"),
                                             HEROES_SOURCE_DIR);
    const auto fontFile = dataDir / ".." / "assets" / "fonts" / "JetBrainsMono-Regular.ttf";
    if (std::error_code ec; !std::filesystem::is_regular_file(fontFile, ec)) {
        std::cerr << "heroes-window: error: font not found: " << fontFile.string() << '\n';
        return 2;
    }
    std::vector<std::string> pathIds;
    for (const game::PathDef& path : s.content.paths())
        pathIds.push_back(path.id);
    auto models = window::loadModelFiles(dataDir / ".." / "assets" / "models", pathIds);
    if (!models) {
        std::cerr << "heroes-window: error: " << models.error() << '\n';
        return 2;
    }

    try {
        app::Session session{s.content, s.sprites, s.saveFile, s.options.seed};
        session.world = &s.world;
        session.dungeon = &s.dungeon;
        session.narrator = &s.narrator;
        // The narrator's settings sit beside the save; nothing is sent before the player consents.
        session.settingsFile = s.saveFile.parent_path() / "settings.json";
        session.settings = app::loadSettings(session.settingsFile);
        session.env = platform::getEnv;
        session.transport = net::curlTransport();
        if (session.settings.consentShown)
            session.liveNarrator = app::makeNarrator(session.settings, s.narrator, session.env, session.transport);
        session.art = s.options.art;
        session.large = s.large;
        game::script::ScriptHost scripts(s.content);
        session.scripts = &scripts;
        return window::run(session, app::firstScreen(session), fontFile,
                           std::move(*models));
    } catch (const std::exception& e) {
        std::cerr << "heroes-window: error: " << e.what() << '\n';
        return 1;
    }
}
