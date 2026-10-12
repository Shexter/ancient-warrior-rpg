#include "game/battle.hpp"
#include "game/content_loader.hpp"
#include "game/script/script_host.hpp"

#include "event_format.hpp"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <functional>
#include <fstream>
#include <string>
#include <vector>

using namespace heroes::game;
namespace fs = std::filesystem;

namespace {

// Shipped data plus a scripted enemy ("scripted": 5000 HP, 100 ATK, 0 DEF) and a scripted skill
// ("scripted_skill", 10 MP), with the two given script sources, in a scratch folder.
fs::path scriptedData(const std::string& name, const std::string& enemyScript,
                      const std::string& skillScript) {
    const fs::path root = fs::temp_directory_path() / "heroes_script_battle" / name;
    fs::remove_all(root);
    fs::create_directories(root / "assets");
    fs::copy(fs::path(HEROES_SOURCE_DIR) / "assets" / "sprites", root / "assets" / "sprites",
             fs::copy_options::recursive);
    fs::copy(fs::path(HEROES_SOURCE_DIR) / "data", root / "data");
    fs::create_directories(root / "data" / "scripts");
    std::ofstream(root / "data" / "scripts" / "enemy.lua") << enemyScript;
    std::ofstream(root / "data" / "scripts" / "skill.lua") << skillScript;

    const auto patch = [&](const char* file, const char* list, nlohmann::json entry) {
        std::ifstream in(root / "data" / file);
        nlohmann::json doc = nlohmann::json::parse(in);
        in.close();
        doc[list].push_back(std::move(entry));
        std::ofstream(root / "data" / file) << doc.dump(2);
    };
    patch("enemies.json", "enemies",
          {{"id", "scripted"}, {"name", "Golem"}, {"sprite", "monster.sprite"}, {"maxHp", 5000},
           {"atk", 100}, {"def", 0}, {"life", 0}, {"script", "scripts/enemy.lua"},
           {"rewardExp", 0}, {"rewardGold", 0}});
    patch("skills.json", "skills",
          {{"id", "scripted_skill"}, {"name", "Test Strike"}, {"description", "A test."},
           {"mpCost", 10}, {"learnLevel", 1}, {"goldCost", 0}, {"keyItem", "topaz"},
           {"script", "scripts/skill.lua"}});
    return root / "data";
}

const char* kAttackEnemy = "return { take_turn = function(ctx) ctx.attack() end }";
const char* kNoopSkill = "return { cast = function(ctx) end }";

// A loaded scripted world: content, a host with its scripts, and battles against "scripted".
struct World {
    World(const std::string& name, const std::string& enemyScript,
          const std::string& skillScript = kNoopSkill) {
        auto loaded = loadContent(scriptedData(name, enemyScript, skillScript));
        if (!loaded) {
            for (const ContentError& e : loaded.error())
                ADD_FAILURE() << formatError(e);
            return;
        }
        content.emplace(std::move(*loaded));
        host.emplace(*content);
    }

    // A 400 ATK, 0 DEF hero with 100 MP who knows the scripted skill.
    Battle battle(std::uint64_t seed = 42, bool withHost = true) {
        Combatant hero{"Aria", 1000, 1000, 100, 100, 400, 0, 0, {}};
        return Battle(*content, hero, content->enemy("scripted"), {"scripted_skill"},
                      Rng::stream(seed, 0), withHost ? &*host : nullptr);
    }

    std::optional<Content> content;
    std::optional<script::ScriptHost> host;
};

std::vector<std::string> formatted(const std::vector<Event>& events) {
    std::vector<std::string> out;
    for (const Event& e : events)
        out.push_back(toString(e));
    return out;
}

bool has(const std::vector<std::string>& lines, const std::string& line) {
    return std::ranges::find(lines, line) != lines.end();
}

} // namespace

TEST(ScriptBattle, ScriptedSkillDealsOneAndAHalfAttack) {
    World w("skill_600", kAttackEnemy,
            "return { cast = function(ctx) ctx.damage(ctx.self.atk * 3 // 2, true) end }");
    Battle b = w.battle();
    const auto events = b.act(CastSkill{"scripted_skill"});
    ASSERT_TRUE(events.has_value());
    const auto lines = formatted(*events);
    EXPECT_TRUE(has(lines, toString(ev::SkillCast{"scripted_skill", 10})));
    EXPECT_TRUE(has(lines, toString(ev::Damaged{Side::Enemy, 600, 4400, DamageCause::Skill})));
    EXPECT_EQ(b.hero().mp, 90);
}

