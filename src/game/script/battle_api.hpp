#pragma once

#include "game/content.hpp"
#include "game/events.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace heroes::game {
class Battle;
struct Combatant;
} // namespace heroes::game

namespace heroes::game::script {

// The `ctx` a script acts through (lua-interfaces.md §4): one per script run, bound to one Battle,
// the events of the action in progress, and the side whose script is running. Every operation
// goes through Battle's own rules and emits Battle's own events; after each one the core death
// checks run (hero first), and once the battle is over every operation does nothing and returns 0.
//
// Arguments arrive already checked (whole numbers, ranges, names) by the Lua binding in
// ScriptHost; invalid ones never reach these methods.
class BattleApi {
  public:
    BattleApi(Battle& battle, Side self, std::vector<Event>& out);

    const Combatant& self() const;
    const Combatant& target() const;
    int turn() const;
    bool over() const;

    int rng(int n);                         // Battle's Rng::below(n); n is 1..1,000,000
    int attack();                           // Attacked, then a normal hit; returns damage dealt
    int damage(int power, bool applyDef);   // a Skill hit of `power`; returns damage dealt
    int heal(Side who, int amount);         // capped at max HP; returns HP restored
    void halveHp(Side who);
    void applyStatus(Side who, StatusId id, int turns);
    void extraTurn();                       // enemy scripts only (the binding enforces it)
    void say(std::string_view text);        // printable ASCII, at most 80 characters

    Side selfSide() const { return self_; }
    Side targetSide() const { return self_ == Side::Hero ? Side::Enemy : Side::Hero; }
    bool extraTurnRequested() const { return extraTurn_; }

  private:
    Battle& battle_;
    Side self_;
    std::vector<Event>& out_;
    bool extraTurn_ = false;
};

} // namespace heroes::game::script
