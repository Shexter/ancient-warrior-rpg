#include "game/script/script_host.hpp"

#include <sol/sol.hpp>

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <format>
#include <map>
#include <optional>
#include <utility>

namespace heroes::game::script {

struct ScriptHost::Impl {
    lua_State* state = nullptr;
    std::optional<sol::state_view> lua;
    std::size_t used = 0; // bytes the state holds right now
    int checks = 0;       // budget checks so far in the current run
    std::map<std::string, sol::table, std::less<>> scripts;
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
    // The environment maker goes in the registry, where scripts can't reach it.
    const sol::protected_function makeEnv = lua.safe_script(kFreeze, sol::script_pass_on_error);
    lua.registry()["heroes.makeEnv"] = makeEnv;
}

ScriptHost::~ScriptHost() {
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

std::expected<void, std::string> ScriptHost::call(const std::string& name,
                                                  std::string_view function) {
    const auto script = impl_->scripts.find(name);
    if (script == impl_->scripts.end())
        return std::unexpected(std::format("{}: script is not loaded", name));
    const sol::object member = script->second.raw_get<sol::object>(function);
    if (member.get_type() != sol::type::function)
        return std::unexpected(std::format("{}: the script has no function '{}'", name, function));
    sol::protected_function fn = member;
    const auto result = budgeted(*impl_, name, [&] { return fn(); });
    if (!result)
        return std::unexpected(result.error());
    return {};
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
