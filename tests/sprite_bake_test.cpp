#include "bake.hpp"

#include "engine/sprite.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <sstream>

using namespace heroes;
namespace fs = std::filesystem;

namespace {

fs::path fixture() { return fs::path(HEROES_SOURCE_DIR) / "tests" / "fixtures" / "sample.png"; }

fs::path scratch(const std::string& name) {
    const fs::path dir = fs::temp_directory_path() / "heroes_bake_test";
    fs::create_directories(dir);
    return dir / name;
}

} // namespace

TEST(SpriteBake, BakesTheSamplePngAndTheResultLoads) {
    const fs::path out = scratch("sample.sprite");
    fs::remove(out);
    ASSERT_TRUE(bake::bake(fixture(), out, 32).has_value());

    const auto sprite = engine::loadSprite(out);
    ASSERT_TRUE(sprite.has_value()) << sprite.error();
    EXPECT_EQ(sprite->width, 8);
    EXPECT_EQ(sprite->height, 8);
    EXPECT_EQ(sprite->at(0, 0), (engine::Rgb{220, 40, 40}));   // top edge red
    EXPECT_EQ(sprite->at(7, 7), (engine::Rgb{40, 80, 220}));   // bottom edge blue
    EXPECT_EQ(sprite->at(0, 3), (engine::Rgb{240, 200, 60}));  // left edge gold
    EXPECT_FALSE(sprite->at(2, 2).has_value());                // empty interior
    EXPECT_FALSE(sprite->at(3, 3).has_value());                // alpha 100 < 128: transparent
    EXPECT_EQ(sprite->at(4, 4), (engine::Rgb{255, 255, 255})); // alpha 128: opaque
}

TEST(SpriteBake, ExactColorsAreKeptWithAFivePaletteEntries) {
    const fs::path out = scratch("palette.sprite");
    ASSERT_TRUE(bake::bake(fixture(), out, 4).has_value()); // red, blue, gold, white
    std::ifstream in(out);
    std::stringstream text;
    text << in.rdbuf();
    EXPECT_NE(text.str().find("a #dc2828"), std::string::npos);
}

TEST(SpriteBake, TooManyColorsFailsWithoutWritingAFile) {
    const fs::path out = scratch("toomany.sprite");
    fs::remove(out);
    const auto result = bake::bake(fixture(), out, 3);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("reduce the palette first"), std::string::npos);
    EXPECT_FALSE(fs::exists(out));
}

TEST(SpriteBake, MissingAndNonPngInputsFail) {
    const auto missing = bake::bake("/no/such/file.png", scratch("x.sprite"), 32);
    ASSERT_FALSE(missing.has_value());
    EXPECT_NE(missing.error().find("cannot open"), std::string::npos);

    const fs::path notPng = scratch("not_a_png.png");
    std::ofstream(notPng) << "this is not an image";
    const auto bad = bake::bake(notPng, scratch("y.sprite"), 32);
    ASSERT_FALSE(bad.has_value());
    EXPECT_NE(bad.error().find("not a readable PNG"), std::string::npos);
}

TEST(SpriteBake, OversizedPngIsRejectedBeforeConversion) {
    const fs::path png = fs::path(HEROES_SOURCE_DIR) / "tests" / "fixtures" / "too_wide.png";
    const auto result = bake::loadPng(png); // 513x1, one pixel over the limit
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("larger than the 512x512 limit"), std::string::npos);
}

TEST(SpriteBake, UnwritableOutputFails) {
    const auto result = bake::bake(fixture(), "/no/such/dir/out.sprite", 32);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("cannot write"), std::string::npos);
}

