#include "app/screens/town.hpp"
#include "game/town.hpp"

#include "frame_text.hpp"
#include "screen_harness.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <format>
#include <string_view>

using namespace screen_test;

namespace {

const KeyEvent kTab{Key::Tab};

// Hub -> Town -> the NPC at `npc` ('1' Merchant, '2' Blacksmith, '3' Guildmaster, '4' Bank).
void enterNpc(Harness& h, game::GameState state, char npc) {
    h.startAtHub(std::move(state));
    ASSERT_TRUE(h.press({kHubTown}));
    ASSERT_TRUE(frameContains(h.frame(), "CIVITAS LUCIS"));
    ASSERT_TRUE(h.press({ch(npc)}));
}

game::GameState withItem(std::string_view id, int qty) {
    game::GameState state = freshGame();
    EXPECT_TRUE(state.hero.bag.add(id, qty).has_value());
    return state;
}

} // namespace

TEST(Town, EscReturnsFromTheTownToTheHub) {
    Harness h("town_esc");
    enterNpc(h, freshGame(), '1');
    EXPECT_TRUE(frameContains(h.frame(), "ALMONRY"));
    ASSERT_TRUE(h.press({kEsc}));
    EXPECT_TRUE(frameContains(h.frame(), "Prior"));
    ASSERT_TRUE(h.press({kEsc}));
    EXPECT_EQ(h.app->depth(), 1u);
    EXPECT_TRUE(frameContains(h.frame(), "▶ Town")); // the Hub keeps its selection
}

TEST(Merchant, BuysAHealthPotion) {
    Harness h("merchant_buy");
    enterNpc(h, freshGame(), '1');
    game::GameState expected = *h.session.state;
    ASSERT_TRUE(game::buy(expected, game::shippedContent(), "health_potion", 2).has_value());

    // For sale: Iron Sword, Fire Claymore, Steel Shield, Diamond Plate, Peace Maker, Health Potion
    ASSERT_TRUE(h.press({ch('6')}));
    EXPECT_TRUE(frameContains(h.frame(), "How many? (1–99)"));
    ASSERT_TRUE(h.press({ch('2')}));
    EXPECT_TRUE(frameContains(h.frame(), "Total 2000 gold"));
    ASSERT_TRUE(h.press({kEnter}));
    EXPECT_TRUE(frameContains(h.frame(), "You bought 2 Holy Water for 2000 gold."));
    EXPECT_EQ(*h.session.state, expected);
    EXPECT_TRUE(h.session.dirty);
    ASSERT_TRUE(h.press({kEnter}));
    EXPECT_TRUE(frameContains(h.frame(), "Holy Water x2"));
}

TEST(Merchant, RefusedPurchaseExplainsAndChangesNothing) {
    Harness h("merchant_refused");
    enterNpc(h, freshGame(), '1');
    const game::GameState before = *h.session.state;
    ASSERT_TRUE(h.press({ch('5'), ch('2'), kEnter})); // 2 Peace Makers: 40000 gold
    EXPECT_TRUE(frameContains(h.frame(), "You don't have enough gold."));
    EXPECT_EQ(*h.session.state, before);
    EXPECT_FALSE(h.session.dirty);
}

TEST(Merchant, AuctionPlaysOneBidEvery600Ms) {
    // Find a seed whose first auction sells after at least one raise.
    auto withTopaz = [](std::uint64_t seed) {
        game::GameState state = game::newGame(game::shippedContent(), "Aria", "fighter", seed);
        EXPECT_TRUE(state.hero.bag.add("topaz", 1).has_value());
        return state;
    };
    std::uint64_t seed = 1;
    game::AuctionResult roll{};
    for (;; ++seed) {
        game::GameState copy = withTopaz(seed);
        roll = game::auction(copy, game::shippedContent(), "topaz").value();
        if (roll.sold && roll.bids >= 1)
            break;
    }
    const game::GameState start = withTopaz(seed);
    game::GameState expected = start;
    (void)game::auction(expected, game::shippedContent(), "topaz");

    Harness h("merchant_auction");
    enterNpc(h, start, '1');
    ASSERT_TRUE(h.press({kTab, ch('1')}));
    EXPECT_TRUE(frameContains(h.frame(), "bids from 500 gold"));
    ASSERT_TRUE(h.press({ch('2')})); // Auction: the result is settled at once
    EXPECT_EQ(*h.session.state, expected);
    EXPECT_TRUE(frameContains(h.frame(), "Bid accepted at 500…"));
    EXPECT_FALSE(frameContains(h.frame(), "Bid accepted at 550…"));
    ASSERT_TRUE(h.wait(0.6));
    EXPECT_TRUE(frameContains(h.frame(), "Bid accepted at 550…"));
    ASSERT_TRUE(h.wait(0.6 * roll.bids));
    EXPECT_TRUE(frameContains(
        h.frame(), std::format("Merchant fee is 10%. You receive {} gold.", roll.payout)));
}

