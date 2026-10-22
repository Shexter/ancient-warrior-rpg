#include "window/grid.hpp"

#include "raylib.h"

#include <algorithm>
#include <array>

namespace heroes::window {

namespace {

// Printable ASCII, then the non-ASCII code points used in src/app and src/engine (a test checks
// this list against the sources).
constexpr auto kCodepoints = [] {
    constexpr std::array<int, 32> extra = {
        0xA7,   0xB1,   0xB7,   0xBB,   0xD7,   0x2013, 0x2014, 0x2022, 0x2026, 0x2190, 0x2191, 0x2192,
        0x2193, 0x2248, 0x232B, 0x2423, 0x2500, 0x2502, 0x250C, 0x2510, 0x2514, 0x2518, 0x2580,
        0x2584, 0x2588, 0x2591, 0x25B2, 0x25B6, 0x25BC, 0x25C0, 0x25CF, 0x2715};
    std::array<int, 95 + extra.size()> all{};
    for (int c = 0x20; c <= 0x7E; ++c)
        all[static_cast<std::size_t>(c - 0x20)] = c;
    for (std::size_t i = 0; i < extra.size(); ++i)
        all[95 + i] = extra[i];
    return all;
}();

Color toColor(engine::Rgb c) { return {c.r, c.g, c.b, 255}; }

// Box-drawing lines through the middle of the cell, one pixel wider than a hairline.
void drawShape(char32_t glyph, int x, int y, int w, int h, Color fg) {
    const int midX = x + w / 2, midY = y + h / 2, t = std::max(1, w / 8);
    const auto hline = [&](int x0, int x1) { DrawRectangle(x0, midY - t / 2, x1 - x0, t, fg); };
    const auto vline = [&](int y0, int y1) { DrawRectangle(midX - t / 2, y0, t, y1 - y0, fg); };
    switch (glyph) {
    case U'▀':
        DrawRectangle(x, y, w, h / 2, fg);
        break;
    case U'▄':
        DrawRectangle(x, y + h / 2, w, h - h / 2, fg);
        break;
    case U'█':
        DrawRectangle(x, y, w, h, fg);
        break;
    case U'─':
        hline(x, x + w);
        break;
    case U'│':
        vline(y, y + h);
        break;
    case U'┌':
        hline(midX, x + w), vline(midY, y + h);
        break;
    case U'┐':
        hline(x, midX + t), vline(midY, y + h);
        break;
    case U'└':
        hline(midX, x + w), vline(y, midY + t);
        break;
    case U'┘':
        hline(x, midX + t), vline(y, midY + t);
        break;
    default:
        break;
    }
}

} // namespace

engine::Size terminalFor(int windowW, int windowH) {
    return {std::max(1, windowW / kBaseCellW), std::max(1, windowH / kBaseCellH)};
}

engine::Size acceptedCanvas(engine::Size wanted, engine::Size terminal) {
    const bool fits = wanted.width <= terminal.width && wanted.height <= terminal.height;
    const bool shape = wanted == engine::kPortraitCanvas ||
                       (wanted.width >= engine::kCanvasWidth && wanted.height >= engine::kCanvasHeight);
    return fits && shape ? wanted : engine::kCompactCanvas;
}

CellLayout layoutFor(engine::Size canvas, int windowW, int windowH) {
    CellLayout l;
    l.canvas = canvas;
    l.cellW = std::max(1, std::min(windowW / canvas.width, windowH / (2 * canvas.height)));
    l.cellH = 2 * l.cellW;
    l.originX = (windowW - l.cellW * canvas.width) / 2;
    l.originY = (windowH - l.cellH * canvas.height) / 2;
    return l;
}

std::optional<std::pair<int, int>> cellAt(const CellLayout& l, float px, float py) {
    const float fx = (px - l.originX) / l.cellW, fy = (py - l.originY) / l.cellH;
    if (fx < 0 || fy < 0 || fx >= l.canvas.width || fy >= l.canvas.height)
        return std::nullopt;
    return std::pair{static_cast<int>(fx), static_cast<int>(fy)};
}

std::span<const int> atlasCodepoints() { return kCodepoints; }

bool drawnAsShape(char32_t g) {
    return g == U'▀' || g == U'▄' || g == U'█' || g == U'─' || g == U'│' || g == U'┌' ||
           g == U'┐' || g == U'└' || g == U'┘';
}

void drawGrid(const engine::Framebuffer& frame, const CellLayout& l, const Font& font,
              bool skipEmpty) {
    const float size = l.cellH * 0.92f;
    for (int cy = 0; cy < frame.height(); ++cy)
        for (int cx = 0; cx < frame.width(); ++cx) {
            const engine::Cell& cell = frame.at(cx, cy);
            if (skipEmpty && cell.glyph == U'\0')
                continue;
            const int x = l.originX + cx * l.cellW, y = l.originY + cy * l.cellH;
            DrawRectangle(x, y, l.cellW, l.cellH, toColor(cell.bg));
            if (cell.glyph == U' ' || cell.glyph == U'\0')
                continue;
            if (drawnAsShape(cell.glyph)) {
                drawShape(cell.glyph, x, y, l.cellW, l.cellH, toColor(cell.fg));
                continue;
            }
            const int cp = static_cast<int>(cell.glyph);
            const GlyphInfo info = GetGlyphInfo(font, cp);
            const float scale = size / static_cast<float>(font.baseSize);
            const float advance = (info.advanceX > 0 ? info.advanceX : font.recs[GetGlyphIndex(font, cp)].width) * scale;
            DrawTextCodepoint(font, cp, {x + (l.cellW - advance) / 2.0f, y + (l.cellH - size) / 2.0f},
                              size, toColor(cell.fg));
        }
}

} // namespace heroes::window
