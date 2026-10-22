#pragma once

#include "game/content.hpp"
#include "game/hero.hpp"
#include "game/rng.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace heroes::game {

// A dungeon expedition in progress, as a save holds it: the floor is rebuilt from (seed, floor)
// and these changes applied (game/dungeon/expedition.hpp has the live form).
struct ExpeditionSave {
    struct Enemy {
        std::string id;
        int x, y;
        bool awake;
        int sleepTurns;
        bool elite;
        bool operator==(const Enemy&) const = default;
    };
    std::uint64_t seed;
    int floor;
    int heroX, heroY;
    Rng::State rng;
    std::vector<bool> explored; // per tile, row-major
    std::vector<Enemy> enemies;
    std::vector<bool> chestsOpened; // by the floor's chest order
    bool operator==(const ExpeditionSave&) const = default;
};

struct GameState {
    Hero hero;
    Rng rng;
    std::uint64_t seed;
    std::optional<ExpeditionSave> expedition = std::nullopt;

    bool operator==(const GameState&) const = default;
};

// Preconditions: `name` passed validateHeroName and `pathId` exists in `content`.
GameState newGame(const Content& content, std::string name, std::string_view pathId,
                  std::uint64_t seed);

} // namespace heroes::game
