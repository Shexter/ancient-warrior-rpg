#pragma once

#include "app/screen.hpp"
#include "app/widgets.hpp"
#include "game/errors.hpp"
#include "game/town.hpp"

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace heroes::app {

// One dialog at a time over a town screen: a message, a yes/no question, a number or text
// prompt, or a choice. A callback runs after its dialog closes, so it may open the next one.
class Modal {
  public:
    using Then = std::function<void()>;

    void message(std::string_view text, Then then = {});
    void confirm(std::string_view text, Then onYes, Then onNo = {});
    // `preview` (optional) describes the typed value on the row below the input.
    void number(std::string prompt, std::int64_t min, std::int64_t max,
                std::function<void(std::int64_t)> onValue,
                std::function<std::string(std::int64_t)> preview = {});
    void text(std::vector<std::string> prompt, int maxLength,
              std::function<void(const std::string&)> onValue);
    void choose(std::string title, std::vector<Menu::Item> items, std::function<void(int)> onChoice);

    bool open() const { return !std::holds_alternative<std::monostate>(state_); }
    void tick(double dtSeconds);
    void handle(const engine::KeyEvent& key); // only while open()
    void draw(engine::Framebuffer& frame) const;

  private:
    struct Message {
        MessageBox box;
        Then then;
    };
    struct Confirm {
        ConfirmBox box;
        Then onYes, onNo;
    };
    struct Number {
        std::string prompt;
        NumberInput input;
        std::function<void(std::int64_t)> onValue;
        std::function<std::string(std::int64_t)> preview;
    };
    struct Text {
        std::vector<std::string> prompt;
        TextInput input;
        std::function<void(const std::string&)> onValue;
    };
    struct Choice {
        std::string title;
        Menu menu;
        std::function<void(int)> onChoice;
    };
    std::variant<std::monostate, Message, Confirm, Number, Text, Choice> state_;
};

// Shared frame of the NPC screens: status panel, title on row 5, body on rows 6-21, hint on
// row 22, and the modal on top. Keys go to the modal while one is open.
class TownScreen : public Screen {
  public:
    Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) final;
    void draw(engine::Framebuffer& frame) const final;

  protected:
    TownScreen(Session& session, std::string title);

    virtual Transition onKey(const engine::KeyEvent& key) = 0;
    virtual void onTick(double /*dtSeconds*/) {}
    // Rebuilds rows that depend on the game state; runs before and after each step's keys.
    virtual void refresh() {}
    virtual void drawBody(engine::Framebuffer& frame) const = 0;
    virtual std::string_view hint() const { return "Enter choose · Esc back"; }

    // A town action's result: success marks the game unsaved; failure shows why.
    template <class T>
    bool succeeded(const std::expected<T, game::TownError>& result, Modal::Then onFailure = {}) {
        if (!result) {
            modal_.message(game::describe(result.error()), std::move(onFailure));
            return false;
        }
        session_.dirty = true;
        return true;
    }

    game::GameState& state() { return *session_.state; }
    const game::Hero& hero() const { return session_.state->hero; }
    const game::Content& content() const { return session_.content; }

    Session& session_;
    Modal modal_;
    bool leave_ = false; // set by a modal callback to return to the parent screen

  private:
    std::string title_;
};

// The 10 slots of a bag or bank as menu rows: "Topaz x3", or "(empty)" disabled.
std::vector<Menu::Item> slotItems(const game::Inventory& inventory, const game::Content& content);
// The item id in slot `index`, if any.
std::optional<std::string> slotItem(const game::Inventory& inventory, int index);

class TownMenuScreen final : public TownScreen {
  public:
    explicit TownMenuScreen(Session& session);

  private:
    Transition onKey(const engine::KeyEvent& key) override;
    void drawBody(engine::Framebuffer& frame) const override;

    Menu menu_;
};

// "For sale" | "Your bag", Tab switches lists. Buy, sell, and auction with animated bids.
class MerchantScreen final : public TownScreen {
  public:
    explicit MerchantScreen(Session& session);

  private:
    Transition onKey(const engine::KeyEvent& key) override;
    void onTick(double dtSeconds) override;
    void refresh() override;
    void drawBody(engine::Framebuffer& frame) const override;
    std::string_view hint() const override;
    void chooseBagItem(const std::string& id);
    void startAuction(const std::string& id);
    void finishAuction();

    struct Auction {
        std::string itemName;
        std::vector<std::int64_t> bids; // accepted prices, opening bid first
        std::int64_t payout;
        std::size_t shown = 1;
        double clock = 0;
    };

    std::vector<const game::ItemDef*> forSale_;
    Menu shop_;
    Menu bag_;
    bool bagFocused_ = false;
    std::optional<Auction> auction_;
};

// Change Equipment, Upgrade (cost lines in green or red), Forge.
class BlacksmithScreen final : public TownScreen {
  public:
    explicit BlacksmithScreen(Session& session);

  private:
    enum class Mode { Main, Upgrade, Forge };

    Transition onKey(const engine::KeyEvent& key) override;
    void drawBody(engine::Framebuffer& frame) const override;
    std::string_view hint() const override;
    void changeEquipment(game::EquipSlot slot);
    void confirmUpgrade();
    void confirmForge(const game::RecipeDef& recipe);
    void drawUpgrade(engine::Framebuffer& frame) const;
    void drawForge(engine::Framebuffer& frame) const;

    Mode mode_ = Mode::Main;
    Menu menu_;
    Menu recipes_;
    game::EquipSlot slot_ = game::EquipSlot::Weapon;
};

// Revives a fallen hero on entry. Restoration, Build Charisma, Learn Skill, Change Name.
class GuildmasterScreen final : public TownScreen {
  public:
    explicit GuildmasterScreen(Session& session);

  private:
    Transition onKey(const engine::KeyEvent& key) override;
    void refresh() override;
    void drawBody(engine::Framebuffer& frame) const override;
    std::string_view hint() const override;
    void restoration();
    void charisma(game::CharismaStat stat, std::string statName);

    bool learning_ = false; // the Learn Skill list is showing
    Menu menu_;
    Menu skills_;
};

// Opens an account on entry if there is none, then "Bank" | "Your bag" with Tab and Enter.
class BankScreen final : public TownScreen {
  public:
    explicit BankScreen(Session& session);

  private:
    Transition onKey(const engine::KeyEvent& key) override;
    void refresh() override;
    void drawBody(engine::Framebuffer& frame) const override;
    std::string_view hint() const override;

    Menu bank_;
    Menu bag_;
    bool bagFocused_ = false;
};

} // namespace heroes::app
