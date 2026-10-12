#include "bake.hpp"

#include <charconv>
#include <iostream>
#include <string_view>

namespace {

int usage() {
    std::cerr << "usage: sprite_bake <in.png> <out.sprite|out.glyphs> [--glyphs] [--max-colors N]\n";
    return 2;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3)
        return usage();
    int maxColors = 32;
    auto format = heroes::bake::Format::HalfBlock;
    for (int i = 3; i < argc; ++i) {
        const std::string_view flag = argv[i];
        if (flag == "--glyphs") {
            format = heroes::bake::Format::Glyphs;
        } else if (flag == "--max-colors" && i + 1 < argc) {
            const std::string_view value = argv[++i];
            const auto [end, ec] =
                std::from_chars(value.data(), value.data() + value.size(), maxColors);
            if (ec != std::errc{} || end != value.data() + value.size() || maxColors < 1) {
                std::cerr << "sprite_bake: error: --max-colors needs a positive whole number\n";
                return 2;
            }
        } else {
            return usage();
        }
    }
    if (const auto result = heroes::bake::bake(argv[1], argv[2], maxColors, format); !result) {
        std::cerr << "sprite_bake: error: " << result.error() << '\n';
        return 1;
    }
    return 0;
}
