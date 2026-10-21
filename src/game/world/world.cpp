#include "game/world/world.hpp"

#include <cstdlib>

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
    if (input.interact)
        if (const auto landmark = nearbyLandmark())
            events.push_back(wev::Interact{*landmark});
    return events;
}

std::optional<std::size_t> World::nearbyLandmark() const {
    std::optional<std::size_t> best;
    int bestDistance = 0;
    for (std::size_t i = 0; i < def_->landmarks.size(); ++i) {
        const Tile at = def_->landmarks[i].at;
        const int dx = std::abs(at.x - hero_.x), dy = std::abs(at.y - hero_.y);
        if (dx > 1 || dy > 1)
            continue;
        if (!best || dx + dy < bestDistance) {
            best = i;
            bestDistance = dx + dy;
        }
    }
    return best;
}

} // namespace heroes::game::world
