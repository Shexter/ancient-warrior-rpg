#include "game/script/script_host.hpp"

#include "game/battle.hpp"
#include "game/content.hpp"
#include "game/script/battle_api.hpp"

#include <sol/sol.hpp>

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <format>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>

namespace heroes::game::script {

struct ScriptHost::Impl {
    lua_State* state = nullptr;
    std::optional<sol::state_view> lua;
    std::size_t used = 0; // bytes the state holds right now
    int checks = 0;       // budget checks so far in the current run
    std::map<std::string, sol::table, std::less<>> scripts;
    BattleApi* api = nullptr; // the battle a running script acts on; null between runs
    std::optional<sol::table> ctx;

    BattleApi& current() const {
        if (!api)
            throw std::runtime_error("ctx is only usable while the battle runs this script");
        return *api;
    }
};

namespace {

// Tracks every byte the state holds and refuses to grow past kMemoryCap. Lua turns a refused
// allocation into a "not enough memory" error after trying an emergency collection.
void* allocate(void* ud, void* ptr, std::size_t osize, std::size_t nsize) {
    auto* impl = static_cast<ScriptHost::Impl*>(ud);
    const std::size_t old = ptr ? osize : 0; // with no block, osize encodes the object type
    if (nsize == 0) {
        impl->used -= old;
        std::free(ptr);
        return nullptr;
    }
    if (nsize > old && impl->used + (nsize - old) > ScriptHost::kMemoryCap)
        return nullptr;
    void* grown = std::realloc(ptr, nsize);
    if (grown)
        impl->used = impl->used - old + nsize;
    return grown;
}

ScriptHost::Impl* implOf(lua_State* state) {
    void* ud = nullptr;
    lua_getallocf(state, &ud);
    return static_cast<ScriptHost::Impl*>(ud);
}

// Every kHookInterval instructions. Past the budget it raises an error and from then on fires
// on every instruction, so a script that catches the error with pcall can't keep running.
void budgetHook(lua_State* state, lua_Debug*) {
    if (++implOf(state)->checks <= ScriptHost::kBudgetChecks)
        return;
    lua_sethook(state, budgetHook, LUA_MASKCOUNT, 1);
    luaL_error(state, "instruction budget exceeded");
}

// Runs once on a fresh state, after the denied functions are removed. Trusted code.
constexpr std::string_view kFreeze = R"lua(
local setmetatable, error = setmetatable, error
local function deny() error("globals are read-only", 2) end
local function frozen(t)
  return setmetatable({}, { __index = t, __newindex = deny, __metatable = false })
end
string, table, math = frozen(string), frozen(table), frozen(math)
local stringMeta = getmetatable("")
stringMeta.__metatable = false
local globals = _G
_G = nil
setmetatable(globals, { __newindex = deny, __metatable = false })
-- Each script's environment: reads fall through to the globals, writes are refused.
return function()
  return setmetatable({}, { __index = globals, __newindex = deny, __metatable = false })
end
)lua";

// "<name>:<line>: <message>" for errors Lua located; anything else gets "<name>: " in front.
std::string located(const std::string& name, std::string message) {
    if (message.starts_with(name + ":"))
        return message;
    return std::format("{}: {}", name, message);
}

// Runs `fn` under a fresh instruction budget. Exceptions (sol2 reporting a Lua panic) and Lua
// errors both come back as the error string; garbage from a failed run is collected at once.
template <class Fn>
std::expected<sol::object, std::string> budgeted(ScriptHost::Impl& impl, const std::string& name,
                                                 Fn&& fn) {
    impl.checks = 0;
    lua_sethook(impl.state, budgetHook, LUA_MASKCOUNT, ScriptHost::kHookInterval);
    std::expected<sol::object, std::string> outcome;
    try {
        sol::protected_function_result result = fn();
        if (result.valid()) {
            outcome = result.get<sol::object>();
        } else {
            const sol::error error = result;
            outcome = std::unexpected(located(name, error.what()));
        }
    } catch (const std::exception& e) {
        outcome = std::unexpected(located(name, e.what()));
    }
    lua_sethook(impl.state, nullptr, 0, 0);
    if (!outcome)
        lua_gc(impl.state, LUA_GCCOLLECT);
    return outcome;
}


// ---- ctx binding ---------------------------------------------------------------------------

[[noreturn]] void reject(std::string message) { throw std::runtime_error(std::move(message)); }

// Engine operations take whole numbers only (a Lua float like 1.5 or 3.0 is refused).
int wholeNumber(const sol::object& value, std::string_view what, int lo, int hi) {
    if (value.get_type() != sol::type::number)
        reject(std::format("{} must be a whole number", what));
    lua_State* state = value.lua_state();
    value.push();
    const bool integer = lua_isinteger(state, -1) != 0;
    const lua_Integer n = integer ? lua_tointeger(state, -1) : 0;
    lua_pop(state, 1);
    if (!integer)
        reject(std::format("{} must be a whole number", what));
    if (n < lo || n > hi)
        reject(std::format("{} must be between {} and {}", what, lo, hi));
    return static_cast<int>(n);
}

Side whoArg(const BattleApi& api, const sol::object& value, std::string_view what) {
    if (value.is<std::string>()) {
        const std::string who = value.as<std::string>();
        if (who == "self")
            return api.selfSide();
        if (who == "target")
            return api.targetSide();
    }
    reject(std::format("{} must be \"self\" or \"target\"", what));
}

StatusId statusArg(const sol::object& value, std::string_view what) {
    if (value.is<std::string>()) {
        const std::string id = value.as<std::string>();
        if (id == "stun")
            return StatusId::Stun;
        if (id == "defense_down")
            return StatusId::DefenseDown;
        if (id == "reflect")
            return StatusId::Reflect;
    }
    reject(std::format("{} must be \"stun\", \"defense_down\" or \"reflect\"", what));
}

// Builds the single ctx table this host hands to every script call.
// Functions act on impl->api; reads of ctx.self, ctx.target and ctx.turn are resolved per call;
// writes are refused and the metatable is locked.
sol::table makeCtx(ScriptHost::Impl* impl) {
    sol::state_view& lua = *impl->lua;
    // ctx.self / ctx.target: one locked table per side, never a pointer, so a view a script keeps
    // for later reads the battle running at that time (or fails cleanly between battles). No sol2
    // usertype: those leave internal tables in the globals (the allowlist test catches it).
    const auto makeView = [&lua, impl](Side side) {
        sol::table view = lua.create_table();
        sol::table meta = lua.create_table();
        meta.set_function("__index", [impl, side](sol::this_state state, const sol::object&,
                                                  std::string_view key) -> sol::object {
            BattleApi& api = impl->current();
            const Combatant& c = side == api.selfSide() ? api.self() : api.target();
            if (key == "name")
                return sol::make_object(state, c.name);
            if (key == "has") // view.has("stun"), called with a dot like every ctx function
                return sol::make_object(state, [impl, side](const sol::object& status) {
                    const StatusId id = statusArg(status, "has: status");
                    BattleApi& now = impl->current();
                    const Combatant& f = side == now.selfSide() ? now.self() : now.target();
                    return std::ranges::any_of(
                        f.statuses, [id](const StatusInstance& st) { return st.id == id; });
                });
            const int* value = key == "hp"       ? &c.hp
                               : key == "max_hp" ? &c.maxHp
                               : key == "mp"     ? &c.mp
                               : key == "max_mp" ? &c.maxMp
                               : key == "atk"    ? &c.atk
                               : key == "def"    ? &c.def
                               : key == "life"   ? &c.life
                                                 : nullptr;
            return value ? sol::make_object(state, *value) : sol::make_object(state, sol::lua_nil);
        });
        meta.set_function("__newindex", [](const sol::object&, const sol::object&,
                                           const sol::object&) { reject("fighters are read-only"); });
        meta["__metatable"] = false;
        view[sol::metatable_key] = meta;
        return view;
    };
    const sol::table heroView = makeView(Side::Hero), enemyView = makeView(Side::Enemy);

    sol::table ctx = lua.create_table();
    ctx.set_function("rng", [impl](const sol::object& n) {
        return impl->current().rng(wholeNumber(n, "rng: n", 1, 1'000'000));
    });
    ctx.set_function("attack", [impl] { return impl->current().attack(); });
    ctx.set_function("damage", [impl](const sol::object& power, const sol::object& applyDef) {
        if (applyDef.get_type() != sol::type::boolean)
            reject("damage: apply_def must be true or false");
        return impl->current().damage(wholeNumber(power, "damage: power", 0, 10'000'000),
                                      applyDef.as<bool>());
    });
    ctx.set_function("heal", [impl](const sol::object& who, const sol::object& amount) {
        BattleApi& api = impl->current();
        return api.heal(whoArg(api, who, "heal: who"),
                        wholeNumber(amount, "heal: amount", 0, kMaxStat));
    });
    ctx.set_function("halve_hp", [impl](const sol::object& who) {
        BattleApi& api = impl->current();
        api.halveHp(whoArg(api, who, "halve_hp: who"));
    });
    ctx.set_function("apply_status", [impl](const sol::object& who, const sol::object& id,
                                             const sol::object& turns) {
        BattleApi& api = impl->current();
        api.applyStatus(whoArg(api, who, "apply_status: who"), statusArg(id, "apply_status: id"),
                        wholeNumber(turns, "apply_status: turns", 1, 20));
    });
    ctx.set_function("extra_turn", [impl] {
        BattleApi& api = impl->current();
        if (api.selfSide() != Side::Enemy)
            reject("extra_turn is only for enemy scripts");
        api.extraTurn();
    });
    ctx.set_function("say", [impl](const sol::object& text) {
        if (text.get_type() != sol::type::string)
            reject("say: text must be a string");
        impl->current().say(text.as<std::string_view>());
    });
    ctx.set_function("over", [impl] { return impl->current().over(); });

    sol::table meta = lua.create_table();
    meta.set_function("__index", [impl, heroView, enemyView](sol::this_state state,
                                                             const sol::object&,
                                                             std::string_view key) -> sol::object {
        BattleApi& api = impl->current();
        if (key == "self" || key == "target") {
            const Side side = key == "self" ? api.selfSide() : api.targetSide();
            return side == Side::Hero ? heroView : enemyView;
        }
        if (key == "turn")
            return sol::make_object(state, api.turn());
        return sol::make_object(state, sol::lua_nil);
    });
    meta.set_function("__newindex", [](const sol::object&, const sol::object&, const sol::object&) {
        reject("ctx is read-only");
    });
    meta["__metatable"] = false;
    ctx[sol::metatable_key] = meta;
    return ctx;
}

} // namespace

