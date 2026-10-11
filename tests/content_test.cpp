#include "game/content_loader.hpp"

#include <gtest/gtest.h>

#include <algorithm>

using namespace heroes::game;

namespace {

const Content& shipped() {
    static const Content content = [] {
        auto loaded = loadContent(std::filesystem::path(HEROES_SOURCE_DIR) / "data");
        EXPECT_TRUE(loaded.has_value()) << (loaded ? "" : formatError(loaded.error().front()));
        return std::move(*loaded);
    }();
    return content;
}

} // namespace

TEST(Content, ShippedDataLoads) { EXPECT_EQ(shipped().paths().size(), 3u); }

TEST(Content, FighterStats) {
    const PathDef& p = shipped().path("fighter");
    EXPECT_EQ(p.maxHp, 350);
    EXPECT_EQ(p.maxMp, 50);
    EXPECT_EQ(p.atk, 50);
    EXPECT_EQ(p.def, 5);
    EXPECT_EQ(p.life, 0);
    EXPECT_EQ(p.multiplier, 1);
}

TEST(Content, IronSwordAtk) { EXPECT_EQ(shipped().item("iron_sword").bonus.atk, 50); }

TEST(Content, ItemCountAndOrder) {
    ASSERT_EQ(shipped().items().size(), 20u);
    EXPECT_EQ(shipped().items().front().id, "iron_sword");
    EXPECT_EQ(shipped().items().back().id, "eternal_youth");
}

TEST(Content, DropCandidatesNumberSixteen) {
    auto items = shipped().items();
    EXPECT_EQ(std::ranges::count_if(items, [](const ItemDef& i) { return i.dropRate > 0; }), 16);
}

TEST(Content, LookupsAndReferences) {
    EXPECT_EQ(shipped().findItem("no_such"), nullptr);
    EXPECT_EQ(shipped().item("health_potion").consumable->restoreHp, 100);
    EXPECT_TRUE(shipped().item("eternal_youth").consumable->fullRestore);
    EXPECT_EQ(shipped().enemy("medusa").rewardItems, std::vector<std::string>{"thors_hammer"});
    EXPECT_EQ(shipped().skill("frozen_throne").effect.targetStatuses.size(), 2u);
    EXPECT_EQ(shipped().story().opening.size(), 7u);
    EXPECT_EQ(shipped().story().pathIntro.size(), 4u);
}

TEST(Content, MissingDirectoryReportsFileErrors) {
    auto loaded = loadContent("no/such/dir");
    ASSERT_FALSE(loaded.has_value());
    EXPECT_EQ(formatError(loaded.error().front()), "items.json: cannot open file");
}
