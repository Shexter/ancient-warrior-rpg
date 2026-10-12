#include "app/screens/town.hpp"

#include <algorithm>
#include <array>
#include <format>

namespace heroes::app {

namespace {

using engine::Key;
using game::CharismaStat;

enum Choice { kRestoration, kCharisma, kLearnSkill, kChangeName, kBack };

int statValue(const game::Hero& hero, CharismaStat stat) {
    switch (stat) {
    case CharismaStat::MaxHp:
        return hero.maxHp;
    case CharismaStat::MaxMp:
        return hero.maxMp;
    case CharismaStat::Atk:
        return hero.atk;
    case CharismaStat::Def:
        return hero.def;
    }
    return 0;
}

engine::Rgb metColor(bool met) { return met ? palette::kHeal : palette::kDmg; }

} // namespace

GuildmasterScreen::GuildmasterScreen(Session& session)
    : TownScreen(session, "GUILDMASTER"), menu_({}), skills_({}) {
    std::vector<Menu::Item> rows;
    for (const game::SkillDef& skill : content().skills())
        rows.emplace_back(skill.name);
    skills_.setItems(std::move(rows));
    refresh();

    const std::int64_t goldBefore = hero().gold;
    if (game::revive(state())) {
        session_.dirty = true;
        modal_.message(std::format("Great warrior, be careful next time... (revived for {} gold)",
                                   goldBefore - hero().gold));
    }
}

void GuildmasterScreen::refresh() {
    const bool charismaOpen = hero().level >= game::kCharismaMinLevel;
    menu_.setItems({{"Restoration"},
                    {"Build Charisma", charismaOpen,
                     charismaOpen ? "" : std::format("Requires Lv {}", game::kCharismaMinLevel)},
                    {"Learn Skill"},
                    {"Change Name"},
                    {"Back"}});
}

Transition GuildmasterScreen::onKey(const engine::KeyEvent& key) {
    if (learning_) {
        if (key.key == Key::Escape) {
            learning_ = false;
        } else if (const auto index = skills_.handle(key)) {
            const game::SkillDef& skill = content().skills()[static_cast<std::size_t>(*index)];
            if (succeeded(game::learnSkill(state(), content(), skill.id)))
                modal_.message(std::format("You learned {}!", skill.name));
        }
        return Stay{};
    }

    if (key.key == Key::Escape)
        return Pop{};
    switch (menu_.handle(key).value_or(-1)) {
    case kRestoration:
        restoration();
        break;
    case kCharisma:
        modal_.choose("Raise which stat?", {{"Max HP"}, {"Max MP"}, {"ATK"}, {"DEF"}},
                      [this](int index) {
                          static constexpr std::array kStats{CharismaStat::MaxHp,
                                                             CharismaStat::MaxMp,
                                                             CharismaStat::Atk, CharismaStat::Def};
                          static constexpr std::array<std::string_view, 4> kNames{
                              "Max HP", "Max MP", "ATK", "DEF"};
                          const auto i = static_cast<std::size_t>(index);
                          charisma(kStats[i], std::string(kNames[i]));
                      });
        break;
    case kLearnSkill:
        learning_ = true;
        break;
    case kChangeName:
        modal_.text({std::format("The fee is {} gold.", game::kRenameCost),
                     "New name (max 8 characters):"},
                    8, [this](const std::string& raw) {
                        if (succeeded(game::rename(state(), raw)))
                            modal_.message(std::format("From now on you are {}.", hero().name));
                    });
        break;
    case kBack:
        return Pop{};
    default:
        break;
    }
    return Stay{};
}

void GuildmasterScreen::restoration() {
    std::vector<std::string> ids;
    std::vector<Menu::Item> rows;
    for (const game::Inventory::Slot& slot : hero().bag.slots()) {
        if (!slot || content().item(slot->first).kind != game::ItemKind::Consumable)
            continue;
        const game::ItemDef& item = content().item(slot->first);
        ids.push_back(item.id);
        rows.emplace_back(slot->second > 1 ? std::format("{} x{}", item.name, slot->second)
                                           : item.name,
                          true, item.description);
    }
    if (ids.empty()) {
        modal_.message("You have nothing to restore with.");
        return;
    }
    modal_.choose("Use which item?", std::move(rows), [this, ids](int index) {
        const std::string& id = ids[static_cast<std::size_t>(index)];
        if (succeeded(game::useConsumable(state(), content(), id)))
            modal_.message(std::format("You use the {}. HP {}/{} · MP {}/{}",
                                       content().item(id).name, hero().hp, hero().maxHp, hero().mp,
                                       hero().maxMp));
    });
}

void GuildmasterScreen::charisma(CharismaStat stat, std::string statName) {
    modal_.number(
        std::format("EXP to sacrifice (you have {}):", hero().exp), 1,
        std::max<std::int64_t>(1, hero().exp),
        [this, stat, statName](std::int64_t exp) {
            const game::CharismaQuote quote = game::charismaQuote(hero(), content(), exp);
            modal_.confirm(std::format("Sacrifice {} EXP and {} gold for +{} {}?", exp,
                                       quote.goldCost, quote.gain, statName),
                           [this, stat, statName, exp] {
                               if (succeeded(game::buyCharisma(state(), content(), stat, exp)))
                                   modal_.message(std::format("Your {} rises to {}.", statName,
                                                              statValue(hero(), stat)));
                           });
        },
        [this, statName](std::int64_t exp) {
            const game::CharismaQuote quote = game::charismaQuote(hero(), content(), exp);
            return std::format("→ +{} {} for {} gold", quote.gain, statName, quote.goldCost);
        });
}

void GuildmasterScreen::drawBody(engine::Framebuffer& frame) const {
    if (!learning_) {
        menu_.draw(frame, 4, 7);
        return;
    }
    skills_.draw(frame, 4, 7);
    const game::Hero& h = hero();
    const auto skills = content().skills();
    for (std::size_t i = 0; i < skills.size(); ++i) {
        const game::SkillDef& skill = skills[i];
        const int y = 7 + static_cast<int>(i);
        if (h.learnedSkills.contains(skill.id)) {
            drawText(frame, 22, y, "Learned", palette::kDim);
            continue;
        }
        const int level = std::max(game::kLearnMinLevel, skill.learnLevel);
        drawText(frame, 22, y, std::format("Lv {}", level), metColor(h.level >= level));
        drawText(frame, 30, y, std::format("{} gold", skill.goldCost),
                 metColor(h.gold >= skill.goldCost));
        drawText(frame, 44, y, content().item(skill.keyItem).name,
                 metColor(h.bag.count(skill.keyItem) > 0));
    }
    if (!skills.empty())
        drawText(frame, 4, 14,
                 skills[static_cast<std::size_t>(skills_.selected())].description,
                 palette::kDim);
}

std::string_view GuildmasterScreen::hint() const {
    return learning_ ? "↑↓ select · Enter learn · Esc back" : "Enter choose · Esc back";
}

} // namespace heroes::app
