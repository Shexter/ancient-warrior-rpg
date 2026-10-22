#pragma once

#include "app/screen.hpp"

#include <filesystem>
#include <memory>

namespace heroes::window {

// Opens the window and runs the screen stack until a screen quits, the stack empties, or the
// window is closed (asking first when the game has unsaved changes). Returns the exit code.
int run(app::Session& session, std::unique_ptr<app::Screen> first,
        const std::filesystem::path& fontFile);

} // namespace heroes::window
