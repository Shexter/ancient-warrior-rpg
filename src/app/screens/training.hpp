#pragma once

#include "app/screen.hpp"
#include "app/widgets.hpp"
#include "game/training.hpp"

#include <optional>
#include <vector>

namespace heroes::app {

// Cave level (1-30), then rounds (1-100). Runs the whole session at once (results are applied
// immediately) and hands them to the playback. Esc goes back to the Hub.
class TrainingSetupScreen final : public Screen {
  public:
    explicit TrainingSetupScreen(Session& session);
    Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) override;
    void draw(engine::Framebuffer& frame) const override;

  private:
    Session& session_;
    NumberInput caveInput_{1, 30};
    NumberInput roundsInput_{1, 100};
    std::optional<int> cave_;
    std::optional<MessageBox> message_; // a refused session
};

// Replays the rounds at 10 events per second, then shows the totals. Enter finishes the
// current round (or ends the pause after it); Esc jumps to the totals.
class TrainingPlaybackScreen final : public Screen {
  public:
    // `requested` is the round count asked for; a defeat can end the session sooner.
    TrainingPlaybackScreen(Session& session, std::vector<game::TrainingRound> rounds, int requested,
                           const game::Hero& before);
    Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) override;
    void draw(engine::Framebuffer& frame) const override;
    // 120x40 during playback when the terminal fits it and both sprites have large art; the
    // totals stay 80x24.
    engine::Size canvasSize(engine::Size terminal) const override;

  private:
    void apply(const game::Event& event);
    void finishRound();
    void nextRound();
    void drawPlayback(engine::Framebuffer& frame) const;
    void drawTotals(engine::Framebuffer& frame) const;
    std::string summary(std::size_t index) const;

    Session& session_;
    std::vector<game::TrainingRound> rounds_;
    int requested_;
    game::Hero before_;
    std::size_t round_ = 0;
    std::size_t event_ = 0;
    double clock_ = 0; // time since the last event, or since the round ended
    bool roundDone_ = false;
    bool totals_ = false;
    int heroHp_;
    int monsterHp_ = 0;
};

} // namespace heroes::app
