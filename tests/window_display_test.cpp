// Needs a display (label window-display): opens a hidden window and draws a real screen.
#include "app/app.hpp"
#include "app/screens/title.hpp"
#include "screen_harness.hpp"
#include "window/grid.hpp"

#include "raylib.h"

#include <gtest/gtest.h>

#include <cstdlib>

using namespace screen_test;

TEST(WindowDisplay, TheTitleScreenDrawsItsCells) {
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(1280, 720, "test");
    ASSERT_TRUE(IsWindowReady());
    const auto codepoints = heroes::window::atlasCodepoints();
    Font font = LoadFontEx((fs::path(HEROES_SOURCE_DIR) / "assets/fonts/JetBrainsMono-Regular.ttf").string().c_str(),
                           48, const_cast<int*>(codepoints.data()), static_cast<int>(codepoints.size()));
    ASSERT_GT(font.texture.id, 0u);

    Harness h("window_display");
    h.startAtTitle();
    const Framebuffer frame = h.frame();
    const auto layout = heroes::window::layoutFor(engine::kCompactCanvas, 1280, 720);
    RenderTexture2D target = LoadRenderTexture(1280, 720);
    BeginTextureMode(target);
    ClearBackground(BLACK);
    heroes::window::drawGrid(frame, layout, font);
    EndTextureMode();
    Image image = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&image);
    if (const char* dir = std::getenv("HEROES_SNAPSHOT_DIR")) // for looking at it by eye
        ExportImage(image, (fs::path(dir) / "window_title.png").string().c_str());

    // Every cell's background lands where the layout says (sampled at each cell's corner pixel),
    // and some cell with a visible glyph has a pixel in its foreground colour.
    int foregroundHits = 0;
    for (int cy = 0; cy < frame.height(); ++cy)
        for (int cx = 0; cx < frame.width(); ++cx) {
            const auto& cell = frame.at(cx, cy);
            const Color corner = GetImageColor(image, layout.originX + cx * layout.cellW, layout.originY + cy * layout.cellH);
            if (!heroes::window::drawnAsShape(cell.glyph)) {
                EXPECT_EQ((engine::Rgb{corner.r, corner.g, corner.b}), cell.bg) << cx << "," << cy;
            }
            if (cell.glyph == U' ' || heroes::window::drawnAsShape(cell.glyph))
                continue;
            for (int py = 0; py < layout.cellH && foregroundHits < 50; ++py)
                for (int px = 0; px < layout.cellW; ++px) {
                    const Color c = GetImageColor(image, layout.originX + cx * layout.cellW + px, layout.originY + cy * layout.cellH + py);
                    if (c.r == cell.fg.r && c.g == cell.fg.g && c.b == cell.fg.b) {
                        ++foregroundHits;
                        py = layout.cellH;
                        break;
                    }
                }
        }
    EXPECT_GE(foregroundHits, 50);
    UnloadImage(image);
    UnloadRenderTexture(target);
    UnloadFont(font);
    CloseWindow();
}
