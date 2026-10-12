#include "app/screens/town.hpp"

#include <algorithm>
#include <format>
#include <memory>
#include <utility>

namespace heroes::app {

namespace {

using engine::Framebuffer;
using engine::Key;
using engine::KeyEvent;

constexpr int kCanvasWidth = 80;
constexpr int kDialogWidth = 60;

// A centered panel `h` rows tall; returns its top row.
int centeredPanel(Framebuffer& frame, int h) {
    const int y = (24 - h) / 2;
    drawPanel(frame, (kCanvasWidth - kDialogWidth) / 2, y, kDialogWidth, h);
    return y;
}

constexpr int kDialogText = (kCanvasWidth - kDialogWidth) / 2 + 2; // first text column

} // namespace

// ---- Modal -------------------------------------------------------------------------------

void Modal::message(std::string_view text, Then then) {
    state_ = Message{MessageBox(text), std::move(then)};
}

void Modal::confirm(std::string_view text, Then onYes, Then onNo) {
    state_ = Confirm{ConfirmBox(text), std::move(onYes), std::move(onNo)};
}

void Modal::number(std::string prompt, std::int64_t min, std::int64_t max,
                   std::function<void(std::int64_t)> onValue,
                   std::function<std::string(std::int64_t)> preview) {
    state_ = Number{std::move(prompt), NumberInput(min, max), std::move(onValue),
                    std::move(preview)};
}

void Modal::text(std::vector<std::string> prompt, int maxLength,
                 std::function<void(const std::string&)> onValue) {
    state_ = Text{std::move(prompt), TextInput(maxLength), std::move(onValue)};
}

void Modal::choose(std::string title, std::vector<Menu::Item> items,
                   std::function<void(int)> onChoice) {
    state_ = Choice{std::move(title), Menu(std::move(items)), std::move(onChoice)};
}

void Modal::tick(double dtSeconds) {
    if (auto* number = std::get_if<Number>(&state_))
        number->input.tick(dtSeconds);
    else if (auto* text = std::get_if<Text>(&state_))
        text->input.tick(dtSeconds);
}

// Each branch closes the dialog before running its callback, which may open the next one.
void Modal::handle(const KeyEvent& key) {
    if (auto* m = std::get_if<Message>(&state_)) {
        if (!m->box.handle(key))
            return;
        Then then = std::move(m->then);
        state_ = std::monostate{};
        if (then)
            then();
    } else if (auto* c = std::get_if<Confirm>(&state_)) {
        const auto answer = c->box.handle(key);
        if (!answer)
            return;
        Then then = std::move(*answer ? c->onYes : c->onNo);
        state_ = std::monostate{};
        if (then)
            then();
    } else if (auto* n = std::get_if<Number>(&state_)) {
        const InputResult result = n->input.handle(key);
        if (result == InputResult::Editing)
            return;
        auto onValue = std::move(n->onValue);
        const auto value = n->input.value();
        state_ = std::monostate{};
        if (result == InputResult::Submitted)
            onValue(*value);
    } else if (auto* t = std::get_if<Text>(&state_)) {
        const InputResult result = t->input.handle(key);
        if (result == InputResult::Editing)
            return;
        auto onValue = std::move(t->onValue);
        const std::string value = t->input.value();
        state_ = std::monostate{};
        if (result == InputResult::Submitted)
            onValue(value);
    } else if (auto* ch = std::get_if<Choice>(&state_)) {
        if (key.key == Key::Escape) {
            state_ = std::monostate{};
            return;
        }
        const auto index = ch->menu.handle(key);
        if (!index)
            return;
        auto onChoice = std::move(ch->onChoice);
        state_ = std::monostate{};
        onChoice(*index);
    }
}

void Modal::draw(Framebuffer& frame) const {
    if (const auto* m = std::get_if<Message>(&state_)) {
        m->box.draw(frame);
    } else if (const auto* c = std::get_if<Confirm>(&state_)) {
        c->box.draw(frame);
    } else if (const auto* n = std::get_if<Number>(&state_)) {
        const int y = centeredPanel(frame, 7);
        drawText(frame, kDialogText, y + 1, n->prompt, palette::kText, kDialogWidth - 4);
        n->input.draw(frame, kDialogText, y + 3); // its range error goes on y + 4
        if (const auto value = n->input.value(); value && n->preview)
            drawText(frame, kDialogText, y + 5, n->preview(*value), palette::kAccent,
                     kDialogWidth - 4);
    } else if (const auto* t = std::get_if<Text>(&state_)) {
        const int rows = static_cast<int>(t->prompt.size());
        const int y = centeredPanel(frame, rows + 4);
        for (int i = 0; i < rows; ++i)
            drawText(frame, kDialogText, y + 1 + i, t->prompt[static_cast<std::size_t>(i)],
                     palette::kText, kDialogWidth - 4);
        t->input.draw(frame, kDialogText, y + rows + 2);
    } else if (const auto* ch = std::get_if<Choice>(&state_)) {
        const int y = centeredPanel(frame, ch->menu.size() + 5);
        drawText(frame, kDialogText, y + 1, ch->title, palette::kAccent, kDialogWidth - 4);
        ch->menu.draw(frame, kDialogText, y + 3);
        drawText(frame, kDialogText + kDialogWidth - 4 - 8, y + ch->menu.size() + 3, "Esc back",
                 palette::kDim);
    }
}

// ---- TownScreen --------------------------------------------------------------------------

TownScreen::TownScreen(Session& session, std::string title)
    : session_(session), title_(std::move(title)) {}

Transition TownScreen::update(std::span<const KeyEvent> keys, double dtSeconds) {
    refresh();
    modal_.tick(dtSeconds);
    onTick(dtSeconds);
    for (const KeyEvent& key : keys) {
        if (leave_)
            break;
        if (modal_.open()) {
            modal_.handle(key);
            continue;
        }
        if (Transition next = onKey(key); !std::holds_alternative<Stay>(next))
            return next;
    }
    refresh();
    if (leave_)
        return Pop{};
    return Stay{};
}

void TownScreen::draw(Framebuffer& frame) const {
    frame.clear(palette::kBg);
    drawStatusPanel(frame, hero(), content());
    drawText(frame, 2, 5, title_, palette::kAccent);
    drawBody(frame);
    drawCentered(frame, 22, hint(), palette::kDim);
    modal_.draw(frame);
}

std::vector<Menu::Item> slotItems(const game::Inventory& inventory, const game::Content& content) {
    std::vector<Menu::Item> rows;
    for (const game::Inventory::Slot& slot : inventory.slots()) {
        if (!slot) {
            rows.emplace_back("(empty)", false);
            continue;
        }
        const std::string& name = content.item(slot->first).name;
        rows.emplace_back(slot->second > 1 ? std::format("{} x{}", name, slot->second) : name);
    }
    return rows;
}

std::optional<std::string> slotItem(const game::Inventory& inventory, int index) {
    const auto slots = inventory.slots();
    if (index < 0 || index >= static_cast<int>(slots.size()) ||
        !slots[static_cast<std::size_t>(index)])
        return std::nullopt;
    return slots[static_cast<std::size_t>(index)]->first;
}

// ---- Town menu ---------------------------------------------------------------------------

namespace {
enum TownChoice { kMerchant, kBlacksmith, kGuildmaster, kBank, kBack };
} // namespace

TownMenuScreen::TownMenuScreen(Session& session)
    : TownScreen(session, "TOWN"),
      menu_({{"Merchant"}, {"Blacksmith"}, {"Guildmaster"}, {"Bank"}, {"Back"}}) {}

Transition TownMenuScreen::onKey(const KeyEvent& key) {
    if (key.key == Key::Escape)
        return Pop{};
    switch (menu_.handle(key).value_or(-1)) {
    case kMerchant:
        return Push{std::make_unique<MerchantScreen>(session_)};
    case kBlacksmith:
        return Push{std::make_unique<BlacksmithScreen>(session_)};
    case kGuildmaster:
        return Push{std::make_unique<GuildmasterScreen>(session_)};
    case kBank:
        return Push{std::make_unique<BankScreen>(session_)};
    case kBack:
        return Pop{};
    default:
        return Stay{};
    }
}

void TownMenuScreen::drawBody(Framebuffer& frame) const { menu_.draw(frame, 4, 7); }

// ---- Bank --------------------------------------------------------------------------------

BankScreen::BankScreen(Session& session) : TownScreen(session, "BANK"), bank_({}), bag_({}) {
    if (hero().bank)
        return;
    modal_.confirm(
        std::format("Open an account for {} gold?", game::kBankCost),
        [this] {
            if (succeeded(game::openBank(state()), [this] { leave_ = true; }))
                modal_.message("Your account is open.");
        },
        [this] { leave_ = true; });
}

void BankScreen::refresh() {
    bank_.setItems(hero().bank ? slotItems(*hero().bank, content()) : std::vector<Menu::Item>{});
    bag_.setItems(slotItems(hero().bag, content()));
}

Transition BankScreen::onKey(const KeyEvent& key) {
    if (key.key == Key::Escape)
        return Pop{};
    if (key.key == Key::Tab) {
        bagFocused_ = !bagFocused_;
        return Stay{};
    }
    if (!hero().bank)
        return Stay{};
    const bool depositing = bagFocused_;
    const game::Inventory& from = depositing ? hero().bag : *hero().bank;
    const auto index = (depositing ? bag_ : bank_).handle(key);
    const auto id = index ? slotItem(from, *index) : std::nullopt;
    if (!id)
        return Stay{};
    const int count = from.count(*id);
    modal_.number(std::format("How many? (1–{})", count), 1, count,
                  [this, depositing, id = *id](std::int64_t qty) {
                      const int n = static_cast<int>(qty);
                      (void)succeeded(depositing ? game::deposit(state(), id, n)
                                                 : game::withdraw(state(), id, n));
                  });
    return Stay{};
}

void BankScreen::drawBody(Framebuffer& frame) const {
    if (!hero().bank)
        return;
    drawText(frame, 1, 6, "Bank", bagFocused_ ? palette::kText : palette::kAccent);
    bank_.draw(frame, 1, 7, !bagFocused_);
    drawText(frame, 41, 6, "Your bag", bagFocused_ ? palette::kAccent : palette::kText);
    bag_.draw(frame, 41, 7, bagFocused_);
}

std::string_view BankScreen::hint() const { return "Tab switch list · Enter move · Esc back"; }

} // namespace heroes::app
