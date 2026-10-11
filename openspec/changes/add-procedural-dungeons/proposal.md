# Proposal

## Why

The original game's world is a set of menus: training is a number prompt, and missions are a single fight. A generated dungeon gives the hero a place to explore between battles. It also adds the classic game-programming algorithms (map generation, field of view, pathfinding), which are what make an engine project read as engine work.

## What Changes

- New "Expedition" option in the main menu that enters a procedurally generated, multi-floor dungeon.
- Seeded generation of rooms and corridors with guaranteed connectivity and stairs between floors.
- Turn-based grid movement with field of view and remembered explored tiles.
- Enemies on the map that pursue the hero using pathfinding. Moving into an enemy starts a battle using the existing combat engine.
- Treasure chests and floor-scaled rewards that draw from the existing item and drop data.
- The training cave stays as-is. Expeditions are an additional way to earn EXP, gold, and items.

## Capabilities

### New Capabilities
- `dungeon-generation`: deterministic generation of multi-floor dungeon layouts and their contents.
- `dungeon-exploration`: moving through a dungeon, visibility, enemy pursuit, encounters, loot, and leaving.

### Modified Capabilities
- None. Depends on `build-core-engine` (combat, game-content, hero-progression, terminal-rendering) being implemented first.

## Impact

- New code under `src/game/dungeon/` and a new screen in `src/app/`.
- New data: `data/dungeon.json` (floor sizes, enemy pools per depth, chest tables).
- The save format gains an optional in-progress expedition section (save version bump to 2 with v1 still loadable).
- No new third-party dependencies.
