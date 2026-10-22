#include "app/screens/hub.hpp"
#include "app/art.hpp"

#include "app/app.hpp"
#include "app/screens/expedition.hpp"
#include "app/screens/mission.hpp"
#include "app/screens/narrator_setup.hpp"
#include "app/screens/overworld.hpp"
#include "app/screens/town.hpp"
#include "app/screens/training.hpp"
#include "game/estimate.hpp"
#include "game/hero.hpp"
#include "game/save.hpp"

#include <algorithm>
#include <format>
#include <system_error>

namespace heroes::app {

namespace {

enum Choice { kExplore, kTraining, kMission, kExpedition, kTown, kSave, kSettings, kQuit };

} // namespace

HubScreen::HubScreen(Session& session) : session_(session), menu_(menuItemsWithSettings()) { refreshGoal(); }

void HubScreen::refreshGoal() {
    if (goalState_ == session_.state)
        return;
    goalState_ = session_.state;
    goal_ = game::nextGoal(*session_.state, session_.content, session_.scripts);
}

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

    std::string exploreNote = adventureNote;
    if (exploreNote.empty() && portrait_)
        exploreNote = "Needs an 80×24 screen";
    else if (exploreNote.empty() && !session_.world)
        exploreNote = "No world loaded";

    std::string expeditionNote = adventureNote;
    if (expeditionNote.empty() && !session_.dungeon)
        expeditionNote = "No dungeon loaded";
    const bool canExpedition = expeditionNote.empty();
    if (canExpedition && session_.state->expedition)
        expeditionNote = "In progress";

    return {{"Explore", exploreNote.empty(), exploreNote},
            {"Training", adventureNote.empty(), adventureNote},
            {"Mission", missionNote.empty(), missionNote},
            {"Expedition", canExpedition, expeditionNote},
            {"Town"},
            {"Save"},
            {"Quit"}};
}

std::vector<Menu::Item> HubScreen::menuItemsWithSettings() const {
    std::vector<Menu::Item> items = menuItems();
    if (!session_.settingsFile.empty()) // the game always has settings; tests usually don't
        items.insert(items.end() - 1, Menu::Item{"Settings"});
    return items;
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
    menu_.setItems(menuItemsWithSettings());
    refreshGoal();
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
        int choice = menu_.handle(key).value_or(-1);
        if (session_.settingsFile.empty() && choice == kSettings)
            choice = kQuit; // no Settings row: the last row is Quit
        switch (choice) {
        case kSettings:
            return Push{std::make_unique<SettingsScreen>(session_)};
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
        case kExplore:
            return Push{std::make_unique<OverworldScreen>(session_)};
        case kTraining:
            return Push{std::make_unique<TrainingSetupScreen>(session_)};
        case kMission:
            return Push{std::make_unique<MissionSelectScreen>(session_)};
        case kExpedition:
            return Push{std::make_unique<ExpeditionScreen>(session_)};
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
    drawHeroHeader(frame,
                   std::format("{} · {}", upper(hero.name),
                               session_.content.path(hero.pathId).name),
                   hero);
    const Art art = compactArt(session_, std::format("hero_{}.sprite", hero.pathId));
    art.draw(frame, (frame.width() - art.width()) / 2, portrait::kSceneTop + 1);
    drawText(frame, 1, portrait::kMenuTop - 2, goal_, palette::kText, frame.width() - 2);
    drawText(frame, 1, portrait::kMenuTop - 1, session_.lastMessage, palette::kText,
             frame.width() - 2);
    drawButtonMenu(frame, menu_);
}

void HubScreen::draw(engine::Framebuffer& frame) const {
    frame.clear(palette::kBg);
    portrait_ = isPortrait(frame);
    if (portrait_) {
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
    drawText(frame, 2, frame.height() - 3, goal_, palette::kText, frame.width() - 4);
    drawText(frame, 2, frame.height() - 2, session_.lastMessage, palette::kText,
             frame.width() - 4);
    if (message_)
        message_->draw(frame);
    if (confirm_)
        confirm_->draw(frame);
}

} // namespace heroes::app
