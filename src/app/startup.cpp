#include "app/startup.hpp"

#include "game/content_loader.hpp"
#include "game/world/world_loader.hpp"

#include <format>
#include <utility>

namespace heroes::app {

namespace {

std::string usage(const StartupInputs& in) {
    return std::format("usage: {} [--seed N] [--data-dir PATH]{} [--art glyph|blocks]",
                       in.program, in.allowLayout ? " [--layout auto|compact|portrait]" : "");
}

} // namespace

StartupResult loadStartup(const StartupInputs& in) {
    StartupResult result;
    auto& lines = result.messages;
    const auto fail = [&](std::string line) {
        lines.push_back(std::move(line));
        return std::move(result); // `result` is captured by reference: move it out
    };
    for (const std::string_view arg : in.args)
        if (!in.allowLayout && arg == "--layout")
            return fail(std::format("{}: error: unknown option '--layout'\n{}", in.program,
                                    usage(in)));
    auto options = parseOptions(in.args);
    if (!options)
        return fail(std::format("{}: error: {}\n{}", in.program, options.error(), usage(in)));

    const auto dataDir = resolveDataDir(*options, in.envDataDir, in.sourceDir);
    // Mods: HEROES_MODS_DIR, else mods/ beside the data folder.
    const std::filesystem::path modsDir =
        in.envModsDir ? std::filesystem::path(*in.envModsDir) : dataDir / ".." / "mods";
    auto content = game::loadContent(dataDir, modsDir, &lines);
    if (!content) {
        for (const game::ContentError& error : content.error())
            lines.push_back(game::formatError(error));
        return result;
    }
    auto world = game::world::loadWorld(dataDir, *content);
    if (!world) {
        for (const game::ContentError& error : world.error())
            lines.push_back(game::formatError(error));
        return result;
    }
    auto dungeon = game::dungeon::loadDungeon(dataDir, *content);
    if (!dungeon) {
        for (const game::ContentError& error : dungeon.error())
            lines.push_back(game::formatError(error));
        return result;
    }
    auto narrator = game::narration::loadNarrator(dataDir);
    if (!narrator) {
        for (const game::ContentError& error : narrator.error())
            lines.push_back(game::formatError(error));
        return result;
    }
    const auto spriteDir = dataDir / ".." / "assets" / "sprites";
    auto sprites = loadSprites(*content, spriteDir);
    if (!sprites)
        return fail(sprites.error());
    auto large = loadLargeSprites(*content, spriteDir / "large");
    if (!large)
        return fail(large.error());
    if (!in.saveFile)
        return fail(std::format("{}: error: cannot find a place to save; set HEROES_SAVE_DIR",
                                in.program));

    result.startup.emplace(Startup{std::move(*options), std::move(*content), std::move(*world),
                                   std::move(*dungeon), std::move(*narrator), std::move(*sprites), std::move(*large), *in.saveFile});
    return result;
}

} // namespace heroes::app
