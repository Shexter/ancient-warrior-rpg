#include "net/providers.hpp"

namespace heroes::net {

std::optional<ProviderError> statusError(const HttpResponse& response) {
    using Kind = ProviderError::Kind;
    const int s = response.status;
    if (s >= 200 && s < 300)
        return std::nullopt;
    if (s == 401 || s == 403)
        return ProviderError{Kind::Auth};
    if (s == 404)
        return ProviderError{Kind::UnknownModel};
    if (s == 429)
        return ProviderError{Kind::RateLimited, response.retryAfterSeconds.value_or(60)};
    if (s >= 500)
        return ProviderError{Kind::Server};
    return ProviderError{Kind::BadResponse};
}

std::expected<std::string, ProviderError> complete(const Provider& provider, const Transport& transport,
                                                   const game::narration::NarrationRequest& request) {
    const auto response = transport(provider.build(request));
    if (!response)
        return std::unexpected(response.error());
    if (const auto error = statusError(*response))
        return std::unexpected(*error);
    return provider.parse(*response);
}

} // namespace heroes::net

// ---- Direct providers ---------------------------------------------------------------------
// Request and response shapes:
// - Anthropic: as given in reference/llm-interfaces.md §5 (checked against the Claude API
//   reference when the change was written).
// - Gemini: https://ai.google.dev/api/generate-content and
//   https://ai.google.dev/gemini-api/docs/models, read 2026-10-22. The default model is
//   gemini-3.5-flash-lite, the docs' fastest, cheapest stable Flash-Lite.
// - OpenAI: the chat completions reference wasn't readable without signing in (2026-10-22);
//   OpenAI's docs mark max_tokens deprecated in favour of max_completion_tokens, so OpenAI gets
//   the new name and local servers (Ollama) keep max_tokens.

namespace heroes::net {

namespace {

using game::narration::NarrationKind;
using game::narration::NarrationRequest;
using game::narration::NarratorDef;
using Json = nlohmann::json;
using Kind = ProviderError::Kind;

std::string_view kindName(NarrationKind kind) {
    switch (kind) {
    case NarrationKind::BossIntro:
        return "BossIntro";
    case NarrationKind::BattleMoment:
        return "BattleMoment";
    case NarrationKind::BattleEnd:
        return "BattleEnd";
    case NarrationKind::FloorEnter:
        return "FloorEnter";
    case NarrationKind::NpcGreeting:
        return "NpcGreeting";
    case NarrationKind::NpcTalk:
        return "NpcTalk";
    }
    return "BattleMoment";
}

std::string replaceAll(std::string text, std::string_view from, std::string_view to) {
    for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
        text.replace(at, from.size(), to);
    return text;
}

// The conversation for a request: the system prompt, then user/assistant turns ending with the user.
struct Messages {
    std::string system;
    std::vector<std::pair<bool, std::string>> turns; // (from the player, text)
};

Messages messagesFor(const NarratorDef& def, const NarrationRequest& r) {
    Messages m;
    const bool npc = r.kind == NarrationKind::NpcGreeting || r.kind == NarrationKind::NpcTalk;
    if (npc) {
        const auto it = def.npcs.find(r.npcId);
        const std::string name = it != def.npcs.end() ? it->second.name : r.npcId;
        const std::string persona = it != def.npcs.end() ? it->second.persona : "";
        m.system = replaceAll(replaceAll(def.npcSystem, "{npcName}", name), "{persona}", persona);
    } else {
        m.system = def.narratorSystem;
    }
    for (const game::narration::TalkTurn& t : r.history) {
        m.turns.push_back({true, "<player>" + t.player + "</player>"});
        m.turns.push_back({false, t.npc});
    }
    const std::string user = def.userPrompts.at(std::string(kindName(r.kind)));
    m.turns.push_back({true, replaceAll(replaceAll(user, "{summary}", r.summary.dump()), "{input}", r.playerInput)});
    return m;
}

std::string modelOr(const NarratorDef& def, const ProviderConfig& c, std::string_view provider) {
    if (!c.model.empty())
        return c.model;
    const auto it = def.defaultModels.find(provider);
    return it != def.defaultModels.end() ? it->second : std::string();
}

// Parses the body as JSON for `read`; malformed JSON, or a field of the wrong type, is BadResponse.
class JsonProvider : public Provider {
  public:
    std::expected<std::string, ProviderError> parse(const HttpResponse& r) const final {
        try {
            const Json j = Json::parse(r.body);
            if (!j.is_object())
                return std::unexpected(ProviderError{ProviderError::Kind::BadResponse});
            return read(j);
        } catch (const Json::exception&) {
            return std::unexpected(ProviderError{ProviderError::Kind::BadResponse});
        }
    }

