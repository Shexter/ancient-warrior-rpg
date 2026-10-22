#pragma once

#include "app/widgets.hpp"

#include <optional>
#include <string>

namespace heroes::app {

// Two rows of narration that type themselves out in the accent colour (the battle log's top two
// rows). A new line replaces the old one.
class NarrationPanel {
  public:
    explicit NarrationPanel(int width) : width_(width) {}
    void show(const std::string& line);
    void tick(double dtSeconds);
    bool empty() const { return !text_; }
    bool revealed() const { return !text_ || text_->revealed(); }
    void draw(engine::Framebuffer& frame, int x, int y) const;

  private:
    int width_;
    std::optional<Typewriter> text_;
};

} // namespace heroes::app
