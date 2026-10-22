#include "window/scene.hpp"

#include "app/widgets.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace heroes::window {

namespace {

using game::world::Terrain;
using game::world::Tile;
using Kind = Item::Kind;
using Mode = game::world::Monster::Mode;
using SceneView = app::OverworldScreen::SceneView;

constexpr float T = kTilePx;
constexpr float kBendRadius = 46.0f; // grass leans away from a creature closer than this
constexpr int kGrassBlades = 14, kTallGrassBlades = 26;

// Every look is a function of position (and time): never the game's random stream.
std::uint32_t hash(int x, int y, int k = 0) {
    std::uint32_t h = static_cast<std::uint32_t>(x) * 0x9E3779B1u ^
                      static_cast<std::uint32_t>(y) * 0x85EBCA77u ^
                      static_cast<std::uint32_t>(k) * 0xC2B2AE3Du;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}
float rnd(int x, int y, int k) { return static_cast<float>(hash(x, y, k) & 0xFFFFFF) / 0xFFFFFF; }

// Smooth value noise in [0, 1].
float noise(float x, float y) {
    const int ix = static_cast<int>(std::floor(x)), iy = static_cast<int>(std::floor(y));
    const float fx = x - static_cast<float>(ix), fy = y - static_cast<float>(iy);
    const float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
    const float a = rnd(ix, iy, 77), b = rnd(ix + 1, iy, 77), c = rnd(ix, iy + 1, 77),
                d = rnd(ix + 1, iy + 1, 77);
    const float top = a + (b - a) * sx, bottom = c + (d - c) * sx;
    return top + (bottom - top) * sy;
}

std::uint8_t channel(float v) { return static_cast<std::uint8_t>(std::clamp(v, 0.0f, 255.0f)); }
Rgba rgba(engine::Rgb c, std::uint8_t a = 255) { return {c.r, c.g, c.b, a}; }
Rgba scale(Rgba c, float f) { return {channel(c.r * f), channel(c.g * f), channel(c.b * f), c.a}; }
Rgba withAlpha(Rgba c, float a) { return {c.r, c.g, c.b, channel(a)}; }
Rgba mix(Rgba a, Rgba b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const auto m = [t](std::uint8_t x, std::uint8_t y) { return channel(x + (y - x) * t); };
    return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a)};
}

// What one build reads, and where it writes.
struct Ctx {
    Scene& scene;
    const SceneView& sv;
    float t;
    float alpha; // how far into its next step the game is
    View v;
    int tx0, ty0, tx1, ty1; // the map tiles in view, plus a margin

    std::optional<Terrain> at(int x, int y) const {
        if (!sv.map.inBounds({x, y}))
            return std::nullopt;
        return sv.map.at({x, y}).terrain;
    }
    std::optional<Terrain> atPx(float x, float y) const {
        return at(static_cast<int>(std::floor(x / T)), static_cast<int>(std::floor(y / T)));
    }

    // Moonlit patches, and the warm light reaching a point from the house windows.
    static float moon(float x, float y) { return 0.45f + 0.55f * noise(x / 210.0f, y / 170.0f); }
    float lantern(float x, float y) const {
        float l = 0;
        for (const House& h : scene.houses) {
            const float dx = x - h.windowX, dy = y - h.windowY;
            l += std::exp(-(dx * dx + dy * dy) / (2 * 70.0f * 70.0f));
        }
        return std::min(1.0f, l);
    }
    Rgba lit(Rgba base, float x, float y) const {
        return mix(scale(base, moon(x, y) * 1.1f), {255, 196, 110, base.a}, lantern(x, y) * 0.55f);
    }

