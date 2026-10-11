#include "game/inventory.hpp"

#include <gtest/gtest.h>

using namespace heroes::game;

TEST(Inventory, StacksIdenticalItemsInOneSlot) {
    Inventory bag;
    ASSERT_TRUE(bag.add("topaz", 3));
    ASSERT_TRUE(bag.add("topaz", 4));
    EXPECT_EQ(bag.count("topaz"), 7);
    EXPECT_TRUE(bag.slots()[0].has_value());
    EXPECT_FALSE(bag.slots()[1].has_value());
}

TEST(Inventory, RemoveClearsEmptiedSlot) {
    Inventory bag;
    ASSERT_TRUE(bag.add("topaz", 2));
    ASSERT_TRUE(bag.remove("topaz", 2));
    EXPECT_EQ(bag.count("topaz"), 0);
    EXPECT_FALSE(bag.slots()[0].has_value());
}

TEST(Inventory, RemoveTooManyFailsAndChangesNothing) {
    Inventory bag;
    ASSERT_TRUE(bag.add("topaz", 2));
    const Inventory before = bag;
    EXPECT_EQ(bag.remove("topaz", 3).error(), TownError::ItemNotInBag);
    EXPECT_EQ(bag.remove("gold_leaf", 1).error(), TownError::ItemNotInBag);
    EXPECT_EQ(bag, before);
}

TEST(Inventory, InvalidQuantityIsRejected) {
    Inventory bag;
    EXPECT_EQ(bag.add("topaz", 0).error(), TownError::InvalidQuantity);
    EXPECT_EQ(bag.remove("topaz", 0).error(), TownError::InvalidQuantity);
}

TEST(Inventory, FullBagRejectsNewItemsButStillStacks) {
    Inventory bag;
    for (int i = 0; i < Inventory::kSlots; ++i)
        ASSERT_TRUE(bag.add("item" + std::to_string(i), 1));
    EXPECT_FALSE(bag.canAdd("extra"));
    EXPECT_TRUE(bag.canAdd("item3"));
    EXPECT_EQ(bag.add("extra", 1).error(), TownError::BagFull);
    EXPECT_TRUE(bag.add("item3", 5));
    EXPECT_EQ(bag.count("item3"), 6);
}
