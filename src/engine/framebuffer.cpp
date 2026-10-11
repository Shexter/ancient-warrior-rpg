#include "engine/framebuffer.hpp"

#include <algorithm>

namespace heroes::engine {

Framebuffer::Framebuffer(int width, int height)
    : width_(std::max(0, width)), height_(std::max(0, height)),
      cells_(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_)) {}

void Framebuffer::set(int x, int y, const Cell& cell) {
    if (inBounds(x, y))
        at(x, y) = cell;
}

void Framebuffer::clear(Rgb bg) {
    Cell blank;
    blank.bg = bg;
    std::ranges::fill(cells_, blank);
}

void Framebuffer::text(int x, int y, std::string_view ascii, Rgb fg, std::optional<Rgb> bg) {
    for (std::size_t i = 0; i < ascii.size(); ++i) {
        const int cx = x + static_cast<int>(i);
        if (!inBounds(cx, y))
            continue;
        const auto ch = static_cast<unsigned char>(ascii[i]);
        Cell& cell = at(cx, y);
        cell.glyph = ch < 0x80 ? static_cast<char32_t>(ch) : U'?';
        cell.fg = fg;
        if (bg)
            cell.bg = *bg;
    }
}

void Framebuffer::fillRect(int x, int y, int w, int h, Cell cell) {
    for (int cy = y; cy < y + h; ++cy)
        for (int cx = x; cx < x + w; ++cx)
            set(cx, cy, cell);
}

void Framebuffer::box(int x, int y, int w, int h, Rgb fg) {
    if (w < 2 || h < 2)
        return;
    auto put = [&](int cx, int cy, char32_t glyph) {
        if (!inBounds(cx, cy))
            return;
        Cell& cell = at(cx, cy);
        cell.glyph = glyph;
        cell.fg = fg;
    };
    for (int cx = x + 1; cx < x + w - 1; ++cx) {
        put(cx, y, U'─');
        put(cx, y + h - 1, U'─');
    }
    for (int cy = y + 1; cy < y + h - 1; ++cy) {
        put(x, cy, U'│');
        put(x + w - 1, cy, U'│');
    }
    put(x, y, U'┌');
    put(x + w - 1, y, U'┐');
    put(x, y + h - 1, U'└');
    put(x + w - 1, y + h - 1, U'┘');
}

} // namespace heroes::engine
