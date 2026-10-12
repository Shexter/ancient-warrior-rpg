#pragma once

#include "game/battle.hpp"
#include "game/content.hpp"

#include <set>
#include <string>

namespace heroes::game {

// The `smart` policy (balance-simulator spec): what a sensible player does on the hero's turn.
// The simulator and the game's win-chance estimates share it, so the odds the player sees and the
// balance numbers come from the same play. In order:
// 1. Defend when the enemy is Charging (a telegraphed strike is coming).
// 2. Below 30% HP, heal: Angel Wings if affordable, else the best healing item in stock.
// 3. The strongest affordable, ready damage skill.
// 4. Attack.
PlayerAction smartAction(const Content& content, const Battle& battle,
                         const std::set<std::string>& learned);

} // namespace heroes::game