TEST(ShippedSprites, AllFivePlaceholdersLoadWithTheDocumentedSizes) {
    const fs::path dir = fs::path(HEROES_SOURCE_DIR) / "assets" / "sprites";
    struct Expected {
        const char* file;
        int width, height;
    };
    for (const Expected& e :
         {Expected{"hero_fighter.sprite", 24, 24}, Expected{"hero_hunter.sprite", 24, 24},
          Expected{"hero_rogue.sprite", 24, 24}, Expected{"medusa.sprite", 32, 28},
          Expected{"avaritia.sprite", 32, 28}, Expected{"gula.sprite", 32, 28},
          Expected{"ira.sprite", 32, 28}, Expected{"monster.sprite", 16, 16}}) {
        const auto sprite = engine::loadSprite(dir / e.file);
        ASSERT_TRUE(sprite.has_value()) << sprite.error();
        EXPECT_EQ(sprite->width, e.width) << e.file;
        EXPECT_EQ(sprite->height, e.height) << e.file;
        const auto opaque =
            std::ranges::count_if(sprite->pixels, [](const auto& p) { return p.has_value(); });
        EXPECT_GT(opaque, e.width * e.height / 5) << e.file << " looks empty";
    }
}

TEST(SpriteBake, GlyphsPickTheCharacterByBrightnessAndKeepTheColor) {
    // 3x2 pixels: a near-black column, a near-white column, a transparent column.
    const fs::path png = fs::path(HEROES_SOURCE_DIR) / "tests" / "fixtures" / "glyph_ramp.png";
    const fs::path out = scratch("ramp.glyphs");
    fs::remove(out);
    ASSERT_TRUE(bake::bake(png, out, 32, bake::Format::Glyphs).has_value());
    const auto glyphs = engine::loadGlyphs(out);
    ASSERT_TRUE(glyphs.has_value()) << glyphs.error();
    EXPECT_EQ(glyphs->width, 3);
    EXPECT_EQ(glyphs->height, 1); // two pixel rows per cell
    EXPECT_EQ(glyphs->at(0, 0), (engine::GlyphCell{'.', engine::Rgb{10, 10, 10}}));    // sparse end
    EXPECT_EQ(glyphs->at(1, 0), (engine::GlyphCell{'@', engine::Rgb{250, 250, 250}})); // dense end
    EXPECT_FALSE(glyphs->at(2, 0).has_value());
}

TEST(SpriteBake, GlyphsWithTooManyColorsWriteNothing) {
    const fs::path png = fs::path(HEROES_SOURCE_DIR) / "tests" / "fixtures" / "glyph_ramp.png";
    const fs::path out = scratch("toomany.glyphs");
    fs::remove(out);
    const auto result = bake::bake(png, out, 1, bake::Format::Glyphs);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("reduce the palette first"), std::string::npos);
    EXPECT_FALSE(fs::exists(out));
}

TEST(SpriteBake, GlyphRampIsStretchedAcrossTheImage) {
    engine::Sprite image;
    image.width = 3;
    image.height = 2;
    // Black (luminance 0), mid gray (128), and dark red over blue (average about 22).
    image.pixels = {engine::Rgb{0, 0, 0},   engine::Rgb{128, 128, 128},
                    engine::Rgb{170, 0, 0}, engine::Rgb{0, 0, 0},
                    std::nullopt,           engine::Rgb{0, 0, 120}};
    const auto glyphs = bake::toGlyphs(image);
    EXPECT_EQ(glyphs.at(0, 0)->ch, bake::kGlyphRamp[1]); // the image's darkest: sparse end
    EXPECT_EQ(glyphs.at(1, 0)->ch, bake::kGlyphRamp[9]); // the image's brightest: dense end
    // Red over blue: about 0.18 of the range, lifted to 0.42 by the square root -> level 3.
    EXPECT_EQ(glyphs.at(2, 0)->ch, bake::kGlyphRamp[4]);
    EXPECT_EQ(glyphs.at(2, 0)->color, (engine::Rgb{170, 0, 0})); // the top pixel's color

    engine::Sprite flat; // one brightness: the absolute scale applies
    flat.width = 1;
    flat.height = 1;
    flat.pixels = {engine::Rgb{255, 255, 255}};
    EXPECT_EQ(bake::toGlyphs(flat).at(0, 0)->ch, bake::kGlyphRamp[9]);
}
