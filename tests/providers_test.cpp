#include "net/providers.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace heroes;
using namespace heroes::net;
using game::narration::NarrationKind;
using game::narration::NarrationRequest;
using Json = nlohmann::json;
using Kind = ProviderError::Kind;

namespace {

const std::filesystem::path kSource = HEROES_SOURCE_DIR;

const game::narration::NarratorDef& narrator() {
    static const auto def = game::narration::loadNarrator(kSource / "data").value();
    return def;
}

Json fixture(const std::string& name) {
    std::ifstream in(kSource / "tests/fixtures/llm" / name, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return Json::parse(text.str());
}

NarrationRequest battleEnd() {
    return {NarrationKind::BattleEnd,
            {{"hero", "Aria"}, {"foe", "Medusa"}, {"result", "victory"}, {"turns", 9}, {"moments", {"Exorcism"}}, {"heroHpPct", 12}},
            {}, {}, {}};
}

std::string header(const HttpRequest& r, const std::string& name) {
    for (const auto& [k, v] : r.headers)
        if (k == name)
            return v;
    return "(none)";
}

HttpResponse ok(const Json& body) { return {200, body.dump(), std::nullopt}; }

} // namespace

TEST(Providers, AnthropicBuildsTheMessagesRequest) {
    const auto p = anthropicProvider(narrator(), {"", "test-key", ""});
    const HttpRequest r = p->build(battleEnd());
    EXPECT_EQ(r.url, "https://api.anthropic.com/v1/messages");
    EXPECT_EQ(header(r, "x-api-key"), "test-key");
    EXPECT_EQ(header(r, "anthropic-version"), "2023-06-01");
    EXPECT_EQ(r.timeoutMs, 8000);
    EXPECT_EQ(Json::parse(r.body), fixture("anthropic_request.json"));
}

TEST(Providers, GeminiBuildsTheGenerateContentRequest) {
    const auto p = geminiProvider(narrator(), {"", "test-key", ""});
    const HttpRequest r = p->build(battleEnd());
    EXPECT_EQ(r.url, "https://generativelanguage.googleapis.com/v1beta/models/gemini-3.5-flash-lite:generateContent");
    EXPECT_EQ(header(r, "x-goog-api-key"), "test-key");
    EXPECT_EQ(Json::parse(r.body), fixture("gemini_request.json"));
}

TEST(Providers, OpenAiBuildsTheChatRequestAndLocalNeedsNoKey) {
    const auto p = openAiProvider(narrator(), {"gpt-test", "test-key", ""}, false);
    const HttpRequest r = p->build(battleEnd());
    EXPECT_EQ(r.url, "https://api.openai.com/v1/chat/completions");
    EXPECT_EQ(header(r, "authorization"), "Bearer test-key");
    EXPECT_EQ(Json::parse(r.body), fixture("openai_request.json"));

    const auto local = openAiProvider(narrator(), {"llama3.2", "", ""}, true);
    const HttpRequest l = local->build(battleEnd());
    EXPECT_EQ(l.url, "http://localhost:11434/v1/chat/completions");
    EXPECT_EQ(header(l, "authorization"), "(none)");
    EXPECT_EQ(Json::parse(l.body)["max_tokens"], 512);
}

TEST(Providers, NpcTalkCarriesThePersonaHistoryAndInput) {
    NarrationRequest r{NarrationKind::NpcTalk, {{"hero", "Aria"}, {"level", 3}, {"heroHpPct", 100}, {"npc", "merchant"}},
                       "merchant", {{"Hello", "Gold talks."}}, "Any discounts?"};
    const Json body = Json::parse(anthropicProvider(narrator(), {"", "k", ""})->build(r).body);
    EXPECT_NE(body["system"].get<std::string>().find("Marlo the Merchant, a greedy but jovial trader"), std::string::npos);
    ASSERT_EQ(body["messages"].size(), 3u);
    EXPECT_EQ(body["messages"][0]["content"], "<player>Hello</player>");
    EXPECT_EQ(body["messages"][1]["role"], "assistant");
    EXPECT_NE(body["messages"][2]["content"].get<std::string>().find("<player>Any discounts?</player>"), std::string::npos);
    const Json gemini = Json::parse(geminiProvider(narrator(), {"", "k", ""})->build(r).body);
    EXPECT_EQ(gemini["contents"][1]["role"], "model");
}

TEST(Providers, AnthropicReadsTextAfterThinkingAndSpotsRefusals) {
    const auto p = anthropicProvider(narrator(), {});
    EXPECT_EQ(*p->parse(ok({{"stop_reason", "end_turn"},
                            {"content", {{{"type", "thinking"}, {"thinking", "hmm"}}, {{"type", "text"}, {"text", "You strike."}}}}})),
              "You strike.");
    EXPECT_EQ(p->parse(ok({{"stop_reason", "refusal"}, {"content", {{{"type", "text"}, {"text", "x"}}}}})).error().kind, Kind::Refused);
    EXPECT_EQ(p->parse(ok({{"content", Json::array()}})).error().kind, Kind::BadResponse);
    EXPECT_EQ(p->parse({200, "not json", std::nullopt}).error().kind, Kind::BadResponse);
    EXPECT_EQ(p->parse(ok({{"stop_reason", 5}})).error().kind, Kind::BadResponse); // a field of the wrong type
}

TEST(Providers, GeminiAndOpenAiSpotTheirRefusals) {
    const auto g = geminiProvider(narrator(), {});
    EXPECT_EQ(*g->parse(ok({{"candidates", {{{"content", {{"parts", {{{"text", "You "}}, {{"text", "win."}}}}}}}}}})), "You win.");
    EXPECT_EQ(g->parse(ok({{"candidates", {{{"finishReason", "SAFETY"}}}}})).error().kind, Kind::Refused);
    EXPECT_EQ(g->parse(ok({{"promptFeedback", {{"blockReason", "SAFETY"}}}})).error().kind, Kind::Refused);
    const auto o = openAiProvider(narrator(), {}, false);
    EXPECT_EQ(*o->parse(ok({{"choices", {{{"finish_reason", "stop"}, {"message", {{"content", "Hi."}}}}}}})), "Hi.");
    EXPECT_EQ(o->parse(ok({{"choices", {{{"finish_reason", "content_filter"}, {"message", {{"content", ""}}}}}}})).error().kind,
              Kind::Refused);
}

TEST(Providers, StatusesMapTheSameForEveryProvider) {
    EXPECT_EQ(statusError({401, "", {}})->kind, Kind::Auth);
    EXPECT_EQ(statusError({403, "", {}})->kind, Kind::Auth);
    EXPECT_EQ(statusError({404, "", {}})->kind, Kind::UnknownModel);
    EXPECT_EQ(*statusError({429, "", 120}), (ProviderError{Kind::RateLimited, 120}));
    EXPECT_EQ(*statusError({429, "", std::nullopt}), (ProviderError{Kind::RateLimited, 60}));
    EXPECT_EQ(statusError({529, "", {}})->kind, Kind::Server);
    EXPECT_EQ(statusError({500, "", {}})->kind, Kind::Server);
    EXPECT_EQ(statusError({400, "", {}})->kind, Kind::BadResponse);
    EXPECT_FALSE(statusError({200, "", {}}));
}
