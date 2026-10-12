#include "app/widgets.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <format>
#include <utility>

namespace heroes::app {

namespace {

using engine::Framebuffer;
using engine::Key;
using engine::KeyEvent;
using engine::Rgb;

constexpr int kCanvasWidth = 80;
constexpr int kCanvasHeight = 24;
constexpr int kDialogMaxWidth = 60;

// Decodes one UTF-8 code point starting at `i`, advancing `i`. Malformed input gives '?'.
char32_t nextCodePoint(std::string_view s, std::size_t& i) {
    const auto lead = static_cast<unsigned char>(s[i++]);
    int extra = lead < 0x80           ? 0
                : (lead >> 5) == 0x6  ? 1
                : (lead >> 4) == 0xE  ? 2
                : (lead >> 3) == 0x1E ? 3
                                      : -1;
    if (extra < 0)
        return U'?';
    char32_t cp = extra == 0 ? lead : lead & (0x3F >> extra);
    for (; extra > 0; --extra) {
        if (i >= s.size() || (static_cast<unsigned char>(s[i]) & 0xC0) != 0x80)
            return U'?';
        cp = (cp << 6) | (static_cast<unsigned char>(s[i++]) & 0x3F);
    }
    return cp;
}

void put(Framebuffer& frame, int x, int y, char32_t glyph, Rgb fg) {
    if (x < 0 || y < 0 || x >= frame.width() || y >= frame.height())
        return;
    engine::Cell& cell = frame.at(x, y);
    cell.glyph = glyph;
    cell.fg = fg;
}

// Centered dialog box: text lines, then `footer` right-aligned in dim on the last inner row.
void drawDialog(Framebuffer& frame, const std::vector<std::string>& lines,
                std::string_view footer) {
    int inner = static_cast<int>(footer.size());
    for (const std::string& line : lines)
        inner = std::max(inner, static_cast<int>(line.size()));
    const int w = std::min(kDialogMaxWidth, inner + 4);
    const int h = static_cast<int>(lines.size()) + 4;
    const int x = (kCanvasWidth - w) / 2;
    const int y = (kCanvasHeight - h) / 2;
    drawPanel(frame, x, y, w, h);
    for (std::size_t i = 0; i < lines.size(); ++i)
        drawText(frame, x + 2, y + 1 + static_cast<int>(i), lines[i], palette::kText, w - 4);
    drawText(frame, x + w - 2 - static_cast<int>(footer.size()), y + h - 2, footer, palette::kDim);
}

std::string upper(std::string_view s) {
    std::string out(s);
    for (char& c : out)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return out;
}

} // namespace

int drawText(Framebuffer& frame, int x, int y, std::string_view utf8, Rgb fg, int maxWidth) {
    int drawn = 0;
    for (std::size_t i = 0; i < utf8.size() && drawn < maxWidth; ++drawn)
        put(frame, x + drawn, y, nextCodePoint(utf8, i), fg);
    return drawn;
}

int textWidth(std::string_view utf8) {
    return static_cast<int>(std::ranges::count_if(
        utf8, [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; }));
}

void drawCentered(Framebuffer& frame, int y, std::string_view utf8, Rgb fg) {
    drawText(frame, (kCanvasWidth - textWidth(utf8)) / 2, y, utf8, fg);
}

std::vector<std::string> wordWrap(std::string_view text, int width) {
    width = std::max(1, width);
    std::vector<std::string> lines;
    std::string line;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t end = std::min(text.find(' ', pos), text.size());
        std::string_view word = text.substr(pos, end - pos);
        pos = end + 1;
        while (static_cast<int>(word.size()) > width) { // a word longer than a whole line
            if (!line.empty())
                lines.push_back(std::exchange(line, {}));
            lines.emplace_back(word.substr(0, static_cast<std::size_t>(width)));
            word.remove_prefix(static_cast<std::size_t>(width));
        }
        if (word.empty())
            continue;
        if (!line.empty() && static_cast<int>(line.size() + 1 + word.size()) > width)
            lines.push_back(std::exchange(line, {}));
        if (!line.empty())
            line += ' ';
        line += word;
    }
    if (!line.empty() || lines.empty())
        lines.push_back(line);
    return lines;
}

void drawPanel(Framebuffer& frame, int x, int y, int w, int h) {
    frame.fillRect(x, y, w, h, engine::Cell{U' ', palette::kText, palette::kPanel});
    frame.box(x, y, w, h, palette::kBorder);
}

void drawBar(Framebuffer& frame, int x, int y, int width, int value, int max, Rgb color) {
    const auto filled =
        max <= 0 || value <= 0
            ? 0
            : static_cast<int>((static_cast<std::int64_t>(std::min(value, max)) * width + max - 1) /
                               max);
    for (int i = 0; i < width; ++i)
        put(frame, x + i, y, i < filled ? U'█' : U'░', i < filled ? color : palette::kDim);
}

