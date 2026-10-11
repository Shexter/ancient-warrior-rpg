#pragma once

#include "engine/color.hpp"
#include "engine/framebuffer.hpp"

#include <cstddef>
#include <optional>
#include <string>

namespace heroes::engine {

struct PresentStats {
    int cellsWritten = 0;
    std::size_t bytesWritten = 0;
};

// Turns frames into escape bytes, writing only the cells that changed since the last present.
class Renderer {
  public:
    explicit Renderer(ColorMode mode) : mode_(mode) {}

    // Appends to `out`. Appends nothing when the frame is identical to the previous one.
    PresentStats present(const Framebuffer& frame, std::string& out);
    // The next present clears the screen and redraws every cell.
    void invalidate() { previous_.reset(); }

  private:
    ColorMode mode_;
    std::optional<Framebuffer> previous_;
};

} // namespace heroes::engine
