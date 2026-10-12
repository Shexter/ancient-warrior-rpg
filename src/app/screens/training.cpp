#include "app/screens/training.hpp"

#include "game/errors.hpp"

#include <algorithm>
#include <format>
#include <utility>

namespace heroes::app {

namespace {

using engine::Key;

constexpr double kEventSeconds = 0.1; // 10 events per second (ui.md)
constexpr double kPauseSeconds = 1.0; // the finished round's summary stays up this long

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

TrainingSetupScreen::TrainingSetupScreen(Session& session) : session_(session) {}

Transition TrainingSetupScreen::update(std::span<const engine::KeyEvent> keys, double dtSeconds) {
    caveInput_.tick(dtSeconds);
    roundsInput_.tick(dtSeconds);
    for (const engine::KeyEvent& key : keys) {
        if (message_) {
            if (message_->handle(key))
                return Pop{};
            continue;
        }
        NumberInput& input = cave_ ? roundsInput_ : caveInput_;
        const InputResult result = input.handle(key);
        if (result == InputResult::Cancelled)
            return Pop{};
        if (result != InputResult::Submitted)
            continue;
        if (!cave_) {
            cave_ = static_cast<int>(*caveInput_.value());
            continue;
        }
        const int rounds = static_cast<int>(*roundsInput_.value());
        const game::Hero before = session_.state->hero;
        auto results = game::runTraining(*session_.state, session_.content, *cave_, rounds);
        if (!results) {
            message_.emplace(game::describe(results.error()));
            continue;
        }
        session_.dirty = true;
        return Replace{std::make_unique<TrainingPlaybackScreen>(session_, std::move(*results),
                                                                rounds, before)};
    }
    return Stay{};
}

void TrainingSetupScreen::draw(engine::Framebuffer& frame) const {
    frame.clear(palette::kBg);
    drawStatusPanel(frame, session_.state->hero, session_.content);
    drawText(frame, 2, 5, "TRAINING CAVE", palette::kAccent);
    drawText(frame, 4, 7, "Cave level (1–30):", palette::kText);
    if (!cave_) {
        caveInput_.draw(frame, 6, 8);
    } else {
        drawText(frame, 6, 8, std::to_string(*cave_), palette::kText);
        drawText(frame, 4, 11, "Rounds (1–100):", palette::kText);
        roundsInput_.draw(frame, 6, 12);
    }
    drawCentered(frame, 22, "Enter confirm · Esc back", palette::kDim);
    if (message_)
        message_->draw(frame);
}

// ---- Playback ----------------------------------------------------------------------------

TrainingPlaybackScreen::TrainingPlaybackScreen(Session& session,
                                               std::vector<game::TrainingRound> rounds,
                                               int requested, const game::Hero& before)
    : session_(session), rounds_(std::move(rounds)), requested_(requested), before_(before),
      heroHp_(before.hp) {
    totals_ = rounds_.empty();
    if (!totals_)
        monsterHp_ = rounds_.front().monsterMaxHp;
}

void TrainingPlaybackScreen::apply(const game::Event& event) {
    if (const auto* damaged = std::get_if<game::ev::Damaged>(&event))
        (damaged->target == game::Side::Hero ? heroHp_ : monsterHp_) = damaged->hpAfter;
    else if (const auto* healed = std::get_if<game::ev::Healed>(&event))
        (healed->target == game::Side::Hero ? heroHp_ : monsterHp_) = healed->hpAfter;
}

void TrainingPlaybackScreen::finishRound() {
    const auto& events = rounds_[round_].events;
    while (event_ < events.size())
        apply(events[event_++]);
    roundDone_ = true;
    clock_ = 0;
}

void TrainingPlaybackScreen::nextRound() {
    if (round_ + 1 >= rounds_.size()) {
        totals_ = true;
        return;
    }
    ++round_;
    event_ = 0;
    monsterHp_ = rounds_[round_].monsterMaxHp;
    roundDone_ = false;
    clock_ = 0;
}

Transition TrainingPlaybackScreen::update(std::span<const engine::KeyEvent> keys,
                                          double dtSeconds) {
    for (const engine::KeyEvent& key : keys) {
        if (totals_) {
            if (key.key == Key::Enter || key.key == Key::Escape) {
                int won = 0;
                for (const game::TrainingRound& r : rounds_)
                    won += r.outcome == game::Outcome::Victory ? 1 : 0;
                session_.lastMessage =
                    std::format("Training: won {} of {} rounds.", won, rounds_.size());
                return Pop{};
            }
        } else if (key.key == Key::Escape) {
            totals_ = true;
        } else if (key.key == Key::Enter) {
            if (roundDone_)
                nextRound();
            else
                finishRound();
        }
    }
    if (totals_)
        return Stay{};

    clock_ += dtSeconds;
    const auto& events = rounds_[round_].events;
    while (!roundDone_ && clock_ + kTimerSlack >= kEventSeconds && event_ < events.size()) {
        apply(events[event_++]);
        clock_ -= kEventSeconds;
    }
    if (!roundDone_ && event_ == events.size()) {
        roundDone_ = true;
        clock_ = 0;
    } else if (roundDone_ && clock_ + kTimerSlack >= kPauseSeconds) {
        nextRound();
    }
    return Stay{};
}

std::string TrainingPlaybackScreen::summary(std::size_t index) const {
    const game::TrainingRound& r = rounds_[index];
    std::string text = std::format("Round {}/{} · ", index + 1, requested_);
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
        break;
    default:
        text += std::format("Stalemate with the {}", r.monsterName);
        break;
    }
    return text;
}

void TrainingPlaybackScreen::drawPlayback(engine::Framebuffer& frame) const {
    const game::TrainingRound& r = rounds_[round_];
    drawText(frame, 2, 1, std::format("TRAINING CAVE · Level {}", r.caveLevel), palette::kAccent);

    const game::Hero& hero = session_.state->hero;
    if (const auto it = session_.sprites.find(std::format("hero_{}.sprite", hero.pathId));
        it != session_.sprites.end())
        engine::drawSprite(frame, it->second, 4, 6);
    if (const auto it = session_.sprites.find("monster.sprite"); it != session_.sprites.end())
        engine::drawSprite(frame, it->second, 56, 6);

    drawText(frame, 56, 15, r.monsterName, palette::kText);
    drawBar(frame, 56, 16, 20, monsterHp_, r.monsterMaxHp, hpColor(monsterHp_, r.monsterMaxHp));
    drawText(frame, 56, 17, std::format("{}/{}", monsterHp_, r.monsterMaxHp), palette::kText);

    drawText(frame, 4, 19, upper(hero.name), palette::kAccent);
    drawBar(frame, 4, 20, 20, heroHp_, hero.maxHp, hpColor(heroHp_, hero.maxHp));
    drawText(frame, 25, 20, std::format("{}/{}", heroHp_, hero.maxHp), palette::kText);

    if (roundDone_)
        drawText(frame, 2, 22, summary(round_), palette::kText, 76);
    else if (round_ > 0)
        drawText(frame, 2, 22, summary(round_ - 1), palette::kDim, 76);
    drawCentered(frame, 23, "Enter skip round · Esc skip to totals", palette::kDim);
}

void TrainingPlaybackScreen::drawTotals(engine::Framebuffer& frame) const {
    const game::Hero& hero = session_.state->hero;
    drawStatusPanel(frame, hero, session_.content);
    drawText(frame, 2, 5, "TRAINING RESULTS", palette::kAccent);

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
    drawText(frame, 4, 10, std::format("Levels gained {}", hero.level - before_.level),
             palette::kText);
    drawText(frame, 4, 11, std::format("Drops         {}", countedList(drops)), palette::kText, 74);
    if (!lost.empty())
        drawText(frame, 4, 12, std::format("Lost (bag full) {}", countedList(lost)), palette::kDmg,
                 74);
    if (rounds_.back().outcome == game::Outcome::Defeat)
        drawText(frame, 4, 14, std::format("{} has been injured badly", upper(hero.name)),
                 palette::kDmg);
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
