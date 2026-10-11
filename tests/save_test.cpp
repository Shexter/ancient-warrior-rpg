#include "game/save.hpp"

#include "state_helpers.hpp"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <fstream>
#include <functional>
#include <sstream>

using namespace heroes::game;
namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace {

fs::path scratchDir(const std::string& name) {
    const fs::path dir = fs::temp_directory_path() / "heroes_save_test" / name;
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir;
}

std::string slurp(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void spit(const fs::path& file, const std::string& body) {
    std::ofstream(file, std::ios::binary | std::ios::trunc) << body;
}

// A state that touches every persisted field, including a bag with a gap and a used RNG.
GameState richState() {
    GameState s = freshState("hunter", 0xDEADBEEFCAFEF00DULL);
    Hero& h = s.hero;
    h.name = "Aria B";
    h.level = 12;
    h.expToNext = 345;
    h.exp = 6789;
    h.gold = 123456789012LL;
    h.hp = 77;
    h.mp = 33;
    h.maxHp = 260;
    h.atk = 130;
    h.weapon = Equipped{"fire_claymore", 4};
    h.armor = Equipped{"diamond_plate", 2};
    h.forgeLevel = 6;
    EXPECT_TRUE(h.bag.add("topaz", 3));
    EXPECT_TRUE(h.bag.add("health_potion", 5));
    EXPECT_TRUE(h.bag.add("fire_dust", 2));
    EXPECT_TRUE(h.bag.remove("health_potion", 5)); // leaves a gap in the middle
    h.bank = Inventory{};
    EXPECT_TRUE(h.bank->add("gold_leaf", 2));
    EXPECT_TRUE(h.bank->add("black_pearl", 1));
    h.learnedSkills = {"angel_wings", "meteor_fist"};
    h.completedMissions = {"the_gorgon"};
    for (int i = 0; i < 37; ++i)
        s.rng.next();
    return s;
}

} // namespace

TEST(Save, RoundTripRestoresEverything) {
    const fs::path file = scratchDir("roundtrip") / "save.json";
    const GameState original = richState();
    ASSERT_TRUE(saveGame(original, file).has_value());
    const auto loaded = loadGame(file, shippedContent());
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(*loaded, original);
    Rng a = original.rng;
    Rng b = loaded->rng;
    EXPECT_EQ(a.next(), b.next());
}

TEST(Save, FreshGameWithNoBankAndNoGearRoundTrips) {
    const fs::path file = scratchDir("fresh") / "save.json";
    GameState s = freshState();
    s.hero.weapon.reset();
    s.hero.armor.reset();
    ASSERT_TRUE(saveGame(s, file).has_value());
    EXPECT_EQ(*loadGame(file, shippedContent()), s);
}

TEST(Save, SecondSaveReplacesTheFirst) {
    const fs::path file = scratchDir("overwrite") / "save.json";
    GameState first = richState();
    ASSERT_TRUE(saveGame(first, file).has_value());
    GameState second = first;
    second.hero.gold += 1;
    ASSERT_TRUE(saveGame(second, file).has_value());
    EXPECT_EQ(*loadGame(file, shippedContent()), second);
    EXPECT_FALSE(fs::exists(fs::path(file) += ".tmp"));
}

TEST(Save, CreatesMissingDirectory) {
    const fs::path file = scratchDir("mkdir") / "a" / "b" / "save.json";
    ASSERT_TRUE(saveGame(freshState(), file).has_value());
    EXPECT_TRUE(fs::exists(file));
}

TEST(Save, FailedWriteLeavesPreviousSaveIntact) {
    const fs::path file = scratchDir("failwrite") / "save.json";
    const GameState good = richState();
    ASSERT_TRUE(saveGame(good, file).has_value());
    const std::string before = slurp(file);

    fs::create_directory(fs::path(file) += ".tmp"); // the temp path is unusable
    const auto result = saveGame(freshState(), file);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), SaveError::WriteFailed);
    EXPECT_EQ(slurp(file), before);
    EXPECT_EQ(*loadGame(file, shippedContent()), good);
}

TEST(Save, StaleTempFileFromACrashIsIgnored) {
    const fs::path file = scratchDir("stale") / "save.json";
    const GameState good = richState();
    ASSERT_TRUE(saveGame(good, file).has_value());
    spit(fs::path(file) += ".tmp", "{ half a sav");
    EXPECT_EQ(*loadGame(file, shippedContent()), good);
    ASSERT_TRUE(saveGame(good, file).has_value()); // and the next save overwrites it
}

TEST(Save, MissingFileIsNotFound) {
    const auto result = loadGame(scratchDir("missing") / "save.json", shippedContent());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), SaveError::NotFound);
}

TEST(Save, DirectoryInsteadOfFileIsUnreadable) {
    const fs::path dir = scratchDir("isdir");
    EXPECT_FALSE(loadGame(dir, shippedContent()).has_value());
}

namespace {

struct BadCase {
    const char* name;
    std::function<void(Json&)> mutate;
    SaveError expected;
};

class BadSave : public testing::TestWithParam<BadCase> {};

} // namespace

TEST_P(BadSave, IsRejectedAndFileUntouched) {
    const BadCase& c = GetParam();
    const fs::path file = scratchDir(std::string("bad_") + c.name) / "save.json";
    ASSERT_TRUE(saveGame(richState(), file).has_value());
    Json doc = Json::parse(slurp(file));
    c.mutate(doc);
    spit(file, doc.dump(2));
    const std::string before = slurp(file);

    const auto result = loadGame(file, shippedContent());
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), c.expected);
    EXPECT_EQ(slurp(file), before);
}

