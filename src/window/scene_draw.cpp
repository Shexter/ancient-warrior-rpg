#include "window/scene.hpp"

#include "window/grid.hpp"

#include "raylib.h"
#include "rlgl.h"

#include <array>
#include <cmath>

namespace heroes::window {

namespace {

Color toColor(Rgba c) { return {c.r, c.g, c.b, c.a}; }

// Turns items into raylib calls. Shapes draw with the font atlas's white corner (raylib puts a
// 3x3 white block there), so glyphs and shapes share one texture and batch together.
class RaylibOut {
  public:
    explicit RaylibOut(const Font& font) : font_(font) {
        for (int c = 0; c < 128; ++c)
            ascii_[static_cast<std::size_t>(c)] = GetGlyphIndex(font, c);
    }
    void layer(Layer l) {
        EndBlendMode();
        BeginBlendMode(blendOf(l) == Blend::Additive ? BLEND_ADDITIVE : BLEND_ALPHA);
    }
    void item(const Item& i) {
        const Color color = toColor(i.color);
        switch (i.kind) {
        case Item::Kind::Glyph:
        case Item::Kind::Blade: {
            const int index = i.glyph < 128 ? ascii_[i.glyph] : GetGlyphIndex(font_, static_cast<int>(i.glyph));
            const Rectangle src = font_.recs[index];
            const GlyphInfo& g = font_.glyphs[index];
            const float base = static_cast<float>(font_.baseSize), s = i.w / base;
            const float advance = g.advanceX > 0 ? static_cast<float>(g.advanceX) : src.width;
            const float anchorY = i.kind == Item::Kind::Blade ? base * 0.85f : base / 2; // foot or middle
            const Vector2 origin{(advance / 2 - static_cast<float>(g.offsetX)) * s,
                                 (anchorY - static_cast<float>(g.offsetY)) * s};
            DrawTexturePro(font_.texture, src, {i.x, i.y, src.width * s, src.height * s}, origin, i.h, color);
            break;
        }
        case Item::Kind::Rect:
            DrawRectangleRec({i.x, i.y, i.w, i.h}, color);
            break;
        case Item::Kind::Ellipse:
            fan(i.x, i.y, i.w, i.h, color, color);
            break;
        case Item::Kind::Glow:
            fan(i.x, i.y, i.w, i.w, color, {color.r, color.g, color.b, 0});
            break;
        }
    }

  private:
    // A filled ellipse shading from `inner` at the centre to `outer` at the rim. raylib's own
    // circles leave texture coordinates unset, which samples the atlas at random.
    void fan(float x, float y, float rx, float ry, Color inner, Color outer) const {
        constexpr int kSteps = 24;
        const float u = (static_cast<float>(font_.texture.width) - 1.5f) / static_cast<float>(font_.texture.width);
        const float v = (static_cast<float>(font_.texture.height) - 1.5f) / static_cast<float>(font_.texture.height);
        rlCheckRenderBatchLimit(3 * kSteps);
        rlSetTexture(font_.texture.id);
        rlBegin(RL_TRIANGLES);
        rlTexCoord2f(u, v);
        for (int k = 0; k < kSteps; ++k) {
            const float a0 = 2 * PI * static_cast<float>(k) / kSteps, a1 = 2 * PI * static_cast<float>(k + 1) / kSteps;
            rlColor4ub(inner.r, inner.g, inner.b, inner.a);
            rlVertex2f(x, y);
            rlColor4ub(outer.r, outer.g, outer.b, outer.a);
            rlVertex2f(x + std::cos(a1) * rx, y + std::sin(a1) * ry);
            rlVertex2f(x + std::cos(a0) * rx, y + std::sin(a0) * ry);
        }
        rlEnd();
        rlSetTexture(0);
    }

    const Font& font_;
    std::array<int, 128> ascii_{};
};

void vignette(int w, int h) {
    for (int i = 0; i < 8; ++i) {
        const int band = (180 - i * 22) * h / 720;
        DrawRectangleGradientH(0, 0, band, h, {0, 0, 0, 26}, {0, 0, 0, 0});
        DrawRectangleGradientH(w - band, 0, band, h, {0, 0, 0, 0}, {0, 0, 0, 26});
        DrawRectangleGradientV(0, 0, w, band / 2, {0, 0, 0, 22}, {0, 0, 0, 0});
        DrawRectangleGradientV(0, h - band / 2, w, band / 2, {0, 0, 0, 0}, {0, 0, 0, 22});
    }
}

} // namespace

void drawScene(const Scene& scene, const Font& font, View view, float zoom) {
    Camera2D camera{};
    camera.target = {view.x, view.y};
    camera.zoom = zoom;
    const float atlasW = static_cast<float>(font.texture.width), atlasH = static_cast<float>(font.texture.height);
    SetShapesTexture(font.texture, {atlasW - 2, atlasH - 2, 1, 1}); // the atlas's white corner
    BeginMode2D(camera);
    RaylibOut out(font);
    submit(scene, out);
    EndBlendMode();
    EndMode2D();
    SetShapesTexture({}, {}); // back to raylib's own
    vignette(static_cast<int>(std::lround(view.w * zoom)), static_cast<int>(std::lround(view.h * zoom)));
}

void drawOverworld(const app::OverworldScreen& screen, engine::Framebuffer& hud, Scene& scene,
                   const Font& font, float time, float dt, float alpha, int w, int h,
                   const Pixels* hero) {
    const float zoom = sceneZoom(h);
    const float viewW = static_cast<float>(w) / zoom, viewH = static_cast<float>(h) / zoom;
    screen.drawHud(hud, {static_cast<int>(std::ceil(viewW / kTilePx)),
                         static_cast<int>(std::ceil(viewH / kTilePx))});
    const app::OverworldScreen::SceneView sv = screen.sceneView();
    const Scene::Point target{static_cast<float>(sv.camera.x) * kTilePx,
                              static_cast<float>(sv.camera.y) * kTilePx};
    const float ease = scene.camera ? 1 - std::exp(-12 * dt) : 1; // most of the way in 0.2 s
    const Scene::Point from = scene.camera.value_or(target);
    scene.camera = {from.x + (target.x - from.x) * ease, from.y + (target.y - from.y) * ease};
    const View view{scene.camera->x, scene.camera->y, viewW, viewH};
    buildScene(scene, sv, time, view, alpha, hero);
    drawScene(scene, font, view, zoom);
    drawGrid(hud, layoutFor({hud.width(), hud.height()}, w, h), font, /*skipEmpty=*/true);
}

game::world::Tile sceneTileAt(const Scene& scene, int x, int y, int windowH) {
    const float zoom = sceneZoom(windowH);
    const Scene::Point camera = scene.camera.value_or(Scene::Point{0, 0});
    return {static_cast<int>(std::floor((camera.x + static_cast<float>(x) / zoom) / kTilePx)),
            static_cast<int>(std::floor((camera.y + static_cast<float>(y) / zoom) / kTilePx))};
}

} // namespace heroes::window
