#pragma once

#include "engine/framebuffer.hpp"
#include "engine/input.hpp"
#include "game/content.hpp"
#include "game/hero.hpp"

#include <climits>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace heroes::app {

namespace palette { // ui.md
inline constexpr engine::Rgb kBg{16, 16, 24};
inline constexpr engine::Rgb kPanel{30, 30, 46};
inline constexpr engine::Rgb kBorder{88, 91, 112};
inline constexpr engine::Rgb kText{205, 214, 244};
inline constexpr engine::Rgb kDim{108, 112, 134};
inline constexpr engine::Rgb kAccent{249, 226, 175};
inline constexpr engine::Rgb kHpHigh{166, 227, 161};
inline constexpr engine::Rgb kHpMid{249, 226, 175};
inline constexpr engine::Rgb kHpLow{243, 139, 168};
inline constexpr engine::Rgb kMp{137, 180, 250};
inline constexpr engine::Rgb kDmg{243, 139, 168};
inline constexpr engine::Rgb kHeal{166, 227, 161};
inline constexpr engine::Rgb kGold{250, 179, 135};
} // namespace palette

// Fixed 1/60 s steps sum to just under a whole interval in floating point; timers compare with
// this slack so an event lands on the step it is due.
inline constexpr double kTimerSlack = 1e-9;

// ASCII upper case, for names in titles and logs ("ARIA").
std::string upper(std::string_view s);

// Why the hero can't train or take missions ("Fallen: visit the Guildmaster", "Equip a weapon
// and armor"), or empty if they can.
std::string adventureBlocker(const game::Hero& hero);

// Draws UTF-8 text (one cell per code point; only single-width glyphs are used), keeping the
// background. Stops after `maxWidth` cells. Returns the number of cells drawn.
int drawText(engine::Framebuffer& frame, int x, int y, std::string_view utf8, engine::Rgb fg,
             int maxWidth = INT_MAX);
// Cells needed for UTF-8 text (one per code point).
int textWidth(std::string_view utf8);
// drawText centered on the frame's width.
void drawCentered(engine::Framebuffer& frame, int y, std::string_view utf8, engine::Rgb fg);
// Splits on spaces into lines of at most `width` characters (longer words are cut).
std::vector<std::string> wordWrap(std::string_view text, int width);
// A bordered box with a panel-colored interior.
void drawPanel(engine::Framebuffer& frame, int x, int y, int w, int h);
// `width` cells of █ (filled, rounded up so any value above 0 shows) and ░.
void drawBar(engine::Framebuffer& frame, int x, int y, int width, int value, int max,
             engine::Rgb color);
engine::Rgb hpColor(int hp, int maxHp);

// Vertical list (↑/↓, w/s) or horizontal row (←/→, a/d). Selection wraps and skips disabled
// items; Enter or a digit 1-9 chooses an enabled item. Esc is left to the screen.
class Menu {
  public:
    enum class Layout { Vertical, Horizontal };
    struct Item {
        Item(std::string text, bool isEnabled = true, std::string why = {})
            : label(std::move(text)), enabled(isEnabled), note(std::move(why)) {}
        std::string label;
        bool enabled;
        std::string note; // dim text after the label, e.g. why it is disabled
    };

    explicit Menu(std::vector<Item> items, Layout layout = Layout::Vertical);

    // The chosen index, or nullopt if the key only moved the selection (or did nothing). A click
    // on a row drawn by the last draw() chooses it, like its number key.
    std::optional<int> handle(const engine::KeyEvent& key);
    // An unfocused menu (the other list of a two-list screen) hides its selection. Rows are
    // clipped to `maxWidth` cells from `x`.
    void draw(engine::Framebuffer& frame, int x, int y, bool focused = true,
              int maxWidth = INT_MAX) const;
    int selected() const { return selected_; }
    int size() const { return static_cast<int>(items_.size()); }
    // Replaces the items (e.g. after the game state changed), keeping the selection where it is
    // unless that item is now missing or disabled.
    void setItems(std::vector<Item> items);

  private:
    void move(int step);

    // Where each row was last drawn, for clicks.
    struct Hit {
        int x0, x1, y, index; // cells x0 <= x < x1 on row y
    };

    std::vector<Item> items_;
    Layout layout_;
    int selected_ = 0;
    mutable std::vector<Hit> hits_;
};

enum class InputResult { Editing, Submitted, Cancelled };

// One line of printable ASCII; Backspace deletes, Enter submits, Esc cancels. The `_` cursor
// blinks every 500 ms.
class TextInput {
  public:
    explicit TextInput(int maxLength) : maxLength_(maxLength) {}
    InputResult handle(const engine::KeyEvent& key);
    void tick(double dtSeconds) { clock_ += dtSeconds; }
    void draw(engine::Framebuffer& frame, int x, int y) const;
    const std::string& value() const { return value_; }

  private:
    int maxLength_;
    std::string value_;
    double clock_ = 0;
};

// Digits only. Enter submits only a value in [min, max]; otherwise it shows
// "Enter a number from MIN to MAX" in the damage color on the row below.
class NumberInput {
  public:
    NumberInput(std::int64_t min, std::int64_t max);
    InputResult handle(const engine::KeyEvent& key);
    void tick(double dtSeconds) { clock_ += dtSeconds; }
    void draw(engine::Framebuffer& frame, int x, int y) const;
    // The typed number, if any (for live previews); valid and in range after Submitted.
    std::optional<std::int64_t> value() const;

  private:
    std::int64_t min_, max_;
    std::size_t maxDigits_;
    std::string digits_;
    bool showError_ = false;
    double clock_ = 0;
};

// A centered box (at most 60 columns) with word-wrapped text and "[Enter]" at the bottom
// right. Enter or Esc dismisses it.
class MessageBox {
  public:
    explicit MessageBox(std::string_view text);
    bool handle(const engine::KeyEvent& key); // true once dismissed (a tap anywhere dismisses)
    void draw(engine::Framebuffer& frame) const;

  private:
    std::vector<std::string> lines_;
};

// A MessageBox answered with Y or N; Esc means No.
class ConfirmBox {
  public:
    explicit ConfirmBox(std::string_view text);
    // The answer, once given; a tap on "[Y]es" or "[N]o" answers too.
    std::optional<bool> handle(const engine::KeyEvent& key);
    void draw(engine::Framebuffer& frame) const;

  private:
    std::vector<std::string> lines_;
    mutable int footerX_ = -1, footerY_ = -1; // where "[Y]es / [N]o" was last drawn
};

// Reveals wrapped text at 40 characters per second. Enter reveals the rest at once; a second
// Enter reports it finished.
class Typewriter {
  public:
    Typewriter(const std::vector<std::string>& paragraphs, int width);
    void tick(double dtSeconds);
    bool handle(const engine::KeyEvent& key); // true when the reader is done
    bool revealed() const { return shown_ >= static_cast<double>(total_); }
    void draw(engine::Framebuffer& frame, int x, int y, engine::Rgb fg) const;

  private:
    std::vector<std::string> rows_;
    std::size_t total_ = 0;
    double shown_ = 0;
};

// The hero summary on rows 0-4 of most town screens (ui.md, plus a gear line), as wide as the
// frame.
void drawStatusPanel(engine::Framebuffer& frame, const game::Hero& hero,
                     const game::Content& content);

} // namespace heroes::app
