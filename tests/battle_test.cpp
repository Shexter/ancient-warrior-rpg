#include "event_format.hpp"
#include "game/battle.hpp"
#include "shipped_content.hpp"

#include <gtest/gtest.h>

using namespace heroes::game;

namespace {

Combatant strongHero() { return {"Aria", 2000, 2000, 200, 200, 400, 200, 50, {}}; }

Battle makeBattle(Combatant hero, std::set<std::string> learned = {}, std::uint64_t seed = 1) {
    return Battle(shippedContent(), std::move(hero), shippedContent().enemy("medusa"),
                  std::move(learned), Rng::stream(seed, 0));
}

void expectActionError(Battle& battle, const PlayerAction& action, ActionError expected) {
    const Combatant hero = battle.hero();
    const Combatant enemy = battle.enemy();
    const int turn = battle.turn();
    const bool over = battle.over();
    auto result = battle.act(action);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), expected);
    EXPECT_EQ(battle.hero(), hero);
    EXPECT_EQ(battle.enemy(), enemy);
    EXPECT_EQ(battle.turn(), turn);
    EXPECT_EQ(battle.over(), over);
}

} // namespace

TEST(Battle, StartEmitsBattleStarted) {
    Battle battle = makeBattle(strongHero());
    const auto events = battle.start();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(toString(events[0]), "BattleStarted Aria Medusa medusa");
}

TEST(Battle, UnknownSkillIsRejected) {
    Battle battle = makeBattle(strongHero(), {"frozen_throne"});
    expectActionError(battle, CastSkill{"no_such_skill"}, ActionError::UnknownSkill);
}

TEST(Battle, UnlearnedSkillIsRejected) {
    Battle battle = makeBattle(strongHero());
    expectActionError(battle, CastSkill{"frozen_throne"}, ActionError::SkillNotLearned);
}

TEST(Battle, SkillWithoutEnoughMpIsRejected) {
    Combatant hero = strongHero();
    hero.mp = 39;
    Battle battle = makeBattle(hero, {"frozen_throne"});
    expectActionError(battle, CastSkill{"frozen_throne"}, ActionError::NotEnoughMp);
}

TEST(Battle, FrozenThroneIsNotReadyWhileEnemyIsStunned) {
    Battle battle = makeBattle(strongHero(), {"frozen_throne"});
    ASSERT_TRUE(battle.act(CastSkill{"frozen_throne"}));
    expectActionError(battle, CastSkill{"frozen_throne"}, ActionError::SkillNotReady);
}

TEST(Battle, ActingAfterTheBattleEndsIsRejected) {
    Battle battle = makeBattle(strongHero());
    ASSERT_TRUE(battle.act(Flee{}));
    expectActionError(battle, Attack{}, ActionError::BattleOver);
}

TEST(Battle, FleeEndsWithFled) {
    Battle battle = makeBattle(strongHero());
    auto events = battle.act(Flee{});
    ASSERT_TRUE(events.has_value());
    ASSERT_EQ(events->size(), 3u);
    EXPECT_EQ(toString((*events)[0]), "TurnStarted Hero 1");
    EXPECT_EQ(toString((*events)[1]), "Fled");
    EXPECT_EQ(toString((*events)[2]), "BattleEnded Fled 1");
    EXPECT_TRUE(battle.over());
    EXPECT_EQ(battle.outcome(), Outcome::Fled);
    EXPECT_EQ(battle.hero().hp, 2000); // the enemy never got a turn
}

TEST(Battle, AbsorbedHitEmitsZeroDamage) {
    Combatant hero = strongHero();
    hero.atk = 10; // below Medusa's DEF of 20
    Battle battle = makeBattle(hero);
    auto events = battle.act(Attack{});
    ASSERT_TRUE(events.has_value());
    EXPECT_EQ(toString((*events)[2]), "Damaged Enemy 0 3000 Attack");
}

TEST(Battle, SkillsHealAndHalve) {
    Combatant hero = strongHero();
    hero.hp = 1000;
    Battle battle = makeBattle(hero, {"angel_wings"});
    auto events = battle.act(CastSkill{"angel_wings"});
    ASSERT_TRUE(events.has_value());
    EXPECT_EQ(toString((*events)[1]), "SkillCast angel_wings 10");
    EXPECT_EQ(toString((*events)[2]), "Healed Hero 1000 2000");
}

TEST(Battle, SameSeedAndActionsGiveSameEvents) {
    auto run = [] {
        Battle battle = makeBattle(strongHero(), {}, 77);
        std::vector<std::string> lines;
        while (!battle.over()) {
            const auto events = *battle.act(Attack{});
            for (const Event& e : events)
                lines.push_back(toString(e));
        }
        return lines;
    };
    EXPECT_EQ(run(), run());
}
