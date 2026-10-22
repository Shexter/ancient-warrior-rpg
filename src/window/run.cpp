#include "window/run.hpp"

#include "app/app.hpp"
#include "app/screens/overworld.hpp"
#include "app/screens/quit_prompt.hpp"
#include "engine/renderer.hpp"
#include "window/grid.hpp"
#include "window/input.hpp"
#include "window/scene.hpp"

#include "raylib.h"

#include <string_view>
#include <utility>
#include <vector>

namespace heroes::window {

namespace {

// App wants a terminal; the window does its own input and drawing, so this one does nothing.
class WindowIo final : public engine::LoopIo {
  public:
    std::pair<int, int> size() const override { return {engine::kCanvasWidth, engine::kCanvasHeight}; }
    std::vector<engine::KeyEvent> readKeys() override { return {}; }
    void write(std::string_view) override {}
};

Color toColor(engine::Rgb c) { return {c.r, c.g, c.b, 255}; }


} // namespace

int run(app::Session& session, std::unique_ptr<app::Screen> first,
        const std::filesystem::path& fontFile) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    SetTraceLogLevel(LOG_ERROR); // box and block glyphs are drawn as shapes, so their atlas size warnings are noise
    InitWindow(1280, 720, "Heroes");
    SetWindowMinSize(800, 450);
    SetExitKey(KEY_NULL); // Esc is a game key; only the close button closes
    const auto codepoints = atlasCodepoints();
    Font font = LoadFontEx(fontFile.string().c_str(), 48, const_cast<int*>(codepoints.data()),
                           static_cast<int>(codepoints.size()));
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);

    WindowIo io;
    engine::Renderer unused(engine::ColorMode::TrueColor); // App's terminal renderer, never used
    app::App game(io, unused, std::move(first));
    engine::FixedStepper stepper;
    std::vector<engine::KeyEvent> pending; // keys go to the first step after they arrive
    engine::Framebuffer frame(engine::kCanvasWidth, engine::kCanvasHeight);
    CellLayout layout = layoutFor(engine::kCompactCanvas, GetScreenWidth(), GetScreenHeight());
    Scene scene;
    float sceneTime = 0;     // animation clock: grass sway, ripples, fireflies
    bool sceneShown = false; // the last frame drew the Overworld scene
    bool running = true;
    while (running) {
        const std::vector<engine::KeyEvent> keys = pollKeys();
        pending.insert(pending.end(), keys.begin(), keys.end());
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (sceneShown) { // the Overworld reads a click as a tile from its view's corner
                const float tile = kTilePx * sceneZoom(GetScreenHeight());
                pending.push_back({engine::Key::Click, 0, static_cast<int>(GetMouseX() / tile),
                                   static_cast<int>(GetMouseY() / tile)});
            } else if (const auto cell = cellAt(layout, GetMouseX(), GetMouseY()))
                pending.push_back({engine::Key::Click, 0, cell->first, cell->second});
        }
        if (WindowShouldClose()) {
            const bool prompting = dynamic_cast<const app::QuitPromptScreen*>(game.top()) != nullptr;
            if (!session.dirty)
                break;
            if (!prompting)
                game.push(std::make_unique<app::QuitPromptScreen>(frame));
        }

        const int steps = stepper.advance(GetFrameTime());
        for (int i = 0; i < steps && running; ++i)
            running = game.update(i == 0 ? std::span<const engine::KeyEvent>(pending)
                                         : std::span<const engine::KeyEvent>(),
                                  engine::kStepSeconds);
        if (steps > 0)
            pending.clear();
        if (!running)
            break;

        const engine::Size terminal = terminalFor(GetScreenWidth(), GetScreenHeight());
        const engine::Size canvas = acceptedCanvas(game.canvasSize(terminal), terminal);
        if (frame.width() != canvas.width || frame.height() != canvas.height)
            frame = engine::Framebuffer(canvas.width, canvas.height);
        layout = layoutFor(canvas, GetScreenWidth(), GetScreenHeight());
        sceneTime += GetFrameTime();
        const auto* overworld = dynamic_cast<const app::OverworldScreen*>(game.top());
        sceneShown = overworld != nullptr;
        if (!overworld)
            game.draw(frame);

        BeginDrawing();
        if (overworld) {
            ClearBackground({4, 6, 9, 255});
            drawOverworld(*overworld, frame, scene, font, sceneTime, GetScreenWidth(), GetScreenHeight());
        } else {
            ClearBackground(toColor(frame.at(0, 0).bg));
            drawGrid(frame, layout, font);
        }
        EndDrawing();
    }
    UnloadFont(font);
    CloseWindow();
    return 0;
}

} // namespace heroes::window
