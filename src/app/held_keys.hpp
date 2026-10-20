#pragma once

#include "engine/input.hpp"
#include "game/world/world_def.hpp"

#include <optional>
#include <span>
#include <vector>

namespace heroes::app {

// Which movement direction the player is asking for, from the arrows and W/A/S/D. Until the
// first Release arrives the terminal is assumed not to report them, and only a direction pressed
// or repeated in this tick's keys counts. After that a direction is held from its press to its
// release, and the most recently pressed held one wins.
class HeldKeys {
  public:
    void update(std::span<const engine::KeyEvent> keys); // once per tick
    std::optional<game::world::Dir> direction() const;

  private:
    bool releases_ = false;                // the terminal has reported a release
    std::vector<game::world::Dir> held_;   // in press order, most recent last
    std::optional<game::world::Dir> tick_; // the last direction pressed or repeated this tick
};

} // namespace heroes::app
