#include "game/dungeon/expedition.hpp"
#include "game/save.hpp"
#include "state_helpers.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

using namespace heroes::game;
using namespace heroes::game::dungeon;

namespace {

const DungeonDef& shippedDungeon() {
    static const DungeonDef def =
        loadDungeon(std::filesystem::path(HEROES_SOURCE_DIR) / "data", shippedContent()).value();
    return def;
}

// An expedition on a hand-built floor 1: rows of '#', '.', '>' and '<', the hero at `hero`.
Expedition onFloor(const std::vector<std::string>& rows, Pos hero) {
    Expedition exp;
    exp.seed = 5;
    exp.rng = Rng::stream(5, 0);
    Floor& f = exp.floor;
    f.index = 1;
    f.width = static_cast<int>(rows[0].size());
    f.height = static_cast<int>(rows.size());
    for (const std::string& row : rows)
        for (const char c : row)
            f.tiles.push_back(c == '#' ? Tile::Wall : c == '>' ? Tile::StairsDown : c == '<' ? Tile::StairsUp : Tile::Floor);
    f.entry = hero;
    exp.hero = hero;
    exp.explored.assign(f.tiles.size(), false);
    exp.visible.assign(f.tiles.size(), false);
    return exp;
}

MapEnemy awakeRat(Pos at) { return {"rat", at, true}; }

Battle battleWith(const Expedition& exp, int enemy, Combatant hero) {
    return Battle(shippedContent(), std::move(hero), encounterEnemy(exp, shippedContent(), shippedDungeon(), enemy), {},
                  Rng::stream(1, 0));
}

void fightUntilOver(Battle& battle, PlayerAction action) {
    while (!battle.over())
        ASSERT_TRUE(battle.act(action).has_value());
}

} // namespace

TEST(Expedition, StartsOnFloorOneAtItsEntryAndAFallenHeroCannot) {
    GameState state = freshState();
    const auto exp = startExpedition(state, shippedDungeon());
    ASSERT_TRUE(exp.has_value());
    EXPECT_EQ(exp->floor.index, 1);
    EXPECT_EQ(exp->hero, exp->floor.entry);
    EXPECT_TRUE(exp->visible[static_cast<std::size_t>(exp->hero.y * exp->floor.width + exp->hero.x)]);
    EXPECT_EQ(exp->floor, generateFloor(shippedDungeon(), exp->seed, 1));

    state.hero.hp = 0;
    EXPECT_EQ(startExpedition(state, shippedDungeon()).error(), TownError::HeroDead);
}

TEST(Expedition, AWallBumpPassesNoTurn) {
    GameState state = freshState();
    Expedition exp = onFloor({"#######", "#.....#", "#######"}, {1, 1});
    exp.floor.enemies.push_back(awakeRat({5, 1}));
    const StepResult r = step(exp, state, shippedContent(), shippedDungeon(), ExpAction::North);
    ASSERT_EQ(r.events.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<xev::BumpedWall>(r.events[0]));
    EXPECT_EQ(exp.hero, (Pos{1, 1}));
    EXPECT_EQ(exp.floor.enemies[0].pos, (Pos{5, 1})); // the rat didn't move
}

TEST(Expedition, ExploredOnlyGrows) {
    GameState state = freshState();
    auto exp = startExpedition(state, shippedDungeon()).value();
    exp.floor.enemies.clear();
    std::vector<bool> before = exp.explored;
    for (int i = 0; i < 60; ++i) {
        const ExpAction a = std::array{ExpAction::East, ExpAction::South, ExpAction::West, ExpAction::North}[(i / 5) % 4];
        step(exp, state, shippedContent(), shippedDungeon(), a);
        for (std::size_t t = 0; t < before.size(); ++t)
            ASSERT_TRUE(!before[t] || exp.explored[t]) << i;
        before = exp.explored;
    }
}

TEST(Expedition, AnEnemyRoundACornerComesForTheHero) {
    // The rat is 5 steps away round the corner: after 4 waits it's next to the hero, and on the
    // 5th it starts the fight instead of moving.
    GameState state = freshState();
    Expedition exp = onFloor({"#######",
                              "#.....#",
                              "#####.#",
                              "#####.#",
                              "#######"},
                             {1, 1});
    exp.floor.enemies.push_back(awakeRat({5, 2}));
    for (int i = 0; i < 4; ++i) {
        const StepResult r = step(exp, state, shippedContent(), shippedDungeon(), ExpAction::Wait);
        ASSERT_FALSE(r.encounterEnemy) << i;
    }
    EXPECT_EQ(exp.floor.enemies[0].pos, (Pos{2, 1}));
    const StepResult r = step(exp, state, shippedContent(), shippedDungeon(), ExpAction::Wait);
    EXPECT_EQ(r.encounterEnemy, 0);
    EXPECT_EQ(exp.floor.enemies[0].pos, (Pos{2, 1}));
}

