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

} // namespace heroes::engine
