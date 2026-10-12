#include "game/estimate.hpp"

#include "game/battle.hpp"
#include "game/training.hpp"

#include <vector>

namespace heroes::game {

int caveWinChance(const GameState& state, const Content& content, int caveLevel) {
    constexpr std::uint64_t kCaveSalt = 0xCA7E'0000'0000'0000ULL;
    const Combatant hero = heroCombatant(state.hero, content);
    std::vector<Event> events; // reused; only the outcome matters
    int wins = 0;
    for (int i = 0; i < kEstimateFights; ++i) {
        Rng rng = Rng::stream(state.seed ^ (kCaveSalt + static_cast<std::uint64_t>(caveLevel)),
                              static_cast<std::uint64_t>(i));
        const TrainingMonster monster = rollTrainingMonster(rng, caveLevel);
        Combatant fighter = hero;
        events.clear();
        wins += fightTrainingMonster(fighter, monster, events) == Outcome::Victory ? 1 : 0;
    }
    return wins * 100 / kEstimateFights;
}

} // namespace heroes::game
