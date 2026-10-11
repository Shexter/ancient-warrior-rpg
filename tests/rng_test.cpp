#include "game/rng.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

using heroes::game::Rng;

namespace {

std::vector<std::uint32_t> nextN(Rng& rng, int n) {
    std::vector<std::uint32_t> out;
    for (int i = 0; i < n; ++i)
        out.push_back(rng.next());
    return out;
}

std::vector<std::uint32_t> belowN(Rng& rng, std::uint32_t bound, int n) {
    std::vector<std::uint32_t> out;
    for (int i = 0; i < n; ++i)
        out.push_back(rng.below(bound));
    return out;
}

using V = std::vector<std::uint32_t>;

} // namespace

TEST(Rng, SplitMix64FromZero) {
    std::uint64_t s = 0;
    EXPECT_EQ(heroes::game::splitmix64(s), 0xe220a8397b1dcdafULL);
    EXPECT_EQ(heroes::game::splitmix64(s), 0x6e789e6aa1b965f4ULL);
    EXPECT_EQ(heroes::game::splitmix64(s), 0x06c45d188009454fULL);
}

// PCG32 seed(42, 54) by hand: inc = (54 << 1) | 1; the first next() sets state = inc, then
// state += 42 and a second next() advances once more.
TEST(Rng, Pcg32PublishedSeed42Seq54) {
    constexpr std::uint64_t kInc = (54ULL << 1) | 1;
    constexpr std::uint64_t kState = (kInc + 42) * 6364136223846793005ULL + kInc;
    auto rng = Rng::fromState({kState, kInc});
    EXPECT_EQ(nextN(rng, 6),
              (V{0xa15c02b7, 0x7b47f409, 0xba1d3330, 0x83d2f293, 0xbfa4784b, 0xcbed606e}));
}

TEST(Rng, StreamNext) {
    auto rng = Rng::stream(42, 0);
    EXPECT_EQ(nextN(rng, 5), (V{0xd11dd51f, 0xa9b04c45, 0xb5d97aa9, 0xa9eab6ce, 0xf63fd201}));
}

TEST(Rng, StreamBelow100) {
    auto rng = Rng::stream(42, 0);
    EXPECT_EQ(belowN(rng, 100, 10), (V{81, 66, 71, 66, 96, 61, 84, 94, 98, 53}));
}

TEST(Rng, StreamIndexBelow10) {
    auto rng = Rng::stream(42, 1);
    EXPECT_EQ(belowN(rng, 10, 10), (V{3, 2, 7, 9, 9, 3, 6, 2, 0, 5}));
}

TEST(Rng, StreamBelow6) {
    auto rng = Rng::stream(7, 123456);
    EXPECT_EQ(belowN(rng, 6, 10), (V{0, 0, 1, 4, 1, 1, 1, 2, 3, 3}));
}

TEST(Rng, BelowOneIsAlwaysZero) {
    auto rng = Rng::stream(1, 0);
    for (int i = 0; i < 1000; ++i)
        ASSERT_EQ(rng.below(1), 0u);
}

TEST(Rng, StateRoundTrips) {
    auto rng = Rng::stream(5, 5);
    rng.next();
    auto copy = Rng::fromState(rng.state());
    EXPECT_EQ(copy, rng);
    EXPECT_EQ(copy.next(), rng.next());
}

TEST(Rng, ForkAdvancesMasterByExactlyTwoDraws) {
    auto master = Rng::stream(9, 0);
    auto reference = master;
    reference.next();
    reference.next();
    auto child = master.fork();
    EXPECT_EQ(master, reference);
    EXPECT_NE(child.state(), master.state());
}
