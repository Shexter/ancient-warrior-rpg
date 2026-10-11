#pragma once

#include <cstdint>

namespace heroes::game {

// SplitMix64 step; used to derive stream seeds. Exposed so tests can check its golden outputs.
std::uint64_t splitmix64(std::uint64_t& s);

// PCG32 (XSH-RR) seeded through SplitMix64. Bit-exact: saves and the simulator depend on it.
class Rng {
  public:
    struct State {
        std::uint64_t state;
        std::uint64_t inc;
        bool operator==(const State&) const = default;
    };

    static Rng stream(std::uint64_t baseSeed, std::uint64_t index);
    static Rng fromState(State s);
    State state() const { return {state_, inc_}; }

    std::uint32_t next();
    std::uint32_t below(std::uint32_t bound); // uniform in [0, bound); bound must be > 0
    bool percent(std::uint32_t p) { return below(100) < p; }
    Rng fork();

    bool operator==(const Rng&) const = default;

  private:
    Rng(std::uint64_t state, std::uint64_t inc) : state_(state), inc_(inc) {}
    void seed(std::uint64_t initState, std::uint64_t initSeq);

    std::uint64_t state_;
    std::uint64_t inc_;
};

} // namespace heroes::game
