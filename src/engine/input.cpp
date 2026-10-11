#include "engine/input.hpp"

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
            // ESC [ ... final  or  ESC O final. Arrows are kept, everything else is dropped.
            const bool csi = bytes[i + 1] == '[';
            std::size_t j = i + 2;
            if (csi)
                while (j < bytes.size() && static_cast<unsigned char>(bytes[j]) < 0x40)
                    ++j; // parameter and intermediate bytes
            if (j < bytes.size()) {
                if (const auto arrow = arrowFor(bytes[j]))
                    keys_.push_back({*arrow});
            }
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
