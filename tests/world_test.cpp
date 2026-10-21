#include "game/battle.hpp"
#include "game/hero.hpp"
#include "game/world/world.hpp"
#include "game/world/world_loader.hpp"
#include "state_helpers.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <string>
#include <regex>
#include <tuple>
#include <vector>

using namespace heroes::game;
using namespace heroes::game::world;

namespace {

// A 12x12 field of grass: '.' grass, '~' water, '#' wall. Spawn (1, 1), one landmark (5, 5).
std::string openMap() {
    std::string map;
    for (int y = 0; y < 12; ++y)
        map += std::string(12, '.') + '\n';
    return map;
}

// Replaces the character at (x, y) of a map made by openMap().
std::string withTile(std::string map, int x, int y, char c) {
    map[static_cast<std::size_t>(y * 13 + x)] = c;
    return map;
}

std::string worldJson(std::string spawn = R"({"x": 1, "y": 1})",
                      std::string landmarks =
                          R"([{"name": "Forge", "x": 5, "y": 5, "opens": {"service": "forge"}}])",
                      std::string zones =
                          R"([{"name": "Meadow", "x": 6, "y": 6, "w": 4, "h": 4, "level": 1, "count": 2}])",
                      std::string legend = R"({".": {"terrain": "grass", "walkable": true},
                                              "~": {"terrain": "water", "walkable": false},
                                              "#": {"terrain": "wall", "walkable": false}})") {
    return R"({"legend": )" + legend + R"(, "spawn": )" + spawn + R"(, "landmarks": )" +
           landmarks + R"(, "zones": )" + zones + "}";
}

// Field tuning for the tests: the shipped pace, and monsters that act quickly.
constexpr const char* kField = R"({"heroStepMs": 125, "monsterWanderMs": 500,
    "monsterChaseMs": 250, "noticeRadius": 4, "windUpMs": 500, "attackCooldownMs": 1000,
    "lifeRegenMs": 2000, "fervorRegenMs": 500, "respawnMs": 1000, "respawnDistance": 5})";

std::expected<WorldDef, std::vector<ContentError>> parse(const std::string& map,
                                                         const std::string& json,
                                                         const std::string& field = kField) {
    return parseWorld(map, json, field, shippedContent());
}

std::string allErrors(const std::vector<ContentError>& errors) {
    std::string all;
    for (const auto& e : errors)
        all += formatError(e) + "\n";
    return all;
}

struct BadWorld {
    const char* name;
    std::string map;
    std::string json;
    std::string field;
    const char* file;
    const char* entry;
    const char* message;
};

class WorldValidation : public testing::TestWithParam<BadWorld> {};

} // namespace

TEST(WorldLoader, ParsesAValidWorld) {
    auto world = parse(openMap(), worldJson());
    ASSERT_TRUE(world.has_value()) << allErrors(world.error());
    EXPECT_EQ(world->width, 12);
    EXPECT_EQ(world->height, 12);
    EXPECT_EQ(world->spawn, (Tile{1, 1}));
    ASSERT_EQ(world->landmarks.size(), 1U);
    EXPECT_EQ(world->landmarks[0].name, "Forge");
    EXPECT_EQ(world->landmarks[0].opens, LandmarkTarget{OpensService{Service::Forge}});
    ASSERT_EQ(world->zones.size(), 1U);
    EXPECT_EQ(world->zones[0].level, 1);
    EXPECT_EQ(world->field.ticksPerTile, 8); // 125 ms at 60 ticks per second
    EXPECT_TRUE(world->walkable({0, 0}));
    EXPECT_FALSE(world->walkable({12, 0}));
}

