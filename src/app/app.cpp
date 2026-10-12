#include "app/app.hpp"

#include "app/art.hpp"
#include "app/widgets.hpp"

#include <charconv>
#include <format>

namespace heroes::app {

std::expected<Options, std::string> parseOptions(std::span<const std::string_view> args) {
    Options options;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view flag = args[i];
        if (flag != "--seed" && flag != "--data-dir" && flag != "--layout" && flag != "--art")
            return std::unexpected(std::format("unknown option '{}'", flag));
        if (i + 1 >= args.size())
            return std::unexpected(std::format("{} needs a value", flag));
        const std::string_view value = args[++i];
        if (flag == "--data-dir") {
            options.dataDir = std::filesystem::path(value);
            continue;
        }
        if (flag == "--layout") {
            if (value == "auto")
                options.layout = Layout::Auto;
            else if (value == "compact")
                options.layout = Layout::Compact;
            else if (value == "portrait")
                options.layout = Layout::Portrait;
            else
                return std::unexpected(
                    std::format("--layout expects auto, compact or portrait, got '{}'", value));
            continue;
        }
        if (flag == "--art") {
            if (value != "glyph" && value != "blocks")
                return std::unexpected(
                    std::format("--art expects glyph or blocks, got '{}'", value));
            options.art = value == "glyph" ? ArtStyle::Glyph : ArtStyle::Blocks;
            continue;
        }
        std::uint64_t seed = 0;
        const auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), seed);
        if (ec != std::errc{} || end != value.data() + value.size())
            return std::unexpected(std::format("--seed expects a whole number, got '{}'", value));
        options.seed = seed;
    }
    return options;
}

std::filesystem::path resolveDataDir(const Options& options,
                                     const std::optional<std::string>& envDataDir,
                                     const std::filesystem::path& sourceDir) {
    if (options.dataDir)
        return *options.dataDir;
    if (envDataDir)
        return *envDataDir;
    return sourceDir / "data";
}

std::string_view describe(game::SaveError error) {
    switch (error) {
    case game::SaveError::NotFound:
        return "No saved game was found.";
    case game::SaveError::Unreadable:
        return "The save file could not be read.";
    case game::SaveError::Corrupt:
        return "The save file is damaged. It was left untouched; start a new game to play.";
    case game::SaveError::NewerVersion:
        return "This save was made by a newer version of the game. It was left untouched.";
    case game::SaveError::WriteFailed:
        return "The game could not be saved. Your previous save is unchanged.";
    }
    return "Something went wrong with the save file.";
}

namespace {

// Every sprite file the screens draw.
std::vector<std::string> spriteFiles(const game::Content& content) {
    std::vector<std::string> files{"monster.sprite"};
    for (const game::PathDef& path : content.paths())
        files.push_back(std::format("hero_{}.sprite", path.id));
    for (const game::EnemyDef& enemy : content.enemies())
        files.push_back(enemy.sprite);
    return files;
}

} // namespace

std::expected<LargeSprites, std::string> loadLargeSprites(const game::Content& content,
                                                          const std::filesystem::path& largeDir) {
    LargeSprites large;
    for (const std::string& file : spriteFiles(content)) {
        const std::filesystem::path blocks = largeDir / file;
        const std::filesystem::path glyphs = (largeDir / file).replace_extension(".glyphs");
        std::error_code ec;
        if (std::filesystem::exists(blocks, ec)) {
            auto sprite = engine::loadSprite(blocks);
            if (!sprite)
                return std::unexpected(sprite.error());
            large.blocks.insert_or_assign(file, std::move(*sprite));
        }
        if (std::filesystem::exists(glyphs, ec)) {
            auto sprite = engine::loadGlyphs(glyphs);
            if (!sprite)
                return std::unexpected(sprite.error());
            large.glyphs.insert_or_assign(file, std::move(*sprite));
        }
    }
    return large;
}

std::expected<SpriteBank, std::string> loadSprites(const game::Content& content,
                                                   const std::filesystem::path& spriteDir) {
    SpriteBank bank;
    for (const std::string& file : spriteFiles(content)) {
        auto sprite = engine::loadSprite(spriteDir / file);
        if (!sprite)
            return std::unexpected(sprite.error());
        bank.insert_or_assign(file, std::move(*sprite));
    }
    return bank;
}

App::App(engine::LoopIo& io, engine::Renderer& renderer, std::unique_ptr<Screen> first)
    : io_(io), renderer_(renderer) {
    stack_.push_back(std::move(first));
}

bool App::update(std::span<const engine::KeyEvent> keys, double dtSeconds) {
    if (stack_.empty())
        return false;
    // On the phone layout, taps on the ✕ and the pad arrive as the keys they stand for.
    std::vector<engine::KeyEvent> mapped(keys.begin(), keys.end());
    if (portrait_)
        for (engine::KeyEvent& key : mapped)
            if (const auto control = controlKey(key))
                key = *control;
    Transition next = stack_.back()->update(mapped, dtSeconds);
    if (std::holds_alternative<Quit>(next))
        return false;
    if (std::holds_alternative<Pop>(next)) {
        stack_.pop_back();
    } else if (auto* push = std::get_if<Push>(&next)) {
        stack_.push_back(std::move(push->screen));
    } else if (auto* replace = std::get_if<Replace>(&next)) {
        stack_.pop_back();
        stack_.push_back(std::move(replace->screen));
    }
    return !stack_.empty();
}

void App::draw(engine::Framebuffer& frame) const {
    portrait_ = isPortrait(frame);
    if (!stack_.empty())
        stack_.back()->draw(frame);
    if (portrait_)
        drawControls(frame);
}

engine::Size App::canvasSize(engine::Size terminal) const {
    return stack_.empty() ? engine::kCompactCanvas : stack_.back()->canvasSize(terminal);
}

int App::run() {
    engine::Loop loop(
        io_, renderer_,
        [this](std::span<const engine::KeyEvent> keys, double dt) { return update(keys, dt); },
        [this](engine::Framebuffer& frame) { draw(frame); }, palette::kBg,
        [this](engine::Size terminal) { return canvasSize(terminal); });
    return loop.run();
}

} // namespace heroes::app
