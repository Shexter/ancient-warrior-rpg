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

TEST(Options, DataDirPrecedence) {
    Options flag;
    flag.dataDir = "/flag";
    EXPECT_EQ(resolveDataDir(flag, "/env", "/src"), fs::path("/flag"));
    EXPECT_EQ(resolveDataDir({}, "/env", "/src"), fs::path("/env"));
    EXPECT_EQ(resolveDataDir({}, std::nullopt, "/src"), fs::path("/src/data"));
}
