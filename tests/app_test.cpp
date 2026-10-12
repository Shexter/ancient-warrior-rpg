#include "app/app.hpp"
#include "app/screens/title.hpp"

#include "frame_text.hpp"
#include "shipped_content.hpp"

#include <gtest/gtest.h>

#include <fstream>
#include <utility>

using namespace heroes;
using namespace heroes::app;
using engine::Framebuffer;
using engine::Key;
using engine::KeyEvent;
namespace fs = std::filesystem;

namespace {

const SpriteBank& shippedSprites() {
    static const SpriteBank bank =
        loadSprites(game::shippedContent(), fs::path(HEROES_SOURCE_DIR) / "assets" / "sprites")
            .value();
    return bank;
}

class NullIo : public engine::LoopIo {
  public:
    std::pair<int, int> size() const override { return {80, 24}; }
    std::vector<KeyEvent> readKeys() override { return {}; }
    void write(std::string_view) override {}
};

// Returns whatever transition it was told to, once; then Stay.
class ScriptedScreen final : public Screen {
  public:
    explicit ScriptedScreen(char name, Transition next = Stay{})
        : name_(name), next_(std::move(next)) {}
    Transition update(std::span<const KeyEvent>, double) override {
        return std::exchange(next_, Stay{});
    }
    void draw(Framebuffer& frame) const override { frame.at(0, 0).glyph = name_; }

  private:
    char name_;
    Transition next_;
};

char topName(const App& app) {
    Framebuffer frame(80, 24);
    app.draw(frame);
    return static_cast<char>(frame.at(0, 0).glyph);
}

struct TitleFixture {
    explicit TitleFixture(bool withSave) {
        dir = fs::temp_directory_path() / "heroes_app_test";
        fs::create_directories(dir);
        fs::remove(dir / "save.json");
        if (withSave)
            std::ofstream(dir / "save.json") << "{}";
        session.saveFile = dir / "save.json";
    }
    fs::path dir;
    Session session{game::shippedContent(), shippedSprites(), {}, std::nullopt};
};

} // namespace

TEST(App, PushReplacePopAndQuit) {
    NullIo io;
    engine::Renderer renderer(engine::ColorMode::TrueColor);
    App app(io, renderer,
            std::make_unique<ScriptedScreen>(
                'A', Push{std::make_unique<ScriptedScreen>(
                         'B', Replace{std::make_unique<ScriptedScreen>('C', Pop{})})}));
    EXPECT_TRUE(app.update({}, 0));
    EXPECT_EQ(topName(app), 'B');
    EXPECT_TRUE(app.update({}, 0));
    EXPECT_EQ(topName(app), 'C');
    EXPECT_EQ(app.depth(), 2u);
    EXPECT_TRUE(app.update({}, 0)); // C pops back to A
    EXPECT_EQ(topName(app), 'A');
    EXPECT_TRUE(app.update({}, 0)); // A has nothing more to do

    App quitting(io, renderer, std::make_unique<ScriptedScreen>('Q', Quit{}));
    EXPECT_FALSE(quitting.update({}, 0));
    App emptying(io, renderer, std::make_unique<ScriptedScreen>('P', Pop{}));
    EXPECT_FALSE(emptying.update({}, 0)); // popping the last screen ends the app
}

TEST(TitleScreen, RendersBannerMenuAndHint) {
    TitleFixture f(false);
    TitleScreen title(f.session);
    Framebuffer frame(80, 24);
    title.draw(frame);
    EXPECT_NE(rowText(frame, 6).find("Ordo Lucis"), std::string::npos);
    EXPECT_EQ(rowText(frame, 4).find("┌"), rowText(frame, 4).find_first_not_of(' '));
    EXPECT_NE(rowText(frame, 14).find("▶ New Game"), std::string::npos);
    EXPECT_NE(rowText(frame, 15).find("Continue"), std::string::npos);
    EXPECT_NE(rowText(frame, 16).find("Quit"), std::string::npos);
    EXPECT_NE(rowText(frame, 22).find("↑↓ select · Enter confirm"), std::string::npos);
    EXPECT_EQ(frame.at(0, 0).bg, palette::kBg);
}

