#include "net/http.hpp"

#include <curl/curl.h>

#include <charconv>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string_view>

namespace heroes::net {

namespace {

std::size_t appendBody(char* data, std::size_t size, std::size_t count, void* out) {
    static_cast<std::string*>(out)->append(data, size * count);
    return size * count;
}

// Picks "retry-after: 120" out of the response headers (seconds only, not an HTTP date).
std::size_t readHeader(char* data, std::size_t size, std::size_t count, void* out) {
    std::string_view line(data, size * count);
    constexpr std::string_view kName = "retry-after:";
    if (line.size() > kName.size()) {
        bool match = true;
        for (std::size_t i = 0; i < kName.size() && match; ++i)
            match = (line[i] | 0x20) == kName[i]; // ASCII case-insensitive
        if (match) {
            line.remove_prefix(kName.size());
            while (!line.empty() && line.front() == ' ')
                line.remove_prefix(1);
            int seconds = 0;
            const auto [end, ec] = std::from_chars(line.data(), line.data() + line.size(), seconds);
            if (ec == std::errc{} && end != line.data())
                *static_cast<std::optional<int>*>(out) = seconds;
        }
    }
    return size * count;
}

} // namespace

Transport curlTransport() {
    static std::once_flag once;
    std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
    return [](const HttpRequest& request) -> std::expected<HttpResponse, ProviderError> {
        const std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
        if (!curl)
            return std::unexpected(ProviderError{ProviderError::Kind::Unreachable});
        curl_slist* rawHeaders = nullptr;
        for (const auto& [name, value] : request.headers)
            rawHeaders = curl_slist_append(rawHeaders, (name + ": " + value).c_str());
        const std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers(rawHeaders, curl_slist_free_all);
        HttpResponse response{0, {}, std::nullopt};
        CURL* c = curl.get();
        curl_easy_setopt(c, CURLOPT_URL, request.url.c_str());
        curl_easy_setopt(c, CURLOPT_POSTFIELDS, request.body.c_str());
        curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
        curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers.get());
        curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, static_cast<long>(request.timeoutMs));
        curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT_MS, 3000L);
        curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 0L);
        curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L); // called off the main thread
        curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, appendBody);
        curl_easy_setopt(c, CURLOPT_WRITEDATA, &response.body);
        curl_easy_setopt(c, CURLOPT_HEADERFUNCTION, readHeader);
        curl_easy_setopt(c, CURLOPT_HEADERDATA, &response.retryAfterSeconds);
        const CURLcode result = curl_easy_perform(c);
        if (result == CURLE_OPERATION_TIMEDOUT)
            return std::unexpected(ProviderError{ProviderError::Kind::Timeout});
        if (result != CURLE_OK)
            return std::unexpected(ProviderError{ProviderError::Kind::Unreachable});
        long status = 0;
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
        response.status = static_cast<int>(status);
        return response;
    };
}

} // namespace heroes::net
