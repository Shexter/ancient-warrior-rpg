#pragma once

#include "game/content.hpp"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
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

// The same, then each subfolder of `modsDir` with a mod.json, merged over the base by ID in
// (priority, folder name) order. Base errors fail the load as above. A mod that can't be read,
// or whose entries break the merged content, is skipped whole and the rest are merged again.
// `modLog` (if given) receives one line per skip ("mod 'x' skipped: ..."), override
// ("mod 'x': overrides items/iron_sword") and loaded mod ("mod 'x' 1.0.0 loaded").
std::expected<Content, std::vector<ContentError>>
loadContent(const std::filesystem::path& dataDir,
            const std::optional<std::filesystem::path>& modsDir, std::vector<std::string>* modLog);

// "items.json: item 'foo': unknown upgrade material 'bar'"
std::string formatError(const ContentError& e);

} // namespace heroes::game