TEST(TitleScreen, ContinueIsDisabledWithoutASaveFile) {
    TitleFixture none(false);
    Framebuffer frame(80, 24);
    TitleScreen(none.session).draw(frame);
    EXPECT_EQ(frame.at(36, 15).fg, palette::kDim);

    TitleFixture saved(true);
    Framebuffer frame2(80, 24);
    TitleScreen(saved.session).draw(frame2);
    EXPECT_EQ(frame2.at(36, 15).fg, palette::kText);
}

TEST(TitleScreen, QuitEndsTheApp) {
    NullIo io;
    engine::Renderer renderer(engine::ColorMode::TrueColor);
    TitleFixture f(false);
    App byDigit(io, renderer, std::make_unique<TitleScreen>(f.session));
    const KeyEvent three{Key::Char, '3'};
    EXPECT_FALSE(byDigit.update(std::span(&three, 1), 0));

    App byArrows(io, renderer, std::make_unique<TitleScreen>(f.session));
    const KeyEvent keys[] = {{Key::Down}, {Key::Enter}}; // one Down: disabled Continue is skipped
    EXPECT_TRUE(byArrows.update(std::span(keys, 1), 0));
    EXPECT_FALSE(byArrows.update(std::span(keys + 1, 1), 0));
}

TEST(Options, ParsesSeedAndDataDir) {
    const std::string_view args[] = {"--seed", "42", "--data-dir", "/tmp/mydata"};
    const auto options = parseOptions(args);
    ASSERT_TRUE(options.has_value());
    EXPECT_EQ(options->seed, 42u);
    EXPECT_EQ(options->dataDir, fs::path("/tmp/mydata"));
    EXPECT_FALSE(parseOptions({}).value().seed.has_value());
}

TEST(Options, RejectsBadInput) {
    const std::string_view badSeed[] = {"--seed", "12x"};
    EXPECT_FALSE(parseOptions(badSeed).has_value());
    const std::string_view missing[] = {"--data-dir"};
    EXPECT_FALSE(parseOptions(missing).has_value());
    const std::string_view unknown[] = {"--cheat"};
    EXPECT_NE(parseOptions(unknown).error().find("--cheat"), std::string::npos);
}

TEST(Options, LayoutAndArtDefaultsAndValues) {
    const Options defaults = parseOptions({}).value();
    EXPECT_EQ(defaults.layout, Layout::Auto);
    EXPECT_EQ(defaults.art, ArtStyle::Glyph);
    const std::string_view compact[] = {"--layout", "compact", "--art", "blocks"};
    const Options set = parseOptions(compact).value();
    EXPECT_EQ(set.layout, Layout::Compact);
    EXPECT_EQ(set.art, ArtStyle::Blocks);
    const std::string_view autoGlyph[] = {"--layout", "auto", "--art", "glyph"};
    EXPECT_EQ(parseOptions(autoGlyph).value().layout, Layout::Auto);
}

TEST(Options, RejectsBadLayoutAndArt) {
    const std::string_view badLayout[] = {"--layout", "big"};
    EXPECT_EQ(parseOptions(badLayout).error(), "--layout expects auto or compact, got 'big'");
    const std::string_view badArt[] = {"--art", "ascii"};
    EXPECT_EQ(parseOptions(badArt).error(), "--art expects glyph or blocks, got 'ascii'");
    const std::string_view missing[] = {"--art"};
    EXPECT_EQ(parseOptions(missing).error(), "--art needs a value");
}

