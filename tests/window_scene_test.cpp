#include "window/scene.hpp"

#include "screen_harness.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <utility>
#include <vector>

using namespace screen_test;
using namespace heroes::window;
using game::world::Tile;
using OverworldView = OverworldScreen::SceneView;

namespace {

constexpr float T = kTilePx;

// The shipped map with no monsters, the hero starting on `spawn`.
game::world::WorldDef quietWorldAt(Tile spawn) {
    game::world::WorldDef w = shippedWorld();
    w.zones.clear();
    w.spawn = spawn;
    return w;
}

// A 12x12 meadow with one level 3 Fox that comes for the hero at once.
game::world::WorldDef huntingGround() {
    std::string map;
    for (int y = 0; y < 12; ++y)
        map += std::string(12, '.') + '\n';
    std::string field = readFile(fs::path(HEROES_SOURCE_DIR) / "data" / "world" / "field.json");
    field.replace(field.find("\"noticeRadius\": 6"), 17, "\"noticeRadius\": 12");
    field.replace(field.find("\"respawnDistance\": 12"), 21, "\"respawnDistance\": 5");
    auto world = game::world::parseWorld(
        map,
        R"({"legend": {".": {"terrain": "grass", "walkable": true}}, "spawn": {"x": 1, "y": 1},
            "landmarks": [],
            "zones": [{"name": "All", "x": 0, "y": 0, "w": 12, "h": 12, "level": 3, "count": 1}]})",
        field, game::shippedContent());
    EXPECT_TRUE(world.has_value());
    return std::move(*world);
}

// A harness in the Overworld of `world`, drawn once so the camera is set.
struct InWorld {
    explicit InWorld(const std::string& name, const game::world::WorldDef& world,
                     game::GameState state = freshGame())
        : h(name) {
        h.session.world = &world;
        h.session.large = shippedLargeSprites();
        h.startAtHub(std::move(state));
        EXPECT_TRUE(h.press({kHubExplore}));
        draw();
    }
    const OverworldScreen& screen() const {
        return *dynamic_cast<const OverworldScreen*>(h.app->top());
    }
    void draw() const {
        Framebuffer hud(80, 24);
        screen().drawHud(hud, {50, 28});
    }
    OverworldView view() const { return screen().sceneView(); }

    Harness h;
};

// The whole map in view.
View wholeMap(const game::world::WorldDef& map) {
    return {0, 0, static_cast<float>(map.width) * T, static_cast<float>(map.height) * T};
}

bool onTile(const Item& item, Tile t) {
    return static_cast<int>(std::floor(item.x / T)) == t.x && static_cast<int>(std::floor(item.y / T)) == t.y;
}

game::world::Terrain terrainUnder(const game::world::WorldDef& map, float x, float y) {
    return map.at({static_cast<int>(std::floor(x / T)), static_cast<int>(std::floor(y / T))}).terrain;
}

} // namespace

TEST(WindowScene, BuildingAgainReusesTheBuffers) {
    const auto world = quietWorldAt(shippedWorld().spawn);
    InWorld w("scene_reuse", world);
    Scene scene;
    buildScene(scene, w.view(), 1.0f, wholeMap(world));
    std::vector<std::pair<const Item*, std::size_t>> before;
    for (const auto& layer : scene.layers)
        before.emplace_back(layer.data(), layer.capacity());
    buildScene(scene, w.view(), 2.0f, wholeMap(world));
    for (std::size_t i = 0; i < scene.layers.size(); ++i) {
        EXPECT_EQ(scene.layers[i].data(), before[i].first) << "layer " << i;
        EXPECT_EQ(scene.layers[i].capacity(), before[i].second) << "layer " << i;
    }
}

TEST(WindowScene, LayersAreSubmittedBottomToTopWithTheirItemsInOrder) {
    const auto world = quietWorldAt(shippedWorld().spawn);
    InWorld w("scene_submit", world);
    Scene scene;
    buildScene(scene, w.view(), 1.0f, wholeMap(world));
    struct Recorder {
        std::vector<Layer> layers;
        std::vector<Item> items;
        void layer(Layer l) { layers.push_back(l); }
        void item(const Item& i) { items.push_back(i); }
    } out;
    submit(scene, out);
    std::vector<Layer> expectedLayers;
    std::vector<Item> expectedItems;
    for (int i = 0; i < kLayerCount; ++i) {
        expectedLayers.push_back(static_cast<Layer>(i));
        const auto& layer = scene.layers[static_cast<std::size_t>(i)];
        expectedItems.insert(expectedItems.end(), layer.begin(), layer.end());
        const bool quiet = i == static_cast<int>(Layer::Under) || i == static_cast<int>(Layer::Text);
        EXPECT_NE(layer.empty(), !quiet) << "layer " << i; // no monster, water or hit near spawn
    }
    EXPECT_EQ(out.layers, expectedLayers);
    EXPECT_EQ(out.items, expectedItems);
    EXPECT_EQ(blendOf(Layer::Pools), Blend::Additive);
    EXPECT_EQ(blendOf(Layer::Glow), Blend::Additive);
    EXPECT_EQ(blendOf(Layer::Grass), Blend::Alpha);
}

