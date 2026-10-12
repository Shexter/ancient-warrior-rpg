#include "game/battle.hpp"
#include "game/game_state.hpp"
#include "shipped_content.hpp"

#include <gtest/gtest.h>

using namespace heroes::game;

namespace {

Hero newHero(std::string_view pathId) { return newGame(shippedContent(), "Aria", pathId, 1).hero; }

} // namespace

TEST(Hero, NewGameStartingState) {
    const GameState state = newGame(shippedContent(), "Aria", "fighter", 42);
    const Hero& h = state.hero;
    EXPECT_EQ(h.level, 1);
    EXPECT_EQ(h.expToNext, 100);
    EXPECT_EQ(h.exp, 0);
    EXPECT_EQ(h.gold, 20000);
    EXPECT_EQ(h.hp, 350);
    EXPECT_EQ(h.mp, 50);
    EXPECT_EQ(h.weapon->itemId, "iron_sword");
    EXPECT_EQ(h.weapon->upgradeLevel, 1);
    EXPECT_EQ(h.armor->itemId, "steel_shield");
    EXPECT_FALSE(h.bank.has_value());
    EXPECT_EQ(h.forgeLevel, 9);
    EXPECT_EQ(state.seed, 42u);
    EXPECT_EQ(state.rng, Rng::stream(42, 0));
}

TEST(Hero, HunterStartsWithDoubleExpToNext) {
    EXPECT_EQ(newHero("hunter").expToNext, 200);
    EXPECT_EQ(newHero("rogue").expToNext, 300);
}

struct LevelVector {
    const char* path;
    std::int64_t gained;
    int level;
    std::int64_t expToNext;
};

class Leveling : public testing::TestWithParam<LevelVector> {};

TEST_P(Leveling, MatchesReferenceLoop) {
    const LevelVector& v = GetParam();
    Hero hero = newHero(v.path);
    const int gained = grantExp(hero, shippedContent(), v.gained);
    EXPECT_EQ(hero.level, v.level);
    EXPECT_EQ(hero.expToNext, v.expToNext);
    EXPECT_EQ(gained, v.level - 1);
    EXPECT_EQ(hero.exp, v.gained); // spendable exp is separate from level progress
}

INSTANTIATE_TEST_SUITE_P(
    Vectors, Leveling,
    testing::Values(LevelVector{"fighter", 10000, 14, 500}, LevelVector{"rogue", 10000, 8, 800},
                    LevelVector{"hunter", 100, 1, 100}, LevelVector{"hunter", 200, 2, 400}),
    [](const testing::TestParamInfo<LevelVector>& param) {
        return std::string(param.param.path) + "_" + std::to_string(param.param.gained);
    });

TEST(Hero, GrantExpIgnoresNonPositiveAmounts) {
    Hero hero = newHero("fighter");
    const Hero before = hero;
    EXPECT_EQ(grantExp(hero, shippedContent(), 0), 0);
    EXPECT_EQ(grantExp(hero, shippedContent(), -50), 0);
    EXPECT_EQ(hero, before);
}

TEST(Hero, LevelingDoesNotRaiseStats) {
    Hero hero = newHero("fighter");
    const Hero before = hero;
    grantExp(hero, shippedContent(), 10000);
    EXPECT_EQ(hero.atk, before.atk);
    EXPECT_EQ(hero.maxHp, before.maxHp);
    EXPECT_EQ(hero.blessings, 13); // one point per level gained, 1 -> 14
}

TEST(Hero, BlessingPointsAccumulateAcrossGains) {
    Hero hero = newHero("fighter");
    grantExp(hero, shippedContent(), 99); // not a level
    EXPECT_EQ(hero.blessings, 0);
    grantExp(hero, shippedContent(), 1); // level 2
    grantExp(hero, shippedContent(), 200); // level 3
    EXPECT_EQ(hero.blessings, 2);
}

TEST(Hero, NameValidation) {
    EXPECT_EQ(validateHeroName("").error(), TownError::InvalidName);
    EXPECT_EQ(validateHeroName("   ").error(), TownError::InvalidName);
    EXPECT_EQ(validateHeroName("123456789").error(), TownError::InvalidName);
    EXPECT_EQ(*validateHeroName("  Aria  "), "Aria");
    EXPECT_EQ(*validateHeroName("12345678"), "12345678");
    EXPECT_EQ(*validateHeroName("A B"), "A B");
    EXPECT_EQ(validateHeroName("Ar\xC3\xAD"
                               "a")
                  .error(),
              TownError::InvalidName);
    EXPECT_EQ(validateHeroName("tab\there").error(), TownError::InvalidName);
}

TEST(Hero, IronSwordAtUpgradeTwoGivesOneHundredFiftyAtk) {
    Hero hero = newHero("fighter");
    hero.weapon->upgradeLevel = 2;
    EXPECT_EQ(hero.atk + equipmentBonus(hero, shippedContent()).atk, 150);
    EXPECT_EQ(heroCombatant(hero, shippedContent()).atk, 150);
}

TEST(Hero, EquipmentBonusSumsWeaponAndArmor) {
    const Hero hero = newHero("fighter"); // iron_sword atk 50, steel_shield def 20, both level 1
    const StatBonus bonus = equipmentBonus(hero, shippedContent());
    EXPECT_EQ(bonus.atk, 50);
    EXPECT_EQ(bonus.def, 20);
    EXPECT_EQ(bonus.life, 0);
}

TEST(Hero, CanAdventureNeedsGearAndHp) {
    Hero hero = newHero("fighter");
    EXPECT_TRUE(canAdventure(hero));
    hero.hp = 0;
    EXPECT_FALSE(canAdventure(hero));
    hero.hp = 1;
    hero.weapon.reset();
    EXPECT_FALSE(canAdventure(hero));
}
