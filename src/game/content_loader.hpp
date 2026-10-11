#pragma once

#include "game/content.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace heroes::game {

struct ContentError {
    std::string file;
    std::string entry; // e.g. "item 'iron_sword'"; empty for file-level errors
    std::string message;
};

// Loads and validates data/*.json. Sprites are looked up in <dataDir>/../assets/sprites/.
// On failure returns every error found, not just the first.
std::expected<Content, std::vector<ContentError>> loadContent(const std::filesystem::path& dataDir);

// "items.json: item 'foo': unknown upgrade material 'bar'"
std::string formatError(const ContentError& e);

} // namespace heroes::game
