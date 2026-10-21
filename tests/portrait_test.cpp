#include "app/art.hpp"

#include "frame_text.hpp"
#include "screen_harness.hpp"

#include <format>

using namespace screen_test;

namespace {

const engine::Size kPhone = engine::kPortraitCanvas;

// The lowest cell holding `glyph` (the pad is at the bottom; menus use ▶ too).
std::pair<int, int> find(const Framebuffer& frame, char32_t glyph) {
    for (int y = frame.height() - 1; y >= 0; --y)
        for (int x = 0; x < frame.width(); ++x)
            if (frame.at(x, y).glyph == glyph)
                return {x, y};
    return {-1, -1};
}

KeyEvent tap(std::pair<int, int> cell) { return {Key::Click, 0, cell.first, cell.second}; }

} // namespace

TEST(Portrait, ChosenForNarrowTerminalsOrWhenForced) {
    Harness h("portrait_rule");
    const engine::Size other{120, 40};
    EXPECT_EQ(portraitOr(h.session, {48, 45}, other), kPhone);
    EXPECT_EQ(portraitOr(h.session, {79, 50}, other), kPhone);
    EXPECT_EQ(portraitOr(h.session, {80, 24}, other), other);
    h.session.layout = Layout::Compact;
    EXPECT_EQ(portraitOr(h.session, {48, 45}, other), other);
    h.session.layout = Layout::Portrait;
    EXPECT_EQ(portraitOr(h.session, {200, 60}, other), kPhone);
}

TEST(Portrait, PadAndCloseButtonStandForKeys) {
    EXPECT_EQ(controlKey({Key::Click, 0, 24, 34}), KeyEvent{Key::Up});
    EXPECT_EQ(controlKey({Key::Click, 0, 3, 37}), KeyEvent{Key::Left});
    EXPECT_EQ(controlKey({Key::Click, 0, 24, 37}), KeyEvent{Key::Enter});
    EXPECT_EQ(controlKey({Key::Click, 0, 45, 37}), KeyEvent{Key::Right});
    EXPECT_EQ(controlKey({Key::Click, 0, 24, 40}), KeyEvent{Key::Down});
    EXPECT_EQ(controlKey({Key::Click, 0, 46, 0}), KeyEvent{Key::Escape});
    EXPECT_EQ(controlKey({Key::Click, 0, 3, 34}), KeyEvent{Key::Escape}); // Back, beside the ▲
    EXPECT_EQ(controlKey({Key::Click, 0, 45, 34}), std::nullopt);         // the other corner
    EXPECT_EQ(controlKey({Key::Enter}), std::nullopt);
}

TEST(Portrait, HubHasAHeaderTheHeroBoxedRowsAndThePad) {
    Harness h("portrait_hub");
    h.startAtHub(freshGame());
    EXPECT_EQ(h.app->canvasSize({48, 45}), kPhone);
    const Framebuffer frame = h.frame(kPhone);
    const game::Hero& hero = h.session.state->hero;
    EXPECT_NE(rowText(frame, 0).find("ARIA · Knight"), std::string::npos) << rowText(frame, 0);
    EXPECT_NE(rowText(frame, 0).find("✕"), std::string::npos);
    EXPECT_NE(rowText(frame, 2).find("LEVEL"), std::string::npos);
    EXPECT_NE(rowText(frame, 2).find("GOLD"), std::string::npos);
    EXPECT_NE(rowText(frame, 3).find(std::format("{}/", hero.hp)), std::string::npos);
    EXPECT_NE(rowText(frame, 3).find("20000"), std::string::npos);
    EXPECT_NE(rowText(frame, 23).find("Explore"), std::string::npos);
    EXPECT_NE(rowText(frame, 24).find("Training"), std::string::npos);
    EXPECT_NE(rowText(frame, 28).find("Quit"), std::string::npos);
    for (char32_t glyph : {U'▲', U'◀', U'●', U'▶', U'▼'})
        EXPECT_GE(find(frame, glyph).second, portrait::kPadTop) << static_cast<int>(glyph);
}

TEST(Portrait, ThePadMovesAndConfirms) {
    Harness h("portrait_pad");
    h.startAtHub(freshGame()); // Explore is off on the phone, Mission locked at Lv 1: Training, Town
    const Framebuffer frame = h.frame(kPhone);
    ASSERT_TRUE(h.press({tap(find(frame, U'▼'))}));
    ASSERT_TRUE(h.press({tap(find(frame, U'●'))}));
    EXPECT_TRUE(frameContains(h.frame(), "CIVITAS LUCIS"));
}

TEST(Portrait, TappingARowOrTheCloseButton) {
    Harness h("portrait_rows");
    h.startAtHub(freshGame());
    h.session.dirty = true;
    const Framebuffer frame = h.frame(kPhone);
    ASSERT_TRUE(h.press({tap(find(frame, U'✕'))})); // Back on the Hub is Quit
    const Framebuffer asking = h.frame(kPhone);
    ASSERT_TRUE(frameContains(asking, "Quit without saving?"));
    for (int y = 0; y < asking.height(); ++y) // the dialog fits the phone width
        EXPECT_NE(asking.at(0, y).glyph, U'│');
    ASSERT_TRUE(h.press({{Key::Char, 'n'}}));
    (void)h.frame(kPhone);
    ASSERT_TRUE(h.press({{Key::Click, 0, 10, portrait::kMenuTop + 4}})); // the Town row
    EXPECT_TRUE(frameContains(h.frame(), "CIVITAS LUCIS"));
}
