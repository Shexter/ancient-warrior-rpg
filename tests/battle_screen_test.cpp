#include "app/battle_view.hpp"
#include "app/screens/mission.hpp"
#include "game/town.hpp"

#include "frame_text.hpp"
#include "screen_harness.hpp"

#include <gtest/gtest.h>

#include <cctype>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <vector>

using namespace screen_test;
namespace ev = game::ev;
using game::Side;

namespace {

constexpr double kStep = 1.0 / 60;

game::GameState levelTwenty() {
    game::GameState state = freshGame();
    state.hero.level = 20;
    return state;
}

struct ViewFixture {
    ViewFixture()
        : hero(game::heroCombatant(freshGame().hero, game::shippedContent())),
          enemy(game::enemyCombatant(game::shippedContent().enemy("medusa"))),
          view(game::shippedContent(), hero, enemy) {}

    void play(std::vector<game::Event> events) { view.push(std::move(events), hero, enemy); }
    Framebuffer frame() const {
        Framebuffer f(80, 24);
        view.draw(f, Art{&shippedSprites().at("hero_fighter.sprite"), nullptr},
                  Art{&shippedSprites().at("medusa.sprite"), nullptr});
        return f;
    }

    game::Combatant hero, enemy;
    BattleView view;
};

// Hub -> Mission -> a mission's intro -> "Enter the lair?" -> Yes. Missions: '1' Avaritia,
// '2' Gula, '3' Ira, '4' the Medusa bonus.
void enterBattle(Harness& h, game::GameState state, char mission = '4',
                 std::string_view location = "MEDUSA LAIR") {
    h.startAtHub(std::move(state));
    ASSERT_TRUE(h.press({kHubMission, ch(mission)}));
    ASSERT_TRUE(frameContains(h.frame(), std::string(location)));
    ASSERT_TRUE(h.press({kEnter, kEnter})); // reveal the story, then finish it
    ASSERT_TRUE(frameContains(h.frame(), "Enter the lair?"));
    ASSERT_TRUE(h.press({ch('y')}));
}

// Plays the battle to its end, choosing `action` on every hero turn.
bool fightUntilResult(Harness& h, KeyEvent action) {
    for (int i = 0; i < 100000 && !frameContains(h.frame(), "Enter return to the Hub"); ++i)
        if (!h.press({action}))
            return false;
    return frameContains(h.frame(), "Enter return to the Hub");
}

} // namespace

TEST(BattleView, AttackedLasts18Steps) {
    ViewFixture f;
    f.play({ev::Attacked{Side::Hero}});
    EXPECT_NE(rowText(f.frame(), 18).find("ARIA attacks!"), std::string::npos);
    for (int step = 1; step < 18; ++step) {
        f.view.tick(kStep);
        ASSERT_TRUE(f.view.busy()) << "step " << step;
    }
    f.view.tick(kStep);
    EXPECT_FALSE(f.view.busy());
}

TEST(BattleView, ZeroLengthEventsDoNotHoldTheQueue) {
    ViewFixture f;
    f.play({ev::TurnStarted{Side::Hero, 3}});
    EXPECT_FALSE(f.view.busy());
    EXPECT_EQ(f.view.turn(), 3);
}

TEST(BattleView, DamageFloatsANumberAndLogsByCause) {
    ViewFixture f;
    f.play({ev::Damaged{Side::Enemy, 380, 2620, game::DamageCause::Attack},
            ev::Damaged{Side::Enemy, 50, 2570, game::DamageCause::Reflect},
            ev::Damaged{Side::Hero, 0, f.hero.hp, game::DamageCause::Attack}});
    EXPECT_TRUE(frameContains(f.frame(), "-380"));
    EXPECT_TRUE(frameContains(f.frame(), "MEDUSA takes 380 damage."));
    EXPECT_TRUE(frameContains(f.frame(), "2620/3000"));
    f.view.skip();
    EXPECT_TRUE(frameContains(f.frame(), "50 damage reflected to MEDUSA."));
    f.view.skip();
    EXPECT_TRUE(frameContains(f.frame(), "BLOCK"));
    EXPECT_TRUE(frameContains(f.frame(), "ARIA's armor absorbs the blow."));
}

