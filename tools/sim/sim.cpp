#include "sim/sim.hpp"

#include "game/battle.hpp"
#include "game/content_loader.hpp"
#include "game/game_state.hpp"

#include <charconv>
#include <format>
#include <set>
#include <thread>

namespace heroes::sim {

using namespace heroes::game;

namespace {

PlayerAction chooseAction(Policy policy, const Content& content, const Battle& battle,
                          const std::set<std::string>& learned) {
    if (policy == Policy::Attack)
        return Attack{};
    const Combatant& hero = battle.hero();
    auto ready = [&](std::string_view id) {
        return learned.contains(std::string(id)) && hero.mp >= content.skill(id).mpCost;
    };
    auto has = [](const Combatant& c, StatusId id) {
        for (const StatusInstance& s : c.statuses)
            if (s.id == id)
                return true;
        return false;
    };
    if (hero.hp * 100 < hero.maxHp * 30 && ready("angel_wings"))
        return CastSkill{"angel_wings"};
    if (ready("supernova"))
        return CastSkill{"supernova"};
    if (ready("frozen_throne") && !has(battle.enemy(), StatusId::Stun))
        return CastSkill{"frozen_throne"};
    if (ready("meteor_fist"))
        return CastSkill{"meteor_fist"};
    if (ready("golden_bell") && !has(hero, StatusId::Reflect))
        return CastSkill{"golden_bell"};
    return Attack{};
}

struct Setup {
    Content content;
    Combatant hero;
    std::set<std::string> skills;
    EnemyDef enemy;
    Report report; // everything except the tally
};

std::expected<void, std::string> checkGear(const Content& content, const std::string& id, int level,
                                           ItemKind kind, std::string_view slot) {
    const ItemDef* item = content.findItem(id);
    if (!item)
        return std::unexpected(std::format("unknown {} '{}'", slot, id));
    if (item->kind != kind)
        return std::unexpected(std::format("'{}' cannot be used as a {}", id, slot));
    if (level < 1 || level > 10)
        return std::unexpected(std::format("{} level must be 1-10, got {}", slot, level));
    return {};
}

std::expected<Setup, std::string> buildSetup(const SimConfig& config) {
    if (config.battles < 1)
        return std::unexpected("battles must be at least 1");
    if (config.threads < 1)
        return std::unexpected("threads must be at least 1");

    auto loaded = loadContent(config.dataDir);
    if (!loaded) {
        std::string message = "cannot load game data:";
        for (const ContentError& e : loaded.error())
            message += "\n  " + formatError(e);
        return std::unexpected(message);
    }
    Content content = std::move(*loaded);

    if (!content.findPath(config.pathId))
        return std::unexpected(std::format("unknown path '{}'", config.pathId));
    const EnemyDef* enemy = content.findEnemy(config.enemyId);
    if (!enemy)
        return std::unexpected(std::format("unknown enemy '{}'", config.enemyId));
    if (auto ok =
            checkGear(content, config.weaponId, config.weaponLevel, ItemKind::Weapon, "weapon");
        !ok)
        return std::unexpected(ok.error());
    if (auto ok = checkGear(content, config.armorId, config.armorLevel, ItemKind::Armor, "armor");
        !ok)
        return std::unexpected(ok.error());
    std::set<std::string> skills;
    for (const std::string& id : config.skills) {
        if (!content.findSkill(id))
            return std::unexpected(std::format("unknown skill '{}'", id));
        skills.insert(id);
    }

    Hero hero = newGame(content, "Sim", config.pathId, config.seed).hero;
    hero.weapon = Equipped{config.weaponId, config.weaponLevel};
    hero.armor = Equipped{config.armorId, config.armorLevel};
    hero.maxHp += config.bonusHp;
    hero.hp += config.bonusHp;
    hero.maxMp += config.bonusMp;
    hero.mp += config.bonusMp;
    hero.atk += config.bonusAtk;
    hero.def += config.bonusDef;

    Combatant combatant = heroCombatant(hero, content);
    Report report;
    report.battles = config.battles;
    report.seed = config.seed;
    report.pathId = config.pathId;
    report.enemyId = config.enemyId;
    report.atk = combatant.atk;
    report.def = combatant.def;
    report.life = combatant.life;
    report.hp = combatant.maxHp;
    report.mp = combatant.maxMp;
    EnemyDef enemyDef = *enemy;
    return Setup{std::move(content), std::move(combatant), std::move(skills), std::move(enemyDef),
                 std::move(report)};
}

Tally runRange(const SimConfig& config, const Setup& setup, std::uint64_t begin,
               std::uint64_t end) {
    Tally tally;
    for (std::uint64_t i = begin; i < end; ++i) {
        Battle battle(setup.content, setup.hero, setup.enemy, setup.skills,
                      Rng::stream(config.seed, i));
        while (!battle.over())
            battle.act(chooseAction(config.policy, setup.content, battle, setup.skills));
        tally.totalTurns += static_cast<std::uint64_t>(battle.turn());
        switch (battle.outcome()) {
        case Outcome::Victory:
            ++tally.wins;
            tally.hpLeftOnWins += static_cast<std::uint64_t>(battle.hero().hp);
            break;
        case Outcome::Defeat:
            ++tally.losses;
            break;
        case Outcome::Draw:
            ++tally.draws;
            break;
        case Outcome::Fled:
            ++tally.fled;
            break;
        }
    }
    return tally;
}

Tally merge(Tally a, const Tally& b) {
    a.wins += b.wins;
    a.losses += b.losses;
    a.draws += b.draws;
    a.fled += b.fled;
    a.totalTurns += b.totalTurns;
    a.hpLeftOnWins += b.hpLeftOnWins;
    return a;
}

} // namespace

std::expected<Report, std::string> runSimulation(const SimConfig& config) {
    auto setup = buildSetup(config);
    if (!setup)
        return std::unexpected(setup.error());

    // Contiguous ranges, merged in thread order, so the result never depends on scheduling.
    const std::uint64_t threads = config.threads;
    std::vector<Tally> tallies(threads);
    std::vector<std::thread> workers;
    for (std::uint64_t t = 0; t < threads; ++t) {
        const std::uint64_t begin = config.battles * t / threads;
        const std::uint64_t end = config.battles * (t + 1) / threads;
        workers.emplace_back(
            [&, t, begin, end] { tallies[t] = runRange(config, *setup, begin, end); });
    }
    for (std::thread& worker : workers)
        worker.join();

    Report report = std::move(setup->report);
    for (const Tally& tally : tallies)
        report.tally = merge(report.tally, tally);
    return report;
}

std::string formatReport(const SimConfig& config, const Report& r) {
    const auto n = static_cast<double>(r.battles);
    auto pct = [&](std::uint64_t count) { return 100.0 * static_cast<double>(count) / n; };
    const double avgHpOnWins = r.tally.wins == 0 ? 0.0
                                                 : static_cast<double>(r.tally.hpLeftOnWins) /
                                                       static_cast<double>(r.tally.wins);
    return std::format("battles: {}  seed: {}  threads: {}  policy: {}\n"
                       "enemy: {}  hero: {} atk={} def={} life={} hp={} mp={}\n"
                       "win: {:.2f}%  loss: {:.2f}%  draw: {:.2f}%  fled: {:.2f}%\n"
                       "avg turns: {:.2f}  avg hp left on wins: {:.2f}\n",
                       r.battles, r.seed, config.threads,
                       config.policy == Policy::Attack ? "attack" : "smart", r.enemyId, r.pathId,
                       r.atk, r.def, r.life, r.hp, r.mp, pct(r.tally.wins), pct(r.tally.losses),
                       pct(r.tally.draws), pct(r.tally.fled),
                       static_cast<double>(r.tally.totalTurns) / n, avgHpOnWins);
}

namespace {

template <class T>
std::expected<T, std::string> parseNumber(std::string_view flag, std::string_view text) {
    T value{};
    auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || end != text.data() + text.size())
        return std::unexpected(std::format("{} expects a number, got '{}'", flag, text));
    return value;
}

// "id" or "id:level"
std::expected<void, std::string> parseGear(std::string_view flag, std::string_view text,
                                           std::string& id, int& level) {
    const auto colon = text.find(':');
    id = std::string(text.substr(0, colon));
    level = 1;
    if (colon == std::string_view::npos)
        return {};
    auto parsed = parseNumber<int>(flag, text.substr(colon + 1));
    if (!parsed)
        return std::unexpected(parsed.error());
    level = *parsed;
    return {};
}

std::vector<std::string> splitCommas(std::string_view text) {
    std::vector<std::string> out;
    while (!text.empty()) {
        const auto comma = text.find(',');
        out.emplace_back(text.substr(0, comma));
        text = comma == std::string_view::npos ? std::string_view{} : text.substr(comma + 1);
    }
    return out;
}

} // namespace

