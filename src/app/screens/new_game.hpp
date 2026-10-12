#pragma once

#include "app/screen.hpp"
#include "app/widgets.hpp"

#include <string>

namespace heroes::app {

// The opening story on a typewriter, then Create Hero. Esc goes back to the Title.
class IntroScreen final : public Screen {
  public:
    explicit IntroScreen(Session& session);
    Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) override;
    void draw(engine::Framebuffer& frame) const override;

  private:
    Session& session_;
    Typewriter story_;
};

// Name, then the path-intro story, then the path menu with a preview. Creates the game and goes
// to the Hub. Esc on the name goes back to the Title; Esc later goes back to the name.
class CreateHeroScreen final : public Screen {
  public:
    explicit CreateHeroScreen(Session& session);
    Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) override;
    void draw(engine::Framebuffer& frame) const override;
    // 120x40 on the path step when the terminal fits it and every path has large art.
    engine::Size canvasSize(engine::Size terminal) const override;

  private:
    enum class Step { Name, Story, Path };

    Transition create(int pathIndex);
    void drawPathPreview(engine::Framebuffer& frame) const;

    Session& session_;
    Step step_ = Step::Name;
    TextInput nameInput_{8};
    std::string nameError_;
    std::string name_;
    Typewriter story_;
    Menu paths_;
};

} // namespace heroes::app