TEST(WorldLoader, AcceptsWindowsLineEndingsAndOtherTargets) {
    std::string map;
    for (int y = 0; y < 12; ++y)
        map += std::string(12, '.') + "\r\n";
    auto world = parse(map, worldJson(R"({"x": 1, "y": 1})",
                                      R"([{"name": "Catacombs", "x": 2, "y": 2, "opens": {"training": true}},
                                          {"name": "Lair", "x": 3, "y": 3, "opens": {"mission": "avaritia"}}])"));
    ASSERT_TRUE(world.has_value()) << allErrors(world.error());
    EXPECT_EQ(world->width, 12);
    EXPECT_EQ(world->landmarks[0].opens, LandmarkTarget{OpensTraining{}});
    EXPECT_EQ(world->landmarks[1].opens, LandmarkTarget{OpensMission{"avaritia"}});
}

TEST(WorldLoader, ShippedWorldLoads) {
    auto world = loadWorld(std::filesystem::path(HEROES_SOURCE_DIR) / "data", shippedContent());
    ASSERT_TRUE(world.has_value()) << allErrors(world.error());
    EXPECT_TRUE(world->walkable(world->spawn));
    EXPECT_FALSE(world->landmarks.empty());
}

TEST(WorldLoader, MissingFilesAreNamed) {
    auto world = loadWorld(std::filesystem::path(HEROES_SOURCE_DIR) / "tests", shippedContent());
    ASSERT_FALSE(world.has_value());
    EXPECT_EQ(allErrors(world.error()), "world/overworld.txt: cannot open file\n"
                                        "world/world.json: cannot open file\n"
                                        "world/field.json: cannot open file\n");
}

TEST_P(WorldValidation, ReportsExpectedError) {
    const BadWorld& c = GetParam();
    auto world = parse(c.map, c.json, c.field);
    ASSERT_FALSE(world.has_value()) << c.name << " unexpectedly loaded";
    const bool found = std::ranges::any_of(world.error(), [&](const ContentError& e) {
        return e.file == c.file && e.entry.contains(c.entry) && e.message.contains(c.message);
    });
    EXPECT_TRUE(found) << c.name << ":\n" << allErrors(world.error());
}

