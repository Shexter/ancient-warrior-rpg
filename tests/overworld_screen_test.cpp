#include "frame_text.hpp"
#include "screen_harness.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

using namespace screen_test;

namespace {

using engine::KeyAction;

const KeyEvent kReleaseD{Key::Char, 'd', 0, 0, KeyAction::Release};

// Where the hero's @ is drawn.
std::pair<int, int> heroAt(const Framebuffer& frame) {
    for (int y = 0; y < frame.height(); ++y)
        for (int x = 0; x < frame.width(); ++x)
            if (frame.at(x, y).glyph == U'@')
                return {x, y};
    return {-1, -1};
}

void enterWorld(Harness& h, game::GameState state = freshGame()) {
    h.startAtHub(std::move(state));
    ASSERT_TRUE(h.press({kHubExplore}));
    ASSERT_EQ(h.app->depth(), 2u);
}

bool isTreeGlyph(char32_t c) { return c == U'Y' || c == U'T' || c == U'^'; }

} // namespace

TEST(Overworld, ExploreOpensTheWorldAtSpawn) {
    Harness h("world_enter");
    enterWorld(h);
    const Framebuffer frame = h.frame();
    // A 72-wide map in an 80-wide view is centred (4 columns of margin). The camera starts with
    // spawn (35, 14) in the middle: map rows 2-25.
    EXPECT_EQ(heroAt(frame), (std::pair{4 + 35, 12}));
    EXPECT_EQ(frame.at(0, 0).bg, palette::kBg);
    EXPECT_EQ(frame.at(0, 0).glyph, U' ');
}

TEST(Overworld, ExploreIsDisabledForAFallenHeroAndOnThePhone) {
    Harness fallen("world_fallen");
    game::GameState state = freshGame();
    state.hero.hp = 0;
    fallen.startAtHub(state);
    ASSERT_TRUE(fallen.press({kHubExplore}));
    EXPECT_EQ(fallen.app->depth(), 1u);
    EXPECT_NE(rowText(fallen.frame(), 7).find("Fallen: visit the Prior"), std::string::npos);

    Harness phone("world_phone");
    phone.startAtHub(freshGame());
    (void)phone.frame(engine::kPortraitCanvas);
    ASSERT_TRUE(phone.press({kHubExplore}));
    EXPECT_EQ(phone.app->depth(), 1u);
    EXPECT_TRUE(frameContains(phone.frame(engine::kPortraitCanvas), "Needs an 80×24 screen"));
}

TEST(Overworld, EscReturnsToTheHubAndReEnteringStartsAtSpawn) {
    Harness h("world_reenter");
    enterWorld(h);
    const Framebuffer start = h.frame();
    ASSERT_TRUE(h.press({ch('d')}));
    ASSERT_TRUE(h.wait(0.5));
    EXPECT_EQ(heroAt(h.frame()).first, heroAt(start).first + 1);
    ASSERT_TRUE(h.press({kEsc}));
    EXPECT_EQ(h.app->depth(), 1u);
    EXPECT_TRUE(frameContains(h.frame(), "▶ Explore")); // the Hub keeps its selection
    ASSERT_TRUE(h.press({kEnter}));
    EXPECT_EQ(h.frame(), start);
}

TEST(Overworld, AHeldKeyWalksUntilReleased) {
    Harness h("world_hold");
    enterWorld(h);
    const int x0 = heroAt(h.frame()).first;
    ASSERT_TRUE(h.press({ch('a'), {Key::Char, 'a', 0, 0, KeyAction::Release}})); // one tile
    ASSERT_TRUE(h.wait(0.2));
    EXPECT_EQ(heroAt(h.frame()).first, x0 - 1);
    ASSERT_TRUE(h.press({ch('d')})); // releases are reported now, so D stays held
    ASSERT_TRUE(h.wait(0.25));        // 16 ticks at 8 per tile
    ASSERT_TRUE(h.press({kReleaseD}));
    ASSERT_TRUE(h.wait(0.5));
    EXPECT_EQ(heroAt(h.frame()).first, x0 - 1 + 2);
}

TEST(Overworld, CanvasFillsTheTerminalUpTo200x60) {
    Harness h("world_canvas");
    enterWorld(h);
    EXPECT_EQ(h.app->canvasSize({150, 45}), (engine::Size{150, 45}));
    EXPECT_EQ(h.app->canvasSize({300, 90}), (engine::Size{200, 60}));
    EXPECT_EQ(h.app->canvasSize({60, 20}), engine::kCompactCanvas); // the loop shows the prompt
    h.session.layout = Layout::Compact;
    EXPECT_EQ(h.app->canvasSize({150, 45}), engine::kCompactCanvas);
}

