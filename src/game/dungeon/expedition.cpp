#include "game/dungeon/expedition.hpp"

#include "game/dungeon/fov.hpp"
#include "game/hero.hpp"

#include <algorithm>
#include <cstdlib>
#include <format>

namespace heroes::game::dungeon {

namespace {

std::size_t indexOf(const Floor& f, Pos p) { return static_cast<std::size_t>(p.y * f.width + p.x); }

void look(Expedition& exp, const DungeonDef& def) {
    computeFov(exp.floor, exp.hero, def.fovRadius, exp.visible);
    for (std::size_t i = 0; i < exp.visible.size(); ++i)
        if (exp.visible[i])
            exp.explored[i] = true;
}

// Puts the hero on `floor`'s entry with nothing explored yet, and looks around.
void arrive(Expedition& exp, Floor floor, const DungeonDef& def) {
    exp.floor = std::move(floor);
    exp.hero = exp.floor.entry;
    exp.explored.assign(exp.floor.tiles.size(), false);
    look(exp, def);
}

Pos neighbour(Pos p, ExpAction a) {
    switch (a) {
    case ExpAction::North:
        return {p.x, p.y - 1};
    case ExpAction::East:
        return {p.x + 1, p.y};
    case ExpAction::South:
        return {p.x, p.y + 1};
    default:
        return {p.x - 1, p.y};
    }
}

void openChest(Expedition& exp, GameState& state, const Content& content, const DungeonDef& def,
               MapChest& chest, std::vector<ExpEvent>& events) {
    const ChestTable& table = def.chestsFor(exp.floor.index);
    int total = 0;
    for (const ChestEntry& e : table.entries)
        total += e.weight;
    int r = static_cast<int>(exp.rng.below(static_cast<std::uint32_t>(total)));
    auto entry = table.entries.begin();
    while (r >= entry->weight)
        r -= (entry++)->weight;
    const int qty = entry->min + static_cast<int>(exp.rng.below(static_cast<std::uint32_t>(entry->max - entry->min + 1)));
    const bool lost = !state.hero.bag.add(entry->item, qty).has_value();
    chest.opened = true;
    const std::string& name = content.item(entry->item).name;
    exp.message = lost ? std::format("Your bag is full: you leave {} {} behind.", qty, name)
                       : std::format("You found {} {}.", qty, name);
    events.push_back(xev::ChestOpened{entry->item, qty, lost});
}

// Every enemy's turn, in order (reference §6 step 5).
void enemyPhase(Expedition& exp, std::vector<ExpEvent>& events, std::optional<int>& encounter) {
    const Floor& f = exp.floor;
    const std::vector<int> dist = bfsDistances(f, exp.hero);
    for (std::size_t i = 0; i < f.enemies.size(); ++i) {
        MapEnemy& e = exp.floor.enemies[i];
        const int index = static_cast<int>(i);
        if (e.sleepTurns > 0) {
            --e.sleepTurns;
            continue;
        }
        if (!e.awake && exp.visible[indexOf(f, e.pos)]) {
            e.awake = true;
            events.push_back(xev::EnemyWoke{index});
        }
        if (!e.awake)
            continue;
        if (std::abs(e.pos.x - exp.hero.x) + std::abs(e.pos.y - exp.hero.y) == 1) {
            encounter = index;
            return;
        }
        const int here = dist[indexOf(f, e.pos)];
        for (const ExpAction d : {ExpAction::North, ExpAction::East, ExpAction::South, ExpAction::West}) {
            const Pos n = neighbour(e.pos, d);
            const bool taken = std::ranges::any_of(f.enemies, [&](const MapEnemy& o) { return o.pos == n; });
            if (here > 0 && f.inBounds(n) && dist[indexOf(f, n)] == here - 1 && !taken) {
                e.pos = n;
                events.push_back(xev::EnemyMoved{index, n});
                break;
            }
        }
    }
}

} // namespace

std::expected<Expedition, TownError> startExpedition(GameState& state, const DungeonDef& def) {
    if (state.hero.hp <= 0)
        return std::unexpected(TownError::HeroDead);
    Rng fork = state.rng.fork();
    Expedition exp;
    exp.seed = (static_cast<std::uint64_t>(fork.next()) << 32) | fork.next();
    exp.rng = Rng::stream(exp.seed, 0); // floors use streams 1 and up
    arrive(exp, generateFloor(def, exp.seed, 1), def);
    return exp;
}

StepResult step(Expedition& exp, GameState& state, const Content& content, const DungeonDef& def,
                ExpAction action) {
    StepResult result;
    exp.message.clear();
    if (action == ExpAction::UseStairs) {
        const Tile here = exp.floor.at(exp.hero);
        if (here == Tile::StairsDown) {
            arrive(exp, generateFloor(def, exp.seed, exp.floor.index + 1), def);
            exp.message = std::format("You go down to floor {}.", exp.floor.index);
            result.events.push_back(xev::Descended{exp.floor.index});
        } else if (here == Tile::StairsUp) {
            result.events.push_back(xev::Left{});
        } else {
            exp.message = "There are no stairs here.";
        }
        return result;
    }
    if (action != ExpAction::Wait) {
        const Pos to = neighbour(exp.hero, action);
        if (!exp.floor.walkable(to)) {
            result.events.push_back(xev::BumpedWall{});
            return result;
        }
        const auto& enemies = exp.floor.enemies;
        if (const auto e = std::ranges::find(enemies, to, &MapEnemy::pos); e != enemies.end()) {
            result.encounterEnemy = static_cast<int>(e - enemies.begin());
            return result;
        }
        exp.hero = to;
        result.events.push_back(xev::Moved{to});
        for (MapChest& chest : exp.floor.chests)
            if (chest.pos == to && !chest.opened)
                openChest(exp, state, content, def, chest, result.events);
    }
    look(exp, def);
    enemyPhase(exp, result.events, result.encounterEnemy);
    return result;
}

ExpeditionSave toSave(const Expedition& exp) {
    ExpeditionSave save{exp.seed, exp.floor.index, exp.hero.x, exp.hero.y, exp.rng.state(), exp.explored, {}, {}};
    for (const MapEnemy& e : exp.floor.enemies)
        save.enemies.push_back({e.enemyId, e.pos.x, e.pos.y, e.awake, e.sleepTurns, e.elite});
    for (const MapChest& c : exp.floor.chests)
        save.chestsOpened.push_back(c.opened);
    return save;
}

std::optional<Expedition> fromSave(const ExpeditionSave& save, const DungeonDef& def) {
    if (save.floor < 1 || save.floor > def.floors)
        return std::nullopt;
    Expedition exp;
    exp.seed = save.seed;
    exp.rng = Rng::fromState(save.rng);
    exp.floor = generateFloor(def, save.seed, save.floor);
    Floor& f = exp.floor;
    exp.hero = {save.heroX, save.heroY};
    // The saved bits come in whole bytes: at most 7 past the last tile.
    if (save.explored.size() < f.tiles.size() || save.explored.size() >= f.tiles.size() + 8 ||
        save.chestsOpened.size() != f.chests.size() || !f.walkable(exp.hero))
        return std::nullopt;
    exp.explored.assign(save.explored.begin(), save.explored.begin() + static_cast<std::ptrdiff_t>(f.tiles.size()));
    f.enemies.clear();
    for (const ExpeditionSave::Enemy& e : save.enemies) {
        if (!f.walkable({e.x, e.y}))
            return std::nullopt;
        f.enemies.push_back({e.id, {e.x, e.y}, e.awake, e.sleepTurns, e.elite});
    }
    for (std::size_t i = 0; i < f.chests.size(); ++i)
        f.chests[i].opened = save.chestsOpened[i];
    computeFov(f, exp.hero, def.fovRadius, exp.visible);
    return exp;
}

EnemyDef encounterEnemy(const Expedition& exp, const Content& content, const DungeonDef& def,
                        int enemyIndex) {
    const MapEnemy& e = exp.floor.enemies[static_cast<std::size_t>(enemyIndex)];
    return scaledEnemy(content.enemy(e.enemyId), def, exp.floor.index);
}

void resolveEncounter(Expedition& exp, GameState& state, const Content& content,
                      const DungeonDef& def, int enemyIndex, const Battle& battle) {
    Hero& hero = state.hero;
    hero.hp = battle.hero().hp;
    hero.mp = battle.hero().mp;
    const EnemyDef enemy = encounterEnemy(exp, content, def, enemyIndex);
    auto& enemies = exp.floor.enemies;
    const auto at = enemies.begin() + enemyIndex;
    switch (battle.outcome()) {
    case Outcome::Victory:
        enemies.erase(at);
        hero.gold += enemy.rewardGold;
        grantExp(hero, content, enemy.rewardExp);
        exp.message = std::format("You defeated the {}.", enemy.name);
        break;
    case Outcome::Defeat:
        hero.hp = 0;
        hero.exp = std::max<std::int64_t>(0, hero.exp - enemy.rewardExp);
        break;
    case Outcome::Fled:
    case Outcome::Draw:
        at->awake = false;
        at->sleepTurns = 3;
        break;
    }
}

} // namespace heroes::game::dungeon
