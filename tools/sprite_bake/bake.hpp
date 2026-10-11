#pragma once

#include "engine/sprite.hpp"

#include <expected>
#include <filesystem>
#include <string>

namespace heroes::bake {

// Decodes a PNG. Pixels with alpha < 128 are transparent; the rest are opaque RGB.
std::expected<engine::Sprite, std::string> loadPng(const std::filesystem::path& file);

// Converts `png` to a `.sprite` file. Fails, writing nothing, if the PNG is unreadable, larger
// than engine::kMaxSpriteSize, or has more than `maxColors` distinct opaque colors.
std::expected<void, std::string> bake(const std::filesystem::path& png,
                                      const std::filesystem::path& out, int maxColors);

} // namespace heroes::bake
