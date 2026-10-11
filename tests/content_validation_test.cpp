#include "game/content_loader.hpp"

#include <gtest/gtest.h>

#include <algorithm>

using namespace heroes::game;

namespace {

struct BadCase {
    const char* dir;
    const char* file;
    const char* entry;   // substring of ContentError::entry
    const char* message; // substring of ContentError::message
};

bool hasError(const std::vector<ContentError>& errors, const BadCase& c) {
    return std::ranges::any_of(errors, [&](const ContentError& e) {
        return e.file == c.file && e.entry.contains(c.entry) && e.message.contains(c.message);
    });
}

std::filesystem::path fixture(const char* dir) {
    return std::filesystem::path(HEROES_SOURCE_DIR) / "tests/fixtures/content_bad" / dir;
}

class ContentValidation : public testing::TestWithParam<BadCase> {};

} // namespace

TEST_P(ContentValidation, ReportsExpectedError) {
    const BadCase& c = GetParam();
    auto loaded = loadContent(fixture(c.dir));
    ASSERT_FALSE(loaded.has_value()) << c.dir << " unexpectedly loaded";
    EXPECT_TRUE(hasError(loaded.error(), c)) << [&] {
        std::string all;
        for (const auto& e : loaded.error())
            all += formatError(e) + "\n";
        return all;
    }();
}

INSTANTIATE_TEST_SUITE_P(
    Rules, ContentValidation,
    testing::Values(
        BadCase{"malformed_json", "items.json", "", "parse error"},
        BadCase{"unknown_key", "items.json", "iron_sword", "unknown key 'pricee'"},
        BadCase{"missing_key", "items.json", "steel_shield", "missing required key 'name'"},
        BadCase{"wrong_type", "items.json", "iron_sword", "'price' has the wrong type"},
        BadCase{"duplicate_id", "items.json", "topaz", "duplicate id"},
        BadCase{"bad_id_pattern", "recipes.json", "Forge Peace", "id must match"},
        BadCase{"negative_price", "items.json", "iron_sword", "'price' must be at least 0"},
        BadCase{"drop_rate_range", "items.json", "topaz", "'dropRate' must be between 0 and 100"},
        BadCase{"multiplier_zero", "paths.json", "hunter", "'multiplier' must be at least 1"},
        BadCase{"mp_cost_negative", "skills.json", "angel_wings", "'mpCost' must be at least 0"},
        BadCase{"status_turns_zero", "skills.json", "frozen_throne", "turns' must be at least 1"},
        BadCase{"unknown_upgrade_material", "items.json", "iron_sword",
                "unknown upgrade material 'no_such'"},
        BadCase{"unknown_key_item", "skills.json", "supernova", "unknown key item 'no_such'"},
        BadCase{"unknown_reward_item", "enemies.json", "medusa", "unknown reward item 'no_such'"},
        BadCase{"unknown_mission_enemy", "missions.json", "the_gorgon", "unknown enemy 'no_such'"},
        BadCase{"unknown_recipe_ingredient", "recipes.json", "forge_fire_claymore",
                "unknown ingredient 'no_such'"},
        BadCase{"unknown_recipe_result", "recipes.json", "forge_fire_claymore",
                "unknown result item 'no_such'"},
        BadCase{"missing_sprite", "enemies.json", "medusa", "sprite file 'nope.sprite' not found"},
        BadCase{"upgrade_material_on_potion", "items.json", "health_potion",
                "only for weapons and armor"},
        BadCase{"weapon_without_upgrade_material", "items.json", "iron_sword",
                "need 'upgradeMaterial'"},
        BadCase{"consumable_on_weapon", "items.json", "iron_sword", "only for consumables"},
        BadCase{"consumable_missing", "items.json", "mana_potion", "need 'consumable'"},
        BadCase{"missing_hunter_path", "paths.json", "", "missing required path 'hunter'"}),
    [](const testing::TestParamInfo<BadCase>& param) { return std::string(param.param.dir); });

TEST(ContentValidationMulti, TwoDefectsAreBothReported) {
    auto loaded = loadContent(fixture("two_defects"));
    ASSERT_FALSE(loaded.has_value());
    EXPECT_TRUE(
        hasError(loaded.error(), {"", "items.json", "iron_sword", "'price' must be at least 0"}));
    EXPECT_TRUE(hasError(loaded.error(), {"", "skills.json", "supernova", "unknown key item"}));
    EXPECT_EQ(loaded.error().size(), 2u);
}