TEST(WindowScene, StableLook) {
    const auto world = quietWorldAt(shippedWorld().spawn);
    InWorld w("scene_stable", world);
    const View here{20 * T, 5 * T, 30 * T, 16 * T}, away{40 * T, 12 * T, 30 * T, 16 * T};
    Scene scene;
    buildScene(scene, w.view(), 3.0f, here);
    const Scene first = scene;
    buildScene(scene, w.view(), 3.0f, away);
    buildScene(scene, w.view(), 3.0f, here);
    for (Layer l : {Layer::Ground, Layer::Grass, Layer::Trees, Layer::Houses})
        EXPECT_EQ(scene[l], first[l]) << "layer " << static_cast<int>(l);
}

TEST(WindowScene, EveryTreeInViewHasAShadowAndADome) {
    const auto world = quietWorldAt(shippedWorld().spawn);
    InWorld w("scene_trees", world);
    const View view{2 * T, 2 * T, 40 * T, 24 * T};
    Scene scene;
    buildScene(scene, w.view(), 0.0f, view);
    int trees = 0;
    for (int y = 2; y < 26; ++y)
        for (int x = 2; x < 42; ++x) {
            if (world.at({x, y}).terrain != game::world::Terrain::Tree)
                continue;
            ++trees;
            const float cx = (x + 0.5f) * T, cy = (y + 0.5f) * T;
            int shadows = 0, glyphs = 0;
            for (const Item& i : scene[Layer::Trees]) {
                shadows += i.kind == Item::Kind::Ellipse && i.x == cx + 10 && i.y == cy + 14;
                glyphs += i.kind == Item::Kind::Glyph && std::hypot(i.x - cx, i.y - cy) <= T * 1.1f;
            }
            EXPECT_EQ(shadows, 1) << x << "," << y;
            EXPECT_GE(glyphs, 100) << x << "," << y;
        }
    EXPECT_GE(trees, 5);
}

TEST(WindowScene, TheStarterMapHasFourLitHouses) {
    const auto world = quietWorldAt(shippedWorld().spawn);
    InWorld w("scene_houses", world);
    Scene scene;
    buildScene(scene, w.view(), 0.0f, wholeMap(world));
    ASSERT_EQ(scene.houses.size(), 4u);
    for (const House& house : scene.houses) {
        EXPECT_EQ(terrainUnder(world, house.x + 1, house.y + 1), game::world::Terrain::Wall);
        int windows = 0, pools = 0;
        for (const Item& i : scene[Layer::Houses])
            windows += i.kind == Item::Kind::Rect && i.x == house.windowX - 7 && i.y == house.windowY - 6 &&
                       i.color == Rgba{255, 190, 95, 255};
        for (const Item& i : scene[Layer::Pools])
            pools += i.x == house.windowX && i.y == house.windowY + 30;
        EXPECT_EQ(windows, 1);
        EXPECT_EQ(pools, 1);
    }
}

TEST(WindowScene, NothingIsDrawnFarOutsideTheView) {
    const auto world = quietWorldAt(shippedWorld().spawn);
    InWorld w("scene_cull", world);
    const View view{20 * T, 8 * T, 20 * T, 10 * T};
    Scene scene;
    buildScene(scene, w.view(), 1.0f, view);
    const float m = 3 * T;
    for (const auto& layer : scene.layers)
        for (const Item& i : layer) {
            const float reach = i.kind == Item::Kind::Rect ? 0 : i.kind == Item::Kind::Glow || i.kind == Item::Kind::Ellipse ? i.w : 0;
            const float right = i.kind == Item::Kind::Rect ? i.x + i.w : i.x + reach;
            const float bottom = i.kind == Item::Kind::Rect ? i.y + i.h : i.y + (i.kind == Item::Kind::Ellipse ? i.h : reach);
            EXPECT_GE(right, view.x - m);
            EXPECT_GE(bottom, view.y - m);
            EXPECT_LE(i.x - reach, view.x + view.w + m);
            EXPECT_LE(i.y - reach, view.y + view.h + m);
        }
}

