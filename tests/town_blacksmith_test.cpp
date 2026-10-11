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

GameState forgeReady() {
    GameState state = freshState();
    addAll(state.hero.bag, {"iron_sword", "phoenix_feather", "fire_dust"});
    return state;
}

} // namespace

TEST(Blacksmith, ForgeVectorsAtLevelNine) {
    const std::uint32_t rolls[] = {32, 44, 68, 3, 39};
    for (std::uint64_t seed = 1; seed <= 5; ++seed) {
        Rng rng = Rng::stream(seed, 0);
        EXPECT_EQ(rng.below(100), rolls[seed - 1]);
        rng = Rng::stream(seed, 0);
        EXPECT_TRUE(rollForge(rng, 9)) << seed;
    }
}

TEST(Blacksmith, ForgeChanceScalesWithLevel) {
    // Same five rolls at forge level 1 (10%): only the roll of 3 succeeds.
    for (std::uint64_t seed = 1; seed <= 5; ++seed) {
        Rng rng = Rng::stream(seed, 0);
        EXPECT_EQ(rollForge(rng, 1), seed == 4) << seed;
    }
}

TEST(Blacksmith, ForgeConsumesIngredientsAndFollowsRoll) {
    for (int forgeLevel : {1, 9}) {
        for (std::uint64_t seed = 1; seed <= 12; ++seed) {
            GameState state = forgeReady();
            state.rng = Rng::stream(seed, 0);
            state.hero.forgeLevel = forgeLevel;
            Rng master = state.rng;
            Rng roll = master.fork();
            const bool expected = rollForge(roll, forgeLevel);

            const auto result = forge(state, content(), "forge_fire_claymore");
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(result->success, expected);
            EXPECT_EQ(result->itemId, "fire_claymore");
            EXPECT_EQ(state.hero.bag.count("iron_sword"), 0);
            EXPECT_EQ(state.hero.bag.count("phoenix_feather"), 0);
            EXPECT_EQ(state.hero.bag.count("fire_dust"), 0);
            EXPECT_EQ(state.hero.bag.count("fire_claymore"), expected ? 1 : 0);
            EXPECT_EQ(state.hero.forgeLevel, expected ? std::min(10, forgeLevel + 1) : forgeLevel);
            EXPECT_EQ(state.rng, master);
        }
    }
}

TEST(Blacksmith, ForgeLevelCapsAtTen) {
    GameState state = forgeReady();
    state.hero.forgeLevel = 10;
    const auto result = forge(state, content(), "forge_fire_claymore");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->success);
    EXPECT_EQ(state.hero.forgeLevel, 10);
}

TEST(Blacksmith, ForgeRefusals) {
    const auto doForge = [](GameState& s) { return forge(s, content(), "forge_fire_claymore"); };
    GameState missing = freshState();
    addAll(missing.hero.bag, {"iron_sword", "fire_dust"});
    expectRefused(missing, TownError::MissingIngredient, doForge);

    GameState dead = forgeReady();
    dead.hero.hp = 0;
    expectRefused(dead, TownError::HeroDead, doForge);

    expectRefused(forgeReady(), TownError::UnknownItem,
                  [](GameState& s) { return forge(s, content(), "nope"); });

    GameState full = freshState();
    addAll(full.hero.bag, {"iron_sword", "phoenix_feather", "fire_dust"}, 2);
    addAll(full.hero.bag, {"topaz", "gold_leaf", "frozen_tears", "broken_staff", "pure_ice",
                           "turtle_shell", "gold_dust"});
    expectRefused(full, TownError::BagFull, doForge);
}

TEST(Blacksmith, EquipSwapsAndResetsOldItemToLevelOne) {
    GameState state = freshState();
    state.hero.weapon->upgradeLevel = 3;
    ASSERT_TRUE(state.hero.bag.add("fire_claymore", 1));
    ASSERT_TRUE(equip(state, content(), EquipSlot::Weapon, "fire_claymore"));
    EXPECT_EQ(state.hero.weapon, (Equipped{"fire_claymore", 1}));
    EXPECT_EQ(state.hero.bag.count("fire_claymore"), 0);
    EXPECT_EQ(state.hero.bag.count("iron_sword"), 1);
    ASSERT_TRUE(equip(state, content(), EquipSlot::Weapon, "iron_sword"));
    EXPECT_EQ(state.hero.weapon, (Equipped{"iron_sword", 1}));
}

