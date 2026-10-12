#include "app/screens/training.hpp"
#include "app/art.hpp"

#include "game/errors.hpp"
#include "game/estimate.hpp"

#include <algorithm>
#include <format>
#include <utility>

namespace heroes::app {

namespace {

using engine::Key;

constexpr double kEventSeconds = 0.1; // 10 events per second (ui.md)
constexpr double kPauseSeconds = 1.0; // the finished round's summary stays up this long

// Green when likely, gold when a gamble, red when it probably goes badly.
engine::Rgb oddsColor(int percent) {
    return percent >= 80 ? palette::kHeal : percent >= 50 ? palette::kGold : palette::kDmg;
}

// "Topaz x2, Gold Leaf" in first-seen order.
std::string countedList(const std::vector<std::string>& names) {
    std::vector<std::pair<std::string, int>> counted;
    for (const std::string& name : names) {
        auto it = std::ranges::find(counted, name, &std::pair<std::string, int>::first);
        if (it == counted.end())
            counted.emplace_back(name, 1);
        else
            ++it->second;
    }
    std::string out;
    for (const auto& [name, count] : counted) {
        if (!out.empty())
            out += ", ";
        out += count > 1 ? std::format("{} x{}", name, count) : name;
    }
    return out.empty() ? "none" : out;
}

} // namespace

// ---- Setup -------------------------------------------------------------------------------

TrainingSetupScreen::TrainingSetupScreen(Session& session) : session_(session) {
    for (int cave = 1; cave <= game::TrainingSession::kMaxCaveLevel; ++cave)
        odds_.push_back(game::caveWinChance(*session_.state, session_.content, cave));
}

Transition TrainingSetupScreen::update(std::span<const engine::KeyEvent> keys, double dtSeconds) {
    caveInput_.tick(dtSeconds);
    for (const engine::KeyEvent& key : keys) {
        if (message_) {
            if (message_->handle(key))
                return Pop{};
            continue;
        }
        const InputResult result = caveInput_.handle(key);
        if (result == InputResult::Cancelled)
            return Pop{};
        if (result != InputResult::Submitted)
            continue;
        const game::Hero before = session_.state->hero;
        auto training = game::TrainingSession::start(*session_.state, session_.content,
                                                      static_cast<int>(*caveInput_.value()));
        if (!training) {
            message_.emplace(game::describe(training.error()));
            continue;
        }
        session_.dirty = true;
        return Replace{
            std::make_unique<TrainingPlaybackScreen>(session_, std::move(*training), before)};
    }
    return Stay{};
}

void TrainingSetupScreen::draw(engine::Framebuffer& frame) const {
    frame.clear(palette::kBg);
    drawStatusPanel(frame, session_.state->hero, session_.content);
    drawText(frame, 2, 5, "THE CATACOMBS", palette::kAccent);
    drawText(frame, 40, 5, "Your odds of winning a round", palette::kDim);
    // Three columns of ten: "Cave 12  ≈85%".
    for (std::size_t i = 0; i < odds_.size(); ++i) {
        const int x = 4 + static_cast<int>(i / 10) * 26, y = 7 + static_cast<int>(i % 10);
        drawText(frame, x, y, std::format("Cave {:>2}", i + 1), palette::kText);
        drawText(frame, x + 9, y, std::format("≈{}%", odds_[i]), oddsColor(odds_[i]));
    }
    drawText(frame, 4, 18, "Cave level (1–30):", palette::kText);
    caveInput_.draw(frame, 23, 18);
    if (const auto cave = caveInput_.value(); cave && *cave >= 1 && *cave <= 30)
        drawText(frame, 4, 19,
                 std::format("Wins pay {}–{} gold and {} EXP; a streak fills the pot.",
                             100 * *cave, 190 * *cave, 100 * *cave),
                 palette::kDim);
    drawCentered(frame, 22, "Enter fight · Esc back", palette::kDim);
    if (message_)
        message_->draw(frame);
}

// ---- Playback ----------------------------------------------------------------------------

TrainingPlaybackScreen::TrainingPlaybackScreen(Session& session, game::TrainingSession training,
                                               const game::Hero& before)
    : session_(session), training_(std::move(training)), before_(before), heroHp_(before.hp) {
    playRound();
}

void TrainingPlaybackScreen::apply(const game::Event& event) {
    if (const auto* damaged = std::get_if<game::ev::Damaged>(&event))
        (damaged->target == game::Side::Hero ? heroHp_ : monsterHp_) = damaged->hpAfter;
    else if (const auto* healed = std::get_if<game::ev::Healed>(&event))
        (healed->target == game::Side::Hero ? heroHp_ : monsterHp_) = healed->hpAfter;
}

// Fights the next round at once (the results are applied now); the playback then catches up.
void TrainingPlaybackScreen::playRound() {
    const std::int64_t pot = training_.pot();
    rounds_.push_back(training_.next());
    if (rounds_.back().outcome == game::Outcome::Defeat)
        lostPot_ = pot;
    else
        nextOdds_ = game::caveWinChance(*session_.state, session_.content, training_.caveLevel());
    event_ = 0;
    monsterHp_ = rounds_.back().monsterMaxHp;
    roundDone_ = false;
    clock_ = 0;
    choice_.setItems({{"Continue"}, {"Leave"}});
}

void TrainingPlaybackScreen::finishRound() {
    const auto& events = rounds_.back().events;
    while (event_ < events.size())
        apply(events[event_++]);
    roundDone_ = true;
    clock_ = 0;
}

void TrainingPlaybackScreen::leave() {
    if (!training_.over())
        banked_ = training_.leave();
    totals_ = true;
}

Transition TrainingPlaybackScreen::update(std::span<const engine::KeyEvent> keys,
                                          double dtSeconds) {
    for (const engine::KeyEvent& key : keys) {
        if (totals_) {
            if (key.key == Key::Enter || key.key == Key::Escape) {
                const auto won = std::ranges::count(rounds_, game::Outcome::Victory,
                                                    &game::TrainingRound::outcome);
                session_.lastMessage =
                    rounds_.back().outcome == game::Outcome::Defeat
                        ? std::format("Training: won {} of {} rounds, then fell.", won,
                                      rounds_.size())
                        : std::format("Training: won {} of {} rounds, banked a {} gold pot.", won,
                                      rounds_.size(), banked_);
                return Pop{};
            }
        } else if (!roundDone_) {
            if (key.key == Key::Enter || key.key == Key::Escape)
                finishRound();
        } else if (training_.over()) {
            if (key.key == Key::Enter || key.key == Key::Escape)
                totals_ = true;
        } else if (key.key == Key::Escape) {
            leave();
        } else if (const auto picked = choice_.handle(key)) {
            if (*picked == 0)
                playRound();
            else
                leave();
        }
    }
    if (totals_ || (roundDone_ && !training_.over()))
        return Stay{}; // waiting for the player

    clock_ += dtSeconds;
    const auto& events = rounds_.back().events;
    while (!roundDone_ && clock_ + kTimerSlack >= kEventSeconds && event_ < events.size()) {
        apply(events[event_++]);
        clock_ -= kEventSeconds;
    }
    if (!roundDone_ && event_ == events.size()) {
        roundDone_ = true;
        clock_ = 0;
    } else if (roundDone_ && clock_ + kTimerSlack >= kPauseSeconds) {
        totals_ = true; // a defeat: the summary stayed up for a moment
    }
    return Stay{};
}

std::string TrainingPlaybackScreen::summary() const {
    const game::TrainingRound& r = rounds_.back();
    std::string text = std::format("Round {} · ", rounds_.size());
    switch (r.outcome) {
    case game::Outcome::Victory:
        text += std::format("{} defeated · +{} gold · +{} exp", r.monsterName, r.goldGained,
                            r.expGained);
        if (r.drop && r.dropLostBagFull)
            text += std::format(" · Bag full, {} lost", session_.content.item(*r.drop).name);
        else if (r.drop)
            text += std::format(" · Drop: {}", session_.content.item(*r.drop).name);
        break;
    case game::Outcome::Defeat:
        text += std::format("Defeated by the {}", r.monsterName);
        if (lostPot_ > 0)
            text += std::format(" · the {} gold pot is lost", lostPot_);
        break;
    default:
        text += std::format("Stalemate with the {} · the streak is broken", r.monsterName);
        break;
    }
    return text;
}

engine::Size TrainingPlaybackScreen::canvasSize(engine::Size terminal) const {
    if (totals_)
        return engine::kCompactCanvas;
    return artCanvas(session_, terminal,
                     {std::format("hero_{}.sprite", session_.state->hero.pathId), "monster.sprite"});
}

void TrainingPlaybackScreen::drawPlayback(engine::Framebuffer& frame) const {
    const game::TrainingRound& r = rounds_.back();
    drawText(frame, 2, 1, std::format("THE CATACOMBS · Level {}", r.caveLevel), palette::kAccent);

    // Compact (ui.md): monster at (56, 6) with its stats below, hero at (4, 6) with name and bar
    // on rows 19-20. Large: hero 48x24 at (6, 4), monster 32x16 at (80, 8).
    const bool large = isLarge(frame);
    const game::Hero& hero = session_.state->hero;
    const std::string heroFile = std::format("hero_{}.sprite", hero.pathId);
    const Art heroArt = large ? largeArt(session_, heroFile) : compactArt(session_, heroFile);
    const Art monsterArt =
        large ? largeArt(session_, "monster.sprite") : compactArt(session_, "monster.sprite");
    const int heroX = large ? 6 : 4, heroY = large ? 4 : 6;
    const int monsterX = large ? 80 : 56, monsterY = large ? 8 : 6;
    const int monsterStats = large ? 25 : 15, heroStats = large ? 29 : 19;
    heroArt.draw(frame, heroX, heroY);
    monsterArt.draw(frame, monsterX, monsterY);

    drawText(frame, monsterX, monsterStats, r.monsterName, palette::kText);
    drawBar(frame, monsterX, monsterStats + 1, 20, monsterHp_, r.monsterMaxHp,
            hpColor(monsterHp_, r.monsterMaxHp));
    drawText(frame, monsterX, monsterStats + 2, std::format("{}/{}", monsterHp_, r.monsterMaxHp),
             palette::kText);

    drawText(frame, heroX, heroStats, upper(hero.name), palette::kAccent);
    drawBar(frame, heroX, heroStats + 1, 20, heroHp_, hero.maxHp, hpColor(heroHp_, hero.maxHp));
    drawText(frame, heroX + 21, heroStats + 1, std::format("{}/{}", heroHp_, hero.maxHp),
             palette::kText);

    // Streak and pot on the top row, opposite the title, so the stakes are always in view.
    drawText(frame, 40, 1,
             std::format("Streak {} · Pot {} gold", training_.streak(), training_.pot()),
             palette::kGold);

    const int bottom = frame.height() - 1;
    if (!roundDone_) {
        drawCentered(frame, bottom, "Enter finish the round", palette::kDim);
        return;
    }
    drawText(frame, 2, bottom - 1, summary(), palette::kText, frame.width() - 4);
    if (training_.over()) {
        drawCentered(frame, bottom, "Enter see the results", palette::kDim);
        return;
    }
    choice_.draw(frame, 2, bottom);
    drawText(frame, 26, bottom,
             std::format("Next round ≈{}% · Leave banks the pot", nextOdds_),
             oddsColor(nextOdds_));
}

void TrainingPlaybackScreen::drawTotals(engine::Framebuffer& frame) const {
    const game::Hero& hero = session_.state->hero;
    drawStatusPanel(frame, hero, session_.content);
    drawText(frame, 2, 5, "CATACOMB RESULTS", palette::kAccent);

    int won = 0;
    std::vector<std::string> drops, lost;
    for (const game::TrainingRound& r : rounds_) {
        won += r.outcome == game::Outcome::Victory ? 1 : 0;
        if (r.drop)
            (r.dropLostBagFull ? lost : drops).push_back(session_.content.item(*r.drop).name);
    }
    const std::int64_t exp = hero.exp - before_.exp;
    drawText(frame, 4, 7, std::format("Rounds won    {} of {}", won, rounds_.size()),
             palette::kText);
    drawText(frame, 4, 8, "Gold", palette::kText);
    drawText(frame, 18, 8, std::format("+{}", hero.gold - before_.gold), palette::kGold);
    drawText(frame, 4, 9, std::format("EXP           {}{}", exp >= 0 ? "+" : "", exp),
             palette::kText);
    const int levels = hero.level - before_.level;
    drawText(frame, 4, 10,
             levels > 0 ? std::format("Levels gained {} · {} blessing{} at the Prior", levels,
                                      levels, levels == 1 ? "" : "s")
                        : "Levels gained 0",
             palette::kText);
    drawText(frame, 4, 11, std::format("Drops         {}", countedList(drops)), palette::kText, 74);
    if (!lost.empty())
        drawText(frame, 4, 12, std::format("Lost (bag full) {}", countedList(lost)), palette::kDmg,
                 74);
    if (rounds_.back().outcome == game::Outcome::Defeat) {
        drawText(frame, 4, 14, std::format("{} has been injured badly", upper(hero.name)),
                 palette::kDmg);
        if (lostPot_ > 0)
            drawText(frame, 4, 15, std::format("The {} gold pot is lost", lostPot_),
                     palette::kDmg);
    } else if (banked_ > 0) {
        drawText(frame, 4, 14, std::format("Pot banked    +{} gold (included above)", banked_),
                 palette::kGold);
    }
    drawCentered(frame, 22, "Enter return to the Hub", palette::kDim);
}

void TrainingPlaybackScreen::draw(engine::Framebuffer& frame) const {
    frame.clear(palette::kBg);
    if (totals_)
        drawTotals(frame);
    else
        drawPlayback(frame);
}

} // namespace heroes::app
