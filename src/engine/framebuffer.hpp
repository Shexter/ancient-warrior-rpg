#pragma once

#include "engine/color.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace heroes::engine {

struct Cell {
    char32_t glyph = U' ';
    Rgb fg{200, 200, 200};
    Rgb bg{0, 0, 0};
    bool operator==(const Cell&) const = default;
};

// A width x height grid of cells. Drawing calls clip silently; only at() has a precondition.
class Framebuffer {
  public:
    Framebuffer(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }
    Cell& at(int x, int y) { return cells_[index(x, y)]; }
    const Cell& at(int x, int y) const { return cells_[index(x, y)]; }

    void set(int x, int y, const Cell& cell);
    void clear(Rgb bg);
    // Non-ASCII bytes are drawn as '?'. Without `bg` the existing background is kept.
    void text(int x, int y, std::string_view ascii, Rgb fg, std::optional<Rgb> bg = {});
    void fillRect(int x, int y, int w, int h, Cell cell);
    void box(int x, int y, int w, int h, Rgb fg); // single-line frame; background kept

    bool operator==(const Framebuffer&) const = default;

  private:
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < width_ && y < height_; }
    std::size_t index(int x, int y) const {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
               static_cast<std::size_t>(x);
    }

    int width_, height_;
    std::vector<Cell> cells_;
};

} // namespace heroes::engine
