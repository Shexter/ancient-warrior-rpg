#include "game/town.hpp"

#include "state_helpers.hpp"

#include <gtest/gtest.h>

using namespace heroes::game;

namespace {

const Content& content() { return shippedContent(); }

// Runs `act` on a copy-checked state and asserts it fails with `expected` leaving state equal.
template <class F> void expectRefused(GameState state, TownError expected, F act) {
    const GameState before = state;
    const auto result = act(state);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), expected) << describe(result.error());
    EXPECT_EQ(state, before);
}

} // namespace

TEST(Merchant, BuyDeductsGoldAndFillsBag) {
    GameState state = freshState();
    ASSERT_TRUE(buy(state, content(), "health_potion", 2));
    EXPECT_EQ(state.hero.gold, 18000);
    EXPECT_EQ(state.hero.bag.count("health_potion"), 2);
}

TEST(Merchant, BuyRefusals) {
    const auto doBuy = [](std::string_view id, int qty) {
        return [=](GameState& s) { return buy(s, content(), id, qty); };
    };
    GameState dead = freshState();
    dead.hero.hp = 0;
    expectRefused(dead, TownError::HeroDead, doBuy("health_potion", 1));
    expectRefused(freshState(), TownError::InvalidQuantity, doBuy("health_potion", 0));
    expectRefused(freshState(), TownError::InvalidQuantity, doBuy("health_potion", 100));
    expectRefused(freshState(), TownError::UnknownItem, doBuy("nope", 1));
    expectRefused(freshState(), TownError::NotTradeable, doBuy("topaz", 1));
    GameState poor = freshState();
    poor.hero.gold = 500;
    expectRefused(poor, TownError::NotEnoughGold, doBuy("health_potion", 1));
    GameState full = freshState();
    fillWithout(full.hero.bag, "health_potion", 10);
    expectRefused(full, TownError::BagFull, doBuy("health_potion", 1));
}

TEST(Merchant, BuyStacksIntoFullBag) {
    GameState state = freshState();
    fillWithout(state.hero.bag, "health_potion", 9);
    ASSERT_TRUE(state.hero.bag.add("health_potion", 1));
    ASSERT_TRUE(buy(state, content(), "health_potion", 1));
    EXPECT_EQ(state.hero.bag.count("health_potion"), 2);
}

TEST(Merchant, SellPaysFullPrice) {
    GameState state = freshState();
    ASSERT_TRUE(state.hero.bag.add("iron_sword", 3));
    ASSERT_TRUE(sell(state, content(), "iron_sword", 2));
    EXPECT_EQ(state.hero.gold, 22000);
    EXPECT_EQ(state.hero.bag.count("iron_sword"), 1);
}

TEST(Merchant, SellRefusals) {
    const auto doSell = [](std::string_view id, int qty) {
        return [=](GameState& s) { return sell(s, content(), id, qty); };
    };
    GameState dead = freshState();
    dead.hero.hp = 0;
    ASSERT_TRUE(dead.hero.bag.add("iron_sword", 1));
    expectRefused(dead, TownError::HeroDead, doSell("iron_sword", 1));
    GameState holder = freshState();
    ASSERT_TRUE(holder.hero.bag.add("iron_sword", 1));
    ASSERT_TRUE(holder.hero.bag.add("topaz", 1));
    expectRefused(holder, TownError::InvalidQuantity, doSell("iron_sword", 0));
    expectRefused(holder, TownError::UnknownItem, doSell("nope", 1));
    expectRefused(holder, TownError::NotTradeable, doSell("topaz", 1));
    expectRefused(holder, TownError::ItemNotInBag, doSell("iron_sword", 2));
    expectRefused(freshState(), TownError::ItemNotInBag, doSell("steel_shield", 1));
}

TEST(Merchant, AuctionVectors) {
    struct Case {
        std::uint64_t seed;
        bool sold;
        int bids;
        std::int64_t payout;
    };
    for (const Case& c : {Case{1, true, 0, 450}, Case{2, true, 1, 495}, Case{3, true, 0, 450},
                          Case{4, false, 0, 0}, Case{5, true, 0, 450}}) {
        Rng rng = Rng::stream(c.seed, 0);
        const AuctionResult r = rollAuction(rng, 500);
        EXPECT_EQ(r.sold, c.sold) << c.seed;
        EXPECT_EQ(r.bids, c.bids) << c.seed;
        EXPECT_EQ(r.payout, c.payout) << c.seed;
    }
}

TEST(Merchant, AuctionPayoutStaysInSpecRange) {
    for (std::uint64_t seed = 0; seed < 2000; ++seed) {
        Rng rng = Rng::stream(seed, 0);
        const AuctionResult r = rollAuction(rng, 500);
        if (r.sold) {
            EXPECT_GE(r.payout, 450);
            EXPECT_LE(r.payout, 855);
        } else {
            EXPECT_EQ(r.payout, 0);
        }
    }
}

