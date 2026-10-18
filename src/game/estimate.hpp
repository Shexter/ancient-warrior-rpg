#pragma once

#include "game/content.hpp"
#include "game/game_state.hpp"
#include "game/script/script_host.hpp"

#include <string>

namespace heroes::game {

// Fights simulated per estimate.
inline constexpr int kEstimateFights = 200;

// The hero's chance, in whole percent, of winning the next training round at `caveLevel`
// (1..30) with their current HP. Simulates on copies with streams derived from the game's seed
// and the cave, so it is deterministic for a given state and never touches the state or its RNG.
int caveWinChance(const GameState& state, const Content& content, int caveLevel);

// The same for a mission battle, played with the `smart` policy and the bag's consumables.
// `scripts` runs scripted enemies and skills (null: they fall back to basic attacks). Does not
// touch the state or its RNG. Precondition: `missionId` exists in `content`.
int missionWinChance(const GameState& state, const Content& content, std::string_view missionId,
                     script::ScriptHost* scripts);

// The Hub's "Next:" line: the most useful next step (revive, spend blessings, train for or
// attempt the first open Act I mission, the bonus mission, or "Act I is complete.").
std::string nextGoal(const GameState& state, const Content& content, script::ScriptHost* scripts);

} // namespace heroes::game
