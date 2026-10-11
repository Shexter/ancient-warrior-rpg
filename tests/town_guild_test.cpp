#include "game/town.hpp"

#include "state_helpers.hpp"

#include <gtest/gtest.h>

using namespace heroes::game;

namespace {

const Content& content() { return shippedContent(); }

template <class F> void expectRefused(GameState state, TownError expected, F act) {
    const GameState before = state;
    const auto result = act(state);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), expected) << describe(result.error());
    EXPECT_EQ(state, before);
}

void finish(Battle& battle) {
    while (!battle.over())
        ASSERT_TRUE(battle.act(Attack{}));
}

GameState missionReady() {
    GameState state = freshState();
    state.hero.level = 20;
    state.hero.atk = 10000; // one-shots Medusa
    return state;
}

} // namespace

TEST(Guild, ReviveWithFortyGold) {
    GameState state = freshState();
    state.hero.hp = 0;
    state.hero.gold = 40;
    EXPECT_TRUE(revive(state));
    EXPECT_EQ(state.hero.hp, 100);
    EXPECT_EQ(state.hero.gold, 0);
}

TEST(Guild, ReviveCostsOneHundredAndCapsAtMaxHp) {
    GameState state = freshState("rogue");
    state.hero.hp = 0;
    state.hero.maxHp = 60;
    EXPECT_TRUE(revive(state));
    EXPECT_EQ(state.hero.hp, 60);
    EXPECT_EQ(state.hero.gold, 19900);
}

TEST(Guild, ReviveDoesNothingToLivingHero) {
    GameState state = freshState();
    const GameState before = state;
    EXPECT_FALSE(revive(state));
    EXPECT_EQ(state, before);
}

TEST(Guild, ConsumablesRestoreAndCap) {
    GameState state = freshState();
    state.hero.hp = 10;
    state.hero.mp = 45;
    addAll(state.hero.bag, {"health_potion", "mana_potion", "eternal_youth"});
    ASSERT_TRUE(useConsumable(state, content(), "health_potion"));
    EXPECT_EQ(state.hero.hp, 110);
    ASSERT_TRUE(useConsumable(state, content(), "mana_potion"));
    EXPECT_EQ(state.hero.mp, 50);
    state.hero.hp = 3;
    state.hero.mp = 1;
    ASSERT_TRUE(useConsumable(state, content(), "eternal_youth"));
    EXPECT_EQ(state.hero.hp, 350);
    EXPECT_EQ(state.hero.mp, 50);
    EXPECT_EQ(state.hero.bag.count("eternal_youth"), 0);
}

TEST(Guild, ConsumableRefusals) {
    GameState holder = freshState();
    addAll(holder.hero.bag, {"topaz"});
    const auto use = [](std::string_view id) {
        return [=](GameState& s) { return useConsumable(s, content(), id); };
    };
    expectRefused(holder, TownError::UnknownItem, use("nope"));
    expectRefused(holder, TownError::ItemNotInBag, use("health_potion"));
    expectRefused(holder, TownError::NotConsumable, use("topaz"));
}

TEST(Guild, CharismaGivesTenForFiveHundredExp) {
    GameState state = freshState();
    state.hero.level = 10;
    state.hero.exp = 500;
    ASSERT_TRUE(buyCharisma(state, content(), CharismaStat::Atk, 500));
    EXPECT_EQ(state.hero.atk, 60);
    EXPECT_EQ(state.hero.gold, 19000);
    EXPECT_EQ(state.hero.exp, 0);
}

TEST(Guild, CharismaRaisesMaxButNotCurrentHp) {
    GameState state = freshState();
    state.hero.level = 10;
    state.hero.exp = 500;
    ASSERT_TRUE(buyCharisma(state, content(), CharismaStat::MaxHp, 500));
    EXPECT_EQ(state.hero.maxHp, 360);
    EXPECT_EQ(state.hero.hp, 350);
}

TEST(Guild, CharismaTargetsEachStat) {
    const struct {
        CharismaStat stat;
        int Hero::* field;
        int base;
    } cases[] = {{CharismaStat::MaxMp, &Hero::maxMp, 50}, {CharismaStat::Def, &Hero::def, 5}};
    for (const auto& c : cases) {
        GameState state = freshState();
        state.hero.level = 10;
        state.hero.exp = 500;
        ASSERT_TRUE(buyCharisma(state, content(), c.stat, 500));
        EXPECT_EQ(state.hero.*c.field, c.base + 10);
    }
}

