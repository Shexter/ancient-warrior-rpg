#pragma once

#include "app/screen.hpp"
#include "app/widgets.hpp"

#include <optional>

namespace heroes::app {

// Status panel, hero sprite and Explore / Training / Mission / Town / Save / Quit. Requires a game state.
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
    std::vector<Menu::Item> menuItemsWithSettings() const; // with Settings before Quit when there are settings
    void save();
    Transition quit();

    Session& session_;
    // Before menu_: building the menu reads it. The last frame drawn was the phone layout.
    mutable bool portrait_ = false;
    Menu menu_;
    std::optional<MessageBox> message_;
    std::optional<ConfirmBox> confirm_;
    Asking asking_ = Asking::QuitUnsaved;
    // The "Next:" line, recomputed only when the state differs from the one it was computed for
    // (a goal costs several 200-fight estimates, so never per frame).
    void refreshGoal();
    std::optional<game::GameState> goalState_;
    std::string goal_;
};

} // namespace heroes::app
