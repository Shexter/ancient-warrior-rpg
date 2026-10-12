# Interfaces and Exact Rules

Implementation reference for `build-core-engine`. Signatures here are the intended public surface. Private helpers are up to the implementer. If something here conflicts with a spec, the spec wins: report it as SPEC DRIFT, don't silently pick one.

---

## 1. Conventions

- C++23. Namespaces: `heroes::game`, `heroes::engine`, `heroes::platform`, `heroes::app`. Headers use `#pragma once`.
- Names: types `PascalCase`, functions and variables `camelCase`, private members `trailing_`, constants `kPascalCase`, enumerators `PascalCase`, files `snake_case.cpp/.hpp`.
- **No floating point in game logic** (`src/game/`). All game math is integer, and division truncates toward zero. Floating point is allowed only in animation and timing code (`src/app/`, `src/engine/loop`).
- Game numbers are `int` (stats, HP, MP, quantities) and `std::int64_t` (gold, exp).
- Errors: `std::expected<T, E>` for anything that can fail on player or file input. Exceptions are caught only at the JSON parsing boundary and converted. Use no `assert` for input validation (asserts are for programmer errors only).
- Game logic never prints, sleeps, reads input, or touches the terminal.
- Don't use `std::print` (GCC 13 on the Ubuntu CI runner lacks it). Use `std::format` and write the string.
- Don't use `std::rand`, `<random>` engines or distributions, or `std::random_device` anywhere except the one call that seeds a new game (§4).
- Format with the repo `.clang-format`: `BasedOnStyle: LLVM`, `IndentWidth: 4`, `ColumnLimit: 100`, `PointerAlignment: Left`.

## 2. Source tree and CMake targets

```
CMakeLists.txt  CMakePresets.json  .clang-format  .gitignore
.github/workflows/ci.yml
cmake/Dependencies.cmake        # all FetchContent declarations
cmake/Warnings.cmake            # heroes_warnings INTERFACE target
data/  paths.json items.json skills.json enemies.json missions.json recipes.json story.json
assets/sprites/  hero_fighter.sprite hero_hunter.sprite hero_rogue.sprite medusa.sprite monster.sprite
src/game/      errors.hpp (TownError + describe) rng.{hpp,cpp} content.{hpp,cpp} content_loader.{hpp,cpp}
               inventory.{hpp,cpp} hero.{hpp,cpp} battle.{hpp,cpp} events.hpp
               training.{hpp,cpp} town.{hpp,cpp} save.{hpp,cpp} game_state.hpp
src/engine/    color.{hpp,cpp} framebuffer.{hpp,cpp} renderer.{hpp,cpp} sprite.{hpp,cpp}
               input.{hpp,cpp} loop.{hpp,cpp}
src/platform/  terminal.hpp terminal_posix.cpp terminal_windows.cpp paths.{hpp,cpp}
src/app/       main.cpp app.{hpp,cpp} screen.hpp widgets.{hpp,cpp} screens/*.{hpp,cpp} battle_view.{hpp,cpp}
tools/sim/main.cpp  tools/sim/sim.{hpp,cpp} (SimConfig, Report, runSimulation, policies)
tools/sprite_bake/main.cpp  tools/sprite_bake/third_party/stb_image.h
tests/  <module>_test.cpp …  tests/fixtures/…
docs/benchmarks.md
```

| target | type | sources | links |
|---|---|---|---|
| `heroes_game` | static lib | `src/game/*` | nlohmann_json |
| `heroes_engine` | static lib | `src/engine/*` | — |
| `heroes_platform` | static lib | `src/platform/terminal_posix.cpp` or `terminal_windows.cpp` (picked by `if(WIN32)`), `paths.cpp` | — |
| `heroes` | executable | `src/app/*` | heroes_game, heroes_engine, heroes_platform |
| `heroes_simlib` | static lib | `tools/sim/sim.cpp` | heroes_game |
| `heroes-sim` | executable | `tools/sim/main.cpp` | heroes_simlib |
| `sprite_bake` | executable | `tools/sprite_bake/*` | heroes_engine |
| `heroes_tests` | executable | `tests/*` | heroes_game, heroes_engine, heroes_platform, heroes_simlib, GTest::gtest_main |

All targets link `heroes_warnings`. Tests are registered with `gtest_discover_tests(heroes_tests)`. Compile definition on `heroes` and `heroes_tests`: `HEROES_SOURCE_DIR="${PROJECT_SOURCE_DIR}"`, used to locate `data/` and `assets/` in dev builds (§11).

## 3. Build, dependencies, CI

- `cmake_minimum_required(VERSION 3.25)` (needed for `FetchContent_Declare(... SYSTEM)`). The local install is Homebrew CMake 4.x.
- `cmake/Dependencies.cmake`:
  - nlohmann/json **v3.12.0**: `URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz`, `SYSTEM`.
  - GoogleTest **v1.17.0**: `URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip`, `SYSTEM`. Set `gtest_force_shared_crt ON` before `FetchContent_MakeAvailable`. Also set `INSTALL_GTEST OFF` and `BUILD_GMOCK OFF`.
  - `URL_HASH SHA256=` values (computed during planning on 2026-10-10 by downloading each URL):
    - json.tar.xz `42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa`
    - googletest v1.17.0.zip `40d4ec942217dcc84a9ebe2a68584ada7d4a33a8ee958755763278ea1c5e18ff`
    - If CMake reports a hash mismatch, **stop and report it**. Don't replace the hash: a changed release archive needs a human look.
