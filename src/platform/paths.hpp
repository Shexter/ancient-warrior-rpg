#pragma once

#include <filesystem>
#include <optional>

namespace heroes::platform {

// `$HEROES_SAVE_DIR/save.json` if set, else the per-user data directory:
//   macOS   ~/Library/Application Support/heroes-rpg/save.json
//   Linux   $XDG_DATA_HOME/heroes-rpg/save.json, else ~/.local/share/heroes-rpg/save.json
//   Windows %APPDATA%\heroes-rpg\save.json
// Returns nullopt if the needed environment variables are missing. Does not create anything.
std::optional<std::filesystem::path> saveFilePath();

} // namespace heroes::platform
