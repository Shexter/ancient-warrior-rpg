#pragma once

#include "game/content.hpp"
#include "game/content_loader.hpp"
#include "game/world/world_def.hpp"

#include <expected>
#include <filesystem>
#include <string_view>
#include <vector>

namespace heroes::game::world {

// Parses the text of the three world files and validates them against each other and `content`
// (mission landmarks). Returns every error found; files are named "world/overworld.txt",
// "world/world.json" and "world/field.json", and map errors carry the entry "line N" (1-based).
std::expected<WorldDef, std::vector<ContentError>>
parseWorld(std::string_view mapText, std::string_view worldJson, std::string_view fieldJson,
           const Content& content);

// Reads <dataDir>/world/{overworld.txt,world.json,field.json} and parses them.
std::expected<WorldDef, std::vector<ContentError>> loadWorld(const std::filesystem::path& dataDir,
                                                             const Content& content);

} // namespace heroes::game::world
