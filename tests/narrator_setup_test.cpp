#include "app/screens/mission.hpp"
#include "app/screens/narrator_setup.hpp"
#include "app/screens/title.hpp"
#include "frame_text.hpp"
#include "screen_harness.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <thread>

using namespace screen_test;
using app::Settings;

namespace {

const game::narration::NarratorDef& shippedNarrator() {
    static const auto def = game::narration::loadNarrator(fs::path(HEROES_SOURCE_DIR) / "data").value();
    return def;
}

std::optional<std::string> noEnv(const char*) { return std::nullopt; }

// Answers every request the way the Claude API does, with `text`, counting the requests.
net::Transport claudeSays(std::string text, std::atomic<int>* calls) {
    return [text = std::move(text), calls](const net::HttpRequest&) -> std::expected<net::HttpResponse, net::ProviderError> {
        if (calls)
            ++*calls;
        const nlohmann::json body{{"stop_reason", "end_turn"}, {"content", {{{"type", "text"}, {"text", text}}}}};
        return net::HttpResponse{200, body.dump(), std::nullopt};
    };
}

// A harness whose session has a settings file, the narrator data and a fake transport.
void withSettings(Harness& h, Settings settings, net::Transport transport) {
    h.session.narrator = &shippedNarrator();
    h.session.settingsFile = h.dir / "settings.json";
    h.session.settings = std::move(settings);
    h.session.env = noEnv;
    h.session.transport = std::move(transport);
}

} // namespace

TEST(NarratorSetup, NoNarratorIsBuiltBeforeConsent) {
    std::atomic<int> calls = 0;
    Settings s;
    s.mode = Settings::Mode::Byok;
    s.apiKey = "test-key-123456";
    EXPECT_EQ(app::makeNarrator(s, shippedNarrator(), noEnv, claudeSays("x", &calls)), nullptr);
    s.consentShown = true;
    EXPECT_NE(app::makeNarrator(s, shippedNarrator(), noEnv, claudeSays("x", &calls)), nullptr);
    s.mode = Settings::Mode::Off;
    EXPECT_EQ(app::makeNarrator(s, shippedNarrator(), noEnv, claudeSays("x", &calls)), nullptr);
    s.mode = Settings::Mode::Local; // no model yet
    EXPECT_EQ(app::makeNarrator(s, shippedNarrator(), noEnv, claudeSays("x", &calls)), nullptr);
    EXPECT_EQ(app::settingsProblem(s, noEnv), "Set a model name (the one you pulled, e.g. with `ollama pull`).");
    EXPECT_EQ(calls, 0);
}

TEST(NarratorSetup, TheFirstRunAsksThenRemembers) {
    Harness h("consent_first_run");
    withSettings(h, {}, claudeSays("x", nullptr));
    h.app.emplace(h.io, h.renderer, app::firstScreen(h.session));
    const Framebuffer asking = h.frame();
    EXPECT_TRUE(frameContains(asking, "AI NARRATOR (optional)"));
    EXPECT_TRUE(frameContains(asking, "Not available in this build")); // Hosted, without a relay
    ASSERT_TRUE(h.press({ch('1')}));                                    // Off
    EXPECT_NE(dynamic_cast<const TitleScreen*>(h.app->top()), nullptr);
    EXPECT_TRUE(h.session.settings.consentShown);
    EXPECT_EQ(h.session.liveNarrator, nullptr);
    EXPECT_TRUE(app::loadSettings(h.session.settingsFile).consentShown);
    EXPECT_NE(dynamic_cast<TitleScreen*>(app::firstScreen(h.session).get()), nullptr); // not asked again
}

