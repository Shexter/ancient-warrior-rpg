#include "engine/renderer.hpp"

#include <gtest/gtest.h>

#include <cctype>

#include <string>

using namespace heroes::engine;

namespace {

// Counts ESC [ row ; col H cursor moves.
int cursorMoves(const std::string& bytes) {
    int count = 0;
    for (std::size_t i = 0; i + 2 < bytes.size(); ++i) {
        if (bytes[i] != '\x1b' || bytes[i + 1] != '[')
            continue;
        std::size_t j = i + 2;
        while (j < bytes.size() &&
               (std::isdigit(static_cast<unsigned char>(bytes[j])) || bytes[j] == ';'))
            ++j;
        if (j < bytes.size() && bytes[j] == 'H' && j > i + 2)
            ++count;
    }
    return count;
}

int occurrences(const std::string& haystack, const std::string& needle) {
    int count = 0;
    for (auto pos = haystack.find(needle); pos != std::string::npos;
         pos = haystack.find(needle, pos + needle.size()))
        ++count;
    return count;
}

Framebuffer filled(int w, int h, char32_t glyph = U'.') {
    Framebuffer fb(w, h);
    Cell c;
    c.glyph = glyph;
    fb.fillRect(0, 0, w, h, c);
    return fb;
}

} // namespace

TEST(Renderer, FirstPresentWritesEveryCell) {
    Renderer r(ColorMode::TrueColor);
    std::string out;
    const PresentStats stats = r.present(filled(10, 4), out);
    EXPECT_EQ(stats.cellsWritten, 40);
    EXPECT_EQ(stats.bytesWritten, out.size());
    EXPECT_NE(out.find("\x1b[0m\x1b[2J"), std::string::npos);
}

TEST(Renderer, IdenticalFrameWritesNothing) {
    Renderer r(ColorMode::TrueColor);
    std::string out;
    r.present(filled(10, 4), out);
    out.clear();
    const PresentStats stats = r.present(filled(10, 4), out);
    EXPECT_EQ(stats.cellsWritten, 0);
    EXPECT_EQ(stats.bytesWritten, 0u);
    EXPECT_TRUE(out.empty());
}

TEST(Renderer, OneChangedCellWritesExactlyOneCellAndOneCursorMove) {
    Renderer r(ColorMode::TrueColor);
    std::string out;
    Framebuffer fb = filled(10, 4);
    r.present(fb, out);
    out.clear();
    fb.at(3, 2).glyph = U'X';
    const PresentStats stats = r.present(fb, out);
    EXPECT_EQ(stats.cellsWritten, 1);
    EXPECT_EQ(cursorMoves(out), 1);
    EXPECT_NE(out.find("\x1b[3;4H"), std::string::npos); // 1-based row;col
    EXPECT_EQ(out.find("\x1b[2J"), std::string::npos);   // no clear on a diff
    EXPECT_NE(out.find('X'), std::string::npos);
}

TEST(Renderer, AdjacentChangesShareOneCursorMoveAndOneColorSet) {
    Renderer r(ColorMode::TrueColor);
    std::string out;
    Framebuffer fb = filled(10, 2);
    r.present(fb, out);
    out.clear();
    for (int x = 2; x < 6; ++x)
        fb.at(x, 0).glyph = U'#';
    EXPECT_EQ(r.present(fb, out).cellsWritten, 4);
    EXPECT_EQ(cursorMoves(out), 1);
    EXPECT_EQ(occurrences(out, "\x1b[38;"), 1);
    EXPECT_EQ(occurrences(out, "\x1b[48;"), 1);
}

TEST(Renderer, ChangesOnSeparateRowsNeedSeparateMoves) {
    Renderer r(ColorMode::TrueColor);
    std::string out;
    Framebuffer fb = filled(10, 3);
    r.present(fb, out);
    out.clear();
    fb.at(9, 0).glyph = U'#';
    fb.at(0, 1).glyph = U'#'; // follows the last column: cursor position is not assumed
    r.present(fb, out);
    EXPECT_EQ(cursorMoves(out), 2);
}

TEST(Renderer, TrueColorAndAnsi256Escapes) {
    Framebuffer fb(1, 1);
    Cell c;
    c.glyph = U'A';
    c.fg = Rgb{255, 0, 0};
    c.bg = Rgb{16, 16, 24};
    fb.set(0, 0, c);

    Renderer tc(ColorMode::TrueColor);
    std::string out;
    tc.present(fb, out);
    EXPECT_NE(out.find("\x1b[38;2;255;0;0m"), std::string::npos);
    EXPECT_NE(out.find("\x1b[48;2;16;16;24m"), std::string::npos);

    Renderer ansi(ColorMode::Ansi256);
    out.clear();
    ansi.present(fb, out);
    EXPECT_NE(out.find("\x1b[38;5;196m"), std::string::npos);
    EXPECT_NE(out.find("\x1b[48;5;16m"), std::string::npos);
}

TEST(Renderer, Ansi256Mapping) {
    EXPECT_EQ(toAnsi256(Rgb{255, 0, 0}), 196);
    EXPECT_EQ(toAnsi256(Rgb{16, 16, 24}), 16);
    EXPECT_EQ(toAnsi256(Rgb{0, 0, 0}), 16);
    EXPECT_EQ(toAnsi256(Rgb{255, 255, 255}), 231);
    EXPECT_EQ(toAnsi256(Rgb{0, 255, 0}), 46);
    EXPECT_EQ(toAnsi256(Rgb{0, 0, 255}), 21);
}

TEST(Renderer, WritesGlyphsAsUtf8) {
    Framebuffer fb(3, 1);
    fb.at(0, 0).glyph = U'▀';
    fb.at(1, 0).glyph = U'×';
    fb.at(2, 0).glyph = U'a';
    Renderer r(ColorMode::TrueColor);
    std::string out;
    r.present(fb, out);
    EXPECT_NE(out.find("\xE2\x96\x80"), std::string::npos);
    EXPECT_NE(out.find("\xC3\x97"), std::string::npos);
    EXPECT_NE(out.find('a'), std::string::npos);
}

TEST(Renderer, DiffIsWrappedInSynchronizedUpdate) {
    Renderer r(ColorMode::TrueColor);
    std::string out;
    Framebuffer fb = filled(4, 2);
    r.present(fb, out);
    out.clear();
    fb.at(0, 0).glyph = U'Z';
    r.present(fb, out);
    EXPECT_EQ(out.rfind("\x1b[?2026h", 0), 0u);
    EXPECT_EQ(out.substr(out.size() - 8), "\x1b[?2026l");
}

TEST(Renderer, SizeChangeAndInvalidateForceFullRedraw) {
    Renderer r(ColorMode::TrueColor);
    std::string out;
    r.present(filled(6, 3), out);

    out.clear();
    EXPECT_EQ(r.present(filled(8, 3), out).cellsWritten, 24);
    EXPECT_NE(out.find("\x1b[2J"), std::string::npos);

    out.clear();
    EXPECT_EQ(r.present(filled(8, 3), out).cellsWritten, 0);
    r.invalidate();
    out.clear();
    EXPECT_EQ(r.present(filled(8, 3), out).cellsWritten, 24);
    EXPECT_NE(out.find("\x1b[2J"), std::string::npos);
}

TEST(Renderer, AppendsToExistingOutput) {
    Renderer r(ColorMode::TrueColor);
    std::string out = "prefix";
    const PresentStats stats = r.present(filled(2, 1), out);
    EXPECT_EQ(out.rfind("prefix", 0), 0u);
    EXPECT_EQ(stats.bytesWritten, out.size() - 6);
}
