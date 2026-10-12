#include "app/screens/new_game.hpp"
#include "app/art.hpp"

#include "app/screens/hub.hpp"
#include "game/errors.hpp"
#include "game/hero.hpp"

#include <format>
#include <random>

namespace heroes::app {

namespace {

constexpr int kTextWidth = 72;

using engine::Key;

std::vector<Menu::Item> pathItems(const game::Content& content) {
    std::vector<Menu::Item> items;
    for (const game::PathDef& path : content.paths())
        items.emplace_back(path.name);
    return items;
}

} // namespace

// ---- Intro -------------------------------------------------------------------------------

IntroScreen::IntroScreen(Session& session)
    : session_(session), story_(session.content.story().opening, kTextWidth) {}

Transition IntroScreen::update(std::span<const engine::KeyEvent> keys, double dtSeconds) {
    story_.tick(dtSeconds);
    for (const engine::KeyEvent& key : keys) {
        if (key.key == Key::Escape)
            return Pop{};
        if (story_.handle(key))
            return Replace{std::make_unique<CreateHeroScreen>(session_)};
    }
    return Stay{};
}

void IntroScreen::draw(engine::Framebuffer& frame) const {
    frame.clear(palette::kBg);
    story_.draw(frame, 4, 3, palette::kText);
    drawCentered(frame, 22, story_.revealed() ? "Enter continue" : "Enter reveal all",
                 palette::kDim);
}

// ---- Create Hero -------------------------------------------------------------------------

CreateHeroScreen::CreateHeroScreen(Session& session)
    : session_(session), story_(session.content.story().pathIntro, kTextWidth),
      paths_(pathItems(session.content)) {}

Transition CreateHeroScreen::create(int pathIndex) {
    const game::PathDef& path = session_.content.paths()[static_cast<std::size_t>(pathIndex)];
    // The only use of std::random_device: seeding a new game without --seed (interfaces.md §4).
    const std::uint64_t seed = session_.seed ? *session_.seed : std::random_device{}();
    session_.state = game::newGame(session_.content, name_, path.id, seed);
    session_.dirty = true;
    session_.lastMessage = std::format("Welcome, {} the {}.", name_, path.name);
    return Replace{std::make_unique<HubScreen>(session_)};
}

Transition CreateHeroScreen::update(std::span<const engine::KeyEvent> keys, double dtSeconds) {
    nameInput_.tick(dtSeconds);
    story_.tick(dtSeconds);
    for (const engine::KeyEvent& key : keys) {
        switch (step_) {
        case Step::Name:
            switch (nameInput_.handle(key)) {
            case InputResult::Cancelled:
                return Pop{};
            case InputResult::Submitted:
                if (auto valid = game::validateHeroName(nameInput_.value())) {
                    name_ = std::move(*valid);
                    nameError_.clear();
                    step_ = Step::Story;
                } else {
                    nameError_ = game::describe(valid.error());
                }
                break;
            case InputResult::Editing:
                break;
            }
            break;
        case Step::Story:
            if (key.key == Key::Escape)
                step_ = Step::Name;
            else if (story_.handle(key))
                step_ = Step::Path;
            break;
        case Step::Path:
            if (key.key == Key::Escape)
                step_ = Step::Name;
            else if (const auto chosen = paths_.handle(key))
                return create(*chosen);
            break;
        }
    }
    return Stay{};
}

engine::Size CreateHeroScreen::canvasSize(engine::Size terminal) const {
    // The highlighted path can change at any time, so every path needs its large art.
    if (step_ != Step::Path || artCanvas(session_, terminal, {}) != kLargeCanvas)
        return engine::kCompactCanvas;
    for (const game::PathDef& path : session_.content.paths())
        if (!largeArt(session_, std::format("hero_{}.sprite", path.id)))
            return engine::kCompactCanvas;
    return kLargeCanvas;
}

void CreateHeroScreen::drawPathPreview(engine::Framebuffer& frame) const {
    const game::PathDef& path =
        session_.content.paths()[static_cast<std::size_t>(paths_.selected())];
    const bool large = isLarge(frame);
    // Compact: ui.md puts the sprite at col 60, but a 24-wide sprite there would run past column
    // 79, so it sits at col 52. Large: a 79x32 box with the 48x24 sprite at (66, 8).
    const int kX = large ? 40 : 30, kY = 4, kW = large ? 79 : 49, kH = large ? 32 : 17;
    drawPanel(frame, kX, kY, kW, kH);
    drawText(frame, kX + 2, kY + 1, path.name, palette::kAccent);
    std::string sentence = path.flavor; // e.g. "blessed with the Strongest Fist"
    if (!sentence.empty() && sentence[0] >= 'a' && sentence[0] <= 'z')
        sentence[0] = static_cast<char>(sentence[0] - 'a' + 'A');
    const auto flavor = wordWrap(sentence + ".", 20);
    for (std::size_t i = 0; i < flavor.size(); ++i)
        drawText(frame, kX + 2, kY + 3 + static_cast<int>(i), flavor[i], palette::kText);
    const int statsY = kY + 4 + static_cast<int>(flavor.size());
    drawText(frame, kX + 2, statsY, std::format("HP     {}", path.maxHp), palette::kText);
    drawText(frame, kX + 2, statsY + 1, std::format("Fervor {}", path.maxMp), palette::kText);
    drawText(frame, kX + 2, statsY + 2, std::format("ATK    {}", path.atk), palette::kText);
    drawText(frame, kX + 2, statsY + 3, std::format("DEF    {}", path.def), palette::kText);
    drawText(frame, kX + 2, statsY + 4, std::format("LIFE   {}", path.life), palette::kText);
    drawText(frame, kX + 2, statsY + 6, std::format("EXP per level x{}", path.multiplier),
             palette::kDim);
    const std::string file = std::format("hero_{}.sprite", path.id);
    if (large)
        largeArt(session_, file).draw(frame, 66, 8);
    else
        compactArt(session_, file).draw(frame, 52, 6);
}

void CreateHeroScreen::draw(engine::Framebuffer& frame) const {
    frame.clear(palette::kBg);
    switch (step_) {
    case Step::Name:
        drawText(frame, 4, 8, "Enter Hero name (max 8 characters):", palette::kText);
        nameInput_.draw(frame, 6, 10);
        drawText(frame, 4, 12, nameError_, palette::kDmg);
        drawCentered(frame, 22, "Enter confirm · Esc back", palette::kDim);
        break;
    case Step::Story:
        story_.draw(frame, 4, 3, palette::kText);
        drawCentered(frame, 22, story_.revealed() ? "Enter continue" : "Enter reveal all",
                     palette::kDim);
        break;
    case Step::Path:
        drawText(frame, 2, 4, "WARRIOR PATHS", palette::kAccent);
        paths_.draw(frame, 2, 6);
        drawPathPreview(frame);
        drawCentered(frame, frame.height() - 2, "↑↓ select · Enter choose · Esc back",
                     palette::kDim);
        break;
    }
}

} // namespace heroes::app
