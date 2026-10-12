#pragma once

#include "game/content.hpp"
#include "game/game_state.hpp"

#include <expected>
#include <filesystem>
#include <span>
#include <vector>

namespace heroes::game {

enum class SaveError { NotFound, Unreadable, Corrupt, NewerVersion, WriteFailed };

// Writes `<file>.tmp` next to the target, then renames it over the target, so a failed or
// interrupted save never damages an existing save. Creates the parent directory if needed.
// `mods` are the active mods, recorded so a later load can warn when they're missing.
std::expected<void, SaveError> saveGame(const GameState& state, const std::filesystem::path& file,
                                        std::span<const ModRef> mods = {});

// Validates everything against `content`. Never modifies or deletes the file.
std::expected<GameState, SaveError> loadGame(const std::filesystem::path& file,
                                             const Content& content);

// The mods a save was made with (none for a version 1 save), without loading the game. Never
// modifies the file.
std::expected<std::vector<ModRef>, SaveError> readSaveMods(const std::filesystem::path& file);

} // namespace heroes::game
