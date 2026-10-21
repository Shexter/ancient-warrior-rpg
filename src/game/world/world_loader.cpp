#include "game/world/world_loader.hpp"

#include "game/json_fields.hpp"
#include "game/training.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>

namespace heroes::game::world {

namespace {

using namespace detail;

constexpr std::string_view kMapFile = "world/overworld.txt";
constexpr std::string_view kWorldFile = "world/world.json";
constexpr std::string_view kFieldFile = "world/field.json";
constexpr int kMaxSide = 1024; // map width and height
constexpr int kMaxZoneMonsters = 50;

constexpr std::array<std::pair<std::string_view, Terrain>, 9> kTerrains{{
    {"grass", Terrain::Grass},
    {"tall_grass", Terrain::TallGrass},
    {"tree", Terrain::Tree},
    {"water", Terrain::Water},
    {"path", Terrain::Path},
    {"sand", Terrain::Sand},
    {"rock", Terrain::Rock},
    {"wall", Terrain::Wall},
    {"floor", Terrain::Floor},
}};
constexpr std::array<std::pair<std::string_view, Service>, 4> kServices{{
    {"almonry", Service::Almonry},
    {"forge", Service::Forge},
    {"prior", Service::Prior},
    {"thesaurus", Service::Thesaurus},
}};

std::optional<Json> parseJson(Ctx& ctx, std::string_view text) {
    try {
        return Json::parse(text);
    } catch (const Json::exception& e) {
        ctx.errors.push_back({ctx.file, "", e.what()});
        return std::nullopt;
    }
}

// The value under `key`, or nullptr after recording that it is missing.
const Json* required(Fields& f, std::string_view key) {
    const Json* v = f.raw(key);
    if (!v)
        f.fail(std::format("missing required key '{}'", key));
    return v;
}

// The array under `key`, or nullptr after recording why there isn't one.
const Json* requiredArray(Fields& f, std::string_view key) {
    const Json* v = required(f, key);
    if (v && !v->is_array()) {
        f.fail(std::format("key '{}' must be an array", key));
        return nullptr;
    }
    return v;
}

// "landmark 'Forge'", or "landmark #2" when it has no usable name.
std::string label(std::string_view noun, const Json& j, std::size_t index) {
    if (j.is_object()) {
        auto it = j.find("name");
        if (it != j.end() && it->is_string())
            return std::format("{} '{}'", noun, it->get<std::string>());
    }
    return std::format("{} #{}", noun, index + 1);
}

Tile readTile(Fields& f) {
    const int x = f.req<int>("x", 0, kMaxSide - 1);
    const int y = f.req<int>("y", 0, kMaxSide - 1);
    return {x, y};
}

std::vector<TileType> readLegend(Fields& top) {
    std::vector<TileType> legend;
    const Json* j = required(top, "legend");
    if (!j)
        return legend;
    if (!j->is_object()) {
        top.fail("key 'legend' must be an object");
        return legend;
    }
    for (const auto& [key, value] : j->items()) {
        Fields f(top.ctx(), value, std::format("legend '{}'", key), {"terrain", "walkable"});
        if (key.size() != 1 || key[0] < '!' || key[0] > '~') {
            f.fail("a tile key must be one printable character other than space");
            continue;
        }
        const Terrain terrain = readEnum(f, "terrain", kTerrains);
        legend.push_back({key[0], terrain, f.req<bool>("walkable")});
    }
    return legend;
}

std::optional<LandmarkTarget> readOpens(Fields& landmark, const Content& content) {
    const Json* j = required(landmark, "opens");
    if (!j)
        return std::nullopt;
    Fields opens(landmark.ctx(), *j, landmark.entry(), {"service", "training", "mission"},
                 "opens.");
    if (!j->is_object())
        return std::nullopt;
    if (j->size() != 1) {
        opens.fail("key 'opens' must hold exactly one of 'service', 'training' or 'mission'");
        return std::nullopt;
    }
    if (opens.raw("service"))
        return OpensService{readEnum(opens, "service", kServices)};
    if (opens.raw("training")) {
        if (!opens.req<bool>("training"))
            opens.fail("key 'opens.training' must be true");
        return OpensTraining{};
    }
    if (opens.raw("mission")) {
        std::string id = opens.req<std::string>("mission");
        if (opens.raw("mission")->is_string() && !content.findMission(id))
            opens.fail(std::format("unknown mission '{}'", id));
        return OpensMission{std::move(id)};
    }
    return std::nullopt; // the unknown key is already reported
}

std::vector<Landmark> readLandmarks(Fields& top, const Content& content) {
    std::vector<Landmark> landmarks;
    const Json* list = requiredArray(top, "landmarks");
    if (!list)
        return landmarks;
    for (std::size_t i = 0; i < list->size(); ++i) {
        const Json& j = (*list)[i];
        Fields f(top.ctx(), j, label("landmark", j, i), {"name", "x", "y", "opens"});
        std::string name = f.req<std::string>("name");
        const Tile at = readTile(f);
        if (auto opens = readOpens(f, content))
            landmarks.push_back({std::move(name), at, std::move(*opens)});
    }
    return landmarks;
}

std::vector<Zone> readZones(Fields& top) {
    std::vector<Zone> zones;
    const Json* list = requiredArray(top, "zones");
    if (!list)
        return zones;
    for (std::size_t i = 0; i < list->size(); ++i) {
        const Json& j = (*list)[i];
        Fields f(top.ctx(), j, label("zone", j, i),
                 {"name", "x", "y", "w", "h", "level", "count"});
        Zone z{f.req<std::string>("name"),
               f.req<int>("x", 0, kMaxSide - 1),
               f.req<int>("y", 0, kMaxSide - 1),
               f.req<int>("w", 1, kMaxSide),
               f.req<int>("h", 1, kMaxSide),
               f.req<int>("level", 1, TrainingSession::kMaxCaveLevel),
               f.req<int>("count", 1, kMaxZoneMonsters)};
        zones.push_back(std::move(z));
    }
    return zones;
}

// Lines without their terminators; a final newline does not start an extra row.
std::vector<std::string> splitRows(std::string_view text) {
    std::vector<std::string> rows;
    while (!text.empty()) {
        const std::size_t end = std::min(text.find('\n'), text.size());
        std::string_view line = text.substr(0, end);
        if (line.ends_with('\r'))
            line.remove_suffix(1);
        rows.emplace_back(line);
        text.remove_prefix(std::min(end + 1, text.size()));
    }
    return rows;
}

// Fills width, height and grid. Reports ragged rows and characters missing from the legend
// (one per line); the legend check is skipped when the legend itself failed to load.
void readMap(std::string_view text, WorldDef& world, bool checkLegend, Errors& errors) {
    const std::vector<std::string> rows = splitRows(text);
    const auto fail = [&](std::size_t row, std::string message) {
        errors.push_back(
            {std::string(kMapFile), std::format("line {}", row + 1), std::move(message)});
    };
    if (rows.empty() || rows[0].empty()) {
        errors.push_back({std::string(kMapFile), "", "the map is empty"});
        return;
    }
    if (rows.size() > kMaxSide || rows[0].size() > kMaxSide) {
        errors.push_back({std::string(kMapFile), "",
                          std::format("the map is larger than {0}x{0} tiles", kMaxSide)});
        return;
    }
    world.width = static_cast<int>(rows[0].size());
    world.height = static_cast<int>(rows.size());
    world.grid.reserve(static_cast<std::size_t>(world.width * world.height));
    bool ok = true;
    for (std::size_t r = 0; r < rows.size(); ++r) {
        if (rows[r].size() != rows[0].size()) {
            fail(r, std::format("row has {} characters, expected {}", rows[r].size(),
                                rows[0].size()));
            ok = false;
            continue;
        }
        for (const char c : rows[r]) {
            const auto it = std::ranges::find(world.legend, c, &TileType::key);
            if (it == world.legend.end()) {
                if (checkLegend)
                    fail(r, std::format("unknown tile character '{}'", c));
                ok = false;
                break;
            }
            world.grid.push_back(static_cast<int>(std::distance(world.legend.begin(), it)));
        }
    }
    if (!ok)
        world.grid.clear();
}

// Every tile reachable on foot from `start` (walkable, four directions), as a row-major mask.
std::vector<bool> reachableFrom(const WorldDef& world, Tile start) {
    std::vector<bool> seen(world.grid.size(), false);
    const auto index = [&](Tile t) { return static_cast<std::size_t>(t.y * world.width + t.x); };
    std::deque<Tile> queue{start};
    seen[index(start)] = true;
    while (!queue.empty()) {
        const Tile t = queue.front();
        queue.pop_front();
        for (const Dir d : {Dir::Up, Dir::Down, Dir::Left, Dir::Right}) {
            const Tile n = neighbor(t, d);
            if (world.walkable(n) && !seen[index(n)]) {
                seen[index(n)] = true;
                queue.push_back(n);
            }
        }
    }
    return seen;
}

// Checks that need both the map and world.json: placement on walkable tiles, zones inside the
// map, and every landmark reachable from spawn.
void checkPlacement(const WorldDef& world, Errors& errors) {
    const auto fail = [&](std::string entry, std::string message) {
        errors.push_back({std::string(kWorldFile), std::move(entry), std::move(message)});
    };
    const auto where = [](Tile t) { return std::format("({}, {})", t.x, t.y); };
    const auto placementError = [&](Tile t) -> std::optional<std::string> {
        if (!world.inBounds(t))
            return std::format("tile {} is outside the map", where(t));
        if (!world.at(t).walkable)
            return std::format("tile {} can't be walked on", where(t));
        return std::nullopt;
    };

    const auto spawnError = placementError(world.spawn);
    if (spawnError)
        fail("spawn", *spawnError);
    for (const Landmark& l : world.landmarks)
        if (auto e = placementError(l.at))
            fail(std::format("landmark '{}'", l.name), *e);
    for (const Zone& z : world.zones)
        if (z.x + z.w > world.width || z.y + z.h > world.height)
            fail(std::format("zone '{}'", z.name), "the zone lies outside the map");
    if (spawnError)
        return;

    const std::vector<bool> reachable = reachableFrom(world, world.spawn);
    for (const Landmark& l : world.landmarks)
        if (world.walkable(l.at) &&
            !reachable[static_cast<std::size_t>(l.at.y * world.width + l.at.x)])
            fail(std::format("landmark '{}'", l.name),
                 "it can't be reached on foot from the spawn tile");
}

FieldTuning readField(std::string_view text, Errors& errors) {
    Ctx ctx{errors, std::string(kFieldFile)};
    FieldTuning field;
    auto doc = parseJson(ctx, text);
    if (!doc)
        return field;
    Fields f(ctx, *doc, "",
             {"heroStepMs", "monsterWanderMs", "monsterChaseMs", "noticeRadius", "windUpMs",
              "attackCooldownMs", "lifeRegenMs", "fervorRegenMs", "respawnMs", "respawnDistance",
              "boltStepMs", "swing", "firebolt", "nova"});
    // Milliseconds to the nearest whole tick, at least 1.
    const auto ticksIn = [](Fields& from, std::string_view key) {
        const int ms = from.req<int>(key, 1, 600000);
        return std::max(
            1, static_cast<int>((static_cast<std::int64_t>(ms) * kTicksPerSecond + 500) / 1000));
    };
    const auto ticks = [&](std::string_view key) { return ticksIn(f, key); };
    const auto ability = [&](std::string_view key, bool hasReach) {
        AbilityTuning a;
        const Json* j = required(f, key);
        if (!j)
            return a;
        Fields af(ctx, *j, std::string(key),
                  hasReach ? std::initializer_list<std::string_view>{"cost", "cooldownMs",
                                                                     "powerPercent", "reach"}
                           : std::initializer_list<std::string_view>{"cost", "cooldownMs",
                                                                     "powerPercent"});
        a.cost = af.req<int>("cost", 0, 1000000);
        a.cooldownTicks = ticksIn(af, "cooldownMs");
        a.powerPercent = af.req<int>("powerPercent", 0, 1000);
        a.reach = hasReach ? af.req<int>("reach", 1, 32) : 1;
        return a;
    };
    field.ticksPerTile = ticks("heroStepMs");
    field.wanderTicks = ticks("monsterWanderMs");
    field.chaseTicks = ticks("monsterChaseMs");
    field.noticeRadius = f.req<int>("noticeRadius", 1, 32);
    field.windUpTicks = ticks("windUpMs");
    field.attackCooldownTicks = ticks("attackCooldownMs");
    field.lifeRegenTicks = ticks("lifeRegenMs");
    field.fervorRegenTicks = ticks("fervorRegenMs");
    field.respawnTicks = ticks("respawnMs");
    field.respawnDistance = f.req<int>("respawnDistance", 0, kMaxSide);
    field.boltTicksPerTile = ticks("boltStepMs");
    field.abilities[static_cast<std::size_t>(Ability::Swing)] = ability("swing", false);
    field.abilities[static_cast<std::size_t>(Ability::Firebolt)] = ability("firebolt", true);
    field.abilities[static_cast<std::size_t>(Ability::Nova)] = ability("nova", true);
    return field;
}

std::optional<std::string> readText(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in)
        return std::nullopt;
    std::ostringstream text;
    text << in.rdbuf();
    return std::move(text).str();
}

} // namespace