- stb_image: copy `stb_image.h` from `https://raw.githubusercontent.com/nothings/stb/2c980bb59875b0d32144a71867fbdebb2f77cd20/stb_image.h` into `tools/sprite_bake/third_party/` and commit it. It's public domain/MIT with the license text inside the file. Expected SHA-256: `594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3`.
- `cmake/Warnings.cmake`: GCC/Clang `-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wimplicit-fallthrough`. MSVC `/W4 /permissive- /utf-8`. Option `HEROES_WERROR` (default OFF) adds `-Werror` or `/WX`.
- `CMakePresets.json` (version 6). Configure presets all inherit a hidden `base` with `binaryDir: ${sourceDir}/build/${presetName}`, and none sets a generator:
  - `debug`: `CMAKE_BUILD_TYPE=Debug`.
  - `release`: `CMAKE_BUILD_TYPE=Release`.
  - `asan`: Debug plus `CMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer` and the same in `CMAKE_EXE_LINKER_FLAGS`. Not for MSVC.
  - `ci`: Release plus `HEROES_WERROR=ON`.
  - Build presets with the same names. Each has `configuration` set (Debug or Release) so multi-config MSVC works. Test presets with the same names, `output.outputOnFailure: true`.
- `.github/workflows/ci.yml`:
  - Trigger on push and pull_request.
  - Job `build`: matrix `os: [ubuntu-24.04, macos-15, windows-2025]`. Steps: checkout, `cmake --preset ci`, `cmake --build --preset ci`, `ctest --preset ci`.
  - Job `asan`: `ubuntu-24.04`, `cmake --preset asan`, build, `ctest --preset asan`.
  - Cache `build/_deps` keyed on the hash of `cmake/Dependencies.cmake`.

## 4. RNG (`src/game/rng.hpp`)

```cpp
namespace heroes::game {
class Rng {
public:
    struct State { std::uint64_t state; std::uint64_t inc; };
    static Rng stream(std::uint64_t baseSeed, std::uint64_t index);  // §4 derivation
    static Rng fromState(State s);
    State state() const;
    std::uint32_t next();                       // PCG32 XSH-RR
    std::uint32_t below(std::uint32_t bound);   // uniform in [0, bound); bound > 0
    bool percent(std::uint32_t p);              // below(100) < p
    Rng fork();                                 // stream((u64(next()) << 32) | next(), 0)
};
}
```

Exact algorithms. These must match bit for bit.

```
splitmix64(uint64& s): s += 0x9E3779B97F4A7C15; z = s;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9; z = (z ^ (z >> 27)) * 0x94D049BB133111EB; return z ^ (z >> 31)

PCG32 seed(initstate, initseq): state = 0; inc = (initseq << 1) | 1; next(); state += initstate; next()
PCG32 next(): old = state; state = old * 6364136223846793005 + inc;
    x = uint32(((old >> 18) ^ old) >> 27); r = uint32(old >> 59); return (x >> r) | (x << ((-r) & 31))

below(b) — Lemire, unbiased:
    x = next(); m = uint64(x) * b; l = uint32(m);
    if (l < b) { t = (0u - b) % b; while (l < t) { x = next(); m = uint64(x) * b; l = uint32(m); } }
    return uint32(m >> 32)

stream(base, index): s = base ^ index; a = splitmix64(s); b = splitmix64(s); seed(a, b)
```

Golden vectors for `tests/rng_test.cpp`. They were computed with the reference implementation and match the published SplitMix64 and PCG32 demo outputs:

| input | expected |
|---|---|
| splitmix64 from state 0, first 3 | `0xe220a8397b1dcdaf 0x6e789e6aa1b965f4 0x06c45d188009454f` |
| PCG32 `seed(42, 54)`, first 6 `next()` | `0xa15c02b7 0x7b47f409 0xba1d3330 0x83d2f293 0xbfa4784b 0xcbed606e` |
| `stream(42, 0)`, first 5 `next()` | `0xd11dd51f 0xa9b04c45 0xb5d97aa9 0xa9eab6ce 0xf63fd201` |
| `stream(42, 0)`, 10 × `below(100)` | `81 66 71 66 96 61 84 94 98 53` |
| `stream(42, 1)`, 10 × `below(10)` | `3 2 7 9 9 3 6 2 0 5` |
| `stream(7, 123456)`, 10 × `below(6)` | `0 0 1 4 1 1 1 2 3 3` |

How RNG is used:
- `GameState::rng` is the master stream. A new game seeds it with `Rng::stream(seed, 0)`, where `seed` comes from `--seed N` or one `std::random_device` call. It is persisted in saves.
- Every battle, training session, forge, and auction calls `state.rng.fork()` once and uses only the fork. Each therefore advances the master by exactly 2 draws, whatever happens inside.
- The simulator uses `Rng::stream(baseSeed, battleIndex)` for battle `battleIndex`.
- RNG draws happen only at the points listed in §7–§9, in the listed order. Never draw speculatively.

## 5. Content (`content.hpp`, `content_loader.hpp`)