TEST(Merchant, TurnsAwayAFallenHero) {
    Harness h("merchant_dead");
    game::GameState state = freshGame();
    state.hero.hp = 0;
    enterNpc(h, state, '1');
    EXPECT_TRUE(frameContains(h.frame(), "The Almonry serves the living. See the Prior first."));
    ASSERT_TRUE(h.press({kEnter}));
    EXPECT_TRUE(frameContains(h.frame(), "CIVITAS LUCIS"));
    EXPECT_EQ(h.app->depth(), 2u);
}

TEST(Bank, OpensAnAccountThenDepositsAndWithdraws) {
    Harness h("bank");
    enterNpc(h, withItem("topaz", 3), '4');
    EXPECT_TRUE(frameContains(h.frame(), "Open a place in the Thesaurus for 10000 gold?"));
    ASSERT_TRUE(h.press({ch('y'), kEnter}));
    EXPECT_TRUE(h.session.state->hero.bank.has_value());
    EXPECT_EQ(h.session.state->hero.gold, 10000);

    ASSERT_TRUE(h.press({kTab, ch('1'), ch('3'), kEnter})); // deposit 3 Topaz
    EXPECT_EQ(h.session.state->hero.bank->count("topaz"), 3);
    EXPECT_EQ(h.session.state->hero.bag.count("topaz"), 0);
    EXPECT_TRUE(frameContains(h.frame(), "Rosary x3"));

    ASSERT_TRUE(h.press({kTab, ch('1'), ch('3'), kEnter})); // and back
    EXPECT_EQ(h.session.state->hero.bank->count("topaz"), 0);
    EXPECT_EQ(h.session.state->hero.bag.count("topaz"), 3);
}

TEST(Bank, DecliningTheAccountLeavesEverythingAsItWas) {
    Harness h("bank_decline");
    enterNpc(h, freshGame(), '4');
    const game::GameState before = *h.session.state;
    ASSERT_TRUE(h.press({ch('n')}));
    EXPECT_TRUE(frameContains(h.frame(), "CIVITAS LUCIS"));
    EXPECT_EQ(*h.session.state, before);
}

TEST(Blacksmith, UnaffordableUpgradeShowsRedCostLines) {
    Harness h("smith_upgrade");
    enterNpc(h, freshGame(), '2');
    ASSERT_TRUE(h.press({ch('2'), ch('1')})); // Upgrade, Weapon
    const Framebuffer frame = h.frame();
    EXPECT_TRUE(frameContains(frame, "Sword  Lv 1 → Lv 2"));
    EXPECT_NE(rowText(frame, 10).find("Altar Ember x1   (you have 0)"), std::string::npos);
    EXPECT_EQ(frame.at(6, 10).fg, palette::kDmg);  // no Fire Dust
    EXPECT_EQ(frame.at(6, 11).fg, palette::kDmg);  // 0 EXP of 1000
    EXPECT_EQ(frame.at(6, 12).fg, palette::kHeal); // 20000 gold of 10000

    const game::GameState before = *h.session.state;
    ASSERT_TRUE(h.press({kEnter, ch('y')}));
    EXPECT_TRUE(frameContains(h.frame(), "You are missing the upgrade materials."));
    EXPECT_EQ(*h.session.state, before);
}

TEST(Blacksmith, AffordableUpgradeRaisesTheLevel) {
    Harness h("smith_upgrade_ok");
    game::GameState state = withItem("fire_dust", 1);
    state.hero.exp = 1000;
    enterNpc(h, state, '2');
    ASSERT_TRUE(h.press({ch('2'), ch('1'), kEnter, ch('y')}));
    EXPECT_TRUE(frameContains(h.frame(), "Your Sword is now Lv 2."));
    EXPECT_EQ(h.session.state->hero.weapon->upgradeLevel, 2);
    EXPECT_EQ(h.session.state->hero.gold, 10000);
}

