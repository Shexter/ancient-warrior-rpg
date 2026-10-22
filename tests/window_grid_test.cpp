#include "window/grid.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>

using namespace heroes;
using namespace heroes::window;
namespace fs = std::filesystem;

TEST(WindowGrid, AWindowStandsForATerminalOfBaseCells) {
    EXPECT_EQ(terminalFor(1280, 720), (engine::Size{142, 40}));
    EXPECT_EQ(terminalFor(1, 1), (engine::Size{1, 1}));
}

TEST(WindowGrid, CanvasesFollowTheTerminalRule) {
    const engine::Size terminal{142, 40};
    EXPECT_EQ(acceptedCanvas({120, 40}, terminal), (engine::Size{120, 40}));
    EXPECT_EQ(acceptedCanvas({200, 60}, terminal), engine::kCompactCanvas); // too big
    EXPECT_EQ(acceptedCanvas({60, 20}, terminal), engine::kCompactCanvas);  // too small
    EXPECT_EQ(acceptedCanvas(engine::kPortraitCanvas, {48, 44}), engine::kPortraitCanvas);
}

TEST(WindowGrid, TheCanvasFillsTheWindowInWholePixelCellsCentred) {
    const CellLayout l = layoutFor({80, 24}, 1280, 720);
    EXPECT_EQ(l.cellW, 15);
    EXPECT_EQ(l.cellH, 30);
    EXPECT_EQ(l.originX, (1280 - 80 * 15) / 2);
    EXPECT_EQ(l.originY, 0);
    const CellLayout resized = layoutFor({80, 24}, 1920, 1080);
    EXPECT_EQ(resized.cellW, 22);
    EXPECT_EQ(layoutFor({80, 24}, 10, 10).cellW, 1); // never below 1x2
}

TEST(WindowGrid, ClicksFindTheirCell) {
    const CellLayout l = layoutFor({80, 24}, 1280, 720); // 15x30 cells from x = 40
    EXPECT_EQ(cellAt(l, 40, 0), (std::pair{0, 0}));
    EXPECT_EQ(cellAt(l, 40 + 15 * 3 + 14, 30 * 2 + 29), (std::pair{3, 2}));
    EXPECT_EQ(cellAt(l, 39, 10), std::nullopt); // the margin
    EXPECT_EQ(cellAt(l, 40 + 15 * 80, 10), std::nullopt);
}

// Every non-ASCII code point in the screens' and engine's sources is in the font atlas, so no
// glyph a screen draws comes out as a missing-glyph box.
TEST(WindowGrid, TheAtlasCoversEveryCodePointTheScreensUse) {
    const auto atlas = atlasCodepoints();
    const std::set<int> have(atlas.begin(), atlas.end());
    for (int c = 0x20; c <= 0x7E; ++c)
        EXPECT_TRUE(have.contains(c)) << c;
    for (const char* dir : {"src/app", "src/engine"})
        for (const auto& entry : fs::recursive_directory_iterator(fs::path(HEROES_SOURCE_DIR) / dir)) {
            if (!entry.is_regular_file())
                continue;
            std::ifstream in(entry.path(), std::ios::binary);
            const std::string text{std::istreambuf_iterator<char>(in), {}};
            for (std::size_t i = 0; i < text.size();) { // decode UTF-8
                const auto b = static_cast<unsigned char>(text[i]);
                const int len = b < 0x80 ? 1 : b < 0xE0 ? 2 : b < 0xF0 ? 3 : 4;
                if (len > 1 && i + len <= text.size()) {
                    int cp = b & (0xFF >> (len + 1));
                    for (int k = 1; k < len; ++k)
                        cp = (cp << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3F);
                    EXPECT_TRUE(have.contains(cp)) << std::hex << cp << " in " << entry.path();
                }
                i += len;
            }
        }
}

TEST(WindowGrid, BlocksAndBoxLinesAreDrawnAsShapes) {
    for (const char32_t g : {U'▀', U'▄', U'█', U'─', U'│', U'┌', U'┐', U'└', U'┘'})
        EXPECT_TRUE(drawnAsShape(g));
    for (const char32_t g : {U'@', U'░', U'▶'})
        EXPECT_FALSE(drawnAsShape(g));
}
