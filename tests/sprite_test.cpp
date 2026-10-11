#include "engine/sprite.hpp"

#include <gtest/gtest.h>

#include <format>

using namespace heroes::engine;

namespace {

constexpr Rgb kRed{255, 0, 0};
constexpr Rgb kBlue{0, 0, 255};
constexpr Rgb kBg{9, 9, 9};

// A sprite from rows of 'r' (red), 'b' (blue) and '.' (transparent).
Sprite make(std::initializer_list<std::string_view> rows) {
    std::string text = std::format("sprite {} {}\npalette\nr #ff0000\nb #0000ff\npixels\n",
                                   rows.begin()->size(), rows.size());
    for (std::string_view row : rows)
        text += std::string(row) + "\n";
    auto sprite = parseSprite(text);
    EXPECT_TRUE(sprite.has_value()) << (sprite ? "" : sprite.error());
    return sprite.value_or(Sprite{});
}

Framebuffer bgFrame(int w = 4, int h = 3) {
    Framebuffer fb(w, h);
    fb.clear(kBg);
    return fb;
}

std::string parseError(std::string_view text) {
    const auto sprite = parseSprite(text);
    EXPECT_FALSE(sprite.has_value());
    return sprite ? "" : sprite.error();
}

} // namespace

// ---- parsing ---------------------------------------------------------------------------------

TEST(SpriteParse, ReadsSizePaletteAndPixels) {
    const auto sprite =
        parseSprite("# a comment\nsprite 3 2\npalette\na #1e1e2e\nB #F5C2E7\npixels\na.B\n.aB\n");
    ASSERT_TRUE(sprite.has_value());
    EXPECT_EQ(sprite->width, 3);
    EXPECT_EQ(sprite->height, 2);
    EXPECT_EQ(sprite->at(0, 0), (Rgb{0x1e, 0x1e, 0x2e}));
    EXPECT_FALSE(sprite->at(1, 0).has_value());
    EXPECT_EQ(sprite->at(2, 0), (Rgb{0xf5, 0xc2, 0xe7}));
    EXPECT_FALSE(sprite->at(0, 1).has_value());
}

TEST(SpriteParse, ToleratesCrLfBlankLinesAndComments) {
    const auto sprite =
        parseSprite("sprite 1 1\r\n\r\n# c\r\npalette\r\na #000000\r\npixels\r\na\r\n");
    ASSERT_TRUE(sprite.has_value());
    EXPECT_EQ(sprite->at(0, 0), (Rgb{0, 0, 0}));
}

TEST(SpriteParse, BadSizeCarriesLineNumber) {
    EXPECT_EQ(parseError("# hi\nsprite 0 4\n"),
              "line 2: bad size: width and height must be whole numbers 1-512");
    EXPECT_NE(parseError("sprite x 4\n").find("line 1: bad size"), std::string::npos);
    EXPECT_NE(parseError("sprite 4\n").find("line 1: expected 'sprite <width> <height>'"),
              std::string::npos);
    EXPECT_NE(parseError("sprite 513 4\n").find("line 1: bad size"), std::string::npos);
    EXPECT_NE(parseError("sprite -2 4\n").find("line 1: bad size"), std::string::npos);
}

TEST(SpriteParse, UnknownPaletteKeyCarriesLineNumber) {
    EXPECT_EQ(parseError("sprite 2 1\npalette\na #000000\npixels\naz\n"),
              "line 5: unknown palette key 'z'");
}

TEST(SpriteParse, WrongRowLengthCarriesLineNumber) {
    EXPECT_EQ(parseError("sprite 3 2\npalette\na #000000\npixels\naaa\naa\n"),
              "line 6: row has 2 characters, expected 3");
    EXPECT_NE(parseError("sprite 2 1\npalette\na #000000\npixels\naaa\n")
                  .find("line 5: row has 3 characters, expected 2"),
              std::string::npos);
}

TEST(SpriteParse, WrongRowCount) {
    EXPECT_EQ(parseError("sprite 1 3\npalette\na #000000\npixels\na\na\n"),
              "expected 3 pixel rows, found 2");
    EXPECT_EQ(parseError("sprite 1 1\npalette\na #000000\npixels\na\na\n"),
              "line 6: more pixel rows than the height");
}

