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

inline constexpr int kMaxUpgradeLevel = 10;
inline constexpr std::int64_t kBankCost = 10000;
inline constexpr std::int64_t kRenameCost = 50000;
inline constexpr int kCharismaMinLevel = 10;
inline constexpr int kLearnMinLevel = 5;

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

// Costs and odds, shared by the actions below and the screens that preview them.
struct UpgradeCost {
    int materialCount;
    std::int64_t exp, gold;
};
UpgradeCost upgradeCost(int currentLevel); // for raising currentLevel (1..9) by one
int forgeChancePercent(int forgeLevel);
struct CharismaQuote {
    std::int64_t points, goldCost, gain;
};
CharismaQuote charismaQuote(const Hero& hero, const Content& content, std::int64_t expSpent);
// What one blessing point adds: +30 max HP, +10 max MP, +6 ATK or +3 DEF.
int blessingGain(CharismaStat stat);

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
// Spends one blessing point on `stat`; current HP/MP rise with their maximum.
std::expected<void, TownError> blessing(GameState&, CharismaStat);
std::expected<void, TownError> learnSkill(GameState&, const Content&, std::string_view id);
std::expected<void, TownError> rename(GameState&, std::string_view raw);

// Bank (alive or dead)
std::expected<void, TownError> openBank(GameState&);
std::expected<void, TownError> deposit(GameState&, std::string_view id, int qty);
std::expected<void, TownError> withdraw(GameState&, std::string_view id, int qty);

// Missions
// The consumables in the bag, which the hero can use during a battle.
ItemStock consumableStock(const Hero&, const Content&);
// A consumable that restores HP (a potion, as the field's Q and HUD count them).
bool restoresHp(const ItemDef& item);
// `scripts` runs scripted enemies and skills in the battle (one host per thread).
std::expected<Battle, TownError> startMission(GameState&, const Content&, std::string_view id,
                                              script::ScriptHost* scripts = nullptr);
// Precondition: battle.over(). Copies HP/MP back on every outcome.
std::expected<MissionResult, TownError> finishMission(GameState&, const Content&,
                                                      std::string_view id, const Battle&);

} // namespace heroes::game
