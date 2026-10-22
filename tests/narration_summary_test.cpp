#include "game/battle.hpp"
#include "game/narration/summary.hpp"
#include "game/narration/templates.hpp"
#include "state_helpers.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace heroes::game;
using namespace heroes::game::narration;

namespace {

const std::filesystem::path kData = std::filesystem::path(HEROES_SOURCE_DIR) / "data";

std::string shippedNarratorJson() {
    std::ifstream in(kData / "narrator.json", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

// The errors from narrator.json after `change`, one per line.
template <class Change> std::string errorsWith(Change change) {
    nlohmann::json j = nlohmann::json::parse(shippedNarratorJson());
    change(j);
    const auto parsed = parseNarrator(j.dump());
    EXPECT_FALSE(parsed.has_value());
    std::string out;
    if (!parsed)
        for (const ContentError& e : parsed.error())
            out += e.file + ": " + e.entry + ": " + e.message + "\n";
    return out;
}

BattleContext contextFor(const Combatant& hero, const EnemyDef& foe) {
    return {&shippedContent(), hero.name, foe.name, hero.hp, hero.maxHp, foe.maxHp, foe.maxHp, false, false, {}};
}

} // namespace

TEST(NarratorDef, TheShippedFileLoadsWithThreeLinesAKey) {
    const auto def = loadNarrator(kData);
    ASSERT_TRUE(def.has_value());
    for (const std::string_view key : kTemplateKeys)
        EXPECT_GE(def->templates.at(std::string(key)).size(), 3u) << key;
    EXPECT_EQ(def->npcs.at("merchant").name, "Marlo the Merchant");
    EXPECT_EQ(def->limits.talkHistory, 6);
}

TEST(NarratorDef, AMissingTemplateKeyIsNamed) {
    const std::string errors = errorsWith([](nlohmann::json& j) { j["templates"].erase("FloorEnter"); });
    EXPECT_NE(errors.find("narrator.json: template 'FloorEnter': missing"), std::string::npos) << errors;
}

TEST(NarratorDef, AnUnknownPlaceholderIsNamed) {
    const std::string errors =
        errorsWith([](nlohmann::json& j) { j["templates"]["BossIntro"].push_back("{boss} waits."); });
    EXPECT_NE(errors.find("narrator.json: template 'BossIntro': unknown placeholder '{boss}'"), std::string::npos)
        << errors;
}

TEST(NarratorDef, TemplatesFillTheirPlaceholders) {
    const NarratorDef def = loadNarrator(kData).value();
    Rng rng = Rng::stream(1, 0xA11CE);
    for (int i = 0; i < 20; ++i) {
        const std::string line = fillTemplate(def, "FloorEnter", {{"floor", "3"}}, rng);
        EXPECT_TRUE(line.starts_with("Floor 3.")) << line;
        EXPECT_EQ(line.find('{'), std::string::npos) << line;
    }
}

TEST(NarrationSummary, GoldenBattleBGivesItsMomentsAndAVictory) {
    // Golden scenario B: Exorcism (frozen_throne) stuns on turn 1, Shield of Faith (golden_bell)
    // reflects on turn 2, then attacks until Medusa falls.
    const Combatant hero{"Aria", 2000, 2000, 200, 200, 400, 200, 50, {}};
    const EnemyDef& medusa = shippedContent().enemy("medusa");
    Battle battle(shippedContent(), hero, medusa, {"frozen_throne", "golden_bell"}, Rng::stream(2026, 0),
                  &shippedScripts());
    BattleContext ctx = contextFor(hero, medusa);
    const GameState state = freshState();
    const GameState before = state;
    battle.start();
    std::vector<NarrationRequest> requests;
    const std::vector<PlayerAction> opening{CastSkill{"frozen_throne"}, CastSkill{"golden_bell"}};
    for (std::size_t turn = 0; !battle.over(); ++turn) {
        const auto events = battle.act(turn < opening.size() ? opening[turn] : PlayerAction{Attack{}});
        ASSERT_TRUE(events.has_value());
        if (auto r = summarizeBattleChunk(*events, ctx))
            requests.push_back(std::move(*r)); // at most one per action, by construction
    }
    ASSERT_GE(requests.size(), 3u);
    EXPECT_EQ(requests[0].kind, NarrationKind::BattleMoment);
    EXPECT_EQ(requests[0].summary["moment"], "skill");
    EXPECT_EQ(requests[0].summary["detail"], "Exorcism"); // the stun that turn is noted, not asked for
    EXPECT_EQ(requests[1].summary["detail"], "Shield of Faith");
    const NarrationRequest& end = requests.back();
    EXPECT_EQ(end.kind, NarrationKind::BattleEnd);
    EXPECT_EQ(end.summary["result"], "victory");
    const std::vector<std::string> moments = end.summary["moments"];
    ASSERT_GE(moments.size(), 4u);
    EXPECT_EQ(moments[0], "Exorcism");
    EXPECT_EQ(moments[1], "stun");
    EXPECT_EQ(moments[2], "Shield of Faith");
    EXPECT_EQ(moments[3], "reflect");
    EXPECT_EQ(state, before); // narration reads events only
}

TEST(NarrationSummary, LowHpFiresOncePerSide) {
    const Combatant hero{"Aria", 100, 100, 0, 0, 10, 0, 0, {}};
    BattleContext ctx = contextFor(hero, shippedContent().enemy("rat"));
    const std::vector<Event> first{ev::Damaged{Side::Hero, 85, 15, DamageCause::Attack}};
    const auto r = summarizeBattleChunk(first, ctx);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->summary["moment"], "low_hp");
    EXPECT_EQ(r->summary["detail"], "hero");
    EXPECT_EQ(r->summary["heroHpPct"], 15);
    const std::vector<Event> again{ev::Damaged{Side::Hero, 5, 10, DamageCause::Attack}};
    EXPECT_FALSE(summarizeBattleChunk(again, ctx).has_value());
    const std::vector<Event> foe{ev::Damaged{Side::Enemy, 190, 10, DamageCause::Attack}};
    EXPECT_EQ(summarizeBattleChunk(foe, ctx)->summary["detail"], "foe");
    EXPECT_FALSE(summarizeBattleChunk(foe, ctx).has_value());
    EXPECT_EQ(ctx.moments, (std::vector<std::string>{"low_hp:hero", "low_hp:foe"}));
}

TEST(NarrationSummary, TheEndSupersedesAMomentInTheSameAction) {
    const Combatant hero{"Aria", 100, 100, 0, 0, 10, 0, 0, {}};
    BattleContext ctx = contextFor(hero, shippedContent().enemy("rat"));
    const std::vector<Event> events{ev::SkillCast{"frozen_throne", 40},
                                    ev::Damaged{Side::Enemy, 200, 0, DamageCause::Skill},
                                    ev::Defeated{Side::Enemy}, ev::BattleEnded{Outcome::Victory, 1}};
    const auto r = summarizeBattleChunk(events, ctx);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->kind, NarrationKind::BattleEnd);
    EXPECT_EQ(r->summary["turns"], 1);
}
