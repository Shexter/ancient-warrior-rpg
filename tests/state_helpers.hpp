#pragma once

#include "game/game_state.hpp"
#include "shipped_content.hpp"

#include <initializer_list>
#include <string_view>

namespace heroes::game {

inline GameState freshState(std::string_view pathId = "fighter", std::uint64_t seed = 1) {
    return newGame(shippedContent(), "Aria", pathId, seed);
}

inline void addAll(Inventory& inv, std::initializer_list<std::string_view> ids, int qty = 1) {
    for (std::string_view id : ids)
        EXPECT_TRUE(inv.add(id, qty).has_value()) << id;
}

// Distinct non-stackable fillers: ten different ids occupy all ten slots.
inline constexpr std::string_view kFillers[] = {
    "topaz",     "gold_leaf", "frozen_tears", "broken_staff",   "pure_ice",   "turtle_shell",
    "gold_dust", "fire_dust", "black_pearl",  "broken_compass", "iron_sword", "steel_shield"};

inline void fillWithout(Inventory& inv, std::string_view skip, int slots) {
    for (std::string_view id : kFillers) {
        if (slots == 0)
            break;
        if (id == skip)
            continue;
        EXPECT_TRUE(inv.add(id, 1).has_value());
        --slots;
    }
}

// What the master stream looks like after exactly one fork.
inline Rng afterOneFork(Rng rng) {
    rng.fork();
    return rng;
}

} // namespace heroes::game
