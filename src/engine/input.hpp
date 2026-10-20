#pragma once

#include <deque>
#include <optional>
#include <string_view>
#include <utility>

namespace heroes::engine {

// Click is a left-button press or a touch (terminals report taps as mouse clicks).
enum class Key { Up, Down, Left, Right, Enter, Escape, Backspace, Tab, Char, Click };

// Only terminals that report key releases (the kitty keyboard protocol, the Windows console)
// send Repeat and Release; everywhere else every key is a Press, OS key repeat included.
enum class KeyAction { Press, Repeat, Release };

struct KeyEvent {
    Key key;
    char ch = 0;      // set when key == Char (printable 0x20-0x7E)
    int x = 0, y = 0; // set when key == Click: the 0-based cell (terminal, then canvas, coordinates)
    KeyAction action = KeyAction::Press;
    bool operator==(const KeyEvent&) const = default;
};

// Decodes raw POSIX terminal bytes, including kitty keyboard protocol reports
// (`CSI code[:...] [; mods[:event]] u` and `CSI 1 ; mods:event A-D`). Each feed() is parsed on
// its own: an escape sequence split across two feeds is not reassembled.
class InputDecoder {
  public:
    void feed(std::string_view bytes);
    std::optional<KeyEvent> pop();
    // True once after a kitty-reported Ctrl+C press, which the terminal no longer turns into
    // SIGINT while the protocol is on.
    bool takeInterrupt() { return std::exchange(interrupt_, false); }

  private:
    std::deque<KeyEvent> keys_;
    bool interrupt_ = false;
};

} // namespace heroes::engine
