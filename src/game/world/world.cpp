#include "game/world/world.hpp"

#include "game/battle.hpp"
#include "game/hero.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdlib>

namespace heroes::game::world {

namespace {

constexpr std::array<Dir, 4> kDirs{Dir::Up, Dir::Down, Dir::Left, Dir::Right};
constexpr int kUnreached = INT_MAX;

int chebyshev(Tile a, Tile b) { return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)); }
int manhattan(Tile a, Tile b) { return std::abs(a.x - b.x) + std::abs(a.y - b.y); }

// Counts a timer down; true when it has run out (also when it was already at 0).
bool expired(int& timer) { return timer <= 0 || --timer == 0; }

} // namespace

World::World(const WorldDef& def, GameState& state, const Content& content)
    : def_(&def), state_(&state), content_(&content), rng_(state.rng.fork()), hero_(def.spawn),
      lifeTicks_(def.field.lifeRegenTicks), fervorTicks_(def.field.fervorRegenTicks),
      respawnTicks_(def.zones.size(), def.field.respawnTicks) {
    const int reach = 2 * def.field.noticeRadius;
    distances_.reserve(static_cast<std::size_t>((2 * reach + 1) * (2 * reach + 1)));
    queue_.reserve(distances_.capacity());
    for (std::size_t z = 0; z < def.zones.size(); ++z)
        for (int i = 0; i < def.zones[z].count; ++i)
            if (!spawnOne(z))
                break; // respawn() fills the rest once tiles far enough from the hero exist
}

std::vector<WorldEvent> World::step(const WorldInput& input) {
    std::vector<WorldEvent> events;
    if (fallen_)
        return events;
    moveHero(input, events);
    if (input.interact)
        if (const auto landmark = nearbyLandmark())
            events.push_back(wev::Interact{*landmark});
    regenerate();
    refreshDistances();
    for (Monster& m : monsters_) {
        updateMonster(m, events);
        if (fallen_)
            return events;
    }
    respawn();
    return events;
}

bool World::occupied(Tile t) const {
    return t == hero_ || std::ranges::any_of(monsters_, [&](const Monster& m) { return m.at == t; });
}

bool World::inZone(std::size_t zone, Tile t) const {
    const Zone& z = def_->zones[zone];
    return t.x >= z.x && t.y >= z.y && t.x < z.x + z.w && t.y < z.y + z.h;
}

void World::moveHero(const WorldInput& input, std::vector<WorldEvent>& events) {
    if (moveTicksLeft_ > 0)
        --moveTicksLeft_;
    if (moveTicksLeft_ != 0 || !input.move)
        return;
    facing_ = *input.move;
    const Tile to = neighbor(hero_, facing_);
    if (!def_->walkable(to) || occupied(to))
        return;
    events.push_back(wev::HeroMoved{hero_, to});
    hero_ = to;
    moveTicksLeft_ = def_->field.ticksPerTile;
}

void World::regenerate() {
    Hero& hero = state_->hero;
    if (expired(lifeTicks_)) {
        lifeTicks_ = def_->field.lifeRegenTicks;
        const int life = hero.life + equipmentBonus(hero, *content_).life;
        if (hero.hp < hero.maxHp && life > 0)
            hero.hp = std::min(hero.maxHp, hero.hp + life);
    }
    if (expired(fervorTicks_)) {
        fervorTicks_ = def_->field.fervorRegenTicks;
        hero.mp = std::min(hero.maxMp, hero.mp + 1);
    }
}

void World::updateMonster(Monster& m, std::vector<WorldEvent>& events) {
    const FieldTuning& field = def_->field;
    switch (m.mode) {
    case Monster::Mode::WindUp:
        if (!expired(m.timer))
            return;
        if (hero_ == m.target)
            hitHero(m, events);
        m.mode = Monster::Mode::Cooldown;
        m.timer = field.attackCooldownTicks;
        return;
    case Monster::Mode::Cooldown:
        if (expired(m.timer))
            m.mode = Monster::Mode::Wander; // and acts on its next tick
        return;
    case Monster::Mode::Wander:
    case Monster::Mode::Chase:
        break;
    }

    const int reach = m.mode == Monster::Mode::Chase ? 2 * field.noticeRadius : field.noticeRadius;
    const bool chasing = inZone(m.zone, hero_) && chebyshev(m.at, hero_) <= reach;
    if (chasing && m.mode == Monster::Mode::Wander)
        m.timer = std::min(m.timer, field.chaseTicks); // noticing speeds it up at once
    m.mode = chasing ? Monster::Mode::Chase : Monster::Mode::Wander;
    if (chasing && manhattan(m.at, hero_) == 1) {
        m.mode = Monster::Mode::WindUp;
        m.target = hero_;
        m.timer = field.windUpTicks;
        return;
    }
    if (!expired(m.timer))
        return;
    m.timer = chasing ? field.chaseTicks : field.wanderTicks;
    if (const auto next = chasing ? chaseStep(m) : wanderStep(m))
        m.at = *next;
}

