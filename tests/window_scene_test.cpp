#include "window/models.hpp"
#include "window/scene.hpp"

#include "screen_harness.hpp"

#include "raylib.h"

#include <gtest/gtest.h>

#include <cmath>
#include <fstream>
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
    const Pixels model{2, 2, {{200, 40, 40, 255}, {200, 40, 40, 255}, {90, 90, 90, 255}, {90, 90, 90, 255}}};
    for (int i = 0; i < 100; ++i) // with the battle art, then with a model
        buildScene(scene, w.view(), static_cast<float>(i) / 10, wholeMap(world), 0, i % 2 ? &model : nullptr);
    EXPECT_EQ(*w.h.session.state, state); // its random stream included
    EXPECT_EQ(w.screen().world().monsters(), monsters);
    EXPECT_EQ(w.screen().world().hero(), hero);
}

TEST(WindowScene, TheHeroIsDrawnPartWayThroughAStep) {
    const Tile spawn = shippedWorld().spawn;
    const auto world = quietWorldAt(spawn);
    InWorld w("scene_halfway", world);
    ASSERT_TRUE(w.h.press({ch('d'), {Key::Char, 'd', 0, 0, engine::KeyAction::Release}})); // one 8-tick move right, started at once
    ASSERT_TRUE(w.h.wait(3.0 / 60));             // 4 ticks in
    ASSERT_EQ(w.screen().sceneView().world.hero(), (Tile{spawn.x + 1, spawn.y}));
    Scene scene;
    buildScene(scene, w.view(), 0.0f, wholeMap(world));
    EXPECT_FLOAT_EQ(scene.feet[0].x, (static_cast<float>(spawn.x) + 0.5f + 0.5f) * T);
    buildScene(scene, w.view(), 0.0f, wholeMap(world), 0.5f); // and half a tick more
    EXPECT_FLOAT_EQ(scene.feet[0].x, (static_cast<float>(spawn.x) + 0.5f + 0.5f + 0.5f / 8) * T);
}

TEST(WindowScene, AClickReadsTheTileUnderTheDrawnCamera) {
    Scene scene;
    scene.camera = Scene::Point{10 * T + 13, 4 * T};
    const Tile t = sceneTileAt(scene, 0, 0, 720); // zoom 1
    EXPECT_EQ(t, (Tile{10, 4}));
    EXPECT_EQ(sceneTileAt(scene, 14, 0, 720), (Tile{11, 4}));
}

TEST(GlyphsFromPixels, PixelsBecomeRampGlyphsStandingOnTheFeet) {
    // 3 x 3: an empty top row, a dark red and an empty pixel, then a white and a mid grey on the
    // bottom row. The lowest drawn row stands on the feet.
    const Rgba none{0, 0, 0, 0};
    const Pixels pixels{3, 3, {none, none, none,
                               {60, 0, 0, 255}, none, none,
                               {255, 255, 255, 255}, {128, 128, 128, 255}, none}};
    std::vector<Item> out;
    const float height = glyphsFromPixels(out, pixels, {100, 200}, 2);
    EXPECT_FLOAT_EQ(height, 4);
    ASSERT_EQ(out.size(), 6u); // a silhouette cell, then a glyph, per drawn pixel
    for (int i = 0; i < 3; ++i)
        EXPECT_EQ(out[static_cast<std::size_t>(i)].kind, Item::Kind::Rect);
    const Item& red = out[3];
    const Item& white = out[4];
    const Item& grey = out[5];
    // Centred on x: pixel 0 of 3 is one pixel left of the middle; rows sit above the feet.
    EXPECT_FLOAT_EQ(red.x, 98);
    EXPECT_FLOAT_EQ(red.y, 197);
    EXPECT_FLOAT_EQ(white.y, 199);
    EXPECT_FLOAT_EQ(grey.x, 100);
    EXPECT_EQ(white.glyph, U'@'); // brightest end of the ramp
    EXPECT_EQ(grey.glyph, U'*');
    EXPECT_EQ(red.glyph, U'-');
    // A dark colour is lifted until its brightest channel reaches 150; a bright one by 1.2.
    EXPECT_EQ(red.color, (Rgba{150, 0, 0, 255}));
    EXPECT_EQ(white.color, (Rgba{255, 255, 255, 255}));
    EXPECT_EQ(grey.color, (Rgba{153, 153, 153, 255}));
}

TEST(GlyphsFromPixels, NothingDrawnForClearPixels) {
    const Pixels pixels{2, 1, {{9, 9, 9, 39}, {0, 0, 0, 0}}};
    std::vector<Item> out;
    EXPECT_EQ(glyphsFromPixels(out, pixels, {0, 0}, 2), 0);
    EXPECT_TRUE(out.empty());
}

namespace {

constexpr float kFrame = 1.0f / 60;
constexpr float kPi = 3.14159265f;

// The pose after `frames` frames moving `step` tiles a frame from `pose`.
Pose walk(Pose pose, Scene::Point step, int frames) {
    for (int i = 0; i < frames; ++i)
        pose = poseOf(pose, {step}, kFrame);
    return pose;
}

} // namespace

