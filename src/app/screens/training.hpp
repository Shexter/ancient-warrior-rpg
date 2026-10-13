#pragma once

#include "app/screen.hpp"
#include "app/widgets.hpp"
#include "game/training.hpp"

#include <optional>
#include <vector>

namespace heroes::app {

// The caves with the hero's estimated win chance for each, then a cave level (1-30). Starts a
// push-your-luck session and hands it to the playback. Esc goes back to the Hub.
class TrainingSetupScreen final : public Screen {
  public:
    explicit TrainingSetupScreen(Session& session);
    Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) override;
    void draw(engine::Framebuffer& frame) const override;
    // The phone layout when the terminal is narrow (see portraitOr), else 80x24.
    engine::Size canvasSize(engine::Size terminal) const override;

  private:
    Session& session_;
    NumberInput caveInput_{1, game::TrainingSession::kMaxCaveLevel};
    std::vector<int> odds_; // per cave, computed once: the state can't change on this screen
    std::optional<MessageBox> message_; // a refused session
    mutable bool portrait_ = false;      // last drawn on the phone layout: taps hit the keypad
};

// Plays one round at a time at 10 events per second (Enter finishes the round). After a win or a
// draw the player chooses Continue or Leave (Esc), seeing the streak, the pot and the odds of
// the next round; Leave banks the pot. A defeat loses it and goes to the totals.
class TrainingPlaybackScreen final : public Screen {
  public:
    TrainingPlaybackScreen(Session& session, game::TrainingSession training,
                           const game::Hero& before);
    Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) override;
    void draw(engine::Framebuffer& frame) const override;
    // 120x40 during playback when the terminal fits it and both sprites have large art; the
    // totals stay 80x24.
    engine::Size canvasSize(engine::Size terminal) const override;

  private:
    void apply(const game::Event& event);
    void playRound();
    void finishRound();
    void leave();
    void drawPlayback(engine::Framebuffer& frame) const;
    void drawPlaybackPortrait(engine::Framebuffer& frame) const;
    void drawTotals(engine::Framebuffer& frame) const;
    std::string summary() const;

    Session& session_;
    game::TrainingSession training_;
    game::Hero before_;
    std::vector<game::TrainingRound> rounds_;
    Menu choice_{{{"Continue"}, {"Leave"}}, Menu::Layout::Horizontal};
    std::size_t event_ = 0;
    double clock_ = 0; // time since the last event, or since a defeat ended the round
    bool roundDone_ = false;
    bool totals_ = false;
    std::int64_t banked_ = 0, lostPot_ = 0;
    int nextOdds_ = 0; // win chance for the next round, refreshed after each round
    int heroHp_;
    int monsterHp_ = 0;
};

} // namespace heroes::app
