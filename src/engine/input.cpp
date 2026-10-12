#include "engine/input.hpp"

#include <algorithm>

namespace heroes::engine {

namespace {

std::optional<Key> arrowFor(char final) {
    switch (final) {
    case 'A':
        return Key::Up;
    case 'B':
        return Key::Down;
    case 'C':
        return Key::Right;
    case 'D':
        return Key::Left;
    default:
        return std::nullopt;
    }
}

// An SGR mouse report's parameters ("<b;x;y" before the final M or m) as a left press, if it is
// one. Releases, other buttons, motion and wheel events are ignored; modifier bits are allowed.
std::optional<KeyEvent> sgrClick(std::string_view params, char final) {
    if (final != 'M' || params.empty() || params[0] != '<')
        return std::nullopt;
    int values[3] = {};
    int count = 0;
    bool digits = false;
    for (const char c : params.substr(1)) {
        if (c >= '0' && c <= '9' && count < 3) {
            if (values[count] > 100000)
                return std::nullopt;
            values[count] = values[count] * 10 + (c - '0');
            digits = true;
        } else if (c == ';' && digits && count < 2) {
            ++count;
            digits = false;
        } else {
            return std::nullopt;
        }
    }
    constexpr int kModifiers = 4 | 8 | 16;
    if (count != 2 || !digits || (values[0] & ~kModifiers) != 0 || values[1] < 1 || values[2] < 1)
        return std::nullopt;
    return KeyEvent{Key::Click, 0, values[1] - 1, values[2] - 1};
}

} // namespace

void InputDecoder::feed(std::string_view bytes) {
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const auto b = static_cast<unsigned char>(bytes[i]);
        if (b == 0x1B) {
            const bool hasNext = i + 1 < bytes.size();
            if (!hasNext || (bytes[i + 1] != '[' && bytes[i + 1] != 'O')) {
                keys_.push_back({Key::Escape});
                continue;
            }
            // ESC [ ... final  or  ESC O final. Arrows and SGR left clicks are kept, everything
            // else is dropped.
            const bool csi = bytes[i + 1] == '[';
            std::size_t j = i + 2;
            if (csi)
                while (j < bytes.size() && static_cast<unsigned char>(bytes[j]) < 0x40)
                    ++j; // parameter and intermediate bytes
            if (j < bytes.size()) {
                if (const auto arrow = arrowFor(bytes[j]))
                    keys_.push_back({*arrow});
                else if (const auto click = sgrClick(bytes.substr(i + 2, j - i - 2), bytes[j]);
                         csi && click)
                    keys_.push_back(*click);
            }
            // A legacy (non-SGR) mouse report, ESC [ M plus three raw bytes, is skipped whole so
            // its bytes don't turn into keys.
            if (csi && j == i + 2 && j < bytes.size() && bytes[j] == 'M')
                j = std::min(j + 3, bytes.size() - 1);
            i = j;
        } else if (b == '\r' || b == '\n') {
            keys_.push_back({Key::Enter});
        } else if (b == 127 || b == 8) {
            keys_.push_back({Key::Backspace});
        } else if (b == '\t') {
            keys_.push_back({Key::Tab});
        } else if (b >= 0x20 && b <= 0x7E) {
            keys_.push_back({Key::Char, static_cast<char>(b)});
        }
    }
}

std::optional<KeyEvent> InputDecoder::pop() {
    if (keys_.empty())
        return std::nullopt;
    const KeyEvent key = keys_.front();
    keys_.pop_front();
    return key;
}

} // namespace heroes::engine
