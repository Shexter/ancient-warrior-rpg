#include "engine/loop.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <string>
#include <thread>
#include <vector>

namespace heroes::engine {

int FixedStepper::advance(double dtSeconds) {
    accumulator_ += dtSeconds;
    int steps = 0;
    while (accumulator_ >= kStepSeconds && steps < kMaxStepsPerFrame) {
        accumulator_ -= kStepSeconds;
        ++steps;
    }
    if (accumulator_ >= kStepSeconds)
        accumulator_ = 0;
    return steps;
}

void drawResizeNotice(Framebuffer& frame, int termWidth, int termHeight) {
    frame.clear(Rgb{0, 0, 0});
    // 'x' stands in for the multiplication sign, which is not ASCII.
    const std::string ascii = std::format(
        "Please resize the terminal to at least {}x{}, or {}x{} on a phone (now {}x{})",
        kCanvasWidth, kCanvasHeight, kPortraitCanvas.width, kPortraitCanvas.height, termWidth,
        termHeight);
    std::u32string message(ascii.begin(), ascii.end());
    for (std::size_t i = 0; i < message.size(); ++i)
        if (ascii[i] == 'x' && i > 0 && ascii[i - 1] >= '0' && ascii[i - 1] <= '9')
            message[i] = U'×';

    // Greedy word wrap to the width; a word longer than a line is cut by the frame's clipping.
    const std::size_t width = static_cast<std::size_t>(std::max(1, frame.width()));
    std::vector<std::u32string> lines{U""};
    std::size_t start = 0;
    while (start < message.size()) {
        std::size_t end = message.find(U' ', start);
        if (end == std::u32string::npos)
            end = message.size();
        const std::u32string word = message.substr(start, end - start);
        if (!lines.back().empty() && lines.back().size() + 1 + word.size() > width)
            lines.emplace_back();
        if (!lines.back().empty())
            lines.back() += U' ';
        lines.back() += word;
        start = end + 1;
    }

    const int top = (frame.height() - static_cast<int>(lines.size())) / 2;
    for (std::size_t row = 0; row < lines.size(); ++row) {
        const std::u32string& line = lines[row];
        const int x = std::max(0, (frame.width() - static_cast<int>(line.size())) / 2);
        for (std::size_t i = 0; i < line.size(); ++i) {
            Cell cell;
            cell.glyph = line[i];
            cell.fg = Rgb{255, 255, 255};
            frame.set(x + static_cast<int>(i), std::max(0, top) + static_cast<int>(row), cell);
        }
    }
}

Loop::Loop(LoopIo& io, Renderer& renderer, UpdateFn update, DrawFn draw, Rgb background,
           CanvasSizeFn canvasSize)
    : io_(io), renderer_(renderer), update_(std::move(update)), draw_(std::move(draw)),
      background_(background), canvasSize_(std::move(canvasSize)) {}

bool Loop::frame(double dtSeconds) {
    const auto [cols, rows] = io_.size();
    std::vector<KeyEvent> keys = io_.readKeys();

    out_.clear();
    if (screen_.width() != cols || screen_.height() != rows)
        screen_ = Framebuffer(cols, rows);
    const auto fits = [&](Size s) { return s.width <= cols && s.height <= rows; };
    const auto choose = [&] {
        Size want = canvasSize_ ? canvasSize_({cols, rows}) : kCompactCanvas;
        const bool known = want == kPortraitCanvas ||
                           (want.width >= kCanvasWidth && want.height >= kCanvasHeight);
        return known && fits(want) ? want : kCompactCanvas;
    };
    if (!fits(choose())) {
        stepper_.reset();
        pending_.clear();
        drawResizeNotice(screen_, cols, rows);
    } else {
        // A click is translated from terminal to canvas cells using the canvas as last drawn
        // (what the player tapped); clicks on the margin are dropped.
        for (KeyEvent key : keys) {
            if (key.key == Key::Click) {
                key.x -= canvasLeft_;
                key.y -= canvasTop_;
                if (key.x < 0 || key.y < 0 || key.x >= canvas_.width() || key.y >= canvas_.height())
                    continue;
            }
            pending_.push_back(key);
        }
        const int steps = stepper_.advance(dtSeconds);
        for (int i = 0; i < steps; ++i) {
            if (!update_(pending_, kStepSeconds))
                return false;
            pending_.clear();
        }
        const Size want = choose(); // the updates may have changed screens
        if (!fits(want)) {
            stepper_.reset();
            pending_.clear();
            drawResizeNotice(screen_, cols, rows);
            renderer_.present(screen_, out_);
            if (!out_.empty())
                io_.write(out_);
            return true;
        }
        if (canvas_.width() != want.width || canvas_.height() != want.height)
            canvas_ = Framebuffer(want.width, want.height);
        draw_(canvas_);
        screen_.clear(background_);
        const int left = canvasLeft_ = (cols - want.width) / 2;
        const int top = canvasTop_ = (rows - want.height) / 2;
        for (int y = 0; y < want.height; ++y)
            for (int x = 0; x < want.width; ++x)
                screen_.at(left + x, top + y) = canvas_.at(x, y);
    }
    renderer_.present(screen_, out_);
    if (!out_.empty())
        io_.write(out_);
    return true;
}

int Loop::run() {
    using Clock = std::chrono::steady_clock;
    const auto period =
        std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(kStepSeconds));
    auto last = Clock::now();
    for (;;) {
        const auto frameStart = Clock::now();
        // Cap dt so resuming after a long pause (e.g. a stopped process) does not matter.
        const double dt = std::min(0.25, std::chrono::duration<double>(frameStart - last).count());
        last = frameStart;
        if (!frame(dt))
            return 0;
        std::this_thread::sleep_until(frameStart + period);
    }
}

} // namespace heroes::engine
