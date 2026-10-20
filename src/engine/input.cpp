#include "engine/input.hpp"

#include <algorithm>
#include <array>

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

// The numbers of a CSI parameter string: fields split by ';', sub-fields by ':'. Only the first
// two sub-fields of the first two fields are kept (-1 when absent); kitty's later text field is
// ignored. nullopt for anything else, such as the '?' of a terminal's reply.
using CsiNumbers = std::array<std::array<int, 2>, 2>;
std::optional<CsiNumbers> csiNumbers(std::string_view params) {
    CsiNumbers out{{{-1, -1}, {-1, -1}}};
    std::size_t field = 0, sub = 0;
    for (const char c : params) {
        if (c == ';') {
            ++field;
            sub = 0;
        } else if (c == ':') {
            ++sub;
        } else if (c >= '0' && c <= '9') {
            if (field >= 2 || sub >= 2)
                continue;
            int& v = out[field][sub];
            if (v > 100000)
                return std::nullopt;
            v = (v < 0 ? 0 : v * 10) + (c - '0');
        } else {
            return std::nullopt;
        }
    }
    return out;
}

// The kitty event sub-field: absent or 1 press, 2 repeat, 3 release.
std::optional<KeyAction> eventAction(int event) {
    switch (event) {
    case -1:
    case 1:
        return KeyAction::Press;
    case 2:
        return KeyAction::Repeat;
    case 3:
        return KeyAction::Release;
    default:
        return std::nullopt;
    }
}

// A kitty `CSI code[:...] [; mods[:event]] u` key. Enter, Esc, Backspace, Tab and printable
// characters are kept; keys held with Alt, Ctrl or Super, and every other key, are dropped, as
// they are without the protocol. A Ctrl+C press sets `interrupt` instead.
std::optional<KeyEvent> kittyKey(std::string_view params, bool& interrupt) {
    const auto n = csiNumbers(params);
    if (!n || (*n)[0][0] < 0)
        return std::nullopt;
    const int code = (*n)[0][0];
    const int mods = (*n)[1][0] > 0 ? (*n)[1][0] - 1 : 0;
    const auto action = eventAction((*n)[1][1]);
    if (!action)
        return std::nullopt;
    constexpr int kShift = 1, kCtrl = 4, kCapsLock = 64, kNumLock = 128;
    if ((mods & kCtrl) != 0 && code == 'c') {
        interrupt = interrupt || *action == KeyAction::Press;
        return std::nullopt;
    }
    if ((mods & ~(kShift | kCapsLock | kNumLock)) != 0)
        return std::nullopt;

    KeyEvent key{Key::Char, 0, 0, 0, *action};
    switch (code) {
    case 13:
        key.key = Key::Enter;
        break;
    case 27:
        key.key = Key::Escape;
        break;
    case 127:
        key.key = Key::Backspace;
        break;
    case 9:
        key.key = Key::Tab;
        break;
    default:
        if (code < 0x20 || code > 0x7E)
            return std::nullopt;
        key.ch = static_cast<char>(code);
        if (key.ch >= 'a' && key.ch <= 'z' && ((mods & kShift) != 0) != ((mods & kCapsLock) != 0))
            key.ch = static_cast<char>(key.ch - 'a' + 'A');
    }
    return key;
}

// An arrow's action: kitty reports it as `CSI 1 ; mods:event A-D`. Anything else (no
// parameters, or modifiers only, as in Ctrl+Right) is a press.
std::optional<KeyAction> arrowAction(std::string_view params) {
    const auto n = csiNumbers(params);
    return n ? eventAction((*n)[1][1]) : KeyAction::Press;
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
                const std::string_view params = bytes.substr(i + 2, j - i - 2);
                if (const auto arrow = arrowFor(bytes[j])) {
                    if (const auto action = arrowAction(params))
                        keys_.push_back({*arrow, 0, 0, 0, *action});
                } else if (const auto click = sgrClick(params, bytes[j]); csi && click) {
                    keys_.push_back(*click);
                } else if (csi && bytes[j] == 'u') {
                    if (const auto key = kittyKey(params, interrupt_))
                        keys_.push_back(*key);
                }
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
