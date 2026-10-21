#include "game/training.hpp"

#include "game/battle.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace heroes::game {

namespace {

constexpr int kMaxRounds = 100;
constexpr int kExchangeCap = 1000;
constexpr std::array<std::string_view, 4> kTierNames{"Fox", "Bat", "Horse", "Bull"};

int roll(Rng& rng, int base, int spread) {
    return base + static_cast<int>(rng.below(static_cast<std::uint32_t>(spread)));
}

} // namespace

Outcome fightTrainingMonster(Combatant& hero, const TrainingMonster& monster,
                             std::vector<Event>& events) {
    int monsterHp = monster.maxHp;
    for (int exchange = 1; exchange <= kExchangeCap; ++exchange) {
        const int heroHit = defendedDamage(hero.atk, monster.def);
        monsterHp = std::max(0, monsterHp - heroHit);
        events.push_back(ev::Damaged{Side::Enemy, heroHit, monsterHp, DamageCause::Attack});
        if (monsterHp == 0)
            return Outcome::Victory;

        const int monsterHit = defendedDamage(monster.atk, hero.def);
        hero.hp = std::max(0, hero.hp - monsterHit);
        events.push_back(ev::Damaged{Side::Hero, monsterHit, hero.hp, DamageCause::Attack});
        if (hero.hp == 0)
            return Outcome::Defeat;

        const int healed = std::min(hero.life, hero.maxHp - hero.hp);
        if (healed > 0) {
            hero.hp += healed;
            events.push_back(ev::Healed{Side::Hero, healed, hero.hp});
        }
    }
    return Outcome::Draw;
}

namespace {

// One round, applied to the hero: rewards and a drop on a win, the EXP penalty on a defeat.
TrainingRound playRound(Hero& hero, const Content& content, Rng& rng, int caveLevel) {
    const TrainingMonster monster = rollTrainingMonster(rng, caveLevel);
    TrainingRound round;
    round.monsterName = monster.name;
    round.caveLevel = caveLevel;
    round.monsterMaxHp = monster.maxHp;
    Combatant fighter = heroCombatant(hero, content);
    round.outcome = fightTrainingMonster(fighter, monster, round.events);
    hero.hp = fighter.hp;

    if (round.outcome == Outcome::Victory) {
        round.goldGained = monster.goldReward;
        round.expGained = monster.expReward;
        hero.gold += monster.goldReward;
        round.levelsGained = grantExp(hero, content, monster.expReward);
        round.drop = rollDrop(rng, content, caveLevel);
        if (round.drop) {
            if (hero.bag.canAdd(*round.drop))
                (void)hero.bag.add(*round.drop, 1);
            else
                round.dropLostBagFull = true;
        }
    } else if (round.outcome == Outcome::Defeat) {
        hero.exp = std::max<std::int64_t>(0, hero.exp - monster.expReward);
    }
    return round;
}

std::expected<void, TownError> checkStart(const Hero& hero, int caveLevel) {
    if (!canAdventure(hero))
        return std::unexpected(hero.hp <= 0 ? TownError::HeroDead : TownError::NotEquipped);
    if (caveLevel < 1 || caveLevel > TrainingSession::kMaxCaveLevel)
        return std::unexpected(TownError::InvalidQuantity);
    return {};
}

} // namespace

TrainingMonster rollTrainingMonster(Rng& rng, int caveLevel) {
    const int atk = roll(rng, 15, 5) * 2 * caveLevel;
    const int def = roll(rng, 5, 5) * caveLevel;
    const std::int64_t gold = static_cast<std::int64_t>(roll(rng, 10, 10)) * 10 * caveLevel;
    const auto tier = static_cast<std::size_t>(caveLevel / 10);
    return {std::string(kTierNames[tier]), 100 * caveLevel, atk, def, gold, 100 * caveLevel};
}

std::optional<std::string> rollDrop(Rng& rng, const Content& content, int caveLevel) {
    std::vector<const ItemDef*> candidates;
    for (const ItemDef& item : content.items())
        if (item.dropRate > 0)
            candidates.push_back(&item);
    if (candidates.empty())
        return std::nullopt;

    const ItemDef& item = *candidates[rng.below(static_cast<std::uint32_t>(candidates.size()))];
    const auto chance = static_cast<std::uint32_t>(std::min(100, item.dropRate + caveLevel));
    if (rng.below(100) < chance)
        return item.id;
    return std::nullopt;
}

std::expected<std::vector<TrainingRound>, TownError>
runTraining(GameState& state, const Content& content, int caveLevel, int rounds) {
    if (auto ok = checkStart(state.hero, caveLevel); !ok)
        return std::unexpected(ok.error());
    if (rounds < 1 || rounds > kMaxRounds)
        return std::unexpected(TownError::InvalidQuantity);

    Rng rng = state.rng.fork();
    std::vector<TrainingRound> results;
    for (int i = 0; i < rounds; ++i) {
        TrainingRound round = playRound(state.hero, content, rng, caveLevel);
        const bool died = round.outcome == Outcome::Defeat;
        results.push_back(std::move(round));
        if (died)
            break;
    }
    return results;
}

TrainingSession::TrainingSession(GameState& state, const Content& content, int caveLevel)
    : state_(&state), content_(&content), caveLevel_(caveLevel), rng_(state.rng.fork()) {}

std::expected<TrainingSession, TownError> TrainingSession::start(GameState& state,
                                                                 const Content& content,
                                                                 int caveLevel) {
    if (auto ok = checkStart(state.hero, caveLevel); !ok)
        return std::unexpected(ok.error());
    return TrainingSession(state, content, caveLevel);
}

TrainingRound TrainingSession::next() {
    TrainingRound round = playRound(state_->hero, *content_, rng_, caveLevel_);
    switch (round.outcome) {
    case Outcome::Victory:
        round.potGained = round.goldGained * std::min(streak_, 10) / 10;
        pot_ += round.potGained;
        ++streak_;
        break;
    case Outcome::Defeat:
        pot_ = 0;
        streak_ = 0;
        over_ = true;
        break;
    default:
        streak_ = 0;
        break;
    }
    return round;
}

std::int64_t TrainingSession::leave() {
    const std::int64_t banked = over_ ? 0 : pot_;
    state_->hero.gold += banked;
    pot_ = 0;
    over_ = true;
    return banked;
}

} // namespace heroes::game
