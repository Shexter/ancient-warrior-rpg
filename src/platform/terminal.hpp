#pragma once

#include "engine/input.hpp"
#include "engine/loop.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace heroes::platform {

// Alternate screen, hidden cursor, and (POSIX) SGR mouse reporting so taps arrive as clicks.
// The leave sequence undoes all of it; it is written on every exit path.
inline constexpr std::string_view kEnterSequence = "\x1b[?1049h\x1b[?25l\x1b[?1000h\x1b[?1006h";
inline constexpr std::string_view kLeaveSequence =
    "\x1b[?1006l\x1b[?1000l\x1b[0m\x1b[?25h\x1b[?1049l";

// The kitty keyboard protocol: a query (with a device-attributes request every terminal
// answers, so the wait always ends), and the push and pop of flags 1|2|8 (disambiguate, report
// event types, report every key as an escape code).
inline constexpr std::string_view kKittyQuery = "\x1b[?u\x1b[c";
inline constexpr std::string_view kKittyPush = "\x1b[>11u";
inline constexpr std::string_view kKittyPop = "\x1b[<u";
inline constexpr std::string_view kLeaveAfterKitty =
    "\x1b[<u\x1b[?1006l\x1b[?1000l\x1b[0m\x1b[?25h\x1b[?1049l"; // kKittyPop + kLeaveSequence

// What restoring the terminal writes. The pop goes only to a terminal it was pushed to: one that
// never answered the query might misread it.
constexpr std::string_view leaveSequence(bool kittyPushed) {
    return kittyPushed ? kLeaveAfterKitty : kLeaveSequence;
}

enum class KittyProbe { Waiting, Supported, Unsupported };

// Reads the replies to kKittyQuery so far: a `CSI ? flags u` before the `CSI ? ... c`
// device-attributes reply means the protocol is supported.
constexpr KittyProbe probeKittyReply(std::string_view bytes) {
    bool kitty = false;
    for (std::size_t at = bytes.find("\x1b[?"); at != std::string_view::npos;
         at = bytes.find("\x1b[?", at + 1)) {
        std::size_t end = at + 3;
        while (end < bytes.size() && ((bytes[end] >= '0' && bytes[end] <= '9') ||
                                      bytes[end] == ';' || bytes[end] == ':'))
            ++end;
        if (end == bytes.size())
            break;
        if (bytes[end] == 'u')
            kitty = true;
        else if (bytes[end] == 'c')
            return kitty ? KittyProbe::Supported : KittyProbe::Unsupported;
    }
    return KittyProbe::Waiting;
}

// RAII: the constructor enters raw mode and the alternate screen, the destructor restores both.
// Only one may exist at a time. The constructor throws std::runtime_error if the process is
// not attached to an interactive terminal. Ctrl+C and termination signals also restore it.
class Terminal final : public engine::LoopIo {
  public:
    Terminal();
    ~Terminal() override;
    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

    std::pair<int, int> size() const override; // cols, rows
    std::string readAvailable();               // non-blocking raw bytes (POSIX only)
    std::vector<engine::KeyEvent> readKeys() override;
    void write(std::string_view bytes) override;
#ifndef _WIN32
    // Pushes or pops the kitty keyboard protocol, if the terminal answered the startup query.
    void setKeyActions(bool on) override;
#endif

  private:
#ifdef _WIN32
    std::array<bool, 256> down_{}; // virtual keys held, to tell a repeat from a press
#else
    // Sends kKittyQuery and waits up to 200 ms for the replies.
    void detectKitty();

    engine::InputDecoder decoder_; // Windows decodes key events natively
    bool kitty_ = false;           // the terminal supports the kitty keyboard protocol
#endif
};

} // namespace heroes::platform
