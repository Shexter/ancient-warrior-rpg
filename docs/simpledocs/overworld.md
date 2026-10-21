# Overworld (`data/world/`, `src/game/world/`, `src/app/screens/overworld.*`)

## World data and simulation (`data/world/`, `src/game/world/`)

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

## Screen (`OverworldScreen`)

- **Opening and closing:** the Hub's first entry, **Explore**, pushes it. Explore is disabled with the Training reason when the hero can't adventure, with "Needs an 80×24 screen" when the last Hub frame was the phone layout, and with "No world loaded" if the session has no world (only possible in tests). Each opening builds a new `World`, so the hero starts on spawn every time. Esc (a press) pops back to the Hub, which keeps its selection.
- **Input:** the screen opts into key actions (`wantsKeyActions`). Each step feeds the keys to a `HeldKeys` and steps the world once with its direction. See `terminal.md` for plain terminals versus release reporting.
- **Canvas:** the terminal size clamped to 80×24–200×60, or 80×24 under `--layout compact`. Below 80×24 the loop falls back to its resize prompt.
- **Camera:** the map tile at the canvas's top-left. Drawing updates it, since only drawing knows the canvas size. The first frame centres it on the hero. After that, on each axis, it moves only when the hero leaves the middle third of the view (`view / 3` cells from each side), and it clamps to the map. A map narrower or shorter than the view is centred, and the margin is the background color with blank cells.
- **Terrain:** each tile is drawn with a character from its terrain's ramp, picked by a hash of its position. Its foreground is the terrain's color at 88–112% brightness from the same hash, on the terrain's dark background. So a tile always looks the same:

  | Terrain | Ramp |
  |---|---|
  | grass | `` .,'`.. `` |
  | tall grass | `ilvY;:` |
  | tree | `YT^` |
  | water | `~-~=` |
  | path | `.:.` |
  | sand | `.:,` |
  | rock | `^nA` |
  | wall | `#=` |
  | floor | `.:` |

- **The hero** is `@` in the accent color over its tile's background.
- **Scroll cost:** see `docs/benchmarks.md` (at most about 233 KB per step at 200×60).
