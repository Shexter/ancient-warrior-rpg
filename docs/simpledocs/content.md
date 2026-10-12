# Content (`data/*.json`, `src/game/content*.{hpp,cpp}`)

Seven files: `paths`, `items`, `skills`, `enemies`, `missions`, `recipes`, `story`. Keys are camelCase; enums are snake_case strings. `data/` reproduces the numbers in `build-core-engine`'s `reference/content-tables.md` (now under `openspec/changes/archive/`). Since the Act I re-skin its names, descriptions, flavor, title and story are the ones in `openspec/changes/archive/2026-10-11-reskin-act-one/reference/act-one-names.md` (for example `iron_sword` is shown as "Sword" and `health_potion` as "Holy Water"). IDs never change, so saves from before the re-skin load and simply show the new names. Missions are listed in file order: `avaritia` (Act I, level 15, enemy `avaritia`: 2200 HP / 160 ATK / 15 DEF / 5 LIFE with Medusa's `medusa_pattern`, rewards 5000 EXP, 15000 gold, Gold Dust and Dark Pearl), then the bonus `the_gorgon` (Medusa, level 20, unchanged). Avaritia's stats are pinned by the `AvaritiaBalance` tests (see `docs/benchmarks.md`).

`loadContent(dataDir)` returns `std::expected<Content, std::vector<ContentError>>`.
- It reads every file and **collects all errors** before failing: malformed JSON (with line/column), unknown or missing keys, duplicate or badly formed ids, out-of-range numbers, unknown references, a recipe listing the same ingredient twice, kind mismatches, missing `fighter`/`hunter`/`rogue`.
- **Upper bounds:** every stat, price, cost, reward, count and level is at most `kMaxStat` (1,000,000); path `multiplier` is 1-100, skill `atkMultiplier` 0-10, `healPercentOfDamage` 0-100. These keep all battle and town arithmetic inside `int`/`int64`, so even extreme data can't overflow.
- Cross-reference checks only run when every file parsed.
- Enemy sprites are looked up in `<dataDir>/../assets/sprites/`.
- `formatError` renders `file: entry: message`.

Skills have `effect` or `script`, and enemies `behavior` or `script` (exactly one each). Referenced scripts are checked and kept as described in `scripting.md`. `Content` is immutable after loading. `item(id)` and friends assume the id exists; `findItem(id)` returns `nullptr` instead. Lists keep file order, which `rollDrop` depends on.
