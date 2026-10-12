#include "engine/sprite.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <tuple>
#include <utility>

namespace heroes::engine {

namespace {

enum class Section { Header, Palette, Pixels };

std::string lineError(int line, const std::string& message) {
    return std::format("line {}: {}", line, message);
}

bool isPaletteKey(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

std::optional<int> parseSize(std::string_view token) {
    int value = 0;
    const auto [end, ec] = std::from_chars(token.data(), token.data() + token.size(), value);
    if (ec != std::errc{} || end != token.data() + token.size() || value < 1 ||
        value > kMaxSpriteSize)
        return std::nullopt;
    return value;
}

std::optional<Rgb> parseHex(std::string_view token) {
    if (token.size() != 7 || token[0] != '#')
        return std::nullopt;
    unsigned value = 0;
    const auto [end, ec] = std::from_chars(token.data() + 1, token.data() + 7, value, 16);
    if (ec != std::errc{} || end != token.data() + 7)
        return std::nullopt;
    return Rgb{static_cast<std::uint8_t>(value >> 16), static_cast<std::uint8_t>(value >> 8),
               static_cast<std::uint8_t>(value)};
}

std::vector<std::string_view> splitWords(std::string_view line) {
    std::vector<std::string_view> words;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
            ++i;
        const std::size_t start = i;
        while (i < line.size() && line[i] != ' ' && line[i] != '\t')
            ++i;
        if (i > start)
            words.push_back(line.substr(start, i - start));
    }
    return words;
}

// `<keyword> <width> <height>`, both 1-kMaxSpriteSize.
std::expected<std::pair<int, int>, std::string>
parseSizeLine(const std::vector<std::string_view>& words, int lineNo, std::string_view keyword) {
    if (words.size() != 3)
        return std::unexpected(
            lineError(lineNo, std::format("expected '{} <width> <height>'", keyword)));
    const auto w = parseSize(words[1]);
    const auto h = parseSize(words[2]);
    if (!w || !h)
        return std::unexpected(lineError(
            lineNo,
            std::format("bad size: width and height must be whole numbers 1-{}", kMaxSpriteSize)));
    return std::pair{*w, *h};
}

// One `<key> #rrggbb` palette line, added to `palette`.
std::expected<void, std::string> addPaletteEntry(const std::vector<std::string_view>& words,
                                                 int lineNo, std::map<char, Rgb>& palette) {
    if (words.size() != 2 || words[0].size() != 1 || !isPaletteKey(words[0][0]))
        return std::unexpected(
            lineError(lineNo, "expected '<key> #rrggbb' with a key in [A-Za-z0-9]"));
    const auto color = parseHex(words[1]);
    if (!color)
        return std::unexpected(
            lineError(lineNo, std::format("bad color '{}': expected #rrggbb", words[1])));
    if (!palette.emplace(words[0][0], *color).second)
        return std::unexpected(
            lineError(lineNo, std::format("duplicate palette key '{}'", words[0][0])));
    return {};
}

Rgb tinted(Rgb c, const SpriteTint& tint) {
    switch (tint.kind) {
    case SpriteTint::Kind::None:
        return c;
    case SpriteTint::Kind::Flash:
        return Rgb{255, 255, 255};
    case SpriteTint::Kind::Dim: {
        const int level = std::clamp(tint.level, 0, 8);
        auto scale = [level](std::uint8_t v) {
            return static_cast<std::uint8_t>(v * (8 - level) / 8);
        };
        return Rgb{scale(c.r), scale(c.g), scale(c.b)};
    }
    }
    return c;
}

constexpr std::string_view kPaletteKeys =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

std::string_view stripCarriageReturn(std::string_view line) {
    if (!line.empty() && line.back() == '\r')
        line.remove_suffix(1);
    return line;
}

} // namespace

std::expected<Sprite, std::string> parseSprite(std::string_view text) {
    Sprite sprite;
    std::map<char, Rgb> palette;
    Section section = Section::Header;
    bool sawSize = false, sawPalette = false, sawPixels = false;
    int rowsRead = 0;
    int lineNo = 0;

    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t eol = std::min(text.find('\n', pos), text.size());
        const std::string_view line = stripCarriageReturn(text.substr(pos, eol - pos));
        pos = eol + 1;
        ++lineNo;
        if (line.empty() || line.front() == '#')
            continue;

        const auto words = splitWords(line);
        if (words.empty())
            continue;

        if (section == Section::Pixels) {
            if (rowsRead >= sprite.height)
                return std::unexpected(lineError(lineNo, "more pixel rows than the height"));
            if (static_cast<int>(line.size()) != sprite.width)
                return std::unexpected(
                    lineError(lineNo, std::format("row has {} characters, expected {}", line.size(),
                                                  sprite.width)));
            for (int x = 0; x < sprite.width; ++x) {
                const char c = line[static_cast<std::size_t>(x)];
                std::optional<Rgb> pixel;
                if (c != '.') {
                    const auto it = palette.find(c);
                    if (it == palette.end())
                        return std::unexpected(
                            lineError(lineNo, std::format("unknown palette key '{}'", c)));
                    pixel = it->second;
                }
                sprite.pixels.push_back(pixel);
            }
            ++rowsRead;
            continue;
        }

        if (words[0] == "sprite") {
            if (sawSize)
                return std::unexpected(lineError(lineNo, "duplicate 'sprite' line"));
            const auto size = parseSizeLine(words, lineNo, "sprite");
            if (!size)
                return std::unexpected(size.error());
            std::tie(sprite.width, sprite.height) = *size;
            sprite.pixels.reserve(static_cast<std::size_t>(sprite.width) *
                                  static_cast<std::size_t>(sprite.height));
            sawSize = true;
        } else if (words[0] == "palette") {
            if (!sawSize)
                return std::unexpected(lineError(lineNo, "'palette' before 'sprite <w> <h>'"));
            if (sawPalette)
                return std::unexpected(lineError(lineNo, "duplicate 'palette' line"));
            sawPalette = true;
            section = Section::Palette;
        } else if (words[0] == "pixels") {
            if (!sawPalette)
                return std::unexpected(lineError(lineNo, "'pixels' before 'palette'"));
            sawPixels = true;
            section = Section::Pixels;
        } else if (section == Section::Palette) {
            if (auto added = addPaletteEntry(words, lineNo, palette); !added)
                return std::unexpected(added.error());
        } else {
            return std::unexpected(lineError(lineNo, std::format("unexpected '{}'", words[0])));
        }
    }