TEST(Guild, CharismaRefusals) {
    const auto charisma = [](std::int64_t exp) {
        return [=](GameState& s) { return buyCharisma(s, content(), CharismaStat::Atk, exp); };
    };
    GameState state = freshState();
    state.hero.level = 10;
    state.hero.exp = 600;
    expectRefused(state, TownError::InvalidQuantity, charisma(30)); // worthless sacrifice
    expectRefused(state, TownError::InvalidQuantity, charisma(0));
    expectRefused(state, TownError::NotEnoughExp, charisma(601));
    GameState poor = state;
    poor.hero.gold = 999;
    expectRefused(poor, TownError::NotEnoughGold, charisma(500));
    GameState low = state;
    low.hero.level = 9;
    expectRefused(low, TownError::LevelTooLow, charisma(500));
}

TEST(Guild, CharismaCannotPushAStatPastTheCap) {
    GameState state = freshState();
    state.hero.level = 10;
    state.hero.exp = 500;
    state.hero.atk = kMaxStat - 5; // 500 exp would add 10
    expectRefused(state, TownError::InvalidQuantity,
                  [](GameState& s) { return buyCharisma(s, content(), CharismaStat::Atk, 500); });
    state.hero.atk = kMaxStat - 10; // exactly reaching the cap is fine
    ASSERT_TRUE(buyCharisma(state, content(), CharismaStat::Atk, 500));
    EXPECT_EQ(state.hero.atk, kMaxStat);
}

TEST(Guild, CharismaQuoteMatchesWhatIsCharged) {
    GameState state = freshState();
    state.hero.level = 10;
    state.hero.exp = 520;
    const CharismaQuote quote = charismaQuote(state.hero, content(), 520);
    EXPECT_EQ(quote.points, 10);
    EXPECT_EQ(quote.goldCost, 1000);
    EXPECT_EQ(quote.gain, 10);
    const std::int64_t gold = state.hero.gold;
    ASSERT_TRUE(buyCharisma(state, content(), CharismaStat::Def, 520));
    EXPECT_EQ(state.hero.gold, gold - quote.goldCost);
    EXPECT_EQ(state.hero.exp, 0); // all entered exp is spent, remainder included
}

TEST(Guild, CharismaRefusesZeroGain) {
    GameState state = freshState(); // Fighter multiplier 1: 1 point at level 11 gains 10/11 = 0
    state.hero.level = 11;
    state.hero.exp = 50;
    expectRefused(state, TownError::InvalidQuantity,
                  [](GameState& s) { return buyCharisma(s, content(), CharismaStat::Atk, 50); });
}

TEST(Guild, LearnFrozenThroneConsumesKeyAndGold) {
    GameState state = freshState();
    state.hero.level = 15;
    ASSERT_TRUE(state.hero.bag.add("broken_staff", 1));
    ASSERT_TRUE(learnSkill(state, content(), "frozen_throne"));
    EXPECT_TRUE(state.hero.learnedSkills.contains("frozen_throne"));
    EXPECT_EQ(state.hero.gold, 0);
    EXPECT_EQ(state.hero.bag.count("broken_staff"), 0);
}

TEST(Guild, LearnRefusals) {
    const auto learn = [](std::string_view id) {
        return [=](GameState& s) { return learnSkill(s, content(), id); };
    };
    GameState ready = freshState();
    ready.hero.level = 15;
    addAll(ready.hero.bag, {"broken_staff"});
    GameState low = ready;
    low.hero.level = 14;
    expectRefused(low, TownError::LevelTooLow, learn("frozen_throne"));
    GameState noKey = freshState();
    noKey.hero.level = 15;
    expectRefused(noKey, TownError::ItemNotInBag, learn("frozen_throne"));
    GameState poor = ready;
    poor.hero.gold = 19999;
    expectRefused(poor, TownError::NotEnoughGold, learn("frozen_throne"));
    expectRefused(ready, TownError::UnknownItem, learn("nope"));
    GameState known = ready;
    known.hero.learnedSkills.insert("frozen_throne");
    expectRefused(known, TownError::AlreadyLearned, learn("frozen_throne"));
}

TEST(Guild, RenameCostsFiftyThousand) {
    GameState state = freshState();
    state.hero.gold = 60000;
    ASSERT_TRUE(rename(state, "  Bob  "));
    EXPECT_EQ(state.hero.name, "Bob");
    EXPECT_EQ(state.hero.gold, 10000);
}