    // Adds an item unless it is wholly outside the view (with a margin for tall glyphs).
    void add(Layer layer, Item item) const {
        const float m = 3 * T;
        const bool round = item.kind == Kind::Ellipse || item.kind == Kind::Glow;
        const float rx = round ? item.w : 0, ry = item.kind == Kind::Ellipse ? item.h : rx;
        const float x0 = item.x - rx, y0 = item.y - ry;
        const float x1 = item.kind == Kind::Rect ? item.x + item.w : item.x + rx;
        const float y1 = item.kind == Kind::Rect ? item.y + item.h : item.y + ry;
        if (x1 < v.x - m || y1 < v.y - m || x0 > v.x + v.w + m || y0 > v.y + v.h + m)
            return;
        scene[layer].push_back(item);
    }
    void glyph(Layer l, float x, float y, float size, float angle, char32_t ch, Rgba c) const {
        add(l, {Kind::Glyph, ch, x, y, size, angle, c});
    }
    void rect(Layer l, float x, float y, float w, float h, Rgba c) const {
        add(l, {Kind::Rect, 0, x, y, w, h, c});
    }
    void ellipse(Layer l, float x, float y, float rx, float ry, Rgba c) const {
        add(l, {Kind::Ellipse, 0, x, y, rx, ry, c});
    }
    void glow(Layer l, float x, float y, float r, Rgba c) const {
        add(l, {Kind::Glow, 0, x, y, r, 0, c});
    }
    // Text centred on (x, y), one glyph per character.
    void label(Layer l, float x, float y, std::string_view text, float size, Rgba c) const {
        const float advance = size * 0.6f;
        float cx = x - advance * static_cast<float>(text.size()) / 2 + advance / 2;
        for (char ch : text) {
            if (ch != ' ')
                glyph(l, cx, y, size, 0, static_cast<char32_t>(static_cast<unsigned char>(ch)), c);
            cx += advance;
        }
    }
};

// Where a moving thing is drawn, in tiles, `alpha` of a step on from its motion.
Scene::Point drawnAt(const game::world::Motion& m, float alpha) {
    const float p = std::min(1.0f, m.progress + alpha * m.perTick);
    return {static_cast<float>(m.from.x) + static_cast<float>(m.to.x - m.from.x) * p,
            static_cast<float>(m.from.y) + static_cast<float>(m.to.y - m.from.y) * p};
}

// Where a creature drawn at `tile` stands.
Scene::Point feetOf(Scene::Point tile) { return {(tile.x + 0.5f) * T, (tile.y + 0.6f) * T}; }
Scene::Point feetOf(const game::world::Monster& m, float alpha) {
    return feetOf(drawnAt(game::world::World::motionOf(m), alpha));
}
Scene::Point heroFeet(const game::world::World& world, float alpha) {
    return feetOf(drawnAt(world.heroMotion(), alpha));
}

// Houses: one per block of wall tiles, found from each block's top-left corner.
void findHouses(Scene& scene, const game::world::WorldDef& map) {
    scene.houses.clear();
    scene.housesOf = &map;
    const auto wall = [&](int x, int y) {
        return map.inBounds({x, y}) && map.at({x, y}).terrain == Terrain::Wall;
    };
    for (int y = 0; y < map.height; ++y)
        for (int x = 0; x < map.width; ++x) {
            if (!wall(x, y) || wall(x - 1, y) || wall(x, y - 1))
                continue;
            int w = 0, h = 0;
            while (wall(x + w, y))
                ++w;
            while (wall(x, y + h))
                ++h;
            const float px = x * T, py = y * T, pw = w * T, ph = h * T;
            scene.houses.push_back({px, py, pw, ph, px + pw * 0.62f, py + ph * 0.84f});
        }
}

// Ground fills, under everything.
void groundLayer(const Ctx& c) {
    for (int ty = c.ty0; ty < c.ty1; ++ty)
        for (int tx = c.tx0; tx < c.tx1; ++tx) {
            const Terrain terrain = *c.at(tx, ty);
            const float x0 = tx * T, y0 = ty * T;
            if (terrain == Terrain::Grass || terrain == Terrain::TallGrass || terrain == Terrain::Tree)
                c.rect(Layer::Ground, x0, y0, T, T,
                       scale({9, 20, 11, 255}, Ctx::moon(x0 + T / 2, y0 + T / 2) * 1.1f));
            else if (terrain == Terrain::Wall)
                c.rect(Layer::Ground, x0, y0, T, T, {14, 10, 8, 255});
        }
}