std::expected<CliOptions, std::string> parseArgs(std::span<const std::string_view> args,
                                                 const std::filesystem::path& defaultDataDir,
                                                 unsigned defaultThreads) {
    CliOptions options;
    SimConfig& c = options.config;
    c.dataDir = defaultDataDir;
    c.threads = defaultThreads;
    bool haveBattles = false, haveSeed = false;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view flag = args[i];
        if (flag == "--bench") {
            options.bench = true;
            continue;
        }
        if (i + 1 >= args.size())
            return std::unexpected(std::format("{} needs a value", flag));
        const std::string_view value = args[++i];

        auto number = [&](auto& target) -> std::expected<void, std::string> {
            auto parsed = parseNumber<std::remove_reference_t<decltype(target)>>(flag, value);
            if (!parsed)
                return std::unexpected(parsed.error());
            target = *parsed;
            return {};
        };
        std::expected<void, std::string> result;
        if (flag == "--path")
            c.pathId = value;
        else if (flag == "--enemy")
            c.enemyId = value;
        else if (flag == "--battles") {
            result = number(c.battles);
            haveBattles = true;
        } else if (flag == "--seed") {
            result = number(c.seed);
            haveSeed = true;
        } else if (flag == "--threads")
            result = number(c.threads);
        else if (flag == "--policy") {
            if (value == "attack")
                c.policy = Policy::Attack;
            else if (value == "smart")
                c.policy = Policy::Smart;
            else
                return std::unexpected(
                    std::format("unknown policy '{}' (valid: attack, smart)", value));
        } else if (flag == "--weapon")
            result = parseGear(flag, value, c.weaponId, c.weaponLevel);
        else if (flag == "--armor")
            result = parseGear(flag, value, c.armorId, c.armorLevel);
        else if (flag == "--skills")
            c.skills = splitCommas(value);
        else if (flag == "--bonus-hp")
            result = number(c.bonusHp);
        else if (flag == "--bonus-mp")
            result = number(c.bonusMp);
        else if (flag == "--bonus-atk")
            result = number(c.bonusAtk);
        else if (flag == "--bonus-def")
            result = number(c.bonusDef);
        else if (flag == "--data-dir")
            c.dataDir = value;
        else
            return std::unexpected(std::format("unknown option '{}'", flag));
        if (!result)
            return std::unexpected(result.error());
    }
    if (c.pathId.empty())
        return std::unexpected("--path is required");
    if (c.enemyId.empty())
        return std::unexpected("--enemy is required");
    if (!haveBattles)
        return std::unexpected("--battles is required");
    if (!haveSeed)
        return std::unexpected("--seed is required");
    return options;
}

} // namespace heroes::sim
