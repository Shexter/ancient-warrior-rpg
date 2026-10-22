#pragma once

#include "game/battle.hpp"
#include "game/content.hpp"
#include "game/dungeon/dungeon_def.hpp"
#include "game/dungeon/floor.hpp"
#include "game/errors.hpp"
#include "game/game_state.hpp"
#include "game/rng.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace heroes::game::dungeon {

// A dungeon run in progress. The floor is rebuilt from (seed, floor.index) plus what changed.
struct Expedition {
    std::uint64_t seed = 0;
    Floor floor;
    Pos hero{};
    std::vector<bool> explored, visible; // row-major, per tile of the floor
    Rng rng = Rng::stream(0, 0);          // chests and battles; floors use their own streams
    std::string message;                  // the last thing to tell the player
    bool operator==(const Expedition&) const = default;
};

enum class ExpAction { North, East, South, West, Wait, UseStairs };

namespace xev {
struct Moved {
    Pos to;
};
struct BumpedWall {};
struct EnemyWoke {
    int index;
};
struct EnemyMoved {
    int index;
    Pos to;
};
struct ChestOpened {
    std::string itemId;
    int qty;
    bool lostBagFull;
};
struct Descended {
    int floor;
};
struct Left {};
} // namespace xev
using ExpEvent = std::variant<xev::Moved, xev::BumpedWall, xev::EnemyWoke, xev::EnemyMoved,
                              xev::ChestOpened, xev::Descended, xev::Left>;

struct StepResult {
    std::vector<ExpEvent> events;
    std::optional<int> encounterEnemy; // index into floor.enemies: start a battle with it
};

// A new expedition on floor 1, at its entry, with sight computed. Its seed comes from one fork of
// the game's RNG. A fallen hero can't start (HeroDead).
std::expected<Expedition, TownError> startExpedition(GameState& state, const DungeonDef& def);

// One player action (reference §6): a move (a wall passes no turn; an enemy starts an encounter;
// a chest on the new tile opens), the stairs (down to the next floor, or out on floor 1), or a
// wait; then sight, then every enemy's turn in order.
StepResult step(Expedition& exp, GameState& state, const Content& content, const DungeonDef& def,
                ExpAction action);

// The expedition as a save holds it, and back. fromSave rebuilds the floor from (seed, floor) and
// applies the saved enemies and opened chests; none when the save doesn't fit this dungeon (a
// floor it doesn't have, a different map size, the hero or an enemy off the open tiles, or a
// different number of chests).
ExpeditionSave toSave(const Expedition& exp);
std::optional<Expedition> fromSave(const ExpeditionSave& save, const DungeonDef& def);

// The enemy at `enemyIndex` as fought on this floor (scaled, no reward items).
EnemyDef encounterEnemy(const Expedition& exp, const Content& content, const DungeonDef& def,
                        int enemyIndex);

// Applies a finished battle with that enemy. Victory removes it and grants its exp and gold;
// defeat leaves the hero at 0 HP and takes the enemy's exp from the hero (gear is kept; the
// caller ends the expedition); fleeing or a draw puts it to sleep for 3 turns. HP and MP always
// come back from the battle. Precondition: battle.over().
void resolveEncounter(Expedition& exp, GameState& state, const Content& content,
                      const DungeonDef& def, int enemyIndex, const Battle& battle);

} // namespace heroes::game::dungeon