TEST(Guild, RenameRefusals) {
    GameState rich = freshState();
    rich.hero.gold = 60000;
    expectRefused(rich, TownError::InvalidName, [](GameState& s) { return rename(s, "   "); });
    expectRefused(rich, TownError::InvalidName,
                  [](GameState& s) { return rename(s, "ninecharz"); });
    expectRefused(freshState(), TownError::NotEnoughGold,
                  [](GameState& s) { return rename(s, "Bob"); });
}

TEST(Mission, StartRefusals) {
    const auto start = [](GameState& s) { return startMission(s, content(), "the_gorgon"); };
    expectRefused(freshState(), TownError::MissionLocked, start);
    expectRefused(freshState(), TownError::UnknownItem,
                  [](GameState& s) { return startMission(s, content(), "nope"); });
    GameState dead = missionReady();
    dead.hero.hp = 0;
    expectRefused(dead, TownError::HeroDead, start);
    GameState bare = missionReady();
    bare.hero.armor.reset();
    expectRefused(bare, TownError::NotEquipped, start);
    GameState done = missionReady();
    done.hero.completedMissions.insert("the_gorgon");
    expectRefused(done, TownError::MissionCompleted, start);
}

TEST(Mission, StartAdvancesMasterByOneFork) {
    GameState state = missionReady();
    const Rng expected = afterOneFork(state.rng);
    ASSERT_TRUE(startMission(state, content(), "the_gorgon"));
    EXPECT_EQ(state.rng, expected);
}

TEST(Mission, VictoryGrantsRewardsAndMarksCompleted) {
    GameState state = missionReady();
    auto battle = startMission(state, content(), "the_gorgon");
    ASSERT_TRUE(battle.has_value());
    finish(*battle);
    ASSERT_EQ(battle->outcome(), Outcome::Victory);
    const auto result = finishMission(state, content(), "the_gorgon", *battle);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->outcome, Outcome::Victory);
    EXPECT_EQ(result->expGained, 10000);
    EXPECT_EQ(result->goldGained, 25000);
    EXPECT_EQ(state.hero.exp, 10000);
    EXPECT_EQ(state.hero.gold, 45000);
    EXPECT_EQ(state.hero.bag.count("thors_hammer"), 1);
    EXPECT_EQ(result->itemsGained, std::vector<std::string>{"thors_hammer"});
    EXPECT_TRUE(state.hero.completedMissions.contains("the_gorgon"));
    EXPECT_EQ(state.hero.hp, battle->hero().hp);
    // Not repeatable.
    EXPECT_EQ(startMission(state, content(), "the_gorgon").error(), TownError::MissionCompleted);
}

TEST(Mission, VictoryWithFullBagLosesRewardItem) {
    GameState state = missionReady();
    fillWithout(state.hero.bag, "thors_hammer", 10);
    auto battle = startMission(state, content(), "the_gorgon");
    ASSERT_TRUE(battle.has_value());
    finish(*battle);
    const auto result = finishMission(state, content(), "the_gorgon", *battle);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->itemsLostBagFull, std::vector<std::string>{"thors_hammer"});
    EXPECT_EQ(state.hero.bag.count("thors_hammer"), 0);
    EXPECT_TRUE(state.hero.completedMissions.contains("the_gorgon"));
}

TEST(Mission, DefeatDestroysGear) {
    GameState state = missionReady();
    state.hero.atk = 1; // cannot hurt Medusa
    state.hero.def = 0;
    ASSERT_TRUE(state.hero.bag.add("iron_sword", 1));
    auto battle = startMission(state, content(), "the_gorgon");
    ASSERT_TRUE(battle.has_value());
    finish(*battle);
    ASSERT_EQ(battle->outcome(), Outcome::Defeat);
    const auto result = finishMission(state, content(), "the_gorgon", *battle);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(state.hero.hp, 0);
    EXPECT_FALSE(state.hero.weapon.has_value());
    EXPECT_FALSE(state.hero.armor.has_value());
    EXPECT_EQ(state.hero.bag.count("iron_sword"), 1); // the bag survives
    EXPECT_TRUE(state.hero.completedMissions.empty());
    EXPECT_EQ(state.hero.exp, 0);
}

TEST(Mission, FleeChangesNothingButHpAndMp) {
    GameState state = missionReady();
    auto battle = startMission(state, content(), "the_gorgon");
    ASSERT_TRUE(battle.has_value());
    ASSERT_TRUE(battle->act(Flee{}));
    GameState expected = state;
    const auto result = finishMission(state, content(), "the_gorgon", *battle);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->outcome, Outcome::Fled);
    EXPECT_EQ(state, expected);
    EXPECT_TRUE(startMission(state, content(), "the_gorgon").has_value());
}
