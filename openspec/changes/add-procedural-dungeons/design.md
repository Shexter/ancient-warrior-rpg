# Design

## Context

This change builds on `build-core-engine`: the PCG32 RNG with stream derivation, content registries, the `Battle` action/event engine, inventory rules, the framebuffer, and save v1. Dungeon logic follows the same pattern as combat: it takes actions, returns events, and never renders.

The implementation detail is in `reference/dungeon-interfaces.md`: data file, new enemies and the `Basic` behavior, the exact generation algorithm and RNG draw order, integer-only shadowcasting, expedition turn rules, screen layout, and save v2. Chunk reading: 1 → §2–§4; 2 → §5; 3 → §6; 4 → §3, §6, §8; 5 → §7. Chunks 1 and 2 get a closer review before approval (determinism, FOV correctness).

## Goals / Non-Goals

**Goals:**
- Map generation, field of view, and pathfinding small enough to read, each unit-tested and benchmarked.
- Map logic stays headless so the simulator, and later the narrator, can drive it.

**Non-Goals:**
- Real-time movement, diagonal movement, ranged enemies, traps, doors, keys.
- Persistent world between expeditions (each expedition gets a new seed).
- Minimap or pixel-art tiles (the map is drawn with glyphs and colors).

## Decisions

### D1. BSP rooms plus L-shaped corridors
Recursively split the floor rectangle, place one room per leaf, and connect sibling leaves with L-shaped corridors. Sibling connection guarantees a spanning tree, which makes the floor connected by construction. A flood-fill check runs in tests over 10,000 seeds.
*Alternatives:* cellular-automata caves (need a post-pass to connect regions, and their organic shapes fit corridors and rooms less well); wave function collapse (more code than the payoff here).

### D2. Recursive shadowcasting for field of view
Symmetric recursive shadowcasting in 8 octants, radius 8. Visibility is a per-floor bitset recomputed after each hero move. The explored set is OR-ed in.
*Alternative:* raycasting to every edge tile. It's simpler but asymmetric and leaves artifacts at corners.

### D3. Breadth-first search for pursuit, not A*
All moves cost the same, so a single BFS from the hero each turn gives every awake enemy its next step (a Dijkstra map). That is O(tiles) once per turn, instead of one A* per enemy, and it's simpler.
*Alternative:* A* per enemy. It's the better fit if weighted terrain is added. Upgrade if that happens.

### D4. Still no ECS
A floor holds a few dozen entities in a `std::vector<MapEntity>` (position, kind, enemy ID or chest table, awake flag). Revisit with a profile showing entity iteration costs something. At this count it doesn't.

### D5. Floor seed derivation
`floorSeed = splitmix64(expeditionSeed ^ floorIndex)`. A floor can be regenerated from (expedition seed, floor) alone, so saves store only those plus deltas: explored bitset, defeated enemies, opened chests, hero position.

### D6. Save v2
Add an optional `expedition` object, and bump `version` to 2. The loader accepts 1 (no expedition) and 2.

## Risks / Trade-offs

- [BSP layouts look repetitive] → Vary room size and split ratios from data. This is acceptable for a first version.
- [A battle mid-expedition interrupts flow] → The battle screen returns to the map in place, with the map state unchanged apart from the removed enemy.
- [A save-format bump breaks old saves] → A load test with a checked-in v1 fixture.
