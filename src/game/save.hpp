#pragma once

#include "game/content.hpp"
#include "game/game_state.hpp"

#include <expected>
#include <filesystem>

namespace heroes::game {

enum class SaveError { NotFound, Unreadable, Corrupt, NewerVersion, WriteFailed };

// Writes `<file>.tmp` next to the target, then renames it over the target, so a failed or
// interrupted save never damages an existing save. Creates the parent directory if needed.
std::expected<void, SaveError> saveGame(const GameState& state, const std::filesystem::path& file);

// Validates everything against `content`. Never modifies or deletes the file.
std::expected<GameState, SaveError> loadGame(const std::filesystem::path& file,
                                             const Content& content);

} // namespace heroes::game