TEST(ScriptBattle, FloatArgumentsAreScriptErrors) {
    World skill("float_skill", kAttackEnemy,
                "return { cast = function(ctx) ctx.damage(1.5, true) end }");
    Battle b = skill.battle();
    EXPECT_EQ(b.act(CastSkill{"scripted_skill"}).error(), ActionError::ScriptFailed);

    World enemy("float_enemy", "return { take_turn = function(ctx) ctx.rng(2.0) end }");
    Battle e = enemy.battle();
    const auto lines = formatted(e.act(Attack{}).value());
    EXPECT_TRUE(has(lines, "ScriptError scripts/enemy.lua"));
    EXPECT_TRUE(has(lines, "Attacked Enemy")); // the basic attack instead
}

TEST(ScriptBattle, OperationsAfterADeathDoNothing) {
    World w("after_death", "return { take_turn = function(ctx)\n"
                           "  ctx.damage(999999, false)\n"
                           "  assert(ctx.over())\n"
                           "  ctx.heal('self', 10)\n"
                           "  ctx.say('too late')\n"
                           "  ctx.apply_status('target', 'stun', 2)\n"
                           "  assert(ctx.attack() == 0)\n"
                           "end }");
    Battle b = w.battle();
    const auto lines = formatted(b.act(Attack{}).value());
    ASSERT_GE(lines.size(), 2u);
    EXPECT_EQ(lines[lines.size() - 2], "Defeated Hero");
    EXPECT_EQ(lines.back().rfind("BattleEnded Defeat", 0), 0u);
    EXPECT_FALSE(has(lines, "ScriptSaid Enemy too late"));
    EXPECT_EQ(b.outcome(), Outcome::Defeat);
}

TEST(ScriptBattle, RunawayEnemyScriptIsUndoneAndAttacksInstead) {
    // The failed script talked, hit and drew a random number before looping: none of it stays,
    // so the battle matches one whose script loops straight away.
    World noisy("loop_noisy", "return { take_turn = function(ctx)\n"
                              "  ctx.say('hi') ctx.damage(10, false) ctx.rng(5)\n"
                              "  while true do end\n"
                              "end }");
    World quiet("loop_quiet", "return { take_turn = function(ctx) while true do end end }");
    Battle a = noisy.battle(), b = quiet.battle();
    for (int turn = 0; turn < 3; ++turn) {
        const auto la = formatted(a.act(Attack{}).value());
        EXPECT_EQ(la, formatted(b.act(Attack{}).value()));
        EXPECT_TRUE(has(la, "ScriptError scripts/enemy.lua"));
        EXPECT_FALSE(has(la, "ScriptSaid Enemy hi"));
        EXPECT_TRUE(has(la, "Damaged Hero 100 " + std::to_string(1000 - 100 * (turn + 1)) +
                                " Attack"));
    }
}

TEST(ScriptBattle, FailedSkillScriptChangesNothing) {
    World w("skill_error", kAttackEnemy,
            "return { cast = function(ctx) ctx.damage(50, false) error('boom') end }");
    Battle tried = w.battle(), untouched = w.battle();
    EXPECT_EQ(tried.act(CastSkill{"scripted_skill"}).error(), ActionError::ScriptFailed);
    EXPECT_EQ(tried.hero(), untouched.hero());   // MP unspent
    EXPECT_EQ(tried.enemy(), untouched.enemy()); // the 50 damage undone
    EXPECT_EQ(tried.turn(), 0);
    // Same RNG and state: the next action plays out identically.
    EXPECT_EQ(formatted(tried.act(Attack{}).value()), formatted(untouched.act(Attack{}).value()));
}

TEST(ScriptBattle, SameSeedSameEvents) {
    const char* random = "return { take_turn = function(ctx)\n"
                         "  if ctx.rng(3) == 0 then ctx.attack() else ctx.damage(ctx.rng(50), true) end\n"
                         "  if ctx.rng(4) == 0 then ctx.extra_turn() end\n"
                         "end }";
    World w("replay", random);
    Battle a = w.battle(42), b = w.battle(42);
    for (int turn = 0; turn < 5; ++turn)
        EXPECT_EQ(formatted(a.act(Attack{}).value()), formatted(b.act(Attack{}).value()));
}

TEST(ScriptBattle, ViewsReadLiveValuesAndSayIsSanitized) {
    World w("views", "return { take_turn = function(ctx)\n"
                     "  local before = ctx.target.hp\n"
                     "  ctx.damage(100, false)\n"
                     "  ctx.say(ctx.self.name .. ' took ' .. (before - ctx.target.hp))\n"
                     "  ctx.say('caf\\195\\169 ' .. string.rep('x', 100))\n"
                     "  ctx.apply_status('self', 'reflect', 2)\n"
                     "  ctx.say(tostring(ctx.self.has('reflect')))\n"
                     "end }");
    Battle b = w.battle();
    const auto lines = formatted(b.act(Attack{}).value());
    EXPECT_TRUE(has(lines, "ScriptSaid Enemy Golem took 100"));
    EXPECT_TRUE(has(lines, "ScriptSaid Enemy caf?? " + std::string(74, 'x'))); // ASCII, 80 max
    EXPECT_TRUE(has(lines, "ScriptSaid Enemy true"));
}

