#include "app/screens/town.hpp"

#include <format>

namespace heroes::app {

namespace {

using engine::Key;
using game::EquipSlot;

enum Choice { kChange, kUpgrade, kForge, kBack };

const std::optional<game::Equipped>& slotOf(const game::Hero& hero, EquipSlot slot) {
    return slot == EquipSlot::Weapon ? hero.weapon : hero.armor;
}

std::string gearName(const std::optional<game::Equipped>& e, const game::Content& content) {
    if (!e)
        return "—";
    return std::format("{} Lv {}", content.item(e->itemId).name, e->upgradeLevel);
}

// Green when the hero has enough, red when not.
engine::Rgb affordColor(bool enough) { return enough ? palette::kHeal : palette::kDmg; }

} // namespace

BlacksmithScreen::BlacksmithScreen(Session& session)
    : TownScreen(session, "BLACKSMITH"),
      menu_({{"Change Equipment"}, {"Upgrade"}, {"Forge"}, {"Back"}}), recipes_({}) {
    std::vector<Menu::Item> rows;
    for (const game::RecipeDef& recipe : content().recipes())
        rows.emplace_back(content().item(recipe.result).name);
    recipes_.setItems(std::move(rows));
    if (hero().hp <= 0)
        modal_.message("Empty soul, you don't belong here", [this] { leave_ = true; });
}

Transition BlacksmithScreen::onKey(const engine::KeyEvent& key) {
    if (mode_ == Mode::Upgrade) {
        if (key.key == Key::Escape)
            mode_ = Mode::Main;
        else if (key.key == Key::Enter)
            confirmUpgrade();
        return Stay{};
    }
    if (mode_ == Mode::Forge) {
        if (key.key == Key::Escape)
            mode_ = Mode::Main;
        else if (const auto index = recipes_.handle(key))
            confirmForge(content().recipes()[static_cast<std::size_t>(*index)]);
        return Stay{};
    }

    if (key.key == Key::Escape)
        return Pop{};
    const auto slots = std::vector<Menu::Item>{{"Weapon", true, gearName(hero().weapon, content())},
                                               {"Armor", true, gearName(hero().armor, content())}};
    auto slotAt = [](int index) { return index == 0 ? EquipSlot::Weapon : EquipSlot::Armor; };
    switch (menu_.handle(key).value_or(-1)) {
    case kChange:
        modal_.choose("Change which slot?", slots,
                      [this, slotAt](int index) { changeEquipment(slotAt(index)); });
        break;
    case kUpgrade:
        modal_.choose("Upgrade which slot?", slots, [this, slotAt](int index) {
            slot_ = slotAt(index);
            if (slotOf(hero(), slot_))
                mode_ = Mode::Upgrade;
            else
                modal_.message(game::describe(game::TownError::NotEquipped));
        });
        break;
    case kForge:
        mode_ = Mode::Forge;
        break;
    case kBack:
        return Pop{};
    default:
        break;
    }
    return Stay{};
}

void BlacksmithScreen::changeEquipment(EquipSlot slot) {
    const game::ItemKind kind =
        slot == EquipSlot::Weapon ? game::ItemKind::Weapon : game::ItemKind::Armor;
    const std::string_view noun = slot == EquipSlot::Weapon ? "weapon" : "armor";
    std::vector<std::string> ids;
    std::vector<Menu::Item> rows;
    for (const game::Inventory::Slot& bagSlot : hero().bag.slots())
        if (bagSlot && content().item(bagSlot->first).kind == kind) {
            ids.push_back(bagSlot->first);
            rows.emplace_back(content().item(bagSlot->first).name);
        }
    if (ids.empty()) {
        modal_.message(std::format("You have no {} in your bag.",
                                   slot == EquipSlot::Weapon ? "weapons" : "armor"));
        return;
    }
    modal_.choose(std::format("Equip which {}?", noun), std::move(rows),
                  [this, slot, noun, ids](int index) {
                      const std::string id = ids[static_cast<std::size_t>(index)];
                      auto equip = [this, slot, id] {
                          if (succeeded(game::equip(state(), content(), slot, id)))
                              modal_.message(
                                  std::format("You equip the {}.", content().item(id).name));
                      };
                      const auto& current = slotOf(hero(), slot);
                      if (current && current->upgradeLevel > 1 && current->itemId != id)
                          modal_.confirm(std::format("Your upgraded {} will return to Lv 1. "
                                                     "Continue?",
                                                     noun),
                                         equip);
                      else
                          equip();
                  });
}

void BlacksmithScreen::confirmUpgrade() {
    const auto& current = slotOf(hero(), slot_);
    if (!current) // cannot happen from the menu; kept so the screen never dereferences empty
        return;
    if (current->upgradeLevel >= game::kMaxUpgradeLevel) {
        modal_.message(game::describe(game::TownError::MaxUpgrade));
        return;
    }
    const std::string name = content().item(current->itemId).name;
    const int next = current->upgradeLevel + 1;
    modal_.confirm(std::format("Upgrade the {} to Lv {}?", name, next), [this, name, next] {
        if (succeeded(game::upgrade(state(), content(), slot_)))
            modal_.message(std::format("Your {} is now Lv {}.", name, next));
    });
}

void BlacksmithScreen::confirmForge(const game::RecipeDef& recipe) {
    const std::string name = content().item(recipe.result).name;
    modal_.confirm(std::format("Forge the {}? Success chance {}%.", name,
                               game::forgeChancePercent(hero().forgeLevel)),
                   [this, &recipe, name] {
                       const auto result = game::forge(state(), content(), recipe.id);
                       if (!succeeded(result))
                           return;
                       modal_.message(result->success
                                          ? std::format("Forging SUCCESS!!! You now possess {}",
                                                        name)
                                          : std::string("Forging FAILED, the materials are lost..."));
                   });
}

void BlacksmithScreen::drawBody(engine::Framebuffer& frame) const {
    switch (mode_) {
    case Mode::Main:
        menu_.draw(frame, 4, 7);
        break;
    case Mode::Upgrade:
        drawUpgrade(frame);
        break;
    case Mode::Forge:
        drawForge(frame);
        break;
    }
}

void BlacksmithScreen::drawUpgrade(engine::Framebuffer& frame) const {
    const auto& current = slotOf(hero(), slot_);
    if (!current)
        return;
    const int level = current->upgradeLevel;
    const game::ItemDef& item = content().item(current->itemId);
    if (level >= game::kMaxUpgradeLevel) {
        drawText(frame, 4, 7, std::format("{}  Lv {}", item.name, level), palette::kText);
        drawText(frame, 4, 9, "Already at maximum upgrade.", palette::kDim);
        return;
    }
    drawText(frame, 4, 7, std::format("{}  Lv {} → Lv {}", item.name, level, level + 1),
             palette::kText);
    const game::UpgradeCost cost = game::upgradeCost(level);
    const game::Hero& h = hero();
    const std::string& material = *item.upgradeMaterial;
    const int have = h.bag.count(material);
    drawText(frame, 4, 9, "Cost", palette::kAccent);
    drawText(frame, 6, 10,
             std::format("{} x{}   (you have {})", content().item(material).name,
                         cost.materialCount, have),
             affordColor(have >= cost.materialCount));
    drawText(frame, 6, 11, std::format("{} EXP   (you have {})", cost.exp, h.exp),
             affordColor(h.exp >= cost.exp));
    drawText(frame, 6, 12, std::format("{} gold   (you have {})", cost.gold, h.gold),
             affordColor(h.gold >= cost.gold));
}

void BlacksmithScreen::drawForge(engine::Framebuffer& frame) const {
    recipes_.draw(frame, 4, 7);
    if (content().recipes().empty())
        return;
    const game::RecipeDef& recipe =
        content().recipes()[static_cast<std::size_t>(recipes_.selected())];
    drawText(frame, 34, 7, "Ingredients", palette::kAccent);
    int y = 8;
    for (const auto& [id, need] : recipe.ingredients) {
        const int have = hero().bag.count(id);
        drawText(frame, 36, y++, std::format("{}  {}/{}", content().item(id).name, have, need),
                 affordColor(have >= need));
    }
    drawText(frame, 34, y + 1,
             std::format("Success chance {}%", game::forgeChancePercent(hero().forgeLevel)),
             palette::kText);
}

std::string_view BlacksmithScreen::hint() const {
    switch (mode_) {
    case Mode::Upgrade:
        return "Enter upgrade · Esc back";
    case Mode::Forge:
        return "↑↓ select · Enter forge · Esc back";
    default:
        return "Enter choose · Esc back";
    }
}

} // namespace heroes::app