TEST(Overworld, TheViewStopsAtTheMapEdge) {
    Harness h("world_edge");
    enterWorld(h);
    ASSERT_TRUE(h.press({ch('w'), {Key::Char, 'w', 0, 0, KeyAction::Release}}));
    ASSERT_TRUE(h.press({ch('w')}));
    ASSERT_TRUE(h.wait(3)); // up the path to the cave mouth at (35, 2)
    const Framebuffer frame = h.frame();
    EXPECT_EQ(heroAt(frame), (std::pair{4 + 35, 2}));
    for (int x = 4; x < 4 + 72; ++x) // the top row is the map's first row: all trees
        EXPECT_TRUE(isTreeGlyph(frame.at(x, 0).glyph)) << x;
    for (int x : {0, 3, 76, 79}) // nothing is drawn beyond the map
        EXPECT_EQ(frame.at(x, 0).glyph, U' ') << x;
}

TEST(Overworld, TerrainLooksTheSameWhenRevisited) {
    Harness h("world_stable");
    enterWorld(h);
    const Framebuffer before = h.frame();
    const auto walk = [&](char key, int tiles) {
        for (int i = 0; i < tiles; ++i) {
            ASSERT_TRUE(h.press({ch(key)})); // one tile per press on a plain terminal
            ASSERT_TRUE(h.wait(0.15));
            (void)h.frame(); // the loop draws every frame, moving the camera
        }
    };
    walk('s', 10); // far enough down that the view scrolls
    walk('w', 10); // and back onto spawn
    const Framebuffer after = h.frame();
    const auto [bx, by] = heroAt(before);
    const auto [ax, ay] = heroAt(after);
    ASSERT_EQ(ax, bx);
    ASSERT_NE(ay, by); // the camera did move
    int compared = 0;  // every map row on screen in both frames matches, wherever it is drawn
    for (int y = 0; y < before.height(); ++y) {
        const int y2 = y + ay - by;
        if (y2 < 0 || y2 >= after.height())
            continue;
        for (int x = 0; x < before.width(); ++x, ++compared)
            EXPECT_EQ(before.at(x, y), after.at(x, y2)) << x << "," << y;
    }
    EXPECT_GT(compared, 1000);
}

