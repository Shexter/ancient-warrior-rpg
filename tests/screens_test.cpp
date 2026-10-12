#include "app/app.hpp"
#include "app/screens/hub.hpp"
#include "app/screens/title.hpp"
#include "game/save.hpp"

#include "frame_text.hpp"
#include "screen_harness.hpp"
#include "shipped_content.hpp"

#include <gtest/gtest.h>

#include <fstream>
#include <iterator>
#include <sstream>

using namespace heroes;
using namespace heroes::app;
using engine::Framebuffer;
using engine::Key;
using engine::KeyEvent;
namespace fs = std::filesystem;

namespace {

using namespace screen_test;

// Title -> Intro -> Create Hero name step.
void toNameEntry(Harness& h) {
    h.startAtTitle();
    ASSERT_TRUE(h.press({ch('1'), kEnter, kEnter}));
}

} // namespace

TEST(NewGame, CreatesTheHeroWithTheSeedAndReachesTheHub) {
    Harness h("new_game");
    toNameEntry(h);
    EXPECT_TRUE(frameContains(h.frame(), "Enter Hero name (max 8 characters):"));
    ASSERT_TRUE(h.type("  Aria "));
    ASSERT_TRUE(h.press({kEnter, kEnter, kEnter})); // name, then reveal and finish the story
    EXPECT_TRUE(frameContains(h.frame(), "WARRIOR PATHS"));
    EXPECT_TRUE(frameContains(h.frame(), "A knight of the")); // wrapped at 20 columns
    EXPECT_TRUE(frameContains(h.frame(), "the Strongest Fist."));
    ASSERT_TRUE(h.press({ch('2')})); // Hunter

    ASSERT_TRUE(h.session.state.has_value());
    EXPECT_EQ(*h.session.state, game::newGame(game::shippedContent(), "Aria", "hunter", 7));
    EXPECT_TRUE(h.session.dirty);
    const Framebuffer frame = h.frame();
    EXPECT_NE(rowText(frame, 1).find("ARIA  Hunter Lv 1"), std::string::npos);
    EXPECT_NE(rowText(frame, 7).find("▶ Training"), std::string::npos);
    EXPECT_TRUE(frameContains(frame, "Welcome, Aria the Hunter."));
}

TEST(NewGame, InvalidNameExplainsAndAsksAgain) {
    Harness h("bad_name");
    toNameEntry(h);
    ASSERT_TRUE(h.type("   "));
    ASSERT_TRUE(h.press({kEnter}));
    EXPECT_TRUE(frameContains(h.frame(), "Names are 1-8 plain characters."));
    EXPECT_TRUE(frameContains(h.frame(), "Enter Hero name"));
    EXPECT_FALSE(h.session.state.has_value());
}

TEST(NewGame, EscBacksOutToTheTitle) {
    Harness h("esc");
    toNameEntry(h);
    ASSERT_TRUE(h.press({kEsc}));
    EXPECT_EQ(h.app->depth(), 1u);
    EXPECT_TRUE(frameContains(h.frame(), "New Game"));
}

TEST(Hub, SaveThenContinueRestoresTheSameGame) {
    Harness h("save_continue");
    h.startAtHub(freshGame());
    h.session.dirty = true;
    ASSERT_TRUE(h.press({ch('4')}));
    EXPECT_TRUE(frameContains(h.frame(), "Game saved."));
    EXPECT_FALSE(h.session.dirty);
    EXPECT_TRUE(h.session.saveOwned);
    ASSERT_TRUE(h.press({kEnter})); // dismiss

    Harness again("save_continue_reload");
    again.session.saveFile = h.session.saveFile;
    again.startAtTitle();
    ASSERT_TRUE(again.press({ch('2')})); // Continue is enabled now
    ASSERT_TRUE(again.session.state.has_value());
    EXPECT_EQ(*again.session.state, *h.session.state);
    EXPECT_TRUE(again.session.saveOwned);
    EXPECT_TRUE(frameContains(again.frame(), "Welcome back, Aria."));
}

TEST(Hub, SavingOverAFileThisSessionDidNotLoadAsksFirst) {
    Harness h("replace");
    std::ofstream(h.session.saveFile) << "someone else's save";
    h.startAtHub(freshGame());
    ASSERT_TRUE(h.press({ch('4')}));
    EXPECT_TRUE(frameContains(h.frame(), "Replace the existing save?"));
    ASSERT_TRUE(h.press({ch('n')}));
    EXPECT_EQ(readFile(h.session.saveFile), "someone else's save");

    ASSERT_TRUE(h.press({ch('4'), ch('y')}));
    EXPECT_TRUE(game::loadGame(h.session.saveFile, game::shippedContent()).has_value());
    ASSERT_TRUE(h.press({kEnter, ch('4')})); // owned now: no question the second time
    EXPECT_FALSE(frameContains(h.frame(), "Replace the existing save?"));
}

TEST(Hub, QuitAsksOnlyWithUnsavedChanges) {
    Harness h("quit");
    h.startAtHub(freshGame());
    h.session.dirty = true;
    ASSERT_TRUE(h.press({ch('5')}));
    EXPECT_TRUE(frameContains(h.frame(), "Quit without saving?"));
    ASSERT_TRUE(h.press({ch('n'), kEsc})); // No keeps playing; Esc asks again
    EXPECT_TRUE(frameContains(h.frame(), "Quit without saving?"));
    EXPECT_FALSE(h.press({ch('y')}));

    Harness saved("quit_saved");
    saved.startAtHub(freshGame());
    EXPECT_FALSE(saved.press({ch('5')})); // nothing unsaved: quits at once
}

