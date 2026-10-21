#pragma once

#include "game/game_state.hpp"
#include "game/rng.hpp"
#include "game/world/world_def.hpp"

#include <cstddef>
#include <optional>
#include <variant>
#include <vector>

namespace heroes::game::world {

// What the player asks for during one tick.
struct WorldInput {
    std::optional<Dir> move; // a movement key is held, or was pressed or repeated this tick
    bool interact = false;   // E was pressed this tick
};

namespace wev {
struct HeroMoved {
    Tile from, to;
    bool operator==(const HeroMoved&) const = default;
};
struct Interact { // E next to a landmark
    std::size_t landmark; // index into WorldDef::landmarks
    bool operator==(const Interact&) const = default;
};
} // namespace wev
using WorldEvent = std::variant<wev::HeroMoved, wev::Interact>;

// The Overworld simulation: inputs in, events out, one fixed 1/60 s tick per step(). No I/O.
class World {
  public:
    // Starts the hero on the spawn tile facing down, and forks the game's random stream once.
    World(const WorldDef& def, GameState& state);

    std::vector<WorldEvent> step(const WorldInput& input);

    Tile hero() const { return hero_; }
    Dir facing() const { return facing_; }
    // The landmark on or next to the hero (diagonals count), the closest first; ties go to the
    // one listed first.
    std::optional<std::size_t> nearbyLandmark() const;

  private:
    const WorldDef* def_;
    Rng rng_;
    Tile hero_;
    Dir facing_ = Dir::Down;
    int moveTicksLeft_ = 0; // until the current tile's move finishes and another may start
};

} // namespace heroes::game::world
