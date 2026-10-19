#include "game/world/world.hpp"

namespace heroes::game::world {

World::World(const WorldDef& def, GameState& state)
    : def_(&def), rng_(state.rng.fork()), hero_(def.spawn) {}

std::vector<WorldEvent> World::step(const WorldInput& input) {
    std::vector<WorldEvent> events;
    if (moveTicksLeft_ > 0)
        --moveTicksLeft_;
    if (moveTicksLeft_ == 0 && input.move) {
        facing_ = *input.move;
        const Tile to = neighbor(hero_, facing_);
        if (def_->walkable(to)) {
            events.push_back(wev::HeroMoved{hero_, to});
            hero_ = to;
            moveTicksLeft_ = def_->field.ticksPerTile;
        }
    }
    return events;
}

} // namespace heroes::game::world
