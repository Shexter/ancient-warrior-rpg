# Lua Scripting — Implementation Reference

Builds on `build-core-engine` (and on `add-procedural-dungeons` if it shipped first). The core conventions apply. If something here conflicts with a spec, the spec wins: report it as SPEC DRIFT.

**What moves to Lua.**
- Enemy behavior: `EnemyBehavior::MedusaPattern` is deleted from C++ and becomes `data/scripts/medusa.lua`. Medusa and Gorgon Spawn reference it. `Basic` stays native because it is the fallback.
- Skills: the five core skills **stay data-driven** (`SkillEffect` in `skills.json`), since their effects are already data. A skill may instead have a `script` (exactly one of `effect` or `script`). The example mod demonstrates a scripted skill.

**Spike result (planning, 2026-10-10, throwaway code).** sol2 v3.5.0 with Lua 5.4.7 compiles warning-free under Apple clang 21 with `-std=c++23 -Wall -Wextra`. Nil-ing the denied globals works, and a `LUA_MASKCOUNT` hook stops `while true do end`.

## 1. Dependencies (`cmake/Dependencies.cmake` additions)

| dep | URL | SHA-256 |
|---|---|---|
| Lua 5.4.7 | `https://www.lua.org/ftp/lua-5.4.7.tar.gz` | `9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30` |
| sol2 v3.5.0 | `https://github.com/ThePhD/sol2/archive/refs/tags/v3.5.0.tar.gz` | `86c0f6d2836b184a250fc2907091c076bf53c9603dd291eaebade36cc342e13c` |

The hashes in the core change, computed the same way during planning, are:
- nlohmann/json v3.12.0 `json.tar.xz`: `42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa`
- GoogleTest v1.17.0 zip: `40d4ec942217dcc84a9ebe2a68584ada7d4a33a8ee958755763278ea1c5e18ff`
- `stb_image.h` @2c980bb: `594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3`

Lua has no CMakeLists. Populate it with FetchContent, then:
```cmake
project(... LANGUAGES C CXX)
file(GLOB LUA_SOURCES ${lua_SOURCE_DIR}/src/*.c)
list(REMOVE_ITEM LUA_SOURCES ${lua_SOURCE_DIR}/src/lua.c ${lua_SOURCE_DIR}/src/luac.c)
add_library(lua_static STATIC ${LUA_SOURCES})
target_include_directories(lua_static SYSTEM PUBLIC ${lua_SOURCE_DIR}/src)
if(UNIX) target_compile_definitions(lua_static PRIVATE LUA_USE_POSIX) endif()
```
Don't link `heroes_warnings` to `lua_static`.

sol2 is header-only. Use `sol2::sol2` from its CMake, or `include_directories(SYSTEM .../include)`. `heroes_game` gets `SOL_ALL_SAFETIES_ON=1` and links `lua_static`.

## 2. Files

```
src/game/script/script_host.{hpp,cpp}   # one lua_State, sandbox, budget, loaded scripts
src/game/script/battle_api.{hpp,cpp}    # ctx object bound for a single call
data/scripts/medusa.lua
mods/example-cyclops/{mod.json, enemies.json, skills.json, scripts/cyclops.lua, scripts/boulder_toss.lua}
tests/script_sandbox_test.cpp tests/script_battle_test.cpp tests/mods_test.cpp tests/fixtures/mods/...
```

## 3. ScriptHost

```cpp
class ScriptHost {   // not thread-safe; one per thread (game thread, each simulator worker)
public:
    explicit ScriptHost(const Content&);   // creates the sandboxed state and loads every script referenced by content
    // Runs `fn` with a fresh ctx. Returns an error string on Lua error, budget exceeded, or memory cap.
    std::expected<void, std::string> runEnemyTurn(const std::string& scriptPath, BattleApi& api);
    std::expected<void, std::string> runSkill(const std::string& scriptPath, BattleApi& api);
};
std::expected<void, std::vector<ContentError>> validateScripts(const Content&);   // load-time syntax/shape check
```
- **Sandbox.**
  - Open `base`, `string`, `table`, and `math`.
  - Set `dofile`, `loadfile`, `load`, `require`, `collectgarbage`, `print`, `math.random`, and `math.randomseed` to nil.
  - Don't open `io`, `os`, `package`, `debug`, `coroutine`, or `utf8`.
  - After setup, freeze `_G` with a metatable whose `__newindex` raises `"globals are read-only"`. Scripts may use locals and their returned table only.