TEST(Pose, FacingFollowsMovement) {
    // Facing down, the hero walks left for a tile at the field pace (8 frames), then stops.
    Pose pose = poseOf({}, {{-1.0f / 8, 0}}, kFrame);
    EXPECT_EQ(pose.clip, Clip::Walk);
    EXPECT_LT(pose.yaw, -0.1f); // turning toward left (-pi/2)...
    EXPECT_GT(pose.yaw, -kPi / 4); // ...not snapped there
    pose = walk(pose, {-1.0f / 8, 0}, 6); // 0.12 s in
    EXPECT_NEAR(pose.yaw, -kPi / 2, 0.1f);
    EXPECT_EQ(pose.clip, Clip::Walk);
    const float facing = pose.yaw;
    pose = poseOf(pose, {}, kFrame); // stops
    EXPECT_EQ(pose.clip, Clip::Idle);
    EXPECT_EQ(pose.from, Clip::Walk);
    EXPECT_LT(pose.blend, 1); // blending into idle, not cut
    EXPECT_FLOAT_EQ(pose.yaw, facing); // and keeps facing its way
    pose = walk(pose, {}, 9);
    EXPECT_FLOAT_EQ(pose.blend, 1);
}

TEST(Pose, TurnsTheShortWayRound) {
    Pose pose;
    pose.yaw = 3.0f; // just short of up, turning toward just past it (-3.0)
    pose = poseOf(pose, {{std::sin(-3.0f), std::cos(-3.0f)}}, kFrame);
    EXPECT_GT(std::abs(pose.yaw), 3.0f); // through up, not back through down
}

TEST(Pose, WalkPaceFollowsTheMove) {
    Pose start;
    start.clip = Clip::Walk;
    const Pose fast = walk(start, {1.0f / 8, 0}, 8);   // a tile at the field pace
    const Pose slow = walk(start, {1.0f / 16, 0}, 16); // a tile at half that pace
    EXPECT_NEAR(fast.time, kWalkSecondsPerTile, 1e-4f);
    EXPECT_NEAR(slow.time, kWalkSecondsPerTile, 1e-4f);
}

TEST(Pose, ClipsCrossfadeOverAFifteenthOfASecond) {
    Pose pose;
    pose.clip = Clip::Walk;
    pose.time = 0.3f;
    pose = poseOf(pose, {.attacked = true}, 0.075f);
    EXPECT_EQ(pose.clip, Clip::Attack);
    EXPECT_EQ(pose.from, Clip::Walk);
    EXPECT_FLOAT_EQ(pose.fromTime, 0.3f);
    EXPECT_FLOAT_EQ(pose.blend, 0.5f);
    pose = poseOf(pose, {}, 0.075f);
    EXPECT_FLOAT_EQ(pose.blend, 1);
}

TEST(Pose, AnAttackPlaysOnceThenTheHeroIdles) {
    Pose pose = poseOf({}, {.attacked = true}, kFrame);
    pose = walk(pose, {}, 60); // 1.0 s: still swinging
    EXPECT_EQ(pose.clip, Clip::Attack);
    pose = walk(pose, {}, 5);
    EXPECT_EQ(pose.clip, Clip::Idle);
    pose = poseOf(pose, {.hit = true}, kFrame); // a hit cuts in at once
    EXPECT_EQ(pose.clip, Clip::Hit);
}

TEST(ModelFiles, AMissingFileLeavesThatHeroOnItsSprite) {
    const fs::path dir = fs::temp_directory_path() / "heroes_no_models";
    fs::create_directories(dir);
    const std::vector<std::string> ids{"fighter"};
    const auto files = loadModelFiles(dir, ids);
    ASSERT_TRUE(files.has_value());
    EXPECT_TRUE(files->empty());
}

TEST(ModelFiles, AModelWithoutAClipIsAnError) {
    // The shipped Fighter with its hit clip renamed (same length, so the file stays valid).
    std::string glb = readFile(fs::path(HEROES_SOURCE_DIR) / "assets/models/hero_fighter.glb");
    const auto at = glb.find("\"name\":\"Hit_A\"");
    ASSERT_NE(at, std::string::npos);
    glb.replace(at, 14, "\"name\":\"Hit_Q\"");
    const fs::path dir = fs::temp_directory_path() / "heroes_clipless_model";
    fs::create_directories(dir);
    std::ofstream(dir / "hero_fighter.glb", std::ios::binary) << glb;
    const std::vector<std::string> ids{"fighter"};
    const auto files = loadModelFiles(dir, ids);
    ASSERT_FALSE(files.has_value());
    EXPECT_NE(files.error().find("hero_fighter.glb: missing clip 'Hit_A'"), std::string::npos) << files.error();
}

TEST(ModelFiles, TheShippedHeroesHaveEveryClip) {
    std::vector<std::string> ids;
    for (const game::PathDef& path : game::shippedContent().paths())
        ids.push_back(path.id);
    auto files = loadModelFiles(fs::path(HEROES_SOURCE_DIR) / "assets/models", ids);
    ASSERT_TRUE(files.has_value()) << files.error();
    EXPECT_EQ(files->size(), 3u);
    for (auto& [id, f] : *files)
        UnloadModelAnimations(f.anims, f.count);
}