// Dark water with drifting ripple rows and the odd moon glint.
void waterLayer(const Ctx& c) {
    for (int ty = c.ty0; ty < c.ty1; ++ty)
        for (int tx = c.tx0; tx < c.tx1; ++tx) {
            if (c.at(tx, ty) != Terrain::Water)
                continue;
            const float x0 = tx * T, y0 = ty * T;
            c.rect(Layer::Ground, x0, y0, T, T, {7, 12, 24, 255});
            for (int r = 0; r < 4; ++r) {
                const float y = y0 + (r + 0.5f) * T / 4;
                for (int k = 0; k < 3; ++k) {
                    const float x = x0 + (k + 0.5f) * T / 3 + std::sin(c.t * 1.2f + y * 0.11f) * 3;
                    const float wave = 0.5f + 0.5f * std::sin(c.t * 1.7f + x * 0.05f + y * 0.3f);
                    const char32_t ch = U"-~-="[hash(tx * 3 + k, ty * 4 + r) % 4];
                    c.glyph(Layer::Ground, x, y, 9, 0, ch, withAlpha({70, 100, 150, 0}, 40 + 70 * wave));
                }
            }
            if (rnd(tx, ty, static_cast<int>(c.t * 2)) < 0.05f)
                c.glyph(Layer::Ground, x0 + rnd(tx, ty, 5) * T, y0 + rnd(tx, ty, 6) * T, 9, 0, U'.',
                        {200, 220, 255, 200});
        }
}

// Cobbles, paths, sand and rock, and the cave mouths where a path runs into rock.
void surfaceLayer(const Ctx& c) {
    for (int ty = c.ty0; ty < c.ty1; ++ty)
        for (int tx = c.tx0; tx < c.tx1; ++tx) {
            const Terrain terrain = *c.at(tx, ty);
            const float x0 = tx * T, y0 = ty * T;
            if (terrain == Terrain::Floor || terrain == Terrain::Path || terrain == Terrain::Sand) {
                const bool cobbles = terrain == Terrain::Floor;
                const Rgba stone = cobbles ? Rgba{118, 120, 138, 255} : Rgba{150, 122, 84, 255};
                c.rect(Layer::Ground, x0, y0, T, T, cobbles ? Rgba{14, 15, 20, 255} : Rgba{20, 15, 10, 255});
                for (int sy = 0; sy < 3; ++sy)
                    for (int sx = 0; sx < 3; ++sx) {
                        const int gx = tx * 3 + sx, gy = ty * 3 + sy;
                        if (cobbles && rnd(gx, gy, 4) < 0.18f)
                            continue; // a missing stone
                        const float x = x0 + (sx + 0.5f + (sy % 2) * 0.5f) * T / 3 + (rnd(gx, gy, 1) - 0.5f) * 4;
                        const float y = y0 + (sy + 0.5f) * T / 3 + (rnd(gx, gy, 2) - 0.5f) * 3;
                        const char32_t ch = cobbles ? U"oOoc.Q"[hash(gx, gy) % 6] : U".:,;"[hash(gx, gy) % 4];
                        const float size = cobbles ? 9 + rnd(gx, gy, 5) * 6 : 11;
                        const float angle = cobbles ? (rnd(gx, gy, 6) - 0.5f) * 50 : 0;
                        c.glyph(Layer::Ground, x, y, size, angle, ch,
                                c.lit(scale(stone, 0.45f + 0.45f * rnd(gx, gy, 3)), x, y));
                    }
            } else if (terrain == Terrain::Rock) {
                c.rect(Layer::Ground, x0, y0, T, T, {13, 13, 17, 255});
                for (int sy = 0; sy < 3; ++sy)
                    for (int sx = 0; sx < 3; ++sx) {
                        const int gx = tx * 3 + sx, gy = ty * 3 + sy;
                        const float x = x0 + (sx + 0.5f) * T / 3, y = y0 + (sy + 0.5f) * T / 3;
                        const float face = noise(x / 40.0f, y / 40.0f); // lit faces, dark cracks
                        const char32_t ch = face > 0.6f    ? U"@#%0"[hash(gx, gy) % 4]
                                            : face > 0.35f ? U"Oo0"[hash(gx, gy) % 3]
                                                           : U".:"[hash(gx, gy) % 2];
                        c.glyph(Layer::Ground, x, y, 11, 0, ch, c.lit(scale({135, 137, 150, 255}, 0.45f + face), x, y));
                    }
            }
        }
    // A cave mouth where a path runs into the cliff, over the rock around it.
    for (int ty = c.ty0; ty < c.ty1; ++ty)
        for (int tx = c.tx0; tx < c.tx1; ++tx)
            if (c.at(tx, ty) == Terrain::Path && c.at(tx, ty - 1) == Terrain::Rock)
                c.ellipse(Layer::Ground, (tx + 0.5f) * T, (ty - 0.4f) * T, T * 1.3f, T * 1.1f, {0, 0, 0, 235});
}

