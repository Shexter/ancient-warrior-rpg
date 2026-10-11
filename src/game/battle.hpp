#pragma once

#include "game/content.hpp"
#include "game/events.hpp"
#include "game/hero.hpp"
#include "game/rng.hpp"

#include <expected>
#include <set>
#include <string>
#include <variant>
#include <vector>

namespace heroes::game {

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
Combatant enemyCombatant(const EnemyDef& enemy);

struct Attack {};
struct CastSkill {
    std::string skillId;
};
struct Flee {};
using PlayerAction = std::variant<Attack, CastSkill, Flee>;

enum class ActionError { BattleOver, SkillNotLearned, NotEnoughMp, SkillNotReady, UnknownSkill };

// Turn-based fight: actions in, events out. No I/O; all randomness comes from the Rng given.
class Battle {
  public:
    static constexpr int kTurnCap = 1000;

    Battle(const Content& content, Combatant hero, const EnemyDef& enemy,
           std::set<std::string> learnedSkills, Rng rng);

    std::vector<Event> start();
    // On error nothing is emitted and no state changes.
    std::expected<std::vector<Event>, ActionError> act(const PlayerAction& action);

    bool over() const { return over_; }
    Outcome outcome() const { return outcome_; } // precondition: over()
    const Combatant& hero() const { return hero_; }
    const Combatant& enemy() const { return enemy_; }
    int turn() const { return turn_; }

  private:
    using Events = std::vector<Event>;

    Combatant& combatant(Side side) { return side == Side::Hero ? hero_ : enemy_; }
    std::expected<void, ActionError> validate(const CastSkill& cast) const;
    void heal(Side side, int amount, Events& out);
    int hit(Side attacker, int power, bool applyDef, DamageCause cause, Events& out);
    void applyStatus(Side target, const StatusSpec& spec, Events& out);
    void endTurn(Side side, Events& out);
    void finish(Outcome outcome, Events& out);
    void castSkill(const SkillDef& skill, Events& out);
    void enemyPhase(Events& out);

    const Content* content_;
    Combatant hero_, enemy_;
    std::string enemyId_;
    std::set<std::string> learnedSkills_;
    Rng rng_;
    int turn_ = 0;
    Side acting_ = Side::Hero;
    bool over_ = false;
    Outcome outcome_ = Outcome::Draw;
};

} // namespace heroes::game
