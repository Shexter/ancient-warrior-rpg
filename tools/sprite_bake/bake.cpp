#include "bake.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <fstream>
#include <iterator>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
// stb is third-party code: keep its warnings out of our -Werror//WX builds. GCC reports
// maybe-uninitialized from inside the header even though it is an -isystem include.
#ifdef _MSC_VER
#pragma warning(push, 0)
#elif defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#include "stb_image.h"
#ifdef _MSC_VER
#pragma warning(pop)
#elif defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace heroes::bake {

std::expected<engine::Sprite, std::string> loadPng(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in)
        return std::unexpected(std::format("{}: cannot open file", file.string()));
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    int width = 0, height = 0, channels = 0;
    unsigned char* data =
        stbi_load_from_memory(reinterpret_cast<const unsigned char*>(bytes.data()),
                              static_cast<int>(bytes.size()), &width, &height, &channels, 4);
    if (!data)
        return std::unexpected(
            std::format("{}: not a readable PNG ({})", file.string(), stbi_failure_reason()));

    if (width > engine::kMaxSpriteSize || height > engine::kMaxSpriteSize) {
        stbi_image_free(data);
        return std::unexpected(std::format("{}: image is {}x{}, larger than the {}x{} limit",
                                           file.string(), width, height, engine::kMaxSpriteSize,
                                           engine::kMaxSpriteSize));
    }

    engine::Sprite sprite;
    sprite.width = width;
    sprite.height = height;
    for (int i = 0; i < width * height; ++i) {
        const unsigned char* px = data + static_cast<std::ptrdiff_t>(i) * 4;
        if (px[3] < 128)
            sprite.pixels.emplace_back(std::nullopt);
        else
            sprite.pixels.emplace_back(engine::Rgb{px[0], px[1], px[2]});
    }
    stbi_image_free(data);
    return sprite;
}

engine::GlyphSprite toGlyphs(const engine::Sprite& image) {
    engine::GlyphSprite glyphs;
    glyphs.width = image.width;
    glyphs.height = (image.height + 1) / 2;
    for (int row = 0; row < glyphs.height; ++row) {
        for (int x = 0; x < image.width; ++x) {
            static const std::optional<engine::Rgb> kNone;
            const auto& top = image.at(x, row * 2);
            const auto& bottom = row * 2 + 1 < image.height ? image.at(x, row * 2 + 1) : kNone;
            if (!top && !bottom) {
                glyphs.cells.emplace_back(std::nullopt);
                continue;
            }
            double luminance = 0;
            int opaque = 0;
            for (const std::optional<engine::Rgb>& pixel : {top, bottom})
                if (pixel) {
                    luminance += 0.2126 * pixel->r + 0.7152 * pixel->g + 0.0722 * pixel->b;
                    ++opaque;
                }
            luminance /= opaque;
            // 1..9: never the space, which marks transparency.
            const auto level = std::min<std::size_t>(8, static_cast<std::size_t>(luminance * 9 / 256));
            glyphs.cells.emplace_back(engine::GlyphCell{kGlyphRamp[1 + level], top ? *top : *bottom});
        }
    }
    return glyphs;
}

std::expected<void, std::string> bake(const std::filesystem::path& png,
                                      const std::filesystem::path& out, int maxColors,
                                      Format format) {
    auto sprite = loadPng(png);
    if (!sprite)
        return std::unexpected(sprite.error());
    auto text = format == Format::Glyphs ? engine::serializeGlyphs(toGlyphs(*sprite), maxColors)
                                         : engine::serializeSprite(*sprite, maxColors);
    if (!text)
        return std::unexpected(std::format("{}: {}", png.string(), text.error()));
    std::ofstream file(out, std::ios::binary | std::ios::trunc);
    file << *text;
    file.flush();
    if (!file)
        return std::unexpected(std::format("{}: cannot write file", out.string()));
    return {};
}

} // namespace heroes::bake
