#pragma once

#include "app/screen.hpp"
#include "app/widgets.hpp"

namespace heroes::app {

// Banner, then New Game / Continue / Quit. Continue is disabled when no save file exists.
class TitleScreen final : public Screen {
  public:
    explicit TitleScreen(Session& session);
    Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) override;
    void draw(engine::Framebuffer& frame) const override;

  private:
    Session& session_;
    Menu menu_;
};

} // namespace heroes::app