    if (!sawSize)
        return std::unexpected("missing 'sprite <width> <height>' line");
    if (!sawPalette)
        return std::unexpected("missing 'palette' section");
    if (!sawPixels)
        return std::unexpected("missing 'pixels' section");
    if (rowsRead != sprite.height)
        return std::unexpected(
            std::format("expected {} pixel rows, found {}", sprite.height, rowsRead));
    return sprite;
}

std::expected<Sprite, std::string> loadSprite(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in)
        return std::unexpected(std::format("{}: cannot open file", file.string()));
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto sprite = parseSprite(text);
    if (!sprite)
        return std::unexpected(std::format("{}: {}", file.string(), sprite.error()));
    return sprite;
}

std::expected<std::string, std::string> serializeSprite(const Sprite& sprite, int maxColors) {
    constexpr std::string_view kKeys = kPaletteKeys;
    const int limit = std::min(maxColors, kMaxSpritePaletteSize);

    std::vector<Rgb> colors;
    for (const auto& pixel : sprite.pixels) {
        if (!pixel || std::ranges::find(colors, *pixel) != colors.end())
            continue;
        colors.push_back(*pixel);
        if (static_cast<int>(colors.size()) > limit)
            return std::unexpected(std::format(
                "sprite has more than {} distinct colors; reduce the palette first", limit));
    }

    std::string out = std::format("# generated by sprite_bake\nsprite {} {}\npalette\n",
                                  sprite.width, sprite.height);
    for (std::size_t i = 0; i < colors.size(); ++i)
        out += std::format("{} #{:02x}{:02x}{:02x}\n", kKeys[i], colors[i].r, colors[i].g,
                           colors[i].b);
    out += "pixels\n";
    for (int y = 0; y < sprite.height; ++y) {
        for (int x = 0; x < sprite.width; ++x) {
            const auto& pixel = sprite.at(x, y);
            out += pixel ? kKeys[static_cast<std::size_t>(std::ranges::find(colors, *pixel) -
                                                          colors.begin())]
                         : '.';
        }
        out += '\n';
    }
    return out;
}