```cpp
enum class ItemKind { Weapon, Armor, SkillKey, Material, Consumable };
struct StatBonus { int atk = 0, def = 0, life = 0; };
struct ConsumableEffect { int restoreHp = 0; int restoreMp = 0; bool fullRestore = false; };
struct ItemDef { std::string id, name, description; ItemKind kind; StatBonus bonus;
                 std::int64_t price = 0, auctionValue = 0; int dropRate = 0;
                 std::optional<std::string> upgradeMaterial;     // weapons and armor only
                 std::optional<ConsumableEffect> consumable; };  // consumables only
struct PathDef { std::string id, name, flavor; int multiplier, maxHp, maxMp, atk, def, life; };
enum class StatusId { Stun, DefenseDown, Reflect };
struct StatusSpec { StatusId id; int turns; };
struct SkillEffect { bool healToFull = false; int atkMultiplier = 0; bool ignoreDef = false;
                     bool halveTargetHpFirst = false; int healPercentOfDamage = 0;
                     std::vector<StatusSpec> selfStatuses, targetStatuses;
                     bool requiresTargetNotStunned = false; };
struct SkillDef { std::string id, name, description; int mpCost, learnLevel;
                  std::int64_t goldCost; std::string keyItem; SkillEffect effect; };
enum class EnemyBehavior { MedusaPattern };  // the only behavior in core; Lua replaces this later
struct EnemyDef { std::string id, name, sprite; int maxHp, atk, def, life; EnemyBehavior behavior;
                  std::int64_t rewardExp, rewardGold; std::vector<std::string> rewardItems; };
struct MissionDef { std::string id, name, location, enemy; int minLevel; bool repeatable;
                    std::vector<std::string> story; };
struct RecipeDef { std::string id, result; std::vector<std::pair<std::string, int>> ingredients; };
struct StoryDef { std::string title; std::vector<std::string> opening, pathIntro; };

class Content {
public:
    const ItemDef& item(std::string_view id) const;     // precondition: id exists (validated at load)
    const ItemDef* findItem(std::string_view id) const; // nullptr if unknown
    // same pair for path, skill, enemy, mission, recipe
    std::span<const ItemDef> items() const;             // in file order; likewise for others
    const StoryDef& story() const;
};
struct ContentError { std::string file; std::string entry; std::string message; };
std::expected<Content, std::vector<ContentError>> loadContent(const std::filesystem::path& dataDir);
std::string formatError(const ContentError&);  // "items.json: item 'foo': unknown upgrade material 'bar'"
```

JSON layout: each file is an object with one array, for example `{"items": [ ... ]}`. Keys are camelCase and match the struct fields. Enums are snake_case strings (`"weapon"`, `"skill_key"`, `"defense_down"`, `"medusa_pattern"`).

```json
{"items": [
  {"id": "iron_sword", "name": "Iron Sword", "description": "Sharp and Light Sword", "kind": "weapon",
   "bonus": {"atk": 50, "def": 0, "life": 0}, "price": 1000, "auctionValue": 0, "dropRate": 30,
   "upgradeMaterial": "fire_dust"},
  {"id": "health_potion", "name": "Health Potion", "description": "Restore Health", "kind": "consumable",
   "price": 1000, "auctionValue": 0, "dropRate": 50, "consumable": {"restoreHp": 100}}
]}
{"skills": [
  {"id": "frozen_throne", "name": "FROZEN Throne", "description": "Deep Freeze and Lower Defense",
   "mpCost": 40, "learnLevel": 15, "goldCost": 20000, "keyItem": "broken_staff",
   "effect": {"atkMultiplier": 1, "targetStatuses": [{"id": "stun", "turns": 3}, {"id": "defense_down", "turns": 3}],
              "requiresTargetNotStunned": true}}
]}
```

Validation rules. Collect **all** errors, then fail:
- Malformed JSON: `file` + nlohmann's message, which includes line and column.
- Unknown keys are an error (catches typos). Missing required keys are an error. Optional keys default as in the structs.
- Duplicate `id` within a file. IDs must match `^[a-z][a-z0-9_]*$`.
- Negative numbers in any stat, price, or cost field. `dropRate` outside 0–100. `multiplier` < 1. `mpCost` < 0. Status `turns` < 1.
- Unknown references: `upgradeMaterial`, `keyItem`, `rewardItems[]`, mission `enemy`, recipe `result` and ingredients, enemy `sprite` file missing under `assets/sprites/`.
- Kind consistency: `upgradeMaterial` only on weapon or armor, and required for both. `consumable` only and always on consumables.
- At least one path, and the `fighter`, `hunter`, `rogue` IDs must exist (hero creation needs them).

## 6. Game state (`game_state.hpp`, `hero.hpp`, `inventory.hpp`)

