#pragma once

#include "game/content.hpp"
#include "game/hero.hpp"
#include "game/rng.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace heroes::game {

struct GameState {
    Hero hero;
    Rng rng;
    std::uint64_t seed;

    bool operator==(const GameState&) const = default;
};

// Preconditions: `name` passed validateHeroName and `pathId` exists in `content`.
GameState newGame(const Content& content, std::string name, std::string_view pathId,
                  std::uint64_t seed);

} // namespace heroes::game