TEST(Blacksmith, SwappingAnUpgradedWeaponWarnsFirst) {
    Harness h("smith_swap");
    game::GameState state = withItem("fire_claymore", 1);
    state.hero.weapon->upgradeLevel = 3;
    enterNpc(h, state, '2');
    ASSERT_TRUE(h.press({ch('1'), ch('1'), ch('1')})); // Change, Weapon, Fire Claymore
    EXPECT_TRUE(frameContains(h.frame(), "Your upgraded weapon will return to Lv 1."));
    ASSERT_TRUE(h.press({ch('y')}));
    EXPECT_TRUE(frameContains(h.frame(), "You equip the Flaming Sword."));
    EXPECT_EQ(h.session.state->hero.weapon, (game::Equipped{"fire_claymore", 1}));
    EXPECT_EQ(h.session.state->hero.bag.count("iron_sword"), 1);
}

TEST(Blacksmith, ForgeShowsIngredientsAndMatchesTheGameResult) {
    Harness h("smith_forge");
    game::GameState state = withItem("iron_sword", 1);
    ASSERT_TRUE(state.hero.bag.add("phoenix_feather", 1).has_value());
    ASSERT_TRUE(state.hero.bag.add("fire_dust", 1).has_value());
    game::GameState expected = state;
    const auto forged = game::forge(expected, game::shippedContent(), "forge_fire_claymore");
    ASSERT_TRUE(forged.has_value());

    enterNpc(h, state, '2');
    ASSERT_TRUE(h.press({ch('3')}));
    EXPECT_TRUE(frameContains(h.frame(), "Phoenix Feather  1/1"));
    EXPECT_TRUE(frameContains(h.frame(), "Success chance 90%"));
    ASSERT_TRUE(h.press({kEnter}));
    EXPECT_TRUE(frameContains(h.frame(), "Success chance 90%."));
    ASSERT_TRUE(h.press({ch('y')}));
    EXPECT_EQ(*h.session.state, expected);
    EXPECT_TRUE(frameContains(h.frame(), forged->success
                                             ? "Forging SUCCESS!!! You now possess Flaming Sword"
                                             : "Forging FAILED, the materials are lost..."));
}

TEST(Guildmaster, RevivesAFallenHeroOnEntry) {
    Harness h("guild_revive");
    game::GameState state = freshGame();
    state.hero.hp = 0;
    state.hero.gold = 40;
    enterNpc(h, state, '3');
    EXPECT_TRUE(frameContains(h.frame(), "Rise. Mercy asks only that you be more careful."));
    EXPECT_EQ(h.session.state->hero.hp, 100);
    EXPECT_EQ(h.session.state->hero.gold, 0);
    EXPECT_TRUE(h.session.dirty);
}

TEST(Guildmaster, CharismaPreviewsTheGainAndThenBuysIt) {
    Harness h("guild_charisma");
    game::GameState state = freshGame();
    state.hero.level = 10;
    state.hero.exp = 5000;
    const auto quote = game::charismaQuote(state.hero, game::shippedContent(), 500);
    game::GameState expected = state;
    ASSERT_TRUE(game::buyCharisma(expected, game::shippedContent(), game::CharismaStat::MaxHp, 500)
                    .has_value());

    enterNpc(h, state, '3');
    ASSERT_TRUE(h.press({ch('3'), ch('1')})); // Build Charisma, Max HP
    ASSERT_TRUE(h.type("500"));
    EXPECT_TRUE(frameContains(
        h.frame(), std::format("→ +{} Max HP for {} gold", quote.gain, quote.goldCost)));
    ASSERT_TRUE(h.press({kEnter, ch('y')}));
    EXPECT_EQ(*h.session.state, expected);
}

TEST(Guildmaster, CharismaIsDisabledBelowLevel10) {
    Harness h("guild_charisma_locked");
    enterNpc(h, freshGame(), '3');
    EXPECT_TRUE(frameContains(h.frame(), "Requires Lv 10"));
    ASSERT_TRUE(h.press({ch('3')}));
    EXPECT_FALSE(frameContains(h.frame(), "Raise which stat?"));
}

TEST(Guildmaster, LearnSkillColorsEachRequirement) {
    Harness h("guild_learn");
    game::GameState state = withItem("topaz", 1);
    state.hero.level = 5;
    enterNpc(h, state, '3');
    ASSERT_TRUE(h.press({ch('4')}));
    const Framebuffer frame = h.frame();
    // Requirement columns follow the longest skill name ("Let There Be Light", 18): level at
    // 6 + 18 + 2 = 26, key item at 26 + 8 + 14 = 48.
    EXPECT_EQ(frame.at(26, 7).fg, palette::kHeal); // Guardian Angel: Lv 5 met
    EXPECT_EQ(frame.at(48, 7).fg, palette::kHeal); // has a Rosary
    EXPECT_EQ(frame.at(48, 8).fg, palette::kDmg);  // Righteous Strike: no Martyr's Relic
    EXPECT_EQ(frame.at(26, 9).fg, palette::kDmg);  // Shield of Faith: needs Lv 10
    EXPECT_NE(rowText(frame, 11).find("Let There Be Light  Lv 20"), std::string::npos);

    ASSERT_TRUE(h.press({ch('1')}));
    EXPECT_TRUE(frameContains(h.frame(), "You learned Guardian Angel!"));
    EXPECT_TRUE(h.session.state->hero.learnedSkills.contains("angel_wings"));
    ASSERT_TRUE(h.press({kEnter}));
    EXPECT_NE(rowText(h.frame(), 7).find("Learned"), std::string::npos);
}