ScriptHost::ScriptHost() : impl_(std::make_unique<Impl>()) {
    impl_->state = lua_newstate(allocate, impl_.get());
    impl_->lua.emplace(impl_->state);
    sol::state_view& lua = *impl_->lua;
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math);
    // sol2 also publishes the global table as `base`: drop it with the rest.
    for (const char* denied : {"dofile", "loadfile", "load", "require", "collectgarbage", "print",
                               "warn", "rawset", "base"})
        lua[denied] = sol::lua_nil;
    lua["math"]["random"] = sol::lua_nil;
    lua["math"]["randomseed"] = sol::lua_nil;
    impl_->ctx = makeCtx(impl_.get());
    // The environment maker goes in the registry, where scripts can't reach it.
    const sol::protected_function makeEnv = lua.safe_script(kFreeze, sol::script_pass_on_error);
    lua.registry()["heroes.makeEnv"] = makeEnv;
}

ScriptHost::ScriptHost(const Content& content) : ScriptHost() {
    for (const auto& [name, source] : content.scripts())
        (void)load(name, source); // validated at content load; a failure here surfaces per call
}

ScriptHost::~ScriptHost() {
    impl_->ctx.reset();
    impl_->scripts.clear();
    impl_->lua.reset();
    lua_close(impl_->state);
}

