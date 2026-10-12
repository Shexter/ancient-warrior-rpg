#pragma once

#include "game/content.hpp"
#include "game/errors.hpp"
#include "game/inventory.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace heroes::game {

// Largest gold/exp a save may hold. Unreachable in play (it would take ~1e9 actions) yet small
// enough that every gold and exp formula stays inside int64.
inline constexpr std::int64_t kMaxCurrency = 1'000'000'000'000'000;

struct Equipped {
    std::string itemId;
    int upgradeLevel = 1; // 1..10
    bool operator==(const Equipped&) const = default;
};

struct Hero {
    std::string name;
    std::string pathId;
    int level = 1;
    std::int64_t expToNext = 0;
    std::int64_t exp = 0; // spendable currency; NOT level progress
    std::int64_t gold = 20000;
    int hp = 0, mp = 0, maxHp = 0, maxMp = 0, atk = 0, def = 0, life = 0; // base stats
    std::optional<Equipped> weapon, armor;
    Inventory bag;
    std::optional<Inventory> bank;
    std::set<std::string> learnedSkills;
    std::set<std::string> completedMissions;
    int forgeLevel = 9;
    int blessings = 0; // unspent points, one per level gained

    bool operator==(const Hero&) const = default;
};

// Trims spaces; accepts 1-8 characters in 0x20-0x7E.
std::expected<std::string, TownError> validateHeroName(std::string_view raw);

// Adds to spendable exp and advances the level, granting one blessing point per level.
// Returns levels gained.
int grantExp(Hero& hero, const Content& content, std::int64_t amount);

// Sum of item.bonus x upgradeLevel over weapon and armor.
StatBonus equipmentBonus(const Hero& hero, const Content& content);

bool canAdventure(const Hero& hero);

} // namespace heroes::game