TEST(Hub, AdventureOptionsExplainWhyTheyAreDisabled) {
    Harness h("disabled");
    game::GameState state = freshGame();
    h.startAtHub(state);
    ASSERT_TRUE(h.press({}));
    EXPECT_NE(rowText(h.frame(), 8).find("Requires Lv 15"), std::string::npos);

    h.session.state->hero.hp = 0;
    ASSERT_TRUE(h.press({ch('x')})); // any step refreshes the menu
    EXPECT_NE(rowText(h.frame(), 7).find("Fallen: visit the Prior"), std::string::npos);

    h.session.state->hero.hp = 100;
    h.session.state->hero.weapon.reset();
    ASSERT_TRUE(h.press({ch('x')}));
    EXPECT_NE(rowText(h.frame(), 7).find("Equip a weapon and armor"), std::string::npos);
    EXPECT_EQ(h.frame().at(36, 7).fg, palette::kDim);
}

TEST(Title, DamagedSaveShowsWhyAndIsLeftUntouched) {
    Harness h("corrupt");
    std::ofstream(h.session.saveFile) << "{not json";
    h.startAtTitle();
    ASSERT_TRUE(h.press({ch('2')}));
    EXPECT_TRUE(frameContains(h.frame(), "The save file is damaged."));
    EXPECT_FALSE(h.session.state.has_value());
    EXPECT_EQ(readFile(h.session.saveFile), "{not json");
    ASSERT_TRUE(h.press({kEnter, ch('1')})); // dismissed; New Game still works
    EXPECT_EQ(h.app->depth(), 2u);
}

TEST(Sprites, MissingSpriteIsReportedByName) {
    const auto bank = loadSprites(game::shippedContent(), "/no/such/dir");
    ASSERT_FALSE(bank.has_value());
    EXPECT_NE(bank.error().find("monster.sprite"), std::string::npos);
    EXPECT_EQ(shippedSprites().size(), 6u);
}

TEST(Title, SaveWithMissingModsAsksBeforeLoading) {
    Harness h("missing_mods");
    ASSERT_TRUE(game::saveGame(freshGame(), h.session.saveFile,
                               std::vector<game::ModRef>{{"cyclops-pack", "1.0.0"}})
                    .has_value());
    const std::string before = readFile(h.session.saveFile);
    h.startAtTitle();
    ASSERT_TRUE(h.press({ch('2')}));
    EXPECT_TRUE(frameContains(h.frame(), "This save used mods that are missing: cyclops-pack"));
    ASSERT_TRUE(h.press({ch('n')})); // No: nothing loaded, file untouched, still the Title
    EXPECT_FALSE(h.session.state.has_value());
    EXPECT_EQ(readFile(h.session.saveFile), before);
    EXPECT_EQ(h.app->depth(), 1u);

    ASSERT_TRUE(h.press({ch('2'), ch('y')})); // Yes: everything it uses exists, so it loads
    EXPECT_TRUE(h.session.state.has_value());
    EXPECT_TRUE(frameContains(h.frame(), "Welcome back, Aria."));
}

TEST(Title, ASaveThatNeedsTheMissingModNamesIt) {
    Harness h("needs_mod");
    game::GameState state = freshGame();
    state.hero.completedMissions.insert("the_gorgon");
    ASSERT_TRUE(game::saveGame(state, h.session.saveFile,
                               std::vector<game::ModRef>{{"cyclops-pack", "1.0.0"}})
                    .has_value());
    // Hand the save a mission only the missing mod defines.
    std::string body = readFile(h.session.saveFile);
    body.replace(body.find("\"the_gorgon\""), 12, "\"cyclops_cave\"");
    std::ofstream(h.session.saveFile, std::ios::binary) << body;
    h.startAtTitle();
    ASSERT_TRUE(h.press({ch('2'), ch('y')}));
    EXPECT_TRUE(frameContains(h.frame(), "The save can't be loaded without these mods:"));
    EXPECT_TRUE(frameContains(h.frame(), "cyclops-pack 1.0.0."));
    EXPECT_FALSE(h.session.state.has_value());
    EXPECT_EQ(readFile(h.session.saveFile), body);
}

TEST(Taps, TappingAHubRowOpensIt) {
    Harness h("tap_hub");
    h.startAtHub(freshGame());
    (void)h.frame(); // the menu is tappable where it was drawn
    ASSERT_TRUE(h.press({{Key::Click, 0, 40, 9}})); // the Town row
    EXPECT_TRUE(frameContains(h.frame(), "CIVITAS LUCIS"));
}

TEST(Taps, QuitIsConfirmedByTappingYes) {
    Harness h("tap_quit");
    h.startAtHub(freshGame());
    h.session.dirty = true;
    (void)h.frame();
    ASSERT_TRUE(h.press({{Key::Click, 0, 36, 11}})); // Quit
    const Framebuffer frame = h.frame();
    ASSERT_TRUE(frameContains(frame, "Quit without saving?"));
    int fx = -1, fy = -1;
    for (int y = 0; y < frame.height() && fx < 0; ++y)
        for (int x = 0; x + 1 < frame.width(); ++x)
            if (frame.at(x, y).glyph == U'[' && frame.at(x + 1, y).glyph == U'N') {
                fx = x;
                fy = y;
                break;
            }
    ASSERT_GE(fx, 0);
    ASSERT_TRUE(h.press({{Key::Click, 0, fx + 1, fy}})); // [N]o keeps playing
    (void)h.frame();
    ASSERT_TRUE(h.press({{Key::Click, 0, 36, 11}}));
    (void)h.frame(); // the loop draws every frame; the harness draws on request
    EXPECT_FALSE(h.press({{Key::Click, 0, fx - 6, fy}})); // [Y]es quits
}
