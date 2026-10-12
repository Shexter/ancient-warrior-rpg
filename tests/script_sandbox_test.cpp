#include "game/script/script_host.hpp"

#include <sol/sol.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

using heroes::game::script::ScriptHost;

namespace {

// Loads a script whose `run` does `body`, calls it, and returns the error ("" on success).
std::string runError(ScriptHost& host, const std::string& body) {
    const std::string source = "return { run = function()\n" + body + "\nend }";
    if (auto loaded = host.load("scripts/test.lua", source); !loaded)
        return loaded.error();
    const auto called = host.call("scripts/test.lua", "run");
    return called ? "" : called.error();
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

} // namespace

TEST(Lua, Sol2RunsAChunk) { // task 1.1: the dependency builds and links everywhere
    sol::state lua;
    EXPECT_EQ(lua.script("return 1 + 1").get<int>(), 2);
}

TEST(ScriptSandbox, ForbiddenLibrariesAndFunctionsFail) {
    ScriptHost host;
    EXPECT_TRUE(contains(runError(host, "io.open('x')"), "global 'io'"));
    EXPECT_TRUE(contains(runError(host, "os.time()"), "global 'os'"));
    EXPECT_TRUE(contains(runError(host, "require('x')"), "global 'require'"));
    EXPECT_TRUE(contains(runError(host, "load('return 1')"), "global 'load'"));
    EXPECT_TRUE(contains(runError(host, "math.random(3)"), "field 'random'"));
    EXPECT_TRUE(contains(runError(host, "debug.getinfo(1)"), "global 'debug'"));
    EXPECT_TRUE(contains(runError(host, "print('hi')"), "global 'print'"));
    EXPECT_EQ(runError(host, "local x = math.floor(2.5) + #string.rep('a', 3)"), "");
}

TEST(ScriptSandbox, GlobalsAndLibrariesAreReadOnly) {
    ScriptHost host;
    EXPECT_TRUE(contains(runError(host, "x = 1"), "scripts/test.lua:2: globals are read-only"));
    EXPECT_TRUE(contains(runError(host, "string.evil = 1"), "globals are read-only"));
    EXPECT_TRUE(contains(runError(host, "math.pi = 3"), "globals are read-only"));
    EXPECT_TRUE(contains(runError(host, "rawset(_ENV, 'x', 1)"), "global 'rawset'"));
    EXPECT_TRUE(contains(runError(host, "_G.x = 1"), "global '_G'"));
    // The freezing metatables can't be read or replaced.
    EXPECT_EQ(runError(host, "assert(getmetatable(_ENV) == false)\n"
                             "assert(getmetatable(string) == false)\n"
                             "assert(getmetatable('') == false)"),
              "");
    EXPECT_TRUE(contains(runError(host, "setmetatable(_ENV, nil)"), "protected metatable"));
    EXPECT_TRUE(contains(runError(host, "setmetatable(string, nil)"), "protected metatable"));
}

TEST(ScriptSandbox, InfiniteLoopsStopAtTheBudget) {
    ScriptHost host;
    EXPECT_TRUE(contains(runError(host, "while true do end"), "instruction budget exceeded"));
    // Catching the error doesn't help: once over budget, every instruction raises it again.
    EXPECT_TRUE(contains(
        runError(host, "while true do pcall(function() while true do end end) end"),
        "instruction budget exceeded"));
    EXPECT_EQ(runError(host, "local n = 0 for i = 1, 1000 do n = n + i end"), ""); // still usable
}

TEST(ScriptSandbox, MemoryIsCapped) {
    ScriptHost host;
    EXPECT_TRUE(contains(runError(host, "local s = string.rep('x', 100 * 1024 * 1024)"),
                         "not enough memory"));
    const std::string doubling = runError(host, "local s = 'x' while true do s = s .. s end");
    EXPECT_TRUE(contains(doubling, "not enough memory")) << doubling;
    EXPECT_EQ(runError(host, "local t = {} for i = 1, 100 do t[i] = i end"), ""); // recovered
}

TEST(ScriptSandbox, ReachableNamesAreExactlyTheAllowlist) {
    ScriptHost host;
    const std::vector<std::string> allowlist = {
        "_VERSION", "assert", "error", "getmetatable", "ipairs",
        "math.abs", "math.acos", "math.asin", "math.atan", "math.ceil", "math.cos", "math.deg", "math.exp", "math.floor", "math.fmod",
        "math.huge", "math.log", "math.max", "math.maxinteger", "math.min", "math.mininteger",
        "math.modf", "math.pi", "math.rad", "math.sin", "math.sqrt", "math.tan",
        "math.tointeger", "math.type", "math.ult",
        "next", "pairs", "pcall", "rawequal", "rawget", "rawlen", "select", "setmetatable",
        "string.byte", "string.char", "string.dump", "string.find", "string.format",
        "string.gmatch", "string.gsub", "string.len", "string.lower", "string.match",
        "string.pack", "string.packsize", "string.rep", "string.reverse", "string.sub",
        "string.unpack", "string.upper",
        "table.concat", "table.insert", "table.move", "table.pack", "table.remove",
        "table.sort", "table.unpack",
        "tonumber", "tostring", "type", "xpcall"};
    const std::vector<std::string> reachable = host.reachableNames();
    std::vector<std::string> extra, missing;
    std::ranges::set_difference(reachable, allowlist, std::back_inserter(extra));
    std::ranges::set_difference(allowlist, reachable, std::back_inserter(missing));
    for (const std::string& name : extra)
        ADD_FAILURE() << "reachable but not allowed: " << name;
    for (const std::string& name : missing)
        ADD_FAILURE() << "allowed but missing: " << name;
}

TEST(ScriptSandbox, ErrorsNameTheFileAndLine) {
    ScriptHost host;
    const auto syntax = host.load("scripts/bad.lua", "return {\n  take_turn = function(ctx\nend }");
    ASSERT_FALSE(syntax.has_value());
    EXPECT_TRUE(contains(syntax.error(), "scripts/bad.lua:3:")) << syntax.error();
    const auto notTable = host.load("scripts/number.lua", "return 42");
    ASSERT_FALSE(notTable.has_value());
    EXPECT_EQ(notTable.error(), "scripts/number.lua: the script must return a table");
    const auto loop = host.load("scripts/loop.lua", "while true do end");
    ASSERT_FALSE(loop.has_value()); // the top level runs under the budget too
    EXPECT_TRUE(contains(loop.error(), "instruction budget exceeded"));
    EXPECT_FALSE(host.call("scripts/missing.lua", "run").has_value());
}