TEST(BattleView, SkillCastShakesTheFrameEvery50Ms) {
    ViewFixture f;
    f.play({ev::SkillCast{"angel_wings", 10}});
    EXPECT_TRUE(frameContains(f.frame(), "Guardian Angel"));
    EXPECT_EQ(f.view.shake(), 1);
    for (int i = 0; i < 3; ++i)
        f.view.tick(kStep);
    EXPECT_EQ(f.view.shake(), -1);
    for (int i = 0; i < 3; ++i)
        f.view.tick(kStep);
    EXPECT_EQ(f.view.shake(), 1);
}

TEST(BattleView, StatusTagsFollowAppliedAndExpired) {
    ViewFixture f;
    f.play({ev::StatusApplied{Side::Enemy, game::StatusId::DefenseDown, 3}});
    EXPECT_TRUE(frameContains(f.frame(), "[DEF v 3]"));
    EXPECT_TRUE(frameContains(f.frame(), "Defense lowered for 3 turns."));
    f.view.skip();
    f.play({ev::StatusExpired{Side::Enemy, game::StatusId::DefenseDown}});
    EXPECT_FALSE(frameContains(f.frame(), "[DEF v"));
    EXPECT_TRUE(frameContains(f.frame(), "MEDUSA's defense recovers."));
}

TEST(BattleView, EndsOnlyAfterTheDefeatHasPlayed) {
    ViewFixture f;
    f.play({ev::Defeated{Side::Enemy}, ev::BattleEnded{game::Outcome::Victory, 4}});
    EXPECT_FALSE(f.view.ended().has_value());
    EXPECT_TRUE(frameContains(f.frame(), "MEDUSA has been defeated!"));
    for (int i = 0; i < 48; ++i) // 800 ms
        f.view.tick(kStep);
    EXPECT_EQ(f.view.ended(), game::Outcome::Victory);
    EXPECT_FALSE(f.view.busy());
}

TEST(MissionSelect, ListsComingSoonAndDisablesClearedMissions) {
    Harness h("mission_select");
    game::GameState state = levelTwenty();
    state.hero.completedMissions.insert("the_gorgon");
    h.startAtHub(state);
    ASSERT_TRUE(h.press({kHubMission}));
    const Framebuffer frame = h.frame();
    EXPECT_NE(rowText(frame, 7).find("Act I: \"Avaritia\""), std::string::npos);
    EXPECT_NE(rowText(frame, 8).find("Act I: \"Gula\""), std::string::npos);
    EXPECT_NE(rowText(frame, 9).find("Act I: \"Ira\""), std::string::npos);
    EXPECT_NE(rowText(frame, 10).find("Bonus: \"The Gorgon\""), std::string::npos);
    EXPECT_NE(rowText(frame, 10).find("Cleared"), std::string::npos);
    EXPECT_NE(rowText(frame, 11).find("Coming Soon"), std::string::npos);
    EXPECT_NE(rowText(frame, 12).find("Coming Soon"), std::string::npos);
    ASSERT_TRUE(h.press({ch('4'), kEsc})); // the cleared bonus can't be chosen
    EXPECT_EQ(h.app->depth(), 1u);
}

TEST(Battle, FleeingPlaysOutAndAppliesTheSameResult) {
    Harness h("battle_flee");
    game::GameState expected = levelTwenty();
    {
        auto battle = game::startMission(expected, game::shippedContent(), "the_gorgon",
                                         &game::shippedScripts())
                          .value();
        (void)battle.start();
        (void)battle.act(game::Flee{});
        (void)game::finishMission(expected, game::shippedContent(), "the_gorgon", battle);
    }

    enterBattle(h, levelTwenty());
    EXPECT_TRUE(frameContains(h.frame(), "Turn 1"));
    EXPECT_TRUE(h.session.dirty);
    ASSERT_TRUE(h.press({ch('5')}));
    EXPECT_EQ(*h.session.state, expected); // applied before the animation finishes
    EXPECT_TRUE(frameContains(h.frame(), "ARIA flees the battle."));
    ASSERT_TRUE(h.wait(0.5));
    EXPECT_TRUE(frameContains(h.frame(), "ESCAPED"));
    EXPECT_TRUE(frameContains(h.frame(), "You escaped."));
    ASSERT_TRUE(h.press({kEnter}));
    EXPECT_EQ(h.app->depth(), 1u);
    EXPECT_TRUE(frameContains(h.frame(), "You escaped."));
}

