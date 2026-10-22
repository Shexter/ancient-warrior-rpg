#include "game/dungeon/dungeon_def.hpp"

#include "game/json_fields.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <sstream>

namespace heroes::game::dungeon {

namespace {

using namespace detail;

constexpr int kMaxCount = 100; // enemies or chests per floor

bool has(const std::vector<int>& floors, int floor) { return std::ranges::find(floors, floor) != floors.end(); }

// The object under `key`, or nullptr after recording that it's missing or not an object.
const Json* object(Fields& f, std::string_view key) {
    const Json* v = f.raw(key);
    if (!v || !v->is_object()) {
        f.fail(std::format("key '{}' must be an object", key));
        return nullptr;
    }
    return v;
}

const Json* array(Fields& f, std::string_view key) {
    const Json* v = f.raw(key);
    if (!v || !v->is_array()) {
        f.fail(std::format("key '{}' must be an array", key));
        return nullptr;
    }
    return v;
}

std::vector<int> floorList(Fields& f, int floors) {
    std::vector<int> out;
    if (const Json* list = array(f, "floors"))
        for (const Json& v : *list) {
            int n = 0;
            if (convert(v, n) && n >= 1 && n <= floors)
                out.push_back(n);
            else
                f.fail(std::format("floors must be numbers from 1 to {}", floors));
        }
    return out;
}

// Each floor 1..floors appears in exactly one entry of `lists`.
template <class T>
void checkCoverage(Ctx& ctx, const std::vector<T>& lists, int floors, std::string_view noun) {
    for (int floor = 1; floor <= floors; ++floor) {
        const auto n = std::ranges::count_if(lists, [&](const T& l) { return has(l.floors, floor); });
        if (n != 1)
            ctx.errors.push_back({ctx.file, "", std::format("floor {} is in {} {}s (needs exactly one)", floor, n, noun)});
    }
}

} // namespace

const EnemyPool& DungeonDef::poolFor(int floor) const {
    return *std::ranges::find_if(pools, [&](const EnemyPool& p) { return has(p.floors, floor); });
}

const ChestTable& DungeonDef::chestsFor(int floor) const {
    return *std::ranges::find_if(chestTables, [&](const ChestTable& t) { return has(t.floors, floor); });
}

std::expected<DungeonDef, std::vector<ContentError>> parseDungeon(std::string_view json,
                                                                  const Content& content) {
    Errors errors;
    Ctx ctx{errors, "dungeon.json"};
    Json doc;
    try {
        doc = Json::parse(json);
    } catch (const Json::exception& e) {
        errors.push_back({ctx.file, "", e.what()});
        return std::unexpected(std::move(errors));
    }
    DungeonDef d{};
    Fields outer(ctx, doc, "", {"dungeon"});
    const Json* body = object(outer, "dungeon");
    if (!body)
        return std::unexpected(std::move(errors));
    Fields f(ctx, *body, "",
             {"name", "width", "height", "floors", "fovRadius", "bsp", "scalingPercentPerFloor",
              "enemyCount", "chestCount", "safeRadius", "pools", "elite", "chestTables"});
    d.name = f.req<std::string>("name");
    d.width = f.req<int>("width", 40, 200);
    d.height = f.req<int>("height", 20, 100);
    d.floors = f.req<int>("floors", 1, 20);
    d.fovRadius = f.req<int>("fovRadius", 1, 20);
    d.scalingPercentPerFloor = f.req<int>("scalingPercentPerFloor", 0, 1000);
    d.safeRadius = f.req<int>("safeRadius", 0, 20);
    if (const Json* j = object(f, "bsp")) {
        Fields b(ctx, *j, "bsp", {"minLeafWidth", "minLeafHeight", "maxDepth", "minRoomWidth", "minRoomHeight"});
        d.bsp = {b.req<int>("minLeafWidth", 3, 200), b.req<int>("minLeafHeight", 3, 100),
                 b.req<int>("maxDepth", 0, 10), b.req<int>("minRoomWidth", 1, 200),
                 b.req<int>("minRoomHeight", 1, 100)};
        if (d.bsp.minLeafWidth < d.bsp.minRoomWidth + 2 || d.bsp.minLeafHeight < d.bsp.minRoomHeight + 2)
            b.fail("a leaf must be at least 2 wider and taller than the smallest room");
    }
    if (const Json* j = object(f, "enemyCount")) {
        Fields c(ctx, *j, "enemyCount", {"base", "perFloor", "random"});
        d.enemyCount = {c.req<int>("base", 0, kMaxCount), c.req<int>("perFloor", 0, kMaxCount),
                        c.req<int>("random", 1, kMaxCount)};
    }
    if (const Json* j = object(f, "chestCount")) {
        Fields c(ctx, *j, "chestCount", {"base", "random", "lastFloorBonus"});
        d.chestCount = {c.req<int>("base", 0, kMaxCount), c.req<int>("random", 1, kMaxCount),
                        c.req<int>("lastFloorBonus", 0, kMaxCount)};
    }
    if (const Json* list = array(f, "pools"))
        for (std::size_t i = 0; i < list->size(); ++i) {
            Fields p(ctx, (*list)[i], std::format("pool #{}", i + 1), {"floors", "enemies"});
            EnemyPool pool{floorList(p, d.floors), {}};
            if (const Json* enemies = array(p, "enemies"))
                for (const Json& e : *enemies) {
                    Fields w(ctx, e, p.entry(), {"id", "weight"});
                    WeightedEnemy we{w.req<std::string>("id"), w.req<int>("weight", 1, 10000)};
                    if (!we.id.empty() && !content.findEnemy(we.id))
                        w.fail(std::format("unknown enemy '{}'", we.id));
                    pool.enemies.push_back(std::move(we));
                }
            if (pool.enemies.empty())
                p.fail("needs at least one enemy");
            d.pools.push_back(std::move(pool));
        }
    if (const Json* j = object(f, "elite")) {
        Fields e(ctx, *j, "elite", {"floor", "enemy"});
        d.elite = {e.req<int>("floor", 1, 20), e.req<std::string>("enemy")};
        if (!d.elite.enemy.empty() && !content.findEnemy(d.elite.enemy))
            e.fail(std::format("unknown enemy '{}'", d.elite.enemy));
        if (d.elite.floor != d.floors)
            e.fail(std::format("the elite must be on the last floor ({})", d.floors));
    }
    if (const Json* list = array(f, "chestTables"))
        for (std::size_t i = 0; i < list->size(); ++i) {
            Fields t(ctx, (*list)[i], std::format("chest table #{}", i + 1), {"floors", "entries"});
            ChestTable table{floorList(t, d.floors), {}};
            if (const Json* entries = array(t, "entries"))
                for (const Json& e : *entries) {
                    Fields c(ctx, e, t.entry(), {"item", "weight", "min", "max"});
                    ChestEntry ce{c.req<std::string>("item"), c.req<int>("weight", 1, 10000),
                                  c.req<int>("min", 1, 99), c.req<int>("max", 1, 99)};
                    if (!ce.item.empty() && !content.findItem(ce.item))
                        c.fail(std::format("unknown item '{}'", ce.item));
                    if (ce.min > ce.max)
                        c.fail(std::format("min {} is more than max {}", ce.min, ce.max));
                    table.entries.push_back(std::move(ce));
                }
            if (table.entries.empty())
                t.fail("needs at least one entry");
            d.chestTables.push_back(std::move(table));
        }
    checkCoverage(ctx, d.pools, d.floors, "pool");
    checkCoverage(ctx, d.chestTables, d.floors, "chest table");
    if (!errors.empty())
        return std::unexpected(std::move(errors));
    return d;
}

std::expected<DungeonDef, std::vector<ContentError>> loadDungeon(const std::filesystem::path& dataDir,
                                                                 const Content& content) {
    std::ifstream in(dataDir / "dungeon.json", std::ios::binary);
    if (!in)
        return std::unexpected(std::vector<ContentError>{{"dungeon.json", "", "cannot open file"}});
    std::ostringstream text;
    text << in.rdbuf();
    return parseDungeon(text.str(), content);
}

EnemyDef scaledEnemy(const EnemyDef& enemy, const DungeonDef& def, int floor) {
    const std::int64_t p = 100 + static_cast<std::int64_t>(def.scalingPercentPerFloor) * (floor - 1);
    const auto scale = [&](auto v) { return static_cast<decltype(v)>(v * p / 100); };
    EnemyDef out = enemy;
    out.maxHp = scale(enemy.maxHp);
    out.atk = scale(enemy.atk);
    out.def = scale(enemy.def);
    out.life = scale(enemy.life);
    out.rewardExp = scale(enemy.rewardExp);
    out.rewardGold = scale(enemy.rewardGold);
    out.rewardItems.clear();
    return out;
}

} // namespace heroes::game::dungeon
