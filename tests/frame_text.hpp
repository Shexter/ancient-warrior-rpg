#pragma once

#include "engine/framebuffer.hpp"

#include <string>

// Reads a framebuffer row back as UTF-8 so screen tests can assert on what is drawn.
inline std::string rowText(const heroes::engine::Framebuffer& frame, int y) {
    std::string out;
    for (int x = 0; x < frame.width(); ++x) {
        const char32_t c = frame.at(x, y).glyph;
        if (c < 0x80) {
            out += static_cast<char>(c);
        } else if (c < 0x800) {
            out += static_cast<char>(0xC0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            out += static_cast<char>(0xE0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return out;
}

// `width` cells starting at (x, y), as UTF-8 (unlike substr on rowText, immune to wide bytes).
inline std::string cellText(const heroes::engine::Framebuffer& frame, int x, int y, int width) {
    heroes::engine::Framebuffer one(width, 1);
    for (int i = 0; i < width; ++i)
        one.at(i, 0) = frame.at(x + i, y);
    return rowText(one, 0);
}

inline bool frameContains(const heroes::engine::Framebuffer& frame, const std::string& needle) {
    for (int y = 0; y < frame.height(); ++y)
        if (rowText(frame, y).find(needle) != std::string::npos)
            return true;
    return false;
}