TEST(Battle, AttackingEveryTurnMatchesTheGameAndShowsTheResult) {
    game::GameState expected = levelTwenty();
    game::Outcome outcome;
    {
        auto battle = game::startMission(expected, game::shippedContent(), "the_gorgon",
                                         &game::shippedScripts())
                          .value();
        (void)battle.start();
        while (!battle.over())
            (void)battle.act(game::Attack{});
        outcome = battle.outcome();
        (void)game::finishMission(expected, game::shippedContent(), "the_gorgon", battle);
    }

    Harness h("battle_attack");
    enterBattle(h, levelTwenty());
    ASSERT_TRUE(fightUntilResult(h, ch('1')));
    EXPECT_EQ(*h.session.state, expected);
    ASSERT_EQ(outcome, game::Outcome::Defeat); // the ported balance (tasks.md 10.5)
    EXPECT_TRUE(frameContains(h.frame(), "DEFEAT"));
    EXPECT_TRUE(frameContains(h.frame(), "All equipment destroyed!"));
    EXPECT_EQ(rowText(h.frame(), 8).find_first_not_of(' '), std::string::npos);
}

TEST(Battle, KeysOtherThanEnterAreIgnoredWhileEventsPlay) {
    Harness h("battle_busy");
    enterBattle(h, levelTwenty());
    ASSERT_TRUE(h.press({ch('1')})); // attack: its events start playing
    const game::GameState during = *h.session.state;
    ASSERT_TRUE(h.press({ch('5')})); // Flee (5) is ignored mid-animation
    EXPECT_EQ(*h.session.state, during);
    EXPECT_FALSE(frameContains(h.frame(), "flees"));
}

TEST(Battle, SkillOverlayDimsUnlearnedSkillsAndEscCloses) {
    Harness h("battle_skills");
    game::GameState state = levelTwenty();
    state.hero.learnedSkills.insert("angel_wings");
    enterBattle(h, state);
    ASSERT_TRUE(h.press({ch('2')}));
    Framebuffer frame = h.frame();
    EXPECT_NE(rowText(frame, 12).find("Guardian Angel"), std::string::npos);
    EXPECT_EQ(frame.at(20, 12).fg, palette::kAccent); // learned, affordable, selected
    EXPECT_EQ(frame.at(20, 13).fg, palette::kDim);    // Meteor FIST: not learned
    for (int y = 12; y <= 19;
         ++y) // eight skills; descriptions are clipped at the panel's right border
        EXPECT_EQ(frame.at(79, y).glyph, U'│') << "row " << y;
    EXPECT_NE(rowText(frame, 16).find("Let There Be Light (50)"), std::string::npos);
    ASSERT_TRUE(h.press({kEsc}));
    EXPECT_FALSE(frameContains(h.frame(), "Guardian Angel"));

    ASSERT_TRUE(h.press({ch('2'), ch('1')})); // cast it
    EXPECT_TRUE(frameContains(h.frame(), "ARIA casts Guardian Angel!"));
}

TEST(Vocabulary, NoScreenShowsAnMpLabel) {
    // "MP" as a whole word, anywhere on the screen.
    const auto showsMp = [](const Framebuffer& frame) {
        for (int y = 0; y < frame.height(); ++y) {
            const std::string row = " " + rowText(frame, y) + " ";
            for (std::size_t at = row.find("MP"); at != std::string::npos;
                 at = row.find("MP", at + 1))
                if (!std::isalpha(static_cast<unsigned char>(row[at - 1])) &&
                    !std::isalpha(static_cast<unsigned char>(row[at + 2])))
                    return true;
        }
        return false;
    };

    Harness create("vocab_create");
    create.startAtTitle();
    ASSERT_TRUE(create.press({ch('1'), kEnter, kEnter}));
    ASSERT_TRUE(create.type("Aria"));
    ASSERT_TRUE(create.press({kEnter, kEnter, kEnter}));
    EXPECT_TRUE(frameContains(create.frame(), "Fervor 50"));
    EXPECT_FALSE(showsMp(create.frame())) << "Create Hero";

    Harness hub("vocab_hub");
    game::GameState state = levelTwenty();
    hub.startAtHub(state);
    EXPECT_TRUE(frameContains(hub.frame(), "FV "));
    EXPECT_FALSE(showsMp(hub.frame())) << "Hub";
    ASSERT_TRUE(hub.press({kHubTown, ch('3'), ch('3')})); // Town, Prior, Build Charisma
    EXPECT_TRUE(frameContains(hub.frame(), "Max Fervor"));
    EXPECT_FALSE(showsMp(hub.frame())) << "Prior";

    Harness battle("vocab_battle");
    enterBattle(battle, levelTwenty());
    ASSERT_TRUE(battle.press({ch('2')})); // skill overlay open
    EXPECT_TRUE(frameContains(battle.frame(), "Fervor cost"));
    EXPECT_FALSE(showsMp(battle.frame())) << "Battle";
}

