#include "game/rng.hpp"

namespace heroes::game {

std::uint64_t splitmix64(std::uint64_t& s) {
    s += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = s;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

Rng Rng::stream(std::uint64_t baseSeed, std::uint64_t index) {
    std::uint64_t s = baseSeed ^ index;
    const std::uint64_t a = splitmix64(s);
    const std::uint64_t b = splitmix64(s);
    Rng rng(0, 0);
    rng.seed(a, b);
    return rng;
}

Rng Rng::fromState(State s) { return Rng(s.state, s.inc); }

void Rng::seed(std::uint64_t initState, std::uint64_t initSeq) {
    state_ = 0;
    inc_ = (initSeq << 1) | 1;
    next();
    state_ += initState;
    next();
}

std::uint32_t Rng::next() {
    const std::uint64_t old = state_;
    state_ = old * 6364136223846793005ULL + inc_;
    const auto x = static_cast<std::uint32_t>(((old >> 18) ^ old) >> 27);
    const auto r = static_cast<std::uint32_t>(old >> 59);
    return (x >> r) | (x << ((0u - r) & 31));
}

std::uint32_t Rng::below(std::uint32_t bound) {
    std::uint64_t m = static_cast<std::uint64_t>(next()) * bound;
    auto low = static_cast<std::uint32_t>(m);
    if (low < bound) {
        const std::uint32_t threshold = (0u - bound) % bound;
        while (low < threshold) {
            m = static_cast<std::uint64_t>(next()) * bound;
            low = static_cast<std::uint32_t>(m);
        }
    }
    return static_cast<std::uint32_t>(m >> 32);
}

Rng Rng::fork() {
    const std::uint64_t hi = next();
    const std::uint64_t lo = next();
    return stream((hi << 32) | lo, 0);
}

} // namespace heroes::game