Rgb hpColor(int hp, int maxHp) {
    const auto doubled = static_cast<std::int64_t>(hp) * 2;
    if (doubled > maxHp)
        return palette::kHpHigh;
    if (static_cast<std::int64_t>(hp) * 5 > maxHp)
        return palette::kHpMid;
    return palette::kHpLow;
}

// ---- Menu --------------------------------------------------------------------------------

Menu::Menu(std::vector<Item> items, Layout layout) : items_(std::move(items)), layout_(layout) {
    const auto first = std::ranges::find_if(items_, &Item::enabled);
    selected_ = first == items_.end() ? 0 : static_cast<int>(first - items_.begin());
}

void Menu::setItems(std::vector<Item> items) {
    items_ = std::move(items);
    if (items_.empty()) {
        selected_ = 0;
        return;
    }
    selected_ = std::min(selected_, static_cast<int>(items_.size()) - 1);
    if (!items_[static_cast<std::size_t>(selected_)].enabled)
        move(1);
}

void Menu::move(int step) {
    const int n = static_cast<int>(items_.size());
    for (int tries = 0, i = selected_; tries < n; ++tries) {
        i = (i + step + n) % n;
        if (items_[static_cast<std::size_t>(i)].enabled) {
            selected_ = i;
            return;
        }
    }
}

std::optional<int> Menu::handle(const KeyEvent& key) {
    if (items_.empty())
        return std::nullopt;
    const bool vertical = layout_ == Layout::Vertical;
    const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(key.ch)));
    const bool isChar = key.key == Key::Char;
    if ((vertical && (key.key == Key::Up || (isChar && c == 'w'))) ||
        (!vertical && (key.key == Key::Left || (isChar && c == 'a')))) {
        move(-1);
    } else if ((vertical && (key.key == Key::Down || (isChar && c == 's'))) ||
               (!vertical && (key.key == Key::Right || (isChar && c == 'd')))) {
        move(1);
    } else if (key.key == Key::Enter) {
        if (items_[static_cast<std::size_t>(selected_)].enabled)
            return selected_;
    } else if (isChar && c >= '1' && c <= '9') {
        const int index = c - '1';
        if (index < static_cast<int>(items_.size()) &&
            items_[static_cast<std::size_t>(index)].enabled) {
            selected_ = index;
            return index;
        }
    }
    return std::nullopt;
}

void Menu::draw(Framebuffer& frame, int x, int y, bool focused) const {
    std::size_t labelWidth = 0;
    for (const Item& item : items_)
        labelWidth = std::max(labelWidth, item.label.size());
    int cursor = x;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const Item& item = items_[i];
        const bool chosen = focused && static_cast<int>(i) == selected_;
        const Rgb color = !item.enabled ? palette::kDim
                          : chosen      ? palette::kAccent
                                        : palette::kText;
        const int row = layout_ == Layout::Vertical ? y + static_cast<int>(i) : y;
        const int col = layout_ == Layout::Vertical ? x : cursor;
        drawText(frame, col, row, chosen ? "▶ " : "  ", palette::kAccent);
        drawText(frame, col + 2, row, item.label, color);
        if (!item.note.empty())
            drawText(frame, col + 4 + static_cast<int>(labelWidth), row, item.note, palette::kDim);
        cursor += 2 + static_cast<int>(item.label.size()) + 4;
    }
}

// ---- Text and number input ---------------------------------------------------------------

InputResult TextInput::handle(const KeyEvent& key) {
    if (key.key == Key::Enter)
        return InputResult::Submitted;
    if (key.key == Key::Escape)
        return InputResult::Cancelled;
    if (key.key == Key::Backspace && !value_.empty())
        value_.pop_back();
    else if (key.key == Key::Char && static_cast<int>(value_.size()) < maxLength_)
        value_ += key.ch;
    return InputResult::Editing;
}

void TextInput::draw(Framebuffer& frame, int x, int y) const {
    const int width = drawText(frame, x, y, value_, palette::kText);
    if (static_cast<long long>(clock_ * 2) % 2 == 0)
        put(frame, x + width, y, U'_', palette::kAccent);
}

NumberInput::NumberInput(std::int64_t min, std::int64_t max)
    : min_(min), max_(max), maxDigits_(std::to_string(max).size()) {}

std::optional<std::int64_t> NumberInput::value() const {
    std::int64_t v = 0;
    const auto [end, ec] = std::from_chars(digits_.data(), digits_.data() + digits_.size(), v);
    if (digits_.empty() || ec != std::errc{} || end != digits_.data() + digits_.size())
        return std::nullopt; // empty, or too large for int64
    return v;
}

InputResult NumberInput::handle(const KeyEvent& key) {
    if (key.key == Key::Escape)
        return InputResult::Cancelled;
    if (key.key == Key::Enter) {
        const auto v = value();
        if (v && *v >= min_ && *v <= max_)
            return InputResult::Submitted;
        showError_ = true;
        return InputResult::Editing;
    }
    if (key.key == Key::Backspace && !digits_.empty()) {
        digits_.pop_back();
        showError_ = false;
    } else if (key.key == Key::Char && key.ch >= '0' && key.ch <= '9' &&
               digits_.size() < maxDigits_) {
        digits_ += key.ch;
        showError_ = false;
    }
    return InputResult::Editing;
}