```cpp
struct Equipped { std::string itemId; int upgradeLevel = 1; };  // 1..10
class Inventory {  // fixed 10 slots, ordered; identical items stack without limit
public:
    static constexpr int kSlots = 10;
    bool canAdd(std::string_view id) const;              // existing stack or empty slot
    std::expected<void, TownError> add(std::string_view id, int qty);      // qty >= 1
    std::expected<void, TownError> remove(std::string_view id, int qty);   // fails if count < qty
    int count(std::string_view id) const;
    std::span<const std::optional<std::pair<std::string, int>>> slots() const;
};
struct Hero { std::string name; std::string pathId; int level = 1; std::int64_t expToNext;
              std::int64_t exp = 0;   // spendable currency; NOT level progress
              std::int64_t gold = 20000;
              int hp, mp, maxHp, maxMp, atk, def, life;   // base stats (Charisma adds here)
              std::optional<Equipped> weapon, armor; Inventory bag; std::optional<Inventory> bank;
              std::set<std::string> learnedSkills; std::set<std::string> completedMissions;
              int forgeLevel = 9; };
struct GameState { Hero hero; Rng rng; std::uint64_t seed; };

std::expected<std::string, TownError> validateHeroName(std::string_view raw); // trim spaces; 1–8 chars of 0x20–0x7E
GameState newGame(const Content&, std::string name, std::string_view pathId, std::uint64_t seed);
int grantExp(Hero&, const Content&, std::int64_t amount);   // adds to exp AND progression; returns levels gained
StatBonus equipmentBonus(const Hero&, const Content&);      // Σ item.bonus × upgradeLevel over weapon+armor
bool canAdventure(const Hero&);                              // weapon && armor && hp > 0
```

`grantExp` progression loop:
```
exp += amount; remaining = amount
while remaining > 0:
    if remaining >= expToNext: remaining -= expToNext; level += 1; expToNext = level × 100 × multiplier
    else: expToNext -= remaining; remaining = 0
```
Test vectors (computed with a script running the loop above), all for a new hero:

| path | exp gained | level | expToNext |
|---|---|---|---|
| Fighter | 10000 | 14 | 500 |
| Rogue | 10000 | 8 | 800 |
| Hunter | 100 | 1 | 100 |
| Hunter | 200 (exact boundary) | 2 | 400 |

## 7. Battle (`battle.hpp`, `events.hpp`)

```cpp
enum class Side { Hero, Enemy };
enum class Outcome { Victory, Defeat, Fled, Draw };
enum class DamageCause { Attack, Skill, Reflect };
namespace ev {
struct BattleStarted { std::string heroName, enemyName, enemyId; };
struct TurnStarted { Side side; int turn; };
struct TurnSkipped { Side side; StatusId reason; };
struct Attacked { Side attacker; };
struct SkillCast { std::string skillId; int mpSpent; };
struct HpHalved { Side target; int hpAfter; };
struct Damaged { Side target; int amount; int hpAfter; DamageCause cause; };  // amount 0 = absorbed
struct Healed { Side target; int amount; int hpAfter; };                       // emitted only if amount > 0
struct StatusApplied { Side target; StatusId status; int turns; };
struct StatusExpired { Side target; StatusId status; };
struct ExtraTurn { Side side; };
struct Fled {};
struct Defeated { Side side; };
struct BattleEnded { Outcome outcome; int turns; };
}
using Event = std::variant<ev::BattleStarted, ev::TurnStarted, ev::TurnSkipped, ev::Attacked, ev::SkillCast,
                           ev::HpHalved, ev::Damaged, ev::Healed, ev::StatusApplied, ev::StatusExpired,
                           ev::ExtraTurn, ev::Fled, ev::Defeated, ev::BattleEnded>;

struct StatusInstance { StatusId id; int turnsRemaining; bool fresh; };
struct Combatant { std::string name; int maxHp, hp, maxMp, mp, atk, def, life;
                   std::vector<StatusInstance> statuses; };
Combatant heroCombatant(const Hero&, const Content&);   // base + equipmentBonus
Combatant enemyCombatant(const EnemyDef&);

struct Attack {}; struct CastSkill { std::string skillId; }; struct Flee {};
using PlayerAction = std::variant<Attack, CastSkill, Flee>;
enum class ActionError { BattleOver, SkillNotLearned, NotEnoughMp, SkillNotReady, UnknownSkill };

class Battle {
public:
    static constexpr int kTurnCap = 1000;
    Battle(const Content&, Combatant hero, const EnemyDef&, std::set<std::string> learnedSkills, Rng rng);
    std::vector<Event> start();                                   // returns {BattleStarted}
    std::expected<std::vector<Event>, ActionError> act(const PlayerAction&);
    bool over() const; Outcome outcome() const;                   // outcome() precondition: over()
    const Combatant& hero() const; const Combatant& enemy() const; int turn() const;
};
```

### Battle turn rules (exact)

`act(action)`:
1. If `over()`: return `BattleOver`. Validate the action before any state change. For `CastSkill`: the ID is unknown → `UnknownSkill`; not learned → `SkillNotLearned`; `hp/mp` short → `NotEnoughMp`; `requiresTargetNotStunned` and the enemy has `stun` → `SkillNotReady`. Return errors with **no** events and **no** state change.
2. `turn += 1`. Emit `TurnStarted{Hero, turn}`.
3. If the hero has `stun`: emit `TurnSkipped{Hero, Stun}` and go to step 5. (Core content never stuns the hero, but the rule is generic.)
4. Resolve the action:
   - `Flee`: emit `Fled`, `BattleEnded{Fled, turn}`, set over, return.
   - `Attack`: emit `Attacked{Hero}`, then `hit(hero→enemy, hero.atk, applyDef=true, Attack)`. If the enemy is alive: heal the hero by `life` (capped), emitting `Healed` if > 0.
   - `CastSkill`: spend MP and emit `SkillCast`. Then, in this order: if `healToFull`, heal to max (`Healed`). If `halveTargetHpFirst`, set `enemy.hp /= 2` (`HpHalved`). If `atkMultiplier > 0`, call `hit(hero→enemy, atkMultiplier × hero.atk, applyDef = !ignoreDef, Skill)`, and if `healPercentOfDamage > 0` heal `dealt × pct / 100` (`Healed`). Apply `selfStatuses`, then `targetStatuses` (`StatusApplied` each).
   - After any damage: if the enemy's HP is 0, emit `Defeated{Enemy}` and `BattleEnded{Victory, turn}`, set over, return.
