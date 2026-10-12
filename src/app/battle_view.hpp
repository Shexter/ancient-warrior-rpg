#pragma once

#include "app/art.hpp"
#include "engine/framebuffer.hpp"
#include "engine/sprite.hpp"
#include "game/battle.hpp"
#include "game/content.hpp"
#include "game/events.hpp"

#include <deque>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace heroes::app {

// Where a battle is drawn. Columns of the hero's stats are fixed; the enemy's start at
// enemyStatsX. The stats take statsRow and the row below; the log shows the newest logRows lines
// from logRow.
struct BattleLayout {
    int heroX, heroY, enemyX, enemyY;
    int bannerRow;
    int statsRow, enemyStatsX;
    int logRow, logRows;
};
inline constexpr BattleLayout kCompactBattle{6, 4, 44, 2, 9, 16, 44, 18, 4}; // ui.md, 80x24
inline constexpr BattleLayout kLargeBattle{6, 3, 60, 2, 14, 29, 62, 32, 6};  // 120x40

// Plays a Battle's events as timed animations (ui.md "Event animations"): one at a time, in
// order, each applying its HP/status change and log line when it starts. Draws the arena, the
// HP/status rows and the log where a BattleLayout says.
class BattleView {
  public:
    BattleView(const game::Content& content, const game::Combatant& hero,
               const game::Combatant& enemy);

    // Queues events. `hero` and `enemy` are the combatants after them: once the queue is empty
    // the bars and status tags are set from them (events don't report status turns ticking down).
    void push(std::vector<game::Event> events, const game::Combatant& hero,
              const game::Combatant& enemy);
    void log(std::string line); // e.g. a rejected action
    void tick(double dtSeconds);
    void skip(); // finishes the current animation
    bool busy() const { return current_.has_value() || !queue_.empty(); }
    // The outcome, once BattleEnded has played.
    std::optional<game::Outcome> ended() const { return ended_; }
    int turn() const { return turn_; }
    // Whole-frame horizontal offset while a skill is cast (±1), else 0.
    int shake() const;
    void draw(engine::Framebuffer& frame, Art hero, Art enemy,
              const BattleLayout& layout = kCompactBattle) const;

  private:
    struct Fighter {
        std::string name; // upper case, as logged
        int hp, maxHp, mp, maxMp;
        std::vector<std::pair<game::StatusId, int>> statuses; // shown as tags, in order applied
        bool gone = false; // defeated or fled: no longer drawn
    };
    struct Playing {
        game::Event event;
        double duration;
        double elapsed = 0;
    };
    struct Floater { // "-380", "+25", "BLOCK" rising over a sprite
        game::Side side;
        std::string text;
        engine::Rgb color;
        double age = 0;
    };

    Fighter& fighter(game::Side side) { return side == game::Side::Hero ? hero_ : enemy_; }
    const Fighter& fighter(game::Side side) const {
        return side == game::Side::Hero ? hero_ : enemy_;
    }
    void startNext();
    double begin(const game::Event& event); // applies and logs the event; returns its duration
    void finishCurrent();
    void drawFighter(engine::Framebuffer& frame, game::Side side, Art art,
                     const BattleLayout& layout) const;
    void drawStats(engine::Framebuffer& frame, const BattleLayout& layout) const;

    const game::Content& content_;
    Fighter hero_, enemy_;
    game::Combatant heroAfter_, enemyAfter_;
    std::deque<game::Event> queue_;
    std::optional<Playing> current_;
    std::vector<Floater> floaters_;
    std::deque<std::string> log_;
    std::optional<game::Outcome> ended_;
    int turn_ = 0;
};

} // namespace heroes::app
