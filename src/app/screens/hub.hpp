#pragma once

#include "app/screen.hpp"
#include "app/widgets.hpp"

#include <optional>

namespace heroes::app {

// Status panel, hero sprite and Training / Mission / Town / Save / Quit. Requires a game state.
class HubScreen final : public Screen {
  public:
    explicit HubScreen(Session& session);
    Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) override;
    void draw(engine::Framebuffer& frame) const override;
    // The phone layout when it applies, else 120x40 when the terminal fits it and the hero has
    // large art.
    engine::Size canvasSize(engine::Size terminal) const override;

  private:
    void drawPortrait(engine::Framebuffer& frame) const;

    enum class Asking { ReplaceSave, QuitUnsaved };

    std::vector<Menu::Item> menuItems() const;
    void save();
    Transition quit();

    Session& session_;
    Menu menu_;
    std::optional<MessageBox> message_;
    std::optional<ConfirmBox> confirm_;
    Asking asking_ = Asking::QuitUnsaved;
};

} // namespace heroes::app