std::expected<WorldDef, std::vector<ContentError>>
parseWorld(std::string_view mapText, std::string_view worldJson, std::string_view fieldJson,
           const Content& content) {
    Errors errors;
    WorldDef world;

    Ctx ctx{errors, std::string(kWorldFile)};
    bool legendLoaded = false;
    if (auto doc = parseJson(ctx, worldJson)) {
        Fields top(ctx, *doc, "", {"legend", "spawn", "landmarks", "zones"});
        const std::size_t before = errors.size();
        world.legend = readLegend(top);
        legendLoaded = errors.size() == before;
        if (const Json* spawn = required(top, "spawn")) {
            Fields f(ctx, *spawn, "spawn", {"x", "y"});
            world.spawn = readTile(f);
        }
        world.landmarks = readLandmarks(top, content);
        world.zones = readZones(top);
    }
    readMap(mapText, world, legendLoaded, errors);
    world.field = readField(fieldJson, errors);

    if (!errors.empty())
        return std::unexpected(std::move(errors));
    checkPlacement(world, errors);
    if (!errors.empty())
        return std::unexpected(std::move(errors));
    return world;
}

std::expected<WorldDef, std::vector<ContentError>> loadWorld(const std::filesystem::path& dataDir,
                                                             const Content& content) {
    Errors errors;
    const auto read = [&](std::string_view file) {
        auto text = readText(dataDir / file);
        if (!text)
            errors.push_back({std::string(file), "", "cannot open file"});
        return text.value_or("");
    };
    const std::string map = read(kMapFile);
    const std::string worldJson = read(kWorldFile);
    const std::string fieldJson = read(kFieldFile);
    if (!errors.empty())
        return std::unexpected(std::move(errors));
    return parseWorld(map, worldJson, fieldJson, content);
}

} // namespace heroes::game::world
