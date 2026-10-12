#pragma once

#include <string_view>

namespace heroes::game {

enum class TownError {
    HeroDead,
    NotEquipped,
    NotEnoughGold,
    NotEnoughExp,
    BagFull,
    BankFull,
    ItemNotInBag,
    NotTradeable,
    NotAuctionable,
    LevelTooLow,
    MissingMaterial,
    MissingIngredient,
    AlreadyLearned,
    NoBankAccount,
    AlreadyHasBank,
    InvalidQuantity,
    InvalidName,
    MaxUpgrade,
    AlreadyEquipped,
    WrongSlot,
    NotConsumable,
    UnknownItem,
    MissionLocked,
    MissionCompleted,
    NoBlessings,
};

// Player-facing sentence for every value.
inline std::string_view describe(TownError e) {
    switch (e) {
    case TownError::HeroDead:
        return "Your hero has fallen and must be revived first.";
    case TownError::NotEquipped:
        return "Nothing is equipped in that slot.";
    case TownError::NotEnoughGold:
        return "You don't have enough gold.";
    case TownError::NotEnoughExp:
        return "You don't have enough EXP.";
    case TownError::BagFull:
        return "Your bag is full.";
    case TownError::BankFull:
        return "Your bank is full.";
    case TownError::ItemNotInBag:
        return "You don't have that item.";
    case TownError::NotTradeable:
        return "The merchant won't deal in that item.";
    case TownError::NotAuctionable:
        return "That item can't be auctioned.";
    case TownError::LevelTooLow:
        return "Your level is too low.";
    case TownError::MissingMaterial:
        return "You are missing the upgrade materials.";
    case TownError::MissingIngredient:
        return "You are missing an ingredient.";
    case TownError::AlreadyLearned:
        return "You already know that skill.";
    case TownError::NoBankAccount:
        return "You don't have a bank account.";
    case TownError::AlreadyHasBank:
        return "You already have a bank account.";
    case TownError::InvalidQuantity:
        return "That quantity is not allowed.";
    case TownError::InvalidName:
        return "Names are 1-8 plain characters.";
    case TownError::MaxUpgrade:
        return "That item is already at maximum upgrade.";
    case TownError::AlreadyEquipped:
        return "That item is already equipped.";
    case TownError::WrongSlot:
        return "That item doesn't fit that slot.";
    case TownError::NotConsumable:
        return "That item can't be used.";
    case TownError::UnknownItem:
        return "There is no such item.";
    case TownError::MissionLocked:
        return "You are not ready for that mission.";
    case TownError::MissionCompleted:
        return "You have already completed that mission.";
    case TownError::NoBlessings:
        return "You have no blessing points to spend.";
    }
    return "Something went wrong.";
}

} // namespace heroes::game
