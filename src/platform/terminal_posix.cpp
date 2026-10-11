#include "platform/terminal.hpp"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <stdexcept>

#include <errno.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace heroes::platform {

namespace {

constexpr char kEnter[] = "\x1b[?1049h\x1b[?25l";
constexpr char kLeave[] = "\x1b[0m\x1b[?25h\x1b[?1049l";
constexpr int kSignals[] = {SIGINT, SIGTERM, SIGHUP};

termios gSaved;
std::atomic<bool> gActive{false};
bool gAtexitRegistered = false;

// Safe to call from a signal handler (only write and tcsetattr) and idempotent.
void restoreTerminal() {
    if (!gActive.exchange(false))
        return;
    const ssize_t ignored = write(STDOUT_FILENO, kLeave, sizeof kLeave - 1);
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
    write(kEnter);
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
