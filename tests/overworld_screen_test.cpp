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

// The shipped world without its monster zones, for tests about walking and landmarks.
const game::world::WorldDef& quietWorld() {
    static const game::world::WorldDef world = [] {
        game::world::WorldDef w = shippedWorld();
        w.zones.clear();
        return w;
    }();
    return world;
}

void enterWorld(Harness& h, game::GameState state = freshGame()) {
    h.session.world = &quietWorld();
    h.startAtHub(std::move(state));
    ASSERT_TRUE(h.press({kHubExplore}));
    ASSERT_EQ(h.app->depth(), 2u);
}

constexpr int kHudRows = 2; // the status and goal rows over the top of an 80x24 view

bool isTreeGlyph(char32_t c) { return c == U'Y' || c == U'T' || c == U'^'; }

} // namespace

TEST(Overworld, ExploreOpensTheWorldAtSpawn) {
    Harness h("world_enter");
    enterWorld(h);
    const Framebuffer frame = h.frame();
    // A 72-wide map in an 80-wide view is centred (4 columns of margin). The camera starts with
    // spawn (35, 14) in the middle: map rows 2-25.
    EXPECT_EQ(heroAt(frame), (std::pair{4 + 35, 12}));
    EXPECT_EQ(frame.at(0, 5).bg, palette::kBg); // the margin, below the two HUD rows
    EXPECT_EQ(frame.at(0, 5).glyph, U' ');
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
    EXPECT_EQ(heroAt(frame), (std::pair{4 + 35, 2})); // the view's top row is map row 0
    for (int y = 2; y < frame.height() - 1; ++y) { // between the HUD rows and the hotbar
        EXPECT_TRUE(isTreeGlyph(frame.at(4, y).glyph)) << y;
        EXPECT_EQ(frame.at(3, y).glyph, U' ') << y;
    }
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
    for (int y = kHudRows; y < before.height() - 1; ++y) { // not the hotbar row
        const int y2 = y + ay - by;
        if (y2 < kHudRows || y2 >= after.height() - 1)
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
    // The first map at spawn in 80x24: the status and goal rows, the centred plaza with its four
    // service letters at the building doors, the meadow's tall grass with the lair's ! east, and
// the hotbar.
    const std::vector<std::string> expected = {
        " ARIA Lv 1  HP ██████████ 350/350  FV ██████ 50/50  $20000  !x0  Sword          ",
        " » Next: train to Lv 15 for Act I: \"Avaritia\"                                   ",
        "    T..'.'`...'.''.'.'Y.,...,.',.,'`,`,:`,``.,,`'..`.`,.Y'`.'`,..'.',.,,.`,Y    ",
        "    ^``..,`...Y'.`....'.`.'`','.`'..'...'.'.,'...`'....'...,`.,.'`.'`.'.''.Y    ",
        "    T.,...'`.,,`'..,',`..,,....'.`,.,.`..'`.,,..``.'.,.`.`,...;:;:v;Ylv:vv'Y    ",
        "    T..,.,.,,`'`..T..`,,`.``''.,`.`.```:.`,.,`.`,..`.,'..,..T.lii;il;vv;YY.T    ",
        "    Y.',.`'..,..`'...,,,`.`'...,.'.`.'.....'`'.'`..',.....`.'`;Y:;::l:::vY.Y    ",
        "    ^,.....`,,`.`...'..,.,',:::....::.:.:......:::.:...`,'`...Yil:vi:;ilvv'T    ",
        "    Y,,'`.`.,''',,`,....,`.,.##===#.:..:.:..:====##:.''.'..`'`:;;vv:l;l:YY'^    ",
        "    ^'......Y,...`.,.'.''..`:==####:....:....#=##==:'.`.,`',,.ivYilliv;lvY.T    ",
        "    Y...','.``..`'`'.',..,...####=#.:::::...:=====#...,'.,'...lv:;:l:vvvi;.T    ",
        "    T`,....,,..'`.`'.''...',.:..A...:...:...:..:F:...''..',``,:vi:;;vi.,.Y,Y    ",
        "    ^,'..',,'..'.'...'.'...'..:::::::::@....::::::::.:.::.....vl;l;il:.!:;`Y    ",
        "    T,''``.,..``'`..'`.,..``:::::.:......::.....:..:'`'...``,.;lv;:Yil.:,;`^    ",
        "    Y..''`,.,...`..''`.'.,.`:.:.P..:..:...::.:..T::..,.,...''.Y;l;iv;:Y:;:`^    ",
        "    Y.,....'.',,,.'',.`..```:###=#=::.::..::.=###=#:.'..'.,.`.;:YYYlvivliY.^    ",
        "    Y...,,'~~--~-~~...`...`,.#=====..:.....::#==#==.,.`'......Yv:lY:ll;;il`Y    ",
        "    ^,..,~=--~~-~=--=.''..,.:=#==##:..:..:...###=##:.....,',..;YvYil;ilYlv.T    ",
        "    Y',.`=~~=~~===~~=,`..',.::..::.::..:..::.:::.:.:.`.`.,```'vilYv;;;viYl'Y    ",
        "    ^.'.,~-=~~-~~~=-~'.`..'...,,'.`..,,.``''.'..`..,,..,..''.`vYv;vivY:vl:,Y    ",
        "    Y.``.~~~-~~~=~-~-...,''.,..,.'.',...`....'.........,`',...;:vl:vivlvlv.^    ",
        "    ^,`..~=-=~~~~==~=`.''''''..`.,,',`...`.`,``'.'.'..,``,''..;;:ii:Yvl:ii,^    ",
        "    ^.,.`~~~~==-~=-~~,`.'.,`..`'`...,,,,'.....,.`..,,,`......'`,,'..,,'`,.'T    ",
        "  SPC  Swing    1  Firebolt 15    2  Nova 30    Q  Potion x0                    ",
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
        readFile(fs::path(HEROES_SOURCE_DIR) / "data" / "world" / "field.json"),
        game::shippedContent());
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

namespace {

// Walks `tiles` tiles, one press each (a plain terminal), drawing every step like the loop does.
void walk(Harness& h, char key, int tiles, engine::Size view = engine::kCompactCanvas) {
    for (int i = 0; i < tiles; ++i) {
        ASSERT_TRUE(h.press({ch(key)}));
        ASSERT_TRUE(h.wait(0.15));
        (void)h.frame(view);
    }
}

// From spawn (35, 14) east along row 14 to (66, 14), next to the Lair of Avaritia at (67, 14).
void walkToLair(Harness& h) { walk(h, 'd', 31); }

game::GameState levelFifteen() {
    game::GameState state = freshGame();
    state.hero.level = 15;
    return state;
}

bool hasPanelAt(const Framebuffer& frame, int x, int y) { return frame.at(x, y).glyph == U'┌'; }

} // namespace

TEST(Overworld, VisitingTheForgeAndComingBack) {
    Harness h("world_forge");
    enterWorld(h);
    walk(h, 'd', 9); // (44, 14), below the Forge at (44, 13)
    const Framebuffer outside = h.frame();
    EXPECT_NE(rowText(outside, 1).find("» Forge — press E"), std::string::npos);
    ASSERT_TRUE(h.press({ch('e')}));
    EXPECT_EQ(h.app->depth(), 3u);
    EXPECT_TRUE(frameContains(h.frame(), "FORGE OF THE ORDER"));
    ASSERT_TRUE(h.press({kEsc}));
    EXPECT_EQ(h.app->depth(), 2u);
    EXPECT_EQ(heroAt(h.frame()), heroAt(outside));
}

TEST(Overworld, TheCatacombsOpenTraining) {
    Harness h("world_catacombs");
    enterWorld(h);
    walk(h, 'w', 11); // up the path to (35, 3), below the cave mouth
    EXPECT_NE(rowText(h.frame(), 1).find("» Catacombs — press E"), std::string::npos);
    ASSERT_TRUE(h.press({ch('E')}));
    EXPECT_TRUE(frameContains(h.frame(), "THE CATACOMBS"));
}

TEST(Overworld, ALockedLairOnlyRepeatsItsNote) {
    Harness h("world_locked");
    game::GameState state = freshGame();
    state.hero.level = 12;
    enterWorld(h, state);
    walkToLair(h);
    EXPECT_NE(rowText(h.frame(), 1).find("» Lair of Avaritia — Requires Lv 15"),
              std::string::npos);
    ASSERT_TRUE(h.press({ch('e')}));
    EXPECT_EQ(h.app->depth(), 2u); // no battle
    EXPECT_NE(rowText(h.frame(), 1).find("Requires Lv 15"), std::string::npos);
}

TEST(Overworld, AClearedLairSaysSo) {
    Harness h("world_cleared");
    game::GameState state = levelFifteen();
    state.hero.completedMissions.insert("avaritia");
    enterWorld(h, state);
    walkToLair(h);
    EXPECT_NE(rowText(h.frame(), 1).find("» Lair of Avaritia — Cleared"), std::string::npos);
    ASSERT_TRUE(h.press({ch('e')}));
    EXPECT_EQ(h.app->depth(), 2u);
}

TEST(Overworld, BackingOutOfALairReturnsToTheWorld) {
    Harness h("world_lair_back");
    enterWorld(h, levelFifteen());
    walkToLair(h);
    const auto at = heroAt(h.frame());
    ASSERT_TRUE(h.press({ch('e')}));
    EXPECT_EQ(h.app->depth(), 3u);
    ASSERT_TRUE(h.press({kEsc})); // not to Mission Select
    EXPECT_EQ(h.app->depth(), 2u);
    EXPECT_EQ(heroAt(h.frame()), at);
}

TEST(Overworld, AWonMissionsResultReturnsToTheWorld) {
    Harness h("world_lair_win");
    game::GameState state = levelFifteen();
    state.hero.atk = 100000; // ends the fight in the hero's favor
    enterWorld(h, state);
    walkToLair(h);
    ASSERT_TRUE(h.press({ch('e'), kEnter, kEnter, ch('y')}));
    for (int i = 0; i < 2000 && !frameContains(h.frame(), "Enter return to the Hub"); ++i)
        ASSERT_TRUE(h.press({ch('1')})); // until the Result screen
    ASSERT_TRUE(frameContains(h.frame(), "VICTORY"));
    ASSERT_TRUE(h.press({kEnter}));
    EXPECT_EQ(h.app->depth(), 2u);
    ASSERT_TRUE(h.wait(1.0 / 60));
    EXPECT_EQ(h.app->depth(), 2u); // still in the world
    EXPECT_NE(rowText(h.frame(), 1).find("Lair of Avaritia — Cleared"), std::string::npos);
}

TEST(Overworld, ALostMissionReturnsToTheHub) {
    Harness h("world_lair_loss");
    game::GameState state = levelFifteen();
    state.hero.hp = 1;
    enterWorld(h, state);
    walkToLair(h);
    ASSERT_TRUE(h.press({ch('e'), kEnter, kEnter, ch('y')}));
    for (int i = 0; i < 2000 && !frameContains(h.frame(), "All equipment destroyed!"); ++i)
        ASSERT_TRUE(h.press({ch('1')})); // the Result screen's line, not the battle log's
    ASSERT_TRUE(frameContains(h.frame(), "All equipment destroyed!"));
    ASSERT_TRUE(h.press({kEnter}));
    ASSERT_TRUE(h.wait(1.0 / 60)); // the Overworld sees the hero can't adventure and closes
    EXPECT_EQ(h.app->depth(), 1u);
    EXPECT_TRUE(frameContains(h.frame(), "Defeated. All equipment destroyed."));
}

TEST(Overworld, MTogglesTheMinimapWhichIsOnByDefaultOnlyWhenWide) {
    Harness h("world_minimap");
    enterWorld(h);
    const engine::Size big{150, 45};
    EXPECT_TRUE(hasPanelAt(h.frame(big), 1, 45 - 16 - 1));
    ASSERT_TRUE(h.press({ch('m')}));
    EXPECT_FALSE(hasPanelAt(h.frame(big), 1, 45 - 16 - 1));
    ASSERT_TRUE(h.press({ch('M')}));
    EXPECT_TRUE(hasPanelAt(h.frame(big), 1, 45 - 16 - 1));

    Harness small("world_minimap_small");
    enterWorld(small);
    EXPECT_FALSE(hasPanelAt(small.frame(), 1, 24 - 12 - 1));
    (void)small.frame();
    ASSERT_TRUE(small.press({ch('m')}));
    EXPECT_TRUE(hasPanelAt(small.frame(), 1, 24 - 12 - 1));
}

TEST(Overworld, GoldenHudAt150x45) {
    Harness h("world_golden_large");
    enterWorld(h);
    const Framebuffer frame = h.frame({150, 45});
    std::vector<std::string> rows; // the status and goal panels, then the minimap
    for (int y = 1; y <= 11; ++y)
        rows.push_back(cellText(frame, 0, y, 44));
    for (int y = 28; y <= 43; ++y)
        rows.push_back(cellText(frame, 0, y, 36));
    const std::vector<std::string> expected = {
        " ┌──────────────────────────────────────┐   ",
        " │ ARIA Lv 1                            │   ",
        " │ HP  ████████████████████ 350/350     │   ",
        " │ FV  ████████████████████ 50/50       │   ",
        " │ XP  ░░░░░░░░░░░░░░░░░░░░ 0/100       │   ",
        " │ $20000  !x0  Sword                   │   ",
        " └──────────────────────────────────────┘TT^",
        "                                       T...`",
        " ┌──────────────────────────────────────────",
        " │ » Next: train to Lv 15 for Act I: \"Avarit",
        " └──────────────────────────────────────────",
        " ┌────────────────────────────────┐ ",
        " │###############C################│ ",
        " │#...........#########...........│ ",
        " │#.......#.......................│ ",
        " │#...............................│ ",
        " │#...............................│ ",
        " │#...........##.....##...........│ ",
        " │#...........A#.@...F#........!..│ ",
        " │#...........P......T............│ ",
        " │#...........##.....##...........│ ",
        " │#..~~~~~....##.....##...........│ ",
        " │#..~~~~~........................│ ",
        " │#..~~~~~........................│ ",
        " │#........#......................│ ",
        " │#...............................│ ",
        " └────────────────────────────────┘ ",
    };
    EXPECT_EQ(rows, expected);
}

namespace {

// A 12x12 meadow around spawn (1, 1) with one level 3 monster (level 1 can't get past a starting
// hero's armor) that notices from anywhere.
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

} // namespace

TEST(Overworld, MonstersAreDrawnAndFallingReturnsToTheHub) {
    const game::world::WorldDef world = huntingGround();
    Harness h("world_fall");
    h.session.world = &world;
    game::GameState state = freshGame();
    state.hero.hp = 1;
    h.startAtHub(state);
    ASSERT_TRUE(h.press({kHubExplore}));
    EXPECT_TRUE(h.session.dirty);
    EXPECT_TRUE(frameContains(h.frame(), "f")); // a Fox, drawn as its lower-case initial
    for (int i = 0; i < 60 * 30 && h.app->depth() == 2; ++i)
        ASSERT_TRUE(h.wait(1.0 / 60));
    ASSERT_EQ(h.app->depth(), 1u);
    EXPECT_EQ(h.session.state->hero.hp, 0);
    ASSERT_TRUE(h.wait(1.0 / 60)); // the Hub refreshes its menu on its next step
    const Framebuffer hub = h.frame();
    EXPECT_TRUE(frameContains(hub, "You fell in the field. Visit the Prior to be revived."));
    EXPECT_NE(rowText(hub, 7).find("Fallen: visit the Prior"), std::string::npos); // Explore
    EXPECT_NE(rowText(hub, 8).find("Fallen: visit the Prior"), std::string::npos); // Training
}

namespace {

std::string hotbar(const Framebuffer& frame) { return rowText(frame, frame.height() - 1); }

// Where `text` starts on the bottom row.
int hotbarAt(const Framebuffer& frame, const std::string& text) {
    const std::string row = hotbar(frame);
    const auto at = row.find(text);
    return at == std::string::npos ? -1 : textWidth(row.substr(0, at));
}

} // namespace

TEST(Overworld, TheHotbarShowsCostsAndCountsDownCooldowns) {
    Harness h("world_hotbar");
    enterWorld(h);
    EXPECT_NE(hotbar(h.frame()).find("SPC  Swing    1  Firebolt 15    2  Nova 30    Q  Potion x0"),
              std::string::npos);
    ASSERT_TRUE(h.press({ch('2')}));
    const Framebuffer cooling = h.frame();
    EXPECT_NE(hotbar(cooling).find("Nova 30 4.0s"), std::string::npos) << hotbar(cooling);
    EXPECT_EQ(cooling.at(hotbarAt(cooling, "Nova"), cooling.height() - 1).fg, palette::kDim);
    ASSERT_TRUE(h.wait(1));
    EXPECT_NE(hotbar(h.frame()).find("Nova 30 3.0s"), std::string::npos);
    ASSERT_TRUE(h.wait(3.1));
    EXPECT_NE(hotbar(h.frame()).find("Nova 30    Q"), std::string::npos);
}

TEST(Overworld, ARefusedAbilityFlashesItsSlot) {
    Harness h("world_flash");
    game::GameState state = freshGame();
    state.hero.mp = 0;
    enterWorld(h, state);
    ASSERT_TRUE(h.press({ch('1')}));
    const Framebuffer frame = h.frame();
    EXPECT_EQ(frame.at(hotbarAt(frame, "Firebolt"), frame.height() - 1).bg, palette::kDmg);
    ASSERT_TRUE(h.press({ch('q')}));
    EXPECT_NE(rowText(h.frame(), 1).find("» No potions"), std::string::npos);
    ASSERT_TRUE(h.wait(0.5));
    const Framebuffer later = h.frame();
    EXPECT_EQ(later.at(hotbarAt(later, "Firebolt"), later.height() - 1).bg, palette::kPanel);
}

TEST(Overworld, AKillIsAnnouncedWithItsRewards) {
    const game::world::WorldDef world = huntingGround();
    Harness h("world_kill");
    h.session.world = &world;
    game::GameState state = freshGame();
    state.hero.atk = 100000;
    h.startAtHub(state);
    ASSERT_TRUE(h.press({kHubExplore}));
    for (int i = 0; i < 60 * 30 && !frameContains(h.frame(), "defeated"); ++i) {
        ASSERT_TRUE(h.press({ch('2')})); // Nova whenever it's ready, until the Fox comes close
        h.session.state->hero.mp = h.session.state->hero.maxMp;
    }
    const std::string line = rowText(h.frame(), 1);
    EXPECT_NE(line.find("» Fox defeated · +"), std::string::npos) << line;
    // A level 3 Fox's 300 EXP takes a fresh hero up two levels.
    EXPECT_NE(line.find("Level up! +2 blessings at the Prior"), std::string::npos) << line;
}

namespace {

// Steps until a monster's wind-up targets the hero: the hero's cell turns dark red.
bool waitForWindUp(Harness& h) {
    for (int i = 0; i < 60 * 20; ++i) {
        const Framebuffer f = h.frame();
        const auto [x, y] = heroAt(f);
        if (x >= 0 && f.at(x, y).bg == (engine::Rgb{96, 24, 36}))
            return true;
        if (!h.wait(1.0 / 60))
            return false;
    }
    return false;
}

} // namespace

TEST(Overworld, GoldenWindUpAndNovaFrames) {
    const game::world::WorldDef world = huntingGround();
    Harness h("world_golden_fight");
    h.session.world = &world;
    game::GameState state = freshGame();
    state.hero.maxHp = state.hero.hp = 100000; // survives as long as the test needs
    h.startAtHub(state);
    ASSERT_TRUE(h.press({kHubExplore}));
    ASSERT_TRUE(waitForWindUp(h));
    const Framebuffer windUp = h.frame();
    std::vector<std::string> windUpRows;
    for (int y = 4; y <= 19; ++y) // the 12x12 meadow, centred
        windUpRows.push_back(cellText(windUp, 32, y, 16));
    const std::vector<std::string> expectedWindUp = {
        "                ",
        "                ",
        "  .,,,.`,,.',`  ",
        "  ,@f.`',,`...  ",
        "  .....,',.''.  ",
        "  `...'....,`.  ",
        "  ,..'.'`...'.  ",
        "  .``..,`...`'  ",
        "  ..,...'`.,,`  ",
        "  ,..,.,.,,`'`  ",
        "  `.',.`'..,..  ",
        "  .,.....`,,`.  ",
        "  `,,'`.`.,'''  ",
        "  ''.......,..  ",
        "                ",
        "                ",
    };
    EXPECT_EQ(windUpRows, expectedWindUp); // the Fox (f) beside the hero, mid wind-up
    const auto [hx, hy] = heroAt(windUp);
    EXPECT_EQ(windUp.at(hx, hy).bg, (engine::Rgb{96, 24, 36})); // the targeted tile

    ASSERT_TRUE(h.press({ch('2')}));
    const Framebuffer nova = h.frame();
    std::vector<std::string> novaRows;
    for (int y = 4; y <= 19; ++y)
        novaRows.push_back(cellText(nova, 32, y, 16));
    const std::vector<std::string> expectedNova = {
        "*******         ",
        "*     *         ",
        "* .93,*`,,.',`  ",
        "* ,@f.*',,`...  ",
        "* ....*,',.''.  ",
        "* `...*....,`.  ",
        "*******'`...'.  ",
        "  .``..,`...`'  ",
        "  ..,...'`.,,`  ",
        "  ,..,.,.,,`'`  ",
        "  `.',.`'..,..  ",
        "  .,.....`,,`.  ",
        "  `,,'`.`.,'''  ",
        "  ''.......,..  ",
        "                ",
        "                ",
    };
    EXPECT_EQ(novaRows, expectedNova); // the radius 3 ring, and Nova's damage over the Fox
}
