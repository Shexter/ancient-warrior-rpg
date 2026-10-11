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

inline bool frameContains(const heroes::engine::Framebuffer& frame, const std::string& needle) {
    for (int y = 0; y < frame.height(); ++y)
        if (rowText(frame, y).find(needle) != std::string::npos)
            return true;
    return false;
}
