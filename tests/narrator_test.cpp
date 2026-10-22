#include "battle_golden_data.hpp"
#include "event_format.hpp"
#include "game/battle.hpp"
#include "game/narration/summary.hpp"
#include "net/narrator.hpp"
#include "shipped_content.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <sstream>
#include <thread>

using namespace heroes;
using namespace heroes::net;
using game::narration::NarrationKind;
using game::narration::NarrationRequest;
using Kind = ProviderError::Kind;

namespace {

// Sends the summary's "say" as the request body and reads the body back as the reply.
class EchoProvider final : public Provider {
  public:
    HttpRequest build(const NarrationRequest& r) const override { return {"fake://", {}, r.summary.value("say", ""), 1000}; }
    std::expected<std::string, ProviderError> parse(const HttpResponse& r) const override { return r.body; }
};

// A transport the test holds shut until it opens it, answering with `status` (and a retry-after),
// counting calls.
struct Gate {
    std::mutex m;
    std::condition_variable cv;
    bool open = true;
    int status = 200;
    std::optional<int> retryAfter;
    std::atomic<int> calls{0}, entered{0};

    Transport transport() {
        return [this](const HttpRequest& r) -> std::expected<HttpResponse, ProviderError> {
            ++calls;
            ++entered;
            std::unique_lock lock(m);
            cv.wait(lock, [&] { return open; });
            return HttpResponse{status, r.body, retryAfter};
        };
    }
    void set(bool isOpen) {
        {
            std::lock_guard lock(m);
            open = isOpen;
        }
        cv.notify_all();
    }
};

NarrationRequest say(std::string text) {
    return {NarrationKind::BattleMoment, {{"say", std::move(text)}}, {}, {}, {}};
}

// Polls until a reply comes back (or 2 s pass).
std::optional<NarrationReply> next(Narrator& n) {
    for (int i = 0; i < 2000; ++i) {
        if (auto r = n.poll())
            return r;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return std::nullopt;
}

void waitFor(const std::atomic<int>& count, int value) {
    for (int i = 0; i < 2000 && count < value; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

} // namespace

TEST(Narrator, OneInFlightAndANewerRequestReplacesTheWaitingOne) {
    Gate gate;
    gate.set(false);
    Narrator n(std::make_unique<EchoProvider>(), gate.transport());
    const auto a = n.request(say("first"));
    waitFor(gate.entered, 1); // A is in flight
    const auto b = n.request(say("second"));
    const auto c = n.request(say("third")); // replaces B, which never starts
    EXPECT_FALSE(n.poll());                 // nothing back while A is held
    gate.set(true);
    const auto first = next(n), second = next(n);
    ASSERT_TRUE(first && second);
    EXPECT_EQ(first->ticket, a);
    EXPECT_EQ(*first->result, "first");
    EXPECT_EQ(second->ticket, c); // in order
    EXPECT_EQ(*second->result, "third");
    EXPECT_NE(b, c);
    EXPECT_FALSE(n.poll());
    EXPECT_EQ(gate.calls, 2);
}

TEST(Narrator, ARateLimitDropsRequestsWhileItBacksOff) {
    Gate gate;
    gate.status = 429;
    gate.retryAfter = 120;
    Narrator n(std::make_unique<EchoProvider>(), gate.transport());
    n.request(say("hello"));
    const auto limited = next(n);
    ASSERT_TRUE(limited);
    EXPECT_EQ(limited->result.error(), (ProviderError{Kind::RateLimited, 120}));
    const auto dropped = n.request(say("again"));
    const auto reply = next(n);
    ASSERT_TRUE(reply);
    EXPECT_EQ(reply->ticket, dropped);
    EXPECT_EQ(reply->result.error().kind, Kind::RateLimited);
    EXPECT_EQ(gate.calls, 1); // the second never went out
}

TEST(Narrator, EachKindOfErrorGivesItsNoticeOnce) {
    Gate gate;
    gate.status = 401;
    Narrator n(std::make_unique<EchoProvider>(), gate.transport());
    n.request(say("a"));
    ASSERT_TRUE(next(n));
    EXPECT_EQ(n.takeNotice(), "AI narrator: the API key was rejected. Check it in Settings.");
    EXPECT_FALSE(n.takeNotice());
    n.request(say("b"));
    ASSERT_TRUE(next(n));
    EXPECT_FALSE(n.takeNotice()); // the same kind again: no second notice
    gate.status = 404;
    n.request(say("c"));
    ASSERT_TRUE(next(n));
    EXPECT_EQ(n.takeNotice(), "AI narrator: model not found. Check the model name in Settings.");
}

TEST(Narrator, UnreachableNamesTheLocalServerInLocalMode) {
    const Transport down = [](const HttpRequest&) -> std::expected<HttpResponse, ProviderError> {
        return std::unexpected(ProviderError{Kind::Unreachable});
    };
    Narrator local(std::make_unique<EchoProvider>(), down, {400, true});
    local.request(say("x"));
    ASSERT_TRUE(next(local));
    EXPECT_EQ(local.takeNotice(), "AI narrator: can't reach the local model server. Is Ollama running?");
    Narrator remote(std::make_unique<EchoProvider>(), down);
    remote.request(say("x"));
    ASSERT_TRUE(next(remote));
    EXPECT_EQ(remote.takeNotice(), "AI narrator unavailable (offline?). Using built-in text.");
}

TEST(Narrator, RepliesAreSanitized) {
    EXPECT_EQ(sanitize("\x1b[2JYou strike.", 400), "You strike.");
    EXPECT_EQ(sanitize("Line one.\nLine\ttwo.", 400), "Line one. Line two.");
    std::string longer;
    while (longer.size() < 450)
        longer += "word ";
    const auto cut = sanitize(longer, 400);
    ASSERT_TRUE(cut);
    EXPECT_TRUE(cut->ends_with("word…")) << *cut; // at a word boundary
    EXPECT_LE(cut->size(), 400u + 3);
    EXPECT_FALSE(sanitize("\x01\x02\x1b[0m\x7f\n", 400));

    Gate gate;
    Narrator n(std::make_unique<EchoProvider>(), gate.transport());
    n.request(say("\x01\x02\n"));
    const auto reply = next(n);
    ASSERT_TRUE(reply);
    EXPECT_EQ(reply->result.error().kind, Kind::BadResponse); // nothing printable left
}

TEST(Narrator, AttachingItLeavesBattlesTheSame) {
    // Golden battle B, played once on its own and once with each action narrated through a
    // Narrator: the events are identical.
    const auto play = [](Narrator* narrator) {
        const game::Combatant hero{"Aria", 2000, 2000, 200, 200, 400, 200, 50, {}};
        const game::EnemyDef& medusa = game::shippedContent().enemy("medusa");
        game::Battle battle(game::shippedContent(), hero, medusa, {"frozen_throne", "golden_bell"},
                            game::Rng::stream(2026, 0), &game::shippedScripts());
        game::narration::BattleContext ctx{&game::shippedContent(), "Aria", medusa.name, 2000, 2000,
                                           medusa.maxHp, medusa.maxHp, false, false, {}};
        std::ostringstream log;
        const std::vector<game::PlayerAction> opening{game::CastSkill{"frozen_throne"}, game::CastSkill{"golden_bell"}};
        for (std::size_t turn = 0; !battle.over(); ++turn) {
            const auto events = battle.act(turn < opening.size() ? opening[turn] : game::PlayerAction{game::Attack{}});
            for (const game::Event& e : *events)
                log << game::toString(e) << '\n';
            if (narrator)
                if (auto r = game::narration::summarizeBattleChunk(*events, ctx))
                    narrator->request(std::move(*r));
            if (narrator)
                (void)narrator->poll();
        }
        return log.str();
    };
    Gate gate;
    Narrator n(std::make_unique<EchoProvider>(), gate.transport());
    EXPECT_EQ(play(&n), play(nullptr));
}
