#include "app/screens/mission.hpp"

#include "app/art.hpp"

#include "game/errors.hpp"
#include "game/hero.hpp"

#include <algorithm>
#include <format>
#include <memory>
#include <utility>

namespace heroes::app {

namespace {

using engine::Framebuffer;
using engine::Key;
using engine::KeyEvent;

std::string_view describe(game::ActionError e) {
    switch (e) {
    case game::ActionError::BattleOver:
        return "The battle is over.";
    case game::ActionError::SkillNotLearned:
        return "You haven't learned that skill.";
    case game::ActionError::NotEnoughMp:
        return "Not enough Fervor.";
    case game::ActionError::SkillNotReady:
        return "That skill can't be used right now.";
    case game::ActionError::UnknownSkill:
        return "There is no such skill.";
    case game::ActionError::ScriptFailed:
        return "Something went wrong with that skill's magic.";
    case game::ActionError::ItemNotInStock:
        return "You have none of that left.";
    }
    return "That action is not possible.";
}

// Moves every cell `offset` columns sideways (the skill-cast screen shake).
void shiftFrame(Framebuffer& frame, int offset) {
    const engine::Cell blank{U' ', palette::kText, palette::kBg};
    for (int y = 0; y < frame.height(); ++y) {
        if (offset > 0) {
            for (int x = frame.width() - 1; x >= 0; --x)
                frame.at(x, y) = x >= offset ? frame.at(x - offset, y) : blank;
        } else {
            for (int x = 0; x < frame.width(); ++x)
                frame.at(x, y) = x - offset < frame.width() ? frame.at(x - offset, y) : blank;
        }
    }
}

std::string itemNames(const std::vector<std::string>& ids, const game::Content& content) {
    std::string out;
    for (const std::string& id : ids)
        out += (out.empty() ? "" : ", ") + content.item(id).name;
    return out;
}

enum Action { kAttack, kSkill, kItem, kDefend, kFlee };

} // namespace

// ---- Mission Select ----------------------------------------------------------------------

MissionSelectScreen::MissionSelectScreen(Session& session) : session_(session), menu_({}) {
    const game::Hero& hero = session_.state->hero;
    std::vector<Menu::Item> rows;
    for (const game::MissionDef& mission : session_.content.missions()) {
        std::string why;
        if (!mission.repeatable && hero.completedMissions.contains(mission.id))
            why = "Cleared";
        else if (hero.level < mission.minLevel)
            why = std::format("Requires Lv {}", mission.minLevel);
        else
            why = adventureBlocker(hero);
        rows.emplace_back(mission.name, why.empty(), why);
    }
    rows.emplace_back("Coming Soon", false);
    rows.emplace_back("Coming Soon", false);
    menu_.setItems(std::move(rows));
}

Transition MissionSelectScreen::update(std::span<const KeyEvent> keys, double) {
    const auto missions = session_.content.missions();
    for (const KeyEvent& key : keys) {
        if (key.key == Key::Escape)
            return Pop{};
        const auto index = menu_.handle(key);
        if (index && *index < static_cast<int>(missions.size()))
            return Replace{std::make_unique<MissionIntroScreen>(
                session_, missions[static_cast<std::size_t>(*index)].id)};
    }
    return Stay{};
}

void MissionSelectScreen::draw(Framebuffer& frame) const {
    frame.clear(palette::kBg);
    drawStatusPanel(frame, session_.state->hero, session_.content);
    drawText(frame, 2, 5, "MISSIONS", palette::kAccent);
    menu_.draw(frame, 4, 7);
    drawCentered(frame, 22, "↑↓ select · Enter choose · Esc back", palette::kDim);
}

// ---- Mission Intro -----------------------------------------------------------------------

MissionIntroScreen::MissionIntroScreen(Session& session, std::string missionId)
    : session_(session), missionId_(std::move(missionId)),
      story_(session.content.mission(missionId_).story, 60) {}

Transition MissionIntroScreen::update(std::span<const KeyEvent> keys, double dtSeconds) {
    story_.tick(dtSeconds);
    for (const KeyEvent& key : keys) {
        if (message_) {
            if (message_->handle(key))
                return Pop{};
            continue;
        }
        if (confirm_) {
            const auto answer = confirm_->handle(key);
            if (!answer)
                continue;
            confirm_.reset();
            if (!*answer)
                return Replace{std::make_unique<MissionSelectScreen>(session_)};
            auto battle = game::startMission(*session_.state, session_.content, missionId_,
                                             session_.scripts);
            if (!battle) {
                message_.emplace(game::describe(battle.error()));
                continue;
            }
            session_.dirty = true; // the battle's random stream was forked from the save's
            return Replace{
                std::make_unique<BattleScreen>(session_, missionId_, std::move(*battle))};
        }
        if (key.key == Key::Escape)
            return Replace{std::make_unique<MissionSelectScreen>(session_)};
        if (story_.handle(key))
            confirm_.emplace("Enter the lair?");
    }
    return Stay{};
}

void MissionIntroScreen::draw(Framebuffer& frame) const {
    frame.clear(palette::kBg);
    const game::MissionDef& mission = session_.content.mission(missionId_);
    drawCentered(frame, 3, mission.name, palette::kAccent);
    drawCentered(frame, 5, mission.location, palette::kDim);
    story_.draw(frame, 10, 8, palette::kText);
    drawCentered(frame, 22, "Enter continue · Esc back", palette::kDim);
    if (confirm_)
        confirm_->draw(frame);
    if (message_)
        message_->draw(frame);
}

// ---- Battle ------------------------------------------------------------------------------

BattleScreen::BattleScreen(Session& session, std::string missionId, game::Battle battle)
    : session_(session), missionId_(std::move(missionId)), battle_(std::move(battle)),
      view_(session.content, battle_.hero(), battle_.enemy()),
      actions_({{"Attack"}, {"Skill"}, {"Item"}, {"Defend"}, {"Flee"}}, Menu::Layout::Horizontal) {
    view_.push(battle_.start(), battle_.hero(), battle_.enemy());
}

BattleScreen::Overlay BattleScreen::skillOverlay() const {
    const game::Hero& hero = session_.state->hero;
    int nameWidth = 0;
    for (const game::SkillDef& skill : session_.content.skills())
        nameWidth = std::max(nameWidth, textWidth(skill.name));
    Overlay overlay{" Skills · Fervor cost in () ", Menu({}), {}};
    std::vector<Menu::Item> rows;
    for (const game::SkillDef& skill : session_.content.skills()) {
        const bool usable =
            hero.learnedSkills.contains(skill.id) && battle_.hero().mp >= skill.mpCost;
        rows.emplace_back(std::format("{:<{}}({})", skill.name, nameWidth + 1, skill.mpCost), usable,
                          skill.description);
        overlay.actions.emplace_back(game::CastSkill{skill.id});
    }
    overlay.menu.setItems(std::move(rows));
    return overlay;
}

std::optional<BattleScreen::Overlay> BattleScreen::itemOverlay() const {
    if (battle_.items().empty())
        return std::nullopt;
    Overlay overlay{" Items · how many you carry ", Menu({}), {}};
    std::vector<Menu::Item> rows;
    for (const auto& [id, count] : battle_.items()) {
        const game::ItemDef& item = session_.content.item(id);
        rows.emplace_back(std::format("{} x{}", item.name, count), true, item.description);
        overlay.actions.emplace_back(game::UseItem{id});
    }
    overlay.menu.setItems(std::move(rows));
    return overlay;
}

void BattleScreen::act(const game::PlayerAction& action) {
    auto events = battle_.act(action);
    if (!events) {
        view_.log(std::string(describe(events.error())));
        return;
    }
    view_.push(std::move(*events), battle_.hero(), battle_.enemy());
    if (!battle_.over())
        return;
    // Applied now, not after the animations, so the outcome can't be lost.
    auto result = game::finishMission(*session_.state, session_.content, missionId_, battle_);
    session_.dirty = true;
    if (result)
        result_ = std::move(*result);
    else
        session_.lastMessage = std::string(game::describe(result.error()));
}

Transition BattleScreen::update(std::span<const KeyEvent> keys, double dtSeconds) {
    for (const KeyEvent& key : keys) {
        if (view_.busy()) { // only Enter (skip) counts while events play
            if (key.key == Key::Enter)
                view_.skip();
            continue;
        }
        if (battle_.over())
            continue;
        if (overlay_) {
            if (key.key == Key::Escape) {
                overlay_.reset();
            } else if (const auto index = overlay_->menu.handle(key)) {
                const game::PlayerAction chosen =
                    overlay_->actions[static_cast<std::size_t>(*index)];
                overlay_.reset();
                act(chosen);
            }
            continue;
        }
        switch (actions_.handle(key).value_or(-1)) {
        case kAttack:
            act(game::Attack{});
            break;
        case kSkill:
            overlay_.emplace(skillOverlay());
            break;
        case kItem:
            overlay_ = itemOverlay();
            if (!overlay_)
                view_.log("You carry no items to use.");
            break;
        case kDefend:
            act(game::Defend{});
            break;
        case kFlee:
            act(game::Flee{});
            break;
        default:
            break;
        }
    }
    view_.tick(dtSeconds);
    if (view_.ended() && !view_.busy()) {
        if (result_)
            return Replace{std::make_unique<ResultScreen>(session_, std::move(*result_))};
        return Pop{};
    }
    return Stay{};
}

std::string BattleScreen::heroFile() const {
    return std::format("hero_{}.sprite", session_.state->hero.pathId);
}

std::string BattleScreen::enemyFile() const {
    return session_.content.enemy(session_.content.mission(missionId_).enemy).sprite;
}

engine::Size BattleScreen::canvasSize(engine::Size terminal) const {
    return artCanvas(session_, terminal, {heroFile(), enemyFile()});
}

void BattleScreen::draw(Framebuffer& frame) const {
    frame.clear(palette::kBg);
    const bool large = isLarge(frame);
    const game::MissionDef& mission = session_.content.mission(missionId_);
    drawText(frame, 1, 0, mission.location, palette::kAccent, 60);
    // While events play, the turn they belong to; while choosing, the turn about to start.
    const int turnNumber =
        view_.busy() || battle_.over() ? view_.turn() : battle_.turn() + 1;
    const std::string turn = std::format("Turn {}", turnNumber);
    drawText(frame, frame.width() - 1 - textWidth(turn), 0, turn, palette::kText);

    if (large)
        view_.draw(frame, largeArt(session_, heroFile()), largeArt(session_, enemyFile()),
                   kLargeBattle);
    else
        view_.draw(frame, compactArt(session_, heroFile()), compactArt(session_, enemyFile()),
                   kCompactBattle);

    actions_.draw(frame, 1, frame.height() - 2, !view_.busy() && !overlay_ && !battle_.over());
    drawCentered(frame, frame.height() - 1,
                 "←→ select · Enter confirm · during animation: Enter skips", palette::kDim);
    if (overlay_) {
        // One row per entry plus the border, ending on row 20 (compact: rows 14-20 for the five
        // base skills, as in ui.md) or row 32 (large); more entries grow it upward.
        const int height = overlay_->menu.size() + 2;
        const int left = large ? 28 : 16, top = (large ? 33 : 21) - height;
        drawPanel(frame, left, top, 64, height);
        drawText(frame, left + 2, top, overlay_->title, palette::kAccent); // on the border
        overlay_->menu.draw(frame, left + 2, top + 1, true, 61); // inside the right border
    }
    if (const int offset = view_.shake())
        shiftFrame(frame, offset);
}

// ---- Result ------------------------------------------------------------------------------

ResultScreen::ResultScreen(Session& session, game::MissionResult result)
    : session_(session), result_(std::move(result)) {}

Transition ResultScreen::update(std::span<const KeyEvent> keys, double) {
    for (const KeyEvent& key : keys) {
        if (key.key != Key::Enter && key.key != Key::Escape)
            continue;
        switch (result_.outcome) {
        case game::Outcome::Victory:
            session_.lastMessage = "Victory! The mission is complete.";
            break;
        case game::Outcome::Defeat:
            session_.lastMessage = "Defeated. All equipment destroyed.";
            break;
        case game::Outcome::Fled:
            session_.lastMessage = "You escaped.";
            break;
        case game::Outcome::Draw:
            session_.lastMessage = "The battle was a stalemate.";
            break;
        }
        return Pop{};
    }
    return Stay{};
}

void ResultScreen::draw(Framebuffer& frame) const {
    frame.clear(palette::kBg);
    const game::Content& content = session_.content;
    drawStatusPanel(frame, session_.state->hero, content);
    int y = 9;
    switch (result_.outcome) {
    case game::Outcome::Victory:
        drawCentered(frame, 7, "VICTORY", palette::kAccent);
        drawText(frame, 20, y++, std::format("EXP   +{}", result_.expGained), palette::kText);
        drawText(frame, 20, y, "Gold", palette::kText);
        drawText(frame, 26, y++, std::format("+{}", result_.goldGained), palette::kGold);
        if (result_.levelsGained > 0)
            drawText(frame, 20, y++,
                     std::format("Level up! +{} · {} blessing{} at the Prior",
                                 result_.levelsGained, result_.levelsGained,
                                 result_.levelsGained == 1 ? "" : "s"),
                     palette::kAccent);
        if (!result_.itemsGained.empty())
            drawText(frame, 20, y++,
                     std::format("Items: {}", itemNames(result_.itemsGained, content)),
                     palette::kText, 58);
        if (!result_.itemsLostBagFull.empty())
            drawText(frame, 20, y++,
                     std::format("Bag full, lost: {}",
                                 itemNames(result_.itemsLostBagFull, content)),
                     palette::kDmg, 58);
        break;
    case game::Outcome::Defeat:
        drawCentered(frame, 7, "DEFEAT", palette::kDmg);
        drawCentered(frame, y, "All equipment destroyed!", palette::kDmg);
        break;
    case game::Outcome::Fled:
        drawCentered(frame, 7, "ESCAPED", palette::kAccent);
        drawCentered(frame, y, "You escaped.", palette::kText);
        break;
    case game::Outcome::Draw:
        drawCentered(frame, 7, "STALEMATE", palette::kAccent);
        drawCentered(frame, y, "The battle was a stalemate.", palette::kText);
        break;
    }
    drawCentered(frame, 22, "Enter return to the Hub", palette::kDim);
}

} // namespace heroes::app
