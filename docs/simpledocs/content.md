# Content (`data/*.json`, `src/game/content*.{hpp,cpp}`)

Seven files: `paths`, `items`, `skills`, `enemies`, `missions`, `recipes`, `story`. Keys are camelCase; enums are snake_case strings. `data/` reproduces the numbers in `build-core-engine`'s `reference/content-tables.md` (now under `openspec/changes/archive/`). Since the Act I re-skin its names, descriptions, flavor, title and story are the ones in `openspec/changes/archive/2026-10-11-reskin-act-one/reference/act-one-names.md` (for example `iron_sword` is shown as "Sword" and `health_potion` as "Holy Water"). IDs never change, so saves from before the re-skin load and simply show the new names. Missions are listed in file order: `avaritia` (Act I, level 15, enemy `avaritia`: 2200 HP / 160 ATK / 15 DEF / 5 LIFE with Medusa's fight pattern from `scripts/medusa.lua`, rewards 5000 EXP, 15000 gold, Gold Dust and Dark Pearl), then the bonus `the_gorgon` (Medusa, level 20, unchanged). Avaritia's stats are pinned by the `AvaritiaBalance` tests (see `docs/benchmarks.md`).

`loadContent(dataDir)` returns `std::expected<Content, std::vector<ContentError>>`.
- It reads every file and **collects all errors** before failing: malformed JSON (with line/column), unknown or missing keys, duplicate or badly formed ids, out-of-range numbers, unknown references, a recipe listing the same ingredient twice, kind mismatches, missing `fighter`/`hunter`/`rogue`.
- **Upper bounds:** every stat, price, cost, reward, count and level is at most `kMaxStat` (1,000,000); path `multiplier` is 1-100, skill `atkMultiplier` 0-10, `healPercentOfDamage` 0-100. These keep all battle and town arithmetic inside `int`/`int64`, so even extreme data can't overflow.
- Cross-reference checks only run when every file parsed.
- Enemy sprites are looked up in `<dataDir>/../assets/sprites/`.
- `formatError` renders `file: entry: message`.

Skills have `effect` or `script`, and enemies `behavior` (only `basic`, an attack every turn) or `script` (exactly one each). Medusa and Avaritia use `scripts/medusa.lua`. Referenced scripts are checked and kept as described in `scripting.md`. `Content` is immutable after loading. `item(id)` and friends assume the id exists; `findItem(id)` returns `nullptr` instead. Lists keep file order, which `rollDrop` depends on.

The JSON reading helpers (`Fields`, `readEnum`, `convert`) live in `src/game/json_fields.hpp` so the world loader shares them.

## World (`data/world/`, `src/game/world/`)

The Overworld is loaded at startup, after content and before sprites, by `world::loadWorld(dataDir, content)`. It reads three files and parses them with `parseWorld(map, worldJson, fieldJson, content)`. Errors are `ContentError`s named `world/overworld.txt`, `world/world.json` or `world/field.json`. Any error prints every problem and exits with code 2 before the terminal changes.
- **`overworld.txt`:** one character per tile, every row as long as the first (at most 1024×1024). A `\r` before each newline is ignored, and a final newline adds no row. Map errors carry the entry `line N` (1-based): a ragged row, or a character not in the legend (the first such character per line).
- **`world.json`:**
  - `legend`: one printable non-space character → `{terrain, walkable}`. The terrains are `grass`, `tall_grass`, `tree`, `water`, `path`, `sand`, `rock`, `wall` and `floor`.
  - `spawn`: `{x, y}`.
  - `landmarks`: `{name, x, y, opens}`, where `opens` holds exactly one of `service` (`almonry`, `forge`, `prior`, `thesaurus`), `training: true`, or `mission` (a mission id).
  - `zones`: `{name, x, y, w, h, level 1–30, count 1–50}`.
  - Placement checks run only when everything parsed. Spawn and landmarks must be on walkable tiles inside the map, zones inside the map, and every landmark reachable from spawn on foot (four directions).
- **`field.json`:** `heroStepMs` (1–10000), the time to walk one tile. It is converted to the nearest whole tick at 60 ticks per second, at least 1 (125 ms → 8 ticks).

The shipped map is a 72×30 starter area. It has the town plaza at spawn (35, 14), the Almonry, Forge, Prior and Thesaurus at the four building doors, the Catacombs (training) at the cave mouth to the north, the level 1 Glyphwick Meadow zone to the east, and the Lair of Avaritia inside it.

`world::World(def, state)` is the simulation. It starts the hero on spawn facing down, and forks `state.rng` once. `step(WorldInput)` advances one tick:
- Once the current move has finished, a held or requested direction turns the hero that way.
- If the next tile is walkable and inside the map, the hero moves there at once and emits `HeroMoved{from, to}`. The next move can start `ticksPerTile` ticks later.
- A refused move still turns the hero.
