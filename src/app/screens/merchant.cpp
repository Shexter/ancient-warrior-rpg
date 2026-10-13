#include "app/screens/town.hpp"
#include "app/art.hpp"

#include <format>
#include <string>

namespace heroes::app {

namespace {

using engine::Key;

constexpr double kBidSeconds = 0.6; // one auction bid per 600 ms (ui.md)

} // namespace

MerchantScreen::MerchantScreen(Session& session)
    : TownScreen(session, "ALMONRY"), shop_({}), bag_({}) {
    std::vector<Menu::Item> rows;
    for (const game::ItemDef& item : content().items())
        if (item.price > 0) {
            forSale_.push_back(&item);
            rows.emplace_back(item.name);
        }
    shop_.setItems(std::move(rows));
    if (hero().hp <= 0)
        modal_.message("The Almonry serves the living. See the Prior first.", [this] { leave_ = true; });
}

void MerchantScreen::refresh() { bag_.setItems(slotItems(hero().bag, content())); }

Transition MerchantScreen::onKey(const engine::KeyEvent& key) {
    if (auction_) { // the bids are already settled; Enter only skips the show
        if (key.key == Key::Enter)
            finishAuction();
        return Stay{};
    }
    if (key.key == Key::Escape)
        return Pop{};
    if (key.key == Key::Tab || key.key == Key::Left || key.key == Key::Right) {
        bagFocused_ = !bagFocused_;
        return Stay{};
    }
    if (bagFocused_) {
        const auto index = bag_.handle(key);
        if (const auto id = index ? slotItem(hero().bag, *index) : std::nullopt)
            chooseBagItem(*id);
        return Stay{};
    }
    const auto index = shop_.handle(key);
    if (!index)
        return Stay{};
    const game::ItemDef& item = *forSale_[static_cast<std::size_t>(*index)];
    modal_.number(
        "How many? (1–99)", 1, 99,
        [this, &item](std::int64_t qty) {
            if (succeeded(game::buy(state(), content(), item.id, static_cast<int>(qty))))
                modal_.message(std::format("You bought {} {} for {} gold.", qty, item.name,
                                           item.price * qty));
        },
        [&item](std::int64_t qty) { return std::format("Total {} gold", item.price * qty); });
    return Stay{};
}

void MerchantScreen::chooseBagItem(const std::string& id) {
    const game::ItemDef& item = content().item(id);
    const int count = hero().bag.count(id);
    const bool sellable = item.price > 0;
    const bool auctionable = item.auctionValue > 0;
    modal_.choose(
        count > 1 ? std::format("{} x{}", item.name, count) : item.name,
        {{"Sell", sellable,
          sellable ? std::format("{} gold each", item.price) : "The merchant won't buy it"},
         {"Auction", auctionable,
          auctionable ? std::format("bids from {} gold", item.auctionValue) : "Nobody bids on it"},
         {"Cancel"}},
        [this, &item, count](int choice) {
            if (choice == 1) {
                startAuction(item.id);
                return;
            }
            if (choice != 0)
                return;
            modal_.number(
                std::format("How many? (1–{})", count), 1, count,
                [this, &item](std::int64_t qty) {
                    if (succeeded(game::sell(state(), content(), item.id, static_cast<int>(qty))))
                        modal_.message(std::format("You sold {} {} for {} gold.", qty, item.name,
                                                   item.price * qty));
                },
                [&item](std::int64_t qty) {
                    return std::format("Total {} gold", item.price * qty);
                });
        });
}

void MerchantScreen::startAuction(const std::string& id) {
    const auto result = game::auction(state(), content(), id);
    if (!succeeded(result))
        return;
    const game::ItemDef& item = content().item(id);
    if (!result->sold) {
        modal_.message(std::format("No buyer came. The {} stays in your bag.", item.name));
        return;
    }
    Auction show{item.name, {}, result->payout};
    for (int i = 0; i <= result->bids; ++i) // the opening bid, then each raise
        show.bids.push_back(item.auctionValue + i * (item.auctionValue / 10));
    auction_ = std::move(show);
}

void MerchantScreen::onTick(double dtSeconds) {
    if (!auction_)
        return;
    auction_->clock += dtSeconds;
    while (auction_->clock + kTimerSlack >= kBidSeconds) {
        auction_->clock -= kBidSeconds;
        if (auction_->shown == auction_->bids.size()) {
            finishAuction();
            return;
        }
        ++auction_->shown;
    }
}

void MerchantScreen::finishAuction() {
    const std::int64_t payout = auction_->payout;
    auction_.reset();
    modal_.message(std::format("Merchant fee is 10%. You receive {} gold.", payout));
}

void MerchantScreen::drawBody(engine::Framebuffer& frame) const {
    if (isPortrait(frame)) {
        drawBodyPortrait(frame);
        return;
    }
    drawText(frame, 1, 6, "For sale", bagFocused_ ? palette::kText : palette::kAccent);
    shop_.draw(frame, 1, 7, !bagFocused_);
    for (std::size_t i = 0; i < forSale_.size(); ++i) {
        const std::string price = std::to_string(forSale_[i]->price);
        drawText(frame, 39 - textWidth(price), 7 + static_cast<int>(i), price, palette::kGold);
    }
    drawText(frame, 41, 6, "Your bag", bagFocused_ ? palette::kAccent : palette::kText);
    bag_.draw(frame, 41, 7, bagFocused_);

    const game::ItemDef* highlighted = nullptr;
    if (!bagFocused_ && !forSale_.empty())
        highlighted = forSale_[static_cast<std::size_t>(shop_.selected())];
    else if (const auto id = bagFocused_ ? slotItem(hero().bag, bag_.selected()) : std::nullopt)
        highlighted = &content().item(*id);
    if (highlighted)
        drawText(frame, 1, 19, highlighted->description, palette::kDim, 78);

    if (!auction_)
        return;
    const int rows = static_cast<int>(auction_->bids.size());
    drawPanel(frame, 20, 7, 40, rows + 4);
    drawText(frame, 22, 8, std::format("Auction: {}", auction_->itemName), palette::kAccent);
    for (std::size_t i = 0; i < auction_->shown; ++i)
        drawText(frame, 22, 10 + static_cast<int>(i),
                 std::format("Bid accepted at {}…", auction_->bids[i]), palette::kText);
}

// One list at a time (◀ ▶ switch), the highlighted item's description above it, prices in the
// labels' notes column.
void MerchantScreen::drawBodyPortrait(engine::Framebuffer& frame) const {
    drawText(frame, 2, 7, bagFocused_ ? "◀ Your bag ▶" : "◀ For sale ▶", palette::kAccent);
    drawText(frame, 2, 8, "◀ ▶ switch list", palette::kDim);
    const game::ItemDef* highlighted = nullptr;
    if (!bagFocused_ && !forSale_.empty())
        highlighted = forSale_[static_cast<std::size_t>(shop_.selected())];
    else if (const auto id = bagFocused_ ? slotItem(hero().bag, bag_.selected()) : std::nullopt)
        highlighted = &content().item(*id);
    if (highlighted) {
        drawText(frame, 2, 11, highlighted->name, palette::kText, 44);
        const auto lines = wordWrap(highlighted->description, 44);
        for (std::size_t i = 0; i < lines.size() && i < 6; ++i)
            drawText(frame, 2, 12 + static_cast<int>(i), lines[i], palette::kDim);
    }
    drawButtonMenu(frame, bagFocused_ ? bag_ : shop_);
    if (!bagFocused_)
        for (std::size_t i = 0; i < forSale_.size() && i < portrait::kMenuRows; ++i) {
            const std::string price = std::to_string(forSale_[i]->price);
            drawText(frame, frame.width() - 4 - textWidth(price),
                     portrait::kMenuTop + 1 + static_cast<int>(i), price, palette::kGold);
        }
    if (!auction_)
        return;
    const int rows = static_cast<int>(auction_->bids.size());
    drawPanel(frame, 2, 7, frame.width() - 4, rows + 4);
    drawText(frame, 4, 8, std::format("Auction: {}", auction_->itemName), palette::kAccent, 40);
    for (std::size_t i = 0; i < auction_->shown; ++i)
        drawText(frame, 4, 10 + static_cast<int>(i),
                 std::format("Bid accepted at {}…", auction_->bids[i]), palette::kText);
}

std::string_view MerchantScreen::hint() const {
    return auction_ ? "Enter skip" : "Tab/←→ switch list · Enter choose · Esc back";
}

} // namespace heroes::app