INSTANTIATE_TEST_SUITE_P(
    Rules, WorldValidation,
    testing::Values(
        BadWorld{"ragged row", "............\n.....\n", worldJson(), kField,
                 "world/overworld.txt", "line 2", "row has 5 characters, expected 12"},
        BadWorld{"unknown tile on line 12", withTile(openMap(), 4, 11, 'Q'), worldJson(), kField,
                 "world/overworld.txt", "line 12", "unknown tile character 'Q'"},
        BadWorld{"empty map", "", worldJson(), kField, "world/overworld.txt", "",
                 "the map is empty"},
        BadWorld{"spawn on water", withTile(openMap(), 1, 1, '~'), worldJson(), kField,
                 "world/world.json", "spawn", "tile (1, 1) can't be walked on"},
        BadWorld{"spawn outside", openMap(), worldJson(R"({"x": 20, "y": 1})"), kField,
                 "world/world.json", "spawn", "tile (20, 1) is outside the map"},
        BadWorld{"landmark on a wall", withTile(openMap(), 5, 5, '#'), worldJson(), kField,
                 "world/world.json", "landmark 'Forge'", "tile (5, 5) can't be walked on"},
        BadWorld{"unknown service", openMap(),
                 worldJson(R"({"x": 1, "y": 1})",
                           R"([{"name": "Smithy", "x": 5, "y": 5, "opens": {"service": "smithy"}}])"),
                 kField, "world/world.json", "landmark 'Smithy'", "unknown service 'smithy'"},
        BadWorld{"unknown mission", openMap(),
                 worldJson(R"({"x": 1, "y": 1})",
                           R"([{"name": "Lair", "x": 5, "y": 5, "opens": {"mission": "luxuria"}}])"),
                 kField, "world/world.json", "landmark 'Lair'", "unknown mission 'luxuria'"},
        BadWorld{"two targets", openMap(),
                 worldJson(R"({"x": 1, "y": 1})",
                           R"([{"name": "Both", "x": 5, "y": 5, "opens": {"training": true, "service": "forge"}}])"),
                 kField, "world/world.json", "landmark 'Both'", "exactly one of"},
        BadWorld{"zone outside", openMap(),
                 worldJson(R"({"x": 1, "y": 1})", "[]",
                           R"([{"name": "Far", "x": 10, "y": 10, "w": 4, "h": 4, "level": 1, "count": 1}])"),
                 kField, "world/world.json", "zone 'Far'", "the zone lies outside the map"},
        BadWorld{"zone level", openMap(),
                 worldJson(R"({"x": 1, "y": 1})", "[]",
                           R"([{"name": "Deep", "x": 1, "y": 1, "w": 2, "h": 2, "level": 31, "count": 1}])"),
                 kField, "world/world.json", "zone 'Deep'", "'level' must be between 1 and 30"},
        BadWorld{"unreachable landmark",
                 withTile(withTile(withTile(withTile(openMap(), 5, 4, '~'), 5, 6, '~'), 4, 5, '~'),
                          6, 5, '~'),
                 worldJson(), kField, "world/world.json", "landmark 'Forge'",
                 "can't be reached on foot from the spawn tile"},
        BadWorld{"long legend key", openMap(),
                 worldJson(R"({"x": 1, "y": 1})", "[]", "[]",
                           R"({".": {"terrain": "grass", "walkable": true},
                               "ab": {"terrain": "grass", "walkable": true}})"),
                 kField, "world/world.json", "legend 'ab'", "one printable character"},
        BadWorld{"unknown terrain", openMap(),
                 worldJson(R"({"x": 1, "y": 1})", "[]", "[]",
                           R"({".": {"terrain": "lava", "walkable": true}})"),
                 kField, "world/world.json", "legend '.'", "unknown terrain 'lava'"},
        BadWorld{"bad json", openMap(), "{", kField, "world/world.json", "", "parse error"},
        BadWorld{"zero step", openMap(), worldJson(),
                 std::regex_replace(kField, std::regex(R"("heroStepMs": 125)"), R"("heroStepMs": 0)"),
                 "world/field.json", "", "'heroStepMs' must be between 1 and 600000"},
        BadWorld{"missing tuning", openMap(), worldJson(), R"({"heroStepMs": 125})",
                 "world/field.json", "", "missing required key 'monsterWanderMs'"}),
    [](const testing::TestParamInfo<BadWorld>& param) {
        std::string name = param.param.name;
        const auto notAlnum = [](char c) { return !std::isalnum(static_cast<unsigned char>(c)); };
        std::ranges::replace_if(name, notAlnum, '_');
        return name;
    });

namespace {

// Spawn (1, 1) on the open 12x12 field, with water at (3, 1).
WorldDef movementWorld() {
    auto world =
        parse(withTile(openMap(), 3, 1, '~'), worldJson(R"({"x": 1, "y": 1})", "[]", "[]"));
    EXPECT_TRUE(world.has_value());
    return std::move(*world);
}

// Runs `ticks` steps holding `dir` (or nothing), collecting the events.
std::vector<WorldEvent> run(World& w, std::optional<Dir> dir, int ticks) {
    std::vector<WorldEvent> all;
    for (int i = 0; i < ticks; ++i) {
        auto events = w.step({dir});
        all.insert(all.end(), events.begin(), events.end());
    }
    return all;
}

} // namespace

TEST(World, StartsOnSpawnFacingDownAndForksOnce) {
    const WorldDef def = movementWorld();
    GameState state = freshState();
    const Rng before = state.rng;
    World w(def, state, shippedContent());
    EXPECT_EQ(w.hero(), (Tile{1, 1}));
    EXPECT_EQ(w.facing(), Dir::Down);
    EXPECT_EQ(state.rng, afterOneFork(before));
}

TEST(World, TapMovesOneTile) {
    const WorldDef def = movementWorld();
    GameState state = freshState();
    World w(def, state, shippedContent());
    auto events = run(w, Dir::Down, 1);
    run(w, std::nullopt, 30);
    EXPECT_EQ(w.hero(), (Tile{1, 2}));
    EXPECT_EQ(w.facing(), Dir::Down);
    ASSERT_EQ(events.size(), 1U);
    EXPECT_EQ(events[0], (WorldEvent{wev::HeroMoved{{1, 1}, {1, 2}}}));
}

