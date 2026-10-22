#include "app/screens/quit_prompt.hpp"

#include <utility>

namespace heroes::app {

QuitPromptScreen::QuitPromptScreen(engine::Framebuffer behind) : behind_(std::move(behind)) {}

Transition QuitPromptScreen::update(std::span<const engine::KeyEvent> keys, double) {
    for (const engine::KeyEvent& key : keys) {
        if (key.key == engine::Key::Escape)
            return Pop{};
        if (const auto answer = confirm_.handle(key))
            return *answer ? Transition{Quit{}} : Transition{Pop{}};
    }
    return Stay{};
}

void QuitPromptScreen::draw(engine::Framebuffer& frame) const {
    if (frame.width() == behind_.width() && frame.height() == behind_.height())
        frame = behind_;
    else
        frame.clear(palette::kBg);
    confirm_.draw(frame);
}

engine::Size QuitPromptScreen::canvasSize(engine::Size) const {
    return {behind_.width(), behind_.height()};
}

} // namespace heroes::app
