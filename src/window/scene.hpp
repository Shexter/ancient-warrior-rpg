#pragma once

#include "app/screens/overworld.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

struct Font; // raylib

namespace heroes::window {

// Scene units: a map tile is kTilePx across. The drawer zooms them to the window.
inline constexpr float kTilePx = 26.0f;

struct Rgba {
    std::uint8_t r = 0, g = 0, b = 0, a = 255;
    bool operator==(const Rgba&) const = default;
};

// One thing to draw, in scene units.
struct Item {
    enum class Kind : std::uint8_t {
        Glyph,   // centred on (x, y); w = font size, h = rotation in degrees
        Blade,   // a glyph standing on (x, y), rotating about its foot; w and h as Glyph
        Rect,    // top-left (x, y), w x h
        Ellipse, // centred on (x, y), radii w and h
        Glow,    // centred on (x, y), radius w; `color` at the centre, fading to clear
    };
    Kind kind;
    char32_t glyph;
    float x, y, w, h;
    Rgba color;
    bool operator==(const Item&) const = default;
};

// Bottom to top. Pools and Glow add light; the rest blend over what is below.
enum class Layer { Ground, Pools, Under, Grass, Trees, Houses, Creatures, Glow, Text };
inline constexpr int kLayerCount = 9;
enum class Blend { Alpha, Additive };
constexpr Blend blendOf(Layer l) {
    return l == Layer::Pools || l == Layer::Glow ? Blend::Additive : Blend::Alpha;
}

// The part of the scene in view, in scene units.
struct View {
    float x, y, w, h;
};

// A house found from a block of wall tiles, with the window its light comes from.
struct House {
    float x, y, w, h;
    float windowX, windowY;
};

struct Scene {
    std::array<std::vector<Item>, kLayerCount> layers;
    std::vector<House> houses; // found once per map
    const game::world::WorldDef* housesOf = nullptr;
    struct Point {
        float x, y;
    };
    std::vector<Point> feet; // every creature's, for the grass to bend away from
    // The drawn camera's top-left in scene units, easing after the screen's; none until the
    // Overworld is first drawn.
    std::optional<Point> camera;

    std::vector<Item>& operator[](Layer l) { return layers[static_cast<std::size_t>(l)]; }
    const std::vector<Item>& operator[](Layer l) const {
        return layers[static_cast<std::size_t>(l)];
    }
};

// A character model as rendered this frame (window/models.hpp), top row first.
struct Pixels {
    int w = 0, h = 0;
    std::vector<Rgba> px;
};

// Where the hero is drawn `alpha` of a step on, in map tiles (fractional while it moves).
Scene::Point heroDrawnAt(const game::world::World& world, float alpha);

// Scene units per model pixel (D8).
inline constexpr float kModelScale = 2.2f;

// Appends `pixels` to `out` as glyphs on a dark silhouette, `scale` scene units per pixel, centred
// on `feet` with the lowest drawn row standing on it: first a silhouette cell per pixel, then a
// glyph from the brightness ramp in the pixel's colour, lifted so dark colours still read. Pixels
// under alpha 40 are empty. Returns the drawn height in scene units (0 when nothing is drawn).
float glyphsFromPixels(std::vector<Item>& out, const Pixels& pixels, Scene::Point feet, float scale);

// Rebuilds `scene` for `view` at animation time `time` (seconds), reusing its buffers, with
// creatures and bolts drawn `alpha` (0 to 1) of a step past the game's state. Reads the game;
// never changes it. Every look varies only by map position and time. The hero is drawn from
// `hero` when given, else from its battle art.
void buildScene(Scene& scene, const app::OverworldScreen::SceneView& sv, float time, View view,
                float alpha = 0, const Pixels* hero = nullptr);

// Hands every layer to `out` bottom to top: out.layer(Layer), then out.item(item) for each item.
template <class Out> void submit(const Scene& scene, Out& out) {
    for (int i = 0; i < kLayerCount; ++i) {
        out.layer(static_cast<Layer>(i));
        for (const Item& item : scene.layers[static_cast<std::size_t>(i)])
            out.item(item);
    }
}

// Draws the scene with raylib into a window-sized area: scene unit (view.x, view.y) lands on
// pixel (0, 0), and `zoom` pixels make one scene unit. Then a vignette over the edges.
void drawScene(const Scene& scene, const Font& font, View view, float zoom);

// Scene units per pixel for a window `windowH` pixels tall: every size shows as much map height.
inline float sceneZoom(int windowH) { return static_cast<float>(windowH) / 720.0f; }

// The Overworld in a w x h pixel window: its scene at animation time `time`, `alpha` of a step
// on, then the screen's HUD drawn into `hud` and over the scene. Rebuilds `scene`, and eases its
// camera over the `dt` seconds since the last frame. `hero` as for buildScene.
void drawOverworld(const app::OverworldScreen& screen, engine::Framebuffer& hud, Scene& scene,
                   const Font& font, float time, float dt, float alpha, int w, int h,
                   const Pixels* hero = nullptr);

// The map tile under window pixel (x, y), as last drawn.
game::world::Tile sceneTileAt(const Scene& scene, int x, int y, int windowH);

} // namespace heroes::window