TEST(SpriteParse, PaletteProblems) {
    EXPECT_NE(parseError("sprite 1 1\npalette\na red\npixels\na\n").find("line 3: bad color 'red'"),
              std::string::npos);
    EXPECT_NE(parseError("sprite 1 1\npalette\na #12345\npixels\na\n").find("line 3: bad color"),
              std::string::npos);
    EXPECT_NE(parseError("sprite 1 1\npalette\na #zzzzzz\npixels\na\n").find("line 3: bad color"),
              std::string::npos);
    EXPECT_NE(parseError("sprite 1 1\npalette\n. #000000\npixels\n.\n").find("line 3:"),
              std::string::npos);
    EXPECT_EQ(parseError("sprite 1 1\npalette\na #000000\na #111111\npixels\na\n"),
              "line 4: duplicate palette key 'a'");
}

TEST(SpriteParse, MissingSections) {
    EXPECT_EQ(parseError(""), "missing 'sprite <width> <height>' line");
    EXPECT_EQ(parseError("sprite 1 1\n"), "missing 'palette' section");
    EXPECT_EQ(parseError("sprite 1 1\npalette\na #000000\n"), "missing 'pixels' section");
    EXPECT_NE(parseError("palette\n").find("line 1: 'palette' before"), std::string::npos);
    EXPECT_NE(parseError("sprite 1 1\npixels\n").find("line 2: 'pixels' before 'palette'"),
              std::string::npos);
    EXPECT_NE(parseError("sprite 1 1\nbogus\n").find("line 2: unexpected 'bogus'"),
              std::string::npos);
}

TEST(SpriteLoad, MissingFileNamesThePath) {
    const auto sprite = loadSprite("/definitely/not/here.sprite");
    ASSERT_FALSE(sprite.has_value());
    EXPECT_NE(sprite.error().find("here.sprite"), std::string::npos);
}

// ---- drawing ---------------------------------------------------------------------------------

TEST(SpriteDraw, TransparentTopPixelKeepsBackground) {
    Framebuffer fb = bgFrame();
    drawSprite(fb, make({"b", "r"}), 1, 1); // sanity: both opaque
    EXPECT_EQ(fb.at(1, 1).bg, kRed);

    Framebuffer fb2 = bgFrame();
    drawSprite(fb2, make({".", "r"}), 1, 1);
    const Cell& c = fb2.at(1, 1);
    EXPECT_EQ(c.glyph, U'▄');
    EXPECT_EQ(c.fg, kRed);
    EXPECT_EQ(c.bg, kBg); // the top half still shows the background
}

TEST(SpriteDraw, TopOnlyBottomOnlyAndBothCells) {
    Framebuffer fb = bgFrame(3, 1);
    drawSprite(fb, make({"rb.", ".br"}), 0, 0);
    // column 0: top red, bottom transparent
    EXPECT_EQ(fb.at(0, 0).glyph, U'▀');
    EXPECT_EQ(fb.at(0, 0).fg, kRed);
    EXPECT_EQ(fb.at(0, 0).bg, kBg);
    // column 1: both blue
    EXPECT_EQ(fb.at(1, 0).glyph, U'▀');
    EXPECT_EQ(fb.at(1, 0).fg, kBlue);
    EXPECT_EQ(fb.at(1, 0).bg, kBlue);
    // column 2: top transparent, bottom red
    EXPECT_EQ(fb.at(2, 0).glyph, U'▄');
    EXPECT_EQ(fb.at(2, 0).fg, kRed);
    EXPECT_EQ(fb.at(2, 0).bg, kBg);
}

TEST(SpriteDraw, BothOpaqueUsesTopAsForegroundAndBottomAsBackground) {
    Framebuffer fb = bgFrame();
    drawSprite(fb, make({"r", "b"}), 0, 0);
    EXPECT_EQ(fb.at(0, 0).glyph, U'▀');
    EXPECT_EQ(fb.at(0, 0).fg, kRed);
    EXPECT_EQ(fb.at(0, 0).bg, kBlue);
}

TEST(SpriteDraw, FullyTransparentCellIsUntouched) {
    Framebuffer fb = bgFrame();
    fb.at(0, 0).glyph = U'X';
    const Cell before = fb.at(0, 0);
    drawSprite(fb, make({".", "."}), 0, 0);
    EXPECT_EQ(fb.at(0, 0), before);
}

