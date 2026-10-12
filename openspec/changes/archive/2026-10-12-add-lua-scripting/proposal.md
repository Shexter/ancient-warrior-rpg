# Proposal

## Why

After `build-core-engine`, item, enemy, and skill numbers live in data. But anything with logic — an enemy's turn pattern, or a skill that does something the fixed skill-effect fields can't express — still needs C++ and a recompile. Embedding Lua moves that behavior into scripts and opens the game to player mods. Adding a sandboxed scripting runtime to a deterministic engine is also a well-known engine problem.

## What Changes

- Enemy turn behavior can be a Lua script referenced from enemy data. Medusa's pattern moves from C++ to `data/scripts/medusa.lua` and must reproduce the core golden battle sequences exactly. The C++ version is deleted. The basic attack stays native as the fallback.
- A skill can be defined by a Lua script instead of the data-driven effect fields. The five original skills stay data-driven.
- Scripts run in a sandbox with no file, OS, module, or debug access, an instruction budget, a memory cap, and only engine-provided randomness, so battles stay deterministic. A failing script never crashes the game.
- A `mods/` folder lets players add or override items, enemies, skills, missions, and scripts without rebuilding. An example mod (a Cyclops mission with a scripted enemy and skill) ships in the repo.
- The balance simulator reports throughput with scripted enemies alongside the native baseline.

## Capabilities

### New Capabilities
- `content-scripting`: defining enemy behavior and custom skills in sandboxed, deterministic scripts.
- `mod-support`: discovering, validating, and loading player mods that add or override content.

### Modified Capabilities
- None. Battle behavior is unchanged: the scripted Medusa must reproduce the existing event sequences. Depends on `build-core-engine`.

## Impact

- New dependencies via FetchContent with pinned hashes: Lua 5.4.7 (built as a C static library) and sol2 v3.5.0 (header-only C++ binding). A planning spike confirmed they compile cleanly on the target toolchain.
- New `src/game/script/`, `data/scripts/`, and `mods/example-cyclops/`. Enemy and skill definitions gain an optional `script` field. New battle event types for script messages and script errors, and a new action error for failed skill scripts.
- The save format gains the list of active mods (version bump).
- Combat throughput drops for scripted enemies. The simulator measures how much.