TEST(World, HeldKeyWalksAtThePace) {
    const WorldDef def = movementWorld();
    GameState state = freshState();
    World w(def, state, shippedContent());
    const auto events = run(w, Dir::Down, 60); // one second at 8 ticks per tile
    EXPECT_EQ(w.hero(), (Tile{1, 9}));
    EXPECT_EQ(events.size(), 8U);
}

TEST(World, ReleaseStopsAtTheEndOfTheTile) {
    const WorldDef def = movementWorld();
    GameState state = freshState();
    World w(def, state, shippedContent());
    run(w, Dir::Down, 3); // released mid-move
    run(w, std::nullopt, 20);
    EXPECT_EQ(w.hero(), (Tile{1, 2}));
}

TEST(World, BlockedByWater) {
    const WorldDef def = movementWorld();
    GameState state = freshState();
    World w(def, state, shippedContent());
    run(w, Dir::Right, 30); // (1,1) -> (2,1), then the water at (3,1) blocks
    EXPECT_EQ(w.hero(), (Tile{2, 1}));
    EXPECT_EQ(w.facing(), Dir::Right);
}

TEST(World, MapEdgeBlocksAndARefusedMoveStillTurns) {
    const WorldDef def = movementWorld();
    GameState state = freshState();
    World w(def, state, shippedContent());
    run(w, Dir::Up, 30);
    EXPECT_EQ(w.hero(), (Tile{1, 0}));
    EXPECT_EQ(w.facing(), Dir::Up);
    run(w, Dir::Left, 30);
    EXPECT_EQ(w.hero(), (Tile{0, 0}));
    run(w, Dir::Up, 1);
    EXPECT_EQ(w.hero(), (Tile{0, 0}));
    EXPECT_EQ(w.facing(), Dir::Up);
}

TEST(World, ReplayIsDeterministic) {
    const WorldDef def = movementWorld();
    Rng script = Rng::stream(42, 0);
    std::vector<std::optional<Dir>> inputs;
    for (int i = 0; i < 600; ++i) {
        const auto pick = script.below(5);
        inputs.push_back(pick == 4 ? std::nullopt : std::optional(static_cast<Dir>(pick)));
    }
    const auto play = [&](GameState& state) {
        World w(def, state, shippedContent());
        std::vector<WorldEvent> events;
        for (const auto& in : inputs) {
            auto step = w.step({in});
            events.insert(events.end(), step.begin(), step.end());
        }
        return std::tuple(events, w.hero(), w.facing());
    };
    GameState a = freshState(), b = freshState();
    EXPECT_EQ(play(a), play(b));
    EXPECT_EQ(a, b);
}

namespace {

// Spawn (1, 1); the Forge at (3, 1) and the Catacombs at (2, 2).
WorldDef landmarkWorld() {
    auto world = parse(openMap(), worldJson(R"({"x": 1, "y": 1})",
                                            R"([{"name": "Forge", "x": 3, "y": 1, "opens": {"service": "forge"}},
                                                {"name": "Catacombs", "x": 2, "y": 2, "opens": {"training": true}}])",
                                            "[]"));
    EXPECT_TRUE(world.has_value());
    return std::move(*world);
}

} // namespace

TEST(World, NearbyLandmarkCountsDiagonalsAndPrefersTheClosest) {
    const WorldDef def = landmarkWorld();
    GameState state = freshState();
    World w(def, state, shippedContent());
    EXPECT_EQ(w.nearbyLandmark(), std::optional<std::size_t>(1)); // Catacombs diagonally
    run(w, Dir::Right, 1);                                          // (2, 1): both adjacent
    EXPECT_EQ(w.nearbyLandmark(), std::optional<std::size_t>(0));   // a tie: listed first
    run(w, std::nullopt, 8);
    run(w, Dir::Left, 9);
    run(w, Dir::Left, 1); // (0, 1): two tiles from the nearest
    EXPECT_EQ(w.hero(), (Tile{0, 1}));
    EXPECT_EQ(w.nearbyLandmark(), std::nullopt);
}