std::optional<Tile> World::chaseStep(const Monster& m) const {
    std::optional<Tile> best;
    int bestDistance = distanceAt(m.at);
    for (const Dir d : kDirs) {
        const Tile n = neighbor(m.at, d);
        if (!def_->walkable(n) || !inZone(m.zone, n) || occupied(n))
            continue;
        if (const int distance = distanceAt(n); distance < bestDistance) {
            best = n;
            bestDistance = distance;
        }
    }
    return best;
}

std::optional<Tile> World::wanderStep(const Monster& m) {
    const std::uint32_t pick = rng_.below(5); // four directions, or stay
    if (pick == 4)
        return std::nullopt;
    const Tile n = neighbor(m.at, kDirs[pick]);
    if (!def_->walkable(n) || !inZone(m.zone, n) || occupied(n))
        return std::nullopt;
    return n;
}

void World::hitHero(const Monster& m, std::vector<WorldEvent>& events) {
    Hero& hero = state_->hero;
    const int damage =
        defendedDamage(m.stats.atk, hero.def + equipmentBonus(hero, *content_).def);
    hero.hp = std::max(0, hero.hp - damage);
    events.push_back(wev::Damaged{hero_, damage, true});
    if (hero.hp > 0)
        return;
    hero.exp = std::max<std::int64_t>(0, hero.exp - m.stats.expReward);
    fallen_ = true;
    events.push_back(wev::HeroFell{});
}

void World::refreshDistances() {
    if (distanceOrigin_ == hero_)
        return;
    distanceOrigin_ = hero_;
    const int reach = 2 * def_->field.noticeRadius, side = 2 * reach + 1;
    distances_.assign(static_cast<std::size_t>(side * side), kUnreached);
    const auto index = [&](Tile t) {
        return static_cast<std::size_t>((t.y - hero_.y + reach) * side + (t.x - hero_.x + reach));
    };
    queue_.clear();
    queue_.push_back(hero_);
    distances_[index(hero_)] = 0;
    for (std::size_t head = 0; head < queue_.size(); ++head) {
        const Tile t = queue_[head];
        for (const Dir d : kDirs) {
            const Tile n = neighbor(t, d);
            if (chebyshev(n, hero_) > reach || !def_->walkable(n) ||
                distances_[index(n)] != kUnreached)
                continue;
            distances_[index(n)] = distances_[index(t)] + 1;
            queue_.push_back(n);
        }
    }
}

int World::distanceAt(Tile t) const {
    const int reach = 2 * def_->field.noticeRadius, side = 2 * reach + 1;
    if (!distanceOrigin_ || chebyshev(t, *distanceOrigin_) > reach)
        return kUnreached;
    return distances_[static_cast<std::size_t>((t.y - distanceOrigin_->y + reach) * side +
                                               (t.x - distanceOrigin_->x + reach))];
}

bool World::spawnOne(std::size_t zone) {
    const Zone& z = def_->zones[zone];
    candidates_.clear();
    for (int y = z.y; y < z.y + z.h; ++y)
        for (int x = z.x; x < z.x + z.w; ++x)
            if (const Tile t{x, y}; def_->walkable(t) && !occupied(t) &&
                                    chebyshev(t, hero_) >= def_->field.respawnDistance)
                candidates_.push_back(t);
    if (candidates_.empty())
        return false;
    // Draw order: the tile, then the monster (atk, def, gold).
    const Tile at = candidates_[rng_.below(static_cast<std::uint32_t>(candidates_.size()))];
    TrainingMonster stats = rollTrainingMonster(rng_, z.level);
    const int hp = stats.maxHp;
    monsters_.push_back(
        {zone, std::move(stats), hp, at, Monster::Mode::Wander, {}, def_->field.wanderTicks});
    return true;
}

void World::respawn() {
    for (std::size_t z = 0; z < def_->zones.size(); ++z) {
        const auto alive = std::ranges::count(monsters_, z, &Monster::zone);
        if (alive >= def_->zones[z].count) {
            respawnTicks_[z] = def_->field.respawnTicks;
            continue;
        }
        if (expired(respawnTicks_[z]) && spawnOne(z))
            respawnTicks_[z] = def_->field.respawnTicks;
    }
}

std::optional<std::size_t> World::nearbyLandmark() const {
    std::optional<std::size_t> best;
    int bestDistance = 0;
    for (std::size_t i = 0; i < def_->landmarks.size(); ++i) {
        const Tile at = def_->landmarks[i].at;
        const int dx = std::abs(at.x - hero_.x), dy = std::abs(at.y - hero_.y);
        if (dx > 1 || dy > 1)
            continue;
        if (!best || dx + dy < bestDistance) {
            best = i;
            bestDistance = dx + dy;
        }
    }
    return best;
}

} // namespace heroes::game::world
