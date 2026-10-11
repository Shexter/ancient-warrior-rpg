#include "engine/renderer.hpp"

#include <charconv>

namespace heroes::engine {

namespace {

constexpr std::string_view kSyncBegin = "\x1b[?2026h";
constexpr std::string_view kSyncEnd = "\x1b[?2026l";
constexpr std::string_view kResetAndClear = "\x1b[0m\x1b[2J";

void appendInt(std::string& out, int value) {
    char buf[16];
    const auto result = std::to_chars(buf, buf + sizeof buf, value);
    out.append(buf, result.ptr);
}

void appendUtf8(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// `layer` is 38 for foreground, 48 for background.
void appendColor(std::string& out, ColorMode mode, int layer, Rgb c) {
    out += "\x1b[";
    appendInt(out, layer);
    if (mode == ColorMode::TrueColor) {
        out += ";2;";
        appendInt(out, c.r);
        out += ';';
        appendInt(out, c.g);
        out += ';';
        appendInt(out, c.b);
    } else {
        out += ";5;";
        appendInt(out, toAnsi256(c));
    }
    out += 'm';
}

void appendCursorMove(std::string& out, int x, int y) {
    out += "\x1b[";
    appendInt(out, y + 1);
    out += ';';
    appendInt(out, x + 1);
    out += 'H';
}

} // namespace

PresentStats Renderer::present(const Framebuffer& frame, std::string& out) {
    const bool full =
        !previous_ || previous_->width() != frame.width() || previous_->height() != frame.height();
    const std::size_t start = out.size();
    PresentStats stats;

    // Only open the synchronized-update block once the first changed cell is found.
    bool opened = false;
    auto open = [&] {
        if (opened)
            return;
        opened = true;
        out += kSyncBegin;
        if (full)
            out += kResetAndClear;
    };
    if (full)
        open();

    // What the terminal is known to hold after our previous write. Unknown after a reset.
    std::optional<Rgb> fg, bg;
    int cursorX = -1, cursorY = -1;
    for (int y = 0; y < frame.height(); ++y) {
        for (int x = 0; x < frame.width(); ++x) {
            const Cell& cell = frame.at(x, y);
            if (!full && cell == previous_->at(x, y))
                continue;
            open();
            if (cursorX != x || cursorY != y)
                appendCursorMove(out, x, y);
            if (fg != cell.fg) {
                appendColor(out, mode_, 38, cell.fg);
                fg = cell.fg;
            }
            if (bg != cell.bg) {
                appendColor(out, mode_, 48, cell.bg);
                bg = cell.bg;
            }
            appendUtf8(out, cell.glyph);
            ++stats.cellsWritten;
            // After the last column the cursor's position is terminal-dependent, so forget it.
            cursorX = x + 1 < frame.width() ? x + 1 : -1;
            cursorY = y;
        }
    }
    if (opened)
        out += kSyncEnd;

    previous_ = frame;
    stats.bytesWritten = out.size() - start;
    return stats;
}

} // namespace heroes::engine
