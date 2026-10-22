#include "net/narrator.hpp"

#include <utility>

namespace heroes::net {

std::optional<std::string> sanitize(std::string_view reply, int limit) {
    std::string clean;
    for (std::size_t i = 0; i < reply.size(); ++i) {
        const auto c = static_cast<unsigned char>(reply[i]);
        if (c == 0x1B) { // an escape sequence: ESC [ ... final byte, or ESC and one more byte
            if (i + 1 < reply.size() && reply[i + 1] == '[') {
                i += 2;
                while (i < reply.size() && !(reply[i] >= 0x40 && reply[i] <= 0x7E))
                    ++i;
            } else {
                ++i;
            }
            continue;
        }
        const bool space = c == ' ' || c == '\n' || c == '\r' || c == '\t';
        if (space) {
            if (!clean.empty() && clean.back() != ' ')
                clean += ' ';
        } else if (c >= 0x20 && c != 0x7F) {
            clean += static_cast<char>(c);
        }
    }
    while (!clean.empty() && clean.back() == ' ')
        clean.pop_back();
    if (clean.empty())
        return std::nullopt;
    // Characters, not bytes: count every byte that doesn't continue a UTF-8 sequence.
    std::size_t chars = 0, cut = clean.size();
    for (std::size_t i = 0; i < clean.size(); ++i)
        if ((static_cast<unsigned char>(clean[i]) & 0xC0) != 0x80 && ++chars > static_cast<std::size_t>(limit)) {
            cut = i;
            break;
        }
    if (cut == clean.size())
        return clean;
    const std::size_t space = clean.rfind(' ', cut);
    clean.resize(space == std::string::npos || space == 0 ? cut : space);
    // Leave room for the ellipsis within the limit.
    return clean + "…";
}

Narrator::Narrator(std::unique_ptr<Provider> provider, Transport transport, Options options)
    : options_(options), transport_(std::move(transport)), provider_(std::move(provider)),
      worker_([this] { work(); }) {}

Narrator::~Narrator() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_all();
    worker_.join();
}

std::uint64_t Narrator::request(game::narration::NarrationRequest request) {
    std::lock_guard lock(mutex_);
    request.ticket = nextTicket_++;
    if (std::chrono::steady_clock::now() < backoffUntil_) {
        replies_.push_back({request.ticket, std::unexpected(ProviderError{ProviderError::Kind::RateLimited})});
        return request.ticket;
    }
    const std::uint64_t ticket = request.ticket;
    pending_ = std::move(request);
    wake_.notify_all();
    return ticket;
}

std::optional<NarrationReply> Narrator::poll() {
    std::lock_guard lock(mutex_);
    if (replies_.empty())
        return std::nullopt;
    NarrationReply reply = std::move(replies_.front());
    replies_.pop_front();
    if (reply.result) {
        if (auto clean = sanitize(*reply.result, options_.charLimit))
            reply.result = std::move(*clean);
        else
            reply.result = std::unexpected(ProviderError{ProviderError::Kind::BadResponse});
    }
    if (!reply.result)
        noteError(reply.result.error());
    return reply;
}

std::optional<std::string> Narrator::takeNotice() {
    std::lock_guard lock(mutex_);
    return std::exchange(notice_, std::nullopt);
}

void Narrator::setProvider(std::unique_ptr<Provider> provider) {
    std::lock_guard lock(mutex_);
    provider_ = std::move(provider);
    pending_.reset();
}

void Narrator::noteError(const ProviderError& error) {
    using Kind = ProviderError::Kind;
    if (!seen_.insert(error.kind).second)
        return;
    switch (error.kind) {
    case Kind::Auth:
        notice_ = "AI narrator: the API key was rejected. Check it in Settings.";
        break;
    case Kind::Unreachable:
        notice_ = options_.local ? "AI narrator: can't reach the local model server. Is Ollama running?"
                                 : "AI narrator unavailable (offline?). Using built-in text.";
        break;
    case Kind::UnknownModel:
        notice_ = "AI narrator: model not found. Check the model name in Settings.";
        break;
    default:
        break; // refusals and the rest fall back to the template silently
    }
}

void Narrator::work() {
    for (;;) {
        game::narration::NarrationRequest next;
        std::shared_ptr<const Provider> provider;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [&] { return stopping_ || pending_.has_value(); });
            if (stopping_)
                return;
            next = std::move(*pending_);
            pending_.reset();
            provider = provider_;
        }
        auto result = complete(*provider, transport_, next);
        std::lock_guard lock(mutex_);
        if (!result && result.error().kind == ProviderError::Kind::RateLimited)
            backoffUntil_ = std::chrono::steady_clock::now() + std::chrono::seconds(result.error().retryAfterSeconds);
        replies_.push_back({next.ticket, std::move(result)});
    }
}

} // namespace heroes::net