5. `endTurn(Hero)`.
6. Enemy phase loop:
   1. Emit `TurnStarted{Enemy, turn}`.
   2. If the enemy has `stun`: emit `TurnSkipped{Enemy, Stun}`, then `endTurn(Enemy)`, then leave the loop.
   3. `MedusaPattern`: `roll = rng.below(10)`. Emit `Attacked{Enemy}`. Call `hit(enemy→hero, enemy.atk, applyDef=true, Attack)`.
      - If the hero's HP is 0: emit `Defeated{Hero}` and `BattleEnded{Defeat, turn}`, set over, return.
      - If the enemy's HP is 0 (from reflect): emit `Defeated{Enemy}` and `BattleEnded{Victory, turn}`, set over, return.
      - If `roll < 5` (Strike): heal the enemy by `life` (`Healed` if > 0). Otherwise (Gaze): `extra = rng.below(10) < 6`.
   4. `endTurn(Enemy)`. If `extra`: emit `ExtraTurn{Enemy}` and repeat the loop. Otherwise leave the loop.
7. If `turn >= kTurnCap`: emit `BattleEnded{Draw, turn}` and set over.

`hit(attacker→target, power, applyDef, cause)`:
- `def = target has defense_down ? target.def / 2 : target.def`
- `d = applyDef ? max(0, power − def) : power`
- If the target has `reflect` and `d > 0`: `r = d / 2`, the target takes `d − r` (`Damaged{target, d−r, …, cause}`), and the attacker takes `r` (`Damaged{attacker, r, …, Reflect}`). Otherwise the target takes `d` (`Damaged`, even when `d == 0`).
- HP never goes below 0. The function returns the damage the target actually took.

Statuses:
- Applying sets `fresh = (currently acting side == target)`.
- Re-applying a status the target already has replaces `turnsRemaining` and `fresh`, and emits `StatusApplied` again. It does not stack.
- `endTurn(side)`: for each of that side's statuses in order: if `fresh`, set `fresh = false`. Otherwise decrement, and at 0 remove it and emit `StatusExpired`.
- Worked example, Frozen Throne on hero turn T: the enemy skips 3 turns, `defense_down` is active during hero turns T+1, T+2, T+3, and both expire at the end of the enemy's 3rd skipped turn. Golden Bell on turn T: `reflect` protects during the enemy phases after T and after T+1, then expires at the end of hero turn T+2.

## 8. Training (`training.hpp`)

```cpp
struct TrainingRound { std::string monsterName; int caveLevel; Outcome outcome; std::vector<Event> events;
                       std::int64_t goldGained = 0, expGained = 0; int levelsGained = 0;
                       std::optional<std::string> drop; bool dropLostBagFull = false; };
std::expected<std::vector<TrainingRound>, TownError>
runTraining(GameState&, const Content&, int caveLevel /*1..30*/, int rounds /*1..100*/);
```

1. Precheck: `canAdventure`, else `HeroDead` or `NotEquipped`. Range-check the inputs (`InvalidQuantity`). `Rng rng = state.rng.fork()`.
2. For each round: generate the monster (exact draw order in content-tables). Build the hero `Combatant` from the current hero (current HP carries over between rounds).
3. Exchange loop, with `t` counting exchanges:
   - `t += 1`. If `t > 1000`: Draw.
   - Hero hits: `max(0, atk − mdef)` → `Damaged`. If the monster is at 0: Victory.
   - Monster hits: `max(0, matk − def)` → `Damaged`. If the hero is at 0: Defeat.
   - Heal the hero by `min(life, maxHp − hp)` → `Healed` if > 0.
   - No statuses, skills, or reflect in training.
4. Victory: add gold, call `grantExp(expReward)`, then roll a drop:
   - Candidates are items with `dropRate > 0` in file order.
   - `i = rng.below(n)`, `item = candidates[i]`. Success if `rng.below(100) < min(100, item.dropRate + caveLevel)`.
   - On success, add it to the bag, or set `dropLostBagFull`.
5. Defeat: `hero.hp = 0`, `hero.exp = max(0, exp − expReward)`, and stop all remaining rounds. Draw: no rewards, continue with the next round.
6. Write the hero's HP back to the state after each round.

## 9. Town (`town.hpp`)

```cpp
enum class TownError { HeroDead, NotEquipped, NotEnoughGold, NotEnoughExp, BagFull, BankFull, ItemNotInBag,
    NotTradeable, NotAuctionable, LevelTooLow, MissingMaterial, MissingIngredient, AlreadyLearned,
    NoBankAccount, AlreadyHasBank, InvalidQuantity, InvalidName, MaxUpgrade, AlreadyEquipped,
    WrongSlot, NotConsumable, UnknownItem, MissionLocked, MissionCompleted };
std::string_view describe(TownError);   // player-facing sentence for every value
enum class EquipSlot { Weapon, Armor };
enum class CharismaStat { MaxHp, MaxMp, Atk, Def };
struct AuctionResult { bool sold; int bids; std::int64_t payout; };
struct ForgeResult { bool success; std::string itemId; };
```

