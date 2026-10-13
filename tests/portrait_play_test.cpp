// Plays the game on the phone layout (48x44) with taps only: no key is pressed in this file.
#include "app/art.hpp"

#include "frame_text.hpp"
#include "screen_harness.hpp"

#include <format>

using namespace screen_test;

namespace {

// A phone: draws the 48x44 frame the player sees and taps what is on it.
struct Phone {
    Harness& h;

    Framebuffer screen() const { return h.frame(engine::kPortraitCanvas); }

    // The cell where `text` starts, searching rows from the bottom up (taps aim at the menu
    // band, keypads and pad, below everything else). {-1, -1} if it isn't shown.
    static std::pair<int, int> locate(const Framebuffer& frame, std::string_view text) {
        for (int y = frame.height() - 1; y >= 0; --y) {
            const std::string row = rowText(frame, y);
            const auto at = row.find(text);
            if (at == std::string::npos)
                continue;
            int column = 0; // code points before the match (every glyph here is one cell wide)
            for (std::size_t i = 0; i < at; ++i)
                column += (static_cast<unsigned char>(row[i]) & 0xC0) != 0x80 ? 1 : 0;
            return {column, y};
        }
        return {-1, -1};
    }

    bool shows(std::string_view text) const { return locate(screen(), text).first >= 0; }

    // Taps `text` where it's drawn. False (and a test failure) if it isn't on screen.
    bool tap(std::string_view text) {
        // Every screen on the way must choose the phone layout on a 48x45 phone.
        EXPECT_EQ(h.app->canvasSize({48, 45}), engine::kPortraitCanvas) << "before tapping " << text;
        const auto [x, y] = locate(screen(), text);
        EXPECT_GE(x, 0) << "not on screen: " << text;
        return x >= 0 && h.press({{Key::Click, 0, x, y}});
    }
};

} // namespace

TEST(PhonePlay, NewGameWithTheLetterPad) {
    Harness h("phone_new_game");
    h.session.layout = Layout::Portrait;
    h.startAtTitle();
    Phone phone{h};
    ASSERT_TRUE(phone.tap("New Game"));
    ASSERT_TRUE(phone.tap("●")); // reveal the story
    ASSERT_TRUE(phone.tap("●")); // continue
    ASSERT_TRUE(phone.shows("Enter Hero name"));
    for (const char* letter : {"R", "U", "T", "H"})
        ASSERT_TRUE(phone.tap(letter));
    ASSERT_TRUE(phone.tap("⌫"));
    ASSERT_TRUE(phone.tap("H"));
    ASSERT_TRUE(phone.tap("OK"));
    ASSERT_TRUE(phone.tap("●"));
    ASSERT_TRUE(phone.tap("●"));
    ASSERT_TRUE(phone.tap("Knight"));
    ASSERT_TRUE(h.session.state.has_value());
    EXPECT_EQ(h.session.state->hero.name, "RUTH");
    EXPECT_TRUE(phone.shows("LEVEL"));
}

