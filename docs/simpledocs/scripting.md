# Scripting (`src/game/script/`)

Lua 5.4.7 (built as the C library `lua_static`) with sol2 v3.5.0, both fetched with pinned SHA-256 hashes in `cmake/Dependencies.cmake`. Only `heroes_game` links them. So far (add-lua-scripting chunk 1) there is the sandbox and nothing in the game runs scripts yet.

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
