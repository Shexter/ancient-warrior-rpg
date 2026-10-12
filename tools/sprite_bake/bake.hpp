#pragma once

#include "engine/sprite.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace heroes::bake {

// Decodes a PNG. Pixels with alpha < 128 are transparent; the rest are opaque RGB.
std::expected<engine::Sprite, std::string> loadPng(const std::filesystem::path& file);

// Brightness ramp for glyph sprites, sparse to dense. The space is kept for transparent cells.
inline constexpr std::string_view kGlyphRamp = " .:-=+*#%@";

// An image W x 2H pixels as W x H glyph cells. Each cell covers one pixel column and two pixel
// rows: transparent if both pixels are. Otherwise its brightness is the average Rec. 709
// luminance of its opaque pixels, stretched across the image's own darkest-to-brightest range and
// lifted by a square root (so dark-colored areas still get visible glyphs); that picks the ramp
// character. Its color is the top pixel's (or the bottom's when the top is transparent), so the
// result uses only the image's own colors.
engine::GlyphSprite toGlyphs(const engine::Sprite& image);

enum class Format { HalfBlock, Glyphs };

// Converts `png` to a `.sprite` (half-block) or `.glyphs` file. Fails, writing nothing, if the
// PNG is unreadable, larger than engine::kMaxSpriteSize, or needs more than `maxColors` colors.
std::expected<void, std::string> bake(const std::filesystem::path& png,
                                      const std::filesystem::path& out, int maxColors,
                                      Format format = Format::HalfBlock);

} // namespace heroes::bake
