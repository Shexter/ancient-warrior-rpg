# Procedural Dungeons — Implementation Reference

Builds on `build-core-engine`; its `reference/interfaces.md` §1 conventions apply. In particular, **no floating point**: slopes in field of view are integer fractions. If something here conflicts with a spec, the spec wins: report it as SPEC DRIFT.

## 1. Files

```
data/dungeon.json                         # §2
data/enemies.json                         # + 5 dungeon enemies (§3)
src/game/dungeon/floor.{hpp,cpp}          # types + generateFloor (§4)
src/game/dungeon/fov.{hpp,cpp}            # computeFov (§5)
src/game/dungeon/expedition.{hpp,cpp}     # state, step(), encounters (§6)
src/app/screens/expedition_screen.{hpp,cpp}
tests/dungeon_gen_test.cpp tests/fov_test.cpp tests/expedition_test.cpp tests/fixtures/dungeon/*.txt
```

## 2. `data/dungeon.json`

```json
{"dungeon": {
  "name": "The Catacombs", "width": 80, "height": 40, "floors": 5, "fovRadius": 8,
  "bsp": {"minLeafWidth": 12, "minLeafHeight": 8, "maxDepth": 4, "minRoomWidth": 4, "minRoomHeight": 3},
  "scalingPercentPerFloor": 25,
  "enemyCount": {"base": 3, "perFloor": 1, "random": 3},
  "chestCount": {"base": 1, "random": 2, "lastFloorBonus": 1},
  "safeRadius": 3,
  "pools": [
    {"floors": [1, 2], "enemies": [{"id": "rat", "weight": 60}, {"id": "bat_swarm", "weight": 40}]},
    {"floors": [3],    "enemies": [{"id": "bat_swarm", "weight": 40}, {"id": "skeleton", "weight": 60}]},
    {"floors": [4],    "enemies": [{"id": "skeleton", "weight": 50}, {"id": "cultist", "weight": 50}]},
    {"floors": [5],    "enemies": [{"id": "cultist", "weight": 100}]}
  ],
  "elite": {"floor": 5, "enemy": "gorgon_spawn"},
  "chestTables": [
    {"floors": [1, 2], "entries": [
      {"item": "health_potion", "weight": 40, "min": 1, "max": 2}, {"item": "topaz", "weight": 25, "min": 1, "max": 1},
      {"item": "fire_dust", "weight": 20, "min": 1, "max": 2}, {"item": "gold_leaf", "weight": 15, "min": 1, "max": 1}]},
    {"floors": [3, 4], "entries": [
      {"item": "mana_potion", "weight": 25, "min": 1, "max": 2}, {"item": "turtle_shell", "weight": 25, "min": 1, "max": 2},
      {"item": "frozen_tears", "weight": 20, "min": 1, "max": 1}, {"item": "black_pearl", "weight": 15, "min": 1, "max": 1},
      {"item": "broken_staff", "weight": 15, "min": 1, "max": 1}]},
    {"floors": [5], "entries": [
      {"item": "phoenix_feather", "weight": 30, "min": 1, "max": 1}, {"item": "pure_ice", "weight": 30, "min": 1, "max": 1},
      {"item": "broken_compass", "weight": 20, "min": 1, "max": 1}, {"item": "eternal_youth", "weight": 20, "min": 1, "max": 1}]}
  ]}}
```

Validation (added to the content loader, all errors collected):
- Every floor 1..`floors` is covered by exactly one pool and one chest table.
- Pool enemies and the elite exist in `enemies.json`. Chest items exist.
- Weights ≥ 1, and 1 ≤ min ≤ max.
- `minLeafWidth ≥ minRoomWidth + 2` and `minLeafHeight ≥ minRoomHeight + 2`.
- `width`/`height` are 40–200 and 20–100.

## 3. New enemies and behavior

Core's `EnemyBehavior` gains `Basic`: on each enemy turn it emits `Attacked{Enemy}`, then `hit(enemy→hero)`. It makes **no RNG draw**, no regen, and no extra turn. The death checks are identical to core §7 step 6.3.