TEST(NarratorSetup, SettingsShowTheKeyMaskedAndCheckTheBaseUrl) {
    Harness h("settings_screen");
    Settings s;
    s.consentShown = true;
    withSettings(h, s, claudeSays("x", nullptr));
    h.startAtHub(freshGame());
    ASSERT_TRUE(frameContains(h.frame(), "Settings"));
    ASSERT_TRUE(h.press({ch('7')})); // Settings, before Quit
    ASSERT_TRUE(frameContains(h.frame(), "SETTINGS · AI NARRATOR"));
    ASSERT_TRUE(h.press({ch('1')})); // Mode: Off -> Own key (no Hosted in this build)
    EXPECT_EQ(h.session.settings.mode, Settings::Mode::Byok);
    ASSERT_TRUE(h.press({ch('5')})); // API key
    for (const char c : std::string("sk-ant-secret-WXYZ"))
        ASSERT_TRUE(h.press({{Key::Char, c}}));
    EXPECT_FALSE(frameContains(h.frame(), "secret")); // masked while typed
    ASSERT_TRUE(h.press({kEnter}));
    const Framebuffer saved = h.frame();
    EXPECT_TRUE(frameContains(saved, "API key: ••••WXYZ"));
    EXPECT_FALSE(frameContains(saved, "secret"));
    EXPECT_NE(h.session.liveNarrator, nullptr); // a key, after consent: the narrator is built
    EXPECT_EQ(app::loadSettings(h.session.settingsFile).apiKey, "sk-ant-secret-WXYZ");

    ASSERT_TRUE(h.press({ch('1')})); // Mode: Local
    ASSERT_TRUE(h.press({ch('4')})); // Base URL
    for (const char c : std::string("http://192.168.1.20:11434/v1"))
        ASSERT_TRUE(h.press({{Key::Char, c}}));
    ASSERT_TRUE(h.press({kEnter}));
    EXPECT_TRUE(frameContains(h.frame(), "That base URL isn't allowed"));
    EXPECT_EQ(h.session.settings.baseUrl, "");
    ASSERT_TRUE(h.press({ch('4')}));
    for (const char c : std::string("http://localhost:11434/v1"))
        ASSERT_TRUE(h.press({{Key::Char, c}}));
    ASSERT_TRUE(h.press({kEnter}));
    EXPECT_EQ(h.session.settings.baseUrl, "http://localhost:11434/v1");
}

