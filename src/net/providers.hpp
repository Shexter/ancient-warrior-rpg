#pragma once

#include "game/narration/summary.hpp"
#include "game/narration/templates.hpp"

#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace heroes::net {

struct HttpRequest {
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    int timeoutMs;
};
struct HttpResponse {
    int status;
    std::string body;
    std::optional<int> retryAfterSeconds;
};

struct ProviderError {
    enum class Kind { Auth, Unreachable, UnknownModel, RateLimited, Refused, Timeout, BadResponse, Server } kind;
    int retryAfterSeconds = 0;
    bool operator==(const ProviderError&) const = default;
};

// Sends one request. A failure to get any response is Timeout or Unreachable.
using Transport = std::function<std::expected<HttpResponse, ProviderError>(const HttpRequest&)>;

// One narration service: turning a request into HTTP and a response back into text. Both pure.
class Provider {
  public:
    virtual ~Provider() = default;
    virtual HttpRequest build(const game::narration::NarrationRequest& request) const = 0;
    virtual std::expected<std::string, ProviderError> parse(const HttpResponse& response) const = 0;
};

// The error a status means, the same for every provider: 401/403 Auth, 404 UnknownModel, 429
// RateLimited (retry-after, default 60 s), 5xx Server, other 4xx BadResponse; none for 2xx.
std::optional<ProviderError> statusError(const HttpResponse& response);

// Which model and key a provider uses. An empty model means the provider's default in
// narrator.json; an empty base URL means OpenAI's for the OpenAI-compatible provider.
struct ProviderConfig {
    std::string model, apiKey, baseUrl;
};

// The direct providers (reference/llm-interfaces.md §5): the Claude Messages API, Gemini
// generateContent, and OpenAI-compatible chat completions (`local`: a model server on this
// machine, where the key is optional).
std::unique_ptr<Provider> anthropicProvider(const game::narration::NarratorDef& def, ProviderConfig config);
std::unique_ptr<Provider> geminiProvider(const game::narration::NarratorDef& def, ProviderConfig config);
std::unique_ptr<Provider> openAiProvider(const game::narration::NarratorDef& def, ProviderConfig config,
                                         bool local);

// The game's own relay (Hosted mode): POST <relayUrl>/v1/narrate with the structured event only
// (reference §8), identified by the install id; the relay holds the model key.
std::unique_ptr<Provider> relayProvider(const game::narration::NarratorDef& def, std::string relayUrl,
                                        std::string installId);

// Builds, sends and parses one request.
std::expected<std::string, ProviderError> complete(const Provider& provider, const Transport& transport,
                                                   const game::narration::NarrationRequest& request);

} // namespace heroes::net