TEST(Expedition, AnEnemyWithNoPathStaysPut) {
    GameState state = freshState();
    Expedition exp = onFloor({"#######", "#..#..#", "#######"}, {1, 1});
    exp.floor.enemies.push_back(awakeRat({5, 1}));
    step(exp, state, shippedContent(), shippedDungeon(), ExpAction::Wait);
    EXPECT_EQ(exp.floor.enemies[0].pos, (Pos{5, 1}));
}

TEST(Expedition, TwoEnemiesInACorridorNeverShareATile) {
    GameState state = freshState();
    Expedition exp = onFloor({"##########", "#........#", "##########"}, {1, 1});
    exp.floor.enemies = {awakeRat({7, 1}), awakeRat({8, 1})};
    for (int i = 0; i < 6; ++i) {
        step(exp, state, shippedContent(), shippedDungeon(), ExpAction::Wait);
        ASSERT_NE(exp.floor.enemies[0].pos, exp.floor.enemies[1].pos) << i;
    }
    EXPECT_EQ(exp.floor.enemies[0].pos, (Pos{2, 1}));
    EXPECT_EQ(exp.floor.enemies[1].pos, (Pos{3, 1}));
}

TEST(Expedition, AChestOpensWhenWalkedOntoAndStaysInTheBag) {
    GameState state = freshState();
    state.hero.bag = Inventory{};
    Expedition exp = onFloor({"#####", "#<..#", "#####"}, {1, 1});
    exp.floor.chests.push_back({{2, 1}});
    const StepResult r = step(exp, state, shippedContent(), shippedDungeon(), ExpAction::East);
    ASSERT_EQ(r.events.size(), 2u);
    const auto& opened = std::get<xev::ChestOpened>(r.events[1]);
    EXPECT_FALSE(opened.lostBagFull);
    EXPECT_EQ(state.hero.bag.count(opened.itemId), opened.qty);
    EXPECT_TRUE(exp.floor.chests[0].opened);
    EXPECT_NE(exp.message.find("You found"), std::string::npos);
    // Back on the stairs up, the hero leaves with it.
    step(exp, state, shippedContent(), shippedDungeon(), ExpAction::West);
    const StepResult left = step(exp, state, shippedContent(), shippedDungeon(), ExpAction::UseStairs);
    ASSERT_EQ(left.events.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<xev::Left>(left.events[0]));
    EXPECT_EQ(state.hero.bag.count(opened.itemId), opened.qty);
}

TEST(Expedition, AFullBagLosesTheChestsFind) {
    GameState state = freshState();
    state.hero.bag = Inventory{};
    fillWithout(state.hero.bag, "", Inventory::kSlots);
    Expedition exp = onFloor({"####", "#..#", "####"}, {1, 1});
    exp.floor.chests.push_back({{2, 1}});
    const StepResult r = step(exp, state, shippedContent(), shippedDungeon(), ExpAction::East);
    EXPECT_TRUE(std::get<xev::ChestOpened>(r.events[1]).lostBagFull);
    EXPECT_TRUE(exp.floor.chests[0].opened);
}

TEST(Expedition, StairsDownLeadToTheNextFloorsEntry) {
    GameState state = freshState();
    auto exp = startExpedition(state, shippedDungeon()).value();
    exp.hero = *exp.floor.stairsDown;
    const StepResult r = step(exp, state, shippedContent(), shippedDungeon(), ExpAction::UseStairs);
    ASSERT_EQ(r.events.size(), 1u);
    EXPECT_EQ(std::get<xev::Descended>(r.events[0]).floor, 2);
    EXPECT_EQ(exp.floor, generateFloor(shippedDungeon(), exp.seed, 2));
    EXPECT_EQ(exp.hero, exp.floor.entry);
    // Off the stairs, nothing happens and no turn passes.
    const StepResult none = step(exp, state, shippedContent(), shippedDungeon(), ExpAction::UseStairs);
    EXPECT_TRUE(none.events.empty());
    EXPECT_EQ(exp.message, "There are no stairs here.");
}

TEST(Expedition, BumpingOrBeingReachedStartsAnEncounter) {
    GameState state = freshState();
    Expedition exp = onFloor({"######", "#....#", "######"}, {1, 1});
    exp.floor.enemies.push_back({"rat", {2, 1}});
    EXPECT_EQ(step(exp, state, shippedContent(), shippedDungeon(), ExpAction::East).encounterEnemy, 0);
    EXPECT_EQ(exp.hero, (Pos{1, 1}));

    Expedition other = onFloor({"######", "#....#", "######"}, {1, 1});
    other.floor.enemies.push_back(awakeRat({3, 1}));
    EXPECT_FALSE(step(other, state, shippedContent(), shippedDungeon(), ExpAction::Wait).encounterEnemy);
    EXPECT_EQ(step(other, state, shippedContent(), shippedDungeon(), ExpAction::Wait).encounterEnemy, 0);
}