TEST(Merchant, AuctionSoldOrKeptMatchesRoll) {
    for (std::uint64_t seed = 1; seed <= 5; ++seed) {
        GameState state = freshState("fighter", seed);
        ASSERT_TRUE(state.hero.bag.add("topaz", 2));
        Rng forked = state.rng;
        Rng roll = forked.fork();
        const AuctionResult expected = rollAuction(roll, 500);
        const Rng masterAfter = forked;

        const auto result = auction(state, content(), "topaz");
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->sold, expected.sold);
        EXPECT_EQ(result->payout, expected.payout);
        EXPECT_EQ(state.hero.bag.count("topaz"), expected.sold ? 1 : 2);
        EXPECT_EQ(state.hero.gold, 20000 + expected.payout);
        EXPECT_EQ(state.rng, masterAfter);
    }
}

TEST(Merchant, AuctionRefusals) {
    const auto doAuction = [](std::string_view id) {
        return [=](GameState& s) { return auction(s, content(), id); };
    };
    GameState dead = freshState();
    dead.hero.hp = 0;
    ASSERT_TRUE(dead.hero.bag.add("topaz", 1));
    expectRefused(dead, TownError::HeroDead, doAuction("topaz"));
    GameState holder = freshState();
    ASSERT_TRUE(holder.hero.bag.add("iron_sword", 1));
    expectRefused(holder, TownError::NotAuctionable, doAuction("iron_sword"));
    expectRefused(holder, TownError::UnknownItem, doAuction("nope"));
    expectRefused(holder, TownError::ItemNotInBag, doAuction("topaz"));
}

TEST(Bank, OpenCostsGoldOnce) {
    GameState state = freshState();
    ASSERT_TRUE(openBank(state));
    EXPECT_EQ(state.hero.gold, 10000);
    ASSERT_TRUE(state.hero.bank.has_value());
    expectRefused(state, TownError::AlreadyHasBank, [](GameState& s) { return openBank(s); });
}

TEST(Bank, CannotOpenWithoutGold) {
    GameState poor = freshState();
    poor.hero.gold = 5000;
    expectRefused(poor, TownError::NotEnoughGold, [](GameState& s) { return openBank(s); });
}

TEST(Bank, DepositAndWithdrawRoundTrip) {
    GameState state = freshState();
    ASSERT_TRUE(openBank(state));
    ASSERT_TRUE(state.hero.bag.add("topaz", 3));
    ASSERT_TRUE(deposit(state, "topaz", 3));
    EXPECT_EQ(state.hero.bag.count("topaz"), 0);
    EXPECT_EQ(state.hero.bank->count("topaz"), 3);
    ASSERT_TRUE(withdraw(state, "topaz", 3));
    EXPECT_EQ(state.hero.bag.count("topaz"), 3);
    EXPECT_EQ(state.hero.bank->count("topaz"), 0);
}

TEST(Bank, Refusals) {
    expectRefused(freshState(), TownError::NoBankAccount,
                  [](GameState& s) { return deposit(s, "topaz", 1); });
    expectRefused(freshState(), TownError::NoBankAccount,
                  [](GameState& s) { return withdraw(s, "topaz", 1); });

    GameState state = freshState();
    ASSERT_TRUE(openBank(state));
    ASSERT_TRUE(state.hero.bag.add("topaz", 2));
    expectRefused(state, TownError::InvalidQuantity,
                  [](GameState& s) { return deposit(s, "topaz", 0); });
    expectRefused(state, TownError::ItemNotInBag,
                  [](GameState& s) { return deposit(s, "topaz", 3); });
    expectRefused(state, TownError::ItemNotInBag,
                  [](GameState& s) { return withdraw(s, "topaz", 1); });

    GameState bankFull = state;
    fillWithout(*bankFull.hero.bank, "topaz", 10);
    expectRefused(bankFull, TownError::BankFull,
                  [](GameState& s) { return deposit(s, "topaz", 1); });

    GameState bagFull = freshState();
    ASSERT_TRUE(openBank(bagFull));
    ASSERT_TRUE(bagFull.hero.bank->add("topaz", 1));
    fillWithout(bagFull.hero.bag, "topaz", 10);
    expectRefused(bagFull, TownError::BagFull,
                  [](GameState& s) { return withdraw(s, "topaz", 1); });
}

TEST(Bank, WorksForDeadHero) {
    GameState state = freshState();
    ASSERT_TRUE(openBank(state));
    state.hero.hp = 0;
    ASSERT_TRUE(state.hero.bag.add("topaz", 1));
    EXPECT_TRUE(deposit(state, "topaz", 1).has_value());
}
