#pragma once

#include "app/screen.hpp"
#include "window/models.hpp"

#include <filesystem>
#include <memory>

namespace heroes::window {

// Opens the window and runs the screen stack until a screen quits, the stack empties, or the
// window is closed (asking first when the game has unsaved changes). The hero is drawn from its
// path's model in `models` when there is one. Returns the exit code: 2 when a model won't load.
int run(app::Session& session, std::unique_ptr<app::Screen> first,
        const std::filesystem::path& fontFile, std::map<std::string, ModelFile> models);

} // namespace heroes::window