TEST(Blacksmith, EquipIntoEmptySlot) {
    GameState state = freshState();
    state.hero.armor.reset();
    ASSERT_TRUE(state.hero.bag.add("diamond_plate", 1));
    ASSERT_TRUE(equip(state, content(), EquipSlot::Armor, "diamond_plate"));
    EXPECT_EQ(state.hero.armor, (Equipped{"diamond_plate", 1}));
    EXPECT_EQ(state.hero.bag.count("diamond_plate"), 0);
}

TEST(Blacksmith, EquipRefusals) {
    const auto equipWeapon = [](std::string_view id) {
        return [=](GameState& s) { return equip(s, content(), EquipSlot::Weapon, id); };
    };
    GameState holder = freshState();
    addAll(holder.hero.bag, {"fire_claymore", "diamond_plate", "topaz"});
    expectRefused(holder, TownError::UnknownItem, equipWeapon("nope"));
    expectRefused(holder, TownError::ItemNotInBag, equipWeapon("thors_hammer"));
    expectRefused(holder, TownError::WrongSlot, equipWeapon("diamond_plate"));
    expectRefused(holder, TownError::WrongSlot, equipWeapon("topaz"));

    GameState same = freshState();
    ASSERT_TRUE(same.hero.bag.add("iron_sword", 1));
    expectRefused(same, TownError::AlreadyEquipped, equipWeapon("iron_sword"));

    GameState dead = holder;
    dead.hero.hp = 0;
    expectRefused(dead, TownError::HeroDead, equipWeapon("fire_claymore"));

    GameState full = freshState();
    ASSERT_TRUE(full.hero.bag.add("fire_claymore", 2));
    fillWithout(full.hero.bag, "fire_claymore", 9);
    expectRefused(full, TownError::BagFull, equipWeapon("fire_claymore"));
}

TEST(Blacksmith, UpgradeConsumesLevelManyMaterials) {
    GameState state = freshState();
    state.hero.weapon->upgradeLevel = 2;
    state.hero.exp = 5000;
    state.hero.gold = 50000;
    ASSERT_TRUE(state.hero.bag.add("fire_dust", 3));
    ASSERT_TRUE(upgrade(state, content(), EquipSlot::Weapon));
    EXPECT_EQ(state.hero.weapon->upgradeLevel, 3);
    EXPECT_EQ(state.hero.bag.count("fire_dust"), 1);
    EXPECT_EQ(state.hero.exp, 3000);
    EXPECT_EQ(state.hero.gold, 30000);
}

TEST(Blacksmith, ArmorUsesTurtleShell) {
    GameState state = freshState();
    state.hero.exp = 1000;
    ASSERT_TRUE(state.hero.bag.add("turtle_shell", 1));
    ASSERT_TRUE(upgrade(state, content(), EquipSlot::Armor));
    EXPECT_EQ(state.hero.armor->upgradeLevel, 2);
    EXPECT_EQ(state.hero.gold, 10000);
    EXPECT_EQ(state.hero.bag.count("turtle_shell"), 0);
}

TEST(Blacksmith, UpgradeCostAndForgeChanceFormulas) {
    const UpgradeCost cost = upgradeCost(3);
    EXPECT_EQ(cost.materialCount, 3);
    EXPECT_EQ(cost.exp, 3000);
    EXPECT_EQ(cost.gold, 30000);
    EXPECT_EQ(forgeChancePercent(9), 90);
}

TEST(Blacksmith, UpgradeRefusals) {
    const auto up = [](GameState& s) { return upgrade(s, content(), EquipSlot::Weapon); };
    GameState rich = freshState();
    rich.hero.weapon->upgradeLevel = 2;
    rich.hero.exp = 5000;
    rich.hero.gold = 50000;
    ASSERT_TRUE(rich.hero.bag.add("fire_dust", 1));
    expectRefused(rich, TownError::MissingMaterial, up);

    GameState ready = rich;
    ASSERT_TRUE(ready.hero.bag.add("fire_dust", 1));
    GameState noExp = ready;
    noExp.hero.exp = 1999;
    expectRefused(noExp, TownError::NotEnoughExp, up);
    GameState noGold = ready;
    noGold.hero.gold = 19999;
    expectRefused(noGold, TownError::NotEnoughGold, up);
    GameState dead = ready;
    dead.hero.hp = 0;
    expectRefused(dead, TownError::HeroDead, up);
    GameState max = ready;
    max.hero.weapon->upgradeLevel = 10;
    expectRefused(max, TownError::MaxUpgrade, up);
    GameState bare = ready;
    bare.hero.weapon.reset();
    expectRefused(bare, TownError::NotEquipped, up);
}
