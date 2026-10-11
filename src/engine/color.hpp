#pragma once

#include <compare>
#include <cstdint>

namespace heroes::engine {

struct Rgb {
    std::uint8_t r, g, b;
    auto operator<=>(const Rgb&) const = default;
};

enum class ColorMode { TrueColor, Ansi256 };

// Nearest xterm-256 color-cube index (16..231) for an RGB value.
int toAnsi256(Rgb c);

} // namespace heroes::engine
