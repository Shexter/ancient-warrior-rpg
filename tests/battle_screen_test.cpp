#include "app/battle_view.hpp"
#include "app/screens/mission.hpp"
#include "game/town.hpp"

#include "frame_text.hpp"
#include "screen_harness.hpp"

#include <gtest/gtest.h>

#include <format>
#include <string>
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
        view.draw(f, &shippedSprites().at("hero_fighter.sprite"),
                  &shippedSprites().at("medusa.sprite"));
        return f;
    }

    game::Combatant hero, enemy;
    BattleView view;
};

// Hub -> Mission -> the first mission's intro -> "Enter the lair?" -> Yes.
void enterBattle(Harness& h, game::GameState state) {
    h.startAtHub(std::move(state));
    ASSERT_TRUE(h.press({ch('2'), ch('1')}));
    ASSERT_TRUE(frameContains(h.frame(), "MEDUSA LAIR"));
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
    ASSERT_TRUE(h.press({ch('2')}));
    const Framebuffer frame = h.frame();
    EXPECT_NE(rowText(frame, 7).find("Cleared"), std::string::npos);
    EXPECT_NE(rowText(frame, 8).find("Coming Soon"), std::string::npos);
    EXPECT_NE(rowText(frame, 9).find("Coming Soon"), std::string::npos);
    ASSERT_TRUE(h.press({ch('1'), kEsc}));
    EXPECT_EQ(h.app->depth(), 1u);
}

TEST(Battle, FleeingPlaysOutAndAppliesTheSameResult) {
    Harness h("battle_flee");
    game::GameState expected = levelTwenty();
    {
        auto battle = game::startMission(expected, game::shippedContent(), "the_gorgon").value();
        (void)battle.start();
        (void)battle.act(game::Flee{});
        (void)game::finishMission(expected, game::shippedContent(), "the_gorgon", battle);
    }

    enterBattle(h, levelTwenty());
    EXPECT_TRUE(frameContains(h.frame(), "Turn 1"));
    EXPECT_TRUE(h.session.dirty);
    ASSERT_TRUE(h.press({ch('3')}));
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
        auto battle = game::startMission(expected, game::shippedContent(), "the_gorgon").value();
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
    ASSERT_TRUE(h.press({ch('3')})); // Flee is ignored mid-animation
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
    EXPECT_NE(rowText(frame, 15).find("Guardian Angel"), std::string::npos);
    EXPECT_EQ(frame.at(20, 15).fg, palette::kAccent); // learned, affordable, selected
    EXPECT_EQ(frame.at(20, 16).fg, palette::kDim);    // Meteor FIST: not learned
    for (int y = 15; y <= 19; ++y) // descriptions are clipped at the panel's right border
        EXPECT_EQ(frame.at(79, y).glyph, U'│') << "row " << y;
    EXPECT_NE(rowText(frame, 19).find("Let There Be Light (50)"), std::string::npos);
    ASSERT_TRUE(h.press({kEsc}));
    EXPECT_FALSE(frameContains(h.frame(), "Guardian Angel"));

    ASSERT_TRUE(h.press({ch('2'), ch('1')})); // cast it
    EXPECT_TRUE(frameContains(h.frame(), "ARIA casts Guardian Angel!"));
}
