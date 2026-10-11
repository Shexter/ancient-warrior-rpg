#include "platform/paths.hpp"

#include "platform/env.hpp"

namespace heroes::platform {

namespace {

namespace fs = std::filesystem;

std::optional<fs::path> dataDirectory() {
#if defined(_WIN32)
    if (auto appData = getEnv("APPDATA"))
        return fs::path(*appData) / "heroes-rpg";
#elif defined(__APPLE__)
    if (auto home = getEnv("HOME"))
        return fs::path(*home) / "Library" / "Application Support" / "heroes-rpg";
#else
    if (auto xdg = getEnv("XDG_DATA_HOME"))
        return fs::path(*xdg) / "heroes-rpg";
    if (auto home = getEnv("HOME"))
        return fs::path(*home) / ".local" / "share" / "heroes-rpg";
#endif
    return std::nullopt;
}

} // namespace

std::optional<fs::path> saveFilePath() {
    if (auto overrideDir = getEnv("HEROES_SAVE_DIR"))
        return fs::path(*overrideDir) / "save.json";
    if (auto dir = dataDirectory())
        return *dir / "save.json";
    return std::nullopt;
}

} // namespace heroes::platform