// Warm light pooling on the ground below each window.
void lightLayer(const Ctx& c) {
    for (const House& h : c.scene.houses)
        c.glow(Layer::Pools, h.windowX, h.windowY + 30, 150, {70, 45, 15, 255});
}

// Grass blades standing on their bases: each with its own lean, size and shade, swaying, and
// bent away from any creature close by.
void grassLayer(const Ctx& c) {
    for (int ty = c.ty0; ty < c.ty1; ++ty)
        for (int tx = c.tx0; tx < c.tx1; ++tx) {
            const Terrain terrain = *c.at(tx, ty);
            if (terrain != Terrain::Grass && terrain != Terrain::TallGrass)
                continue;
            const bool tall = terrain == Terrain::TallGrass;
            const int blades = tall ? kTallGrassBlades : kGrassBlades;
            for (int b = 0; b < blades; ++b) {
                const float x = (tx + rnd(tx, ty, b * 3)) * T, y = (ty + rnd(tx, ty, b * 3 + 1)) * T;
                const float r = rnd(tx, ty, b * 3 + 2);
                float angle = (r - 0.5f) * 36.0f + std::sin(c.t * 1.1f + x * 0.013f + y * 0.007f) * 8.0f;
                for (const Scene::Point& f : c.scene.feet) {
                    const float dx = x - f.x, dy = y - f.y, d = std::sqrt(dx * dx + dy * dy);
                    if (d < kBendRadius)
                        angle += (dx > 0 ? 1.0f : -1.0f) * (kBendRadius - d) * 0.9f;
                }
                const float size = tall ? 17 + r * 10 : 12 + r * 8;
                const Rgba base = tall ? Rgba{78, 168, 82, 255} : Rgba{52, 128, 62, 255};
                const float shade = 0.55f + 0.6f * rnd(tx, ty, b + 99);
                const Rgba color = withAlpha(scale(base, shade), 150 + r * 105);
                c.add(Layer::Grass, {Kind::Blade, U"|/\\l!|"[hash(tx, ty, b) % 6], x, y, size, angle,
                                     c.lit(color, x, y)});
            }
        }
}

