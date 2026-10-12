#pragma once

#include "game/content.hpp"

#include <string>
#include <variant>

namespace heroes::game {

enum class Side { Hero, Enemy };
enum class Outcome { Victory, Defeat, Fled, Draw };
enum class DamageCause { Attack, Skill, Reflect };

namespace ev {
struct BattleStarted {
    std::string heroName, enemyName, enemyId;
};
struct TurnStarted {
    Side side;
    int turn;
};
struct TurnSkipped {
    Side side;
    StatusId reason;
};
struct Attacked {
    Side attacker;
};
struct SkillCast {
    std::string skillId;
    int mpSpent;
};
struct HpHalved {
    Side target;
    int hpAfter;
};
struct Damaged {
    Side target;
    int amount; // 0 = absorbed
    int hpAfter;
    DamageCause cause;
};
struct Healed { // emitted only if amount > 0
    Side target;
    int amount;
    int hpAfter;
};
struct StatusApplied {
    Side target;
    StatusId status;
    int turns;
};
struct StatusExpired {
    Side target;
    StatusId status;
};
struct ExtraTurn {
    Side side;
};
struct Fled {};
struct Defeated {
    Side side;
};
struct BattleEnded {
    Outcome outcome;
    int turns;
};
struct ScriptSaid { // a script's ctx.say line: printable ASCII, at most 80 characters
    Side side;
    std::string text;
};
struct ScriptError { // an enemy script failed and was undone; a basic attack follows
    std::string script;
    std::string message; // for developers; players see a generic line
};
} // namespace ev

using Event =
    std::variant<ev::BattleStarted, ev::TurnStarted, ev::TurnSkipped, ev::Attacked, ev::SkillCast,
                 ev::HpHalved, ev::Damaged, ev::Healed, ev::StatusApplied, ev::StatusExpired,
                 ev::ExtraTurn, ev::Fled, ev::Defeated, ev::BattleEnded, ev::ScriptSaid,
                 ev::ScriptError>;

} // namespace heroes::game
