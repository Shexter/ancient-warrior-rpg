#pragma once

#include "net/providers.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>

namespace heroes::net {

struct NarrationReply {
    std::uint64_t ticket;
    std::expected<std::string, ProviderError> result;
};

// The reply as safe plain text: control bytes and escape sequences removed (newlines become
// spaces), whitespace collapsed, and cut at the last space within `limit` characters with "…".
// None when nothing printable is left.
std::optional<std::string> sanitize(std::string_view reply, int limit);

// Narration off the game thread (reference/llm-interfaces.md §6). One request is in flight at a
// time and one waits; a newer request replaces the waiting one. Replies come back through poll(),
// which never blocks. It never touches game state: the game shows its template line at once and
// swaps in a reply only if one comes back in time.
class Narrator {
  public:
    struct Options {
        int charLimit = 400;
        bool local = false; // the provider is a model server on this machine (changes one notice)
    };
    Narrator(std::unique_ptr<Provider> provider, Transport transport, Options options);
    Narrator(std::unique_ptr<Provider> provider, Transport transport)
        : Narrator(std::move(provider), std::move(transport), Options{}) {}
    ~Narrator();
    Narrator(const Narrator&) = delete;
    Narrator& operator=(const Narrator&) = delete;

    // Queues `request`, replacing any not yet started, and returns its ticket (1, 2, ...). While
    // backing off after a rate limit, the request is dropped at once and its reply is RateLimited.
    std::uint64_t request(game::narration::NarrationRequest request);
    // The next reply, sanitized (an empty one becomes BadResponse); none when nothing is back yet.
    std::optional<NarrationReply> poll();
    // A message for the player the first time an error of a kind is seen this session (Auth,
    // Unreachable, UnknownModel); none otherwise. Taking it clears it.
    std::optional<std::string> takeNotice();
    // Switches provider (a Settings change); the waiting request is dropped.
    void setProvider(std::unique_ptr<Provider> provider);

  private:
    void work();
    void noteError(const ProviderError& error); // under mutex_

    Options options_;
    Transport transport_;
    std::mutex mutex_;
    std::condition_variable wake_;
    bool stopping_ = false;
    std::shared_ptr<const Provider> provider_;
    std::optional<game::narration::NarrationRequest> pending_;
    std::deque<NarrationReply> replies_;
    std::uint64_t nextTicket_ = 1;
    std::chrono::steady_clock::time_point backoffUntil_{};
    std::set<ProviderError::Kind> seen_;
    std::optional<std::string> notice_;
    std::thread worker_; // last: it starts after everything it uses (std::jthread isn't in Xcode 16's libc++)
};

} // namespace heroes::net