// Trees: a shadow and a dense dome of glyphs, lit from the top-left.
void treeLayer(const Ctx& c) {
    for (int ty = c.ty0; ty < c.ty1; ++ty)
        for (int tx = c.tx0; tx < c.tx1; ++tx) {
            if (c.at(tx, ty) != Terrain::Tree)
                continue;
            const float cx = (tx + 0.5f) * T, cy = (ty + 0.5f) * T;
            c.ellipse(Layer::Trees, cx + 10, cy + 14, T * 1.15f, T * 1.15f, {0, 0, 0, 150});
            for (int i = 0; i < 100; ++i) {
                const float a = rnd(tx, ty, i) * 6.283f, rr = std::sqrt(rnd(tx, ty, i + 50)) * T * 1.1f;
                const float x = cx + std::cos(a) * rr, y = cy + std::sin(a) * rr * 0.9f;
                const float light = 0.35f + 0.8f * std::max(0.0f, ((cx - x) + (cy - y)) / (T * 1.6f) + 0.45f);
                c.glyph(Layer::Trees, x, y, 12, 0, U"@#&8%Q0$"[hash(tx, ty, i) % 8],
                        c.lit(scale({48, 124, 72, 255}, light), x, y));
            }
        }
}

// Houses: a shadow; shingle rows on a lit back slope and a dark front slope with a ridge between;
// then the front wall with a door and a glowing window.
void houseLayer(const Ctx& c) {
    constexpr Layer L = Layer::Houses;
    for (const House& h : c.scene.houses) {
        c.rect(L, h.x + 10, h.y + 12, h.w, h.h, {0, 0, 0, 140});
        const float roofH = h.h * 0.68f, ridge = h.y + roofH * 0.45f;
        c.rect(L, h.x, h.y, h.w, roofH, {22, 12, 8, 255});
        int row = 0;
        for (float y = h.y + 4; y < h.y + roofH; y += 6.5f, ++row) {
            const bool back = y < ridge;
            for (float x = h.x + 4 + (row % 2) * 4; x < h.x + h.w - 2; x += 8) {
                const Rgba shingle = back ? Rgba{176, 98, 56, 255} : Rgba{112, 58, 34, 255};
                const float r = rnd(static_cast<int>(x), static_cast<int>(y), 9);
                c.glyph(L, x, y, 10, 0, back ? U'n' : U'u', c.lit(scale(shingle, 0.8f + 0.4f * r), x, y));
            }
        }
        for (float x = h.x + 3; x < h.x + h.w - 2; x += 6)
            c.glyph(L, x, ridge, 10, 0, U'=', c.lit({205, 140, 90, 255}, x, ridge));
        const float wallY = h.y + roofH;
        c.rect(L, h.x, wallY, h.w, h.h - roofH, {34, 22, 14, 255});
        for (float x = h.x + 3; x < h.x + h.w - 2; x += 5)
            for (float y = wallY + 4; y < h.y + h.h - 1; y += 8)
                c.glyph(L, x, y, 10, 0, U'|', c.lit({110, 74, 46, 255}, x, y));
        c.rect(L, h.x + h.w * 0.25f - 6, wallY + 2, 12, h.h - roofH - 2, {12, 7, 4, 255});
        c.glow(L, h.windowX, h.windowY, 46, {255, 180, 80, 90});
        c.rect(L, h.windowX - 7, h.windowY - 6, 14, 10, {255, 190, 95, 255});
        c.glyph(L, h.windowX, h.windowY - 1, 11, 0, U'+', {120, 70, 30, 255});
    }
}