- **Script shape.** Each file must `return` a table: `{ take_turn = function(ctx) end }` for enemies, or `{ cast = function(ctx) end }` for skills. Anything else is a load-time `ContentError` naming the file. Each script loads once in its own environment table, with `__index = _G`.
- **Budget.** Before each call, reset the counter and set `lua_sethook(L, hook, LUA_MASKCOUNT, 10000)`. The hook raises `"instruction budget exceeded"` after 100 calls (≈1,000,000 instructions). Remove the hook after the call.
- **Memory cap.** Create the state with `lua_newstate` and a custom allocator that tracks bytes and refuses growth past 8 MiB (returns NULL, which Lua turns into a memory error). With sol2, use `sol::state(sol::default_at_panic, allocator)`, or construct the raw state and wrap it in `sol::state_view`.
- **Errors.** The returned string is `"<scriptPath>:<line>: <message>"`, taken from the Lua message (which already carries `[string "..."]:N:` or the chunk name). Load chunks with the chunk name `@<relative path>` so messages show the file path.

## 4. ctx API (all numbers are integers; floats from Lua are rejected by `luaL_checkinteger`)

| member | type | meaning |
|---|---|---|
| `ctx.self`, `ctx.target` | read-only table | `name, hp, max_hp, mp, max_mp, atk, def, life` and `has(status)`, where status is `"stun"`, `"defense_down"`, or `"reflect"`. For enemy turns `self` is the enemy; for skills `self` is the hero |
| `ctx.rng(n)` | → int in `[0, n)` | `Battle`'s `Rng::below(n)`. `n` must be 1–1,000,000 |
| `ctx.attack()` | → damage dealt | exactly core `hit(self→target, self.atk, applyDef=true, Attack)`, including reflect, and emits `Attacked{side}` first |
| `ctx.damage(power, apply_def)` | → damage dealt | `hit(self→target, power, apply_def, Skill)`. `power` is 0–10,000,000 |
| `ctx.heal(who, amount)` | → healed | `who` is `"self"` or `"target"`. Capped. Emits `Healed` if > 0 |
| `ctx.halve_hp(who)` | | emits `HpHalved` |
| `ctx.apply_status(who, id, turns)` | | `turns` is 1–20. Core apply rules |
| `ctx.extra_turn()` | | enemy scripts only. Grants one more enemy turn after this one, via the core `ExtraTurn` rule |
| `ctx.say(text)` | | adds a log line. ASCII only, cut to 80 chars, emits `ev::ScriptSaid{side, text}` (new event) |
| `ctx.over()` | → bool | whether the battle has ended |
| `ctx.turn` | read-only int | the current battle turn number (core `Battle::turn()`) |

- After every op, `Battle` runs the core death checks (hero first, then enemy). Once the battle is over, every further op is a no-op returning 0. That matches native ordering: nothing happens after a death.
- New core event: `ev::ScriptError{std::string script; std::string message}`. The UI log shows "Something went wrong with <name>'s magic." The details go to the dev log only.

`data/scripts/medusa.lua` must reproduce core §7 step 6.3 **including RNG draw order**:
```lua
return {
  take_turn = function(ctx)
    local roll = ctx.rng(10)
    ctx.attack()
    if ctx.over() then return end
    if roll < 5 then
      ctx.heal("self", ctx.self.life)
    elseif ctx.rng(10) < 6 then
      ctx.extra_turn()
    end
  end,
}
```

## 5. Failure semantics (transactional)

