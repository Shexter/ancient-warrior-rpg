#pragma once

#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace heroes::game {
class Content;
} // namespace heroes::game

namespace heroes::game::script {

class BattleApi;

// One sandboxed Lua state. Not thread-safe: use one per thread (the game thread, each simulator
// worker). Scripts load once, each into its own read-only environment; every load and call runs
// under an instruction budget, and the whole state's memory is capped. Every failure comes back
// as an error string ("<script>:<line>: <message>"); a script can never crash the game.
//
// Sandbox (lua-interfaces.md §3, plus hardening found in review):
// - only the base, string, table and math libraries;
// - no dofile, loadfile, load, require, collectgarbage, print, warn, rawset, math.random,
//   math.randomseed, _G, or sol2's `base` alias of the global table;
// - globals and the library tables are read-only, and their metatables (and the string
//   metatable) are locked, so no script can unfreeze them or leave state for the next battle.
class ScriptHost {
  public:
    static constexpr std::size_t kMemoryCap = std::size_t{8} << 20; // bytes for the whole state
    static constexpr int kHookInterval = 10'000; // VM instructions between budget checks
    static constexpr int kBudgetChecks = 100;    // so about 1,000,000 instructions per run

    ScriptHost();
    // A host with every script `content` references already loaded (content validation has
    // checked they load).
    explicit ScriptHost(const Content& content);
    ~ScriptHost();
    ScriptHost(const ScriptHost&) = delete;
    ScriptHost& operator=(const ScriptHost&) = delete;

    // Runs `source` as script `name` (errors say "name:line: ..."). It must return a table,
    // which is kept under `name` for call().
    std::expected<void, std::string> load(const std::string& name, std::string_view source);
    // Calls `function` from loaded script `name` with no arguments.
    std::expected<void, std::string> call(const std::string& name, std::string_view function);
    // Whether loaded script `name` returned a table with a function called `function`.
    bool hasFunction(const std::string& name, std::string_view function) const;

    // Runs an enemy script's take_turn(ctx) or a skill script's cast(ctx), with ctx acting on
    // `api`'s battle (lua-interfaces.md §4). The caller undoes everything on failure.
    std::expected<void, std::string> runEnemyTurn(const std::string& name, BattleApi& api);
    std::expected<void, std::string> runSkill(const std::string& name, BattleApi& api);

    // Every name a script can reach: globals and the library tables' fields ("string.rep"),
    // sorted. Tests compare it with the allowlist so an upgrade can't silently widen it.
    std::vector<std::string> reachableNames() const;

    struct Impl; // public so the C allocator and hook callbacks can reach it

  private:
    std::unique_ptr<Impl> impl_;
};

} // namespace heroes::game::script
