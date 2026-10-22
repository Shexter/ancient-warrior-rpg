#include "app/startup.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

using namespace heroes::app;
namespace fs = std::filesystem;

namespace {

StartupResult start(std::vector<std::string_view> args, bool allowLayout = true,
                    std::optional<fs::path> save = fs::path("/tmp/save.json")) {
    return loadStartup({args, allowLayout ? "heroes" : "heroes-window", allowLayout, std::nullopt,
                        (fs::path(HEROES_SOURCE_DIR) / "no_mods").string(), save,
                        HEROES_SOURCE_DIR});
}

std::string joined(const StartupResult& r) {
    std::string all;
    for (const auto& line : r.messages)
        all += line + "\n";
    return all;
}

} // namespace

TEST(Startup, LoadsTheShippedGame) {
    const StartupResult r = start({"--seed", "7", "--art", "blocks"});
    ASSERT_TRUE(r.startup.has_value()) << joined(r);
    EXPECT_EQ(r.startup->options.seed, 7u);
    EXPECT_EQ(r.startup->options.art, ArtStyle::Blocks);
    EXPECT_FALSE(r.startup->world.landmarks.empty());
    EXPECT_TRUE(r.startup->sprites.contains("hero_fighter.sprite"));
    EXPECT_EQ(r.startup->saveFile, fs::path("/tmp/save.json"));
}

TEST(Startup, ABadOptionPrintsTheErrorAndUsage) {
    const StartupResult r = start({"--layout", "big"});
    EXPECT_FALSE(r.startup.has_value());
    EXPECT_EQ(joined(r), "heroes: error: --layout expects auto, compact or portrait, got 'big'\n"
                         "usage: heroes [--seed N] [--data-dir PATH] [--layout "
                         "auto|compact|portrait] [--art glyph|blocks]\n");
}

TEST(Startup, TheWindowHasNoLayoutOption) {
    const StartupResult r = start({"--layout", "compact"}, false);
    EXPECT_FALSE(r.startup.has_value());
    EXPECT_EQ(joined(r), "heroes-window: error: unknown option '--layout'\n"
                         "usage: heroes-window [--seed N] [--data-dir PATH] [--art glyph|blocks]\n");
}

TEST(Startup, BadContentIsReportedByFile) {
    const StartupResult r = start(
        {"--data-dir",
         (fs::path(HEROES_SOURCE_DIR) / "tests/fixtures/content_bad/negative_price").string()});
    EXPECT_FALSE(r.startup.has_value());
    EXPECT_NE(joined(r).find("items.json: item 'iron_sword': key 'price' must be between"),
              std::string::npos)
        << joined(r);
}

TEST(Startup, NoSaveLocationIsAnError) {
    const StartupResult r = start({}, true, std::nullopt);
    EXPECT_FALSE(r.startup.has_value());
    EXPECT_EQ(joined(r), "heroes: error: cannot find a place to save; set HEROES_SAVE_DIR\n");
}
