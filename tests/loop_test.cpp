#include "engine/loop.hpp"

#include <gtest/gtest.h>

#include <string>
#include <utility>

using namespace heroes::engine;

namespace {

class FakeIo : public LoopIo {
  public:
    std::pair<int, int> size() const override { return {cols, rows}; }
    std::vector<KeyEvent> readKeys() override { return std::exchange(queued, {}); }
    void write(std::string_view bytes) override {
        written.append(bytes);
        ++writes;
    }

    int cols = 80, rows = 24;
    std::vector<KeyEvent> queued;
    std::string written;
    int writes = 0;
};

} // namespace

TEST(FixedStepper, OneStepPerStepOfTime) {
    FixedStepper s;
    EXPECT_EQ(s.advance(kStepSeconds), 1);
    EXPECT_EQ(s.advance(kStepSeconds * 2.5), 2);
    EXPECT_EQ(s.advance(kStepSeconds * 0.6), 1); // the leftover half step carries over
}

TEST(FixedStepper, SubStepFramesAccumulate) {
    FixedStepper s;
    EXPECT_EQ(s.advance(kStepSeconds * 0.4), 0);
    EXPECT_EQ(s.advance(kStepSeconds * 0.4), 0);
    EXPECT_EQ(s.advance(kStepSeconds * 0.4), 1);
}

TEST(FixedStepper, CapsStepsAndDropsTheExcess) {
    FixedStepper s;
    EXPECT_EQ(s.advance(10.0), kMaxStepsPerFrame);
    EXPECT_EQ(s.advance(0.0), 0); // the backlog was dropped, not carried
}

TEST(FixedStepper, SimulatedTimeIsIndependentOfFrameRate) {
    FixedStepper fast, slow;
    int fastSteps = 0, slowSteps = 0;
    for (int i = 0; i < 240; ++i)
        fastSteps += fast.advance(1.0 / 240.0);
    for (int i = 0; i < 30; ++i)
        slowSteps += slow.advance(1.0 / 30.0);
    EXPECT_NEAR(fastSteps, 60, 1);
    EXPECT_NEAR(slowSteps, 60, 1);
}

TEST(Loop, UpdateAndDrawRunAndOutputIsWritten) {
    FakeIo io;
    Renderer renderer(ColorMode::TrueColor);
    int updates = 0;
    Loop loop(
        io, renderer,
        [&](std::span<const KeyEvent>, double dt) {
            EXPECT_DOUBLE_EQ(dt, kStepSeconds);
            ++updates;
            return true;
        },
        [](Framebuffer& fb) {
            EXPECT_EQ(fb.width(), kCanvasWidth);
            EXPECT_EQ(fb.height(), kCanvasHeight);
            fb.text(0, 0, "hi", Rgb{255, 255, 255});
        });
    EXPECT_TRUE(loop.frame(kStepSeconds));
    EXPECT_EQ(updates, 1);
    EXPECT_NE(io.written.find("hi"), std::string::npos);
    const std::string afterFirst = io.written;
    EXPECT_TRUE(loop.frame(kStepSeconds));
    EXPECT_EQ(io.written, afterFirst); // identical frame: nothing more is written
}

TEST(Loop, KeysAreDeliveredOnceEvenWhenNoStepRuns) {
    FakeIo io;
    Renderer renderer(ColorMode::TrueColor);
    std::vector<std::size_t> keyCounts;
    Loop loop(
        io, renderer,
        [&](std::span<const KeyEvent> keys, double) {
            keyCounts.push_back(keys.size());
            return true;
        },
        [](Framebuffer&) {});
    io.queued = {{Key::Up}, {Key::Enter}};
    loop.frame(kStepSeconds * 0.3); // too short for a step: keys must wait
    EXPECT_TRUE(keyCounts.empty());
    loop.frame(kStepSeconds * 3); // three steps: only the first sees the keys
    EXPECT_EQ(keyCounts, (std::vector<std::size_t>{2, 0, 0}));
}

