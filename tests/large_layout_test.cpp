#include "app/art.hpp"
#include "app/screens/mission.hpp"

#include "frame_text.hpp"
#include "screen_harness.hpp"

#include <gtest/gtest.h>

#include <iostream>
#include <string>

using namespace screen_test;

namespace {

constexpr engine::Size kBigTerminal{150, 45};

game::GameState levelTwenty() {
    game::GameState state = freshGame();
    state.hero.level = 20;
    state.hero.maxHp = state.hero.hp = 100000; // outlasts Medusa, so the battle keeps going
    return state;
}

// A harness whose session has the shipped large art.
void withLargeArt(Harness& h, ArtStyle art = ArtStyle::Glyph) {
    h.session.large = shippedLargeSprites();
    h.session.art = art;
}

// Hub -> Mission -> Medusa (the bonus, '4') -> Yes.
void enterMedusa(Harness& h) {
    h.startAtHub(levelTwenty());
    ASSERT_TRUE(h.press({ch('2'), ch('4'), kEnter, kEnter, ch('y')}));
    ASSERT_TRUE(frameContains(h.frame(), "MEDUSA LAIR"));
}

// The first opaque cell of a glyph sprite, as (x, y, char).
std::tuple<int, int, char> firstGlyph(const engine::GlyphSprite& sprite) {
    for (int y = 0; y < sprite.height; ++y)
        for (int x = 0; x < sprite.width; ++x)
            if (const auto& cell = sprite.at(x, y))
                return {x, y, cell->ch};
    return {-1, -1, ' '};
}

} // namespace

TEST(LargeLayout, BattleUsesTheLargeGlyphArtAndRows) {
    Harness h("large_battle");
    withLargeArt(h);
    enterMedusa(h);
    EXPECT_EQ(h.app->canvasSize(kBigTerminal), kLargeCanvas);
    Framebuffer frame = h.frame(kLargeCanvas);
    EXPECT_NE(rowText(frame, 0).find("MEDUSA LAIR"), std::string::npos);
    EXPECT_EQ(cellText(frame, 113, 0, 6), "Turn 1"); // right-aligned on 120 columns

    const auto [hx, hy, hc] = firstGlyph(shippedLargeSprites().glyphs.at("hero_fighter.sprite"));
    EXPECT_EQ(frame.at(6 + hx, 3 + hy).glyph, static_cast<char32_t>(hc)); // hero at (6, 3)
    const auto [ex, ey, ec] = firstGlyph(shippedLargeSprites().glyphs.at("medusa.sprite"));
    EXPECT_EQ(frame.at(60 + ex, 2 + ey).glyph, static_cast<char32_t>(ec)); // enemy at (60, 2)

    EXPECT_NE(rowText(frame, 29).find("ARIA"), std::string::npos);
    EXPECT_NE(rowText(frame, 29).find("MEDUSA"), std::string::npos);
    EXPECT_NE(rowText(frame, 30).find("FV"), std::string::npos);
    EXPECT_NE(rowText(frame, 38).find("▶ Attack"), std::string::npos);
    EXPECT_NE(rowText(frame, 39).find("Enter skips"), std::string::npos);

    // After a few turns the log fills all six rows (32-37).
    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(h.press({ch('1')}));
        ASSERT_TRUE(h.wait(5));
    }
    frame = h.frame(kLargeCanvas);
    for (int y = 32; y <= 37; ++y)
        EXPECT_NE(rowText(frame, y).find_first_not_of(' '), std::string::npos) << "row " << y;

    ASSERT_TRUE(h.press({ch('2')})); // skill overlay, rows 26-32
    frame = h.frame(kLargeCanvas);
    EXPECT_NE(rowText(frame, 26).find("Skills · Fervor cost in ()"), std::string::npos);
    EXPECT_NE(rowText(frame, 27).find("Guardian Angel"), std::string::npos);
}

TEST(LargeLayout, BlocksArtDrawsHalfBlocks) {
    Harness h("large_blocks");
    withLargeArt(h, ArtStyle::Blocks);
    enterMedusa(h);
    const Framebuffer frame = h.frame(kLargeCanvas);
    const engine::Sprite& hero = shippedLargeSprites().blocks.at("hero_fighter.sprite");
    for (int y = 0; y < hero.height; ++y)
        for (int x = 0; x < hero.width; ++x)
            if (hero.at(x, y)) {
                const char32_t g = frame.at(6 + x, 3 + y / 2).glyph;
                EXPECT_TRUE(g == U'▀' || g == U'▄') << x << "," << y;
                return;
            }
}

TEST(LargeLayout, ShrinkingMidBattleFallsBackWithTheSameState) {
    Harness h("large_shrink");
    withLargeArt(h);
    enterMedusa(h);
    ASSERT_TRUE(h.press({ch('1')}));
    const game::GameState during = *h.session.state;
    (void)h.frame(kLargeCanvas);
    EXPECT_EQ(h.app->canvasSize({100, 30}), engine::kCompactCanvas);
    const Framebuffer compact = h.frame(engine::kCompactCanvas);
    EXPECT_NE(rowText(compact, 16).find("ARIA"), std::string::npos); // the compact stats row
    EXPECT_EQ(*h.session.state, during);
    ASSERT_TRUE(h.wait(5)); // the battle carries on
    EXPECT_EQ(h.app->canvasSize(kBigTerminal), kLargeCanvas); // and grows back
}