- **Enemy turn.**
  1. Snapshot `{hero, enemy, rng, events.size()}`, then run the script.
  2. On error: restore the snapshot (drop events added since), emit `ScriptError`, then run the native `Basic` turn.
  3. `endTurn` and the extra-turn handling follow as normal (no extra turn after a fallback).
- **Skill.**
  1. Validate MP as in core. Snapshot, spend MP, emit `SkillCast`, run the script.
  2. On error: restore the snapshot so MP is unspent and there are no events, and `act()` returns the new `ActionError::ScriptFailed`.
  3. The UI shows the ScriptError text in the log, and it's still the hero's turn.
- Determinism holds because the snapshot includes the `Rng`.

## 6. Mods

- Directory: `HEROES_MODS_DIR`, else `<data-dir>/../mods`. A missing directory means no mods.
- `mod.json`: `{"name": "example-cyclops", "version": "1.0.0", "priority": 0, "description": "..."}`.
  - `name` matches `^[a-z0-9-]{1,40}$` and must equal the folder name.
  - `version` is a free string ≤ 20.
  - `priority` is an int, default 0.
- A mod folder may contain any of the core data file names (`items.json`, `enemies.json`, `skills.json`, `missions.json`, `recipes.json`, `dungeon.json` excluded) and `scripts/`.
  - Script paths in a mod's data are relative to the mod folder, and must stay inside it (reject `..` and absolute paths).
- Merge order: base, then mods by `(priority asc, name asc)`.
  - An entry whose `id` exists replaces it whole. New IDs append.
  - Each override prints `mod '<name>': overrides <file-stem>/<id>` to stderr before the alternate screen starts. Each mod loaded prints `mod '<name>' <version> loaded`.
- Validation runs on the merged content.
  - If errors come from a mod's entries or scripts, drop that whole mod, print `mod '<name>' skipped: <first error>` plus the count of further errors, and re-merge without it. Repeat until valid.
  - If base content alone is invalid, exit 2 as in core.
- Saves:
  - Add top-level `"mods": [{"name": "...", "version": "..."}]` and bump the save version by one from whatever is current when this ships. Older saves load with an empty list.
  - On load, if any listed mod is missing or has a different version, show a ConfirmBox: "This save used mods that are missing: <list>. Continue anyway?". No leaves the file untouched and returns to Title.
  - Unknown item or enemy IDs from a missing mod then make the load fail as `Corrupt`, and the message names the mod list.

## 7. Example mod (`mods/example-cyclops/`)

- `enemies.json`: `cyclops`, hp 2500, atk 300, def 60, life 0, exp 3000, gold 2000, sprite `monster.sprite`, `"script": "scripts/cyclops.lua"`:
  ```lua
  return {
    take_turn = function(ctx)
      if ctx.turn % 3 == 0 then
        ctx.say("The Cyclops hurls a boulder!")
        ctx.damage(ctx.self.atk * 2, true)
      else
        ctx.attack()
      end
    end,
  }
  ```
- `missions.json`: `cyclops_cave`, name "The Cyclops Cave", location "CYCLOPS CAVE", minLevel 15, repeatable true, enemy `cyclops`, story lines "A one-eyed giant blocks the mountain pass..." / "...and it has not eaten in days." This makes the enemy reachable in game without touching dungeons.
- `skills.json`: `boulder_toss`, mp 30, learnLevel 10, gold 8000, keyItem `topaz`, `"script": "scripts/boulder_toss.lua"`:
  ```lua
  return { cast = function(ctx) ctx.damage(ctx.self.atk * 3 // 2, true) end }
  ```
  (`//` is Lua integer floor division.)
- Mods cannot modify `dungeon.json` in v1, so mod enemies don't appear in dungeon pools. That's a documented limitation, not a bug.

## 8. Simulator

Each worker thread constructs its own `ScriptHost(content)`. Report scripted throughput by running the Medusa benchmark (which now goes through Lua) and comparing it with the native baseline recorded in `docs/benchmarks.md` before this change. The target is ≥ 25% of native. If it's below that, cache `sol::protected_function` handles per script and reuse one ctx userdata per battle.
