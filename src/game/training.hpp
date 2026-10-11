#pragma once

#include "game/content.hpp"
#include "game/events.hpp"
#include "game/game_state.hpp"
#include "game/rng.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace heroes::game {

struct TrainingMonster {
    std::string name;
    int maxHp, atk, def;
    std::int64_t goldReward, expReward;
};

struct TrainingRound {
    std::string monsterName;
    int caveLevel = 0;
    int monsterMaxHp = 0;
    Outcome outcome = Outcome::Draw;
    std::vector<Event> events;
    std::int64_t goldGained = 0, expGained = 0;
    int levelsGained = 0;
    std::optional<std::string> drop;
    bool dropLostBagFull = false;
};

// Draw order: atk, def, gold. `caveLevel` is 1..30.
TrainingMonster rollTrainingMonster(Rng& rng, int caveLevel);

// Always draws twice when any item can drop, never when none can.
std::optional<std::string> rollDrop(Rng& rng, const Content& content, int caveLevel);

// Runs up to `rounds` fights; a defeat stops the remaining rounds. On error nothing changes.
std::expected<std::vector<TrainingRound>, TownError>
runTraining(GameState& state, const Content& content, int caveLevel, int rounds);

} // namespace heroes::game