TEST(ScriptBattle, EnemyOnlyAndReadOnlyRulesHold) {
    World w("rules", kAttackEnemy, "return { cast = function(ctx) ctx.extra_turn() end }");
    Battle b = w.battle();
    EXPECT_EQ(b.act(CastSkill{"scripted_skill"}).error(), ActionError::ScriptFailed);

    World ro("readonly", "return { take_turn = function(ctx) ctx.self.hp = 1 end }");
    Battle r = ro.battle();
    EXPECT_TRUE(has(formatted(r.act(Attack{}).value()), "ScriptError scripts/enemy.lua"));
}

TEST(ScriptBattle, WithoutAHostScriptedEnemiesAttackAndSkillsFail) {
    World w("no_host", "return { take_turn = function(ctx) ctx.say('never') end }");
    Battle b = w.battle(42, false);
    const auto lines = formatted(b.act(Attack{}).value());
    EXPECT_TRUE(has(lines, "ScriptError scripts/enemy.lua"));
    EXPECT_TRUE(has(lines, "Attacked Enemy"));
    EXPECT_EQ(b.act(CastSkill{"scripted_skill"}).error(), ActionError::ScriptFailed);
}

namespace {

// Every startup error for a data folder, formatted.
std::vector<std::string> loadErrors(const fs::path& dataDir) {
    std::vector<std::string> out;
    if (auto loaded = loadContent(dataDir); !loaded)
        for (const ContentError& e : loaded.error())
            out.push_back(formatError(e));
    return out;
}

void editSkill(const fs::path& dataDir, const std::function<void(nlohmann::json&)>& change) {
    std::ifstream in(dataDir / "skills.json");
    nlohmann::json doc = nlohmann::json::parse(in);
    in.close();
    for (auto& skill : doc["skills"])
        if (skill["id"] == "scripted_skill")
            change(skill);
    std::ofstream(dataDir / "skills.json") << doc.dump(2);
}

} // namespace

TEST(ScriptContent, ASkillHasAnEffectOrAScriptNotBoth) {
    const fs::path data = scriptedData("both", kAttackEnemy, kNoopSkill);
    editSkill(data, [](nlohmann::json& s) { s["effect"] = {{"healToFull", true}}; });
    EXPECT_EQ(loadErrors(data), std::vector<std::string>{
                                    "skills.json: skill 'scripted_skill': has both 'effect' and "
                                    "'script'; use one"});
}

TEST(ScriptContent, MissingAndEscapingScriptPathsAreRejected) {
    const fs::path data = scriptedData("missing", kAttackEnemy, kNoopSkill);
    editSkill(data, [](nlohmann::json& s) { s["script"] = "scripts/nope.lua"; });
    EXPECT_EQ(loadErrors(data),
              std::vector<std::string>{"skills.json: skill 'scripted_skill': script file "
                                       "'scripts/nope.lua' not found"});
    editSkill(data, [](nlohmann::json& s) { s["script"] = "../outside.lua"; });
    EXPECT_EQ(loadErrors(data),
              std::vector<std::string>{"skills.json: skill 'scripted_skill': script path "
                                       "'../outside.lua' must stay inside the data folder"});
}

TEST(ScriptContent, ScriptsMustReturnTheRequiredFunction) {
    const fs::path data = scriptedData(
        "shape", "return { cast = function(ctx) end }", // an enemy needs take_turn
        "return 7");
    const auto errors = loadErrors(data);
    ASSERT_EQ(errors.size(), 2u);
    EXPECT_EQ(errors[0], "skills.json: skill 'scripted_skill': scripts/skill.lua: the script must "
                         "return a table");
    EXPECT_EQ(errors[1], "enemies.json: enemy 'scripted': script 'scripts/enemy.lua' must return a "
                         "table with a 'take_turn' function");
}

TEST(ScriptContent, SyntaxErrorsNameThePathAndLine) {
    const fs::path data =
        scriptedData("syntax", kAttackEnemy, "return {\n  cast = function(ctx)\n    ctx.say('x'\nend }");
    const auto errors = loadErrors(data);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_NE(errors[0].find("skill 'scripted_skill': scripts/skill.lua:4:"), std::string::npos)
        << errors[0];
}

TEST(ScriptContent, ShippedContentHasNoScriptsYet) { // Medusa moves to Lua in chunk 3
    EXPECT_TRUE(loadContent(fs::path(HEROES_SOURCE_DIR) / "data").value().scripts().empty());
}
