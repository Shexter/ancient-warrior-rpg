#include "app/battle_view.hpp"

#include "app/widgets.hpp"

#include <algorithm>
#include <cmath>
#include <format>

namespace heroes::app {

namespace {

using engine::Framebuffer;
using game::Side;
using game::StatusId;
namespace ev = game::ev;

constexpr double kFloatSeconds = 1.0; // a damage number floats 300 ms + 700 ms (ui.md)
constexpr double kFlashSeconds = 0.075;
constexpr double kShakeSeconds = 0.05;
constexpr std::size_t kMaxLogRows = 6; // the most any layout shows


std::string tag(StatusId id, int turns) {
    switch (id) {
    case StatusId::Stun:
        return std::format("[STUN {}]", turns);
    case StatusId::DefenseDown:
        return std::format("[DEF v {}]", turns); // `v`, not `↓`: single-width (ui.md)
    case StatusId::Reflect:
        return std::format("[REFLECT {}]", turns);
    }
    return {};
}

std::vector<std::pair<StatusId, int>> statusesOf(const game::Combatant& c) {
    std::vector<std::pair<StatusId, int>> out;
    for (const game::StatusInstance& s : c.statuses)
        out.emplace_back(s.id, s.turnsRemaining);
    return out;
}

// Flash on for 75 ms, off for 75 ms, twice.
bool flashOn(double elapsed) {
    return elapsed < 4 * kFlashSeconds && std::fmod(elapsed, 2 * kFlashSeconds) < kFlashSeconds;
}

} // namespace

BattleView::BattleView(const game::Content& content, const game::Combatant& hero,
                       const game::Combatant& enemy)
    : content_(content),
      hero_{upper(hero.name), hero.hp, hero.maxHp, hero.mp, hero.maxMp, statusesOf(hero)},
      enemy_{upper(enemy.name), enemy.hp, enemy.maxHp, enemy.mp, enemy.maxMp, statusesOf(enemy)},
      heroAfter_(hero), enemyAfter_(enemy) {}

void BattleView::push(std::vector<game::Event> events, const game::Combatant& hero,
                      const game::Combatant& enemy) {
    queue_.insert(queue_.end(), std::make_move_iterator(events.begin()),
                  std::make_move_iterator(events.end()));
    heroAfter_ = hero;
    enemyAfter_ = enemy;
    startNext();
}

void BattleView::log(std::string line) {
    log_.push_back(std::move(line));
    while (log_.size() > kMaxLogRows)
        log_.pop_front();
}

void BattleView::tick(double dtSeconds) {
    for (Floater& f : floaters_)
        f.age += dtSeconds;
    std::erase_if(floaters_, [](const Floater& f) { return f.age + kTimerSlack >= kFloatSeconds; });

    while (current_) {
        const double left = current_->duration - current_->elapsed;
        if (dtSeconds + kTimerSlack < left) {
            current_->elapsed += dtSeconds;
            return;
        }
        dtSeconds = std::max(0.0, dtSeconds - left);
        finishCurrent();
        startNext();
        if (dtSeconds <= kTimerSlack)
            return;
    }
}

void BattleView::skip() {
    if (!current_)
        return;
    finishCurrent();
    startNext();
}

// Starts queued events, finishing zero-length ones at once, until one takes time.
void BattleView::startNext() {
    while (!current_ && !queue_.empty()) {
        game::Event event = std::move(queue_.front());
        queue_.pop_front();
        const double duration = begin(event);
        current_ = Playing{std::move(event), duration};
        if (duration <= 0)
            finishCurrent();
    }
    if (!current_ && queue_.empty()) { // settled: show the battle's real numbers
        hero_.hp = heroAfter_.hp;
        hero_.mp = heroAfter_.mp;
        hero_.statuses = statusesOf(heroAfter_);
        enemy_.hp = enemyAfter_.hp;
        enemy_.mp = enemyAfter_.mp;
        enemy_.statuses = statusesOf(enemyAfter_);
    }
}

double BattleView::begin(const game::Event& event) {
    const auto name = [this](Side side) -> const std::string& { return fighter(side).name; };
    const auto floatUp = [this](Side side, std::string text, engine::Rgb color) {
        floaters_.push_back({side, std::move(text), color});
    };

    if (const auto* e = std::get_if<ev::TurnStarted>(&event)) {
        turn_ = e->turn;
        return 0;
    }
    if (const auto* e = std::get_if<ev::Attacked>(&event)) {
        log(std::format("{} attacks!", name(e->attacker)));
        return 0.3;
    }
    if (const auto* e = std::get_if<ev::SkillCast>(&event)) {
        hero_.mp = std::max(0, hero_.mp - e->mpSpent); // only the hero casts skills
        log(std::format("{} casts {}!", hero_.name, content_.skill(e->skillId).name));
        return 0.8;
    }
    if (const auto* e = std::get_if<ev::Damaged>(&event)) {
        fighter(e->target).hp = e->hpAfter;
        if (e->amount == 0) {
            floatUp(e->target, "BLOCK", palette::kDim);
            log(std::format("{}'s armor absorbs the blow.", name(e->target)));
        } else {
            floatUp(e->target, std::format("-{}", e->amount), palette::kDmg);
            log(e->cause == game::DamageCause::Reflect
                    ? std::format("{} damage reflected to {}.", e->amount, name(e->target))
                    : std::format("{} takes {} damage.", name(e->target), e->amount));
        }
        return 0.3;
    }
    if (const auto* e = std::get_if<ev::Healed>(&event)) {
        fighter(e->target).hp = e->hpAfter;
        floatUp(e->target, std::format("+{}", e->amount), palette::kHeal);
        log(std::format("{} recovers {} HP.", name(e->target), e->amount));
        return 0.3;
    }
    if (const auto* e = std::get_if<ev::HpHalved>(&event)) {
        fighter(e->target).hp = e->hpAfter;
        log(std::format("{}'s life force is torn in half!", name(e->target)));
        return 0.5;
    }
    if (const auto* e = std::get_if<ev::StatusApplied>(&event)) {
        auto& statuses = fighter(e->target).statuses;
        std::erase_if(statuses, [e](const auto& s) { return s.first == e->status; });
        statuses.emplace_back(e->status, e->turns);
        switch (e->status) {
        case StatusId::Stun:
            log(std::format("{} is frozen for {} turns.", name(e->target), e->turns));
            break;
        case StatusId::DefenseDown:
            log(std::format("Defense lowered for {} turns.", e->turns));
            break;
        case StatusId::Reflect:
            log(std::format("{} is shielded by a Golden Bell.", name(e->target)));
            break;
        }
        return 0.2;
    }
    if (const auto* e = std::get_if<ev::StatusExpired>(&event)) {
        std::erase_if(fighter(e->target).statuses,
                      [e](const auto& s) { return s.first == e->status; });
        switch (e->status) {
        case StatusId::Stun:
            log(std::format("{} thaws.", name(e->target)));
            break;
        case StatusId::DefenseDown:
            log(std::format("{}'s defense recovers.", name(e->target)));
            break;
        case StatusId::Reflect:
            log("The Golden Bell fades.");
            break;
        }
        return 0.2;
    }
    if (const auto* e = std::get_if<ev::TurnSkipped>(&event)) {
        log(std::format("{} is frozen and cannot move!", name(e->side)));
        return 0.5;
    }
    if (const auto* e = std::get_if<ev::ExtraTurn>(&event)) {
        log(std::format("{} strikes again!", name(e->side)));
        return 0.5;
    }
    if (const auto* e = std::get_if<ev::Defeated>(&event)) {
        log(e->side == Side::Enemy ? std::format("{} has been defeated!", enemy_.name)
                                   : std::format("{} has fallen...", hero_.name));
        return 0.8;
    }
    if (std::holds_alternative<ev::Fled>(event)) {
        log(std::format("{} flees the battle.", hero_.name));
        return 0.5;
    }
    if (const auto* e = std::get_if<ev::BattleEnded>(&event))
        ended_ = e->outcome;
    return 0; // BattleStarted, BattleEnded
}

void BattleView::finishCurrent() {
    if (const auto* e = std::get_if<ev::Defeated>(&current_->event))
        fighter(e->side).gone = true;
    else if (std::holds_alternative<ev::Fled>(current_->event))
        hero_.gone = true;
    current_.reset();
}

int BattleView::shake() const {
    if (!current_ || !std::holds_alternative<ev::SkillCast>(current_->event))
        return 0;
    return static_cast<int>((current_->elapsed + kTimerSlack) / kShakeSeconds) % 2 == 0 ? 1 : -1;
}

void BattleView::drawFighter(Framebuffer& frame, Side side, Art art,
                             const BattleLayout& layout) const {
    const Fighter& self = fighter(side);
    if (self.gone || !art)
        return;
    int x = side == Side::Hero ? layout.heroX : layout.enemyX;
    const int y = side == Side::Hero ? layout.heroY : layout.enemyY;
    engine::SpriteTint tint;
    if (current_) {
        const game::Event& e = current_->event;
        const double t = current_->duration > 0 ? current_->elapsed / current_->duration : 1;
        if (const auto* a = std::get_if<ev::Attacked>(&e); a && a->attacker == side) {
            const int step = static_cast<int>(std::lround(6 * std::min(t, 1 - t))); // 0..3..0
            x += side == Side::Hero ? step : -step;
        } else if (const auto* d = std::get_if<ev::Damaged>(&e);
                   d && d->target == side && d->amount > 0 && flashOn(current_->elapsed)) {
            tint = engine::SpriteTint::flash();
        } else if (const auto* h = std::get_if<ev::HpHalved>(&e);
                   h && h->target == side && flashOn(current_->elapsed)) {
            tint = engine::SpriteTint::flash();
        } else if (const auto* s = std::get_if<ev::TurnSkipped>(&e); s && s->side == side) {
            tint = engine::SpriteTint::dim(3);
        } else if (const auto* f = std::get_if<ev::Defeated>(&e); f && f->side == side) {
            tint = engine::SpriteTint::dim(std::min(8, static_cast<int>(t * 9)));
        } else if (std::holds_alternative<ev::Fled>(e) && side == Side::Hero) {
            x -= static_cast<int>(t * (layout.heroX + art.width()));
        }
    }
    art.draw(frame, x, y, tint);

    for (const Floater& f : floaters_) {
        if (f.side != side)
            continue;
        const int rise = std::min(3, static_cast<int>(f.age / kFloatSeconds * 4));
        drawText(frame, x + (art.width() - textWidth(f.text)) / 2, y + 1 - rise, f.text,
                 f.color);
    }
}

void BattleView::drawStats(Framebuffer& frame, const BattleLayout& layout) const {
    // Name and HP bar for each side on statsRow. Below: the hero's Fervor and the enemy's HP
    // numbers (ui.md's single HP row needs 83 columns), then status tags.
    const int row = layout.statsRow;
    drawText(frame, 1, row, hero_.name, palette::kAccent, 8);
    drawText(frame, 10, row, "HP", palette::kText);
    drawBar(frame, 13, row, 20, hero_.hp, hero_.maxHp, hpColor(hero_.hp, hero_.maxHp));
    // Up to the enemy's stats: 9 cells in the compact layout, more in the large one.
    drawText(frame, 34, row, std::format("{}/{}", hero_.hp, hero_.maxHp), palette::kText,
             layout.enemyStatsX - 1 - 34);
    drawText(frame, 10, row + 1, "FV", palette::kText); // Fervor
    drawBar(frame, 13, row + 1, 8, hero_.mp, hero_.maxMp, palette::kMp);
    drawText(frame, 22, row + 1, std::format("{}/{}", hero_.mp, hero_.maxMp), palette::kText, 7);
    int x = 30;
    for (const auto& [id, turns] : hero_.statuses)
        x += drawText(frame, x, row + 1, tag(id, turns), palette::kAccent,
                      std::max(0, layout.enemyStatsX - 1 - x)) +
             1;

    const int ex = layout.enemyStatsX;
    drawText(frame, ex, row, enemy_.name, palette::kAccent, 10);
    drawText(frame, ex + 11, row, "HP", palette::kText);
    drawBar(frame, ex + 14, row, 20, enemy_.hp, enemy_.maxHp, hpColor(enemy_.hp, enemy_.maxHp));
    x = ex + drawText(frame, ex, row + 1, std::format("{}/{}", enemy_.hp, enemy_.maxHp),
                      palette::kText, 15) +
        2;
    for (const auto& [id, turns] : enemy_.statuses)
        x += drawText(frame, x, row + 1, tag(id, turns), palette::kAccent,
                      std::max(0, frame.width() - 1 - x)) +
             1;
}

void BattleView::draw(Framebuffer& frame, Art hero, Art enemy, const BattleLayout& layout) const {
    drawFighter(frame, Side::Enemy, enemy, layout);
    drawFighter(frame, Side::Hero, hero, layout);

    if (current_) {
        std::string banner;
        if (const auto* s = std::get_if<ev::SkillCast>(&current_->event))
            banner = content_.skill(s->skillId).name;
        else if (std::holds_alternative<ev::HpHalved>(current_->event))
            banner = "HP HALVED";
        else if (std::holds_alternative<ev::ExtraTurn>(current_->event))
            banner = "AGAIN!";
        if (!banner.empty())
            drawCentered(frame, layout.bannerRow, banner, palette::kAccent);
    }

    drawStats(frame, layout);
    const std::size_t shown = std::min(log_.size(), static_cast<std::size_t>(layout.logRows));
    for (std::size_t i = 0; i < shown; ++i)
        drawText(frame, 1, layout.logRow + static_cast<int>(i), log_[log_.size() - shown + i],
                 palette::kText, frame.width() - 2);
}

} // namespace heroes::app