// A creature as glyph cells on a dark silhouette, standing on `feet`, or its reflection: mirrored
// about the feet and drawn only where it falls on water.
void drawCreature(const Ctx& c, const engine::Sprite* sprite, Scene::Point feet, float cell,
                  bool flip, std::optional<Rgba> flash, bool reflect) {
    if (!sprite) { // no art loaded: a single mark
        if (!reflect)
            c.glyph(Layer::Creatures, feet.x, feet.y - T / 2, 22, 0, U'@', flash.value_or(rgba(app::palette::kAccent)));
        return;
    }
    constexpr int step = 2; // sprite pixels per glyph cell
    for (int sy = 0; sy < sprite->height; sy += step)
        for (int sx = 0; sx < sprite->width; sx += step) {
            const auto& p = sprite->at(flip ? sprite->width - 1 - sx : sx, sy);
            if (!p)
                continue;
            const Rgba px = rgba(*p);
            const float x = feet.x + (sx - sprite->width / 2.0f) * cell / step;
            const float dy = (sy - sprite->height) * cell / step;
            const float y = reflect ? feet.y - dy + 2 : feet.y + dy;
            const float lum = (0.3f * p->r + 0.59f * p->g + 0.11f * p->b) / 255.0f;
            if (reflect) {
                if (c.atPx(x, y) != Terrain::Water)
                    continue;
                const float ripple = std::sin(y * 0.35f + c.t * 4) * 1.6f;
                c.glyph(Layer::Under, x + ripple, y, 9, 0, U":-=+"[static_cast<int>(std::min(3.0f, lum * 6))],
                        mix({60, 80, 120, 170}, withAlpha(scale(px, 0.7f), 170), 0.35f));
                continue;
            }
            c.rect(Layer::Creatures, x - cell / 2, y - cell / 2, cell, cell, {6, 6, 10, 235});
            const char32_t ch = U" .:-=+*#%@"[static_cast<int>(std::min(9.0f, 3 + lum * 9))];
            const Rgba color = mix(scale(px, 1.7f), {255, 205, 130, 255}, c.lantern(x, y) * 0.35f);
            c.glyph(Layer::Creatures, x, y, cell * 2.8f, 0, ch, flash ? mix(color, *flash, 0.7f) : color);
        }
}

// Landmark marks, monsters and the hero with shadows and the hero's name; their reflections and
// the tiles monsters are winding up to hit go under the grass.
void creatureLayer(const Ctx& c) {
    for (const game::world::Landmark& landmark : c.sv.map.landmarks) {
        const bool lair = std::holds_alternative<game::world::OpensMission>(landmark.opens);
        const float x = (landmark.at.x + 0.5f) * T, y = (landmark.at.y + 0.5f) * T;
        c.ellipse(Layer::Creatures, x, y, 9, 9, rgba(lair ? app::palette::kDmg : app::palette::kAccent, 230));
        const char32_t mark = lair ? U'!' : static_cast<char32_t>(landmark.name.empty() ? '?' : landmark.name[0]);
        c.glyph(Layer::Creatures, x, y, 14, 0, mark, rgba(lair ? app::palette::kText : app::palette::kBg));
    }
    const Rgba accent = rgba(app::palette::kAccent);
    for (const game::world::Monster& m : c.sv.world.monsters()) {
        if (m.mode == Mode::WindUp) {
            c.rect(Layer::Under, m.target.x * T, m.target.y * T, T, T, {150, 30, 45, 120});
            c.glow(Layer::Under, (m.target.x + 0.5f) * T, (m.target.y + 0.5f) * T, T, {255, 60, 70, 90});
        }
        const Scene::Point feet = feetOf(m, c.alpha);
        const bool flash = m.mode == Mode::WindUp && (c.sv.ticks / 4) % 2 == 0;
        c.ellipse(Layer::Creatures, feet.x, feet.y + 1, 13, 4, {0, 0, 0, 170});
        drawCreature(c, c.sv.monster, feet, 2.6f, false, std::nullopt, true);
        drawCreature(c, c.sv.monster, feet, 2.6f, false, flash ? std::optional(accent) : std::nullopt, false);
    }
    const Scene::Point feet = heroFeet(c.sv.world, c.alpha);
    const bool flip = c.sv.world.facing() == game::world::Dir::Left;
    c.ellipse(Layer::Creatures, feet.x, feet.y + 1, 19, 6, {0, 0, 0, 170});
    drawCreature(c, c.sv.hero, feet, 3.4f, flip, std::nullopt, true);
    drawCreature(c, c.sv.hero, feet, 3.4f, flip, std::nullopt, false);
    const float height = c.sv.hero ? c.sv.hero->height * 3.4f / 2 : T;
    c.label(Layer::Creatures, feet.x, feet.y - height - 12, c.sv.heroName, 14, {235, 235, 235, 235});
}

