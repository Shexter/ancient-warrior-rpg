#include "engine/sprite.hpp"

#include <gtest/gtest.h>

#include <string>

using namespace heroes::engine;

namespace {

// 3x2: a '#' in red, a transparent cell, an '@' in white / a space-led row with '.' in red.
const std::string kValid = "# a comment\n"
                           "glyphs 3 2\n"
                           "palette\n"
                           "a #ff0000\n"
                           "b #ffffff\n"
                           "chars\n"
                           "|# @|\n"
                           "|  .|\n"
                           "colors\n"
                           "a.b\n"
                           "..a\n";

std::string errorOf(const std::string& text) {
    const auto parsed = parseGlyphs(text);
    return parsed ? "" : parsed.error();
}

std::string replaced(std::string text, const std::string& from, const std::string& to) {
    text.replace(text.find(from), from.size(), to);
    return text;
}

} // namespace

TEST(GlyphSprite, ParsesCellsAndTransparency) {
    const auto sprite = parseGlyphs(kValid);
    ASSERT_TRUE(sprite.has_value()) << sprite.error();
    EXPECT_EQ(sprite->width, 3);
    EXPECT_EQ(sprite->height, 2);
    EXPECT_EQ(sprite->at(0, 0), (GlyphCell{'#', Rgb{255, 0, 0}})); // '#' inside |...| is a glyph
    EXPECT_FALSE(sprite->at(1, 0).has_value());
    EXPECT_EQ(sprite->at(2, 0), (GlyphCell{'@', Rgb{255, 255, 255}}));
    EXPECT_FALSE(sprite->at(0, 1).has_value());
    EXPECT_EQ(sprite->at(2, 1), (GlyphCell{'.', Rgb{255, 0, 0}}));
}

TEST(GlyphSprite, RoundTripsThroughSerialize) {
    const auto sprite = parseGlyphs(kValid).value();
    const auto text = serializeGlyphs(sprite, 62);
    ASSERT_TRUE(text.has_value());
    EXPECT_EQ(parseGlyphs(*text).value(), sprite);
    EXPECT_FALSE(serializeGlyphs(sprite, 1).has_value()); // two colors don't fit in one
}

TEST(GlyphSprite, ErrorsNameTheLine) {
    EXPECT_EQ(errorOf(replaced(kValid, "|  .|", "|  |")), "line 8: row has 2 characters, expected 3");
    EXPECT_EQ(errorOf(replaced(kValid, "..a\n", "..z\n")), "line 11: unknown palette key 'z'");
    EXPECT_EQ(errorOf(replaced(kValid, "a.b\n", "aab\n")),
              "line 10: column 2: a cell must be transparent ('.' and a space) in both grids or "
              "in neither");
    EXPECT_EQ(errorOf(replaced(kValid, "|# @|", "|# \xC3\xA9|")),
              "line 7: chars must be printable ASCII (0x20-0x7E)");
    EXPECT_EQ(errorOf(replaced(kValid, "|  .|\n", "")), "line 8: expected 2 char rows, found 1");
    EXPECT_EQ(errorOf(replaced(kValid, "|# @|", "|# @")), "line 7: char row must end with '|'");
}

TEST(GlyphSprite, DrawingKeepsTransparentCellsAndBackgrounds) {
    const auto sprite = parseGlyphs(kValid).value();
    Framebuffer frame(5, 3);
    frame.fillRect(0, 0, 5, 3, Cell{U'x', Rgb{1, 1, 1}, Rgb{9, 9, 9}});
    drawGlyphs(frame, sprite, 1, 1);
    EXPECT_EQ(frame.at(1, 1).glyph, U'#');
    EXPECT_EQ(frame.at(1, 1).fg, (Rgb{255, 0, 0}));
    EXPECT_EQ(frame.at(1, 1).bg, (Rgb{9, 9, 9})); // background kept
    EXPECT_EQ(frame.at(2, 1), (Cell{U'x', Rgb{1, 1, 1}, Rgb{9, 9, 9}})); // transparent: untouched
    drawGlyphs(frame, sprite, 4, 2); // clipped at the edges without crashing
    EXPECT_EQ(frame.at(4, 2).glyph, U'#');
}

TEST(GlyphSprite, TintsChangeOnlyTheForeground) {
    const auto sprite = parseGlyphs(kValid).value();
    Framebuffer frame(3, 2);
    frame.fillRect(0, 0, 3, 2, Cell{U' ', Rgb{}, Rgb{9, 9, 9}});
    drawGlyphs(frame, sprite, 0, 0, SpriteTint::flash());
    EXPECT_EQ(frame.at(0, 0).fg, (Rgb{255, 255, 255}));
    EXPECT_EQ(frame.at(0, 0).glyph, U'#');
    EXPECT_EQ(frame.at(0, 0).bg, (Rgb{9, 9, 9}));
    drawGlyphs(frame, sprite, 0, 0, SpriteTint::dim(4));
    EXPECT_EQ(frame.at(0, 0).fg, (Rgb{127, 0, 0}));
    EXPECT_EQ(frame.at(0, 0).bg, (Rgb{9, 9, 9}));
}
