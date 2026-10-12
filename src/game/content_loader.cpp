#include "game/content_loader.hpp"

#include "game/script/script_host.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <limits>
#include <optional>
#include <type_traits>
#include <unordered_set>

namespace heroes::game {

// Lets the loader fill a Content, whose tables are otherwise private.
struct ContentBuilder {
    static Content assemble(std::vector<ItemDef> items, std::vector<PathDef> paths,
                            std::vector<SkillDef> skills, std::vector<EnemyDef> enemies,
                            std::vector<MissionDef> missions, std::vector<RecipeDef> recipes,
                            StoryDef story,
                            std::map<std::string, std::string, std::less<>> scripts = {}) {
        Content c;
        c.scripts_ = std::move(scripts);
        c.items_.all = std::move(items);
        c.paths_.all = std::move(paths);
        c.skills_.all = std::move(skills);
        c.enemies_.all = std::move(enemies);
        c.missions_.all = std::move(missions);
        c.recipes_.all = std::move(recipes);
        c.story_ = std::move(story);
        c.items_.build();
        c.paths_.build();
        c.skills_.build();
        c.enemies_.build();
        c.missions_.build();
        c.recipes_.build();
        return c;
    }
};

namespace {

using Json = nlohmann::json;
using Errors = std::vector<ContentError>;

struct Ctx {
    Errors& errors;
    std::string file;
};

template <class T> bool convert(const Json& v, T& out) {
    if constexpr (std::is_same_v<T, std::string>) {
        if (!v.is_string())
            return false;
        out = v.get<std::string>();
        return true;
    } else if constexpr (std::is_same_v<T, bool>) {
        if (!v.is_boolean())
            return false;
        out = v.get<bool>();
        return true;
    } else {
        if (!v.is_number_integer())
            return false;
        if (v.is_number_unsigned()) {
            const auto u = v.get<std::uint64_t>();
            if (!std::in_range<T>(u))
                return false;
            out = static_cast<T>(u);
        } else {
            const auto s = v.get<std::int64_t>();
            if (!std::in_range<T>(s))
                return false;
            out = static_cast<T>(s);
        }
        return true;
    }
}

// Reads the keys of one JSON object, recording type, range, missing and unknown-key errors.
class Fields {
  public:
    Fields(Ctx& ctx, const Json& j, std::string entry,
           std::initializer_list<std::string_view> allowed, std::string prefix = "")
        : ctx_(ctx), json_(j), entry_(std::move(entry)), prefix_(std::move(prefix)) {
        if (!j.is_object()) {
            fail("expected an object");
            return;
        }
        for (const auto& [key, value] : j.items()) {
            if (std::ranges::find(allowed, std::string_view(key)) == allowed.end())
                fail(std::format("unknown key '{}{}'", prefix_, key));
        }
    }

    void fail(std::string message) {
        ctx_.errors.push_back({ctx_.file, entry_, std::move(message)});
    }
    Ctx& ctx() { return ctx_; }
    const std::string& entry() const { return entry_; }

    const Json* raw(std::string_view key) const {
        if (!json_.is_object())
            return nullptr;
        auto it = json_.find(key);
        return it == json_.end() ? nullptr : &*it;
    }

    template <class T>
    T req(std::string_view key, T lo = std::numeric_limits<T>::lowest(),
          T hi = std::numeric_limits<T>::max()) {
        return get<T>(key, std::nullopt, lo, hi);
    }
    template <class T>
    T opt(std::string_view key, T fallback, T lo = std::numeric_limits<T>::lowest(),
          T hi = std::numeric_limits<T>::max()) {
        return get<T>(key, std::move(fallback), lo, hi);
    }

    // Array of strings; missing is an error only when `required`.
    std::vector<std::string> strings(std::string_view key, bool required) {
        std::vector<std::string> out;
        const Json* v = raw(key);
        if (!v) {
            if (required && json_.is_object())
                fail(std::format("missing required key '{}{}'", prefix_, key));
            return out;
        }
        if (!v->is_array()) {
            fail(std::format("key '{}{}' must be an array of strings", prefix_, key));
            return out;
        }
        for (const Json& s : *v) {
            std::string str;
            if (convert(s, str))
                out.push_back(std::move(str));
            else
                fail(std::format("key '{}{}' must contain only strings", prefix_, key));
        }
        return out;
    }

