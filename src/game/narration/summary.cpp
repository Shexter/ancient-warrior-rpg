#include "game/narration/summary.hpp"

#include <variant>

namespace heroes::game::narration {

namespace {

constexpr std::size_t kMaxMoments = 5;

int percent(int hp, int maxHp) { return maxHp > 0 ? hp * 100 / maxHp : 0; }

std::string_view resultName(Outcome o) {
    switch (o) {
    case Outcome::Victory:
        return "victory";
    case Outcome::Defeat:
        return "defeat";
    case Outcome::Fled:
        return "fled";
    case Outcome::Draw:
        return "draw";
    }
    return "draw";
}

} // namespace

std::optional<NarrationRequest> summarizeBattleChunk(std::span<const Event> events, BattleContext& ctx) {
    std::optional<NarrationRequest> moment;
    // Records a moment; the first one of the chunk becomes its request.
    const auto note = [&](std::string_view kind, std::string_view detail, std::string remembered) {
        if (ctx.moments.size() < kMaxMoments)
            ctx.moments.push_back(std::move(remembered));
        if (moment)
            return;
        moment = NarrationRequest{NarrationKind::BattleMoment,
                                  {{"hero", ctx.hero},
                                   {"foe", ctx.foe},
                                   {"moment", kind},
                                   {"detail", detail},
                                   {"heroHpPct", percent(ctx.heroHp, ctx.heroMaxHp)},
                                   {"foeHpPct", percent(ctx.foeHp, ctx.foeMaxHp)}},
                                  {}, {}, {}};
    };
    const auto hpNow = [&](Side side, int hp) {
        (side == Side::Hero ? ctx.heroHp : ctx.foeHp) = hp;
        bool& low = side == Side::Hero ? ctx.heroLow : ctx.foeLow;
        const int maxHp = side == Side::Hero ? ctx.heroMaxHp : ctx.foeMaxHp;
        if (!low && percent(hp, maxHp) <= 20) {
            low = true;
            const std::string_view who = side == Side::Hero ? "hero" : "foe";
            note("low_hp", who, "low_hp:" + std::string(who));
        }
    };
    for (const Event& event : events) {
        if (const auto* cast = std::get_if<ev::SkillCast>(&event)) {
            const std::string name = ctx.content->skill(cast->skillId).name;
            note("skill", name, name);
        } else if (const auto* hit = std::get_if<ev::Damaged>(&event)) {
            hpNow(hit->target, hit->hpAfter);
        } else if (const auto* halved = std::get_if<ev::HpHalved>(&event)) {
            hpNow(halved->target, halved->hpAfter);
        } else if (const auto* healed = std::get_if<ev::Healed>(&event)) {
            (healed->target == Side::Hero ? ctx.heroHp : ctx.foeHp) = healed->hpAfter;
        } else if (const auto* status = std::get_if<ev::StatusApplied>(&event)) {
            if (status->status == StatusId::Stun)
                note("status", "stun", "stun");
            else if (status->status == StatusId::Reflect)
                note("status", "reflect", "reflect");
        } else if (const auto* ended = std::get_if<ev::BattleEnded>(&event)) {
            return NarrationRequest{NarrationKind::BattleEnd,
                                    {{"hero", ctx.hero},
                                     {"foe", ctx.foe},
                                     {"result", resultName(ended->outcome)},
                                     {"turns", ended->turns},
                                     {"moments", ctx.moments},
                                     {"heroHpPct", percent(ctx.heroHp, ctx.heroMaxHp)}},
                                    {}, {}, {}};
        }
    }
    return moment;
}

} // namespace heroes::game::narration
