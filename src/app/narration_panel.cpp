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
    ticket_ = narrator->request(std::move(request));
    waited_ = 0;
    timeout_ = timeoutMs / 1000.0;
}

void NarrationPanel::tick(double dtSeconds) {
    if (text_)
        text_->tick(dtSeconds);
    waited_ += dtSeconds;
}

void NarrationPanel::take(const net::NarrationReply& reply) {
    if (ticket_ == 0 || reply.ticket != ticket_)
        return;
    if (reply.result && waited_ <= timeout_)
        text_.emplace(std::vector<std::string>{*reply.result}, width_);
    ticket_ = 0;
}

void NarrationPanel::draw(engine::Framebuffer& frame, int x, int y) const {
    if (text_)
        text_->draw(frame, x, y, palette::kAccent);
}

} // namespace heroes::app
