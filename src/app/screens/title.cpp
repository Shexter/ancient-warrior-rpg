#include "app/screens/title.hpp"

#include <filesystem>
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

Transition TitleScreen::update(std::span<const engine::KeyEvent> keys, double) {
    for (const engine::KeyEvent& key : keys) {
        const auto choice = menu_.handle(key);
        if (choice == kQuit)
            return Quit{};
        // New Game and Continue lead to screens added in task 10.2.
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
}

} // namespace heroes::app
