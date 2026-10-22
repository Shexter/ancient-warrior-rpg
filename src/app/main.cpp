#include "app/app.hpp"
#include "app/screens/title.hpp"
#include "app/startup.hpp"
#include "game/script/script_host.hpp"
#include "platform/env.hpp"
#include "platform/paths.hpp"
#include "platform/terminal.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

int main(int argc, char** argv) {
    using namespace heroes;

    const std::vector<std::string_view> args(argv + 1, argv + argc);
    // Startup messages and errors are printed before the terminal changes, so they stay readable.
    const app::StartupResult loaded = app::loadStartup(
        {args, "heroes", true, platform::getEnv("HEROES_DATA_DIR"),
         platform::getEnv("HEROES_MODS_DIR"), platform::saveFilePath(), HEROES_SOURCE_DIR});
    for (const std::string& line : loaded.messages)
        std::cerr << line << '\n';
    if (!loaded.startup)
        return 2;
    const app::Startup& s = *loaded.startup;

    try {
        // The Terminal is destroyed (restoring the screen) before the catch block prints.
        platform::Terminal terminal;
        engine::Renderer renderer(platform::detectColorMode());
        app::Session session{s.content, s.sprites, s.saveFile, s.options.seed};
        session.world = &s.world;
        session.layout = s.options.layout;
        session.art = s.options.art;
        session.large = s.large;
        game::script::ScriptHost scripts(s.content); // content validation checked every script
        session.scripts = &scripts;
        app::App game(terminal, renderer, std::make_unique<app::TitleScreen>(session));
        return game.run();
    } catch (const std::exception& e) {
        std::cerr << "heroes: error: " << e.what() << '\n';
        return 1;
    }
}