| id | name | maxHp | atk | def | life | exp | gold | behavior | sprite |
|---|---|---|---|---|---|---|---|---|---|
| `rat` | Giant Rat | 200 | 60 | 5 | 0 | 150 | 100 | basic | monster.sprite |
| `bat_swarm` | Bat Swarm | 300 | 90 | 10 | 0 | 250 | 150 | basic | monster.sprite |
| `skeleton` | Skeleton | 600 | 150 | 40 | 0 | 500 | 300 | basic | monster.sprite |
| `cultist` | Cultist | 800 | 220 | 60 | 0 | 800 | 500 | basic | monster.sprite |
| `gorgon_spawn` | Gorgon Spawn | 1500 | 280 | 80 | 20 | 2000 | 1500 | medusa_pattern | medusa.sprite |

Floor scaling: `p = 100 + scalingPercentPerFloor × (floor − 1)`. Then `maxHp`, `atk`, `def`, `life`, `rewardExp`, and `rewardGold` are each `× p / 100` (integer). Dungeon enemies have no `rewardItems`.

## 4. Floor generation

```cpp
enum class Tile : std::uint8_t { Wall, Floor, StairsDown, StairsUp };
struct Pos { int x, y; auto operator<=>(const Pos&) const = default; };
struct Rect { int x, y, w, h; };
struct MapEnemy { std::string enemyId; Pos pos; bool awake = false; int sleepTurns = 0; bool elite = false; };
struct MapChest { Pos pos; bool opened = false; };
struct Floor { int width, height, index; std::vector<Tile> tiles; std::vector<Rect> rooms; Pos entry;
               std::optional<Pos> stairsDown; std::vector<MapEnemy> enemies; std::vector<MapChest> chests; };
Floor generateFloor(const DungeonDef&, const Content&, std::uint64_t expeditionSeed, int floorIndex);  // floorIndex 1-based
std::vector<int> bfsDistances(const Floor&, Pos from);   // −1 = unreachable; walls block; 4-neighbour
std::string renderAscii(const Floor&);                   // tests/fixtures: '#','.','>','<','e','E','$'
```

`rng = Rng::stream(expeditionSeed, floorIndex)`. All draws come from this `rng`, in exactly this order:

1. **BSP split.** Call `split({0, 0, width, height}, depth 0)` recursively, left child before right:
   - If `depth == maxDepth`, the rect is a leaf.
   - `canV = w ≥ 2·minLeafWidth`. `canH = h ≥ 2·minLeafHeight`. If neither, it's a leaf.
   - Direction:
     - Only `canV` → V. Only `canH` → H.
     - Otherwise `4w ≥ 5h` → V. Otherwise `4h ≥ 5w` → H.
     - Otherwise `below(2) == 0 ? V : H`.
   - V: `cut = minLeafWidth + below(w − 2·minLeafWidth + 1)`. The children are `{x, y, cut, h}` and `{x+cut, y, w−cut, h}`. H works the same way on heights.
2. **Rooms.** For each leaf in creation order:
   - `rw = minRoomWidth + below(w − 2 − minRoomWidth + 1)`.
   - `rh = minRoomHeight + below(h − 2 − minRoomHeight + 1)`.
   - `rx = x + 1 + below(w − 2 − rw + 1)`.
   - `ry = y + 1 + below(h − 2 − rh + 1)`.
   - Carve the room to Floor and append it to `rooms`.
3. **Corridors.**
   - Post-order over internal nodes (children first, left before right). Each node connects `center(rep(left))` to `center(rep(right))`.
   - `rep(leaf)` is its room. `rep(internal) = rep(left child)`.
   - `center = (rx + rw/2, ry + rh/2)`.
   - `below(2) == 0` → horizontal leg first (along y of the start, then vertical along x of the end). Otherwise vertical first.
   - Carve every tile of both legs to Floor.
4. **Entry and stairs.**
   - `entry = center(rooms[0])`. On floor 1 that tile becomes `StairsUp`.
   - Run BFS from `entry`.
   - Unless this is the last floor, `stairsDown` is the room center with the greatest distance (ties go to the lower room index), and that tile becomes `StairsDown`.
   - On the last floor, that same room center is the **elite spot**.
5. **Enemies.**
   - Candidates: tiles that are exactly `Floor`, with `dist > safeRadius`, not the elite spot, in row-major order.
   - Last floor: first place the elite (`elite = true`) at the elite spot.
   - Then `n = base + perFloor·(floor − 1) + below(random)`. Repeat `n` times while candidates remain:
     - `i = below(size)`. Remove `candidates[i]` with an order-preserving erase.
     - Pick the kind with a weighted pick from the floor's pool: `r = below(totalWeight)`, then walk the cumulative weights in file order.
     - Append the enemy.