// Fireflies, bolts and the Nova ring glow; damage numbers rise over the hits.
void effectLayer(const Ctx& c) {
    for (int ty = c.ty0; ty < c.ty1; ++ty)
        for (int tx = c.tx0; tx < c.tx1; ++tx) {
            const Terrain terrain = *c.at(tx, ty);
            if ((terrain != Terrain::Grass && terrain != Terrain::TallGrass && terrain != Terrain::Water) ||
                rnd(tx, ty, 31) >= 0.07f)
                continue;
            const float phase = rnd(tx, ty, 34) * 6.283f, speed = 0.3f + rnd(tx, ty, 35) * 0.7f;
            const float x = (tx + rnd(tx, ty, 32)) * T + std::sin(c.t * speed + phase) * 14;
            const float y = (ty + rnd(tx, ty, 33)) * T + std::cos(c.t * speed * 0.8f + phase) * 9;
            const float pulse = 0.5f + 0.5f * std::sin(c.t * 2.4f + phase * 3);
            c.glow(Layer::Glow, x, y, 11, withAlpha({190, 255, 150, 0}, 80 * pulse));
            c.ellipse(Layer::Glow, x, y, 1.5f, 1.5f, withAlpha({235, 255, 215, 0}, 220 * pulse));
        }
    for (const game::world::Bolt& bolt : c.sv.world.bolts()) {
        const Scene::Point at = drawnAt(c.sv.world.motionOf(bolt), c.alpha);
        const float x = (at.x + 0.5f) * T, y = (at.y + 0.5f) * T;
        c.glow(Layer::Glow, x, y, 18, {255, 170, 80, 220});
        c.glyph(Layer::Glow, x, y, 16, 0, U'*', {255, 230, 170, 255});
    }
    if (const auto& nova = c.sv.nova) {
        const float alpha = std::max(0.0f, 220.0f - nova->age * 15.0f);
        for (int dy = -nova->radius; dy <= nova->radius; ++dy)
            for (int dx = -nova->radius; dx <= nova->radius; ++dx)
                if (std::max(std::abs(dx), std::abs(dy)) == nova->radius)
                    c.glow(Layer::Glow, (nova->center.x + dx + 0.5f) * T, (nova->center.y + dy + 0.5f) * T,
                           16, withAlpha(rgba(app::palette::kMp), alpha));
    }
    for (const app::OverworldScreen::FloatingNumber& n : c.sv.numbers) {
        const float x = (n.at.x + 0.5f) * T, y = n.at.y * T - 10 - n.age * 0.7f;
        const Rgba color = rgba(n.onHero ? app::palette::kDmg : app::palette::kAccent);
        c.label(Layer::Text, x, y, std::to_string(n.amount), 18, color);
    }
}

} // namespace

void buildScene(Scene& scene, const SceneView& sv, float time, View view, float alpha) {
    for (std::vector<Item>& layer : scene.layers)
        layer.clear();
    if (scene.housesOf != &sv.map)
        findHouses(scene, sv.map);
    scene.feet.clear();
    scene.feet.push_back(heroFeet(sv.world, alpha));
    for (const game::world::Monster& m : sv.world.monsters())
        scene.feet.push_back(feetOf(m, alpha));

    const auto tile = [](float px) { return static_cast<int>(std::floor(px / T)); };
    const Ctx c{scene, sv, time, alpha, view,
                std::max(0, tile(view.x) - 2), std::max(0, tile(view.y) - 2),
                std::min(sv.map.width, tile(view.x + view.w) + 3),
                std::min(sv.map.height, tile(view.y + view.h) + 3)};
    groundLayer(c);
    waterLayer(c);
    surfaceLayer(c);
    lightLayer(c);
    grassLayer(c);
    treeLayer(c);
    houseLayer(c);
    creatureLayer(c);
    effectLayer(c);
}

} // namespace heroes::window
