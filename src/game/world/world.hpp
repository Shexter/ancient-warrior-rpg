#pragma once

#include "game/content.hpp"
#include "game/game_state.hpp"
#include "game/rng.hpp"
#include "game/training.hpp"
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
struct Damaged { // a hit landed on the tile, for the number shown over it
    Tile at;
    int amount;
    bool onHero;
    bool operator==(const Damaged&) const = default;
};
struct HeroFell { // HP reached 0; the world does nothing more
    bool operator==(const HeroFell&) const = default;
};
} // namespace wev
using WorldEvent = std::variant<wev::HeroMoved, wev::Interact, wev::Damaged, wev::HeroFell>;

struct Monster {
    enum class Mode { Wander, Chase, WindUp, Cooldown };
    std::size_t zone; // index into WorldDef::zones
    TrainingMonster stats;
    int hp;
    Tile at;
    Mode mode = Mode::Wander;
    Tile target{}; // WindUp: the tile its hit lands on
    int timer = 0; // ticks to the next step (Wander, Chase), the hit (WindUp), or Cooldown's end
    bool operator==(const Monster&) const = default;
};

// The Overworld simulation: inputs in, events out, one fixed 1/60 s tick per step(). No I/O.
class World {
  public:
    // Starts the hero on the spawn tile facing down, forks the game's random stream once, and
    // places each zone's monsters.
    World(const WorldDef& def, GameState& state, const Content& content);

    std::vector<WorldEvent> step(const WorldInput& input);

    Tile hero() const { return hero_; }
    Dir facing() const { return facing_; }
    const std::vector<Monster>& monsters() const { return monsters_; }
    bool fallen() const { return fallen_; }
    // The landmark on or next to the hero (diagonals count), the closest first; ties go to the
    // one listed first.
    std::optional<std::size_t> nearbyLandmark() const;

  private:
    bool occupied(Tile t) const; // by the hero or a monster
    bool inZone(std::size_t zone, Tile t) const;
    void moveHero(const WorldInput& input, std::vector<WorldEvent>& events);
    void regenerate();
    void updateMonster(Monster& m, std::vector<WorldEvent>& events);
    std::optional<Tile> chaseStep(const Monster& m) const;
    std::optional<Tile> wanderStep(const Monster& m);
    void hitHero(const Monster& m, std::vector<WorldEvent>& events);
    void refreshDistances(); // the BFS distance window around the hero, when the hero has moved
    int distanceAt(Tile t) const;
    bool spawnOne(std::size_t zone);
    void respawn();

    const WorldDef* def_;
    GameState* state_;
    const Content* content_;
    Rng rng_;
    Tile hero_;
    Dir facing_ = Dir::Down;
    int moveTicksLeft_ = 0; // until the current tile's move finishes and another may start
    int lifeTicks_ = 0, fervorTicks_ = 0;
    bool fallen_ = false;
    std::vector<Monster> monsters_;
    std::vector<int> respawnTicks_; // per zone, counting down while the zone is below its count
    // Walking distance from the hero over a (4r+1)^2 window centred on `distanceOrigin_`.
    std::vector<int> distances_;
    std::vector<Tile> queue_, candidates_; // reused buffers
    std::optional<Tile> distanceOrigin_;
};

} // namespace heroes::game::world