TEST(App, TheTopScreenChoosesTheCanvasSize) {
    // A screen that asks for 120x40 when the terminal allows it, records what it was drawn
    // into, and quits on its first update.
    class BigScreen final : public Screen {
      public:
        explicit BigScreen(engine::Size& drawn) : drawn_(drawn) {}
        Transition update(std::span<const KeyEvent>, double) override { return Quit{}; }
        void draw(Framebuffer& frame) const override { drawn_ = {frame.width(), frame.height()}; }
        engine::Size canvasSize(engine::Size terminal) const override {
            return terminal.width >= 120 && terminal.height >= 40 ? engine::Size{120, 40}
                                                                  : engine::kCompactCanvas;
        }

      private:
        engine::Size& drawn_;
    };
    class BigIo : public engine::LoopIo {
      public:
        std::pair<int, int> size() const override { return {150, 45}; }
        std::vector<KeyEvent> readKeys() override { return {}; }
        void write(std::string_view) override {}
    };

    BigIo io;
    engine::Renderer renderer(engine::ColorMode::TrueColor);
    engine::Size drawn{};
    App app(io, renderer, std::make_unique<BigScreen>(drawn));
    EXPECT_EQ(app.canvasSize({150, 45}), (engine::Size{120, 40}));
    EXPECT_EQ(app.canvasSize({100, 30}), engine::kCompactCanvas);
    EXPECT_EQ(app.run(), 0); // the first frame draws before any step runs, then it quits
    EXPECT_EQ(drawn, (engine::Size{120, 40}));

    App compact(io, renderer, std::make_unique<ScriptedScreen>('A'));
    EXPECT_EQ(compact.canvasSize({150, 45}), engine::kCompactCanvas); // the default
}

TEST(Options, DataDirPrecedence) {
    Options flag;
    flag.dataDir = "/flag";
    EXPECT_EQ(resolveDataDir(flag, "/env", "/src"), fs::path("/flag"));
    EXPECT_EQ(resolveDataDir({}, "/env", "/src"), fs::path("/env"));
    EXPECT_EQ(resolveDataDir({}, std::nullopt, "/src"), fs::path("/src/data"));
}

TEST(LargeSprites, ShippedArtLoadsInBothLooks) {
    const auto large = loadLargeSprites(game::shippedContent(),
                                        fs::path(HEROES_SOURCE_DIR) / "assets" / "sprites" / "large");
    ASSERT_TRUE(large.has_value()) << large.error();
    EXPECT_EQ(large->blocks.size(), 6u);
    EXPECT_EQ(large->glyphs.size(), 6u);
    for (const char* hero : {"hero_fighter.sprite", "hero_hunter.sprite", "hero_rogue.sprite"}) {
        EXPECT_EQ(large->blocks.at(hero).width, 48) << hero;
        EXPECT_EQ(large->blocks.at(hero).height, 48) << hero;
        EXPECT_EQ(large->glyphs.at(hero).width, 48) << hero; // the same cell footprint
        EXPECT_EQ(large->glyphs.at(hero).height, 24) << hero;
    }
    for (const char* boss : {"medusa.sprite", "avaritia.sprite"}) {
        EXPECT_EQ(large->blocks.at(boss).width, 56) << boss;
        EXPECT_EQ(large->glyphs.at(boss).height, 26) << boss;
    }
    EXPECT_EQ(large->glyphs.at("monster.sprite").width, 32);
    EXPECT_EQ(large->glyphs.at("monster.sprite").height, 16);
}

TEST(LargeSprites, MissingArtIsSimplyAbsent) {
    const fs::path dir = fs::temp_directory_path() / "heroes_large_missing";
    fs::remove_all(dir);
    fs::create_directories(dir);
    std::ofstream(dir / "medusa.glyphs") << "glyphs 1 1\npalette\na #ffffff\nchars\n|@|\ncolors\na\n";
    const auto large = loadLargeSprites(game::shippedContent(), dir);
    ASSERT_TRUE(large.has_value()) << large.error();
    EXPECT_TRUE(large->blocks.empty());
    EXPECT_EQ(large->glyphs.size(), 1u); // only the one that exists
    EXPECT_TRUE(large->glyphs.contains("medusa.sprite"));
}

TEST(LargeSprites, BrokenFileIsReportedByNameAndLine) {
    const fs::path dir = fs::temp_directory_path() / "heroes_large_broken";
    fs::remove_all(dir);
    fs::create_directories(dir);
    std::ofstream(dir / "hero_fighter.glyphs")
        << "glyphs 2 1\npalette\na #ffffff\nchars\n|@|\ncolors\na\n"; // row 1 char short
    const auto large = loadLargeSprites(game::shippedContent(), dir);
    ASSERT_FALSE(large.has_value());
    EXPECT_NE(large.error().find("hero_fighter.glyphs"), std::string::npos) << large.error();
    EXPECT_NE(large.error().find("line 5"), std::string::npos) << large.error();
}
