#include "app/screens/expedition.hpp"

#include "app/app.hpp"
#include "app/screens/mission.hpp"
#include "game/save.hpp"

#include <algorithm>
#include <format>

namespace heroes::app {

namespace {

using engine::Key;
using engine::KeyEvent;
using engine::Rgb;
using game::dungeon::ExpAction;
using game::dungeon::Pos;
using game::dungeon::Tile;

constexpr int kViewW = 80, kViewH = 19, kViewTop = 1;
enum MenuChoice { kResume, kSave, kInventory };

std::optional<ExpAction> actionFor(const KeyEvent& key) {
    switch (key.key) {
    case Key::Up:
        return ExpAction::North;
    case Key::Right:
        return ExpAction::East;
    case Key::Down:
        return ExpAction::South;
    case Key::Left:
        return ExpAction::West;
    case Key::Enter:
        return ExpAction::UseStairs;
    case Key::Char:
        switch (key.ch) {
        case 'w':
            return ExpAction::North;
        case 'd':
            return ExpAction::East;
        case 's':
            return ExpAction::South;
        case 'a':
            return ExpAction::West;
        case ' ':
            return ExpAction::Wait;
        default:
            return std::nullopt;
        }
    default:
        return std::nullopt;
    }
}

} // namespace

ExpeditionScreen::ExpeditionScreen(Session& session) : session_(session) {
    game::GameState& state = *session_.state;
    std::optional<game::dungeon::Expedition> resumed;
    if (state.expedition)
        resumed = game::dungeon::fromSave(*state.expedition, *session_.dungeon);
    exp_ = resumed ? std::move(*resumed) : game::dungeon::startExpedition(state, *session_.dungeon).value();
    if (state.expedition && !resumed)
        exp_.message = "The saved expedition no longer fits the Catacombs; a new one begins.";
    keep();
}

void ExpeditionScreen::keep() {
    session_.state->expedition = game::dungeon::toSave(exp_);
    session_.dirty = true;
}

void ExpeditionScreen::end(std::string message) {
    session_.state->expedition.reset();
    session_.dirty = true;
    session_.lastMessage = std::move(message);
    over_ = true;
}

void ExpeditionScreen::act(ExpAction action) {
    const game::dungeon::StepResult result =
        game::dungeon::step(exp_, *session_.state, session_.content, *session_.dungeon, action);
    for (const game::dungeon::ExpEvent& e : result.events)
        if (std::holds_alternative<game::dungeon::xev::Left>(e)) {
            end("You climb out of the Catacombs.");
            return;
        }
    encounter_ = result.encounterEnemy;
    keep();
}

Transition ExpeditionScreen::update(std::span<const KeyEvent> keys, double /*dtSeconds*/) {
    if (over_)
        return Pop{};
    if (encounter_) {
        const int index = *encounter_;
        encounter_.reset();
        const std::string& id = exp_.floor.enemies[static_cast<std::size_t>(index)].enemyId;
        game::Battle battle = game::dungeon::encounterBattle(exp_, *session_.state, session_.content,
                                                             *session_.dungeon, index, session_.scripts);
        keep(); // the forked RNG
        return Push{std::make_unique<BattleScreen>(
            session_, std::move(battle),
            std::format("{} · Floor {}", session_.dungeon->name, exp_.floor.index),
            session_.content.enemy(id).sprite, [this, index](const game::Battle& b) {
                game::dungeon::resolveEncounter(exp_, *session_.state, session_.content,
                                                *session_.dungeon, index, b);
                if (session_.state->hero.hp <= 0)
                    end("You fell in the Catacombs. Visit the Prior to be revived.");
                else
                    keep();
            })};
    }
    for (const KeyEvent& key : keys) {
        if (box_) {
            if (box_->handle(key))
                box_.reset();
            continue;
        }
        if (menu_) {
            if (key.key == Key::Escape) {
                menu_.reset();
                continue;
            }
            const auto chosen = menu_->handle(key);
            if (!chosen)
                continue;
            menu_.reset();
            if (*chosen == kSave) {
                keep();
                std::error_code ec;
                if (!session_.saveOwned && std::filesystem::exists(session_.saveFile, ec)) {
                    box_.emplace("Another save is there: save from the Hub to replace it.");
                } else if (auto saved = game::saveGame(*session_.state, session_.saveFile,
                                                       session_.content.mods())) {
                    session_.dirty = false;
                    session_.saveOwned = true;
                    box_.emplace("Game saved.");
                } else {
                    box_.emplace(describe(saved.error()));
                }
            } else if (*chosen == kInventory) {
                std::string bag;
                for (const game::Inventory::Slot& slot : session_.state->hero.bag.slots())
                    if (slot)
                        bag += std::format("{}{} x{}", bag.empty() ? "" : ", ",
                                           session_.content.item(slot->first).name, slot->second);
                box_.emplace(bag.empty() ? "Your bag is empty." : "Bag: " + bag);
            }
            continue;
        }
        if (key.key == Key::Escape) {
            menu_.emplace(std::vector<Menu::Item>{{"Resume"}, {"Save"}, {"Inventory"}});
            continue;
        }
        if (const auto action = actionFor(key)) {
            act(*action);
            if (over_ || encounter_)
                break; // the rest of the keys were meant for the map
        }
    }
    return over_ ? Transition{Pop{}} : Transition{Stay{}};
}

void ExpeditionScreen::draw(engine::Framebuffer& frame) const {
    frame.clear(palette::kBg);
    const game::dungeon::Floor& f = exp_.floor;
    const game::Hero& hero = session_.state->hero;
    drawText(frame, 0, 0,
             std::format("{} — Floor {}/{}", upper(session_.dungeon->name), f.index, session_.dungeon->floors),
             palette::kAccent);

    const int left = std::clamp(exp_.hero.x - kViewW / 2, 0, std::max(0, f.width - kViewW));
    const int top = std::clamp(exp_.hero.y - kViewH / 2, 0, std::max(0, f.height - kViewH));
    for (int vy = 0; vy < kViewH; ++vy)
        for (int vx = 0; vx < kViewW; ++vx) {
            const Pos p{left + vx, top + vy};
            if (!f.inBounds(p))
                continue;
            const auto i = static_cast<std::size_t>(p.y * f.width + p.x);
            if (!exp_.explored[i])
                continue;
            const bool lit = exp_.visible[i];
            char32_t glyph = U'.';
            Rgb fg = lit ? Rgb{100, 100, 115} : Rgb{45, 45, 55};
            switch (f.at(p)) {
            case Tile::Wall:
                glyph = U'#';
                fg = lit ? Rgb{150, 140, 120} : Rgb{70, 65, 60};
                break;
            case Tile::StairsDown:
            case Tile::StairsUp:
                glyph = f.at(p) == Tile::StairsDown ? U'>' : U'<';
                fg = lit ? palette::kAccent : palette::kDim;
                break;
            case Tile::Floor:
                break;
            }
            if (lit) {
                for (const game::dungeon::MapChest& c : f.chests)
                    if (c.pos == p && !c.opened) {
                        glyph = U'$';
                        fg = palette::kGold;
                    }
                for (const game::dungeon::MapEnemy& e : f.enemies)
                    if (e.pos == p) {
                        glyph = e.elite ? U'G' : static_cast<char32_t>(e.enemyId[0]);
                        fg = e.elite ? Rgb{255, 80, 80} : palette::kDmg;
                    }
            }
            if (p == exp_.hero) {
                glyph = U'@';
                fg = palette::kAccent;
            }
            frame.set(vx, kViewTop + vy, {glyph, fg, palette::kBg});
        }

    for (int x = 0; x < frame.width(); ++x)
        frame.set(x, kViewTop + kViewH, {U'─', palette::kBorder, palette::kBg});
    const int y = kViewTop + kViewH + 1;
    int x = drawText(frame, 0, y, upper(hero.name), palette::kText) + 1;
    x += drawText(frame, x, y, "HP ", palette::kDim);
    drawBar(frame, x, y, 20, hero.hp, hero.maxHp, hpColor(hero.hp, hero.maxHp));
    x += 21;
    x += drawText(frame, x, y, std::format("{}/{}", hero.hp, hero.maxHp), palette::kText) + 3;
    x += drawText(frame, x, y, "MP ", palette::kDim);
    drawBar(frame, x, y, 8, hero.mp, hero.maxMp, palette::kMp);
    x += 9;
    x += drawText(frame, x, y, std::format("{}/{}", hero.mp, hero.maxMp), palette::kText) + 3;
    drawText(frame, x, y, std::format("Gold {}", hero.gold), palette::kGold);
    drawText(frame, 0, y + 1, exp_.message, palette::kText, frame.width());
    drawText(frame, 0, y + 2, "arrows/WASD move · space wait · Enter stairs · Esc menu", palette::kDim);

    if (menu_) {
        drawPanel(frame, 30, 8, 20, 5);
        menu_->draw(frame, 32, 9);
    }
    if (box_)
        box_->draw(frame);
}

} // namespace heroes::app