  private:
    virtual std::expected<std::string, ProviderError> read(const Json& j) const = 0;
};

std::expected<std::string, ProviderError> textOr(std::string text) {
    if (text.empty())
        return std::unexpected(ProviderError{Kind::BadResponse});
    return text;
}

class Anthropic final : public JsonProvider {
  public:
    Anthropic(const NarratorDef& def, ProviderConfig c) : def_(def), c_(std::move(c)) {}
    HttpRequest build(const NarrationRequest& r) const override {
        const Messages m = messagesFor(def_, r);
        Json messages = Json::array();
        for (const auto& [player, text] : m.turns)
            messages.push_back({{"role", player ? "user" : "assistant"}, {"content", text}});
        const Json body{{"model", modelOr(def_, c_, "anthropic")},
                        {"max_tokens", 2048},
                        {"output_config", {{"effort", "low"}}},
                        {"fallbacks", "default"},
                        {"system", m.system},
                        {"messages", std::move(messages)}};
        return {"https://api.anthropic.com/v1/messages",
                {{"content-type", "application/json"},
                 {"x-api-key", c_.apiKey},
                 {"anthropic-version", "2023-06-01"},
                 {"anthropic-beta", "server-side-fallback-2026-07-01"}},
                body.dump(),
                def_.limits.timeoutMs};
    }

  private:
    std::expected<std::string, ProviderError> read(const Json& json) const override {
        const Json* j = &json;
        if (j->value("stop_reason", "") == "refusal") // before reading content
            return std::unexpected(ProviderError{Kind::Refused});
        std::string text;
        if (const auto it = j->find("content"); it != j->end() && it->is_array())
            for (const Json& block : *it)
                if (block.is_object() && block.value("type", "") == "text")
                    text += block.value("text", "");
        return textOr(std::move(text));
    }

    const NarratorDef& def_;
    ProviderConfig c_;
};

class Gemini final : public JsonProvider {
  public:
    Gemini(const NarratorDef& def, ProviderConfig c) : def_(def), c_(std::move(c)) {}
    HttpRequest build(const NarrationRequest& r) const override {
        const Messages m = messagesFor(def_, r);
        Json contents = Json::array();
        for (const auto& [player, text] : m.turns)
            contents.push_back({{"role", player ? "user" : "model"}, {"parts", {{{"text", text}}}}});
        const Json body{{"systemInstruction", {{"parts", {{{"text", m.system}}}}}},
                        {"contents", std::move(contents)},
                        {"generationConfig", {{"maxOutputTokens", 512}}}};
        return {"https://generativelanguage.googleapis.com/v1beta/models/" + modelOr(def_, c_, "gemini") +
                    ":generateContent",
                {{"content-type", "application/json"}, {"x-goog-api-key", c_.apiKey}},
                body.dump(),
                def_.limits.timeoutMs};
    }

  private:
    std::expected<std::string, ProviderError> read(const Json& json) const override {
        const Json* j = &json;
        if (const auto f = j->find("promptFeedback"); f != j->end() && f->is_object() && f->contains("blockReason"))
            return std::unexpected(ProviderError{Kind::Refused});
        const auto candidates = j->find("candidates");
        if (candidates == j->end() || !candidates->is_array() || candidates->empty())
            return std::unexpected(ProviderError{Kind::BadResponse});
        const Json& first = (*candidates)[0];
        if (!first.is_object())
            return std::unexpected(ProviderError{Kind::BadResponse});
        if (first.value("finishReason", "") == "SAFETY")
            return std::unexpected(ProviderError{Kind::Refused});
        std::string text;
        if (const auto content = first.find("content"); content != first.end() && content->is_object())
            if (const auto parts = content->find("parts"); parts != content->end() && parts->is_array())
                for (const Json& part : *parts)
                    if (part.is_object())
                        text += part.value("text", "");
        return textOr(std::move(text));
    }

