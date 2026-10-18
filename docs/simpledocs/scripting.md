# Scripting (`src/game/script/`)

Lua 5.4.7 (built as the C library `lua_static`) with sol2 v3.5.0, both fetched with pinned SHA-256 hashes in `cmake/Dependencies.cmake`. Lua is compiled as C++, so a Lua error is a C++ exception that unwinds the engine functions a script called (`SOL_USING_CXX_LUA`). Only `heroes_game` links them. Shipped scripts: `data/scripts/medusa.lua` is Medusa's turn, the same rules and random-draw order as the old C++ pattern, which was deleted; the core golden battles pass unchanged with it. `avaritia.lua`, `gula.lua` and `ira.lua` are the Act I bosses: each rolls a telegraph (say a line, put Charging on itself for one turn), strikes harder on the next turn while Charging is still on (it then expires), and varies every hit by ±15%. Gula heals half of each hit (all of its devour); Ira's power grows with its missing HP and it only charges below half HP. `ActOneBalance` pins their stats. `smite.lua`, `sanctuary.lua` and `judgment.lua` are the three Act I skills after the original five: Smite is ATK through armor, tripled while the enemy is Charging; Sanctuary puts Guard on the hero for 2 turns and heals 20% of max HP; Judgment is 3×ATK through armor if the enemy is below a quarter of its max HP, otherwise ATK with DEF applying. Their key items are Martyr's Relic, Tears of Penance (shared with Righteous Strike and Shield of Faith) and Ira's Ember of Wrath. The skill overlay grows upward, so eight skills (nine with the example mod) still fit.

**`ScriptHost`**: one sandboxed `lua_State`. It is not thread-safe, so use one per thread.
- `load(name, source)` runs the source as chunk `@name` in a fresh environment of its own. It must return a table, kept under `name`; otherwise the error is "`name`: the script must return a table".
- `call(name, function)` calls that table's function with no arguments.
- Both return `std::expected<void, std::string>`. Errors read `name:line: message` when Lua knows the line, else `name: message`. Any C++ exception from sol2 is caught and returned the same way, so a script never crashes the game.

**Sandbox:**
- Only the `base`, `string`, `table` and `math` libraries are opened. There is no `io`, `os`, `package`, `debug`, `coroutine` or `utf8`.
- Removed: `dofile`, `loadfile`, `load`, `require`, `collectgarbage`, `print`, `warn`, `rawset`, `math.random`, `math.randomseed`, `_G`, and sol2's `base` (a second name for the global table).
- Globals are read-only: assigning one raises "globals are read-only". A script's own environment refuses writes the same way.
- `string`, `table` and `math` are read-only proxies, so a script can't leave anything behind for the next one.
- Every protective metatable, including the string metatable, has `__metatable = false`. `getmetatable` returns `false`, and `setmetatable` on them fails.
- `ScriptSandbox.ReachableNamesAreExactlyTheAllowlist` compares every reachable name (`reachableNames()`) with the explicit list, so a Lua or sol2 upgrade can't widen the sandbox silently. That test is how sol2's `base` was found.

**Budget and memory:**
- A count hook checks every 10,000 VM instructions. After 100 checks (about 1,000,000 instructions) it raises "instruction budget exceeded".
- From then on the hook fires on every instruction, so a script that catches the error with `pcall` is stopped again at its next instruction.
- Loads run under the budget too, since a script's top level can loop.
- A tracking allocator caps the whole state at 8 MiB. A refused allocation becomes Lua's "not enough memory" error, and a full collection runs after any failed call. The host stays usable after budget and memory errors.

**Determinism note:** Lua seeds its string hashing per state, so `pairs` order over string keys can differ between runs. Scripts must not depend on it; the engine's own randomness comes through the battle API (chunk 2).

**Scripts in content.**
- A skill has either `effect` or `script` (both, or neither, is an error naming the skill).
- An enemy has either `behavior` or `script`. The script path is relative to the data folder and must stay inside it: no absolute paths, no `..`.
- At load every referenced script is read and run once in a sandbox. Its file must exist, load without errors (syntax errors give `path:line`), and return a table with `cast` (skills) or `take_turn` (enemies).
- The sources are kept in `Content::scripts()`. `ScriptHost(content)` loads them all. The game makes one host in `main` for the game thread and passes it through `Session` to `startMission` and `Battle`. The simulator makes one per worker thread, and tests share `shippedScripts()`.

**ctx** (`BattleApi`, bound in `ScriptHost`). Scripts call these with a dot (`ctx.attack()`):
- `ctx.self` and `ctx.target` are read-only views: `name`, `hp`, `max_hp`, `mp`, `max_mp`, `atk`, `def`, `life`, and `has("stun"|"defense_down"|"reflect")`. For an enemy turn `self` is the enemy; for a skill `self` is the hero.
  - A view only names a side, never holds a pointer. Values are read live, so they change after an attack.
  - A view kept by a script past its battle reads the next battle's fighter, or fails cleanly between battles.
- `ctx.turn` is the current turn.
- `ctx.rng(n)`, with `n` from 1 to 1,000,000, draws from the battle's RNG.
- `ctx.attack(power)` emits `Attacked` and makes a normal hit (DEF applies). `power` is optional, from 0 to 10,000,000, and defaults to the attacker's ATK.
- `ctx.damage(power, apply_def)` makes a Skill hit, with `power` from 0 to 10,000,000.
- `ctx.heal(who, amount)`, `ctx.halve_hp(who)` and `ctx.apply_status(who, id, turns)` take `who` as `"self"` or `"target"`, and `turns` from 1 to 20.
- `ctx.extra_turn()` is for enemy scripts only.
- `ctx.say(text)` emits `ScriptSaid`. Anything that isn't printable ASCII becomes `?`, and the text is cut to 80 characters, so nothing reaches the terminal raw.
- `ctx.over()` says whether the battle has ended.
- Every number must be a whole number in range: Lua floats such as `1.5` or `3.0` are refused. A bad argument, a write to `ctx` or to a view, or `extra_turn` from a skill is a script error.
- After each operation the death checks run, hero first (the same order as native turns). Once the battle is over every operation does nothing and returns 0.
- `ctx` is one table per host. Its functions act on the battle whose script is running.

**When a script fails**, through an error, the budget or memory:
- **Enemy turn:** the battle restores both fighters, the RNG and the action's events to how they were before the script ran. It emits `ScriptError{script, message}` and the enemy makes a native basic attack instead, with no extra turn.
- **Skill:** the whole action is rolled back (MP unspent, turn not started) and `act()` returns `ActionError::ScriptFailed`. It's still the hero's turn.
- **No host:** a battle without a script host treats scripted enemies and skills as failing the same way.
- **Player-facing text:** the battle log shows "Something went wrong with <enemy>'s magic." for a `ScriptError`, and "Something went wrong with that skill's magic." for `ScriptFailed`. The detailed message is for developers and never shown.
