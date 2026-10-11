#pragma once

#include "game/battle.hpp"
#include "game/content.hpp"
#include "game/errors.hpp"
#include "game/events.hpp"
#include "game/game_state.hpp"
#include "game/rng.hpp"

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

// Every function checks all conditions first and mutates only after they pass, so a returned
// error means the GameState is bit-identical to before the call.
namespace heroes::game {

enum class EquipSlot { Weapon, Armor };
enum class CharismaStat { MaxHp, MaxMp, Atk, Def };

struct AuctionResult {
    bool sold;
    int bids;
    std::int64_t payout;
};
struct ForgeResult {
    bool success;
    std::string itemId;
};
struct MissionResult {
    Outcome outcome = Outcome::Draw;
    int levelsGained = 0;
    std::int64_t expGained = 0, goldGained = 0;
    std::vector<std::string> itemsGained, itemsLostBagFull;
};

// Roll helpers, exposed so tests can drive them with an explicit Rng.
AuctionResult rollAuction(Rng& rng, std::int64_t auctionValue);
bool rollForge(Rng& rng, int forgeLevel);

// Merchant (refuses a dead hero)
std::expected<void, TownError> buy(GameState&, const Content&, std::string_view id, int qty);
std::expected<void, TownError> sell(GameState&, const Content&, std::string_view id, int qty);
std::expected<AuctionResult, TownError> auction(GameState&, const Content&, std::string_view id);

// Blacksmith (refuses a dead hero)
std::expected<void, TownError> equip(GameState&, const Content&, EquipSlot, std::string_view id);
std::expected<void, TownError> upgrade(GameState&, const Content&, EquipSlot);
std::expected<ForgeResult, TownError> forge(GameState&, const Content&, std::string_view recipeId);

// Guildmaster
// Revives a hero at 0 HP for min(100, gold). Returns false, changing nothing, if already alive.
bool revive(GameState&);
std::expected<void, TownError> useConsumable(GameState&, const Content&, std::string_view id);
// Spends `expSpent` EXP in full; only whole units of 50 earn points.
std::expected<void, TownError> buyCharisma(GameState&, const Content&, CharismaStat,
                                           std::int64_t expSpent);
std::expected<void, TownError> learnSkill(GameState&, const Content&, std::string_view id);
std::expected<void, TownError> rename(GameState&, std::string_view raw);

// Bank (alive or dead)
std::expected<void, TownError> openBank(GameState&);
std::expected<void, TownError> deposit(GameState&, std::string_view id, int qty);
std::expected<void, TownError> withdraw(GameState&, std::string_view id, int qty);

// Missions
std::expected<Battle, TownError> startMission(GameState&, const Content&, std::string_view id);
// Precondition: battle.over(). Copies HP/MP back on every outcome.
std::expected<MissionResult, TownError> finishMission(GameState&, const Content&,
                                                      std::string_view id, const Battle&);

} // namespace heroes::game
