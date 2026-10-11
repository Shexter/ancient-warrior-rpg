#include "bake.hpp"

#include <charconv>
#include <cstring>
#include <iostream>
#include <string_view>

namespace {

int usage() {
    std::cerr << "usage: sprite_bake <in.png> <out.sprite> [--max-colors N]\n";
    return 2;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3 && argc != 5)
        return usage();
    int maxColors = 32;
    if (argc == 5) {
        if (std::strcmp(argv[3], "--max-colors") != 0)
            return usage();
        const std::string_view value = argv[4];
        const auto [end, ec] =
            std::from_chars(value.data(), value.data() + value.size(), maxColors);
        if (ec != std::errc{} || end != value.data() + value.size() || maxColors < 1) {
            std::cerr << "sprite_bake: error: --max-colors needs a positive whole number\n";
            return 2;
        }
    }
    if (const auto result = heroes::bake::bake(argv[1], argv[2], maxColors); !result) {
        std::cerr << "sprite_bake: error: " << result.error() << '\n';
        return 1;
    }
    return 0;
}
