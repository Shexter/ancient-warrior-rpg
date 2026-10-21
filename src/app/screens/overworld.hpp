#pragma once

#include "app/held_keys.hpp"
#include "app/screen.hpp"
#include "game/world/world.hpp"

#include <optional>

namespace heroes::app {

// The real-time world map: the hero walks with the arrows or W/A/S/D (held keys keep walking),
// and Esc returns to the Hub. Requires a game state and a loaded world.
class OverworldScreen final : public Screen {
  public:
    explicit OverworldScreen(Session& session);
    Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) override;
    void draw(engine::Framebuffer& frame) const override;
    // The whole terminal from 80x24 up to 200x60 (80x24 under --layout compact).
    engine::Size canvasSize(engine::Size terminal) const override;
    bool wantsKeyActions() const override { return true; }

    const game::world::World& world() const { return world_; }

  private:
    Session& session_;
    game::world::World world_;
    HeldKeys held_;
    // The map tile drawn at the canvas's top-left. It follows the hero as frames are drawn, since
    // only drawing knows the canvas size; unset until the first frame centres it on the hero.
    mutable std::optional<game::world::Tile> camera_;
};

} // namespace heroes::app