  private:
    template <class T> T get(std::string_view key, std::optional<T> fallback, T lo, T hi) {
        const Json* v = raw(key);
        if (!v) {
            if (!fallback && json_.is_object())
                fail(std::format("missing required key '{}{}'", prefix_, key));
            return fallback.value_or(T{});
        }
        T out{};
        if (!convert(*v, out)) {
            fail(std::format("key '{}{}' has the wrong type", prefix_, key));
            return fallback.value_or(T{});
        }
        if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
            if (out < lo || out > hi) {
                fail(hi == std::numeric_limits<T>::max()
                         ? std::format("key '{}{}' must be at least {}", prefix_, key, lo)
                         : std::format("key '{}{}' must be between {} and {}", prefix_, key, lo,
                                       hi));
                return fallback.value_or(T{});
            }
        }
        return out;
    }

    Ctx& ctx_;
    const Json& json_;
    std::string entry_;
    std::string prefix_;
};

template <class E, std::size_t N>
E readEnum(Fields& f, std::string_view key,
           const std::array<std::pair<std::string_view, E>, N>& table) {
    const std::string name = f.req<std::string>(key);
    for (const auto& [text, value] : table)
        if (text == name)
            return value;
    if (f.raw(key) != nullptr)
        f.fail(std::format("unknown {} '{}'", key, name));
    return table[0].second;
}

constexpr std::array<std::pair<std::string_view, ItemKind>, 5> kItemKinds{{
    {"weapon", ItemKind::Weapon},
    {"armor", ItemKind::Armor},
    {"skill_key", ItemKind::SkillKey},
    {"material", ItemKind::Material},
    {"consumable", ItemKind::Consumable},
}};
constexpr std::array<std::pair<std::string_view, StatusId>, 3> kStatuses{{
    {"stun", StatusId::Stun},
    {"defense_down", StatusId::DefenseDown},
    {"reflect", StatusId::Reflect},
}};
constexpr std::array<std::pair<std::string_view, EnemyBehavior>, 1> kBehaviors{{
    {"basic", EnemyBehavior::Basic},
}};

// "item 'iron_sword'", or "item #3" when the entry has no usable id.
std::string entryLabel(std::string_view noun, const Json& j, std::size_t index) {
    if (j.is_object()) {
        auto it = j.find("id");
        if (it != j.end() && it->is_string())
            return std::format("{} '{}'", noun, it->get<std::string>());
    }
    return std::format("{} #{}", noun, index + 1);
}

// Opens `<dir>/<file>` and returns the array under `arrayKey`, or reports a file-level error.
std::optional<Json> loadFile(Ctx& ctx, const std::filesystem::path& dir) {
    std::ifstream in(dir / ctx.file);
    if (!in) {
        ctx.errors.push_back({ctx.file, "", "cannot open file"});
        return std::nullopt;
    }
    try {
        return Json::parse(in);
    } catch (const Json::exception& e) {
        ctx.errors.push_back({ctx.file, "", e.what()});
        return std::nullopt;
    }
}

// Parses `{ "<arrayKey>": [ ... ] }`, calling `parseOne(json, index)` for every element.
template <class T, class ParseOne>
std::optional<std::vector<T>> loadList(Ctx& ctx, const std::filesystem::path& dir,
                                       std::string_view arrayKey, ParseOne parseOne) {
    auto doc = loadFile(ctx, dir);
    if (!doc)
        return std::nullopt;
    Fields top(ctx, *doc, "", {arrayKey});
    const Json* list = top.raw(arrayKey);
    if (!list || !list->is_array()) {
        top.fail(std::format("expected an object with an array '{}'", arrayKey));
        return std::nullopt;
    }
    std::vector<T> out;
    for (std::size_t i = 0; i < list->size(); ++i)
        out.push_back(parseOne((*list)[i], i));
    return out;
}

bool validId(const std::string& id) {
    if (id.empty() || id[0] < 'a' || id[0] > 'z')
        return false;
    return std::ranges::all_of(
        id, [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
}

template <class T> void checkIds(Ctx& ctx, const std::vector<T>& defs, std::string_view noun) {
    std::unordered_set<std::string> seen;
    for (const T& d : defs) {
        const std::string entry = std::format("{} '{}'", noun, d.id);
        if (!validId(d.id))
            ctx.errors.push_back({ctx.file, entry, "id must match [a-z][a-z0-9_]*"});
        if (!seen.insert(d.id).second)
            ctx.errors.push_back({ctx.file, entry, "duplicate id"});
    }
}

ItemDef parseItem(Ctx& ctx, const Json& j, std::size_t index) {
    Fields f(ctx, j, entryLabel("item", j, index),
             {"id", "name", "description", "kind", "bonus", "price", "auctionValue", "dropRate",
              "upgradeMaterial", "consumable"});
    ItemDef d;
    d.id = f.req<std::string>("id");
    d.name = f.req<std::string>("name");
    d.description = f.req<std::string>("description");
    d.kind = readEnum(f, "kind", kItemKinds);
    if (const Json* b = f.raw("bonus")) {
        Fields bf(ctx, *b, f.entry(), {"atk", "def", "life"}, "bonus.");
        d.bonus = {bf.opt<int>("atk", 0, 0, kMaxStat), bf.opt<int>("def", 0, 0, kMaxStat),
                   bf.opt<int>("life", 0, 0, kMaxStat)};
    }
    d.price = f.opt<std::int64_t>("price", 0, 0, kMaxStat);
    d.auctionValue = f.opt<std::int64_t>("auctionValue", 0, 0, kMaxStat);
    d.dropRate = f.opt<int>("dropRate", 0, 0, 100);
    if (f.raw("upgradeMaterial"))
        d.upgradeMaterial = f.req<std::string>("upgradeMaterial");
    if (const Json* c = f.raw("consumable")) {
        Fields cf(ctx, *c, f.entry(), {"restoreHp", "restoreMp", "fullRestore"}, "consumable.");
        d.consumable = ConsumableEffect{cf.opt<int>("restoreHp", 0, 0, kMaxStat),
                                        cf.opt<int>("restoreMp", 0, 0, kMaxStat),
                                        cf.opt<bool>("fullRestore", false)};
    }
    return d;
}

PathDef parsePath(Ctx& ctx, const Json& j, std::size_t index) {
    Fields f(ctx, j, entryLabel("path", j, index),
             {"id", "name", "flavor", "multiplier", "maxHp", "maxMp", "atk", "def", "life"});
    PathDef d;
    d.id = f.req<std::string>("id");
    d.name = f.req<std::string>("name");
    d.flavor = f.req<std::string>("flavor");
    d.multiplier = f.req<int>("multiplier", 1, kMaxPathMultiplier);
    d.maxHp = f.req<int>("maxHp", 0, kMaxStat);
    d.maxMp = f.req<int>("maxMp", 0, kMaxStat);
    d.atk = f.req<int>("atk", 0, kMaxStat);
    d.def = f.req<int>("def", 0, kMaxStat);
    d.life = f.req<int>("life", 0, kMaxStat);
    return d;
}

std::vector<StatusSpec> parseStatuses(Fields& f, std::string_view key) {
    std::vector<StatusSpec> out;
    const Json* list = f.raw(key);
    if (!list)
        return out;
    if (!list->is_array()) {
        f.fail(std::format("key '{}' must be an array", key));
        return out;
    }
    for (std::size_t i = 0; i < list->size(); ++i) {
        Fields sf(f.ctx(), (*list)[i], f.entry(), {"id", "turns"}, std::format("{}[{}].", key, i));
        const StatusId id = readEnum(sf, "id", kStatuses);
        out.push_back({id, sf.req<int>("turns", 1, kMaxStat)});
    }
    return out;
}

SkillDef parseSkill(Ctx& ctx, const Json& j, std::size_t index) {
    Fields f(
        ctx, j, entryLabel("skill", j, index),
        {"id", "name", "description", "mpCost", "learnLevel", "goldCost", "keyItem", "effect",
         "script"});
    SkillDef d;
    d.id = f.req<std::string>("id");
    d.name = f.req<std::string>("name");
    d.description = f.req<std::string>("description");
    d.mpCost = f.req<int>("mpCost", 0, kMaxStat);
    d.learnLevel = f.req<int>("learnLevel", 0, kMaxStat);
    d.goldCost = f.req<std::int64_t>("goldCost", 0, kMaxStat);
    d.keyItem = f.req<std::string>("keyItem");
    if (f.raw("script")) {
        d.script = f.req<std::string>("script");
        if (f.raw("effect"))
            f.fail("has both 'effect' and 'script'; use one");
        return d;
    }
    if (const Json* e = f.raw("effect")) {
        Fields ef(ctx, *e, f.entry(),
                  {"healToFull", "atkMultiplier", "ignoreDef", "halveTargetHpFirst",
                   "healPercentOfDamage", "selfStatuses", "targetStatuses",
                   "requiresTargetNotStunned"},
                  "effect.");
        SkillEffect& s = d.effect;
        s.healToFull = ef.opt<bool>("healToFull", false);
        s.atkMultiplier = ef.opt<int>("atkMultiplier", 0, 0, kMaxSkillMultiplier);
        s.ignoreDef = ef.opt<bool>("ignoreDef", false);
        s.halveTargetHpFirst = ef.opt<bool>("halveTargetHpFirst", false);
        s.healPercentOfDamage = ef.opt<int>("healPercentOfDamage", 0, 0, 100);
        s.selfStatuses = parseStatuses(ef, "selfStatuses");
        s.targetStatuses = parseStatuses(ef, "targetStatuses");
        s.requiresTargetNotStunned = ef.opt<bool>("requiresTargetNotStunned", false);
    } else {
        f.fail("needs 'effect' or 'script'");
    }
    return d;
}

EnemyDef parseEnemy(Ctx& ctx, const Json& j, std::size_t index) {
    Fields f(ctx, j, entryLabel("enemy", j, index),
             {"id", "name", "sprite", "maxHp", "atk", "def", "life", "behavior", "script",
              "rewardExp", "rewardGold", "rewardItems"});
    EnemyDef d;
    d.id = f.req<std::string>("id");
    d.name = f.req<std::string>("name");
    d.sprite = f.req<std::string>("sprite");
    d.maxHp = f.req<int>("maxHp", 0, kMaxStat);
    d.atk = f.req<int>("atk", 0, kMaxStat);
    d.def = f.req<int>("def", 0, kMaxStat);
    d.life = f.req<int>("life", 0, kMaxStat);
    if (f.raw("script")) {
        d.script = f.req<std::string>("script");
        if (f.raw("behavior"))
            f.fail("has both 'behavior' and 'script'; use one");
    } else {
        d.behavior = readEnum(f, "behavior", kBehaviors);
    }
    d.rewardExp = f.req<std::int64_t>("rewardExp", 0, kMaxStat);
    d.rewardGold = f.req<std::int64_t>("rewardGold", 0, kMaxStat);
    d.rewardItems = f.strings("rewardItems", false);
    return d;
}

MissionDef parseMission(Ctx& ctx, const Json& j, std::size_t index) {
    Fields f(ctx, j, entryLabel("mission", j, index),
             {"id", "name", "location", "enemy", "minLevel", "repeatable", "story"});
    MissionDef d;
    d.id = f.req<std::string>("id");
    d.name = f.req<std::string>("name");
    d.location = f.req<std::string>("location");
    d.enemy = f.req<std::string>("enemy");
    d.minLevel = f.req<int>("minLevel", 0, kMaxStat);
    d.repeatable = f.req<bool>("repeatable");
    d.story = f.strings("story", false);
    return d;
}

RecipeDef parseRecipe(Ctx& ctx, const Json& j, std::size_t index) {
    Fields f(ctx, j, entryLabel("recipe", j, index), {"id", "result", "ingredients"});
    RecipeDef d;
    d.id = f.req<std::string>("id");
    d.result = f.req<std::string>("result");
    const Json* list = f.raw("ingredients");
    if (!list || !list->is_array()) {
        f.fail("key 'ingredients' must be an array");
        return d;
    }
    for (std::size_t i = 0; i < list->size(); ++i) {
        Fields inf(ctx, (*list)[i], f.entry(), {"item", "count"},
                   std::format("ingredients[{}].", i));
        auto item = inf.req<std::string>("item");
        d.ingredients.emplace_back(std::move(item), inf.req<int>("count", 1, kMaxStat));
    }
    return d;
}

StoryDef loadStory(Ctx& ctx, const std::filesystem::path& dir, bool& fileOk) {
    StoryDef s;
    auto doc = loadFile(ctx, dir);
    fileOk = doc.has_value();
    if (!doc)
        return s;
    Fields f(ctx, *doc, "", {"title", "opening", "pathIntro"});
    s.title = f.req<std::string>("title");
    s.opening = f.strings("opening", true);
    s.pathIntro = f.strings("pathIntro", true);
    return s;
}

// Reads one referenced script into `sources` and checks it in `host`: the path stays inside the
// data folder, the file exists, it loads in the sandbox, and it returns a table with `function`.
void checkScript(Ctx& ctx, const std::string& entry, const std::string& path,
                 std::string_view function, const std::filesystem::path& dataDir,
                 script::ScriptHost& host, std::map<std::string, std::string, std::less<>>& sources) {
    auto fail = [&](std::string message) {
        ctx.errors.push_back({ctx.file, entry, std::move(message)});
    };
    const std::filesystem::path relative(path);
    if (path.empty() || relative.is_absolute() || relative.has_root_name() ||
        std::ranges::any_of(relative, [](const auto& part) { return part == ".."; })) {
        fail(std::format("script path '{}' must stay inside the data folder", path));
        return;
    }
    if (!sources.contains(path)) {
        std::ifstream in(dataDir / relative, std::ios::binary);
        if (!in) {
            fail(std::format("script file '{}' not found", path));
            return;
        }
        std::string source((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (auto loaded = host.load(path, source); !loaded) {
            fail(loaded.error());
            return;
        }
        sources.emplace(path, std::move(source));
    }
    if (!host.hasFunction(path, function))
        fail(std::format("script '{}' must return a table with a '{}' function", path, function));
}

void checkItemRef(Ctx& ctx, const Content& c, const std::string& entry, const std::string& what,
                  const std::string& id) {
    if (!c.findItem(id))
        ctx.errors.push_back({ctx.file, entry, std::format("unknown {} '{}'", what, id)});
}

} // namespace

std::string formatError(const ContentError& e) {
    return e.entry.empty() ? std::format("{}: {}", e.file, e.message)
                           : std::format("{}: {}: {}", e.file, e.entry, e.message);
}

std::expected<Content, std::vector<ContentError>>
loadContent(const std::filesystem::path& dataDir) {
    Errors errors;
    bool allFilesRead = true;
    auto load = [&](const char* file, std::string_view arrayKey, auto parseOne) {
        Ctx ctx{errors, file};
        using T = decltype(parseOne(ctx, std::declval<const Json&>(), std::size_t{}));
        auto list = loadList<T>(ctx, dataDir, arrayKey,
                                [&](const Json& j, std::size_t i) { return parseOne(ctx, j, i); });
        allFilesRead = allFilesRead && list.has_value();
        return list.value_or(std::vector<T>{});
    };

    auto items = load("items.json", "items", parseItem);
    auto paths = load("paths.json", "paths", parsePath);
    auto skills = load("skills.json", "skills", parseSkill);
    auto enemies = load("enemies.json", "enemies", parseEnemy);
    auto missions = load("missions.json", "missions", parseMission);
    auto recipes = load("recipes.json", "recipes", parseRecipe);
    bool storyOk = false;
    Ctx storyCtx{errors, "story.json"};
    StoryDef story = loadStory(storyCtx, dataDir, storyOk);
    allFilesRead = allFilesRead && storyOk;

    Ctx itemsCtx{errors, "items.json"}, pathsCtx{errors, "paths.json"},
        skillsCtx{errors, "skills.json"}, enemiesCtx{errors, "enemies.json"},
        missionsCtx{errors, "missions.json"}, recipesCtx{errors, "recipes.json"};
    checkIds(itemsCtx, items, "item");
    checkIds(pathsCtx, paths, "path");
    checkIds(skillsCtx, skills, "skill");
    checkIds(enemiesCtx, enemies, "enemy");
    checkIds(missionsCtx, missions, "mission");
    checkIds(recipesCtx, recipes, "recipe");

    for (const ItemDef& d : items) {
        const std::string entry = std::format("item '{}'", d.id);
        const bool gear = d.kind == ItemKind::Weapon || d.kind == ItemKind::Armor;
        if (gear && !d.upgradeMaterial)
            itemsCtx.errors.push_back(
                {itemsCtx.file, entry, "weapons and armor need 'upgradeMaterial'"});
        if (!gear && d.upgradeMaterial)
            itemsCtx.errors.push_back(
                {itemsCtx.file, entry, "'upgradeMaterial' is only for weapons and armor"});
        if (d.kind == ItemKind::Consumable && !d.consumable)
            itemsCtx.errors.push_back({itemsCtx.file, entry, "consumables need 'consumable'"});
        if (d.kind != ItemKind::Consumable && d.consumable)
            itemsCtx.errors.push_back(
                {itemsCtx.file, entry, "'consumable' is only for consumables"});
    }
    for (const char* required : {"fighter", "hunter", "rogue"}) {
        if (allFilesRead &&
            std::ranges::none_of(paths, [&](const PathDef& p) { return p.id == required; }))
            pathsCtx.errors.push_back(
                {pathsCtx.file, "", std::format("missing required path '{}'", required)});
    }

    // Cross-references are checked only when every file was read, to avoid cascading noise.
    std::map<std::string, std::string, std::less<>> scripts;
    if (allFilesRead) {
        script::ScriptHost host;
        for (const SkillDef& d : skills)
            if (d.script)
                checkScript(skillsCtx, std::format("skill '{}'", d.id), *d.script, "cast", dataDir,
                            host, scripts);
        for (const EnemyDef& d : enemies)
            if (d.script)
                checkScript(enemiesCtx, std::format("enemy '{}'", d.id), *d.script, "take_turn",
                            dataDir, host, scripts);
        Content refs =
            ContentBuilder::assemble(items, paths, skills, enemies, missions, recipes, story);
        for (const ItemDef& d : items)
            if (d.upgradeMaterial)
                checkItemRef(itemsCtx, refs, std::format("item '{}'", d.id), "upgrade material",
                             *d.upgradeMaterial);
        for (const SkillDef& d : skills)
            checkItemRef(skillsCtx, refs, std::format("skill '{}'", d.id), "key item", d.keyItem);
        const auto spriteDir = (dataDir / ".." / "assets" / "sprites").lexically_normal();
        for (const EnemyDef& d : enemies) {
            const std::string entry = std::format("enemy '{}'", d.id);
            for (const std::string& reward : d.rewardItems)
                checkItemRef(enemiesCtx, refs, entry, "reward item", reward);
            if (!std::filesystem::exists(spriteDir / d.sprite))
                enemiesCtx.errors.push_back(
                    {enemiesCtx.file, entry,
                     std::format("sprite file '{}' not found in assets/sprites", d.sprite)});
        }
        for (const MissionDef& d : missions)
            if (!refs.findEnemy(d.enemy))
                missionsCtx.errors.push_back({missionsCtx.file, std::format("mission '{}'", d.id),
                                              std::format("unknown enemy '{}'", d.enemy)});
        for (const RecipeDef& d : recipes) {
            const std::string entry = std::format("recipe '{}'", d.id);
            checkItemRef(recipesCtx, refs, entry, "result item", d.result);
            std::unordered_set<std::string> seen;
            for (const auto& [item, count] : d.ingredients) {
                checkItemRef(recipesCtx, refs, entry, "ingredient", item);
                // forge() checks each entry's count separately, so one item must be one entry.
                if (!seen.insert(item).second)
                    recipesCtx.errors.push_back(
                        {recipesCtx.file, entry,
                         std::format("ingredient '{}' listed twice", item)});
            }
        }
    }

    if (!errors.empty())
        return std::unexpected(std::move(errors));
    return ContentBuilder::assemble(std::move(items), std::move(paths), std::move(skills),
                                    std::move(enemies), std::move(missions), std::move(recipes),
                                    std::move(story), std::move(scripts));
}

} // namespace heroes::game
