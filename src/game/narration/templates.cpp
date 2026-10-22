#include "game/narration/templates.hpp"

#include "game/json_fields.hpp"
#include "game/narration/summary.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <sstream>

namespace heroes::game::narration {

const std::vector<std::string_view> kTemplateKeys{
    "BossIntro",          "BattleMoment.skill",     "BattleMoment.low_hp.hero", "BattleMoment.low_hp.foe",
    "BattleMoment.status", "BattleEnd.victory",     "BattleEnd.defeat",         "BattleEnd.fled",
    "BattleEnd.draw",      "FloorEnter",            "NpcGreeting.merchant",     "NpcGreeting.blacksmith",
    "NpcGreeting.guildmaster", "NpcGreeting.bank",  "NpcTalk"};

namespace {

using namespace game::detail;

constexpr std::array<std::string_view, 7> kPlaceholders{"hero", "foe", "location", "detail",
                                                        "floor", "npcName", "turns"};
constexpr std::array<std::string_view, 6> kKinds{"BossIntro", "BattleMoment", "BattleEnd",
                                                 "FloorEnter", "NpcGreeting", "NpcTalk"};

// Calls `each(name)` for every {name} in `line`.
template <class Each> void forPlaceholders(std::string_view line, Each each) {
    for (std::size_t open = line.find('{'); open != std::string_view::npos; open = line.find('{', open + 1)) {
        const std::size_t close = line.find('}', open);
        if (close == std::string_view::npos)
            return;
        each(line.substr(open + 1, close - open - 1));
    }
}

const Json* object(Fields& f, std::string_view key) {
    const Json* v = f.raw(key);
    if (!v || !v->is_object()) {
        f.fail(std::format("key '{}' must be an object", key));
        return nullptr;
    }
    return v;
}

} // namespace

std::expected<NarratorDef, std::vector<ContentError>> parseNarrator(std::string_view json) {
    Errors errors;
    Ctx ctx{errors, "narrator.json"};
    Json doc;
    try {
        doc = Json::parse(json);
    } catch (const Json::exception& e) {
        errors.push_back({ctx.file, "", e.what()});
        return std::unexpected(std::move(errors));
    }
    NarratorDef d{};
    Fields top(ctx, doc, "", {"providers", "limits", "prompts", "npcs", "templates"});
    if (const Json* providers = object(top, "providers"))
        for (const auto& [id, p] : providers->items()) {
            Fields f(ctx, p, "provider '" + id + "'", {"defaultModel", "defaultBaseUrl"});
            d.defaultModels[id] = f.req<std::string>("defaultModel");
            if (id == "local")
                d.localBaseUrl = f.req<std::string>("defaultBaseUrl");
        }
    if (const Json* limits = object(top, "limits")) {
        Fields f(ctx, *limits, "limits", {"narrationChars", "npcChars", "talkInputChars", "talkHistory", "timeoutMs"});
        d.limits = {f.req<int>("narrationChars", 1, 4000), f.req<int>("npcChars", 1, 4000),
                    f.req<int>("talkInputChars", 1, 1000), f.req<int>("talkHistory", 0, 50),
                    f.req<int>("timeoutMs", 100, 60000)};
    }
    if (const Json* prompts = object(top, "prompts")) {
        Fields f(ctx, *prompts, "prompts", {"narratorSystem", "npcSystem", "user"});
        d.narratorSystem = f.req<std::string>("narratorSystem");
        d.npcSystem = f.req<std::string>("npcSystem");
        if (const Json* user = object(f, "user")) {
            Fields u(ctx, *user, "prompts.user", {kKinds[0], kKinds[1], kKinds[2], kKinds[3], kKinds[4], kKinds[5]});
            for (const std::string_view kind : kKinds)
                d.userPrompts[std::string(kind)] = u.req<std::string>(kind);
        }
    }
    if (const Json* npcs = object(top, "npcs"))
        for (const auto& [id, n] : npcs->items()) {
            Fields f(ctx, n, "npc '" + id + "'", {"name", "persona"});
            d.npcs[id] = {f.req<std::string>("name"), f.req<std::string>("persona")};
        }
    if (const Json* templates = object(top, "templates")) {
        for (const auto& [key, lines] : templates->items()) {
            const std::string entry = "template '" + key + "'";
            if (std::ranges::find(kTemplateKeys, std::string_view(key)) == kTemplateKeys.end()) {
                errors.push_back({ctx.file, entry, "unknown template key"});
                continue;
            }
            if (!lines.is_array() || lines.empty()) {
                errors.push_back({ctx.file, entry, "must be a list of at least one line"});
                continue;
            }
            for (const Json& line : lines) {
                if (!line.is_string()) {
                    errors.push_back({ctx.file, entry, "lines must be strings"});
                    continue;
                }
                const std::string text = line.get<std::string>();
                forPlaceholders(text, [&](std::string_view name) {
                    if (std::ranges::find(kPlaceholders, name) == kPlaceholders.end())
                        errors.push_back({ctx.file, entry, std::format("unknown placeholder '{{{}}}'", name)});
                });
                d.templates[key].push_back(text);
            }
        }
        for (const std::string_view key : kTemplateKeys)
            if (!templates->contains(key))
                errors.push_back({ctx.file, "template '" + std::string(key) + "'", "missing"});
    }
    if (!errors.empty())
        return std::unexpected(std::move(errors));
    return d;
}

std::expected<NarratorDef, std::vector<ContentError>> loadNarrator(const std::filesystem::path& dataDir) {
    std::ifstream in(dataDir / "narrator.json", std::ios::binary);
    if (!in)
        return std::unexpected(std::vector<ContentError>{{"narrator.json", "", "cannot open file"}});
    std::ostringstream text;
    text << in.rdbuf();
    return parseNarrator(text.str());
}

std::string fillTemplate(const NarratorDef& def, std::string_view key,
                         const std::map<std::string, std::string, std::less<>>& values, Rng& rng) {
    const std::vector<std::string>& lines = def.templates.find(key)->second;
    const std::string& line = lines[rng.below(static_cast<std::uint32_t>(lines.size()))];
    std::string out;
    std::size_t from = 0;
    forPlaceholders(line, [&](std::string_view name) {
        const std::size_t open = static_cast<std::size_t>(name.data() - line.data()) - 1;
        out.append(line, from, open - from);
        if (const auto it = values.find(name); it != values.end())
            out += it->second;
        from = open + name.size() + 2;
    });
    out.append(line, from);
    return out;
}

std::string narrateOffline(const NarratorDef& def, const NarrationRequest& request, Rng& rng) {
    const nlohmann::json& s = request.summary;
    std::map<std::string, std::string, std::less<>> values;
    for (const auto& [key, value] : s.items())
        values[key] = value.is_string() ? value.get<std::string>() : value.dump();
    if (const auto npc = def.npcs.find(request.npcId); npc != def.npcs.end())
        values["npcName"] = npc->second.name;
    std::string key;
    switch (request.kind) {
    case NarrationKind::BossIntro:
        key = "BossIntro";
        break;
    case NarrationKind::BattleMoment:
        key = "BattleMoment." + s.value("moment", "status");
        if (key == "BattleMoment.low_hp")
            key += "." + s.value("detail", "hero");
        break;
    case NarrationKind::BattleEnd:
        key = "BattleEnd." + s.value("result", "draw");
        break;
    case NarrationKind::FloorEnter:
        key = "FloorEnter";
        break;
    case NarrationKind::NpcGreeting:
        key = "NpcGreeting." + request.npcId;
        break;
    case NarrationKind::NpcTalk:
        key = "NpcTalk";
        break;
    }
    if (!def.templates.contains(key))
        return {};
    return fillTemplate(def, key, values, rng);
}

} // namespace heroes::game::narration