TEST(Expedition, AWonFightRemovesTheEnemyAndPaysItsScaledRewards) {
    GameState state = freshState();
    Expedition exp = onFloor({"####", "#..#", "####"}, {1, 1});
    exp.floor.index = 3;
    exp.floor.enemies.push_back({"rat", {2, 1}});
    Battle battle = battleWith(exp, 0, {"Aria", 5000, 5000, 200, 200, 2000, 500, 0, {}});
    battle.start();
    fightUntilOver(battle, Attack{});
    ASSERT_EQ(battle.outcome(), Outcome::Victory);
    const std::int64_t gold = state.hero.gold;
    const EnemyDef rat = encounterEnemy(exp, shippedContent(), shippedDungeon(), 0);
    resolveEncounter(exp, state, shippedContent(), shippedDungeon(), 0, battle);
    EXPECT_TRUE(exp.floor.enemies.empty());
    EXPECT_EQ(state.hero.gold, gold + rat.rewardGold);
    EXPECT_EQ(rat.rewardGold, 150); // 100 x 150%
    EXPECT_EQ(state.hero.hp, battle.hero().hp);
}

TEST(Expedition, ALostFightLeavesTheHeroAtZeroWithLessExpAndTheirGear) {
    GameState state = freshState();
    state.hero.exp = 100;
    const auto gear = state.hero.weapon;
    Expedition exp = onFloor({"####", "#..#", "####"}, {1, 1});
    exp.floor.enemies.push_back({"rat", {2, 1}});
    Battle battle = battleWith(exp, 0, {"Aria", 10, 10, 0, 0, 1, 0, 0, {}});
    battle.start();
    fightUntilOver(battle, Defend{});
    ASSERT_EQ(battle.outcome(), Outcome::Defeat);
    resolveEncounter(exp, state, shippedContent(), shippedDungeon(), 0, battle);
    EXPECT_EQ(state.hero.hp, 0);
    EXPECT_EQ(state.hero.exp, 0); // 100 - 150, floored at 0
    EXPECT_EQ(state.hero.weapon, gear);
}

TEST(Expedition, FleeingPutsTheEnemyToSleep) {
    GameState state = freshState();
    Expedition exp = onFloor({"######", "#....#", "######"}, {1, 1});
    exp.floor.enemies.push_back(awakeRat({2, 1}));
    Battle battle = battleWith(exp, 0, {"Aria", 5000, 5000, 200, 200, 2000, 500, 0, {}});
    battle.start();
    ASSERT_TRUE(battle.act(Flee{}).has_value());
    ASSERT_EQ(battle.outcome(), Outcome::Fled);
    resolveEncounter(exp, state, shippedContent(), shippedDungeon(), 0, battle);
    EXPECT_FALSE(exp.floor.enemies[0].awake);
    EXPECT_EQ(exp.floor.enemies[0].sleepTurns, 3);
    for (int i = 0; i < 3; ++i) // asleep beside the hero: no fight for three turns
        EXPECT_FALSE(step(exp, state, shippedContent(), shippedDungeon(), ExpAction::Wait).encounterEnemy) << i;
}

TEST(Expedition, ASaveMidFloorTwoComesBackTheSame) {
    // Down to floor 2, a few steps, an enemy moved and asleep, a chest opened: saved and loaded,
    // the expedition (position, explored, enemies, chests, rng) is unchanged.
    GameState state = freshState();
    auto exp = startExpedition(state, shippedDungeon()).value();
    exp.hero = *exp.floor.stairsDown;
    step(exp, state, shippedContent(), shippedDungeon(), ExpAction::UseStairs);
    for (const ExpAction a : {ExpAction::East, ExpAction::East, ExpAction::South, ExpAction::Wait})
        step(exp, state, shippedContent(), shippedDungeon(), a);
    ASSERT_FALSE(exp.floor.enemies.empty());
    exp.floor.enemies[0].pos = {exp.floor.rooms.back().x, exp.floor.rooms.back().y}; // a room's corner
    exp.floor.enemies[0].sleepTurns = 2;
    exp.floor.chests[0].opened = true;
    exp.rng.next();
    exp.message.clear();
    state.expedition = toSave(exp);

    const auto file = std::filesystem::temp_directory_path() / "heroes_expedition_save" / "save.json";
    ASSERT_TRUE(saveGame(state, file).has_value());
    const auto loaded = loadGame(file, shippedContent());
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(*loaded, state);
    const auto back = fromSave(*loaded->expedition, shippedDungeon());
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(*back, exp);
}

TEST(Expedition, ASaveThatDoesntFitTheDungeonIsDropped) {
    GameState state = freshState();
    const auto exp = startExpedition(state, shippedDungeon()).value();
    ExpeditionSave save = toSave(exp);
    save.floor = 9;
    EXPECT_FALSE(fromSave(save, shippedDungeon()));
    save = toSave(exp);
    save.heroX = 0; // in the border wall
    EXPECT_FALSE(fromSave(save, shippedDungeon()));
    save = toSave(exp);
    save.chestsOpened.push_back(true);
    EXPECT_FALSE(fromSave(save, shippedDungeon()));
}
