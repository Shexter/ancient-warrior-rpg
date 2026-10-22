// Needs a display (label window-display): opens a hidden window and draws a real screen.
#include "app/app.hpp"
#include "app/screens/title.hpp"
#include "app/screens/overworld.hpp"
#include "screen_harness.hpp"
#include "window/grid.hpp"
#include "window/scene.hpp"

#include "raylib.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
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

namespace {

Font loadFont() {
    const auto codepoints = heroes::window::atlasCodepoints();
    return LoadFontEx((fs::path(HEROES_SOURCE_DIR) / "assets/fonts/JetBrainsMono-Regular.ttf").string().c_str(),
                      48, const_cast<int*>(codepoints.data()), static_cast<int>(codepoints.size()));
}

// Draws the Overworld `frames` times into a w x h target; the average milliseconds per frame.
double renderOverworld(const OverworldScreen& screen, const Font& font, int w, int h, int frames,
                       Image* last) {
    RenderTexture2D target = LoadRenderTexture(w, h);
    heroes::window::Scene scene;
    Framebuffer hud(engine::kCanvasWidth, engine::kCanvasHeight);
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < frames; ++i) {
        BeginTextureMode(target);
        ClearBackground({4, 6, 9, 255});
        heroes::window::drawOverworld(screen, hud, scene, font, static_cast<float>(i) / 60, 1.0f / 60, 0, w, h);
        EndTextureMode();
        BeginDrawing(); // a swap per frame, so the GPU keeps up with the timing
        EndDrawing();
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    if (last) {
        *last = LoadImageFromTexture(target.texture);
        ImageFlipVertical(last);
    }
    UnloadRenderTexture(target);
    return ms / frames;
}

} // namespace

// Renders the starter area. With HEROES_SNAPSHOT_DIR set it is saved for looking at; with
// HEROES_BENCH set, 600 frames are timed at 1280x720 and 1920x1080 (docs/benchmarks.md).
TEST(WindowDisplay, TheOverworldSceneDrawsUnderTheHud) {
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(1280, 720, "test");
    ASSERT_TRUE(IsWindowReady());
    Font font = loadFont();
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);

    Harness h("window_display_world");
    h.session.large = shippedLargeSprites();
    h.startAtHub(freshGame());
    ASSERT_TRUE(h.press({kHubExplore}));
    const auto& screen = dynamic_cast<const OverworldScreen&>(*h.app->top());
    Image image{};
    renderOverworld(screen, font, 1280, 720, 1, &image);
    if (const char* dir = std::getenv("HEROES_SNAPSHOT_DIR"))
        ExportImage(image, (fs::path(dir) / "window_overworld.png").string().c_str());
    // The HUD's status row is drawn over the scene in the panel colour (top-left cell).
    const auto layout = heroes::window::layoutFor(engine::kCompactCanvas, 1280, 720);
    const Color panel = GetImageColor(image, layout.originX + 1, layout.originY + 1);
    EXPECT_EQ((engine::Rgb{panel.r, panel.g, panel.b}), palette::kPanel);
    // The scene shows through below it: some pixel in the middle is lit and not the panel.
    const Color middle = GetImageColor(image, 640, 400);
    EXPECT_NE((engine::Rgb{middle.r, middle.g, middle.b}), palette::kPanel);
    UnloadImage(image);

    if (std::getenv("HEROES_BENCH"))
        for (const auto& [w, h2] : {std::pair{1280, 720}, std::pair{1920, 1080}})
            std::printf("overworld %dx%d: %.2f ms/frame over 600 frames\n", w, h2,
                        renderOverworld(screen, font, w, h2, 600, nullptr));
    UnloadFont(font);
    CloseWindow();
}