`TownError` and `describe()` live in `src/game/errors.hpp`, because `Inventory` also uses them. All functions take `(GameState&, const Content&, …)` and return `std::expected<Result, TownError>`. **Rule: check everything first, then mutate.** On any error the state must be bit-identical to before the call (tests compare a copy of `GameState` with `==`, so give all state types defaulted `operator==`).

| function | checks (in order) | effect |
|---|---|---|
| `buy(id, qty)` | hero alive, qty 1–99, item exists, `price > 0`, gold ≥ price×qty, `canAdd` | gold −, bag + |
| `sell(id, qty)` | alive, qty ≥ 1, `price > 0`, count ≥ qty | bag −, gold + price×qty |
| `auction(id)` | alive, `auctionValue > 0`, count ≥ 1 | fork the rng. `r = below(10)`. If `r == 0`: not sold, item kept. Else `result = value`; for `i < r`: if `below(3) == 0` break; `result += value / 10; bids++`. Payout `result × 9 / 10`. Remove 1 item and add gold |
| `equip(slot, id)` | alive, in bag, kind matches slot, not the same item as equipped, bag can take the old item after removing 1 new | the old item returns to the bag at upgrade 1. The new item is equipped at upgrade 1 |
| `upgrade(slot)` | alive, slot equipped, level < 10, material count ≥ L, exp ≥ 1000L, gold ≥ 10000L | consume all costs, level + 1 |
| `forge(recipeId)` | alive, all ingredients in bag, bag can take the result after removing the ingredients | fork the rng, consume ingredients. Success if `below(100) < forgeLevel × 10`: add the result, `forgeLevel = min(10, +1)` |
| `revive()` | hp == 0 | gold −= min(100, gold), hp = min(100, maxHp) |
| `useConsumable(id)` | in bag, kind consumable | apply the effect (capped), remove 1 |
| `buyCharisma(stat, expSpent)` | level ≥ 10, expSpent ≥ 1 and ≤ exp, points ≥ 1, gold ≥ points×100, gain ≥ 1 | exp −, gold −, stat + gain (does **not** raise current HP or MP) |
| `learnSkill(id)` | level ≥ max(5, learnLevel), not learned, key item ≥ 1, gold ≥ cost | consume the key and gold, add to learned |
| `rename(raw)` | `validateHeroName`, gold ≥ 50000 | gold −, name = |
| `openBank()` | no bank, gold ≥ 10000 | gold −, bank = Inventory{} |
| `deposit(id, qty)` / `withdraw(id, qty)` | has bank, qty ≥ 1, source count ≥ qty, destination `canAdd` | move |
| `startMission(id)` → `Battle` | `canAdventure`, level ≥ minLevel, not completed unless repeatable | construct the Battle with `state.rng.fork()` |
| `finishMission(id, const Battle&)` | battle over | Victory: grant exp (`grantExp`), gold, and items (bag full → item lost, and the result reports it), mark completed. Defeat: hp = 0, weapon and armor = nullopt. Fled or Draw: no change. Copy the battle hero's HP and MP back on every outcome |

Merchant and blacksmith refuse a dead hero (`HeroDead`). Guildmaster and bank don't.

## 10. Save (`save.hpp`)

```cpp
enum class SaveError { NotFound, Unreadable, Corrupt, NewerVersion, WriteFailed };
std::expected<void, SaveError> saveGame(const GameState&, const std::filesystem::path& file);
std::expected<GameState, SaveError> loadGame(const std::filesystem::path& file, const Content&);
```

```json
{"version": 1, "seed": "0x000000000000002a",
 "rng": {"state": "0x…16 hex…", "inc": "0x…16 hex…"},
 "hero": {"name": "Aria", "pathId": "fighter", "level": 14, "expToNext": 500, "exp": 1200, "gold": 20000,
          "hp": 350, "mp": 50, "maxHp": 350, "maxMp": 50, "atk": 50, "def": 5, "life": 0,
          "weapon": {"itemId": "iron_sword", "upgradeLevel": 1}, "armor": null,
          "bag": [["topaz", 3], null, null, null, null, null, null, null, null, null],
          "bank": null, "learnedSkills": ["angel_wings"], "completedMissions": [], "forgeLevel": 9}}
```

- 64-bit RNG values are stored as hex strings (JSON numbers above 2^53 lose precision in many readers).
- Write: serialize to a string, write `save.json.tmp` in the same directory, flush and close, then `std::filesystem::rename(tmp, final)`.
- Load: parse failure, a missing field, or unknown item, path, or skill IDs → `Corrupt`. `version > 1` → `NewerVersion`. Loading never modifies or deletes the file.
- Location (`src/platform/paths.cpp`): macOS `~/Library/Application Support/heroes-rpg/save.json`. Linux `$XDG_DATA_HOME/heroes-rpg/save.json`, else `~/.local/share/heroes-rpg/save.json`. Windows `%APPDATA%\heroes-rpg\save.json`. Create the directory on first save. A `HEROES_SAVE_DIR` env var overrides it (used by tests).

## 11. Executable flags and data location

`heroes [--seed N] [--data-dir PATH]`. The data dir comes from `--data-dir`, then the `HEROES_DATA_DIR` env var, then `HEROES_SOURCE_DIR "/data"`. Sprites come from `<data-dir>/../assets/sprites`. On content errors, print every formatted error to stderr **before** entering the alternate screen and exit with code 2.