TEST(Avaritia, MissionUnlocksAtLevel15) {
    Harness h("avaritia_unlock");
    game::GameState state = freshGame();
    state.hero.level = 14;
    h.startAtHub(state);
    ASSERT_TRUE(h.press({}));
    EXPECT_NE(rowText(h.frame(), 9).find("Requires Lv 15"), std::string::npos);
    h.session.state->hero.level = 15;
    ASSERT_TRUE(h.press({ch('x')})); // refresh
    EXPECT_EQ(rowText(h.frame(), 9).find("Requires"), std::string::npos);
    ASSERT_TRUE(h.press({kHubMission}));
    EXPECT_TRUE(frameContains(h.frame(), "Act I: \"Avaritia\""));
    EXPECT_TRUE(frameContains(h.frame(), "Requires Lv 20")); // the Medusa bonus still waits
}

TEST(Avaritia, VictoryPaysGoldDustAndADarkPearl) {
    game::GameState state = levelTwenty();
    state.hero.level = 15;
    state.hero.atk = 100000;  // ends the fight in the hero's favor
    state.hero.expToNext = 1; // the 5000 EXP reward is sure to level up
    game::GameState expected = state;
    {
        auto battle = game::startMission(expected, game::shippedContent(), "avaritia",
                                         &game::shippedScripts())
                          .value();
        (void)battle.start();
        while (!battle.over())
            (void)battle.act(game::Attack{});
        ASSERT_EQ(battle.outcome(), game::Outcome::Victory);
        (void)game::finishMission(expected, game::shippedContent(), "avaritia", battle);
    }

    Harness h("avaritia_win");
    enterBattle(h, state, '1', "THE COUNTING HOUSE");
    EXPECT_TRUE(frameContains(h.frame(), "AVARITIA"));
    ASSERT_TRUE(fightUntilResult(h, ch('1')));
    EXPECT_EQ(*h.session.state, expected);
    EXPECT_TRUE(frameContains(h.frame(), "VICTORY"));
    EXPECT_TRUE(frameContains(h.frame(), "Items: Gold Dust, Dark Pearl"));
    const int levels = expected.hero.level - state.hero.level;
    ASSERT_GT(levels, 0);
    EXPECT_TRUE(frameContains(h.frame(), std::format("Level up! +{} · {} blessing{} at the Prior",
                                                     levels, levels, levels == 1 ? "" : "s")));
    EXPECT_EQ(h.session.state->hero.bag.count("gold_dust"), 1);
    EXPECT_EQ(h.session.state->hero.bag.count("black_pearl"), 1);
    EXPECT_TRUE(h.session.state->hero.completedMissions.contains("avaritia"));
}

namespace {

// FNV-1a over every cell (glyph, colors) of a frame: a stable fingerprint across platforms.
std::uint64_t fingerprint(const Framebuffer& frame, std::uint64_t hash = 14695981039346656037ull) {
    const auto mix = [&hash](std::uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            hash ^= (v >> (8 * i)) & 0xFF;
            hash *= 1099511628211ull;
        }
    };
    for (int y = 0; y < frame.height(); ++y)
        for (int x = 0; x < frame.width(); ++x) {
            const engine::Cell& c = frame.at(x, y);
            mix(static_cast<std::uint32_t>(c.glyph));
            mix(static_cast<std::uint32_t>(c.fg.r << 16 | c.fg.g << 8 | c.fg.b));
            mix(static_cast<std::uint32_t>(c.bg.r << 16 | c.bg.g << 8 | c.bg.b));
        }
    return hash;
}

} // namespace