std::expected<void, std::string> ScriptHost::load(const std::string& name,
                                                  std::string_view source) {
    sol::state_view& lua = *impl_->lua;
    sol::load_result chunk = lua.load(source, "@" + name);
    if (!chunk.valid()) {
        const sol::error error = chunk;
        return std::unexpected(located(name, error.what()));
    }
    sol::protected_function body = chunk;
    const sol::protected_function makeEnv = lua.registry()["heroes.makeEnv"];
    const sol::table env = makeEnv().get<sol::table>();
    // A main chunk's first upvalue is always _ENV: point it at the script's own environment.
    body.push();
    env.push();
    if (!lua_setupvalue(impl_->state, -2, 1))
        lua_pop(impl_->state, 1); // no upvalue to set: drop the environment ourselves
    lua_pop(impl_->state, 1);

    const auto returned = budgeted(*impl_, name, [&] { return body(); });
    if (!returned)
        return std::unexpected(returned.error());
    if (returned->get_type() != sol::type::table)
        return std::unexpected(std::format("{}: the script must return a table", name));
    impl_->scripts.insert_or_assign(name, returned->as<sol::table>());
    return {};
}

namespace {

// The function `function` of loaded script `name`, or why there isn't one.
std::expected<sol::protected_function, std::string>
scriptFunction(const ScriptHost::Impl& impl, const std::string& name, std::string_view function) {
    const auto script = impl.scripts.find(name);
    if (script == impl.scripts.end())
        return std::unexpected(std::format("{}: script is not loaded", name));
    const sol::object member = script->second.raw_get<sol::object>(function);
    if (member.get_type() != sol::type::function)
        return std::unexpected(std::format("{}: the script has no function '{}'", name, function));
    return sol::protected_function(member);
}

} // namespace

