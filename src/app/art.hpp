#pragma once

#include "app/screen.hpp"
#include "engine/framebuffer.hpp"
#include "engine/loop.hpp"
#include "engine/sprite.hpp"

#include <initializer_list>
#include <string_view>

namespace heroes::app {

inline constexpr engine::Size kLargeCanvas{120, 40};

// One sprite in either look, as a screen draws it. Empty when the sprite isn't loaded.
struct Art {
    const engine::Sprite* blocks = nullptr;
    const engine::GlyphSprite* glyphs = nullptr;

    explicit operator bool() const { return blocks || glyphs; }
    int width() const;  // in cells
    int height() const; // in cells
    void draw(engine::Framebuffer& frame, int x, int y, engine::SpriteTint tint = {}) const;
};

// The 80x24 sprite for `file` ("hero_fighter.sprite"), or empty.
Art compactArt(const Session& session, std::string_view file);
// The large sprite for `file` in the session's --art look, or empty.
Art largeArt(const Session& session, std::string_view file);

// The canvas for a screen with a large layout: 120x40 when --layout allows it, the terminal is
// at least that big, and every sprite in `files` has large art in the chosen look; else 80x24.
engine::Size artCanvas(const Session& session, engine::Size terminal,
                       std::initializer_list<std::string_view> files);

// Whether a frame is the large canvas (screens pick their layout by the frame they are given).
inline bool isLarge(const engine::Framebuffer& frame) {
    return frame.width() >= kLargeCanvas.width && frame.height() >= kLargeCanvas.height;
}

} // namespace heroes::app
