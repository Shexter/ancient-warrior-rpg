#pragma once

#include "engine/framebuffer.hpp"
#include "engine/loop.hpp"

#include <optional>
#include <span>
#include <utility>

struct Font; // raylib

namespace heroes::window {

// Where a canvas sits in the window: whole-pixel cells, twice as tall as wide, centred.
struct CellLayout {
    engine::Size canvas;
    int cellW = 1, cellH = 2;
    int originX = 0, originY = 0;
    bool operator==(const CellLayout&) const = default;
};

inline constexpr int kBaseCellW = 9, kBaseCellH = 18;

// The "terminal" a window of this many pixels stands for, measured in base-size cells.
engine::Size terminalFor(int windowW, int windowH);
// The terminal's canvas rule: the phone canvas or anything from 80x24 up that fits the terminal
// is kept; anything else becomes 80x24.
engine::Size acceptedCanvas(engine::Size wanted, engine::Size terminal);
// The largest whole-pixel cells that fit `canvas` in the window, centred (at least 1x2).
CellLayout layoutFor(engine::Size canvas, int windowW, int windowH);
// The canvas cell under a window pixel, or nullopt in the margin.
std::optional<std::pair<int, int>> cellAt(const CellLayout& layout, float px, float py);

// Code points the font atlas holds: printable ASCII plus every other one the screens draw.
std::span<const int> atlasCodepoints();
// Glyphs drawn as rectangles and lines instead of font glyphs, so they tile without seams.
bool drawnAsShape(char32_t glyph);

// Draws every cell: background, then its glyph in the foreground colour. With `skipEmpty`,
// cells whose glyph is U'\0' are left untouched (the Overworld's HUD over its scene).
void drawGrid(const engine::Framebuffer& frame, const CellLayout& layout, const Font& font,
              bool skipEmpty = false);

} // namespace heroes::window