void drawSprite(Framebuffer& frame, const Sprite& sprite, int cellX, int cellY, SpriteTint tint) {
    const int cellRows = (sprite.height + 1) / 2;
    for (int row = 0; row < cellRows; ++row) {
        for (int x = 0; x < sprite.width; ++x) {
            const int cx = cellX + x;
            const int cy = cellY + row;
            if (cx < 0 || cy < 0 || cx >= frame.width() || cy >= frame.height())
                continue;
            const int topY = row * 2;
            const auto& top = sprite.at(x, topY);
            // An odd height leaves the last row's bottom pixel transparent.
            static const std::optional<Rgb> kNone;
            const auto& bottom = topY + 1 < sprite.height ? sprite.at(x, topY + 1) : kNone;
            if (!top && !bottom)
                continue;

            Cell& cell = frame.at(cx, cy);
            if (top && bottom) {
                cell.glyph = U'▀';
                cell.fg = tinted(*top, tint);
                cell.bg = tinted(*bottom, tint);
            } else if (top) {
                cell.glyph = U'▀';
                cell.fg = tinted(*top, tint);
            } else {
                cell.glyph = U'▄';
                cell.fg = tinted(*bottom, tint);
            }
        }
    }
}

// ---- Glyph sprites ---------------------------------------------------------------------

std::expected<GlyphSprite, std::string> parseGlyphs(std::string_view text) {
    enum class Part { Header, Palette, Chars, Colors };
    GlyphSprite sprite;
    std::map<char, Rgb> palette;
    std::vector<std::string> chars; // the char rows, without their | delimiters
    std::vector<int> charLines;     // where each came from, for errors
    Part part = Part::Header;
    bool sawSize = false, sawPalette = false, sawChars = false, sawColors = false;
    int colorRows = 0;
    int lineNo = 0;

    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t eol = std::min(text.find('\n', pos), text.size());
        const std::string_view line = stripCarriageReturn(text.substr(pos, eol - pos));
        pos = eol + 1;
        ++lineNo;
        // Char rows are delimited by |...| so leading '#' or all-space rows survive.
        if (part == Part::Chars && !line.empty() && line.front() == '|') {
            if (line.size() < 2 || line.back() != '|')
                return std::unexpected(lineError(lineNo, "char row must end with '|'"));
            const std::string_view row = line.substr(1, line.size() - 2);
            if (static_cast<int>(chars.size()) >= sprite.height)
                return std::unexpected(lineError(lineNo, "more char rows than the height"));
            for (const char c : row) // before the width check: a UTF-8 glyph is several bytes
                if (c < 0x20 || c > 0x7E)
                    return std::unexpected(
                        lineError(lineNo, "chars must be printable ASCII (0x20-0x7E)"));
            if (static_cast<int>(row.size()) != sprite.width)
                return std::unexpected(lineError(
                    lineNo, std::format("row has {} characters, expected {}", row.size(),
                                        sprite.width)));
            chars.emplace_back(row);
            charLines.push_back(lineNo);
            continue;
        }
        if (line.empty() || line.front() == '#')
            continue;
        const auto words = splitWords(line);
        if (words.empty())
            continue;

        if (part == Part::Colors) {
            if (colorRows >= sprite.height)
                return std::unexpected(lineError(lineNo, "more color rows than the height"));
            if (static_cast<int>(line.size()) != sprite.width)
                return std::unexpected(lineError(
                    lineNo, std::format("row has {} characters, expected {}", line.size(),
                                        sprite.width)));
            const std::string& row = chars[static_cast<std::size_t>(colorRows)];
            for (int x = 0; x < sprite.width; ++x) {
                const char key = line[static_cast<std::size_t>(x)];
                const char ch = row[static_cast<std::size_t>(x)];
                if ((key == '.') != (ch == ' '))
                    return std::unexpected(lineError(
                        lineNo, std::format("column {}: a cell must be transparent ('.' and a "
                                            "space) in both grids or in neither",
                                            x + 1)));
                if (key == '.') {
                    sprite.cells.emplace_back(std::nullopt);
                    continue;
                }
                const auto it = palette.find(key);
                if (it == palette.end())
                    return std::unexpected(
                        lineError(lineNo, std::format("unknown palette key '{}'", key)));
                sprite.cells.emplace_back(GlyphCell{ch, it->second});
            }
            ++colorRows;
            continue;
        }

        if (words[0] == "glyphs") {
            if (sawSize)
                return std::unexpected(lineError(lineNo, "duplicate 'glyphs' line"));
            const auto size = parseSizeLine(words, lineNo, "glyphs");
            if (!size)
                return std::unexpected(size.error());
            std::tie(sprite.width, sprite.height) = *size;
            sawSize = true;
        } else if (words[0] == "palette") {
            if (!sawSize)
                return std::unexpected(lineError(lineNo, "'palette' before 'glyphs <w> <h>'"));
            if (sawPalette)
                return std::unexpected(lineError(lineNo, "duplicate 'palette' line"));
            sawPalette = true;
            part = Part::Palette;
        } else if (words[0] == "chars") {
            if (!sawPalette)
                return std::unexpected(lineError(lineNo, "'chars' before 'palette'"));
            sawChars = true;
            part = Part::Chars;
        } else if (words[0] == "colors") {
            if (!sawChars)
                return std::unexpected(lineError(lineNo, "'colors' before 'chars'"));
            if (static_cast<int>(chars.size()) != sprite.height)
                return std::unexpected(lineError(
                    lineNo, std::format("expected {} char rows, found {}", sprite.height,
                                        chars.size())));
            sawColors = true;
            part = Part::Colors;
        } else if (part == Part::Palette) {
            if (auto added = addPaletteEntry(words, lineNo, palette); !added)
                return std::unexpected(added.error());
        } else {
            return std::unexpected(lineError(lineNo, std::format("unexpected '{}'", words[0])));
        }
    }

    if (!sawSize)
        return std::unexpected("missing 'glyphs <width> <height>' line");
    if (!sawPalette)
        return std::unexpected("missing 'palette' section");
    if (!sawChars)
        return std::unexpected("missing 'chars' section");
    if (!sawColors)
        return std::unexpected("missing 'colors' section");
    if (colorRows != sprite.height)
        return std::unexpected(
            std::format("expected {} color rows, found {}", sprite.height, colorRows));
    return sprite;
}