TEST(Overworld, GoldenFrameAtSpawn) {
    Harness h("world_golden");
    enterWorld(h);
    const Framebuffer frame = h.frame();
    // The first map at spawn in 80x24: the centred plaza with the Forge and Almonry buildings,
    // and the meadow's tall grass to the east.
    const std::vector<std::string> expected = {
        "    T....,',.''.`..,,.,.`.,'``AA^^AAn^n:n^n^nnA^n^'.''`...,..,`....`...`.,.T    ",
        "    Y...'....,`....`'...`.,`''An^A^A^^^:^nn^^nn^^n...,.'`,,'...,.,``.`'`,.'Y    ",
        "    T..'.'`...'.''.'.'Y.,...,.',.,'`,`,:`,``.,,`'..`.`,.Y'`.'`,..'.',.,,.`,Y    ",
        "    ^``..,`...Y'.`....'.`.'`','.`'..'...'.'.,'...`'....'...,`.,.'`.'`.'.''.Y    ",
        "    T.,...'`.,,`'..,',`..,,....'.`,.,.`..'`.,,..``.'.,.`.`,...;:;:v;Ylv:vv'Y    ",
        "    T..,.,.,,`'`..T..`,,`.``''.,`.`.```:.`,.,`.`,..`.,'..,..T.lii;il;vv;YY.T    ",
        "    Y.',.`'..,..`'...,,,`.`'...,.'.`.'.....'`'.'`..',.....`.'`;Y:;::l:::vY.Y    ",
        "    ^,.....`,,`.`...'..,.,',:::....::.:.:......:::.:...`,'`...Yil:vi:;ilvv'T    ",
        "    Y,,'`.`.,''',,`,....,`.,.##===#.:..:.:..:====##:.''.'..`'`:;;vv:l;l:YY'^    ",
        "    ^'......Y,...`.,.'.''..`:==####:....:....#=##==:'.`.,`',,.ivYilliv;lvY.T    ",
        "    Y...','.``..`'`'.',..,...####=#.:::::...:=====#...,'.,'...lv:;:l:vvvi;.T    ",
        "    T`,....,,..'`.`'.''...',.:......:...:...:..:.:...''..',``,:vi:;;vi.,.Y,Y    ",
        "    ^,'..',,'..'.'...'.'...'..:::::::::@....::::::::.:.::.....vl;l;il:.,:;`Y    ",
        "    T,''``.,..``'`..'`.,..``:::::.:......::.....:..:'`'...``,.;lv;:Yil.:,;`^    ",
        "    Y..''`,.,...`..''`.'.,.`:.:.:..:..:...::.:..:::..,.,...''.Y;l;iv;:Y:;:`^    ",
        "    Y.,....'.',,,.'',.`..```:###=#=::.::..::.=###=#:.'..'.,.`.;:YYYlvivliY.^    ",
        "    Y...,,'~~--~-~~...`...`,.#=====..:.....::#==#==.,.`'......Yv:lY:ll;;il`Y    ",
        "    ^,..,~=--~~-~=--=.''..,.:=#==##:..:..:...###=##:.....,',..;YvYil;ilYlv.T    ",
        "    Y',.`=~~=~~===~~=,`..',.::..::.::..:..::.:::.:.:.`.`.,```'vilYv;;;viYl'Y    ",
        "    ^.'.,~-=~~-~~~=-~'.`..'...,,'.`..,,.``''.'..`..,,..,..''.`vYv;vivY:vl:,Y    ",
        "    Y.``.~~~-~~~=~-~-...,''.,..,.'.',...`....'.........,`',...;:vl:vivlvlv.^    ",
        "    ^,`..~=-=~~~~==~=`.''''''..`.,,',`...`.`,``'.'.'..,``,''..;;:ii:Yvl:ii,^    ",
        "    ^.,.`~~~~==-~=-~~,`.'.,`..`'`...,,,,'.....,.`..,,,`......'`,,'..,,'`,.'T    ",
        "    Y'`.,`..,''`.,.',.,.^.....,'..`..``,.'...`.,T`....'..`.``''.`,''.'.,.,.T    ",
    };
    std::vector<std::string> rows;
    for (int y = 0; y < frame.height(); ++y)
        rows.push_back(rowText(frame, y));
    EXPECT_EQ(rows, expected);
}

namespace {

// A 400x200 meadow of grass and tall grass, so every view size scrolls.
game::world::WorldDef bigMeadow() {
    std::string map;
    for (int y = 0; y < 200; ++y) {
        for (int x = 0; x < 400; ++x)
            map += (x / 7 + y / 5) % 3 == 0 ? ',' : '.';
        map += '\n';
    }
    auto world = game::world::parseWorld(
        map,
        R"({"legend": {".": {"terrain": "grass", "walkable": true},
                       ",": {"terrain": "tall_grass", "walkable": true}},
            "spawn": {"x": 200, "y": 100}, "landmarks": [], "zones": []})",
        R"({"heroStepMs": 125})", game::shippedContent());
    EXPECT_TRUE(world.has_value());
    return std::move(*world);
}

// The largest diff the renderer writes for one step while the hero walks right far enough to
// scroll the view several times (the camera moves one column per tile at the dead-zone edge).
std::size_t worstScrollBytes(engine::Size view) {
    const game::world::WorldDef world = bigMeadow();
    Harness h("world_bytes");
    h.session.world = &world;
    h.startAtHub(freshGame());
    EXPECT_TRUE(h.press({kHubExplore}));
    engine::Renderer renderer(engine::ColorMode::TrueColor);
    std::string out;
    renderer.present(h.frame(view), out); // the first frame is a full redraw
    std::size_t worst = 0;
    for (int tile = 0; tile < view.width; ++tile) {
        EXPECT_TRUE(h.press({ch('d')}));
        out.clear();
        renderer.present(h.frame(view), out);
        worst = std::max(worst, out.size());
        EXPECT_TRUE(h.wait(0.12));
    }
    return worst;
}

} // namespace

// Task 3.3: recorded in docs/benchmarks.md. The design's budget is 300 KB per scroll at 200x60.
TEST(Overworld, ScrollingStaysInsideTheByteBudget) {
    const std::size_t large = worstScrollBytes({200, 60});
    const std::size_t medium = worstScrollBytes({120, 40});
    std::printf("overworld scroll bytes: 200x60 %zu, 120x40 %zu\n", large, medium);
    EXPECT_LE(large, 300u * 1024u);
    EXPECT_LT(medium, large);
}
