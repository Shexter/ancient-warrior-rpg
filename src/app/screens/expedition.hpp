#pragma once

#include "app/screen.hpp"
#include "app/widgets.hpp"
#include "game/dungeon/expedition.hpp"

#include <optional>

namespace heroes::app {

// The Catacombs expedition (reference §7): an 80x19 window on the floor following the hero, a
// status row and the last message. Arrows or WASD move, Space waits, Enter takes the stairs, Esc
// opens a menu (Resume, Save, Inventory). An encounter opens the Battle screen; the outcome comes
// back here, or to the Hub when the hero falls or leaves by the stairs up. The run is kept in
// the game state after every turn, so saving and Continue resume it.
class ExpeditionScreen final : public Screen {
  public:
    // Resumes the saved expedition when there is one that fits the dungeon, else starts a new one.
    // Preconditions: the hero is alive and session.dungeon is set.
    explicit ExpeditionScreen(Session& session);
    Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) override;
    void draw(engine::Framebuffer& frame) const override;

  private:
    void act(game::dungeon::ExpAction action);
    void keep(); // copies the run into the game state, unsaved
    void end(std::string message); // the run is over: back to the Hub with `message`

    Session& session_;
    game::dungeon::Expedition exp_;
    std::optional<Menu> menu_;          // Esc menu, while open
    std::optional<MessageBox> box_;     // the bag, or what saving said
    std::optional<int> encounter_;      // an enemy to fight, opened on the next update
    bool over_ = false;
};

} // namespace heroes::app