TEST(Loop, QuitStopsTheLoop) {
    FakeIo io;
    Renderer renderer(ColorMode::TrueColor);
    Loop loop(
        io, renderer, [](std::span<const KeyEvent>, double) { return false; }, [](Framebuffer&) {});
    EXPECT_FALSE(loop.frame(kStepSeconds));
}

TEST(Loop, TooSmallShowsNoticeAndSkipsUpdate) {
    FakeIo io;
    io.cols = 60;
    io.rows = 20;
    Renderer renderer(ColorMode::TrueColor);
    int updates = 0, draws = 0;
    Loop loop(
        io, renderer,
        [&](std::span<const KeyEvent>, double) {
            ++updates;
            return true;
        },
        [&](Framebuffer&) { ++draws; });
    io.queued = {{Key::Enter}};
    EXPECT_TRUE(loop.frame(kStepSeconds));
    EXPECT_EQ(updates, 0);
    EXPECT_EQ(draws, 0);
    EXPECT_NE(io.written.find("Please resize the terminal to at least 80\xC3\x97"
                              "24 (now 60\xC3\x97"
                              "20)"),
              std::string::npos);

    // Restoring the size resumes rendering with a full redraw, and the old keys are gone.
    io.cols = 80;
    io.rows = 24;
    io.written.clear();
    std::size_t keysSeen = 99;
    Loop resumed(
        io, renderer,
        [&](std::span<const KeyEvent> keys, double) {
            keysSeen = keys.size();
            return true;
        },
        [](Framebuffer&) {});
    EXPECT_TRUE(resumed.frame(kStepSeconds));
    EXPECT_EQ(keysSeen, 0u);
    EXPECT_NE(io.written.find("\x1b[2J"), std::string::npos);
}

TEST(Loop, TooSmallDropsKeysTypedWhileSmall) {
    FakeIo io;
    io.cols = 79;
    Renderer renderer(ColorMode::TrueColor);
    std::size_t keysSeen = 99;
    Loop loop(
        io, renderer,
        [&](std::span<const KeyEvent> keys, double) {
            keysSeen = keys.size();
            return true;
        },
        [](Framebuffer&) {});
    io.queued = {{Key::Enter}};
    loop.frame(kStepSeconds);
    io.cols = 80;
    loop.frame(kStepSeconds);
    EXPECT_EQ(keysSeen, 0u);
}

TEST(Loop, CanvasIsCenteredInALargerTerminal) {
    FakeIo io;
    io.cols = 100;
    io.rows = 30;
    Renderer renderer(ColorMode::TrueColor);
    char32_t corner = U'X';
    Loop loop(
        io, renderer, [](std::span<const KeyEvent>, double) { return true; },
        [&](Framebuffer& fb) { fb.at(0, 0).glyph = corner; }, Rgb{1, 2, 3});
    loop.frame(kStepSeconds);
    EXPECT_NE(io.written.find("48;2;1;2;3m"), std::string::npos); // margin uses the background

    // Changing the canvas's top-left cell rewrites exactly the centered cell (col 10, row 3).
    io.written.clear();
    corner = U'Y';
    loop.frame(kStepSeconds);
    const auto move = io.written.find("\x1b[4;11H");
    ASSERT_NE(move, std::string::npos) << io.written;
    EXPECT_EQ(io.written.find('Y'), io.written.rfind('Y')); // one cell written
    EXPECT_GT(io.written.find('Y'), move);
}

TEST(Loop, ResizeNoticeIsCenteredAndSurvivesTinyTerminals) {
    Framebuffer fb(100, 30);
    drawResizeNotice(fb, 100, 30);
    EXPECT_EQ(fb.at(0, 15).glyph, U' '); // centered, so the edge is blank
    Framebuffer tiny(10, 3);
    drawResizeNotice(tiny, 10, 3); // longer than the width: must clip, not crash
    Framebuffer empty(0, 0);
    drawResizeNotice(empty, 0, 0);
}
