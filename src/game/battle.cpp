#include "game/battle.hpp"

#include "game/script/battle_api.hpp"
#include "game/script/script_host.hpp"

#include <algorithm>
#include <format>
#include <optional>

namespace heroes::game {

namespace {

Side opponent(Side side) { return side == Side::Hero ? Side::Enemy : Side::Hero; }

bool hasStatus(const Combatant& c, StatusId id) {
    return std::ranges::any_of(c.statuses, [&](const StatusInstance& s) { return s.id == id; });
}

} // namespace

Combatant heroCombatant(const Hero& hero, const Content& content) {
    const StatBonus bonus = equipmentBonus(hero, content);
    return {hero.name,
            hero.maxHp,
            hero.hp,
            hero.maxMp,
            hero.mp,
            hero.atk + bonus.atk,
            hero.def + bonus.def,
            hero.life + bonus.life,
            {}};
}

Combatant enemyCombatant(const EnemyDef& enemy) {
    return {enemy.name, enemy.maxHp, enemy.maxHp, 0, 0, enemy.atk, enemy.def, enemy.life, {}};
}

Battle::Battle(const Content& content, Combatant hero, const EnemyDef& enemy,
               std::set<std::string> learnedSkills, Rng rng, script::ScriptHost* scripts)
    : content_(&content), hero_(std::move(hero)), enemy_(enemyCombatant(enemy)), enemyId_(enemy.id),
      learnedSkills_(std::move(learnedSkills)), rng_(rng), scripts_(scripts) {}

std::vector<Event> Battle::start() {
    return {ev::BattleStarted{hero_.name, enemy_.name, enemyId_}};
}

std::expected<void, ActionError> Battle::validate(const CastSkill& cast) const {
    const SkillDef* skill = content_->findSkill(cast.skillId);
    if (!skill)
        return std::unexpected(ActionError::UnknownSkill);
    if (!learnedSkills_.contains(cast.skillId))
        return std::unexpected(ActionError::SkillNotLearned);
    if (hero_.mp < skill->mpCost)
        return std::unexpected(ActionError::NotEnoughMp);
    if (skill->effect.requiresTargetNotStunned && hasStatus(enemy_, StatusId::Stun))
        return std::unexpected(ActionError::SkillNotReady);
    return {};
}

void Battle::heal(Side side, int amount, Events& out) {
    Combatant& c = combatant(side);
    const int healed = std::min(amount, c.maxHp - c.hp);
    if (healed <= 0)
        return;
    c.hp += healed;
    out.push_back(ev::Healed{side, healed, c.hp});
}

// Returns the damage the target took (before clamping HP at 0).
int Battle::hit(Side attacker, int power, bool applyDef, DamageCause cause, Events& out) {
    const Side targetSide = opponent(attacker);
    Combatant& target = combatant(targetSide);
    const int def = hasStatus(target, StatusId::DefenseDown) ? target.def / 2 : target.def;
    const int damage = applyDef ? std::max(0, power - def) : power;
    if (hasStatus(target, StatusId::Reflect) && damage > 0) {
        const int reflected = damage / 2;
        const int taken = damage - reflected;
        target.hp = std::max(0, target.hp - taken);
        out.push_back(ev::Damaged{targetSide, taken, target.hp, cause});
        Combatant& source = combatant(attacker);
        source.hp = std::max(0, source.hp - reflected);
        out.push_back(ev::Damaged{attacker, reflected, source.hp, DamageCause::Reflect});
        return taken;
    }
    target.hp = std::max(0, target.hp - damage);
    out.push_back(ev::Damaged{targetSide, damage, target.hp, cause});
    return damage;
}

void Battle::applyStatus(Side target, const StatusSpec& spec, Events& out) {
    Combatant& c = combatant(target);
    const StatusInstance instance{spec.id, spec.turns, acting_ == target};
    auto existing =
        std::ranges::find_if(c.statuses, [&](const StatusInstance& s) { return s.id == spec.id; });
    if (existing != c.statuses.end())
        *existing = instance;
    else
        c.statuses.push_back(instance);
    out.push_back(ev::StatusApplied{target, spec.id, spec.turns});
}

void Battle::endTurn(Side side, Events& out) {
    std::vector<StatusInstance>& statuses = combatant(side).statuses;
    std::vector<StatusInstance> kept;
    for (StatusInstance s : statuses) {
        if (s.fresh) {
            s.fresh = false;
        } else if (--s.turnsRemaining == 0) {
            out.push_back(ev::StatusExpired{side, s.id});
            continue;
        }
        kept.push_back(s);
    }
    statuses = std::move(kept);
}

void Battle::finish(Outcome outcome, Events& out) {
    over_ = true;
    outcome_ = outcome;
    out.push_back(ev::BattleEnded{outcome, turn_});
}

void Battle::castSkill(const SkillDef& skill, Events& out) {
    const SkillEffect& effect = skill.effect;
    hero_.mp -= skill.mpCost;
    out.push_back(ev::SkillCast{skill.id, skill.mpCost});
    if (effect.healToFull)
        heal(Side::Hero, hero_.maxHp, out);
    if (effect.halveTargetHpFirst) {
        enemy_.hp /= 2;
        out.push_back(ev::HpHalved{Side::Enemy, enemy_.hp});
    }
    if (effect.atkMultiplier > 0) {
        const int dealt = hit(Side::Hero, effect.atkMultiplier * hero_.atk, !effect.ignoreDef,
                              DamageCause::Skill, out);
        if (effect.healPercentOfDamage > 0)
            heal(Side::Hero,
                 static_cast<int>(static_cast<std::int64_t>(dealt) * effect.healPercentOfDamage /
                                  100),
                 out);
    }
    for (const StatusSpec& spec : effect.selfStatuses)
        applyStatus(Side::Hero, spec, out);
    for (const StatusSpec& spec : effect.targetStatuses)
        applyStatus(Side::Enemy, spec, out);
}

bool Battle::checkDeaths(Events& out) {
    if (hero_.hp == 0) {
        out.push_back(ev::Defeated{Side::Hero});
        finish(Outcome::Defeat, out);
        return true;
    }
    if (enemy_.hp == 0) { // e.g. killed by reflected damage on its own turn
        out.push_back(ev::Defeated{Side::Enemy});
        finish(Outcome::Victory, out);
        return true;
    }
    return false;
}

void Battle::basicAttack(Side attacker, Events& out) {
    out.push_back(ev::Attacked{attacker});
    hit(attacker, combatant(attacker).atk, true, DamageCause::Attack, out);
    checkDeaths(out);
}

// A failed script is undone completely (combatants, RNG, its events) and reported, and the
// enemy makes a basic attack instead, with no extra turn (lua-interfaces.md §5).
bool Battle::scriptedEnemyTurn(const std::string& script, Events& out) {
    const Combatant hero = hero_, enemy = enemy_;
    const Rng rng = rng_;
    const std::size_t events = out.size();
    script::BattleApi api(*this, Side::Enemy, out);
    const auto ran = scripts_ ? scripts_->runEnemyTurn(script, api)
                              : std::expected<void, std::string>(std::unexpected(
                                    std::format("{}: no script host to run it", script)));
    if (ran)
        return api.extraTurnRequested() && !over_;
    hero_ = hero;
    enemy_ = enemy;
    rng_ = rng;
    over_ = false;
    outcome_ = Outcome::Draw;
    out.erase(out.begin() + static_cast<std::ptrdiff_t>(events), out.end());
    out.push_back(ev::ScriptError{script, ran.error()});
    basicAttack(Side::Enemy, out);
    return false;
}

void Battle::enemyPhase(Events& out) {
    acting_ = Side::Enemy;
    const std::optional<std::string>& script = content_->enemy(enemyId_).script;
    for (;;) {
        out.push_back(ev::TurnStarted{Side::Enemy, turn_});
        if (hasStatus(enemy_, StatusId::Stun)) {
            out.push_back(ev::TurnSkipped{Side::Enemy, StatusId::Stun});
            endTurn(Side::Enemy, out);
            return;
        }
        bool extraTurn = false;
        if (script)
            extraTurn = scriptedEnemyTurn(*script, out);
        else // EnemyBehavior::Basic
            basicAttack(Side::Enemy, out);
        if (over_)
            return;
        endTurn(Side::Enemy, out);
        if (!extraTurn)
            return;
        out.push_back(ev::ExtraTurn{Side::Enemy});
    }
}

bool Battle::castScriptedSkill(const SkillDef& skill, Events& out) {
    hero_.mp -= skill.mpCost;
    out.push_back(ev::SkillCast{skill.id, skill.mpCost});
    script::BattleApi api(*this, Side::Hero, out);
    return scripts_ && scripts_->runSkill(*skill.script, api).has_value();
}

std::expected<std::vector<Event>, ActionError> Battle::act(const PlayerAction& action) {
    if (over_)
        return std::unexpected(ActionError::BattleOver);
    if (const auto* cast = std::get_if<CastSkill>(&action)) {
        if (auto ok = validate(*cast); !ok)
            return std::unexpected(ok.error());
    }

    // A scripted skill that fails undoes the whole action, including this turn's start.
    std::optional<Battle> rollback;
    if (const auto* cast = std::get_if<CastSkill>(&action);
        cast && content_->findSkill(cast->skillId)->script)
        rollback = *this;

    Events out;
    ++turn_;
    acting_ = Side::Hero;
    out.push_back(ev::TurnStarted{Side::Hero, turn_});

    if (hasStatus(hero_, StatusId::Stun)) {
        out.push_back(ev::TurnSkipped{Side::Hero, StatusId::Stun});
    } else if (std::holds_alternative<Flee>(action)) {
        out.push_back(ev::Fled{});
        finish(Outcome::Fled, out);
        return out;
    } else {
        if (const auto* cast = std::get_if<CastSkill>(&action)) {
            const SkillDef& skill = *content_->findSkill(cast->skillId);
            if (!skill.script) {
                castSkill(skill, out);
            } else if (!castScriptedSkill(skill, out)) {
                *this = std::move(*rollback);
                return std::unexpected(ActionError::ScriptFailed);
            }
            if (over_) // a script's operations already ran the death checks
                return out;
        } else {
            out.push_back(ev::Attacked{Side::Hero});
            hit(Side::Hero, hero_.atk, true, DamageCause::Attack, out);
            if (enemy_.hp > 0)
                heal(Side::Hero, hero_.life, out);
        }
        if (enemy_.hp == 0) {
            out.push_back(ev::Defeated{Side::Enemy});
            finish(Outcome::Victory, out);
            return out;
        }
    }

    endTurn(Side::Hero, out);
    enemyPhase(out);
    if (!over_ && turn_ >= kTurnCap)
        finish(Outcome::Draw, out);
    return out;
}

} // namespace heroes::game
