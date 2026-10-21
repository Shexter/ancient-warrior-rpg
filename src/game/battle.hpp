#pragma once

#include "game/content.hpp"
#include "game/events.hpp"
#include "game/hero.hpp"
#include "game/rng.hpp"

#include <algorithm>
#include <expected>
#include <map>
#include <set>
#include <string>
#include <variant>
#include <vector>

namespace heroes::game {

namespace script {
class ScriptHost;
class BattleApi;
} // namespace script

struct StatusInstance {
    StatusId id;
    int turnsRemaining;
    bool fresh; // applied during its owner's own turn; skips that turn's end-of-turn tick
    bool operator==(const StatusInstance&) const = default;
};

struct Combatant {
    std::string name;
    int maxHp, hp, maxMp, mp, atk, def, life;
    std::vector<StatusInstance> statuses;
    bool operator==(const Combatant&) const = default;
};

Combatant heroCombatant(const Hero& hero, const Content& content); // base + equipmentBonus

// The game's damage rule for a hit that DEF applies to: battles, training and the field.
constexpr int defendedDamage(int power, int def) { return std::max(0, power - def); }
Combatant enemyCombatant(const EnemyDef& enemy);

struct Attack {};
struct CastSkill {
    std::string skillId;
};
struct Flee {};
struct UseItem { // a consumable from the battle's stock
    std::string itemId;
};
struct Defend {}; // Guard until the hero's next turn ends, and +10 MP
using PlayerAction = std::variant<Attack, CastSkill, Flee, UseItem, Defend>;

// Consumable item id -> how many the hero carries into the battle.
using ItemStock = std::map<std::string, int, std::less<>>;

enum class ActionError {
    BattleOver,
    SkillNotLearned,
    NotEnoughMp,
    SkillNotReady,
    UnknownSkill,
    ScriptFailed, // a scripted skill failed; nothing changed and it is still the hero's turn
    ItemNotInStock, // none of that consumable left in the battle stock
};

// Turn-based fight: actions in, events out. No I/O; all randomness comes from the Rng given.
class Battle {
  public:
    static constexpr int kTurnCap = 1000;

    // `scripts` runs scripted enemies and skills (one host per thread). Without one, a scripted
    // enemy falls back to basic attacks and a scripted skill fails.
    // `items` is the consumable stock the hero brings in (UseItem draws from it).
    Battle(const Content& content, Combatant hero, const EnemyDef& enemy,
           std::set<std::string> learnedSkills, Rng rng, script::ScriptHost* scripts = nullptr,
           ItemStock items = {});

    std::vector<Event> start();
    // On error nothing is emitted and no state changes.
    std::expected<std::vector<Event>, ActionError> act(const PlayerAction& action);

    bool over() const { return over_; }
    Outcome outcome() const { return outcome_; } // precondition: over()
    const Combatant& hero() const { return hero_; }
    const Combatant& enemy() const { return enemy_; }
    int turn() const { return turn_; }
    const ItemStock& items() const { return items_; }         // what's left
    const ItemStock& itemsUsed() const { return itemsUsed_; } // to take out of the bag after

  private:
    friend class script::BattleApi;
    using Events = std::vector<Event>;

    Combatant& combatant(Side side) { return side == Side::Hero ? hero_ : enemy_; }
    std::expected<void, ActionError> validate(const CastSkill& cast) const;
    void heal(Side side, int amount, Events& out);
    int hit(Side attacker, int power, bool applyDef, DamageCause cause, Events& out);
    void applyStatus(Side target, const StatusSpec& spec, Events& out);
    void endTurn(Side side, Events& out);
    void finish(Outcome outcome, Events& out);
    void castSkill(const SkillDef& skill, Events& out);
    void restoreMp(Side side, int amount, Events& out);
    void useItem(const ItemDef& item, Events& out);
    // Runs a skill's script; false if it failed (the caller rolls the whole action back).
    bool castScriptedSkill(const SkillDef& skill, Events& out);
    void enemyPhase(Events& out);
    // A scripted enemy turn; returns whether the enemy gets an extra turn.
    bool scriptedEnemyTurn(const std::string& script, Events& out);
    void basicAttack(Side attacker, Events& out);
    // Defeated + BattleEnded for whoever is at 0 HP, hero first. True if the battle ended.
    bool checkDeaths(Events& out);

    const Content* content_;
    Combatant hero_, enemy_;
    std::string enemyId_;
    std::set<std::string> learnedSkills_;
    Rng rng_;
    script::ScriptHost* scripts_;
    ItemStock items_, itemsUsed_;
    int turn_ = 0;
    Side acting_ = Side::Hero;
    bool over_ = false;
    Outcome outcome_ = Outcome::Draw;
};

} // namespace heroes::game
