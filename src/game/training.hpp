#pragma once

#include "game/battle.hpp"
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
    bool operator==(const TrainingMonster&) const = default;
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
    std::int64_t potGained = 0; // TrainingSession only
};

// Draw order: atk, def, gold. `caveLevel` is 1..30.
TrainingMonster rollTrainingMonster(Rng& rng, int caveLevel);

// Always draws twice when any item can drop, never when none can.
std::optional<std::string> rollDrop(Rng& rng, const Content& content, int caveLevel);

// What beating a training-roll monster gave the hero.
struct MonsterRewards {
    std::int64_t gold = 0, exp = 0;
    int levelsGained = 0;
    std::optional<std::string> drop;
    bool dropLostBagFull = false;
    bool operator==(const MonsterRewards&) const = default;
};
// Applies a win over `monster` at `level`: its gold, its EXP (through grantExp), then a drop
// roll; a drop that doesn't fit the bag is lost. Shared by training rounds and the field.
MonsterRewards grantMonsterRewards(Hero& hero, const Content& content, Rng& rng,
                                   const TrainingMonster& monster, int level);
// A loss to `monster`: the hero gives up its EXP reward, never going below 0.
void loseToMonster(Hero& hero, const TrainingMonster& monster);

// The hero (a copy, e.g. from heroCombatant) trades hits with the monster until one falls or the
// exchange cap is hit, appending the events. Shared by training and its win-chance estimate.
Outcome fightTrainingMonster(Combatant& hero, const TrainingMonster& monster,
                             std::vector<Event>& events);

// Push-your-luck training: one round at a time. Each win adds
// `round gold x min(streak, 10) / 10` to a pot (streak = wins earlier in the session; a draw
// resets it). leave() banks the pot; a defeat forfeits it and ends the session. The game's
// random stream is forked once, at start.
class TrainingSession {
  public:
    static constexpr int kMaxCaveLevel = 30;

    // Refused, changing nothing, for a dead or unequipped hero or a cave outside 1..30.
    static std::expected<TrainingSession, TownError> start(GameState& state, const Content& content,
                                                           int caveLevel);

    // Fights and applies one round. Precondition: !over().
    TrainingRound next();
    // Adds the pot to the hero's gold and ends the session. Returns the amount banked.
    std::int64_t leave();

    bool over() const { return over_; }
    int caveLevel() const { return caveLevel_; }
    int streak() const { return streak_; }
    std::int64_t pot() const { return pot_; }

  private:
    TrainingSession(GameState& state, const Content& content, int caveLevel);

    GameState* state_;
    const Content* content_;
    int caveLevel_;
    Rng rng_;
    int streak_ = 0;
    std::int64_t pot_ = 0;
    bool over_ = false;
};

// Runs up to `rounds` fights; a defeat stops the remaining rounds. On error nothing changes.
std::expected<std::vector<TrainingRound>, TownError>
runTraining(GameState& state, const Content& content, int caveLevel, int rounds);

} // namespace heroes::game
