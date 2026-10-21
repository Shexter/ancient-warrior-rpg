#include "app/screens/overworld.hpp"

#include "app/widgets.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

namespace heroes::app {

namespace {

using game::world::Terrain;
using game::world::Tile;

inline constexpr engine::Size kMaxCanvas{200, 60};

struct TerrainStyle {
    std::string_view ramp; // glyphs picked per tile by its position
    engine::Rgb fg, bg;
};

constexpr TerrainStyle styleOf(Terrain t) {
    switch (t) {
    case Terrain::Grass:
        return {".,'`..", {86, 150, 78}, {12, 20, 14}};
    case Terrain::TallGrass:
        return {"ilvY;:", {110, 180, 86}, {12, 22, 14}};
    case Terrain::Tree:
        return {"YT^", {58, 130, 70}, {8, 16, 10}};
    case Terrain::Water:
        return {"~-~=", {84, 136, 210}, {10, 20, 42}};
    case Terrain::Path:
        return {".:.", {176, 154, 112}, {26, 22, 16}};
    case Terrain::Sand:
        return {".:,", {214, 192, 132}, {32, 28, 18}};
    case Terrain::Rock:
        return {"^nA", {146, 146, 158}, {20, 20, 26}};
    case Terrain::Wall:
        return {"#=", {156, 134, 112}, {30, 26, 22}};
    case Terrain::Floor:
        return {".:", {124, 124, 140}, {24, 24, 32}};
    }
    return {".", palette::kText, palette::kBg};
}

// A stable per-tile hash, so a tile always looks the same.
std::uint32_t tileHash(Tile t) {
    std::uint32_t h = static_cast<std::uint32_t>(t.x) * 0x9E3779B1u ^
                      static_cast<std::uint32_t>(t.y) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    return h;
}

engine::Rgb scaled(engine::Rgb c, int percent) {
    const auto f = [&](std::uint8_t v) {
        return static_cast<std::uint8_t>(std::min(255, v * percent / 100));
    };
    return {f(c.r), f(c.g), f(c.b)};
}

engine::Cell terrainCell(const game::world::WorldDef& world, Tile t) {
    const TerrainStyle style = styleOf(world.at(t).terrain);
    const std::uint32_t h = tileHash(t);
    const char glyph = style.ramp[h % style.ramp.size()];
    const int brightness = 88 + static_cast<int>((h >> 8) % 25); // 88-112%
    return {static_cast<char32_t>(glyph), scaled(style.fg, brightness), style.bg};
}

// One axis of the camera: keep the hero inside the middle third of the view, stay inside the map,
// and centre a map smaller than the view.
int followAxis(int camera, int hero, int view, int map) {
    if (map <= view)
        return -(view - map) / 2;
    const int lo = view / 3, hi = view - 1 - view / 3;
    if (hero < camera + lo)
        camera = hero - lo;
    else if (hero > camera + hi)
        camera = hero - hi;
    return std::clamp(camera, 0, map - view);
}

} // namespace

OverworldScreen::OverworldScreen(Session& session)
    : session_(session), world_(*session.world, *session.state) {}

Transition OverworldScreen::update(std::span<const engine::KeyEvent> keys, double) {
    for (const engine::KeyEvent& key : keys)
        if (key.key == engine::Key::Escape && key.action == engine::KeyAction::Press)
            return Pop{};
    held_.update(keys);
    world_.step({held_.direction()});
    return Stay{};
}

engine::Size OverworldScreen::canvasSize(engine::Size terminal) const {
    if (session_.layout == Layout::Compact)
        return engine::kCompactCanvas;
    return {std::clamp(terminal.width, engine::kCompactCanvas.width, kMaxCanvas.width),
            std::clamp(terminal.height, engine::kCompactCanvas.height, kMaxCanvas.height)};
}

void OverworldScreen::draw(engine::Framebuffer& frame) const {
    const game::world::WorldDef& map = *session_.world;
    const Tile hero = world_.hero();
    const Tile start = camera_.value_or(
        Tile{hero.x - frame.width() / 2, hero.y - frame.height() / 2});
    camera_ = Tile{followAxis(start.x, hero.x, frame.width(), map.width),
                   followAxis(start.y, hero.y, frame.height(), map.height)};

    frame.clear(palette::kBg);
    for (int y = 0; y < frame.height(); ++y)
        for (int x = 0; x < frame.width(); ++x)
            if (const Tile t{camera_->x + x, camera_->y + y}; map.inBounds(t))
                frame.set(x, y, terrainCell(map, t));
    engine::Cell& heroCell = frame.at(hero.x - camera_->x, hero.y - camera_->y);
    heroCell.glyph = U'@';
    heroCell.fg = palette::kAccent;
}

} // namespace heroes::app
