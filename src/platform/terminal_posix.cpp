#include "platform/terminal.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <stdexcept>

#include <errno.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace heroes::platform {

namespace {

// SIGABRT covers failed asserts and std::terminate.
constexpr int kSignals[] = {SIGINT, SIGTERM, SIGHUP, SIGABRT};

termios gSaved;
std::atomic<bool> gActive{false};
std::atomic<bool> gKittyPushed{false};
bool gAtexitRegistered = false;

// Safe to call from a signal handler (only write and tcsetattr) and idempotent.
void restoreTerminal() {
    if (!gActive.exchange(false))
        return;
    const std::string_view leave = leaveSequence(gKittyPushed.exchange(false));
    const ssize_t ignored = write(STDOUT_FILENO, leave.data(), leave.size());
    (void)ignored;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &gSaved);
}

extern "C" void onFatalSignal(int sig) {
    restoreTerminal();
    signal(sig, SIG_DFL);
    raise(sig);
}

extern "C" void restoreAtExit() { restoreTerminal(); }

} // namespace

Terminal::Terminal() {
    if (gActive.load())
        throw std::runtime_error("a Terminal already exists");
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))
        throw std::runtime_error("this game must be run in an interactive terminal");
    if (tcgetattr(STDIN_FILENO, &gSaved) != 0)
        throw std::runtime_error("cannot read terminal settings");

    termios raw = gSaved;
    raw.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON | IEXTEN); // ISIG stays: Ctrl+C works
    raw.c_iflag &= ~static_cast<tcflag_t>(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0)
        throw std::runtime_error("cannot enter raw mode");

    if (!gAtexitRegistered) {
        std::atexit(restoreAtExit);
        gAtexitRegistered = true;
    }
    for (const int sig : kSignals)
        signal(sig, onFatalSignal);
    gActive = true;
    write(kEnterSequence);
    detectKitty();
}

void Terminal::detectKitty() {
    using Clock = std::chrono::steady_clock;
    constexpr auto kWait = std::chrono::milliseconds(200);
    write(kKittyQuery);
    std::string replies;
    KittyProbe probe = KittyProbe::Waiting;
    for (const auto deadline = Clock::now() + kWait;
         probe == KittyProbe::Waiting && Clock::now() < deadline;) {
        const auto left =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now());
        pollfd in{STDIN_FILENO, POLLIN, 0};
        if (poll(&in, 1, static_cast<int>(std::max<long long>(1, left.count()))) > 0)
            replies += readAvailable();
        probe = probeKittyReply(replies);
    }
    kitty_ = probe == KittyProbe::Supported;
    decoder_.feed(replies); // keys typed meanwhile still count; the replies decode to nothing
}

void Terminal::setKeyActions(bool on) {
    if (!kitty_ || on == gKittyPushed.load())
        return;
    if (on) {
        gKittyPushed = true; // flag first: a signal now pops a push that may not have landed
        write(kKittyPush);
    } else {
        write(kKittyPop);
        gKittyPushed = false;
    }
}

Terminal::~Terminal() {
    restoreTerminal();
    for (const int sig : kSignals)
        signal(sig, SIG_DFL);
}

std::pair<int, int> Terminal::size() const {
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0 || ws.ws_col == 0 || ws.ws_row == 0)
        return {80, 24};
    return {ws.ws_col, ws.ws_row};
}

std::string Terminal::readAvailable() {
    std::string bytes;
    char buf[256];
    for (;;) {
        const ssize_t n = read(STDIN_FILENO, buf, sizeof buf);
        if (n <= 0)
            break;
        bytes.append(buf, static_cast<std::size_t>(n));
        if (static_cast<std::size_t>(n) < sizeof buf)
            break;
    }
    return bytes;
}

std::vector<engine::KeyEvent> Terminal::readKeys() {
    decoder_.feed(readAvailable());
    if (decoder_.takeInterrupt())
        raise(SIGINT); // Ctrl+C as a kitty key report: quit as the terminal's own Ctrl+C would
    std::vector<engine::KeyEvent> keys;
    while (const auto key = decoder_.pop())
        keys.push_back(*key);
    return keys;
}

void Terminal::write(std::string_view bytes) {
    while (!bytes.empty()) {
        const ssize_t n = ::write(STDOUT_FILENO, bytes.data(), bytes.size());
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            return; // the terminal is gone; nothing useful left to do
        }
        bytes.remove_prefix(static_cast<std::size_t>(n));
    }
}

} // namespace heroes::platform
