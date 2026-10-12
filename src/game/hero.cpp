#include "game/hero.hpp"

#include "game/game_state.hpp"

#include <algorithm>

namespace heroes::game {

namespace {

constexpr std::string_view kStartWeapon = "iron_sword";
constexpr std::string_view kStartArmor = "steel_shield";

bool printable(char c) { return c >= 0x20 && c <= 0x7E; }

} // namespace

std::expected<std::string, TownError> validateHeroName(std::string_view raw) {
    const auto first = raw.find_first_not_of(' ');
    if (first == std::string_view::npos)
        return std::unexpected(TownError::InvalidName);
    const auto last = raw.find_last_not_of(' ');
    const std::string_view trimmed = raw.substr(first, last - first + 1);
    if (trimmed.size() > 8 || !std::ranges::all_of(trimmed, printable))
        return std::unexpected(TownError::InvalidName);
    return std::string(trimmed);
}

GameState newGame(const Content& content, std::string name, std::string_view pathId,
                  std::uint64_t seed) {
    const PathDef& path = content.path(pathId);
    Hero hero;
    hero.name = std::move(name);
    hero.pathId = path.id;
    hero.expToNext = 100 * static_cast<std::int64_t>(path.multiplier);
    hero.hp = hero.maxHp = path.maxHp;
    hero.mp = hero.maxMp = path.maxMp;
    hero.atk = path.atk;
    hero.def = path.def;
    hero.life = path.life;
    hero.weapon = Equipped{std::string(kStartWeapon), 1};
    hero.armor = Equipped{std::string(kStartArmor), 1};
    return GameState{std::move(hero), Rng::stream(seed, 0), seed};
}

int grantExp(Hero& hero, const Content& content, std::int64_t amount) {
    if (amount <= 0)
        return 0;
    const int multiplier = content.path(hero.pathId).multiplier;
    hero.exp += amount;
    int levelsGained = 0;
    std::int64_t remaining = amount;
    while (remaining > 0) {
        if (remaining >= hero.expToNext) {
            remaining -= hero.expToNext;
            ++hero.level;
            ++levelsGained;
            ++hero.blessings;
            hero.expToNext = static_cast<std::int64_t>(hero.level) * 100 * multiplier;
        } else {
            hero.expToNext -= remaining;
            remaining = 0;
        }
    }
    return levelsGained;
}

StatBonus equipmentBonus(const Hero& hero, const Content& content) {
    StatBonus total;
    for (const std::optional<Equipped>* slot : {&hero.weapon, &hero.armor}) {
        if (!*slot)
            continue;
        const StatBonus& b = content.item((*slot)->itemId).bonus;
        total.atk += b.atk * (*slot)->upgradeLevel;
        total.def += b.def * (*slot)->upgradeLevel;
        total.life += b.life * (*slot)->upgradeLevel;
    }
    return total;
}

bool canAdventure(const Hero& hero) { return hero.weapon && hero.armor && hero.hp > 0; }

} // namespace heroes::game
