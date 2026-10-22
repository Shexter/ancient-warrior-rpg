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
        std::string why = missionBlocker(hero, mission);
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

engine::Size MissionSelectScreen::canvasSize(engine::Size terminal) const {
    return portraitOr(session_, terminal, engine::kCompactCanvas);
}

void MissionSelectScreen::draw(Framebuffer& frame) const {
    frame.clear(palette::kBg);
    if (isPortrait(frame)) {
        drawHeroHeader(frame, "MISSIONS", session_.state->hero);
        const auto missions = session_.content.missions();
        if (menu_.selected() < static_cast<int>(missions.size())) {
            const game::MissionDef& m = missions[static_cast<std::size_t>(menu_.selected())];
            drawText(frame, 2, 8, m.name, palette::kAccent, frame.width() - 4);
            drawText(frame, 2, 9, m.location, palette::kDim, frame.width() - 4);
            drawText(frame, 2, 11, std::format("Level {} or higher", m.minLevel), palette::kText);
        }
        drawButtonMenu(frame, menu_);
        return;
    }
    drawStatusPanel(frame, session_.state->hero, session_.content);
    drawText(frame, 2, 5, "MISSIONS", palette::kAccent);
    menu_.draw(frame, 4, 7);
    drawCentered(frame, 22, "↑↓ select · Enter choose · Esc back", palette::kDim);
}

// ---- Mission Intro -----------------------------------------------------------------------

MissionIntroScreen::MissionIntroScreen(Session& session, std::string missionId, bool fromWorld)
    : session_(session), missionId_(std::move(missionId)), fromWorld_(fromWorld),
      story_(session.content.mission(missionId_).story, 60) {}

Transition MissionIntroScreen::back() const {
    if (fromWorld_)
        return Pop{};
    return Replace{std::make_unique<MissionSelectScreen>(session_)};
}

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
                return back();
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
            return back();
        if (story_.handle(key))
            confirm_.emplace("Enter the lair?");
    }
    return Stay{};
}

engine::Size MissionIntroScreen::canvasSize(engine::Size terminal) const {
    return portraitOr(session_, terminal, engine::kCompactCanvas);
}

void MissionIntroScreen::draw(Framebuffer& frame) const {
    frame.clear(palette::kBg);
    const game::MissionDef& mission = session_.content.mission(missionId_);
    if (isPortrait(frame)) {
        drawHeader(frame, mission.name, {});
        drawText(frame, 1, 2, mission.location, palette::kDim, frame.width() - 4);
        story_.draw(frame, 2, 7, palette::kText, frame.width() - 4);
        drawCentered(frame, portrait::kPadTop - 2, "● continue · ✕ back", palette::kDim);
    } else {
        drawCentered(frame, 3, mission.name, palette::kAccent);
        drawCentered(frame, 5, mission.location, palette::kDim);
        story_.draw(frame, 10, 8, palette::kText);
        drawCentered(frame, 22, "Enter continue · Esc back", palette::kDim);
    }
    if (confirm_)
        confirm_->draw(frame);
    if (message_)
        message_->draw(frame);
}

// ---- Battle ------------------------------------------------------------------------------

BattleScreen::BattleScreen(Session& session, std::string missionId, game::Battle battle)
    : session_(session), missionId_(std::move(missionId)),
      location_(session.content.mission(missionId_).location),
      enemySprite_(session.content.enemy(session.content.mission(missionId_).enemy).sprite),
      battle_(std::move(battle)), view_(session.content, battle_.hero(), battle_.enemy()),
      actions_({{"Attack"}, {"Skill"}, {"Item"}, {"Defend"}, {"Flee"}}, Menu::Layout::Horizontal) {
    view_.push(battle_.start(), battle_.hero(), battle_.enemy());
    startNarration(true);
}

void BattleScreen::startNarration(bool bossIntro) {
    if (!session_.narrator)
        return;
    narrationRng_ = game::Rng::stream(session_.state->seed, 0xA11CE); // never the game's own stream
    const game::Combatant& hero = battle_.hero();
    const game::Combatant& enemy = battle_.enemy();
    narrationCtx_ = game::narration::BattleContext{&session_.content, hero.name, enemy.name, hero.hp, hero.maxHp,
                                                   enemy.hp, enemy.maxHp, false, false, {}};
    if (!bossIntro)
        return;
    const game::narration::NarrationRequest intro{
        game::narration::NarrationKind::BossIntro,
        {{"hero", hero.name}, {"foe", enemy.name}, {"location", location_}, {"level", session_.state->hero.level}},
        {}, {}, {}};
    narration_.ask(session_.liveNarrator, intro, game::narration::narrateOffline(*session_.narrator, intro, narrationRng_),
                   session_.narrator->limits.timeoutMs);
}

