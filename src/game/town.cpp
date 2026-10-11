#include "game/town.hpp"

#include <algorithm>
#include <cassert>

namespace heroes::game {

namespace {

constexpr int kMaxBuyQty = 99;
constexpr int kMaxUpgradeLevel = 10;
constexpr std::int64_t kBankCost = 10000;
constexpr std::int64_t kRenameCost = 50000;
constexpr std::int64_t kReviveCost = 100;
constexpr int kReviveHp = 100;
constexpr int kCharismaMinLevel = 10;
constexpr std::int64_t kCharismaExpPerPoint = 50;
constexpr std::int64_t kCharismaGoldPerPoint = 100;
constexpr int kLearnMinLevel = 5;

std::unexpected<TownError> fail(TownError e) { return std::unexpected(e); }

std::optional<Equipped>& slotOf(Hero& hero, EquipSlot slot) {
    return slot == EquipSlot::Weapon ? hero.weapon : hero.armor;
}

} // namespace

AuctionResult rollAuction(Rng& rng, std::int64_t auctionValue) {
    const auto rounds = static_cast<int>(rng.below(10));
    if (rounds == 0)
        return {false, 0, 0};
    std::int64_t price = auctionValue;
    int bids = 0;
    for (int i = 0; i < rounds; ++i) {
        if (rng.below(3) == 0)
            break;
        price += auctionValue / 10;
        ++bids;
    }
    return {true, bids, price * 9 / 10};
}

bool rollForge(Rng& rng, int forgeLevel) {
    return rng.below(100) < static_cast<std::uint32_t>(forgeLevel * 10);
}

// ---- Merchant ----------------------------------------------------------------------------

std::expected<void, TownError> buy(GameState& state, const Content& content, std::string_view id,
                                   int qty) {
    Hero& hero = state.hero;
    if (hero.hp <= 0)
        return fail(TownError::HeroDead);
    if (qty < 1 || qty > kMaxBuyQty)
        return fail(TownError::InvalidQuantity);
    const ItemDef* item = content.findItem(id);
    if (!item)
        return fail(TownError::UnknownItem);
    if (item->price <= 0)
        return fail(TownError::NotTradeable);
    const std::int64_t total = item->price * qty;
    if (hero.gold < total)
        return fail(TownError::NotEnoughGold);
    if (!hero.bag.canAdd(id))
        return fail(TownError::BagFull);

    hero.gold -= total;
    (void)hero.bag.add(id, qty);
    return {};
}

std::expected<void, TownError> sell(GameState& state, const Content& content, std::string_view id,
                                    int qty) {
    Hero& hero = state.hero;
    if (hero.hp <= 0)
        return fail(TownError::HeroDead);
    if (qty < 1)
        return fail(TownError::InvalidQuantity);
    const ItemDef* item = content.findItem(id);
    if (!item)
        return fail(TownError::UnknownItem);
    if (item->price <= 0)
        return fail(TownError::NotTradeable);
    if (hero.bag.count(id) < qty)
        return fail(TownError::ItemNotInBag);

    (void)hero.bag.remove(id, qty);
    hero.gold += item->price * qty;
    return {};
}

std::expected<AuctionResult, TownError> auction(GameState& state, const Content& content,
                                                std::string_view id) {
    Hero& hero = state.hero;
    if (hero.hp <= 0)
        return fail(TownError::HeroDead);
    const ItemDef* item = content.findItem(id);
    if (!item)
        return fail(TownError::UnknownItem);
    if (item->auctionValue <= 0)
        return fail(TownError::NotAuctionable);
    if (hero.bag.count(id) < 1)
        return fail(TownError::ItemNotInBag);

    Rng rng = state.rng.fork();
    const AuctionResult result = rollAuction(rng, item->auctionValue);
    if (result.sold) {
        (void)hero.bag.remove(id, 1);
        hero.gold += result.payout;
    }
    return result;
}

// ---- Blacksmith --------------------------------------------------------------------------

std::expected<void, TownError> equip(GameState& state, const Content& content, EquipSlot slot,
                                     std::string_view id) {
    Hero& hero = state.hero;
    if (hero.hp <= 0)
        return fail(TownError::HeroDead);
    const ItemDef* item = content.findItem(id);
    if (!item)
        return fail(TownError::UnknownItem);
    if (hero.bag.count(id) < 1)
        return fail(TownError::ItemNotInBag);
    const ItemKind wanted = slot == EquipSlot::Weapon ? ItemKind::Weapon : ItemKind::Armor;
    if (item->kind != wanted)
        return fail(TownError::WrongSlot);

    std::optional<Equipped>& current = slotOf(hero, slot);
    if (current && current->itemId == id)
        return fail(TownError::AlreadyEquipped);
    if (current) {
        Inventory afterRemoval = hero.bag;
        (void)afterRemoval.remove(id, 1);
        if (!afterRemoval.canAdd(current->itemId))
            return fail(TownError::BagFull);
    }

    (void)hero.bag.remove(id, 1);
    if (current)
        (void)hero.bag.add(current->itemId, 1);
    current = Equipped{std::string(id), 1};
    return {};
}

std::expected<void, TownError> upgrade(GameState& state, const Content& content, EquipSlot slot) {
    Hero& hero = state.hero;
    if (hero.hp <= 0)
        return fail(TownError::HeroDead);
    std::optional<Equipped>& current = slotOf(hero, slot);
    if (!current)
        return fail(TownError::NotEquipped);
    const int level = current->upgradeLevel;
    if (level >= kMaxUpgradeLevel)
        return fail(TownError::MaxUpgrade);
    const std::string& material = *content.item(current->itemId).upgradeMaterial;
    if (hero.bag.count(material) < level)
        return fail(TownError::MissingMaterial);
    if (hero.exp < 1000 * static_cast<std::int64_t>(level))
        return fail(TownError::NotEnoughExp);
    if (hero.gold < 10000 * static_cast<std::int64_t>(level))
        return fail(TownError::NotEnoughGold);

    (void)hero.bag.remove(material, level);
    hero.exp -= 1000 * static_cast<std::int64_t>(level);
    hero.gold -= 10000 * static_cast<std::int64_t>(level);
    ++current->upgradeLevel;
    return {};
}

std::expected<ForgeResult, TownError> forge(GameState& state, const Content& content,
                                            std::string_view recipeId) {
    Hero& hero = state.hero;
    if (hero.hp <= 0)
        return fail(TownError::HeroDead);
    const RecipeDef* recipe = content.findRecipe(recipeId);
    if (!recipe)
        return fail(TownError::UnknownItem);
    for (const auto& [ingredient, count] : recipe->ingredients)
        if (hero.bag.count(ingredient) < count)
            return fail(TownError::MissingIngredient);

    Inventory afterRemoval = hero.bag;
    for (const auto& [ingredient, count] : recipe->ingredients)
        (void)afterRemoval.remove(ingredient, count);
    if (!afterRemoval.canAdd(recipe->result))
        return fail(TownError::BagFull);

    Rng rng = state.rng.fork();
    hero.bag = std::move(afterRemoval);
    const bool success = rollForge(rng, hero.forgeLevel);
    if (success) {
        (void)hero.bag.add(recipe->result, 1);
        hero.forgeLevel = std::min(10, hero.forgeLevel + 1);
    }
    return ForgeResult{success, recipe->result};
}

// ---- Guildmaster -------------------------------------------------------------------------

bool revive(GameState& state) {
    Hero& hero = state.hero;
    if (hero.hp != 0)
        return false;
    hero.gold -= std::min(kReviveCost, hero.gold);
    hero.hp = std::min(kReviveHp, hero.maxHp);
    return true;
}

std::expected<void, TownError> useConsumable(GameState& state, const Content& content,
                                             std::string_view id) {
    Hero& hero = state.hero;
    const ItemDef* item = content.findItem(id);
    if (!item)
        return fail(TownError::UnknownItem);
    if (hero.bag.count(id) < 1)
        return fail(TownError::ItemNotInBag);
    if (item->kind != ItemKind::Consumable)
        return fail(TownError::NotConsumable);

    const ConsumableEffect& effect = *item->consumable;
    (void)hero.bag.remove(id, 1);
    if (effect.fullRestore) {
        hero.hp = hero.maxHp;
        hero.mp = hero.maxMp;
    } else {
        hero.hp = std::min(hero.maxHp, hero.hp + effect.restoreHp);
        hero.mp = std::min(hero.maxMp, hero.mp + effect.restoreMp);
    }
    return {};
}

std::expected<void, TownError> buyCharisma(GameState& state, const Content& content,
                                           CharismaStat stat, std::int64_t expSpent) {
    Hero& hero = state.hero;
    if (hero.level < kCharismaMinLevel)
        return fail(TownError::LevelTooLow);
    if (expSpent < 1)
        return fail(TownError::InvalidQuantity);
    if (expSpent > hero.exp)
        return fail(TownError::NotEnoughExp);
    const std::int64_t points = expSpent / kCharismaExpPerPoint;
    if (points < 1)
        return fail(TownError::InvalidQuantity);
    if (hero.gold < points * kCharismaGoldPerPoint)
        return fail(TownError::NotEnoughGold);
    const std::int64_t multiplier = content.path(hero.pathId).multiplier;
    const std::int64_t gain = points * 10 * multiplier / hero.level;
    if (gain < 1)
        return fail(TownError::InvalidQuantity);

    hero.exp -= expSpent;
    hero.gold -= points * kCharismaGoldPerPoint;
    const int amount = static_cast<int>(gain);
    switch (stat) {
    case CharismaStat::MaxHp:
        hero.maxHp += amount;
        break;
    case CharismaStat::MaxMp:
        hero.maxMp += amount;
        break;
    case CharismaStat::Atk:
        hero.atk += amount;
        break;
    case CharismaStat::Def:
        hero.def += amount;
        break;
    }
    return {};
}

std::expected<void, TownError> learnSkill(GameState& state, const Content& content,
                                          std::string_view id) {
    Hero& hero = state.hero;
    const SkillDef* skill = content.findSkill(id);
    if (!skill)
        return fail(TownError::UnknownItem);
    if (hero.level < std::max(kLearnMinLevel, skill->learnLevel))
        return fail(TownError::LevelTooLow);
    if (hero.learnedSkills.contains(skill->id))
        return fail(TownError::AlreadyLearned);
    if (hero.bag.count(skill->keyItem) < 1)
        return fail(TownError::ItemNotInBag);
    if (hero.gold < skill->goldCost)
        return fail(TownError::NotEnoughGold);

    (void)hero.bag.remove(skill->keyItem, 1);
    hero.gold -= skill->goldCost;
    hero.learnedSkills.insert(skill->id);
    return {};
}

std::expected<void, TownError> rename(GameState& state, std::string_view raw) {
    auto name = validateHeroName(raw);
    if (!name)
        return fail(name.error());
    if (state.hero.gold < kRenameCost)
        return fail(TownError::NotEnoughGold);

    state.hero.gold -= kRenameCost;
    state.hero.name = std::move(*name);
    return {};
}

// ---- Bank --------------------------------------------------------------------------------

std::expected<void, TownError> openBank(GameState& state) {
    Hero& hero = state.hero;
    if (hero.bank)
        return fail(TownError::AlreadyHasBank);
    if (hero.gold < kBankCost)
        return fail(TownError::NotEnoughGold);

    hero.gold -= kBankCost;
    hero.bank = Inventory{};
    return {};
}

namespace {

// Moves `qty` of `id` from one inventory to the other, or fails leaving both untouched.
std::expected<void, TownError> transfer(Inventory& from, Inventory& to, std::string_view id, int qty,
                                    TownError destinationFull) {
    if (qty < 1)
        return fail(TownError::InvalidQuantity);
    if (from.count(id) < qty)
        return fail(TownError::ItemNotInBag);
    if (!to.canAdd(id))
        return fail(destinationFull);
    (void)from.remove(id, qty);
    (void)to.add(id, qty);
    return {};
}

} // namespace

std::expected<void, TownError> deposit(GameState& state, std::string_view id, int qty) {
    if (!state.hero.bank)
        return fail(TownError::NoBankAccount);
    return transfer(state.hero.bag, *state.hero.bank, id, qty, TownError::BankFull);
}

std::expected<void, TownError> withdraw(GameState& state, std::string_view id, int qty) {
    if (!state.hero.bank)
        return fail(TownError::NoBankAccount);
    return transfer(*state.hero.bank, state.hero.bag, id, qty, TownError::BagFull);
}

// ---- Missions ----------------------------------------------------------------------------

std::expected<Battle, TownError> startMission(GameState& state, const Content& content,
                                              std::string_view id) {
    const Hero& hero = state.hero;
    const MissionDef* mission = content.findMission(id);
    if (!mission)
        return fail(TownError::UnknownItem);
    if (!canAdventure(hero))
        return fail(hero.hp <= 0 ? TownError::HeroDead : TownError::NotEquipped);
    if (hero.level < mission->minLevel)
        return fail(TownError::MissionLocked);
    if (!mission->repeatable && hero.completedMissions.contains(mission->id))
        return fail(TownError::MissionCompleted);

    return Battle(content, heroCombatant(hero, content), content.enemy(mission->enemy),
                  hero.learnedSkills, state.rng.fork());
}

std::expected<MissionResult, TownError> finishMission(GameState& state, const Content& content,
                                                      std::string_view id, const Battle& battle) {
    assert(battle.over());
    Hero& hero = state.hero;
    const MissionDef* mission = content.findMission(id);
    if (!mission)
        return fail(TownError::UnknownItem);

    MissionResult result;
    result.outcome = battle.outcome();
    hero.hp = battle.hero().hp;
    hero.mp = battle.hero().mp;

    if (result.outcome == Outcome::Victory) {
        const EnemyDef& enemy = content.enemy(mission->enemy);
        result.expGained = enemy.rewardExp;
        result.goldGained = enemy.rewardGold;
        result.levelsGained = grantExp(hero, content, enemy.rewardExp);
        hero.gold += enemy.rewardGold;
        for (const std::string& itemId : enemy.rewardItems) {
            if (hero.bag.canAdd(itemId)) {
                (void)hero.bag.add(itemId, 1);
                result.itemsGained.push_back(itemId);
            } else {
                result.itemsLostBagFull.push_back(itemId);
            }
        }
        hero.completedMissions.insert(mission->id);
    } else if (result.outcome == Outcome::Defeat) {
        hero.hp = 0;
        hero.weapon.reset();
        hero.armor.reset();
    }
    return result;
}

} // namespace heroes::game
