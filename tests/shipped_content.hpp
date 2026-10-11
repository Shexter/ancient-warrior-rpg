#pragma once

#include "game/content_loader.hpp"

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

} // namespace heroes::game
