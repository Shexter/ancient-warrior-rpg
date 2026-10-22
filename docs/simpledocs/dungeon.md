# Dungeon (`src/game/dungeon/`)

Expeditions are being built (OpenSpec `add-procedural-dungeons`). So far there's the data, floor generation and field of view; nothing in the game uses them yet.

## Data (`data/dungeon.json`, `dungeon_def.*`)

- `loadDungeon(dataDir, content)` reads `dungeon.json` and `parseDungeon` validates it, collecting every error in the file "dungeon.json" with entries like `pool #1` or `chest table #2`. It's a loader of its own (like the world's), not part of `loadContent`, and mods can't change the file.
- Fields: `name`, `width` 40–200, `height` 20–100, `floors`, `fovRadius`, `bsp` (`minLeafWidth`, `minLeafHeight`, `maxDepth`, `minRoomWidth`, `minRoomHeight`; a leaf must be at least 2 wider and taller than the smallest room), `scalingPercentPerFloor`, `enemyCount` (`base`, `perFloor`, `random` ≥ 1), `chestCount` (`base`, `random` ≥ 1, `lastFloorBonus`), `safeRadius`, `pools`, `elite`, `chestTables`.
- Every floor from 1 to `floors` must be in exactly one pool and one chest table. Pool enemies and the elite must exist in `enemies.json`, and chest items in `items.json`. Weights are at least 1 and 1 ≤ `min` ≤ `max`. The elite must be on the last floor.
- **Enemies:** `enemies.json` holds five dungeon enemies: Giant Rat, Bat Swarm, Skeleton and Cultist attack plainly (`basic`, no random draws), and the Gorgon Spawn (the floor 5 elite) runs `scripts/medusa.lua`.
- `scaledEnemy(enemy, def, floor)` multiplies maxHp, atk, def, life and the exp and gold rewards by `(100 + 25 × (floor − 1)) / 100` (rounded down) and drops reward items.

## Floors (`floor.*`)

`generateFloor(def, expeditionSeed, floor)` builds one floor from `Rng::stream(expeditionSeed, floor)` alone, in a fixed draw order:

1. **Split** the map recursively (binary space partition) until `maxDepth` or too small. A wide area splits vertically and a tall one horizontally; a near-square area flips a coin.
2. **One room per leaf**, inside a one-tile margin.
3. **Corridors:** each split joins a room from either side with an L-shaped corridor, children first, so every room is connected.
4. **Entry and stairs:** the hero arrives at the first room's centre (stairs up on floor 1). Stairs down go in the room centre farthest from it by walking distance. On the last floor that spot holds the elite instead.
5. **Enemies** go on plain floor tiles more than `safeRadius` steps from the entry: `base + perFloor × (floor − 1) + random(0..random−1)`, each kind a weighted pick from the floor's pool.
6. **Chests** go on the tiles left over: `base + random(0..random−1)`, plus `lastFloorBonus` on the last floor. What's inside is rolled when a chest is opened (not yet built).

`bfsDistances(floor, from)` gives walking steps to every tile (−1 if unreachable). `renderAscii` draws a floor as `#` `.` `>` `<` `e` `E` `$`.

## Field of view (`fov.*`)

`computeFov(floor, origin, radius, visible)` is symmetric shadowcasting (Albert Ford, 2020) in four quadrants, with integers only: slopes are fractions compared by cross-multiplying, and `floorDiv` rounds toward minus infinity where C++ `/` would round toward zero. The origin is always seen. A tile is seen when it's within `dx² + dy² ≤ radius²` and either a wall in view or a floor tile whose centre lies inside the lit arc, so if A sees B then B sees A. Walls block, and off the map counts as wall.

`tests/fov_test.cpp` checks `floorDiv`, that the origin is seen, that an open room shows exactly the radius-8 circle, the pillar room against `tests/fixtures/dungeon/fov_pillar.txt` (worked out with an independent Python version of the algorithm using exact fractions), and symmetry for 50 random pairs on each of 200 generated floors.

## Tests (`tests/dungeon_gen_test.cpp`)

- Loader errors: an unknown pool enemy, an uncovered floor, and min > max, each named.
- A Basic enemy attacks once a turn and the battle comes out the same whatever the seed.
- Over 10,000 seeds: border walls, every open tile reachable, and stairs on every floor but the last.
- Over 2,000 seeds: no two entities share a tile, none stands on the entry or stairs, enemies are outside the safe radius, and the elite is only on the last floor.
- Floor 5's enemies average more HP than floor 1's, and the same seed gives the same floor.
- `seed1234_f1.txt` and `seed1234_f5.txt` in `tests/fixtures/dungeon/` pin the output exactly. Rewrite them with `HEROES_WRITE_FIXTURES=1` after an intended change.

`heroes-sim --bench-dungeon N` times N floors (`docs/benchmarks.md`).
