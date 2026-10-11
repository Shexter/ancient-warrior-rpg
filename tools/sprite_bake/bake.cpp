#include "bake.hpp"

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

std::expected<void, std::string> bake(const std::filesystem::path& png,
                                      const std::filesystem::path& out, int maxColors) {
    auto sprite = loadPng(png);
    if (!sprite)
        return std::unexpected(sprite.error());
    auto text = engine::serializeSprite(*sprite, maxColors);
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
