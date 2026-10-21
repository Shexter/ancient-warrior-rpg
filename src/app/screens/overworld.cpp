#include "app/screens/overworld.hpp"

#include "app/art.hpp"
#include "app/screens/mission.hpp"
#include "app/screens/town.hpp"
#include "app/screens/training.hpp"
#include "app/widgets.hpp"
#include "game/estimate.hpp"
#include "game/hero.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <string>
#include <utility>
#include <variant>
#include <string_view>

namespace heroes::app {

namespace {

using game::world::Terrain;
using game::world::Tile;

inline constexpr engine::Size kMaxCanvas{200, 60};
inline constexpr int kNumberTicks = 36;           // how long a damage number shows (0.6 s)
inline constexpr int kNovaTicks = 12;             // how long the Nova ring shows (0.2 s)
inline constexpr int kNoticeTicks = 150;          // how long a notice holds the goal line (2.5 s)
inline constexpr int kFlashTicks = 18;            // a refused hotbar slot's flash (0.3 s)
inline constexpr engine::Rgb kMonster{226, 110, 120};
inline constexpr engine::Rgb kWindUpTile{96, 24, 36}; // under the tile a wind-up will hit

struct TerrainStyle {
    std::string_view ramp; // glyphs picked per tile by its position
    engine::Rgb fg, bg;
};

constexpr TerrainStyle styleOf(Terrain t) {
    switch (t) {
    case Terrain::Grass:
        return {".,'`..", {86, 150, 78}, {12, 20, 14}};
    case Terrain::TallGrass:
        return {"ilvY;:", {110, 180, 86}, {12, 22, 14}};
    case Terrain::Tree:
        return {"YT^", {58, 130, 70}, {8, 16, 10}};
    case Terrain::Water:
        return {"~-~=", {84, 136, 210}, {10, 20, 42}};
    case Terrain::Path:
        return {".:.", {176, 154, 112}, {26, 22, 16}};
    case Terrain::Sand:
        return {".:,", {214, 192, 132}, {32, 28, 18}};
    case Terrain::Rock:
        return {"^nA", {146, 146, 158}, {20, 20, 26}};
    case Terrain::Wall:
        return {"#=", {156, 134, 112}, {30, 26, 22}};
    case Terrain::Floor:
        return {".:", {124, 124, 140}, {24, 24, 32}};
    }
    return {".", palette::kText, palette::kBg};
}

// A stable per-tile hash, so a tile always looks the same.
std::uint32_t tileHash(Tile t) {
    std::uint32_t h = static_cast<std::uint32_t>(t.x) * 0x9E3779B1u ^
                      static_cast<std::uint32_t>(t.y) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    return h;
}

engine::Rgb scaled(engine::Rgb c, int percent) {
    const auto f = [&](std::uint8_t v) {
        return static_cast<std::uint8_t>(std::min(255, v * percent / 100));
    };
    return {f(c.r), f(c.g), f(c.b)};
}

engine::Cell terrainCell(const game::world::WorldDef& world, Tile t) {
    const TerrainStyle style = styleOf(world.at(t).terrain);
    const std::uint32_t h = tileHash(t);
    const char glyph = style.ramp[h % style.ramp.size()];
    const int brightness = 88 + static_cast<int>((h >> 8) % 25); // 88-112%
    return {static_cast<char32_t>(glyph), scaled(style.fg, brightness), style.bg};
}

// One axis of the camera: keep the hero inside the middle third of the view, stay inside the map,
// and centre a map smaller than the view.
int followAxis(int camera, int hero, int view, int map) {
    if (map <= view)
        return -(view - map) / 2;
    const int lo = view / 3, hi = view - 1 - view / 3;
    if (hero < camera + lo)
        camera = hero - lo;
    else if (hero > camera + hi)
        camera = hero - hi;
    return std::clamp(camera, 0, map - view);
}

// The status bar's numbers: how many HP-restoring consumables the bag holds.
int potionCount(const game::Hero& hero, const game::Content& content) {
    int count = 0;
    for (const game::Inventory::Slot& slot : hero.bag.slots()) {
        if (!slot)
            continue;
        if (game::restoresHp(content.item(slot->first)))
            count += slot->second;
    }
    return count;
}

// A landmark's mark: a lair is a light ! on red, anything else its name's first letter on the
// accent color, inverted so it can't be mistaken for terrain (the Thesaurus's T is not a tree).
engine::Cell landmarkMark(const game::world::Landmark& landmark) {
    if (std::holds_alternative<game::world::OpensMission>(landmark.opens))
        return {U'!', palette::kText, palette::kDmg};
    return {static_cast<char32_t>(landmark.name.empty() ? '?' : landmark.name[0]), palette::kBg,
            palette::kAccent};
}

// "1 blessing" / "3 blessings".
std::string blessingsText(int n) { return std::format("{} blessing{}", n, n == 1 ? "" : "s"); }

// A line of the panel background across `w` cells.
void fillRow(engine::Framebuffer& frame, int x, int y, int w) {
    frame.fillRect(x, y, w, 1, {U' ', palette::kText, palette::kPanel});
}

} // namespace

OverworldScreen::OverworldScreen(Session& session)
    : session_(session), world_(*session.world, *session.state, session.content),
      goal_(game::nextGoal(*session.state, session.content, session.scripts)) {
    session_.dirty = true; // the world forked the game's random stream, and the field changes HP
}

Transition OverworldScreen::update(std::span<const engine::KeyEvent> keys, double) {
    if (!game::canAdventure(session_.state->hero))
        return Pop{}; // fell in a mission or training round opened from here
    if (refreshGoal_) {
        goal_ = game::nextGoal(*session_.state, session_.content, session_.scripts);
        refreshGoal_ = false;
    }
    const auto input = readKeys(keys);
    if (!input)
        return Pop{};
    ageEffects();
    for (const game::world::WorldEvent& event : world_.step(*input))
        if (auto next = handle(event))
            return std::move(*next);
    return Stay{};
}

std::optional<game::world::WorldInput>
OverworldScreen::readKeys(std::span<const engine::KeyEvent> keys) {
    game::world::WorldInput input;
    for (const engine::KeyEvent& key : keys) {
        if (key.action == engine::KeyAction::Release)
            continue;
        const bool press = key.action == engine::KeyAction::Press;
        if (key.key == engine::Key::Escape && press)
            return std::nullopt;
        if (key.key == engine::Key::Click && camera_) {
            const Tile at{camera_->x + key.x, camera_->y + key.y};
            if (session_.world->inBounds(at))
                input.aim = at;
        }
        if (key.key != engine::Key::Char)
            continue;
        switch (key.ch) { // abilities also take a held key's repeats; the cooldown paces them
        case ' ':
            input.swing = true;
            break;
        case '1':
            input.firebolt = true;
            break;
        case '2':
            input.nova = true;
            break;
        case 'q':
        case 'Q':
            input.potion = input.potion || press;
            break;
        case 'e':
        case 'E':
            input.interact = input.interact || press;
            break;
        case 'm':
        case 'M':
            if (press)
                minimap_ = !minimapShown(lastCanvasWidth_);
            break;
        default:
            break;
        }
    }
    held_.update(keys);
    input.move = held_.direction();
    return input;
}

void OverworldScreen::ageEffects() {
    ++ticks_;
    for (FloatingNumber& n : numbers_)
        ++n.age;
    std::erase_if(numbers_, [](const FloatingNumber& n) { return n.age >= kNumberTicks; });
    if (nova_ && ++nova_->age >= kNovaTicks)
        nova_.reset();
    if (!notice_.empty() && ++noticeAge_ >= kNoticeTicks)
        notice_.clear();
    for (int& flash : flashes_)
        flash = std::max(0, flash - 1);
}

void OverworldScreen::notify(std::string text) {
    notice_ = std::move(text);
    noticeAge_ = 0;
}

std::optional<Transition> OverworldScreen::handle(const game::world::WorldEvent& event) {
    namespace wev = game::world::wev;
    if (std::holds_alternative<wev::HeroFell>(event)) {
        session_.lastMessage = "You fell in the field. Visit the Prior to be revived.";
        return Pop{};
    }
    if (const auto* hit = std::get_if<wev::Damaged>(&event))
        numbers_.push_back({hit->at, hit->amount, hit->onHero, 0});
    else if (const auto* refused = std::get_if<wev::AbilityRefused>(&event))
        flashes_[static_cast<std::size_t>(refused->ability)] = kFlashTicks;
    else if (const auto* burst = std::get_if<wev::NovaBurst>(&event))
        nova_ = NovaRing{burst->center, burst->radius, 0};
    else if (std::holds_alternative<wev::NoPotions>(event)) {
        notify("No potions");
        flashes_[game::world::kAbilityCount] = kFlashTicks;
    } else if (const auto* drunk = std::get_if<wev::PotionDrunk>(&event))
        notify(std::format("Drank {}", session_.content.item(drunk->itemId).name));
    else if (const auto* kill = std::get_if<wev::MonsterDefeated>(&event)) {
        const game::MonsterRewards& r = kill->rewards;
        std::string text =
            std::format("{} defeated · +{} gold · +{} exp", kill->name, r.gold, r.exp);
        if (r.levelsGained > 0)
            text += std::format(" · Level up! +{} at the Prior", blessingsText(r.levelsGained));
        if (r.drop)
            text += r.dropLostBagFull
                        ? std::format(" · Bag full, {} lost", session_.content.item(*r.drop).name)
                        : std::format(" · Drop: {}", session_.content.item(*r.drop).name);
        notify(std::move(text));
    } else if (const auto* use = std::get_if<wev::Interact>(&event)) {
        if (auto next = open(session_.world->landmarks[use->landmark])) {
            refreshGoal_ = true;
            return Push{std::move(next)};
        }
    }
    return std::nullopt;
}

std::unique_ptr<Screen> OverworldScreen::open(const game::world::Landmark& landmark) {
    using namespace game::world;
    if (const auto* service = std::get_if<OpensService>(&landmark.opens)) {
        switch (service->service) {
        case Service::Almonry:
            return std::make_unique<MerchantScreen>(session_);
        case Service::Forge:
            return std::make_unique<BlacksmithScreen>(session_);
        case Service::Prior:
            return std::make_unique<GuildmasterScreen>(session_);
        case Service::Thesaurus:
            return std::make_unique<BankScreen>(session_);
        }
    }
    if (std::holds_alternative<OpensTraining>(landmark.opens))
        return std::make_unique<TrainingSetupScreen>(session_);
    const std::string& id = std::get<OpensMission>(landmark.opens).missionId;
    if (!missionBlocker(session_.state->hero, session_.content.mission(id)).empty())
        return nullptr; // the prompt already says why
    return std::make_unique<MissionIntroScreen>(session_, id, /*fromWorld=*/true);
}

std::string OverworldScreen::landmarkPrompt() const {
    const auto index = world_.nearbyLandmark();
    if (!index)
        return {};
    const game::world::Landmark& landmark = session_.world->landmarks[*index];
    if (const auto* lair = std::get_if<game::world::OpensMission>(&landmark.opens))
        if (std::string why =
                missionBlocker(session_.state->hero, session_.content.mission(lair->missionId));
            !why.empty())
            return std::format("{} — {}", landmark.name, why);
    return std::format("{} — press E", landmark.name);
}

bool OverworldScreen::minimapShown(int canvasWidth) const {
    return minimap_.value_or(canvasWidth >= kLargeCanvas.width);
}

engine::Size OverworldScreen::canvasSize(engine::Size terminal) const {
    if (session_.layout == Layout::Compact)
        return engine::kCompactCanvas;
    return {std::clamp(terminal.width, engine::kCompactCanvas.width, kMaxCanvas.width),
            std::clamp(terminal.height, engine::kCompactCanvas.height, kMaxCanvas.height)};
}

void OverworldScreen::draw(engine::Framebuffer& frame) const {
    lastCanvasWidth_ = frame.width();
    drawTerrain(frame);
    drawHud(frame);
    if (minimapShown(frame.width()))
        drawMinimap(frame);
    drawHotbar(frame);
}

void OverworldScreen::drawHotbar(engine::Framebuffer& frame) const {
    struct Slot {
        std::string_view key, name;
        int cost;
        int cooldown; // ticks left
        int flash;
    };
    using game::world::Ability;
    const game::world::FieldTuning& field = session_.world->field;
    const auto ability = [&](Ability a, std::string_view key, std::string_view name) {
        return Slot{key, name, field.ability(a).cost, world_.cooldownLeft(a),
                    flashes_[static_cast<std::size_t>(a)]};
    };
    const std::string potions =
        std::format("Potion x{}", potionCount(session_.state->hero, session_.content));
    const std::array<Slot, 4> slots{
        ability(Ability::Swing, "SPC", "Swing"), ability(Ability::Firebolt, "1", "Firebolt"),
        ability(Ability::Nova, "2", "Nova"),
        Slot{"Q", potions, 0, 0, flashes_[game::world::kAbilityCount]}};

    const int y = frame.height() - 1;
    fillRow(frame, 0, y, frame.width());
    int x = 1;
    for (const Slot& slot : slots) {
        const engine::Rgb bg = slot.flash > 0 ? palette::kDmg : palette::kPanel;
        frame.fillRect(x, y, textWidth(slot.key) + 2, 1, {U' ', palette::kBg, palette::kText});
        drawText(frame, x + 1, y, slot.key, palette::kBg);
        x += textWidth(slot.key) + 3;
        const engine::Rgb fg = slot.cooldown > 0 ? palette::kDim : palette::kText;
        std::string label(slot.name);
        if (slot.cost > 0)
            label += std::format(" {}", slot.cost);
        if (slot.cooldown > 0)
            label += std::format(" {:.1f}s", slot.cooldown / 60.0);
        const int width = textWidth(label);
        frame.fillRect(x, y, width, 1, {U' ', fg, bg});
        x += drawText(frame, x, y, label, slot.cost > 0 && slot.cooldown == 0 ? palette::kMp : fg,
                      std::max(0, frame.width() - x)) +
             3;
    }
}

void OverworldScreen::drawTerrain(engine::Framebuffer& frame) const {
    const game::world::WorldDef& map = *session_.world;
    const Tile hero = world_.hero();
    const Tile start = camera_.value_or(
        Tile{hero.x - frame.width() / 2, hero.y - frame.height() / 2});
    camera_ = Tile{followAxis(start.x, hero.x, frame.width(), map.width),
                   followAxis(start.y, hero.y, frame.height(), map.height)};

    frame.clear(palette::kBg);
    for (int y = 0; y < frame.height(); ++y)
        for (int x = 0; x < frame.width(); ++x)
            if (const Tile t{camera_->x + x, camera_->y + y}; map.inBounds(t))
                frame.set(x, y, terrainCell(map, t));
    for (const game::world::Landmark& landmark : map.landmarks) {
        const int x = landmark.at.x - camera_->x, y = landmark.at.y - camera_->y;
        if (x >= 0 && y >= 0 && x < frame.width() && y < frame.height())
            frame.set(x, y, landmarkMark(landmark));
    }
    drawCreatures(frame);
}

void OverworldScreen::drawCreatures(engine::Framebuffer& frame) const {
    using Mode = game::world::Monster::Mode;
    const auto cell = [&](Tile t) -> engine::Cell* {
        const int x = t.x - camera_->x, y = t.y - camera_->y;
        return x >= 0 && y >= 0 && x < frame.width() && y < frame.height() ? &frame.at(x, y)
                                                                            : nullptr;
    };
    for (const game::world::Monster& m : world_.monsters())
        if (engine::Cell* target = m.mode == Mode::WindUp ? cell(m.target) : nullptr)
            target->bg = kWindUpTile;
    for (const game::world::Monster& m : world_.monsters()) {
        engine::Cell* c = cell(m.at);
        if (!c)
            continue;
        const char letter = m.stats.name.empty() ? 'm' : m.stats.name[0];
        c->glyph = static_cast<char32_t>(letter >= 'A' && letter <= 'Z' ? letter - 'A' + 'a' : letter);
        const bool flash = m.mode == Mode::WindUp && (ticks_ / 4) % 2 == 0;
        c->fg = flash ? palette::kAccent : kMonster;
    }
    for (const game::world::Bolt& bolt : world_.bolts())
        if (engine::Cell* c = cell(bolt.at)) {
            c->glyph = U'*';
            c->fg = palette::kGold;
        }
    if (nova_)
        for (int dy = -nova_->radius; dy <= nova_->radius; ++dy)
            for (int dx = -nova_->radius; dx <= nova_->radius; ++dx)
                if (std::max(std::abs(dx), std::abs(dy)) == nova_->radius)
                    if (engine::Cell* c = cell({nova_->center.x + dx, nova_->center.y + dy})) {
                        c->glyph = U'*';
                        c->fg = palette::kMp;
                    }
    engine::Cell& heroCell = *cell(world_.hero());
    heroCell.glyph = U'@';
    heroCell.fg = palette::kAccent;
    for (const FloatingNumber& n : numbers_) {
        const std::string text = std::to_string(n.amount);
        const int x = n.at.x - camera_->x - static_cast<int>(text.size()) / 2;
        const int y = n.at.y - camera_->y - (n.age < kNumberTicks / 2 ? 1 : 2);
        drawText(frame, x, y, text, n.onHero ? palette::kDmg : palette::kAccent);
    }
}

void OverworldScreen::drawHud(engine::Framebuffer& frame) const {
    const game::Hero& hero = session_.state->hero;
    const game::Content& content = session_.content;
    const std::string weapon = hero.weapon ? content.item(hero.weapon->itemId).name : "—";
    const int potions = potionCount(hero, content);
    const std::string prompt = landmarkPrompt();
    // A notice beats a landmark prompt, which beats the Next: goal (the only one in plain text).
    const bool showGoal = notice_.empty() && prompt.empty();
    const std::string goalLine =
        "» " + (!notice_.empty() ? notice_ : !prompt.empty() ? prompt : goal_);
    const engine::Rgb goalColor = showGoal ? palette::kText : palette::kAccent;

    if (frame.width() < kLargeCanvas.width) { // two rows across the top
        fillRow(frame, 0, 0, frame.width());
        fillRow(frame, 0, 1, frame.width());
        int x = 1;
        const auto add = [&](std::string_view text, engine::Rgb fg) {
            x += drawText(frame, x, 0, text, fg, std::max(0, frame.width() - 1 - x));
        };
        const auto bar = [&](int width, int value, int max, engine::Rgb color) {
            if (x + width < frame.width()) {
                drawBar(frame, x, 0, width, value, max, color);
                x += width;
            }
        };
        add(upper(hero.name), palette::kAccent);
        add(std::format(" Lv {}  HP ", hero.level), palette::kText);
        bar(10, hero.hp, hero.maxHp, hpColor(hero.hp, hero.maxHp));
        add(std::format(" {}/{}  FV ", hero.hp, hero.maxHp), palette::kText);
        bar(6, hero.mp, hero.maxMp, palette::kMp);
        add(std::format(" {}/{}  ", hero.mp, hero.maxMp), palette::kText);
        add(std::format("${}", hero.gold), palette::kGold);
        add(std::format("  !x{}  {}", potions, weapon), palette::kText);
        drawText(frame, 1, 1, goalLine, goalColor, frame.width() - 2);
        return;
    }

    constexpr int kPanelWidth = 40;
    drawPanel(frame, 1, 1, kPanelWidth, 7);
    drawText(frame, 3, 2, upper(hero.name), palette::kAccent);
    drawText(frame, 3 + textWidth(upper(hero.name)) + 1, 2, std::format("Lv {}", hero.level),
             palette::kDim);
    const auto statRow = [&](int y, std::string_view label, int value, int max, engine::Rgb color) {
        drawText(frame, 3, y, label, color);
        drawBar(frame, 7, y, 20, value, max, color);
        drawText(frame, 28, y, std::format("{}/{}", value, max), color);
    };
    statRow(3, "HP", hero.hp, hero.maxHp, hpColor(hero.hp, hero.maxHp));
    statRow(4, "FV", hero.mp, hero.maxMp, palette::kMp);
    const int expShown = static_cast<int>(std::min<std::int64_t>(hero.exp, hero.expToNext));
    drawText(frame, 3, 5, "XP", palette::kDim);
    drawBar(frame, 7, 5, 20, expShown, static_cast<int>(std::max<std::int64_t>(1, hero.expToNext)),
            palette::kDim);
    drawText(frame, 28, 5, std::format("{}/{}", hero.exp, hero.expToNext), palette::kDim,
             kPanelWidth - 28);
    int x = 3;
    x += drawText(frame, x, 6, std::format("${}", hero.gold), palette::kGold);
    x += drawText(frame, x, 6, std::format("  !x{}  ", potions), palette::kHpLow);
    drawText(frame, x, 6, weapon, palette::kText, kPanelWidth - 1 - x);

    const int goalWidth = std::min(frame.width() - 2, std::max(kPanelWidth, textWidth(goalLine) + 4));
    drawPanel(frame, 1, 9, goalWidth, 3);
    drawText(frame, 3, 10, goalLine, goalColor, goalWidth - 4);
}

void OverworldScreen::drawMinimap(engine::Framebuffer& frame) const {
    const game::world::WorldDef& map = *session_.world;
    const bool large = frame.width() >= kLargeCanvas.width;
    const int w = large ? 34 : 26, h = large ? 16 : 12;
    const int left = 1, top = frame.height() - h - 1;
    drawPanel(frame, left, top, w, h);
    const int innerW = w - 2, innerH = h - 2;
    const auto cellOf = [&](Tile t) {
        return std::pair{left + 1 + t.x * innerW / map.width, top + 1 + t.y * innerH / map.height};
    };
    for (int cy = 0; cy < innerH; ++cy)
        for (int cx = 0; cx < innerW; ++cx) {
            const Tile t{cx * map.width / innerW, cy * map.height / innerH};
            if (!map.inBounds(t))
                continue;
            const game::world::TileType& type = map.at(t);
            const char32_t glyph = type.terrain == Terrain::Water ? U'~'
                                   : type.walkable                ? U'.'
                                                                  : U'#';
            frame.set(left + 1 + cx, top + 1 + cy,
                      {glyph, scaled(styleOf(type.terrain).fg, 80), palette::kPanel});
        }
    for (const game::world::Landmark& landmark : map.landmarks) {
        const auto [x, y] = cellOf(landmark.at);
        frame.set(x, y, landmarkMark(landmark));
    }
    const auto [hx, hy] = cellOf(world_.hero());
    frame.set(hx, hy, {U'@', palette::kAccent, palette::kPanel});
}

} // namespace heroes::app