TEST(World, InteractEmitsOnlyNextToALandmark) {
    const WorldDef def = landmarkWorld();
    GameState state = freshState();
    World w(def, state, shippedContent());
    EXPECT_EQ(w.step({std::nullopt, true}),
              (std::vector<WorldEvent>{wev::Interact{1}}));
    run(w, Dir::Left, 1); // (0, 1)
    run(w, std::nullopt, 8);
    EXPECT_TRUE(w.step({std::nullopt, true}).empty());
}

// ---- Field monsters (chunk 5) ---------------------------------------------------------------

namespace {

// A 12x12 open field with `zones` and tuning `fieldJson`, spawn (1, 1).
WorldDef fieldWorld(const std::string& zones, const std::string& map = openMap(),
                    const std::string& fieldJson = kField) {
    auto world = parse(map, worldJson(R"({"x": 1, "y": 1})", "[]", zones), fieldJson);
    EXPECT_TRUE(world.has_value()) << allErrors(world.error());
    return std::move(*world);
}

std::string withField(std::string field, const std::string& key, int value) {
    return std::regex_replace(field, std::regex("\"" + key + "\": [0-9]+"),
                              "\"" + key + "\": " + std::to_string(value));
}

// Steps with no input until `done` holds, at most `limit` ticks; collects the events.
template <class Done>
std::vector<WorldEvent> until(World& w, Done done, int limit = 6000) {
    std::vector<WorldEvent> all;
    for (int i = 0; i < limit && !done(); ++i) {
        auto events = w.step({});
        all.insert(all.end(), events.begin(), events.end());
    }
    return all;
}

bool windingUp(const World& w) {
    return !w.monsters().empty() && w.monsters()[0].mode == Monster::Mode::WindUp;
}

int heroDef(const GameState& state) {
    return state.hero.def + equipmentBonus(state.hero, shippedContent()).def;
}

// One monster, in a 3x3 zone at x 4-6 that the hero walks into: (4, 1) is in the zone, and with
// respawnDistance 5 the monster can only start on the zone's far column (x = 6).
const std::string kCornerZone =
    R"([{"name": "Corner", "x": 4, "y": 0, "w": 3, "h": 3, "level": 1, "count": 1}])";

// Walks the hero into the corner zone and waits for the monster's wind-up.
void provoke(World& w) {
    run(w, Dir::Right, 17); // (1,1) -> (4,1): moves start at ticks 0, 8 and 16
    until(w, [&] { return windingUp(w); });
    ASSERT_TRUE(windingUp(w));
}

} // namespace

TEST(Field, ZoneLevelSetsTheMonstersStats) {
    const WorldDef def = fieldWorld(
        R"([{"name": "Deep", "x": 6, "y": 6, "w": 6, "h": 6, "level": 5, "count": 3}])");
    GameState state = freshState();
    World w(def, state, shippedContent());
    ASSERT_EQ(w.monsters().size(), 3U);
    for (const Monster& m : w.monsters()) {
        EXPECT_EQ(m.stats.name, "Fox");
        EXPECT_EQ(m.stats.maxHp, 500);
        EXPECT_EQ(m.hp, 500);
        EXPECT_EQ(m.stats.expReward, 500);
        EXPECT_TRUE(m.stats.atk >= 150 && m.stats.atk <= 190 && m.stats.atk % 10 == 0);
        EXPECT_TRUE(m.stats.def >= 25 && m.stats.def <= 45 && m.stats.def % 5 == 0);
        EXPECT_GE(std::max(std::abs(m.at.x - 1), std::abs(m.at.y - 1)), 5); // respawnDistance
    }
}

