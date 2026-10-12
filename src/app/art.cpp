#include "app/art.hpp"

namespace heroes::app {

int Art::width() const { return blocks ? blocks->width : glyphs ? glyphs->width : 0; }

int Art::height() const {
    return blocks ? (blocks->height + 1) / 2 : glyphs ? glyphs->height : 0;
}

void Art::draw(engine::Framebuffer& frame, int x, int y, engine::SpriteTint tint) const {
    if (blocks)
        engine::drawSprite(frame, *blocks, x, y, tint);
    else if (glyphs)
        engine::drawGlyphs(frame, *glyphs, x, y, tint);
}

Art compactArt(const Session& session, std::string_view file) {
    const auto it = session.sprites.find(file);
    return it == session.sprites.end() ? Art{} : Art{&it->second, nullptr};
}

Art largeArt(const Session& session, std::string_view file) {
    if (session.art == ArtStyle::Blocks) {
        const auto it = session.large.blocks.find(file);
        return it == session.large.blocks.end() ? Art{} : Art{&it->second, nullptr};
    }
    const auto it = session.large.glyphs.find(file);
    return it == session.large.glyphs.end() ? Art{} : Art{nullptr, &it->second};
}

engine::Size artCanvas(const Session& session, engine::Size terminal,
                       std::initializer_list<std::string_view> files) {
    if (session.layout == Layout::Compact || terminal.width < kLargeCanvas.width ||
        terminal.height < kLargeCanvas.height)
        return engine::kCompactCanvas;
    for (const std::string_view file : files)
        if (!largeArt(session, file))
            return engine::kCompactCanvas;
    return kLargeCanvas;
}

engine::Size portraitOr(const Session& session, engine::Size terminal, engine::Size other) {
    if (session.layout == Layout::Portrait ||
        (session.layout == Layout::Auto && terminal.width < engine::kCompactCanvas.width))
        return engine::kPortraitCanvas;
    return other;
}

} // namespace heroes::app
