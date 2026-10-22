#pragma once

#include "engine/framebuffer.hpp"
#include "engine/input.hpp"
#include "engine/renderer.hpp"

#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace heroes::engine {

inline constexpr int kCanvasWidth = 80;
inline constexpr int kCanvasHeight = 24;
inline constexpr double kStepSeconds = 1.0 / 60.0;
inline constexpr int kMaxStepsPerFrame = 5;

struct Size {
    int width = 0, height = 0;
    bool operator==(const Size&) const = default;
};
inline constexpr Size kCompactCanvas{kCanvasWidth, kCanvasHeight};
// The phone (portrait) canvas: header, scene, menu and a tap pad, top to bottom.
inline constexpr Size kPortraitCanvas{48, 44};

// What the loop needs from a terminal. platform::Terminal implements it; tests use a fake.
class LoopIo {
  public:
    virtual ~LoopIo() = default;
    virtual std::pair<int, int> size() const = 0; // cols, rows
    virtual std::vector<KeyEvent> readKeys() = 0; // non-blocking
    virtual void write(std::string_view bytes) = 0;
    // Asks for key Repeat and Release events (on) or plain key presses (off), where the
    // terminal can switch. Terminals that always report them, or never can, ignore it.
    virtual void setKeyActions(bool /*on*/) {}
};

// Turns variable frame times into whole fixed steps. At most kMaxStepsPerFrame per call; time
// beyond that is dropped so a stall does not cause a burst of catch-up updates.
class FixedStepper {
  public:
    int advance(double dtSeconds);
    void reset() { accumulator_ = 0; }
    // How far into the next step the leftover time is, 0 to 1.
    double alpha() const { return accumulator_ / kStepSeconds; }

  private:
    double accumulator_ = 0;
};

// Fills `frame` (sized to the real terminal) with the resize prompt, word-wrapped to its width
// and centered.
void drawResizeNotice(Framebuffer& frame, int termWidth, int termHeight);

class Loop {
  public:
    // Returns false to quit. Keys reach the first step after they arrive, once.
    using UpdateFn = std::function<bool(std::span<const KeyEvent>, double dtSeconds)>;
    // Draws the full canvas (sized by CanvasSizeFn, 80x24 by default).
    using DrawFn = std::function<void(Framebuffer&)>;
    // The canvas wanted for a terminal of the given size, asked before and after each frame's
    // updates. Anything but the portrait canvas or 80x24 and up, or a size larger than the
    // terminal, falls back to 80x24; if that doesn't fit either, the resize prompt shows.
    using CanvasSizeFn = std::function<Size(Size terminal)>;

    // In a terminal larger than the canvas the canvas is centered and the margin filled with
    // `background`.
    Loop(LoopIo& io, Renderer& renderer, UpdateFn update, DrawFn draw, Rgb background = {},
         CanvasSizeFn canvasSize = {});

    // One iteration without sleeping. Returns false when the update asked to quit.
    bool frame(double dtSeconds);
    // Runs frames at 60 Hz until the update asks to quit. Returns 0.
    int run();

  private:
    LoopIo& io_;
    Renderer& renderer_;
    UpdateFn update_;
    DrawFn draw_;
    Rgb background_;
    CanvasSizeFn canvasSize_;
    FixedStepper stepper_;
    Framebuffer canvas_{kCanvasWidth, kCanvasHeight}; // reallocated only when its size changes
    Framebuffer screen_{0, 0}; // terminal-sized; reallocated only when the size changes
    std::vector<KeyEvent> pending_;
    int canvasLeft_ = 0, canvasTop_ = 0; // where the canvas was last placed in the terminal
    std::string out_;
};

} // namespace heroes::engine