TEST(LargeLayout, FallsBackWhenArtIsMissingOrCompactIsForced) {
    Harness h("large_fallback");
    withLargeArt(h);
    h.session.large.glyphs.erase("medusa.sprite"); // no large Medusa in the glyph look
    enterMedusa(h);
    EXPECT_EQ(h.app->canvasSize(kBigTerminal), engine::kCompactCanvas);

    Harness hub("large_fallback_hub");
    withLargeArt(hub);
    hub.startAtHub(levelTwenty());
    EXPECT_EQ(hub.app->canvasSize(kBigTerminal), kLargeCanvas);
    hub.session.layout = Layout::Compact;
    EXPECT_EQ(hub.app->canvasSize(kBigTerminal), engine::kCompactCanvas);
    hub.session.layout = Layout::Auto;
    hub.session.art = ArtStyle::Blocks;
    hub.session.large.blocks.clear(); // the chosen look has no art at all
    EXPECT_EQ(hub.app->canvasSize(kBigTerminal), engine::kCompactCanvas);
}

TEST(LargeLayout, HubPlacesTheLargeHeroAndMenu) {
    Harness h("large_hub");
    withLargeArt(h);
    h.startAtHub(levelTwenty());
    h.session.lastMessage = "Welcome back.";
    const Framebuffer frame = h.frame(kLargeCanvas);
    EXPECT_EQ(frame.at(119, 0).glyph, U'┐'); // the status panel spans 120 columns
    EXPECT_EQ(cellText(frame, 60, 12, 10), "▶ Training");
    const auto [x, y, c] = firstGlyph(shippedLargeSprites().glyphs.at("hero_fighter.sprite"));
    EXPECT_EQ(frame.at(4 + x, 7 + y).glyph, static_cast<char32_t>(c));
    EXPECT_NE(rowText(frame, 38).find("Welcome back."), std::string::npos);
}

TEST(LargeLayout, CreateHeroIsLargeOnlyOnThePathStep) {
    Harness h("large_create");
    withLargeArt(h);
    h.startAtTitle();
    ASSERT_TRUE(h.press({ch('1'), kEnter, kEnter}));
    EXPECT_EQ(h.app->canvasSize(kBigTerminal), engine::kCompactCanvas); // the name step
    ASSERT_TRUE(h.type("Aria"));
    ASSERT_TRUE(h.press({kEnter, kEnter, kEnter}));
    EXPECT_EQ(h.app->canvasSize(kBigTerminal), kLargeCanvas);
    const Framebuffer frame = h.frame(kLargeCanvas);
    EXPECT_EQ(frame.at(40, 4).glyph, U'┌'); // the preview box at cols 40-118
    EXPECT_EQ(frame.at(118, 4).glyph, U'┐');
    const auto [x, y, c] = firstGlyph(shippedLargeSprites().glyphs.at("hero_fighter.sprite"));
    EXPECT_EQ(frame.at(66 + x, 8 + y).glyph, static_cast<char32_t>(c));
    EXPECT_TRUE(frameContains(frame, "Fervor 50"));
}

TEST(LargeLayout, TrainingPlaybackIsLargeButTheTotalsAreNot) {
    Harness h("large_training");
    withLargeArt(h);
    h.startAtHub(freshGame());
    ASSERT_TRUE(h.press({ch('1'), ch('1'), kEnter}));
    EXPECT_EQ(h.app->canvasSize(kBigTerminal), kLargeCanvas);
    const Framebuffer frame = h.frame(kLargeCanvas);
    EXPECT_NE(rowText(frame, 1).find("THE CATACOMBS · Level 1"), std::string::npos);
    const auto [x, y, c] = firstGlyph(shippedLargeSprites().glyphs.at("monster.sprite"));
    EXPECT_EQ(frame.at(80 + x, 8 + y).glyph, static_cast<char32_t>(c));
    EXPECT_NE(rowText(frame, 29).find("ARIA"), std::string::npos);
    ASSERT_TRUE(h.press({kEsc, kEsc})); // finish the round, leave: totals
    EXPECT_EQ(h.app->canvasSize(kBigTerminal), engine::kCompactCanvas);
}

// docs/benchmarks.md: bytes per frame, diff renderer vs a full redraw (a fresh Renderer each
// frame), over 120 frames of an attack animation. Printed for the record; the test only checks
// that diffing still pays off at every size.
TEST(LargeLayout, DiffRenderingStillPaysOffAt120x40) {
    struct Case {
        const char* name;
        engine::Size size;
        ArtStyle art;
    };
    for (const Case& c : {Case{"80x24 compact", engine::kCompactCanvas, ArtStyle::Glyph},
                          Case{"120x40 glyph", kLargeCanvas, ArtStyle::Glyph},
                          Case{"120x40 blocks", kLargeCanvas, ArtStyle::Blocks}}) {
        Harness h(std::string("bytes_") + std::to_string(c.size.width) +
                  (c.art == ArtStyle::Glyph ? "g" : "b"));
        withLargeArt(h, c.art);
        enterMedusa(h);
        ASSERT_TRUE(h.press({ch('1')}));
        engine::Renderer diff(engine::ColorMode::TrueColor);
        std::string out;
        diff.present(h.frame(c.size), out); // the first frame is a full redraw either way
        std::size_t diffBytes = 0, fullBytes = 0;
        for (int i = 0; i < 120; ++i) {
            ASSERT_TRUE(h.wait(1.0 / 60));
            const Framebuffer frame = h.frame(c.size);
            out.clear();
            diff.present(frame, out);
            diffBytes += out.size();
            engine::Renderer fresh(engine::ColorMode::TrueColor);
            out.clear();
            fresh.present(frame, out);
            fullBytes += out.size();
        }
        std::cout << c.name << ": full redraw " << fullBytes / 120 << " bytes/frame, diff "
                  << diffBytes / 120 << " bytes/frame\n";
        EXPECT_LT(diffBytes * 4, fullBytes) << c.name; // under a quarter of a full redraw
    }
}
