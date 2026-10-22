#pragma once

#include "game/dungeon/floor.hpp"

#include <vector>

namespace heroes::game::dungeon {

// Integer division rounding toward minus infinity (C++ `/` rounds toward zero). Precondition: b > 0.
constexpr int floorDiv(int a, int b) { return a / b - (a % b < 0 ? 1 : 0); }

// What the hero at `origin` sees within `radius` (dx² + dy² ≤ radius²): symmetric shadowcasting
// (Albert Ford, 2020) in four quadrants, integers only. Walls are seen but block; off the map
// counts as wall. Clears `visible`, then sets it per tile, row-major.
void computeFov(const Floor& floor, Pos origin, int radius, std::vector<bool>& visible);

} // namespace heroes::game::dungeon