## 12. Engine (`src/engine/`)

```cpp
struct Rgb { std::uint8_t r, g, b; auto operator<=>(const Rgb&) const = default; };
struct Cell { char32_t glyph = U' '; Rgb fg{200, 200, 200}; Rgb bg{0, 0, 0}; bool operator==(const Cell&) const = default; };
class Framebuffer {
public:
    Framebuffer(int width, int height);
    int width() const; int height() const;
    Cell& at(int x, int y); const Cell& at(int x, int y) const;   // precondition: in bounds
    void set(int x, int y, const Cell&);                          // silently clips out of bounds
    void clear(Rgb bg);
    void text(int x, int y, std::string_view ascii, Rgb fg, std::optional<Rgb> bg = {});  // clips
    void fillRect(int x, int y, int w, int h, Cell);
    void box(int x, int y, int w, int h, Rgb fg);  // ┌─┐│└┘
};
enum class ColorMode { TrueColor, Ansi256 };
struct PresentStats { int cellsWritten = 0; std::size_t bytesWritten = 0; };
class Renderer {
public:
    explicit Renderer(ColorMode);
    PresentStats present(const Framebuffer&, std::string& out);   // appends escape bytes to `out`
    void invalidate();                                            // next present = full redraw
};
```

`present` algorithm:
- If there's no previous frame, the size changed, or `invalidate()` was called: emit `ESC[0m ESC[2J`, then every cell is "changed".
- Wrap the frame in synchronized-update markers `ESC[?2026h` … `ESC[?2026l`.
- Walk the rows top to bottom, left to right. For each changed cell:
  - If the cursor isn't already at it, emit `ESC[{y+1};{x+1}H`.
  - If `fg` differs from the last emitted fg, emit `ESC[38;2;r;g;bm` (or `ESC[38;5;nm` in Ansi256). Same for bg with `48`.
  - Emit the glyph as UTF-8. The cursor advances one column.
- Store the frame as previous.
- `cellsWritten` counts the changed cells written.
- RGB to 256: per channel `v < 48 ? 0 : v < 115 ? 1 : (v − 35) / 40`, then `16 + 36r + 6g + b`.
- ColorMode detection: `COLORTERM` env var ∈ {`truecolor`, `24bit`} → TrueColor. Otherwise Ansi256. Windows Terminal sets `WT_SESSION`, which also means TrueColor.
- Only single-width glyphs are used anywhere: ASCII, box drawing, `▀ ▄ █ ░`.

Sprite file format (`.sprite`, UTF-8 text):
```
# lines starting with # are comments
sprite 24 24
palette
a #1e1e2e
b #f5c2e7
pixels
........aaaaaaaa........
…exactly <height> rows of exactly <width> chars; '.' = transparent; other chars must be palette keys…
```
- Palette keys are single `[A-Za-z0-9]`. Hex colors are `#rrggbb`.
- Errors carry the line number: `std::expected<Sprite, std::string>`.
- `drawSprite(Framebuffer&, const Sprite&, int cellX, int cellY, SpriteTint tint = None)` covers pixel rows `2k, 2k+1` with cell row `cellY + k`:
  - Both transparent: leave the cell.
  - Top only: `▀` with fg = top, bg = the existing cell bg.
  - Bottom only: `▄` with fg = bottom, bg = the existing bg.
  - Both: `▀` with fg = top, bg = bottom.
  - Odd height: the last row's bottom pixel is transparent.
- `SpriteTint`: `None`, `Flash` (all opaque pixels white), `Dim(n)` (each channel × (8 − n) / 8, n 0..8).

Input (`input.hpp`):
```cpp
enum class Key { Up, Down, Left, Right, Enter, Escape, Backspace, Tab, Char };
struct KeyEvent { Key key; char ch = 0; };   // ch set when key == Char (printable 0x20–0x7E)
class InputDecoder { public: void feed(std::string_view bytes); std::optional<KeyEvent> pop(); };
```
- POSIX decoding: `ESC [ A/B/C/D` and `ESC O A/B/C/D` map to arrows. A lone ESC with no following byte in the same `feed` is Escape. `\r` or `\n` is Enter. `127` or `8` is Backspace. `\t` is Tab. `0x20–0x7E` is Char. Other bytes are dropped.
- Windows (`terminal_windows.cpp`) translates `KEY_EVENT_RECORD` (key-down only) into the same `KeyEvent`s directly.

Platform (`terminal.hpp`):
```cpp
class Terminal {   // RAII: constructor enters raw mode + alt screen; destructor restores
public:
    Terminal(); ~Terminal(); Terminal(const Terminal&) = delete; Terminal& operator=(const Terminal&) = delete;
    std::pair<int, int> size() const;          // cols, rows
    std::string readAvailable();               // non-blocking; POSIX raw bytes
    std::vector<KeyEvent> readKeys();          // decoded keys (Windows decodes natively)
    void write(std::string_view);
};
```
- POSIX: save `tcgetattr`. Clear `ECHO ICANON IEXTEN` in lflag and **keep `ISIG`**. Clear `IXON ICRNL BRKINT INPCK ISTRIP` in iflag. Set `VMIN=0 VTIME=0`. Emit `ESC[?1049h ESC[?25l`.
- Restore: write `ESC[0m ESC[?25h ESC[?1049l` (one static string), then `tcsetattr(saved)`.
- Install handlers for SIGINT, SIGTERM, and SIGHUP. Each restores using only async-signal-safe calls (`write`, `tcsetattr`), then `signal(sig, SIG_DFL); raise(sig)`.
- Also register `std::atexit` restore. Restoring must be idempotent (guard with an atomic flag).
- Size comes from `ioctl(TIOCGWINSZ)`.
- Windows: save both console modes. Input clears `ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT` and keeps `ENABLE_PROCESSED_INPUT`. Output adds `ENABLE_VIRTUAL_TERMINAL_PROCESSING`. Call `SetConsoleOutputCP(CP_UTF8)`. `SetConsoleCtrlHandler` restores, then returns FALSE. Size comes from `GetConsoleScreenBufferInfo().srWindow`.

