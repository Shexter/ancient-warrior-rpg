#pragma once

#include "app/screen.hpp"
#include "app/widgets.hpp"

#include <optional>

namespace heroes::app {

// Banner, then New Game / Continue / Quit. Continue is disabled when no save file exists; if
// the save fails to load, a MessageBox explains why and the Title stays (the file is untouched).
class TitleScreen final : public Screen {
  public:
    explicit TitleScreen(Session& session);
    Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) override;
    void draw(engine::Framebuffer& frame) const override;

  private:
    Transition continueGame();

    Session& session_;
    Menu menu_;
    std::optional<MessageBox> message_;
};

} // namespace heroes::app
