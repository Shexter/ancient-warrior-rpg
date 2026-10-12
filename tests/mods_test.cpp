#include "game/battle.hpp"
#include "game/content_loader.hpp"
#include "game/game_state.hpp"
#include "game/script/script_host.hpp"
#include "game/town.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

using namespace heroes::game;
namespace fs = std::filesystem;

namespace {

struct Loaded {
    std::expected<Content, std::vector<ContentError>> content;
    std::vector<std::string> log;
};

// The shipped data with the mods in tests/fixtures/mods/<scenario>.
Loaded withMods(const std::string& scenario) {
    Loaded out{std::unexpected(std::vector<ContentError>{}), {}};
    out.content = loadContent(fs::path(HEROES_SOURCE_DIR) / "data",
                              fs::path(HEROES_SOURCE_DIR) / "tests" / "fixtures" / "mods" / scenario,
                              &out.log);
    return out;
}

bool logged(const Loaded& l, const std::string& line) {
    return std::ranges::find(l.log, line) != l.log.end();
}

} // namespace

TEST(Mods, AModAddsAnEnemyItsScriptAndAMissionUsingIt) {
    const Loaded l = withMods("add_enemy");
    ASSERT_TRUE(l.content.has_value());
    const Content& c = *l.content;
    ASSERT_NE(c.findEnemy("cyclops"), nullptr);
    EXPECT_EQ(c.enemy("cyclops").script, "mods/cyclops-lite/scripts/cyclops.lua");
    EXPECT_TRUE(c.scripts().contains("mods/cyclops-lite/scripts/cyclops.lua"));
    EXPECT_EQ(c.mission("cyclops_cave").enemy, "cyclops");
    EXPECT_EQ(c.mods(), (std::vector<ModRef>{{"cyclops-lite", "1.0.0"}})); // README folder ignored
    EXPECT_EQ(l.log, std::vector<std::string>{"mod 'cyclops-lite' 1.0.0 loaded"});
    EXPECT_NE(c.findEnemy("medusa"), nullptr); // the base is still there
}

TEST(Mods, HigherPriorityWinsAndEveryOverrideIsLogged) {
    const Loaded l = withMods("override");
    ASSERT_TRUE(l.content.has_value());
    EXPECT_EQ(l.content->item("iron_sword").bonus.atk, 222); // b-second has priority 2
    EXPECT_EQ(l.content->mods(),
              (std::vector<ModRef>{{"a-first", "1.0.0"}, {"b-second", "1.0.0"}}));
    EXPECT_TRUE(logged(l, "mod 'a-first': overrides items/iron_sword"));
    EXPECT_TRUE(logged(l, "mod 'b-second': overrides items/iron_sword"));
    // Overrides replace in place, so the item keeps its spot (drop tables depend on order).
    EXPECT_EQ(l.content->items().front().id, "iron_sword");
}

TEST(Mods, ABrokenModIsSkippedWholeAndTheRestLoad) {
    const Loaded l = withMods("broken");
    ASSERT_TRUE(l.content.has_value());
    EXPECT_EQ(l.content->mods(), (std::vector<ModRef>{{"good-mod", "1.0.0"}}));
    EXPECT_NE(l.content->findItem("lucky_coin"), nullptr);
    EXPECT_EQ(l.content->findSkill("bad_skill"), nullptr);
    EXPECT_TRUE(logged(l, "mod 'bad-mod' skipped: mods/bad-mod/skills.json: skill 'bad_skill': "
                          "unknown key item 'nope_item'"));
    EXPECT_TRUE(logged(l, "mod 'wrong-name' skipped: mods/wrong-name/mod.json: name "
                          "'not-the-folder' must match the folder name 'wrong-name'"));
}

TEST(Mods, AScriptPathLeavingTheModFolderIsRejected) {
    const Loaded l = withMods("escape");
    ASSERT_TRUE(l.content.has_value());
    EXPECT_TRUE(l.content->mods().empty());
    EXPECT_EQ(l.content->findEnemy("sneak"), nullptr);
    EXPECT_TRUE(logged(l, "mod 'sneaky' skipped: mods/sneaky/enemies.json: enemy 'sneak': "
                          "script path '../../evil.lua' must stay inside the mod folder"));
}

TEST(Mods, NoModsFolderMeansNoMods) {
    std::vector<std::string> log;
    const auto c = loadContent(fs::path(HEROES_SOURCE_DIR) / "data", fs::path("/no/such/mods"), &log);
    ASSERT_TRUE(c.has_value());
    EXPECT_TRUE(c->mods().empty());
    EXPECT_TRUE(log.empty());
}

namespace {

const Content& withExampleMod() {
    static const Content content =
        loadContent(fs::path(HEROES_SOURCE_DIR) / "data", fs::path(HEROES_SOURCE_DIR) / "mods",
                    nullptr)
            .value();
    return content;
}

} // namespace

TEST(ExampleMod, TheCyclopsCaveIsALevel15Mission) {
    const Content& c = withExampleMod();
    EXPECT_EQ(c.mods(), (std::vector<ModRef>{{"example-cyclops", "1.0.0"}}));
    EXPECT_EQ(c.mission("cyclops_cave").minLevel, 15);
    EXPECT_EQ(c.mission("cyclops_cave").enemy, "cyclops");
    EXPECT_TRUE(c.mission("cyclops_cave").repeatable);
}

TEST(ExampleMod, TheCyclopsThrowsABoulderOnTurns3And6) {
    const Content& c = withExampleMod();
    script::ScriptHost host(c);
    Combatant hero{"Aria", 100000, 100000, 0, 0, 1, 0, 0, {}}; // survives, barely scratches it
    Battle battle(c, hero, c.enemy("cyclops"), {}, Rng::stream(7, 0), &host);
    std::vector<int> boulderTurns;
    for (int i = 0; i < 7; ++i) {
        const std::vector<Event> events = battle.act(Attack{}).value(); // kept alive for the loop
        for (const Event& e : events)
            if (const auto* said = std::get_if<ev::ScriptSaid>(&e)) {
                EXPECT_EQ(said->text, "The Cyclops hurls a boulder!");
                boulderTurns.push_back(battle.turn());
            }
    }
    EXPECT_EQ(boulderTurns, (std::vector<int>{3, 6}));
}

TEST(ExampleMod, BoulderTossCanBeLearnedAndCast) {
    const Content& c = withExampleMod();
    GameState state = newGame(c, "Aria", "fighter", 7);
    state.hero.level = 10;
    ASSERT_TRUE(state.hero.bag.add("topaz", 1).has_value());
    ASSERT_TRUE(learnSkill(state, c, "boulder_toss").has_value());
    EXPECT_EQ(state.hero.gold, 20000 - 8000);

    script::ScriptHost host(c);
    Combatant hero{"Aria", 1000, 1000, 100, 100, 400, 0, 0, {}};
    Battle battle(c, hero, c.enemy("cyclops"), state.hero.learnedSkills, Rng::stream(7, 0), &host);
    const auto events = battle.act(CastSkill{"boulder_toss"}).value();
    bool hit = false;
    for (const Event& e : events)
        if (const auto* d = std::get_if<ev::Damaged>(&e); d && d->target == Side::Enemy)
            hit = d->amount == 400 * 3 / 2 - 60; // 1.5x ATK, minus the Cyclops's 60 DEF
    EXPECT_TRUE(hit);
    EXPECT_EQ(battle.hero().mp, 70);
}
