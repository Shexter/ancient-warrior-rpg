#pragma once

#include <deque>
#include <optional>
#include <string_view>

namespace heroes::engine {

enum class Key { Up, Down, Left, Right, Enter, Escape, Backspace, Tab, Char };

struct KeyEvent {
    Key key;
    char ch = 0; // set when key == Char (printable 0x20-0x7E)
    bool operator==(const KeyEvent&) const = default;
};

// Decodes raw POSIX terminal bytes. Each feed() is parsed on its own: an escape sequence split
// across two feeds is not reassembled.
class InputDecoder {
  public:
    void feed(std::string_view bytes);
    std::optional<KeyEvent> pop();

  private:
    std::deque<KeyEvent> keys_;
};

} // namespace heroes::engine
