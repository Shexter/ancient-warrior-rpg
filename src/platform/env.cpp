#include "platform/env.hpp"

#include <cstdlib>

namespace heroes::platform {

std::optional<std::string> getEnv(const char* name) {
#ifdef _WIN32
    char* raw = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&raw, &size, name) != 0 || raw == nullptr)
        return std::nullopt;
    std::string value(raw);
    std::free(raw);
#else
    const char* raw = std::getenv(name);
    if (raw == nullptr)
        return std::nullopt;
    std::string value(raw);
#endif
    if (value.empty())
        return std::nullopt;
    return value;
}

engine::ColorMode detectColorMode() {
    if (const auto colorTerm = getEnv("COLORTERM");
        colorTerm && (*colorTerm == "truecolor" || *colorTerm == "24bit"))
        return engine::ColorMode::TrueColor;
    if (getEnv("WT_SESSION"))
        return engine::ColorMode::TrueColor;
    return engine::ColorMode::Ansi256;
}

} // namespace heroes::platform
