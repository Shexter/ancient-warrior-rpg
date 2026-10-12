# Proposal

## Why

The original Heroes RPG (2021–22) is Windows-only, procedural, and stores all content in parallel arrays (see `DESIGN_SPEC.md`). It cannot be built on a clean machine and does not demonstrate modern C++. This change rebuilds it as a cross-platform terminal game on a small engine whose game logic is deterministic and independent of the UI. That separation is the foundation the dungeon, scripting, and LLM narrator changes all build on.

## What Changes

- New terminal renderer: cell framebuffer, diff-only redraw, truecolor half-block sprites, non-blocking keyboard input, fixed-timestep loop.
- Content (paths, items, skills, enemies, recipes, missions) loaded from JSON data files with validation instead of hard-coded arrays.
- Turn-based combat engine with status effects that replaces the original `turn` counter hacks; emits a stream of game events instead of printing.
- Hero progression: creation, leveling, equipment and upgrades, inventory, training cave.
- Town services: merchant, blacksmith (upgrade and forge), guildmaster (restore, stat purchase, skills), bank.
- Single versioned JSON save file. The original sign-in/sign-up accounts and Caesar-cipher "encryption" are dropped (single-player game).
- Headless balance simulator that runs N seeded battles and reports win rate and timing; doubles as the performance benchmark.
- CMake build, unit tests, sanitizer build, and CI on macOS, Linux, Windows.
- Bugs and quirks listed in `DESIGN_SPEC.md` ("Bugs / quirks to NOT carry over") are fixed, not ported.

## Capabilities

### New Capabilities
- `terminal-rendering`: drawing sprites and UI to the terminal, input handling, frame pacing.
- `game-content`: loading and validating data-driven game definitions.
- `combat`: turn-based battles, skills, status effects, and the battle event stream.
- `hero-progression`: hero creation, leveling, equipment, inventory, and training.
- `town-services`: merchant, blacksmith, guildmaster, and bank.
- `save-system`: persisting and restoring a playthrough.
- `balance-simulator`: headless batch battle simulation and benchmarking.

### Modified Capabilities
- None (greenfield).

## Impact

- New repository code under `src/`, `tests/`, `data/`, `tools/`; new `CMakeLists.txt` and CI workflow.
- New build dependencies fetched by CMake: a JSON library and a unit-test framework.
- Developer machines need CMake installed (not present on the author's Mac today).
- `heroes RPG/` stays as read-only reference; nothing builds it.
