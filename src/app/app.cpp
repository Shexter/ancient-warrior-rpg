#include "app/app.hpp"

#include "app/widgets.hpp"

#include <charconv>
#include <format>

namespace heroes::app {

std::expected<Options, std::string> parseOptions(std::span<const std::string_view> args) {
    Options options;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view flag = args[i];
        if (flag != "--seed" && flag != "--data-dir")
            return std::unexpected(std::format("unknown option '{}'", flag));
        if (i + 1 >= args.size())
            return std::unexpected(std::format("{} needs a value", flag));
        const std::string_view value = args[++i];
        if (flag == "--data-dir") {
            options.dataDir = std::filesystem::path(value);
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

App::App(engine::LoopIo& io, engine::Renderer& renderer, std::unique_ptr<Screen> first)
    : io_(io), renderer_(renderer) {
    stack_.push_back(std::move(first));
}

bool App::update(std::span<const engine::KeyEvent> keys, double dtSeconds) {
    if (stack_.empty())
        return false;
    Transition next = stack_.back()->update(keys, dtSeconds);
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
    if (!stack_.empty())
        stack_.back()->draw(frame);
}

int App::run() {
    engine::Loop loop(
        io_, renderer_,
        [this](std::span<const engine::KeyEvent> keys, double dt) { return update(keys, dt); },
        [this](engine::Framebuffer& frame) { draw(frame); }, palette::kBg);
    return loop.run();
}

} // namespace heroes::app
