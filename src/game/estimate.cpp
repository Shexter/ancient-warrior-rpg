#include "game/estimate.hpp"

#include "game/autoplay.hpp"
#include "game/battle.hpp"
#include "game/town.hpp"
#include "game/training.hpp"

#include <format>
#include <vector>

namespace heroes::game {

int caveWinChance(const GameState& state, const Content& content, int caveLevel) {
    constexpr std::uint64_t kCaveSalt = 0xCA7E'0000'0000'0000ULL;
    const Combatant hero = heroCombatant(state.hero, content);
    std::vector<Event> events; // reused; only the outcome matters
    int wins = 0;
    for (int i = 0; i < kEstimateFights; ++i) {
        Rng rng = Rng::stream(state.seed ^ (kCaveSalt + static_cast<std::uint64_t>(caveLevel)),
                              static_cast<std::uint64_t>(i));
        const TrainingMonster monster = rollTrainingMonster(rng, caveLevel);
        Combatant fighter = hero;
        events.clear();
        wins += fightTrainingMonster(fighter, monster, events) == Outcome::Victory ? 1 : 0;
    }
    return wins * 100 / kEstimateFights;
}

int missionWinChance(const GameState& state, const Content& content, std::string_view missionId,
                     script::ScriptHost* scripts) {
    constexpr std::uint64_t kMissionSalt = 0x4D15'0000'0000'0000ULL;
    const Hero& hero = state.hero;
    const MissionDef& mission = content.mission(missionId);
    const Combatant fighter = heroCombatant(hero, content);
    const ItemStock stock = consumableStock(hero, content);
    std::uint64_t salt = kMissionSalt;
    for (char c : mission.id)
        salt = salt * 31 + static_cast<unsigned char>(c);
    int wins = 0;
    for (int i = 0; i < kEstimateFights; ++i) {
        Battle battle(content, fighter, content.enemy(mission.enemy), hero.learnedSkills,
                      Rng::stream(state.seed ^ salt, static_cast<std::uint64_t>(i)), scripts, stock);
        while (!battle.over())
            (void)battle.act(smartAction(content, battle, hero.learnedSkills));
        wins += battle.outcome() == Outcome::Victory ? 1 : 0;
    }
    return wins * 100 / kEstimateFights;
}

std::string nextGoal(const GameState& state, const Content& content, script::ScriptHost* scripts) {
    const Hero& hero = state.hero;
    if (hero.hp <= 0)
        return "Next: visit the Prior to be revived";
    if (hero.blessings > 0)
        return std::format("Next: spend {} blessing{} at the Prior", hero.blessings,
                           hero.blessings == 1 ? "" : "s");
    const MissionDef* bonus = nullptr;
    for (const MissionDef& m : content.missions()) {
        if (hero.completedMissions.contains(m.id))
            continue;
        if (!m.name.starts_with("Act I:")) {
            bonus = bonus ? bonus : &m;
            continue;
        }
        if (hero.level < m.minLevel)
            return std::format("Next: train to Lv {} for {}", m.minLevel, m.name);
        const int odds = missionWinChance(state, content, m.id, scripts);
        return std::format("Next: {} (Lv {}) \u2014 your odds \u2248{}%; {}", content.enemy(m.enemy).name,
                           m.minLevel, odds,
                           odds < 50 ? "upgrade gear, learn skills, or train" : "you're ready");
    }
    if (bonus) { // the quoted title, as in: Bonus: "The Gorgon"
        const auto open = bonus->name.find('"');
        const auto close = bonus->name.rfind('"');
        std::string title = open < close ? bonus->name.substr(open + 1, close - open - 1) : bonus->name;
        if (title.starts_with("The "))
            title[0] = 't';
        return std::format("Next: {} awaits (Lv {}), if you dare", title, bonus->minLevel);
    }
    return "Act I is complete.";
}

} // namespace heroes::game
