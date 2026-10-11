#include "platform/paths.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

using heroes::platform::saveFilePath;
namespace fs = std::filesystem;

namespace {

void setEnv(const char* name, const std::optional<std::string>& value) {
#ifdef _WIN32
    _putenv_s(name, value ? value->c_str() : ""); // empty removes the variable
#else
    if (value)
        setenv(name, value->c_str(), 1);
    else
        unsetenv(name);
#endif
}

std::optional<std::string> getEnv(const char* name) {
#ifdef _WIN32
    char* raw = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&raw, &size, name) != 0 || raw == nullptr)
        return std::nullopt;
    std::string value(raw);
    std::free(raw);
    return value;
#else
    const char* raw = std::getenv(name);
    return raw ? std::optional<std::string>(raw) : std::nullopt;
#endif
}

// Restores every variable it touched when the test ends.
class EnvGuard {
  public:
    EnvGuard() {
        for (const char* name : kNames)
            saved_.push_back(getEnv(name));
    }
    ~EnvGuard() {
        for (std::size_t i = 0; i < saved_.size(); ++i)
            setEnv(kNames[i], saved_[i]);
    }
    EnvGuard(const EnvGuard&) = delete;
    EnvGuard& operator=(const EnvGuard&) = delete;

  private:
    static constexpr const char* kNames[] = {"HEROES_SAVE_DIR", "HOME", "XDG_DATA_HOME", "APPDATA"};
    std::vector<std::optional<std::string>> saved_;
};

} // namespace

TEST(Paths, SaveDirOverrideWins) {
    EnvGuard guard;
    setEnv("HEROES_SAVE_DIR", "/somewhere/else");
    EXPECT_EQ(saveFilePath(), fs::path("/somewhere/else") / "save.json");
}

TEST(Paths, EmptyOverrideIsIgnored) {
    EnvGuard guard;
    setEnv("HEROES_SAVE_DIR", std::string());
    const auto path = saveFilePath();
    if (path) {
        EXPECT_NE(path->parent_path().filename(), "");
    }
    EXPECT_NE(path, fs::path("save.json"));
}

#if defined(_WIN32)
TEST(Paths, WindowsUsesAppData) {
    EnvGuard guard;
    setEnv("HEROES_SAVE_DIR", std::nullopt);
    setEnv("APPDATA", "C:\\Users\\x\\AppData\\Roaming");
    EXPECT_EQ(saveFilePath(),
              fs::path("C:\\Users\\x\\AppData\\Roaming") / "heroes-rpg" / "save.json");
    setEnv("APPDATA", std::nullopt);
    EXPECT_EQ(saveFilePath(), std::nullopt);
}
#elif defined(__APPLE__)
TEST(Paths, MacUsesApplicationSupport) {
    EnvGuard guard;
    setEnv("HEROES_SAVE_DIR", std::nullopt);
    setEnv("HOME", "/Users/x");
    EXPECT_EQ(saveFilePath(),
              fs::path("/Users/x/Library/Application Support") / "heroes-rpg" / "save.json");
    setEnv("HOME", std::nullopt);
    EXPECT_EQ(saveFilePath(), std::nullopt);
}
#else
TEST(Paths, LinuxPrefersXdgThenHome) {
    EnvGuard guard;
    setEnv("HEROES_SAVE_DIR", std::nullopt);
    setEnv("XDG_DATA_HOME", "/xdg");
    setEnv("HOME", "/home/x");
    EXPECT_EQ(saveFilePath(), fs::path("/xdg") / "heroes-rpg" / "save.json");
    setEnv("XDG_DATA_HOME", std::nullopt);
    EXPECT_EQ(saveFilePath(), fs::path("/home/x/.local/share") / "heroes-rpg" / "save.json");
    setEnv("HOME", std::nullopt);
    EXPECT_EQ(saveFilePath(), std::nullopt);
}
#endif