std::expected<GlyphSprite, std::string> loadGlyphs(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in)
        return std::unexpected(std::format("{}: cannot open file", file.string()));
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto sprite = parseGlyphs(text);
    if (!sprite)
        return std::unexpected(std::format("{}: {}", file.string(), sprite.error()));
    return sprite;
}

std::expected<std::string, std::string> serializeGlyphs(const GlyphSprite& sprite, int maxColors) {
    const int limit = std::min(maxColors, kMaxSpritePaletteSize);
    std::vector<Rgb> colors;
    for (const auto& cell : sprite.cells) {
        if (!cell || std::ranges::find(colors, cell->color) != colors.end())
            continue;
        colors.push_back(cell->color);
        if (static_cast<int>(colors.size()) > limit)
            return std::unexpected(std::format(
                "sprite has more than {} distinct colors; reduce the palette first", limit));
    }

    std::string out = std::format("# generated by sprite_bake\nglyphs {} {}\npalette\n",
                                  sprite.width, sprite.height);
    for (std::size_t i = 0; i < colors.size(); ++i)
        out += std::format("{} #{:02x}{:02x}{:02x}\n", kPaletteKeys[i], colors[i].r, colors[i].g,
                           colors[i].b);
    out += "chars\n";
    for (int y = 0; y < sprite.height; ++y) {
        out += '|';
        for (int x = 0; x < sprite.width; ++x)
            out += sprite.at(x, y) ? sprite.at(x, y)->ch : ' ';
        out += "|\n";
    }
    out += "colors\n";
    for (int y = 0; y < sprite.height; ++y) {
        for (int x = 0; x < sprite.width; ++x) {
            const auto& cell = sprite.at(x, y);
            out += cell ? kPaletteKeys[static_cast<std::size_t>(
                              std::ranges::find(colors, cell->color) - colors.begin())]
                        : '.';
        }
        out += '\n';
    }
    return out;
}

void drawGlyphs(Framebuffer& frame, const GlyphSprite& sprite, int cellX, int cellY,
                SpriteTint tint) {
    for (int y = 0; y < sprite.height; ++y) {
        for (int x = 0; x < sprite.width; ++x) {
            const int cx = cellX + x;
            const int cy = cellY + y;
            const auto& glyph = sprite.at(x, y);
            if (!glyph || cx < 0 || cy < 0 || cx >= frame.width() || cy >= frame.height())
                continue;
            Cell& cell = frame.at(cx, cy);
            cell.glyph = static_cast<char32_t>(glyph->ch);
            cell.fg = tinted(glyph->color, tint);
        }
    }
}

} // namespace heroes::engine
