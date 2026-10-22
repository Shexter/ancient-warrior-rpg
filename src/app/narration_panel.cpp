#include "app/narration_panel.hpp"

namespace heroes::app {

void NarrationPanel::show(const std::string& line) {
    if (line.empty())
        return;
    text_.emplace(std::vector<std::string>{line}, width_);
}

void NarrationPanel::tick(double dtSeconds) {
    if (text_)
        text_->tick(dtSeconds);
}

void NarrationPanel::draw(engine::Framebuffer& frame, int x, int y) const {
    if (text_)
        text_->draw(frame, x, y, palette::kAccent);
}

} // namespace heroes::app