TEST(PhonePlay, BuyingWithTheNumberPadAndGoingBack) {
    Harness h("phone_buy");
    h.session.layout = Layout::Portrait;
    h.startAtHub(freshGame());
    Phone phone{h};
    ASSERT_TRUE(phone.tap("Town"));
    ASSERT_TRUE(phone.tap("Almonry"));
    ASSERT_TRUE(phone.tap("Holy Water"));
    ASSERT_TRUE(phone.shows("How many?"));
    ASSERT_TRUE(phone.tap("2"));
    ASSERT_TRUE(phone.shows("Total 2000 gold"));
    ASSERT_TRUE(phone.tap("OK"));
    EXPECT_TRUE(phone.shows("You bought 2 Holy Water"));
    ASSERT_TRUE(phone.tap("●")); // any tap closes a message
    EXPECT_EQ(h.session.state->hero.bag.count("health_potion"), 2);

    ASSERT_TRUE(phone.tap("▶")); // the other list: the bag
    EXPECT_TRUE(phone.shows("◀ Your bag ▶"));
    ASSERT_TRUE(phone.tap("✕"));

    // The other town screens, in and out.
    ASSERT_TRUE(phone.tap("Forge"));
    ASSERT_TRUE(phone.tap("Forge")); // its Forge row lists the recipes
    EXPECT_TRUE(phone.shows("Ingredients"));
    ASSERT_TRUE(phone.tap("✕"));
    ASSERT_TRUE(phone.tap("✕"));
    ASSERT_TRUE(phone.tap("Prior"));
    ASSERT_TRUE(phone.tap("Learn Skill"));
    EXPECT_TRUE(phone.shows("Guardian Angel"));
    ASSERT_TRUE(phone.tap("✕"));
    ASSERT_TRUE(phone.tap("✕"));
    ASSERT_TRUE(phone.tap("Thesaurus"));
    ASSERT_TRUE(phone.tap("[Y]es")); // open an account
    ASSERT_TRUE(phone.tap("●"));
    EXPECT_TRUE(h.session.state->hero.bank.has_value());
    ASSERT_TRUE(phone.tap("▶")); // the bag list
    ASSERT_TRUE(phone.tap("Holy Water")); // deposit from it...
    EXPECT_TRUE(phone.shows("How many?"));
    ASSERT_TRUE(phone.tap("▲")); // ...one, set with the pad
    ASSERT_TRUE(phone.tap("OK"));
    EXPECT_EQ(h.session.state->hero.bank->count("health_potion"), 1);
    ASSERT_TRUE(phone.tap("✕"));
    ASSERT_TRUE(phone.tap("✕"));
    EXPECT_EQ(h.app->depth(), 1u); // back at the Hub
}

TEST(PhonePlay, TrainingRoundByRound) {
    Harness h("phone_training");
    h.session.layout = Layout::Portrait;
    h.startAtHub(freshGame());
    Phone phone{h};
    ASSERT_TRUE(phone.tap("Training"));
    ASSERT_TRUE(phone.shows("≈100%"));
    ASSERT_TRUE(phone.tap("1"));
    ASSERT_TRUE(phone.tap("OK"));
    ASSERT_TRUE(phone.tap("●")); // finish the round
    ASSERT_TRUE(phone.tap("Continue"));
    ASSERT_TRUE(phone.tap("●"));
    ASSERT_TRUE(phone.tap("Leave"));
    EXPECT_TRUE(phone.shows("CATACOMB RESULTS"));
    ASSERT_TRUE(phone.tap("●"));
    EXPECT_EQ(h.app->depth(), 1u);
    EXPECT_NE(h.session.lastMessage.find("Training: won 2 of 2 rounds"), std::string::npos)
        << h.session.lastMessage;
}

TEST(PhonePlay, ABossBattle) {
    Harness h("phone_battle");
    h.session.layout = Layout::Portrait;
    game::GameState state = freshGame();
    state.hero.level = 15;
    state.hero.atk = 100000; // the outcome isn't the point; tapping through is
    h.startAtHub(std::move(state));
    Phone phone{h};
    ASSERT_TRUE(phone.tap("Mission"));
    ASSERT_TRUE(phone.tap("Avaritia"));
    ASSERT_TRUE(phone.tap("●"));
    ASSERT_TRUE(phone.tap("●"));
    ASSERT_TRUE(phone.tap("[Y]es"));
    ASSERT_TRUE(phone.shows("AVARITIA"));
    for (int i = 0; i < 50 && !phone.shows("VICTORY"); ++i) {
        if (phone.shows("Attack"))
            ASSERT_TRUE(phone.tap("Attack"));
        else
            ASSERT_TRUE(phone.tap("●")); // skip the animation
    }
    ASSERT_TRUE(phone.shows("VICTORY"));
    ASSERT_TRUE(phone.tap("●"));
    EXPECT_EQ(h.app->depth(), 1u);
    EXPECT_TRUE(h.session.state->hero.completedMissions.contains("avaritia"));
}
