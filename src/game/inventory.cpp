#include "game/inventory.hpp"

#include <algorithm>

namespace heroes::game {

const Inventory::Slot* Inventory::find(std::string_view id) const {
    auto it = std::ranges::find_if(slots_, [&](const Slot& s) { return s && s->first == id; });
    return it == slots_.end() ? nullptr : &*it;
}

Inventory::Slot* Inventory::find(std::string_view id) {
    return const_cast<Slot*>(std::as_const(*this).find(id));
}

bool Inventory::canAdd(std::string_view id) const {
    return find(id) != nullptr || std::ranges::any_of(slots_, [](const Slot& s) { return !s; });
}

std::expected<void, TownError> Inventory::add(std::string_view id, int qty) {
    if (qty < 1)
        return std::unexpected(TownError::InvalidQuantity);
    if (Slot* existing = find(id)) {
        (*existing)->second += qty;
        return {};
    }
    auto empty = std::ranges::find_if(slots_, [](const Slot& s) { return !s; });
    if (empty == slots_.end())
        return std::unexpected(TownError::BagFull);
    *empty = std::pair{std::string(id), qty};
    return {};
}

std::expected<void, TownError> Inventory::remove(std::string_view id, int qty) {
    if (qty < 1)
        return std::unexpected(TownError::InvalidQuantity);
    Slot* slot = find(id);
    if (!slot || (*slot)->second < qty)
        return std::unexpected(TownError::ItemNotInBag);
    (*slot)->second -= qty;
    if ((*slot)->second == 0)
        slot->reset();
    return {};
}

int Inventory::count(std::string_view id) const {
    const Slot* slot = find(id);
    return slot ? (*slot)->second : 0;
}

} // namespace heroes::game
