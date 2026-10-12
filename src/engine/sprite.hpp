#pragma once

#include "engine/color.hpp"
#include "engine/framebuffer.hpp"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace heroes::engine {

struct Sprite {
    int width = 0, height = 0;
    std::vector<std::optional<Rgb>> pixels; // row-major; nullopt = transparent

    const std::optional<Rgb>& at(int x, int y) const {
        return pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                      static_cast<std::size_t>(x)];
    }
    bool operator==(const Sprite&) const = default;
};

inline constexpr int kMaxSpriteSize = 512;
inline constexpr int kMaxSpritePaletteSize = 62; // keys are [A-Za-z0-9]

// Parses the `.sprite` text format. Errors name the 1-based line: "line 7: unknown palette key
// 'z'".
std::expected<Sprite, std::string> parseSprite(std::string_view text);

// Reads and parses a file; the error is prefixed with the path.
std::expected<Sprite, std::string> loadSprite(const std::filesystem::path& file);

// Writes the `.sprite` text format, assigning palette keys in order of first appearance.
// Fails if the sprite uses more than `maxColors` distinct colors.
std::expected<std::string, std::string> serializeSprite(const Sprite& sprite, int maxColors);

struct SpriteTint {
    enum class Kind { None, Flash, Dim };
    Kind kind = Kind::None;
    int level = 0; // Dim only: 0 = unchanged .. 8 = black

    static SpriteTint none() { return {}; }
    static SpriteTint flash() { return {Kind::Flash, 0}; }
    static SpriteTint dim(int level) { return {Kind::Dim, level}; }
};

// Draws two pixel rows per cell row, starting at cell (cellX, cellY). Clips to the framebuffer.
void drawSprite(Framebuffer& frame, const Sprite& sprite, int cellX, int cellY,
                SpriteTint tint = {});

// A sprite of whole cells: each is one printable ASCII character in one color, or transparent.
struct GlyphCell {
    char ch;
    Rgb color;
    bool operator==(const GlyphCell&) const = default;
};
struct GlyphSprite {
    int width = 0, height = 0; // in cells
    std::vector<std::optional<GlyphCell>> cells; // row-major; nullopt = transparent

    const std::optional<GlyphCell>& at(int x, int y) const {
        return cells[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                     static_cast<std::size_t>(x)];
    }
    bool operator==(const GlyphSprite&) const = default;
};

// Parses the `.glyphs` text format:
//   glyphs <w> <h>
//   palette          <key> #rrggbb lines, as in .sprite
//   chars            h rows of |w characters|, printable ASCII; space = transparent
//   colors           h rows of w palette keys; '.' = transparent
// A cell must be transparent in both grids or in neither. Errors name the 1-based line.
std::expected<GlyphSprite, std::string> parseGlyphs(std::string_view text);
std::expected<GlyphSprite, std::string> loadGlyphs(const std::filesystem::path& file);
// Writes the `.glyphs` format; fails if it needs more than `maxColors` colors.
std::expected<std::string, std::string> serializeGlyphs(const GlyphSprite& sprite, int maxColors);

// Draws one cell per glyph cell from (cellX, cellY), clipping to the framebuffer. An opaque cell
// sets the character and its (tinted) foreground and keeps the background; a transparent cell is
// untouched.
void drawGlyphs(Framebuffer& frame, const GlyphSprite& sprite, int cellX, int cellY,
                SpriteTint tint = {});

} // namespace heroes::engine
