#include "app/screens/title.hpp"

#include "app/app.hpp"
#include "app/screens/hub.hpp"
#include "app/screens/new_game.hpp"
#include "game/save.hpp"

#include <algorithm>
#include <filesystem>
#include <format>
#include <system_error>

namespace heroes::app {

namespace {

enum Choice { kNewGame, kContinue, kQuit };

bool saveExists(const std::filesystem::path& file) {
    std::error_code ec;
    return std::filesystem::exists(file, ec);
}

} // namespace

TitleScreen::TitleScreen(Session& session)
    : session_(session),
      menu_({{"New Game"}, {"Continue", saveExists(session.saveFile)}, {"Quit"}}) {}

Transition TitleScreen::continueGame() {
    const auto mods = game::readSaveMods(session_.saveFile);
    if (!mods) {
        message_.emplace(describe(mods.error()));
        return Stay{};
    }
    std::string missing;
    for (const game::ModRef& mod : *mods)
        if (std::ranges::find(session_.content.mods(), mod) == session_.content.mods().end())
            missing += std::format("{}{} {}", missing.empty() ? "" : ", ", mod.name, mod.version);
    if (missing.empty())
        return load("");
    missingMods_ = missing;
    confirm_.emplace(
        std::format("This save used mods that are missing: {}. Continue anyway?", missing));
    return Stay{};
}

// Loads the save. With missing mods, a load that fails on their content names them.
Transition TitleScreen::load(const std::string& missingMods) {
    auto loaded = game::loadGame(session_.saveFile, session_.content);
    if (!loaded) {
        message_.emplace(loaded.error() == game::SaveError::Corrupt && !missingMods.empty()
                             ? std::format("The save can't be loaded without these mods: {}.",
                                           missingMods)
                             : std::string(describe(loaded.error())));
        return Stay{};
    }
    session_.state = std::move(*loaded);
    session_.dirty = false;
    session_.saveOwned = true;
    session_.lastMessage = std::format("Welcome back, {}.", session_.state->hero.name);
    return Replace{std::make_unique<HubScreen>(session_)};
}

Transition TitleScreen::update(std::span<const engine::KeyEvent> keys, double) {
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
            if (*answer)
                if (Transition next = load(missingMods_); !std::holds_alternative<Stay>(next))
                    return next;
            continue; // No: the save stays untouched and the Title stays
        }
        switch (menu_.handle(key).value_or(-1)) {
        case kNewGame:
            return Push{std::make_unique<IntroScreen>(session_)};
        case kContinue:
            if (Transition next = continueGame(); !std::holds_alternative<Stay>(next))
                return next;
            break;
        case kQuit:
            return Quit{};
        default:
            break;
        }
    }
    return Stay{};
}

void TitleScreen::draw(engine::Framebuffer& frame) const {
    frame.clear(palette::kBg);
    const std::string& title = session_.content.story().title;
    const int bannerWidth = textWidth(title) + 16;
    const int bannerX = (80 - bannerWidth) / 2;
    frame.box(bannerX, 4, bannerWidth, 6, palette::kAccent);
    frame.box(bannerX + 2, 5, bannerWidth - 4, 4, palette::kBorder);
    drawCentered(frame, 6, title, palette::kAccent);
    menu_.draw(frame, 34, 14);
    drawCentered(frame, 22, "↑↓ select · Enter confirm", palette::kDim);
    if (message_)
        message_->draw(frame);
    if (confirm_)
        confirm_->draw(frame);
}

} // namespace heroes::app