std::expected<void, std::string> ScriptHost::call(const std::string& name,
                                                  std::string_view function) {
    auto fn = scriptFunction(*impl_, name, function);
    if (!fn)
        return std::unexpected(fn.error());
    const auto result = budgeted(*impl_, name, [&] { return (*fn)(); });
    if (!result)
        return std::unexpected(result.error());
    return {};
}

bool ScriptHost::hasFunction(const std::string& name, std::string_view function) const {
    return scriptFunction(*impl_, name, function).has_value();
}

namespace {

std::expected<void, std::string> runWithCtx(ScriptHost::Impl& impl, const std::string& name,
                                            std::string_view function, BattleApi& api) {
    auto fn = scriptFunction(impl, name, function);
    if (!fn)
        return std::unexpected(fn.error());
    impl.api = &api;
    const auto result = budgeted(impl, name, [&] { return (*fn)(*impl.ctx); });
    impl.api = nullptr;
    if (!result)
        return std::unexpected(result.error());
    return {};
}

} // namespace

std::expected<void, std::string> ScriptHost::runEnemyTurn(const std::string& name,
                                                          BattleApi& api) {
    return runWithCtx(*impl_, name, "take_turn", api);
}

std::expected<void, std::string> ScriptHost::runSkill(const std::string& name, BattleApi& api) {
    return runWithCtx(*impl_, name, "cast", api);
}

std::vector<std::string> ScriptHost::reachableNames() const {
    // Reads through the C API, which ignores the locked metatables scripts see. Keys are copied
    // with lua_pushvalue before lua_tostring, which would otherwise convert a number key in
    // place and break lua_next.
    lua_State* state = impl_->state;
    const auto keyName = [state] {
        lua_pushvalue(state, -2);
        std::string name = lua_type(state, -1) == LUA_TSTRING ? lua_tostring(state, -1) : "?";
        lua_pop(state, 1);
        return name;
    };
    std::vector<std::string> names;
    lua_pushglobaltable(state);
    lua_pushnil(state);
    while (lua_next(state, -2) != 0) { // stack: globals, key, value
        const std::string key = keyName();
        bool library = false;
        // A frozen library is an empty proxy whose metatable's __index is the real table.
        if (lua_type(state, -1) == LUA_TTABLE && lua_getmetatable(state, -1)) {
            if (lua_getfield(state, -1, "__index") == LUA_TTABLE) {
                library = true;
                lua_pushnil(state);
                while (lua_next(state, -2) != 0) {
                    names.push_back(key + "." + keyName());
                    lua_pop(state, 1);
                }
            }
            lua_pop(state, 2); // __index, metatable
        }
        if (!library)
            names.push_back(key);
        lua_pop(state, 1); // value
    }
    lua_pop(state, 1);
    std::ranges::sort(names);
    return names;
}

} // namespace heroes::game::script