void NumberInput::draw(Framebuffer& frame, int x, int y) const {
    const int width = drawText(frame, x, y, digits_, palette::kText);
    if (static_cast<long long>(clock_ * 2) % 2 == 0)
        put(frame, x + width, y, U'_', palette::kAccent);
    if (showError_)
        drawText(frame, x, y + 1, std::format("Enter a number from {} to {}", min_, max_),
                 palette::kDmg);
}

// ---- Dialogs -----------------------------------------------------------------------------

MessageBox::MessageBox(std::string_view text) : lines_(wordWrap(text, kDialogMaxWidth - 4)) {}

bool MessageBox::handle(const KeyEvent& key) {
    return key.key == Key::Enter || key.key == Key::Escape;
}

void MessageBox::draw(Framebuffer& frame) const { drawDialog(frame, lines_, "[Enter]"); }

ConfirmBox::ConfirmBox(std::string_view text) : lines_(wordWrap(text, kDialogMaxWidth - 4)) {}

std::optional<bool> ConfirmBox::handle(const KeyEvent& key) {
    if (key.key == Key::Escape)
        return false;
    if (key.key != Key::Char)
        return std::nullopt;
    if (key.ch == 'y' || key.ch == 'Y')
        return true;
    if (key.ch == 'n' || key.ch == 'N')
        return false;
    return std::nullopt;
}

void ConfirmBox::draw(Framebuffer& frame) const { drawDialog(frame, lines_, "[Y]es / [N]o"); }

// ---- Typewriter --------------------------------------------------------------------------

Typewriter::Typewriter(const std::vector<std::string>& paragraphs, int width) {
    for (const std::string& paragraph : paragraphs)
        for (std::string& row : wordWrap(paragraph, width)) {
            total_ += row.size();
            rows_.push_back(std::move(row));
        }
}

void Typewriter::tick(double dtSeconds) {
    constexpr double kCharsPerSecond = 40;
    shown_ = std::min(static_cast<double>(total_), shown_ + dtSeconds * kCharsPerSecond);
}

bool Typewriter::handle(const KeyEvent& key) {
    if (key.key != Key::Enter)
        return false;
    if (revealed())
        return true;
    shown_ = static_cast<double>(total_);
    return false;
}

void Typewriter::draw(Framebuffer& frame, int x, int y, Rgb fg) const {
    auto left = static_cast<std::size_t>(shown_);
    for (std::size_t i = 0; i < rows_.size() && left > 0; ++i) {
        const std::size_t n = std::min(left, rows_[i].size());
        drawText(frame, x, y + static_cast<int>(i), std::string_view(rows_[i]).substr(0, n), fg);
        left -= n;
    }
}

// ---- Status panel ------------------------------------------------------------------------

void drawStatusPanel(Framebuffer& frame, const game::Hero& hero, const game::Content& content) {
    // Rows 0-4: ui.md's two-line layout overflows 80 columns once real gear names appear, so
    // the gear gets its own line (row 4 is free on every screen that shows the panel).
    drawPanel(frame, 0, 0, kCanvasWidth, 5);
    constexpr int kRight = kCanvasWidth - 2; // last column inside the border
    int x = 2;
    int y = 1;
    auto add = [&](std::string_view text, Rgb fg) {
        x += drawText(frame, x, y, text, fg, std::max(0, kRight - x));
    };

    add(upper(hero.name), palette::kAccent);
    add(std::format("  {} Lv {}   HP ", content.path(hero.pathId).name, hero.level),
        palette::kText);
    if (x + 20 <= kRight) {
        drawBar(frame, x, y, 20, hero.hp, hero.maxHp, hpColor(hero.hp, hero.maxHp));
        x += 20;
    }
    add(std::format(" {}/{}   MP ", hero.hp, hero.maxHp), palette::kText);
    if (x + 8 <= kRight) {
        drawBar(frame, x, y, 8, hero.mp, hero.maxMp, palette::kMp);
        x += 8;
    }
    add(std::format(" {}/{}", hero.mp, hero.maxMp), palette::kText);

    x = 2;
    y = 2;
    const game::StatBonus bonus = game::equipmentBonus(hero, content);
    auto gear = [&](const std::optional<game::Equipped>& e) {
        if (!e)
            return std::string("—");
        const std::string& name = content.item(e->itemId).name;
        return e->upgradeLevel > 1 ? std::format("{}+{}", name, e->upgradeLevel) : name;
    };
    add(std::format("ATK {}  DEF {}  LIFE {}   EXP {}  Next {}   ", hero.atk + bonus.atk,
                    hero.def + bonus.def, hero.life + bonus.life, hero.exp, hero.expToNext),
        palette::kText);
    add("Gold ", palette::kText);
    add(std::to_string(hero.gold), palette::kGold);

    x = 2;
    y = 3;
    add(std::format("Weapon {}   Armor {}", gear(hero.weapon), gear(hero.armor)), palette::kText);
}

} // namespace heroes::app