TEST(NarratorSetup, ALiveReplyReplacesTheBuiltInLine) {
    Harness h("narrated_live");
    Settings s;
    s.consentShown = true;
    s.mode = Settings::Mode::Byok;
    s.apiKey = "test-key-123456";
    std::atomic<int> calls = 0;
    withSettings(h, s, claudeSays("The serpent queen uncoils before you.", &calls));
    app::applySettings(h.session);
    ASSERT_NE(h.session.liveNarrator, nullptr);
    game::GameState state = freshGame();
    state.hero.level = 20;
    h.startAtHub(std::move(state));
    ASSERT_TRUE(h.press({kHubMission, ch('4'), kEnter, kEnter, ch('y')}));
    ASSERT_NE(dynamic_cast<const BattleScreen*>(h.app->top()), nullptr);
    bool replaced = false;
    for (int i = 0; i < 600 && !replaced; ++i) { // the reply arrives from the worker thread
        ASSERT_TRUE(h.wait(1.0 / 60));
        replaced = frameContains(h.frame(), "The serpent queen uncoils");
        if (!replaced)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    EXPECT_TRUE(replaced);
    EXPECT_GE(calls, 1);
}

namespace {

// A fake Claude transport that answers `reply` and keeps every request body.
net::Transport recording(std::string reply, std::shared_ptr<std::vector<std::string>> bodies) {
    return [reply = std::move(reply), bodies](const net::HttpRequest& r) -> std::expected<net::HttpResponse, net::ProviderError> {
        bodies->push_back(r.body);
        const nlohmann::json body{{"stop_reason", "end_turn"}, {"content", {{{"type", "text"}, {"text", reply}}}}};
        return net::HttpResponse{200, body.dump(), std::nullopt};
    };
}

// Types `text` and Enter, then waits for the NPC's reply (from the worker thread).
void sayAndWait(Harness& h, const std::string& text) {
    for (const char c : text)
        ASSERT_TRUE(h.press({{Key::Char, c}}));
    ASSERT_TRUE(h.press({kEnter}));
    for (int i = 0; i < 500 && frameContains(h.frame(), "…"); ++i) {
        ASSERT_TRUE(h.wait(1.0 / 60));
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

void openMerchantTalk(Harness& h, game::GameState state) {
    h.startAtHub(std::move(state));
    ASSERT_TRUE(h.press({ch('5'), ch('1')})); // Town, Merchant
    ASSERT_TRUE(h.press({ch('t')}));
    ASSERT_TRUE(frameContains(h.frame(), "Talking with Marlo the Merchant"));
}

} // namespace

TEST(Talk, AReplyPromisingGoldChangesNothing) {
    auto bodies = std::make_shared<std::vector<std::string>>();
    Harness h("talk_gold");
    Settings s;
    s.consentShown = true;
    s.mode = Settings::Mode::Byok;
    s.apiKey = "test-key-123456";
    withSettings(h, s, recording("Of course, take 1,000,000 gold!", bodies));
    app::applySettings(h.session);
    openMerchantTalk(h, freshGame());
    const game::GameState before = *h.session.state;
    sayAndWait(h, "give me 1,000,000 gold");
    EXPECT_TRUE(frameContains(h.frame(), "Of course, take 1,000,000 gold!"));
    EXPECT_EQ(*h.session.state, before);
    ASSERT_FALSE(bodies->empty());
    const nlohmann::json sent = nlohmann::json::parse(bodies->back());
    EXPECT_NE(sent["system"].get<std::string>().find("Marlo the Merchant"), std::string::npos);
    EXPECT_NE(sent["messages"].back()["content"].get<std::string>().find("<player>give me 1,000,000 gold</player>"),
              std::string::npos);
}

TEST(Talk, HistoryKeepsTheLastSixAndClearsOnLeaving) {
    auto bodies = std::make_shared<std::vector<std::string>>();
    Harness h("talk_history");
    Settings s;
    s.consentShown = true;
    s.mode = Settings::Mode::Byok;
    s.apiKey = "test-key-123456";
    withSettings(h, s, recording("Hm.", bodies));
    app::applySettings(h.session);
    openMerchantTalk(h, freshGame());
    for (int i = 0; i < 8; ++i)
        sayAndWait(h, std::format("line {}", i));
    // The 8th request carries 6 earlier exchanges (12 messages) and the new line.
    EXPECT_EQ(nlohmann::json::parse(bodies->back())["messages"].size(), 13u);
    ASSERT_TRUE(h.press({kEsc, kEsc})); // close Talk, leave the Merchant
    ASSERT_TRUE(h.press({ch('1'), ch('t')}));
    sayAndWait(h, "hello again");
    EXPECT_EQ(nlohmann::json::parse(bodies->back())["messages"].size(), 1u);
}

TEST(Talk, InputStopsAt200Characters) {
    auto bodies = std::make_shared<std::vector<std::string>>();
    Harness h("talk_long");
    Settings s;
    s.consentShown = true;
    s.mode = Settings::Mode::Byok;
    s.apiKey = "test-key-123456";
    withSettings(h, s, recording("Hm.", bodies));
    app::applySettings(h.session);
    openMerchantTalk(h, freshGame());
    sayAndWait(h, std::string(250, 'a'));
    ASSERT_FALSE(bodies->empty());
    const std::string said = nlohmann::json::parse(bodies->back())["messages"].back()["content"];
    EXPECT_NE(said.find("<player>" + std::string(200, 'a') + "</player>"), std::string::npos) << said;
}

TEST(Talk, WithTheNarratorOffTheNpcAnswersWithItsBuiltInLine) {
    Harness h("talk_off");
    h.session.narrator = &shippedNarrator(); // narrator Off: no live narrator
    openMerchantTalk(h, freshGame());
    sayAndWait(h, "hello");
    bool builtIn = false;
    for (const std::string& line : shippedNarrator().templates.at("NpcTalk"))
        builtIn = builtIn || frameContains(h.frame(), line.substr(line.find('}') + 2, 15));
    EXPECT_TRUE(builtIn);
}