6. **Chests.**
   - Candidates are the remaining enemy candidates (the same list after step 5's removals).
   - `n = base + below(random) + (last floor ? lastFloorBonus : 0)`.
   - Same pick-and-erase as step 5. Append chests.

A chest's contents are rolled when it's **opened**, using the expedition RNG (§6), not here.

Invariants tested over 10,000 seeds:
- All Floor-type tiles are reachable from `entry`.
- `stairsDown` exists on non-last floors.
- No two entities share a tile, and no entity sits on entry or stairs.
- Every enemy has `dist > safeRadius`.
- Border tiles are walls.

## 5. Field of view: symmetric shadowcasting (Albert Ford, 2020), integers only

```cpp
void computeFov(const Floor&, Pos origin, int radius, std::vector<bool>& visible);  // clears then fills
```
- Slopes are fractions `num/den` with `int` parts. Compare `a/b` with `c/d` as `a·d` vs `c·b` (all denominators positive).
- `visible[origin] = true`. For each quadrant (N, E, S, W), call `scan(depth = 1, start = −1/1, end = 1/1)`.
- Quadrant transforms `(depth, col) → (x, y)`:
  - N `(ox + col, oy − depth)`.
  - S `(ox + col, oy + depth)`.
  - E `(ox + depth, oy + col)`.
  - W `(ox − depth, oy + col)`.
- Out-of-bounds counts as a wall.

```
scan(depth, start, end):
    if depth > radius: return
    minCol = roundTiesUp(depth × start)     # floor((2·depth·start.num + start.den) / (2·start.den))
    maxCol = roundTiesDown(depth × end)     # ceil((2·depth·end.num − end.den) / (2·end.den))
    prevWall = none
    for col in minCol..maxCol:
        wall = isWall(depth, col)
        if wall or isSymmetric(depth, col, start, end):
            if col² + depth² ≤ radius²: reveal(depth, col)
        if prevWall == true and not wall:  start = (2·col − 1) / (2·depth)
        if prevWall == false and wall:     scan(depth + 1, start, (2·col − 1) / (2·depth))
        prevWall = wall
    if prevWall == false: scan(depth + 1, start, end)

isSymmetric(depth, col, start, end): col ≥ depth × start  and  col ≤ depth × end   (exact fraction compare)
```
- Floor division must round toward −∞. Write a `floorDiv` helper; C++ `/` truncates toward 0, which breaks negative columns.
- Tests:
  - Symmetry: for random floors and random floor-tile pairs A and B within radius, A sees B exactly when B sees A.
  - A hand-built 11×7 room with a pillar matches the expected ASCII in `tests/fixtures/dungeon/fov_pillar.txt`. Create the expected picture by hand from the algorithm and review it carefully. If the test and the picture disagree, re-derive by hand before changing either one.

## 6. Expedition

```cpp
struct Expedition { std::uint64_t seed; Floor floor; Pos hero; std::vector<bool> explored, visible; Rng rng; std::string message; };
enum class ExpAction { North, East, South, West, Wait, UseStairs };
namespace xev {
struct Moved { Pos to; }; struct BumpedWall {}; struct EnemyWoke { int index; }; struct EnemyMoved { int index; Pos to; };
struct ChestOpened { std::string itemId; int qty; bool lostBagFull; }; struct Descended { int floor; }; struct Left {};
}
using ExpEvent = std::variant<xev::Moved, xev::BumpedWall, xev::EnemyWoke, xev::EnemyMoved, xev::ChestOpened, xev::Descended, xev::Left>;
struct StepResult { std::vector<ExpEvent> events; std::optional<int> encounterEnemy; };
std::expected<Expedition, TownError> startExpedition(GameState&, const Content&);   // canAdventure gate; seed = fork().next() pair
StepResult step(Expedition&, GameState&, const Content&, ExpAction);
Combatant encounterCombatant(const Expedition&, const Content&, int enemyIndex);   // scaled EnemyDef
void resolveEncounter(Expedition&, GameState&, const Content&, int enemyIndex, const Battle&);
```

- `startExpedition`:
  - `auto f = state.rng.fork()`. `seed = (u64(f.next()) << 32) | f.next()`.
  - `rng = Rng::stream(seed, 0)`. Note that floors use `stream(seed, floorIndex ≥ 1)`, so streams never collide.
  - Generate floor 1. The hero starts at entry. Compute FOV.

`step(action)`:
1. **Moves.**
   - Into a wall or out of bounds → `BumpedWall`. Return with no turn passing.
   - Into an enemy → `encounterEnemy = index`. Return without an enemy phase.
   - Otherwise move (`Moved`).
   - If the new tile has an unopened chest, open it:
     - Weighted pick from the floor's chest table with `rng`, then `qty = min + rng.below(max − min + 1)`.
     - Add it to the bag, or report `lostBagFull`.
     - Emit `ChestOpened`.
2. **`UseStairs`.**
   - On `StairsDown`: generate the next floor, put the hero at its entry, emit `Descended`, recompute FOV. Return with no enemy phase.
   - On `StairsUp` (floor 1 only): emit `Left` and return. The app ends the expedition and the hero keeps everything.
   - Anywhere else: the message "There are no stairs here." with no turn passing.
3. **`Wait`** passes a turn.
4. **FOV.** Recompute `visible`, then `explored |= visible`.
5. **Enemy phase.**
   - Compute `bfsDistances(floor, hero)` once.
   - For each enemy in vector order:
     - If `sleepTurns > 0`: decrement it and skip.
     - If not awake and `visible[pos]`: set awake and emit `EnemyWoke`.
     - If awake and Manhattan distance to the hero is 1: set `encounterEnemy = index` and **stop the phase**.
     - If awake otherwise: try neighbours in the order N, E, S, W. Move to the first with `dist == dist[pos] − 1` that isn't occupied by another enemy, and emit `EnemyMoved`. If none qualifies, stay.

`resolveEncounter`:
- **Victory:** remove the enemy and grant its scaled exp (`grantExp`) and gold.
- **Defeat:** `hp = 0`, `exp = max(0, exp − scaled rewardExp)`, and the expedition ends (the app drops it). Gear is kept.
- **Fled or Draw:** the enemy gets `awake = false, sleepTurns = 3`.
- In every case, copy HP and MP back from the battle.

Battles are constructed with `Battle(content, heroCombatant(...), scaledEnemyDef, skills, exp.rng.fork())`. Build `scaledEnemyDef` as a temporary `EnemyDef` copy with the scaled stats.

## 7. Screen

```
row 0      THE CATACOMBS — Floor 2/5                                   (accent)
rows 1–19  map viewport 80×19; camera top = clamp(hero.y − 9, 0, height − 19); camera left = clamp(hero.x − 40, 0, width − 80)
row 20     ────────────────────────────────────────────────────────────────── (border)
row 21     ARIA HP ████████████░░░░░░░░ 250/350   MP ██████░░ 30/50   Gold 20000
row 22     last message (e.g. "You found 2 Health Potion.")
row 23     dim: arrows/WASD move · space wait · Enter stairs · Esc menu
```

| thing | glyph | fg (lit) | fg (remembered) |
|---|---|---|---|
| wall | `#` | 150,140,120 | 70,65,60 |
| floor | `.` | 100,100,115 | 45,45,55 |
| stairs | `>` `<` | accent | dim |
| chest | `$` | gold | (only while visible) |
| hero | `@` | accent | — |
| enemy | first letter of the enemy **id** (`r` rat, `b` bat_swarm, `s` skeleton, `c` cultist) | dmg | (only while visible) |
| elite | `G` | 255,80,80 | (only while visible) |

Never-seen tiles are blank (bg). Keys:
- Arrows or WASD: move.
- Space: Wait.
- Enter: UseStairs.
- Esc: a Menu with Resume, Save, and Inventory (read-only bag list).

An encounter opens the core Battle screen. Returning from it runs `resolveEncounter` and comes back to the map, or to the Hub on defeat.

## 8. Save v2

The `hero` object is unchanged. A top-level `"version": 2` adds `"expedition": null` or:

```json
{"seed": "0x…", "floor": 2, "hero": [12, 30], "rng": {"state": "0x…", "inc": "0x…"},
 "explored": "<hex of packed bits, row-major, LSB first, width×height bits>",
 "enemies": [{"id": "rat", "pos": [40, 11], "awake": true, "sleepTurns": 0, "elite": false}],
 "chestsOpened": [true, false]}
```

- Loading regenerates the floor from `(seed, floor)`, then replaces its enemy list with the saved one and applies `chestsOpened` by index.
- A version 1 file loads with `expedition = null`. Version 3 or higher → `NewerVersion`.
- Saving writes version 2 always.
