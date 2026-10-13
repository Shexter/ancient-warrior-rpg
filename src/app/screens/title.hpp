#pragma once

#include "app/screen.hpp"
#include "app/widgets.hpp"

#include <optional>

namespace heroes::app {

// Banner, then New Game / Continue / Quit. Continue is disabled when no save file exists; if
// the save fails to load, a MessageBox explains why and the Title stays (the file is untouched).
// A save made with mods that aren't loaded now (missing, or another version) first asks
// whether to continue anyway.
class TitleScreen final : public Screen {
  public:
    explicit TitleScreen(Session& session);
    Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) override;
    void draw(engine::Framebuffer& frame) const override;
    // The phone layout when the terminal is narrow (see portraitOr), else 80x24.
    engine::Size canvasSize(engine::Size terminal) const override;

  private:
    Transition continueGame();
    Transition load(const std::string& missingMods);

    Session& session_;
    Menu menu_;
    std::optional<MessageBox> message_;
    std::optional<ConfirmBox> confirm_;
    std::string missingMods_; // what confirm_ is asking about
};

} // namespace heroes::app
