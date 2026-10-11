// Chunk 8 demo: a moving box and the last key pressed. Replaced by the real app in chunk 10.
#include "engine/loop.hpp"
#include "engine/renderer.hpp"
#include "platform/env.hpp"
#include "platform/terminal.hpp"

#include <cstring>
#include <exception>
#include <format>
#include <iostream>
#include <string>

namespace {

using namespace heroes::engine;

constexpr int kBoxWidth = 12;
constexpr int kBoxHeight = 5;

struct Demo {
    double x = 2;
    double speed = 20; // cells per second
    std::string lastKey = "(none)";
};

std::string keyName(const KeyEvent& key) {
    switch (key.key) {
    case Key::Up:
        return "Up";
    case Key::Down:
        return "Down";
    case Key::Left:
        return "Left";
    case Key::Right:
        return "Right";
    case Key::Enter:
        return "Enter";
    case Key::Escape:
        return "Escape";
    case Key::Backspace:
        return "Backspace";
    case Key::Tab:
        return "Tab";
    case Key::Char:
        return std::format("'{}'", key.ch);
    }
    return "?";
}

// Returns false when the player pressed q.
bool update(Demo& demo, std::span<const KeyEvent> keys, double dt) {
    for (const KeyEvent& key : keys) {
        if (key.key == Key::Char && key.ch == 'q')
            return false;
        demo.lastKey = keyName(key);
    }
    demo.x += demo.speed * dt;
    const double right = kCanvasWidth - kBoxWidth - 2;
    if (demo.x > right) {
        demo.x = right;
        demo.speed = -demo.speed;
    } else if (demo.x < 2) {
        demo.x = 2;
        demo.speed = -demo.speed;
    }
    return true;
}

void draw(const Demo& demo, Framebuffer& frame) {
    frame.clear(Rgb{10, 12, 24});
    frame.text(2, 1, "Ancient Warrior RPG - terminal demo", Rgb{240, 220, 140});
    frame.text(2, 3, std::format("Last key: {}", demo.lastKey), Rgb{200, 200, 200});
    frame.text(2, 4, "Press q to quit", Rgb{120, 120, 140});
    const int x = static_cast<int>(demo.x);
    frame.fillRect(x, 10, kBoxWidth, kBoxHeight, Cell{U' ', Rgb{0, 0, 0}, Rgb{40, 70, 140}});
    frame.box(x, 10, kBoxWidth, kBoxHeight, Rgb{150, 190, 255});
}

// Headless: bytes per frame for the demo with diff rendering vs a full redraw every frame.
int printStats() {
    constexpr int kFrames = 120;
    Demo demo;
    Framebuffer frame(kCanvasWidth, kCanvasHeight);
    Renderer diff(ColorMode::TrueColor);
    std::size_t diffBytes = 0, fullBytes = 0, diffCells = 0;
    for (int i = 0; i < kFrames; ++i) {
        update(demo, {}, kStepSeconds);
        draw(demo, frame);
        std::string out;
        const PresentStats stats = diff.present(frame, out);
        diffBytes += stats.bytesWritten;
        diffCells += static_cast<std::size_t>(stats.cellsWritten);
        Renderer full(ColorMode::TrueColor);
        std::string fullOut;
        fullBytes += full.present(frame, fullOut).bytesWritten;
    }
    std::cout << std::format("frames: {}  full redraw: {} bytes/frame  diff: {} bytes/frame "
                             "({} cells/frame)\n",
                             kFrames, fullBytes / kFrames, diffBytes / kFrames,
                             diffCells / kFrames);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 1 && std::strcmp(argv[1], "--demo-stats") == 0)
            return printStats();
        Demo demo;
        heroes::platform::Terminal terminal;
        Renderer renderer(heroes::platform::detectColorMode());
        Loop loop(
            terminal, renderer,
            [&](std::span<const KeyEvent> keys, double dt) { return update(demo, keys, dt); },
            [&](Framebuffer& frame) { draw(demo, frame); });
        return loop.run();
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