TEST(Guildmaster, RestorationUsesAPotion) {
    Harness h("guild_restore");
    game::GameState state = withItem("health_potion", 1);
    state.hero.hp = 10;
    enterNpc(h, state, '3');
    ASSERT_TRUE(h.press({ch('1'), ch('1')}));
    EXPECT_EQ(h.session.state->hero.hp, std::min(110, state.hero.maxHp));
    EXPECT_EQ(h.session.state->hero.bag.count("health_potion"), 0);
}

TEST(Guildmaster, ChangeNameChargesTheFee) {
    Harness h("guild_rename");
    game::GameState state = freshGame();
    state.hero.gold = 60000;
    enterNpc(h, state, '3');
    ASSERT_TRUE(h.press({ch('5')}));
    EXPECT_TRUE(frameContains(h.frame(), "The fee is 50000 gold."));
    ASSERT_TRUE(h.type("Roland"));
    ASSERT_TRUE(h.press({kEnter}));
    EXPECT_TRUE(frameContains(h.frame(), "From now on you are Roland."));
    EXPECT_EQ(h.session.state->hero.name, "Roland");
    EXPECT_EQ(h.session.state->hero.gold, 10000);
}

TEST(Reskin, SaveFromBeforeTheReskinShowsTheNewNames) {
    // Written by the game before the Act I re-skin (IDs only; names come from the data).
    const auto loaded = game::loadGame(fs::path(HEROES_SOURCE_DIR) / "tests" / "fixtures" /
                                           "pre_reskin_save.json",
                                       game::shippedContent());
    ASSERT_TRUE(loaded.has_value());
    Harness h("reskin_save");
    enterNpc(h, *loaded, '1');
    ASSERT_TRUE(h.press({kTab}));
    const Framebuffer frame = h.frame();
    EXPECT_TRUE(frameContains(frame, "Weapon Sword"));
    EXPECT_TRUE(frameContains(frame, "Holy Water x2"));
    EXPECT_TRUE(frameContains(frame, "Rosary"));
}

TEST(Reskin, LongestNamesStayInsideTheirBoxes) {
    Harness h("reskin_widths");
    game::GameState state = withItem("pure_ice", 1);
    state.hero.level = 20;
    enterNpc(h, state, '3');
    ASSERT_TRUE(h.press({ch('4')})); // Learn Skill
    EXPECT_NE(rowText(h.frame(), 11).find("Fragment of the Cross"), std::string::npos);
}

TEST(Guildmaster, BlessingsShowTheGainAndThePointsLeft) {
    Harness h("guild_blessings");
    game::GameState state = freshGame();
    state.hero.blessings = 2;
    game::GameState expected = state;
    ASSERT_TRUE(game::blessing(expected, game::CharismaStat::Atk).has_value());

    enterNpc(h, state, '3');
    EXPECT_TRUE(frameContains(h.frame(), "Blessings (2)"));
    ASSERT_TRUE(h.press({ch('2')}));
    EXPECT_TRUE(frameContains(h.frame(), "Spend a blessing (2 left)"));
    EXPECT_TRUE(frameContains(h.frame(), "Max HP +30"));
    EXPECT_TRUE(frameContains(h.frame(), "DEF +3"));
    ASSERT_TRUE(h.press({ch('3')})); // ATK +6
    EXPECT_EQ(*h.session.state, expected);
    EXPECT_TRUE(frameContains(h.frame(), std::format("Your ATK rises to {}. 1 blessing left.",
                                                     expected.hero.atk)));
    ASSERT_TRUE(h.press({kEnter}));
    EXPECT_TRUE(frameContains(h.frame(), "Blessings (1)"));
}

TEST(Guildmaster, BlessingsAreDisabledWithoutPoints) {
    Harness h("guild_blessings_none");
    enterNpc(h, freshGame(), '3');
    EXPECT_TRUE(frameContains(h.frame(), "Level up to earn more"));
    ASSERT_TRUE(h.press({ch('2')}));
    EXPECT_FALSE(frameContains(h.frame(), "Spend a blessing"));
}
