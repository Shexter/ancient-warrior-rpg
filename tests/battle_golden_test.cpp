#include "battle_golden_data.hpp"
#include "event_format.hpp"
#include "game/battle.hpp"
#include "shipped_content.hpp"

#include <gtest/gtest.h>

#include <sstream>

using namespace heroes::game;

namespace {

std::vector<std::string> splitLines(std::string_view text) {
    std::vector<std::string> lines;
    std::istringstream in{std::string(text)};
    for (std::string line; std::getline(in, line);)
        if (!line.empty())
            lines.push_back(line);
    return lines;
}

// Plays the listed skills, then Attack until the battle ends or `limit` events exist.
std::vector<std::string> play(Combatant hero, std::uint64_t seed, std::vector<std::string> skills,
                              std::size_t limit = SIZE_MAX) {
    const std::set<std::string> learned(skills.begin(), skills.end());
    Battle battle(shippedContent(), std::move(hero), shippedContent().enemy("medusa"), learned,
                  Rng::stream(seed, 0));
    std::vector<std::string> lines;
    std::size_t next = 0;
    while (!battle.over() && lines.size() < limit) {
        PlayerAction action = Attack{};
        if (next < skills.size())
            action = CastSkill{skills[next++]};
        auto events = battle.act(action);
        EXPECT_TRUE(events.has_value());
        for (const Event& e : *events)
            lines.push_back(toString(e));
    }
    if (lines.size() > limit)
        lines.resize(limit);
    return lines;
}

} // namespace

TEST(BattleGolden, A_StartingFighterLosesToMedusa) {
    EXPECT_EQ(play({"Aria", 350, 350, 50, 50, 100, 25, 0, {}}, 1, {}), splitLines(kGoldenA));
}

TEST(BattleGolden, B_FrozenThroneThenGoldenBell) {
    EXPECT_EQ(play({"Aria", 2000, 2000, 200, 200, 400, 200, 50, {}}, 2026,
                   {"frozen_throne", "golden_bell"}),
              splitLines(kGoldenB));
}

TEST(BattleGolden, C_GoldenBellReflect) {
    EXPECT_EQ(play({"Aria", 2000, 2000, 200, 200, 400, 200, 50, {}}, 2026, {"golden_bell"}, 26),
              splitLines(kGoldenC));
}

TEST(BattleGolden, D_SupernovaOneShot) {
    EXPECT_EQ(
        play({"Aria", 900, 900, 100, 100, 300, 100, 0, {}}, 99, {"supernova", "meteor_fist"}, 14),
        splitLines(kGoldenD));
}
