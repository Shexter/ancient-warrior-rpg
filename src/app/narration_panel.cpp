#include "app/narration_panel.hpp"

namespace heroes::app {

void NarrationPanel::show(const std::string& line) {
    ticket_ = 0;
    if (!line.empty())
        text_.emplace(std::vector<std::string>{line}, width_);
}

void NarrationPanel::ask(const std::shared_ptr<net::Narrator>& narrator, game::narration::NarrationRequest request,
                         const std::string& fallback, int timeoutMs) {
    show(fallback);
    if (!narrator)
        return;
    narrator_ = narrator;
    ticket_ = narrator->request(std::move(request));
    waited_ = 0;
    timeout_ = timeoutMs / 1000.0;
}

std::optional<std::string> NarrationPanel::tick(double dtSeconds) {
    if (text_)
        text_->tick(dtSeconds);
    if (!narrator_)
        return std::nullopt;
    waited_ += dtSeconds;
    while (const auto reply = narrator_->poll())
        if (reply->ticket == ticket_ && reply->result && waited_ <= timeout_) {
            text_.emplace(std::vector<std::string>{*reply->result}, width_);
            ticket_ = 0;
        }
    return narrator_->takeNotice();
}

void NarrationPanel::draw(engine::Framebuffer& frame, int x, int y) const {
    if (text_)
        text_->draw(frame, x, y, palette::kAccent);
}

} // namespace heroes::app