BattleScreen::BattleScreen(Session& session, game::Battle battle, std::string location,
                           std::string enemySprite, std::function<void(const game::Battle&)> onOver)
    : session_(session), location_(std::move(location)), enemySprite_(std::move(enemySprite)),
      onOver_(std::move(onOver)), battle_(std::move(battle)),
      view_(session.content, battle_.hero(), battle_.enemy()),
      actions_({{"Attack"}, {"Skill"}, {"Item"}, {"Defend"}, {"Flee"}}, Menu::Layout::Horizontal) {
    view_.push(battle_.start(), battle_.hero(), battle_.enemy());
    startNarration(false);
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
    if (narrationCtx_)
        if (const auto request = game::narration::summarizeBattleChunk(*events, *narrationCtx_))
            narrationNext_.emplace(*request, game::narration::narrateOffline(*session_.narrator, *request, narrationRng_));
    view_.push(std::move(*events), battle_.hero(), battle_.enemy());
    if (!battle_.over())
        return;
    // Applied now, not after the animations, so the outcome can't be lost.
    session_.dirty = true;
    if (onOver_) {
        onOver_(battle_);
        return;
    }
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
    if (narrationNext_ && !view_.busy()) { // after the events it describes have played
        auto [request, line] = std::move(*std::exchange(narrationNext_, std::nullopt));
        narration_.ask(session_.liveNarrator, std::move(request), line, session_.narrator->limits.timeoutMs);
    }
    if (const auto notice = narration_.tick(dtSeconds))
        view_.log(*notice);
    if (view_.ended() && !view_.busy() && narration_.revealed()) // let the last line be read
        endHold_ += dtSeconds;
    if (view_.ended() && !view_.busy() && (narration_.empty() || endHold_ >= 1.0)) {
        if (result_)
            return Replace{std::make_unique<ResultScreen>(session_, std::move(*result_))};
        return Pop{};
    }
    return Stay{};
}

std::string BattleScreen::heroFile() const {
    return std::format("hero_{}.sprite", session_.state->hero.pathId);
}

std::string BattleScreen::enemyFile() const { return enemySprite_; }

engine::Size BattleScreen::canvasSize(engine::Size terminal) const {
    return portraitOr(session_, terminal, artCanvas(session_, terminal, {heroFile(), enemyFile()}));
}

void BattleScreen::drawPortrait(Framebuffer& frame) const {
    const int turnNumber = view_.busy() || battle_.over() ? view_.turn() : battle_.turn() + 1;
    drawText(frame, 1, 0, std::format("{} · Turn {}", location_, turnNumber),
             palette::kAccent, 42);
    view_.draw(frame, {}, compactArt(session_, enemyFile()), kPortraitBattle);
    if (overlay_) {
        drawText(frame, 2, portrait::kMenuTop - 1, overlay_->title, palette::kAccent,
                 frame.width() - 4);
        drawButtonMenu(frame, overlay_->menu);
        return;
    }
    if (view_.busy()) {
        drawPanel(frame, 1, portrait::kMenuTop, frame.width() - 2, portrait::kMenuRows + 2);
        drawText(frame, 3, portrait::kMenuTop + 1, "● skips the animation", palette::kDim);
        return;
    }
    drawButtonMenu(frame, actions_);
}

void BattleScreen::draw(Framebuffer& frame) const {
    frame.clear(palette::kBg);
    if (isPortrait(frame)) {
        drawPortrait(frame);
        if (const int offset = view_.shake())
            shiftFrame(frame, offset);
        return;
    }
    const bool large = isLarge(frame);
    drawText(frame, 1, 0, location_, palette::kAccent, 60);
    // While events play, the turn they belong to; while choosing, the turn about to start.
    const int turnNumber =
        view_.busy() || battle_.over() ? view_.turn() : battle_.turn() + 1;
    const std::string turn = std::format("Turn {}", turnNumber);
    drawText(frame, frame.width() - 1 - textWidth(turn), 0, turn, palette::kText);

    // With narration, it takes the log box's top two rows and the log keeps the rest.
    BattleLayout layout = large ? kLargeBattle : kCompactBattle;
    const int narrationRow = layout.logRow;
    if (!narration_.empty()) {
        layout.logRow += 2;
        layout.logRows -= 2;
    }
    if (large)
        view_.draw(frame, largeArt(session_, heroFile()), largeArt(session_, enemyFile()), layout);
    else
        view_.draw(frame, compactArt(session_, heroFile()), compactArt(session_, enemyFile()), layout);
    narration_.draw(frame, 1, narrationRow);

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

engine::Size ResultScreen::canvasSize(engine::Size terminal) const {
    return portraitOr(session_, terminal, engine::kCompactCanvas);
}

void ResultScreen::draw(Framebuffer& frame) const {
    frame.clear(palette::kBg);
    const game::Content& content = session_.content;
    const bool phone = isPortrait(frame);
    if (phone)
        drawHeroHeader(frame, "RESULT", session_.state->hero);
    else
        drawStatusPanel(frame, session_.state->hero, content);
    // Compact: lines from column 20, 58 wide. Phone: from column 1, the full width.
    const int x = phone ? 1 : 20, width = phone ? frame.width() - 2 : 58;
    int y = 9;
    switch (result_.outcome) {
    case game::Outcome::Victory:
        drawCentered(frame, 7, "VICTORY", palette::kAccent);
        drawText(frame, x, y++, std::format("EXP   +{}", result_.expGained), palette::kText);
        drawText(frame, x, y, "Gold", palette::kText);
        drawText(frame, x + 6, y++, std::format("+{}", result_.goldGained), palette::kGold);
        if (result_.levelsGained > 0)
            drawText(frame, x, y++,
                     std::format("Level up! +{} · {} blessing{} at the Prior",
                                 result_.levelsGained, result_.levelsGained,
                                 result_.levelsGained == 1 ? "" : "s"),
                     palette::kAccent, width);
        if (!result_.itemsGained.empty())
            drawText(frame, x, y++,
                     std::format("Items: {}", itemNames(result_.itemsGained, content)),
                     palette::kText, width);
        if (!result_.itemsLostBagFull.empty())
            drawText(frame, x, y++,
                     std::format("Bag full, lost: {}",
                                 itemNames(result_.itemsLostBagFull, content)),
                     palette::kDmg, width);
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
    drawCentered(frame, phone ? portrait::kPadTop - 2 : 22,
                 phone ? "● return to the Hub" : "Enter return to the Hub", palette::kDim);
}

} // namespace heroes::app
