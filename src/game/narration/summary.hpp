#pragma once

#include "game/content.hpp"
#include "game/events.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace heroes::game::narration {

enum class NarrationKind { BossIntro, BattleMoment, BattleEnd, FloorEnter, NpcGreeting, NpcTalk };

struct TalkTurn {
    std::string player, npc;
};

// What to narrate, built only from game data (reference/llm-interfaces.md §2).
struct NarrationRequest {
    NarrationKind kind;
    nlohmann::json summary;
    std::string npcId;             // NpcGreeting and NpcTalk
    std::vector<TalkTurn> history; // NpcTalk, oldest first
    std::string playerInput;       // NpcTalk
    std::uint64_t ticket = 0;      // set by the narrator
};

// One battle's running notes for narration: who is fighting, the HP seen so far, which low-HP
// moments have fired, and the moments so far for the ending.
struct BattleContext {
    const Content* content;
    std::string hero, foe;
    int heroHp, heroMaxHp, foeHp, foeMaxHp;
    bool heroLow = false, foeLow = false;
    std::vector<std::string> moments; // at most 5, in order
};

// The request one player action's events call for: the battle's end when it ended in them
// (superseding any moment), else the first moment among them (a skill cast, the first time a
// side falls to 20% HP or less, or a stun or reflect applied), else none. Updates `ctx`; reads
// nothing else and draws no random numbers.
std::optional<NarrationRequest> summarizeBattleChunk(std::span<const Event> events, BattleContext& ctx);

} // namespace heroes::game::narration