#define MUT(...) [](Json& j) { __VA_ARGS__; }
INSTANTIATE_TEST_SUITE_P(
    Cases, BadSave,
    testing::Values(
        BadCase{"version2", MUT(j["version"] = 2), SaveError::NewerVersion},
        BadCase{"version99_with_other_shape", MUT(j = Json{{"version", 99}, {"x", 1}}),
                SaveError::NewerVersion},
        BadCase{"version0", MUT(j["version"] = 0), SaveError::Corrupt},
        BadCase{"version_string", MUT(j["version"] = "1"), SaveError::Corrupt},
        BadCase{"no_version", MUT(j.erase("version")), SaveError::Corrupt},
        BadCase{"not_an_object", MUT(j = Json::array()), SaveError::Corrupt},
        BadCase{"missing_gold", MUT(j["hero"].erase("gold")), SaveError::Corrupt},
        BadCase{"missing_hero", MUT(j.erase("hero")), SaveError::Corrupt},
        BadCase{"missing_rng", MUT(j.erase("rng")), SaveError::Corrupt},
        BadCase{"gold_is_string", MUT(j["hero"]["gold"] = "lots"), SaveError::Corrupt},
        BadCase{"gold_is_float", MUT(j["hero"]["gold"] = 1.5), SaveError::Corrupt},
        BadCase{"negative_gold", MUT(j["hero"]["gold"] = -1), SaveError::Corrupt},
        BadCase{"hp_above_max", MUT(j["hero"]["hp"] = 9999), SaveError::Corrupt},
        BadCase{"level_zero", MUT(j["hero"]["level"] = 0), SaveError::Corrupt},
        BadCase{"forge_level_11", MUT(j["hero"]["forgeLevel"] = 11), SaveError::Corrupt},
        BadCase{"unknown_path", MUT(j["hero"]["pathId"] = "wizard"), SaveError::Corrupt},
        BadCase{"unknown_bag_item", MUT(j["hero"]["bag"][0][0] = "excalibur"), SaveError::Corrupt},
        BadCase{"unknown_bank_item", MUT(j["hero"]["bank"][0][0] = "excalibur"),
                SaveError::Corrupt},
        BadCase{"unknown_equipped", MUT(j["hero"]["weapon"]["itemId"] = "excalibur"),
                SaveError::Corrupt},
        BadCase{"armor_in_weapon_slot", MUT(j["hero"]["weapon"]["itemId"] = "steel_shield"),
                SaveError::Corrupt},
        BadCase{"upgrade_level_11", MUT(j["hero"]["weapon"]["upgradeLevel"] = 11),
                SaveError::Corrupt},
        BadCase{"unknown_skill", MUT(j["hero"]["learnedSkills"] = Json::array({"fireball"})),
                SaveError::Corrupt},
        BadCase{"unknown_mission", MUT(j["hero"]["completedMissions"] = Json::array({"nope"})),
                SaveError::Corrupt},
        BadCase{"zero_count", MUT(j["hero"]["bag"][0][1] = 0), SaveError::Corrupt},
        BadCase{"short_bag", MUT(j["hero"]["bag"].erase(j["hero"]["bag"].begin())),
                SaveError::Corrupt},
        BadCase{"duplicate_stack", MUT(j["hero"]["bag"][3] = Json::array({"topaz", 1})),
                SaveError::Corrupt},
        BadCase{"bad_seed_hex", MUT(j["seed"] = "0xZZZZZZZZZZZZZZZZ"), SaveError::Corrupt},
        BadCase{"short_seed_hex", MUT(j["seed"] = "0x2a"), SaveError::Corrupt},
        BadCase{"even_rng_inc", MUT(j["rng"]["inc"] = "0x0000000000000002"), SaveError::Corrupt},
        BadCase{"empty_name", MUT(j["hero"]["name"] = ""), SaveError::Corrupt},
        BadCase{"long_name", MUT(j["hero"]["name"] = "ninechars"), SaveError::Corrupt}),
    [](const testing::TestParamInfo<BadCase>& param) { return std::string(param.param.name); });
#undef MUT

TEST(Save, GarbageFilesAreCorruptAndUntouched) {
    const fs::path dir = scratchDir("garbage");
    for (const std::string& body :
         {std::string(), std::string("not json"), std::string("{\"version\": 1"),
          std::string("\xff\xfe\x00\x01", 4), std::string(2 << 20, '[')}) {
        const fs::path file = dir / "save.json";
        spit(file, body);
        const auto result = loadGame(file, shippedContent());
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), SaveError::Corrupt);
        EXPECT_EQ(slurp(file), body);
    }
}

TEST(Save, TruncatedValidSaveIsCorruptAtEveryLength) {
    const fs::path file = scratchDir("truncate") / "save.json";
    ASSERT_TRUE(saveGame(richState(), file).has_value());
    const std::string full = slurp(file);
    for (std::size_t n = 0; n < full.size(); n += 7) {
        spit(file, full.substr(0, n));
        const auto result = loadGame(file, shippedContent());
        ASSERT_FALSE(result.has_value()) << n;
        EXPECT_EQ(result.error(), SaveError::Corrupt) << n;
    }
}
