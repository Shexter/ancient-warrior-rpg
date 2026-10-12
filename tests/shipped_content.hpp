#pragma once

#include "game/content_loader.hpp"
#include "game/script/script_host.hpp"

#include <gtest/gtest.h>

#include <filesystem>

namespace heroes::game {

// The shipped data/, loaded once per test binary.
inline const Content& shippedContent() {
    static const Content content = [] {
        auto loaded = loadContent(std::filesystem::path(HEROES_SOURCE_DIR) / "data");
        EXPECT_TRUE(loaded.has_value()) << (loaded ? "" : formatError(loaded.error().front()));
        return std::move(*loaded);
    }();
    return content;
}

// A script host for the shipped content (Medusa's script), shared by the single-threaded tests.
inline script::ScriptHost& shippedScripts() {
    static script::ScriptHost host(shippedContent());
    return host;
}

} // namespace heroes::game