TEST(Field, SteppingAwayDuringAWindUpDodgesTheHit) {
    const WorldDef def = fieldWorld(kCornerZone);
    GameState state = freshState();
    World w(def, state, shippedContent());
    provoke(w);
    EXPECT_EQ(w.monsters()[0].target, w.hero());
    const int hp = state.hero.hp;
    auto events = run(w, Dir::Left, 9); // out of the targeted tile (once the last move is done)
    ASSERT_NE(w.hero(), w.monsters()[0].target);
    const auto rest = until(w, [&] { return w.monsters()[0].mode == Monster::Mode::Cooldown; });
    events.insert(events.end(), rest.begin(), rest.end());
    EXPECT_EQ(w.monsters()[0].mode, Monster::Mode::Cooldown);
    EXPECT_EQ(state.hero.hp, hp);
    EXPECT_TRUE(std::ranges::none_of(
        events, [](const WorldEvent& e) { return std::holds_alternative<wev::Damaged>(e); }));
}

TEST(Field, StandingStillTakesTheHit) {
    const WorldDef def = fieldWorld(kCornerZone);
    GameState state = freshState();
    World w(def, state, shippedContent());
    provoke(w);
    const int hp = state.hero.hp;
    const int expected = defendedDamage(w.monsters()[0].stats.atk, heroDef(state));
    const auto events = until(w, [&] { return !windingUp(w); });
    EXPECT_EQ(state.hero.hp, hp - expected);
    EXPECT_NE(std::ranges::find(events, WorldEvent{wev::Damaged{w.hero(), expected, true}}),
              events.end());
}

TEST(Field, AChasingMonsterWalksAroundATree) {
    // A tree wall at x = 3 from y = 0 to 9; the way round is through rows 10-11.
    std::string map = openMap();
    for (int y = 0; y < 10; ++y)
        map = withTile(map, 3, y, '#');
    const WorldDef def = fieldWorld(
        R"([{"name": "All", "x": 0, "y": 0, "w": 12, "h": 12, "level": 1, "count": 1}])", map,
        withField(kField, "noticeRadius", 10));
    GameState state = freshState();
    World w(def, state, shippedContent());
    ASSERT_EQ(w.monsters().size(), 1U);
    ASSERT_GT(w.monsters()[0].at.x, 3); // this seed starts it beyond the wall
    bool wentRound = false;
    until(w, [&] {
        const Tile at = w.monsters()[0].at;
        wentRound = wentRound || (at.x == 3 && at.y >= 10);
        return windingUp(w);
    });
    EXPECT_TRUE(windingUp(w));
    EXPECT_TRUE(wentRound); // through the gap in rows 10-11
}

TEST(Field, AChaseGivesUpWhenTheHeroLeavesTheZone) {
    const WorldDef def = fieldWorld(
        R"([{"name": "East", "x": 4, "y": 0, "w": 8, "h": 12, "level": 1, "count": 1}])",
        openMap(), withField(kField, "noticeRadius", 10));
    GameState state = freshState();
    World w(def, state, shippedContent());
    run(w, Dir::Right, 17); // into the zone at (4, 1)
    until(w, [&] { return w.monsters()[0].mode == Monster::Mode::Chase; });
    ASSERT_EQ(w.monsters()[0].mode, Monster::Mode::Chase);
    run(w, Dir::Left, 9); // (3, 1): outside
    ASSERT_EQ(w.hero(), (Tile{3, 1}));
    for (int i = 0; i < 600; ++i) {
        w.step({});
        ASSERT_GE(w.monsters()[0].at.x, 4); // monsters stay in their zone
        if (w.monsters()[0].mode == Monster::Mode::Chase)
            FAIL() << "still chasing at tick " << i;
    }
}

TEST(Field, TheHeroCannotWalkThroughAMonster) {
    const WorldDef def = fieldWorld(kCornerZone);
    GameState state = freshState();
    World w(def, state, shippedContent());
    provoke(w);
    const Tile monster = w.monsters()[0].at;
    const Tile before = w.hero();
    const Dir toward = monster.x > before.x   ? Dir::Right
                       : monster.x < before.x ? Dir::Left
                       : monster.y > before.y ? Dir::Down
                                              : Dir::Up;
    run(w, toward, 1);
    EXPECT_EQ(w.hero(), before);
}

