#include "platform/terminal.hpp"

#include <atomic>
#include <stdexcept>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace heroes::platform {

namespace {

constexpr char kEnter[] = "\x1b[?1049h\x1b[?25l";
constexpr char kLeave[] = "\x1b[0m\x1b[?25h\x1b[?1049l";

HANDLE gIn = INVALID_HANDLE_VALUE;
HANDLE gOut = INVALID_HANDLE_VALUE;
DWORD gSavedInMode = 0;
DWORD gSavedOutMode = 0;
UINT gSavedCodePage = 0;
std::atomic<bool> gActive{false};

// Idempotent. Also run from the console control handler thread.
void restoreTerminal() {
    if (!gActive.exchange(false))
        return;
    DWORD written = 0;
    WriteFile(gOut, kLeave, static_cast<DWORD>(sizeof kLeave - 1), &written, nullptr);
    SetConsoleMode(gIn, gSavedInMode);
    SetConsoleMode(gOut, gSavedOutMode);
    SetConsoleOutputCP(gSavedCodePage);
}

BOOL WINAPI onCtrl(DWORD) {
    restoreTerminal();
    return FALSE; // let the default handler end the process
}

bool printable(wchar_t c) { return c >= 0x20 && c <= 0x7E; }

} // namespace

Terminal::Terminal() {
    if (gActive.load())
        throw std::runtime_error("a Terminal already exists");
    gIn = GetStdHandle(STD_INPUT_HANDLE);
    gOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!GetConsoleMode(gIn, &gSavedInMode) || !GetConsoleMode(gOut, &gSavedOutMode))
        throw std::runtime_error("this game must be run in an interactive terminal");

    const DWORD inMode =
        (gSavedInMode & ~static_cast<DWORD>(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT)) |
        ENABLE_PROCESSED_INPUT;
    const DWORD outMode =
        gSavedOutMode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(gIn, inMode) || !SetConsoleMode(gOut, outMode))
        throw std::runtime_error("cannot configure the console");
    gSavedCodePage = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);

    SetConsoleCtrlHandler(onCtrl, TRUE);
    gActive = true;
    write(kEnter);
}

Terminal::~Terminal() {
    restoreTerminal();
    SetConsoleCtrlHandler(onCtrl, FALSE);
}

std::pair<int, int> Terminal::size() const {
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(gOut, &info))
        return {80, 24};
    return {info.srWindow.Right - info.srWindow.Left + 1,
            info.srWindow.Bottom - info.srWindow.Top + 1};
}

std::string Terminal::readAvailable() { return {}; }

std::vector<engine::KeyEvent> Terminal::readKeys() {
    std::vector<engine::KeyEvent> keys;
    DWORD pending = 0;
    if (!GetNumberOfConsoleInputEvents(gIn, &pending))
        return keys;
    while (pending-- > 0) {
        INPUT_RECORD record;
        DWORD read = 0;
        if (!ReadConsoleInputW(gIn, &record, 1, &read) || read == 0)
            break;
        if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown)
            continue;
        const KEY_EVENT_RECORD& key = record.Event.KeyEvent;
        switch (key.wVirtualKeyCode) {
        case VK_UP:
            keys.push_back({engine::Key::Up});
            break;
        case VK_DOWN:
            keys.push_back({engine::Key::Down});
            break;
        case VK_LEFT:
            keys.push_back({engine::Key::Left});
            break;
        case VK_RIGHT:
            keys.push_back({engine::Key::Right});
            break;
        case VK_RETURN:
            keys.push_back({engine::Key::Enter});
            break;
        case VK_ESCAPE:
            keys.push_back({engine::Key::Escape});
            break;
        case VK_BACK:
            keys.push_back({engine::Key::Backspace});
            break;
        case VK_TAB:
            keys.push_back({engine::Key::Tab});
            break;
        default:
            if (printable(key.uChar.UnicodeChar))
                keys.push_back({engine::Key::Char, static_cast<char>(key.uChar.UnicodeChar)});
            break;
        }
    }
    return keys;
}

void Terminal::write(std::string_view bytes) {
    while (!bytes.empty()) {
        DWORD written = 0;
        if (!WriteFile(gOut, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) ||
            written == 0)
            return;
        bytes.remove_prefix(written);
    }
}

} // namespace heroes::platform
