#pragma once

#include "engine/color.hpp"

#include <optional>
#include <string>

namespace heroes::platform {

// Value of an environment variable; unset and empty both give nullopt.
std::optional<std::string> getEnv(const char* name);

// TrueColor if COLORTERM is `truecolor` or `24bit`, or WT_SESSION is set (Windows Terminal).
engine::ColorMode detectColorMode();

} // namespace heroes::platform