TEST(WindowScene, GrassBendsAwayFromACreatureCloseBy) {
    const Tile pondside{10, 17};
    const auto near = quietWorldAt(pondside), far = quietWorldAt({10, 5});
    InWorld a("scene_bend_near", near), b("scene_bend_far", far);
    Scene withHero, without;
    buildScene(withHero, a.view(), 2.0f, wholeMap(near));
    buildScene(without, b.view(), 2.0f, wholeMap(far));
    const float fx = (pondside.x + 0.5f) * T, fy = (pondside.y + 0.6f) * T;
    int checked = 0;
    for (const Item& blade : withHero[Layer::Grass]) {
        const float dx = blade.x - fx, d = std::hypot(dx, blade.y - fy);
        if (d >= 40 || std::abs(dx) < 4)
            continue;
        for (const Item& same : without[Layer::Grass])
            if (same.x == blade.x && same.y == blade.y) {
                // Leans further right when to the hero's right, further left when to the left.
                EXPECT_GT((blade.h - same.h) * dx, 0) << blade.x << "," << blade.y;
                ++checked;
            }
    }
    EXPECT_GT(checked, 5);
}

TEST(WindowScene, BladeCountsFollowTheGrass) {
    const auto world = quietWorldAt(shippedWorld().spawn);
    InWorld w("scene_blades", world);
    Scene scene;
    buildScene(scene, w.view(), 0.0f, wholeMap(world));
    const auto bladesOn = [&](Tile t) {
        int n = 0;
        for (const Item& i : scene[Layer::Grass])
            n += i.kind == Item::Kind::Blade && onTile(i, t);
        return n;
    };
    ASSERT_EQ(world.at({3, 3}).terrain, game::world::Terrain::Grass);
    ASSERT_EQ(world.at({60, 8}).terrain, game::world::Terrain::TallGrass);
    ASSERT_EQ(world.at({30, 15}).terrain, game::world::Terrain::Floor);
    EXPECT_EQ(bladesOn({3, 3}), 14);
    EXPECT_EQ(bladesOn({60, 8}), 26);
    EXPECT_EQ(bladesOn({30, 15}), 0);
}

TEST(WindowScene, ReflectionsFallOnlyOnWater) {
    const auto world = quietWorldAt({10, 17}); // on the grass just above the pond
    InWorld w("scene_reflect", world);
    Scene scene;
    buildScene(scene, w.view(), 1.0f, wholeMap(world));
    int reflections = 0;
    for (const Item& i : scene[Layer::Under]) {
        if (i.kind != Item::Kind::Glyph)
            continue;
        ++reflections;
        // A ripple shifts a reflection glyph sideways by at most 1.6 units.
        const auto onWater = [&](float x) { return terrainUnder(world, x, i.y) == game::world::Terrain::Water; };
        EXPECT_TRUE(onWater(i.x - 1.6f) || onWater(i.x + 1.6f)) << i.x << "," << i.y;
    }
    EXPECT_GT(reflections, 20);
}

TEST(WindowScene, WindUpStaysReadable) {
    const auto world = huntingGround();
    game::GameState state = freshGame();
    state.hero.maxHp = state.hero.hp = 100000;
    InWorld w("scene_windup", world, state);
    const auto windingUp = [&] {
        for (const auto& m : w.screen().world().monsters())
            if (m.mode == game::world::Monster::Mode::WindUp)
                return true;
        return false;
    };
    for (int i = 0; i < 60 * 20 && !windingUp(); ++i)
        ASSERT_TRUE(w.h.wait(1.0 / 60));
    ASSERT_TRUE(windingUp());
    w.draw();
    const game::world::Monster& fox = w.screen().world().monsters().front();

    OverworldView flashing = w.view(), steady = w.view();
    flashing.ticks = 0; // the flash alternates every 4 ticks
    steady.ticks = 4;
    Scene a, b;
    buildScene(a, flashing, 1.0f, wholeMap(world));
    buildScene(b, steady, 1.0f, wholeMap(world));
    for (const Scene* s : {&a, &b}) {
        int highlights = 0;
        for (const Item& i : (*s)[Layer::Under])
            highlights += i.kind == Item::Kind::Rect && i.x == fox.target.x * T && i.y == fox.target.y * T;
        EXPECT_EQ(highlights, 1);
    }
    EXPECT_NE(a[Layer::Creatures], b[Layer::Creatures]); // the Fox's colours alternate
}

TEST(WindowScene, DrawingLeavesTheGameUntouched) {
    const auto world = huntingGround();
    InWorld w("scene_untouched", world);
    ASSERT_TRUE(w.h.wait(1.0)); // the Fox is about
    const game::GameState state = *w.h.session.state;
    const auto monsters = w.screen().world().monsters();
    const Tile hero = w.screen().world().hero();
    Scene scene;
    for (int i = 0; i < 100; ++i)
        buildScene(scene, w.view(), static_cast<float>(i) / 10, wholeMap(world));
    EXPECT_EQ(*w.h.session.state, state); // its random stream included
    EXPECT_EQ(w.screen().world().monsters(), monsters);
    EXPECT_EQ(w.screen().world().hero(), hero);
}