// The compact battle layout must stay byte-identical when BattleView gains other layouts
// (add-large-art-layout task 4.1). The expected value was recorded before that change.
TEST(BattleView, CompactFramesAreUnchanged) {
    ViewFixture f;
    f.play({ev::TurnStarted{Side::Hero, 1}, ev::Attacked{Side::Hero},
            ev::Damaged{Side::Enemy, 380, 2620, game::DamageCause::Attack},
            ev::SkillCast{"angel_wings", 10}, ev::Healed{Side::Hero, 25, 300},
            ev::StatusApplied{Side::Enemy, game::StatusId::Stun, 3},
            ev::TurnSkipped{Side::Enemy, game::StatusId::Stun}, ev::HpHalved{Side::Enemy, 1310},
            ev::ExtraTurn{Side::Enemy}, ev::Defeated{Side::Enemy},
            ev::BattleEnded{game::Outcome::Victory, 2}});
    std::uint64_t hash = 14695981039346656037ull;
    for (int step = 0; step < 300; ++step) {
        hash = fingerprint(f.frame(), hash);
        f.view.tick(kStep);
    }
    EXPECT_EQ(hash, 1102535103608328823ull) << "record: " << hash;
}

TEST(BattleView, ScriptLinesAndErrorsAreLogged) {
    ViewFixture f;
    f.play({ev::ScriptSaid{Side::Enemy, "The Gorgon hisses."},
            ev::ScriptError{"scripts/medusa.lua", "scripts/medusa.lua:3: boom"}});
    EXPECT_TRUE(frameContains(f.frame(), "The Gorgon hisses."));
    f.view.skip();
    EXPECT_TRUE(frameContains(f.frame(), "Something went wrong with MEDUSA's magic."));
    EXPECT_FALSE(frameContains(f.frame(), "boom")); // details stay out of the player's log
}

TEST(BattleChoicesScreen, FiveActionsAndAnItemOverlayWithCounts) {
    Harness h("item_overlay");
    game::GameState state = levelTwenty();
    state.hero.hp = 100;
    ASSERT_TRUE(state.hero.bag.add("health_potion", 2).has_value());
    enterBattle(h, state);
    EXPECT_NE(rowText(h.frame(), 22).find("Attack      Skill      Item      Defend      Flee"),
              std::string::npos);
    ASSERT_TRUE(h.press({ch('3')})); // Item
    Framebuffer frame = h.frame();
    EXPECT_TRUE(frameContains(frame, "Items · how many you carry"));
    EXPECT_TRUE(frameContains(frame, "Holy Water x2"));
    ASSERT_TRUE(h.press({ch('1')}));
    EXPECT_TRUE(frameContains(h.frame(), "ARIA uses Holy Water."));
}

TEST(BattleChoicesScreen, ItemWithNothingToUseSaysSo) {
    Harness h("no_items");
    enterBattle(h, levelTwenty());
    ASSERT_TRUE(h.press({ch('3')}));
    EXPECT_TRUE(frameContains(h.frame(), "You carry no items to use."));
    EXPECT_FALSE(frameContains(h.frame(), "Items · how many"));
}

TEST(BattleChoicesScreen, DefendShowsTheGuard) {
    Harness h("defend_screen");
    enterBattle(h, levelTwenty());
    ASSERT_TRUE(h.press({ch('4')}));
    const Framebuffer frame = h.frame();
    EXPECT_TRUE(frameContains(frame, "ARIA raises a guard."));
    EXPECT_TRUE(frameContains(frame, "[GUARD 1]"));
}

TEST(BattleView, ChargingShowsATagAndAWarning) {
    ViewFixture f;
    f.play({ev::StatusApplied{Side::Enemy, game::StatusId::Charging, 1}});
    EXPECT_TRUE(frameContains(f.frame(), "[CHARGE]"));
    EXPECT_TRUE(frameContains(f.frame(), "MEDUSA is gathering its strength!"));
}