TEST(SpriteDraw, OddHeightTreatsLastBottomPixelAsTransparent) {
    Framebuffer fb = bgFrame();
    drawSprite(fb, make({"r", "r", "b"}), 0, 0); // 3 rows -> 2 cell rows
    EXPECT_EQ(fb.at(0, 0).glyph, U'▀');
    EXPECT_EQ(fb.at(0, 1).glyph, U'▀'); // blue on top, nothing below
    EXPECT_EQ(fb.at(0, 1).fg, kBlue);
    EXPECT_EQ(fb.at(0, 1).bg, kBg);
}

TEST(SpriteDraw, SpansTwoPixelRowsPerCellRow) {
    Framebuffer fb = bgFrame(1, 3);
    drawSprite(fb, make({"r", "r", "b", "b"}), 0, 0);
    EXPECT_EQ(fb.at(0, 0).fg, kRed);
    EXPECT_EQ(fb.at(0, 1).fg, kBlue);
    EXPECT_EQ(fb.at(0, 2).glyph, U' '); // beyond the sprite
}

TEST(SpriteDraw, ClipsOffTheFramebuffer) {
    Framebuffer fb = bgFrame(2, 2);
    const Sprite big = make({"rrrr", "rrrr", "rrrr", "rrrr"});
    drawSprite(fb, big, -2, -1); // partly off the top-left
    drawSprite(fb, big, 1, 1);   // partly off the bottom-right
    drawSprite(fb, big, 50, 50); // entirely off
    drawSprite(fb, big, -50, -50);
    EXPECT_EQ(fb.at(0, 0).glyph, U'▀');
}

TEST(SpriteDraw, FlashMakesOpaquePixelsWhite) {
    Framebuffer fb = bgFrame(3, 1);
    drawSprite(fb, make({"rb.", ".b."}), 0, 0, SpriteTint::flash());
    constexpr Rgb white{255, 255, 255};
    EXPECT_EQ(fb.at(0, 0).fg, white);
    EXPECT_EQ(fb.at(1, 0).fg, white);
    EXPECT_EQ(fb.at(1, 0).bg, white);
    EXPECT_EQ(fb.at(2, 0).glyph, U' ');
}

TEST(SpriteDraw, DimScalesChannels) {
    const Sprite one = make({"r", "b"});
    Framebuffer fb = bgFrame();
    drawSprite(fb, one, 0, 0, SpriteTint::dim(0));
    EXPECT_EQ(fb.at(0, 0).fg, kRed);
    drawSprite(fb, one, 0, 0, SpriteTint::dim(4));
    EXPECT_EQ(fb.at(0, 0).fg, (Rgb{127, 0, 0}));
    EXPECT_EQ(fb.at(0, 0).bg, (Rgb{0, 0, 127}));
    drawSprite(fb, one, 0, 0, SpriteTint::dim(8));
    EXPECT_EQ(fb.at(0, 0).fg, (Rgb{0, 0, 0}));
    drawSprite(fb, one, 0, 0, SpriteTint::dim(99)); // clamped
    EXPECT_EQ(fb.at(0, 0).fg, (Rgb{0, 0, 0}));
    drawSprite(fb, one, 0, 0, SpriteTint::dim(-3)); // clamped
    EXPECT_EQ(fb.at(0, 0).fg, kRed);
}

// ---- serializing -----------------------------------------------------------------------------

TEST(SpriteSerialize, RoundTripsThroughTheParser) {
    const Sprite original = make({"rb.", ".br", "rrb"});
    const auto text = serializeSprite(original, 32);
    ASSERT_TRUE(text.has_value());
    const auto parsed = parseSprite(*text);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, original);
}

TEST(SpriteSerialize, TooManyColorsFails) {
    Sprite s;
    s.width = 3;
    s.height = 1;
    s.pixels = {Rgb{1, 0, 0}, Rgb{2, 0, 0}, Rgb{3, 0, 0}};
    EXPECT_TRUE(serializeSprite(s, 3).has_value());
    const auto result = serializeSprite(s, 2);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("reduce the palette"), std::string::npos);
}