TEST(Field, LifeAndFervorRegenerate) {
    const WorldDef def = fieldWorld("[]");
    GameState state = freshState("hunter"); // Life 10
    state.hero.hp = 100;
    state.hero.mp = 10;
    World w(def, state, shippedContent());
    const int life = state.hero.life + equipmentBonus(state.hero, shippedContent()).life;
    ASSERT_GT(life, 0);
    run(w, std::nullopt, 120); // 2 s: one Life tick, four Fervor ticks
    EXPECT_EQ(state.hero.hp, 100 + life);
    EXPECT_EQ(state.hero.mp, 14);
    state.hero.hp = state.hero.maxHp;
    state.hero.mp = state.hero.maxMp;
    run(w, std::nullopt, 120);
    EXPECT_EQ(state.hero.hp, state.hero.maxHp); // never above the maximum
    EXPECT_EQ(state.hero.mp, state.hero.maxMp);
}

TEST(Field, FallingCostsTheHittersExpAndStopsTheWorld) {
    const WorldDef def = fieldWorld(kCornerZone);
    for (const std::int64_t exp : {1000, 30}) {
        GameState state = freshState();
        state.hero.exp = exp;
        World w(def, state, shippedContent());
        provoke(w);
        state.hero.hp = 1;
        const auto reward = w.monsters()[0].stats.expReward; // 100 at level 1
        const auto weapon = state.hero.weapon;
        const auto events = until(w, [&] { return w.fallen(); });
        ASSERT_TRUE(w.fallen());
        EXPECT_EQ(events.back(), WorldEvent{wev::HeroFell{}});
        EXPECT_EQ(state.hero.hp, 0);
        EXPECT_EQ(state.hero.exp, std::max<std::int64_t>(0, exp - reward));
        EXPECT_EQ(state.hero.weapon, weapon); // equipment is kept
        EXPECT_TRUE(w.step({Dir::Left}).empty());
        EXPECT_FALSE(canAdventure(state.hero));
    }
}

TEST(Field, ZonesRefillAwayFromTheHero) {
    // The whole 3x3 zone is within 5 tiles of spawn, so nothing can appear at first.
    const WorldDef def = fieldWorld(
        R"([{"name": "Near", "x": 2, "y": 0, "w": 3, "h": 3, "level": 1, "count": 1}])");
    GameState state = freshState();
    World w(def, state, shippedContent());
    EXPECT_TRUE(w.monsters().empty());
    run(w, std::nullopt, 600); // standing over the zone: it waits
    EXPECT_TRUE(w.monsters().empty());
    run(w, Dir::Down, 90); // to (1, 11): far enough from row 0-2? (distance 9 from row 2)
    ASSERT_EQ(w.hero(), (Tile{1, 11}));
    run(w, std::nullopt, 61); // the 1 s respawn delay
    ASSERT_EQ(w.monsters().size(), 1U);
    const Tile at = w.monsters()[0].at;
    EXPECT_GE(std::max(std::abs(at.x - 1), std::abs(at.y - 11)), 5);
}

TEST(Field, ReplayWithMonstersIsDeterministic) {
    const WorldDef def = fieldWorld(
        R"([{"name": "All", "x": 0, "y": 0, "w": 12, "h": 12, "level": 3, "count": 4}])");
    Rng script = Rng::stream(9, 0);
    std::vector<std::optional<Dir>> inputs;
    for (int i = 0; i < 1200; ++i) {
        const auto pick = script.below(6);
        inputs.push_back(pick >= 4 ? std::nullopt : std::optional(static_cast<Dir>(pick)));
    }
    const auto play = [&](GameState& state) {
        World w(def, state, shippedContent());
        std::vector<WorldEvent> events;
        for (const auto& in : inputs) {
            auto step = w.step({in});
            events.insert(events.end(), step.begin(), step.end());
        }
        return std::tuple(events, w.hero(), w.monsters());
    };
    GameState a = freshState(), b = freshState();
    EXPECT_EQ(play(a), play(b));
    EXPECT_EQ(a, b);
}