    const NarratorDef& def_;
    ProviderConfig c_;
};

class OpenAi final : public JsonProvider {
  public:
    OpenAi(const NarratorDef& def, ProviderConfig c, bool local) : def_(def), c_(std::move(c)), local_(local) {}
    HttpRequest build(const NarrationRequest& r) const override {
        const Messages m = messagesFor(def_, r);
        Json messages = Json::array({{{"role", "system"}, {"content", m.system}}});
        for (const auto& [player, text] : m.turns)
            messages.push_back({{"role", player ? "user" : "assistant"}, {"content", text}});
        Json body{{"model", modelOr(def_, c_, local_ ? "local" : "openai")}, {"messages", std::move(messages)}};
        body[local_ ? "max_tokens" : "max_completion_tokens"] = 512;
        std::string base = c_.baseUrl.empty() ? (local_ ? def_.localBaseUrl : "https://api.openai.com/v1") : c_.baseUrl;
        while (base.ends_with('/'))
            base.pop_back();
        HttpRequest out{base + "/chat/completions", {{"content-type", "application/json"}}, body.dump(),
                        def_.limits.timeoutMs};
        if (!c_.apiKey.empty())
            out.headers.push_back({"authorization", "Bearer " + c_.apiKey});
        return out;
    }

  private:
    std::expected<std::string, ProviderError> read(const Json& json) const override {
        const Json* j = &json;
        const auto choices = j->find("choices");
        if (choices == j->end() || !choices->is_array() || choices->empty() || !(*choices)[0].is_object())
            return std::unexpected(ProviderError{Kind::BadResponse});
        const Json& first = (*choices)[0];
        if (first.value("finish_reason", "") == "content_filter")
            return std::unexpected(ProviderError{Kind::Refused});
        const auto message = first.find("message");
        if (message == first.end() || !message->is_object())
            return std::unexpected(ProviderError{Kind::BadResponse});
        const auto content = message->find("content");
        return textOr(content != message->end() && content->is_string() ? content->get<std::string>() : "");
    }

    const NarratorDef& def_;
    ProviderConfig c_;
    bool local_;
};

class Relay final : public JsonProvider {
  public:
    Relay(const NarratorDef& def, std::string url, std::string installId)
        : def_(def), url_(std::move(url)), installId_(std::move(installId)) {}
    HttpRequest build(const NarrationRequest& r) const override {
        Json body{{"v", 1}, {"installId", installId_}, {"kind", kindName(r.kind)}, {"summary", r.summary}};
        if (r.kind == NarrationKind::NpcGreeting || r.kind == NarrationKind::NpcTalk)
            body["npc"] = r.npcId;
        if (r.kind == NarrationKind::NpcTalk) {
            Json history = Json::array();
            for (const game::narration::TalkTurn& t : r.history)
                history.push_back({{"player", t.player}, {"npc", t.npc}});
            body["history"] = std::move(history);
            body["input"] = r.playerInput;
        }
        std::string base = url_;
        while (base.ends_with('/'))
            base.pop_back();
        return {base + "/v1/narrate", {{"content-type", "application/json"}}, body.dump(), def_.limits.timeoutMs};
    }

  private:
    std::expected<std::string, ProviderError> read(const Json& j) const override {
        const auto text = j.find("text");
        return textOr(text != j.end() && text->is_string() ? text->get<std::string>() : "");
    }

    const NarratorDef& def_;
    std::string url_, installId_;
};

} // namespace

std::unique_ptr<Provider> anthropicProvider(const NarratorDef& def, ProviderConfig config) {
    return std::make_unique<Anthropic>(def, std::move(config));
}
std::unique_ptr<Provider> geminiProvider(const NarratorDef& def, ProviderConfig config) {
    return std::make_unique<Gemini>(def, std::move(config));
}
std::unique_ptr<Provider> relayProvider(const NarratorDef& def, std::string relayUrl, std::string installId) {
    return std::make_unique<Relay>(def, std::move(relayUrl), std::move(installId));
}
std::unique_ptr<Provider> openAiProvider(const NarratorDef& def, ProviderConfig config, bool local) {
    return std::make_unique<OpenAi>(def, std::move(config), local);
}

} // namespace heroes::net
