#pragma once

#include "game/content.hpp"
#include "game/game_state.hpp"
#include "game/rng.hpp"
#include "game/training.hpp"
#include "game/world/world_def.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <optional>
#include <variant>
#include <vector>

namespace heroes::game::world {

// What the player asks for during one tick.
struct WorldInput {
    std::optional<Dir> move{}; // a movement key is held, or was pressed or repeated this tick
    bool interact = false;   // E was pressed this tick
    bool swing = false, firebolt = false, nova = false, potion = false;
    std::optional<Tile> aim{}; // a clicked map tile: turn toward it, then swing
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
struct AbilityRefused { // not enough Fervor, or still cooling down; nothing changed
    Ability ability;
    bool operator==(const AbilityRefused&) const = default;
};
struct NovaBurst {
    Tile center;
    int radius;
    bool operator==(const NovaBurst&) const = default;
};
struct PotionDrunk {
    std::string itemId;
    bool operator==(const PotionDrunk&) const = default;
};
struct NoPotions {
    bool operator==(const NoPotions&) const = default;
};
struct MonsterDefeated {
    std::string name;
    Tile at;
    MonsterRewards rewards;
    bool operator==(const MonsterDefeated&) const = default;
};
} // namespace wev
using WorldEvent =
    std::variant<wev::HeroMoved, wev::Interact, wev::Damaged, wev::HeroFell, wev::AbilityRefused,
                 wev::NovaBurst, wev::PotionDrunk, wev::NoPotions, wev::MonsterDefeated>;

struct Bolt { // a Firebolt in flight
    Tile at;
    Dir dir;
    int tilesLeft;
    int timer; // ticks to its next tile
    bool operator==(const Bolt&) const = default;
};

struct Monster {
    enum class Mode { Wander, Chase, WindUp, Cooldown };
    std::size_t zone; // index into WorldDef::zones
    TrainingMonster stats;
    int hp;
    Tile at;
    Mode mode = Mode::Wander;
    Tile target{}; // WindUp: the tile its hit lands on
    int timer = 0; // ticks to the next step (Wander, Chase), the hit (WindUp), or Cooldown's end
    int id = 0;    // a serial number given at spawn, never reused
    // For drawing only: the tile its last step left, and how long that step lasts and has run.
    Tile from{};
    int stepTicks = 0, sinceStep = 0;
    bool operator==(const Monster&) const = default;
};

// Where a thing is drawn between two tiles: `from` moving to `to`, `progress` (0 to 1) of the
// way, gaining `perTick` each tick until it arrives.
struct Motion {
    Tile from, to;
    float progress, perTick;
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
    const std::vector<Bolt>& bolts() const { return bolts_; }
    Motion heroMotion() const;
    static Motion motionOf(const Monster& m);
    Motion motionOf(const Bolt& b) const; // toward the tile it reaches next
    int cooldownLeft(Ability a) const { return cooldowns_[static_cast<std::size_t>(a)]; } // ticks
    bool fallen() const { return fallen_; }
    // The landmark on or next to the hero (diagonals count), the closest first; ties go to the
    // one listed first.
    std::optional<std::size_t> nearbyLandmark() const;

  private:
    bool occupied(Tile t) const; // by the hero or a monster
    bool inZone(std::size_t zone, Tile t) const;
    void moveHero(const WorldInput& input, std::vector<WorldEvent>& events);
    void useAbilities(const WorldInput& input, std::vector<WorldEvent>& events);
    bool spend(Ability a, std::vector<WorldEvent>& events); // Fervor and cooldown, or refuse
    int power(Ability a) const;
    void drinkPotion(std::vector<WorldEvent>& events);
    void advanceBolts(std::vector<WorldEvent>& events);
    // Hits monsters_[index]; a monster brought to 0 HP is removed and pays out.
    void hitMonster(std::size_t index, int power, std::vector<WorldEvent>& events);
    std::optional<std::size_t> monsterAt(Tile t) const;
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
    Tile hero_, heroFrom_;
    Dir facing_ = Dir::Down;
    int moveTicksLeft_ = 0; // until the current tile's move finishes and another may start
    int lifeTicks_ = 0, fervorTicks_ = 0;
    bool fallen_ = false;
    std::vector<Monster> monsters_;
    int nextMonsterId_ = 0;
    std::vector<Bolt> bolts_;
    std::array<int, kAbilityCount> cooldowns_{};
    std::vector<int> respawnTicks_; // per zone, counting down while the zone is below its count
    // Walking distance from the hero over a (4r+1)^2 window centred on `distanceOrigin_`.
    std::vector<int> distances_;
    std::vector<Tile> queue_, candidates_; // reused buffers
    std::optional<Tile> distanceOrigin_;
};

} // namespace heroes::game::world
