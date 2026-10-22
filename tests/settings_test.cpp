#include "app/settings.hpp"
#include "game/save.hpp"
#include "state_helpers.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace heroes;
using app::Settings;
namespace fs = std::filesystem;

namespace {

fs::path scratch(const std::string& name) {
    const fs::path dir = fs::temp_directory_path() / "heroes_settings_test" / name;
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir;
}

std::string slurp(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

} // namespace

TEST(Settings, AMissingFileMeansConsentNotShownAndOff) {
    const Settings s = app::loadSettings(scratch("missing") / "settings.json");
    EXPECT_FALSE(s.consentShown);
    EXPECT_EQ(s.mode, Settings::Mode::Off);
}

TEST(Settings, RoundTripThroughTheFile) {
    const fs::path file = scratch("round") / "settings.json";
    Settings s;
    s.consentShown = true;
    s.mode = Settings::Mode::Local;
    s.provider = "openai";
    s.model = "llama3.2";
    s.baseUrl = "http://localhost:11434/v1";
    s.apiKey = "sk-test-123456789";
    s.installId = app::newInstallId();
    ASSERT_TRUE(app::saveSettings(s, file));
    EXPECT_EQ(app::loadSettings(file), s);
#ifndef _WIN32
    EXPECT_EQ(fs::status(file).permissions() & fs::perms::all, fs::perms::owner_read | fs::perms::owner_write);
#endif
}

TEST(Settings, ADamagedFileFallsBackToDefaults) {
    const fs::path file = scratch("damaged") / "settings.json";
    std::ofstream(file) << "{not json";
    EXPECT_EQ(app::loadSettings(file), Settings{});
}

TEST(Settings, KeysAreMasked) {
    EXPECT_EQ(app::mask("short"), "••••");
    EXPECT_EQ(app::mask("1234567"), "••••");
    EXPECT_EQ(app::mask("sk-ant-api03-abcdWXYZ"), "••••WXYZ");
}

TEST(Settings, TheEnvVarWinsAndIsNeverWritten) {
    const fs::path file = scratch("env") / "settings.json";
    Settings s;
    s.provider = "gemini";
    s.apiKey = "from-the-file-0000";
    const auto env = [](const char* name) -> std::optional<std::string> {
        if (std::string_view(name) == "GEMINI_API_KEY")
            return "from-the-env-9999";
        return std::nullopt;
    };
    EXPECT_EQ(app::resolveKey(s, env), "from-the-env-9999");
    EXPECT_EQ(app::resolveKey(s, [](const char*) { return std::optional<std::string>(); }), "from-the-file-0000");
    ASSERT_TRUE(app::saveSettings(s, file));
    EXPECT_EQ(slurp(file).find("from-the-env"), std::string::npos);
}

TEST(Settings, TheSaveNeverHoldsTheKey) {
    const fs::path dir = scratch("save");
    Settings s;
    s.apiKey = "sk-secret-abcdef123456";
    ASSERT_TRUE(app::saveSettings(s, dir / "settings.json"));
    ASSERT_TRUE(game::saveGame(game::freshState(), dir / "save.json").has_value());
    EXPECT_EQ(slurp(dir / "save.json").find("sk-secret"), std::string::npos);
}

TEST(Settings, PlainHttpOnlyForThisMachine) {
    EXPECT_TRUE(app::validBaseUrl("http://localhost:11434/v1"));
    EXPECT_TRUE(app::validBaseUrl("http://127.0.0.1:8080/v1"));
    EXPECT_TRUE(app::validBaseUrl("http://[::1]:11434/v1"));
    EXPECT_TRUE(app::validBaseUrl("https://api.openai.com/v1"));
    EXPECT_FALSE(app::validBaseUrl("http://192.168.1.20:11434/v1"));
    EXPECT_FALSE(app::validBaseUrl("http://localhost:11434")); // no path
    EXPECT_FALSE(app::validBaseUrl("ftp://localhost/v1"));
    EXPECT_FALSE(app::validBaseUrl("http://localhost:port/v1"));
}

TEST(Settings, InstallIdsAre32HexCharacters) {
    const std::string id = app::newInstallId();
    EXPECT_EQ(id.size(), 32u);
    EXPECT_EQ(id.find_first_not_of("0123456789abcdef"), std::string::npos);
    EXPECT_NE(id, app::newInstallId());
}
