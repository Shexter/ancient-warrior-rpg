#pragma once

#include "app/battle_view.hpp"
#include "app/narration_panel.hpp"
#include "app/screen.hpp"
#include "app/widgets.hpp"
#include "game/battle.hpp"
#include "game/narration/summary.hpp"
#include "game/town.hpp"

#include <functional>
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
    // The phone layout when the terminal is narrow (see portraitOr), else 80x24.
    engine::Size canvasSize(engine::Size terminal) const override;

  private:
    Session& session_;
    Menu menu_;
};

// The mission's story on a typewriter, then "Enter the lair?". Yes starts the battle; No or Esc
// goes back.
class MissionIntroScreen final : public Screen {
  public:
    // `fromWorld`: opened from a lair on the Overworld, so backing out returns there instead of
    // to Mission Select.
    MissionIntroScreen(Session& session, std::string missionId, bool fromWorld = false);
    Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) override;
    void draw(engine::Framebuffer& frame) const override;
    // The phone layout when the terminal is narrow (see portraitOr), else 80x24.
    engine::Size canvasSize(engine::Size terminal) const override;

  private:
    Transition back() const;

    Session& session_;
    std::string missionId_;
    bool fromWorld_;
    Typewriter story_;
    std::optional<ConfirmBox> confirm_;
    std::optional<MessageBox> message_; // a refused start
};

// Actions row (Attack, Skill, Item, Defend, Flee) and the Skill and Item overlays. Actions reach the Battle only while
// the view is idle; Enter skips the current animation. The result is applied the moment the
// battle ends, and the Result screen follows once the last event has played.
class BattleScreen final : public Screen {
  public:
    BattleScreen(Session& session, std::string missionId, game::Battle battle);
    // A battle outside a mission, titled `location`, the enemy drawn from `enemySprite`: `onOver`
    // applies the outcome as soon as the battle ends, and the screen pops back once the last
    // events have played.
    BattleScreen(Session& session, game::Battle battle, std::string location, std::string enemySprite,
                 std::function<void(const game::Battle&)> onOver);
    Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) override;
    void draw(engine::Framebuffer& frame) const override;
    // 120x40 when the terminal fits it and both fighters have large art.
    engine::Size canvasSize(engine::Size terminal) const override;

  private:
    void drawPortrait(engine::Framebuffer& frame) const;
    std::string heroFile() const;
    std::string enemyFile() const;
    // A boxed list over the battle: each row, when chosen, performs its action.
    struct Overlay {
        std::string title;
        Menu menu;
        std::vector<game::PlayerAction> actions;
    };

    void act(const game::PlayerAction& action);
    // Starts narrating (with the boss's introduction for a mission); nothing without a narrator.
    void startNarration(bool bossIntro);
    Overlay skillOverlay() const;
    std::optional<Overlay> itemOverlay() const; // none when the stock is empty

    Session& session_;
    std::string missionId_;
    std::string location_, enemySprite_;
    std::function<void(const game::Battle&)> onOver_; // set instead of a mission
    game::Battle battle_;
    BattleView view_;
    Menu actions_;
    std::optional<Overlay> overlay_; // while open
    std::optional<game::MissionResult> result_;
    // Narration: built-in lines for the battle's moments, shown once the events before them play.
    std::optional<game::narration::BattleContext> narrationCtx_;
    game::Rng narrationRng_ = game::Rng::stream(0, 0xA11CE);
    std::optional<std::string> narrationNext_;
    NarrationPanel narration_{78};
    double endHold_ = 0; // seconds the ending line has been fully shown
};

class ResultScreen final : public Screen {
  public:
    ResultScreen(Session& session, game::MissionResult result);
    Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) override;
    void draw(engine::Framebuffer& frame) const override;
    // The phone layout when the terminal is narrow (see portraitOr), else 80x24.
    engine::Size canvasSize(engine::Size terminal) const override;

  private:
    Session& session_;
    game::MissionResult result_;
};

} // namespace heroes::app
