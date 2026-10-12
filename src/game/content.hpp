#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace heroes::game {

// Upper bounds for loaded numbers (content and saves). They keep every battle and town formula
// inside `int`/`int64` range: e.g. 5x ATK with maxed base stat and two +10 items.
inline constexpr int kMaxStat = 1'000'000;
inline constexpr int kMaxPathMultiplier = 100;
inline constexpr int kMaxSkillMultiplier = 10;

enum class ItemKind { Weapon, Armor, SkillKey, Material, Consumable };

struct StatBonus {
    int atk = 0, def = 0, life = 0;
};

struct ConsumableEffect {
    int restoreHp = 0;
    int restoreMp = 0;
    bool fullRestore = false;
};

struct ItemDef {
    std::string id, name, description;
    ItemKind kind;
    StatBonus bonus;
    std::int64_t price = 0, auctionValue = 0;
    int dropRate = 0;
    std::optional<std::string> upgradeMaterial; // weapons and armor only
    std::optional<ConsumableEffect> consumable; // consumables only
};

struct PathDef {
    std::string id, name, flavor;
    int multiplier, maxHp, maxMp, atk, def, life;
};

// Guard halves damage taken; Charging does nothing itself: a boss script sets it a turn before a
// strong action, as a visible warning.
enum class StatusId { Stun, DefenseDown, Reflect, Guard, Charging };

struct StatusSpec {
    StatusId id;
    int turns;
};

struct SkillEffect {
    bool healToFull = false;
    int atkMultiplier = 0;
    bool ignoreDef = false;
    bool halveTargetHpFirst = false;
    int healPercentOfDamage = 0;
    std::vector<StatusSpec> selfStatuses, targetStatuses;
    bool requiresTargetNotStunned = false;
};

struct SkillDef {
    std::string id, name, description;
    int mpCost, learnLevel;
    std::int64_t goldCost;
    std::string keyItem;
    SkillEffect effect;                // when there is no script
    std::optional<std::string> script; // a Lua file under the data dir, instead of `effect`
};

enum class EnemyBehavior { Basic }; // a plain attack every turn; also the fallback for a failed scriptr

struct EnemyDef {
    std::string id, name, sprite;
    int maxHp, atk, def, life;
    EnemyBehavior behavior = EnemyBehavior::Basic; // when there is no script
    std::optional<std::string> script; // a Lua file under the data dir, instead of `behavior`
    std::int64_t rewardExp, rewardGold;
    std::vector<std::string> rewardItems;
};

struct MissionDef {
    std::string id, name, location, enemy;
    int minLevel;
    bool repeatable;
    std::vector<std::string> story;
};

struct RecipeDef {
    std::string id, result;
    std::vector<std::pair<std::string, int>> ingredients;
};

struct StoryDef {
    std::string title;
    std::vector<std::string> opening, pathIntro;
};

// An active mod, as saves record it.
struct ModRef {
    std::string name, version;
    bool operator==(const ModRef&) const = default;
};

// Immutable, validated game data. Built only by loadContent().
class Content {
  public:
    // item()/path()/... require that the id exists; loadContent() validates every stored reference.
    const ItemDef& item(std::string_view id) const { return items_.get(id); }
    const ItemDef* findItem(std::string_view id) const { return items_.find(id); }
    std::span<const ItemDef> items() const { return items_.all; }

    const PathDef& path(std::string_view id) const { return paths_.get(id); }
    const PathDef* findPath(std::string_view id) const { return paths_.find(id); }
    std::span<const PathDef> paths() const { return paths_.all; }

    const SkillDef& skill(std::string_view id) const { return skills_.get(id); }
    const SkillDef* findSkill(std::string_view id) const { return skills_.find(id); }
    std::span<const SkillDef> skills() const { return skills_.all; }

    const EnemyDef& enemy(std::string_view id) const { return enemies_.get(id); }
    const EnemyDef* findEnemy(std::string_view id) const { return enemies_.find(id); }
    std::span<const EnemyDef> enemies() const { return enemies_.all; }

    const MissionDef& mission(std::string_view id) const { return missions_.get(id); }
    const MissionDef* findMission(std::string_view id) const { return missions_.find(id); }
    std::span<const MissionDef> missions() const { return missions_.all; }

    const RecipeDef& recipe(std::string_view id) const { return recipes_.get(id); }
    const RecipeDef* findRecipe(std::string_view id) const { return recipes_.find(id); }
    std::span<const RecipeDef> recipes() const { return recipes_.all; }

    const StoryDef& story() const { return story_; }

    // The source of every script referenced by skills and enemies, by its path ("scripts/x.lua").
    const std::map<std::string, std::string, std::less<>>& scripts() const { return scripts_; }
    // The mods merged into this content, in load order.
    const std::vector<ModRef>& mods() const { return mods_; }

  private:
    template <class T> struct Table {
        std::vector<T> all;
        std::unordered_map<std::string, std::size_t> index;

        void build() {
            for (std::size_t i = 0; i < all.size(); ++i)
                index.emplace(all[i].id, i);
        }
        const T* find(std::string_view id) const {
            auto it = index.find(std::string(id));
            return it == index.end() ? nullptr : &all[it->second];
        }
        const T& get(std::string_view id) const { return *find(id); }
    };

    friend struct ContentBuilder;

    Table<ItemDef> items_;
    Table<PathDef> paths_;
    Table<SkillDef> skills_;
    Table<EnemyDef> enemies_;
    Table<MissionDef> missions_;
    Table<RecipeDef> recipes_;
    StoryDef story_;
    std::map<std::string, std::string, std::less<>> scripts_;
    std::vector<ModRef> mods_;
};

} // namespace heroes::game
