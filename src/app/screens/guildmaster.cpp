#include "app/screens/town.hpp"
#include "app/art.hpp"

#include <algorithm>
#include <array>
#include <format>

namespace heroes::app {

namespace {

using engine::Key;
using game::CharismaStat;

enum Choice { kRestoration, kBlessings, kCharisma, kLearnSkill, kChangeName, kBack };

constexpr std::array kStats{CharismaStat::MaxHp, CharismaStat::MaxMp, CharismaStat::Atk,
                           CharismaStat::Def};
constexpr std::array<std::string_view, 4> kStatNames{"Max HP", "Max Fervor", "ATK", "DEF"};

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
    : TownScreen(session, "THE PRIOR"), menu_({}), skills_({}) {
    greet("guildmaster");
    std::vector<Menu::Item> rows;
    for (const game::SkillDef& skill : content().skills())
        rows.emplace_back(skill.name);
    skills_.setItems(std::move(rows));
    refresh();

    const std::int64_t goldBefore = hero().gold;
    if (game::revive(state())) {
        session_.dirty = true;
        modal_.message(std::format("Rise. Mercy asks only that you be more careful. ({} gold to the poor box)",
                                   goldBefore - hero().gold));
    }
}

void GuildmasterScreen::refresh() {
    const bool charismaOpen = hero().level >= game::kCharismaMinLevel;
    const int points = hero().blessings;
    menu_.setItems({{"Restoration"},
                    {points > 0 ? std::format("Blessings ({})", points) : "Blessings", points > 0,
                     points > 0 ? "" : "Level up to earn more"},
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
    case kBlessings:
        blessings();
        break;
    case kCharisma:
        modal_.choose("Raise which stat?", {{"Max HP"}, {"Max Fervor"}, {"ATK"}, {"DEF"}},
                      [this](int index) {
                          const auto i = static_cast<std::size_t>(index);
                          charisma(kStats[i], std::string(kStatNames[i]));
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
            modal_.message(std::format("You use the {}. HP {}/{} · Fervor {}/{}",
                                       content().item(id).name, hero().hp, hero().maxHp, hero().mp,
                                       hero().maxMp));
    });
}

void GuildmasterScreen::blessings() {
    std::vector<Menu::Item> rows;
    for (std::size_t i = 0; i < kStats.size(); ++i)
        rows.emplace_back(std::format("{} +{}", kStatNames[i], game::blessingGain(kStats[i])));
    modal_.choose(std::format("Spend a blessing ({} left)", hero().blessings), std::move(rows),
                  [this](int index) {
                      const auto i = static_cast<std::size_t>(index);
                      if (!succeeded(game::blessing(state(), kStats[i])))
                          return;
                      refresh();
                      modal_.message(std::format("Your {} rises to {}. {} blessing{} left.",
                                                 kStatNames[i], statValue(hero(), kStats[i]),
                                                 hero().blessings,
                                                 hero().blessings == 1 ? "" : "s"));
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
    if (isPortrait(frame)) {
        drawBodyPortrait(frame);
        return;
    }
    if (!learning_) {
        menu_.draw(frame, 4, 7);
        return;
    }
    skills_.draw(frame, 4, 7);
    const game::Hero& h = hero();
    const auto skills = content().skills();
    // Requirement columns start after the longest skill name (menu labels begin at col 6).
    int nameWidth = 0;
    for (const game::SkillDef& skill : skills)
        nameWidth = std::max(nameWidth, textWidth(skill.name));
    const int levelX = 6 + nameWidth + 2, goldX = levelX + 8, keyX = goldX + 14;
    for (std::size_t i = 0; i < skills.size(); ++i) {
        const game::SkillDef& skill = skills[i];
        const int y = 7 + static_cast<int>(i);
        if (h.learnedSkills.contains(skill.id)) {
            drawText(frame, levelX, y, "Learned", palette::kDim);
            continue;
        }
        const int level = std::max(game::kLearnMinLevel, skill.learnLevel);
        drawText(frame, levelX, y, std::format("Lv {}", level), metColor(h.level >= level));
        drawText(frame, goldX, y, std::format("{} gold", skill.goldCost),
                 metColor(h.gold >= skill.goldCost));
        drawText(frame, keyX, y, content().item(skill.keyItem).name,
                 metColor(h.bag.count(skill.keyItem) > 0));
    }
    if (!skills.empty())
        drawText(frame, 4, 14,
                 skills[static_cast<std::size_t>(skills_.selected())].description,
                 palette::kDim);
}

// Phone: the menu (or the skills) in the band; while learning, the highlighted skill's needs
// above it, each green when met and red when not.
void GuildmasterScreen::drawBodyPortrait(engine::Framebuffer& frame) const {
    if (!learning_) {
        drawButtonMenu(frame, menu_);
        return;
    }
    drawButtonMenu(frame, skills_);
    const auto skills = content().skills();
    if (skills.empty())
        return;
    const game::SkillDef& skill = skills[static_cast<std::size_t>(skills_.selected())];
    const game::Hero& h = hero();
    drawText(frame, 2, 7, skill.name, palette::kAccent, 44);
    if (h.learnedSkills.contains(skill.id)) {
        drawText(frame, 2, 9, "Learned", palette::kDim);
    } else {
        const int level = std::max(game::kLearnMinLevel, skill.learnLevel);
        drawText(frame, 2, 9, std::format("Lv {}", level), metColor(h.level >= level));
        drawText(frame, 2, 10, std::format("{} gold", skill.goldCost),
                 metColor(h.gold >= skill.goldCost));
        drawText(frame, 2, 11, content().item(skill.keyItem).name,
                 metColor(h.bag.count(skill.keyItem) > 0), 44);
    }
    const auto lines = wordWrap(skill.description, 44);
    for (std::size_t i = 0; i < lines.size() && i < 6; ++i)
        drawText(frame, 2, 13 + static_cast<int>(i), lines[i], palette::kDim);
}

std::string_view GuildmasterScreen::hint() const {
    return learning_ ? "↑↓ select · Enter learn · Esc back" : "Enter choose · Esc back";
}

} // namespace heroes::app
