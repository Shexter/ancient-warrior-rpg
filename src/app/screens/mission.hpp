#pragma once

#include "app/battle_view.hpp"
#include "app/screen.hpp"
#include "app/widgets.hpp"
#include "game/battle.hpp"
#include "game/town.hpp"

#include <optional>
#include <string>
#include <vector>

namespace heroes::app {

// Every mission from data plus two "Coming Soon" rows. Locked or cleared missions are disabled
// with the reason. Esc returns to the Hub.
class MissionSelectScreen final : public Screen {
  public:
    explicit MissionSelectScreen(Session& session);
    Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) override;
    void draw(engine::Framebuffer& frame) const override;

  private:
    Session& session_;
    Menu menu_;
};

// The mission's story on a typewriter, then "Enter the lair?". Yes starts the battle.
class MissionIntroScreen final : public Screen {
  public:
    MissionIntroScreen(Session& session, std::string missionId);
    Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) override;
    void draw(engine::Framebuffer& frame) const override;

  private:
    Session& session_;
    std::string missionId_;
    Typewriter story_;
    std::optional<ConfirmBox> confirm_;
    std::optional<MessageBox> message_; // a refused start
};

// Actions row (Attack, Skill, Flee) and the skill overlay. Actions reach the Battle only while
// the view is idle; Enter skips the current animation. The result is applied the moment the
// battle ends, and the Result screen follows once the last event has played.
class BattleScreen final : public Screen {
  public:
    BattleScreen(Session& session, std::string missionId, game::Battle battle);
    Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) override;
    void draw(engine::Framebuffer& frame) const override;
    // 120x40 when the terminal fits it and both fighters have large art.
    engine::Size canvasSize(engine::Size terminal) const override;

  private:
    std::string heroFile() const;
    std::string enemyFile() const;
    void act(const game::PlayerAction& action);
    std::vector<Menu::Item> skillItems() const;

    Session& session_;
    std::string missionId_;
    game::Battle battle_;
    BattleView view_;
    Menu actions_;
    std::optional<Menu> skills_; // the overlay, while open
    std::optional<game::MissionResult> result_;
};

class ResultScreen final : public Screen {
  public:
    ResultScreen(Session& session, game::MissionResult result);
    Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) override;
    void draw(engine::Framebuffer& frame) const override;

  private:
    Session& session_;
    game::MissionResult result_;
};

} // namespace heroes::app
