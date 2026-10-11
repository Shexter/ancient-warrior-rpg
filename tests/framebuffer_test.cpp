#include "engine/framebuffer.hpp"

#include <gtest/gtest.h>

using namespace heroes::engine;

TEST(Framebuffer, StartsBlank) {
    const Framebuffer fb(4, 3);
    EXPECT_EQ(fb.width(), 4);
    EXPECT_EQ(fb.height(), 3);
    EXPECT_EQ(fb.at(3, 2), Cell{});
}

TEST(Framebuffer, SetClipsOutOfBounds) {
    Framebuffer fb(3, 3);
    const Framebuffer before = fb;
    Cell c;
    c.glyph = U'X';
    fb.set(-1, 0, c);
    fb.set(0, -1, c);
    fb.set(3, 0, c);
    fb.set(0, 3, c);
    EXPECT_EQ(fb, before);
    fb.set(2, 2, c);
    EXPECT_EQ(fb.at(2, 2).glyph, U'X');
}

TEST(Framebuffer, ClearSetsBackgroundAndBlanksGlyphs) {
    Framebuffer fb(2, 2);
    fb.text(0, 0, "ab", Rgb{1, 2, 3});
    fb.clear(Rgb{9, 8, 7});
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x) {
            EXPECT_EQ(fb.at(x, y).glyph, U' ');
            EXPECT_EQ(fb.at(x, y).bg, (Rgb{9, 8, 7}));
        }
}

TEST(Framebuffer, TextClipsAtBothEdges) {
    Framebuffer fb(5, 1);
    fb.text(-2, 0, "abcdefg", Rgb{1, 1, 1});
    EXPECT_EQ(fb.at(0, 0).glyph, U'c');
    EXPECT_EQ(fb.at(4, 0).glyph, U'g');
    fb.text(0, 5, "zzz", Rgb{1, 1, 1}); // row out of range: nothing drawn, no crash
}

TEST(Framebuffer, TextKeepsBackgroundUnlessGiven) {
    Framebuffer fb(3, 1);
    fb.clear(Rgb{5, 5, 5});
    fb.text(0, 0, "a", Rgb{1, 1, 1});
    EXPECT_EQ(fb.at(0, 0).bg, (Rgb{5, 5, 5}));
    fb.text(1, 0, "b", Rgb{1, 1, 1}, Rgb{6, 6, 6});
    EXPECT_EQ(fb.at(1, 0).bg, (Rgb{6, 6, 6}));
}

TEST(Framebuffer, NonAsciiTextBecomesQuestionMark) {
    Framebuffer fb(2, 1);
    fb.text(0, 0, "\xC3\xA9", Rgb{1, 1, 1}); // UTF-8 bytes, each drawn as '?'
    EXPECT_EQ(fb.at(0, 0).glyph, U'?');
}

TEST(Framebuffer, FillRectClips) {
    Framebuffer fb(4, 4);
    Cell c;
    c.glyph = U'#';
    fb.fillRect(2, 2, 10, 10, c);
    EXPECT_EQ(fb.at(3, 3).glyph, U'#');
    EXPECT_EQ(fb.at(1, 1).glyph, U' ');
}

TEST(Framebuffer, BoxDrawsFrameAndKeepsBackground) {
    Framebuffer fb(5, 4);
    fb.clear(Rgb{3, 3, 3});
    fb.box(0, 0, 5, 4, Rgb{9, 9, 9});
    EXPECT_EQ(fb.at(0, 0).glyph, U'┌');
    EXPECT_EQ(fb.at(4, 0).glyph, U'┐');
    EXPECT_EQ(fb.at(0, 3).glyph, U'└');
    EXPECT_EQ(fb.at(4, 3).glyph, U'┘');
    EXPECT_EQ(fb.at(2, 0).glyph, U'─');
    EXPECT_EQ(fb.at(0, 1).glyph, U'│');
    EXPECT_EQ(fb.at(2, 1).glyph, U' '); // interior untouched
    EXPECT_EQ(fb.at(0, 0).bg, (Rgb{3, 3, 3}));
}

TEST(Framebuffer, DegenerateAndPartlyOffscreenBoxesAreSafe) {
    Framebuffer fb(3, 3);
    const Framebuffer before = fb;
    fb.box(0, 0, 1, 5, Rgb{1, 1, 1});
    EXPECT_EQ(fb, before);
    fb.box(-2, -2, 10, 10, Rgb{1, 1, 1}); // all edges off-screen except none: no crash
}
