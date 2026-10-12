#pragma once

#include "game/content.hpp"
#include "game/game_state.hpp"

namespace heroes::game {

// Fights simulated per estimate.
inline constexpr int kEstimateFights = 200;

// The hero's chance, in whole percent, of winning the next training round at `caveLevel`
// (1..30) with their current HP. Simulates on copies with streams derived from the game's seed
// and the cave, so it is deterministic for a given state and never touches the state or its RNG.
int caveWinChance(const GameState& state, const Content& content, int caveLevel);

} // namespace heroes::game
