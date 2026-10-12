# Design

## Context

This change builds on `build-core-engine`:
- Combat is `Battle::act(action) -> events`.
- Skills are data-driven `SkillEffect` entries.
- Medusa's turn is native `EnemyBehavior::MedusaPattern`.
- Seeded golden event-sequence tests exist (`build-core-engine/reference/golden-vectors.md`). They are the parity oracle for this change.

It is independent of `add-procedural-dungeons`. If dungeons ship first, Gorgon Spawn also uses the Medusa script, and the save version bump is relative to whatever is current.

The implementation detail is in `reference/lua-interfaces.md`: pinned URLs and hashes, CMake for Lua, the ScriptHost sandbox, the full ctx API, the Medusa script, transactional failure semantics, mod rules, and the example mod.

| chunk | read |
|---|---|
| 1 Runtime and sandbox | §1, §3 |
| 2 API and fallbacks | §4, §5 |
| 3 Port Medusa | §4 (medusa.lua), §8 |
| 4 Mods | §6, §7 |

Chunk 1 (sandbox) and chunk 2 (transactional rollback) get a closer review before approval.

## Goals / Non-Goals

**Goals:**
- New enemy behaviors and custom skills need no C++ change, and are sandboxed and deterministic.
- One implementation of each behavior: when Medusa moves to Lua, the C++ pattern is deleted.
- Measured scripting overhead in the simulator.

**Non-Goals:**
- Scripting UI, screens, dungeon generation, or town services.
- Re-scripting the five data-driven skills (their data form is simpler and already tested).
- Hot reload, a mod browser, downloads, or signing.
- Mods changing `dungeon.json` (v1 limitation).

## Decisions

### D1. Lua 5.4.7 + sol2 v3.5.0
- Lua is small, embeddable, and the industry default for game scripting. sol2 gives type-safe bindings without manual stack code.
- Both are pinned by URL and SHA-256. A planning spike compiled them warning-free on Apple clang 21 in C++23 mode.
- *Alternatives:*
  - The raw Lua C API: no binding dependency, but verbose stack handling. It's the fallback if sol2 ever blocks a compiler upgrade.
  - LuaJIT: Lua 5.1 semantics and spotty arm64 support.
  - Wren or AngelScript: smaller communities.

### D2. One sandboxed `lua_State` per thread
- Scripts load once per `ScriptHost`. Each call gets a fresh ctx bound to one `Battle`.
- `_G` is frozen, so no state leaks between battles or simulator runs.
- Simulator threads each own a host. Lua states are not thread-safe.

### D3. Sandbox by allowlist
- Open only `base`, `string`, `table`, and `math`, with the dangerous base functions and `math.random`/`randomseed` removed.
- There is no `io`, `os`, `package`, `debug`, `coroutine`, or `utf8`.
- A test enumerates every reachable global and compares it to the allowlist, so a sol2 or Lua upgrade can't silently widen it.

### D4. Instruction budget and memory cap
- A count hook aborts after ≈1M VM instructions. A tracking allocator caps each state at 8 MiB.
- Both surface as ordinary Lua errors, so there is one failure path.

### D5. Transactional failure
- Before running a script, `Battle` snapshots its combatants, its RNG, and the event count.
- On failure it restores the snapshot. Enemies then fall back to the native basic attack. Skills are rejected (`ActionError::ScriptFailed`) with MP unspent.
- No partial effects, and determinism is preserved because the RNG is part of the snapshot.

### D6. Mods as registry layers
- Mods merge over base content by ID, ordered by priority then name.
- Validation runs on the merged result. A failing mod is dropped whole and the merge is redone.
- Saves record the active mods (name and version), so a missing mod is detected before load.

## Risks / Trade-offs

- [Scripted battles are slower] → Measured by the simulator against the native baseline. The target is ≥ 25%, and the reference lists the mitigation.
- [A sandbox escape through a binding] → Only listed ops are bound. The reachable-globals test fails on anything unexpected, and script paths can't leave their mod folder.
- [Lua numbers are floats] → All ops use `luaL_checkinteger`, which rejects non-integral numbers. Integer arithmetic in scripts uses `//`. The parity tests run on all three CI platforms.
- [Shared script table state across battles] → `_ENV` writes are blocked, and the documented pattern is to derive state from `ctx` (for example `ctx.turn`) rather than captured locals.
