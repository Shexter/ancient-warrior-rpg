#include "shipped_content.hpp"

#include <gtest/gtest.h>

#include <algorithm>

using namespace heroes::game;

TEST(Content, ShippedDataLoads) { EXPECT_EQ(shippedContent().paths().size(), 3u); }

TEST(Content, FighterStats) {
    const PathDef& p = shippedContent().path("fighter");
    EXPECT_EQ(p.maxHp, 350);
    EXPECT_EQ(p.maxMp, 50);
    EXPECT_EQ(p.atk, 50);
    EXPECT_EQ(p.def, 5);
    EXPECT_EQ(p.life, 0);
    EXPECT_EQ(p.multiplier, 1);
}

TEST(Content, IronSwordAtk) { EXPECT_EQ(shippedContent().item("iron_sword").bonus.atk, 50); }

TEST(Content, ItemCountAndOrder) {
    ASSERT_EQ(shippedContent().items().size(), 20u);
    EXPECT_EQ(shippedContent().items().front().id, "iron_sword");
    EXPECT_EQ(shippedContent().items().back().id, "eternal_youth");
}

TEST(Content, DropCandidatesNumberSixteen) {
    auto items = shippedContent().items();
    EXPECT_EQ(std::ranges::count_if(items, [](const ItemDef& i) { return i.dropRate > 0; }), 16);
}

TEST(Content, LookupsAndReferences) {
    EXPECT_EQ(shippedContent().findItem("no_such"), nullptr);
    EXPECT_EQ(shippedContent().item("health_potion").consumable->restoreHp, 100);
    EXPECT_TRUE(shippedContent().item("eternal_youth").consumable->fullRestore);
    EXPECT_EQ(shippedContent().enemy("medusa").rewardItems,
              std::vector<std::string>{"thors_hammer"});
    EXPECT_EQ(shippedContent().skill("frozen_throne").effect.targetStatuses.size(), 2u);
    EXPECT_EQ(shippedContent().story().opening.size(), 7u);
    EXPECT_EQ(shippedContent().story().pathIntro.size(), 4u);
}

TEST(Content, MissingDirectoryReportsFileErrors) {
    auto loaded = loadContent("no/such/dir");
    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(formatError(loaded.error().front()), "items.json: cannot open file");
}
