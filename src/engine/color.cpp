#include "engine/color.hpp"

namespace heroes::engine {

namespace {

int cubeLevel(std::uint8_t v) { return v < 48 ? 0 : v < 115 ? 1 : (v - 35) / 40; }

} // namespace

int toAnsi256(Rgb c) { return 16 + 36 * cubeLevel(c.r) + 6 * cubeLevel(c.g) + cubeLevel(c.b); }

} // namespace heroes::engine
