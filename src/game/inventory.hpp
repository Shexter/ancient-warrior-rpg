#pragma once

#include "game/errors.hpp"

#include <array>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace heroes::game {

// Fixed 10 ordered slots; identical items stack without limit.
class Inventory {
  public:
    static constexpr int kSlots = 10;
    using Slot = std::optional<std::pair<std::string, int>>;

    bool canAdd(std::string_view id) const;
    std::expected<void, TownError> add(std::string_view id, int qty);    // qty >= 1
    std::expected<void, TownError> remove(std::string_view id, int qty); // fails if count < qty
    int count(std::string_view id) const;
    std::span<const Slot> slots() const { return slots_; }

    bool operator==(const Inventory&) const = default;

  private:
    Slot* find(std::string_view id);
    const Slot* find(std::string_view id) const;

    std::array<Slot, kSlots> slots_;
};

} // namespace heroes::game
