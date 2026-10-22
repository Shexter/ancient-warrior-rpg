#pragma once

#include "app/screen.hpp"
#include "app/widgets.hpp"

namespace heroes::app {

// "Quit without saving?" over a picture of the screen it interrupted (the window's close
// button with unsaved changes). Yes quits the game; No, or Esc, returns to that screen.
class QuitPromptScreen final : public Screen {
  public:
    explicit QuitPromptScreen(engine::Framebuffer behind);
    Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) override;
    void draw(engine::Framebuffer& frame) const override;
    engine::Size canvasSize(engine::Size terminal) const override;

  private:
    engine::Framebuffer behind_;
    ConfirmBox confirm_{"Quit without saving?"};
};

} // namespace heroes::app
