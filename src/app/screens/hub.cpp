#include "app/screens/hub.hpp"
#include "app/art.hpp"

#include "app/app.hpp"
#include "app/screens/mission.hpp"
#include "app/screens/town.hpp"
#include "app/screens/training.hpp"
#include "game/battle.hpp"
#include "game/hero.hpp"
#include "game/save.hpp"

#include <algorithm>
#include <format>
#include <system_error>

namespace heroes::app {

namespace {

enum Choice { kTraining, kMission, kTown, kSave, kQuit };

} // namespace

HubScreen::HubScreen(Session& session) : session_(session), menu_(menuItems()) {}

std::vector<Menu::Item> HubScreen::menuItems() const {
    const game::Hero& hero = session_.state->hero;
    const std::string adventureNote = adventureBlocker(hero);

    const auto missions = session_.content.missions();
    const auto lowest = std::ranges::min_element(missions, {}, &game::MissionDef::minLevel);
    std::string missionNote = adventureNote;
    if (missionNote.empty() && lowest == missions.end())
        missionNote = "No missions";
    else if (missionNote.empty() && hero.level < lowest->minLevel)
        missionNote = std::format("Requires Lv {}", lowest->minLevel);

    return {{"Training", adventureNote.empty(), adventureNote},
            {"Mission", missionNote.empty(), missionNote},
            {"Town"},
            {"Save"},
            {"Quit"}};
}

void HubScreen::save() {
    if (auto saved = game::saveGame(*session_.state, session_.saveFile, session_.content.mods())) {
        session_.dirty = false;
        session_.saveOwned = true;
        session_.lastMessage = "Game saved.";
    } else {
        session_.lastMessage = describe(saved.error());
    }
    message_.emplace(session_.lastMessage);
}

Transition HubScreen::quit() {
    if (!session_.dirty)
        return Quit{};
    asking_ = Asking::QuitUnsaved;
    confirm_.emplace("Quit without saving?");
    return Stay{};
}

Transition HubScreen::update(std::span<const engine::KeyEvent> keys, double) {
    menu_.setItems(menuItems());
    for (const engine::KeyEvent& key : keys) {
        if (message_) {
            if (message_->handle(key))
                message_.reset();
            continue;
        }
        if (confirm_) {
            const auto answer = confirm_->handle(key);
            if (!answer)
                continue;
            confirm_.reset();
            if (*answer && asking_ == Asking::QuitUnsaved)
                return Quit{};
            if (*answer && asking_ == Asking::ReplaceSave)
                save();
            continue;
        }
        if (key.key == engine::Key::Escape) {
            if (Transition next = quit(); !std::holds_alternative<Stay>(next))
                return next;
            continue;
        }
        switch (menu_.handle(key).value_or(-1)) {
        case kSave: {
            std::error_code ec;
            if (!session_.saveOwned && std::filesystem::exists(session_.saveFile, ec)) {
                asking_ = Asking::ReplaceSave;
                confirm_.emplace("Replace the existing save?");
            } else {
                save();
            }
            break;
        }
        case kQuit:
            if (Transition next = quit(); !std::holds_alternative<Stay>(next))
                return next;
            break;
        case kTraining:
            return Push{std::make_unique<TrainingSetupScreen>(session_)};
        case kMission:
            return Push{std::make_unique<MissionSelectScreen>(session_)};
        case kTown:
            return Push{std::make_unique<TownMenuScreen>(session_)};
        default:
            break;
        }
    }
    return Stay{};
}

engine::Size HubScreen::canvasSize(engine::Size terminal) const {
    return portraitOr(session_, terminal,
                      artCanvas(session_, terminal,
                                {std::format("hero_{}.sprite", session_.state->hero.pathId)}));
}

void HubScreen::drawPortrait(engine::Framebuffer& frame) const {
    const game::Hero& hero = session_.state->hero;
    const game::Combatant c = game::heroCombatant(hero, session_.content);
    const HeaderStat stats[] = {
        {"LEVEL", std::to_string(hero.level)},
        {"HP", std::format("{}/{}", hero.hp, c.maxHp), hpColor(hero.hp, c.maxHp)},
        {"FERVOR", std::format("{}/{}", hero.mp, c.maxMp), palette::kMp},
        {"GOLD", std::to_string(hero.gold), palette::kGold},
    };
    drawHeader(frame,
               std::format("{} · {}", upper(hero.name), session_.content.path(hero.pathId).name),
               stats);
    drawBar(frame, 1, 4, 46, hero.hp, c.maxHp, hpColor(hero.hp, c.maxHp));
    const Art art = compactArt(session_, std::format("hero_{}.sprite", hero.pathId));
    art.draw(frame, (frame.width() - art.width()) / 2, portrait::kSceneTop + 1);
    drawText(frame, 1, portrait::kMenuTop - 1, session_.lastMessage, palette::kText,
             frame.width() - 2);
    drawButtonMenu(frame, menu_);
}

void HubScreen::draw(engine::Framebuffer& frame) const {
    frame.clear(palette::kBg);
    if (isPortrait(frame)) {
        drawPortrait(frame);
        if (message_)
            message_->draw(frame);
        if (confirm_)
            confirm_->draw(frame);
        return;
    }
    const game::Hero& hero = session_.state->hero;
    drawStatusPanel(frame, hero, session_.content);
    const std::string heroFile = std::format("hero_{}.sprite", hero.pathId);
    if (isLarge(frame)) {
        largeArt(session_, heroFile).draw(frame, 4, 7);
        menu_.draw(frame, 60, 12);
    } else {
        compactArt(session_, heroFile).draw(frame, 4, 6);
        menu_.draw(frame, 34, 7);
    }
    drawText(frame, 2, frame.height() - 2, session_.lastMessage, palette::kText,
             frame.width() - 4);
    if (message_)
        message_->draw(frame);
    if (confirm_)
        confirm_->draw(frame);
}

} // namespace heroes::app
