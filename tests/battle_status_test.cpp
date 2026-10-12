#include "event_format.hpp"
#include "game/battle.hpp"
#include "shipped_content.hpp"

#include <gtest/gtest.h>

#include <algorithm>

using namespace heroes::game;

namespace {

Battle makeBattle(Combatant hero, std::set<std::string> learned) {
    return Battle(shippedContent(), std::move(hero), shippedContent().enemy("medusa"),
                  std::move(learned), Rng::stream(5, 0), &shippedScripts());
}

Combatant strongHero() { return {"Aria", 2000, 2000, 200, 200, 400, 200, 50, {}}; }

int count(const std::vector<Event>& events, std::string_view line) {
    return static_cast<int>(
        std::ranges::count_if(events, [&](const Event& e) { return toString(e) == line; }));
}

} // namespace

TEST(BattleStatus, FrozenThroneSkipsExactlyThreeEnemyTurns) {
    Battle battle = makeBattle(strongHero(), {"frozen_throne"});
    std::vector<Event> all = *battle.act(CastSkill{"frozen_throne"});
    auto second = *battle.act(Attack{}); // the cast turn already had the 1st skip
    all.insert(all.end(), second.begin(), second.end());
    // After the 2nd skip both statuses are still active.
    EXPECT_EQ(count(all, "TurnSkipped Enemy stun"), 2);
    EXPECT_EQ(count(all, "StatusExpired Enemy stun"), 0);
    EXPECT_EQ(battle.enemy().statuses.size(), 2u);

    auto third = *battle.act(Attack{});
    EXPECT_EQ(count(third, "TurnSkipped Enemy stun"), 1);
    EXPECT_EQ(count(third, "StatusExpired Enemy stun"), 1);
    EXPECT_EQ(count(third, "StatusExpired Enemy defense_down"), 1);
    EXPECT_TRUE(battle.enemy().statuses.empty());

    auto fourth = *battle.act(Attack{});
    EXPECT_EQ(count(fourth, "TurnSkipped Enemy stun"), 0);
    EXPECT_EQ(count(fourth, "Attacked Enemy"), 1);
}

TEST(BattleStatus, ReapplyingRefreshesAndDoesNotStack) {
    Battle battle = makeBattle(strongHero(), {"golden_bell"});
    ASSERT_TRUE(battle.act(CastSkill{"golden_bell"}));
    ASSERT_EQ(battle.hero().statuses.size(), 1u);
    EXPECT_EQ(battle.hero().statuses[0].turnsRemaining, 2);

    // Without a recast the status would tick down to 1 on turn 2.
    auto events = *battle.act(CastSkill{"golden_bell"});
    EXPECT_EQ(count(events, "StatusApplied Hero reflect 2"), 1);
    ASSERT_EQ(battle.hero().statuses.size(), 1u);
    EXPECT_EQ(battle.hero().statuses[0].turnsRemaining, 2);
}

TEST(BattleStatus, ReflectCoversExactlyTwoEnemyPhases) {
    Battle battle = makeBattle(strongHero(), {"golden_bell"});
    std::vector<Event> all = *battle.act(CastSkill{"golden_bell"});
    for (int i = 0; i < 2; ++i) {
        auto events = *battle.act(Attack{});
        all.insert(all.end(), events.begin(), events.end());
    }
    EXPECT_EQ(count(all, "StatusExpired Hero reflect"), 1);
    EXPECT_TRUE(battle.hero().statuses.empty());
}

TEST(BattleStatus, TurnCapEndsInDraw) {
    // Neither side can hurt the other: hero ATK below Medusa's DEF, hero DEF above her ATK.
    Combatant hero{"Aria", 2000, 2000, 0, 0, 0, 1000, 0, {}};
    Battle battle = makeBattle(hero, {});
    std::vector<Event> last;
    while (!battle.over()) {
        ASSERT_LE(battle.turn(), Battle::kTurnCap);
        last = *battle.act(Attack{});
    }
    EXPECT_EQ(battle.turn(), Battle::kTurnCap);
    EXPECT_EQ(battle.outcome(), Outcome::Draw);
    EXPECT_EQ(toString(last.back()), "BattleEnded Draw 1000");
}
