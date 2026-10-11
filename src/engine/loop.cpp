#include "engine/loop.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <thread>

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
    const std::string ascii =
        std::format("Please resize the terminal to at least {}x{} (now {}x{})", kCanvasWidth,
                    kCanvasHeight, termWidth, termHeight);
    std::u32string message(ascii.begin(), ascii.end());
    for (std::size_t i = 0; i < message.size(); ++i)
        if (ascii[i] == 'x' && i > 0 && ascii[i - 1] >= '0' && ascii[i - 1] <= '9')
            message[i] = U'×';
    const int x = (frame.width() - static_cast<int>(message.size())) / 2;
    const int y = frame.height() / 2;
    for (std::size_t i = 0; i < message.size(); ++i) {
        Cell cell;
        cell.glyph = message[i];
        cell.fg = Rgb{255, 255, 255};
        frame.set(std::max(0, x) + static_cast<int>(i), y, cell);
    }
}

Loop::Loop(LoopIo& io, Renderer& renderer, UpdateFn update, DrawFn draw, Rgb background)
    : io_(io), renderer_(renderer), update_(std::move(update)), draw_(std::move(draw)),
      background_(background) {}

bool Loop::frame(double dtSeconds) {
    const auto [cols, rows] = io_.size();
    std::vector<KeyEvent> keys = io_.readKeys();

    out_.clear();
    if (screen_.width() != cols || screen_.height() != rows)
        screen_ = Framebuffer(cols, rows);
    if (cols < kCanvasWidth || rows < kCanvasHeight) {
        stepper_.reset();
        pending_.clear();
        drawResizeNotice(screen_, cols, rows);
    } else {
        pending_.insert(pending_.end(), keys.begin(), keys.end());
        const int steps = stepper_.advance(dtSeconds);
        for (int i = 0; i < steps; ++i) {
            if (!update_(pending_, kStepSeconds))
                return false;
            pending_.clear();
        }
        draw_(canvas_);
        screen_.clear(background_);
        const int left = (cols - kCanvasWidth) / 2;
        const int top = (rows - kCanvasHeight) / 2;
        for (int y = 0; y < kCanvasHeight; ++y)
            for (int x = 0; x < kCanvasWidth; ++x)
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
