#include "game/content_loader.hpp"

#include "game/json_fields.hpp"
#include "game/script/script_host.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <unordered_set>

namespace heroes::game {

// Lets the loader fill a Content, whose tables are otherwise private.
struct ContentBuilder {
    static Content assemble(std::vector<ItemDef> items, std::vector<PathDef> paths,
                            std::vector<SkillDef> skills, std::vector<EnemyDef> enemies,
                            std::vector<MissionDef> missions, std::vector<RecipeDef> recipes,
                            StoryDef story,
                            std::map<std::string, std::string, std::less<>> scripts = {},
                            std::vector<ModRef> mods = {}) {
        Content c;
        c.scripts_ = std::move(scripts);
        c.mods_ = std::move(mods);
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

using namespace detail;

constexpr std::array<std::pair<std::string_view, ItemKind>, 5> kItemKinds{{
    {"weapon", ItemKind::Weapon},
    {"armor", ItemKind::Armor},
    {"skill_key", ItemKind::SkillKey},
    {"material", ItemKind::Material},
    {"consumable", ItemKind::Consumable},
}};
constexpr std::array<std::pair<std::string_view, StatusId>, 5> kStatuses{{
    {"stun", StatusId::Stun},
    {"defense_down", StatusId::DefenseDown},
    {"reflect", StatusId::Reflect},
    {"guard", StatusId::Guard},
    {"charging", StatusId::Charging},
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

// Whether a script path written in content stays inside its folder: relative, no "..".
bool containedPath(const std::string& path) {
    const std::filesystem::path relative(path);
    return !path.empty() && !relative.is_absolute() && !relative.has_root_name() &&
           std::ranges::none_of(relative, [](const auto& part) { return part == ".."; });
}

using Sources = std::map<std::string, std::string, std::less<>>;

// Reads one referenced script (named `key`, stored at `file`) into `sources` and checks it in
// `host`: it exists, loads in the sandbox, and returns a table with `function`.
void checkScript(Ctx& ctx, const std::string& entry, const std::string& key,
                 const std::filesystem::path& file, std::string_view function,
                 script::ScriptHost& host, Sources& sources) {
    auto fail = [&](std::string message) {
        ctx.errors.push_back({ctx.file, entry, std::move(message)});
    };
    if (!sources.contains(key)) {
        std::ifstream in(file, std::ios::binary);
        if (!in) {
            fail(std::format("script file '{}' not found", key));
            return;
        }
        std::string source((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (auto loaded = host.load(key, source); !loaded) {
            fail(loaded.error());
            return;
        }
        sources.emplace(key, std::move(source));
    }
    if (!host.hasFunction(key, function))
        fail(std::format("script '{}' must return a table with a '{}' function", key, function));
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

namespace {

// Everything one layer of content defines: the base data folder, or one mod. Paths and story
// come only from the base.
struct Layer {
    std::vector<ItemDef> items;
    std::vector<SkillDef> skills;
    std::vector<EnemyDef> enemies;
    std::vector<MissionDef> missions;
    std::vector<RecipeDef> recipes;
};

struct Base {
    Layer layer;
    std::vector<PathDef> paths;
    StoryDef story;
};

// Where each script key's file lives: base keys are paths under the data folder; mod keys are
// "mods/<name>/<path>" and live in the mod's folder.
using ScriptFiles = std::map<std::string, std::filesystem::path, std::less<>>;

// Merged content, checked: ids, kinds, required paths, cross references, sprites and scripts.
// Returns every error; on success fills `sources` with each referenced script.
Errors validate(const std::filesystem::path& dataDir, const Base& base, const Layer& merged,
                const ScriptFiles& scriptFiles, Sources& sources) {
    Errors errors;
    Ctx itemsCtx{errors, "items.json"}, pathsCtx{errors, "paths.json"},
        skillsCtx{errors, "skills.json"}, enemiesCtx{errors, "enemies.json"},
        missionsCtx{errors, "missions.json"}, recipesCtx{errors, "recipes.json"};
    const auto& [items, skills, enemies, missions, recipes] = merged;
    const auto& paths = base.paths;
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
    for (const char* required : {"fighter", "hunter", "rogue"})
        if (std::ranges::none_of(paths, [&](const PathDef& p) { return p.id == required; }))
            pathsCtx.errors.push_back(
                {pathsCtx.file, "", std::format("missing required path '{}'", required)});

    const Content refs =
        ContentBuilder::assemble(items, paths, skills, enemies, missions, recipes, base.story);
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
                    {recipesCtx.file, entry, std::format("ingredient '{}' listed twice", item)});
        }
    }

    script::ScriptHost host;
    const auto checkScriptOf = [&](Ctx& ctx, const std::string& entry, const std::string& key,
                                   std::string_view function) {
        if (const auto file = scriptFiles.find(key); file != scriptFiles.end()) {
            checkScript(ctx, entry, key, file->second, function, host, sources);
        } else if (!containedPath(key)) { // a base path: relative to the data folder
            ctx.errors.push_back(
                {ctx.file, entry,
                 std::format("script path '{}' must stay inside the data folder", key)});
        } else {
            checkScript(ctx, entry, key, dataDir / key, function, host, sources);
        }
    };
    for (const SkillDef& d : skills)
        if (d.script)
            checkScriptOf(skillsCtx, std::format("skill '{}'", d.id), *d.script, "cast");
    for (const EnemyDef& d : enemies)
        if (d.script)
            checkScriptOf(enemiesCtx, std::format("enemy '{}'", d.id), *d.script, "take_turn");
    return errors;
}

// ---- Mods ----------------------------------------------------------------------------------

struct Mod {
    ModRef ref;
    int priority = 0;
    Layer layer;
    ScriptFiles scriptFiles;
};

bool validModName(std::string_view name) {
    return !name.empty() && name.size() <= 40 &&
           std::ranges::all_of(name, [](char c) {
               return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
           });
}

// Reads one mod folder. Any problem (manifest, JSON, keys, a file mods can't change, a script
// path leaving the folder) is returned as errors; the caller then skips the whole mod.
std::expected<Mod, Errors> readMod(const std::filesystem::path& folder) {
    const std::string folderName = folder.filename().string();
    const std::string prefix = std::format("mods/{}/", folderName);
    Errors errors;
    Mod mod;
    {
        Ctx ctx{errors, "mod.json"};
        if (auto doc = loadFile(ctx, folder)) {
            Fields f(ctx, *doc, "", {"name", "version", "priority", "description"});
            mod.ref.name = f.req<std::string>("name");
            mod.ref.version = f.req<std::string>("version");
            mod.priority = f.opt<int>("priority", 0, -1'000'000, 1'000'000);
            (void)f.opt<std::string>("description", "");
            if (errors.empty()) {
                if (!validModName(mod.ref.name))
                    f.fail("name must match [a-z0-9-] and be 1-40 characters");
                else if (mod.ref.name != folderName)
                    f.fail(std::format("name '{}' must match the folder name '{}'", mod.ref.name,
                                       folderName));
                else if (mod.ref.version.empty() || mod.ref.version.size() > 20)
                    f.fail("version must be 1-20 characters");
            }
        }
    }
    for (const char* fixed : {"paths.json", "story.json", "dungeon.json"})
        if (std::filesystem::exists(folder / fixed))
            errors.push_back({fixed, "", "mods can't change this file"});
    const auto load = [&](const char* file, std::string_view key, std::string_view noun,
                          auto parseOne, auto& out) {
        if (!std::filesystem::exists(folder / file))
            return; // a mod only has the files it changes
        Ctx ctx{errors, file};
        auto list = loadList<std::decay_t<decltype(out[0])>>(
            ctx, folder, key, [&](const Json& j, std::size_t i) { return parseOne(ctx, j, i); });
        if (list)
            out = std::move(*list);
        checkIds(ctx, out, noun);
    };
    load("items.json", "items", "item", parseItem, mod.layer.items);
    load("skills.json", "skills", "skill", parseSkill, mod.layer.skills);
    load("enemies.json", "enemies", "enemy", parseEnemy, mod.layer.enemies);
    load("missions.json", "missions", "mission", parseMission, mod.layer.missions);
    load("recipes.json", "recipes", "recipe", parseRecipe, mod.layer.recipes);

    // Script paths are relative to the mod folder and must stay inside it.
    const auto rehome = [&](std::optional<std::string>& script, const char* file,
                            const std::string& entry) {
        if (!script)
            return;
        if (!containedPath(*script)) {
            errors.push_back({file, entry,
                              std::format("script path '{}' must stay inside the mod folder",
                                          *script)});
            return;
        }
        const std::string key = prefix + *script;
        mod.scriptFiles.emplace(key, folder / *script);
        script = key;
    };
    for (SkillDef& d : mod.layer.skills)
        rehome(d.script, "skills.json", std::format("skill '{}'", d.id));
    for (EnemyDef& d : mod.layer.enemies)
        rehome(d.script, "enemies.json", std::format("enemy '{}'", d.id));

    if (!errors.empty()) {
        for (ContentError& e : errors)
            e.file = prefix + e.file;
        return std::unexpected(std::move(errors));
    }
    return mod;
}

// Which mod each merged entry came from, keyed "<file>|<entry label>".
using Origins = std::map<std::string, std::string, std::less<>>;

template <class T>
void mergeList(std::vector<T>& into, const std::vector<T>& from, const char* file,
               std::string_view noun, const std::string& mod, Origins& origins,
               std::vector<std::string>* overrides) {
    for (const T& entry : from) {
        const auto existing = std::ranges::find(into, entry.id, &T::id);
        if (existing != into.end()) {
            *existing = entry;
            if (overrides)
                overrides->push_back(std::format("mod '{}': overrides {}/{}", mod,
                                                 std::string_view(file).substr(
                                                     0, std::string_view(file).size() - 5),
                                                 entry.id));
        } else {
            into.push_back(entry);
        }
        origins.insert_or_assign(std::format("{}|{} '{}'", file, noun, entry.id), mod);
    }
}

Layer merge(const Layer& base, const std::vector<Mod>& mods, Origins& origins,
            std::vector<std::string>* overrides) {
    Layer out = base;
    for (const Mod& m : mods) {
        const std::string& name = m.ref.name;
        mergeList(out.items, m.layer.items, "items.json", "item", name, origins, overrides);
        mergeList(out.skills, m.layer.skills, "skills.json", "skill", name, origins, overrides);
        mergeList(out.enemies, m.layer.enemies, "enemies.json", "enemy", name, origins,
                  overrides);
        mergeList(out.missions, m.layer.missions, "missions.json", "mission", name, origins,
                  overrides);
        mergeList(out.recipes, m.layer.recipes, "recipes.json", "recipe", name, origins,
                  overrides);
    }
    return out;
}

std::string skipLine(const std::string& mod, const Errors& errors) {
    std::string line = std::format("mod '{}' skipped: {}", mod, formatError(errors.front()));
    if (errors.size() > 1)
        line += std::format(" (and {} more)", errors.size() - 1);
    return line;
}

} // namespace

std::expected<Content, std::vector<ContentError>>
loadContent(const std::filesystem::path& dataDir) {
    return loadContent(dataDir, std::nullopt, nullptr);
}

std::expected<Content, std::vector<ContentError>>
loadContent(const std::filesystem::path& dataDir, const std::optional<std::filesystem::path>& modsDir,
            std::vector<std::string>* modLog) {
    // 1. The base content, which must be valid on its own (as before mods existed).
    Errors errors;
    bool allFilesRead = true;
    Base base;
    const auto load = [&](const char* file, std::string_view arrayKey, auto parseOne, auto& out) {
        Ctx ctx{errors, file};
        auto list = loadList<std::decay_t<decltype(out[0])>>(
            ctx, dataDir, arrayKey, [&](const Json& j, std::size_t i) { return parseOne(ctx, j, i); });
        allFilesRead = allFilesRead && list.has_value();
        if (list)
            out = std::move(*list);
    };
    load("items.json", "items", parseItem, base.layer.items);
    load("paths.json", "paths", parsePath, base.paths);
    load("skills.json", "skills", parseSkill, base.layer.skills);
    load("enemies.json", "enemies", parseEnemy, base.layer.enemies);
    load("missions.json", "missions", parseMission, base.layer.missions);
    load("recipes.json", "recipes", parseRecipe, base.layer.recipes);
    bool storyOk = false;
    Ctx storyCtx{errors, "story.json"};
    base.story = loadStory(storyCtx, dataDir, storyOk);
    allFilesRead = allFilesRead && storyOk;
    // Cross-references are checked only when every file was read, to avoid cascading noise.
    Sources sources;
    if (allFilesRead) {
        Errors more = validate(dataDir, base, base.layer, {}, sources);
        errors.insert(errors.end(), more.begin(), more.end());
    }
    if (!errors.empty())
        return std::unexpected(std::move(errors));

    // 2. Mods: each subfolder with a mod.json, by priority then folder name. A mod that can't be
    //    read is skipped at once; one that breaks the merged content is skipped and the rest are
    //    merged again, until what remains is valid.
    std::vector<Mod> active;
    std::error_code ec;
    if (modsDir && std::filesystem::is_directory(*modsDir, ec)) {
        std::vector<std::filesystem::path> folders;
        for (const auto& entry : std::filesystem::directory_iterator(*modsDir, ec))
            if (entry.is_directory() && std::filesystem::exists(entry.path() / "mod.json"))
                folders.push_back(entry.path());
        std::ranges::sort(folders);
        for (const std::filesystem::path& folder : folders) {
            auto mod = readMod(folder);
            if (mod)
                active.push_back(std::move(*mod));
            else if (modLog)
                modLog->push_back(skipLine(folder.filename().string(), mod.error()));
        }
        std::ranges::stable_sort(active, {}, &Mod::priority);
    }
    Layer merged = base.layer;
    for (;;) {
        Origins origins;
        merged = merge(base.layer, active, origins, nullptr);
        ScriptFiles scriptFiles;
        for (const Mod& m : active)
            scriptFiles.insert(m.scriptFiles.begin(), m.scriptFiles.end());
        sources.clear();
        Errors found = validate(dataDir, base, merged, scriptFiles, sources);
        if (found.empty())
            break;
        // Blame the mod whose entry each error is about; the base alone was valid.
        std::map<std::string, Errors, std::less<>> blamed;
        for (ContentError& e : found) {
            const auto origin = origins.find(std::format("{}|{}", e.file, e.entry));
            const std::string mod = origin != origins.end() ? origin->second
                                                            : active.back().ref.name;
            e.file = std::format("mods/{}/{}", mod, e.file);
            blamed[mod].push_back(std::move(e));
        }
        for (const auto& [mod, modErrors] : blamed) {
            if (modLog)
                modLog->push_back(skipLine(mod, modErrors));
            std::erase_if(active, [&](const Mod& m) { return m.ref.name == mod; });
        }
    }

    // 3. Report what was applied, then assemble.
    std::vector<ModRef> mods;
    if (modLog) {
        Origins unused;
        (void)merge(base.layer, active, unused, modLog); // just for the override lines
    }
    for (const Mod& m : active) {
        if (modLog)
            modLog->push_back(std::format("mod '{}' {} loaded", m.ref.name, m.ref.version));
        mods.push_back(m.ref);
    }
    return ContentBuilder::assemble(std::move(merged.items), std::move(base.paths),
                                    std::move(merged.skills), std::move(merged.enemies),
                                    std::move(merged.missions), std::move(merged.recipes),
                                    std::move(base.story), std::move(sources), std::move(mods));
}

} // namespace heroes::game
