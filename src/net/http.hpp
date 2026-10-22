#pragma once

#include "net/providers.hpp"

namespace heroes::net {

// The one place libcurl is used: an HTTPS (or local http) POST with the request's headers and
// body, TLS verification left on, no redirects followed, a 3 s connect timeout and the request's
// own overall timeout. A timeout is Timeout; any other failure to get a response is Unreachable.
// The integer form of a Retry-After header is passed back.
Transport curlTransport();

} // namespace heroes::net
