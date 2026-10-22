#pragma once

#include "game/content_loader.hpp"
#include "game/rng.hpp"

#include <expected>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace heroes::game::narration {

struct NpcDef {
    std::string name, persona;
};

// data/narrator.json: provider defaults, limits, prompts, the four town NPCs, and the offline
// template lines (reference/llm-interfaces.md §3).
struct NarratorDef {
    std::map<std::string, std::string, std::less<>> defaultModels; // by provider id
    std::string localBaseUrl;
    struct {
        int narrationChars, npcChars, talkInputChars, talkHistory, timeoutMs;
    } limits;
    std::string narratorSystem, npcSystem;
    std::map<std::string, std::string, std::less<>> userPrompts; // by kind name ("BattleEnd")
    std::map<std::string, NpcDef, std::less<>> npcs;              // by id ("merchant")
    std::map<std::string, std::vector<std::string>, std::less<>> templates; // by key
};

// The template keys every narrator.json must have, each with at least one line.
extern const std::vector<std::string_view> kTemplateKeys;

// Parses narrator.json; every error is reported in the file "narrator.json", naming the key. A
// template may only use {hero} {foe} {location} {detail} {floor} {npcName} {turns}.
std::expected<NarratorDef, std::vector<ContentError>> parseNarrator(std::string_view json);
std::expected<NarratorDef, std::vector<ContentError>> loadNarrator(const std::filesystem::path& dataDir);

// One of `key`'s lines, picked with `rng`, with each {name} replaced from `values` (a name it
// lacks is left out). Precondition: `key` is one of kTemplateKeys.
std::string fillTemplate(const NarratorDef& def, std::string_view key,
                         const std::map<std::string, std::string, std::less<>>& values, Rng& rng);

} // namespace heroes::game::narration
