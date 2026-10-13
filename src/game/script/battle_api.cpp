#include "game/script/battle_api.hpp"

#include "game/battle.hpp"

#include <algorithm>

namespace heroes::game::script {

BattleApi::BattleApi(Battle& battle, Side self, std::vector<Event>& out)
    : battle_(battle), self_(self), out_(out) {}

const Combatant& BattleApi::self() const {
    return self_ == Side::Hero ? battle_.hero() : battle_.enemy();
}

const Combatant& BattleApi::target() const {
    return self_ == Side::Hero ? battle_.enemy() : battle_.hero();
}

int BattleApi::turn() const { return battle_.turn(); }

bool BattleApi::over() const { return battle_.over(); }

int BattleApi::rng(int n) {
    if (battle_.over_)
        return 0;
    return static_cast<int>(battle_.rng_.below(static_cast<std::uint32_t>(n)));
}

int BattleApi::attack(int power) {
    if (battle_.over_)
        return 0;
    out_.push_back(ev::Attacked{self_});
    const int dealt = battle_.hit(self_, power, true, DamageCause::Attack, out_);
    battle_.checkDeaths(out_);
    return dealt;
}

int BattleApi::damage(int power, bool applyDef) {
    if (battle_.over_)
        return 0;
    const int dealt = battle_.hit(self_, power, applyDef, DamageCause::Skill, out_);
    battle_.checkDeaths(out_);
    return dealt;
}

int BattleApi::heal(Side who, int amount) {
    if (battle_.over_)
        return 0;
    const int before = battle_.combatant(who).hp;
    battle_.heal(who, amount, out_);
    return battle_.combatant(who).hp - before;
}

void BattleApi::halveHp(Side who) {
    if (battle_.over_)
        return;
    Combatant& c = battle_.combatant(who);
    c.hp /= 2;
    out_.push_back(ev::HpHalved{who, c.hp});
    battle_.checkDeaths(out_);
}

void BattleApi::applyStatus(Side who, StatusId id, int turns) {
    if (battle_.over_)
        return;
    battle_.applyStatus(who, StatusSpec{id, turns}, out_);
}

void BattleApi::extraTurn() {
    if (!battle_.over_)
        extraTurn_ = true;
}

void BattleApi::say(std::string_view text) {
    if (battle_.over_)
        return;
    std::string line;
    for (const char c : text.substr(0, 80)) // only printable ASCII reaches the terminal
        line += c >= 0x20 && c <= 0x7E ? c : '?';
    out_.push_back(ev::ScriptSaid{self_, std::move(line)});
}

} // namespace heroes::game::script
