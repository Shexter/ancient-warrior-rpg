# Dungeon (`src/game/dungeon/`)

Expeditions are being built (OpenSpec `add-procedural-dungeons`). The dungeon data, floor generation, field of view, the expedition rules, its save and the Expedition screen are built. Startup loads `dungeon.json` after the world (an error stops startup with exit 2) and puts it on the Session.

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

## Expedition (`expedition.*`)

- `startExpedition(state, def)` refuses a fallen hero (`HeroDead`). Otherwise it forks the game's RNG once, takes a 64-bit seed from two draws, puts the hero on floor 1's entry and looks around. Its own RNG (`Rng::stream(seed, 0)`, for chests and battles) never collides with the floors' streams (1 and up).
- `step(exp, state, content, def, action)` takes one action:
  - **A move** into a wall reports `BumpedWall`, and no turn passes. Into an enemy it returns `encounterEnemy` with no enemy turn. Otherwise the hero moves, and an unopened chest there opens: a weighted pick from the floor's chest table, then a quantity from `min` to `max`, into the bag ("You found 2 Health Potion."), or lost if the bag is full.
  - **The stairs:** down builds the next floor and puts the hero on its entry; up (floor 1 only) reports `Left`; anywhere else says "There are no stairs here." None of these pass a turn.
  - **Wait** passes a turn.
  - After a move or a wait, sight is recomputed and added to `explored`. Then each enemy in order: asleep, it counts down; seen, it wakes; awake and next to the hero, it starts the encounter and the phase stops; awake otherwise, it steps one tile closer by walking distance (trying north, east, south, west), unless another enemy is there.
- `encounterEnemy` is the enemy scaled to the floor. `resolveEncounter` applies the finished battle. Victory removes it and pays its exp and gold. Defeat leaves the hero at 0 HP with that much exp taken (not below 0) and their gear, and the caller ends the run. Fleeing or a draw puts it to sleep for 3 turns. HP and MP always come back from the battle.
- `toSave`/`fromSave` turn it into `GameState::expedition` and back (`save.md`). `fromSave` rebuilds the floor from (seed, floor), applies the saved enemies and opened chests, and recomputes sight. It gives nothing when the save doesn't fit the dungeon.

`tests/expedition_test.cpp` covers each rule above on hand-built floors: a wall bump moving no one, explored only growing, a rat 5 steps round a corner reaching the hero after 4 waits and fighting on the 5th, no path, two rats in a corridor, chests and a full bag, both stairs, encounters both ways, each battle outcome, and a save round trip mid floor 2.

## Screen (`src/app/screens/expedition.*`)

- The Hub's Expedition opens `ExpeditionScreen`. It resumes the game's saved run when `fromSave` accepts it ("The saved expedition no longer fits the Catacombs; a new one begins." otherwise), else starts a new one. After every turn the run is copied into `GameState::expedition` and the game is marked unsaved, so the Hub's Save, the screen's own Save and Continue all resume it.
- **Layout** (80×24): row 0 `THE CATACOMBS — Floor 2/5` in the accent colour. Rows 1–19 are an 80×19 window on the floor, its top-left `clamp(hero − (40, 9))` so it stops at the map edges. Row 20 is a `─` rule, row 21 the hero's name, HP and MP bars and gold, row 22 the last message, row 23 the keys in dim.
- **Tiles** never seen stay blank. Seen ones are lit while in sight and dimmed from memory: walls `#` (150,140,120 / 70,65,60), floor `.` (100,100,115 / 45,45,55), stairs `>` `<` (accent / dim). Chests `$` (gold) and enemies show only while in sight: an enemy is its id's first letter in the damage colour (`r` rat, `b` bat swarm, `s` skeleton, `c` cultist), the elite `G` (255,80,80). The hero is `@`.
- **Keys:** arrows or WASD move, Space waits, Enter takes the stairs, Esc opens Resume / Save / Inventory. Save asks nothing; if another save is there that this session didn't load or write, it says to save from the Hub instead. Inventory lists the bag.
- **An encounter** opens the Battle screen (titled "The Catacombs · Floor N", the enemy's battle sprite), with the hero's skills and carried consumables (`encounterBattle`). When it ends, `resolveEncounter` applies it at once. The screen comes back to the map, or to the Hub when the hero fell ("You fell in the Catacombs. Visit the Prior to be revived."), with the run gone. The stairs up on floor 1 return to the Hub ("You climb out of the Catacombs.") with everything found kept.
- `tests/expedition_screen_test.cpp`: the Hub opens floor 1 with the layout above; saving mid-run then Continue shows the same map; a fight opens the Battle and fleeing returns to the same spot with the rat asleep; falling returns to the Hub with no run left.

## Tests (`tests/dungeon_gen_test.cpp`)

- Loader errors: an unknown pool enemy, an uncovered floor, and min > max, each named.
- A Basic enemy attacks once a turn and the battle comes out the same whatever the seed.
- Over 10,000 seeds: border walls, every open tile reachable, and stairs on every floor but the last.
- Over 2,000 seeds: no two entities share a tile, none stands on the entry or stairs, enemies are outside the safe radius, and the elite is only on the last floor.
- Floor 5's enemies average more HP than floor 1's, and the same seed gives the same floor.
- `seed1234_f1.txt` and `seed1234_f5.txt` in `tests/fixtures/dungeon/` pin the output exactly. Rewrite them with `HEROES_WRITE_FIXTURES=1` after an intended change.

`heroes-sim --bench-dungeon N` times N floors (`docs/benchmarks.md`).
