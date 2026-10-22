#include "game/world/world.hpp"

#include "game/battle.hpp"
#include "game/hero.hpp"
#include "game/town.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>

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
      heroFrom_(def.spawn), lifeTicks_(def.field.lifeRegenTicks), fervorTicks_(def.field.fervorRegenTicks),
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
    for (int& cooldown : cooldowns_)
        if (cooldown > 0)
            --cooldown;
    moveHero(input, events);
    useAbilities(input, events);
    if (input.interact)
        if (const auto landmark = nearbyLandmark())
            events.emplace_back(std::in_place_type<wev::Interact>, *landmark); // in place: GCC 13 misreads a moved temporary as uninitialised
    regenerate();
    refreshDistances();
    advanceBolts(events);
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
    heroFrom_ = hero_;
    hero_ = to;
    moveTicksLeft_ = def_->field.ticksPerTile;
}

namespace {
Motion motion(Tile from, Tile to, int ticksLeft, int ticks) {
    if (ticks <= 0)
        return {to, to, 1, 0};
    const float left = static_cast<float>(std::clamp(ticksLeft, 0, ticks));
    return {from, to, 1 - left / static_cast<float>(ticks), 1 / static_cast<float>(ticks)};
}
} // namespace

Motion World::heroMotion() const {
    return motion(heroFrom_, hero_, moveTicksLeft_, def_->field.ticksPerTile);
}

Motion World::motionOf(const Monster& m) {
    return motion(m.from, m.at, m.stepTicks - m.sinceStep, m.stepTicks);
}

Motion World::motionOf(const Bolt& b) const {
    return motion(b.at, neighbor(b.at, b.dir), b.timer, def_->field.boltTicksPerTile);
}

void World::useAbilities(const WorldInput& input, std::vector<WorldEvent>& events) {
    if (input.aim && *input.aim != hero_) { // turn along the axis with the larger distance
        const int dx = input.aim->x - hero_.x, dy = input.aim->y - hero_.y;
        facing_ = std::abs(dx) >= std::abs(dy) ? (dx > 0 ? Dir::Right : Dir::Left)
                                               : (dy > 0 ? Dir::Down : Dir::Up);
    }
    if ((input.swing || input.aim) && spend(Ability::Swing, events))
        if (const auto target = monsterAt(neighbor(hero_, facing_)))
            hitMonster(*target, power(Ability::Swing), events);
    if (input.firebolt && spend(Ability::Firebolt, events))
        bolts_.push_back({hero_, facing_, def_->field.ability(Ability::Firebolt).reach, 0});
    if (input.nova && spend(Ability::Nova, events)) {
        const int radius = def_->field.ability(Ability::Nova).reach;
        events.push_back(wev::NovaBurst{hero_, radius});
        const int novaPower = power(Ability::Nova);
        for (std::size_t i = monsters_.size(); i-- > 0;) // backwards: a kill removes monsters_[i]
            if (chebyshev(monsters_[i].at, hero_) <= radius)
                hitMonster(i, novaPower, events);
    }
    if (input.potion)
        drinkPotion(events);
}

bool World::spend(Ability a, std::vector<WorldEvent>& events) {
    const AbilityTuning& tuning = def_->field.ability(a);
    int& cooldown = cooldowns_[static_cast<std::size_t>(a)];
    Hero& hero = state_->hero;
    if (cooldown > 0 || hero.mp < tuning.cost) {
        events.push_back(wev::AbilityRefused{a});
        return false;
    }
    hero.mp -= tuning.cost;
    cooldown = tuning.cooldownTicks;
    return true;
}

int World::power(Ability a) const {
    const Hero& hero = state_->hero;
    const std::int64_t atk = hero.atk + equipmentBonus(hero, *content_).atk;
    return static_cast<int>(atk * def_->field.ability(a).powerPercent / 100);
}

void World::drinkPotion(std::vector<WorldEvent>& events) {
    for (const Inventory::Slot& slot : state_->hero.bag.slots()) {
        if (!slot || !restoresHp(content_->item(slot->first)))
            continue;
        std::string id = slot->first; // the slot may empty when it is used
        (void)useConsumable(*state_, *content_, id); // it is in the bag and consumable
        events.push_back(wev::PotionDrunk{std::move(id)});
        return;
    }
    events.push_back(wev::NoPotions{});
}

void World::advanceBolts(std::vector<WorldEvent>& events) {
    for (std::size_t b = 0; b < bolts_.size();) {
        Bolt& bolt = bolts_[b];
        bool gone = false;
        if (expired(bolt.timer)) {
            bolt.timer = def_->field.boltTicksPerTile;
            const Tile next = neighbor(bolt.at, bolt.dir);
            if (!def_->walkable(next)) {
                gone = true; // a tree, wall, water or the map edge stops it
            } else if (const auto target = monsterAt(next)) {
                hitMonster(*target, power(Ability::Firebolt), events);
                gone = true;
            } else {
                bolt.at = next;
                gone = --bolt.tilesLeft == 0;
            }
        }
        if (gone)
            bolts_.erase(bolts_.begin() + static_cast<std::ptrdiff_t>(b));
        else
            ++b;
    }
}

void World::hitMonster(std::size_t index, int power, std::vector<WorldEvent>& events) {
    Monster& m = monsters_[index];
    const int damage = defendedDamage(power, m.stats.def);
    m.hp = std::max(0, m.hp - damage);
    events.push_back(wev::Damaged{m.at, damage, false});
    if (m.hp > 0)
        return;
    const Monster dead = std::move(m);
    monsters_.erase(monsters_.begin() + static_cast<std::ptrdiff_t>(index));
    MonsterRewards rewards = grantMonsterRewards(state_->hero, *content_, rng_, dead.stats,
                                                 def_->zones[dead.zone].level);
    events.push_back(wev::MonsterDefeated{dead.stats.name, dead.at, std::move(rewards)});
}

std::optional<std::size_t> World::monsterAt(Tile t) const {
    const auto it = std::ranges::find(monsters_, t, &Monster::at);
    if (it == monsters_.end())
        return std::nullopt;
    return static_cast<std::size_t>(it - monsters_.begin());
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
    m.sinceStep = std::min(m.sinceStep + 1, m.stepTicks);
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
    if (const auto next = chasing ? chaseStep(m) : wanderStep(m)) {
        m.from = m.at;
        m.at = *next;
        m.stepTicks = m.timer;
        m.sinceStep = 0;
    }
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
    loseToMonster(hero, m.stats);
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
    monsters_.push_back({zone, std::move(stats), hp, at, Monster::Mode::Wander, {},
                         def_->field.wanderTicks, nextMonsterId_++, at});
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
