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
