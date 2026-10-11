#include "platform/paths.hpp"

#include <cstdlib>
#include <string>

namespace heroes::platform {

namespace {

namespace fs = std::filesystem;

// Unset and empty both read as "not set".
std::optional<std::string> env(const char* name) {
#ifdef _WIN32
    char* raw = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&raw, &size, name) != 0 || raw == nullptr)
        return std::nullopt;
    std::string value(raw);
    std::free(raw);
#else
    const char* raw = std::getenv(name);
    if (raw == nullptr)
        return std::nullopt;
    std::string value(raw);
#endif
    if (value.empty())
        return std::nullopt;
    return value;
}

std::optional<fs::path> dataDirectory() {
#if defined(_WIN32)
    if (auto appData = env("APPDATA"))
        return fs::path(*appData) / "heroes-rpg";
#elif defined(__APPLE__)
    if (auto home = env("HOME"))
        return fs::path(*home) / "Library" / "Application Support" / "heroes-rpg";
#else
    if (auto xdg = env("XDG_DATA_HOME"))
        return fs::path(*xdg) / "heroes-rpg";
    if (auto home = env("HOME"))
        return fs::path(*home) / ".local" / "share" / "heroes-rpg";
#endif
    return std::nullopt;
}

} // namespace

std::optional<fs::path> saveFilePath() {
    if (auto overrideDir = env("HEROES_SAVE_DIR"))
        return fs::path(*overrideDir) / "save.json";
    if (auto dir = dataDirectory())
        return *dir / "save.json";
    return std::nullopt;
}

} // namespace heroes::platform
