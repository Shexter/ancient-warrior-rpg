#pragma once

#include "app/app.hpp"
#include "app/screen.hpp"
#include "game/content.hpp"
#include "game/dungeon/dungeon_def.hpp"
#include "game/world/world_def.hpp"

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace heroes::app {

// What both executables need from the platform before loading anything.
struct StartupInputs {
    std::span<const std::string_view> args; // after argv[0]
    std::string_view program;                // "heroes" or "heroes-window", for messages
    bool allowLayout = true;                 // heroes-window has no --layout
    std::optional<std::string> envDataDir, envModsDir;
    std::optional<std::filesystem::path> saveFile; // the platform save path, if one was found
    std::filesystem::path sourceDir;
};

// Everything loaded and checked, ready for a Session.
struct Startup {
    Options options;
    game::Content content;
    game::world::WorldDef world;
    game::dungeon::DungeonDef dungeon;
    SpriteBank sprites;
    LargeSprites large;
    std::filesystem::path saveFile;
};

// Lines for stderr (mod log lines on success; those plus the errors on failure), and the
// loaded game when nothing failed. On failure the caller exits with code 2 before touching the
// terminal or opening a window.
struct StartupResult {
    std::vector<std::string> messages;
    std::optional<Startup> startup;
};

// Parses the options, then loads content and mods, the world, the sprites and the large sprites,
// in that order, stopping at the first that fails.
StartupResult loadStartup(const StartupInputs& in);

} // namespace heroes::app
