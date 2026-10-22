#pragma once

#include "app/widgets.hpp"
#include "game/narration/summary.hpp"
#include "net/narrator.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace heroes::app {

// Two rows of narration that type themselves out in the accent colour. A new line replaces the
// old one. With a live narrator, a line can be asked for: the built-in line shows at once and the
// narrator's reply replaces it if it comes back within the time limit.
class NarrationPanel {
  public:
    explicit NarrationPanel(int width) : width_(width) {}
    void show(const std::string& line);
    // Shows `fallback`, and sends `request` when `narrator` is set.
    void ask(const std::shared_ptr<net::Narrator>& narrator, game::narration::NarrationRequest request,
             const std::string& fallback, int timeoutMs);
    // Types on, and takes a reply (or a one-time notice, returned) from the narrator.
    std::optional<std::string> tick(double dtSeconds);
    bool empty() const { return !text_; }
    bool revealed() const { return !text_ || text_->revealed(); }
    void draw(engine::Framebuffer& frame, int x, int y) const;

  private:
    int width_;
    std::optional<Typewriter> text_;
    std::shared_ptr<net::Narrator> narrator_;
    std::uint64_t ticket_ = 0; // the reply wanted; 0 for none
    double waited_ = 0, timeout_ = 0;
};

} // namespace heroes::app