Loop (`loop.hpp`): fixed step 1/60 s. Each frame: read keys, add elapsed time to an accumulator, run `update(step)` while accumulator ≥ step (max 5 per frame, dropping the excess), draw, present, then `sleep_until(frameStart + 1/60 s)`. Check the terminal size every frame. Below 80×24, draw only the centered message "Please resize the terminal to at least 80×24 (now W×H)" and skip `update`.

## 13. Simulator (`tools/sim`)

```
heroes-sim --path fighter --enemy medusa --battles 100000 --seed 7 [--threads N] [--policy attack|smart]
           [--weapon ID[:LEVEL]] [--armor ID[:LEVEL]] [--skills id,id,...]
           [--bonus-hp N] [--bonus-mp N] [--bonus-atk N] [--bonus-def N] [--bench] [--data-dir PATH]
```
- Defaults: `--threads` = `std::thread::hardware_concurrency()` (min 1), `--policy attack`, `--weapon iron_sword:1`, `--armor steel_shield:1`, no skills, bonuses 0. `--battles` and `--seed` are required.
- Parse arguments by hand (no argument library).
- On errors, print `heroes-sim: error: <message>` to stderr, exit 2, and run no battles.
- The hero is `newGame(...)` with the path, then the CLI loadout and bonuses applied to base stats, then `heroCombatant`. Battle `i` gets `Battle(content, hero, enemy, skills, Rng::stream(seed, i))`.
- `smart` policy, each turn, picks the first that applies:
  1. HP < 30% of max and Angel Wings is learned and affordable → Angel Wings.
  2. Supernova learned and affordable → Supernova.
  3. Frozen Throne learned, affordable, and the enemy isn't stunned → Frozen Throne.
  4. Meteor Fist learned and affordable → Meteor Fist.
  5. Golden Bell learned, affordable, and the hero lacks reflect → Golden Bell.
  6. Otherwise Attack.
- Threading: split `[0, N)` into `threads` contiguous ranges. Each thread fills a local `Tally{wins, losses, draws, fled, totalTurns, hpLeftOnWins}` (all `std::uint64_t`). Merge the tallies in thread order.
- Output (exact format; percentages and averages to 2 decimals, computed only here):
```
battles: 100000  seed: 7  threads: 8  policy: attack
enemy: medusa  hero: fighter atk=100 def=25 life=0 hp=350 mp=50
win: 0.00%  loss: 100.00%  draw: 0.00%  fled: 0.00%
avg turns: 2.00  avg hp left on wins: 0.00
```
  With `--bench`, add: `wall: 0.081 s  throughput: 1234567 battles/s`
- The determinism test runs the binary logic in-process (factor `runSimulation(const SimConfig&) -> Report` into `tools/sim/sim.{hpp,cpp}`, linked into `heroes_tests`) and compares Reports with threads 1 vs 8.

## 14. App (`src/app/`)

```cpp
struct Session { const game::Content& content; std::optional<game::GameState> state;
                 std::filesystem::path saveFile; bool dirty = false; std::string lastMessage; };
struct Stay {}; struct Pop {}; struct Quit {};
struct Push { std::unique_ptr<class Screen> screen; };
struct Replace { std::unique_ptr<class Screen> screen; };   // pop current, push new
using Transition = std::variant<Stay, Push, Pop, Replace, Quit>;
class Screen {
public:
    virtual ~Screen() = default;
    virtual Transition update(std::span<const engine::KeyEvent> keys, double dtSeconds) = 0;
    virtual void draw(engine::Framebuffer&) const = 0;   // draws the full 80×24 canvas
};
class App {
public:
    App(Session&, platform::Terminal&);
    int run();   // loop per §12 until Quit or the stack is empty; returns the exit code
};
```
- Screens get `Session&` in their constructor. Only screens mutate `session.state`, and only through the `game::` functions. Any successful mutation sets `dirty = true`, and a successful save clears it.
- `main.cpp`:
  1. Parse flags (§11).
  2. `loadContent`, printing errors and exiting 2 on failure.
  3. Construct `Terminal`, `Renderer(detectColorMode())`, `Session`, and `App`.
  4. `return app.run()`.
  - Catch `std::exception` at the top level: let the Terminal destructor restore the screen, then print `what()` and exit 1.
- `BattleView` (`battle_view.hpp`) owns the animation queue. It takes `std::vector<Event>` from `Battle`, plays them per `ui.md`, and exposes `busy()`. The Battle screen sends actions to `Battle` only when `!busy()`.
- Equality: `Rng`, `Inventory`, `Hero`, `GameState`, `Combatant`, and every `ev::` struct get a defaulted `operator==`. Tests depend on this.
