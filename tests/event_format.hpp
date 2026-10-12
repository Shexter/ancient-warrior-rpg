#pragma once

#include "game/events.hpp"

#include <format>
#include <string>
#include <string_view>
#include <variant>

namespace heroes::game {

// Renders an event in the golden-vector line format: name, then fields in struct order.
inline std::string toString(const Event& event) {
    auto side = [](Side s) { return s == Side::Hero ? "Hero" : "Enemy"; };
    auto status = [](StatusId id) -> std::string_view {
        switch (id) {
        case StatusId::Stun:
            return "stun";
        case StatusId::DefenseDown:
            return "defense_down";
        case StatusId::Reflect:
            return "reflect";
        case StatusId::Guard:
            return "guard";
        case StatusId::Charging:
            return "charging";
        }
        return "?";
    };
    auto cause = [](DamageCause c) -> std::string_view {
        switch (c) {
        case DamageCause::Attack:
            return "Attack";
        case DamageCause::Skill:
            return "Skill";
        case DamageCause::Reflect:
            return "Reflect";
        }
        return "?";
    };
    auto outcome = [](Outcome o) -> std::string_view {
        switch (o) {
        case Outcome::Victory:
            return "Victory";
        case Outcome::Defeat:
            return "Defeat";
        case Outcome::Fled:
            return "Fled";
        case Outcome::Draw:
            return "Draw";
        }
        return "?";
    };
    struct Visitor {
        decltype(side) s;
        decltype(status) st;
        decltype(cause) c;
        decltype(outcome) o;
        std::string operator()(const ev::BattleStarted& e) const {
            return std::format("BattleStarted {} {} {}", e.heroName, e.enemyName, e.enemyId);
        }
        std::string operator()(const ev::TurnStarted& e) const {
            return std::format("TurnStarted {} {}", s(e.side), e.turn);
        }
        std::string operator()(const ev::TurnSkipped& e) const {
            return std::format("TurnSkipped {} {}", s(e.side), st(e.reason));
        }
        std::string operator()(const ev::Attacked& e) const {
            return std::format("Attacked {}", s(e.attacker));
        }
        std::string operator()(const ev::SkillCast& e) const {
            return std::format("SkillCast {} {}", e.skillId, e.mpSpent);
        }
        std::string operator()(const ev::HpHalved& e) const {
            return std::format("HpHalved {} {}", s(e.target), e.hpAfter);
        }
        std::string operator()(const ev::Damaged& e) const {
            return std::format("Damaged {} {} {} {}", s(e.target), e.amount, e.hpAfter, c(e.cause));
        }
        std::string operator()(const ev::Healed& e) const {
            return std::format("Healed {} {} {}", s(e.target), e.amount, e.hpAfter);
        }
        std::string operator()(const ev::StatusApplied& e) const {
            return std::format("StatusApplied {} {} {}", s(e.target), st(e.status), e.turns);
        }
        std::string operator()(const ev::StatusExpired& e) const {
            return std::format("StatusExpired {} {}", s(e.target), st(e.status));
        }
        std::string operator()(const ev::ExtraTurn& e) const {
            return std::format("ExtraTurn {}", s(e.side));
        }
        std::string operator()(const ev::Fled&) const { return "Fled"; }
        std::string operator()(const ev::Defeated& e) const {
            return std::format("Defeated {}", s(e.side));
        }
        std::string operator()(const ev::BattleEnded& e) const {
            return std::format("BattleEnded {} {}", o(e.outcome), e.turns);
        }
        std::string operator()(const ev::ScriptSaid& e) const {
            return std::format("ScriptSaid {} {}", s(e.side), e.text);
        }
        std::string operator()(const ev::ScriptError& e) const {
            return std::format("ScriptError {}", e.script);
        }
        std::string operator()(const ev::MpRestored& e) const {
            return std::format("MpRestored {} {} {}", s(e.target), e.amount, e.mpAfter);
        }
        std::string operator()(const ev::ItemUsed& e) const {
            return std::format("ItemUsed {}", e.itemId);
        }
    };
    return std::visit(Visitor{side, status, cause, outcome}, event);
}

} // namespace heroes::game
