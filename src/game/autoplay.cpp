#include "game/autoplay.hpp"

#include <algorithm>

namespace heroes::game {

namespace {

bool has(const Combatant& c, StatusId id) {
    return std::ranges::any_of(c.statuses, [id](const StatusInstance& s) { return s.id == id; });
}

} // namespace

PlayerAction smartAction(const Content& content, const Battle& battle,
                         const std::set<std::string>& learned) {
    const Combatant& hero = battle.hero();
    const Combatant& enemy = battle.enemy();
    const auto ready = [&](const char* id) {
        return learned.contains(id) && content.findSkill(id) &&
               hero.mp >= content.skill(id).mpCost;
    };

    if (has(enemy, StatusId::Charging))
        return ready("smite") ? PlayerAction{CastSkill{"smite"}} : PlayerAction{Defend{}};

    if (static_cast<long long>(hero.hp) * 100 < static_cast<long long>(hero.maxHp) * 30) {
        if (ready("angel_wings"))
            return CastSkill{"angel_wings"};
        if (ready("sanctuary") && !has(hero, StatusId::Guard))
            return CastSkill{"sanctuary"};
        // The item that restores the most HP (a full restore counts as all the HP missing).
        const ItemDef* best = nullptr;
        int bestHeal = 0;
        for (const auto& [id, count] : battle.items()) {
            const ItemDef& item = content.item(id);
            if (count <= 0 || !item.consumable)
                continue;
            const int heal = item.consumable->fullRestore ? hero.maxHp - hero.hp
                                                          : item.consumable->restoreHp;
            if (heal > bestHeal) {
                best = &item;
                bestHeal = heal;
            }
        }
        if (best)
            return UseItem{best->id};
    }

    if (ready("judgment") && enemy.hp * 4 < enemy.maxHp)
        return CastSkill{"judgment"};
    if (ready("supernova"))
        return CastSkill{"supernova"};
    if (ready("frozen_throne") && !has(enemy, StatusId::Stun))
        return CastSkill{"frozen_throne"};
    if (ready("meteor_fist"))
        return CastSkill{"meteor_fist"};
    if (ready("golden_bell") && !has(hero, StatusId::Reflect))
        return CastSkill{"golden_bell"};
    return Attack{};
}

} // namespace heroes::game
